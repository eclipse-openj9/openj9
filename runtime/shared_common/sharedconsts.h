/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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
#ifndef SHAREDCONSTS_H_
#define SHAREDCONSTS_H_

/* @ddr_namespace: default */
#include "shcdatatypes.h"


#define PATHSEP DIR_SEPARATOR_STR

#define CACHE_ROOT_PREFIX "sharedcc"

/*
 * For Windows,  cacheNameWithVersionString
 * = version string + prefix separator char + cacheName + prefix separator char + generation string
 *
 * For Unix,  cacheNameWithVersionString
 * = version string + identifier string + cacheName + prefix separator char + generation string
 *
 * Pick the unix version since it has the maximum possible length for cacheNameWithVersionString
 * compared to windows version for the same cacheName.
 *
 * length of version string =>  11 chars
 * identifier string is "_memory_" length => 8 chars
 * maximum length of user specified or default cacheName  => 64 chars
 * prefix separator char is _     => 1 char
 * generation string is of form Gnn where nn stands for 2 digit number => 3 chars
 * layer number string is of form Lnn where nn stands for 2 digit number => 3 chars
 *
 * Total chars needed  = 11 + 8 + 64 + 1 + 3 + 3 = 90 chars.
 *
 * Add 1 to above length to accommodate NULL char.
 * Reference:  getCacheVersionAndGen function
 */
#define CACHE_ROOT_MAXLEN 91

#define SHR_SUBOPT_BUFLEN (2 * J9SH_MAXPATH)

#define PROTO_JAR 1
#define PROTO_DIR 2
#define PROTO_JIMAGE 4
#define PROTO_TOKEN 8
#define PROTO_UNKNOWN 16

#define IS_IN_CACHE_FLAG 0x100
#define MARKED_STALE_FLAG 0x200
#define FREE_PATH_IN_CLEANUP 0x400
#define HELPERID_NOT_APPLICABLE -1

#define TIMESTAMP_DOES_NOT_EXIST -1
#define TIMESTAMP_UNCHANGED 0
#define TIMESTAMP_DISAPPEARED -2

#define MANAGER_STATE_INITIALIZED 1
#define MANAGER_STATE_STARTED 2
#define MANAGER_STATE_STARTING 3
#define MANAGER_STATE_SHUTDOWN 4

/* Public Filters */
#define PRINTSTATS_SHOW_NONE 0x0
#define PRINTSTATS_SHOW_ALL (PRINTSTATS_SHOW_CLASSPATH|PRINTSTATS_SHOW_URL|PRINTSTATS_SHOW_TOKEN|PRINTSTATS_SHOW_ROMCLASS|PRINTSTATS_SHOW_ROMMETHOD|PRINTSTATS_SHOW_AOT|PRINTSTATS_SHOW_JITPROFILE|PRINTSTATS_SHOW_JITHINT|PRINTSTATS_SHOW_ZIPCACHE|PRINTSTATS_SHOW_INVALIDATEDAOT|PRINTSTATS_SHOW_ALL_STALE|PRINTSTATS_SHOW_STARTUPHINT|PRINTSTATS_SHOW_CRVSNIPPET)
#define PRINTSTATS_SHOW_CLASSPATH 0x1
#define PRINTSTATS_SHOW_URL 0x2
#define PRINTSTATS_SHOW_TOKEN 0x4
#define PRINTSTATS_SHOW_ROMCLASS 0x8
#define PRINTSTATS_SHOW_ROMMETHOD 0x10
#define PRINTSTATS_SHOW_AOT  0x20
#define PRINTSTATS_SHOW_JITPROFILE  0x40
#define PRINTSTATS_SHOW_ZIPCACHE  0x80
#define PRINTSTATS_SHOW_JITHINT  0x100
#define PRINTSTATS_SHOW_INVALIDATEDAOT 0x20000
#define PRINTSTATS_SHOW_ALL_STALE 0x40000
#define PRINTSTATS_SHOW_STARTUPHINT 0x80000
#define PRINTSTATS_SHOW_TOP_LAYER_ONLY 0x100000
#define PRINTSTATS_SHOW_CRVSNIPPET 0x200000

/* Private filters */
#define PRINTSTATS_SHOW_EXTRA (PRINTSTATS_SHOW_ALL|PRINTSTATS_SHOW_ORPHAN|PRINTSTATS_SHOW_AOTCH|PRINTSTATS_SHOW_AOTTHUNK|PRINTSTATS_SHOW_AOTDATA|PRINTSTATS_SHOW_JCL|PRINTSTATS_SHOW_BYTEDATA)
#define PRINTSTATS_SHOW_ORPHAN  0x200
#define PRINTSTATS_SHOW_AOTCH  0x400
#define PRINTSTATS_SHOW_AOTTHUNK  0x800
#define PRINTSTATS_SHOW_AOTDATA  0x1000
#define PRINTSTATS_SHOW_JCL  0x2000
#define PRINTSTATS_SHOW_BYTEDATA  0x4000

/* Helpers */
#define PRINTSTATS_SHOW_HELP  0x8000
#define PRINTSTATS_SHOW_MOREHELP  0x10000

#define J9OSCACHE_DATA_WRITE_LOCK 1
#define J9OSCACHE_DATA_READ_LOCK 2

#define J9OSCACHE_OPEN_MODE_DO_READONLY 0x1
#define J9OSCACHE_OPEN_MODE_TRY_READONLY_ON_FAIL 0x2
#define J9OSCACHE_OPEN_MODE_GROUPACCESS 0x8
#define J9OSCACHE_OPEN_MODE_CHECKBUILDID 0x10
#define J9OSCACHE_OPEN_MODE_CHECK_NETWORK_CACHE 0x20

#define SHARE_PATHBUF_SIZE 512
#define SHARE_CLASS_FILE_EXT_STRING ".class"
#define SHARE_CLASS_FILE_EXT_STRLEN 6

#define MONITOR_ENTER_RETRY_TIMES 10

#define J9SHR_CLASS_ARRAY_CHAR_OBJECT_DATA_KEY "j9shrClassArrayCharObjectDataKey"

#define J9SHR_STRING_POOL_OK 0
#define J9SHR_STRING_POOL_FAILED_VERIFY 1
#define J9SHR_STRING_POOL_FAILED_CONSISTENCY 2

/* Codes to indicate cause of corruption */
#define NO_CORRUPTION										0
#define CACHE_CRC_INVALID									-1
#define ROMCLASS_CORRUPT									-2
#define ITEM_TYPE_CORRUPT									-3
#define ITEM_LENGTH_CORRUPT									-4
#define CACHE_HEADER_INCORRECT_DATA_LENGTH					-6
#define CACHE_HEADER_INCORRECT_DATA_START_ADDRESS			-7
#define CACHE_HEADER_BAD_EYECATCHER							-8
#define CACHE_HEADER_INCORRECT_CACHE_SIZE					-9
#define ACQUIRE_HEADER_WRITE_LOCK_FAILED 					-10
#define CACHE_SIZE_INVALID 									-11
#define CACHE_DEBUGAREA_BAD_SIZES_FOR_COMMIT				-12
#define CACHE_DEBUGAREA_BAD_FREE_SPACE						-13
#define CACHE_DEBUGAREA_BAD_LNT_HEADER_INFO					-14
#define CACHE_DEBUGAREA_BAD_LVT_HEADER_INFO					-15
#define CACHE_DATA_NULL										-16
#define CACHE_DEBUGAREA_BAD_FREE_SPACE_SIZE  				-17
#define CACHE_DEBUGAREA_BAD_SIZE  							-18
#define CACHE_SEMAPHORE_MISMATCH							-19
#define CACHE_BAD_CC_INIT									-20

/* constants used when calling SH_OSCache::getAllCacheStatistics */
#define SHR_STATS_REASON_TEST 		1
#define SHR_STATS_REASON_ITERATE 	1
#define SHR_STATS_REASON_LIST 		2
#define SHR_STATS_REASON_DESTROY 	3
#define SHR_STATS_REASON_GETNAME 	4
#define SHR_STATS_REASON_EXPIRE 	5

/* constants used when calling SH_OSCache::startup */
#define SHR_STARTUP_REASON_NORMAL 	0
#define SHR_STARTUP_REASON_DESTROY 	1
#define SHR_STARTUP_REASON_EXPIRE 	2

/* constants used when invalidating/revalidating/finding AOT methods */
#define SHR_METHOD_SPEC_TABLE_MAX_SIZE 64
#define SHR_FIND_AOT_METHOTHODS 0
#define SHR_INVALIDATE_AOT_METHOTHODS 1
#define SHR_REVALIDATE_AOT_METHOTHODS 2

#define J9SH_CACHE_FILE_MODE_USERDIR_WITH_GROUPACCESS		0664
#define J9SH_CACHE_FILE_MODE_USERDIR_WITHOUT_GROUPACCESS	0644
#define J9SH_CACHE_FILE_MODE_DEFAULTDIR_WITH_GROUPACCESS	0660
#define J9SH_CACHE_FILE_MODE_DEFAULTDIR_WITHOUT_GROUPACCESS	0600

/**
 * This enum contains constants that to indicate if the cache is accessible by current user or not.
 * It is used when printing cache stats.
 */
typedef enum SH_CacheAccess {
	J9SH_CACHE_ACCESS_ALLOWED 				= 0,
	J9SH_CACHE_ACCESS_ALLOWED_WITH_GROUPACCESS,
	J9SH_CACHE_ACCESS_ALLOWED_WITH_GROUPACCESS_READONLY,
	J9SH_CACHE_ACCESS_NOT_ALLOWED
} SH_CacheAccess;

#endif /*SHAREDCONSTS_H_*/
