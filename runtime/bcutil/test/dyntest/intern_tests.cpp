/*******************************************************************************
 * Copyright (c) 2008, 2019 IBM Corp. and others
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

#include "j9comp.h"
#include "j9.h"
#include "omravldefines.h"
#include "simplepool_api.h"

#include "testHelpers.h"

#include "../../StringInternTable.hpp"
#include "../../bcutil_internal.h"
#include "SCStringInternHelpers.h"
#include "srphashtable_api.h"
#include "../bcutil/ROMClassBuilder.hpp"
#include "j9modron.h"

#include <stdlib.h>

/* Utility functions. */
static J9UTF8 *portAllocUTF8(J9PortLibrary *portLib, const char *string);
static bool nodeFieldsEqual(J9InternHashTableEntry *node, J9UTF8 *utf8, J9ClassLoader *classLoader, J9InternHashTableEntry *nextNode, J9InternHashTableEntry *prevNode);
static bool searchResultFieldsEqual(J9InternSearchResult *searchResult, J9UTF8 *utf8, J9InternHashTableEntry *node, bool isSharedNode);
static J9SharedInvariantInternTable *createJ9SharedInvariantInternTable(J9PortLibrary *portLib, J9ClassLoader *systemClassLoader, J9SharedCacheHeader *dummyHeader, void * allocatedMemory, U_32 memorySize);
static void freeJ9SharedInvariantInternTable(J9PortLibrary *portLib, J9SharedInvariantInternTable *invariantInternTable);

/* Actual test functions. */
static IDATA testStringInternTableWithZeroSize(J9PortLibrary *portLib);
static IDATA testStringInternTableWithSize1(J9PortLibrary *portLib);
static IDATA testStringInternTableRemoveLocalNodesWithDeadClassLoaders(J9PortLibrary *portLib);
static IDATA testStringInternTableStressLocal(J9PortLibrary *portLib, UDATA numIterations);
static IDATA testStringInternTableStressShared(J9PortLibrary *portLib, UDATA numIterations);


static J9UTF8 *
portAllocUTF8(J9PortLibrary *portLib, const char *string)
{
	PORT_ACCESS_FROM_PORT(portLib);
	UDATA length = strlen(string);

	if (0 != (length & 1)) {
		/* UTF8s are always even-aligned. */
		length++;
	}

	J9UTF8 *utf8 = (J9UTF8*)j9mem_allocate_memory(sizeof(U_16) + length, J9MEM_CATEGORY_CLASSES);
	memcpy(J9UTF8_DATA(utf8), string, length);
	J9UTF8_SET_LENGTH(utf8, U_16(length));

	return utf8;
}


static bool
nodeFieldsEqual(J9InternHashTableEntry *node, J9UTF8 *utf8, J9ClassLoader *classLoader, J9InternHashTableEntry *nextNode, J9InternHashTableEntry *prevNode)
{
	return (NULL != node) && (utf8 == node->utf8) && (classLoader == node->classLoader) &&
		(nextNode == node->nextNode) && (prevNode == node->prevNode);
}


static bool
searchResultFieldsEqual(J9InternSearchResult *searchResult, J9UTF8 *utf8, J9InternHashTableEntry *node, bool isSharedNode)
{
	return (NULL != searchResult) && (utf8 == searchResult->utf8) && (node == searchResult->node) && (isSharedNode == searchResult->isSharedNode);
}


static IDATA
testStringInternTableWithZeroSize(J9PortLibrary *portLib)
{
	PORT_ACCESS_FROM_PORT(portLib);
	const char * testName = "testStringInternTableWithZeroSize";

	bool found;
	J9UTF8 *utf8;
	J9ClassLoader dummyClassLoader;
	J9InternSearchInfo searchInfo;
	J9InternSearchResult searchResult;

	reportTestEntry(PORTLIB, testName);

	StringInternTable stringInternTable(NULL, portLib, 0);
	/* Zero-size table should be OK - this is the case when interning is not enabled. */
	if (!stringInternTable.isOK()) {
		outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.isOK() failed!\n");
		goto _exit_test;
	}

	/* Verify that we don't crash trying to intern(). */
	utf8 = portAllocUTF8(portLib, "test");

	stringInternTable.internUtf8(utf8, &dummyClassLoader);
	if (NULL != stringInternTable.getLRUHead()) {
		outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.getLRUHead() returned a node!\n");
		goto _exit_test;
	}

	/* Verify that we don't crash trying to find(). */
	searchInfo.stringData = J9UTF8_DATA(utf8);
	searchInfo.stringLength = J9UTF8_LENGTH(utf8);
	searchInfo.classloader = &dummyClassLoader;

	found = stringInternTable.findUtf8(&searchInfo, NULL, /* requiresSharedUtf8 = */ false, &searchResult);
	if (found) {
		outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.findUtf8() returned true!\n");
		goto _exit_test;
	}

_exit_test:
	return reportTestExit(PORTLIB, testName);
}


static IDATA
testStringInternTableWithSize1(J9PortLibrary *portLib)
{
	const char * testName = "testStringInternTableWithSize1";
	PORT_ACCESS_FROM_PORT(portLib);

	/* Note: Only the address of the classLoader is used for intern matching, so this dummy one will do. */
	J9ClassLoader dummyClassLoader;
	const char *utf8StringData;
	J9UTF8 *utf8;
	J9UTF8 *secondUtf8;
	J9InternHashTableEntry *head;
	J9InternSearchInfo searchInfo;
	J9InternSearchResult searchResult;
	bool found;

	reportTestEntry(PORTLIB, testName);

	StringInternTable stringInternTable(NULL, portLib, 1);
	if (!stringInternTable.isOK()) {
		outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.isOK() failed!\n");
		goto _exit_test;
	}

	utf8StringData = "Batman";
	utf8 = portAllocUTF8(portLib, utf8StringData);
	if (NULL == utf8) {
		outputErrorMessage(TEST_ERROR_ARGS, "portAllocUTF8() failed!\n");
		goto _exit_test;
	}

	/* Intern a UTF8 and check whether it was inserted into the LRU. */
	stringInternTable.internUtf8(utf8, &dummyClassLoader);

	head = stringInternTable.getLRUHead();
	if (!nodeFieldsEqual(head, utf8, &dummyClassLoader, NULL, NULL)) {
		outputErrorMessage(TEST_ERROR_ARGS, "nodeFieldsEqual() returned false!\n");
		goto _exit_test;
	}

	/* Now, verify that we can look it up. */
	searchInfo.stringData = (U_8*)utf8StringData;
	searchInfo.stringLength = J9UTF8_LENGTH(utf8);
	searchInfo.classloader = &dummyClassLoader;
	searchInfo.romClassBaseAddr = (U_8*)utf8 + 10;
	searchInfo.romClassEndAddr = (U_8*)utf8 + 20;
	searchInfo.sharedCacheSRPRangeInfo = SC_COMPLETELY_OUT_OF_THE_SRP_RANGE;

	found = stringInternTable.findUtf8(&searchInfo, NULL, /* requiresSharedUtf8 = */ false, &searchResult);
	if (!found) {
		outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.findUtf8() returned false!\n");
		goto _exit_test;
	}
	if (!searchResultFieldsEqual(&searchResult, utf8, head, false)) {
		outputErrorMessage(TEST_ERROR_ARGS, "searchResultFieldsEqual() returned false!\n");
		goto _exit_test;
	}

	/* Intern another node and verify it replaces the existing one. */
	utf8StringData = "Joker";
	secondUtf8 = portAllocUTF8(portLib, utf8StringData);
	if (NULL == secondUtf8) {
		outputErrorMessage(TEST_ERROR_ARGS, "portAllocUTF8() failed!\n");
		goto _exit_test;
	}

	stringInternTable.internUtf8(secondUtf8, &dummyClassLoader);

	head = stringInternTable.getLRUHead();
	if (!nodeFieldsEqual(head, secondUtf8, &dummyClassLoader, NULL, NULL)) {
		outputErrorMessage(TEST_ERROR_ARGS, "nodeFieldsEqual() returned false!\n");
		goto _exit_test;
	}

	found = stringInternTable.findUtf8(&searchInfo, NULL, /* requiresSharedUtf8 = */ false, &searchResult);
	if (found) {
		outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.findUtf8() returned true!\n");
		goto _exit_test;
	}

	/* Now, find the newly inserted node. */
	searchInfo.stringData = (U_8*)utf8StringData;
	searchInfo.stringLength = J9UTF8_LENGTH(secondUtf8);
	/*
	 *  Make sure that ROM class is in SRP range of found UTF8.
	 * 	In order to return the UTF8, range check between ROM Class and found UTF8 should pass
	 */
	searchInfo.romClassBaseAddr = (U_8*)secondUtf8 + 10;
	searchInfo.romClassEndAddr = (U_8*)secondUtf8 + 20;

	found = stringInternTable.findUtf8(&searchInfo, NULL, /* requiresSharedUtf8 = */ false, &searchResult);
	if (!found) {
		outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.findUtf8() returned false!\n");
		goto _exit_test;
	}
	if (!searchResultFieldsEqual(&searchResult, secondUtf8, head, false)) {
		outputErrorMessage(TEST_ERROR_ARGS, "searchResultFieldsEqual() returned false!\n");
		goto _exit_test;
	}

_exit_test:
	j9mem_free_memory(utf8);
	j9mem_free_memory(secondUtf8);
	return reportTestExit(PORTLIB, testName);
}


static IDATA
testStringInternTableSRPRangeCheck(J9PortLibrary *portLib)
{
	const char * testName = "testStringInternTableRange";
	PORT_ACCESS_FROM_PORT(portLib);

	/* Note: Only the address of the classLoader is used for intern matching, so these dummy ones will do. */
	J9ClassLoader dummyClassLoader;
	J9ClassLoader dummyClassLoader2;
	J9ClassLoader dummySystemClassLoader;
	/* Not needed for this testing. Used as one parameter to create shared invariant intern table */
	J9SharedCacheHeader dummyHeader;
	/* String to be interned in string intern tables */
	const char *commonString;
	J9UTF8 * localCommonUTF8;
	J9UTF8 * localCommonUTF8_2;
	J9UTF8 * sharedCommonUTF8;
	J9InternSearchInfo searchInfo;
	J9InternSearchResult searchResult;
	bool found;
	J9SharedInvariantInternTable * invariantInternTable = NULL;
	void * allocatedMemory = NULL;

	reportTestEntry(PORTLIB, testName);

	/* Create string intern table */
	StringInternTable stringInternTable(NULL, portLib, 2);
	if (!stringInternTable.isOK()) {
		outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.isOK() failed!\n");
		goto _exit_test;
	}

	/*
	 * Allocate memory for SRPHashTable and UTF8s
	 * Use first 1000 bytes for SRPHashtable which is big enough to have more than 10 nodes
	 * Use the rest for UT8s
	 */
	allocatedMemory = j9mem_allocate_memory(5000, J9MEM_CATEGORY_CLASSES);
	if (NULL == allocatedMemory) {
		outputErrorMessage(TEST_ERROR_ARGS, "j9mem_allocate_memory(%u) failed!\n", 5000);
		goto _exit_test;
	}

	invariantInternTable = createJ9SharedInvariantInternTable(portLib, &dummySystemClassLoader, &dummyHeader, allocatedMemory, 1000);

	if (NULL == invariantInternTable) {
		outputErrorMessage(TEST_ERROR_ARGS, "createJ9SharedInvariantInternTable() failed!\n");
		goto _exit_test;
	}

	/**
	 * Build three UTF8. Make sure that shared one which will be interned into shared string intern table is in the middle regarding to memory addresses.
	 * localCommonUTF8 : interned into local string intern table; classLoader = dummyClassLoader; sharedUTF8 = false;
	 * sharedCommonUTF8 : interned into shared string intern table; sharedUTF8 = true
	 * localCommonUTF8_2 : interned into local string intern table; classLoader = dummyClassLoader2; sharedUTF8 = true
	 *
	 */
	commonString = "commonUTF8";
	localCommonUTF8 = (J9UTF8*)((U_8*)allocatedMemory + 2000);
	memcpy(J9UTF8_DATA(localCommonUTF8), commonString, strlen(commonString));
	J9UTF8_SET_LENGTH(localCommonUTF8, U_16(strlen(commonString)));

	sharedCommonUTF8 = (J9UTF8*)((U_8*)allocatedMemory + 3000);
	memcpy(sharedCommonUTF8, localCommonUTF8, sizeof(U_16) + strlen(commonString));

	localCommonUTF8_2 = (J9UTF8*)((U_8*)allocatedMemory + 4000);
	memcpy(localCommonUTF8_2, localCommonUTF8, sizeof(U_16) + strlen(commonString));


	/* This is the first interned string and should be interned into shared string intern table */
	invariantInternTable->flags = 0;
	stringInternTable.internUtf8(sharedCommonUTF8, &dummyClassLoader, true /* fromSharedROMClass */, invariantInternTable);

	/* Utf8s will be interned into local string intern table */
	invariantInternTable->flags = J9AVLTREE_DISABLE_SHARED_TREE_UPDATES;
	stringInternTable.internUtf8(localCommonUTF8, &dummyClassLoader, false /* fromSharedROMClass */, invariantInternTable);
	stringInternTable.internUtf8(localCommonUTF8_2, &dummyClassLoader2, true /* fromSharedROMClass */, invariantInternTable);

	/* Now, verify that we can look it up. */
	searchInfo.stringData = (U_8*)commonString;
	searchInfo.stringLength = (UDATA)strlen(commonString);
	/* Let ROM Class be in SRP range to all UTF8s */
	searchInfo.romClassBaseAddr = (U_8*)allocatedMemory + 8000;
	searchInfo.romClassEndAddr = (U_8*)allocatedMemory + 10000;

	/**
	 *	Test: When shared cache is out of SRP range, local string table needs to be search and non sharedUTF8 should be returned if one exist.
	 *	sharedCacheSRPRangeInfo = 1
	 *	Success : localCommonUTF8 is found
	 */
	/* 1 (SC_COMPLETELY_OUT_OF_THE_SRP_RANGE): Shared cache is out of range */
	searchInfo.sharedCacheSRPRangeInfo = SC_COMPLETELY_OUT_OF_THE_SRP_RANGE;
	searchInfo.classloader = &dummyClassLoader;
	found = stringInternTable.findUtf8(&searchInfo, invariantInternTable, /* requiresSharedUtf8 = */ false, &searchResult);
	if (!found || !(searchResult.isSharedNode == false) || (searchResult.utf8 != localCommonUTF8)) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to find local UTF8!\n");
		goto _exit_test;
	}

#if defined(J9VM_ENV_DATA64)
	/**
	 *	Test: When shared cache is out of SRP range, local string table needs to be search and non sharedUTF8 should be returned if one exist.
	 *	sharedCacheSRPRangeInfo = 1 (SC_COMPLETELY_OUT_OF_THE_SRP_RANGE)
	 *	Success : localCommonUTF8_2 is not found since it is sharedUTF8
	 */
	/* 1 (SC_COMPLETELY_OUT_OF_THE_SRP_RANGE): Shared cache is out of range */
	searchInfo.sharedCacheSRPRangeInfo = SC_COMPLETELY_OUT_OF_THE_SRP_RANGE;
	searchInfo.classloader = &dummyClassLoader2;
	found = stringInternTable.findUtf8(&searchInfo, invariantInternTable, /* requiresSharedUtf8 = */ false, &searchResult);
	if (found) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to not find local UTF8!\n");
		goto _exit_test;
	}
#endif

	/**
	 *	Test: If shared cache is in range, then shared table needs to be searched first.
	 *	sharedCacheSRPRangeInfo = 2 (SC_COMPLETELY_IN_THE_SRP_RANGE)
	 *	Success : sharedCommonUTF8 is found
	 */
	 /* 2 (SC_COMPLETELY_IN_THE_SRP_RANGE): Shared cache is in range */
	searchInfo.sharedCacheSRPRangeInfo = SC_COMPLETELY_IN_THE_SRP_RANGE;
	found = stringInternTable.findUtf8(&searchInfo, invariantInternTable, /* requiresSharedUtf8 = */ false, &searchResult);
	if (!found || !(searchResult.isSharedNode == true) || (searchResult.utf8 != sharedCommonUTF8)) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to find shared UTF8!\n");
		goto _exit_test;
	}

	/**
	 *	Test: If shared cache is partially in range, then range check is required for the found UTF8. In this test case, it should be in range.
	 *	sharedCacheSRPRangeInfo = 3 (SC_PARTIALLY_IN_THE_SRP_RANGE)
	 *	Success : sharedCommonUTF8 is found
	 */
	/* 3 (SC_PARTIALLY_IN_THE_SRP_RANGE): Part of the shared cache is in the SRP range */
	searchInfo.sharedCacheSRPRangeInfo = SC_PARTIALLY_IN_THE_SRP_RANGE;
	found = stringInternTable.findUtf8(&searchInfo, invariantInternTable, /* requiresSharedUtf8 = */ false, &searchResult);
	if (!found || !(searchResult.isSharedNode == true) || (searchResult.utf8 != sharedCommonUTF8)) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to find shared UTF8 although it was in range!\n");
		goto _exit_test;
	}

#if defined(J9VM_ENV_DATA64)
	/**
	 * Test: If shared cache is partially in range, then range check is required for the found UTF8 in shared cache. In this test case, it should not be in range.
	 * sharedUTF8 is out of SRP range
	 * sharedCacheSRPRangeInfo = 3 (SC_PARTIALLY_IN_THE_SRP_RANGE)
	 * Success : one of the local utf8s is found.
	 *
	 */
	/* Start and end addresses of ROM classes is just used for range check so can be equal */

	searchInfo.romClassBaseAddr = (U_8*)((UDATA)sharedCommonUTF8 + 0x7FFFFFFF + 1);
	searchInfo.romClassEndAddr = (U_8*)((UDATA)sharedCommonUTF8 + 0x7FFFFFFF + 10);

	searchInfo.classloader = &dummyClassLoader2;
	found = stringInternTable.findUtf8(&searchInfo, invariantInternTable, /* requiresSharedUtf8 = */ false, &searchResult);
	if (!found || !(searchResult.isSharedNode == false) || (searchResult.utf8 != localCommonUTF8_2)) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to find local UTF8 2!\n");
		goto _exit_test;
	}

	/**
	 * Test: If shared cache is partially in range, then range check is required for the found UTF8 in shared cache.
	 * If found UTF8 is in the range of part of the rom class, then it should not be used.
	 * sharedUTF8 is in SRP range of part of ROM Class
	 * sharedCacheSRPRangeInfo = 3
	 * Success : one of the local utf8s is found.
	 *
	 */
	if (((UDATA)sharedCommonUTF8 + 0x7FFFFFFF + 1) > 0x7FFFFFFF) {
		/* Start and end addresses of ROM classes is just used for range check so can be equal */
		searchInfo.romClassBaseAddr = (U_8*)((UDATA)sharedCommonUTF8 + 0x7FFFFFFF - 10);
		searchInfo.romClassEndAddr = (U_8*)((UDATA)sharedCommonUTF8 + 0x7FFFFFFF + 1);
		searchInfo.classloader = &dummyClassLoader2;
		found = stringInternTable.findUtf8(&searchInfo, invariantInternTable, /* requiresSharedUtf8 = */ false, &searchResult);
		if (!found || !(searchResult.isSharedNode == false) || (searchResult.utf8 != localCommonUTF8_2)) {
			outputErrorMessage(TEST_ERROR_ARGS, "Failed to find local UTF8 2!\n");
			goto _exit_test;
		}
	} else {
		searchInfo.romClassBaseAddr = (U_8*)((UDATA)sharedCommonUTF8 - 0x7FFFFFFF + 10);
		searchInfo.romClassEndAddr = (U_8*)((UDATA)sharedCommonUTF8 - 0x7FFFFFFF - 1);
		searchInfo.classloader = &dummyClassLoader;
		found = stringInternTable.findUtf8(&searchInfo, invariantInternTable, /* requiresSharedUtf8 = */ false, &searchResult);
		if (!found || !(searchResult.isSharedNode == false) || (searchResult.utf8 != localCommonUTF8)) {
			outputErrorMessage(TEST_ERROR_ARGS, "Failed to find local UTF8 !\n");
			goto _exit_test;
		}
	}
#endif

	/* Just test local string intern table logic from now on */
	/* 1 (SC_COMPLETELY_OUT_OF_THE_SRP_RANGE): Shared cache is out of range */
	searchInfo.sharedCacheSRPRangeInfo = SC_COMPLETELY_OUT_OF_THE_SRP_RANGE;
	/**
	 * node->utf8 can be used in 3 conditions out of 4.
	 * 1. sharedUTF8 && isSharedROMClass	-	node->utf8 can be used and no SRP range check is required for 64 bit environment.
	 * 											(sharedCacheSRPRangeInfo = SC_COMPLETELY_IN_THE_SRP_RANGE)
	 * 2. sharedUTF8 && !isSharedROMClass	-	node->utf8 maybe used if it if in SRP range.
	 * 											(sharedCacheSRPRangeInfo = SC_COMPLETELY_OUT_OF_THE_SRP_RANGE, SC_COMPLETELY_IN_THE_SRP_RANGE or SC_PARTIALLY_IN_THE_SRP_RANGE)
	 * 3. !sharedUTF8 && isSharedROMClass	-	node->utf8 can not be used.
	 * 											ROM classes in shared cache can only use sharedUTF8s in the shared cache.
	 * 4. !sharedUTF8 && !isSharedROMClass	-	node->utf8 maybe used if it if in SRP range.
	 * 											(sharedCacheSRPRangeInfo = SC_COMPLETELY_OUT_OF_THE_SRP_RANGE, SC_COMPLETELY_IN_THE_SRP_RANGE or SC_PARTIALLY_IN_THE_SRP_RANGE)
	 *
	 */

	/**
	 * Test : Condition 3 above
	 * Success : Not found.
	 *
	 */
	searchInfo.classloader = &dummyClassLoader;
	/* Let ROM Class be in SRP range to all UTF8s */
	searchInfo.romClassBaseAddr = (U_8*)allocatedMemory + 8000;
	searchInfo.romClassEndAddr = (U_8*)allocatedMemory + 10000;
	found = stringInternTable.findUtf8(&searchInfo, invariantInternTable, /* requiresSharedUtf8 = */ true, &searchResult);
	if (found) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to not find any node !\n");
		goto _exit_test;
	}

	/**
	 * Test : Condition 1 above
	 * Make sure that ROM class range check fails on 64 bit environments.
	 * In this test, range check should not be run.
	 * Success : Found without range check
	 *
	 */

#if defined(J9VM_ENV_DATA64)
	/* Start and end addresses of ROM classes is just used for range check so can be equal */
	searchInfo.romClassBaseAddr = (U_8*)((UDATA)localCommonUTF8_2 + 0x7FFFFFFF + 1);
	searchInfo.romClassEndAddr = (U_8*)((UDATA)localCommonUTF8_2 + 0x7FFFFFFF + 10);
#endif
	searchInfo.classloader = &dummyClassLoader2;
	found = stringInternTable.findUtf8(&searchInfo, invariantInternTable, /* requiresSharedUtf8 = */ true, &searchResult);
	if (!found) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to find local UTF8 2!\n");
		goto _exit_test;
	}

#if defined(J9VM_ENV_DATA64)
	/**
	 * Test : Condition 2 above
	 * In this test, range check should not be run since shared cache is out of range
	 * Success : Not found.
	 *
	 */
	/* Let ROM Class be in SRP range to all UTF8s */
	searchInfo.romClassBaseAddr = (U_8*)allocatedMemory + 8000;
	searchInfo.romClassEndAddr = (U_8*)allocatedMemory + 10000;
	found = stringInternTable.findUtf8(&searchInfo, invariantInternTable, /* requiresSharedUtf8 = */ false, &searchResult);
	if (found) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to find local UTF8 2!\n");
		goto _exit_test;
	}
#endif

	/**
	 * Test : Condition 4 above
	 * In this test, range check should be run and pass
	 * Success : Found
	 *
	 */
	searchInfo.classloader = &dummyClassLoader;
	found = stringInternTable.findUtf8(&searchInfo, invariantInternTable, /* requiresSharedUtf8 = */ false, &searchResult);
	if (!found) {
		outputErrorMessage(TEST_ERROR_ARGS, "Failed to find local UTF8!\n");
		goto _exit_test;
	}

#if defined(J9VM_ENV_DATA64)
	/**
	 * Test : Condition 4 above
	 * In this test, range check should be run and fail
	 * Success : Not found.
	 *
	 */
	if ((UDATA)localCommonUTF8 < 0x7FFFFFFF) {
		/* Start and end addresses of ROM classes is just used for range check so can be equal */
		searchInfo.romClassBaseAddr = (U_8*)((UDATA)localCommonUTF8 + 0x7FFFFFFF + 1);
		searchInfo.romClassEndAddr = (U_8*)((UDATA)localCommonUTF8 + 0x7FFFFFFF + 10);

		found = stringInternTable.findUtf8(&searchInfo, invariantInternTable, /* requiresSharedUtf8 = */ false, &searchResult);
		if (found) {
			outputErrorMessage(TEST_ERROR_ARGS, "Failed to find local UTF8!\n");
			goto _exit_test;
		}
	}
#endif

_exit_test:
	j9mem_free_memory(allocatedMemory);
	if (NULL != invariantInternTable) {
		freeJ9SharedInvariantInternTable(portLib, invariantInternTable);
	}
	return reportTestExit(PORTLIB, testName);
}

static IDATA
testStringInternTableRemoveLocalNodesWithDeadClassLoaders(J9PortLibrary *portLib)
{
	const char * testName = "testStringInternTableRemoveLocalNodesWithDeadClassLoaders";
	PORT_ACCESS_FROM_PORT(portLib);

	J9ClassLoader dummyClassLoader1, dummyClassLoader2;
	J9UTF8 *utf8a, *utf8b, *utf8c;
	UDATA nodeCount;
	J9InternHashTableEntry *node;

	reportTestEntry(PORTLIB, testName);

	StringInternTable stringInternTable(NULL, portLib, 10);
	if (!stringInternTable.isOK()) {
		outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.isOK() failed!\n");
		goto _exit_test;
	}

	utf8a = portAllocUTF8(portLib, "foo");
	utf8b = portAllocUTF8(portLib, "bar");
	utf8c = portAllocUTF8(portLib, "bam");
	if ((NULL == utf8a) || (NULL == utf8b) || (NULL == utf8c)) {
		outputErrorMessage(TEST_ERROR_ARGS, "portAllocUTF8() failed!\n");
		goto _exit_test;
	}

	stringInternTable.internUtf8(utf8a, &dummyClassLoader1);
	stringInternTable.internUtf8(utf8b, &dummyClassLoader1);
	stringInternTable.internUtf8(utf8a, &dummyClassLoader2);
	stringInternTable.internUtf8(utf8c, &dummyClassLoader2);

	/* Verify that the above nodes were inserted by counting them. */
	nodeCount = 0;
	node = stringInternTable.getLRUHead();
	while (NULL != node) {
		nodeCount++;
		node = node->nextNode;
	}

	if (4 != nodeCount) {
		outputErrorMessage(TEST_ERROR_ARGS, "4 != nodeCount (got %d)!\n", nodeCount);
		goto _exit_test;
	}

	dummyClassLoader1.gcFlags |= J9_GC_CLASS_LOADER_DEAD;
	dummyClassLoader2.gcFlags &= !J9_GC_CLASS_LOADER_DEAD;

	stringInternTable.removeLocalNodesWithDeadClassLoaders();

	/* Verify that nodes with that classLoader have been removed, while the remaining nodes are still there. */
	nodeCount = 0;
	node = stringInternTable.getLRUHead();
	while (NULL != node) {
		if (node->classLoader == &dummyClassLoader1) {
			outputErrorMessage(TEST_ERROR_ARGS, "removeLocalNodesWithDeadClassLoaders() failed!\n");
			goto _exit_test;
		}
		nodeCount++;
		node = node->nextNode;
	}

	if (2 != nodeCount) {
		outputErrorMessage(TEST_ERROR_ARGS, "2 != nodeCount (got %d)!\n", nodeCount);
		goto _exit_test;
	}

_exit_test:
	j9mem_free_memory(utf8a);
	j9mem_free_memory(utf8b);
	j9mem_free_memory(utf8c);
	return reportTestExit(PORTLIB, testName);
}


static IDATA
testStringInternTableStressLocal(J9PortLibrary *portLib, UDATA numIterations)
{
	const char * testName = "testStringInternTableStressLocal";
	PORT_ACCESS_FROM_PORT(portLib);

	J9UTF8 *utf8s[5000];
	const UDATA utfs8Count = sizeof(utf8s)/sizeof(utf8s[0]);

	/* Class loaders are not initialized as currently the StringInternTable only uses their addresses. */
	J9ClassLoader classLoaders[25];
	const UDATA classLoadersCount = sizeof(classLoaders)/sizeof(classLoaders[0]);

	reportTestEntry(PORTLIB, testName);

	StringInternTable stringInternTable(NULL, portLib, 500);
	if (!stringInternTable.isOK()) {
		outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.isOK() failed!\n");
		goto _exit_test;
	}

	/* Generate random UTF8s. */
	for (UDATA i = 0; i < utfs8Count; i++) {
		UDATA length = UDATA(rand() % 256);
		UDATA allocSize = sizeof(U_16) + length + (0 != (length & 1) ? 1 : 0);

		utf8s[i] = (J9UTF8*)j9mem_allocate_memory(allocSize, J9MEM_CATEGORY_CLASSES);
		if (NULL == utf8s[i]) {
			outputErrorMessage(TEST_ERROR_ARGS, "j9mem_allocate_memory(%u) failed for utf8!\n", allocSize);
			goto _exit_test;
		}
		J9UTF8_SET_LENGTH(utf8s[i], U_16(length));
		for (UDATA j = 0; j < length; j++) {
			J9UTF8_DATA(utf8s[i])[j] = U_8(0x20 + (rand() % (0x7E - 0x20)));
		}
	}

	for (UDATA i = 0; i < numIterations; i++) {
		UDATA utf8Index = (rand() % utfs8Count);
		UDATA classLoaderIndex = (rand() % classLoadersCount);

		J9InternSearchInfo searchInfo;
		searchInfo.stringData = J9UTF8_DATA(utf8s[utf8Index]);
		searchInfo.stringLength = J9UTF8_LENGTH(utf8s[utf8Index]);
		searchInfo.classloader = &classLoaders[classLoaderIndex];

		J9InternSearchResult searchResult;
		bool found = stringInternTable.findUtf8(&searchInfo, NULL, /* requiresSharedUtf8 = */ false, &searchResult);

		if (!stringInternTable.verify(__FILE__, __LINE__)) {
			outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.verify() failed!\n");
			goto _exit_test;
		}

		if (0 == (rand() % 2)) {
			/* Perform an action. */
			if (found) {
				stringInternTable.markNodeAsUsed(&searchResult, NULL);
			} else {
				stringInternTable.internUtf8(utf8s[utf8Index], &classLoaders[classLoaderIndex]);
			}
			if (!stringInternTable.verify(__FILE__, __LINE__)) {
				outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.verify() failed!\n");
				goto _exit_test;
			}
		}
	}

	for (UDATA i = 0; i < utfs8Count; i++) {
		j9mem_free_memory(utf8s[i]);
	}

_exit_test:
	return reportTestExit(PORTLIB, testName);
}


static J9SharedInvariantInternTable *
createJ9SharedInvariantInternTable(J9PortLibrary *portLib, J9ClassLoader *systemClassLoader, J9SharedCacheHeader *dummyHeader, void *allocatedMemory, U_32 memorySize)
{
	PORT_ACCESS_FROM_PORT(portLib);

	omrthread_monitor_t tableInternFxMutex;
	J9SharedInvariantInternTable * invariantInternTable = (J9SharedInvariantInternTable *) j9mem_allocate_memory(sizeof(J9SharedInvariantInternTable), J9MEM_CATEGORY_CLASSES);
	if (NULL == invariantInternTable) {
		return NULL;
	}
	memset(invariantInternTable, 0, sizeof(J9SharedInvariantInternTable));
	invariantInternTable->performNodeAction =
			(UDATA (*)(J9SharedInvariantInternTable*, J9SharedInternSRPHashTableEntry*, UDATA, void*))sharedInternTable_performNodeAction;
	invariantInternTable->flags = BCU_ENABLE_INVARIANT_INTERNING;
	if (omrthread_monitor_init_with_name(&tableInternFxMutex, 0, "XshareclassesVerifyInternTableMon") != 0) {
		j9mem_free_memory(invariantInternTable);
		return NULL;
	}
	invariantInternTable->tableInternFxMutex = tableInternFxMutex;
	invariantInternTable->sharedInvariantSRPHashtable = srpHashTableNewInRegion(portLib,
																		"test_srphashtable",
																		allocatedMemory,
																		memorySize,
																		sizeof(J9SharedInternSRPHashTableEntry),
																		0,
																		sharedInternHashFn,
																		sharedInternHashEqualFn,
																		NULL,
																		NULL);

	if (NULL == invariantInternTable->sharedInvariantSRPHashtable) {
		if (invariantInternTable->tableInternFxMutex != NULL) {
			omrthread_monitor_destroy(invariantInternTable->tableInternFxMutex);
			invariantInternTable->tableInternFxMutex = NULL;
		}
		j9mem_free_memory(invariantInternTable);
		return NULL;
	}

	invariantInternTable->flags |= J9AVLTREE_SHARED_TREE_INITIALIZED;
	invariantInternTable->systemClassLoader = systemClassLoader;

	memset(dummyHeader, 0, sizeof(J9SharedCacheHeader));
	invariantInternTable->sharedHeadNodePtr = &(dummyHeader->sharedStringHead);
	invariantInternTable->sharedTailNodePtr = &(dummyHeader->sharedStringTail);
	invariantInternTable->totalSharedNodesPtr = &(dummyHeader->totalSharedStringNodes);
	invariantInternTable->totalSharedWeightPtr = &(dummyHeader->totalSharedStringWeight);

	return invariantInternTable;
}


static void
freeJ9SharedInvariantInternTable(J9PortLibrary *portLib, J9SharedInvariantInternTable *invariantInternTable)
{
	PORT_ACCESS_FROM_PORT(portLib);
	srpHashTableFree(invariantInternTable->sharedInvariantSRPHashtable);
	if (invariantInternTable->tableInternFxMutex != NULL) {
		omrthread_monitor_destroy(invariantInternTable->tableInternFxMutex);
		invariantInternTable->tableInternFxMutex = NULL;
	}
	j9mem_free_memory(invariantInternTable);
}


static IDATA
testStringInternTableStressShared(J9PortLibrary *portLib, UDATA numIterations)
{
	const char * testName = "testStringInternTableStressShared";
	PORT_ACCESS_FROM_PORT(portLib);

	J9UTF8 *utf8s[5000];
	const UDATA utfs8Count = sizeof(utf8s)/sizeof(utf8s[0]);

	/* Class loaders are not initialized as currently the StringInternTable only uses their addresses. */
	J9ClassLoader classLoaders[25];
	const UDATA classLoadersCount = sizeof(classLoaders)/sizeof(classLoaders[0]);

	J9SharedInvariantInternTable *invariantInternTable = NULL;
	J9SharedCacheHeader dummyHeader;

	U_32 memoryUsedBySRPHashtable;
	UDATA allocSize;
	void * allocatedMemory;
	UDATA walker;

	reportTestEntry(PORTLIB, testName);

	StringInternTable stringInternTable(NULL, portLib, 500);
	if (!stringInternTable.isOK()) {
		outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.isOK() failed!\n");
		goto _exit_test;
	}

	memoryUsedBySRPHashtable = srpHashTable_requiredMemorySize(utfs8Count,sizeof(J9SharedInternSRPHashTableEntry), TRUE);
	allocSize = memoryUsedBySRPHashtable + (utfs8Count*256);
	allocatedMemory = j9mem_allocate_memory(allocSize, J9MEM_CATEGORY_CLASSES);
	if (NULL == allocatedMemory) {
		outputErrorMessage(TEST_ERROR_ARGS, "j9mem_allocate_memory(%u) failed!\n", allocSize);
		goto _exit_test;
	}
	
	walker = memoryUsedBySRPHashtable;
	/* Generate random UTF8s. */
	for (UDATA i = 0; i < utfs8Count; i++) {
		UDATA length = UDATA(rand() % 256);
		UDATA utf8Size = sizeof(U_16) + length + (0 != (length & 1) ? 1 : 0);
	
		utf8s[i] = (J9UTF8*)(((char*)allocatedMemory) + walker);

		J9UTF8_SET_LENGTH(utf8s[i], U_16(length));
		for (UDATA j = 0; j < length; j++) {
			J9UTF8_DATA(utf8s[i])[j] = U_8(0x20 + (rand() % (0x7E - 0x20)));
		}
		walker += utf8Size;
	}

	invariantInternTable = createJ9SharedInvariantInternTable(portLib, &classLoaders[0], &dummyHeader, allocatedMemory, memoryUsedBySRPHashtable);
	if (NULL == invariantInternTable) {
		outputErrorMessage(TEST_ERROR_ARGS, "createJ9SharedInvariantInternTable() failed!\n");
		goto _exit_test;
	}

	for (UDATA i = 0; i < numIterations; i++) {
		UDATA utf8Index = (rand() % utfs8Count);
		UDATA classLoaderIndex = (rand() % classLoadersCount);
		bool simulateShared = false;
		J9SharedInvariantInternTable *table = NULL;
		J9SimplePool *pool = NULL;

		if (1 == (rand() % 2)) {
			/* Simulate interning for/from shared ROM class. */
			simulateShared = true;
			classLoaderIndex = 0; /* the "systemClassLoader" */
		}

		if (1 == (rand() % 2)) {
			/* Search both the local table and the shared intern tree. */
			table = invariantInternTable;
		}

		J9InternSearchInfo searchInfo;
		searchInfo.stringData = J9UTF8_DATA(utf8s[utf8Index]);
		searchInfo.stringLength = J9UTF8_LENGTH(utf8s[utf8Index]);
		searchInfo.classloader = &classLoaders[classLoaderIndex];

		J9InternSearchResult searchResult;
		bool found = stringInternTable.findUtf8(&searchInfo, table, simulateShared, &searchResult);

		if (!stringInternTable.verify(__FILE__, __LINE__)) {
			outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.verify() failed!\n");
			goto _exit_test;
		}

		if (0 == (rand() % 2)) {
			/* Perform an action. */
			if (found) {
				stringInternTable.markNodeAsUsed(&searchResult, table);
			} else {
				stringInternTable.internUtf8(utf8s[utf8Index], &classLoaders[classLoaderIndex], simulateShared, table);
			}
			if (!stringInternTable.verify(__FILE__, __LINE__)) {
				outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.verify() failed!\n");
				goto _exit_test;
			}
		}
		if (!stringInternTable.verify(__FILE__, __LINE__)) {
			outputErrorMessage(TEST_ERROR_ARGS, "stringInternTable.verify() failed after mark/internUtf8!\n");
			goto _exit_test;
		}

	}

_exit_test:
	if (NULL != allocatedMemory) {
		j9mem_free_memory(allocatedMemory);
	}
	
	if (NULL != invariantInternTable) {
		freeJ9SharedInvariantInternTable(portLib, invariantInternTable);
	}
	return reportTestExit(PORTLIB, testName);
}


extern "C" {
IDATA
j9dyn_testInterning(J9PortLibrary *portLib, int randomSeed)
{
	PORT_ACCESS_FROM_PORT(portLib);
	IDATA rc = 0;

	HEADING(PORTLIB, "j9dyn_testInterning");

	if (0 == randomSeed) {
		randomSeed = (int) j9time_current_time_millis();
	}

	outputComment(portLib, "NOTE: Run 'dyntest -srand:%d' to reproduce this test manually.\n\n", randomSeed);
	srand(randomSeed);

	rc |= testStringInternTableWithZeroSize(PORTLIB);
	rc |= testStringInternTableWithSize1(PORTLIB);
	rc |= testStringInternTableRemoveLocalNodesWithDeadClassLoaders(PORTLIB);
	rc |= testStringInternTableStressLocal(PORTLIB, 10000);
	rc |= testStringInternTableStressShared(PORTLIB, 10000);
	rc |= testStringInternTableSRPRangeCheck(PORTLIB);


	return rc;
}
}

