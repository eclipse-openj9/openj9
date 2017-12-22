/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#ifndef zip_api_h
#define zip_api_h

/**
* @file zip_api.h
* @brief Public API for the ZIP module.
*
* This file contains public function prototypes and
* type definitions for the ZIP module.
*
*/

#include "j9cfg.h"
#include "j9comp.h"
#include "pool_api.h"
#include "j9port.h"
#include "zipsup.h"
#include "omrhookable.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Empty set of options */
#define J9ZIP_OPEN_NO_FLAGS 0

/* Read the central directory and cache the information. */
#define J9ZIP_OPEN_READ_CACHE_DATA 1

/* Accept the file as a zip file even if it does not start with a local header */
#define J9ZIP_OPEN_ALLOW_NONSTANDARD_ZIP 2

/* Empty set of options */
#define J9ZIP_GETENTRY_NO_FLAGS 0

/* Match a directory even if filename does not end in '/'.  Supported (for the JCL) only when there is a cache. */
#define J9ZIP_GETENTRY_FIND_DIRECTORY 1

/* Do extra work to read the data pointer. If false the dataPointer will be 0. */
#define J9ZIP_GETENTRY_READ_DATA_POINTER 2

/* Look for the entry using the central directory without a cache.  This handles non-standard ZIP files. */
#define J9ZIP_GETENTRY_USE_CENTRAL_DIRECTORY 4

typedef struct J9ZipFunctionTable {
	void (*zip_freeZipComment)(J9PortLibrary * portLib, U_8 * commentString);
	void (*zip_freeZipEntry)(J9PortLibrary * portLib, J9ZipEntry * entry);
	I_32 (*zip_getNextZipEntry)(J9PortLibrary* portLib, J9ZipFile* zipFile, J9ZipEntry* zipEntry, IDATA* nextEntryPointer, BOOLEAN readDataPointer);
	I_32 (*zip_getZipComment)(J9PortLibrary* portLib, J9ZipFile *zipFile, U_8 ** commentString, UDATA * commentLength);
	I_32 (*zip_getZipEntry)(J9PortLibrary *portLib, J9ZipFile *zipFile, J9ZipEntry *entry, const char *filename, IDATA fileNameLength, U_32 flags);
	I_32 (*zip_getZipEntryComment)(J9PortLibrary * portLib, J9ZipFile * zipFile, J9ZipEntry * entry, U_8 * buffer, U_32 bufferSize);
	I_32 (*zip_getZipEntryData)(J9PortLibrary* portLib, J9ZipFile* zipFile, J9ZipEntry* entry, U_8* buffer, U_32 bufferSize);
	I_32 (*zip_getZipEntryExtraField)(J9PortLibrary* portLib, J9ZipFile* zipFile, J9ZipEntry* entry, U_8* buffer, U_32 bufferSize);
	I_32 (*zip_getZipEntryFromOffset)(J9PortLibrary * portLib, J9ZipFile * zipFile, J9ZipEntry * entry, IDATA offset, BOOLEAN readDataPointer);
	I_32 (*zip_getZipEntryRawData)(J9PortLibrary* portLib, J9ZipFile* zipFile, J9ZipEntry* entry, U_8* buffer, U_32 bufferSize, U_32 offset);
	void (*zip_initZipEntry)(J9PortLibrary* portLib, J9ZipEntry* entry);
	I_32 (*zip_openZipFile)(J9PortLibrary *portLib, char *filename, J9ZipFile *zipFile, J9ZipCachePool *cachePool, U_32 flags);
	I_32 (*zip_releaseZipFile)(J9PortLibrary* portLib, struct J9ZipFile* zipFile);
	void (*zip_resetZipFile)(J9PortLibrary* portLib, J9ZipFile* zipFile, IDATA *nextEntryPointer);
} J9ZipFunctionTable;

/* ---------------- zcpool.c ---------------- */

/**
* @brief
* @param *zcp
* @param *zipCache
* @return BOOLEAN
*/
BOOLEAN zipCachePool_addCache(J9ZipCachePool *zcp, J9ZipCache *zipCache);


/**
* @brief
* @param *zcp
* @param *zipCache
* @return BOOLEAN
*/
BOOLEAN zipCachePool_addRef(J9ZipCachePool *zcp, J9ZipCache *zipCache);


/**
* @brief
* @param *zcp
* @param *zipFileName
* @param zipFileNameLength
* @param zipFileSize
* @param zipTimeStamp
* @return J9ZipCache *
*/
J9ZipCache * zipCachePool_findCache(J9ZipCachePool *zcp, char const *zipFileName, IDATA zipFileNameLength, IDATA zipFileSize, I_64 zipTimeStamp);


/**
* @brief
* @param *zcp
* @return void
*/
void zipCachePool_kill(J9ZipCachePool *zcp);

/**
 * Get the list of core Zipfile functions.  Initializes the zip library if necessary.
 * @param portLib port library
 * @param libDir directory containing the zlib
 * @return table of functions or null if the zip library cannot be initialized.
 */
J9ZipFunctionTable *getZipFunctions(J9PortLibrary *portLib, void *libDir);

/**
* @brief
* @param portLib
* @return J9ZipCachePool *
*/
J9ZipCachePool *zipCachePool_new(J9PortLibrary * portLib, void * userData);


/**
* @brief
* @param *zcp
* @param *zipCache
* @return BOOLEAN
*/
BOOLEAN zipCachePool_release(J9ZipCachePool *zcp, J9ZipCache *zipCache);

/* ---------------- zipalloc.c ---------------- */

#if (defined(J9VM_OPT_ZLIB_SUPPORT)) 
/**
* @brief
* @param opaque
* @param items
* @param size
* @return void*
*/
void* zalloc(void* opaque, U_32 items, U_32 size);
#endif /* J9VM_OPT_ZLIB_SUPPORT */


#if (defined(J9VM_OPT_ZLIB_SUPPORT)) 
/**
* @brief
* @param opaque
* @param address
* @return void
*/
void zfree(void* opaque, void* address);
#endif /* J9VM_OPT_ZLIB_SUPPORT */

/* ---------------- zipcache.c ---------------- */

/**
* @brief
* @param zipCache
* @param *elementName
* @param elementNameLength
* @param elementOffset
* @return BOOLEAN
*/
BOOLEAN 
zipCache_addElement(J9ZipCache * zipCache, char *elementName, IDATA elementNameLength, UDATA elementOffset);


/**
* @brief
* @param *handle
* @param *nameBuf
* @param nameBufSize
* @param offset
* @return IDATA
*/
IDATA 
zipCache_enumElement(void *handle, char *nameBuf, UDATA nameBufSize, UDATA * offset);


/**
* @brief
* @param *handle
* @param *nameBuf
* @param nameBufSize
* @return IDATA
*/
IDATA 
zipCache_enumGetDirName(void *handle, char *nameBuf, UDATA nameBufSize);


/**
* @brief
* @param *handle
* @return void
*/
void 
zipCache_enumKill(void *handle);


/**
* @brief
* @param zipCache
* @param *directoryName
* @param **handle
* @return IDATA
*/
IDATA 
zipCache_enumNew(J9ZipCache * zipCache, char *directoryName, void **handle);


/**
* @brief
* @param zipCache
* @param *elementName
* @param elementNameLength
* @param searchDirList
* @return UDATA
*/
UDATA 
zipCache_findElement(J9ZipCache * zipCache, const char *elementName, IDATA elementNameLength, BOOLEAN searchDirList);


/**
* @brief
* @param zipCache
* @return void
*/
void 
zipCache_kill(J9ZipCache * zipCache);


/**
* @brief
* @param portLib
* @param zipName
* @param zipNameLength
* @param zipFileSize
* @param zipTimeStamp
* @return J9ZipCache *
*/
J9ZipCache *
zipCache_new(J9PortLibrary * portLib, char *zipName, IDATA zipNameLength, IDATA zipFileSize, I_64 zipTimeStamp);


/**
* @brief
* @param zipCache
* @param startCentralDir
*/
void zipCache_setStartCentralDir(J9ZipCache * zipCache, IDATA startCentralDir);


/**
* @brief
* @param zipCache
* @return BOOLEAN
*/
BOOLEAN
zipCache_hasData(J9ZipCache * zipCache);


#if defined(J9VM_OPT_SHARED_CLASSES)
/**
* @brief
* @param zipCache
* @return UDATA
*/
UDATA
zipCache_cacheSize(J9ZipCache * zipCache);
#endif


#if defined(J9VM_OPT_SHARED_CLASSES)
/**
* @brief
* @param zipCache
* @param cacheData
* @return BOOLEAN
*/
BOOLEAN
zipCache_copy(J9ZipCache * zipCache, void *cacheData, UDATA dataSize);
#endif


#if defined(J9VM_OPT_SHARED_CLASSES)
/**
* @brief
* @param zipCache
* @param cacheData
* @return void
*/
void
zipCache_useCopiedCache(J9ZipCache * zipCache, void *cacheData);
#endif


#if defined(J9VM_OPT_SHARED_CLASSES)
/**
* @brief
* @param zipCache
* @return BOOLEAN
*/
BOOLEAN
zipCache_isCopied(J9ZipCache * zipCache);
#endif


#if defined(J9VM_OPT_SHARED_CLASSES)
/**
* @brief
* @param zipCache
* @return const char *
*/
const char *
zipCache_uniqueId(J9ZipCache * zipCache);
#endif

struct J9ZipFile;
/**
* @brief
* @param portLib
* @param zipFile
* @return I_32
*/
I_32 
zip_releaseZipFile(J9PortLibrary* portLib, struct J9ZipFile* zipFile);


/**
* @brief
* @param portLib
* @param dir
* @return I_32
*/
I_32 initZipLibrary (J9PortLibrary* portLib, char *dir);

/**
* @brief
* @param portLib
* @param filename
* @param cachePool
* @param cache
* @return I_32
*/
I_32
zip_searchCache(J9PortLibrary * portLib, char *filename, J9ZipCachePool *cachePool, J9ZipCache **cache);

/**
* @brief
* @param portLib
* @param zipFile
* @param cache
* @param cachePool
* @return I_32
*/
I_32 
zip_setupCache(J9PortLibrary * portLib, J9ZipFile *zipFile, J9ZipCache *cache, J9ZipCachePool *cachePool);


/**
* @brief
* @param portLib
* @param zipFile
* @return I_32
*/
I_32
zip_readCacheData(J9PortLibrary * portLib, J9ZipFile *zipFile);


/**
* @brief
* @param portLib
* @param entry
* @return void
*/
void 
zip_freeZipEntry(J9PortLibrary * portLib, J9ZipEntry * entry);


/**
* @brief
* @param portLib
* @param zipFile
* @param zipEntry
* @param nextEntryPointer
* @param readDataPointer
* @return I_32
*/
I_32 
zip_getNextZipEntry(J9PortLibrary* portLib, J9ZipFile* zipFile, J9ZipEntry* zipEntry, IDATA* nextEntryPointer, BOOLEAN readDataPointer);


/**
* @brief
* @param portLib
* @param zipFile
* @param entry
* @param *filename
* @param filenameLength
* @param flags
* @return I_32
*/
I_32 
zip_getZipEntry(J9PortLibrary *portLib, J9ZipFile *zipFile, J9ZipEntry *entry, const char *filename, IDATA filenameLength, U_32 flags);


/**
* @brief
* @param portLib
* @param zipFile
* @param entry
* @param buffer
* @param bufferSize
* @return I_32
*/
I_32 
zip_getZipEntryComment(J9PortLibrary * portLib, J9ZipFile * zipFile, J9ZipEntry * entry, U_8 * buffer,
							U_32 bufferSize);

/* @brief
 * @param[in] portLib
 * @param[in] zipFile
 * @param[in/out]pointer to commentString
 * @param[out] pointer to commentLength
 * @return I32
*/
I_32
zip_getZipComment(J9PortLibrary * portLib, J9ZipFile * zipFile, U_8 ** commentString, UDATA * commentLength);


/*
 * @brief
 * @param[in] portLib
 * @param[in] commentString
 *
*/
void
zip_freeZipComment(J9PortLibrary * portLib, U_8 * commentString);

/**
* @brief
* @param portLib
* @param zipFile
* @param entry
* @param buffer
* @param bufferSize
* @return I_32
*/
I_32 
zip_getZipEntryData(J9PortLibrary* portLib, J9ZipFile* zipFile, J9ZipEntry* entry, U_8* buffer, U_32 bufferSize);


/**
* @brief
* @param portLib
* @param zipFile
* @param entry
* @param buffer
* @param bufferSize
* @param offset
* @return I_32
*/
I_32 
zip_getZipEntryRawData(J9PortLibrary* portLib, J9ZipFile* zipFile, J9ZipEntry* entry, U_8* buffer, U_32 bufferSize, U_32 offset);


/**
* @brief
* @param portLib
* @param zipFile
* @param entry
* @param buffer
* @param bufferSize
* @return I_32
*/
I_32 
zip_getZipEntryExtraField(J9PortLibrary* portLib, J9ZipFile* zipFile, J9ZipEntry* entry, U_8* buffer, U_32 bufferSize);


/**
* @brief
* @param portLib
* @param zipFile
* @param entry
* @param offset
* @param readDataPointer
* @return I_32
*/
I_32 
zip_getZipEntryFromOffset(J9PortLibrary * portLib, J9ZipFile * zipFile, J9ZipEntry * entry, IDATA offset, BOOLEAN readDataPointer);


/**
* @brief
* @param portLib
* @param entry
* @return void
*/
void 
zip_initZipEntry(J9PortLibrary* portLib, J9ZipEntry* entry);


/**
* @brief
* @param portLib
* @param *cachePool
* @return UDATA
*/
J9HookInterface**
zip_getVMZipCachePoolHookInterface(J9ZipCachePool *cachePool);

/**
* @brief
* @param javaVM
* @param *cachePool
* @return void
*/
IDATA 
zip_initZipCachePoolHookInterface(J9PortLibrary* portLib, J9ZipCachePool *cachePool);

/**
* @brief
* @param *cachePool
* @return void
*/
void 
zip_shutdownZipCachePoolHookInterface(J9ZipCachePool *cachePool);

/**
* @brief
* @param portLib
* @param filename
* @param zipFile
* @param cachePool
* @param flags
* @return I_32
*/
I_32
zip_openZipFile(J9PortLibrary *portLib, char *filename, J9ZipFile *zipFile, J9ZipCachePool *cachePool, U_32 flags);


/**
* @brief
* @param portLib
* @param zipFile
* @param *nextEntryPointer
* @return void
*/
void 
zip_resetZipFile(J9PortLibrary* portLib, J9ZipFile* zipFile, IDATA *nextEntryPointer);


void
zip_triggerZipLoadHook (J9PortLibrary* portLib, const char* filename, UDATA newState, J9ZipCachePool *cachePool);

#ifdef __cplusplus
}
#endif

#endif /* zip_api_h */
