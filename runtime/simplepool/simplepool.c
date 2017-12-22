/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
 * @ingroup SimplePool
 * @brief SimplePool primitives (creation, iteration, deletion, etc.)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "j9comp.h"
#include "j9.h"
#include "simplepool_api.h"

#include "ut_simplepool.h"

/* First slot where an element can possibly be stored within a simple pool */
#define SP_FIRST_POSSIBLE_ELEMENT_SLOT(simplePool)		( (U_8*) ( (UDATA)simplePool + sizeof(J9SimplePool) ) )

#define ROUND_TO_SIZEOF_UDATA(number) (((number) + (sizeof(UDATA) - 1)) & (~(sizeof(UDATA) - 1)))
/**
 * The following macro evaluates to TRUE if the element is aligned within the simplePool
 *
 * @note We pass in the firstPossibleElementSlot instead of using the
 * SP_FIRST_POSSIBLE_ELEMENT_SLOT so that callers which have already
 * used the SP_FIRST_POSSIBLE_ELEMENT_SLOT macro can save the value
 * and pass it in for speed
 */
#define SP_ELEMENT_IS_ALIGNED(simplePool, firstPossibleElementSlot, element)	(0 == ( ( (UDATA)element - (UDATA)firstPossibleElementSlot ) % simplePool->elementSize ) )

static BOOLEAN iterateOverSimplePoolElements(J9SimplePool* simplePool, J9PortLibrary *portLib, BOOLEAN (*doFunction) (void* anElement, void* userData), void* userData, UDATA skipCount);

/**
 * Turns the block of memory pointed to by poolAddress into a simple pool
 * and returns a J9SimplePool pointer to the newly created data structure
 *
 * @param[in] poolAddress 	Address of the memory that will store the J9SimplePool
 * @param[in] memorySize 	Size in bytes of the pool, must be large enough to fit the
 * 							J9SimplePool struct and at least one element while not exceeding
 * 							SIMPLEPOOL_MAX_MEMORY_SIZE
 * @param[in] elementSize 	Size in bytes of one element in the pool, element size must
 * 							be a multiple of 4 and greater than or equal to SIMPLEPOOL_MIN_ELEMENT_SIZE
 * @param[in] flags 		None defined
 *
 * @return pointer to the simple pool, or NULL if the simple pool could not be created.
 */
J9SimplePool*
simplepool_new(void* poolAddress, U_32 memorySize, U_32 elementSize, U_32 flags)
{
	J9SimplePool* simplePool = poolAddress;
	UDATA blockEndAddr = 0;
	U_32 maxElements = 0;

	Trc_simplepool_new_Entry(poolAddress, memorySize, elementSize, flags);

	if (!(poolAddress && memorySize && elementSize)) {
		Trc_simplepool_new_NullParameters(poolAddress, memorySize, elementSize);
		simplePool = NULL;
	} else if ( 0 != (elementSize % 4) ) {
		Trc_simplepool_new_elementSizeUnaligned(elementSize);
		simplePool = NULL;
	} else if (elementSize < SIMPLEPOOL_MIN_ELEMENT_SIZE) {
		Trc_simplepool_new_elementSizeTooSmall(elementSize, SIMPLEPOOL_MIN_ELEMENT_SIZE);
		simplePool = NULL;
	} else if (memorySize < (sizeof(J9SimplePool) + elementSize)) {
		Trc_simplepool_new_memorySizeTooSmall(memorySize, elementSize);
		simplePool = NULL;
	} else if (memorySize > SIMPLEPOOL_MAX_MEMORY_SIZE) {
		Trc_simplepool_new_memorySizeTooLarge(memorySize, SIMPLEPOOL_MAX_MEMORY_SIZE);
		simplePool = NULL;
	}

	if (NULL != simplePool) {
		/**
		 * LAYOUT IN MEMORY
		 * 	 _______________________________________________
		 * 	|					|			|			 	|
		 * 	|struct J9SimplePool| element1 	|element 2	...	|
		 * 	|___________________|___________|_______________|
		 *
		 */

		memset(simplePool, 0, sizeof(J9SimplePool));

		simplePool->numElements = 0;
		simplePool->elementSize = elementSize;
		SRP_SET_TO_NULL(simplePool->freeList);
		SRP_SET(simplePool->firstFreeSlot, SP_FIRST_POSSIBLE_ELEMENT_SLOT(simplePool)); /* address of first element */

		maxElements = (memorySize - sizeof(J9SimplePool)) / elementSize;
		blockEndAddr = (UDATA)SP_FIRST_POSSIBLE_ELEMENT_SLOT(simplePool) + (maxElements * elementSize);
		SRP_SET(simplePool->blockEnd, blockEndAddr);
	}

	Trc_simplepool_new_Exit(simplePool);

	return simplePool;
}


/**
 *	Asks for the address of a new simple pool element.
 *
 *	If it succeeds, the address returned will have space for
 *	one element of the correct simple pool elementSize.
 *
 *	The contents of the element will be set to 0's
 *
 * @param[in] simplePool	A pointer to the simple pool
 *
 * @return 	Pointer to a new element or NULL if the simple pool is full
 * 			or an error occurred
 */
void*
simplepool_newElement(J9SimplePool* simplePool)
{
	void* newElement = NULL;
	J9SimplePoolFreeList* freeList = NULL;

	Trc_simplepool_newElement_Entry(simplePool);

	if (NULL == simplePool) {
		Trc_simplepool_newElement_NullSimplePool();
	} else {
		freeList = J9SIMPLEPOOL_FREELIST(simplePool);

		if (NULL == freeList) {
			U_8* firstFreeSlot = J9SIMPLEPOOL_FIRSTFREESLOT(simplePool);
			U_8* blockEnd = J9SIMPLEPOOL_BLOCKEND(simplePool);

			if (firstFreeSlot == blockEnd) {
				/* the simple pool is full */
				Trc_simplepool_newElement_simplePoolFull(simplePool);
			} else {
				/* use the element at the firstFreeSlot */
				newElement = (void*)firstFreeSlot;
				/* move firstFreeSlot pointer forward by the size of one element */
				SRP_SET(simplePool->firstFreeSlot, (UDATA)firstFreeSlot + simplePool->elementSize);
				simplePool->numElements++;
			}

		} else {
			J9SimplePoolFreeList* nextFree = J9SIMPLEPOOLFREELIST_NEXT(freeList);

			newElement = (void*)freeList;

			if (NULL != newElement) {
				SRP_SET(simplePool->freeList, nextFree);
				simplePool->numElements++;
			}
		}

		/* Set newElement memory to 0's */
		if (NULL != newElement) {
			memset(newElement, 0, simplePool->elementSize);
		}

	}

	Trc_simplepool_newElement_Exit(simplePool, newElement);

	return newElement;
}


/**
 *	Checks if the specified address corresponds to a valid element in the pool.
 *
 * @param[in] simplePool	A pointer to the simple pool
 * @param[in] address 		Pointer to the address to be checked
 *
 * @return  TRUE if the specified address corresponds to a valid element
 */
BOOLEAN
simplepool_isElement(J9SimplePool *simplePool, void *address)
{
	BOOLEAN rc = FALSE;

	Trc_simplepool_isElement_Entry(simplePool, address);

	if (!simplePool) {
		Trc_simplepool_isElement_NullParameter();
	} else {
		U_8* firstFreeSlot = J9SIMPLEPOOL_FIRSTFREESLOT(simplePool);
		U_8* firstPossibleElementSlot = SP_FIRST_POSSIBLE_ELEMENT_SLOT(simplePool);

		if ( ((U_8*)address >= firstFreeSlot) || ((U_8*)address < firstPossibleElementSlot) ) {
			/* the address is out of the possible address range for the simplePool */
			Trc_simplepool_isElement_elementAddressOutOfRange(simplePool, address, firstPossibleElementSlot, firstFreeSlot);
		} else {
			/* check if the address is aligned */
			if ( ! SP_ELEMENT_IS_ALIGNED(simplePool, firstPossibleElementSlot, address) ) {
				Trc_simplepool_isElement_unaligned(simplePool, simplePool->elementSize, address);
			} else {
				rc = TRUE;
			}
		}
	}

	Trc_simplepool_isElement_Exit(rc);

	return rc;
}


/**
 *	Deallocates an element from a simple pool.
 *
 * @param[in] simplePool	A pointer to the simple pool
 * @param[in] element 		Pointer to the element to be removed
 *
 * @return  0 upon success, -1 an error occurred
 */
IDATA
simplepool_removeElement(J9SimplePool *simplePool, void *element)
{
	IDATA rc = 0;

	Trc_simplepool_removeElement_Entry(simplePool, element);

	if (!simplePool) {
		Trc_simplepool_removeElement_NullParameter();
		rc = -1;
	} else {
		if (!simplepool_isElement(simplePool, element)) {
			Assert_simplepool_ShouldNeverHappen();
			rc = -1;
		} else {
			J9SimplePoolFreeList* freeList = J9SIMPLEPOOL_FREELIST(simplePool);

			/* passed all checks, free element by moving it to the head of the free list */
			SRP_SET( ((J9SimplePoolFreeList*)element)->next, freeList);
			SRP_SET(simplePool->freeList, element);

			SRP_SET( ((J9SimplePoolFreeList*)element)->simplePool, simplePool);

			simplePool->numElements--;
		}
	}

	Trc_simplepool_removeElement_Exit(rc);

	return rc;
}

/**
 * Get the maximum number of elements the simple pool can hold
 *
 * @param[in] simplePool The simple pool
 *
 * @return UDATA 0 upon error, the maximum number of elements the simple pool can hold otherwise
 */
UDATA
simplepool_maxNumElements(J9SimplePool *simplePool)
{
	UDATA maxElements = 0;
	Trc_simplepool_maxNumElements_Entry(simplePool);

	if (NULL == simplePool) {
		Trc_simplepool_maxNumElements_NullSimplePool();
	} else {
		maxElements = ((UDATA)J9SIMPLEPOOL_BLOCKEND(simplePool) - (UDATA)SP_FIRST_POSSIBLE_ELEMENT_SLOT(simplePool)) / simplePool->elementSize;
	}

	Trc_simplepool_maxNumElements_Exit(maxElements);

	return maxElements;
}

/**
 * Calculates the total size simple pool requires for the given number of elements.
 * @param	nodeSize			size of one node in simple pool
 * @param	numberOfElements 	number of elements to be stored in simplepool
 * @return 	total size that simple pools requires or using for the given number of elements.
 */
U_32
simplepool_totalSize(U_32  nodeSize, U_32 numberOfElements)
{
	return sizeof(J9SimplePool) + (nodeSize * numberOfElements);
}

/**
 *	Returns the number of elements in a given simple pool.
 *
 * @param[in] simplePool	A pointer to the simple pool
 *
 * @return 0 on error, the number of elements in the pool otherwise
 *
 */
UDATA
simplepool_numElements(J9SimplePool *simplePool)
{
	UDATA numElements = 0;

	Trc_simplepool_numElements_Entry(simplePool);

	if (NULL == simplePool) {
		Trc_simplepool_numElements_NullSimplePool();
	} else {
		numElements = simplePool->numElements;
	}

	Trc_simplepool_numElements_Exit(numElements);

	return numElements;
}



/**
 *	Calls a user provided function for each element in the simple pool. If the function
 *	returns FALSE, the iteration stops.
 *
 * @param[in] simplePool	The simple pool to "do" things to
 * @param[in] portLib		A pointer to the port library, (this is required for j9mem_allocate/free functions)
 * @param[in] doFunction	Pointer to function which will "do" things to the elements of simple pool. Return TRUE
 * 							to continue iterating, FALSE to stop iterating.
 * @param[in] userData		Pointer to data to be passed to "do" function, along with each simple pool element
 *
 * @return BOOLEAN FALSE if the doFunction returns FALSE, TRUE otherwise
 */
BOOLEAN
simplepool_do(J9SimplePool* simplePool, J9PortLibrary *portLib, BOOLEAN (*doFunction) (void* anElement, void* userData), void* userData)
{
	BOOLEAN rc = TRUE;

	Trc_simplepool_do_Entry(simplePool, doFunction, userData);

	if (! (simplePool && doFunction && portLib)) {
		Trc_simplepool_do_NullParameters();
	} else {
		rc = iterateOverSimplePoolElements(simplePool, portLib, doFunction, userData, 0);
	}

	Trc_simplepool_do_Exit(rc);
	return rc;
}

/**
 *  Helper function for @ref simplepool_do and @ref simplepool_checkConsistency
 *
 *	Calls a user provided function for each element in the simple pool skipping over skipCount
 *	allocated elements each time.
 *
 *	@note Callers are responsible for checking input parameters
 *
 * @param[in] simplePool	The simple pool to iterate over
 * @param[in] portLib		A pointer to the port library, (this is required for j9mem_allocate/free functions)
 * @param[in] doFunction	Pointer to function which will "do" things to the elements of simple pool. Return TRUE
 * 							to continue iterating, FALSE to stop iterating.
 * @param[in] userData		Pointer to data to be passed to "do" function, along with each simple pool element
 * @param[in] skipCount		Number of elements to skip after each iteration, 0 means every element will be iterated over
 *
 * @return BOOLEAN FALSE if the doFunction returns FALSE, TRUE otherwise
 */
static BOOLEAN
iterateOverSimplePoolElements(J9SimplePool* simplePool, J9PortLibrary *portLib, BOOLEAN (*doFunction) (void* anElement, void* userData), void* userData, UDATA skipCount)
{
	BOOLEAN rc = TRUE;
	U_8* allocMap = NULL;
	U_32 allocMapSize = 0;
	U_32 numElementsInFreeList = 0;
	J9SimplePoolFreeList* freeList = NULL;
	PORT_ACCESS_FROM_PORT(portLib);

	/* count the number of elements in the free list */
	freeList = J9SIMPLEPOOL_FREELIST(simplePool);
	while(NULL != freeList) {
		numElementsInFreeList++;
		freeList = J9SIMPLEPOOLFREELIST_NEXT(freeList);
	}

	allocMapSize = ((simplePool->numElements + numElementsInFreeList) / 8) + 1; /* +1 byte to account for possible remainder */
	allocMap = j9mem_allocate_memory(allocMapSize, OMRMEM_CATEGORY_VM);

	if (NULL != allocMap) {
		void* anElement = NULL;
		U_8* firstFreeSlot = NULL;
		U_8* firstPossibleElementSlot = SP_FIRST_POSSIBLE_ELEMENT_SLOT(simplePool);
		UDATA currElementIndex = 0;
		UDATA addCount = (skipCount + 1);

		/* mark all the bits in the allocMap as allocated
		 * 0 == allocated, 1 == free
		 */
		memset(allocMap, 0, allocMapSize);

		/* mark the slots corresponding to the elements in the free list as free in the allocMap */
		freeList = J9SIMPLEPOOL_FREELIST(simplePool);
		while(NULL != freeList) {
			currElementIndex = ( (UDATA)freeList - (UDATA) firstPossibleElementSlot ) / simplePool->elementSize;
			allocMap[currElementIndex/8] |= 0x80 >> (currElementIndex % 8); /* mark the corresponding bit in the map as free */
			freeList = J9SIMPLEPOOLFREELIST_NEXT(freeList);
		}

		/* Iterate over simple pool calling doFunction on each allocated element and
		 * if checkConsistency is TRUE, checking that the next pointer and simplePool 
		 * back pointer are valid for each free element
		 */
		currElementIndex = 0;
		firstFreeSlot = J9SIMPLEPOOL_FIRSTFREESLOT(simplePool);
		anElement = (void*) ((UDATA)firstPossibleElementSlot);
		/* everything past firstFreeSlot is free so we can stop once we reach it,
		 * we also need to check that anElement is greater than simplePool to
		 * protect against a possible UDATA wrap around
		 */
		while ( ((UDATA)anElement < (UDATA)firstFreeSlot) && ((UDATA)anElement > (UDATA)simplePool) ) {
			U_8 freeMask = 0x80 >> (currElementIndex % 8);
			if ( 0 == (allocMap[currElementIndex/8] & freeMask)) {
				/* corresponding bit is 0, element is allocated, call doFunction on it */
				BOOLEAN result = (doFunction)(anElement, userData);
				if (result == FALSE) {
					rc = FALSE;
					break;
				}
			}

			/* move to next element */
			anElement = (void*) ( (UDATA)anElement + (addCount * simplePool->elementSize));
			currElementIndex += addCount;
		}

		j9mem_free_memory(allocMap);
	} else {
		Trc_iterateOverSimplePoolElements_unableToAllocateAllocMap(allocMapSize);
	}
	
	return rc;
}

/**
 * Verify the simple pool structure.
 *
 * Perform verification that simplePool structure is consistent with memorySize and elementSize
 * Also verify that the freeList pointer is within the simplePool's boundaries, is aligned and
 * that the first element's back pointer correctly points back to the simplePool
 *
 * @param[in] simplePool	The simple pool to verify
 * @param[in] memorySize	The size in bytes of the memory block containing the simple pool
 * @param[in] elementSize	The size in bytes of each element
 *
 * @return TRUE if the simple pool verifies, FALSE otherwise
 *
 */
BOOLEAN
simplepool_verify(J9SimplePool* simplePool, U_32 memorySize, U_32 elementSize)
{
	BOOLEAN verifies = FALSE;

	Trc_simplepool_verify_Entry(simplePool, memorySize, elementSize);

	if (NULL != simplePool) {
		U_8* firstFreeSlot = J9SIMPLEPOOL_FIRSTFREESLOT(simplePool);
		U_8* firstPossibleElementSlot = SP_FIRST_POSSIBLE_ELEMENT_SLOT(simplePool);
		U_8* blockEnd = J9SIMPLEPOOL_BLOCKEND(simplePool);
		J9SimplePoolFreeList* freeList = J9SIMPLEPOOL_FREELIST(simplePool);
		UDATA elementSpaceSize = (UDATA)blockEnd - ((UDATA)simplePool + sizeof(J9SimplePool));
		/* blockEnd is calculated such that it coincides with the end of the last
		 * element we can fit into the allocated memory block. This means that the
		 * last few bytes in the allocated memory region could be unusable because
		 * they are not large enough to fit an entire element.
		 */
		UDATA lostSpace = ((UDATA)simplePool + memorySize) - (UDATA)blockEnd;
		UDATA freeListCounter = 0;

		if (elementSize != simplePool->elementSize) {
			Trc_simplepool_verify_incorrectElementSize(simplePool, simplePool->elementSize, elementSize);
		} else if (( lostSpace >= elementSize ) /* we should not waste more space than elementSize - 1 */
				|| ( elementSpaceSize < elementSize ) /* the simple pool should be large enough to fit at least one element */
				|| ( 0 != (elementSpaceSize % elementSize) ) /* element space should be a multiple of elementSize */
				|| ( memorySize != (sizeof(J9SimplePool) + elementSpaceSize + lostSpace) )
		) {
			/* the memorySize is incorrect or does not correspond with the values in the header */
			Trc_simplepool_verify_incorrectMemorySize(simplePool, elementSpaceSize, lostSpace, memorySize);
		} else if ( (UDATA)firstFreeSlot < (UDATA) firstPossibleElementSlot || (UDATA)firstFreeSlot > (UDATA)blockEnd ) {
			/* the firstFreeSlot is out of range */
			Trc_simplepool_verify_firstFreeSlotOutOfRange(simplePool, firstFreeSlot, firstPossibleElementSlot, blockEnd);
		} else if ( ! SP_ELEMENT_IS_ALIGNED(simplePool, firstPossibleElementSlot, firstFreeSlot) ) {
			/* the firstFreeSlot is unaligned */
			Trc_simplepool_verify_firstFreeSlotUnaligned(simplePool, firstFreeSlot, firstPossibleElementSlot, elementSize);
		} else if ( 0 != simplePool->flags ) {
			/* after zeroing out all valid flags from simplePool->flags, the result was not 0
			 * therefore simplePool->flags contains invalid flags
			 */
			Trc_simplepool_verify_invalidFlags(simplePool, simplePool->flags, 0);
		} else {
			/* all checks passed so far */
			verifies = TRUE;
		}

		/* Perform free list verification */
		while ( verifies && (NULL != freeList) ) {
			if ( ((UDATA)freeList >= (UDATA)firstFreeSlot) || ((UDATA)freeList < (UDATA)firstPossibleElementSlot)) {
				/* the freeList pointer is out of range */
				Trc_simplepool_verify_freeListoutOfRange(simplePool, freeList, firstPossibleElementSlot, firstFreeSlot);
				verifies = FALSE;
				break;
			} else if ( ! SP_ELEMENT_IS_ALIGNED(simplePool, firstPossibleElementSlot, freeList) ) {
				/* the freeList pointer is unaligned */
				Trc_simplepool_verify_freeListUnaligned(simplePool, freeList, simplePool->elementSize);
				verifies = FALSE;
				break;
			} else {
				/* check that the simple pool back pointer is correct */
				J9SimplePool* simplePoolPtr = J9SIMPLEPOOLFREELIST_SIMPLEPOOL(freeList);

				if (simplePoolPtr != simplePool) {
					Trc_simplepool_verify_freeListInvalidBackPointer(simplePool, freeList, simplePoolPtr);
					verifies = FALSE;
					break;
				}
			}
			freeListCounter++;
			freeList = J9SIMPLEPOOLFREELIST_NEXT(freeList);
		}
		/**
		 * 	Between the firstFreeSlot and the end of J9SimplePool header, the nodes should be either being used
		 * 	or should be in freelist. By using this logic, we can recalculate the number of elements being stored.
		 */
		if (verifies && (simplePool->numElements !=
				(((UDATA)firstFreeSlot - (UDATA)simplePool - sizeof(J9SimplePool)) / elementSize) - freeListCounter)
		) {
			Trc_simplepool_verify_numberOfElementsIncorrect(simplePool, simplePool->numElements, (((UDATA)firstFreeSlot - (UDATA)simplePool - sizeof(J9SimplePool)) / elementSize) - freeListCounter);
			verifies = FALSE;
		}

	} else {
		Trc_simplepool_verify_NullSimplePool();
	}

	Trc_simplepool_verify_Exit(verifies);

	return verifies;
}


/**
 * Clear the contents of a simple pool
 *
 * @note Make no assumptions about the contents of the simple pool after
 * invoking this method (it currently does not zero the memory)
 *
 * @param[in] simplePool The simple pool to clear
 *
 * @return void
 */
void
simplepool_clear(J9SimplePool *simplePool)
{
	Trc_simplepool_clear_Entry(simplePool);

	if (NULL == simplePool) {
		Trc_simplepool_clear_NullParameters();
	} else {
		simplePool->numElements = 0;
		SRP_SET_TO_NULL(simplePool->freeList);
		SRP_SET(simplePool->firstFreeSlot, SP_FIRST_POSSIBLE_ELEMENT_SLOT(simplePool)); /* address of first element */
	}

	Trc_simplepool_clear_Exit();
}


/**
 *	Calls a user provided function for each element in the simple pool skipping over
 *	skipCount allocated elements each time. If the function returns FALSE, the iteration
 *	stops.
 *
 * @param[in] simplePool	The simple pool to check for consistency
 * @param[in] portLib		A pointer to the port library, (this is required for j9mem_allocate/free functions)
 * @param[in] doFunction	Pointer to function which will "do" things to the elements of simple pool. Return TRUE
 * 							to continue iterating, FALSE to stop iterating.
 * @param[in] userData		Pointer to data to be passed to "do" function, along with each simple pool element
 * @param[in] skipCount		Number of elements to skip after each iteration, 0 means every element will be iterated over
 *
 * @return BOOLEAN	Returns FALSE if the doFunction returns FALSE for any element, TRUE otherwise
 */
BOOLEAN
simplepool_checkConsistency(J9SimplePool* simplePool, J9PortLibrary *portLib, BOOLEAN (*doFunction) (void* anElement, void* userData), void* userData, UDATA skipCount)
{
	BOOLEAN rc = TRUE;

	Trc_simplepool_checkConsistency_Entry(simplePool, doFunction, userData, skipCount);

	if (! (simplePool && doFunction && portLib)) {
		Trc_simplepool_checkConsistency_NullParameters();
	} else {
		rc = iterateOverSimplePoolElements(simplePool, portLib, doFunction, userData, skipCount);
	}

	Trc_simplepool_checkConsistency_Exit(rc);
	
	return rc;
}

/**
 * Calculates the size of J9SimplePool header size.
 * @return 		Header size
 */
U_32
simplepool_headerSize(void)
{
	return sizeof(J9SimplePool);
}
