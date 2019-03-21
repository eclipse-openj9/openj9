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
#include "OSCacheTestSysv.hpp"

extern "C" {
#include "j9.h"
}

#include "main.h"
#include "OSCachesysv.hpp"
#include "OSCacheTest.hpp"
#include "ProcessHelper.h"
#include "j2sever.h"
#include <string.h>
#include "CompositeCacheImpl.hpp"
#include "CacheLifecycleManager.hpp"

#define _UTE_STATIC_
#include "ut_j9shr.h"

extern "C" {
void setCurrentCacheVersion(J9JavaVM *vm, UDATA j2seVersion, J9PortShcVersion *versionData);
}

J9VMThread *SH_OSCacheTestSysv::currentThread = NULL;

IDATA
SH_OSCacheTestSysv::testBasic(J9PortLibrary* portLibrary, J9JavaVM *vm)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	IDATA rc = PASS;
	SH_OSCachesysv *osc = NULL, *osc1 = NULL;
	Init *initObj = NULL;
	J9SharedClassPreinitConfig *piconfig = NULL;
	char *s, *r;
	J9PortShcVersion versionData;
	char cacheDir[J9SH_MAXPATH];
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /* defined(OPENJ9_BUILD) */
	
	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;

	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		rc = FAIL;
		goto cleanup;
	}

	piconfig = (J9SharedClassPreinitConfig *) j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES);
	if (NULL == piconfig) {
		rc = FAIL;
		goto cleanup;
	}

	piconfig->sharedClassCacheSize = SHM_REGIONSIZE;
	
	initObj = new(j9mem_allocate_memory(sizeof(Init), J9MEM_CATEGORY_CLASSES)) Init();
	if (NULL == initObj) {
		rc = FAIL;
		goto cleanup;
	}

	/* Testing 2 ways of creating OSCache: This is the way Composite cache creates OSCache object */
	osc = (SH_OSCachesysv *) j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
	if (NULL == osc) {
		rc = FAIL;
		goto cleanup;
	}
	/* Currently explicitly requests non-persistent cache */
	SH_OSCache::newInstance(PORTLIB, (SH_OSCache*)osc, CACHE_NAME, SH_OSCache::getCurrentCacheGen(), &versionData);
	if (!osc->startup(vm, cacheDir, J9SH_DIRPERM_ABSENT, CACHE_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 0, 0, 0, 0, &versionData, initObj, SHR_STARTUP_REASON_NORMAL)) {
		j9tty_printf(PORTLIB, "testBasic: cannot create new region");
		rc = FAIL;
		goto cleanup;
	}

	/* This is how Utility creating oscache object*/
	osc1 = new(j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES)) SH_OSCachesysv(PORTLIB, vm, cacheDir, CACHE_NAME1, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, initObj);
	if (NULL == osc1) {
		rc = FAIL;
		goto cleanup;
	}
	
	r = (char *) osc->attach(SH_OSCacheTestSysv::currentThread, &versionData);
	s = (char *) osc->attach(SH_OSCacheTestSysv::currentThread, &versionData);

	if (r == NULL || s == NULL) {
		j9tty_printf(PORTLIB, "testBasic: failed to attach\n");
		rc = FAIL;
	} else {
		char* q = (char*) osc->attach(SH_OSCacheTestSysv::currentThread, &versionData);
		if (q != NULL) {
			UDATA cacheSize = osc->getDataSize();
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
		} else {
			j9tty_printf(PORTLIB, "testBasic: failed to access block\n");
			rc = FAIL;
		}
	}
	
cleanup:
	if(NULL != osc) {
		osc->destroy(false);
	}
	if(NULL != osc1) {
		osc1->destroy(false);
	}
	j9mem_free_memory(piconfig);
	j9mem_free_memory(initObj);
	j9mem_free_memory(osc);
	j9mem_free_memory(osc1);

	j9tty_printf(PORTLIB, "testBasic: %s", rc==PASS?"PASS\n":"FAIL\n");	
	
	return rc;
}
#if defined(J9SHR_CACHELET_SUPPORT) || defined (J9VM_GC_REALTIME)
	/* rtjavm7 does not seem to like testing with 10 processes. Without doing a
	 * terrible amount of investigation lowering this to 3 appears to fix the problem
	 */
	#define MAX_PROCESS 3
#else
	#define MAX_PROCESS 10
#endif

#define LAUNCH_CONTROL_SEMAPHORE "testMultipleCreate"
#define OSCACHE_AREA_NAME "OSCTest1"
/**
 * This test starts multiple child OSCaches, each of which tries to create a
 * cache with the same name. Obviously, only one should succeed (the rest connect
 * to the cache that the first one created). The parent counts the number of children
 * which report they created the cache - if it is not 1 then the test has failed.
 */
IDATA
SH_OSCacheTestSysv::testMultipleCreate(J9PortLibrary* portLibrary, J9JavaVM *vm, struct j9cmdlineOptions *arg, UDATA child)
{
	J9ProcessHandle pid[MAX_PROCESS];
	IDATA rc = PASS;
	PORT_ACCESS_FROM_PORT(portLibrary);
	struct j9shsem_handle *launchsem;
	SH_OSCachesysv *oscHandle;
	char cacheDir[J9SH_MAXPATH];
	J9SharedClassPreinitConfig *piconfig;
	J9PortShcVersion versionData;
	int argc = arg->argc;
	char **argv = arg->argv;
	char * childargv[SHRTEST_MAX_CMD_OPTS];
	UDATA childargc = 0;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /*  defined(OPENJ9_BUILD) */
	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
	
	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		return FAIL;
	}

	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		return FAIL;
	}

	IDATA semhandle = openLaunchSemaphore(PORTLIB, LAUNCH_CONTROL_SEMAPHORE, MAX_PROCESS);
	if(-1 == semhandle) {
		return FAIL;
	}
	
	if(!child) {
		/* parent */
		UDATA opened = 0;
		UDATA created = 0; 

		if(-1 == semhandle) {
			j9tty_printf(PORTLIB, "testMultipleCreate: Cannot open launch control semaphores\n");
			return FAIL;
		}

		/*Ensure there is no cache before we start*/
		oscHandle = new(j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES)) SH_OSCachesysv(PORTLIB, vm, cacheDir, OSCACHE_AREA_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);
		if(oscHandle->getError() == J9SH_OSCACHE_OPENED) {
			oscHandle->destroy(false);
		}else if(oscHandle->getError() == J9SH_OSCACHE_CREATED) {
			oscHandle->destroy(false);
		}

		childargc = buildChildCmdlineOption(argc, argv, (OSCACHETEST_CMDLINE_STARTSWITH OSCACHETEST_CMDLINE_MULTIPLECREATE), childargv);
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
			IDATA procrc = WaitForTestProcess(PORTLIB, pid[i]);

			switch(procrc) {
			case 0:
				/* Cache opened*/
				opened++;
				break;					
			case 1:
				/* Cache created */
				created++;
				break;
			default:
				j9tty_printf(PORTLIB, "testMultipleCreate: Unknown return value from WaitForTestProcess(): failed (procrc = %p)\n",procrc);
				rc = FAIL;
				break;
			}
		}

		switch(created) {
		case 0:
			j9tty_printf(PORTLIB, "testMultipleCreate: None of the children created the OSCache: failed (opened %u)\n",opened);
			rc = FAIL;
			break;
		case 1:
			/* One child created the cache - success */
			break;
		default:
			j9tty_printf(PORTLIB, "testMultipleCreate: more then 1 child created the OSCache.\n");
			rc = FAIL;
			break;
		}

	} else {
		/* child */
		if(-1 == semhandle) {
			j9tty_printf(PORTLIB, "testMultipleCreate: Cannot open launch control semaphores\n");
			return 2;
		}
		
		WaitForLaunchSemaphore(PORTLIB, semhandle);
		CloseLaunchSemaphore(PORTLIB, semhandle);

		piconfig->sharedClassCacheSize = SHM_REGIONSIZE;
		oscHandle = new(j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES)) SH_OSCachesysv(PORTLIB, vm, cacheDir, OSCACHE_AREA_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);
		
		if(oscHandle->getError() == J9SH_OSCACHE_OPENED) {
			rc = 0;
		} else if(oscHandle->getError() == J9SH_OSCACHE_CREATED) {
			rc = 1;
		} else {
			rc = 2;
		}
		
		if(oscHandle->attach(SH_OSCacheTestSysv::currentThread, &versionData) == NULL) {
			rc = 2;
		}
		return rc;
	}
	
	oscHandle = new(j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES)) SH_OSCachesysv(PORTLIB, vm, cacheDir, OSCACHE_AREA_NAME, piconfig, 1, J9SH_OSCACHE_OPEXIST_DESTROY, 1, 0, 0, &versionData, NULL);
	oscHandle->destroy(false);
	CloseLaunchSemaphore(PORTLIB, semhandle);

	j9tty_printf(PORTLIB, "testMultipleCreate: %s\n", PASS==rc?"PASS":"FAIL");

	j9mem_free_memory(piconfig);
	return rc;
}

#define OSCACHETEST_CONSTRUCTOR_NAME "testConstructor"
/* This test exercises the OSCache constructor */
IDATA 
SH_OSCacheTestSysv::testConstructor(J9PortLibrary *portLibrary, J9JavaVM *vm)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SH_OSCachesysv *osc = NULL;
	char cacheDir[J9SH_MAXPATH];
	IDATA rc = PASS;
	J9SharedClassPreinitConfig *piconfig;
	J9PortShcVersion versionData;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /*  defined(OPENJ9_BUILD) */
	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;

	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testConstructor: memory allocation failed \n");		
		rc = FAIL;
		goto cleanup;
	}

	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		rc = FAIL;
		goto cleanup;
	}

	osc = (SH_OSCachesysv *) j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
	if(NULL == osc) {
		j9tty_printf(PORTLIB, "testConstructor: memory allocation failed \n");		
		rc = FAIL;
		goto cleanup;
	}

	/* First, we try to allocate SH_OSCache with 0 size */
	piconfig->sharedClassCacheSize = 0;
	
	osc = new(osc) SH_OSCachesysv(PORTLIB, vm, cacheDir, OSCACHETEST_CONSTRUCTOR_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);

	if(osc->getError() >= 0) {
		j9tty_printf(PORTLIB, "testConstructor: setting size to 0 does not fail the constructor \n");
		rc = FAIL;
	}

	osc->destroy(false);

	/* Try to create a cache that is too small */
	piconfig->sharedClassCacheSize = 2;
	
	osc = new(osc) SH_OSCachesysv(PORTLIB, vm, cacheDir, OSCACHETEST_CONSTRUCTOR_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);

	if(osc->getError() >= 0) {
		j9tty_printf(PORTLIB, "testConstructor: allocate small area does not fail the constructor \n");
		rc = FAIL;
	}
	
	osc->destroy(false);

	/* Now try to use a nonexistent cache with OPEXIST flag set */
	piconfig->sharedClassCacheSize = 1024*1024;
	
	osc = new(osc) SH_OSCachesysv(PORTLIB, vm, cacheDir, OSCACHETEST_CONSTRUCTOR_NAME, piconfig, 1, J9SH_OSCACHE_OPEXIST_DESTROY, 1, 0, 0, &versionData, NULL);
	if(osc->getError() > 0) {
		j9tty_printf(PORTLIB, "testConstructor: opening existing shared memory area with J9SH_OSCACHE_OPEXIST_DESTROY should fail\n");
		rc = FAIL;
	}

cleanup:
	if(osc != NULL) {
		osc->destroy(false);
		j9mem_free_memory(osc);
	}
	j9mem_free_memory(piconfig);
	
	j9tty_printf(PORTLIB, "testConstructor: %s\n", PASS==rc?"PASS":"FAIL");
	return rc;
}

/**
 * This test deliberately causes the constructor to fail, then tests that none
 * of the other public methods work or crash afterwards.
 */
IDATA 
SH_OSCacheTestSysv::testFailedConstructor(J9PortLibrary *portLibrary, J9JavaVM *vm)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SH_OSCachesysv *osc = NULL;
	char cacheDir[J9SH_MAXPATH];
	IDATA rc = PASS;
	J9SharedClassPreinitConfig *piconfig;
	J9PortShcVersion versionData;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /* defined(OPENJ9_BUILD) */
	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;

	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testFailedConstructor: J9SharedClassPreinitConfig memory allocation failed \n");
		return FAIL;
	}

	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		rc = FAIL;
		goto cleanup;
	}

	osc = (SH_OSCachesysv *) j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
	if(NULL == osc) {
		j9tty_printf(PORTLIB, "testFailedConstructor: SH_OSCachesysv memory allocation failed \n");		
		rc = FAIL;
		goto cleanup;
	}

	/* First, we try to create a cache with 0 size - this should fail */
	piconfig->sharedClassCacheSize = 0;
	osc = new(osc) SH_OSCachesysv(PORTLIB, vm, cacheDir, OSCACHETEST_CONSTRUCTOR_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);

	if(osc->getError() >= 0) {
		j9tty_printf(PORTLIB, "testFailedConstructor: failed: setting size to 0 should have failed the constructor\n");
		rc = FAIL;
		goto cleanup;
	}

	/* Now call the public methods in the class, we should get failed results */
	if(osc->attach(SH_OSCacheTestSysv::currentThread, &versionData) != NULL) {
		j9tty_printf(PORTLIB, "testFailedConstructor: failed constructor but attach did not return NULL");
		rc = FAIL;
	}
	
	/* Negative test. Disable Trc_SHR_Assert_ShouldNeverHappen in SH_OSCachesysv::acquireWriteLock() */
	j9shr_UtActive[1009] = 0;

	if(osc->acquireWriteLock(1) != -1) {
		j9tty_printf(PORTLIB, "testFailedConstructor: failed constructor but acquireWriteLock did not return NULL");
		rc = FAIL;
	}

	if(osc->releaseWriteLock(1) != -1) {
		j9tty_printf(PORTLIB, "testFailedConstructor: failed constructor but releaseWriteLock did not return NULL");
		rc = FAIL;
	}

	/* Negative test done. Enable Trc_SHR_Assert_ShouldNeverHappen back */
	j9shr_UtActive[1009] = 1;

cleanup:
	if(osc != NULL) {
		osc->destroy(false);
		j9mem_free_memory(osc);
	}
	j9mem_free_memory(piconfig);

	j9tty_printf(PORTLIB, "testFailedConstructor: %s\n", PASS==rc?"PASS":"FAIL");
	return rc;	
}

IDATA
SH_OSCacheTestSysv::testGetAllCacheStatistics(J9JavaVM* vm)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
#define NUMBER_OF_CACHES_TO_TEST 3
	const char *cacheName[] = {"abc", "def", "ghi"};
	SH_OSCachesysv *osc[NUMBER_OF_CACHES_TO_TEST] = {NULL, NULL, NULL};
	J9Pool *cacheStat = NULL;
	IDATA rc = FAIL;
	IDATA ret = 0;
	J9SharedClassPreinitConfig *piconfig;
	char* ctrlDirName = NULL;
	char cacheDirName[J9SH_MAXPATH];
	char* testDir = NULL;
	UDATA numCacheBeforeTest = 0;
	J9PortShcVersion versionData;
	U_32 flags = 0;
	
	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;

	if(NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testGetAllCacheStatistics: J9SharedClassPreinitConfig memory allocation failed \n");
		goto cleanup;
	}

#if defined(OPENJ9_BUILD)
	flags |= (J9SHMEM_GETDIR_USE_USERHOME | J9SHMEM_GETDIR_APPEND_BASEDIR);
#endif /* defined(OPENJ9_BUILD) */

	ret = j9shmem_getDir(ctrlDirName, flags, cacheDirName, J9SH_MAXPATH);
	if (0 > ret) {
		j9tty_printf(PORTLIB, "testGetAllCacheStatistics: j9shmem_getDir() failed \n");
		goto cleanup;
	}
	
#if !defined(OPENJ9_BUILD)
	testDir = cacheDirName;
#endif /* !defined(OPENJ9_BUILD) */

	/* Before starting existing caches must be cleaned up in case previous test runs failed to clean up (CMVC 167936).
	 * If any step fails the loop continues in order to 'get as much clean up as possible'.
	 */
	for(UDATA i = 0; i < NUMBER_OF_CACHES_TO_TEST; i++) {
		SH_OSCachesysv * tmposc = NULL;
		
		piconfig->sharedClassCacheSize = 10240;

		tmposc = (SH_OSCachesysv *)j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
		if(NULL == tmposc) {
			j9tty_printf(PORTLIB, "testGetAllCacheStatistics: clean up old caches : failed to allocate memory for cache %zu\n", i);
			goto loop_cleanup;
		}

		tmposc = new(tmposc) SH_OSCachesysv(PORTLIB, vm, testDir, cacheName[i], piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);
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
		osc[i] = (SH_OSCachesysv *)j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
		if(NULL == osc[i]) {
			j9tty_printf(PORTLIB, "testGetAllCacheStatistics: failed to allocate memory for cache %zu\n", i);
			goto cleanup;
		}
		/* J9SH_BASEDIR will be appended to cacheDirName. Caches are created in "/tmp/javasharedresources". */
		osc[i] = new(osc[i]) SH_OSCachesysv(PORTLIB, vm, testDir, cacheName[i], piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);
		if(osc[i]->getError() < 0) {
			j9tty_printf(PORTLIB, "testGetAllCacheStatistics: failed to create create cache %zu (getError() =%zd)\n", i, osc[i]->getError());
			goto cleanup;
		}
	}

	/* Search for caches in "/tmp/javasharedresources". */
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
SH_OSCacheTestSysv::testMutex(J9PortLibrary *portLibrary, J9JavaVM *vm, struct j9cmdlineOptions *arg, UDATA child)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SH_OSCachesysv *osc;
	char cacheDir[J9SH_MAXPATH];
	J9SharedClassPreinitConfig* piconfig;
	J9PortShcVersion versionData;
	int argc = arg->argc;
	char **argv = arg->argv;
	char * childargv[SHRTEST_MAX_CMD_OPTS];
	UDATA childargc = 0;
	IDATA rc;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if	defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /* defined(OPENJ9_BUILD) */
	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;

	if (NULL == (osc = (SH_OSCachesysv *)j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testMutex: failed SH_OSCachesysv malloc\n");
		return FAIL;
	}
	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testMutex: failed J9SharedClassPreinitConfig malloc\n");
		return FAIL;
	}

	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		return FAIL;
	}

	piconfig->sharedClassCacheSize = SHM_REGIONSIZE;
	osc = new(osc) SH_OSCachesysv(PORTLIB, vm, cacheDir, OSCACHE_TEST_MUTEX_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);
	if (osc->getError() < 0) {
		j9tty_printf(PORTLIB, "testMutex: cannot open OSCache\n");
		return FAIL;
	}
	
	if(child) {
		if(osc->getError() == J9SH_OSCACHE_CREATED) {
			j9tty_printf(PORTLIB, "testMutex: failed: we are the child but we have created the cache\n");
			return FAIL;
		}
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
	childargc = buildChildCmdlineOption(argc, argv, (OSCACHETEST_CMDLINE_STARTSWITH OSCACHETEST_CMDLINE_MUTEX), childargv);
	J9ProcessHandle pid = LaunchChildProcess(PORTLIB, "testMutex", childargv, childargc);
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
SH_OSCacheTestSysv::testMutexHang(J9PortLibrary *portLibrary, J9JavaVM *vm, struct j9cmdlineOptions *arg, UDATA child)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SH_OSCachesysv *osc = NULL;
	char cacheDir[J9SH_MAXPATH];
	J9ProcessHandle pid = NULL; 
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
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;

	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testMutexHang: cannot allocate J9SharedClassPreinitConfig memory");
		rc = FAIL;
		goto cleanup;
	}
	
	osc = (SH_OSCachesysv *)j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
	if(osc == NULL) {
		j9tty_printf(PORTLIB, "testMutexHang: cannot allocate SH_OSCachesysv memory");
		rc = FAIL;
		goto cleanup;
	}
	
	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		rc = FAIL;
		goto cleanup;
	}

	piconfig->sharedClassCacheSize = 10240;
	osc = new(osc) SH_OSCachesysv(PORTLIB, vm, cacheDir, OSCACHETEST_MUTEX_HANG_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);
	
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
	childargc = buildChildCmdlineOption(argc, argv, (OSCACHETEST_CMDLINE_STARTSWITH OSCACHETEST_CMDLINE_MUTEXHANG), childargv);
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

#define SHM_CACHEDATASIZE(size) (size - SH_OSCachesysv::getHeaderSize())
#define OSCACHETEST_SIZE_NAME "testSize"
#define NO_SIZE_TESTS 3
IDATA 
SH_OSCacheTestSysv::testSize(J9PortLibrary* portLibrary, J9JavaVM *vm)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SH_OSCachesysv *osc = NULL;
	char cacheDir[J9SH_MAXPATH];
	IDATA rc = PASS;
	UDATA length = 0;
	U_64 size[NO_SIZE_TESTS] = {0};
	UDATA expectedLength[NO_SIZE_TESTS] = {0};
	const char *desc[NO_SIZE_TESTS] = {"unknown"};
	UDATA getLimitRc = 0;
	I_32 maxint32 = 0x7fffffff;
	U_64 shmmax64 = 0;
	U_64 shmmax = 0;
	IDATA i = 0;
	J9SharedClassPreinitConfig *piconfig;
	J9PortShcVersion versionData;
	U_32 flags = J9SHMEM_GETDIR_APPEND_BASEDIR;

#if defined(OPENJ9_BUILD)
	flags |= J9SHMEM_GETDIR_USE_USERHOME;
#endif /* defined(OPENJ9_BUILD) */
	setCurrentCacheVersion(vm, J2SE_CURRENT_VERSION, &versionData);
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;

	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testSize: cannot allocate J9SharedClassPreinitConfig memory\n");
		rc = FAIL;
		goto cleanup;
	}

	rc = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (rc < 0) {
		rc = FAIL;
		goto cleanup;
	}

	osc = (SH_OSCachesysv *)j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
	if(osc == NULL) {
		j9tty_printf(PORTLIB, "testSize: cannot allocate SH_OSCachesysv memory\n");
		rc = FAIL;
		goto cleanup;
	}

	/* Try to allocate old cache and destroy it */
	piconfig->sharedClassCacheSize = 10240;
	osc = new(osc) SH_OSCachesysv(PORTLIB, vm, cacheDir, OSCACHETEST_SIZE_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);
	if (osc->getError() >= 0) {
		j9tty_printf(PORTLIB, "testSize found existing cache, destroying\n");
		osc->destroy(false);
	}

	/* Get shmmax */
	getLimitRc = j9sysinfo_get_limit(J9PORT_RESOURCE_SHARED_MEMORY, &shmmax64);
	shmmax = shmmax64;
	if (J9PORT_LIMIT_LIMITED == getLimitRc) {
		j9tty_printf(PORTLIB, "testSize retrieved max shared cache size of %u\n", shmmax);
	} else {
		j9tty_printf(PORTLIB, "testSize: rc %u from j9sysinfo_get_limit\n"
									"If maximum shared memory size (shmmax) should be available on this platform\n"
									"consider this a failure\n", getLimitRc);
		goto cleanup;
	}
	
	if (shmmax > (U_64)maxint32) {
		/* CMVC 143434: If shmmax is greater than max int. Then we set it to be max int. This may happen on 64 bit systems. For example: */
		/* lnxem64tvm1% more /proc/sys/kernel/shmmax */
		/* 		3294967296 */
		j9tty_printf(PORTLIB,
				"testSize: shmmax was %lld setting to max int instead.\n",
				shmmax);
		shmmax = maxint32;
	}

	/* Set up sizes to use for cache */
	desc[0] = "Cache = shmmax";
	size[0] = shmmax;
	expectedLength[0] = SHM_CACHEDATASIZE(((UDATA)size[0]));
	desc[1] = "Cache = shmmax -1";
	size[1] = shmmax - 1;
	expectedLength[1] = SHM_CACHEDATASIZE(((UDATA)size[1]));
	desc[2] = "Cache = shmmax - 1k";
	size[2] = shmmax < 1024 ? 1024 : shmmax - 1024;
	expectedLength[2] = SHM_CACHEDATASIZE(((UDATA)size[2]));

	/* Try allocating cache with the various sizes */
	for(i = 0; i < NO_SIZE_TESTS; i++) {
		piconfig->sharedClassCacheSize = (UDATA)size[i];
		osc = new(osc) SH_OSCachesysv(PORTLIB, vm, cacheDir, OSCACHETEST_SIZE_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);

		if(osc->getError() < 0) {
			UDATA defaultSize = J9_SHARED_CLASS_CACHE_DEFAULT_SIZE;
#if defined(J9VM_ENV_DATA64)
#if defined(OPENJ9_BUILD)
			defaultSize = J9_SHARED_CLASS_CACHE_DEFAULT_SIZE_64BIT_PLATFORM;
#else /* OPENJ9_BUILD */
			if (J2SE_VERSION(vm) >= J2SE_V11) {
				defaultSize = J9_SHARED_CLASS_CACHE_DEFAULT_SIZE_64BIT_PLATFORM;
			}
#endif /* OPENJ9_BUILD */
#endif /* J9VM_ENV_DATA64 */
			if ((i == 0) && (defaultSize > 1024) && (defaultSize < shmmax)) {
				/*CMVC 153490: if we fail alloc ulimit the lets retry tests with the default size*/
				j9tty_printf(PORTLIB, "testSize: shmmax (%lld Bytes) was to big, retrying with default size(%d Bytes).\n", shmmax, defaultSize);
				i = 0;
				shmmax = (defaultSize - 1024);
				/* Set up sizes to use default value */
				size[0] = shmmax;
				expectedLength[0] = SHM_CACHEDATASIZE(((UDATA)size[0]));
				size[1] = shmmax - 1;
				expectedLength[1] = SHM_CACHEDATASIZE(((UDATA)size[1]));
				size[2] = shmmax < 1024 ? 1024 : shmmax - 1024;
				expectedLength[2] = SHM_CACHEDATASIZE(((UDATA)size[2]));

				osc->destroy(false);
				continue;
			}
		
			j9tty_printf(PORTLIB, "testSize: %s(%d): cannot create oscache for size %llu\n", desc[i], i, size[i]);
			rc = FAIL;
			goto cleanup;
		}	

		if (NULL == osc->attach(SH_OSCacheTestSysv::currentThread, &versionData)) {
			j9tty_printf(PORTLIB, "testSize: %s(%d): failed to attach", desc[i], i); 
			rc = FAIL;
			goto cleanup;
		}
		length = osc->getDataSize();
		if (length == expectedLength[i]) {
			j9tty_printf(PORTLIB, "testSize: %s(%d): Successfully allocated cache of size %llu as %llu\n", desc[i], i, size[i], expectedLength[i]);
		} else {
			j9tty_printf(PORTLIB, "testSize: %s(%d): Unable to allocate cache of size %llu, expected %llu, got %llu\n", desc[i], i, size[i], expectedLength[i], length); 
			rc = FAIL;
			goto cleanup;
		}

		osc->destroy(false);
	}
	
cleanup:
	if(osc != NULL) {
		osc->destroy(false);
		j9mem_free_memory(osc);
	}

	j9mem_free_memory(piconfig);

	j9tty_printf(PORTLIB, "testSize: %s\n", PASS==rc?"PASS":"FAIL");
	return rc;
}

#define OSCACHETEST_DESTROY_NAME "testDestroy"
/**
 * This test creates a cache and connects to it, then launches a child process. The child also connects to
 * the cache, then tries to destroy it. Once the child has exited, the parent tries to get the write lock.
 * This should succeed, since the child should have failed to destroy the cache.
 */
IDATA
SH_OSCacheTestSysv::testDestroy (J9PortLibrary* portLibrary, J9JavaVM *vm, struct j9cmdlineOptions *arg, UDATA child)
{
	PORT_ACCESS_FROM_PORT(portLibrary);
	SH_OSCachesysv *osc = NULL;
	char cacheDir[J9SH_MAXPATH];
	J9ProcessHandle pid = NULL;
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
	versionData.cacheType = J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;

	if (NULL == (piconfig = (J9SharedClassPreinitConfig *)j9mem_allocate_memory(sizeof(J9SharedClassPreinitConfig), J9MEM_CATEGORY_CLASSES))) {
		j9tty_printf(PORTLIB, "testDestroy cannot allocate J9SharedClassPreinitConfig memory\n");
		goto cleanup;
	}

	ret = j9shmem_getDir(NULL, flags, cacheDir, J9SH_MAXPATH);
	if (0 > ret) {
	j9tty_printf(PORTLIB, "testDestroy: j9shmem_getDir() failed \n");
		goto cleanup;
	}

	osc = (SH_OSCachesysv *)j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
	if(osc == NULL) {
		j9tty_printf(PORTLIB, "testDestroy cannot allocate SH_OSCachesysv memory\n");
		goto cleanup;
	}

	piconfig->sharedClassCacheSize = 10240;
	osc = new(osc) SH_OSCachesysv(PORTLIB, vm, cacheDir, OSCACHETEST_DESTROY_NAME, piconfig, 1, J9SH_OSCACHE_CREATE, 1, 0, 0, &versionData, NULL);

	if(osc->getError() < 0) {
		j9tty_printf(PORTLIB, "testDestroy: cannot open oscache area\n");
		goto cleanup;
	}	

	if(NULL == osc->attach(SH_OSCacheTestSysv::currentThread, &versionData)) {
		j9tty_printf(PORTLIB, "testDestroy: cannot attach to cache\n");
		goto cleanup;
	}

	if(child) {
		/* For child we attempt to destroy the cache - since the parent is connected it should silently fail */
		osc->destroy(false);
		return PASS;
	}
	
	/* Parent */
	
	childargc = buildChildCmdlineOption(argc, argv, (OSCACHETEST_CMDLINE_STARTSWITH OSCACHETEST_CMDLINE_DESTROY), childargv);
	/* Start the child process, which calls destroy. We should still be able to get the write lock afterwards */
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
SH_OSCacheTestSysv::runTests(J9JavaVM* vm, struct j9cmdlineOptions *arg, const char *cmdline)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA rc = PASS;

	SH_OSCacheTestSysv::currentThread = vm->internalVMFunctions->currentVMThread(vm);
	/* Detect children */
	if (NULL != cmdline) {
		/* We are using strstr, so we need to put the longer string first */
		if (NULL != strstr(cmdline, OSCACHETEST_CMDLINE_MULTIPLECREATE)) {
			return testMultipleCreate(PORTLIB, vm, arg, 1);
		} else if(NULL != strstr(cmdline, OSCACHETEST_CMDLINE_MUTEXHANG)) {
			return testMutexHang(PORTLIB, vm, arg, 1);
		} else if(NULL != strstr(cmdline, OSCACHETEST_CMDLINE_MUTEX)) {
			return testMutex(PORTLIB, vm, arg, 1);
		} else if(NULL != strstr(cmdline, OSCACHETEST_CMDLINE_DESTROY)) {
			return testDestroy(PORTLIB, vm, arg, 1);
		}
	}

	j9tty_printf(PORTLIB, "OSCacheTestSysv begin\n");
	
	j9tty_printf(PORTLIB, "testBasic begin\n");
	rc |= testBasic(PORTLIB, vm);
	
	j9tty_printf(PORTLIB, "testMultipleCreate begin\n");
	rc |= testMultipleCreate(PORTLIB, vm, arg, 0);
	
	j9tty_printf(PORTLIB, "testConstructor begin\n");
	rc |= testConstructor(PORTLIB, vm);
	
	j9tty_printf(PORTLIB, "testFailedConstructor begin\n");
	rc |= testFailedConstructor(PORTLIB, vm);
	
	j9tty_printf(PORTLIB, "testGetAllCacheStatistics begin\n");
	rc |= testGetAllCacheStatistics(vm);
	
	j9tty_printf(PORTLIB, "testMutex begin\n");
	rc |= testMutex(PORTLIB, vm, arg, 0);
	
	j9tty_printf(PORTLIB, "testMutexHang begin\n");
	rc |= testMutexHang(PORTLIB, vm, arg, 0);

	j9tty_printf(PORTLIB, "testSize begin\n");
	rc |= testSize(PORTLIB, vm);

	j9tty_printf(PORTLIB, "testDestroy begin\n");
	rc |= testDestroy(PORTLIB, vm, arg, 0);

	return rc;
}
