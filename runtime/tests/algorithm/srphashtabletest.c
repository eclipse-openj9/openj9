/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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

#include <stdlib.h>
#include "j9.h"
#include "j9consts.h"
#include "j9protos.h"
#include "util_api.h"

/*
 * Testing the following functions of J9SRPHashTable:
 * 		srpHashTableAdd()
 * 		srpHashTableGetCount()
 * 		srpHashTableFind()
 * 		srpHashTableForEachDo()
 * 		srpHashTableRemove()
 */

#define ROUND_TO_SIZEOF_UDATA(number) (((number) + (sizeof(UDATA) - 1)) & (~(sizeof(UDATA) - 1)))

static UDATA hashFn (void *key, void *userData);
static UDATA hashEqualFn (void *leftKey, void *rightKey, void *userData);
static UDATA dataOffset(UDATA startOffset, UDATA dataLength, UDATA i);
static BOOLEAN checkIntegrity(J9PortLibrary *portLib, char * id, J9SRPHashTable * srptable, UDATA *data, U_32 dataLength, UDATA removeOffset, UDATA i);
static BOOLEAN runTests(J9PortLibrary *portLib, char * id, J9SRPHashTable **srptable, UDATA *data, U_32 dataLength, UDATA reverseRemove, BOOLEAN recreateSRPHashTable);
static void testSRPHashtable(J9PortLibrary *portLib, char * id, UDATA *data, U_32 dataLength, UDATA *passCount, UDATA *failCount);
static UDATA getExpectedNumOfElements(UDATA removeOffset, UDATA i, UDATA dataLength, UDATA tableSize);
extern const U_8 RandomValues[256];

#define FORWARD 0
#define REVERSE -1

/**
 * A function to be used for SRPHashTable elements to check their equality between two elements.
 * Since the test just uses numbers, it checks whether they represent the same number.
 * If so, it returns 1 meaning equal, it return 0 otherwise.
 *
 * @param	leftKey		Pointer to the entry in SRPHashTable.
 * @param	rightKey	Pointer to another entry in SRPHashTable.
 * @param 	userData	Pointer to the userData.
 * @return  1 if left and right key are equal, 0 otherwise.
 *
 */
static UDATA  
hashEqualFn(void *leftKey, void *rightKey, void *userData) 
{
	return *(UDATA *)leftKey == *(UDATA *)rightKey;
}

/**
 * A function that calculates the hash value for a given key.
 * Since entries are all UDATA numbers in this test, it just returns the value of a number.
 *
 * @key			Pointer to an element to calculate the hash value for.
 * @userData	Pointer to the user data.
 * @return 		Hash value of a given key.
 *
 */
static UDATA 
hashFn(void *key, void *userData) 
{
	return *(UDATA *)key;
}

/**
 * It finds previous or next offset after i iterations.
 * If startOffset is -1 then it finds previous offset starting from end.
 * If startOffset is not -1, then it finds next offset starting from startOffset for i iterations.
 * If the next hits the end of the data, it returns to the start.
 *
 * @param 	startOffset	Starting offset of the data. If it is -1, then it is reverse order and we start from the end.
 * @param	dataLength	The size of the data array.
 * @param 	i			Number of iterations to find the next/previous offset starting from startOffset/end.
 * @return	UDATA		New offSet.
 *
 */
static UDATA dataOffset(UDATA startOffset, UDATA dataLength, UDATA i)
{
	UDATA result;
	if (startOffset == REVERSE) {
		return dataLength - (i + 1);
	}
	result = startOffset + i;
	if (result >= dataLength) {
		result -= dataLength;
	}
	return result; 
}

typedef struct UserIntegrityCheckData{
	UDATA count;
	UDATA dup[256];
	UDATA dataLength;
	char * id;
	UDATA i;
	J9PortLibrary *portLib;
	UDATA *data;
	UDATA removeOffset;
	J9SRPHashTable * srptable;
}UserIntegrityCheckData;

/**
 * A function to be used in integrity check.
 *
 * @param 	in			Pointer to an entry in SRPHashTable
 * @param	userData	Pointer to a user data.
 * @return 	TRUE if successful, FALSE otherwise.
 *
 */
static UDATA
userCheckIntegrityFn(void *in, void *userData) {
	UserIntegrityCheckData * userIntegrityCheckData = (UserIntegrityCheckData *)userData;
	UDATA * currentNode = (UDATA *)in;
	UDATA entry;
	UDATA * node;
	UDATA j;
	BOOLEAN found = FALSE;
	PORT_ACCESS_FROM_PORT(userIntegrityCheckData->portLib);

	userIntegrityCheckData->count++;

	if (*currentNode >= sizeof(userIntegrityCheckData->dup) / sizeof(UDATA)) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s dup detection, values must be < %d\n", userIntegrityCheckData->id, sizeof(userIntegrityCheckData->dup) / sizeof(UDATA));
		return FALSE;
	}
	if (userIntegrityCheckData->dup[*currentNode]) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s walk duplicate failure\n", userIntegrityCheckData->id);
		return FALSE;
	}
	userIntegrityCheckData->dup[*currentNode] = 1;
	for (j = userIntegrityCheckData->i + 1; j < userIntegrityCheckData->dataLength; j++) {
		if (*currentNode == userIntegrityCheckData->data[dataOffset(userIntegrityCheckData->removeOffset, userIntegrityCheckData->dataLength, j)]) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s walk element failure\n", userIntegrityCheckData->id);
		return FALSE;
	}

	entry = *currentNode;
	node = srpHashTableFind(userIntegrityCheckData->srptable, &entry);
	if (node == NULL || *node != entry) {
		if (node == NULL) {
			j9tty_printf(PORTLIB, "SRP Hashtable %s walk find failure: search %d\n", userIntegrityCheckData->id, entry);
		} else {
			j9tty_printf(PORTLIB, "SRP Hashtable %s walk find failure: search %d found %d\n", userIntegrityCheckData->id, entry, *node);
		}
		return FALSE;
	}
	return TRUE;
}

/**
 * A function that checks the integrity of the SRPHashtable.
 * It runs userCheckIntegrityFn for every element in the SRPHashTable.
 * It also tries to find every element user passed by data array.
 * It fails if it is not supposed to find/not find the element.
 * First dataLength elements of data array should be found in SRPHashtable
 * and test fails if any of these elements can not be found.
 * Any element after the first dataLength elements of data array should not be found in SRPHashTable.
 * If any is found, then test fails.
 *
 * @param 	portLib			Pointer to a port library.
 * @param 	srptable		Pointer to the SRPHashTable that is being checked for integrity.
 * @param 	data			Array of UDATA to be used in this testing.
 * @param 	dataLength		Number of elements in data array.
 * @param	removeOffset 	Offset of the last removed element.
 * @param 	i				Number of iterations.
 * @return TRUE	if integrity check passes, FALSE otherwise.
 *
 */

static BOOLEAN
checkIntegrity(J9PortLibrary *portLib, char * id, J9SRPHashTable * srptable, UDATA *data, U_32 dataLength, UDATA removeOffset, UDATA i)
{
	PORT_ACCESS_FROM_PORT(portLib);
	UDATA actualCount, expectedCount, j;
	UDATA bucketIndex = 0;
	UserIntegrityCheckData userIntegrityCheckData;
	UDATA tableSize = srpHashTable_tableSize(srptable);

	actualCount = srpHashTableGetCount(srptable);
	expectedCount = getExpectedNumOfElements(removeOffset, i, dataLength, tableSize);
	if (actualCount != expectedCount) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s count failure: count %d expected %d\n", id, actualCount, expectedCount);
		return FALSE;
	}

	/* walk all the elements, checking for duplicates and ensuring the elements are valid */

	userIntegrityCheckData.count = 0;
	memset(userIntegrityCheckData.dup, 0, sizeof(userIntegrityCheckData.dup));
	userIntegrityCheckData.dataLength = dataLength;
	userIntegrityCheckData.id = id;
	userIntegrityCheckData.i = i;
	userIntegrityCheckData.portLib = portLib;
	userIntegrityCheckData.data = data;
	userIntegrityCheckData.removeOffset = removeOffset;
	userIntegrityCheckData.srptable = srptable;

	srpHashTableForEachDo(srptable, userCheckIntegrityFn, &userIntegrityCheckData);


	if (userIntegrityCheckData.count != expectedCount) {
			j9tty_printf(PORTLIB, "SRP Hashtable %s walk count failure\n", id);
			return FALSE;
	}

	for (j = i + 1; j < dataLength; j++) {
		UDATA * node;
		UDATA offset = dataOffset(removeOffset, dataLength, j);
		UDATA entry = data[offset];
		node = srpHashTableFind(srptable, &entry);
		if (offset < tableSize) {
			if (node == NULL || *node != entry) {
				if (node == NULL) {
					j9tty_printf(PORTLIB, "SRP Hashtable %s find failure: search %d\n", id, entry);
				} else {
					j9tty_printf(PORTLIB, "SRP Hashtable %s find failure: search %d found %d\n", id, entry, *node);
				}
				return FALSE;
			}
		} else {
			if (node != NULL) {
				if (*node == entry) {
					j9tty_printf(PORTLIB, "SRP Hashtable %s find failure: "
							"Entry %d was not added to SRPHashTable due to no vacancy,"
							"but it is found in the table\n", id, entry);
				} else {
					j9tty_printf(PORTLIB, "SRP Hashtable %s find failure: "
							"Entry %d was not added to SRPHashTable due to no vacancy. "
							"In the search for entry %d, another entry returned as found: %d\n", id, entry, entry, *node);
				}
				return FALSE;
			}
		}

	}
	return TRUE;
}

/**
 * This functions runs the test on a given srphashtable with the given data.
 * It has 3 main stages.
 *
 * 1. It tries to add every element of data array.
 * Data array might be bigger than tableSize of the SRPHAshtable.
 * In such cases, it knows when an element should successfully be added to SRPHashTable
 * and when it should not cause of no vacancy.
 *
 * 2. It tries to find every element of data array in the SRPHashtable.
 * It knows which elements should be found and not found.
 * Any first tableSize element of data array should be found. The rest should not be found.
 *
 * 3. It tries to remove every element of data array from SRPHashTable.
 *
 * @param 	portLib					Pointer to a port library.
 * @param 	id		  				Char pointer to the test's name.
 * @param 	srptable				A SRPHashTable which is used for this testing.
 * @param 	data					Array of UDATA values to be used in this testing.
 * @param 	dataLength				Number of elements in data array.
 * @param	removeOffset 			Offset of the last element that is removed from SRPHashTable.
 * @param	recreateSRPHashTable	A flag that is used whether to test srpHashTableRecreate function or not.
 * @return 	TRUE if the test is successful, FALSE otherwise.
 *
 */
static BOOLEAN
runTests(J9PortLibrary *portLib, char * id, J9SRPHashTable **srptable, UDATA *data, U_32 dataLength, UDATA removeOffset, BOOLEAN recreateSRPHashTable)
{
	PORT_ACCESS_FROM_PORT(portLib);
	UDATA i, entry;
	char * operation = "";
	char buf[64];
	U_32 tableSize = srpHashTable_tableSize(*srptable);
	UDATA offset;
	void * addedNode;
	J9SRPHashTable * tempSRPTable;
	UDATA iterationNumber = 20;
	UDATA executedFunctionCounter = 0;

	/* add all the data elements */
	for (i = 0; i < dataLength; i++) {
		entry = data[i];
		addedNode = srpHashTableAdd(*srptable, &entry);
		executedFunctionCounter++;

		if (addedNode == NULL) {
			if (i < tableSize) {
				/* Failure to allocate a new node */
				j9tty_printf(PORTLIB, "SRP Hashtable %s add failure. Adding %d\n", id, entry);
				return FALSE;
			}
		} else {
			if (IS_NEW_ELEMENT(addedNode)) {
				UNMARK_NEW_ELEMENT(addedNode, void *);
				*(UDATA *)(addedNode) = entry;
				if (i >= tableSize) {
					/* We can not add more elements than tableSize into srphashtable */
					j9tty_printf(PORTLIB, "SRP Hashtable %s add failure. "
							"There is no space left to add the entry %d but it is added successfully.\n", id, entry);
					return FALSE;
				}
			}
		}

		if (recreateSRPHashTable && ((executedFunctionCounter % iterationNumber) == 0)) {
			tempSRPTable = *srptable;
			*srptable = srpHashTableRecreate(
										portLib,
										"testTable",
										(*srptable)->srpHashtableInternal,
										hashFn,
										hashEqualFn,
										NULL,
										NULL);
			j9mem_free_memory(tempSRPTable);
		}
	}

	if (recreateSRPHashTable) {
		tempSRPTable = *srptable;
		*srptable = srpHashTableRecreate(
									portLib,
									"testTable",
									(*srptable)->srpHashtableInternal,
									hashFn,
									hashEqualFn,
									NULL,
									NULL);
		j9mem_free_memory(tempSRPTable);
	}

	/**
	 *  ensure the count is correct
	 *  If srphashtable is big enough to store all the elements,
	 *  then datalength should be equal to number of elements stored in srphashtable.
	 *  Otherwise, srphashtable should be completely full.
	 */
	if (tableSize < dataLength) {
		if (srpHashTableGetCount(*srptable) != tableSize) {
			j9tty_printf(PORTLIB, "SRP Hashtable %s count failure. dataLength: %d, srphashtable count: %d, expected count=tableSize: %d\n", id, dataLength, srpHashTableGetCount(*srptable), tableSize);
			j9tty_printf(PORTLIB, "Should be:\t%d,\tFound :\t%d\n", dataLength, srpHashTableGetCount(*srptable));
			return FALSE;
		}
	} else {
		if (srpHashTableGetCount(*srptable) != dataLength) {
			j9tty_printf(PORTLIB, "SRP Hashtable %s count failure. dataLength: %d, srphashtable count:\n", id, dataLength, srpHashTableGetCount(*srptable));
			j9tty_printf(PORTLIB, "Should be:\t%d,\tFound :\t%d\n", tableSize, srpHashTableGetCount(*srptable));
			return FALSE;
		}
	}

	
	/**
	 *  find all the elements
	 *  If srphashtable is not big enough to store all the elements,
	 *  then we can only find the first tableSize number of elements in data array,
	 *  the rest should not be found.
	 */
	for (i = 0; i < dataLength; i++) {
		UDATA * node;
		entry = data[i];
		node = srpHashTableFind(*srptable, &entry);
		executedFunctionCounter++;

		if (i < tableSize) {
			if (node == NULL || *node != entry) {
				if (node == NULL) {
					j9tty_printf(PORTLIB, "SRP Hashtable %s find failure: search %d\n", id, entry);
				} else {
					j9tty_printf(PORTLIB, "SRP Hashtable %s find failure: search %d found %d\n", id, entry, *node);
				}
				return FALSE;
			}
		} else {
			if (node != NULL) {
				if (*node == entry) {
					j9tty_printf(PORTLIB, "SRP Hashtable %s find failure: "
							"Entry %d was not added to SRPHashTable due to no vacancy,"
							"but it is found in the table\n", id, entry);
				} else {
					j9tty_printf(PORTLIB, "SRP Hashtable %s find failure: "
							"Entry %d was not added to SRPHashTable due to no vacancy. "
							"In the search for entry %d, another entry returned as found: %d\n", id, entry, entry, *node);
				}
				return FALSE;
			}
		}

		if (recreateSRPHashTable && ((executedFunctionCounter % iterationNumber) == 0)) {
			tempSRPTable = *srptable;
			*srptable = srpHashTableRecreate(
										portLib,
										"testTable",
										(*srptable)->srpHashtableInternal,
										hashFn,
										hashEqualFn,
										NULL,
										NULL);
			j9mem_free_memory(tempSRPTable);
		}

	}

	if (recreateSRPHashTable) {
		tempSRPTable = *srptable;
		*srptable = srpHashTableRecreate(
									portLib,
									"testTable",
									(*srptable)->srpHashtableInternal,
									hashFn,
									hashEqualFn,
									NULL,
									NULL);
		j9mem_free_memory(tempSRPTable);
	}

	if (checkIntegrity(portLib, id, *srptable, data, dataLength, 0, -1) == FALSE) {
		j9tty_printf(PORTLIB, "Check integrity failure.\n");
		return FALSE;
	}

	/* remove all elements verifying the integrity */
	if (removeOffset == REVERSE) {
		j9str_printf(PORTLIB, buf, sizeof(buf), "%s reverse remove", id);
		operation = buf;
	} else if (removeOffset > 0) {
		j9str_printf(PORTLIB, buf, sizeof(buf), "%s offset %d remove", id, removeOffset);
		operation = buf;
	}
	for (i = 0; i < dataLength; i++) {
		offset = dataOffset(removeOffset, dataLength, i);
		entry = data[offset];
		if (offset < tableSize) {
			if (srpHashTableRemove(*srptable, &entry) != 0) {
				j9tty_printf(PORTLIB, "SRP Hashtable %s remove failure. Removing: %d\n", operation, entry);
				return FALSE;
			}
		} else {
			if (srpHashTableRemove(*srptable, &entry) == 0) {
				j9tty_printf(PORTLIB, "SRP Hashtable %s remove failure. Removing: %d"
						"This entry was not added to SRPHashTable due to no vacancy,"
						"but it is found and removed successfully.\n", operation, entry);
				return FALSE;
			}
		}

		if (checkIntegrity(portLib, operation, *srptable, data, dataLength, removeOffset, i) == FALSE) {
			j9tty_printf(PORTLIB, "Check integrity failure.\n");
		}
		executedFunctionCounter++;
		if (recreateSRPHashTable && (executedFunctionCounter % iterationNumber == 0)) {
			tempSRPTable = *srptable;
			*srptable = srpHashTableRecreate(
										portLib,
										"testTable",
										(*srptable)->srpHashtableInternal,
										hashFn,
										hashEqualFn,
										NULL,
										NULL);
			j9mem_free_memory(tempSRPTable);
		}
	}
	return TRUE;
}

/**
 * This functions test the functionality of srpHashTableNewInRegion.
 * It has 3 stages.
 * 1. It uses exact required memory size to store all of the elements of data array.
 * 2. It uses 1 byte short memory to store all of the elements of data array.
 * 3. It uses 1000 byte bigger memory size than the size required to store all of the elements of data array.
 *
 * @param 	portLib		Pointer to a port library.
 * @param 	id			Pointer to the test's name.
 * @param 	data		Pointer to UDATA array to be used for this testing.
 * @param	dataLength	Number of UDATA elements in the data array.
 * @param 	passCount	Pointer to the passed tests counter.
 * @param 	failCount	Pointer to the failed tests counter.
 * @return 	void
 *
 *
 */
static void
testSRPHashtableNewInRegion(J9PortLibrary *portLib, char * id, UDATA *data, U_32 dataLength, UDATA *passCount, UDATA *failCount)
{
	PORT_ACCESS_FROM_PORT(portLib);
	J9SRPHashTable * srptable = NULL;
	U_32 memorySize;
	UDATA i;
	UDATA tableSize;
	void * allocatedMemory;

	/* TEST srpHashTableNewInRegion */
	/* This is the minimum size of memory that is needed to store dataLength number of elements */
	memorySize = srpHashTable_requiredMemorySize(dataLength, sizeof(UDATA), TRUE);
	allocatedMemory =j9mem_allocate_memory(memorySize, OMRMEM_CATEGORY_VM);

	/* Check whether memory allocation was successful */
	if (!allocatedMemory) {
		goto fail;
	}
	if ((srptable = srpHashTableNewInRegion(
									portLib,
									"testTableInRegion",
									allocatedMemory,
									memorySize,
									sizeof(UDATA),
									0,
									hashFn,
									hashEqualFn,
									NULL,
									NULL)) == NULL) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s creation failure\n", id);
		goto fail;
	}

	/* Calculate expected tableSize */
	tableSize = findSmallestPrimeGreaterThanOrEqualTo(dataLength);
	if (srpHashTable_tableSize(srptable) != tableSize) {
		j9tty_printf(PORTLIB,
				"SRP Hashtable %s generated with wrong tableSize : %d, expected tableSize : %d\n",
				id,
				srpHashTable_tableSize(srptable), tableSize);
		goto fail;
	}

	if (runTests(portLib, id, &srptable, data, dataLength, REVERSE, FALSE) == FALSE) {
		goto fail;
	}

	for (i = 0; i < dataLength; i++) {
		if (runTests(portLib, id, &srptable, data, dataLength, i, FALSE) == FALSE) {
			goto fail;
		}
	}

	srpHashTableFree(srptable);
	j9mem_free_memory(allocatedMemory);
	(*passCount)++;

	/* TEST srpHashTableNewInRegion with extra large memory */
	memorySize += 1000;
	allocatedMemory = j9mem_allocate_memory(memorySize, OMRMEM_CATEGORY_VM);
	/* Check whether memory allocation was successful */
	if (!allocatedMemory) {
		goto fail;
	}

	if ((srptable = srpHashTableNewInRegion(portLib,
			"testTableInRegion",
			allocatedMemory,
			memorySize,
			sizeof(UDATA),
			0,
			hashFn,
			hashEqualFn,
			NULL,
			NULL)) == NULL) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s creation failure\n", id);
		goto fail;
	}

	if (runTests(portLib, id, &srptable, data, dataLength, REVERSE, FALSE) == FALSE) {
		goto fail;
	}

	for (i = 0; i < dataLength; i++) {
		if (runTests(portLib, id, &srptable, data, dataLength, i, FALSE) == FALSE) {
			goto fail;
		}
	}

	memorySize -= 1000;
	srpHashTableFree(srptable);
	j9mem_free_memory(allocatedMemory);
	(*passCount)++;

	/* TEST srpHashTableNewInRegion with slightly not enough memory to store all the elements (a byte short ) */
	memorySize -= 1;
	/**
	 *  Since memory size is one byte short,
	 *  max number of elements that can be stored should be the previous prime number
	 */
	allocatedMemory = j9mem_allocate_memory(memorySize, OMRMEM_CATEGORY_VM);
	/* Check whether memory allocation was successful */
	if (!allocatedMemory) {
		goto fail;
	}

	/* Calculate expected tableSize */
	tableSize = findLargestPrimeLessThanOrEqualTo(tableSize - 1);

	if ((srptable = srpHashTableNewInRegion(portLib,
			"testTableInRegion",
			allocatedMemory,
			memorySize,
			sizeof(UDATA),
			0,
			hashFn,
			hashEqualFn,
			NULL,
			NULL)) == NULL) {
		if (tableSize != 0) {
			j9tty_printf(PORTLIB, "SRP Hashtable %s creation failure\n", id);
			goto fail;
		} else {
			/**
			 *  If tableSize is 0, then srphashtable can not be created.
			 *  This is expected since there is not enough space to create minimum srphashtable
			 *  which can store smallest prime number of elements which is 2.
			 */
			goto cont;
		}
	}
	
	if (srpHashTable_tableSize(srptable) != tableSize) {
		j9tty_printf(PORTLIB,
				"SRP Hashtable %s generated with wrong tableSize : %d, expected tableSize : %d\n",
				id,
				srpHashTable_tableSize(srptable), tableSize);
		goto fail;
	}
	
	if (runTests(portLib, id, &srptable, data, dataLength, REVERSE, FALSE) == FALSE) {
		goto fail;
	}
	for (i = 0; i < dataLength; i++) {
		if (runTests(portLib, id, &srptable, data, dataLength, i, FALSE) == FALSE) {
			goto fail;
		}
	}

cont:
	if(srptable) {
		srpHashTableFree(srptable);
	}
	j9mem_free_memory(allocatedMemory);
	(*passCount)++;

	return;

fail:
	(*failCount)++;
	j9mem_free_memory(allocatedMemory);
	if (srptable) {
		srpHashTableFree(srptable);
	}
	return;

}

/**
 * This functions test the functionality of srpHashTableNew.
 *
 * @param 	portLib		Pointer to a port library.
 * @param 	id			Pointer to the test's name.
 * @param 	data		Pointer to UDATA array to be used for this testing.
 * @param	dataLength	Number of UDATA elements in the data array.
 * @param 	passCount	Pointer to the passed tests counter.
 * @param 	failCount	Pointer to the failed tests counter.
 * @return 	void
 *
 */
static void
testSRPHashtableNew(J9PortLibrary *portLib, char * id, UDATA *data, U_32 dataLength, UDATA *passCount, UDATA *failCount)
{
	PORT_ACCESS_FROM_PORT(portLib);
	J9SRPHashTable * srptable;
	UDATA i;
	
	if ((srptable = srpHashTableNew(
							portLib,
							"testTable",
							dataLength,
							sizeof(UDATA),
							0,
							hashFn,
							hashEqualFn,
							NULL,
							NULL)) == NULL) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s creation failure\n", id);
		goto fail;
	}

	if (runTests(portLib, id, &srptable, data, dataLength, REVERSE, FALSE) == FALSE) {
		goto fail;
	}
	for (i = 0; i < dataLength; i++) {
		if (runTests(portLib, id, &srptable, data, dataLength, i, FALSE) == FALSE) {
			goto fail;
		}
	}
	srpHashTableFree(srptable);
	(*passCount)++;
	
	return;

fail:
	(*failCount)++;
	srpHashTableFree(srptable);
	return;
}

/**
 * This functions test the functionality of srpHashTableRecreate.
 *
 * @param 	portLib		Pointer to a port library.
 * @param 	id			Pointer to the test's name.
 * @param 	data		Pointer to UDATA array to be used for this testing.
 * @param	dataLength	Number of UDATA elements in the data array.
 * @param 	passCount
 * @param 	failCount
 * @return 	void
 *
 */
static void
testSRPHashtableRecreate(J9PortLibrary *portLib, char * id, UDATA *data, U_32 dataLength, UDATA *passCount, UDATA *failCount)
{
	PORT_ACCESS_FROM_PORT(portLib);
	J9SRPHashTable * srptable;
	UDATA i;
	if ((srptable = srpHashTableNew(
							portLib,
							"testTable",
							dataLength,
							sizeof(UDATA),
							0,
							hashFn,
							hashEqualFn,
							NULL,
							NULL)) == NULL) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s creation failure\n", id);
		goto fail;
	}

	if (runTests(portLib, id, &srptable, data, dataLength, REVERSE, TRUE) == FALSE) {
		goto fail;
	}
	for (i = 0; i < dataLength; i++) {
		if (runTests(portLib, id, &srptable, data, dataLength, i, TRUE) == FALSE) {
			goto fail;
		}
	}
	j9mem_free_memory(srptable->srpHashtableInternal);
	j9mem_free_memory(srptable);
	(*passCount)++;

	return;

fail:
	(*failCount)++;
	srpHashTableFree(srptable);
	return;
}

/**
 * This functions test the functionality of srpHashTableVerify.
 *
 * @param 	portLib		Pointer to a port library.
 * @param 	id			Pointer to the test's name.
 * @param 	data		Pointer to UDATA array to be used for this testing.
 * @param	dataLength	Number of UDATA elements in the data array.
 * @param 	passCount
 * @param 	failCount
 * @return 	void
 *
 */
static void
testSRPHashtableVerify(J9PortLibrary *portLib, char * id, UDATA *data, U_32 dataLength, UDATA *passCount, UDATA *failCount)
{
	PORT_ACCESS_FROM_PORT(portLib);
	J9SRPHashTable * srptable;
	UDATA i, entry;
	U_32 tableSize;
	void * addedNode;
	J9SRPHashTableInternal * srphashtableInternal;
	J9SimplePool * simplepool;
	U_32 srphashtable_memorySize;
	void *addedSimplePoolEntry;

	if ((srptable = srpHashTableNew(
							portLib,
							"testTable",
							dataLength,
							sizeof(UDATA),
							0,
							hashFn,
							hashEqualFn,
							NULL,
							NULL)) == NULL) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s creation failure\n", id);
		goto fail;
	}

	tableSize = srpHashTable_tableSize(srptable);

	/* add all the data elements */
	for (i = 0; i < dataLength; i++) {
		entry = data[i];
		addedNode = srpHashTableAdd(srptable, &entry);

		if (addedNode == NULL) {
			if (i < tableSize) {
				/* Failure to allocate a new node */
				j9tty_printf(PORTLIB, "SRP Hashtable %s add failure. Adding %d\n", id, entry);
				goto fail;
			}
		} else {
			if (IS_NEW_ELEMENT(addedNode)) {
				UNMARK_NEW_ELEMENT(addedNode, void *);
				*(UDATA *)(addedNode) = entry;
				if (i >= tableSize) {
					/* We can not add more elements than tableSize into srphashtable */
					j9tty_printf(PORTLIB, "SRP Hashtable %s add failure. "
							"There is no space left to add the entry %d but it is added successfully.\n", id, entry);
					goto fail;
				}
			}
		}
	}

	/* Make sure srpHashTable verifies before we corrupt srpHashTable data */
	if (!srpHashTableVerify(srptable, srpHashTable_requiredMemorySize(tableSize, sizeof(UDATA), TRUE), sizeof(UDATA))) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s can not be verified during the setup of testing srpHashTableVerify ", id);
		goto fail;
	}

	srphashtableInternal =  srptable->srpHashtableInternal;
	simplepool = J9SRPHASHTABLEINTERNAL_NODEPOOL(srphashtableInternal);
	srphashtable_memorySize = srpHashTable_requiredMemorySize(tableSize, sizeof(UDATA), TRUE);
	
	/* Try to verify with a wrong entrySize */
	if (srpHashTableVerify(srptable, srphashtable_memorySize, sizeof(UDATA) + 1)) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s verifies with a wrong entrySize \n", id);
		goto fail;
	}

	/* Corrupt entrySize in srpHashtable */
	srphashtableInternal->entrySize++;
	if (srpHashTableVerify(srptable, srphashtable_memorySize, sizeof(UDATA))) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s verifies although its entrySize is corrupted ", id);
		goto fail;
	}
	srphashtableInternal->entrySize--;

	/* Corrupt nodeSize in srpHashtable */
	srphashtableInternal->nodeSize++;
	if (srpHashTableVerify(srptable, srphashtable_memorySize, sizeof(UDATA))) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s verifies although its nodeSize is corrupted \n", id);
		goto fail;
	}
	srphashtableInternal->nodeSize--;

	/* Try to verify it with wrong memorySize */
	if (srpHashTableVerify(srptable, srphashtable_memorySize * 2, sizeof(UDATA))) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s verifies with wrong memorySize \n", id);
		goto fail;
	}

	/* Corrupt the number of nodes in srpHashTable */
	srphashtableInternal->numberOfNodes++;
	if (srpHashTableVerify(srptable, srphashtable_memorySize, sizeof(UDATA))) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s verifies with wrong numberOfNodes in srpHashTable \n", id);
		goto fail;
	}
	srphashtableInternal->numberOfNodes--;

	/* Add new entry to simplepool directly */
	addedSimplePoolEntry = simplepool_newElement(simplepool);
	if (srpHashTableVerify(srptable, srphashtable_memorySize, sizeof(UDATA))) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s verifies with wrong numberOfNodes in srpHashTable \n", id);
		goto fail;
	}
	simplepool_removeElement(simplepool, addedSimplePoolEntry);

	/* Do a sanity check */
	if (!srpHashTableVerify(srptable, srpHashTable_requiredMemorySize(tableSize, sizeof(UDATA), TRUE), sizeof(UDATA))) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s can not be verified during the setup of testing srpHashTableVerify ", id);
		goto fail;
	}

	/* corrupt simplepool address */
	srphashtableInternal->nodePool += 4;
	if (srpHashTableVerify(srptable, srphashtable_memorySize, sizeof(UDATA))) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s verifies with a wrong simplepool address \n", id);
		goto fail;
	}
	srphashtableInternal->nodePool -= 4;

	/* corrupt SRPs address */
	srphashtableInternal->nodes += 4;
	if (srpHashTableVerify(srptable, srphashtable_memorySize, sizeof(UDATA))) {
		j9tty_printf(PORTLIB, "SRP Hashtable %s verifies with a wrong SRP address \n", id);
		goto fail;
	}
	srphashtableInternal->nodes -= 4;

	j9mem_free_memory(srptable->srpHashtableInternal);
	j9mem_free_memory(srptable);
	(*passCount)++;

	return;

fail:
	(*failCount)++;
	srpHashTableFree(srptable);
	return;
}

/**
 * Verifies the functionality of SRPHashTable.
 *
 * @param 	portLib 	Pointer to a port library.
 * @param 	passCount	Pointer to the passed tests counter.
 * @param 	failCount	Pointer to the failed tests counter.
 * @return	0
 */
I_32 
verifySRPHashtable(J9PortLibrary *portLib, UDATA *passCount, UDATA *failCount)
{
	I_32 rc = 0;
	PORT_ACCESS_FROM_PORT(portLib);
	UDATA start, end;
	UDATA randSize, i, orgFail;
	U_32 tableSize;
	UDATA data1[] = {1, 2, 3, 4, 5, 6, 7, 17, 18, 19, 20, 21, 22, 23, 24, 25};
	UDATA data2[] = {1, 3, 5, 7, 18, 20, 22, 23, 24, 35, 36, 37};
	UDATA data3[] = {25, 24, 23, 22, 21, 20, 19, 18, 17, 7, 6, 5, 4, 3, 2, 1};
	UDATA data4[] = {37, 36, 35, 24, 23, 22, 20, 18, 7, 5, 3, 1};
	UDATA data5[] = {17, 14, 8, 73, 23, 38, 91, 84, 31, 25, 11, 41, 26, 87, 35, 6};
	UDATA data6[] = {17, 14, 8, 73, 23, 38, 91, 84, 31, 25, 11, 41, 26, 87, 35, 6, 13, 61, 10, 20, 92};
	UDATA randData[256];

	j9tty_printf(PORTLIB, "Testing SRP hashtable functions...\n");

	start = j9time_usec_clock();

	testSRPHashtableNew(portLib, "test1", data1, sizeof(data1) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableNewInRegion(portLib, "test2", data1, sizeof(data1) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableRecreate(portLib, "test3", data1, sizeof(data1) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableVerify(portLib, "test4", data1, sizeof(data1) / sizeof(UDATA), passCount, failCount);

	testSRPHashtableNew(portLib, "test5", data2, sizeof(data2) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableNewInRegion(portLib, "test6", data2, sizeof(data2) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableRecreate(portLib, "test7", data2, sizeof(data2) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableVerify(portLib, "test8", data2, sizeof(data2) / sizeof(UDATA), passCount, failCount);

	testSRPHashtableNew(portLib, "test9", data3, sizeof(data3) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableNewInRegion(portLib, "test10", data3, sizeof(data3) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableRecreate(portLib, "test11", data3, sizeof(data3) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableVerify(portLib, "test12", data3, sizeof(data3) / sizeof(UDATA), passCount, failCount);

	testSRPHashtableNew(portLib, "test13", data4, sizeof(data4) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableNewInRegion(portLib, "test14", data4, sizeof(data4) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableRecreate(portLib, "test15", data4, sizeof(data4) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableVerify(portLib, "test16", data4, sizeof(data4) / sizeof(UDATA), passCount, failCount);

	testSRPHashtableNew(portLib, "test17", data5, sizeof(data5) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableNewInRegion(portLib, "test18", data5, sizeof(data5) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableRecreate(portLib, "test19", data5, sizeof(data5) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableVerify(portLib, "test20", data5, sizeof(data5) / sizeof(UDATA), passCount, failCount);

	testSRPHashtableNew(portLib, "test21", data6, sizeof(data6) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableNewInRegion(portLib, "test22", data6, sizeof(data6) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableRecreate(portLib, "test23", data6, sizeof(data6) / sizeof(UDATA), passCount, failCount);
	testSRPHashtableVerify(portLib, "test24", data6, sizeof(data6) / sizeof(UDATA), passCount, failCount);


	for (i = 0; i < sizeof(RandomValues); i++) {
		UDATA j;
		UDATA k;
		UDATA offset = i;
		char name[32];

		j9str_printf(PORTLIB, name, sizeof(name), "randomData%d", i);
		if (RandomValues[i] == 0) {
			continue;
		}
		randSize = RandomValues[offset++] % 38;
		for (j = 0; j < randSize; j++) {
			if (offset == 256) {
				offset = 0;
			}
			randData[j] = RandomValues[offset++];
			if (randData[j] == 0) {
				if (offset == 256) {
					offset = 0;
				}
				randData[j] = RandomValues[offset++];
			}
		}
		tableSize = (U_32)randSize;
		orgFail = *failCount;
		testSRPHashtableNew(portLib, name, randData, tableSize, passCount, failCount);
		if (orgFail != *failCount) {
			j9tty_printf(PORTLIB, "random data:\n");
			for (k = 0; k < randSize; k++) {
				j9tty_printf(PORTLIB, "%03d ", randData[k]);
				if ((k+1) % 16 == 0) {
					j9tty_printf(PORTLIB, "\n");
				}
			}
			j9tty_printf(PORTLIB, "\n");
		}

		orgFail = *failCount;
		testSRPHashtableNewInRegion(portLib, name, randData, tableSize, passCount, failCount);
		if (orgFail != *failCount) {
			j9tty_printf(PORTLIB, "random data:\n");
			for (k = 0; k < randSize; k++) {
				j9tty_printf(PORTLIB, "%03d ", randData[k]);
				if ((k+1) % 16 == 0) {
					j9tty_printf(PORTLIB, "\n");
				}
			}
			j9tty_printf(PORTLIB, "\n");
		}
	}

	end = j9time_usec_clock();
	j9tty_printf(PORTLIB, "Finished testing SRP hashtable functions.\n");
	j9tty_printf(PORTLIB, "SRP Hashtable functions execution time was %d (usec).\n", (end-start));

	return rc;
}

/**
 * This function calculates the number of elements that should be left in the SRPHashTable with tableSize
 * RemoveOffset is used as start point in data array.
 * We check whether next or previous(if removeOffset == -1) i elements should be removed from SRPHashTable.
 * If dataLength is bigger than tableSize, then first tableSize elements of data array should have been added,
 * thus only first tableSize elements of data array can be removed. Trying to remove the rest will not cause
 * any affect on SRPHashTable and its tableSize.
 *
 *	@param removeOffset		Is the offset where removal starts.
 *	@param i				number of iterations that remove is executed.
 *	@param dataLength		Length of the data array used for this SRPHashtable test
 *	@param tableSize		Size of SRPHashTable that is created for this test.
 */
static UDATA
getExpectedNumOfElements(UDATA removeOffset, UDATA i, UDATA dataLength, UDATA tableSize)
{
	UDATA j, removedElementCount, count;

	removedElementCount = 0;

	if (dataLength > tableSize) {
		for (j = 0; j < i + 1; j++) {
			if (dataOffset(removeOffset, dataLength, j) < tableSize) {
				removedElementCount++;
			}
		}
		count = tableSize - removedElementCount;
	} else {
		count = dataLength - (i + 1);
	}
	return count;
}
