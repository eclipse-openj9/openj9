/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
 * @brief Zip Support Header
*/

#ifndef ZIPSUP_H
#define ZIPSUP_H

/* @ddr_namespace: default */
#ifdef __cplusplus
extern "C" {
#endif


	/* DO NOT DIRECTLY INCLUDE THIS FILE! */
	/* Use zip_api.h from the include/ directory instead. */


#include "j9port.h"

typedef struct J9ZipCachePool J9ZipCachePool;

#define J9WSRP IDATA

#define ZIP_INTERNAL_MAX  80
#define ZIP_CM_Reduced1  2
#define ZIP_Unknown  0
#define ZIP_GZIP  2
#define ZIP_ERR_OUT_OF_MEMORY  -3
#define ZIP_ERR_FILE_CORRUPT  -6
#define ZIP_ERR_INTERNAL_ERROR  -11
#define ZIP_CM_Imploded  6
#define ZIP_CM_Reduced4  5
#define ZIP_CM_Shrunk  1
#define ZIP_CM_Reduced2  3
#define ZIP_ERR_FILE_READ_ERROR  -1
#define ZIP_CentralHeader  0x2014B50
#define ZIP_ERR_FILE_CLOSE_ERROR  -10
#define ZIP_ERR_BUFFER_TOO_SMALL  -7
#define ZIP_CM_Reduced3  4
#define ZIP_CM_Deflated  8
#define ZIP_LocalHeader  0x4034B50
#define ZIP_CM_Tokenized  7
#define ZIP_PKZIP  1
#define ZIP_CM_Stored  0
#define ZIP_ERR_UNSUPPORTED_FILE_TYPE  -5
#define ZIP_ERR_NO_MORE_ENTRIES  -2
#define ZIP_CentralEnd  0x6054B50
#define ZIP_ERR_FILE_OPEN_ERROR  -9
#define ZIP_ERR_UNKNOWN_FILE_TYPE  -4
#define ZIP_ERR_ENTRY_NOT_FOUND  -8
#define ZIP_DataDescriptor  0x8074B50

typedef struct J9ZipCache {
    struct J9PortLibrary * portLib;
    void* cachePool;
    void* cachePoolEntry;
} J9ZipCache;

typedef struct J9ZipCentralEnd {
    U_16 diskNumber;
    U_16 dirStartDisk;
    U_16 thisDiskEntries;
    U_16 totalEntries;
    U_32 dirSize;
    U_32 dirOffset;
    U_16 commentLength;
    U_8* comment;
    U_64 endCentralDirRecordPosition;
} J9ZipCentralEnd;

typedef struct J9ZipEntry {
    U_8* data;
    U_8* filename;
    U_8* extraField;
    U_8* fileComment;
    U_32 dataPointer;
    U_32 filenamePointer;
    U_32 extraFieldPointer;
    I_32 fileCommentPointer;
    U_32 compressedSize;
    U_32 uncompressedSize;
    U_32 crc32;
    U_16 filenameLength;
    U_16 extraFieldLength;
    U_16 fileCommentLength;
    U_16 internalAttributes;
    U_16 versionCreated;
    U_16 versionNeeded;
    U_16 flags;
    U_16 compressionMethod;
    U_16 lastModTime;
    U_16 lastModDate;
    U_8 internalFilename[80];
} J9ZipEntry;

typedef struct J9ZipFile {
    U_8* filename;
    struct J9ZipCache* cache;
    void* cachePool;
    IDATA fd;
    U_32 pointer;
    U_8 internalFilename[80];
    U_8 type;
} J9ZipFile;

typedef struct J9ZipChunkHeader {
    J9WSRP next;
    U_8* beginFree;
    U_8* endFree;
} J9ZipChunkHeader;

#define J9ZIPCHUNKHEADER_NEXT(base) WSRP_GET((base)->next, struct J9ZipChunkHeader*)

typedef struct J9ZipFileEntry {
    UDATA nameLength;
    UDATA zipFileOffset;
} J9ZipFileEntry;

typedef struct J9ZipFileRecord {
    J9WSRP next;
    UDATA entryCount;
    struct J9ZipFileEntry entry[1];
} J9ZipFileRecord;

#define J9ZIPFILERECORD_NEXT(base) WSRP_GET((base)->next, struct J9ZipFileRecord*)

typedef struct J9ZipDirEntry {
    J9WSRP next;
    J9WSRP fileList;
    J9WSRP dirList;
    UDATA zipFileOffset;
} J9ZipDirEntry;

#define J9ZIPDIRENTRY_NEXT(base) WSRP_GET((base)->next, struct J9ZipDirEntry*)
#define J9ZIPDIRENTRY_FILELIST(base) WSRP_GET((base)->fileList, struct J9ZipFileRecord*)
#define J9ZIPDIRENTRY_DIRLIST(base) WSRP_GET((base)->dirList, struct J9ZipDirEntry*)

typedef struct J9ZipCacheEntry {
    J9WSRP zipFileName;
    IDATA zipFileSize;
    I_64 zipTimeStamp;
    IDATA startCentralDir;
    J9WSRP currentChunk;
    J9WSRP chunkActiveDir;
    struct J9ZipDirEntry root;
} J9ZipCacheEntry;

#define J9ZIPCACHEENTRY_ZIPFILENAME(base) WSRP_GET((base)->zipFileName, U_8*)
#define J9ZIPCACHEENTRY_CURRENTCHUNK(base) WSRP_GET((base)->currentChunk, struct J9ZipChunkHeader*)
#define J9ZIPCACHEENTRY_CHUNKACTIVEDIR(base) WSRP_GET((base)->chunkActiveDir, struct J9ZipDirEntry*)
#define J9ZIPCACHEENTRY_NEXT(base) WSRP_GET((&((base)->root))->next, struct J9ZipDirEntry*)
#define J9ZIPCACHEENTRY_FILELIST(base) WSRP_GET((&((base)->root))->fileList, struct J9ZipFileRecord*)
#define J9ZIPCACHEENTRY_DIRLIST(base) WSRP_GET((&((base)->root))->dirList, struct J9ZipDirEntry*)

#undef J9WSRP

#ifdef __cplusplus
}
#endif

#endif /* ZIPSUP_H */
