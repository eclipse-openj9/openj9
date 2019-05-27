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

#include "j9protos.h"
#include "j9port.h"
#include "simplepool_api.h"

#define SIMPLEPOOL_MEDIUM_ELEMENT_SIZE (SIMPLEPOOL_MIN_ELEMENT_SIZE * 2)
#define SIMPLEPOOL_LARGE_ELEMENT_SIZE (SIMPLEPOOL_MIN_ELEMENT_SIZE * 4)
#define SIMPLEPOOL_HEADER_SIZE	sizeof(J9SimplePool)

typedef struct J9SimplePoolInputData {
	U_32 memorySize;
	U_32 elementSize;
} J9SimplePoolInputData;

typedef struct J9SimplePoolResultData {
	J9SimplePool* simplePool;
} J9SimplePoolResultData;

typedef struct J9SimplePoolUserData {
	J9SimplePool* simplePool;
	BOOLEAN resultValue;
	UDATA count;
	void *lastElement;
} J9SimplePoolUserData;

/* Table of parameters for simplepool_new, which will be used to test the various simplepool API functions. */
static J9SimplePoolInputData INPUTDATA[] = {
	{(SIMPLEPOOL_HEADER_SIZE + SIMPLEPOOL_MIN_ELEMENT_SIZE),				SIMPLEPOOL_MIN_ELEMENT_SIZE},		/* minimum sized pool */
	{(SIMPLEPOOL_HEADER_SIZE + SIMPLEPOOL_MIN_ELEMENT_SIZE * 10),			SIMPLEPOOL_MIN_ELEMENT_SIZE},		/* 10 minimum sized elements */
	{(SIMPLEPOOL_HEADER_SIZE + SIMPLEPOOL_MIN_ELEMENT_SIZE * 50),			SIMPLEPOOL_MIN_ELEMENT_SIZE},		/* 50 minimum sized elements */
	{(SIMPLEPOOL_HEADER_SIZE + SIMPLEPOOL_MIN_ELEMENT_SIZE * 2000),			SIMPLEPOOL_MIN_ELEMENT_SIZE},		/* 2000 minimum sized elements */
	{(SIMPLEPOOL_HEADER_SIZE + SIMPLEPOOL_MEDIUM_ELEMENT_SIZE * 5),			SIMPLEPOOL_MEDIUM_ELEMENT_SIZE},	/* 5 medium sized elements */
	{(SIMPLEPOOL_HEADER_SIZE + SIMPLEPOOL_MEDIUM_ELEMENT_SIZE * 200),		SIMPLEPOOL_MEDIUM_ELEMENT_SIZE},	/* 200 medium sized elements */
	{(SIMPLEPOOL_HEADER_SIZE + SIMPLEPOOL_MEDIUM_ELEMENT_SIZE * 3000),		SIMPLEPOOL_MEDIUM_ELEMENT_SIZE},	/* 3000 medium sized elements */
	{(SIMPLEPOOL_HEADER_SIZE + SIMPLEPOOL_LARGE_ELEMENT_SIZE * 2),			SIMPLEPOOL_LARGE_ELEMENT_SIZE},		/* 2 large sized elements */
	{(SIMPLEPOOL_HEADER_SIZE + SIMPLEPOOL_LARGE_ELEMENT_SIZE * 501),		SIMPLEPOOL_LARGE_ELEMENT_SIZE},		/* 501 large sized elements */
	{(SIMPLEPOOL_HEADER_SIZE + SIMPLEPOOL_LARGE_ELEMENT_SIZE * 1000),		SIMPLEPOOL_LARGE_ELEMENT_SIZE},		/* 1000 large sized elements */
	{(SIMPLEPOOL_HEADER_SIZE + SIMPLEPOOL_LARGE_ELEMENT_SIZE * 1023 + SIMPLEPOOL_MEDIUM_ELEMENT_SIZE),
			SIMPLEPOOL_LARGE_ELEMENT_SIZE},																	/* 1023 large sized elements, + SIMPLEPOOL_MEDIUM_ELEMENT_SIZE lost space in memory size */
	{2*1024*1024,		SIMPLEPOOL_LARGE_ELEMENT_SIZE}														/* 2 MB pool with large sized elements */
};

/* Table of faulty parameters for pool_new */
static J9SimplePoolInputData FAULTYINPUTDATA[] = {
	{0,			SIMPLEPOOL_MIN_ELEMENT_SIZE},							/* zero sized pool */
	{30000,		0},														/* zero sized elements */
	{1,			SIMPLEPOOL_MIN_ELEMENT_SIZE},							/* pool size less than minimum required */
	{30000,		SIMPLEPOOL_MIN_ELEMENT_SIZE-4},							/* element size less than minimum required */
	{30000,		(SIMPLEPOOL_MIN_ELEMENT_SIZE+1)},						/* element size not a multiple of 4 */
	{SIMPLEPOOL_MAX_MEMORY_SIZE + 1, 	SIMPLEPOOL_MIN_ELEMENT_SIZE}	/* pool size larger than maximum supported */
};

#define NUM_POOLS sizeof(INPUTDATA)/sizeof(J9SimplePoolInputData)

#define NUM_FAULTY_POOLS sizeof(FAULTYINPUTDATA)/sizeof(J9SimplePoolInputData)
#define SIMPLEPOOL_TEST_BYTE_FILL ((U_8)0xDD)

/* Results data for the pool tests. */
static J9SimplePoolResultData RESULTDATA[NUM_POOLS] = { 0 };


static BOOLEAN createSimplePools(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);
static BOOLEAN insertAndRemoveElements(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);
static BOOLEAN fillIterateAndRemove(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);
static BOOLEAN fillIterateAndRemove2(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);
static BOOLEAN detectCorruption(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);
static BOOLEAN detectCorruptionInRemoveElement(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);
static void freeSimplePools(J9PortLibrary *portLib);
static BOOLEAN doVerifyAndRemoveElement(void *element, void * userData);
static BOOLEAN doVerifyAndCountElements(void *element, void * userData);
static BOOLEAN checkConsistency(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount);

I_32 
verifySimplePools(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount)
{
	I_32 rc = 0;
	PORT_ACCESS_FROM_PORT(portLib);
	UDATA start, end;

	j9tty_printf(PORTLIB, "Testing simple pool functions using %d test pools...\n", NUM_POOLS + NUM_FAULTY_POOLS);

	start = j9time_usec_clock();

	if (createSimplePools(portLib, passCount, failCount)) {
		insertAndRemoveElements(portLib, passCount, failCount);
		fillIterateAndRemove(portLib, passCount, failCount);
		fillIterateAndRemove2(portLib, passCount, failCount);
		detectCorruption(portLib, passCount, failCount);
		detectCorruptionInRemoveElement(portLib, passCount, failCount);
		checkConsistency(portLib, passCount, failCount);
	}


	/* clean up, should always get called */
	freeSimplePools(portLib);

	end = j9time_usec_clock();

	j9tty_printf(PORTLIB, "Finished testing simple pool functions.\n");
	j9tty_printf(PORTLIB, "Simple pool functions execution time was %d (usec).\n", (end-start));

	return rc;
}


/**
 * Create a bunch of new simple pools
 *
 * @return TRUE upon success, FALSE otherwise
 */
static BOOLEAN
createSimplePools(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount)
{
	UDATA poolCntr;
	PORT_ACCESS_FROM_PORT(portLib);
	
	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9SimplePoolInputData* inputData = &INPUTDATA[poolCntr];
		J9SimplePoolResultData* resultData = &RESULTDATA[poolCntr];
		void* poolAddress = j9mem_allocate_memory(inputData->memorySize, OMRMEM_CATEGORY_VM);

		if ( NULL == poolAddress) {
			j9tty_printf(PORTLIB, "Error: createSimplePools() - unable to allocate memory for simple pool creation test %u\n", poolCntr);
			(*failCount)++;
			return 0;
		} else {
			resultData->simplePool = simplepool_new(poolAddress, inputData->memorySize, inputData->elementSize, 0);
			
			/* Check that creation succeeded */
			if (resultData->simplePool == NULL) {
				j9tty_printf(PORTLIB, "Error: createSimplePools() - simplepool_new returned NULL for simple pool test %u\n", poolCntr);
				(*failCount)++;
				return FALSE;
			} else {
				/* verify simple pool */
				if ( ! simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
					j9tty_printf(PORTLIB, "Error: createSimplePools() - simplePool # %u failed verification\n", poolCntr);
					(*failCount)++;
					return FALSE;
				}
			}

			(*passCount)++;
		}
	}

	/* test simplepool_new error handling */
	for (poolCntr = 0; poolCntr < NUM_FAULTY_POOLS; poolCntr++) {
		J9SimplePoolInputData* inputData = &FAULTYINPUTDATA[poolCntr];
		J9SimplePool *simplePool = NULL;
		void* poolAddress = NULL;

		if (0 < inputData->memorySize) {
			if (SIMPLEPOOL_MAX_MEMORY_SIZE < inputData->memorySize) {
				/* allocate a small amount of memory to allow the test to continue and verify that
				 * simplepool_new will fail because SIMPLEPOOL_MAX_MEMORY_SIZE < inputData->memorySize
				 */
				poolAddress = j9mem_allocate_memory(10, OMRMEM_CATEGORY_VM);
			} else {
				poolAddress = j9mem_allocate_memory(inputData->memorySize, OMRMEM_CATEGORY_VM);
			}
		}

		if ( 0 < inputData->memorySize && NULL == poolAddress) {
			j9tty_printf(PORTLIB, "Error: createSimplePools() - unable to allocate memory for faulty simple pool creation test %u\n", poolCntr);
			(*failCount)++;
			return FALSE;
		} else {
			simplePool = simplepool_new(poolAddress, inputData->memorySize, inputData->elementSize, 0);

			if (NULL != poolAddress) {
				j9mem_free_memory(poolAddress);
			}
			
			/* Check that creation failed */
			if (NULL != simplePool) {
				j9tty_printf(PORTLIB, "Error: createSimplePools() - simplepool_new did not return NULL for faulty input test %u\n", poolCntr);
				(*failCount)++;
				return FALSE;
			}

			(*passCount)++;
		}
	}

	/* Passing NULL to simplepool_clear should not crash */
	simplepool_clear(NULL);

	return TRUE;
}

/**
 * Inserts and removes elements from the simple pools
 *
 * @return TRUE upon success, FALSE otherwise
 */
static BOOLEAN
insertAndRemoveElements(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount)
{
	UDATA poolCntr;
	PORT_ACCESS_FROM_PORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9SimplePoolInputData* inputData = &INPUTDATA[poolCntr];
		J9SimplePoolResultData* resultData = &RESULTDATA[poolCntr];
		U_32 maxElements = (inputData->memorySize - SIMPLEPOOL_HEADER_SIZE) / inputData->elementSize;
		U_32 numElementsAdded = 0;
		U_8* element = NULL;
		UDATA i = 0;

		/* Attempt to allocation an element from a NULL simplepool */
		if (simplepool_newElement(NULL) != NULL) {
			j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - simplepool_newElement(NULL) succeeded simplePool # %u \n", poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* add and remove elements */
		for (i = 0; i < maxElements; i++) {
			element = simplepool_newElement(resultData->simplePool);

			if (NULL == element) {
				j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - simplepool_newElement # %u failed for simplePool # %u \n", i, poolCntr);
				(*failCount)++;
				return FALSE;
			} else {
				memset(element, SIMPLEPOOL_TEST_BYTE_FILL, inputData->elementSize); /* fill element with a value */
				
				if (i == 0) {
					/* Attempt to remove an element from a NULL simplepool */
					if ( 0 == simplepool_removeElement(NULL, element)) {
						j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - Removing element from NULL simplePool # %u \n", i, poolCntr);
						(*failCount)++;
						return FALSE;
					}
					/* Attempt to remove a NULL element from a simplepool */
					if ( 0 == simplepool_removeElement(resultData->simplePool, NULL)) {
						j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - Removing NULL element from simplePool # %u \n", i, poolCntr);
						(*failCount)++;
						return FALSE;
					}
				}

				/* remove every third element, tests adding elements to free list */
				if ( 0 == (i % 3) ) {
					if ( 0 != simplepool_removeElement(resultData->simplePool, element)) {
						j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - Removing element # %u failed for simplePool # %u \n", i, poolCntr);
						(*failCount)++;
						return FALSE;
					}
				} else {
					numElementsAdded++;
				}
			}
		}

		/* verify simple pool */
		if ( ! simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
			j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - simplePool # %u failed verification in insertAndRemoveElements()\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* verify simplepool_numElements(NULL) returns 0 */
		i = simplepool_numElements(NULL);
		if ( i != 0 ) {
			j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - simplepool_numElements(NULL) returned %u instead of 0 for simplePool # %u \n", i, poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* verify that simplepool_numElements == numElementsAdded */
		i = simplepool_numElements(resultData->simplePool);
		if ( i != numElementsAdded ) {
			j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - simplepool_numElements returned %u instead of %u for simplePool # %u \n", i, numElementsAdded, poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* fill up simple pool */
		for (i = 0; i < (maxElements - numElementsAdded) ; i++) {
			element = simplepool_newElement(resultData->simplePool);

			if (NULL == element) {
				j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - Filling up simple pool - simplepool_newElement # %u failed for simplePool # %u \n", i, poolCntr);
				(*failCount)++;
				return FALSE;
			} else {
				memset(element, SIMPLEPOOL_TEST_BYTE_FILL, inputData->elementSize); /* fill element with a value */
			}
		}

		/* verify that simplepool_maxNumElements(NULL) returns 0 */
		i = simplepool_maxNumElements(NULL);
		if ( i != 0 ) {
			j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - simplepool_maxNumElements returned %u instead of 0 for simplePool # %u \n", i, poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* verify that simplepool_maxNumElements == numElementsAdded */
		i = simplepool_maxNumElements(resultData->simplePool);
		if ( i != maxElements ) {
			j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - simplepool_maxNumElements returned %u instead of %u for simplePool # %u \n", i, maxElements, poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* attempt to add one more element, this should fail */
		if ( NULL != simplepool_newElement(resultData->simplePool) ) {
			j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - simplePool should have been full and simplepool_newElement should have returned NULL for simplePool  # %u \n", poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* verify that simplepool_numElements == maxElements */
		i = simplepool_numElements(resultData->simplePool);
		if ( i != maxElements ) {
			j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - simplepool_numElements returned %u instead of returning maxElements = %u after getting filled, simplePool # %u \n", i, maxElements, poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* verify simple pool again */
		if ( ! simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
			j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - simplePool # %u failed second verification in insertAndRemoveElements()\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* empty simple pool */
		simplepool_clear(resultData->simplePool);

		/* verify that simplepool_numElements == 0 */
		i = simplepool_numElements(resultData->simplePool);
		if ( i != 0 ) {
			j9tty_printf(PORTLIB, "Error: insertAndRemoveElements() - simplepool_numElements returned %u instead of returning 0 after simplepool_clear() was called, simplePool # %u \n", i, poolCntr);
			(*failCount)++;
			return FALSE;
		}

		(*passCount)++;
	}

	return TRUE;
}

/**
 * Verify that the element contains the same data we filled
 * it with and if so remove it from the simple pool
 */
static BOOLEAN
doVerifyAndRemoveElement(void *element, void * voidUserData)
{
	J9SimplePoolUserData *userData = (J9SimplePoolUserData *)voidUserData;
	U_8* currByte = element;
	UDATA i=0;

	for (i = 0; i < userData->simplePool->elementSize ; i++){
		if (*currByte != SIMPLEPOOL_TEST_BYTE_FILL) {
			return FALSE;
		}
		currByte++;
	}

	simplepool_removeElement(userData->simplePool, element);
	return userData->resultValue;
}

/**
 * Fill simple pools with elements, remove some of them to create a free list
 * and test iteration over the elements with simplepool_do() verifying and
 * removing all the elements from the simple pool
 */
static BOOLEAN
fillIterateAndRemove(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount)
{
	UDATA poolCntr;
	J9SimplePoolUserData userData;
	PORT_ACCESS_FROM_PORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9SimplePoolInputData* inputData = &INPUTDATA[poolCntr];
		J9SimplePoolResultData* resultData = &RESULTDATA[poolCntr];
		U_32 maxElements = (inputData->memorySize - SIMPLEPOOL_HEADER_SIZE) / inputData->elementSize;
		U_32 numElementsAdded = 0;
		U_8* element = NULL;
		UDATA i = 0;

		UDATA elementCount = 0;
		UDATA listSize = (sizeof(U_8 **) * maxElements) - (maxElements / 4);
		U_8 **elementList = (U_8 **)j9mem_allocate_memory(listSize, OMRMEM_CATEGORY_VM);
		if (elementList == NULL) {
			j9tty_printf(PORTLIB, "Error: fillIterateAndRemove() - out of memory allocating %d bytes\n", listSize);
			(*failCount)++;
			return FALSE;
		}

		/* empty simple pool */
		simplepool_clear(resultData->simplePool);

		/* add and remove elements */
		for (i = 0; i < maxElements; i++) {
			element = simplepool_newElement(resultData->simplePool);

			if (NULL == element) {
				j9tty_printf(PORTLIB, "Error: fillIterateAndRemove() - simplepool_newElement # %u failed for simplePool # %u \n", i, poolCntr);
				(*failCount)++;
				return FALSE;
			} else {
				elementList[elementCount] = element;
				memset(element, SIMPLEPOOL_TEST_BYTE_FILL, inputData->elementSize); /* fill element with a value */

				/* remove every 4th element */
				if ( 0 == (i % 4) ) {
					if ( 0 != simplepool_removeElement(resultData->simplePool, element)) {
						j9tty_printf(PORTLIB, "Error: fillIterateAndRemove() - Removing element # %u failed for simplePool # %u \n", i, poolCntr);
						(*failCount)++;
						return FALSE;
					}
				} else {
					elementCount++;
					numElementsAdded++;
				}
			}
		}

		/* remove every nth element to create a non-trivial free list */
		for (i = 0; i < elementCount; i += OMR_MAX(3, elementCount / 5)) {
			if ( 0 != simplepool_removeElement(resultData->simplePool, elementList[i])) {
				j9tty_printf(PORTLIB, "Error: fillIterateAndRemove() - Removing element # %u %x failed for simplePool # %u \n", i, elementList[i], poolCntr);
				(*failCount)++;
				return FALSE;
			}
			numElementsAdded--;
		}
		
		j9mem_free_memory(elementList);
		
		/* verify simple pool */
		if ( ! simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
			j9tty_printf(PORTLIB, "Error: fillIterateAndRemove() - simplePool # %u failed verification in fillIterateAndRemove() after elements were added and removed from the simple pool\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* verify that simplepool_numElements == numElementsAdded */
		i = simplepool_numElements(resultData->simplePool);
		if ( i != numElementsAdded ) {
			j9tty_printf(PORTLIB, "Error: fillIterateAndRemove() - simplepool_numElements returned %u instead of %u for simplePool # %u  after initial fill and remove\n", i, numElementsAdded, poolCntr);
			(*failCount)++;
			return FALSE;
		}
		
		userData.simplePool = resultData->simplePool;
		userData.resultValue = TRUE;

		/* NULL parameters should do nothing */
		simplepool_do(NULL, portLib, doVerifyAndRemoveElement, &userData);
		simplepool_do(resultData->simplePool, NULL, doVerifyAndRemoveElement, &userData);
		simplepool_do(resultData->simplePool, portLib, NULL, &userData);

		if (numElementsAdded > 0) {
			/* iterate over the first element and remove it from the simplepool, but fail the iterator so it stops */
			userData.resultValue = FALSE;
			if (TRUE == simplepool_do(resultData->simplePool, portLib, doVerifyAndRemoveElement, &userData)) {
				j9tty_printf(PORTLIB, "Error: fillIterateAndRemove() - simplepool_do returned TRUE for simplePool # %u\n", poolCntr);
				(*failCount)++;
				return FALSE;
			}
			/* verify the simplepool has one less element */
			i = simplepool_numElements(resultData->simplePool);
			if ( i != (numElementsAdded - 1) ) {
				j9tty_printf(PORTLIB, "Error: fillIterateAndRemove() - simplepool_numElements returned %u instead of %u for simplePool # %u  after simplepool_do stopped\n", i, numElementsAdded - 1, poolCntr);
				(*failCount)++;
				return FALSE;
			}
		}
		
		/* iterate over all the elements and remove them from the simple pool */
		userData.resultValue = TRUE;
		if (FALSE == simplepool_do(resultData->simplePool, portLib, doVerifyAndRemoveElement, &userData)) {
			j9tty_printf(PORTLIB, "Error: fillIterateAndRemove() - simplepool_do returned FALSE for simplePool # %u\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* verify that simplepool_numElements == 0 */
		i = simplepool_numElements(resultData->simplePool);
		if ( i != 0 ) {
			j9tty_printf(PORTLIB, "Error: fillIterateAndRemove() - simplepool_numElements returned %u instead of 0 for simplePool # %u after iterating with simpelpool_do and attempting to remove all elements\n", i, poolCntr);
			(*failCount)++;
			return FALSE;
		}

		(*passCount)++;
	}

	return TRUE;
}

/**
 * Similar to @ref fillIterateAndRemove with the two main differences being
 * that simplepool_do() is called on an empty pool at the beginning and that the
 * elements are not removed from the simple pool with simplepool_removeElement()
 * to test simplepool_do() on a simple pool with a NULL freeList
 */
static BOOLEAN
fillIterateAndRemove2(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount)
{
	UDATA poolCntr;
	J9SimplePoolUserData userData;
	PORT_ACCESS_FROM_PORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9SimplePoolInputData* inputData = &INPUTDATA[poolCntr];
		J9SimplePoolResultData* resultData = &RESULTDATA[poolCntr];
		U_32 maxElements = (inputData->memorySize - SIMPLEPOOL_HEADER_SIZE) / inputData->elementSize;
		U_8* element = NULL;
		UDATA i = 0;

		/* empty simple pool */
		simplepool_clear(resultData->simplePool);

		userData.simplePool = resultData->simplePool;
		userData.resultValue = TRUE;

		/* iterate over all the elements and remove them from the simple pool
		 * This should do nothing since the simple pool should be empty at this point
		 */
		simplepool_do(resultData->simplePool, portLib, doVerifyAndRemoveElement, &userData);
		/* verify simple pool */
		if ( ! simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
			j9tty_printf(PORTLIB, "Error: fillIterateAndRemove2() - simplePool # %u failed verification at the beginning of fillIterateAndRemove()\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* fill simple pool with elements */
		for (i = 0; i < maxElements; i++) {
			element = simplepool_newElement(resultData->simplePool);

			if (NULL == element) {
				j9tty_printf(PORTLIB, "Error: fillIterateAndRemove2() - simplepool_newElement # %u failed for simplePool # %u \n", i, poolCntr);
				(*failCount)++;
				return FALSE;
			} else {
				memset(element, SIMPLEPOOL_TEST_BYTE_FILL, inputData->elementSize); /* fill element with a value */
			}
		}

		/* verify simple pool */
		if ( ! simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
			j9tty_printf(PORTLIB, "Error: fillIterateAndRemove2() - simplePool # %u failed verification in fillIterateAndRemove() after elements were added and removed from the simple pool\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* verify that simplepool_numElements == maxElements */
		i = simplepool_numElements(resultData->simplePool);
		if ( i != maxElements ) {
			j9tty_printf(PORTLIB, "Error: fillIterateAndRemove2() - simplepool_numElements returned %u instead of %u for simplePool # %u  after initial fill and remove\n", i, maxElements, poolCntr);
			(*failCount)++;
			return FALSE;
		}

		userData.simplePool = resultData->simplePool;

		/* iterate over all the elements and remove them from the simple pool */
		if (FALSE == simplepool_do(resultData->simplePool, portLib, doVerifyAndRemoveElement, &userData)) {
			j9tty_printf(PORTLIB, "Error: fillIterateAndRemove2() - simplepool_do returned FALSE for simplePool # %u\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* verify that simplepool_numElements == 0 */
		i = simplepool_numElements(resultData->simplePool);
		if ( i != 0 ) {
			j9tty_printf(PORTLIB, "Error: fillIterateAndRemove2() - simplepool_numElements returned %u instead of 0 for simplePool # %u after iterating with simpelpool_do and attempting to remove all elements\n", i, poolCntr);
			(*failCount)++;
			return FALSE;
		}

		(*passCount)++;
	}

	return TRUE;
}

static BOOLEAN
detectCorruption(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount)
{
	UDATA poolCntr;
	PORT_ACCESS_FROM_PORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9SimplePoolInputData* inputData = &INPUTDATA[poolCntr];
		J9SimplePoolResultData* resultData = &RESULTDATA[poolCntr];
		U_32 maxElements = (inputData->memorySize - SIMPLEPOOL_HEADER_SIZE) / inputData->elementSize;
		U_8* blockEnd = NULL;
		J9SimplePoolFreeList* freeList = NULL;
		UDATA i = 0;
		U_8* firstFreeSlot;

		UDATA elementCount = 0;
		UDATA listSize = (sizeof(U_8 **) * maxElements) - (maxElements / 4);
		U_8 **elementList = (U_8 **)j9mem_allocate_memory(listSize, OMRMEM_CATEGORY_VM);
		if (elementList == NULL) {
			j9tty_printf(PORTLIB, "Error: detectCorruption() - out of memory allocating %d bytes\n", listSize);
			(*failCount)++;
			return FALSE;
		}
		
		/*
		 * Add and remove elements at first to simulate a live simple pool and set up a freeList
		 */

		/* empty simple pool */
		simplepool_clear(resultData->simplePool);

		/* add and remove elements */
		for (i = 0; i < maxElements; i++) {
			U_8* element = simplepool_newElement(resultData->simplePool);

			if (NULL == element) {
				j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_newElement # %u failed for simplePool # %u \n", i, poolCntr);
				(*failCount)++;
				return FALSE;
			} else {
				elementList[elementCount] = element;
				memset(element, SIMPLEPOOL_TEST_BYTE_FILL, inputData->elementSize); /* fill element with a value */

				/* remove every 4th element */
				if ( 0 == (i % 4) ) {
					if ( 0 != simplepool_removeElement(resultData->simplePool, element)) {
						j9tty_printf(PORTLIB, "Error: detectCorruption() - Removing element # %u failed for simplePool # %u \n", i, poolCntr);
						(*failCount)++;
						return FALSE;
					}
				} else {
					elementCount++;
				}
			}
		}

		/* remove every nth element to create a non-trivial free list */
		for (i = 0; i < elementCount; i += OMR_MAX(3, elementCount / 5)) {
			if ( 0 != simplepool_removeElement(resultData->simplePool, elementList[i])) {
				j9tty_printf(PORTLIB, "Error: detectCorruption() - Removing element # %u %x failed for simplePool # %u \n", i, elementList[i], poolCntr);
				(*failCount)++;
				return FALSE;
			}
		}
		
		j9mem_free_memory(elementList);

		/**
		 * corrupt elementSize and verify that the corruption is detected
		 */
		resultData->simplePool->elementSize--;
		if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
			j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect elementSize corruption for simplepool # %u\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}
		resultData->simplePool->elementSize++;

		/**
		 * corrupt firstFreeSlot and verify that the corruption is detected
		 */
		firstFreeSlot = J9SIMPLEPOOL_FIRSTFREESLOT(resultData->simplePool);
		SRP_SET(resultData->simplePool->firstFreeSlot, resultData->simplePool);
		if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
			j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect firstFreeSlot corruption for simplepool # %u\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}

		SRP_SET(resultData->simplePool->firstFreeSlot, (UDATA)firstFreeSlot - 1);
		if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
			j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect firstFreeSlot corruption for simplepool # %u\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}
		SRP_SET(resultData->simplePool->firstFreeSlot, firstFreeSlot);

		/**
		 * corrupt flags and verify that the corruption is detected
		 */
		resultData->simplePool->flags = 1;
		if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
			j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect flags corruption for simplepool # %u\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}
		resultData->simplePool->flags = 0;

		/**
		 * corrupt blockEnd and verify that the corruption is detected
		 */
		blockEnd = J9SIMPLEPOOL_BLOCKEND(resultData->simplePool);
		SRP_SET(resultData->simplePool->blockEnd, blockEnd - 1);
		if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
			j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect blockEnd corruption for simplepool # %u\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}
		SRP_SET(resultData->simplePool->blockEnd, blockEnd);

		/**
		 * corrupt freeList and verify that the corruption is detected
		 */
		freeList = J9SIMPLEPOOL_FREELIST(resultData->simplePool);
		if (NULL != freeList) {
			J9SimplePool* backPtr = J9SIMPLEPOOLFREELIST_SIMPLEPOOL(freeList);

			/* unaligned pointer corruption */
			SRP_SET(resultData->simplePool->freeList, (U_8*)((UDATA)freeList + 1));
			if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
				j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect unaligned freeList pointer corruption for simplepool # %u\n", poolCntr);
				(*failCount)++;
				return FALSE;
			}
			SRP_SET(resultData->simplePool->freeList, freeList);

			/* out of range pointer corruption */
			SRP_SET(resultData->simplePool->freeList, firstFreeSlot + 1);
			if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
				j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect out of range freeList pointer corruption for simplepool # %u\n", poolCntr);
				(*failCount)++;
				return FALSE;
			}
			SRP_SET(resultData->simplePool->freeList, resultData->simplePool); /* set free list before first possible element slot */
			if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
				j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect out of range freeList pointer corruption when free list was set before the first possible element slot for simplepool # %u\n", poolCntr);
				(*failCount)++;
				return FALSE;
			}
			SRP_SET(resultData->simplePool->freeList, freeList);

			do {
				J9SimplePoolFreeList* nextFree = J9SIMPLEPOOLFREELIST_NEXT(freeList);

				/* incorrect free list back pointer */
				SRP_SET_TO_NULL(freeList->simplePool);
				if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
					j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect incorrect (NULL) free list back pointer for simplepool # %u\n", poolCntr);
					(*failCount)++;
					return FALSE;
				}
				/* incorrect free list back pointer corruption */
				SRP_SET(freeList->simplePool, backPtr + 1);
				if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
					j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect incorrect simple pool back pointer freeList corruption for simplepool # %u\n", poolCntr);
					(*failCount)++;
					return FALSE;
				}
				SRP_SET(freeList->simplePool, backPtr);
	
				/* freeList->next out of range */
				SRP_SET(freeList->next, resultData->simplePool); /* out of range - before possible element range */
				if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
					j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect out of range free list next pointer (before range) for simplepool # %u\n", poolCntr);
					(*failCount)++;
					return FALSE;
				}
				SRP_SET(freeList->next, J9SIMPLEPOOL_FIRSTFREESLOT(resultData->simplePool)); /* out of range - after possible element range */
				if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
					j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect out of range free list next pointer (after range) for simplepool # %u\n", poolCntr);
					(*failCount)++;
					return FALSE;
				}
	
				/* freeList->next unaligned */
				SRP_SET(freeList->next, (void*)((UDATA)nextFree + 1));
				if ( simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
					j9tty_printf(PORTLIB, "Error: detectCorruption() - simplepool_verify did not detect unaligned free list next pointer for simplepool # %u\n", poolCntr);
					(*failCount)++;
					return FALSE;
				}
	
				/* reset freeList->next to its correct value */
				SRP_SET(freeList->next, nextFree);
				
				freeList = nextFree;
			} while (freeList != NULL);

			/* ensure simple pool still passes verification */
			if ( ! simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
				j9tty_printf(PORTLIB, "Error: detectCorruptionInFreeList() - final simplepool_verify failed for simplepool # %u\n", poolCntr);
				(*failCount)++;
				return FALSE;
			}
		}

		(*passCount)++;
	}

	return TRUE;
}

static BOOLEAN
detectCorruptionInRemoveElement(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount)
{
	UDATA poolCntr;
	PORT_ACCESS_FROM_PORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9SimplePoolInputData* inputData = &INPUTDATA[poolCntr];
		J9SimplePoolResultData* resultData = &RESULTDATA[poolCntr];
		U_32 numElementsToAdd = ((inputData->memorySize - SIMPLEPOOL_HEADER_SIZE) / inputData->elementSize) / 3;
		UDATA i = 0;

		/* empty simple pool */
		simplepool_clear(resultData->simplePool);

		/* add and remove elements */
		for (i = 0; i < numElementsToAdd ; i++) {
			U_8* element = simplepool_newElement(resultData->simplePool);

			if (NULL == element) {
				j9tty_printf(PORTLIB, "Error: detectCorruptionInRemoveElement() - simplepool_newElement # %u failed for simplePool # %u \n", i, poolCntr);
				(*failCount)++;
				return FALSE;
			} else {
				memset(element, SIMPLEPOOL_TEST_BYTE_FILL, inputData->elementSize); /* fill element with a value */

				/* remove every 3rd element */
				if ( 0 == (i % 3) ) {
					if ( 0 == simplepool_removeElement(resultData->simplePool, resultData->simplePool) ) { /* element out of range - before */
						j9tty_printf(PORTLIB, "Error: detectCorruptionInRemoveElement() - simplepool_removeElement() #u did not detect that the passed in element address was before the first possible element slot for simplePool # %u \n", i, poolCntr);
						(*failCount)++;
						return FALSE;
					} else if ( 0 == simplepool_removeElement(resultData->simplePool, J9SIMPLEPOOL_FIRSTFREESLOT(resultData->simplePool)) ) { /* element out of range - after */
						j9tty_printf(PORTLIB, "Error: detectCorruptionInRemoveElement() - simplepool_removeElement() #u did not detect that the passed in element address was after the possible range of elements for simplePool # %u \n", i, poolCntr);
						(*failCount)++;
						return FALSE;
					} else if ( 0 == simplepool_removeElement(resultData->simplePool, (void*)((UDATA)element + 1) ) ) { /* element unaligned */
						j9tty_printf(PORTLIB, "Error: detectCorruptionInRemoveElement() - simplepool_removeElement() #u did not detect that the passed in element address was unaligned for simplePool # %u \n", i, poolCntr);
						(*failCount)++;
						return FALSE;
					} else if ( 0 != simplepool_removeElement(resultData->simplePool, element)) { /* this one should succeed */
						j9tty_printf(PORTLIB, "Error: detectCorruptionInRemoveElement() - Removing element # %u failed for simplePool # %u \n", i, poolCntr);
						(*failCount)++;
						return FALSE;
					}
				}
			}
		}

		/* ensure simple pool still passes verification */
		if ( ! simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
			j9tty_printf(PORTLIB, "Error: detectCorruptionInRemoveElement() - simplepool_verify failed for simplepool # %u\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}
		(*passCount)++;
	}

	return TRUE;
}


static void
freeSimplePools(J9PortLibrary *portLib)
{
	IDATA poolCntr;
	PORT_ACCESS_FROM_PORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9SimplePoolResultData* resultData = &RESULTDATA[poolCntr];

		if(NULL != resultData->simplePool) {
			j9mem_free_memory(resultData->simplePool);
		}
	}
}

/**
 * Verify that the element contains the same data we filled
 * it with and if so modify it
 */
static BOOLEAN
doVerifyAndCountElements(void *element, void * voidUserData)
{
	J9SimplePoolUserData *userData = (J9SimplePoolUserData *)voidUserData;
	U_8* currByte = element;
	UDATA i=0;

	for (i = 0; i < userData->simplePool->elementSize ; i++){
		if (*currByte != SIMPLEPOOL_TEST_BYTE_FILL) {
			return FALSE;
		}
		currByte++;
	}
	
	if (((UDATA)userData->lastElement + userData->simplePool->elementSize) == (UDATA)element) {
		/* expected to skip an element */
		return FALSE;
	}
	userData->lastElement = element;

	userData->count++;
	return TRUE;
}

/**
 * Fill simple pools with elements, remove some of them to create a free list
 * and test iterate over the elements with simplepool_checkConsistency() verifying and
 * removing all the elements from the simple pool
 */
static BOOLEAN
checkConsistency(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount)
{
	UDATA poolCntr;
	J9SimplePoolUserData userData;
	PORT_ACCESS_FROM_PORT(portLib);

	for (poolCntr = 0; poolCntr < NUM_POOLS; poolCntr++) {
		J9SimplePoolInputData* inputData = &INPUTDATA[poolCntr];
		J9SimplePoolResultData* resultData = &RESULTDATA[poolCntr];
		U_32 maxElements = (inputData->memorySize - SIMPLEPOOL_HEADER_SIZE) / inputData->elementSize;
		U_32 numElementsAdded = 0;
		U_8* element = NULL;
		UDATA i = 0;

		UDATA elementCount = 0;
		UDATA listSize = (sizeof(U_8 **) * maxElements) - (maxElements / 4);
		U_8 **elementList = (U_8 **)j9mem_allocate_memory(listSize, OMRMEM_CATEGORY_VM);
		if (elementList == NULL) {
			j9tty_printf(PORTLIB, "Error: fillCheckConsistencyAndRemove() - out of memory allocating %d bytes\n", listSize);
			(*failCount)++;
			return FALSE;
		}

		/* empty simple pool */
		simplepool_clear(resultData->simplePool);

		/* add and remove elements */
		for (i = 0; i < maxElements; i++) {
			element = simplepool_newElement(resultData->simplePool);

			if (NULL == element) {
				j9tty_printf(PORTLIB, "Error: fillCheckConsistencyAndRemove() - simplepool_newElement # %u failed for simplePool # %u \n", i, poolCntr);
				(*failCount)++;
				return FALSE;
			} else {
				elementList[elementCount] = element;
				memset(element, SIMPLEPOOL_TEST_BYTE_FILL, inputData->elementSize); /* fill element with a value */

				/* remove every 4th element */
				if ( 0 == (i % 4) ) {
					if ( 0 != simplepool_removeElement(resultData->simplePool, element)) {
						j9tty_printf(PORTLIB, "Error: fillCheckConsistencyAndRemove() - Removing element # %u failed for simplePool # %u \n", i, poolCntr);
						(*failCount)++;
						return FALSE;
					}
				} else {
					elementCount++;
					numElementsAdded++;
				}
			}
		}
		
		/* remove every nth element to create a non-trivial free list */
		for (i = 1; i < elementCount; i += OMR_MAX(3, elementCount / 5)) {
			if ( 0 != simplepool_removeElement(resultData->simplePool, elementList[i])) {
				j9tty_printf(PORTLIB, "Error: fillCheckConsistencyAndRemove() - Removing element # %u %x failed for simplePool # %u \n", i, elementList[i], poolCntr);
				(*failCount)++;
				return FALSE;
			}
			numElementsAdded--;
		}
		
		j9mem_free_memory(elementList);
		
		/* verify simple pool */
		if ( ! simplepool_verify(resultData->simplePool, inputData->memorySize, inputData->elementSize) ) {
			j9tty_printf(PORTLIB, "Error: fillCheckConsistencyAndRemove() - simplePool # %u failed verification in fillIterateAndRemove() after elements were added and removed from the simple pool\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}

		/* verify that simplepool_numElements == numElementsAdded */
		i = simplepool_numElements(resultData->simplePool);
		if ( i != numElementsAdded ) {
			j9tty_printf(PORTLIB, "Error: fillCheckConsistencyAndRemove() - simplepool_numElements returned %u instead of %u for simplePool # %u  after initial fill and remove\n", i, numElementsAdded, poolCntr);
			(*failCount)++;
			return FALSE;
		}
		
		userData.simplePool = resultData->simplePool;
		userData.resultValue = TRUE;

		/* NULL parameters should do nothing */
		simplepool_checkConsistency(NULL, portLib, doVerifyAndRemoveElement, &userData, 1);
		simplepool_checkConsistency(resultData->simplePool, NULL, doVerifyAndRemoveElement, &userData, 1);
		simplepool_checkConsistency(resultData->simplePool, portLib, NULL, &userData, 1);

		if (numElementsAdded > 0) {
			/* iterate over the first element and remove it from the simplepool, but fail the iterator so it stops */
			userData.resultValue = FALSE;
			if (TRUE == simplepool_checkConsistency(resultData->simplePool, portLib, doVerifyAndRemoveElement, &userData, 1)) {
				j9tty_printf(PORTLIB, "Error: fillCheckConsistencyAndRemove() - simplepool_checkConsistency returned TRUE for simplePool # %u\n", poolCntr);
				(*failCount)++;
				return FALSE;
			}
			/* verify the simplepool has one less element */
			numElementsAdded--;
			i = simplepool_numElements(resultData->simplePool);
			if ( i != numElementsAdded ) {
				j9tty_printf(PORTLIB, "Error: fillCheckConsistencyAndRemove() - simplepool_numElements returned %u instead of %u for simplePool # %u  after simplepool_checkConsistency stopped\n", i, numElementsAdded, poolCntr);
				(*failCount)++;
				return FALSE;
			}
		}
		
		/* iterate over every second element and count the number of elements iterated over */
		userData.resultValue = TRUE;
		userData.count = 0;
		userData.lastElement = NULL;
		if (FALSE == simplepool_checkConsistency(resultData->simplePool, portLib, doVerifyAndCountElements, &userData, 1)) {
			j9tty_printf(PORTLIB, "Error: fillCheckConsistencyAndRemove() - simplepool_checkConsistency returned FALSE for simplePool # %u\n", poolCntr);
			(*failCount)++;
			return FALSE;
		}
		
		/* verify that simplepool_checkConsistency iterated the correct number of elements */
		if ( ((userData.count == 0) && (numElementsAdded != 0)) ||
				((numElementsAdded > 1) && (userData.count >= numElementsAdded)) || 
				((numElementsAdded > 1) && (userData.count < (numElementsAdded / 2))) || 
				(userData.count > (elementCount / 2)) ) {
			j9tty_printf(PORTLIB, "Error: fillCheckConsistencyAndRemove() - simplepool_checkConsistency count is %u for %u elements for simplePool # %u after iterating with simplepool_checkConsistency\n", userData.count, numElementsAdded, poolCntr);
			(*failCount)++;
			return FALSE;
		}

		(*passCount)++;
	}

	return TRUE;
}
