/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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

/* Includes */

extern "C"
{
#include "shrinit.h"
}
#include "UnitTest.hpp"
#include "SCTestCommon.h"
#include "main.h"

typedef char* BlockPtr;
/* Exports */
extern "C"
{
	IDATA testStartupHints(J9JavaVM* vm);
}

/* Function prototypes */
IDATA storeAndFindHintsTest(J9JavaVM* currentThread);

/* Main test function */
IDATA testStartupHints(J9JavaVM* vm)
{
	UDATA rc = FAIL;

	PORT_ACCESS_FROM_JAVAVM(vm);

	REPORT_START("Test Startup Hints");

	UnitTest::unitTest = UnitTest::STARTUP_HINTS_TEST;

	rc = storeAndFindHintsTest(vm);

	UnitTest::unitTest = UnitTest::NO_TEST;

	return rc;
}

/* Individual test functions... */
IDATA storeAndFindHintsTest(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);

	const char* testName = "storeAndFindHintsTest";
	IDATA rc = PASS;
	BlockPtr cache = NULL;
	BlockPtr cacheAllocated = NULL;
	SH_CacheMap* cacheObject = NULL;
	void* cacheObjectMemory = NULL;
	J9SharedClassPreinitConfig* origPiConfig = NULL;
	J9SharedClassPreinitConfig* sharedpiConfig = NULL;
	J9SharedClassConfig* origSharedClassConfig = NULL;
	J9SharedClassConfig* sharedConfig = NULL;
	bool cacheHasIntegrity = false;
	UDATA data1 = 0;
	UDATA data2 = 0;
	UDATA hints1[2] = {10, 20};
	UDATA hints2[2] = {30, 40};
	const U_8* addressInCache = NULL;
	const U_8* storeRet = NULL;


	J9VMThread* currentThread = vm->internalVMFunctions->currentVMThread(vm);

	IDATA cacheObjectSize = SH_CacheMap::getRequiredConstrBytes(false);
	/* j9mmap_get_region_granularity returns 0 on zOS */
	UDATA osPageSize = j9mmap_get_region_granularity(NULL);

	INFOPRINTF("Entering Store And Find Hints Test\n");

	/* Create a shared config object and attach it to the VM */
	sharedConfig = (J9SharedClassConfig*)j9mem_allocate_memory(sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
	if (NULL == sharedConfig) {
		ERRPRINTF("Failed to allocate memory for sharedConfig\n");
		rc = FAIL;
		goto cleanup;
	}

	memset(sharedConfig, 0, sizeof(J9SharedClassConfig) + sizeof(J9SharedClassCacheDescriptor));
	sharedConfig->softMaxBytes = -1;
	sharedConfig->minAOT = -1;
	sharedConfig->maxAOT = -1;
	sharedConfig->minJIT = -1;
	sharedConfig->maxJIT = -1;
	sharedConfig->cacheDescriptorList = (J9SharedClassCacheDescriptor*)((UDATA)sharedConfig + sizeof(J9SharedClassConfig));
	sharedConfig->cacheDescriptorList->next = sharedConfig->cacheDescriptorList;
	sharedConfig->storeGCHints = j9shr_storeGCHints;
	sharedConfig->findGCHints = j9shr_findGCHints;

	sharedpiConfig = (J9SharedClassPreinitConfig*)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == sharedpiConfig) {
		ERRPRINTF("Failed to allocate memory for sharedpiConfig\n");
		rc = FAIL;
		goto cleanup;
	}

	memset(sharedpiConfig, 0, sizeof(J9SharedClassPreinitConfig));

	sharedConfig->runtimeFlags =
		J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING          |
		J9SHR_RUNTIMEFLAG_ENABLE_TIMESTAMP_CHECKS        |
		J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION |
		J9SHR_RUNTIMEFLAG_ENABLE_BYTECODEFIX;

	origSharedClassConfig = vm->sharedClassConfig;
	origPiConfig = vm->sharedClassPreinitConfig;

	vm->sharedClassConfig = sharedConfig;
	vm->sharedClassPreinitConfig = sharedpiConfig;

	if (osPageSize != 0) {
		sharedpiConfig->sharedClassCacheSize = ROUND_UP_TO(osPageSize, SMALL_CACHE_SIZE);
	} else {
		sharedpiConfig->sharedClassCacheSize = SMALL_CACHE_SIZE;
	}

	sharedpiConfig->sharedClassDebugAreaBytes = -1;
	sharedpiConfig->sharedClassReadWriteBytes = 0;
	sharedpiConfig->sharedClassMinAOTSize = -1;
	sharedpiConfig->sharedClassMaxAOTSize = -1;
	sharedpiConfig->sharedClassMinJITSize = -1;
	sharedpiConfig->sharedClassMaxJITSize = -1;
	sharedpiConfig->sharedClassSoftMaxBytes = -1;

	/* Allocate and initialize the required memory */
	cacheAllocated = (BlockPtr)j9mem_allocate_memory(sharedpiConfig->sharedClassCacheSize + osPageSize, J9MEM_CATEGORY_CLASSES);

	if (NULL == cacheAllocated) {
		ERRPRINTF("Failed to allocate memory for cacheAllocated\n");
		rc = FAIL;
		goto cleanup;
	}
	memset(cacheAllocated, 0, sharedpiConfig->sharedClassCacheSize + osPageSize);
	if (osPageSize != 0) {
		cache = (BlockPtr) ROUND_UP_TO(osPageSize, (UDATA)cacheAllocated);
	} else {
		cache = cacheAllocated;
	}

	cacheObjectMemory = j9mem_allocate_memory(cacheObjectSize, J9MEM_CATEGORY_CLASSES);
	if (NULL == cacheObjectMemory) {
		ERRPRINTF("Failed to allocate memory for cacheObjectMemory\n");
		rc = FAIL;
		goto cleanup;
	}

	memset(cacheObjectMemory, 0, cacheObjectSize);

	/* Create and initialize the cache map object */
	cacheObject = SH_CacheMap::newInstance(vm, sharedConfig, (SH_CacheMap*)cacheObjectMemory, "cache1", 0);

  	/* Start the cache object */

	rc = cacheObject->startup(currentThread, sharedpiConfig, "Root1", NULL, J9SH_DIRPERM_ABSENT, cache, &cacheHasIntegrity);

	/* Report progress so far */
	INFOPRINTF5("Store And Find Hints Test cos=%d cs=%d co=%x cba=%x rc=%d\n", cacheObjectSize, sharedpiConfig->sharedClassCacheSize, cacheObject, cache, rc);
	if (0 != rc) {
		ERRPRINTF("Failed to startup cacheObject\n");
		rc = FAIL;
		goto cleanup;
	}
	sharedConfig->sharedClassCache = (void*)cacheObject;
	sharedConfig->runtimeFlags |= J9SHR_RUNTIMEFLAG_CACHE_INITIALIZATION_COMPLETE;


	/* write a set of startup hints to the cache, and test such hints can be found */
	memset(&vm->sharedClassConfig->localStartupHints, 0, sizeof(J9SharedLocalStartupHints));
	sharedConfig->storeGCHints(currentThread, hints1[0], hints1[1], TRUE);
	storeRet = storeStartupHintsToSharedCache(currentThread);
	if (NULL == storeRet) {
		ERRPRINTF("storeStartupHintsToSharedCache returns NULL. Failed to store hints to the shared cache\n");
		rc = FAIL;
		goto cleanup;
	}
	addressInCache = storeRet;
	/* Make sure the hints are found from the cache, not the local copy, so clear local hints */
	memset(&vm->sharedClassConfig->localStartupHints, 0, sizeof(J9SharedLocalStartupHints));
	sharedConfig->findGCHints(currentThread, &data1, &data2);
	if (hints1[0] != data1) {
		ERRPRINTF2("The hintData1 is %zu, the expected value is %zu\n", data1, hints1[0]);
		rc = FAIL;
		goto cleanup;
	}
	if (hints1[1] != data2) {
		ERRPRINTF2("The hintData1 is %zu, the expected value is %zu\n", data2, hints1[1]);
		rc = FAIL;
		goto cleanup;
	}

	/* Try writing another startup hints to the cache, the existing hints should not be overwritten */
	memset(&vm->sharedClassConfig->localStartupHints, 0, sizeof(J9SharedLocalStartupHints));
	sharedConfig->storeGCHints(currentThread, hints2[0], hints2[1], FALSE);
	storeRet = storeStartupHintsToSharedCache(currentThread);
	if (NULL == storeRet) {
		ERRPRINTF("storeStartupHintsToSharedCache returns NULL. Failed to store hints to the shared cache\n");
		rc = FAIL;
		goto cleanup;
	} else if (addressInCache != storeRet) {
		ERRPRINTF2("storeStartupHintsToSharedCache returns incorrect address. It returns %p. it is expected to be %p\n", storeRet, addressInCache);
		rc = FAIL;
		goto cleanup;
	}
	/* Make sure the hints are found from the cache, not the local copy, so clear local hints */
	memset(&vm->sharedClassConfig->localStartupHints, 0, sizeof(J9SharedLocalStartupHints));
	sharedConfig->findGCHints(currentThread, &data1, &data2);
	if (hints1[0] != data1) {
		ERRPRINTF2("The hintData1 is %zu, the expected value is %zu\n", data1, hints1[0]);
		rc = FAIL;
		goto cleanup;
	}
	if (hints1[1] != data2) {
		ERRPRINTF2("The hintData1 is %zu, the expected value is %zu\n", data2, hints1[1]);
		rc = FAIL;
		goto cleanup;
	}

	/* Try overwrite the startup hints in the shared cache */
	memset(&vm->sharedClassConfig->localStartupHints, 0, sizeof(J9SharedLocalStartupHints));
	sharedConfig->storeGCHints(currentThread, hints2[0], hints2[1], TRUE);
	storeRet = storeStartupHintsToSharedCache(currentThread);
	if (NULL == storeRet) {
		ERRPRINTF("storeStartupHintsToSharedCache returns NULL. Failed to store hints to the shared cache\n");
		rc = FAIL;
		goto cleanup;
	} else if (addressInCache != storeRet) {
		ERRPRINTF2("storeStartupHintsToSharedCache returns incorrect address. It returns %p. it is expected to be %p\n", storeRet, addressInCache);
		rc = FAIL;
		goto cleanup;
	}
	/* Make sure the hints are found from the cache, not the local copy, so clear local hints */
	memset(&vm->sharedClassConfig->localStartupHints, 0, sizeof(J9SharedLocalStartupHints));
	sharedConfig->findGCHints(currentThread, &data1, &data2);
	if (hints2[0] != data1) {
		ERRPRINTF2("The hintData1 is %zu, the expected value is %zu\n", data1, hints2[0]);
		rc = FAIL;
		goto cleanup;
	}
	if (hints2[1] != data2) {
		ERRPRINTF2("The hintData1 is %zu, the expected value is %zu\n", data2, hints2[1]);
		rc = FAIL;
		goto cleanup;
	}


cleanup:
	if (NULL != cacheObject) {
		cacheObject->cleanup(vm->mainThread);
		j9mem_free_memory(cacheObject);
		cacheObject = NULL;
	}
	if (NULL != sharedConfig) {
		vm->sharedClassConfig = origSharedClassConfig;
		j9mem_free_memory(sharedConfig);
	}
	if (NULL != sharedpiConfig) {
		vm->sharedClassPreinitConfig = origPiConfig;
		j9mem_free_memory(sharedpiConfig);
	}

	if (NULL != cacheAllocated) {
		j9mem_free_memory(cacheAllocated);
		cacheAllocated = NULL;
		cache = NULL;
	}

	REPORT_SUMMARY("Startup Hints Test", rc);

	return rc;
}

