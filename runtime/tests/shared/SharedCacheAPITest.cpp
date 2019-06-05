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

extern "C" {

#include "j9port.h"
#include <string.h>
#include "CacheMap.hpp"
#include "OSCache.hpp"
#include "OSCacheFile.hpp"
#include "OSCachemmap.hpp"
#include "OSCachesysv.hpp"
#include "OSCachesysv.cpp"
#include "CacheLifecycleManager.hpp"
#include "main.h"
#include "j2sever.h"
#include "SharedCacheAPITest.hpp"

static CacheInfo cacheInfoList[NUM_CACHE + NUM_SNAPSHOT];
I_32 cacheCount;
I_32 testCacheCount;

void setCurrentCacheVersion(J9JavaVM *vm, UDATA j2seVersion, J9PortShcVersion *versionData);
IDATA j9shr_createCacheSnapshot(J9JavaVM* vm, const char* cacheName);

void
getCacheDir(J9JavaVM *vm, char *cacheDir)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA rc;
	char cacheDirNoTestBasedir[J9SH_MAXPATH];
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /* defined(OPENJ9_BUILD) */

	rc = j9shmem_getDir(NULL, flags, cacheDirNoTestBasedir, J9SH_MAXPATH);
	if (rc < 0) {
		j9tty_printf(PORTLIB, "Cannot get a directory\n");
	}

	sprintf(cacheDir, "%s%s", cacheDirNoTestBasedir, TEST_BASEDIR);
}

void
populateCacheInfoList(J9JavaVM *vm, J9SharedClassConfig *sharedClassConfig, const char *cacheName, char *cacheDir)
{
	IDATA i;

	for (i = 0; i < NUM_CACHE; i++) {

		cacheInfoList[i].name = cacheName;
		cacheInfoList[i].cacheSize = CACHE_SIZE;
		cacheInfoList[i].cacheDir = cacheDir;
		cacheInfoList[i].found = FALSE;

		if (NONPERSISTENT_CACHE_INDEX == i) {
			cacheInfoList[i].cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
		} else {
			cacheInfoList[i].cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
		}
	}
#if !defined(WIN32)
	if (NUM_SNAPSHOT > 0) {
		cacheInfoList[NUM_CACHE].name = cacheName;
		cacheInfoList[NUM_CACHE].cacheSize = (UDATA)J9SH_OSCACHE_UNKNOWN;
		cacheInfoList[NUM_CACHE].softMaxBytes = (UDATA)J9SH_OSCACHE_UNKNOWN;
		cacheInfoList[NUM_CACHE].cacheDir = cacheDir;
		cacheInfoList[NUM_CACHE].found = FALSE;
		cacheInfoList[NUM_CACHE].cacheType = J9PORT_SHR_CACHE_TYPE_SNAPSHOT;
		cacheInfoList[NUM_CACHE].debugBytes = (UDATA)J9SH_OSCACHE_UNKNOWN;
	}
#endif /* !defined(WIN32) */
}

IDATA
createTestCache(J9JavaVM* vm, J9SharedClassPreinitConfig *piconfig, J9SharedClassConfig *sharedClassConfig)
{
	void* memory;
	IDATA rc = PASS;
	IDATA cacheObjectSize;
	I_32 cacheType;
	IDATA i;
	SH_CacheMap* cacheMap[NUM_CACHE];
	PORT_ACCESS_FROM_JAVAVM(vm);

	memset(cacheMap, 0, sizeof(cacheMap));

	for(i = 0; i < NUM_CACHE; i++) {
		memset(piconfig, 0, sizeof(J9SharedClassPreinitConfig));
		memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig));

		piconfig->sharedClassDebugAreaBytes = -1;
		piconfig->sharedClassCacheSize = CACHE_SIZE;
		piconfig->sharedClassSoftMaxBytes = CACHE_SIZE;

		sharedClassConfig->runtimeFlags =
				J9SHR_RUNTIMEFLAG_ENABLE_LOCAL_CACHEING          |
				J9SHR_RUNTIMEFLAG_ENABLE_TIMESTAMP_CHECKS        |
				J9SHR_RUNTIMEFLAG_ENABLE_REDUCE_STORE_CONTENTION |
				J9SHR_RUNTIMEFLAG_ENABLE_BYTECODEFIX;

		sharedClassConfig->ctrlDirName = cacheInfoList[i].cacheDir;
		sharedClassConfig->cacheDescriptorList = (J9SharedClassCacheDescriptor *) j9mem_allocate_memory(sizeof(J9SharedClassCacheDescriptor), J9MEM_CATEGORY_CLASSES);
		if (sharedClassConfig->cacheDescriptorList == NULL) {
			j9tty_printf(PORTLIB, "failed to alloc cacheDescriptorList\n");
			rc = FAIL;
			goto cleanup;
		}

		memset(sharedClassConfig->cacheDescriptorList, 0, sizeof(J9SharedClassCacheDescriptor));
		sharedClassConfig->cacheDescriptorList->next = sharedClassConfig->cacheDescriptorList;

		vm->sharedClassConfig = sharedClassConfig;
		vm->sharedClassPreinitConfig = piconfig;

		cacheObjectSize = SH_CacheMap::getRequiredConstrBytes(false);
		memory = j9mem_allocate_memory(cacheObjectSize, J9MEM_CATEGORY_CLASSES);
		if (memory == 0) {
			j9tty_printf(PORTLIB, "failed to alloc CacheMap object\n");
			rc = FAIL;
			goto cleanup;
		}

		cacheType = (I_32)cacheInfoList[i].cacheType;
		cacheMap[i] = SH_CacheMap::newInstance(vm, sharedClassConfig, (SH_CacheMap*)memory, cacheInfoList[i].name, cacheType);

		bool cacheHasIntegrity;
		rc = cacheMap[i]->startup(vm->mainThread, piconfig, cacheInfoList[i].name, cacheInfoList[i].cacheDir, J9SH_DIRPERM_ABSENT, NULL, &cacheHasIntegrity);
		if (rc != 0) {
			j9tty_printf(PORTLIB, "CacheMap.startup() failed\n");
			rc = FAIL;
			goto cleanup;
		}
		cacheInfoList[i].debugBytes = cacheMap[i]->getDebugBytes();
	}
#if !defined(WIN32)
	if (NUM_SNAPSHOT > 0) {
		vm ->sharedClassConfig->sharedClassCache = (SH_CacheMap*)cacheMap[NONPERSISTENT_CACHE_INDEX];
		rc = j9shr_createCacheSnapshot(vm, cacheInfoList[NUM_CACHE].name);
		vm ->sharedClassConfig->sharedClassCache= NULL;
		if (rc != 0) {
			j9tty_printf(PORTLIB, "j9shr_createCacheSnapshot() failed\n");
			rc = FAIL;
			goto cleanup;
		}
	}
#endif /* !defined(WIN32) */

cleanup:
	for (i = 0; i < NUM_CACHE; i++) {
		if (cacheMap[i] != NULL) {
			cacheMap[i]->cleanup(vm->mainThread);
			j9mem_free_memory(cacheMap[i]);
		}
	}

	if (sharedClassConfig->cacheDescriptorList != NULL) {
		j9mem_free_memory(sharedClassConfig->cacheDescriptorList);
	}

	return rc;
}

IDATA
countSharedCacheCallback(J9JavaVM *vm, J9SharedCacheInfo *cacheInfo, void *userData) {
	BOOLEAN displayCacheInfo = *(BOOLEAN *)userData;
	PORT_ACCESS_FROM_JAVAVM(vm);

	cacheCount++;

	if (TRUE == displayCacheInfo) {
		I_32 i;

		j9tty_printf(PORTLIB, "cache name: %s\t", cacheInfo->name);
		j9tty_printf(PORTLIB, "cacheType:: %d\t", cacheInfo->cacheType);
		j9tty_printf(PORTLIB, "isCompatible: %d\t", cacheInfo->isCompatible);
		j9tty_printf(PORTLIB, "modLevel: %d\t", cacheInfo->modLevel);
		j9tty_printf(PORTLIB, "address mode: %d\t", cacheInfo->addrMode);
		j9tty_printf(PORTLIB, "isCorrupt: %d\n", cacheInfo->isCorrupt);
		for (i = 0; i < (NUM_CACHE + NUM_SNAPSHOT); i++) {
			if ((strcmp(cacheInfo->name, cacheInfoList[i].name) == 0) && (cacheInfo->cacheType == cacheInfoList[i].cacheType)) {
				testCacheCount++;
			}
		}
	}

	return 0;
}

IDATA
validateSharedCacheCallback(J9JavaVM *vm, J9SharedCacheInfo *cacheInfo, void *userData)
{
	I_32 i;
	UDATA cacheSize;
	UDATA addrMode = J9SH_ADDRMODE;

	cacheCount++;

	if (J9JAVAVM_COMPRESS_OBJECT_REFERENCES(vm)) {
		addrMode |= COM_IBM_ITERATE_SHARED_CACHES_COMPRESSED_POINTERS_MODE;
	} else {
		addrMode |= COM_IBM_ITERATE_SHARED_CACHES_NON_COMPRESSED_POINTERS_MODE;
	}

	/* calculate cache size for empty cache */
	if (J9PORT_SHR_CACHE_TYPE_PERSISTENT == cacheInfo->cacheType) {
		cacheSize = CACHE_SIZE-(MMAP_CACHEHEADERSIZE+sizeof(J9SharedCacheHeader));
	} else if(J9PORT_SHR_CACHE_TYPE_NONPERSISTENT == cacheInfo->cacheType) {
		cacheSize = CACHE_SIZE-(SHM_CACHEHEADERSIZE+sizeof(J9SharedCacheHeader));
#if !defined(WIN32)
	} else {
		/* J9PORT_SHR_CACHE_TYPE_SNAPSHOT == cacheInfo->cacheType */
		if ((strcmp(cacheInfo->name, cacheInfoList[NUM_CACHE].name) == 0)
			&& (cacheInfo->cacheType == cacheInfoList[NUM_CACHE].cacheType)
			&& (J9SH_OSCACHE_UNKNOWN == (IDATA)cacheInfo->isCorrupt)
			&& (1 == cacheInfo->isCompatible)
			&& (addrMode == cacheInfo->addrMode)
			&& (getShcModlevelForJCL(J2SE_VERSION(vm)) == cacheInfo->modLevel)
			&& (J9SH_OSCACHE_UNKNOWN == cacheInfo->lastDetach)
			&& (J9SH_OSCACHE_UNKNOWN == (IDATA)cacheInfo->os_shmid)
			&& (J9SH_OSCACHE_UNKNOWN == (IDATA)cacheInfo->os_semid)
			&& (J9SH_OSCACHE_UNKNOWN == (IDATA)cacheInfo->cacheSize)
			&& (J9SH_OSCACHE_UNKNOWN == (IDATA)cacheInfo->freeBytes)
			&& (J9SH_OSCACHE_UNKNOWN == (IDATA)cacheInfo->softMaxBytes)
		) {
			cacheInfoList[NUM_CACHE].found = TRUE;
		}
		return 0;
#endif /* !defined(WIN32) */
	}

	/* no need to check for modLevel and addrMode as isCompatible field takes care of it */
	for (i = 0; i < NUM_CACHE; i++) {
		if (strcmp(cacheInfo->name, cacheInfoList[i].name) == 0) {
			if ((cacheInfo->cacheType != cacheInfoList[i].cacheType) ||
				(cacheInfo->isCorrupt != 0) ||
				(cacheInfo->isCompatible != 1) ||
				(cacheInfo->cacheSize != cacheSize) ||
				(CACHE_SIZE != cacheInfo->softMaxBytes) ||
				(cacheInfo->freeBytes != (cacheSize - cacheInfoList[i].debugBytes)) ||
				(cacheInfo->addrMode != addrMode) ||
				(cacheInfo->modLevel != getShcModlevelForJCL(J2SE_VERSION(vm)) ||
				(cacheInfo->lastDetach <= 0))) {
				continue;
			}

			/* for persistent cache, os_shmid and os_semid should always be -1 */
			if ((J9PORT_SHR_CACHE_TYPE_PERSISTENT == cacheInfo->cacheType) && (((IDATA)cacheInfo->os_shmid != -1) || ((IDATA)cacheInfo->os_semid != -1))) {
				continue;
			}
#if !defined(WIN32)
			if ((J9PORT_SHR_CACHE_TYPE_NONPERSISTENT == cacheInfo->cacheType) && (((IDATA)cacheInfo->os_shmid <= 0) || ((IDATA)cacheInfo->os_semid <= 0))) {
				continue;
			}
#else
			if ((J9PORT_SHR_CACHE_TYPE_NONPERSISTENT == cacheInfo->cacheType) && (((IDATA)cacheInfo->os_shmid > 0) || ((IDATA)cacheInfo->os_semid > 0))) {
				continue;
			}
#endif
			cacheInfoList[i].found = TRUE;
			break;
		}
	}
	return 0;
}

IDATA
iterateSharedCache(J9JavaVM *vm, char *ctrlDirName, UDATA groupPerm, BOOLEAN useCommandLineValues, IDATA (*callback)(J9JavaVM *vm, J9SharedCacheInfo *cacheInfo, void *userData)) {
	IDATA rc = PASS;
	BOOLEAN userData = TRUE;

	rc = j9shr_iterateSharedCaches(vm, ctrlDirName, groupPerm, useCommandLineValues, callback, (void *)&userData);
	if (rc != 0) {
		rc = FAIL;
		return rc;
	}
	return rc;
}

IDATA
destroySharedCache(J9JavaVM *vm, BOOLEAN useCommandLineValues)
{
	IDATA rc = PASS;
	I_32 i;
	PORT_ACCESS_FROM_JAVAVM(vm);

	for (i = 0; i < (NUM_CACHE + NUM_SNAPSHOT); i++) {
		UDATA cacheType = cacheInfoList[i].cacheType;
		IDATA expectedRc = (J9PORT_SHR_CACHE_TYPE_SNAPSHOT == cacheType) ? J9SH_DESTROYED_ALL_SNAPSHOT : J9SH_DESTROYED_ALL_CACHE;
		rc = j9shr_destroySharedCache(vm, cacheInfoList[i].cacheDir, cacheInfoList[i].name, (U_32)cacheType, useCommandLineValues);
		if (expectedRc != rc) {
			j9tty_printf(PORTLIB, "j9shr_destroySharedCache failed to destroy all caches of type: %d, rc: %d\n", cacheType, rc);
			rc = FAIL;
			return rc;
		}
	}
	rc = PASS;

	return rc;
}

IDATA
runTestCycle(J9JavaVM *vm, J9SharedClassPreinitConfig *piconfig, J9SharedClassConfig *sharedClassConfig, BOOLEAN createCache, char *cacheDir, BOOLEAN useCommandLineValues)
{
	IDATA rc = PASS;
	IDATA i;
	I_32 oldCacheCount;
	PORT_ACCESS_FROM_JAVAVM(vm);

	j9tty_printf(PORTLIB, "Test cycle started. createCache: %d\tcacheDir: %s\n", createCache, cacheDir);

	memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig));
	sharedClassConfig->ctrlDirName = cacheDir;

	testCacheCount = 0;
	cacheCount = 0;
	j9tty_printf(PORTLIB, "Existing cache information:\n");
	rc = iterateSharedCache(vm, cacheDir, 0, 0, countSharedCacheCallback);
	if (rc != PASS) {
		j9tty_printf(PORTLIB, "testSharedCacheAPI: failed to iterate existing shared caches \n");
		rc = FAIL;
		goto exit;
	}
	if (0 == cacheCount) {
		j9tty_printf(PORTLIB, "None found\n");
	}
	oldCacheCount = cacheCount;

	if ((FALSE == createCache) && (0 != testCacheCount)) {
		j9tty_printf(PORTLIB, "WARNING!!! cache with same name as used by test case found, testCacheCount: %d\n", testCacheCount);
	}

	if (createCache == TRUE) {
		rc = createTestCache(vm, piconfig, sharedClassConfig);
		if (rc != PASS) {
			goto exit;
		}
	}

	cacheCount = 0;
	rc = iterateSharedCache(vm, cacheDir, 0, useCommandLineValues, validateSharedCacheCallback);

	if (createCache == TRUE) {
		if((rc != PASS) || ((cacheCount - oldCacheCount) != (NUM_CACHE + NUM_SNAPSHOT))) {
			j9tty_printf(PORTLIB, "testSharedCacheAPI: failed to iterate newly created shared caches\texpected: %d\tfound: %d\n", cacheCount, (oldCacheCount+NUM_CACHE));
			rc = FAIL;
			goto exit;
		}
		for (i = 0; i < (NUM_CACHE + NUM_SNAPSHOT); i++) {
			if (cacheInfoList[i].found != TRUE) {
				j9tty_printf(PORTLIB, "testSharedCacheAPI: failed to iterate newly created shared caches; could not find cache with name %s (index: %d)\n", cacheInfoList[i].name, i);
				rc = FAIL;
				goto exit;
			}
		}
	}

	if (createCache == FALSE) {
		if (cacheCount != oldCacheCount) {
			j9tty_printf(PORTLIB, "testSharedCacheAPI: no new cache should exist. oldCacheCount = %d\tnewCacheCount = %d \n", oldCacheCount, cacheCount);
			rc = FAIL;
			goto exit;
		}
	}

	UnitTest::unitTest = UnitTest::SHAREDCACHE_API_TEST;

	rc = destroySharedCache(vm, useCommandLineValues);
	if ((createCache == TRUE) && (rc != PASS)) {
		j9tty_printf(PORTLIB, "testSharedCacheAPI: failed to destroy Shared Cache \n");
		goto exit;
	}
	if ((createCache == FALSE) && (rc != PASS)) {
		j9tty_printf(PORTLIB, "testSharedCacheAPI: there should not be any cache to destroy \n");
		goto exit;
	}

exit:
	UnitTest::unitTest = UnitTest::NO_TEST;
	return rc;
}

IDATA
testSharedCacheAPI(J9JavaVM* vm)
{
	J9SharedClassPreinitConfig *piconfig = NULL;
	J9SharedClassPreinitConfig *tempPiconfig = NULL;
	J9SharedClassConfig *sharedClassConfig = NULL;
	J9SharedClassConfig *tempSharedClassConfig = NULL;
	IDATA rc = PASS;
	char *cacheDir = NULL;
	const char *cacheName = "sharedcacheapicache";
	PORT_ACCESS_FROM_JAVAVM(vm);

	REPORT_START("SharedCacheAPI");

	/* vm->sharedClassPreinitConfig and vm->sharedClassConfig needs to be modified to create new test cache.
	 * Store current in temporary location.
	 */
	tempPiconfig = vm->sharedClassPreinitConfig;
	tempSharedClassConfig = vm->sharedClassConfig;

	cacheDir = (char *) j9mem_allocate_memory(J9SH_MAXPATH, J9MEM_CATEGORY_CLASSES);
	if (cacheDir == NULL) {
		j9tty_printf(PORTLIB, "testSharedCacheAPI: failed to allocate memory for cache directory \n");
		rc = FAIL;
		goto cleanup;
	}
	getCacheDir(vm, cacheDir);

	piconfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == piconfig) {
		j9tty_printf(PORTLIB, "testSharedCacheAPI: failed to allocate memory for J9SharedClassPreinitConfig\n");
		rc = FAIL;
		goto cleanup;
	}

	sharedClassConfig = (J9SharedClassConfig *) j9mem_allocate_memory(sizeof(J9SharedClassConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == sharedClassConfig) {
		j9tty_printf(PORTLIB, "testSharedCacheAPI: failed to allocate memory for J9SharedClassConfig\n");
		rc = FAIL;
		goto cleanup;
	}

	memset(piconfig, 0, sizeof(J9SharedClassPreinitConfig));
	memset(sharedClassConfig, 0, sizeof(J9SharedClassConfig));

	/* cacheName is not NULL, cacheDir is NULL. */
	populateCacheInfoList(vm, sharedClassConfig, cacheName, NULL);
	/* Do not create cache. Do not use command line values. */
	rc = runTestCycle(vm, piconfig, sharedClassConfig, FALSE, NULL, FALSE);
	if (rc != PASS) {
		j9tty_printf(PORTLIB, "runTestCycle: test1 failed\n");
		goto cleanup;
	}

	/* cacheName is not NULL, cacheDir is not NULL. */
	populateCacheInfoList(vm, sharedClassConfig, cacheName, cacheDir);
	/* Do not create cache. Do not use command line values. */
	rc = runTestCycle(vm, piconfig, sharedClassConfig, FALSE, NULL, FALSE);
	if (rc != PASS) {
		j9tty_printf(PORTLIB, "runTestCycle: test2 failed\n");
		goto cleanup;
	}

	/* cacheName is not NULL, cacheDir is NULL. */
	populateCacheInfoList(vm, sharedClassConfig, cacheName, NULL);
	/* Create cache. Do not use command line values. */
	rc = runTestCycle(vm, piconfig, sharedClassConfig, TRUE, NULL, FALSE);
	if (rc != PASS) {
		j9tty_printf(PORTLIB, "runTestCycle: test3 failed\n");
		goto cleanup;
	}

	/* cacheName is not NULL, cacheDir is not NULL. */
	populateCacheInfoList(vm, sharedClassConfig, cacheName, cacheDir);
	/* Create cache. Do not use command line values. */
	rc = runTestCycle(vm, piconfig, sharedClassConfig, TRUE, cacheDir, FALSE);
	if (rc != PASS) {
		j9tty_printf(PORTLIB, "runTestCycle: test4 failed\n");
		goto cleanup;
	}

cleanup:

	/* restore vm->sharedClassPreinitConfig and vm->sharedClassConfig */
	vm->sharedClassPreinitConfig = tempPiconfig;
	vm->sharedClassConfig = tempSharedClassConfig;

	if (sharedClassConfig != NULL) {
		j9mem_free_memory(sharedClassConfig);
	}
	if (piconfig != NULL) {
		j9mem_free_memory(piconfig);
	}
	if (cacheDir != NULL) {
		j9mem_free_memory(cacheDir);
	}

	REPORT_SUMMARY("SharedCacheAPI", rc);
	return rc;
}

} /* extern "C" */
