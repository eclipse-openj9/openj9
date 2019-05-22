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

/**
 * @file
 * @ingroup Shared_Common
 */

extern "C" {
#include "shrclssup.h"
#include "shrinit.h"
}

#include "j9.h"
#include "jvminit.h"
#include "j9port.h"
#include "j9shrnls.h"
#include "OSCache.hpp"
#include "OSCachesysv.hpp"
#include "util_api.h"
#include "ut_j9shr.h"
#include "j2sever.h"
#include "UnitTest.hpp"
/* #include "OSCachemmap.hpp" */
#include <time.h>
#include <string.h>

#if defined(J9ZTPF)
extern "C" {
	char * atoe_ctime (const time_t*);
}
#undef ctime
#define ctime atoe_ctime
#endif

#include "CacheLifecycleManager.hpp"

#define CLM_TRACE(var1) if (verboseFlags) j9nls_printf(PORTLIB, J9NLS_INFO, var1)
#define CLM_TRACE1(var1, p1) if (verboseFlags) j9nls_printf(PORTLIB, J9NLS_INFO, var1, p1)
#define CLM_TRACE2(var1, p1, p2) if (verboseFlags) j9nls_printf(PORTLIB, J9NLS_INFO, var1, p1, p2)
#define CLM_TRACE_NOTAG(var1) if (verboseFlags) j9nls_printf(PORTLIB, J9NLS_DO_NOT_PRINT_MESSAGE_TAG, var1)
#define CLM_TRACE1_NOTAG(var1, p1) if (verboseFlags) j9nls_printf(PORTLIB, J9NLS_DO_NOT_PRINT_MESSAGE_TAG, var1, p1)
#define CLM_TRACE2_NOTAG(var1, p1, p2) if (verboseFlags) j9nls_printf(PORTLIB, J9NLS_DO_NOT_PRINT_MESSAGE_TAG, var1, p1, p2)
#define CLM_ERR_TRACE(var1) if (verboseFlags) j9nls_printf(PORTLIB, J9NLS_ERROR, var1)
#define CLM_ERR_TRACE1(var1, p1) if (verboseFlags) j9nls_printf(PORTLIB, J9NLS_ERROR, var1, p1)

typedef struct J9SharedCacheManagerParm {
	J9JavaVM* vm;
	I_64 delete_since;
	IDATA cacheRemoved;
	IDATA result;
	UDATA verboseFlags;
	UDATA printIntro;
	UDATA printHeader;
	UDATA printCompatibleHeader;
	UDATA printIncompatibleHeader;
	bool printCompatibleCache;
	bool printIncompatibleCache;
	const char* ctrlDirName;
	UDATA groupPerm;
} J9SharedCacheManagerParm;

extern "C" {
/* Defined in shrinit.cpp */
bool
modifyCacheName(J9JavaVM *vm, const char* origName, UDATA verboseFlags, char** modifiedCacheName, UDATA bufLen);
}

/* Internal function declarations */
static void printSharedCache(void* element, void* param);
static void deleteExpiredSharedCache(void * element, void* param);
static void deleteSharedCache (void* element, void* param);
static IDATA deleteSnapshot(struct J9JavaVM* vm, UDATA verboseFlags, const char* pathFileName);
static J9Pool* getCacheList(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, bool includeOldGenerations, UDATA reason);

/* Function to iterate over all shared class caches and snapshots present in a directory. This function is exposed through
 * the sharedCacheAPI table.
 * If useCommandLineValues is TRUE, then get cacheDir from command line option.
 * If useCommandLineValues is FALSE, then use parameter to get cache signature.
 * If cacheDir is still not available then use default value.
 *
 * @param [in] vm  pointer to J9JavaVM structure.
 * @param [in] ctrlDirName  root of the shared class cache directory.
 * 		A platform default will be chosen if ctrlDirName is NULL
 * @param [in] useCommandLineValue  flag to indicate if command line parameters are to be used or not.
 * @param [in] callback  function pointer to a user provided callback routine.
 * @param [in] user_data  user supplied data, passed to callback function.
 *
 * @return 0 on success, -1 on failure.
 */
IDATA
j9shr_iterateSharedCaches(J9JavaVM *vm,	const char *ctrlDirName, UDATA groupPerm, BOOLEAN useCommandLineValue,
 		IDATA (*callback)(J9JavaVM *vm, J9SharedCacheInfo *event_data, void *user_data),
 		void *user_data)
{
	J9SharedCacheInfo callbackData;
	SH_OSCache_Info *cacheInfo;
	J9Pool *cacheList = NULL;
	J9Pool *snapshotList = NULL;
	pool_state state;
	bool noCacheExist = false;
	bool noSnapshotExist = false;

	if (useCommandLineValue == TRUE){
		UDATA vmGroupPerm = 0;
		if (vm->sharedCacheAPI->runtimeFlags & J9SHR_RUNTIMEFLAG_ENABLE_GROUP_ACCESS) {
			vmGroupPerm = 1;
		}
		/* Don't include old generations */
		cacheList = getCacheList(vm, vm->sharedCacheAPI->ctrlDirName, vmGroupPerm, false, SHR_STATS_REASON_ITERATE); 
		snapshotList = SH_OSCache::getAllCacheStatistics(vm, vm->sharedCacheAPI->ctrlDirName, vmGroupPerm, 0, J2SE_VERSION(vm), false, false, SHR_STATS_REASON_ITERATE, false);
	} else {
		/* Don't include old generations */
		cacheList = getCacheList(vm, ctrlDirName, groupPerm, false, SHR_STATS_REASON_ITERATE);
		snapshotList = SH_OSCache::getAllCacheStatistics(vm, ctrlDirName, groupPerm, 0, J2SE_VERSION(vm), false, false, SHR_STATS_REASON_ITERATE, false);
	}

	if (NULL == cacheList || pool_numElements(cacheList) == 0) {
		if (NULL != cacheList) {
			pool_kill(cacheList);
		}
		noCacheExist = true;
	}
	
	if (NULL == snapshotList || pool_numElements(snapshotList) == 0) {
		if (NULL != snapshotList) {
			pool_kill(snapshotList);
		}
		noSnapshotExist = true;
	}

	if (noCacheExist && noSnapshotExist) {
		return 0;
	}

	if (!noCacheExist) {
		cacheInfo = (SH_OSCache_Info *) pool_startDo(cacheList, &state);
		while (cacheInfo) {
			callbackData.name = cacheInfo->name;
			callbackData.isCompatible = cacheInfo->isCompatible;
			callbackData.cacheType = cacheInfo->versionData.cacheType;
			callbackData.modLevel = cacheInfo->versionData.modlevel;
			callbackData.addrMode = cacheInfo->versionData.addrmode;
			callbackData.isCorrupt = cacheInfo->isCorrupt;
			callbackData.os_shmid = cacheInfo->os_shmid;
			callbackData.os_semid = cacheInfo->os_semid;
			callbackData.lastDetach = cacheInfo->lastdetach;

			if ((callbackData.isCompatible == 1) && (cacheInfo->isJavaCorePopulated == 1)) {
				callbackData.cacheSize = cacheInfo->javacoreData.cacheSize;
				callbackData.freeBytes = cacheInfo->javacoreData.freeBytes;
				callbackData.softMaxBytes =  cacheInfo->javacoreData.softMaxBytes;
			} else {
				callbackData.cacheSize = (UDATA)-1;
				callbackData.freeBytes = (UDATA)-1;
				callbackData.softMaxBytes = (UDATA)-1;
			}
			
			if (J9_ARE_ALL_BITS_SET(cacheInfo->versionData.feature, J9SH_FEATURE_COMPRESSED_POINTERS)) {
				Trc_SHR_Assert_True(J9SH_ADDRMODE_32 != callbackData.addrMode);
				callbackData.addrMode |= COM_IBM_ITERATE_SHARED_CACHES_COMPRESSED_POINTERS_MODE;
			} else {
				callbackData.addrMode |= COM_IBM_ITERATE_SHARED_CACHES_NON_COMPRESSED_POINTERS_MODE;
			}

			if (-1 == callback(vm, &callbackData, user_data)) {
				pool_kill(cacheList);
				return -1;
			}
		
			cacheInfo = (SH_OSCache_Info *) pool_nextDo(&state);
		}
		pool_kill(cacheList);
	}
	if (!noSnapshotExist) {
		cacheInfo = (SH_OSCache_Info *) pool_startDo(snapshotList, &state);
		while (cacheInfo) {
			callbackData.name = cacheInfo->name;
			callbackData.isCompatible = cacheInfo->isCompatible;
			Trc_SHR_Assert_Equals(cacheInfo->versionData.cacheType, J9PORT_SHR_CACHE_TYPE_SNAPSHOT);
			callbackData.cacheType = cacheInfo->versionData.cacheType;
			callbackData.modLevel = cacheInfo->versionData.modlevel;
			callbackData.addrMode = cacheInfo->versionData.addrmode;
			callbackData.isCorrupt = cacheInfo->isCorrupt;
			callbackData.os_shmid = cacheInfo->os_shmid;
			callbackData.os_semid = cacheInfo->os_semid;
			callbackData.lastDetach = cacheInfo->lastdetach;

			callbackData.cacheSize = (UDATA)J9SH_OSCACHE_UNKNOWN;
			callbackData.softMaxBytes = (UDATA)J9SH_OSCACHE_UNKNOWN;
			callbackData.freeBytes = (UDATA)J9SH_OSCACHE_UNKNOWN;
			
			if (J9_ARE_ALL_BITS_SET(cacheInfo->versionData.feature, J9SH_FEATURE_COMPRESSED_POINTERS)) {
				Trc_SHR_Assert_True(J9SH_ADDRMODE_32 != callbackData.addrMode);
				callbackData.addrMode |= COM_IBM_ITERATE_SHARED_CACHES_COMPRESSED_POINTERS_MODE;
			} else {
				callbackData.addrMode |= COM_IBM_ITERATE_SHARED_CACHES_NON_COMPRESSED_POINTERS_MODE;
			}

			if (-1 == callback(vm, &callbackData, user_data)) {
				pool_kill(snapshotList);
				return -1;
			}

			cacheInfo = (SH_OSCache_Info *) pool_nextDo(&state);
		}
		pool_kill(snapshotList);
	}
	
	return 0;
}

static void 
printSharedCache(void* element, void* param)
{
	J9SharedCacheManagerParm* state = (J9SharedCacheManagerParm *) param;
	UDATA verboseFlags = J9SHR_VERBOSEFLAG_ENABLE_VERBOSE;
	SH_OSCache_Info* currentItem = (SH_OSCache_Info*) element;
	if (((state->printCompatibleCache == true) && (currentItem->isCompatible))
			|| ((state->printIncompatibleCache == true) && (!currentItem->isCompatible))) {
		char addrmodeStr[10];

		PORT_ACCESS_FROM_JAVAVM(state->vm);

		Trc_SHR_CLM_printSharedCache_Entry();

		if (state->printIntro) {
			char cacheDir[J9SH_MAXPATH];

			SH_OSCache::getCacheDir(state->vm, state->ctrlDirName, cacheDir, J9SH_MAXPATH, J9PORT_SHR_CACHE_TYPE_PERSISTENT);
			j9tty_printf(PORTLIB, "\n");
			CLM_TRACE1_NOTAG(J9NLS_SHRC_INFO_LISTING_FOR_CACHEDIR, cacheDir);
			j9tty_printf(PORTLIB, "\n");
			state->printIntro = 0;
			state->printHeader = 1;
		}

		if (state->printHeader) {
			j9tty_printf(PORTLIB, "%-16s\t", "Cache name");
			j9tty_printf(PORTLIB, "%-14s", "level");
			j9tty_printf(PORTLIB, "%-16s", "cache-type");
			j9tty_printf(PORTLIB, "%-16s", "feature");
#if !defined(WIN32)
			j9tty_printf(PORTLIB, "%-15s", "OS shmid");
			j9tty_printf(PORTLIB, "%-15s", "OS semid");
#endif
			j9tty_printf(PORTLIB, "%-15s", "last detach time\n");
			state->printHeader = 0;
			if (currentItem->isCompatible) {
				state->printCompatibleHeader = 1;
			} else {
				state->printIncompatibleHeader = 1;
			}
		}
		
		if (!currentItem->isCompatible && state->printIncompatibleHeader == 0) {
			state->printIncompatibleHeader = 1;
		}

		if (state->printCompatibleHeader == 1) {
			j9tty_printf(PORTLIB, "\nCompatible shared caches\n");
			state->printCompatibleHeader = 2;
		}

		if (state->printIncompatibleHeader == 1) {
			j9tty_printf(PORTLIB, "\nIncompatible shared caches\n");
			state->printIncompatibleHeader = 2;
		}
		j9tty_printf(PORTLIB, "%-16s\t", currentItem->name);
		char jclLevelStr[10];
		memset(jclLevelStr, 0, sizeof(jclLevelStr));
		getStringForShcModlevel(PORTLIB, currentItem->versionData.modlevel, jclLevelStr, sizeof(jclLevelStr));
		getStringForShcAddrmode(PORTLIB, currentItem->versionData.addrmode, addrmodeStr);
		j9tty_printf(PORTLIB, "%s %s  ", jclLevelStr, addrmodeStr);
		if (J9PORT_SHR_CACHE_TYPE_PERSISTENT == currentItem->versionData.cacheType) {
			j9tty_printf(PORTLIB, "%-16s", "persistent");
		} else if (J9PORT_SHR_CACHE_TYPE_SNAPSHOT == currentItem->versionData.cacheType) {
			j9tty_printf(PORTLIB, "%-16s", "snapshot");
		} else if (J9PORT_SHR_CACHE_TYPE_CROSSGUEST == currentItem->versionData.cacheType) {
			j9tty_printf(PORTLIB, "%-16s", "crossguest");
		} else {
			j9tty_printf(PORTLIB, "%-16s", "non-persistent");
		}
		if (J9_ARE_ALL_BITS_SET(currentItem->versionData.feature, J9SH_FEATURE_COMPRESSED_POINTERS)) {
			j9tty_printf(PORTLIB, "%-16s", "cr");
		} else if (J9_ARE_ALL_BITS_SET(currentItem->versionData.feature, J9SH_FEATURE_NON_COMPRESSED_POINTERS)) {
			j9tty_printf(PORTLIB, "%-16s", "non-cr");
		} else {
			j9tty_printf(PORTLIB, "%-16s", "default");
		}
#if !defined(WIN32)
		if (currentItem->os_shmid != (UDATA)J9SH_OSCACHE_UNKNOWN) {
			j9tty_printf(PORTLIB, "%-15d", currentItem->os_shmid);
		} else {
			j9tty_printf(PORTLIB, "%-15s", "");
		}
		if (currentItem->os_semid != (UDATA)J9SH_OSCACHE_UNKNOWN) {
			j9tty_printf(PORTLIB, "%-15d", currentItem->os_semid);
		} else {
			j9tty_printf(PORTLIB, "%-15s", "");
		}
#endif
		if (currentItem->nattach == 0) {
			if (J9SH_OSCACHE_UNKNOWN == currentItem->lastdetach) {
				j9tty_printf(PORTLIB, "%-15s\n", "Unknown");
			} else {
				time_t t;

				t = (time_t) (currentItem->lastdetach/1000);

				/*
				 *	For now we will use ctime function - we need to modify str_ftime to take in the time at some point
				 *	j9str_ftime(formatted, 30, "%d %b %Y %H:%m", currentItem->lastdetach * 1000);
				 *  j9tty_printf(PORTLIB, "%-15s", formatted);
				 */

				j9tty_printf(PORTLIB, "%-15s", ctime(&t));
			}
		} else if ((currentItem->nattach == J9SH_OSCACHE_UNKNOWN) || (currentItem->lastdetach == J9SH_OSCACHE_UNKNOWN)) {
			if (J9PORT_SHR_CACHE_TYPE_SNAPSHOT == currentItem->versionData.cacheType) {
				/* no last detach time for snapshot,  currentItem->nattach and currentItem->lastdetach are both J9SH_OSCACHE_UNKNOWN */
				j9tty_printf(PORTLIB,"\n");
			} else {
				j9tty_printf(PORTLIB, "%-15s\n", "Unknown");
			}
		} else {
			j9tty_printf(PORTLIB, "%-15s\n", "In use");
		}
	}
	Trc_SHR_CLM_printSharedCache_Exit();
}

static void 
deleteSharedCache(void *element, void* param)
{
	J9SharedCacheManagerParm* state = (J9SharedCacheManagerParm *) param;
	UDATA verboseFlags = state->verboseFlags;
	SH_OSCache_Info* cacheInfo = (SH_OSCache_Info*) element;
	IDATA returnVal = 0;
	UDATA cacheType = cacheInfo->versionData.cacheType;
	
	Trc_SHR_CLM_deleteSharedCache_Entry();
	
	if (J9PORT_SHR_CACHE_TYPE_SNAPSHOT == cacheType) {
		returnVal = j9shr_destroy_snapshot(state->vm, state->ctrlDirName, verboseFlags, cacheInfo->name, cacheInfo->generation, cacheInfo->generation, &(cacheInfo->versionData));
	} else {
		returnVal = j9shr_destroy_cache(state->vm, state->ctrlDirName, verboseFlags, cacheInfo->name, cacheInfo->generation, cacheInfo->generation, &(cacheInfo->versionData), false);
	}
	if ((J9SH_DESTROY_FAILED_CURRENT_GEN_CACHE == returnVal) ||
		(J9SH_DESTROY_FAILED_OLDER_GEN_CACHE == returnVal) ||
		(J9SH_DESTROY_FAILED_CURRENT_GEN_SNAPSHOT == returnVal) ||
		(J9SH_DESTROY_FAILED_OLDER_GEN_SNAPSHOT == returnVal) ||
		(J9SH_DESTROYED_NONE == returnVal )) {
		state->result = -1;
		Trc_SHR_CLM_deleteSharedCache_ExitError();
		return;
	}

	state->result = 0;
	Trc_SHR_CLM_deleteSharedCache_Exit();
	return;
}

static void
deleteExpiredSharedCache(void *element, void *param)
{
	J9SharedCacheManagerParm* state = (J9SharedCacheManagerParm *) param;
	SH_OSCache_Info* cacheInfo = (SH_OSCache_Info*) element;

	Trc_SHR_CLM_deleteExpiredSharedCache_Entry();

	if (cacheInfo->nattach > 0) {
		/* Somebody still attached to the cache */
		Trc_SHR_CLM_deleteExpiredSharedCache_StillAttached();
		return;
	}

	if ((state->delete_since == 0) || (cacheInfo->lastdetach < state->delete_since)) {
		deleteSharedCache(element, param);
		state->cacheRemoved++;
	}

	Trc_SHR_CLM_deleteExpiredSharedCache_Exit();
	return;
}

/* If includeOldGenerations is true, function returns caches of all generations. 
 * Otherwise returns only caches of current generation 
 * It is the responsibility of the caller to free the pool created */
static J9Pool*
getCacheList(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, bool includeOldGenerations, UDATA reason)
{
	J9Pool* cacheList;
	
	Trc_SHR_CLM_getCacheList_Entry();

	/* Verbose flags not required - do not want to see permissions error accessing all caches */
	cacheList = SH_OSCache::getAllCacheStatistics(vm, ctrlDirName, groupPerm, 0, J2SE_VERSION(vm), includeOldGenerations, false, reason, true);
	
	Trc_SHR_CLM_getCacheList_Exit();
	return cacheList;
}

/* Finds all caches of a given name which are not compatible with the current JVM
 * It is the responsibility of the caller to free the pool created */
static J9Pool*
findIncompatibleCachesForName(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, const char* name)
{
	J9Pool* cacheList;

	Trc_SHR_CLM_findIncompatibleCachesForName_Entry(name);
	
	/* Verbose flags not required - do not want to see permissions error accessing all caches */
	cacheList = SH_OSCache::getAllCacheStatistics(vm, ctrlDirName, groupPerm, 0, J2SE_VERSION(vm), true, true, SHR_STATS_REASON_GETNAME, true);
	
	Trc_SHR_CLM_findIncompatibleCachesForName_Exit();
	return cacheList;
}

/**
 * This function lists all the shared class caches and snapshots that can be found in a given cacheDir.
 *
 * The list is written to standard err.
 *
 * @param [in] vm  A Java VM
 * @param [in] ctrlDirName  root of the shared class cache directory.
 * @param [in] verboseFlags  Flags controlling the level of verbose messages issued
 *
 * @return 0 on success, -1 on failure
 */
IDATA 
j9shr_list_caches(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, UDATA verboseFlags)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	J9SharedCacheManagerParm param;
	J9Pool* cacheList = NULL;
	J9Pool* snapshotList = NULL;
	bool noCacheExist = false;
	bool noSnapshotExist = false;
	
	Trc_SHR_CLM_j9shr_list_caches_Entry(verboseFlags);
	
	/* Don't include old generations, don't populate javacore data */
	cacheList = getCacheList(vm, ctrlDirName, groupPerm, false, SHR_STATS_REASON_LIST);
	snapshotList = SH_OSCache::getAllCacheStatistics(vm, ctrlDirName, groupPerm, 0, J2SE_VERSION(vm), false, false, SHR_STATS_REASON_LIST, false);

	noCacheExist = (NULL == cacheList) || (0 == pool_numElements(cacheList));
	noSnapshotExist = (NULL == snapshotList) || (0 == pool_numElements(snapshotList));

	if (noCacheExist && noSnapshotExist) {
		/* There are no caches or snapshots */
		CLM_TRACE(J9NLS_SHRC_CLCM_NO_SHARE_CACHE);
		if (NULL != cacheList) {
			pool_kill(cacheList);
		}
		if (NULL != snapshotList) {
			pool_kill(snapshotList);
		}
		Trc_SHR_CLM_j9shr_list_caches_noCaches();
		return -1;
	}
	param.vm = vm;
	param.printIntro = 1;
	param.printHeader = 1;
	param.groupPerm = groupPerm;
	param.ctrlDirName = ctrlDirName;
	param.printIncompatibleHeader=0;

	/* CMVC 167896: needs to print the compatible caches first, because the
	 * order of the pool items is not always the order the items were
	 * put in */
	/* Print the compatible caches first */
	param.printCompatibleCache = true;
	param.printIncompatibleCache = false;
	if (!noCacheExist) {
		pool_do(cacheList, &printSharedCache, &param);
	}
	if (!noSnapshotExist) {
		pool_do(snapshotList, &printSharedCache, &param);
	}

	/* Print the incompatible caches second */
	param.printCompatibleCache = false;
	param.printIncompatibleCache = true;
	if (!noCacheExist) {
		pool_do(cacheList, &printSharedCache, &param);
	}
	if (!noSnapshotExist) {
		pool_do(snapshotList, &printSharedCache, &param);
	}
	
	j9tty_printf(PORTLIB, "\n");
	if (NULL != cacheList) {
		pool_kill(cacheList);
	}
	if (NULL != snapshotList) {
		pool_kill(snapshotList);
	}
	Trc_SHR_CLM_j9shr_list_caches_Exit();
	return 0;
}

/**
 * This function will destroy all the shared class caches which haven't been attached to for 
 * greater than the specified number of minutes. 
 *
 * On Windows NTFS, this function is only accurate to the nearest hour
 *
 * @param [in] vm  A Java VM
 * @param [in] ctrlDirName  root of the shared class cache directory.
 * @param [in] verboseFlags  Flags controlling the level of verbose messages issued
 * @param [in] minutes  The time period in minutes
 * 
 * @return 0 on success, -1 on failure
 */
IDATA 
j9shr_destroy_expire_cache(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, UDATA verboseFlags, UDATA minutes) {
	J9SharedCacheManagerParm param;
	J9Pool* cacheList;

	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_SHR_CLM_j9shr_destroy_expire_cache_Entry(verboseFlags, minutes);

	param.vm = vm;
	param.verboseFlags = verboseFlags;
	/* Include all generations in the list, don't populate javacore data */
	cacheList = getCacheList(vm, ctrlDirName, groupPerm, true, SHR_STATS_REASON_EXPIRE);

	if (NULL == cacheList || pool_numElements(cacheList) == 0) {
		CLM_TRACE(J9NLS_SHRC_CLCM_NO_SHARE_CACHE);
		Trc_SHR_CLM_j9shr_destroy_expire_cache_noCaches();
		return -1;
	}

	if (minutes > 0) {
		param.delete_since = j9time_current_time_millis() - (minutes*60*1000);
	} else {
		param.delete_since = 0;					/* If expire=0, then treat as destroyAll */
	}
	
	param.cacheRemoved = 0;
	param.groupPerm = groupPerm;
	param.ctrlDirName = ctrlDirName;
	pool_do(cacheList, &deleteExpiredSharedCache, &param);
	
	pool_kill(cacheList);

	if (verboseFlags & J9SHR_VERBOSEFLAG_ENABLE_VERBOSE) {
		CLM_TRACE2_NOTAG(J9NLS_SHRC_CLCM_CACHE_EXPIRED, minutes, param.cacheRemoved);
	}
	
	Trc_SHR_CLM_j9shr_destroy_expire_cache_Exit();
	return 0;
}

/**
 * This method will destroy all the shared class caches that can be found on the system
 *
 * @param [in] vm  A Java VM
 * @param [in] ctrlDirName  root of the shared class cache directory.
 * @param [in] verboseFlags  Flags controlling the level of verbose messages issued
 * @return 0 on success, -1 on failure
 */
IDATA 
j9shr_destroy_all_cache(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, UDATA verboseFlags) {
	J9SharedCacheManagerParm param;
	J9Pool* cacheList;
	char cacheDir[J9SH_MAXPATH];
	
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_SHR_CLM_j9shr_destroy_all_cache_Entry(verboseFlags);

	param.vm = vm;
	param.verboseFlags = verboseFlags;
	param.groupPerm = groupPerm;
	param.ctrlDirName = ctrlDirName;
	
	/* Include old generations, don't populate javacore data */
	cacheList = getCacheList(vm, ctrlDirName, groupPerm, true, SHR_STATS_REASON_DESTROY);

	if (NULL == cacheList || pool_numElements(cacheList) == 0) {
		CLM_TRACE(J9NLS_SHRC_CLCM_NO_SHARE_CACHE);
		Trc_SHR_CLM_j9shr_destroy_all_cache_noCaches();
		return -1;
	}
	
	SH_OSCache::getCacheDir(vm, ctrlDirName, cacheDir, J9SH_MAXPATH, J9PORT_SHR_CACHE_TYPE_PERSISTENT);
	j9tty_printf(PORTLIB, "\n");
	CLM_TRACE1_NOTAG(J9NLS_SHRC_INFO_DESTROYING_FOR_CACHEDIR, cacheDir);
	j9tty_printf(PORTLIB, "\n");

	pool_do(cacheList, &deleteSharedCache, &param);
	
	pool_kill(cacheList);
	
	Trc_SHR_CLM_j9shr_destroy_all_cache_Exit();
	return 0;
}

/**
 * This function will destroy all the snapshots that can be found in the ctrlDirName.
 *  
 * @param [in] vm  The current J9JavaVM
 * @param [in] ctrlDirName The snapshot file directory
 * @param [in] groupPerm The group permissions to open the snapshot directory
 * @param [in] verboseFlags  Flags controlling the level of verbose messages issued
 *
 * @return 0 on success, -1 on failure
 */
IDATA
j9shr_destroy_all_snapshot(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, UDATA verboseFlags)
{
	J9SharedCacheManagerParm param;
	J9Pool* snapshotList = NULL;
	char cacheDir[J9SH_MAXPATH];
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_SHR_CLM_j9shr_destroy_all_snapshot_Entry(verboseFlags);

	param.vm = vm;
	param.verboseFlags = verboseFlags;
	param.groupPerm = groupPerm;
	param.ctrlDirName = ctrlDirName;

	snapshotList = SH_OSCache::getAllCacheStatistics(vm, ctrlDirName, groupPerm, 0, J2SE_VERSION(vm), true, false, SHR_STATS_REASON_DESTROY, false);

	if ((NULL == snapshotList) || (0 == pool_numElements(snapshotList))) {
		Trc_SHR_CLM_j9shr_destroy_all_snapshot_noSnapshots();
		CLM_TRACE(J9NLS_SHRC_CLCM_NO_SNAPSHOT);
		return -1;
	}

	if (-1 == SH_OSCache::getCacheDir(vm, ctrlDirName, cacheDir, J9SH_MAXPATH, J9PORT_SHR_CACHE_TYPE_SNAPSHOT)) {
		Trc_SHR_CLM_j9shr_destroy_all_snapshot_getCacheDirFailed();
		/* NLS message has been printed out inside SH_OSCache::getCacheDir() if verbose flag is not 0 */
		return -1;
	}
	j9tty_printf(PORTLIB, "\n");
	CLM_TRACE1_NOTAG(J9NLS_SHRC_INFO_SNAPSHOT_DESTROYING_FOR_CACHEDIR, cacheDir);
	j9tty_printf(PORTLIB, "\n");

	pool_do(snapshotList, &deleteSharedCache, &param);

	pool_kill(snapshotList);

	Trc_SHR_CLM_j9shr_destroy_all_snapshot_Exit();
	return 0;
}

/**
 * Wrapper function to destroy a named cache/snapshot. This function is exposed through
 * the sharedCacheAPI table.
 * Cache signature consists of cacheDir, name and cacheType (persistent, non-persistent or snapshot).
 * If useCommandLineValues is TRUE, then get the cache signature from command line option.
 * If useCommandLineValues is FALSE, then use parameters to get cache signature.
 * If any of the field in cache signature is not available then use default value.
 *
 * @param [in] vm  pointer to J9JavaVM structure.
 * @param [in] ctrlDirName  root of the shared class cache directory.
 * @param [in] name  name of the cache/snapshot to be destroyed.
 * @param [in] cacheType flag to indicate the cache's type.
 * @param [in] useCommandLineValues  flag to indicate if command line parameters are to be used or not.
 *
 * @returns	J9SH_DESTROYED_ALL_CACHE/J9SH_DESTROYED_ALL_SNAPSHOT when no cache/snapshot exists or successfully destroyed all caches/snapshots,
 * 			J9SH_DESTROYED_NONE when it fails to destroy any cache/snapshot,
 * 			J9SH_DESTROY_FAILED_CURRENT_GEN_CACHE/J9SH_DESTROY_FAILED_CURRENT_GEN_SNAPSHOT when it failed to destroy cache/snapshot of current generation,
 * 			J9SH_DESTROY_FAILED_OLDER_GEN_CACHE/J9SH_DESTROY_FAILED_OLDER_GEN_SNAPSHOT when it failed to destroy one or more older generation cache/snapshot and either current generation cache/snapshot does not exists or is successfully destroyed.
 *
 * Note: cacheType = 0 is an indicator to use platform default value for cacheType.
 */
IDATA
j9shr_destroySharedCache(J9JavaVM *vm, const char *ctrlDirName, const char *name, U_32 cacheType, BOOLEAN useCommandLineValues)
{
	J9PortShcVersion versionData;
	const char *cacheName;
	char modifiedCacheName[CACHE_ROOT_MAXLEN];
	char *modifiedCacheNamePtr = modifiedCacheName;
	const char *ctrlDir;
	UDATA verboseFlags = 0;
	IDATA rc = 0;

	setCurrentCacheVersion(vm, J2SE_VERSION(vm), &versionData);
	if (useCommandLineValues == TRUE){
		cacheName = vm->sharedCacheAPI->cacheName;
		versionData.cacheType = (U_32) vm->sharedCacheAPI->cacheType;
		ctrlDir = vm->sharedCacheAPI->ctrlDirName;
	} else {
		cacheName = name;
		versionData.cacheType = cacheType;
		ctrlDir = ctrlDirName;
	}
	if (cacheName == NULL) {
		cacheName = CACHE_ROOT_PREFIX;
	}
	if(modifyCacheName(vm, cacheName, vm->sharedCacheAPI->verboseFlags, &modifiedCacheNamePtr, USER_SPECIFIED_CACHE_NAME_MAXLEN) == false) {
		return -1;
	}
	if (versionData.cacheType == 0) {
		versionData.cacheType = (j9shr_isPlatformDefaultPersistent(vm) == TRUE) ? J9PORT_SHR_CACHE_TYPE_PERSISTENT : J9PORT_SHR_CACHE_TYPE_NONPERSISTENT;
	}

	if (UnitTest::SHAREDCACHE_API_TEST == UnitTest::unitTest) {
		verboseFlags = J9SHR_VERBOSEFLAG_ENABLE_VERBOSE_DEFAULT;
	}
	if (J9PORT_SHR_CACHE_TYPE_SNAPSHOT == cacheType) {
		rc = j9shr_destroy_snapshot(vm, ctrlDir, verboseFlags, modifiedCacheNamePtr, OSCACHE_LOWEST_ACTIVE_GEN, OSCACHE_CURRENT_CACHE_GEN, &versionData);
	} else {
		rc = j9shr_destroy_cache(vm, ctrlDir, verboseFlags, modifiedCacheNamePtr, OSCACHE_LOWEST_ACTIVE_GEN, OSCACHE_CURRENT_CACHE_GEN, &versionData, false);
	}

	return rc;
}

/**
 * This method will destroy the shared classes cache with the specified name.
 *
 * Caller must have the correct permissions to remove the cache
 *
 * @param [in] vm  A Java VM
 * @param [in] ctrlDirName  root of the shared class cache directory.
 * @param [in] verboseFlags  Flags controlling the level of verbose messages issued
 * @param [in] cacheName  A pointer to the name of the cache to be destroyed
 * @param [in] generationStart  Delete all cache generations starting from generation specified
 * @param [in] generationEnd  Delete all cache generations upto and including generation specified
 * @param [in] versionData  The version data describing the cache to destroy
 * @param [in] isReset  True if reset option is being used, false otherwise.
 *
 * @returns	J9SH_DESTROYED_ALL_CACHE when no cache exists or successfully destroyed all caches,
 * 			J9SH_DESTROYED_NONE when it fails to destroy any cache,
 * 			J9SH_DESTROY_FAILED_CURRENT_GEN_CACHE when it failed to destroy cache of current generation,
 * 			J9SH_DESTROY_FAILED_OLDER_GEN_CACHE when it failed to destroy one or more older generation cache and either current generation cache does not exists or is successfully destroyed.
 */
IDATA
j9shr_destroy_cache(struct J9JavaVM* vm, const char* ctrlDirName, UDATA verboseFlags, const char* cacheName, UDATA generationStart, UDATA generationEnd, J9PortShcVersion* versionData, BOOLEAN isReset) {
	SH_OSCache* oscache;
	IDATA returnVal = J9SH_DESTROYED_ALL_CACHE;
	IDATA cacheStatus = J9SH_DESTROYED_NONE;
	char cacheDirName[J9SH_MAXPATH];
	bool noCacheExists = true;
	UDATA lastGeneration = generationEnd;
	PORT_ACCESS_FROM_JAVAVM(vm);

	Trc_SHR_CLM_j9shr_destroy_cache_Entry(verboseFlags, cacheName, generationStart, generationEnd);

	oscache = (SH_OSCache*) j9mem_allocate_memory(SH_OSCache::getRequiredConstrBytes(), J9MEM_CATEGORY_CLASSES);
	if (oscache == NULL) {
		Trc_SHR_CLM_j9shr_destroy_cache_allocFailed();
		CLM_TRACE1(J9NLS_SHRC_CLCM_FAILED_REMOVED, cacheName);
		return J9SH_DESTROYED_NONE;
	}

	if (SH_OSCache::getCacheDir(vm, ctrlDirName, cacheDirName, J9SH_MAXPATH, versionData->cacheType) == -1) {
		Trc_SHR_CLM_j9shr_destroy_cache_getCacheDirFailed();
		CLM_TRACE1(J9NLS_SHRC_CLCM_FAILED_REMOVED, cacheName);
		return J9SH_DESTROYED_NONE;
	}

	if (OSCACHE_CURRENT_CACHE_GEN == generationEnd) {
		/* current generation cache is destroyed after destroying existing older generation caches */
		lastGeneration = generationEnd - 1;
	}
	/* try to remove older generation cache first */
	for (UDATA i = generationStart; i <= lastGeneration; i++) {
		/* explicitly pass 0 as verboseFlags to j9shr_stat_cache to suppress printing of messages related to cache existence in all cases */
		if (1 == j9shr_stat_cache(vm, cacheDirName, 0 , cacheName, versionData, i)) {
			cacheStatus = J9SH_DESTROYED_OLDER_GEN_CACHE;
			SH_OSCache::newInstance(PORTLIB, oscache, cacheName, i, versionData);
			/*Pass in 0 for runtime flags means we will not check semid in cache header when we destroy a cache*/
			if (!oscache->startup(vm, ctrlDirName, vm->sharedCacheAPI->cacheDirPerm, cacheName, vm->sharedClassPreinitConfig, 0, J9SH_OSCACHE_OPEXIST_DESTROY, verboseFlags, 0/*runtime check*/, 0, vm->sharedCacheAPI->storageKeyTesting, versionData, NULL, SHR_STARTUP_REASON_DESTROY)) {
				if (oscache->getError() != J9SH_OSCACHE_NO_CACHE) {
					cacheStatus = J9SH_DESTROY_FAILED_OLDER_GEN_CACHE;
					noCacheExists = false;
				}
			} else {
				noCacheExists = false;
				if (-1 == oscache->destroy(false, (FALSE != isReset))) {
					/* failed to destroy a cache */
					cacheStatus = J9SH_DESTROY_FAILED_OLDER_GEN_CACHE;
				}
			}
			oscache->cleanup();
		}
	}

	if (false == noCacheExists) {
		if (J9SH_DESTROYED_OLDER_GEN_CACHE == cacheStatus) {
			CLM_TRACE1(J9NLS_SHRC_CLCM_REMOVED_OLDER_GEN, cacheName);
		} else {
			returnVal = cacheStatus;
			CLM_TRACE1(J9NLS_SHRC_CLCM_FAILED_REMOVED_OLDER_GEN, cacheName);
		}
	}

	/* try to remove current generation cache */
	if (OSCACHE_CURRENT_CACHE_GEN == generationEnd) {
		/* explicitly pass 0 as verboseFlags to j9shr_stat_cache to suppress printing of messages related to cache existence in all cases */
		if (1 == j9shr_stat_cache(vm, cacheDirName, 0 , cacheName, versionData, OSCACHE_CURRENT_CACHE_GEN)) {
			SH_OSCache::newInstance(PORTLIB, oscache, cacheName, OSCACHE_CURRENT_CACHE_GEN, versionData);
			/*Pass in 0 for runtime flags means we will not check semid in cache header when we destroy a cache*/
			if (!oscache->startup(vm, ctrlDirName, vm->sharedCacheAPI->cacheDirPerm, cacheName, vm->sharedClassPreinitConfig, 0, J9SH_OSCACHE_OPEXIST_DESTROY, verboseFlags, 0/*runtime check*/, 0, vm->sharedCacheAPI->storageKeyTesting, versionData, NULL, SHR_STARTUP_REASON_DESTROY)) {
				/* failed to destroy current gen cache. */
				if (oscache->getError() != J9SH_OSCACHE_NO_CACHE) {
					noCacheExists = false;
					returnVal = J9SH_DESTROY_FAILED_CURRENT_GEN_CACHE;
					CLM_TRACE1(J9NLS_SHRC_CLCM_FAILED_REMOVED_CURRENT_GEN, cacheName);
				}
			} else {
				noCacheExists = false;
				if (-1 == oscache->destroy(false, (FALSE != isReset))) {
					/* failed to destroy current gen cache. */
					returnVal = J9SH_DESTROY_FAILED_CURRENT_GEN_CACHE;
					CLM_TRACE1(J9NLS_SHRC_CLCM_FAILED_REMOVED_CURRENT_GEN, cacheName);
				}
			}
			oscache->cleanup();
		}
	}

	if (true == noCacheExists) {
		CLM_ERR_TRACE(J9NLS_SHRC_OSCACHE_NOT_EXIST);
	}

	j9mem_free_memory(oscache);

	Trc_SHR_CLM_j9shr_destroy_cache_Exit(returnVal);
	return returnVal;
}

/**
 * Helper function to delete the snapshot file.
 *
 * @param [in] vm  The current J9JavaVM
 * @param [in] verboseFlags  Flags controlling the level of verbose messages issued
 * @param [in] pathFileName  The name of the snapshot file to deleted.
 *
 * @return	0 	when it successfully deleted the snapshot file,
 * 			-1 	when J9PORT_ERROR_FILE_NOENT occurred while deleting the snapshot file,
 * 			-2 	when it failed to delete the snapshot file
 */

static IDATA
deleteSnapshot(struct J9JavaVM* vm, UDATA verboseFlags, const char* pathFileName)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	I_32 fileRc = -1;
	I_64 fileSize = 0;
	IDATA rc = 0;
	IDATA fd = j9file_open(pathFileName, EsOpenRead | EsOpenWrite, 0);

	Trc_SHR_CLM_deleteSnapshot_Entry(pathFileName);

	if (fd < 0) {
		I_32 errorno = j9error_last_error_number();

		if (J9PORT_ERROR_FILE_NOENT == errorno) {
			rc = -1;
		} else {
			const char * errormsg = j9error_last_error_message();

			Trc_SHR_CLM_deleteSnapshot_fileOpenFailed(pathFileName);
			CLM_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
			Trc_SHR_Assert_True(errormsg != NULL);
			CLM_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
			CLM_ERR_TRACE1(J9NLS_SHRC_ERROR_SNAPSHOT_FILE_OPEN, pathFileName);
			rc = -2;
		}
		goto done;;
	}
	/* Get the length of the file, acquire file lock and delete the file in the lock */
	fileSize = j9file_flength(fd);
	if (fileSize < 0) {
		I_32 errorno = j9error_last_error_number();
		const char * errormsg = j9error_last_error_message();

		Trc_SHR_CLM_deleteSnapshot_fileGetLengthFailed(pathFileName);
		CLM_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
		Trc_SHR_Assert_True(errormsg != NULL);
		CLM_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
		CLM_ERR_TRACE1(J9NLS_SHRC_CLCM_ERROR_SNAPSHOT_FILE_LENGTH, pathFileName);
		rc = -2;
		j9file_close(fd);
		goto done;
	}

	if (j9file_lock_bytes(fd, J9PORT_FILE_WRITE_LOCK | J9PORT_FILE_WAIT_FOR_LOCK, 0, fileSize) < 0) {
		I_32 errorno = j9error_last_error_number();
		const char * errormsg = j9error_last_error_message();

		Trc_SHR_CLM_deleteSnapshot_fileLockFailed(pathFileName);
		CLM_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
		Trc_SHR_Assert_True(errormsg != NULL);
		CLM_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
		CLM_ERR_TRACE1(J9NLS_SHRC_ERROR_SNAPSHOT_FILE_LOCK, pathFileName);
		rc = -2;
		j9file_close(fd);
		goto done;
	}

	fileRc = j9file_unlink(pathFileName);
	if (fileRc < 0) {
		I_32 errorno = j9error_last_error_number();

		if (J9PORT_ERROR_FILE_NOENT == errorno) {
			rc = -1;
		} else {
			/* failed to delete the file */
			const char * errormsg = j9error_last_error_message();

			Trc_SHR_CLM_deleteSnapshot_fileUnlinkFailed(pathFileName);
			CLM_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_NUMBER, errorno);
			Trc_SHR_Assert_True(errormsg != NULL);
			CLM_ERR_TRACE1(J9NLS_SHRC_PORT_ERROR_MESSAGE, errormsg);
			rc = -2;
		}
	}
	/* file lock will be released when closed */
	j9file_close(fd);
done:
	Trc_SHR_CLM_deleteSnapshot_Exit(rc);
	return rc;
}


/**
 * This function will destroy the snapshot with the specified name in the ctrlDirName.
 * 
 * @param [in] vm  The current J9JavaVM
 * @param [in] ctrlDirName The snapshot file directory
 * @param [in] verboseFlags  Flags controlling the level of verbose messages issued
 * @param [in] snapshotName  The name of the snapshot to be destroyed
 * @param [in] generationStart  Delete all snapshot of cache generations starting from generation specified
 * @param [in] generationEnd  Delete all snapshot of cache generations upto and including generation specified
 * @param [in] versionData  The version data describing the snapshot to destroy
 *
 * @return	J9SH_DESTROYED_ALL_SNAPSHOT when no snapshot exists or successfully destroyed all snapshots,
 * 			J9SH_DESTROYED_NONE when it fails to destroy any snapshots,
 * 			J9SH_DESTROY_FAILED_CURRENT_GEN_SNAPSHOT when it failed to destroy snapshot of cache of current generation,
 * 			J9SH_DESTROY_FAILED_OLDER_GEN_SNAPSHOT when it failed to destroy one or more snapshot of older generation cache 
 * 													and either snapshot of current generation cache does not exists or is successfully destroyed.
 */
IDATA
j9shr_destroy_snapshot(struct J9JavaVM* vm, const char* ctrlDirName, UDATA verboseFlags, const char* snapshotName, UDATA generationStart, UDATA generationEnd, J9PortShcVersion* versionData)
{
	IDATA returnVal = J9SH_DESTROYED_ALL_SNAPSHOT;
	IDATA snapshotStatus = J9SH_DESTROYED_NONE;
	char nameWithVGen[CACHE_ROOT_MAXLEN];
	char pathFileName[J9SH_MAXPATH];
	char cacheDirName[J9SH_MAXPATH];
	bool noSnapshotExists = true;
	UDATA lastGeneration = generationEnd;
	PORT_ACCESS_FROM_JAVAVM(vm);
	
	Trc_SHR_CLM_j9shr_destroy_snapshot_Entry(verboseFlags, snapshotName, generationStart, generationEnd);

	if (-1 == SH_OSCache::getCacheDir(vm, ctrlDirName, cacheDirName, J9SH_MAXPATH, J9PORT_SHR_CACHE_TYPE_SNAPSHOT)) {
		Trc_SHR_CLM_j9shr_destroy_snapshot_getCacheDirFailed();
		/* NLS message has been printed out inside SH_OSCache::getCacheDir() if verbose flag is not 0 */
		returnVal = J9SH_DESTROYED_NONE;
		goto done;
	}

	if (OSCACHE_CURRENT_CACHE_GEN == generationEnd) {
		/* current generation cache snapshot is destroyed after destroying existing older generation cache snapshots */
		lastGeneration = generationEnd - 1;
	}
	/* try to remove older generation cache snapshot first */
	for (UDATA i = generationStart; i <= lastGeneration; i++) {
		SH_OSCache::getCacheVersionAndGen(PORTLIB, vm, nameWithVGen, CACHE_ROOT_MAXLEN, snapshotName, versionData, i, false);
		/* No check for the return value of getCachePathName() as it always returns 0 */
		SH_OSCache::getCachePathName(PORTLIB, cacheDirName, pathFileName, J9SH_MAXPATH, nameWithVGen);
		if (EsIsFile == j9file_attr(pathFileName)) {
			IDATA rc = deleteSnapshot(vm, verboseFlags, pathFileName);

			if (0 == rc) {
				noSnapshotExists = false;
				snapshotStatus = J9SH_DESTROYED_OLDER_GEN_SNAPSHOT;
				CLM_TRACE1(J9NLS_SHRC_CLCM_SNAPSHOT_REMOVED_OLDER_GEN, snapshotName);
			} else if (-2 == rc) {
				noSnapshotExists = false;
				snapshotStatus = J9SH_DESTROY_FAILED_OLDER_GEN_SNAPSHOT;
				returnVal = snapshotStatus;
				CLM_TRACE1(J9NLS_SHRC_CLCM_SNAPSHOT_FAILED_REMOVED_OLDER_GEN, snapshotName);
			}
			/* rc is -1 means J9PORT_ERROR_FILE_NOENT occurred while deleting the snapshot file, silently ignore this case*/
		}
	}

	/* try to remove current generation cache snapshot */
	if (OSCACHE_CURRENT_CACHE_GEN == generationEnd) {
		SH_OSCache::getCacheVersionAndGen(PORTLIB, vm, nameWithVGen, CACHE_ROOT_MAXLEN, snapshotName, versionData, OSCACHE_CURRENT_CACHE_GEN, false);
		/* No check for the return value of getCachePathName() as it always returns 0 */
		SH_OSCache::getCachePathName(PORTLIB, cacheDirName, pathFileName, J9SH_MAXPATH, nameWithVGen);
		if (EsIsFile == j9file_attr(pathFileName)) {
			IDATA rc = deleteSnapshot(vm, verboseFlags, pathFileName);

			if (0 == rc) {
				/* Destroy the snapshot successfully */
				J9PortShcVersion versionData;

				noSnapshotExists = false;
				memset(&versionData, 0, sizeof(J9PortShcVersion));
				/* Do not care about the getValuesFromShcFilePrefix() return value */
				getValuesFromShcFilePrefix(PORTLIB, nameWithVGen, &versionData);
				if (J9SH_FEATURE_COMPRESSED_POINTERS == versionData.feature) {
					CLM_TRACE1(J9NLS_SHRC_CLCM_SNAPSHOT_DESTROYED_CR, snapshotName);
				} else if (J9SH_FEATURE_NON_COMPRESSED_POINTERS == versionData.feature) {
					CLM_TRACE1(J9NLS_SHRC_CLCM_SNAPSHOT_DESTROYED_NONCR, snapshotName);
				} else {
					CLM_TRACE1(J9NLS_SHRC_CLCM_SNAPSHOT_DESTROYED, snapshotName);
				}
			} else if (-2 == rc) {
				noSnapshotExists = false;
				returnVal = J9SH_DESTROY_FAILED_CURRENT_GEN_SNAPSHOT;
				CLM_TRACE1(J9NLS_SHRC_CLCM_SNAPSHOT_FAILED_REMOVED_CURRENT_GEN, snapshotName);
			}
			/* rc is -1 means J9PORT_ERROR_FILE_NOENT occurred while deleting the snapshot file, silently ignore this case*/
		}
	}
	if (true == noSnapshotExists) {
		CLM_ERR_TRACE(J9NLS_SHRC_CLCM_SNAPSHOT_NOT_EXIST);
	}
done:
	Trc_SHR_CLM_j9shr_destroy_snapshot_Exit(returnVal);
	return returnVal;
}


/**
 * This method will stat to see if a named cache exists.
 *
 * @param [in] vm  A Java VM
 * @param [in] cacheDirName The directory where the cache is situated
 * @param [in] verboseFlags  Flags controlling the level of verbose messages issued
 * @param [in] name  The name of the cache to stat
 * @param [in] versionData  The version data of the cache to stat
 * @param [in] generation  The generation of cache to stat
 *
 * @return 1 if the cache exists, 0 otherwise
 */	
IDATA
j9shr_stat_cache(struct J9JavaVM* vm, const char* cacheDirName, UDATA verboseFlags, const char* name, J9PortShcVersion* versionData, UDATA generation)
{
	char nameWithVGen[CACHE_ROOT_MAXLEN];
	IDATA result;

	PORT_ACCESS_FROM_JAVAVM(vm);
	
	Trc_SHR_CLM_j9shr_stat_cache_Entry(verboseFlags, name, generation);

	SH_OSCache::getCacheVersionAndGen(PORTLIB, vm, nameWithVGen, CACHE_ROOT_MAXLEN, name, versionData, generation, true);
	result = SH_OSCache::statCache(PORTLIB, cacheDirName, nameWithVGen, (verboseFlags != 0));
	
	Trc_SHR_CLM_j9shr_stat_cache_Exit(result);
	return result;
}

/**
 * This method goes through all incompatible caches of a given name and informs the user that 
 * the utility specified cannot apply to the incompatible caches.
 * 
 * @param [in] vm  A Java VM
 * @param [in] ctrlDirName  root of the shared class cache directory.
 * @param [in] verboseFlags  Flags controlling the level of verbose messages issued
 * @param [in] name  The name of the caches to search for
 * @param [in] utility  The name of the utility being used
 * 
 * @returns the number of caches reported 
 */
IDATA
j9shr_report_utility_incompatible(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, UDATA verboseFlags, const char* name, const char* utility)
{
	J9Pool* list;
	PORT_ACCESS_FROM_JAVAVM(vm);
	IDATA rc = 0;
	
	Trc_SHR_CLM_j9shr_report_utility_incompatible_Entry(verboseFlags, name, utility);
	
	list = findIncompatibleCachesForName(vm, ctrlDirName, groupPerm, name);
	if (list != NULL) {
		if (pool_numElements(list) > 0) {
			pool_state state;
			SH_OSCache_Info* info;
			
			info = (SH_OSCache_Info*)pool_startDo(list, &state);
			do {
				if (strcmp(info->name, name)==0) {
					++rc;
					CLM_TRACE2(J9NLS_SHRC_CLCM_NOT_DONE_FOR_CACHE, utility, name);
				}
			} while ((info = (SH_OSCache_Info*)pool_nextDo(&state)));
		}
		pool_kill(list);
	}
	
	Trc_SHR_CLM_j9shr_report_utility_incompatible_Exit(rc);
	return rc;
}
