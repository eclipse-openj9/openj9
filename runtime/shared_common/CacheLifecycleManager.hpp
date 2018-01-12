/*******************************************************************************
 * Copyright (c) 2001, 2017 IBM Corp. and others
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

#if !defined(CACHELIFECYCLEMANAGER_HPP_INCLUDE)
#define CACHELIFECYCLEMANAGER_HPP_INCLUDE

/* @ddr_namespace: default */
#include <j9port.h>
#include "ibmjvmti.h"

/*
 * Macros used by j9shr_destroy_cache to specify cache status.
 * Except J9SH_DESTROYED_OLDER_GEN_CACHE, rest are also used to specify return value for j9shr_destroy_cache.
 */
#define J9SH_DESTROYED_ALL_CACHE				0
#define J9SH_DESTROYED_OLDER_GEN_CACHE			1
#define J9SH_DESTROYED_NONE						-1
#define J9SH_DESTROY_FAILED_CURRENT_GEN_CACHE	-2
#define J9SH_DESTROY_FAILED_OLDER_GEN_CACHE		-3

#define J9SH_DESTROYED_ALL_SNAPSHOT					J9SH_DESTROYED_ALL_CACHE
#define J9SH_DESTROYED_OLDER_GEN_SNAPSHOT			J9SH_DESTROYED_OLDER_GEN_CACHE
#define J9SH_DESTROY_FAILED_CURRENT_GEN_SNAPSHOT 	J9SH_DESTROY_FAILED_CURRENT_GEN_CACHE
#define J9SH_DESTROY_FAILED_OLDER_GEN_SNAPSHOT		J9SH_DESTROY_FAILED_OLDER_GEN_CACHE

#ifdef __cplusplus
extern "C" {
#endif
	
IDATA j9shr_iterateSharedCaches(J9JavaVM *vm, const char *ctrlDirName, UDATA groupPerm, BOOLEAN useCommandLineValue, IDATA (*callback)(J9JavaVM *vm, J9SharedCacheInfo *event_data, void *user_data), void *user_data);
IDATA j9shr_destroySharedCache(J9JavaVM *vm, const char *ctrlDirName, const char *name, U_32 cacheType, BOOLEAN useCommandLineValue);

IDATA j9shr_destroy_cache(struct J9JavaVM* vm, const char* ctrlDirName, UDATA verboseFlags, const char* cacheName, UDATA generationStart, UDATA generationEnd, J9PortShcVersion* versionData, BOOLEAN isReset);

IDATA j9shr_destroy_snapshot(struct J9JavaVM* vm, const char* ctrlDirName, UDATA verboseFlags, const char* cacheName, UDATA generationStart, UDATA generationEnd, J9PortShcVersion* versionData);

IDATA j9shr_destroy_all_snapshot(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, UDATA verboseFlags);

IDATA j9shr_list_caches(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, UDATA verboseFlags);

IDATA j9shr_destroy_all_cache(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, UDATA verboseFlags);

IDATA j9shr_destroy_expire_cache(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, UDATA verboseFlags, UDATA minutes);

IDATA j9shr_stat_cache(struct J9JavaVM* vm, const char* cacheDirName, UDATA verboseFlags, const char* name, J9PortShcVersion* versionData, UDATA generation);

IDATA j9shr_report_utility_incompatible(struct J9JavaVM* vm, const char* ctrlDirName, UDATA groupPerm, UDATA verboseFlags, const char* name, const char* utility);

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !defined(CACHELIFECYCLEMANAGER_HPP_INCLUDE) */
