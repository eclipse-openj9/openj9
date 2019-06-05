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
#include "OSCacheTestMmap.hpp"

extern "C" {
#include "j9port.h"
}

#include "main.h"
#include "OSCachemmap.hpp"
#include "ProcessHelper.h"
#include "j2sever.h"
#include <string.h>

extern "C" {
void setCurrentCacheVersion(J9JavaVM *vm, UDATA j2seVersion, J9PortShcVersion *versionData);
}

J9VMThread *SH_OSCacheTestMmap::currentThread = NULL;

/**
 * Basic test to create a persistent cache, verify we can connect to it and
 * successfully read the contents.
 */
IDATA
SH_OSCacheTestMmap::testBasic(J9PortLibrary *portLibrary, J9JavaVM *vm)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc = PASS;
	J9SharedClassPreinitConfig *piconfig = NULL;
	Init *init = NULL;
	SH_OSCache *osc = NULL;
	char cacheDir[J9SH_MAXPATH];
	UDATA cacheSize;
	char *q;
	J9PortShcVersion versionData;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /* defined(OPENJ9_BUILD) */
	
	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;
	
	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		rc = FAIL;
		goto cleanup;
	}

	piconfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == piconfig) {
		j9tty_printf(PORTLIB, "testBasic: failed to allocate memory for J9SharedClassPreinitConfig\n");
		rc = FAIL;
		goto cleanup;
	}

	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassCacheSize = CACHE_SIZE;
	
	init = new(j9mem_allocate_memory(sizeof(Init), J9MEM_CATEGORY_CLASSES)) Init();
	if (NULL == init) {
		j9tty_printf(PORTLIB, "testBasic: failed to allocate memory for Init\n");
		rc = FAIL;
		goto cleanup;
	}
	
	osc = new(j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES)) SH_OSCachemmap(PORTLIB, vm, cacheDir, CACHE_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, init);
	if (NULL == osc) {
		j9tty_printf(PORTLIB, "testBasic: failed to initialize cache\n");
		rc = FAIL;
		goto cleanup;
	}
	
	if (NULL == (q = (char *)osc->attach(SH_OSCacheTestMmap::currentThread, &versionData))) {
		j9tty_printf(PORTLIB, "testBasic: failed to attach to cache\n");
		rc = FAIL;
		goto cleanup;
	}
	
	cacheSize = osc->getDataSize();
	if (cacheSize > 5) {
		j9tty_printf(PORTLIB, "testBasic: block accessed of length %d\n", cacheSize);
		if (memcmp(q, "abcde", 5) == 0) {
			j9tty_printf(PORTLIB, "testBasic: contents correct\n");
		} else {
			j9tty_printf(PORTLIB, "testBasic: contents incorrect, should be 'abcde': ");
			for (int i = 0; i < 10; i++) {
				j9tty_printf(PORTLIB, "%c", q[i]);	
			}
			j9tty_printf(PORTLIB, "\n");
			rc = FAIL;
		}
	} else {
		j9tty_printf(PORTLIB,"testBasic: block length incorrectly accessed - should be 5\n");
		rc = FAIL;
	}

cleanup:
	if(NULL != osc) {
		osc->destroy(false);
		j9mem_free_memory(osc);
	}
	j9mem_free_memory(piconfig);
	j9mem_free_memory(init);
	
	j9tty_printf(PORTLIB, "testBasic: %s", rc==PASS?"PASS\n":"FAIL\n");	
	
	return rc;
}

#define OSCACHETEST_CONSTRUCTOR_NAME "testConstructor"
/* This test exercises the OSCache constructor */
IDATA 
SH_OSCacheTestMmap::testConstructor(J9PortLibrary *portLibrary, J9JavaVM *vm)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SH_OSCachemmap *osc = NULL;
	char cacheDir[J9SH_MAXPATH];
	IDATA rc = PASS;
	J9SharedClassPreinitConfig *piconfig = NULL;
	J9PortShcVersion versionData;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /* defined(OPENJ9_BUILD) */
	
	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;

	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		rc = FAIL;
		goto cleanup;
	}

	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testConstructor: memory allocation failed \n");
		rc = FAIL;
		goto cleanup;
	}

	piconfig->sharedClassDebugAreaBytes = -1;
	osc = (SH_OSCachemmap *) j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
	if(NULL == osc) {
		j9tty_printf(PORTLIB, "testConstructor: memory allocation failed \n");		
		rc = FAIL;
		goto cleanup;
	}

	/* First, we try to allocate SH_OSCache with 0 size */
	piconfig->sharedClassCacheSize = 0;
	
	osc = new(osc) SH_OSCachemmap(PORTLIB, vm, cacheDir, OSCACHETEST_CONSTRUCTOR_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);

	if(osc->getError() >= 0) {
		j9tty_printf(PORTLIB, "testConstructor: setting size to 0 does not fail the constructor \n");
		rc = FAIL;
	}
	osc->destroy(false);
	
	/* Try to create a cache that is too small */
	piconfig->sharedClassCacheSize = 2;
	
	osc = new(osc) SH_OSCachemmap(PORTLIB, vm, cacheDir, OSCACHETEST_CONSTRUCTOR_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);

	if(osc->getError() >= 0) {
		j9tty_printf(PORTLIB, "testConstructor: allocate small area does not fail the constructor \n");
		rc = FAIL;
	}
	osc->destroy(false);

cleanup:
	j9mem_free_memory(osc);
	j9mem_free_memory(piconfig);
	
	j9tty_printf(PORTLIB, "testConstructor: %s\n", PASS==rc?"PASS":"FAIL");
	return rc;
}

/**
 * This test deliberately causes the constructor to fail, then tests that none
 * of the other public methods work or crash afterwards.
 */
IDATA 
SH_OSCacheTestMmap::testFailedConstructor(J9PortLibrary *portLibrary, J9JavaVM *vm)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SH_OSCachemmap *osc = NULL;
	char cacheDir[J9SH_MAXPATH];
	IDATA rc = PASS;
	J9SharedClassPreinitConfig *piconfig;
	J9PortShcVersion versionData;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /* defined(OPENJ9_BUILD) */

	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;

	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testFailedConstructor: J9SharedClassPreinitConfig memory allocation failed \n");
		return FAIL;
	}

	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		rc = FAIL;
		goto cleanup;
	}

	piconfig->sharedClassDebugAreaBytes = -1;

	osc = (SH_OSCachemmap *) j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
	if(NULL == osc) {
		j9tty_printf(PORTLIB, "testFailedConstructor: SH_OSCachemmap memory allocation failed \n");		
		rc = FAIL;
		goto cleanup;
	}

	/* First, we try to create a cache with 0 size - this should fail */
	piconfig->sharedClassCacheSize = 0;
	osc = new(osc) SH_OSCachemmap(PORTLIB, vm, cacheDir, OSCACHETEST_CONSTRUCTOR_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);

	if(osc->getError() >= 0) {
		j9tty_printf(PORTLIB, "testFailedConstructor: failed: setting size to 0 should have failed the constructor\n");
		rc = FAIL;
		goto cleanup;
	}

	/* Now call the public methods in the class, we should get failed results */
	if(osc->attach(SH_OSCacheTestMmap::currentThread, &versionData) != NULL) {
		j9tty_printf(PORTLIB, "testFailedConstructor: failed constructor but attach did not return NULL");
		rc = FAIL;
	}
	
	if(osc->acquireWriteLock(1) != -1) {
		j9tty_printf(PORTLIB, "testFailedConstructor: failed constructor but acquireWriteLock did not return NULL");
		rc = FAIL;
	}

	if(osc->releaseWriteLock(1) != -1) {
		j9tty_printf(PORTLIB, "testFailedConstructor: failed constructor but releaseWriteLock did not return NULL");
		rc = FAIL;
	}

cleanup:
	if(osc != NULL) {
		osc->destroy(false);
		j9mem_free_memory(osc);
	}
	j9mem_free_memory(piconfig);

	j9tty_printf(PORTLIB, "testFailedConstructor: %s\n", PASS==rc?"PASS":"FAIL");
	return rc;	
}

#if defined(J9SHR_CACHELET_SUPPORT) || defined (J9VM_GC_REALTIME)
	/* rtjavm7 does not seem to like testing with processes. Without doing a
	 * terrible amount of investigation lowering this to 3 appears to fix the problem
	 */
	#define MAX_PROCESS 3
#else
	#define MAX_PROCESS 5
#endif
#define LAUNCH_CONTROL_SEMAPHORE "testMultipleCreate"
#define OSCACHE_AREA_NAME "OSCTest1"
/**
 * This test starts multiple child OSCaches, each of which tries to create a
 * cache with the same name. Obviously, one should succeed (the rest connect
 * to the cache that the first one created). Each child tests it can read from
 * the cache.
 */
IDATA
SH_OSCacheTestMmap::testMultipleCreate(J9PortLibrary* portLibrary, J9JavaVM *vm, struct j9cmdlineOptions *arg, UDATA child)
{
	J9ProcessHandle pid[MAX_PROCESS];
	IDATA rc = PASS;
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shsem_handle *launchsem;
	SH_OSCachemmap *oscHandle;
	char cacheDir[J9SH_MAXPATH];
	Init *init = NULL;
	J9SharedClassPreinitConfig *piconfig;
	IDATA semhandle;
	J9PortShcVersion versionData;
	int argc = arg->argc;
	char **argv = arg->argv;
	char * childargv[SHRTEST_MAX_CMD_OPTS];
	UDATA childargc = 0;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /* defined(OPENJ9_BUILD) */
	
	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;

	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		return FAIL;
	}

	/* Jazz 76224: piconfig is memsetted to zero before assigning particular values */
	memset(piconfig, 0, sizeof(*piconfig));

	piconfig->sharedClassDebugAreaBytes = -1;
	piconfig->sharedClassCacheSize = SHM_REGIONSIZE;
	
	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		return FAIL;
	}

	init = new(j9mem_allocate_memory(sizeof(Init), J9MEM_CATEGORY_CLASSES)) Init();
	if (NULL == init) {
		j9tty_printf(PORTLIB, "testMultipleCreate: failed to allocate memory for Init\n");
		return FAIL;
	}

	semhandle = openLaunchSemaphore(PORTLIB, LAUNCH_CONTROL_SEMAPHORE, MAX_PROCESS);
	if(-1 == semhandle) {
		return FAIL;
	}
	
	if(!child) {
		/* parent */
		
		if(-1 == semhandle) {
			j9tty_printf(PORTLIB, "testMultipleCreate: Cannot open launch control semaphores\n");
			return FAIL;
		}

		/*Ensure there is no cache before we start*/
		oscHandle = new(j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES)) SH_OSCachemmap(PORTLIB, vm, cacheDir, OSCACHE_AREA_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);
		/* Jazz 76224: delete the cache file "OSCTest1" unconditionally unless it doesn't exist at specified directory */
		if (oscHandle->getError() != J9SH_OSCACHE_FAILURE) {
			oscHandle->destroy(false);
		}

		childargc = buildChildCmdlineOption(argc, argv,(OSCACHETEST_CMDLINE_STARTSWITH OSCACHETESTMMAP_CMDLINE_MULTIPLECREATE), childargv);
		for(UDATA i = 0; i < MAX_PROCESS; i++) {
			pid[i] = LaunchChildProcess(PORTLIB, "testMultipleCreate", childargv, childargc);
			if(NULL == pid[i]) {
				j9tty_printf(PORTLIB, "testMultipleCreate: Failed to launch child process\n");
				j9shsem_deprecated_destroy(&launchsem);
				return FAIL;
			}
		}

		/* Let all the children start */
		ReleaseLaunchSemaphore(PORTLIB, semhandle, MAX_PROCESS);
		/* For reason why we have to sleep, see porttest/shmem.c, j9shmem_test9 */
		SleepFor(2);
		
		for(UDATA i = 0; i < MAX_PROCESS; i++) {
			if(-1 == WaitForTestProcess(PORTLIB, pid[i])) {
				j9tty_printf(PORTLIB, "testMultipleCreate: child process %d reported failure: failed\n", i);
				rc = FAIL;
			}
		}
		
	} else {
		char *q;
		UDATA cacheSize;

		/* child */
		if(-1 == semhandle) {
			j9tty_printf(PORTLIB, "testMultipleCreate: Cannot open launch control semaphores\n");
			return -1;
		}
		
		WaitForLaunchSemaphore(PORTLIB, semhandle);
		CloseLaunchSemaphore(PORTLIB, semhandle);

		oscHandle = new(j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES)) SH_OSCachemmap(PORTLIB, vm, cacheDir, OSCACHE_AREA_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, init);
		
		if(oscHandle->getError() < 0) {
			return -1;
		}
		
		if(NULL == (q = (char *)oscHandle->attach(SH_OSCacheTestMmap::currentThread, &versionData))) {
			return -1;
		}
		
		cacheSize = oscHandle->getDataSize();
		if (cacheSize > 5) {
			if (memcmp(q, "abcde", 5) != 0) {
				j9tty_printf(PORTLIB, "testMultipleCreate: contents incorrect, should be 'abcde': ");
				for (int i = 0; i < 10; i++) {
					j9tty_printf(PORTLIB, "%c", q[i]);	
				}
				j9tty_printf(PORTLIB, "\n");
				return -1;
			}
		} else {
			j9tty_printf(PORTLIB,"testtestMultipleCreate: block length incorrectly accessed - should be 5\n");
			return -1;
		}

		return 0;
	}
	
	oscHandle = new(j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES)) SH_OSCachemmap(PORTLIB, vm, cacheDir, OSCACHE_AREA_NAME, piconfig, 1, J9SH_OSCACHE_OPEXIST_DESTROY, 1, 0, 0, &versionData, NULL);
	oscHandle->destroy(false);
	CloseLaunchSemaphore(PORTLIB, semhandle);

	j9tty_printf(PORTLIB, "testMultipleCreate: %s\n", PASS==rc?"PASS":"FAIL");

	j9mem_free_memory(piconfig);
	return rc;
}

IDATA
SH_OSCacheTestMmap::testGetAllCacheStatistics(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
#define NUMBER_OF_CACHES_TO_TEST 3
	const char *cacheName[] = {"abc", "def", "ghi"};
	char* ctrlDirName = NULL;
	char cacheDirName[J9SH_MAXPATH];
	SH_OSCachemmap *osc[NUMBER_OF_CACHES_TO_TEST] = {NULL, NULL, NULL};
	J9Pool *cacheStat = NULL;
	IDATA rc = FAIL;
	IDATA ret = 0;
	J9SharedClassPreinitConfig *piconfig;
	UDATA numCacheBeforeTest = 0;
	J9PortShcVersion versionData;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;

	if(NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testGetAllCacheStatistics: J9SharedClassPreinitConfig memory allocation failed \n");
		goto cleanup;
	}
	piconfig->sharedClassDebugAreaBytes = -1;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /*  defined(OPENJ9_BUILD) */

	ret = j9shmem_getDir(ctrlDirName, flags, cacheDirName, J9SH_MAXPATH);
	if (0 > ret) {
		j9tty_printf(PORTLIB, "testGetAllCacheStatistics: j9shmem_getDir() failed \n");
		goto cleanup;
	}

	/* Before starting existing caches must be cleaned up in case previous test runs failed to clean up (CMVC 168463).
	 * If any step fails the loop continues in order to 'get as much clean up as possible'.
	 */
	for(UDATA i = 0; i < NUMBER_OF_CACHES_TO_TEST; i++) {
		SH_OSCachemmap * tmposc = NULL;

		piconfig->sharedClassCacheSize = 10240;

		tmposc = (SH_OSCachemmap *)j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
		if(NULL == tmposc) {
			j9tty_printf(PORTLIB, "testGetAllCacheStatistics: clean up old caches : failed to allocate memory for cache %zu\n", i);
			goto loop_cleanup;
		}

		tmposc = new(tmposc) SH_OSCachemmap(PORTLIB, vm, cacheDirName, cacheName[i], piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);
		if(tmposc->getError() < 0) {
			j9tty_printf(PORTLIB, "testGetAllCacheStatistics: clean up old caches : failed to create create cache %zu (getError() =%zd)\n", i, tmposc->getError());
			goto loop_cleanup;
		}

	loop_cleanup:
		if (tmposc != NULL) {
			tmposc->destroy(false);
			tmposc->cleanup();
			j9mem_free_memory(tmposc);
		}
	}

	cacheStat = SH_OSCache::getAllCacheStatistics(vm, ctrlDirName, 0, 1, J2SE_CURRENT_VERSION, false, false, SHR_STATS_REASON_TEST, true);

	if(cacheStat != NULL) {
		numCacheBeforeTest += pool_numElements(cacheStat);
		pool_kill(cacheStat);
		cacheStat = NULL;
	}
	
	for(UDATA i = 0; i < NUMBER_OF_CACHES_TO_TEST; i++) {
		piconfig->sharedClassCacheSize = 10240;
		osc[i] = (SH_OSCachemmap *)j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
		if(NULL == osc[i]) {
			j9tty_printf(PORTLIB, "testGetAllCacheStatistics: failed to allocate memory for cache %zu\n", i);
			goto cleanup;
		}
		osc[i] = new(osc[i]) SH_OSCachemmap(PORTLIB, vm, cacheDirName, cacheName[i], piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);
		if(osc[i]->getError() < 0) {
			j9tty_printf(PORTLIB, "testGetAllCacheStatistics: failed to create create cache %zu (getError() =%zd)\n", i, osc[i]->getError());
			goto cleanup;
		}
	}

	cacheStat = SH_OSCache::getAllCacheStatistics(vm, ctrlDirName, 0, 1, J2SE_CURRENT_VERSION, false, false, SHR_STATS_REASON_TEST, true);
	if(cacheStat == NULL) {
		j9tty_printf(PORTLIB, "testGetAllCacheStatistics: failed to get cache statistics - there should be at least one cache!\n");
		goto cleanup;
	}

	if(pool_numElements(cacheStat) != (NUMBER_OF_CACHES_TO_TEST + numCacheBeforeTest)) {
		j9tty_printf(PORTLIB, "testGetAllCacheStatistics: failed: number of elements expected = %zu, result =%zu\n", NUMBER_OF_CACHES_TO_TEST+numCacheBeforeTest, pool_numElements(cacheStat));
		goto cleanup; 
	}
	
	rc = PASS;

cleanup:
	if(cacheStat != NULL) {
		pool_kill(cacheStat);
	}

	for(UDATA i = 0; i < NUMBER_OF_CACHES_TO_TEST; i++) {
		if(osc[i] != NULL) {
			osc[i]->destroy(false);
			osc[i]->cleanup();
			j9mem_free_memory(osc[i]);
		}
	}

	j9mem_free_memory(piconfig);

	j9tty_printf(PORTLIB, "testGetAllCacheStatistics: %s\n", rc==PASS?"PASS":"FAIL");	

	return rc;
}

#define OSCACHE_TEST_MUTEX_NAME "testMutex"
IDATA 
SH_OSCacheTestMmap::testMutex(J9PortLibrary *portLibrary, J9JavaVM *vm, struct j9cmdlineOptions *arg, UDATA child)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SH_OSCachemmap *osc;
	char cacheDir[J9SH_MAXPATH];
	J9SharedClassPreinitConfig* piconfig;
	J9ProcessHandle pid;
	IDATA rc;
	J9PortShcVersion versionData;
	int argc = arg->argc;
	char **argv = arg->argv;
	char * childargv[SHRTEST_MAX_CMD_OPTS];
	UDATA childargc = 0;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /* defined(OPENJ9_BUILD) */

	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;

	if (NULL == (osc = (SH_OSCachemmap *)j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testMutex: failed SH_OSCachemmap malloc\n");
		return FAIL;
	}
	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testMutex: failed J9SharedClassPreinitConfig malloc\n");
		return FAIL;
	}
	piconfig->sharedClassDebugAreaBytes = -1;

	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		return FAIL;
	}

	piconfig->sharedClassCacheSize = SHM_REGIONSIZE;
	osc = new(osc) SH_OSCachemmap(PORTLIB, vm, cacheDir, OSCACHE_TEST_MUTEX_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);
	if (osc->getError() < 0) {
		j9tty_printf(PORTLIB, "testMutex: cannot open OSCache\n");
		return FAIL;
	}
	
	if(child) {
		if (osc->acquireWriteLock(1) == -1) {
			j9tty_printf(PORTLIB, "testMutex: child process failed to acquire the lock\n");
			return FAIL;
		}
		SleepFor(5);
		if (osc->releaseWriteLock(1) == -1) {
			j9tty_printf(PORTLIB, "testMutex: child process failed to release the lock\n");
			return FAIL;
		}
		return PASS;
	}

	/* Parent */
	childargc = buildChildCmdlineOption(argc, argv, (OSCACHETEST_CMDLINE_STARTSWITH OSCACHETESTMMAP_CMDLINE_MUTEX), childargv);
	pid = LaunchChildProcess(PORTLIB, "testMutex", childargv, childargc);
	if(NULL == pid) {
		j9tty_printf(PORTLIB, "testMutex: failed to launch child process\n");
		return FAIL;
	}

	if (osc->acquireWriteLock(1) == -1) {
		j9tty_printf(PORTLIB, "testMutex: parent process failed to acquire the lock\n");
		return FAIL;
	}
	SleepFor(1);
	if (osc->releaseWriteLock(1) == -1) {
		j9tty_printf(PORTLIB, "testMutex: parent process failed to release the lock\n");
		return FAIL;
	}
	
	rc = WaitForTestProcess(PORTLIB, pid);

	osc->destroy(false);

	j9mem_free_memory(piconfig);

	j9tty_printf(PORTLIB, "testMutex: %s\n", PASS==rc?"PASS":"FAIL");
	
	return rc;
}

/**
 * This test launches a child OSCache, which exits whilst holding the write lock. The
 * master then attempts to acquire the write lock. It should succeed, because JVMs are
 * supposed to release the write lock if they crash.
 */
#define OSCACHETEST_MUTEX_HANG_NAME "testMutexHang"
IDATA
SH_OSCacheTestMmap::testMutexHang(J9PortLibrary *portLibrary, J9JavaVM *vm, struct j9cmdlineOptions *arg, UDATA child)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SH_OSCachemmap *osc = NULL;
	char cacheDir[J9SH_MAXPATH];
	J9ProcessHandle pid;
	IDATA rc = PASS;
	J9SharedClassPreinitConfig *piconfig;
	J9PortShcVersion versionData;
	int argc = arg->argc;
	char **argv = arg->argv;
	char * childargv[SHRTEST_MAX_CMD_OPTS];
	UDATA childargc = 0;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /* defined(OPENJ9_BUILD) */
	
	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;

	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testMutexHang: cannot allocate J9SharedClassPreinitConfig memory");
		rc = FAIL;
		goto cleanup;
	}
	piconfig->sharedClassDebugAreaBytes = -1;
	
	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		return FAIL;
	}

	osc = (SH_OSCachemmap *)j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
	if(osc == NULL) {
		j9tty_printf(PORTLIB, "testMutexHang: cannot allocate SH_OSCachemmap memory");
		rc = FAIL;
		goto cleanup;
	}
	
	piconfig->sharedClassCacheSize = 10240;
	osc = new(osc) SH_OSCachemmap(PORTLIB, vm, cacheDir, OSCACHETEST_MUTEX_HANG_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);
	
	if(osc->getError() < 0) {
		j9tty_printf(PORTLIB, "testMutexHang: cannot open oscache area");
		rc = FAIL;
		goto cleanup;
	}
	
	if(child) {
		osc->acquireWriteLock(1);
		/* "Stop" abruptly, holding the write lock */
		return PASS;
	}
	
	/* Parent */
	childargc = buildChildCmdlineOption(argc, argv, (OSCACHETEST_CMDLINE_STARTSWITH OSCACHETESTMMAP_CMDLINE_MUTEXHANG), childargv);
	pid = LaunchChildProcess(PORTLIB, "testMutexHang", childargv, childargc);
	if(NULL == pid) {
		j9tty_printf(PORTLIB, "testMutexHang: Failed to launch child process\n");
		rc = FAIL;
		goto cleanup;
	}
	
	/* We don't care about the rc from child... */
	WaitForTestProcess(PORTLIB, pid);
	
	j9tty_printf(PORTLIB, "If test hangs at this point, testMutexHang failed\n");
	osc->acquireWriteLock(1);
	
	rc = PASS;		

cleanup:
	if(osc != NULL) {
		osc->destroy(false);
		j9mem_free_memory(osc);
	}
	
	j9mem_free_memory(piconfig);
	
	j9tty_printf(PORTLIB, "testMutexHang: %s\n", rc==PASS?"PASS":"FAIL");	
	
	return rc;
}

#define OSCACHETEST_DESTROY_NAME "testDestroy"
/**
 * This test creates a cache and connects to it, then launches a child process. The child also connects to
 * the cache, then tries to destroy it. Once the child has exited, the parent tries to get the write lock.
 * This should succeed, since the child should have failed to destroy the cache.
 */
IDATA
SH_OSCacheTestMmap::testDestroy (J9PortLibrary* portLibrary, J9JavaVM *vm, struct j9cmdlineOptions *arg, UDATA child)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SH_OSCachemmap *osc = NULL;
	char cacheDir[J9SH_MAXPATH];
	J9ProcessHandle pid;
	IDATA rc = FAIL;
	IDATA ret = 0;
	J9SharedClassPreinitConfig *piconfig;
	J9PortShcVersion versionData;
	int argc = arg->argc;
	char **argv = arg->argv;
	char * childargv[SHRTEST_MAX_CMD_OPTS];
	UDATA childargc = 0;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /* defined(OPENJ9_BUILD) */

	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_PERSISTENT;

	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testDestroy cannot allocate J9SharedClassPreinitConfig memory\n");
		goto cleanup;
	}
	piconfig->sharedClassDebugAreaBytes = -1;

	ret = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (0 > ret) {
		j9tty_printf(PORTLIB, "testDestroy: j9shmem_getDir() failed \n");
		goto cleanup;
	}

	osc = (SH_OSCachemmap *)j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
	if(osc == NULL) {
		j9tty_printf(PORTLIB, "testDestroy cannot allocate SH_OSCachemmap memory\n");
		goto cleanup;
	}

	piconfig->sharedClassCacheSize = 10240;
	osc = new(osc) SH_OSCachemmap(PORTLIB, vm, cacheDir, OSCACHETEST_DESTROY_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);

	if(osc->getError() < 0) {
		j9tty_printf(PORTLIB, "testDestroy: cannot open oscache area\n");
		goto cleanup;
	}	

	if(NULL == osc->attach(SH_OSCacheTestMmap::currentThread, &versionData)) {
		j9tty_printf(PORTLIB, "testDestroy: cannot attach to cache\n");
		goto cleanup;
	}

	if(child) {
		/* For child we attempt to destroy the cache - since the parent is connected it should silently fail */
		osc->destroy(false);
		return PASS;
	}
	
	/* Parent */
	
	/* Start the child process, which calls destroy. We should still be able to get the write lock afterwards */
	childargc = buildChildCmdlineOption(argc, argv, (OSCACHETEST_CMDLINE_STARTSWITH OSCACHETESTMMAP_CMDLINE_DESTROY), childargv);
	pid = LaunchChildProcess(PORTLIB, "testDestroy", childargv, childargc);
	if(NULL == pid) {
		j9tty_printf(PORTLIB, "testDestroy: failed to launch child process\n");
		goto cleanup;
	}
		
	/* We don't care about rc from child... */
	WaitForTestProcess(PORTLIB, pid);

	if(osc->acquireWriteLock(1) == -1) {
		j9tty_printf(PORTLIB, "testDestroy: failed: unable to acquireWriteLock after calling destroy\n");
		goto cleanup;
	}
	
	rc = PASS;
	
cleanup:
	if(osc != NULL) {
		osc->destroy(false);
		j9mem_free_memory(osc);
	}

	j9mem_free_memory(piconfig);

	j9tty_printf(PORTLIB, "testDestroy: %s\n", PASS==rc?"PASS":"FAIL");
	return rc;
}

IDATA
SH_OSCacheTestMmap::runTests(J9JavaVM* vm, struct j9cmdlineOptions* arg, const char *cmdline)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA rc = PASS;
	
	SH_OSCacheTestMmap::currentThread = vm->internalVMFunctions->currentVMThread(vm);
	/* Detect children */
	if (NULL != cmdline) {
		/* We are using strstr, so we need to put the longer string first */
		if (NULL != strstr(cmdline, OSCACHETESTMMAP_CMDLINE_MULTIPLECREATE)) {
			return testMultipleCreate(PORTLIB, vm, arg, 1);
		} else if(NULL != strstr(cmdline, OSCACHETESTMMAP_CMDLINE_MUTEXHANG)) {
			return testMutexHang(PORTLIB, vm, arg, 1);
		} else if(NULL != strstr(cmdline, OSCACHETESTMMAP_CMDLINE_MUTEX)) {
			return testMutex(PORTLIB, vm, arg, 1);
		} else if(NULL != strstr(cmdline, OSCACHETESTMMAP_CMDLINE_DESTROY)) {
			return testDestroy(PORTLIB, vm, arg, 1);
		}
	}
	
	j9tty_printf(PORTLIB, "OSCacheTestMmap begin\n");
	
	j9tty_printf(PORTLIB, "testBasic begin\n");
	rc |= testBasic(PORTLIB, vm);
	
	j9tty_printf(PORTLIB, "testConstructor begin\n");
	rc |= testConstructor(PORTLIB, vm);
	
	j9tty_printf(PORTLIB, "testFailedConstructor begin\n");
	rc |= testFailedConstructor(PORTLIB, vm);

	j9tty_printf(PORTLIB, "testMultipleCreate begin\n");
	rc |= testMultipleCreate(PORTLIB, vm, arg, 0);
	
	j9tty_printf(PORTLIB, "testGetAllCacheStatistics begin\n");
	rc |= testGetAllCacheStatistics(vm);
	
	j9tty_printf(PORTLIB, "testMutex begin\n");
	rc |= testMutex(PORTLIB, vm, arg, 0);

	j9tty_printf(PORTLIB, "testMutexHang begin\n");
	rc |= testMutexHang(PORTLIB, vm, arg, 0);

	j9tty_printf(PORTLIB, "testDestroy begin\n");
	rc |= testDestroy(PORTLIB, vm, arg, 0);

	return rc;
}
