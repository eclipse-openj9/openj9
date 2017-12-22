/*******************************************************************************
 * Copyright (c) 1991, 2015 IBM Corp. and others
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

/*
 * $RCSfile: j9heapTest.c,v $
 * $Revision: 1.27 $
 * $Date: 2011-12-30 14:15:00 $
 */

/**
 * @file
 * @ingroup PortTest
 * @brief Verify port library heap management.
 * 
 * Exercise the API for port library memory management operations.  These functions 
 * can be found in the file @ref j9mem.c  
 * 
 * @note port library memory management operations are not optional in the port library table.
 * 
 */
#include <stdlib.h>
#include <string.h>
#include "j9cfg.h"

#include "testHelpers.h"
#include "j9port.h"
#include "j9pool.h"
#include "pool_api.h"
#include "omrutil.h"

#undef HEAPTEST_VERBOSE

/** 
 * The amount of metadata used for heap management in bytes.
 * We operate on the internal knowledge of sizeof(J9Heap) = 2*sizeof(UDATA), i.e. white box testing.
 * This information is required for us to walk the heap and check its integrity after an allocation or free.
 */
struct J9Heap{
	UDATA heapSize; /* total size of the heap in number of slots */
	UDATA firstFreeBlock; /* slot number of the first free block within the heap */
	UDATA lastAllocSlot; /* slot number for the last allocation */
	UDATA largestAllocSizeVisited; /* largest free list entry visited while performing the last allocation */
};

#define NON_J9HEAP_HEAP_OVERHEAD 2
#define SIZE_OF_J9HEAP_HEADER (sizeof(J9Heap))
#define HEAP_MANAGEMENT_OVERHEAD (SIZE_OF_J9HEAP_HEADER + NON_J9HEAP_HEAP_OVERHEAD*sizeof(U_64))
#define MINIMUM_HEAP_SIZE (HEAP_MANAGEMENT_OVERHEAD + sizeof(U_64))

typedef struct AllocListElement {
	UDATA allocSize;
	void *allocPtr;
	UDATA allocSerialNumber;
} AllocListElement;

static void walkHeap(struct J9PortLibrary *portLibrary, J9Heap *heapBase, const char *testName);
static void verifySubAllocMem(struct J9PortLibrary *portLibrary, void *subAllocMem, UDATA allocSize, J9Heap *heapBase, const char *testName);
static void iteratePool(struct J9PortLibrary *portLibrary, J9Pool *allocPool);
static void* removeItemFromPool(struct J9PortLibrary *portLibrary, J9Pool *allocPool, UDATA removeIndex);
static AllocListElement* getElementFromPool(struct J9PortLibrary *portLibrary, J9Pool *allocPool, UDATA index);
static UDATA allocLargestChunkPossible(struct J9PortLibrary *portLibrary, J9Heap *heapBase, UDATA heapSize);
static void freeRemainingElementsInPool(struct J9PortLibrary *portLibrary, J9Heap *heapBase, J9Pool *allocPool);
static void verifyHeapOutofRegionWrite(struct J9PortLibrary *portLibrary, U_8 *memAllocStart, U_8 *heapEnd, UDATA heapStartOffset, const char *testName);

/**
 * Verify port library heap sub-allocator.
 * 
 * Ensure the port library is properly setup to run heap operations.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9heap_test0(struct J9PortLibrary *portLibrary)
{
	const char* testName = "j9heap_test0";
	PORT_ACCESS_FROM_PORT(portLibrary);
	reportTestEntry(portLibrary, testName);
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->heap_create) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->heap_create is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->heap_allocate) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->heap_allocate is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->heap_free) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->heap_free is NULL\n");
	}

	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->heap_reallocate) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->heap_reallocate is NULL\n");
	}
	
	if (NULL == OMRPORT_FROM_J9PORT(PORTLIB)->heap_query_size) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "portLibrary->heap_query_size is NULL\n");
	}

	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library heap sub-allocator.
 * 
 * Ensure basic function of heap API. This test allocates 2MB of memory, and fill the memory with 1's. 
 * It then enters a loop that calls j9heap_create on each of the first 16 byte-aligned locations with a heap size of 1MB. 
 * After each j9heap_create, we perform a series of heap_alloc and heap_free from size 0 to the maximum possible.
 * And finally, we check the bytes after the heap memory to see if we have overwrote any of them.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9heap_test1(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9heap_test1";
	
	/*malloc 2MB as backing storage for the heap*/
	UDATA memAllocAmount = 2*1024*1024;
	U_8 *allocPtr = NULL;
	UDATA heapStartOffset = 50;
	UDATA heapSize[] = {10, 30, 100, 200, 1000, 1*1024*1024};
	UDATA i;
	UDATA j;
	
	reportTestEntry(portLibrary, testName);

	/* Allocate backing storage using mem_allocate and pass in the returned pointer and the allocated size  
	 * Verify heap integrity after each heap operation
	 */
	allocPtr = j9mem_allocate_memory(memAllocAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == allocPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate %zu bytes for the heap\n", memAllocAmount);
		goto exit;
	} else {
		outputComment(portLibrary, "allocPtr: 0x%p\n", allocPtr);
	}
	
	for (j=0; j<(sizeof(heapSize)/sizeof(heapSize[0])); j++) {
		outputComment(portLibrary, "test for heap size %zu bytes\n", heapSize[j]);
		for (i=0; i<16; i++) {
			/*we leave the first <heapStartOffset> bytes alone so that we can check if they are corrupted by heap operations*/
			U_8 *allocPtrAdjusted = allocPtr+(i+heapStartOffset); 
			UDATA subAllocSize = 0;
			void *subAllocPtr = NULL;
			J9Heap *heapBase;
			
			memset(allocPtr, 0xff, memAllocAmount);	
			heapBase = j9heap_create(allocPtrAdjusted, heapSize[j], 0);
			if (((NULL == heapBase)&&heapSize[j]>MINIMUM_HEAP_SIZE) || ((NULL != heapBase)&&heapSize[j]<MINIMUM_HEAP_SIZE)) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to create heap at %p with size %zu\n", allocPtrAdjusted, heapSize[j]);
				goto exit;
			}
			if (NULL == heapBase) {
				break;
			}
			do{
				subAllocPtr = j9heap_allocate(heapBase, subAllocSize);
				if (subAllocPtr) {
					UDATA querySize = j9heap_query_size(heapBase, subAllocPtr);
					/*The size returned may have been rounded-up to the nearest U64, and may have an extra 2 slots added in some circumstances*/
					if (querySize < subAllocSize || querySize > (subAllocSize + 3 * sizeof(U_64))) {
						outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_query_size returned the wrong size. Expected %zu, got %zu\n", subAllocSize, querySize);
						j9heap_free(heapBase, subAllocPtr);
						break;
					}
				}
				walkHeap(portLibrary, heapBase, testName);
				j9heap_free(heapBase, subAllocPtr);
				walkHeap(portLibrary, heapBase, testName);
				subAllocSize  += 1;
			}while(NULL != subAllocPtr);
			
			verifyHeapOutofRegionWrite(portLibrary, allocPtr, allocPtrAdjusted+heapSize[j], heapStartOffset+i, testName);
		}
	}
	j9mem_free_memory(allocPtr);
	
exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library heap sub-allocator.
 * 
 * This is a more exhaustive test that performs random allocations and frees in a fairly large heap.
 * We go into a loop of heap_allocate and heap_free, the size of alloc is based on the random value (i.e. [0, allocSizeBoundary]).
 * We store the returned address, request size, etc in a sub-allocated memory identifier, which is stored in a J9Pool.
 * When we do a heap_free, we randomly pick an element from the pool and free the memory based on the values stored in the identifier. 
 * We keep a separate counter for number of heap_allocate and heap_free calls, and we terminate the loop when heap_free count reaches a certain value e.g. 500,000.
 * 
 * As an additional test for memory leak. We determine the largest amount of memory we can allocate before we enter the loop of heap_alloc/heap_free. 
 * After the loop is terminated, we free any remaining memory chunks in the pool and again allocate the largest possible size.
 * If this allocation is no longer successful, it means we have leaked memory on the heap during our loop execution.
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return TEST_PASS on success, TEST_FAIL on error 
 */
int
j9heap_test2(struct J9PortLibrary *portLibrary, int randomSeed)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9heap_test2";
	UDATA allocSerialNumber = 0;
	UDATA freeSerialNumber = 0;
	UDATA reallocSerialNumber = 0;
	UDATA heapSize = 10*1024*1024;
	UDATA serialNumberTop = 100000;
	UDATA allocSizeBoundary = heapSize/2;
	
	UDATA largestAllocSize = 0;
	J9Heap *allocHeapPtr = NULL;
	J9Heap *heapBase = NULL;
	void *subAllocMem;
	UDATA outputInterval = 1;
	J9Pool *allocPool;
	
	reportTestEntry(portLibrary, testName);
	
	allocPool = pool_new(sizeof(AllocListElement),  0, 0, 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_PORT_LIBRARY, POOL_FOR_PORT(portLibrary));
	if (NULL == allocPool) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate pool\n", heapSize);
		goto exit;
	}
	
	allocHeapPtr = j9mem_allocate_memory(heapSize, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == allocHeapPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate %zu bytes for the heap\n", heapSize);
		goto exit;
	}
	heapBase = j9heap_create(allocHeapPtr, heapSize, 0);
	if (NULL == heapBase) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "failed to create the heap\n");
		goto exit;
	}
	walkHeap(portLibrary, heapBase, testName);

	/*attempt to allocate the largest possible chunk from the heap*/
	largestAllocSize = allocLargestChunkPossible(portLibrary, heapBase, heapSize);
	if (0 == largestAllocSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "failed to allocate the largest chunk\n");
		goto exit;
	}
	outputComment(portLibrary, "Largest possible chunk size: %zu bytes\n", largestAllocSize);			
	walkHeap(portLibrary, heapBase, testName);
	
	if (0 == randomSeed) {
		randomSeed = (int) j9time_current_time_millis();
	}
	outputComment(portLibrary, "Random seed value: %d. Add -srand:[seed] to the command line to reproduce this test manually.\n", randomSeed);
	srand(randomSeed);
	
	while (freeSerialNumber <= serialNumberTop) {
		UDATA allocSize = 0;
		BOOLEAN isHeapFull = FALSE;
		AllocListElement *allocElement;
		UDATA operation = (UDATA)rand()%3;

		if (0 == operation) {
			allocSize = rand()%allocSizeBoundary;
			while((subAllocMem=j9heap_allocate(heapBase, allocSize)) == NULL) {
				/*outputComment(portLibrary, "**Failed j9heap_allocate, sizeRequested=%d\n", allocSize);*/  
				allocSize = allocSize*3/4;
				if (0 == allocSize) {
					/* No space left on the heap, go to next loop	j9mem_free_memory(allocHeapPtr);
					 *  and hope we do a j9heap_free soon. */
					isHeapFull = TRUE;
					break;
				}
			}
			if (isHeapFull) {
				continue;
			}
			
			allocSerialNumber+=1;

			allocElement = pool_newElement(allocPool);
			allocElement->allocPtr = subAllocMem;
			allocElement->allocSize = allocSize;
			allocElement->allocSerialNumber = allocSerialNumber;
			
#if defined(HEAPTEST_VERBOSE)
			if (0 == allocSerialNumber%outputInterval) {
				outputComment(portLibrary, "Alloc: size=%zu, allocSerialNumber=%zu, elementCount=%zu\n", allocSize, allocSerialNumber, pool_numElements(allocPool));
			}
#endif
			verifySubAllocMem(portLibrary, subAllocMem, allocSize, heapBase, testName);
			walkHeap(portLibrary, heapBase, testName);
			iteratePool(portLibrary, allocPool);
		} else if (1 == operation) {
			UDATA reallocIndex;
			if (0 == pool_numElements(allocPool)) {
				continue;
			}
			reallocIndex = rand()%pool_numElements(allocPool);
			allocElement = getElementFromPool(portLibrary, allocPool, reallocIndex);

			if (rand() & 1) {
				allocSize = allocElement->allocSize - 8 + (rand()%16);
			} else {
				allocSize = rand()%allocSizeBoundary;
			}
			subAllocMem = j9heap_reallocate(heapBase, allocElement->allocPtr, allocSize);
			reallocSerialNumber+=1;

			if (NULL != subAllocMem) {
				allocElement->allocPtr = subAllocMem;
				allocElement->allocSize = allocSize;
			}

#if defined(HEAPTEST_VERBOSE)
			if (0 == reallocSerialNumber%outputInterval) {
				outputComment(portLibrary, "Realloc: index=%zu, size=%zu, result=%p, reallocSerialNumber=%zu, elementCount=%zu\n",
					reallocIndex, allocSize, subAllocMem, freeSerialNumber, pool_numElements(allocPool));
			}
#endif
			walkHeap(portLibrary, heapBase, testName);
		} else {
			UDATA removeIndex;
			void *subAllocPtr;
			if (0 == pool_numElements(allocPool)) {
				continue;
			}
			removeIndex = rand()%pool_numElements(allocPool);
			
			subAllocPtr = removeItemFromPool(portLibrary, allocPool, removeIndex);
			
			freeSerialNumber+=1;
			
			j9heap_free(heapBase, subAllocPtr);
#if defined(HEAPTEST_VERBOSE)
			if (0 == freeSerialNumber%outputInterval) {
				outputComment(portLibrary, "Freed: index=%zu, freeSerialNumber=%zu, elementCount=%zu\n", removeIndex, freeSerialNumber, pool_numElements(allocPool));
			}
#endif
			walkHeap(portLibrary, heapBase, testName);
			iteratePool(portLibrary, allocPool);
		}
	}
	
	freeRemainingElementsInPool(portLibrary, heapBase, allocPool);
	walkHeap(portLibrary, heapBase, testName);
	if (NULL == j9heap_allocate(heapBase, largestAllocSize)){
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Possible memory leak on the heap: "
				"we cannot allocate the previously determined largest chunk size after sequence of random allocate/free\n");
		goto exit;
	}
	walkHeap(portLibrary, heapBase, testName);
	
exit:
	j9mem_free_memory(allocHeapPtr);
	pool_kill(allocPool);
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library heap sub-allocator.
 *
 * Tests for integer overflow handling.
 *
 * Ensure the port library is properly setup to run heap operations.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9heap_test3(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9heap_test3";

	/*malloc 1KB as backing storage for the heap*/
	UDATA memAllocAmount = 1024;
	U_8 *allocPtr, *subAllocPtr;
	UDATA i;

	J9Heap *heapBase;

	reportTestEntry(portLibrary, testName);

	/* Allocate backing storage using mem_allocate and pass in the returned pointer and the allocated size
	 * Verify heap integrity after each heap operation
	 */
	allocPtr = j9mem_allocate_memory(memAllocAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == allocPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate %zu bytes for the heap\n", memAllocAmount);
		goto exit;
	}

	heapBase = j9heap_create(allocPtr, memAllocAmount, 0);
	if (NULL == allocPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_create() failed!\n");
		goto exit;
	}

	for (i = 0; i < sizeof(U_64); i++) {
		subAllocPtr = j9heap_allocate(heapBase, UDATA_MAX-i);
		if (NULL != subAllocPtr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_allocate(%u) returned non-NULL!\n", UDATA_MAX-i);
			goto exit;
		}
	}
	walkHeap(portLibrary, heapBase, testName);

	subAllocPtr = j9heap_allocate(heapBase, 128);
	for (i = 0; i < sizeof(U_64); i++) {
		subAllocPtr = j9heap_reallocate(heapBase, subAllocPtr, UDATA_MAX-i);
		if (NULL != subAllocPtr) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_reallocate(%u) returned non-NULL!\n", UDATA_MAX-i);
			goto exit;
		}
	}
	walkHeap(portLibrary, heapBase, testName);

	j9mem_free_memory(allocPtr);

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library heap sub-allocator.
 *
 * Tests that j9heap_reallocate() works as expected.
 *
 * Ensure the port library is properly setup to run heap operations.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9heap_realloc_test0(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9heap_realloc_test0";

	/*malloc 2MB as backing storage for the heap*/
	UDATA memAllocAmount = 1024;
	U_8 *allocPtr, *subAllocPtr, *subAllocPtrSaved, *subAllocPtr2;
	IDATA i;

	J9Heap *heapBase;

	reportTestEntry(portLibrary, testName);

	/* Allocate backing storage using mem_allocate and pass in the returned pointer and the allocated size
	 * Verify heap integrity after each heap operation
	 */
	allocPtr = j9mem_allocate_memory(memAllocAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == allocPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate %zu bytes for the heap\n", memAllocAmount);
		goto exit;
	}

	heapBase = j9heap_create(allocPtr, memAllocAmount, 0);
	if (NULL == allocPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_create() failed!\n");
		goto exit;
	}

	subAllocPtr = j9heap_allocate(heapBase, 128);
	if (NULL == subAllocPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_allocate() failed!\n");
		goto exit;
	}
	walkHeap(portLibrary, heapBase, testName);
	subAllocPtrSaved = subAllocPtr;
	for (i = 0; i < 128; i++) {
		subAllocPtr[i] = (U_8)i;
	}
	subAllocPtr = j9heap_reallocate(heapBase, subAllocPtr, 256);
	walkHeap(portLibrary, heapBase, testName);
	if (subAllocPtr != subAllocPtrSaved) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected (subAllocPtr (%p) != subAllocPtrSaved (%p))\n",
			subAllocPtr, subAllocPtrSaved);
		goto exit;
	}
	for (i = 0; i < 128; i++) {
		if (subAllocPtr[i] != i) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected (subAllocPtr[i] (%d) != i (%d))\n",
				subAllocPtr[i], i);
			goto exit;
		}
	}
	subAllocPtr2 = j9heap_allocate(heapBase, 512);
	walkHeap(portLibrary, heapBase, testName);
	if (NULL != j9heap_reallocate(heapBase, subAllocPtr, 512)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected j9heap_reallocate() succeeded\n");
		goto exit;
	}
	walkHeap(portLibrary, heapBase, testName);
	j9heap_free(heapBase, subAllocPtr2);
	walkHeap(portLibrary, heapBase, testName);
	subAllocPtr2 = j9heap_reallocate(heapBase, subAllocPtr, 512);
	if (subAllocPtr2 != subAllocPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Unexpected j9heap_reallocate() failed\n");
		goto exit;
	}
	j9mem_free_memory(allocPtr);

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * Verify port library heap grow functionality.
 *
 * Tests that j9heap_grow() works as expected.
 *
 * Ensure the port library is properly setup to run heap operations.
 *
 * @param[in] portLibrary The port library under test
 *
 * @return TEST_PASS on success, TEST_FAIL on error
 */
int
j9heap_grow_test(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9heap_grow_test";

	/*malloc 2K as backing storage for the heap*/
	UDATA memAllocAmount = 2*1024;
	/* Initial heap will use 1K */
	UDATA initialHeapAmount = 1024;
	UDATA initialFirstFreeBlockValue = (sizeof(J9Heap)/sizeof(U_64));
	UDATA initialNumberOfSlots;
	UDATA firstFreeBlock;
	UDATA heapSize;
	I_64 headTailValue;
	U_64* baseSlot;
	J9Heap *heapBase;
	U_8 *allocPtr, *subAllocPtr1, *subAllocPtr2, *subAllocPtr3;
	IDATA i;

	reportTestEntry(portLibrary, testName);

	/* Allocate backing storage using mem_allocate and pass in the returned pointer and the allocated size
	 * Verify heap integrity after each heap operation
	 */
	allocPtr = j9mem_allocate_memory(memAllocAmount, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == allocPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate %zu bytes for the heap \n", memAllocAmount);
		goto exit;
	}

	heapBase = j9heap_create(allocPtr, initialHeapAmount, 0);
	if (NULL == heapBase) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_create() failed! \n");
		goto exit;
	}

	initialNumberOfSlots = initialHeapAmount/sizeof(U_64) - initialFirstFreeBlockValue;

	baseSlot = (U_64*)heapBase;
    /* test the integrity of the heap */
	walkHeap(portLibrary, heapBase, testName);

	/*
	 * struct J9Heap{
	 *		UDATA heapSize;  total size of the heap in number of slots
     *  	UDATA firstFreeBlock;  slot number of the first free block within the heap
     *  }
	 *
	 * HeapSize is represented by the number of slots in it. Each slot is the size of U_64 which is 8 bytes.
	 * So expected heapSize is 1024/8 = 128 slots
	 * Initially when heap is created, 2*sizeof(UDATA) of the slots are used for heap header.
	 * So 1 slot is used for header on 32 bit platforms, 2 slots are used on 64 bit platforms.
	 * For each block of memory, either allocated or free, there is a head and tail slots.
	 * If the value in tail is negative, then it means that the block is allocated.
	 * If it is free, then it means that the block is free.
	 * Absolute value of the integer in head and tail show how many slots there are BETWEEN head and tail slots.
	 * So when j9heap created initially, firstFreeBlock always has the value 2(on 64bit) or 1 (on 32bit) and heapSize should be 128 slots in this test heap.
	 * And firstFreeBlock should point to the first slot after header
	 * which should have value 128 - sizeof(j9heap)/sizeof(U_64) - 2(head and tail of the slot) = 124 (on 64bit) or 125 (on 32bit)
	 *
	 *
	 * |--------|-----|--------------------------------------------------------------------------------------------------------
	 * |heapSize|first|    |    |																						 |	  |
	 * |		|Free |	124|    | ....................................all heap is free...................................| 124|
	 * |		|Block|	   |    |																						 |    |
	 * |--------|-----|-------------------------------------------------------------------------------------------------------
	 * 0        1     2	   3	4  																						 127  128
     *     128    1|2
     *          (32|64bit)
	 *
	 */
	if (128 != heapBase->heapSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_create failed to set the heap->heapSize correctly. Expected :  128, Found : %zu \n", heapBase->heapSize);
		goto exit;
	}

	if (initialFirstFreeBlockValue != heapBase->firstFreeBlock) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_create failed to set the heap->firstFreeBlock correctly. Expected :  %zu, Found : %zu \n", initialFirstFreeBlockValue, heapBase->firstFreeBlock);
		goto exit;
	}

	/* 2 slots are being used for the head and tail of the free block */
	if (initialNumberOfSlots - 2 != baseSlot[heapBase->firstFreeBlock]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_allocate failed to set the firstFreeBlock's head value correctly. Expected :  %zu, Found : %zd \n", initialNumberOfSlots - 2, baseSlot[heapBase->firstFreeBlock]);
		goto exit;
	}

	/*
	 * Heap is created successfully.
	 * Currently there are 128 - 2 (j9header) = 126 Slots free.
	 *
	 * + in the slot means that it is occupied/allocated by heap.
	 *
	 * 1. Allocate 64 bytes (8 slots). (Consumed 8 + 1 head + 1 tail = 10 slots) (Remaining free = 126 - 10 = 116)
	 * |--------|-----|--------------------------------------------------------------------------------------------------------
	 * |heapSize|first|    |    |		|    |	 |																		 |	  |
	 * |		|Free |	-8 |  + | ++++++| -8 |114|.......................................................................| 114|
	 * |		|Block|	   |    |		|    |	 |																		 |    |
	 * |--------|-----|-------------------------------------------------------------------------------------------------------
	 * 0        1     2    3    4       11   12                                                                         127   128
     *     128   11|12
     *          (32|64bit)
	 */
	subAllocPtr1 = j9heap_allocate(heapBase, 64);
	if (NULL == subAllocPtr1) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_allocate() failed!\n");
		goto exit;
	}
    /* test the integrity of the heap */
	walkHeap(portLibrary, heapBase, testName);

	if (128 != heapBase->heapSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_allocate corrupted heapBase->heapSize. Expected :  128, Found : %zu \n", heapBase->heapSize);
		goto exit;
	}

	if (initialFirstFreeBlockValue + 10 != heapBase->firstFreeBlock) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_allocate failed to set the heap->firstFreeBlock correctly. Expected :  %zu, Found : %zu \n", initialFirstFreeBlockValue + 10, heapBase->firstFreeBlock);
		goto exit;
	}

	if (initialNumberOfSlots - 12 != baseSlot[heapBase->firstFreeBlock]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_allocate failed to set the firstFreeBlock's head value correctly. Expected :  %zu, Found : %zd \n", initialNumberOfSlots - 10, baseSlot[heapBase->firstFreeBlock]);
		goto exit;
	}

	 /* 2. Allocate the rest of the heap and make it full. So (initialNumberOfSlots-12(used above)-2(head and tail))*8 bytes need to be allocated
	 * |--------|-----|--------------------------------------------------------------------------------------------------------
	 * |heapSize|first|    |    |		|    |	  |																		 |	  |
	 * |		|Free |	-8 |  + | ++++++| -8 |-114|++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|-114|
	 * |		|Block|	   |    |		|    |	  |																		 |    |
	 * |--------|-----|-------------------------------------------------------------------------------------------------------
	 * 0        1     2    3    4       11   12																			127   128
     *     128     0
	 */

	subAllocPtr2 = j9heap_allocate(heapBase, (initialNumberOfSlots-14)*8);
	if (NULL == subAllocPtr2) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_allocate() failed!\n");
		goto exit;
	}
    /* test the integrity of the heap */
	walkHeap(portLibrary, heapBase, testName);

	if (128 != heapBase->heapSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_allocate corrupted heapBase->heapSize. Expected :  128, Found : %zu \n", heapBase->heapSize);
		goto exit;
	}

	/* firstFreeBlock is set to 0 once the heap is completely full */
	if (0 != heapBase->firstFreeBlock) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_allocate failed to set the heap->firstFreeBlock correctly. Expected :  0, Found : %zu \n", heapBase->firstFreeBlock);
		goto exit;
	}

	/*
	 * 3. Try to allocate more memory from the full heap and
	 * make sure returned pointer is NULL meaning that heap_allocate failed
	 *
	 */

	subAllocPtr3 = j9heap_allocate(heapBase, 912);
	if (NULL != subAllocPtr3) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_allocate() failed!\n");
		goto exit;
	}
    /* test the integrity of the heap */
	walkHeap(portLibrary, heapBase, testName);

	if (128 != heapBase->heapSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_allocate corrupted heapBase->heapSize. Expected :  128, Found : %zu \n", heapBase->heapSize);
		goto exit;
	}

	/* firstFreeBlock is set to 0 once the heap is completely full */
	if (0 != heapBase->firstFreeBlock) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_allocate failed to set the heap->firstFreeBlock correctly. Expected :  0, Found : %zu \n", heapBase->firstFreeBlock);
		goto exit;
	}

	 /* 4. Try to add 10 slots to the heap
	 * |--------|-----|--------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first|    |    |		|    |	  |																 |	  |    |   				|	 |
	 * |		|Free |	-8 |  + | ++++++| -8 |-114|++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++|-114|  8 |................| 8  |
	 * |		|Block|	   |    |		|    |	  |																 |    |    | 				|	 |
	 * |--------|-----|--------------------------------------------------------------------------------------------------------------------------
	 * 0        1     2    3    4       11   12	  13											     			127   128  129              137  138
     *    138     128
     *
     *  Since the heap was completely full, growing should update following:
     *  a. firstFreeBlock should be set to 128 which points to the beginning of newly added slots.
     *  b. heapSize should be incremented 10 and be set to 138
     *  c. The head of newly added free block of slots should have the value 8. Since 10 slots are added, 2 slots are used for head and tail.
	 */

	if (TRUE != j9heap_grow(heapBase, 80)) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_grow() failed!\n");
		goto exit;
	}

	/* test the integrity of the heap */
	walkHeap(portLibrary, heapBase, testName);

	if (138 != heapBase->heapSize) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_allocate corrupted heapBase->heapSize. Expected :  138, Found : %zu", heapBase->heapSize);
		goto exit;
	}

	if (128 != heapBase->firstFreeBlock) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_grow() failed to set firstFreeBlock correctly. Expected value = 128, Found : %zu \n", heapBase->firstFreeBlock);
		goto exit;
	}

	if (8 != baseSlot[heapBase->firstFreeBlock]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_allocate failed to set the firstFreeBlock's head value correctly. Expected :  8, Found : %zd", baseSlot[heapBase->firstFreeBlock]);
		goto exit;
	}

	if (8 != baseSlot[heapBase->heapSize - 1]) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "heap_allocate failed to set the firstFreeBlock's tail value correctly. Expected :  8, Found : %zd", baseSlot[heapBase->firstFreeBlock]);
		goto exit;
	}

	/* Recursively grow heap.
	 * Since the last slots of the heap has just added and they are empty,
	 * it is expected that newly added slots should merge with the current heap's last slots.
	 * In this case, here is what is expected:
	 * firstFreeBlock never changes value.
	 * j9heap->heapSize size grows
	 * firstFreeBlock points to the head of last slots and the value of head changes as we grow heap. So do tail.
	 *
	 */
	firstFreeBlock = heapBase->firstFreeBlock;
	headTailValue = baseSlot[heapBase->firstFreeBlock];
	heapSize = heapBase->heapSize;
	for (i = 0; i < 20; i++ ) {
		/* 40 bytes = 5 slots*/
		if (TRUE != j9heap_grow(heapBase, 40)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_grow() failed!\n");
			goto exit;
		}

		/* test the integrity of the heap */
		walkHeap(portLibrary, heapBase, testName);

		if (firstFreeBlock != heapBase->firstFreeBlock) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_grow() failed!\n");
			goto exit;
		}

		if (heapBase->heapSize != heapSize + (i+1)*5) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_grow() failed!\n");
			goto exit;
		}

		if (baseSlot[heapBase->firstFreeBlock] != headTailValue + (i+1)*5) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_grow() failed!\n");
			goto exit;
		}

		if (baseSlot[heapBase->heapSize -1 ] != headTailValue + (i+1)*5) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "j9heap_grow() failed!\n");
			goto exit;
		}
	}


	j9mem_free_memory(allocPtr);

exit:
	return reportTestExit(portLibrary, testName);
}

/**
 * For details:
 * https://jazz103.hursley.ibm.com:9443/jazz/resource/itemName/com.ibm.team.workitem.WorkItem/72449
 * https://jazz103.hursley.ibm.com:9443/jazz/resource/itemName/com.ibm.team.workitem.WorkItem/71328
 *
 * In order to create the scenario in this PMR, heap's different segments need to be allocated and freed at a certain order.
 *
 * This is the scenario where lastAllocSlot is not updated correctly and it points to somewhere in the middle of a segment and has the value of 0.
 *
 *
 */
int
j9heap_test_pmr_28277_999_760(struct J9PortLibrary *portLibrary)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	const char* testName = "j9heap_test_pmr_28277_999_760";

	UDATA heapSize = 800; /* Big enough */
	J9Heap *heapBase;
	U_8 *allocPtr = NULL;

	void *alloc0 = NULL;
	void *alloc1 = NULL;
	void *alloc2 = NULL;
	void *alloc3 = NULL;
	void *alloc4_1 = NULL;
	void *alloc4_2 = NULL;
	void *alloc5 = NULL;
	void *alloc6 = NULL;
	void *alloc7 = NULL;

	reportTestEntry(portLibrary, testName);

	allocPtr = j9mem_allocate_memory(heapSize, OMRMEM_CATEGORY_PORT_LIBRARY);
	if (NULL == allocPtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Failed to allocate %zu bytes for the heap\n", heapSize);
		goto exit;
	} else {
		outputComment(portLibrary, "allocPtr: 0x%p\n", allocPtr);
	}

	memset(allocPtr, 0xff, heapSize);

	heapBase = j9heap_create(allocPtr, heapSize, 0);

	 /*
	  * struct J9Heap{
      *  	UDATA heapSize;
      *  	UDATA firstFreeBlock;
      *  	UDATA lastAllocSlot;
      *  	UDATA largestAllocSizeVisited;
      *	};

	  * After heap is created.*
	  *
	  * |	    HEAP HEADER					   |
	  * |--------------------------------------|------------------------------------------------------------------------------------------------
	  * |heapSize|first   | last    | largest  |  |																							|	|
	  * |		 |Free    | Alloc	|  Alloc   |  |																							|	|
	  * |		 |Block   | Slot	|  Size    |94|			FREE			FREE			FREE					FREE					|94	|
	  * |		 |        |     	|  Visited |  |																							|	|
	  * |	100	 |  4     |  4   	|   94     |  |																							|	|
	  * |--------|--------|---------|----------|--|-----------------------------------------------------------------------------------------|---|
	  * 0        1        2         3          4  5     																					99		100
      *
      *FYI : Each segment is the size of U_64
      */


	if (NULL == heapBase) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "failed to create the heap\n");
		goto exit;
	}

	walkHeap(portLibrary, heapBase, testName);

	/*
	 * Following series of j9heap function calls is to create the scenario in the mentioned PMRs above.
	 *
	 * Illustration of the heap after every call is before this PMR fixed!!!
	 *
	 * With the fix, firstFree and lastAllocSlot values should point to the right segments of heap. This test verifies they do.
	 *
	 */

	alloc0 = j9heap_allocate(heapBase, 16); /* 16/8 = 2   Alloc 2 slots. Slot Size = sizeof(U_64) */
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|	|	|														  								     					|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|	|	|																	     										|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc0|-2	|90	|   FREE			FREE			FREE					FREE			FREE			FREE				|90	|
	 * |		|       |	   	|  Visited|	 |2Slots|	|	|																												|   |
	 * |	100	|  8    |  8    |   0     |	 |		|	|	|																												|   |
	 * |--------|-------|-------|---------|--|------|---|---|---------------------------------------------------------------------------------------------------------------|---|
	 * 0        1       2       3         4  5  	7	8	9																			99	100
     *
     */


	alloc1 = j9heap_allocate(heapBase, 16);  /* 16/8 = 2   Alloc 2 slots. Slot Size = sizeof(U_64) */
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |																									|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |															     									|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc0|-2|-2|Alloc1|-2|86|   FREE			FREE			FREE				FREE			FREE			FREE			|86	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |    																								|   |
	 * |	100	|  12   |  12   |   0     |	 |		|  |  |		 |  |  |																									|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|----------------------------------------------------------------------------------------------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11	12 13		     																						99	100
     *
     */


	alloc2 = j9heap_allocate(heapBase, 16); /* 16/8 = 2   Alloc 2 slots. Slot Size = sizeof(U_64) */
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |																						|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |												     									|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc0|-2|-2|Alloc1|-2|-2|Alloc2|-2|82|	FREE			FREE				FREE			FREE			FREE			|82	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  | 																						|   |
	 * |	100	|  16   |  16   |   0     |	 |		|  |  |		 |  |  |	  |  |  |																						|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|---------------------------------------------------------------------------------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17    																					99	100
     *
     */

	alloc3 = j9heap_allocate(heapBase, 16); /* 16/8 = 2   Alloc 2 slots. Slot Size = sizeof(U_64)*/
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |																			|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |												     						|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc0|-2|-2|Alloc1|-2|-2|Alloc2|-2|-2|Alloc |-2|78|	FREE			FREE				FREE			FREE		FREE	|78	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  |2Slots|  |  |											     							|   |
	 * |	100	|  20   |  20   |   0     |	 |		|  |  |		 |  |  |	  |  |  |      |  |  |																			|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|------|--|--|--------------------------------------------------------------------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17	  19 20 21																			99	100
     *
     */

	alloc4_1 = j9heap_allocate(heapBase, 64);  /* 64/8 = 8   Alloc 8 Slots. Slot Size = sizeof(U_64)*/
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                  	|  |  |												|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                 		|  |  |											 	|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc0|-2|-2|Alloc1|-2|-2|Alloc2|-2|-2|Alloc3|-2|-2|Alloc4_1         		|-2|68|			FREE				FREE			|68	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  |2Slots|  |  |8Slots            	|  |  |												|   |
	 * |	100	|  30   |  30   |   0     |	 |		|  |  |		 |  |  |	  |  |  |      |  |  |                  	|  |  |												|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|------|--|--|----------------------|--|--|---------------------------------------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17	  19 20 21	               		29 30 31										   99	100
     *
     */


	alloc5 = j9heap_allocate(heapBase, 16);  /* 16/8 = 2   Alloc 2 slots. Slot Size = sizeof(U_64) */
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                  	|  |  |      |  |  |								|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                  	|  |  |      |  |  |						     	|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc0|-2|-2|Alloc1|-2|-2|Alloc2|-2|-2|Alloc3|-2|-2|Alloc4_1          	|-2|-2|Alloc5|-2|64|				FREE			|64	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  |2Slots|  |  |8Slots            	|  |  |2Slots|  |  |					     		|   |
	 * |	100	|  34   |  34   |   0     |	 |		|  |  |		 |  |  |	  |  |  |      |  |  |                  	|  |  |      |  |  |								|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|------|--|--|----------------------|--|--|------|--|--|--------------------------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17	  19 20 21	               		29 30 31     33 34 35    							99	100
     *
     */

	alloc6 = j9heap_allocate(heapBase, 16);  /* 16/8 = 2   Alloc 2 slots. Slot Size = sizeof(U_64) */
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                  	|  |  |      |  |  |      |  |  |					|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                  	|  |  |      |  |  |      |  |  |			     	|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc0|-2|-2|Alloc1|-2|-2|Alloc2|-2|-2|Alloc3|-2|-2|Alloc4_1          	|-2|-2|Alloc5|-2|-2|Alloc6|-2|60|	FREE			|60	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  |2Slots|  |  |8Slots            	|  |  |2Slots|  |  |2Slots|  |  |		     		|   |
	 * |	100	|  38   |  38   |   0     |	 |		|  |  |		 |  |  |	  |  |  |      |  |  |                  	|  |  |      |  |  |      |  |  | 					|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|------|--|--|----------------------|--|--|------|--|--|------|--|--|-------------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17	  19 20 21	               		29 30 31     33 34 35     37 38 39					99	100
     *
     */

	walkHeap(portLibrary, heapBase, testName); /* Just a sanity check */

	j9heap_free(heapBase, alloc1);
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |              	    |  |  |      |  |  |      |  |  |					|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |              	    |  |  |      |  |  |      |  |  |			    	|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc0|-2| 2|FREE  | 2|-2|Alloc2|-2|-2|Alloc3|-2|-2|Alloc4_1      	    |-2|-2|Alloc5|-2|-2|Alloc6|-2|60|		FREE		|60	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  |2Slots|  |  |8Slots        	    |  |  |2Slots|  |  |2Slots|  |  |		     		|   |
	 * |	100	|   8   |   8   |   0     |	 |		|  |  |		 |  |  |	  |  |  |      |  |  |              	    |  |  |      |  |  |      |  |  |					|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|------|--|--|----------------------|--|--|------|--|--|------|--|--|-------------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17	  19 20 21	               		29 30 31     33 34 35     37 38 39   				99	100
     *
     */

	j9heap_free(heapBase, alloc4_1);
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                 		|  |  |      |  |  |      |  |  |					|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                  	|  |  |      |  |  |      |  |  |			     	|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc0|-2| 2|FREE  | 2|-2|Alloc2|-2|-2|Alloc3|-2|8 |FREE              	|8 |-2|Alloc |-2|-2|Alloc |-2|60|	FREE			|60	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  |2Slots|  |  |8slots            	|  |  |2Slots|  |  |2Slots|  |  |		     		|   |
	 * |	100	|   8   |   8   |   0     |	 |		|  |  |		 |  |  |	  |  |  |      |  |  |                  	|  |  |      |  |  |      |  |  |					|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|------|--|--|----------------------|--|--|------|--|--|------|--|--|-------------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17	  19 20 21	               		29 30 31     33 34 35     37 38 39					99	100
     *
     */

	alloc4_1 = j9heap_allocate(heapBase, 24);  /* 24/8 = 3   Alloc 3 slots. Slot Size = sizeof(U_64) */
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |        |  |  |        |  |  |      |  |  |      |  |  |					|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |        |  |  |        |  |  |      |  |  |      |  |  |			     	|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc	|-2| 2|FREE  | 2|-2|Alloc |-2|-2|Alloc |-2|-3|Alloc4_1|-3|3 |FREE    |3 |-2|Alloc |-2|-2|Alloc |-2|60|	FREE			|60	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  |2Slots|  |  | 3Slots |  |  |3Slots  |  |  |2Slots|  |  |2Slots|  |  |		     		|   |
	 * |	100	|   8   |   25  |   2     |	 |		|  |  |		 |  |  |	  |  |  |      |  |  |        |  |  |        |  |  |      |  |  |      |  |  |					|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|------|--|--|--------|--|--|--------|--|--|------|--|--|------|--|--|-- ---------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17	  19 20 21	     24 25 26        29 30 31     33 34 35   	37 38 39 				99	100
     *
     */

	alloc4_2 = j9heap_allocate(heapBase, 24);  /* 24/8 = 3   Alloc 3 slots. Slot Size = sizeof(U_64) */
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |        |  |  |        |  |  |      |  |  |      |  |  |					|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |        |  |  |        |  |  |      |  |  |      |  |  |			     	|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc	|-2| 2|FREE  | 2|-2|Alloc |-2|-2|Alloc |-2|-3|Alloc4_1|-3|-3|Alloc4_2|-3|-2|Alloc |-2|-2|Alloc |-2|60|	FREE			|60	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  |2Slots|  |  | 3Slots |  |  |3Slots  |  |  |2Slots|  |  |2Slots|  |  |		     		|   |
	 * |	100	|   8   |   25  |   2     |	 |		|  |  |		 |  |  |	  |  |  |      |  |  |        |  |  |        |  |  |      |  |  |      |  |  |					|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|------|--|--|--------|--|--|--------|--|--|------|--|--|------|--|--|-- ---------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17	  19 20 21	     24 25 26        29 30 31     33 34 35   	37 38 39 				99	100
     *
     */

	j9heap_free(heapBase, alloc4_1);
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |        |  |  |        |  |  |      |  |  |      |  |  |					|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |        |  |  |        |  |  |      |  |  |      |  |  |			     	|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc	|-2| 2|FREE  | 2|-2|Alloc |-2|-2|Alloc |-2| 3|FREE	  | 3|-3|Alloc4_2|-3|-2|Alloc |-2|-2|Alloc |-2|60|	FREE			|60	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  |2Slots|  |  | 3Slots |  |  |3Slots  |  |  |2Slots|  |  |2Slots|  |  |		     		|   |
	 * |	100	|   8   |   25  |   3     |	 |		|  |  |		 |  |  |	  |  |  |      |  |  |        |  |  |        |  |  |      |  |  |      |  |  |					|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|------|--|--|--------|--|--|--------|--|--|------|--|--|------|--|--|-- ---------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17	  19 20 21	     24 25 26        29 30 31     33 34 35   	37 38 39 				99	100
     *
     */

	j9heap_free(heapBase, alloc4_2);
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                 		|  |  |      |  |  |      |  |  |					|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                  	|  |  |      |  |  |      |  |  |			     	|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc0|-2| 2|FREE  | 2|-2|Alloc2|-2|-2|Alloc3|-2|8 |FREE              	|8 |-2|Alloc |-2|-2|Alloc |-2|60|	FREE			|60	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  |2Slots|  |  |8slots            	|  |  |2Slots|  |  |2Slots|  |  |		     		|   |
	 * |	100	|   8	|   25  |   8     |	 |		|  |  |		 |  |  |	  |  |  |      |  |  |                  	|  |  |      |  |  |      |  |  |					|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|------|--|--|----------------------|--|--|------|--|--|------|--|--|-------------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17	  19 20 21	               		29 30 31     33 34 35     37 38 39					99	100
     *
     *
     * PROBLEM: As you can see above, lastAllocSlot is not pointing to the beginning of alloc/free segment. It is in the middle of the free segment.
     *
     * If this segment is allocated again, and value is written in it, then lastAllocSlot will have random values. See below calls.
     *
     */


	alloc4_1 = j9heap_allocate(heapBase, 48);
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                 		|  |  |      |  |  |      |  |  |					|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                  	|  |  |      |  |  |      |  |  |			     	|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc0|-2| 2|FREE  | 2|-2|Alloc2|-2|-2|Alloc3|-2|-8|Alloc4_1            	|-8|-2|Alloc |-2|-2|Alloc |-2|60|	FREE			|60	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  |2Slots|  |  |8slots            	|  |  |2Slots|  |  |2Slots|  |  |		     		|   |
	 * |	100	|   8	|   25  |   8     |	 |		|  |  |		 |  |  |	  |  |  |      |  |  |                  	|  |  |      |  |  |      |  |  |					|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|------|--|--|----------------------|--|--|------|--|--|------|--|--|-------------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17	  19 20 21	               		29 30 31     33 34 35     37 38 39					99	100
     *
     */

	memset(alloc4_1, 0, 48);

	j9heap_free(heapBase, alloc4_1);
	/*
	 *
	 *
	 * |	    HEAP HEADER               |
	 * |----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------
	 * |heapSize|first  | last  | largest |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                 		|  |  |      |  |  |      |  |  |					|   |
	 * |		|Free   | Alloc |  Alloc  |	 |		|  |  |		 |  |  |	  |  |  |	   |  |  |                  	|  |  |      |  |  |      |  |  |			     	|   |
	 * |		|Block  | Slot	|  Size   |-2|Alloc0|-2| 2|FREE  | 2|-2|Alloc2|-2|-2|Alloc3|-2|8 |FREE              	|8 |-2|Alloc |-2|-2|Alloc |-2|60|	FREE			|60	|
	 * |		|       |	   	|  Visited|	 |2Slots|  |  |2Slots|  |  |2Slots|  |  |2Slots|  |  |8slots            	|  |  |2Slots|  |  |2Slots|  |  |		     		|   |
	 * |	100	|   8	|   25  |   8     |	 |		|  |  |		 |  |  |	  |  |  |      |  |  |                  	|  |  |      |  |  |      |  |  |					|   |
	 * |--------|-------|-------|---------|--|------|--|--|------|--|--|------|--|--|------|--|--|----------------------|--|--|------|--|--|------|--|--|-------------------|---|
	 * 0        1       2       3         4  5  	7  8  9	 	11 12 13     15 16 17	  19 20 21	               		29 30 31     33 34 35     37 38 39					99	100
     *
     */


	/*
	 * 100 bytes is big enough to start from lastAllocSlot  (100 > largestAllocSizeVisited)
	 * lastAllocSlot should be bogus and should have value 0 before the fix.
	 *
	 */
	 alloc7 = j9heap_allocate(heapBase, 100);

exit:
	j9mem_free_memory(allocPtr);
	return reportTestExit(portLibrary, testName);
}


/**
 * Helper function that iterates all used elements in the pool and call heap_free to free each memory blocks, whose information is stored in a pool element.
 */
static void
freeRemainingElementsInPool(struct J9PortLibrary *portLibrary, J9Heap *heapBase, J9Pool *allocPool)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	pool_state state;
	AllocListElement *element = pool_startDo(allocPool, &state);

	while(NULL != element){
		j9heap_free(heapBase, element->allocPtr);
		element = pool_nextDo(&state);
	}
}

/**
 * Helper function determines the largest size one can allocate in a empty J9Heap.
 * It tries to call heap_allocate with the size used to create the heap, and gradually decreases the size until an heap_allocate succeeds.
 * It returns the size of the largest possible allocation.
 */
static UDATA
allocLargestChunkPossible(struct J9PortLibrary *portLibrary, J9Heap *heapBase, UDATA heapSize)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	UDATA allocSize = heapSize;
	void *allocMem = NULL;
	
	do{
		allocMem = j9heap_allocate(heapBase, allocSize);
		if (NULL != allocMem) {
			j9heap_free(heapBase, allocMem);
			return allocSize;
		}
		allocSize-=1;
	}while(allocSize > 0);
	return 0;
}

/**
 * Helper function that removes a specific element from the pool.
 * The element to be removed is the removeIndex'th one when we iterate the used elements in pool.
 * Returns the AllocListElement->allocPtr stored in the pool element.
 */
static void*
removeItemFromPool(struct J9PortLibrary *portLibrary, J9Pool *allocPool, UDATA removeIndex)
{
	UDATA i = 0;
	pool_state state;
	void *subAllocPtr = NULL;
	AllocListElement *element = pool_startDo(allocPool, &state);
	
	for (i = 0; i < removeIndex; i++) {
		element = pool_nextDo(&state);
	}
	subAllocPtr = element->allocPtr;
	pool_removeElement(allocPool, element);
	
	return subAllocPtr;
}

/**
 * Helper function that gets a specific element from the pool.
 */
static AllocListElement*
getElementFromPool(struct J9PortLibrary *portLibrary, J9Pool *allocPool, UDATA index)
{
	UDATA i = 0;
	pool_state state;
	AllocListElement *element = pool_startDo(allocPool, &state);

	for (i = 0; i < index; i++) {
		element = pool_nextDo(&state);
	}

	return element;
}

/**
 * validate sub-allocated memory pointer returned by heap_allocate.
 */
static void
verifySubAllocMem(struct J9PortLibrary *portLibrary, void *subAllocMem, UDATA allocSize, J9Heap *heapBase, const char *testName)
{
	U_64 *basePtr = (U_64 *)heapBase;
	UDATA heapSize = ((UDATA *)basePtr)[0];
	I_64 *blockStart = (I_64 *)subAllocMem;
	I_64 blockSizeStart = blockStart[-1];
	I_64 blockSizeEnd = 0;
	
	/* verify the mem ptr is aligned */
	if (((UDATA)subAllocMem) & 7) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Sub-allocated memory is not 8-aligned: 0x%p\n", subAllocMem);
		return;
	}
	
	/* verify the mem ptr is within the range of heap */
	if ((subAllocMem < (void *)&basePtr[SIZE_OF_J9HEAP_HEADER/sizeof(U_64)]) || (subAllocMem > (void *)&basePtr[heapSize-1])) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Sub-allocated memory is not within the valid range of heap: 0x%p\n", subAllocMem);
		return;
	}
	
	/* verify the mem ptr is the first slot of an empty block and the block size should not be overly large */
	if (blockSizeStart >= 0) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Sub-allocated memory is not part of an occupied block: 0x%p, top size: %lld, bottom size: %lld\n", subAllocMem, blockSizeStart, blockSizeEnd);
		return;
	}
	blockSizeEnd = blockStart[-blockSizeStart];
	if (blockSizeStart != blockSizeEnd) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "Top and bottom padding of the Sub-allocated memory do not match: 0x%p\n", subAllocMem);
		return;
	}
	if (0 == allocSize) {
		if ( ! (1 <= -blockSizeStart && -blockSizeStart <= 3)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "The allocated block seems to be overly large (3 or more free slots): 0x%p, request size: %zu, block size: %lld\n", subAllocMem, allocSize, blockSizeStart);
			return;
		}
	} else {
		if ( ! (allocSize <= ((UDATA)(-blockSizeStart)*sizeof(U_64)) && ((UDATA)(-blockSizeStart)*sizeof(U_64)) < (allocSize+3*sizeof(U_64))) ) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "The allocated block seems to be overly large (3 or more free slots): 0x%p, request size: %zu, block size: %lld\n", subAllocMem, allocSize, blockSizeStart);
			return;
		}
	}
}

/**
 * helper function that iterates the used elements in a Pool and produces some verbose output of the information stored in AllocListElement. 
 */
static void
iteratePool(struct J9PortLibrary *portLibrary, J9Pool *allocPool)
{
#if defined(HEAPTEST_VERBOSE)
	pool_state state;
	AllocListElement *element;
	
	if(0 == pool_numElements(allocPool)){
		outputComment(portLibrary, "Pool has become empty");
	} else {
		element = pool_startDo(allocPool, &state);
		do{
			outputComment(portLibrary, "[%zu] ", element->allocSerialNumber);
			element = pool_nextDo(&state);
		}while(NULL != element);
	}
	outputComment(portLibrary, "\n\n");
#endif
}

/**
 * Verifies that we did not touch any of the memory immediately before and after the heap.
 * We check for memory location from memAllocStart to memAllocStart+heapStartOffset, which immediate precedes the heap.
 * Also check for the next 500 bytes after the heap.
 */
static void
verifyHeapOutofRegionWrite(struct J9PortLibrary *portLibrary, U_8 *memAllocStart, U_8 *heapEnd, UDATA heapStartOffset, const char *testName)
{
	UDATA i;
	
	/* ensure the memory location after the heap is not corrupted*/
	for (i=0; i<=500; i++) {
		if (0xff != *(heapEnd+i)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Address %p following the heap seems to have been modified. value: %x\n", heapEnd+i, *(heapEnd+i));
			return;
		}
	}
	/* ensure the memory location before the heap is not corrupted*/
	for (i=0; i<heapStartOffset; i++) {
		if (0xff != *(memAllocStart+i)) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "Address %p before the heap seems to have been modified. value: %x\n", memAllocStart+i, *(memAllocStart+i));
			return;
		}
	}
}

/*
 * The walkHeap routine knows the internal structure of the heap to better test its integrity 
 */
static void
walkHeap(struct J9PortLibrary *portLibrary, J9Heap *heapBase, const char *testName)
{
	U_64 *basePtr = (U_64 *)heapBase; 
	UDATA heapSize, firstFreeBlock;
	U_64 *lastSlot, *blockTopPaddingCursor;
	BOOLEAN firstFreeUnmatched = TRUE;
	
	if (NULL == basePtr) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\nHeap base is NULL\n");
		return;
	} else if (((UDATA)basePtr) & 7) {
		outputErrorMessage(PORTTEST_ERROR_ARGS, "\nHeap is not 8-aligned\n");
		return;
	}
	
	/*the fields of J9Heap is UDATA*/
	heapSize = ((UDATA *)basePtr)[0];
	firstFreeBlock = ((UDATA *)basePtr)[1];
	
	lastSlot = &basePtr[heapSize-1];
	blockTopPaddingCursor= &basePtr[SIZE_OF_J9HEAP_HEADER/sizeof(U_64)];
	
#if defined(HEAPTEST_VERBOSE)
	outputComment(portLibrary, "J9Heap @ 0x%p: ", heapBase);
	outputComment(portLibrary, "%zu|", heapSize);
	outputComment(portLibrary, "%zu|", firstFreeBlock);
#endif
	
	while ( ((UDATA)blockTopPaddingCursor) < ((UDATA)lastSlot) ) {
		I_64 topBlockSize, bottomBlockSize, absSize;
		U_64 *blockBottomPaddingCursor;
		
		absSize = topBlockSize = *blockTopPaddingCursor;
		
		if (0 == topBlockSize) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\nThe block size is 0 @ 0x%p\n", blockTopPaddingCursor);
			return;
		} else if (topBlockSize < 0) {
			absSize = -topBlockSize;
		} else {
			if ( 0 == firstFreeBlock ) {
				outputErrorMessage(PORTTEST_ERROR_ARGS, "\nfirstFreeBlock indicates heap is full but found empty block @ 0x%p\n", blockTopPaddingCursor);
				return;
			}
			if (firstFreeUnmatched == TRUE) {
				if (firstFreeBlock != (((UDATA)blockTopPaddingCursor-(UDATA)basePtr)/sizeof(U_64))) {
					outputErrorMessage(PORTTEST_ERROR_ARGS, "\nfirstFreeBlock and the actual fist free block don't match @ 0x%p\n", blockTopPaddingCursor);
					return;
				}
				firstFreeUnmatched = FALSE;
			}
		}
		
		blockBottomPaddingCursor = blockTopPaddingCursor+(absSize+1);
		bottomBlockSize = (I_64)*blockBottomPaddingCursor;

		if (topBlockSize != bottomBlockSize) {
			outputErrorMessage(PORTTEST_ERROR_ARGS, "\nsize in top and bottom block padding don't match @ 0x%p\n", blockTopPaddingCursor);
			return;
		}
#if defined(HEAPTEST_VERBOSE)			
		outputComment(portLibrary, "%lld|", topBlockSize);
#endif
		blockTopPaddingCursor = blockBottomPaddingCursor + 1;
	}
#if defined(HEAPTEST_VERBOSE)
	outputComment(portLibrary, "\n");
#endif
}

/**
 * Verify port library heap operations.
 * 
 * Exercise all API related to port library heap operations found in
 * @ref j9heap.c
 * 
 * @param[in] portLibrary The port library under test
 * 
 * @return 0 on success, -1 on failure
 */
I_32 
j9heap_runTests(struct J9PortLibrary *portLibrary, int randomSeed)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	int rc = 0;
	int i;
	I_64 timeStart = 0;
	I_64 testDurationMin = 5;

	/* Display unit under test */
	HEADING(portLibrary, "Heap test");

	/* verify sanity of port library for these tests.  If this fails there is no
	 * point in continuing
	 */
	rc = j9heap_test0(portLibrary);
	if (TEST_PASS == rc) {
		rc |= j9heap_test_pmr_28277_999_760(portLibrary);
		rc |= j9heap_test1(portLibrary);
		rc |= j9heap_realloc_test0(portLibrary);
		rc |= j9heap_test3(portLibrary);
		rc |= j9heap_grow_test(portLibrary);
		timeStart = j9time_current_time_millis();
		for (i = 0;i<10;++i){
			rc |= j9heap_test2(portLibrary, randomSeed);
			if (rc) {
				outputComment(portLibrary, "j9heap_test2 failed loop %d\n\n", i);
				break;
			} else {
				outputComment(portLibrary, "j9heap_test2 passed loop %d.\n", i);
				/* we check for time here and terminate this loop if we've run for too long */
				if ((j9time_current_time_millis() - timeStart) >= testDurationMin*60*1000) {
					outputComment(portLibrary, "We've run for too long, ending the test. This is not a test failure.\n\n");
					break;
				}
				outputComment(portLibrary, "\n\n");
			}
		}
	}
	/* Output results */
	j9tty_printf(PORTLIB, "\nHeap test done%s\n\n", rc == TEST_PASS ? "." : ", failures detected.");
	return TEST_PASS == rc ? 0 : -1;	
}
