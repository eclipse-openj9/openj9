/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#include <string.h>
#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include "jithash.h"
#include "AtomicSupport.hpp"

extern "C" {

#define BIT_MASK(bit) ((UDATA) 1 << (UDATA) (bit))
#define DETERMINE_BIT_SET(value, bit) ((UDATA) (value) & BIT_MASK(bit))
#define SET_BIT(value, bit) ((UDATA) (value) | BIT_MASK(bit))
#define REMOVE_BIT(value, bit) ((UDATA) (value) & ~BIT_MASK(bit))

#define LOW_BIT_SET(value) ((UDATA) (value) & (UDATA) 1)
#define SET_LOW_BIT(value) ((UDATA) (value) | (UDATA) 1)
#define REMOVE_LOW_BIT(value) ((UDATA) (value) & (UDATA) -2)

#define DETERMINE_BUCKET(value, start, buckets) (((((UDATA)(value) - (UDATA)(start)) >> (UDATA) 9) * (UDATA)sizeof(UDATA)) + (UDATA)(buckets))
#define BUCKET_SIZE 512
#define METHOD_STORE_SIZE 256

#define COUNT_MARK_BIT 20


J9JITExceptionTable** 
hash_jit_artifact_array_insert(J9PortLibrary *portLibrary, J9JITHashTable *table, J9JITExceptionTable** array, J9JITExceptionTable *dataToInsert, UDATA startPC)
{
	J9JITExceptionTable** returnVal = array;
	PORT_ACCESS_FROM_PORT(portLibrary);

	/* If the array pointer is tagged, then it's not a chain, it's a single tagged metadata */

	if (LOW_BIT_SET(array)) {
		/* There is a single tagged entry in the bucket, not a chain.  In this case, we will
		 * always be allocating a new chain from the current allocate of the method store.
		 * We'll need 2 entries (one for the new entry and one for the existing tagged entry
		 * which will also terminate the chain.
		 */

		if ((table->currentAllocate + 2) > table->methodStoreEnd) {		/* This comparison is safe since currentAllocate and methodStoreEnd will always be pointing into the same allocated block */
			if (hash_jit_allocate_method_store(portLibrary, table) == NULL) {
				return NULL;
			}
		}
		returnVal = (J9JITExceptionTable**) table->currentAllocate;
		table->currentAllocate += 2;
		returnVal[0] = (J9JITExceptionTable *)dataToInsert;
 		returnVal[1] = (J9JITExceptionTable *)array;
	} else {
		J9JITExceptionTable** newElement;

		/* There is already a a chain, so we need to either add to the end of it if there is
		 * free space, or copy the chain and add to the end of the copy.
		 */

		newElement = array;
		do {
			++newElement;
		} while (!LOW_BIT_SET(*(newElement-1)));

		/* If there is a NULL at the newElement position, then we can just add there.
		 * It is safe to check *newElement even when the method store is full because
		 * the method store always has an extra non-NULL entry on the end.
		 */

		if (*newElement == NULL) {
			/* Adding to the end of an existing chain with a free slot after it.  To avoid twizzling
			 * bits, copy the existing chain end forward one slot, and place the new entry in the
			 * old end slot.  A write barrier must be issued before storing the new element, both
			 * to ensure the metadata is fully visible before it can be seen in the array, and to
			 * make the new chain end visible.
			*/

			*newElement = *(newElement - 1);
			VM_AtomicSupport::writeBarrier();
			*(newElement - 1) = dataToInsert;

			/* CMVC 117169 - increase the next allocation point only if the new element is not being stored into freed space */
			if (newElement == (J9JITExceptionTable **) table->currentAllocate) {
				table->currentAllocate += 1;
			}
		} else {
			UDATA chainLength = newElement - array;

			/* Not enough space to add to the end of the existing chain, so it must be copied
			 * and extended in free space.  There may be enough free space in the current
			 * method store.  Once space is found, copy the chain into it with the new entry at
			 * the beginning (to avoid twizzling tag bits).  There's no need for a write barrier
			 * here since the new chain is not visible to anyone yet, and the caller of this
			 * function issues a write barrier before updating the bucket pointer.
			*/

			if ((table->currentAllocate + chainLength + 1) > table->methodStoreEnd) {		/* This comparison is safe since currentAllocate and methodStoreEnd will always be pointing into the same allocated block */
				if (hash_jit_allocate_method_store(portLibrary, table) == NULL) {
					return NULL;
				}
			}

			returnVal = (J9JITExceptionTable**) table->currentAllocate;
			table->currentAllocate += (chainLength + 1);
			returnVal[0] = dataToInsert;
			memcpy(returnVal + 1, array, chainLength * sizeof(UDATA));	/* safe to memcpy since the new array is not yet visible */
		}
	}

	return returnVal;
}

J9JITExceptionTable** hash_jit_artifact_array_remove(J9PortLibrary *portLibrary, J9JITExceptionTable** array, J9JITExceptionTable *dataToRemove) {
	J9JITExceptionTable**  index;
	UDATA count= 0;
	UDATA size;
	UDATA removeSpot = 0;
	PORT_ACCESS_FROM_PORT(portLibrary);

	index = array;

	while(!LOW_BIT_SET(*index)) {						/* search for dataToRemove in the array */
		++count;
		if (*index == dataToRemove)
			removeSpot = count;
		++index;
	}

	if ((J9JITExceptionTable*) REMOVE_LOW_BIT(*index) == dataToRemove) {
		*index=0;										/* dataToRemove is last pointer in the array. */
		--index;
		*index = (J9JITExceptionTable*)SET_LOW_BIT(*index);
	} else if(removeSpot) {
		size = (count-removeSpot+1)*sizeof(UDATA);		/* dataToRemove is in middle (or start) of the array. */ 
		memmove((void*)(array+removeSpot-1),(void*)(array+removeSpot),size);
		*index = 0;
	} else {
		return (J9JITExceptionTable**) 1;					/* We did not find dataToRemove in array */
	}

	/* tidy the case of the array having contracted to becoming a single element - it can be recycled. */
	if(LOW_BIT_SET(*array)) {
		/* CMVC 117169 - NULL unused slots to allow re-use, and to simplify debugging */
		J9JITExceptionTable ** rc = (J9JITExceptionTable**) *array;		/* Only one pointer left.  Just return the one pointer */
		*array = NULL;
		return rc;
	}

	return array;
}

UDATA 
hash_jit_artifact_insert_range(J9PortLibrary *portLibrary, J9JITHashTable *table, J9JITExceptionTable *dataToInsert, UDATA startPC, UDATA endPC)
{
	J9JITExceptionTable** index;
	J9JITExceptionTable** endIndex;
	J9JITExceptionTable** temp;
	PORT_ACCESS_FROM_PORT(portLibrary);

	if ((startPC < table->start) || (endPC > table->end)) {
		return 1;
	}

	index = (J9JITExceptionTable **) DETERMINE_BUCKET(startPC, table->start, table->buckets);
	endIndex = (J9JITExceptionTable **) DETERMINE_BUCKET(endPC, table->start, table->buckets);

	do {
		if (*index) {
			temp = hash_jit_artifact_array_insert(portLibrary, table, (J9JITExceptionTable**) *index, dataToInsert, startPC);
			if (!temp) {
				return 2;
			}
			VM_AtomicSupport::writeBarrier();
			*index = (J9JITExceptionTable *) temp;
		} else {
			VM_AtomicSupport::writeBarrier();
			*index = (J9JITExceptionTable *) SET_LOW_BIT(dataToInsert);
		}

	} while (++index <= endIndex);
	
	return 0;
}

UDATA hash_jit_artifact_insert(J9PortLibrary *portLibrary, J9JITHashTable *table, J9JITExceptionTable *dataToInsert) {
	UDATA result;

        result = hash_jit_artifact_insert_range(portLibrary, table, dataToInsert, dataToInsert->startPC, dataToInsert->endWarmPC);
        if (result)
                return result;
        if (dataToInsert->startColdPC)
                result = hash_jit_artifact_insert_range(portLibrary, table, dataToInsert, dataToInsert->startColdPC, dataToInsert->endPC);

	return result;
}

UDATA hash_jit_artifact_remove_range(J9PortLibrary *portLibrary, J9JITHashTable *table, J9JITExceptionTable *dataToRemove, UDATA startPC, UDATA endPC) {
	J9JITExceptionTable** index;
	J9JITExceptionTable** endIndex;
	J9JITExceptionTable* temp;
	PORT_ACCESS_FROM_PORT(portLibrary);

	if ((startPC < table->start) || (endPC > table->end))
		return (UDATA) 1;

	index = (J9JITExceptionTable**) DETERMINE_BUCKET(startPC, table->start, table->buckets);
	endIndex = (J9JITExceptionTable**) DETERMINE_BUCKET(endPC, table->start, table->buckets);
	do {
		if (LOW_BIT_SET(*index)) {
			if ((J9JITExceptionTable *)REMOVE_LOW_BIT(*index) == dataToRemove)
				*index = 0;
			else
				return (UDATA) 1;
		} else if (*index) {
			temp = (J9JITExceptionTable*) hash_jit_artifact_array_remove(portLibrary, (J9JITExceptionTable**) *index, dataToRemove);
			if (!temp) return (UDATA) 1;
			else if (temp == (J9JITExceptionTable*) 1) return (UDATA) 2;
			else
				*index = temp;
		} else
			return (UDATA) 1;
	} while (++index <= endIndex);
	
	return (UDATA) 0;
}

UDATA hash_jit_artifact_remove(J9PortLibrary *portLibrary, J9JITHashTable *table, J9JITExceptionTable *dataToRemove) {
	UDATA result;

	result = hash_jit_artifact_remove_range(portLibrary, table, dataToRemove, dataToRemove->startPC, dataToRemove->endWarmPC);
        if (result)
                return result;

	if (dataToRemove->startColdPC)
                result = hash_jit_artifact_remove_range(portLibrary, table, dataToRemove, dataToRemove->startColdPC, dataToRemove->endPC);
	return result;
}

J9JITHashTable *hash_jit_allocate(J9PortLibrary * portLibrary, UDATA start, UDATA end)
{
	J9JITHashTable *table;
	UDATA size;
	PORT_ACCESS_FROM_PORT(portLibrary);

	table = (J9JITHashTable *) j9mem_allocate_memory(sizeof(J9JITHashTable), OMRMEM_CATEGORY_JIT);
	if (table == NULL) {
		return NULL;
	}
	memset(table, 0, sizeof(*table));
	table->start = start;
	table->end = end;

	size = DETERMINE_BUCKET(end, start, 0) + sizeof(UDATA);
	table->buckets = (UDATA *) j9mem_allocate_memory(size, OMRMEM_CATEGORY_JIT);
	if (table->buckets == NULL) {
		j9mem_free_memory(table);
		return NULL;
	}
	memset(table->buckets, 0, size);

	if (hash_jit_allocate_method_store(portLibrary, table) == NULL) {
		j9mem_free_memory(table->buckets);
		j9mem_free_memory(table);
		return NULL;
	}

	return table;
}

void hash_jit_free(J9PortLibrary * portLibrary, J9JITHashTable * table) {
	UDATA *methodStoreCurrent, *methodStoreNext;
	PORT_ACCESS_FROM_PORT(portLibrary);

	methodStoreCurrent = table->methodStoreStart;	

	while (methodStoreCurrent) {
		methodStoreNext = (UDATA *)*methodStoreCurrent;
		j9mem_free_memory(methodStoreCurrent);
		methodStoreCurrent = methodStoreNext;
	}
	j9mem_free_memory(table->buckets);
	j9mem_free_memory(table);
}

J9JITHashTable* hash_jit_toJ9MemorySegment(J9JITHashTable * table, J9MemorySegment * codeCache, J9MemorySegment * dataCache) {
	J9JITHashTable * dataCacheHashTable;
	J9JITExceptionTable** index;
	J9JITExceptionTable** endIndex;
	J9JITExceptionTable** traversal;
	J9JITExceptionTable** array;
	UDATA start, end, bucketSize;
	UDATA byteCount;
	U_8* allocate;
	U_8* arrayAllocate;

	/* first determine amount of room needed for data structure */
	index = (J9JITExceptionTable**) DETERMINE_BUCKET(table->start, table->start, table->buckets);
	endIndex = (J9JITExceptionTable**) DETERMINE_BUCKET(table->end, table->start, table->buckets);

	while ((*index == 0) && (index < endIndex)) ++index;
	while ((*endIndex == 0) && (index <= endIndex)) --endIndex;

	if (endIndex < index) return 0;

	if (LOW_BIT_SET(*index)) {
		start = ((J9JITExceptionTable *) REMOVE_LOW_BIT(*index))->startPC;
	} else {
		array = (J9JITExceptionTable**) *index;
		/* assumes array contains more than one element */
		start = (*array)->startPC;
		++array;

		while (!LOW_BIT_SET(*array)) {
			if (start > (*array)->startPC)
				start = (*array)->startPC;
			++array;
		}

		array = (J9JITExceptionTable **) REMOVE_LOW_BIT(*array);
		if (start > ((J9JITExceptionTable *) array)->startPC)
			start = ((J9JITExceptionTable *) array)->startPC;
	}
	/* The start must be the same delta from the granularity (BUCKET_SIZE) */
	/* as table->start was (which is the base of the code cache) */
	start = ((start - table->start) / BUCKET_SIZE) * BUCKET_SIZE + table->start;

	if (LOW_BIT_SET(*endIndex)) {
		end = ((J9JITExceptionTable *) REMOVE_LOW_BIT(*endIndex))->endPC;
	} else {
		array = (J9JITExceptionTable**) *endIndex;
		/* assumes array contains more than one element */
		end = (*array)->endPC;
		++array;

		while (!LOW_BIT_SET(*array)) {
			if (end < (*array)->endPC)
				end = (*array)->endPC;
			++array;
		}

		array = (J9JITExceptionTable **) REMOVE_LOW_BIT(*array);
		if (end < ((J9JITExceptionTable *) array)->endPC)
			end = ((J9JITExceptionTable *) array)->endPC;
	}

	bucketSize = DETERMINE_BUCKET(end, start, 0) + sizeof(UDATA);
	byteCount = bucketSize;

	traversal = index;
	while (traversal <= endIndex) {
		if (!LOW_BIT_SET(*traversal) && (*traversal)) {
			byteCount += sizeof(UDATA);
			array = (J9JITExceptionTable**) *traversal;
			while (!LOW_BIT_SET(*array)) {
				byteCount += sizeof(UDATA);
				++array;
			}
		}
		++traversal;
	}

	byteCount += sizeof(J9JITHashTable);
	/* check alignment - currently removed */
	/*if (LOW_BIT_SET(dataCache->heapAlloc)) 
		++byteCount;*/

	/* determine if enough room is available */
	if ((UDATA)(dataCache->heapTop - dataCache->heapAlloc) < (byteCount + sizeof(J9JITDataCacheHeader)))
		return 0;

	allocate = dataCache->heapAlloc;
	((J9JITDataCacheHeader *) allocate)->size = (U_32) (byteCount + sizeof(J9JITDataCacheHeader));
	((J9JITDataCacheHeader *) allocate)->type = J9_JIT_DCE_HASH_TABLE;

	allocate += sizeof(J9JITDataCacheHeader);

	/* point buckets to first aligned chunk after the J9JITHashTable structure */
	dataCacheHashTable = (J9JITHashTable *) allocate;
	/* check alignment - currently removed */
	/*if (LOW_BIT_SET(dataCache->heapAlloc))
		dataCacheHashTable->buckets = (UDATA *) (allocate + sizeof(J9JITHashTable) + 1);
	else*/
		dataCacheHashTable->buckets = (UDATA *) (allocate + sizeof(J9JITHashTable));

	dataCacheHashTable->parentAVLTreeNode.rightChild = dataCacheHashTable->parentAVLTreeNode.leftChild = 0;
	dataCacheHashTable->flags = JIT_HASH_IN_DATA_CACHE;

	dataCacheHashTable->start = start;
	dataCacheHashTable->end = end;
	allocate = (U_8 *) dataCacheHashTable->buckets;
	arrayAllocate = allocate + bucketSize;

	dataCache->heapAlloc += (byteCount + sizeof(J9JITDataCacheHeader));

	/* traverse hash table inserting required data */
	traversal = index;
	while (traversal <= endIndex) {
		if (LOW_BIT_SET(*traversal) || !(*traversal)) {
			*((J9JITExceptionTable **) allocate) = *traversal;
		} else {
			*((J9JITExceptionTable ***) allocate) = (J9JITExceptionTable **) arrayAllocate;

			array = (J9JITExceptionTable**) *traversal;
			while (!LOW_BIT_SET(*array)) {
				*((J9JITExceptionTable **) arrayAllocate) = *array;
				arrayAllocate += sizeof(UDATA);
				++array;
			}
			*((J9JITExceptionTable **) arrayAllocate) = *array;
			arrayAllocate += sizeof(UDATA);
		}
		allocate += sizeof(UDATA);
		++traversal;
	}

	return dataCacheHashTable;
}

/*
 * Start walking the specified J9JITHashTable.
 *
 * Returns the first element in the table, or NULL if the table is empty
 *
 * See hash_jit_next_do
 */
J9JITExceptionTable * hash_jit_start_do(J9JITHashTableWalkState* walkState, J9JITHashTable* table) {
	walkState->table = table;
	walkState->index = 0;
	walkState->bucket = NULL;
	return hash_jit_next_do(walkState);
}

/*
 * Iterate over a J9JITHashTable, using a walk state initialized by hash_jit_start_do
 *
 * Returns the next element in the table, or NULL if the table has no more elements
 *
 * Notes:
 *  1) The same element may be returned more than once, since it may be stored in
 *      more than one hash bucket
 *  2) It is not safe to call this function if the table has been modified (elements added
 *      or removed) since the last call the hash_jit_next_do or hash_jit_start_do
 */
J9JITExceptionTable * hash_jit_next_do(J9JITHashTableWalkState* walkState) {
	J9JITHashTable* table = walkState->table;
	UDATA size = DETERMINE_BUCKET(table->end, table->start, 0) / sizeof(UDATA) + 1;
	J9JITExceptionTable* metaData;

	while (walkState->bucket == NULL) {
		UDATA bucket;

		if (walkState->index >= size) return NULL;

		bucket = table->buckets[walkState->index];
		if (bucket == 0) {
			walkState->index += 1;
		} else if (LOW_BIT_SET(bucket)) {
			/* just pretend that this is a list with one element */
			walkState->bucket = &table->buckets[walkState->index];
		} else {
			walkState->bucket = (UDATA*)bucket;
		}
	}

	metaData = (J9JITExceptionTable*)REMOVE_LOW_BIT(*walkState->bucket);

	if ( LOW_BIT_SET(*walkState->bucket) ) {
		walkState->bucket = NULL;
		walkState->index += 1;
	} else {
		walkState->bucket += 1;
	}

	return metaData;
}

J9JITExceptionTable** hash_jit_allocate_method_store(J9PortLibrary *portLibrary, J9JITHashTable *table)
{
	/* CMVC 117169 - add 1 slot for link, 1 slot for terminator */
	UDATA size = (METHOD_STORE_SIZE + 2) * sizeof(UDATA);
	UDATA* newStore;
	PORT_ACCESS_FROM_PORT(portLibrary);

	newStore = (UDATA *) j9mem_allocate_memory(size, OMRMEM_CATEGORY_JIT);

	if (newStore != NULL) {
		memset(newStore, 0, size);

		*newStore = (UDATA) table->methodStoreStart;						/* Link to the old store */

		table->methodStoreStart = (UDATA *) newStore;
		table->methodStoreEnd = (UDATA *) (newStore + METHOD_STORE_SIZE + 1);
		table->currentAllocate = (UDATA *) (newStore + 1);

		/* CMVC 117169 - Ensure that the method store is terminated with a non-NULL entry */
		*(table->methodStoreEnd) = (UDATA) 0xBAAD076D;
	}

	return (J9JITExceptionTable**) newStore;
}

}
