/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

#ifndef zip_internal_h
#define zip_internal_h

/**
* @file zip_internal.h
* @brief Internal prototypes used within the ZIP module.
*
* This file contains implementation-private function prototypes and
* type definitions for the ZIP module.
*
*/

#include "j9comp.h"
#include "zip_api.h"
#include "vmzipcachehook_internal.h"

/* -- Necessary for the J9ZipCachePool definition -- */
#if defined(J9VM_THR_PREEMPTIVE)
#include "omrmutex.h"
#else  /* not multithreaded */
typedef int MUTEX;
#define MUTEX_INIT(mutex)					(TRUE)
#define MUTEX_ENTER(mutex)				/* nothing */
#define MUTEX_EXIT(mutex)					/* nothing */
#define MUTEX_DESTROY(mutex)			/* nothing */
#endif

#ifdef __cplusplus
extern "C" {
#endif

	
#define ZIP_WORK_BUFFER_SIZE 64000


/* ---------------- zcpool.c ---------------- */

/**
* @typedef
* @struct
*/
typedef struct J9ZipCachePoolEntry  {
	J9ZipCache *cache;
	UDATA referenceCount;
} J9ZipCachePoolEntry;

/* No typedef because an opaque typedef appears in zipsup.h (already included) */
/**
* @struct
*/
struct J9ZipCachePool  {
	J9Pool *pool;
	J9ZipCache *desiredCache;
	I_64 zipTimeStamp;
	char const *zipFileName;
	IDATA zipFileNameLength;
	IDATA zipFileSize;
	MUTEX mutex;
	IDATA hookInitRC;
	void* userData;
	struct J9VMZipCachePoolHookInterface hookInterface;
	BOOLEAN allocateWorkBuffer;
	UDATA *workBuffer;
};


/**
* @brief
* @param *entry
* @param *zcp
* @return void
*/
void zipCachePool_doFindHandler(J9ZipCachePoolEntry *entry, J9ZipCachePool *zcp);


/**
* @brief
* @param *entry
* @param *zcp
* @return void
*/
void zipCachePool_doKillHandler(J9ZipCachePoolEntry *entry, J9ZipCachePool *zcp);

/* ---------------- zipcache.c ---------------- */

#if (defined(J9VM_OPT_ZIP_SUPPORT))  /* File Level Build Flags */

/* This should be a multiple of the page size, minus a few UDATAs in case
   the OS allocator needs header space (so we don't waste a page).
   If the OS provides a fine-grain allocator (e.g. Windows) then it doesn't really
   matter if we don't fit in one page, but the KISS principle applies.. */
#define ACTUAL_CHUNK_SIZE	(4096 - 4*sizeof(UDATA) )

/**
* @typedef
* @struct
*/
/* trick: a J9ZipCache * is really a pointer to a J9ZipCacheInternal. */
typedef struct J9ZipCacheInternal {
	J9ZipCache info;
	J9ZipCacheEntry *entry;
	IDATA zipFileFd;
	U_8 zipFileType;
} J9ZipCacheInternal;

/**
* @typedef
* @struct
*/
typedef struct J9ZipCacheTraversal {
	J9ZipCache *zipCache;
	J9PortLibrary *portLib;
	J9ZipDirEntry *dirEntry;
	J9ZipFileRecord *fileRecord;
	UDATA fileRecordPos;
	J9ZipFileEntry *fileRecordEntry;
} J9ZipCacheTraversal;

/**
* @brief
* @param zipCache
* @param zipTimeStamp
* @param zipFileSize
* @param zipFileName
* @param zipFileNameLength
* @return BOOLEAN
*/
BOOLEAN 
zipCache_isSameZipFile(J9ZipCache * zipCache, IDATA zipTimeStamp, IDATA zipFileSize, const char *zipFileName, IDATA zipFileNameLength);


/**
* @brief
* @param zipCache
* @return IDATA
*/
IDATA 
zipCache_getStartCentralDir(J9ZipCache * zipCache);


/**
* @brief
* @param zipCache
* @return void
*/
void 
zipCache_invalidateCache(J9ZipCache * zipCache);


#endif /* J9VM_OPT_ZIP_SUPPORT */ /* End File Level Build Flags */


/* ---------------- zipsup.c ---------------- */

#if (defined(J9VM_OPT_ZIP_SUPPORT))  /* File Level Build Flags */


#if (defined(J9VM_OPT_ZLIB_SUPPORT)) 
/**
* @brief
* @param portLib
* @param dir
* @return I_32
*/
I_32 
initZipLibrary(J9PortLibrary* portLib, char* dir);
#endif /* J9VM_OPT_ZLIB_SUPPORT */


/**
* @brief
* @param portLib
* @param *zipFile
* @param endEntry
* @return I_32
*/
I_32 
scanForCentralEnd(J9PortLibrary* portLib, J9ZipFile *zipFile, J9ZipCentralEnd* endEntry);


/**
* @brief
* @param portLib
* @param *zipFile
* @param zipEntry
* @return I_32
*/
I_32 
scanForDataDescriptor(J9PortLibrary* portLib, J9ZipFile *zipFile, J9ZipEntry* zipEntry);


/**
* @brief
* @param opaque
* @param items
* @param size
* @return void*
*/
void* 
zdataalloc(void* opaque, U_32 items, U_32 size);


/**
* @brief
* @param opaque
* @param address
* @return void
*/
void 
zdatafree(void* opaque, void* address);


#endif /* J9VM_OPT_ZIP_SUPPORT */ /* End File Level Build Flags */


#ifdef __cplusplus
}
#endif

#endif /* zip_internal_h */
