/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
#ifndef vmizip_h
#define vmizip_h

#include "j9comp.h"
#include "hookable_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* function return codes */
#define ZIP_ERR_FILE_READ_ERROR  -1
#define ZIP_ERR_NO_MORE_ENTRIES  -2
#define ZIP_ERR_OUT_OF_MEMORY  -3
#define ZIP_ERR_UNKNOWN_FILE_TYPE  -4
#define ZIP_ERR_UNSUPPORTED_FILE_TYPE  -5
#define ZIP_ERR_FILE_CORRUPT  -6
#define ZIP_ERR_BUFFER_TOO_SMALL  -7
#define ZIP_ERR_ENTRY_NOT_FOUND  -8
#define ZIP_ERR_FILE_OPEN_ERROR  -9
#define ZIP_ERR_FILE_CLOSE_ERROR  -10
#define ZIP_ERR_INTERNAL_ERROR  -11

/* flags used in VMIZipEntry compressionMethod */
#define ZIP_CM_Stored  0
#define ZIP_CM_Shrunk  1
#define ZIP_CM_Reduced1  2
#define ZIP_CM_Reduced2  3
#define ZIP_CM_Reduced3  4
#define ZIP_CM_Imploded  6
#define ZIP_CM_Reduced4  5
#define ZIP_CM_Tokenized  7
#define ZIP_CM_Deflated  8

/* flags used in zip_getZipEntry(), zip_getZipEntryFromOffset(), and zip_getNextZipEntry() */
#define ZIP_FLAG_FIND_DIRECTORY 1
#define ZIP_FLAG_READ_DATA_POINTER 2

/* flags used in zip_openZipFile() */
#define ZIP_FLAG_OPEN_CACHE 1
#define ZIP_FLAG_BOOTSTRAP 2

typedef struct VMIZipEntry
{
  U_8 *data;
  U_8 *filename;
  U_8 *extraField;
  U_8 *fileComment;
  I_32 dataPointer;
  I_32 filenamePointer;
  I_32 extraFieldPointer;
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
} VMIZipEntry;

typedef struct VMIZipFile
{
  U_8 *filename;
  void *cache;
  void *cachePool;
  I_32 fd;
  I_32 pointer;
  U_8 internalFilename[80];
  U_8 type;
  char _vmipadding0065[3];  /* 3 bytes of automatic padding */
} VMIZipFile;

typedef struct VMIZipFunctionTable {
	I_32 (*zip_closeZipFile) (VMInterface * vmi, VMIZipFile * zipFile) ;
	void (*zip_freeZipEntry) (VMInterface * vmi, VMIZipEntry * entry) ;
	I_32 (*zip_getNextZipEntry) (VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * zipEntry, IDATA * nextEntryPointer, I_32 flags) ;
	I_32 (*zip_getZipEntry) (VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, const char *filename, I_32 flags) ;
	I_32 (*zip_getZipEntryComment) (VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, U_8 * buffer, U_32 bufferSize) ;
	I_32 (*zip_getZipEntryData) (VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, U_8 * buffer, U_32 bufferSize) ;
	I_32 (*zip_getZipEntryExtraField) (VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, U_8 * buffer, U_32 bufferSize) ;
	I_32 (*zip_getZipEntryFromOffset) (VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, IDATA offset, I_32 flags) ;
	I_32 (*zip_getZipEntryRawData) (VMInterface * vm, VMIZipFile * zipFile, VMIZipEntry * entry, U_8 * buffer, U_32 bufferSize, U_32 offset) ;
	void (*zip_initZipEntry) (VMInterface * vmi, VMIZipEntry * entry) ;
	I_32 (*zip_openZipFile) (VMInterface * vmi, char *filename, VMIZipFile * zipFile, I_32 flags) ;
	void (*zip_resetZipFile) (VMInterface * vmi, VMIZipFile * zipFile, IDATA * nextEntryPointer) ;
	IDATA (*zipCache_enumElement) (void *handle, char *nameBuf, UDATA nameBufSize, UDATA * offset) ;
	IDATA (*zipCache_enumGetDirName) (void *handle, char *nameBuf, UDATA nameBufSize) ;
	void (*zipCache_enumKill) (void *handle) ;
	IDATA (*zipCache_enumNew) (void * zipCache, char *directoryName, void **handle) ;
	I_32 (*zip_getZipComment) (VMInterface * vmi, VMIZipFile * zipFile, U_8 ** comment, UDATA * commentSize);
	void (*zip_freeZipComment) (VMInterface * vmi, U_8 * comment);
	I_32 (*zip_getZipEntryWithSize) (VMInterface * vmi, VMIZipFile * zipFile, VMIZipEntry * entry, const char *filename, IDATA filenameLength, I_32 flags) ;

	/* J9 specific, keep these at the end */
	struct J9HookInterface** (*zip_getZipHookInterface) (VMInterface * vmi) ;

	void *reserved;
} VMIZipFunctionTable;

extern VMIZipFunctionTable VMIZipLibraryTable; 

#ifdef __cplusplus
}
#endif

#endif     /* vmi_h */
