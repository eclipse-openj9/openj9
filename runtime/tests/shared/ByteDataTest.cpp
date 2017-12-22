/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#include "CacheMap.hpp"
#include "CompositeCache.hpp"
#include "ByteDataManager.hpp"
#include "UnitTest.hpp"
#include "sharedconsts.h"
#include "j9port.h"
#include "main.h"
#include "j9.h"
#include <string.h>

extern "C" 
{ 
	IDATA testByteDataManager(J9JavaVM* vm); 
}

typedef struct CachePointers {
	J9SharedClassConfig* config1;
	J9SharedClassConfig* config2;
	J9SharedClassPreinitConfig* preConfig1;
	J9SharedClassPreinitConfig* preConfig2;
	SH_CacheMap* testCache1;
	SH_CacheMap* testCache2;
} CachePointers;

#define data1 "abcdefghijklmnopqrstuvwxyz"
#define data1a "1234567890abcdefghijklmnopqrstuvwxyz"
#define key1 "myKey1"
#define data2 "uiopuiopoupupoiupouoiuiuoiuoiu"
#define data2a "1234567890uiopuiopoupupoiupouoiuiuoiuoiu"
#define data2b "raindropsonrosesandwhiskersonkittens"
#define data2c "ianpartridgedoesunspeakablethingstogoats"
#define key2 "myKey2"
#define data3 "comehitherwenchandhelpmewithmypelvicgirdle"
#define key3 "myKey3"
#define data4 "takemedowntotheparadisecitywherethegrassisgreenandthegirlsarepretty"
#define key4 "myKey4"
#define data5 "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
#define key5 "myKey5"
#define key6 "myKey6"
#define data6 "allworkandnoplaymakesjackadullboy"
#define key7 "myKey7"
#define data7 "thecarrothasmystery.flowersaremerelytarts-prostitutesforthebees"
#define data8 "needsmorecowbell"
#define key8 "myKey8"

J9SharedDataDescriptor INPUTDATA[] = {
	{(U_8*)data1,		strlen(data1),		1,		0},
	{(U_8*)data1,		strlen(data1),		2,		0},
	{(U_8*)data1a,		strlen(data1a),		3,		0},
	{(U_8*)data2,		strlen(data2),		1,		0},
	{(U_8*)data2,		strlen(data2),		1,		0},
	{(U_8*)data2a,		strlen(data2a),		1,		0},
	{(U_8*)data3,		strlen(data3),		1,		J9SHRDATA_IS_PRIVATE},
	{(U_8*)data3,		strlen(data3),		1,		J9SHRDATA_IS_PRIVATE},
	{(U_8*)data3,		strlen(data3),		2,		J9SHRDATA_IS_PRIVATE},
	{(U_8*)data4,		strlen(data4),		1,		J9SHRDATA_IS_PRIVATE},
	{(U_8*)data2b,		strlen(data2b),		1,		J9SHRDATA_IS_PRIVATE},
	{(U_8*)data2c,		strlen(data2c),		1,		J9SHRDATA_IS_PRIVATE},
	{NULL,				strlen(data5),		1,		J9SHRDATA_ALLOCATE_ZEROD_MEMORY},
	{(U_8*)data6,		strlen(data6),		1,		J9SHRDATA_NOT_INDEXED},
	{(U_8*)data7,		strlen(data7),		1,		J9SHRDATA_SINGLE_STORE_FOR_KEY_TYPE},
	{(U_8*)data8,		strlen(data8),		1,		J9SHRDATA_SINGLE_STORE_FOR_KEY_TYPE},
	{NULL,				100000,				1,		J9SHRDATA_ALLOCATE_ZEROD_MEMORY}
};

IDATA storeTest(J9JavaVM* currentThread, struct CachePointers* pointers);
IDATA releaseAcquireTest(J9JavaVM* currentThread, struct CachePointers* pointers);
SH_CacheMap* createTestCache(J9JavaVM* vm, UDATA size, char* existingCache, char** resultCache);
IDATA checkFindResults(J9JavaVM* vm, struct CachePointers* cachePointers, UDATA testJustRun);
const IDATA SMALL_CACHE_SIZE = 16384;

IDATA testByteDataManager(J9JavaVM* vm)
{
	UDATA success = PASS;
	UDATA rc = 0;
	char* existingCachePtr = NULL;
	struct CachePointers cachePointers;
	
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	REPORT_START("Byte Data Manager");

	UnitTest::unitTest = UnitTest::BYTE_DATA_TEST;

	cachePointers.testCache1 = createTestCache(vm, SMALL_CACHE_SIZE, NULL, &existingCachePtr);
	if (cachePointers.testCache1 && existingCachePtr) {
		cachePointers.config1 = vm->sharedClassConfig;
		cachePointers.preConfig1 = vm->sharedClassPreinitConfig;
		cachePointers.testCache2 = createTestCache(vm, SMALL_CACHE_SIZE, existingCachePtr, &existingCachePtr);
	}
	if (cachePointers.testCache1 && cachePointers.testCache2) {
		IDATA storeTestResult;
		
		cachePointers.config2 = vm->sharedClassConfig;
		cachePointers.preConfig2 = vm->sharedClassPreinitConfig;
		storeTestResult = storeTest(vm, &cachePointers);
		SHC_TEST_ASSERT("Store", storeTestResult, success, rc);
		if (storeTestResult == 0) {
			SHC_TEST_ASSERT("ReleaseAcquire", releaseAcquireTest(vm, &cachePointers), success, rc);
		}
	} else {
		success = FAIL;
		j9tty_printf(PORTLIB, "Failed to initialize Byte Data Test\n");
	}

	UnitTest::unitTest = UnitTest::NO_TEST;

	REPORT_SUMMARY("Byte Data", success);

	return success;
}

SH_CacheMap* createTestCache(J9JavaVM* vm, UDATA size, char* existingCache, char** resultCache)
{
	void* memory;
	PORT_ACCESS_FROM_JAVAVM(vm);
	SH_CacheMap* returnVal;
	IDATA rc = 0;
	IDATA cacheObjectSize, totalSize;
	char* cache;
	
	j9tty_printf(PORTLIB, "Creating test cache\n");

	// Create a shared config object and attach it to the VM
	J9SharedClassConfig* sharedConfig = (J9SharedClassConfig*)j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
	if (sharedConfig == 0) {
		return NULL;
	}

	memset(sharedConfig, 0, sizeof(J9SharedClassConfig));
	sharedConfig->cacheDescriptorList = (J9SharedClassCacheDescriptor*)((UDATA)sharedConfig + sizeof(J9SharedClassConfig));

	J9SharedClassPreinitConfig* sharedpiConfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (sharedpiConfig == 0) {
		return NULL;
	}

	memset(sharedpiConfig, 0, sizeof(J9SharedClassPreinitConfig));

	sharedConfig->runtimeFlags =
		J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING          |
		J9SHR_RUNTIMEFLAG_ENABLE_TIMESTAMP_CHECKS        | 
		J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION |
		J9SHR_RUNTIMEFLAG_ENABLE_BYTECODEFIX;

	vm->sharedClassConfig = sharedConfig;
	vm->sharedClassPreinitConfig = sharedpiConfig;

	cacheObjectSize = SH_CacheMap::getRequiredConstrBytes(false);
	sharedpiConfig->sharedClassCacheSize = size;
	totalSize = cacheObjectSize;
	if (!existingCache) {
		totalSize += sharedpiConfig->sharedClassCacheSize;
	}

	memory = j9mem_allocate_memory(totalSize, J9MEM_CATEGORY_CLASSES);
	if (memory == 0) {
		return NULL;
	}

	memset(memory, 0, totalSize);

	returnVal = SH_CacheMap::newInstance(vm, sharedConfig, (SH_CacheMap*)memory, "cache1", 0);

	if (!existingCache) {
		cache = (char*)((char*)returnVal + cacheObjectSize);
	} else {
		cache = existingCache;
	}
	bool cacheHasIntegrity;
	rc = returnVal->startup(vm->mainThread, sharedpiConfig, "Root1", NULL, J9SH_DIRPERM_ABSENT, cache, &cacheHasIntegrity);
	if (rc != 0) {
		return NULL;
	}
	*resultCache = cache;
	return returnVal;
}

static void selectCache(J9JavaVM* vm, CachePointers* cachePointers, UDATA whichCache, SH_CacheMap** testCache)
{
	*testCache = (whichCache == 1) ? cachePointers->testCache1 : cachePointers->testCache2;
	vm->sharedClassConfig = (whichCache == 1) ? cachePointers->config1 : cachePointers->config2;
	vm->sharedClassPreinitConfig = (whichCache == 1) ? cachePointers->preConfig1 : cachePointers->preConfig2;
}

static IDATA checkStoreValues(J9JavaVM* vm, U_8* result, UDATA resultShouldBeNULL, U_8* compareTo1, U_8* oldResult, UDATA shouldMatchOldResult, UDATA testID) 
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	if (resultShouldBeNULL) {
		if (result) {
			j9tty_printf(PORTLIB, "Store result is not NULL for test %d\n", testID);
			return 0;
		}
	} else {
		if (!result) {
			j9tty_printf(PORTLIB, "Store result is NULL for test %d\n", testID);
			return 0;
		}
	}
	if (oldResult) {
		if (shouldMatchOldResult) {
			if (result != oldResult) {
				j9tty_printf(PORTLIB, "Store result does not equal previous result for test %d\n", testID);
				return 0;
			}
		} else {
			if (result == oldResult) {
				j9tty_printf(PORTLIB, "Store result should not equal previous result for test %d\n", testID);
				return 0;
			}
		}
	}
	if (compareTo1) {
		if (result == compareTo1) {
			j9tty_printf(PORTLIB, "Store result pointer should not equal original data pointer for test %d\n", testID);
			return 0;
		}
		if (strncmp((const char*)compareTo1, (const char*)result, strlen((const char*)compareTo1))) {
			j9tty_printf(PORTLIB, "Store result does not match expected result for test %d\n", testID);
			return 0;
		}
	}
	j9tty_printf(PORTLIB, "Test %d store results OK\n", testID);
	return 1;
}

IDATA storeTest(J9JavaVM* vm, struct CachePointers* cachePointers)
{
	U_8 *newEntry, *oldNewEntry;
	UDATA test = 0;
	SH_CacheMap* testCache = cachePointers->testCache1;

	/** TEST 1 **/
	
	/* Store public data under key 1 */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key1, strlen(key1), &INPUTDATA[0]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[0].address, NULL, FALSE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	oldNewEntry = newEntry;

	/** TEST 2 **/

	/* Store same data under a different type - same key */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key1, strlen(key1), &INPUTDATA[1]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[1].address, oldNewEntry, FALSE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	oldNewEntry = newEntry;

	/** TEST 3 **/

	/* Store different data under a different type - same key */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key1, strlen(key1), &INPUTDATA[2]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[2].address, oldNewEntry, FALSE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	oldNewEntry = newEntry;

	/** TEST 4 **/

	/* Store different data under original type - different key */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key2, strlen(key2), &INPUTDATA[3]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[3].address, oldNewEntry, FALSE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	oldNewEntry = newEntry;

	/** TEST 5 **/

	/* Store exact same public data a second time - should return same address as prev test */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key2, strlen(key2), &INPUTDATA[4]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[4].address, oldNewEntry, TRUE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	oldNewEntry = newEntry;

	/** TEST 6 **/

	/* Store new public data for an existing key - original data should be marked stale */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key2, strlen(key2), &INPUTDATA[5]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[5].address, oldNewEntry, FALSE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	oldNewEntry = newEntry;

	/** TEST 7 **/

	/* Store private data using cache1 */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key3, strlen(key3), &INPUTDATA[6]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[6].address, oldNewEntry, FALSE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	oldNewEntry = newEntry;

	/** TEST 8 **/

	/* Store private data using cache1 that already exists */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key3, strlen(key3), &INPUTDATA[7]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[7].address, oldNewEntry, TRUE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}

	/** TEST 9 **/

	/* Store same private data using cache1 under a different type - same key */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key3, strlen(key3), &INPUTDATA[8]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[8].address, oldNewEntry, FALSE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	oldNewEntry = newEntry;

	/** TEST 10 **/

	/* Store different private data using cache1 under a different key (original type) */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key4, strlen(key4), &INPUTDATA[9]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[9].address, oldNewEntry, FALSE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	oldNewEntry = newEntry;
	
	/** TEST 11 **/

	/* Store private data using cache1 under under the same key as some existing public data - should succeed */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key2, strlen(key2), &INPUTDATA[10]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[10].address, oldNewEntry, FALSE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	oldNewEntry = newEntry;

	/** TEST 12 **/

	/* Store a piece of private data using cache2 but using the same key as cache1 - expect them to co-exist */
	selectCache(vm, cachePointers, 2, &testCache);
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key2, strlen(key2), &INPUTDATA[11]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[11].address, oldNewEntry, FALSE, ++test)) {
		return 1;
	}
	selectCache(vm, cachePointers, 1, &testCache);
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	oldNewEntry = newEntry;
	
	/** TEST 13 **/

	/* Store zero'd data */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key5, strlen(key5), &INPUTDATA[12]);
	memcpy(newEntry, data5, strlen(data5));				/* copy the data */
	if (!checkStoreValues(vm, newEntry, FALSE, (U_8*)data5, oldNewEntry, FALSE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	oldNewEntry = newEntry;
	
	/** TEST 14 **/

	/* Store unindexed data */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key6, strlen(key6), &INPUTDATA[13]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[13].address, oldNewEntry, FALSE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}
	
	/** TEST 15 **/

	/* Call store with NULL and check that the items are made stale */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key2, strlen(key2), NULL);
	if (!checkStoreValues(vm, newEntry, TRUE, NULL, NULL, TRUE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}

	/** TEST 16 **/

	/* Store some public data under a key */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key7, strlen(key7), &INPUTDATA[14]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[14].address, oldNewEntry, FALSE, ++test)) {
		return 1;
	}
	oldNewEntry = newEntry;

	/** TEST 17 **/

	/* Store different public data for an existing key with J9SHRDATA_SINGLE_STORE_FOR_KEY_TYPE - expect existing data */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key7, strlen(key7), &INPUTDATA[15]);
	if (!checkStoreValues(vm, newEntry, FALSE, INPUTDATA[14].address, oldNewEntry, TRUE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}

	/** TEST 18 **/

	/* Try to store data larger than the cache size */
	newEntry = (U_8*)testCache->storeSharedData(vm->mainThread, key8, strlen(key8), &INPUTDATA[16]);
	if (!checkStoreValues(vm, newEntry, TRUE, NULL, NULL, TRUE, ++test)) {
		return 1;
	}
	if (!checkFindResults(vm, cachePointers, test)) {
		return 1;
	}

	return 0;
}

IDATA releaseAcquireTest(J9JavaVM* vm, struct CachePointers* cachePointers)
{
	SH_CacheMap* testCache = cachePointers->testCache1;
	J9SharedDataDescriptor firstEntry1, firstEntry2;
	UDATA resCount, acquireResult, releaseResult;

	PORT_ACCESS_FROM_JAVAVM(vm);
	
	/* After the storeTest, there should only be one piece of private data against key2, stored by cache 2 */
	resCount = testCache->findSharedData(vm->mainThread, key2, strlen(key2), FALSE, TRUE, &firstEntry1, NULL);
	if (resCount == 1) {
		/* We are cache1 so we should not be able to acquire the data from cache 2 */
		acquireResult = testCache->acquirePrivateSharedData(vm->mainThread, &firstEntry1);
		if (acquireResult) {
			j9tty_printf(PORTLIB, "Initial acquire should not have succeeded for cache1\n");
			return 1;
		}
	} else {
		j9tty_printf(PORTLIB, "findSharedData prereq failed for releaseAcquireTest\n");
		return 1;
	}
	/* Cache1 should not be able to release cache2's private data */
	releaseResult = testCache->releasePrivateSharedData(vm->mainThread, &firstEntry1);
	if (releaseResult) {
		j9tty_printf(PORTLIB, "cache1 should not have been able to release cache2's private data\n");
		return 1;
	}

	/* Select cache 2 */
	selectCache(vm, cachePointers, 2, &testCache);
	resCount = testCache->findSharedData(vm->mainThread, key2, strlen(key2), FALSE, TRUE, &firstEntry2, NULL);

	if (resCount == 1) {
		/* We're cache2 - we can release our own data */
		releaseResult = testCache->releasePrivateSharedData(vm->mainThread, &firstEntry2);
		if (!releaseResult) {
			j9tty_printf(PORTLIB, "cache2 failed to release its private data entry\n");
			return 1;
		}
	} else {
		j9tty_printf(PORTLIB, "cache2 failed to its private data entry\n");
		return 1;
	}
	
	selectCache(vm, cachePointers, 1, &testCache);
	
	/* Should now be able to acquire the data */
	acquireResult = testCache->acquirePrivateSharedData(vm->mainThread, &firstEntry1);
	if (!acquireResult) {
		j9tty_printf(PORTLIB, "cache1 failed to acquire the released private data\n");
		return 1;
	}
	
	/* Should now be able to verify the acquired data */
	resCount = testCache->findSharedData(vm->mainThread, key2, strlen(key2), FALSE, TRUE, &firstEntry1, NULL);
	if (resCount==0 || (firstEntry1.flags & J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM)) {
		j9tty_printf(PORTLIB, "verification of acquired data failed\n");
		return 1;
	}
	return 0;
}

static IDATA compareDescriptors(J9JavaVM* vm, J9SharedDataDescriptor* compare1, J9SharedDataDescriptor* compare2, UDATA testID, const char* descName)
{
	if (compare1->type != compare2->type) {
/*		j9tty_printf(PORTLIB, "%s descriptors types did not match for test %d\n", descName, testID); */
		return 0; 
	}
	if (compare1->flags != compare2->flags) {
/*		j9tty_printf(PORTLIB, "%s descriptors flags did not match for test %d\n", descName, testID); */
		return 0; 
	}
	if (compare1->length != compare2->length) {		
/*		j9tty_printf(PORTLIB, "%s descriptors lengths did not match for test %d\n", descName, testID); */
		return 0; 
	}
	if (strncmp((const char*)compare1->address, (const char*)compare2->address, compare2->length)) {
/*		j9tty_printf(PORTLIB, "%s descriptors data did not match for test %d\n", descName, testID); */
		return 0; 
	}
	return 1;
}

/* Note that there is no ordering of results guaranteed, so this function has to be order-agnostic */
static IDATA checkFindValues(J9JavaVM* vm, IDATA resultCount, IDATA expectedResultCount, J9SharedDataDescriptor* firstEntry,
				J9Pool* descriptorPool, J9SharedDataDescriptor* expectedDesc1, J9SharedDataDescriptor* expectedDesc2, 
				J9SharedDataDescriptor* expectedDesc3, UDATA testID)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	pool_state state;
	J9SharedDataDescriptor* walk;
	IDATA found1, found2, found3;
	
	found1 = found2 = found3 = -1;

	if (resultCount != expectedResultCount) {
		j9tty_printf(PORTLIB, "Find result count does not equal expected result for test %d\n", testID);
		return 0;
	}
	if (expectedResultCount > 0) {
		
		if (firstEntry) {
			if (!((expectedDesc1 && compareDescriptors(vm, firstEntry, expectedDesc1, testID, "firstEntry1")) ||
				 (expectedDesc2 && compareDescriptors(vm, firstEntry, expectedDesc2, testID, "firstEntry2")) ||
				 (expectedDesc3 && compareDescriptors(vm, firstEntry, expectedDesc3, testID, "firstEntry3")))) {
				j9tty_printf(PORTLIB, "FirstEntry could not be found for test%d\n", testID);
				return 0;		
			}
		}
		if (descriptorPool) {
			IDATA i;
			
			if (pool_numElements(descriptorPool) != (UDATA)expectedResultCount) {
				j9tty_printf(PORTLIB, "Number of elements in descriptor pools is not as expected for test %d\n", testID);
				return 0;
			}
			
			walk = (J9SharedDataDescriptor*)pool_startDo(descriptorPool, &state);
			for (i=0; i<expectedResultCount; i++) {
				if (compareDescriptors(vm, walk, expectedDesc1, testID, "poolEntry0")) {
					found1 = i;
					break;
				}
				walk = (J9SharedDataDescriptor*)pool_nextDo(&state);
			}
				
			if (expectedDesc2) {
				walk = (J9SharedDataDescriptor*)pool_startDo(descriptorPool, &state);
				for (i=0; i<expectedResultCount; i++) {
					if ((i != found1) && compareDescriptors(vm, walk, expectedDesc2, testID, "poolEntry1")) {
						found2 = i;
						break;
					}
					walk = (J9SharedDataDescriptor*)pool_nextDo(&state);
				}
			}

			if (expectedDesc3) {
				walk = (J9SharedDataDescriptor*)pool_startDo(descriptorPool, &state);
				for (i=0; i<expectedResultCount; i++) {
					if ((i != found1) && (i != found2) && compareDescriptors(vm, walk, expectedDesc3, testID, "poolEntry2")) {
						found3 = i;
						break;
					}
					walk = (J9SharedDataDescriptor*)pool_nextDo(&state);
				}
			}
			
			if (expectedDesc1) {
				if (expectedDesc2) {
					if (expectedDesc3) {
						if ((found1 == found2) || (found2 == found3) || (found1 == -1) || (found2 == -1) || (found3 == -1)) {
							j9tty_printf(PORTLIB, "Unexpected results for pool: found1=%d, found2=%d, found3=%d in test", found1, found2, found3, testID);
							return 0;
						}
					} else {
						if ((found1 == found2) || (found1 == -1) || (found2 == -1)) {
							j9tty_printf(PORTLIB, "Unexpected results for pool: found1=%d, found2=%d in test", found1, found2, testID);
							return 0;
						}
					}
				} else {
					if (found1 != 0) {
						j9tty_printf(PORTLIB, "Unexpected results for pool: found1=%d in test", found1, testID);
						return 0;
					}
				}
			}	
		}
	}
	j9tty_printf(PORTLIB, "> Test %d find results OK\n", testID);
	return 1;	
} 

IDATA checkFindResults(J9JavaVM* vm, struct CachePointers* cachePointers, UDATA testJustRun)
{
	SH_CacheMap* testCache;
	UDATA resCount1, resCount2, resCount3;
	J9Pool* resultsPool;
	J9SharedDataDescriptor firstEntry;
	IDATA i;
	UDATA test = 0;
	
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	resultsPool = pool_new(sizeof(J9SharedDataDescriptor),  0, 0, 0, J9_GET_CALLSITE(), J9MEM_CATEGORY_CLASSES, POOL_FOR_PORT(PORTLIB));
	
	if (!resultsPool) {
		j9tty_printf(PORTLIB, "Failed to create results pool - exiting\n");
		return 1;
	}

	/* Check the results of test 1 - simply added public data */
	if (testJustRun == ++test) {
		/* Run through both views on the same cache */
		for (i=1; i<3; i++) {
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);
			/* Check that calling with NULLs is ok */ 
			resCount1 = testCache->findSharedData(vm->mainThread, key1, strlen(key1), FALSE, FALSE, NULL, NULL);
			resCount2 = testCache->findSharedData(vm->mainThread, key1, strlen(key1), FALSE, FALSE, &firstEntry, NULL);
			resCount3 = testCache->findSharedData(vm->mainThread, key1, strlen(key1), FALSE, FALSE, NULL, resultsPool);
			if ((resCount1 != resCount2) || (resCount2 != resCount3)) {
				j9tty_printf(PORTLIB, "Result numbers do not match for test and cache %d\n", i);
				return 0;
			}
			/* Check the values returned */
			if (!checkFindValues(vm, resCount1, 1, &firstEntry, resultsPool, &INPUTDATA[0], NULL, NULL, test)) {
				return 0;
			}
		}
	}
	
	/* Check the results of test 2 - two pieces of public data added under the same key */
	if (testJustRun == ++test) {
		for (i=1; i<3; i++) {
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			/* Check that both entries are returned */
			resCount1 = testCache->findSharedData(vm->mainThread, key1, strlen(key1), FALSE, FALSE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 2, &firstEntry, resultsPool, &INPUTDATA[0], &INPUTDATA[1], NULL, test)) {
				return 0;
			}

			/* Check that limiting the data type only returns one value */
			pool_clear(resultsPool);
			resCount1 = testCache->findSharedData(vm->mainThread, key1, strlen(key1), 1, FALSE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 1, &firstEntry, resultsPool, &INPUTDATA[0], NULL, NULL, test)) {
				return 0;
			}

			/* Check that limiting the data type only returns one value */
			pool_clear(resultsPool);
			resCount1 = testCache->findSharedData(vm->mainThread, key1, strlen(key1), 2, FALSE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 1, &firstEntry, resultsPool, &INPUTDATA[1], NULL, NULL, test)) {
				return 0;
			}
		}		
	}
	
	/* Check the results of test 3 */
	if (testJustRun >= ++test) {
		for (i=1; i<3; i++) {
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			/* We now have 3 pieces of public data under the same key - check that we get the right ones back */ 
			resCount1 = testCache->findSharedData(vm->mainThread, key1, strlen(key1), FALSE, FALSE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 3, &firstEntry, resultsPool, &INPUTDATA[0], &INPUTDATA[1], &INPUTDATA[2], test)) {
				return 0;
			}
		
			/* Check that limiting the data type only returns one value */
			pool_clear(resultsPool);
			resCount1 = testCache->findSharedData(vm->mainThread, key1, strlen(key1), 3, FALSE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 1, &firstEntry, resultsPool, &INPUTDATA[2], NULL, NULL, test)) {
				return 0;
			}
		}
	}

	/* Check the results of test 4 - note that in test 6, the data is replaced, so this test is only valid until then */
	if ((testJustRun >= ++test) && (testJustRun < 6)) {
		for (i=1; i<3; i++) {
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			/* We now have some different data under a different key */
			resCount1 = testCache->findSharedData(vm->mainThread, key2, strlen(key2), FALSE, FALSE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 1, &firstEntry, resultsPool, &INPUTDATA[3], NULL, NULL, test)) {
				return 0;
			}
		}
	}
	
	/* Results of test 5 are checked by test 4 code above */
	++test;
	
	/* Test 6 has added some new data against an existing key - check that the new data is found */
	if ((testJustRun >= ++test) && (testJustRun < 15)) {
		for (i=1; i<3; i++) {
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			/* We now have some different data under a different key */
			resCount1 = testCache->findSharedData(vm->mainThread, key2, strlen(key2), FALSE, FALSE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 1, &firstEntry, resultsPool, &INPUTDATA[5], NULL, NULL, test)) {
				return 0;
			}
		}
	}

	/* Test 7 has stored a piece of private data under cache 1 - test 9 adds another piece of data under the same key */
	if ((testJustRun >= ++test) && (testJustRun < 9)) {
		for (i=1; i<3; i++) {
			J9SharedDataDescriptor tempDesc;
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			/* Check that we get no data returned with includePrivateData == FALSE */
			resCount1 = testCache->findSharedData(vm->mainThread, key3, strlen(key3), FALSE, FALSE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 0, &firstEntry, resultsPool, &INPUTDATA[6], NULL, NULL, test)) {
				return 0;
			}

			memcpy(&tempDesc, &INPUTDATA[6], sizeof(J9SharedDataDescriptor));
			if (i==2) {
				tempDesc.flags |= J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM;
			}
			/* Check that we get correct data returned with includePrivateData == TRUE */
			resCount1 = testCache->findSharedData(vm->mainThread, key3, strlen(key3), FALSE, TRUE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 1, &firstEntry, resultsPool, &tempDesc, NULL, NULL, test)) {
				return 0;
			}
		}
	}

	/* Results of test 8 are checked by test 7 code above */
	++test;
	
	/* Test 9 - another piece of private data is added under the same key (different type) */
	if (testJustRun >= ++test) {
		for (i=1; i<3; i++) {
			J9SharedDataDescriptor tempDesc1, tempDesc2;
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			memcpy(&tempDesc1, &INPUTDATA[7], sizeof(J9SharedDataDescriptor));
			memcpy(&tempDesc2, &INPUTDATA[8], sizeof(J9SharedDataDescriptor));
			if (i==2) {
				tempDesc1.flags |= J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM;
				tempDesc2.flags |= J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM;
			}
			/* We now have some different data under a different key */
			resCount1 = testCache->findSharedData(vm->mainThread, key3, strlen(key3), FALSE, TRUE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 2, &firstEntry, resultsPool, &tempDesc1, &tempDesc2, NULL, test)) {
				return 0;
			}
		}
	}
	
	/* Test 10 - another piece of private data is added under a different key */
	if (testJustRun >= ++test) {
		for (i=1; i<3; i++) {
			J9SharedDataDescriptor tempDesc;
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			memcpy(&tempDesc, &INPUTDATA[9], sizeof(J9SharedDataDescriptor));
			if (i==2) {
				tempDesc.flags |= J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM;
			}
			/* We now have some different data under a different key */
			resCount1 = testCache->findSharedData(vm->mainThread, key4, strlen(key4), FALSE, TRUE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 1, &firstEntry, resultsPool, &tempDesc, NULL, NULL, test)) {
				return 0;
			}
		}
	}
	
	/* key2 now has a piece of private data and a piece of public data against it */
	if ((testJustRun >= ++test) && (testJustRun < 12)) {
		for (i=1; i<3; i++) {
			J9SharedDataDescriptor tempDesc;
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			memcpy(&tempDesc, &INPUTDATA[10], sizeof(J9SharedDataDescriptor));
			if (i==2) {
				tempDesc.flags |= J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM;
			}
			/* We now have some different data under a different key */
			resCount1 = testCache->findSharedData(vm->mainThread, key2, strlen(key2), FALSE, TRUE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 2, &firstEntry, resultsPool, &INPUTDATA[5], &tempDesc, NULL, test)) {
				return 0;
			}
		}
	}
	
	/* test 12 - cache 2 has just stored a piece of private data under the same key as cache 1 */
	if ((testJustRun >= ++test) && (testJustRun < 15)) {
		for (i=1; i<3; i++) {
			J9SharedDataDescriptor tempDesc1, tempDesc2;
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			memcpy(&tempDesc1, &INPUTDATA[10], sizeof(J9SharedDataDescriptor));
			memcpy(&tempDesc2, &INPUTDATA[11], sizeof(J9SharedDataDescriptor));
			if (i==1) {
				tempDesc2.flags |= J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM;
			} else {
				tempDesc1.flags |= J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM;
			}
				
			/* Check that all the pieces of data under key2 are co-existing happily */
			resCount1 = testCache->findSharedData(vm->mainThread, key2, strlen(key2), FALSE, TRUE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 3, &firstEntry, resultsPool, &INPUTDATA[5], &tempDesc1, &tempDesc2, test)) {
				return 0;
			}
		}
	}

	/* test 13 is where zero'd memory has been allocated and data written into it */
	if (testJustRun >= ++test) {
		J9SharedDataDescriptor tempDesc;

		memcpy(&tempDesc, &INPUTDATA[12], sizeof(J9SharedDataDescriptor));
		tempDesc.flags = 0;			/* Remove the ALLOCATE flag */

		for (i=1; i<3; i++) {
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			/* We now have some different data under a different key */
			resCount1 = testCache->findSharedData(vm->mainThread, key5, strlen(key5), FALSE, FALSE, &firstEntry, resultsPool);
			tempDesc.address = (U_8*)data5;		/* Supply the copied data for comparison */
			if (!checkFindValues(vm, resCount1, 1, &firstEntry, resultsPool, &tempDesc, NULL, NULL, test)) {
				return 0;
			}
		}
	}
	
	/* Test 14 is unindexed data - so we shouldn't be able to find it */	
	if (testJustRun >= ++test) {
		for (i=1; i<3; i++) {
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			resCount1 = testCache->findSharedData(vm->mainThread, key6, strlen(key6), FALSE, FALSE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 0, &firstEntry, resultsPool, &INPUTDATA[13], NULL, NULL, test)) {
				return 0;
			}
		}
	}
	
	/* Expect zero results now under key2 for test 15 */
	if (testJustRun >= ++test) {
		for (i=1; i<3; i++) {
			J9SharedDataDescriptor tempDesc;
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			memcpy(&tempDesc, &INPUTDATA[11], sizeof(J9SharedDataDescriptor));
			if (i==1) {
				tempDesc.flags |= J9SHRDATA_PRIVATE_TO_DIFFERENT_JVM;
			}
			/* Check that all items stored by cache 1 and all public items are stale. Should only be one item remaining */
			resCount1 = testCache->findSharedData(vm->mainThread, key2, strlen(key2), FALSE, TRUE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 1, &firstEntry, resultsPool, &tempDesc, NULL, NULL, test)) {
				return 0;
			}
		}
	}

	/* For test 16 and 17 */
	if (testJustRun >= ++test) {
		J9SharedDataDescriptor tempDesc;

		memcpy(&tempDesc, &INPUTDATA[14], sizeof(J9SharedDataDescriptor));
		tempDesc.flags = 0;		/* Remove J9SHRDATA_SINGLE_STORE_FOR_KEY_TYPE */

		for (i=1; i<3; i++) {
			selectCache(vm, cachePointers, i, &testCache);
			pool_clear(resultsPool);

			resCount1 = testCache->findSharedData(vm->mainThread, key7, strlen(key7), FALSE, FALSE, &firstEntry, resultsPool);
			if (!checkFindValues(vm, resCount1, 1, &firstEntry, resultsPool, &tempDesc, NULL, NULL, test)) {
				return 0;
			}
		}
	}
	
	return 1;
}
