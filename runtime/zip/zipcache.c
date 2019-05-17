/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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
 * @ingroup ZipSupport
 * @brief Zip Support for Java VM
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "j9.h"
#include "j9port.h"
#include "zipsup.h"
#include "zip_internal.h"
#include "j9memcategories.h"
#include "util_api.h"

#if (defined(J9VM_OPT_ZIP_SUPPORT))  /* File Level Build Flags */

/**
 * Used to identify the zip cache format in shared memory.
 * The zip cache version number must be changed if the zip
 * cache format changes.
 */
#define ZIP_CACHE_VERSION 1

#define UDATA_TOP_BIT    (((UDATA)1)<<(sizeof(UDATA)*8-1))
#define ISCLASS_BIT    UDATA_TOP_BIT
#define NOT_FOUND	((UDATA) (~0))
#define OFFSET_MASK	(~ISCLASS_BIT)
#define	IMPLICIT_ENTRY	(~ISCLASS_BIT)


void zipCache_freeChunk (J9PortLibrary * portLib, J9ZipChunkHeader *chunk);
J9ZipDirEntry *zipCache_searchDirListCaseInsensitive (J9ZipDirEntry * dirEntry, const char *namePtr, UDATA nameSize, BOOLEAN isClass);
J9ZipChunkHeader *zipCache_allocateChunk (J9PortLibrary * portLib);
J9ZipFileEntry *zipCache_addToFileList (J9PortLibrary * portLib, J9ZipCacheEntry * zce, J9ZipDirEntry * dirEntry, const char *namePtr, IDATA nameSize, BOOLEAN isClass, UDATA elementOffset);
static UDATA *zipCache_reserveEntry (J9ZipCacheEntry *zce, J9ZipChunkHeader * chunk, UDATA entryBytes, UDATA stringBytes, char **namePtrOut);
J9ZipFileEntry *zipCache_searchFileList (J9ZipDirEntry * dirEntry, const char *namePtr, UDATA nameSize, BOOLEAN isClass);
J9ZipDirEntry *zipCache_addToDirList (J9PortLibrary * portLib, J9ZipCacheEntry * zce, J9ZipDirEntry * dirEntry,const char *namePtr, IDATA nameSize, BOOLEAN isClass);
J9ZipDirEntry *zipCache_searchDirList (J9ZipDirEntry * dirEntry, const char *namePtr, UDATA nameSize, BOOLEAN isClass);
IDATA helper_memicmp (const void *src1, const void *src2, UDATA length);
J9ZipDirEntry *zipCache_copyDirEntry(J9ZipCacheEntry *orgzce, J9ZipDirEntry *orgDirEntry, J9ZipCacheEntry *zce, J9ZipDirEntry *rootEntry);
void zipCache_freeChunks(J9PortLibrary *portLib, J9ZipCacheEntry *zce);
void zipCache_walkCache(J9PortLibrary * portLib, J9ZipCacheEntry *zce, J9ZipDirEntry *dirEntry);

#define ZIP_SRP_SET(field, value) WSRP_PTR_SET(&field, value)
#define ZIP_SRP_GET(field, type) WSRP_PTR_GET(&field, type)
#define ZIP_SRP_SET_TO_NULL(field) WSRP_SET_TO_NULL(field)

#define J9ZIPDIRENTRY_NAME(entry) ((char*)entry+sizeof(J9ZipDirEntry))
#define J9ZIPFILEENTRY_NAME(entry) ((char*)entry+sizeof(J9ZipFileEntry))
#define J9ZIPFILEENTRY_NEXT(entry) ((J9ZipFileEntry*)((UDATA)entry+sizeof(J9ZipFileEntry)+ALIGN_ENTRY(entry->nameLength)))

#define ALIGN_ENTRY(stringBytes) ((stringBytes + sizeof(UDATA) - 1) & ~(sizeof(UDATA) - 1))

#if 0
#define ZIP_SRP_SET(field, value) field = value
#define ZIP_SRP_GET(field, type) field 
#define ZIP_SRP_SET_TO_NULL(field) field = NULL
#endif

/** 
 * Creates a new, empty zip cache for the provided zip file.
 *
 * @param[in] portLib the port library
 * @param[in] zipName the zip file name
 * @param[in] zipNameLength
 *
 * @return the new zip cache if one was successfully created, NULL otherwise
 *
*/

J9ZipCache *zipCache_new(J9PortLibrary * portLib, char *zipName, IDATA zipNameLength, IDATA zipFileSize, I_64 zipTimeStamp)
{
	J9ZipChunkHeader *chunk;
	J9ZipCacheInternal *zci;
	J9ZipCacheEntry *zce;
	char *zipFileName;

	PORT_ACCESS_FROM_PORT(portLib);

	chunk = zipCache_allocateChunk(portLib);
	if (!chunk)
		return NULL;

	zci = j9mem_allocate_memory(sizeof(J9ZipCacheInternal), J9MEM_CATEGORY_VM_JCL);
	if (!zci) {
		zipCache_freeChunk(portLib, chunk);
		return NULL;
	}

	zce = (J9ZipCacheEntry *) zipCache_reserveEntry(NULL, chunk, sizeof(J9ZipCacheEntry), 0, &zipFileName);
	if (!zce) {
		/* ACTUAL_CHUNK_SIZE is so small it can't hold one J9ZipCacheEntry?? */
		zipCache_freeChunk(portLib, chunk);
		return NULL;
	}
	zci->entry = zce;
	zci->zipFileFd = -1;
	zci->zipFileType = ZIP_Unknown;

	zci->info.portLib = portLib;
	ZIP_SRP_SET(zce->currentChunk, chunk);

	/* Try to put the name string in this chunk.  If it won't fit, we'll allocate it separately */
	if (NULL == zipCache_reserveEntry(zce, chunk, 0, zipNameLength+1, &zipFileName))  {
		zipFileName = j9mem_allocate_memory(zipNameLength+1, J9MEM_CATEGORY_VM_JCL);
		if (!zipFileName)  {
			zipCache_freeChunk(portLib, chunk);
			return NULL;
		}
	}
	ZIP_SRP_SET(zce->zipFileName, zipFileName);
	if(NULL == zipFileName) {
		zipCache_freeChunk(portLib, chunk);
		return NULL;
	}
	memcpy(zipFileName, zipName, zipNameLength);
	zipFileName[zipNameLength] = '\0';
	zce->zipFileSize = zipFileSize;
	zce->zipTimeStamp = zipTimeStamp;
	zce->root.zipFileOffset = 1;
	
	return (J9ZipCache *)zci;
}


/** 
 * Set the startCentralDir for the cache.
 *
 * @param[in] zipCache the zip cache to modify
 * @param[in] startCentralDir the offset to the central dir in the zip file
*/
void zipCache_setStartCentralDir(J9ZipCache * zipCache, IDATA startCentralDir)
{
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *zce = zci->entry;
	zce->startCentralDir = startCentralDir;
}


#if defined(J9VM_OPT_SHARED_CLASSES)
/** 
 * Returns a unique id for the zip cache. Used to identify the zip
 * cache data in shared memory. Consists of the zip file name (but
 * not the path), the zip file timestamp, the zip file size, and a
 * zip cache version number. The zip cache version number must be
 * changed if the zip cache format changes.
 * 
 * The unique id is allocated by j9mem_allocate_memory() and must be
 * freed using j9mem_free_memory().
 *
 * @param[in] zipCache the zip cache
 *
 * @return the unique id C string
 */
const char *zipCache_uniqueId(J9ZipCache * zipCache) {
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *zce = zci->entry;
	J9PortLibrary * portLib = zipCache->portLib;
	PORT_ACCESS_FROM_PORT(portLib);

	UDATA sizeRequired;
	char *buf;
	const char *fileName = ZIP_SRP_GET(zce->zipFileName, const char *);
	UDATA nameLength;
	IDATA i = 0;
	if (!fileName) {
		return NULL;
	}
	nameLength = strlen(fileName);


	for (i = nameLength - 1; i >= 0; i--) {
		if (fileName[i] == '\\' || fileName[i] == '/') {
			fileName = &fileName[i+1];
			break;
		}
	}
	
	sizeRequired = j9str_printf(PORTLIB, NULL, 0, "%s_%d_%lld_%d", fileName, zce->zipFileSize, zce->zipTimeStamp, ZIP_CACHE_VERSION);
	buf = j9mem_allocate_memory(sizeRequired, J9MEM_CATEGORY_VM_JCL);
	if (!buf) {
		return NULL;
	}
	j9str_printf(PORTLIB, buf, sizeRequired, "%s_%d_%lld_%d", fileName, zce->zipFileSize, zce->zipTimeStamp, ZIP_CACHE_VERSION);
	return (const char *)buf;
}
#endif


#if defined(J9VM_OPT_SHARED_CLASSES)
/** 
 * Returns the size required for the zip cache data, in bytes. If the
 * cache data has been copied, zero is returned.
 *
 * @param[in] zipCache the zip cache
 *
 * @return the size of the zip cache in bytes
 */
UDATA zipCache_cacheSize(J9ZipCache * zipCache) {
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *zce = zci->entry;
	UDATA sizeRequired = 0;
	J9ZipChunkHeader *chunk = ZIP_SRP_GET(zce->currentChunk, J9ZipChunkHeader *);

	while (chunk) {
		sizeRequired += ACTUAL_CHUNK_SIZE - (chunk->endFree - chunk->beginFree);
		chunk = ZIP_SRP_GET(chunk->next, J9ZipChunkHeader *);
	}
	if (sizeRequired) {
		/* If the zip cache has already been copied, the currentChunk will be NULL and
		 * the sizeRequired will be zero. */
		U_8 *zipFileName = ZIP_SRP_GET(zce->zipFileName, U_8 *);
		/*If zipFileName is Null, then we should not add the length of zipFileName into sizerequired*/
		if (NULL == zipFileName){
			return sizeRequired;
		}
		chunk = (J9ZipChunkHeader *)(((U_8 *)zce) - sizeof(J9ZipChunkHeader));
		if (((UDATA)(zipFileName - (U_8 *)chunk)) >= ACTUAL_CHUNK_SIZE)   {
			/* HACK!!  zce->info.zipFileName points outside the first chunk, therefore it was allocated
				separately rather than being reserved from the chunk */
			sizeRequired += strlen((const char*)zipFileName) + 1;
		}
	}
	return sizeRequired;
}
#endif

/* remove the #if 0 for testing locally */
#if 0
void zipCache_walkCache(J9PortLibrary * portLib, J9ZipCacheEntry *zce, J9ZipDirEntry *dirEntry) {
	PORT_ACCESS_FROM_PORT(portLib);
	J9ZipFileRecord *record;
	UDATA i,j;

	while (dirEntry) {
		strlen(J9ZIPDIRENTRY_NAME(dirEntry));
		record = ZIP_SRP_GET(dirEntry->fileList, J9ZipFileRecord *);
		while (record) {
			J9ZipFileEntry *fileEntry = record->entry;
			for (i=0; i<record->entryCount; i++) {
				char* name = J9ZIPFILEENTRY_NAME(fileEntry);
				for (j=0; j<fileEntry->nameLength; j++) {
					name[j] = name[j];
				}
				fileEntry = J9ZIPFILEENTRY_NEXT(fileEntry);
			}
			record = ZIP_SRP_GET(record->next, J9ZipFileRecord *);
		}
		if (dirEntry->dirList) {
			zipCache_walkCache(portLib, zce, ZIP_SRP_GET(dirEntry->dirList, J9ZipDirEntry *));
		}
		
		dirEntry = ZIP_SRP_GET(dirEntry->next, J9ZipDirEntry *);
	}
}
#endif


#if defined(J9VM_OPT_SHARED_CLASSES)
/** 
 * Copies the zip cache data into the memory chunk provided. The new cache
 * data isn't used unless zipCache_useCopiedCache is called.
 *
 * @param[in] zipCache the zip cache
 * @param[in] cacheData a chunk of memory of at least zipCache_cacheSize() bytes
 * @parma[in] dataSize the size of the memory chunk. Must be at least zipCache_cacheSize() bytes
 *
 * @return TRUE if the copy was successful (should never fail)
 */
BOOLEAN zipCache_copy(J9ZipCache * zipCache, void *cacheData, UDATA dataSize) {
	J9ZipCacheInternal *orgzci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *orgzce = orgzci->entry;
	J9PortLibrary * portLib = zipCache->portLib;
	J9ZipChunkHeader *chunk;
	J9ZipCacheEntry *zce;
	J9ZipFileRecord *record;
	J9ZipDirEntry *orgDirEntry;
	UDATA i;
	char *copyZipFileName;
	const char *zipFileName = ZIP_SRP_GET(orgzce->zipFileName, const char *);
	UDATA zipNameLength;
	PORT_ACCESS_FROM_PORT(portLib);
	if (!zipFileName) {
		return FALSE;
	}
	zipNameLength = strlen(zipFileName);

	if (dataSize < sizeof(J9ZipChunkHeader)) {
		/* A zip cache cannot be copied twice, as zipCache_cacheSize() will return 0
		 * for a copied cache. */
		return FALSE;
	}

	memset(cacheData, 0, dataSize);
	chunk = cacheData;
	zce = (J9ZipCacheEntry *)(chunk + 1);
	chunk->beginFree = (U_8 *)(zce + 1);
	chunk->endFree = (U_8 *)cacheData + dataSize;

	ZIP_SRP_SET(zce->currentChunk, chunk);

	if ((zipCache_reserveEntry(zce, chunk, 0, zipNameLength + 1, &copyZipFileName)) && (NULL != copyZipFileName))  {
		ZIP_SRP_SET(zce->zipFileName, copyZipFileName);
	} else  {
		return FALSE;
	}
	strcpy(copyZipFileName, zipFileName);
	zce->zipFileSize = orgzce->zipFileSize;
	zce->zipTimeStamp = orgzce->zipTimeStamp;
	zce->startCentralDir = orgzce->startCentralDir;
	zce->root.zipFileOffset = 1;

	orgDirEntry = &orgzce->root;
	/* copy the root file list */
	record = ZIP_SRP_GET(orgDirEntry->fileList, J9ZipFileRecord *);
	if (NULL != record) {
		while (record) {
			J9ZipFileEntry *fileEntry = record->entry;
			for (i=0; i<record->entryCount; i++) {
				if (!zipCache_addToFileList(portLib, zce, &zce->root, J9ZIPFILEENTRY_NAME(fileEntry), fileEntry->nameLength,
						   (fileEntry->zipFileOffset & ISCLASS_BIT) != 0, fileEntry->zipFileOffset & OFFSET_MASK))
				{
					return FALSE;
				}
				fileEntry = J9ZIPFILEENTRY_NEXT(fileEntry);
			}
			record = ZIP_SRP_GET(record->next, J9ZipFileRecord *);
		}
	}

	if (orgDirEntry->dirList && !zipCache_copyDirEntry(orgzce, ZIP_SRP_GET(orgDirEntry->dirList, J9ZipDirEntry *), zce, &zce->root)) {
		return FALSE;
	}

	/* Null the currentChunk so it can't be free'd */
	ZIP_SRP_SET_TO_NULL(zce->currentChunk);

/*	zipCache_walkCache(portLib, zce, &zce->root); */
	return TRUE;
}
#endif


/** 
 * Returns if the cache has data. The cache data is created by zip_readCacheData().
 *
 * @param[in] portLib the port library
 * @param[in] zipCache the zip cache to modify
 * 
 * @return TRUE if the cache has data
 */
BOOLEAN zipCache_hasData(J9ZipCache * zipCache)
{
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *zce = zci->entry;

	return zce->root.fileList || zce->root.dirList;
}


#if defined(J9VM_OPT_SHARED_CLASSES)
/** 
 * Changes the zip cache to use the copy created by zipCache_copy().
 *
 * @param[in] zipCache the zip cache
 * @param[in] cacheData the cache data populated by zipCache_copy()
 */
void zipCache_useCopiedCache(J9ZipCache * zipCache, void *cacheData) {
	J9ZipCacheInternal *orgzci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *orgzce = orgzci->entry;
	J9PortLibrary * portLib = zipCache->portLib;
	J9ZipChunkHeader *chunk;
	J9ZipCacheEntry *zce;

	PORT_ACCESS_FROM_PORT(portLib);

	zipCache_freeChunks(portLib, orgzce);
	chunk = cacheData;
	zce = (J9ZipCacheEntry *)(chunk + 1);
/*	zipCache_walkCache(portLib, zce, &zce->root); */
	orgzci->entry = zce;
}
#endif


#if defined(J9VM_OPT_SHARED_CLASSES)
/** 
 * Returns is the zip cache data has been copied into a contiguous chunk.
 *
 * @param[in] zipCache the zip cache
 * 
 * @returns TRUE is the zip cache data has been copied using zipCache_copy()
 */
BOOLEAN zipCache_isCopied(J9ZipCache * zipCache) {
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *zce = zci->entry;

	return !zce->currentChunk;
}
#endif


/**
 * Return if the specified parameters match this zip cache.
 * 
 * @param[in] zipCache the zip cache
 * @param[in] zipTimeStamp the zip timestamp
 * @param[in] zipFileSize the zip file size
 * @param[in] zipFileName the zip file name
 * @param[in] zipFileNameLength
 * 
 * @return TRUE if the parameters match this zip cache
 */
BOOLEAN zipCache_isSameZipFile(J9ZipCache * zipCache, IDATA zipTimeStamp, IDATA zipFileSize, const char *zipFileName, IDATA zipFileNameLength) {
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *zce = zci->entry;
	const char *localZipFileName;

	if (zce->zipTimeStamp != zipTimeStamp) {
		return FALSE;
	}
	if (zce->zipFileSize != zipFileSize) {
		return FALSE;
	}
	localZipFileName = ZIP_SRP_GET(zce->zipFileName, const char *);
	if ((NULL == localZipFileName) && (0 >= zipFileNameLength)) {
		return FALSE;
	}
	else if (NULL != localZipFileName) {
		if (memcmp(localZipFileName, zipFileName, zipFileNameLength)) {
			return FALSE;
		}
	}
	if (localZipFileName[zipFileNameLength] != '\0') {
		return FALSE;
	}
	
	return TRUE;
}


/**
 * Return the startCentralDir of the cache.
 * 
 * @param[in] zipCache the zip cache
 * 
 * @return the startCentralDir of the cache
 */
IDATA zipCache_getStartCentralDir(J9ZipCache *zipCache) {
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *zce = zci->entry;
	return zce->startCentralDir;
}


/**
 * Whack the cache timestamp to keep other people from starting to use it.  Once all the current
 * users of the cache have stopped using it, it will go away.
 * 
 * @param[in] zipCache the zip cache
 */
void zipCache_invalidateCache(J9ZipCache * zipCache) {
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *zce = zci->entry;
#if defined(J9VM_OPT_SHARED_CLASSES)
	if (zipCache_isCopied(zipCache)) {
		/* Cannot invalid the cache if its read-only in the shared cache */
		return;
	}
#endif
	zce->zipTimeStamp = -2;
}


#if defined(J9VM_OPT_SHARED_CLASSES)
J9ZipDirEntry *zipCache_copyDirEntry(J9ZipCacheEntry *orgzce, J9ZipDirEntry *orgDirEntry, J9ZipCacheEntry *zce, J9ZipDirEntry *rootEntry) {
	J9ZipFileRecord *record;
	J9ZipDirEntry *dirEntry;
	UDATA i;

	if (!orgDirEntry) {
		return NULL;
	}
	while (orgDirEntry) {
		const char *orgName = J9ZIPDIRENTRY_NAME(orgDirEntry);
		if (!(dirEntry = zipCache_addToDirList(NULL, zce, rootEntry, orgName, strlen(orgName), FALSE))) {
			return NULL;
		}
		dirEntry->zipFileOffset = orgDirEntry->zipFileOffset;
		record = ZIP_SRP_GET(orgDirEntry->fileList, J9ZipFileRecord *);
		while (record) {
			J9ZipFileEntry *fileEntry = record->entry;
			for (i=0; i<record->entryCount; i++) {
				if (!(zipCache_addToFileList(NULL, zce, dirEntry, J9ZIPFILEENTRY_NAME(fileEntry), fileEntry->nameLength,
										(fileEntry->zipFileOffset & ISCLASS_BIT) != 0, fileEntry->zipFileOffset & OFFSET_MASK)))
				{
					return NULL;
				}
				fileEntry = J9ZIPFILEENTRY_NEXT(fileEntry);
			}
			record = ZIP_SRP_GET(record->next, J9ZipFileRecord *);
		}
		if (orgDirEntry->dirList && !zipCache_copyDirEntry(orgzce, ZIP_SRP_GET(orgDirEntry->dirList, J9ZipDirEntry *), zce, dirEntry)) {
			return NULL;
		}
		
		orgDirEntry = ZIP_SRP_GET(orgDirEntry->next, J9ZipDirEntry *);
	}
	return rootEntry;
}
#endif


/**
 * Add an association between a file or directory named elementName and offset elementOffset to the zip cache provided
 *
 * @param[in] zipCache the zip cache being added to
 * @param[in] elementName the name of the file or directory element
 * @param[in] elementNameLength length of elementName
 * @param[in] offset the corresponding offset of the element
 *
 * @return TRUE if the association was made, FALSE otherwise
 *
*/

BOOLEAN 
zipCache_addElement(J9ZipCache * zipCache, char *elementName, IDATA elementNameLength, UDATA elementOffset)
{
	J9PortLibrary * portLib = zipCache->portLib;
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *zce = zci->entry;
	J9ZipDirEntry *dirEntry;
	J9ZipFileEntry *fileEntry;
	char *curName;
	IDATA nameLength;
	IDATA curSize;
	IDATA prefixSize;
	BOOLEAN isClass;

	if (!zipCache ||
		(elementNameLength == 0) ||
		((elementName[0] == 0) && (elementNameLength == 1)) ||
		((elementOffset & ISCLASS_BIT) != 0) ||
		((elementOffset & OFFSET_MASK) == IMPLICIT_ENTRY))
		return FALSE;

	dirEntry = &zce->root;

	curName = elementName;
	nameLength = elementNameLength;
	for (;;) {
		J9ZipDirEntry *d;

		/* scan forwards in curName until we find '/' or whole string is exhausted */
		for (curSize = 0; (curSize != nameLength) && (curName[curSize] != '/'); curSize++)
			/* nothing */ ;

		prefixSize = curSize + 1;
		isClass = FALSE;

		if ((curSize >= 6) && !memcmp(&curName[curSize - 6], ".class", 6)) {
			isClass = TRUE;
			curSize -= 6;
		}

		if ((curName-elementName) == elementNameLength) {
			/* We ran out of string, which means the elementName was */
			/* a directory name---in fact, it was the subdir we parsed */
			/* last time through the loop. */

			if ((dirEntry->zipFileOffset & OFFSET_MASK) != IMPLICIT_ENTRY) {
				/* Can't add the same directory more than once! */
				return TRUE;
			}
			dirEntry->zipFileOffset = elementOffset | (isClass ? ISCLASS_BIT : 0);
			return TRUE;
		}

		if (curName[curSize] != '/') {
			/* The prefix we're looking at doesn't end with a '/', which means */
			/* it is really the suffix of the elementName, and it's a filename. */

			fileEntry = zipCache_searchFileList(dirEntry, curName, curSize, isClass);
			if(fileEntry) {
				/* We've seen this file before...update the entry to the new offset. */
				fileEntry->zipFileOffset = elementOffset | (isClass ? ISCLASS_BIT : 0);
			} else {
				if (!zipCache_addToFileList(portLib, zce, dirEntry, curName, curSize, isClass, elementOffset))
					return FALSE;
			}
			return TRUE;
		}

		/* If we got here, we're looking at a prefix which ends with '/' */
		/* Treat that prefix as a subdirectory.  If it doesn't exist, create it implicitly */

		if (!(d = zipCache_searchDirList(dirEntry, curName, curSize, isClass))) {
			if (!(d = zipCache_addToDirList(portLib, zce, dirEntry, curName, curSize, isClass))) {
				return FALSE;
			}
		}
		dirEntry = d;
		curName += prefixSize;
		nameLength -= prefixSize;
	}
}



/**
 * Returns the offset associated with a file or directory element named elementName 
 * in a zipCache.
 *
 * @param[in] zipCache the zip cache we are searching
 * @param[in] elementName the name of the element of which we want the offset
 * @param[in] elementNameLength length of elementName
 * @param[in] searchDirList when TRUE, search the dir list even if elementName does not end in '/'
 *
 * @return the zipCache if a match is found
 * @return -1 if no element of that name has been explicitly added to the cache.
 *
*/
UDATA
zipCache_findElement(J9ZipCache * zipCache, const char *elementName, IDATA elementNameLength, BOOLEAN searchDirList)
{
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *zce = zci->entry;
	J9ZipDirEntry *dirEntry;
	J9ZipFileEntry *fileEntry;
	const char *curName;
	IDATA nameLength;
	IDATA curSize;
	IDATA prefixSize;
	BOOLEAN isClass;

	if (!zipCache ||
		(elementNameLength == 0) ||
		((elementName[0] == 0) && (elementNameLength == 1)))
		return NOT_FOUND;

	dirEntry = &zce->root;

	curName = elementName;
	nameLength = elementNameLength;
	for (;;) {

		/* scan forwards in curName until we find '/' or whole string is exhausted */
		for (curSize = 0; (curSize != nameLength) && (curName[curSize] != '/'); curSize++)
			/* nothing */ ;

		prefixSize = (curSize != nameLength) ? curSize + 1 : curSize;
		isClass = FALSE;

		if ((curSize >= 6) && !memcmp(&curName[curSize - 6], ".class", 6)) {
			isClass = TRUE;
			curSize -= 6;
		}

		if ((curName-elementName) == elementNameLength) {
			/* We ran out of string, which means the elementName was */
			/* a directory name---in fact, it was the subdir we parsed */
			/* last time through the loop. */

			/* HACK: directory may have been implicitly but not explicitly added */
			if ((dirEntry->zipFileOffset & OFFSET_MASK) == IMPLICIT_ENTRY)
				return NOT_FOUND;	/* if it was never added, it doesn't "really" exist! */

			return dirEntry->zipFileOffset & OFFSET_MASK;
		}

		if (curName[curSize] != '/') {
			/* The prefix we're looking at doesn't end with a '/', which means */
			/* it is really the suffix of the elementName, and it's a filename. */

			fileEntry = zipCache_searchFileList(dirEntry, curName, curSize, isClass);
			if (fileEntry) {
				return fileEntry->zipFileOffset & OFFSET_MASK;
			}
			if (!searchDirList) {
				return NOT_FOUND;
			}
		}

		/* If we got here, we're looking at a prefix which ends with '/', or searchDirList is TRUE */
		/* Treat that prefix as a subdirectory.  It will exist if elementName was added before. */

		dirEntry = zipCache_searchDirList(dirEntry, curName, curSize, isClass);
		if (!dirEntry)
			return NOT_FOUND;
		curName += prefixSize;
		nameLength -= prefixSize;
	}
}

/** 
 * Frees the zip cache chunks
 *
 * @param[in] zci the zip cache to be freed
 *
 * @return none
 *
 * @see zipCache_new
 *
*/
void zipCache_freeChunks(J9PortLibrary *portLib, J9ZipCacheEntry *zce)
{
	J9ZipChunkHeader *chunk, *chunk2;
	U_8 *zipFileName = ZIP_SRP_GET(zce->zipFileName, U_8 *);

	PORT_ACCESS_FROM_PORT(portLib);

	chunk = ZIP_SRP_GET(zce->currentChunk, J9ZipChunkHeader *);
	if (!chunk) {
		/* if currentChunk is NULL, the cache has been copied to shared memory
		 * and shouldn't be free'd */
		return;
	}

	chunk2 = (J9ZipChunkHeader *)(((U_8 *)zce) - sizeof(J9ZipChunkHeader));
	if (((UDATA)(zipFileName - (U_8 *)chunk2)) >= ACTUAL_CHUNK_SIZE)   {
		/* HACK!!  zce->info.zipFileName points outside the first chunk, therefore it was allocated
			separately rather than being reserved from the chunk */
		j9mem_free_memory(zipFileName);
	}

	while (chunk) {
		chunk2 = ZIP_SRP_GET(chunk->next, J9ZipChunkHeader *);
		zipCache_freeChunk(portLib, chunk);
		chunk = chunk2;
	}
}


/** 
 * Deletes a zip cache and frees its resources. Also closes the zip file descriptor.
 *
 * @param[in] zipCache the zip cache to be freed
 *
 * @return none
 *
 * @see zipCache_new
 *
*/

void zipCache_kill(J9ZipCache * zipCache)
{
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *zce = zci->entry;
	J9PortLibrary *portLib = zipCache->portLib;
	PORT_ACCESS_FROM_PORT(portLib);

	zipCache_freeChunks(portLib, zce);
	if (-1 != zci->zipFileFd) {
		j9file_close(zci->zipFileFd);
	}
	j9mem_free_memory(zipCache);
}




/* Allocate a new J9ZipDirEntry and insert into dirEntry's dirList. */

J9ZipDirEntry *zipCache_addToDirList(J9PortLibrary * portLib, J9ZipCacheEntry *zce, J9ZipDirEntry * dirEntry, const char *namePtr, IDATA nameSize, BOOLEAN isClass)
{
	J9ZipDirEntry *entry;
	J9ZipChunkHeader *chunk = ZIP_SRP_GET(zce->currentChunk, J9ZipChunkHeader *);
	char *name;

	ZIP_SRP_SET_TO_NULL(zce->chunkActiveDir);

	entry = (J9ZipDirEntry *) zipCache_reserveEntry(zce, chunk, sizeof(*entry), nameSize + 1, &name);
	if (!entry) {
		if (!portLib || !(chunk = zipCache_allocateChunk(portLib)))
			return NULL;
		ZIP_SRP_SET(chunk->next, ZIP_SRP_GET(zce->currentChunk, J9ZipChunkHeader *));
		ZIP_SRP_SET(zce->currentChunk, chunk);
		entry = (J9ZipDirEntry *) zipCache_reserveEntry(zce, chunk, sizeof(*entry), nameSize + 1, &name);
		if (!entry) {
			/* ACTUAL_CHUNK_SIZE is so small it can't hold one J9ZipDirEntry?? */
			return NULL;
		}
	}
	ZIP_SRP_SET(entry->next, ZIP_SRP_GET(dirEntry->dirList, J9ZipDirEntry *));
	ZIP_SRP_SET(dirEntry->dirList, entry);
	entry->zipFileOffset = IMPLICIT_ENTRY | (isClass ? ISCLASS_BIT : 0);
	memcpy(name, namePtr, nameSize);
	/* name[nameSize] is already zero (NUL) */
	return entry;
}



/* Allocate a new zipFileEntry and insert it into dirEntry's fileList. */
/* If possible, the new file entry will be appended to the active zipFileRecord. */
/* Otherwise, a new zipFileRecord will be allocated to hold the new zipFileEntry. */

J9ZipFileEntry *zipCache_addToFileList(J9PortLibrary *portLib, J9ZipCacheEntry *zce, J9ZipDirEntry * dirEntry, const char *namePtr, IDATA nameSize,
									 BOOLEAN isClass, UDATA elementOffset)
{
	J9ZipFileEntry *entry;
	J9ZipFileRecord *record;
	J9ZipChunkHeader *chunk = ZIP_SRP_GET(zce->currentChunk, J9ZipChunkHeader *);
	J9ZipDirEntry *chunkActiveDir = ZIP_SRP_GET(zce->chunkActiveDir, J9ZipDirEntry *);
	char *name;

	if (chunkActiveDir == dirEntry) {
		entry = (J9ZipFileEntry *) zipCache_reserveEntry(zce, chunk, sizeof(*entry), nameSize, &name);
		if (NULL != entry) {
			J9ZipFileRecord *fileList = ZIP_SRP_GET(chunkActiveDir->fileList, J9ZipFileRecord *);
			/* add to end of existing entry */
			fileList->entryCount++;
			goto haveEntry;
		}
	}

	record = (J9ZipFileRecord *) zipCache_reserveEntry(zce, chunk, sizeof(*record), nameSize, &name);
	if (!record) {
		if (!portLib || !(chunk = zipCache_allocateChunk(portLib)))
			return NULL;
		ZIP_SRP_SET(chunk->next, ZIP_SRP_GET(zce->currentChunk, J9ZipChunkHeader *));
		ZIP_SRP_SET(zce->currentChunk, chunk);
		ZIP_SRP_SET_TO_NULL(zce->chunkActiveDir);
		record = (J9ZipFileRecord *) zipCache_reserveEntry(zce, chunk, sizeof(*record), nameSize, &name);
		if (!record) {
			/* ACTUAL_CHUNK_SIZE is so small it can't hold one zipFileRecord?? */
			return NULL;
		}
	}
	ZIP_SRP_SET(record->next, ZIP_SRP_GET(dirEntry->fileList, J9ZipFileRecord *));
	ZIP_SRP_SET(dirEntry->fileList, record);

	ZIP_SRP_SET(zce->chunkActiveDir, dirEntry);
	record->entryCount = 1;
	entry = record->entry;

haveEntry:
	memcpy(name, namePtr, nameSize);
	entry->nameLength = nameSize;
	entry->zipFileOffset = elementOffset | (isClass ? ISCLASS_BIT : 0);
	return entry;
}



/* Allocate a new chunk and initialize its zipChunkHeader. */

J9ZipChunkHeader *zipCache_allocateChunk(J9PortLibrary * portLib)
{
	J9ZipChunkHeader *chunk;
	PORT_ACCESS_FROM_PORT(portLib);

	chunk = (J9ZipChunkHeader *) j9mem_allocate_memory(ACTUAL_CHUNK_SIZE, J9MEM_CATEGORY_VM_JCL);
	if (!chunk)
		return NULL;
	memset(chunk, 0, ACTUAL_CHUNK_SIZE);
	chunk->beginFree = ((U_8 *)chunk) + sizeof(J9ZipChunkHeader);
	chunk->endFree = ((U_8 *) chunk) + ACTUAL_CHUNK_SIZE;
	return chunk;
}



/* Frees a chunk which is no longer used. */
/* portLib must be the original portLib which was passed to zipCache_allocateChunk. */

void zipCache_freeChunk(J9PortLibrary * portLib, J9ZipChunkHeader *chunk)
{
	PORT_ACCESS_FROM_PORT(portLib);

	j9mem_free_memory(chunk);
}



/*
 * Tries to reserve storage in a chunk for entryBytes of header data, and
 * stringBytes of string data.  If there is not enough storage, NULL is
 * returned and no storage is reserved.  If there is enough storage, a
 * pointer is returned to the allocated entryBytes, and namePtrOut points
 * to the allocated stringBytes.
 * 
 * Space will be reserved at the end of the entry struct for the name, and
 * *namePtrOut will be changed to point at the name for a length of
 * stringBytes.
 */

static UDATA*
zipCache_reserveEntry(J9ZipCacheEntry *zce, J9ZipChunkHeader * chunk, UDATA entryBytes, UDATA stringBytes, char **namePtrOut)
{
	UDATA *entry;
	UDATA stringBytesAligned;

	if (!chunk)
		return NULL;

	stringBytesAligned = ALIGN_ENTRY(stringBytes);

	if ((chunk->endFree - chunk->beginFree) < (IDATA)(entryBytes + stringBytesAligned))
		return NULL;

	entry = (UDATA *) (chunk->beginFree);
	chunk->beginFree += entryBytes;

	*namePtrOut = (char *)chunk->beginFree;
	chunk->beginFree += stringBytesAligned;

	return entry;
}



/* Searches the dirList in dirEntry for a directory entry named */
/* namePtr[0..nameSize-1] with the specified isClass value. */

J9ZipDirEntry *zipCache_searchDirList(J9ZipDirEntry * dirEntry, const char *namePtr, UDATA nameSize, BOOLEAN isClass)
{
	J9ZipDirEntry *entry;
	const char *name;

	if (!dirEntry || !namePtr)
		return NULL;

	entry = ZIP_SRP_GET(dirEntry->dirList, J9ZipDirEntry *);
	while (entry) {
		name = J9ZIPDIRENTRY_NAME(entry);
		if (!strncmp(name, namePtr, nameSize) && !name[nameSize]) {
			if (isClass && ((entry->zipFileOffset & ISCLASS_BIT) != 0)) {
				return entry;
			}
			if (!isClass && ((entry->zipFileOffset & ISCLASS_BIT) == 0)) {
				return entry;
			}
		}
		entry = ZIP_SRP_GET(entry->next, J9ZipDirEntry *);
	}
	return NULL;
}



/* Searches the fileList in dirEntry for a file entry named */
/* namePtr[0..nameSize-1] with the specified isClass value. */

J9ZipFileEntry *zipCache_searchFileList(J9ZipDirEntry * dirEntry, const char *namePtr, UDATA nameSize, BOOLEAN isClass)
{
	J9ZipFileRecord *record;
	J9ZipFileEntry *entry;
	IDATA i;

	if (!dirEntry || !namePtr)
		return NULL;

	record = ZIP_SRP_GET(dirEntry->fileList, J9ZipFileRecord *);
	while (record) {
		entry = record->entry;
		for (i = record->entryCount; i--; ) {
			if (entry->nameLength == nameSize) {
				if (!memcmp(J9ZIPFILEENTRY_NAME(entry), namePtr, nameSize)) {
					if (isClass && ((entry->zipFileOffset & ISCLASS_BIT) != 0))
						return entry;
					if (!isClass && ((entry->zipFileOffset & ISCLASS_BIT) == 0))
						return entry;
				}
			}
			entry = J9ZIPFILEENTRY_NEXT(entry);
		}
		record = ZIP_SRP_GET(record->next, J9ZipFileRecord *);
	}
	return NULL;
}



/** 
 * Searches for a directory named elementName in zipCache and if found provides 
 * a handle to it that can be used to enumerate through all of the directory's files.
 * 
 * @note The search is CASE-INSENSITIVE (contrast with @ref zipCache_findElement, which is case-sensitive).
 * @note The search is NOT recursive.
 *
 * @param[in] zipCache the zip cache that is being searched
 * @param[in] directoryName the directory we want to enumerate
 * @param[out] handle enumerate all the files in directory directoryName on this handle
 *
 * @return 0 on success and sets handle
 * @return -1 if the directory is not found
 * @return -2 if there is not enough memory to complete this call
 *
 * @see zipCache_findElement
 */

IDATA zipCache_enumNew(J9ZipCache * zipCache, char *directoryName, void **handle)
{
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipCache;
	J9ZipCacheEntry *zce = zci->entry;
	J9ZipDirEntry *dirEntry;
	char *curName;
	IDATA curSize;
	IDATA prefixSize;
	BOOLEAN isClass;

	if (!zipCache || !directoryName || !directoryName[0] || !handle) {
		return -3;
	} else {
		PORT_ACCESS_FROM_PORT(zipCache->portLib);

		dirEntry = &zce->root;

		curName = directoryName;
		for (;;) {

			/* scan forwards in curName until '/' or NUL */
			for (curSize = 0; curName[curSize] && (curName[curSize] != '/'); curSize++)
				/* nothing */ ;

			prefixSize = curSize + 1;
			isClass = FALSE;

			/* Note: CASE-INSENSITIVE HERE */
			if ((curSize >= 6) && !helper_memicmp(&curName[curSize - 6], ".class", 6)) {
				isClass = TRUE;
				curSize -= 6;
			}

			if (!*curName) {
				/* We ran out of string, which means directoryName was */
				/* the subdir we parsed last time through the loop.  Begin the traversal here. */
				J9ZipCacheTraversal *traversal = j9mem_allocate_memory(sizeof(*traversal), J9MEM_CATEGORY_VM_JCL);
				if (!traversal) {
					return -2;
				}
				traversal->zipCache = zipCache;
				traversal->portLib = zipCache->portLib;
				traversal->dirEntry = dirEntry;
				traversal->fileRecord = ZIP_SRP_GET(dirEntry->fileList, J9ZipFileRecord *);
				traversal->fileRecordPos = 0;
				traversal->fileRecordEntry = traversal->fileRecord->entry;

				/* big hack: ensure an automatically-managed cache doesn't go away while enumerating */
				if (zipCache->cachePool) {
					zipCachePool_addRef(zipCache->cachePool, zipCache);
				}

				*handle = traversal;
				return 0;
			}

			if (curName[curSize] != '/') {
				/* The prefix we're looking at doesn't end with a '/', which means */
				/* it is really the suffix of the directoryName, and it's a filename. */

				return -1;			/* We're not interested in filenames */
			}

			/* If we got here, we're looking at a prefix which ends with '/' */
			/* Treat that prefix as a subdirectory.  It will exist if directoryName has been
				added or if any file or directory inside directoryName has been added. */

			dirEntry = zipCache_searchDirListCaseInsensitive(dirEntry, curName, curSize, isClass);
			if (!dirEntry) {
				return -1;
			}
			curName += prefixSize;
		}
	}
}



/** 
 * Gets the name and offset of the next element in the directory being enumerated.
 *
 *	If nameBufSize is insufficient to hold the entire name, returns the required size for nameBuf.

 * @note Does NOT skip the element if nameBufSize buffer is of insufficient size to hold the entire name.
 *
 * @param[in] handle returned from @ref zipCache_enumNew. Used to enumerate the elements corresponding to the directory name returned by @ref zipCache_enumGetDirName
 * @param[out] nameBuf holder for element in the directory being enumerated
 * @param[in] nameBufSize
 * @param[out] offset the offset of the next element
 *
 * @return 0 on success
 * @return -1 if all the directories have been returned already
 * @return the required size of nameBuf if nameBufSize is insufficient to hold the entire name (does not skip the element)
 *
 * @see zipCache_enumNew
*
*/
IDATA zipCache_enumElement(void *handle, char *nameBuf, UDATA nameBufSize, UDATA * offset)
{
	J9ZipCacheTraversal *traversal = (J9ZipCacheTraversal *) handle;
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)traversal->zipCache;
	J9ZipCacheEntry *zce = zci->entry;
	J9ZipFileEntry *fileEntry;
	UDATA nameLen;
	if (!traversal || !nameBuf || !nameBufSize)
		return -3;

	if (!traversal->fileRecord)
		return -1;				/* No more elements */

	fileEntry = traversal->fileRecordEntry;

	nameLen = fileEntry->nameLength + 1;
	if ((fileEntry->zipFileOffset & ISCLASS_BIT) != 0)
		nameLen += 6;
	if (nameBufSize < nameLen) {
		/* Buffer is too small.  Return size the caller must allocate to try again. */
		return nameLen;
	}

	memcpy(nameBuf, J9ZIPFILEENTRY_NAME(fileEntry), fileEntry->nameLength);
	if ((fileEntry->zipFileOffset & ISCLASS_BIT) != 0)
		memcpy(nameBuf + fileEntry->nameLength, ".class", 6);
	nameBuf[nameLen-1] = 0;
	if (offset)
		*offset = fileEntry->zipFileOffset & OFFSET_MASK;

	/* Advance to the next element */
	++traversal->fileRecordPos;
	if (traversal->fileRecordPos >= traversal->fileRecord->entryCount) {
		traversal->fileRecord = ZIP_SRP_GET(traversal->fileRecord->next, J9ZipFileRecord *);
		traversal->fileRecordPos = 0;
		traversal->fileRecordEntry = traversal->fileRecord->entry;
	} else {
		traversal->fileRecordEntry = J9ZIPFILEENTRY_NEXT(traversal->fileRecordEntry);
	}
	return 0;
}



/** 
 * Gets the name of the directory on which the enumeration is based.
 * 
 * @param[in] handle handle returned from @ref zipCache_enumNew.
 * @param[out] nameBuf buffer to hold the directory name
 * @param[in] nameBufSize
 *
 * @return 0 on success
 * @return -3 on param failures
 * @return the required size for nameBuf if nameBufSize is insufficient to hold the entire name
 * 
 */

IDATA zipCache_enumGetDirName(void *handle, char *nameBuf, UDATA nameBufSize)
{
	J9ZipCacheTraversal *traversal = (J9ZipCacheTraversal *) handle;
	J9ZipCacheInternal *zci = (J9ZipCacheInternal *)traversal->zipCache;
	J9ZipCacheEntry *zce = zci->entry;
	J9ZipDirEntry *dirEntry;
	UDATA nameLen;
	const char *name;

	if (!traversal || !nameBuf || !nameBufSize)
		return -3;

	dirEntry = traversal->dirEntry;
	name = J9ZIPDIRENTRY_NAME(dirEntry);
	nameLen = strlen(name) + 1 + 1;	/* for '/' and null */
	if (nameBufSize < nameLen) {
		/* Buffer is too small.  Return size the caller must allocate to try again. */
		return nameLen;
	}

	strcpy(nameBuf, name);
	strcat(nameBuf, "/");
	return 0;
}



/** 
 * Frees any resources allocated by @ref zipCache_enumNew.
 * 
 * @param[in] handle enumerate on this handle
 *
 * @return none
 *
 * @see zipCache_enumNew
 */

void zipCache_enumKill(void *handle)
{
	J9ZipCacheTraversal *traversal = (J9ZipCacheTraversal *) handle;

	if (!traversal) {
		return;
	} else {
		PORT_ACCESS_FROM_PORT(traversal->portLib);

		/* big hack: */
		if (traversal->zipCache) {
			zipCachePool_release(traversal->zipCache->cachePool, traversal->zipCache);
		}
		j9mem_free_memory(traversal);
	}
}



/* Searches the dirList in dirEntry for a directory entry named */
/* namePtr[0..nameSize-1] with the specified isClass value. */

J9ZipDirEntry *zipCache_searchDirListCaseInsensitive(J9ZipDirEntry * dirEntry, const char *namePtr, UDATA nameSize, BOOLEAN isClass)
{
	J9ZipDirEntry *entry;

	if (!dirEntry || !namePtr)
		return NULL;

	entry = ZIP_SRP_GET(dirEntry->dirList, J9ZipDirEntry *);
	while (entry) {
		const char *name = J9ZIPDIRENTRY_NAME(entry);
		if (!helper_memicmp(name, namePtr, nameSize) && !name[nameSize]) {
			if (isClass && ((entry->zipFileOffset & ISCLASS_BIT) != 0))
				return entry;
			if (!isClass && ((entry->zipFileOffset & ISCLASS_BIT) == 0))
				return entry;
		}
		entry = ZIP_SRP_GET(entry->next, J9ZipDirEntry *);
	}
	return NULL;
}



/* Returns zero if the two strings are equal over the first length characters.  Otherwise,
	returns 1 or -1 ala stricmp. */

IDATA helper_memicmp(const void *src1, const void *src2, UDATA length)
{
	char const *s1 = (char const *)src1;
	char const *s2 = (char const *)src2;
	UDATA i;
	for (i = 0; i < length; i++) {
		if (j9_ascii_toupper(s1[i]) > j9_ascii_toupper(s2[i])) return 1;
		if (j9_ascii_toupper(s1[i]) < j9_ascii_toupper(s2[i])) return -1;
	}
	return 0;
}


#endif /* J9VM_OPT_ZIP_SUPPORT */ /* End File Level Build Flags */
