/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

/**
 * @file
 * @ingroup Shared_Common
 */

#include "classpathcache.h"
#include "ut_j9shr.h"
#include <string.h>

#include "classpathcache.h"

static J9ClasspathByID* findIdentifiedWithPartition(J9VMThread* currentThread, struct J9ClasspathByIDArray* theArray, UDATA element, const char* partition, UDATA partitionLen);
static UDATA localMatchCheck(J9VMThread* currentThread, struct J9ClasspathByIDArray* theArray, IDATA callerHelperID, IDATA arrayIndex, UDATA indexInCacheHelper, const char* partition, UDATA partitionLen, UDATA doSet);



/**
 * SPEC: Clears ALL classpaths for found ID.
 * No need to have "toFree" parameter in this function. 
 * The caller knows what the classpath is, and will free it if required 
 *
 * @param [in] portlib  The portlib
 * @param [in] theArray  Pointer to the J9ClasspathByIDArray linked list
 * @param [in] cp Pointer to the classpath to be cleared
 */
void clearIdentifiedClasspath(J9PortLibrary* portlib, struct J9ClasspathByIDArray* theArray, void* cp) 
{
	IDATA idToKill = -1;

	Trc_SHR_CPC_ClearIdentified_Entry(theArray, theArray->size, cp);

	do {
		idToKill = getIDForIdentified(portlib, theArray, cp, idToKill+1);
		Trc_SHR_CPC_ClearIdentified_Killing(idToKill);

		if (idToKill != ID_NOT_FOUND) {
			J9ClasspathByIDArray* walk = theArray;
			while (walk) {
				resetIdentifiedClasspath(walk->array[idToKill], walk->size);
				walk = walk->next;
			}
		}
	} while (idToKill != ID_NOT_FOUND);

	Trc_SHR_CPC_ClearIdentified_Exit();
}
/**
 * SPEC: Resets a J9ClasspathByID. 
 * Note: Should not reset the partition.
 *
 * @param [in] theArray  Pointer to the J9ClasspathByIDArray array
 * @param [in] arrayLength  Length of the failed matches array for this J9ClasspathByIDArray element
 */
void 
resetIdentifiedClasspath(struct J9ClasspathByID* toReset, UDATA arrayLength)
{
	UDATA i;

	toReset->header.id = 0;
	toReset->header.cpData = NULL;
	toReset->entryCount = 0;
	for (i=0; i<arrayLength; i++) {
		toReset->failedMatches[i] = 0;
	}
}

/**
 * SPEC: Given classpath array "theArray", find a classpath which has helper ID "helperID"
 * and which contains # of items "itemsAdded". "partition" and "partitionLen can be NULL or can be a partition.
 * Function returns a pointer to a classpath or NULL (classpath may be in the cache or may be local - hence void*).
 * If an entry exists for given helperID and partition, but itemsAdded is different, the old entry in the array is cleared and the new entry is added.
 * In this case, the pointer to the old classpath is also returned via cpToFree, giving the option for the caller to free the classpath memory if appropriate.
 *
 * @param [in] currentThread  Pointer to the J9VMThread structure for the current thread
 * @param [in] theArrayPtr  Pointer to the J9ClasspathByIDArray array
 * @param [in] helperId  Index into the J9ClasspathByID array within theArrayPtr linked list element
 * @param [in] itemsAdded  Number of items in the classpath
 * @param [in] partition  Pointer to the name of the partition (nul terminated string)
 * @param [in] partitionLen  Length of the partition name string
 * @param [out] cpToFree  Pointer to old classpath that can be freed
 *
 * @return If successful, pointer to classpath, otherwise NULL
 */
void* 
getIdentifiedClasspath(J9VMThread* currentThread, struct J9ClasspathByIDArray* theArray, IDATA helperID, UDATA itemsAdded, const char* partition, UDATA partitionLen, void** cpToFree) 
{
	struct J9ClasspathByID* identified = NULL;
	UDATA arrayIndex = (UDATA)helperID;

	Trc_SHR_CPC_getIdentifiedClasspath_Entry(currentThread, theArray, theArray->size, helperID, itemsAdded);

	if ((helperID > MAX_CACHED_CLASSPATHS) || (arrayIndex >= theArray->size)) {
		Trc_SHR_CPC_getIdentifiedClasspath_ExitBadIndex(currentThread);
		return NULL;
	}
	if (cpToFree) {
		*cpToFree = NULL;		/* Ensure this is initialized */
	}

	/* Bootstrap loader has cpID == 0 */
	if (!partition) {
		identified = theArray->array[arrayIndex];
	} else {
		identified = findIdentifiedWithPartition(currentThread, theArray, arrayIndex, partition, partitionLen);
	}
	if (!identified) {
		Trc_SHR_CPC_getIdentifiedClasspath_ExitNotFound(currentThread);
		return NULL;
	}

	Trc_SHR_CPC_getIdentifiedClasspath_Found(currentThread, identified);
	if (identified->header.cpData != NULL) {
		if (identified->entryCount != itemsAdded) {
			/* Classpath has been updated */
			if (cpToFree) {
				*cpToFree = identified->header.cpData;
			}
			resetIdentifiedClasspath(identified, theArray->size);
			Trc_SHR_CPC_getIdentifiedClasspath_Reset(currentThread, identified->entryCount, itemsAdded);
			return NULL;
		}
	} else {
		Trc_SHR_CPC_getIdentifiedClasspath_NullCpData(currentThread);
		return NULL;
	}

	Trc_SHR_CPC_getIdentifiedClasspath_ExitSuccess(currentThread, identified->header.cpData);
	return identified->header.cpData;
}

/**
 * SPEC: Given a classpath linked list "theArray", find a helperID for a given classpath "cp".
 * 
 * @param [in] portlib  The portlib
 * @param [in] theArray  Pointer to the J9ClasspathByIDArray linked list
 * @param [in] cp  Pointer to the classpath
 * @param [in] walkFrom  The index to in theArray to start looking for ID from (allows the function to be called multiple times to find multiple IDs)
 *
 * @return  helperID, if found, ID_NOT_FOUND otherwise
 */
IDATA 
getIDForIdentified(J9PortLibrary* portlib, struct J9ClasspathByIDArray* theArray, void* cp, IDATA walkFrom) 
{
	UDATA i;
	J9ClasspathByIDArray* walk = theArray;

	Trc_SHR_CPC_getIDForIdentified_Entry(theArray, theArray->size, cp);

	while (walk) {
		for (i=walkFrom; i<walk->size; i++) {
			if (walk->array[i]->header.cpData==cp) {
				Trc_SHR_CPC_getIDForIdentified_ExitFound(i);
				return i;
			}
		}
		walk = walk->next;
	}
	Trc_SHR_CPC_getIDForIdentified_ExitNotFound();
	return ID_NOT_FOUND;
}

/**
 * SPEC: Sets an element in classpath array within an element of "theArrayPtr" linked list. The classpath being added has helper ID "helperID",
 * contains items "itemsAdded" and partition "partition" of length "partitionLen" (partition may be NULL). The "cp" parameter is the actual classpath
 * represented by the array entry. This could be a classpath in the cache or a local classpath.
 * If helperID is greater than or equal to the length of the array, the array is grown and copied and the new reference is passed back through "theArrayPtr".
 * The function always returns the current length of the array, so if it is grown, the new length is returned.
 *
 * @param [in] currentThread  Pointer to the J9VMThread structure for the current thread
 * @param [in,out] theArrayPtr  Pointer to a pointer to the J9ClasspathByIDArray linked list.  
 *        If the J9ClasspathByID array within this is grown, it will be updated to the location of the new J9ClasspathByIDArray structure.
 * @param [in] helperId  Index into J9ClasspathByID array
 * @param [in] itemsAdded  Number of items in the classpath
 * @param [in] partition  Pointer to the name of the partition (nul terminated string)
 * @param [in] partitionLen  Length of the partition name string
 * @param [in] cp  Pointer to the classpath to be stored in the array
 *
 * @return Current length of the array, if the array has grown this will be the new length.  Please note that as partition support is not currently implemented
 *        this will be 1 on success and 0 on failure to allocate an array
 */
UDATA 
setIdentifiedClasspath(J9VMThread* currentThread, struct J9ClasspathByIDArray** theArrayPtr, IDATA helperID, UDATA itemsAdded, const char* partition, UDATA partitionLen, void* cp)
{
	UDATA arrayIndex = (UDATA)helperID;
	UDATA origSize = (*theArrayPtr)->size;
	UDATA newSize = origSize;
	struct J9ClasspathByIDArray* newArray = NULL;
	struct J9ClasspathByIDArray* walk = NULL;
	UDATA i = 0;
	struct J9ClasspathByID* newLink = NULL;
	J9PortLibrary* portlib = currentThread->javaVM->portLibrary;
	
	PORT_ACCESS_FROM_PORT(portlib);

	Trc_SHR_CPC_setIdentifiedClasspath_Entry(currentThread, (*theArrayPtr), origSize, helperID, itemsAdded, cp);

	if (helperID > MAX_CACHED_CLASSPATHS) {
		Trc_SHR_CPC_setIdentifiedClasspath_ExitAlloc(currentThread);
		return 0;
	}
	
	if (arrayIndex >= origSize) {
		struct J9ClasspathByIDArray* prev = NULL;
		struct J9ClasspathByIDArray* toFree = NULL;

		walk = *theArrayPtr;
		*theArrayPtr = NULL;
		newSize = (origSize + arrayIndex);
		Trc_SHR_CPC_setIdentifiedClasspath_GrowingArray(currentThread, newSize);

		while (walk) {
			UDATA walkPartitionLen = (walk->partition) ? strlen(walk->partition) : 0;
			if (!(newArray = initializeIdentifiedClasspathArray(portlib, newSize, walk->partition, walkPartitionLen, walk->partitionHash))) {
				Trc_SHR_CPC_setIdentifiedClasspath_ExitAlloc(currentThread);
				return 0;
			}
			for (i=0; i<origSize; i++) {
				U_8* failedMatchPtr = newArray->array[i]->failedMatches;
				memcpy(newArray->array[i], walk->array[i], (sizeof(struct J9ClasspathByID) + (origSize * sizeof(U_8))));
				newArray->array[i]->failedMatches = failedMatchPtr;		/* Reset overwritten ptr */
			}
			toFree = walk;
			walk = walk->next;
			j9mem_free_memory(toFree);
			if (!(*theArrayPtr)) {
				*theArrayPtr = newArray;
			}
			if (prev) {
				prev->next = newArray;
			}
			prev = newArray;
		}
	}

	if (!partition) {
		newLink = (*theArrayPtr)->array[arrayIndex];
	} else {
		Trc_SHR_CPC_setIdentifiedClasspath_Partition(currentThread, partitionLen, partition);

		if ((newLink = findIdentifiedWithPartition(currentThread, (*theArrayPtr), arrayIndex, partition, partitionLen))) {
			Trc_SHR_CPC_setIdentifiedClasspath_ExistingPartition(currentThread);
		}

		if (!newLink) {
			UDATA compareHash = currentThread->javaVM->internalVMFunctions->computeHashForUTF8((U_8*)partition, (U_16)partitionLen);
			struct J9ClasspathByIDArray* temp;

			Trc_SHR_CPC_setIdentifiedClasspath_CreatePartitionLink(currentThread);

			if (!(newArray = initializeIdentifiedClasspathArray(portlib, newSize, partition, partitionLen, compareHash))) {
				Trc_SHR_CPC_setIdentifiedClasspath_ExitAlloc(currentThread);
				return 0;
			}
			temp = (*theArrayPtr)->next;
			(*theArrayPtr)->next = newArray;
			newArray->next = temp;
			newLink = newArray->array[arrayIndex];
		}
	}
	Trc_SHR_CPC_setIdentifiedClasspath_SetupLink(currentThread, newLink, helperID, cp, itemsAdded);
	newLink->header.id = (U_16)helperID;
	newLink->header.cpData = cp;
	newLink->entryCount = itemsAdded;

	Trc_SHR_CPC_setIdentifiedClasspath_ExitDone(currentThread, newSize, (*theArrayPtr));
	return 1;
}


/**
 * SPEC: Should free the array and all partition arrays 
 *
 * @param [in] portlib  The portlib
 * @param [in] toFree  Pointer to the J9ClasspathByIDArray linked list
 */
void 
freeIdentifiedClasspathArray(J9PortLibrary* portlib, struct J9ClasspathByIDArray* toFree)
{
	J9ClasspathByIDArray* walk = toFree;
	J9ClasspathByIDArray* nextFree = NULL;

	PORT_ACCESS_FROM_PORT(portlib);

	Trc_SHR_CPC_FreeIdentifiedClasspathArray_Entry(toFree, toFree->size);

	while (walk) {
		nextFree = walk->next;
		j9mem_free_memory(walk);
		walk = nextFree;
	}

	Trc_SHR_CPC_FreeIdentifiedClasspathArray_Exit();
}

/**
 * SPEC: Create and return a linked list element, J9ClasspathByIDArray which includes an array of J9ClasspathByID structs of size "elements".
 *
 * @param [in] portlib  The portlib
 * @param [in] elements  Number of elements required in array
 * @param [in] partition  Pointer to the name of the partition (nul terminated string)
 * @param [in] partitionLen  Length of the partition name string
 * @param [in] partitionHash  Hash value for partition to be stored in array header
 * 
 * @result  If the array is successfully built, returns a pointer to the J9ClasspathByIDArray for the first element of the array, otherwise returns NULL
 */
struct J9ClasspathByIDArray*
initializeIdentifiedClasspathArray(J9PortLibrary* portlib, UDATA elements, const char* partition, UDATA partitionLen, IDATA partitionHash)
{
	struct J9ClasspathByIDArray* returnVal = NULL;
	UDATA identifiedSize = (sizeof(struct J9ClasspathByIDArray) + ((sizeof(struct J9ClasspathByID*) + sizeof(struct J9ClasspathByID)) * elements) + CPC_PAD(sizeof(U_8) * elements * elements));
	UDATA sizeWithPartition = identifiedSize;
	UDATA elementsAddress = 0;
	UDATA i = 0;
	UDATA j = 0;
	char* copiedPartition = NULL;

	PORT_ACCESS_FROM_PORT(portlib);
	
	Trc_SHR_CPC_InitializeIdentifiedClasspathArray_Entry(elements);

	/* If we get into territory where we have more classpaths than MAX_CACHED_CLASSPATHS, stop using classpath cacheing as
	 * the array will get too large and searching it will be too expensive */
	if ((elements < 1) || (elements > MAX_CACHED_CLASSPATHS)) {
		Trc_SHR_CPC_InitializeIdentifiedClasspathArray_Exit1();
		return NULL;
	}

	if (partitionLen>0) {
		sizeWithPartition += CPC_PAD(partitionLen+1);
	}
	if (!(returnVal = (struct J9ClasspathByIDArray*)j9mem_allocate_memory(sizeWithPartition, J9MEM_CATEGORY_CLASSES))) {
		Trc_SHR_CPC_InitializeIdentifiedClasspathArray_Exit2();
		return NULL;
	}
	memset(returnVal, 0, sizeWithPartition);
	returnVal->array = (J9ClasspathByID**)(((UDATA)returnVal) + sizeof(J9ClasspathByIDArray));
	returnVal->size = elements;
	elementsAddress = ((UDATA)returnVal->array) + (elements * sizeof(struct J9ClasspathByID*));
	if (partitionLen>0) {
		copiedPartition = (char*)((UDATA)returnVal + identifiedSize);
		strncpy(copiedPartition, partition, partitionLen);
		returnVal->partition = copiedPartition;
		returnVal->partitionHash = partitionHash;
	}
	for (i=0; i<elements; i++) {
		returnVal->array[i] = (struct J9ClasspathByID*)(elementsAddress + ( i * (sizeof(struct J9ClasspathByID) + (sizeof(U_8) * elements)) ));
		returnVal->array[i]->failedMatches = (U_8*)((UDATA)returnVal->array[i]) + sizeof(struct J9ClasspathByID);
		for (j=0; j<elements; j++) {
			returnVal->array[i]->failedMatches[j] = FAILED_MATCH_NOTSET;
		}
	}

	Trc_SHR_CPC_InitializeIdentifiedClasspathArray_Exit3(returnVal);
	return returnVal;
}

static J9ClasspathByID*
findIdentifiedWithPartition(J9VMThread* currentThread, struct J9ClasspathByIDArray* theArray, UDATA element, const char* partition, UDATA partitionLen) 
{
	J9ClasspathByID* returnVal = NULL;
	J9ClasspathByIDArray* walk = theArray->next;
	UDATA compareHash = currentThread->javaVM->internalVMFunctions->computeHashForUTF8((U_8*)partition, (U_16)partitionLen);

	Trc_SHR_CPC_findIdentifiedWithPartition_Partition(currentThread, partitionLen, partition);
	while (walk) {
		if (walk->partitionHash==compareHash) {
			Trc_SHR_CPC_getIdentifiedClasspath_ComparePartitions(currentThread, walk->partition, partitionLen, partition);
			if (strncmp(walk->partition, partition, partitionLen)==0) {
				returnVal = walk->array[element];
				break;
			}
		}
		walk = walk->next;
	}
	return returnVal;
}
/**
 * SPEC: Registers a failed match between 2 identified classpaths.
 * This is an optimization that reduces the number of failed classpath matches. 
 * This is particularly useful if the bootstrap classpath is long,
 * as every non-bootstrap FIND will have to first fail the bootstrap classpath check.
 * This only works if a ROMClass was found and if both the caller classpath and the cache classpath are identified.
 * The caller classpath ID is callerHelperID and the cache classpath ID is arrayIndex.
 * IndexInCacheHelper is the index of the ROMClass in the cache classpath.
 *
 * @param [in] currentThread  Pointer to the J9VMThread structure for the current thread
 * @param [in] theArray  Pointer to the J9ClasspathByIDArray array
 * @param [in] callerHelperId  Calling classloader id.  It is the index of the failedMatches array to be set
 * @param [in] arrayIndex  Index into theArrayPtr 
 * @param [in] indexInCacheHelper Other classloader id.  It is the value to be stored in failedMatches[callerHelperId] 
 * @param [in] partition  Pointer to the name of the partition (nul terminated string)
 * @param [in] partitionLen  Length of the partition name string
 */
void
registerFailedMatch(J9VMThread* currentThread, struct J9ClasspathByIDArray* theArray, IDATA callerHelperID, IDATA arrayIndex, UDATA indexInCacheHelper, const char* partition, UDATA partitionLen) 
{
	localMatchCheck(currentThread, theArray, callerHelperID, arrayIndex, indexInCacheHelper, partition, partitionLen, 1);
}

static UDATA
localMatchCheck(J9VMThread* currentThread, struct J9ClasspathByIDArray* theArray, IDATA callerHelperID, IDATA arrayIndex, UDATA indexInCacheHelper, const char* partition, UDATA partitionLen, UDATA doSet) 
{
	struct J9ClasspathByID* identified = NULL;
	UDATA returnVal = 0;

	Trc_SHR_CPC_localMatchCheck_Enter(currentThread, callerHelperID, arrayIndex, indexInCacheHelper);
	if ((arrayIndex > MAX_CACHED_CLASSPATHS) || 
			(callerHelperID > MAX_CACHED_CLASSPATHS) || 
			((UDATA)arrayIndex >= theArray->size) || 
			((UDATA)callerHelperID >= theArray->size) || 
			(indexInCacheHelper >= FAILED_MATCH_NOTSET)) {
		goto _done;
	}
	if (!partition) {
		identified = theArray->array[arrayIndex];
	} else {
		identified = findIdentifiedWithPartition(currentThread, theArray, arrayIndex, partition, partitionLen);
	}
	/* If no classpath has been identified for this id and partition, return 0 */
	if (!identified->header.cpData) {
		goto _done;
	}
	if (doSet) {
		identified->failedMatches[callerHelperID] = (U_8)indexInCacheHelper;
	} else {
		returnVal = (identified->failedMatches[callerHelperID] == indexInCacheHelper);
	}

_done:
	Trc_SHR_CPC_localMatchCheck_Exit(currentThread, returnVal);
	return returnVal;
}

/**
 * SPEC: Checks to see if a failed match has been previously registered. See @ref registerFailedMatch for details.
 *
 * @param [in] currentThread  Pointer to the J9VMThread structure for the current thread
 * @param [in] theArray  Pointer to the J9ClasspathByIDArray linked list
 * @param [in] callerHelperId  Calling classloader id.  It is the index of the failedMatches array to be checked
 * @param [in] arrayIndex  Index into theArrayPtr 
 * @param [in] indexInCacheHelper Other classloader id.  It is the value to be checked against failedMatches[callerHelperId] 
 * @param [in] partition  Pointer to the name of the partition (nul terminated string)
 * @param [in] partitionLen  Length of the partition name string
 *
 * @return ???
 */
UDATA
hasMatchFailedBefore(J9VMThread* currentThread, struct J9ClasspathByIDArray* theArray, IDATA callerHelperID, IDATA arrayIndex, UDATA indexInCacheHelper, const char* partition, UDATA partitionLen) 
{
	return localMatchCheck(currentThread, theArray, callerHelperID, arrayIndex, indexInCacheHelper, partition, partitionLen, 0);
}

