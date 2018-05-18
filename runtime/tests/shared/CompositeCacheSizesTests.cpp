/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

extern "C"
{
#include "shrinit.h"
}
#include "CompositeCacheImpl.hpp"
#include "CompositeCacheSizesTests.hpp"
#include "UnitTest.hpp"

#define TEST_PASS 0
#define TEST_ERROR -1

#define ERRPRINTF(args) \
do { \
	j9tty_printf(PORTLIB,"\t"); \
	j9tty_printf(PORTLIB,"ERROR: %s",__FILE__); \
	j9tty_printf(PORTLIB,"(%d)",__LINE__); \
	j9tty_printf(PORTLIB," %s ",testName); \
	j9tty_printf(PORTLIB,args); \
}while(0) \

static IDATA test1(J9JavaVM* vm);
static IDATA test2(J9JavaVM* vm);
static IDATA test3(J9JavaVM* vm);
static IDATA test4(J9JavaVM* vm);
static IDATA test5(J9JavaVM* vm);
static IDATA test6(J9JavaVM* vm);
static IDATA test7(J9JavaVM* vm);
static IDATA test8(J9JavaVM* vm);
static IDATA test9(J9JavaVM* vm);
static IDATA test10(J9JavaVM* vm);
static IDATA test11(J9JavaVM* vm);
static IDATA test12(J9JavaVM* vm);
static IDATA test13(J9JavaVM* vm);
static IDATA test14(J9JavaVM* vm);

static IDATA
createTestCompositeCache (J9JavaVM* vm, SH_CompositeCacheImpl** cc, J9SharedClassPreinitConfig* piconfig, const char* testName){
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA requiredBytes;
	U_32 cacheSize = 0;
	SH_CompositeCacheImpl *memForCC, *actualCC;
	UDATA totalSize;
	UDATA localCrashCntr = 0;
	U_64 runtimeFlags = 0;
	bool cacheHasIntegrity;
	char* cache;

	requiredBytes = SH_CompositeCacheImpl::getRequiredConstrBytesWithCommonInfo(false, false);
	totalSize = requiredBytes + piconfig->sharedClassCacheSize;
	if (!(memForCC = (SH_CompositeCacheImpl*)j9mem_allocate_memory(totalSize, J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for composite cache.\n"));
		*cc = NULL;
		return TEST_ERROR;
	}

	memset((void*)memForCC, 0, totalSize);

	/*Request non-persistent cache */
	actualCC = SH_CompositeCacheImpl::newInstance(vm, NULL, memForCC, "myRoot", false, false);
	cache = (char*)((char*)actualCC + requiredBytes);
	if (actualCC->startup(vm->mainThread, piconfig, cache, &runtimeFlags, 1, "myRoot", NULL, J9SH_DIRPERM_ABSENT, &cacheSize, &localCrashCntr, true, &cacheHasIntegrity) != 0) {
		ERRPRINTF(("Failed to start the composite cache.\n"));
		*cc = actualCC;
		return TEST_ERROR;
	}
	*cc = actualCC;
	return TEST_PASS;
}



IDATA
testCompositeCacheSizes(J9JavaVM* vm)
{
	const char * testName = "testCompositeCacheSizes";
	if (vm == NULL) {
		/*vm is null*/
		return TEST_ERROR;
	}
	PORT_ACCESS_FROM_JAVAVM(vm);

	IDATA rc = TEST_PASS;

	vm->internalVMFunctions->internalEnterVMFromJNI(vm->mainThread);
	UnitTest::unitTest = UnitTest::COMPOSITE_CACHE_SIZES_TEST;
	rc |= test1(vm);
	rc |= test2(vm);
	/* Exclude test3 from AIX machines because of memory allocation limits on some AIX machines. Fails to allocate big enough memory for this test. */
#if !defined(AIXPPC)
	rc |= test3(vm);
#endif
	rc |= test4(vm);
	rc |= test5(vm);
	rc |= test6(vm);
	rc |= test7(vm);
	rc |= test8(vm);
	rc |= test9(vm);
	rc |= test10(vm);
	rc |= test11(vm);
	rc |= test12(vm);
	rc |= test13(vm);
	rc |= test14(vm);
	UnitTest::unitTest = UnitTest::NO_TEST;

	vm->internalVMFunctions->internalExitVMToJNI(vm->mainThread);

	j9tty_printf(PORTLIB, "%s: %s\n", testName, TEST_PASS==rc?"PASS":"FAIL");
	if (rc == TEST_ERROR) {
		return TEST_ERROR;
	} else {
		return TEST_PASS;
	}
}

static IDATA
test1(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
	SH_CompositeCacheImpl *testCC = NULL;
	J9SharedClassPreinitConfig* piconfig = NULL;
	const char* testName = "test1";
	UDATA requiredCacheSize;
	j9tty_printf(PORTLIB, "%s: create composite cache with read/write area larger than cache size\n", testName);

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}
	
	requiredCacheSize = srpHashTable_requiredMemorySize(MAIN_SHARED_TABLE_NODE_COUNT, sizeof(J9SharedInternSRPHashTableEntry), TRUE);
	piconfig->sharedClassCacheSize = requiredCacheSize;
	piconfig->sharedClassInternTableNodeCount = MAIN_SHARED_TABLE_NODE_COUNT * 2;
	piconfig->sharedClassMinAOTSize = -1;
	piconfig->sharedClassMaxAOTSize = -1;
	piconfig->sharedClassMinJITSize = -1;
	piconfig->sharedClassMaxJITSize = -1;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;

	ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, piconfig);
	retval = createTestCompositeCache(vm, &testCC, piconfig, testName);

	if (retval != TEST_PASS) {
		/* Error is captured and printed in createTestCompositeCache */
		goto done;
	}

	if (piconfig->sharedClassReadWriteBytes != -1){
		ERRPRINTF(("Read/write area check failed.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if (!(testCC->getReadWriteBytes() < (testCC->getTotalUsableCacheSize()))) {
		ERRPRINTF(("Incorrect read/write area default size.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	j9mem_free_memory(testCC);
	j9mem_free_memory(piconfig);
	return retval;
}

static IDATA
test2(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
	SH_CompositeCacheImpl *testCC = NULL;
	J9SharedClassPreinitConfig* piconfig = NULL;
	const char* testName = "test2";
	j9tty_printf(PORTLIB, "%s: create default composite cache without using -Xitsn option and check string table bytes and read/write are bytes are same\n", testName);

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	piconfig->sharedClassCacheSize = MAIN_CACHE_SIZE;
	piconfig->sharedClassInternTableNodeCount = -1;
	piconfig->sharedClassReadWriteBytes = -1;
	piconfig->sharedClassMinAOTSize = -1;
	piconfig->sharedClassMaxAOTSize = -1;
	piconfig->sharedClassMinJITSize = -1;
	piconfig->sharedClassMaxJITSize = -1;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;
	ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, piconfig);
	retval = createTestCompositeCache(vm, &testCC, piconfig, testName);

	if (retval != TEST_PASS) {
		/* Error is captured and printed in createTestCompositeCache */
		goto done;
	}
	
	if (piconfig->sharedClassReadWriteBytes != -1){
		ERRPRINTF(("Read/write area check failed.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	/* Currently all the read write bytes are used for string intern table.
	 * Unless string table bytes set externally with -Xitsn option, read/write are bytes and string table bytes must be same.
	 *
	 */
	if (testCC->getStringTableBytes() != testCC->getReadWriteBytes()){
		ERRPRINTF(("Read/write area size and string table bytes are not same.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if (testCC->getStringTableBytes() <= 0 ) {
		ERRPRINTF(("Read/write area size should be bigger than zero.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	j9mem_free_memory(testCC);
	j9mem_free_memory(piconfig);
	return retval;
}

static IDATA
test3(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
	SH_CompositeCacheImpl *testCC = NULL;
	J9SharedClassPreinitConfig* piconfig = NULL;
	UDATA maxSharedStringTableSize;
	const char* testName = "test3";
	j9tty_printf(PORTLIB, "%s: create composite cache with read/write area smaller than cache size but larger than maximum allowed r/w size\n", testName);

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	maxSharedStringTableSize = srpHashTable_requiredMemorySize(SHRINIT_MAX_SHARED_STRING_TABLE_NODE_COUNT, sizeof(J9SharedInternSRPHashTableEntry), TRUE);

	/*
	 * make sure that cache size is big enough that
	 * proportion of it which is given to shared string table is bigger than
	 * maxSharedStringTableSize
	 * Creating cache size 10 per cent bigger than the enough memory for max shared string intern table
	 */
	piconfig->sharedClassCacheSize = (UDATA)(1.1 * (maxSharedStringTableSize * 300));
	piconfig->sharedClassReadWriteBytes = -1;
	piconfig->sharedClassInternTableNodeCount = -1;
	piconfig->sharedClassMinAOTSize = -1;
	piconfig->sharedClassMaxAOTSize = -1;
	piconfig->sharedClassMinJITSize = -1;
	piconfig->sharedClassMaxJITSize = -1;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;
	/* 1.1 * (maxSharedStringTableSize * 300) might be greater than SHMMAX and pass J9SHR_RUNTIMEFLAG_ENABLE_PERSISTENT_CACHE to avoid cache size to be resized to SHMMAX */
	ensureCorrectCacheSizes(vm, vm->portLibrary, J9SHR_RUNTIMEFLAG_ENABLE_PERSISTENT_CACHE, (UDATA)0, piconfig);
	retval = createTestCompositeCache(vm, &testCC, piconfig, testName);

	if (retval != TEST_PASS) {
		/* Error is captured and printed in createTestCompositeCache */
		goto done;
	}

	if (testCC->getReadWriteBytes() != maxSharedStringTableSize){
		ERRPRINTF(("Read/write area size incorrect.\n"));
		j9tty_printf(PORTLIB, "testCC->getReadWriteBytes()=%u", testCC->getReadWriteBytes());
		j9tty_printf(PORTLIB, "maxSharedStringTableSize=%u", maxSharedStringTableSize);
		retval = TEST_ERROR;
		goto done;
	}

	done:
	j9mem_free_memory(testCC);
	j9mem_free_memory(piconfig);
	return retval;
}

static IDATA
test4(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
	J9SharedClassPreinitConfig piconfig;
	UDATA requiredBytes;
	U_32 cacheSize = 0;
	SH_CompositeCacheImpl *memForCC, *actualCC;
	UDATA totalSize;
	UDATA localCrashCntr = 0;
	U_64 runtimeFlags = 0;
	bool cacheHasIntegrity;
	char* cache;
	ShcItem item;
	ShcItem* itemPtr = &item;
	char* address;

	const char* testName = "test4";
	j9tty_printf(PORTLIB, "%s: create composite cache, fill it, start it again and ensure the cache is marked full\n", testName);

	piconfig.sharedClassCacheSize = MAIN_CACHE_SIZE;
	piconfig.sharedClassInternTableNodeCount = 0;
	piconfig.sharedClassReadWriteBytes = -1;
	piconfig.sharedClassMinAOTSize = -1;
	piconfig.sharedClassMaxAOTSize = -1;
	piconfig.sharedClassMinJITSize = -1;
	piconfig.sharedClassMaxJITSize = -1;
	piconfig.sharedClassDebugAreaBytes = -1;
	piconfig.sharedClassSoftMaxBytes = -1;

	ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, &piconfig);
	requiredBytes = SH_CompositeCacheImpl::getRequiredConstrBytesWithCommonInfo(false, false);
	totalSize = requiredBytes + piconfig.sharedClassCacheSize;
	if (!(memForCC = (SH_CompositeCacheImpl*)j9mem_allocate_memory(totalSize, J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for composite cache.\n"));
		j9tty_printf(PORTLIB, "Tried to allocate memory= %u bytes\n", totalSize);
		retval = TEST_ERROR;
		goto done;
	}

	memset((void*)memForCC, 0, totalSize);

	/*Request non-persistent cache */
	actualCC = SH_CompositeCacheImpl::newInstance(vm, NULL, memForCC, "myRoot", false, false);
	cache = (char*)((char*)actualCC + requiredBytes);

	runtimeFlags = J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION;
	if (actualCC->startup(vm->mainThread, &piconfig, cache, &runtimeFlags, 1, "myRoot", NULL, J9SH_DIRPERM_ABSENT, &cacheSize, &localCrashCntr, true, &cacheHasIntegrity) != 0) {
		ERRPRINTF(("Could not start cache initially\n"));
		retval = TEST_ERROR;
		goto done;
	}
	
	if ((runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION) == 0) {
		ERRPRINTF(("Store contention not enabled after first startup\n"));
		retval = TEST_ERROR;
		goto done;
	}
	if ((runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) != 0) {
		ERRPRINTF(("Cache full after first startup\n"));
		retval = TEST_ERROR;
		goto done;
	}

	itemPtr->dataLen = MAIN_CACHE_SIZE - actualCC->getDebugBytes() - CC_MIN_SPACE_BEFORE_CACHE_FULL;

	/* Allocate a big enough block so the cache will be full next time its started */
	actualCC->enterWriteMutex(vm->mainThread, false, "test4");
	address = actualCC->allocateBlock(vm->mainThread, itemPtr, SHC_WORDALIGN, 0);
	if (address == NULL) {
		ERRPRINTF(("Could not allocate block\n"));
		retval = TEST_ERROR;
		goto done;
	}
	actualCC->commitUpdate(vm->mainThread, false);
	
	do {
		itemPtr->dataLen = 200;
		address = actualCC->allocateBlock(vm->mainThread, itemPtr, SHC_WORDALIGN, 0);
		if (address != NULL) {
			actualCC->commitUpdate(vm->mainThread, false);
		}
	} while (address != NULL);

	actualCC->exitWriteMutex(vm->mainThread, "test4");

	if ((runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) == 0) {
		ERRPRINTF(("Cache is not marked full\n"));
		retval = TEST_ERROR;
		goto done;
	}

/* JAZZ 56433: Following tests are disabled as they are no longer valid after these changes.
 * These tests rely on cache being checked for full during SH_CompositeCache::startup().
 * But in this task, the check for full cache has been moved to SH_CacheMap::startup().
 */
#if 0
	memset(actualCC, 0, requiredBytes);
	actualCC = SH_CompositeCacheImpl::newInstance(vm, NULL, memForCC, "myRoot", false, false);

	runtimeFlags = J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION;
	if (actualCC->startup(vm->mainThread, &piconfig, cache, &runtimeFlags, 1, "myRoot", NULL, J9SH_DIRPERM_ABSENT, &cacheSize, &localCrashCntr, true, &cacheHasIntegrity) != 0) {
		ERRPRINTF(("Could not start cache the second time\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if ((runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION) != 0) {
		ERRPRINTF(("Store contention is enabled after second startup\n"));
		retval = TEST_ERROR;
		goto done;
	}

	if ((runtimeFlags & J9SHR_RUNTIMEFLAG_BLOCK_SPACE_FULL) == 0) {
		ERRPRINTF(("Cache is not full after second startup\n"));
		retval = TEST_ERROR;
	}
#endif

	done:
	j9mem_free_memory(memForCC);
	return retval;
}

static IDATA
test5(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
	SH_CompositeCacheImpl *testCC = NULL;
	J9SharedClassPreinitConfig* piconfig = NULL;
	UDATA maxSharedStringTableSize, sharedStringTableSize;
	const char* testName = "test5";
	j9tty_printf(PORTLIB, "%s: create composite cache with -Xitsn value bigger than max shared intern table node count and check -Xitsn option is not restricted by max shared table node count \n", testName);

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	maxSharedStringTableSize = srpHashTable_requiredMemorySize(SHRINIT_MAX_SHARED_STRING_TABLE_NODE_COUNT, sizeof(J9SharedInternSRPHashTableEntry), TRUE);
	/* Make sure shared cache is big enough */
	piconfig->sharedClassCacheSize = 2 * maxSharedStringTableSize;
	piconfig->sharedClassReadWriteBytes = -1;
	/* simulate -Xitsn option is used with a value bigger than SHRINIT_MAX_SHARED_STRING_TABLE_NODE_COUNT */
	piconfig->sharedClassInternTableNodeCount = SHRINIT_MAX_SHARED_STRING_TABLE_NODE_COUNT + 100;
	piconfig->sharedClassMinAOTSize = -1;
	piconfig->sharedClassMaxAOTSize = -1;
	piconfig->sharedClassMinJITSize = -1;
	piconfig->sharedClassMaxJITSize = -1;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;
	ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, piconfig);
	retval = createTestCompositeCache(vm, &testCC, piconfig, testName);

	if (retval != TEST_PASS) {
		/* Error is captured and printed in createTestCompositeCache */
		goto done;
	}

	sharedStringTableSize = srpHashTable_requiredMemorySize((U_32)(piconfig->sharedClassInternTableNodeCount), sizeof(J9SharedInternSRPHashTableEntry), TRUE);

	if (testCC->getStringTableBytes() != sharedStringTableSize){
		ERRPRINTF(("Read/write area size incorrect.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	j9mem_free_memory(testCC);
	j9mem_free_memory(piconfig);
	return retval;
}

static IDATA
test6(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
	SH_CompositeCacheImpl *testCC = NULL;
	J9SharedClassPreinitConfig* piconfig = NULL;
	IDATA maxSharedStringTableSize;
	const char* testName = "test6";
	j9tty_printf(PORTLIB, "%s: create composite cache with -Xitsn that requires more memory than cache size \n", testName);

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}
	maxSharedStringTableSize = (IDATA)srpHashTable_requiredMemorySize(SHRINIT_MAX_SHARED_STRING_TABLE_NODE_COUNT, sizeof(J9SharedInternSRPHashTableEntry), TRUE);

	/* set cache size half of the required size by shared string table */
	piconfig->sharedClassCacheSize = maxSharedStringTableSize / 2;
	/*set sharedClassReadWriteBytes to a random value other than -1*/
	piconfig->sharedClassReadWriteBytes = 100;
	piconfig->sharedClassInternTableNodeCount = SHRINIT_MAX_SHARED_STRING_TABLE_NODE_COUNT;
	piconfig->sharedClassMinAOTSize = -1;
	piconfig->sharedClassMaxAOTSize = -1;
	piconfig->sharedClassMinJITSize = -1;
	piconfig->sharedClassMaxJITSize = -1;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;
	ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, piconfig);
	retval = createTestCompositeCache(vm, &testCC, piconfig, testName);

	if (retval != TEST_PASS) {
		/* Error is captured and printed in createTestCompositeCache */
		goto done;
	}

	/* Since -Xitsn value is too big to fit into shared cache,
	 * we expect sharedClassReadWriteBytes to be set to -1,
	 * indicating that shared string table will be generated by proportional to the given shared cache size.
	 *
	 */
	if (piconfig->sharedClassReadWriteBytes != -1){
		ERRPRINTF(("Read/write area size incorrect.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	j9mem_free_memory(testCC);
	j9mem_free_memory(piconfig);
	return retval;
}


static IDATA
test7(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
#if !defined(J9SHR_CACHELET_SUPPORT)
	SH_CompositeCacheImpl *testCC = NULL;
	J9SharedClassPreinitConfig* piconfig = NULL;
#endif

	const char* testName = "test7";
	j9tty_printf(PORTLIB, "%s: create composite cache with minaot size that reserves memory more than cache size \n", testName);

#if defined(J9SHR_CACHELET_SUPPORT)
	retval = TEST_PASS;
#else

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	piconfig->sharedClassCacheSize = MAIN_CACHE_SIZE;
	piconfig->sharedClassReadWriteBytes = -1;
	piconfig->sharedClassInternTableNodeCount = -1;
	piconfig->sharedClassMinAOTSize = MAIN_CACHE_SIZE + 1024;
	piconfig->sharedClassMaxAOTSize = -1;
	piconfig->sharedClassMinJITSize = -1;
	piconfig->sharedClassMaxJITSize = -1;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;
	ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, piconfig);
	retval = createTestCompositeCache(vm, &testCC, piconfig, testName);

	if (retval != TEST_PASS) {
		/* Error is captured and printed in createTestCompositeCache */
		goto done;
	}

	/* Since -XscminAOT value is too big to fit into shared cache,
	 * we expect it to be setback to cachesize
	 *
	 */
	if (piconfig->sharedClassMinAOTSize != MAIN_CACHE_SIZE){
		ERRPRINTF(("Minimum AOT size is incorrect.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	j9mem_free_memory(testCC);
	j9mem_free_memory(piconfig);
#endif
	return retval;
}

static IDATA
test8(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
	SH_CompositeCacheImpl *testCC = NULL;
	J9SharedClassPreinitConfig* piconfig = NULL;
	const char* testName = "test8";
	j9tty_printf(PORTLIB, "%s: create composite cache with maxaot size equal to cache size \n", testName);

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	piconfig->sharedClassCacheSize = MAIN_CACHE_SIZE;
	piconfig->sharedClassReadWriteBytes = -1;
	piconfig->sharedClassInternTableNodeCount = -1;
	piconfig->sharedClassMinAOTSize = -1;
	piconfig->sharedClassMaxAOTSize = MAIN_CACHE_SIZE;
	piconfig->sharedClassMinJITSize = -1;
	piconfig->sharedClassMaxJITSize = -1;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;
	ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, piconfig);
	retval = createTestCompositeCache(vm, &testCC, piconfig, testName);

	if (retval != TEST_PASS) {
		/* Error is captured and printed in createTestCompositeCache */
		goto done;
	}

	/*  -XscmaxAOT value should be set to shared
	 *  cache size
	 */

	if ((UDATA)piconfig->sharedClassMaxAOTSize != piconfig->sharedClassCacheSize ){
		ERRPRINTF(("Maximum AOT size is incorrect.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	j9mem_free_memory(testCC);
	j9mem_free_memory(piconfig);
	return retval;
}


static IDATA
test9(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
#if !defined(J9SHR_CACHELET_SUPPORT)
	SH_CompositeCacheImpl *testCC = NULL;
	J9SharedClassPreinitConfig* piconfig = NULL;
#endif
	const char* testName = "test9";
	j9tty_printf(PORTLIB, "%s: create composite cache with minjit data size that reserves memory more than cache size \n", testName);

#if defined(J9SHR_CACHELET_SUPPORT)
	retval = TEST_PASS;
#else

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	piconfig->sharedClassCacheSize = MAIN_CACHE_SIZE;
	piconfig->sharedClassReadWriteBytes = -1;
	piconfig->sharedClassInternTableNodeCount = -1;
	piconfig->sharedClassMinAOTSize = -1 ;
	piconfig->sharedClassMaxAOTSize = -1;
	piconfig->sharedClassMinJITSize = MAIN_CACHE_SIZE + 1024;
	piconfig->sharedClassMaxJITSize = -1;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;
	ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, piconfig);
	retval = createTestCompositeCache(vm, &testCC, piconfig, testName);

	if (retval != TEST_PASS) {
		/* Error is captured and printed in createTestCompositeCache */
		goto done;
	}

	/* Since -Xscminjitdata value is too big to fit into shared cache,
	 * we expect it to be setback to cachesize
	 *
	 */
	if (piconfig->sharedClassMinJITSize != MAIN_CACHE_SIZE){
		ERRPRINTF(("Minimum JIT data size is incorrect.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	j9mem_free_memory(testCC);
	j9mem_free_memory(piconfig);
#endif
	return retval;
}

static IDATA
test10(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
	SH_CompositeCacheImpl *testCC = NULL;
	J9SharedClassPreinitConfig* piconfig = NULL;
	const char* testName = "test10";
	j9tty_printf(PORTLIB, "%s: create composite cache with maxjit data equal to cache size \n", testName);

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}
	piconfig->sharedClassCacheSize = MAIN_CACHE_SIZE;
	piconfig->sharedClassReadWriteBytes = -1;
	piconfig->sharedClassInternTableNodeCount = -1;
	piconfig->sharedClassMinAOTSize = -1;
	piconfig->sharedClassMaxAOTSize = -1;
	piconfig->sharedClassMinJITSize = -1;
	piconfig->sharedClassMaxJITSize = MAIN_CACHE_SIZE;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;
	ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, piconfig);
	retval = createTestCompositeCache(vm, &testCC, piconfig, testName);

	if (retval != TEST_PASS) {
		/* Error is captured and printed in createTestCompositeCache */
		goto done;
	}

	/*
	 * -Xscmaxjitdata value should be set to shared cache size
	 */
	if ((UDATA)piconfig->sharedClassMaxJITSize != piconfig->sharedClassCacheSize ){
		ERRPRINTF(("Maximum JIT data size is incorrect.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	j9mem_free_memory(testCC);
	j9mem_free_memory(piconfig);
	return retval;
}

static IDATA
test11(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
	SH_CompositeCacheImpl *testCC = NULL;
	J9SharedClassPreinitConfig* piconfig = NULL;
	const char* testName = "test11";
	j9tty_printf(PORTLIB, "%s: create composite cache with maxaot size bigger than cache size \n", testName);

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}
	piconfig->sharedClassCacheSize = MAIN_CACHE_SIZE;
	piconfig->sharedClassReadWriteBytes = -1;
	piconfig->sharedClassInternTableNodeCount = -1;
	piconfig->sharedClassMinAOTSize = -1;
	piconfig->sharedClassMaxAOTSize = MAIN_CACHE_SIZE+1024;
	piconfig->sharedClassMinJITSize = -1;
	piconfig->sharedClassMaxJITSize = -1;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;
	ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, piconfig);
	retval = createTestCompositeCache(vm, &testCC, piconfig, testName);

	if (retval != TEST_PASS) {
		/* Error is captured and printed in createTestCompositeCache */
		goto done;
	}

	/*
	 * -Xscmaxaot value should be set to unlimited
	 */
	if (-1 != piconfig->sharedClassMaxAOTSize ){
		ERRPRINTF(("Maximum AOT data size is incorrect.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	j9mem_free_memory(testCC);
	j9mem_free_memory(piconfig);
	return retval;
}

static IDATA
test12(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
	SH_CompositeCacheImpl *testCC = NULL;
	J9SharedClassPreinitConfig* piconfig = NULL;
	const char* testName = "test12";
	j9tty_printf(PORTLIB, "%s: create composite cache with maxjit data size bigger than cache size \n", testName);

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}
	piconfig->sharedClassCacheSize = MAIN_CACHE_SIZE;
	piconfig->sharedClassReadWriteBytes = -1;
	piconfig->sharedClassInternTableNodeCount = -1;
	piconfig->sharedClassMinAOTSize = -1;
	piconfig->sharedClassMaxAOTSize = -1;
	piconfig->sharedClassMinJITSize = -1;
	piconfig->sharedClassMaxJITSize = MAIN_CACHE_SIZE+1024;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;
	ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, piconfig);
	retval = createTestCompositeCache(vm, &testCC, piconfig, testName);

	if (retval != TEST_PASS) {
		/* Error is captured and printed in createTestCompositeCache */
		goto done;
	}

	/*
	 * -Xscmaxjitdata value should be set to unlimited
	 */
	if (-1 != piconfig->sharedClassMaxJITSize ){
		ERRPRINTF(("Maximum JIT data size is incorrect.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	done:
	j9mem_free_memory(testCC);
	j9mem_free_memory(piconfig);
	return retval;
}


static IDATA
test13(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
	J9SharedClassPreinitConfig* piconfig = NULL;
	const char* testName = "test13";
	j9tty_printf(PORTLIB, "%s: create composite cache with maxaot data size is lesser than minaot data size\n", testName);

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	piconfig->sharedClassCacheSize = MAIN_CACHE_SIZE;
	piconfig->sharedClassReadWriteBytes = -1;
	piconfig->sharedClassInternTableNodeCount = -1;
	piconfig->sharedClassMinAOTSize = MAIN_CACHE_SIZE-100;
	piconfig->sharedClassMaxAOTSize = MAIN_CACHE_SIZE-200;
	piconfig->sharedClassMinJITSize = -1;
	piconfig->sharedClassMaxJITSize = -1;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;

	if(0 == ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, piconfig))
	{
		ERRPRINTF(("Failed to ensure correct sizes for minaot and maxaot.\n"));
		retval = TEST_ERROR;
	}

done:
	j9mem_free_memory(piconfig);
	return retval;
}

static IDATA
test14(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA retval = TEST_PASS;
	J9SharedClassPreinitConfig* piconfig = NULL;
	const char* testName = "test14";
	j9tty_printf(PORTLIB, "%s: create composite cache with maxjit data size is lesser than minjit data size \n", testName);

	if (!(piconfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		ERRPRINTF(("Failed to allocate memory for piconfig.\n"));
		retval = TEST_ERROR;
		goto done;
	}

	piconfig->sharedClassCacheSize = MAIN_CACHE_SIZE;
	piconfig->sharedClassReadWriteBytes = -1;
	piconfig->sharedClassInternTableNodeCount = -1;
	piconfig->sharedClassMinAOTSize = -1;
	piconfig->sharedClassMaxAOTSize = -1;
	piconfig->sharedClassMinJITSize = MAIN_CACHE_SIZE-100;
	piconfig->sharedClassMaxJITSize = MAIN_CACHE_SIZE-200;
	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassSoftMaxBytes = -1;
	if(0 == ensureCorrectCacheSizes(vm, vm->portLibrary, 0, (UDATA)0, piconfig))
	{
		ERRPRINTF(("Failed to ensure correct sizes for minjit and maxjit.\n"));
		retval = TEST_ERROR;
	}

done:
	j9mem_free_memory(piconfig);
	return retval;
}

