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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

/**
 * @file
 * @ingroup ZipSupport
 * @brief Zip Support for Java VM
*/

#include <string.h>
#include <limits.h>

#include "j9port.h"
#include "j9lib.h"
#include "j9zipnls.h"
#include "zip_internal.h"
#include "hookable_api.h"
#include "vmzipcachehook_internal.h"
#include "j9memcategories.h"

#ifdef AIXPPC	/* hack for zlib/AIX problem */
#define STDC
#endif

#include "zlib.h"

/* Globals for the zip library */
int (*inflateInit2Func)(void*, int, const char*, int) = NULL;
int (*inflateFunc)(void*, int) = NULL;
int (*inflateEndFunc)(void*) = NULL;

J9ZipFunctionTable zipFunctions = {
	zip_freeZipComment,
	zip_freeZipEntry,
	zip_getNextZipEntry,
	zip_getZipComment,
	zip_getZipEntry,
	zip_getZipEntryComment,
	zip_getZipEntryData,
	zip_getZipEntryExtraField,
	zip_getZipEntryFromOffset,
	zip_getZipEntryRawData,
	zip_initZipEntry,
	zip_openZipFile,
	zip_releaseZipFile,
	zip_resetZipFile
};

#define ZIP_NEXT_U8(value, index) (value = *(index++))
#define ZIP_NEXT_U16(value, index) ((value = (index[1] << 8) | index[0]), index += 2, value)
#define ZIP_NEXT_U32(value, index) ((value = ((U_32)index[3] << 24) | ((U_32)index[2] << 16) | ((U_32)index[1] << 8) | (U_32)index[0]), index += 4, value)

#define SCAN_CHUNK_SIZE 1024

struct workBuffer {
	J9PortLibrary *portLib;
	UDATA *bufferStart;
	UDATA *bufferEnd;
	UDATA *currentAlloc;
	UDATA cntr;
};

static I_32 zip_populateCache (J9PortLibrary* portLib, J9ZipFile *zipFile, J9ZipCentralEnd *endEntry, IDATA startCentralDir);
static I_32 inflateData (struct workBuffer* workBuf, U_8* inputBuffer, U_32 inputBufferSize, U_8* outputBuffer, U_32 outputBufferSize);
I_32 scanForDataDescriptor (J9PortLibrary* portLib, J9ZipFile *zipFile, J9ZipEntry* zipEntry);
void zdatafree (void* opaque, void* address);
static I_32 readZipEntry (J9PortLibrary *portLib, J9ZipFile *zipFile, J9ZipEntry *zipEntry,
		const char *filename, IDATA filenameLength, IDATA *enumerationPointer, IDATA *entryStart, BOOLEAN findDirectory, BOOLEAN readDataPointer);
I_32 scanForCentralEnd (J9PortLibrary* portLib, J9ZipFile *zipFile, J9ZipCentralEnd* endEntry);
void* zdataalloc (void* opaque, U_32 items, U_32 size);
static I_32 getZipEntryUsingDirectory(J9PortLibrary *portLib, J9ZipFile *zipFile, J9ZipEntry *entry,
		const char *fileName, IDATA fileNameLength, BOOLEAN readDataPointer);
static BOOLEAN isSeekFailure(I_64 seekResult, I_64 expectedValue);

#if defined(J9VM_THR_PREEMPTIVE)
#include "omrthread.h"
#define ENTER() omrthread_monitor_enter(omrthread_global_monitor())
#define EXIT() omrthread_monitor_exit(omrthread_global_monitor())
#else
#define ENTER()
#define EXIT()
#endif

#if !defined(PATH_MAX)
/* This is a somewhat arbitrarily selected fixed buffer size. */
#define PATH_MAX 1024
#endif
#define MIN_ZIPFILE_SIZE 22
#define ZIPFILE_COMMENT_OFFSET 21

/*
	Ensure that the zip library is loaded.
	Return 0 on success, ZIP_ERR_FILE_READ_ERROR or ZIP_ERR_OUT_OF_MEMORY on failure.
*/
I_32 initZipLibrary(J9PortLibrary* portLib, char* dir)
{
	char correctPath[PATH_MAX] = "";
	char *correctPathPtr = correctPath;
	UDATA zipDLLDescriptor = 0;
	I_32 rc = 0;
	PORT_ACCESS_FROM_PORT(portLib);

	if (NULL != inflateInit2Func) {
		return 0; /* already initialized */
	}
#if defined (J9VM_OPT_BUNDLE_CORE_MODULES) && !defined (J9VM_STATIC_LINKAGE)
	inflateInit2Func = j9zlib_inflateInit2_;
	inflateFunc = j9zlib_inflate;
	inflateEndFunc = j9zlib_inflateEnd;
	return 0;
#else
	/* open up the zip library by name */
	if (dir != NULL) {
		/* expectedPathLength - %s/%s - +2 includes / and NUL terminator */
		UDATA expectedPathLength = strlen(dir) + strlen(J9_ZIP_DLL_NAME) + 2;
		if (expectedPathLength > PATH_MAX) {
			correctPathPtr = j9mem_allocate_memory(expectedPathLength, J9MEM_CATEGORY_VM_JCL);
			if (NULL == correctPathPtr) {
				inflateInit2Func  = NULL; /* indicate that the library is not initialized */
				return ZIP_ERR_OUT_OF_MEMORY;
			}
		}
		j9str_printf(portLib, correctPathPtr, expectedPathLength, "%s/%s", dir, J9_ZIP_DLL_NAME);
		if(j9sl_open_shared_library(correctPathPtr, &zipDLLDescriptor, TRUE)) goto openFailed;
	} else {
		/* dir is NULL. It shouldn't happen, but in case, revert back to original dlload that
		 * replies on LIBPATH
		 */
		if(j9sl_open_shared_library(J9_ZIP_DLL_NAME, &zipDLLDescriptor, TRUE)) goto openFailed;
	}

	/* look up the functions */
	if(j9sl_lookup_name(zipDLLDescriptor, "j9zlib_inflateInit2_", (void *) &inflateInit2Func, "ILILI")) goto loadFailed;
	if(j9sl_lookup_name(zipDLLDescriptor, "j9zlib_inflate", (void *) &inflateFunc, "IPI")) goto loadFailed;
	if(j9sl_lookup_name(zipDLLDescriptor, "j9zlib_inflateEnd", (void *) &inflateEndFunc, "IP")) goto loadFailed;

exit:
	if (correctPath != correctPathPtr) {
		j9mem_free_memory(correctPathPtr);
	}
	
	if (0 != rc) {
		inflateInit2Func  = NULL; /* indicate that the library is not initialized */
	};

	return rc;

loadFailed:
	j9sl_close_shared_library(zipDLLDescriptor);

	/* Unable to open %s (Missing export) */
	j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_ZIP_MISSING_EXPORT, J9_ZIP_DLL_NAME);

	rc = ZIP_ERR_FILE_READ_ERROR;
	goto exit;
	
openFailed:

	/* Unable to open %s (%s) */
	j9nls_printf(PORTLIB, J9NLS_WARNING, J9NLS_ZIP_UNABLE_TO_OPEN_ZIP_DLL, J9_ZIP_DLL_NAME, j9error_last_error_message());
	
	rc = ZIP_ERR_FILE_READ_ERROR;
	goto exit;
#endif
}

/**
 * @param seekResult actual result of the seek
 * @param expectedValue expected seek result
 * @return true if seekResult is negative or larger than the maximum I_32 positive value or does not match the expected position
 */
static VMINLINE BOOLEAN
isSeekFailure(I_64 seekResult, I_64 expectedValue) {
	return (seekResult < 0) || (seekResult > INT_MAX) || (seekResult != expectedValue);
}

/*
	Returns 0 on success or one of the following:
			ZIP_ERR_UNSUPPORTED_FILE_TYPE
			ZIP_ERR_FILE_CORRUPT
			ZIP_ERR_OUT_OF_MEMORY
			ZIP_ERR_INTERNAL_ERROR
*/
static I_32 inflateData(struct workBuffer* workBuf, U_8* inputBuffer, U_32 inputBufferSize, U_8* outputBuffer, U_32 outputBufferSize)
{
	PORT_ACCESS_FROM_PORT(workBuf->portLib);

	z_stream stream;
	I_32 err;

	stream.next_in = inputBuffer;
	stream.avail_in = inputBufferSize;
	stream.next_out = outputBuffer;
	stream.avail_out = outputBufferSize;

	stream.opaque = (voidpf) workBuf;
	stream.zalloc = (alloc_func) zdataalloc;
	stream.zfree = (free_func) zdatafree;

	/* Initialize stream. Pass "-15" as max number of window bits, negated
		to indicate that no zlib header is present in the data. */
	err = inflateInit2Func(&stream, -15, ZLIB_VERSION, sizeof(z_stream));
	if(err != Z_OK)
		return -1;

	/* Inflate the data. */
	err = inflateFunc(&stream, Z_SYNC_FLUSH);

	/* Clean up the stream. */
	inflateEndFunc(&stream);

	/* Check the return code. Did we complete the inflate? */
	if((err == Z_STREAM_END)||(err == Z_OK)) {
		if(stream.total_out == outputBufferSize) {
			return 0;
		}
	}

	switch (err)  {
	case Z_OK:  /* an error if file is incomplete */
	case Z_STREAM_END:  /* an error if file is incomplete */
	case Z_ERRNO:  /* a random error */
	case Z_STREAM_ERROR:  /* stream inconsistent */
	case Z_DATA_ERROR:  /* corrupted zip */
		return ZIP_ERR_FILE_CORRUPT;

	case Z_VERSION_ERROR:  /* wrong zlib version */
	case Z_NEED_DICT:  /* needs a preset dictionary that we can't provide */
		return ZIP_ERR_UNSUPPORTED_FILE_TYPE;

	case Z_MEM_ERROR:  /* out of memory */
		return ZIP_ERR_OUT_OF_MEMORY;

	case Z_BUF_ERROR:  /* no progress / out of output buffer */
	default:  /* jic */
		return ZIP_ERR_INTERNAL_ERROR;
	}
}

/*
	Scan backward from end of file and read zip file comment.
	 * @param[in] portLib the port library
	 * @param[in] zipFile the zip file concerned
	 * @param[in/out] pointer to commentString buffer, buffer will be allocated
	 * 				  and filled with zip file comments if present on return
	 * @param[out] pointer to commentLength

	 * @return 0 on success or one of the following
	 * @return	ZIP_ERR_FILE_CORRUPT if zipFile is corrupt
 			    ZIP_ERR_FILE_READ_ERROR if error reading zipFile
 			    ZIP_ERR_OUT_OF_MEMORY if can't allocate memory
*/
I_32
zip_getZipComment(J9PortLibrary* portLib, J9ZipFile *zipFile, U_8 ** commentString, UDATA * commentLength)
{
	U_8 *current;
	U_8 buffer[SCAN_CHUNK_SIZE + MIN_ZIPFILE_SIZE];
	I_32 i, size, state;
	U_32 dataSize;
	I_64 seekResult;
	I_32 fileSize;
	I_32 bytesAlreadyRead = 0;
	I_32 rBytes = 0;
	I_16 commentOffsetFromEnd = 0;
	BOOLEAN readFromEnd = TRUE;
	I_16 loopCount = 0;

	PORT_ACCESS_FROM_PORT(portLib);

	ENTER ();
	/* Haven't seen anything yet. */
	state = 0;
	*commentString = NULL;
	*commentLength = 0;
	seekResult = j9file_seek(zipFile->fd, 0, EsSeekEnd);
	if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF))) {
		zipFile->pointer = -1;
		EXIT();
		return ZIP_ERR_FILE_READ_ERROR;
	}
	fileSize = (I_32) seekResult;
	zipFile->pointer = fileSize;
	while (TRUE)  {
		/* Fill the buffer. */
		if (bytesAlreadyRead == fileSize) {
			if (fileSize == MIN_ZIPFILE_SIZE) {
				/* empty zip file with just end of central dir record */
				EXIT();
				return 0;
			} else {
				zipFile->pointer = -1;
				EXIT();
				return ZIP_ERR_FILE_CORRUPT;
			}
		}

		size = SCAN_CHUNK_SIZE;
		if (size > fileSize-bytesAlreadyRead) {
			size = fileSize-bytesAlreadyRead;
		}
		bytesAlreadyRead += size;
		seekResult = j9file_seek(zipFile->fd, fileSize-bytesAlreadyRead, EsSeekSet);
		if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF))) {
			zipFile->pointer = -1;
			EXIT();
			return ZIP_ERR_FILE_READ_ERROR;
		}
		zipFile->pointer = (I_32)seekResult;
		if(readFromEnd == FALSE) {
			/* First scan of SCAN_CHUNK_SIZE should find ECDR
			 * if not then zipfile comment starts near 1k*n boundary
			 * where n is 1..64 (one eg is comment size of 1003 bytes),
			 * so read 22(size of ECDR) more bytes in 2nd and later scans
			 * to have overlap of ECDR from previous scan.
			 * This will ensure that perfect scan can  still be done even
			 * when ECD record is spreads between two scans with minimal file reads
			 * and use existing mechanism.
			 */
			size += MIN_ZIPFILE_SIZE;
		}

		if (j9file_read( zipFile->fd, buffer, size) != size)  {
			zipFile->pointer = -1;
			EXIT();
			return ZIP_ERR_FILE_READ_ERROR;
		}
		zipFile->pointer += size;
		dataSize = 0;
		/* Scan the buffer (backwards) for CentralEnd signature = PK^E^F. */
		for (i = size; i--; dataSize++, commentOffsetFromEnd++)
		{
			switch(state)
			{
				case 0:
					/* Nothing yet. */
					if (buffer[i] == 6) {
						state = 1;
					}
					break;

				case 1:
					/* Seen ^F */
					if (buffer[i] == 5) {
						state = 2;
					}
					else {
						state = 0;
					}
					break;

				case 2:
					/* Seen ^E^F */
					if (buffer[i] == 'K') {
						state = 3;
					}
					else {
						state = 0;
					}
					break;

				case 3:
					/* Seen K^E^F */
					if (buffer[i] == 'P' && dataSize >= ZIPFILE_COMMENT_OFFSET) {
						/* Found it.  Read the data from the end-of-central-dir record. */
						current = buffer + i + 20;
						ZIP_NEXT_U16(*commentLength, current);
						/* Check for valid value for comment length, loopCount*MIN_ZIPFILE_SIZE helps to get actual
						 * number times we read extra bytes in 2nd and subsequent scans for zipfile with
						 * comments approximately > 1k bytes.
						 */
						if (*commentLength != (commentOffsetFromEnd - ZIPFILE_COMMENT_OFFSET - loopCount*MIN_ZIPFILE_SIZE )) {
							/* may be bogus marker, continue scanning */
							state = 0;
							break;
						}
						if (*commentLength > 0) {
							*commentString = j9mem_allocate_memory(*commentLength, J9MEM_CATEGORY_VM_JCL);
							if (*commentString == NULL) {
								EXIT();
								return ZIP_ERR_OUT_OF_MEMORY;
							}
							/* If buffer holds all zip file comments then use it to get comment string */
							if (dataSize >= ZIPFILE_COMMENT_OFFSET + *commentLength) {
								memcpy((U_8*)*commentString, current, *commentLength );
								EXIT();
								return 0;
							}
							else {
								/* Buffer may not be able to hold complete comment string, so get it from file */
								zipFile->pointer =  zipFile->pointer - dataSize + ZIPFILE_COMMENT_OFFSET;
								seekResult = j9file_seek(zipFile->fd, zipFile->pointer, EsSeekSet);
								if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF))) {
									zipFile->pointer = -1;
									j9mem_free_memory(*commentString);
									EXIT();
									return ZIP_ERR_FILE_READ_ERROR;
								}
								if ((rBytes = (I_32)(j9file_read( zipFile->fd, (U_8*)*commentString, *commentLength))) != *commentLength)  {
									/* may be bogus marker, continue scanning */
									j9mem_free_memory(*commentString);
								}
								zipFile->pointer += rBytes;
								if (zipFile->pointer != fileSize) {
									zipFile->pointer = -1;
									if (*commentString != NULL)	{
										j9mem_free_memory(*commentString);
									}
									EXIT();
									return ZIP_ERR_FILE_READ_ERROR;
								}
								else {
									EXIT();
									return 0;
								}
							}
						} else {
							EXIT();
							return 0;
						}
					}
					state = 0;
					break;
			}
			if (readFromEnd) {
				/* 2nd and subsequent scans are treated special due to huge zipfile comments!! */
				readFromEnd = FALSE;
			}
		}
		loopCount++;
	}
}

/*
 * deallocates memory allocated by zip_getZipComment()
 * @param[in] portLib the port library
 * @param[in] commentString buffer
 *
*/
void
zip_freeZipComment(J9PortLibrary * portLib, U_8 * commentString)
{
	PORT_ACCESS_FROM_PORT(portLib);

	if (commentString != NULL) {
		 j9mem_free_memory(commentString);
	}
}


/*
	Scan backward from end of file for a central end header. Read from zipFile and update the J9ZipCentralEnd provided.

	Returns 0 on success or one of the following:
			ZIP_ERR_FILE_READ_ERROR
			ZIP_ERR_FILE_CORRUPT
*/
I_32 scanForCentralEnd(J9PortLibrary* portLib, J9ZipFile *zipFile, J9ZipCentralEnd* endEntry)
{
	U_8 *current;
	U_8 buffer[SCAN_CHUNK_SIZE + MIN_ZIPFILE_SIZE];
	I_32 i, size, state;
	U_32 dataSize = 0;
	I_64 seekResult;
	I_32 fileSize;
	I_32 bytesAlreadyRead = 0;
	BOOLEAN readFromEnd = TRUE;


	PORT_ACCESS_FROM_PORT(portLib);

	/* Haven't seen anything yet. */
	state = 0;

	seekResult = j9file_seek(zipFile->fd, 0, EsSeekEnd);
	if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF))) {
		zipFile->pointer = -1;
		return ZIP_ERR_FILE_READ_ERROR;
	}
	fileSize = (I_32) seekResult;
	zipFile->pointer = fileSize;

	while(TRUE)  {
		/* Fill the buffer. */
		if (bytesAlreadyRead == fileSize)  {
			zipFile->pointer = -1;
			return ZIP_ERR_FILE_CORRUPT;
		}
		
		size = SCAN_CHUNK_SIZE;
		if (size > fileSize-bytesAlreadyRead) {
			size = fileSize-bytesAlreadyRead;
		}
		bytesAlreadyRead += size;
		seekResult = j9file_seek(zipFile->fd, fileSize-bytesAlreadyRead, EsSeekSet);
		if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF))) {
			zipFile->pointer = -1;
			return ZIP_ERR_FILE_READ_ERROR;
		}
		zipFile->pointer = (I_32)seekResult;
		if(readFromEnd == FALSE) {
			/* First scan of SCAN_CHUNK_SIZE should find ECDR
			 * if not then zipfile comments are greater than 1002 bytes,
			 * so read 22(size of ECDR) more bytes in 2nd and later scans
			 * to have overlap of ECDR from previous scan.
			 * This will ensure that perfect scan can still be done even when ECD record is spread
			 * between two scans.
			 */
			size += MIN_ZIPFILE_SIZE;
		}
		if (j9file_read( zipFile->fd, buffer, size) != size)  {
				zipFile->pointer = -1;
				return ZIP_ERR_FILE_READ_ERROR;
		}
		zipFile->pointer += size;

		/* Scan the buffer (backwards) for CentralEnd signature = PK^E^F. */
		for (i = size; i--; dataSize++)
		{
			switch(state)
			{
				case 0:
					/* Nothing yet. */
					if(buffer[i] == 6) state = 1;
					break;

				case 1:
					/* Seen ^F */
					if(buffer[i] == 5) state = 2;
					else state = 0;
					break;

				case 2:
					/* Seen ^E^F */
					if(buffer[i] == 'K') state = 3;
					else state = 0;
					break;

				case 3:
					/* Seen K^E^F */
					if(buffer[i] == 'P' && dataSize >= ZIPFILE_COMMENT_OFFSET)
					{
						endEntry->endCentralDirRecordPosition = seekResult + i;
						/* Found it.  Read the data from the end-of-central-dir record. */
						current = buffer+i+4;
						ZIP_NEXT_U16(endEntry->diskNumber, current);
						ZIP_NEXT_U16(endEntry->dirStartDisk, current);
						ZIP_NEXT_U16(endEntry->thisDiskEntries, current);
						ZIP_NEXT_U16(endEntry->totalEntries, current);
						ZIP_NEXT_U32(endEntry->dirSize, current);
						ZIP_NEXT_U32(endEntry->dirOffset, current);
						ZIP_NEXT_U16(endEntry->commentLength, current);

						/* Quick test to ensure that the header isn't bogus.
							Current dataSize is the number of bytes of data scanned, up to the ^H in the stream. */
						if(dataSize >= (U_32)(ZIPFILE_COMMENT_OFFSET+endEntry->commentLength)) return 0;
						/* Header looked bogus. Pretend we didn't see it and keep scanning.. */
					}
					state = 0;
					break;
			}
			if (readFromEnd) {
				/* 2nd and subsequent scans are treated special due to huge zipfile comments!! */
				readFromEnd = FALSE;
			}
		}
	}
}


/*
	Scan ahead for a data descriptor. Read from zipFile and update the J9ZipLocalHeader provided.

	Returns 0 on success or one of the following:
			ZIP_ERR_FILE_READ_ERROR
			ZIP_ERR_FILE_CORRUPT
*/
I_32 scanForDataDescriptor(J9PortLibrary* portLib, J9ZipFile *zipFile, J9ZipEntry* zipEntry)
{
	U_8 *current;
	U_8 buffer[SCAN_CHUNK_SIZE], descriptor[16];
	I_32 i, size, state;
	U_32 dataSize, blockPointer;
	I_64 seekResult;

	PORT_ACCESS_FROM_PORT(portLib);

	/* Skip ahead and read the data descriptor. The compressed size should be 0. */
	if (zipFile->pointer != (IDATA)(zipEntry->dataPointer + zipEntry->compressedSize))  {
		zipFile->pointer = (I_32)(zipEntry->dataPointer + zipEntry->compressedSize);
	}
	seekResult = j9file_seek(zipFile->fd, zipFile->pointer, EsSeekSet);
	if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF)) || (zipFile->pointer != seekResult)) {
		zipFile->pointer = -1;
		return ZIP_ERR_FILE_READ_ERROR;
	}

	/* Haven't seen anything yet. */
	blockPointer = dataSize = zipEntry->compressedSize;
	state = 0;

	/* Scan until we find PK^G^H (otherwise it's an error). */
	while(1)	{
		/* Fill the buffer. */
		size = (I_32)j9file_read( zipFile->fd, buffer, SCAN_CHUNK_SIZE);
		if(size == 0) {
			return ZIP_ERR_FILE_CORRUPT;
		} else if(size < 0) {
			zipFile->pointer = -1;
			return ZIP_ERR_FILE_READ_ERROR;
		}
		zipFile->pointer += size;
		blockPointer += size;

		/* Scan the buffer. */
		for(i = 0; i < size; i++, dataSize++) {
			switch(state)
			{
				case 0:
					/* Nothing yet. */
					if(buffer[i] == 'P') {
						state = 1;
					}
					break;

				case 1:
					/* Seen P */
					if(buffer[i] == 'K') {
						state = 2;
					}
					else state = 0;
					break;

				case 2:
					/* Seen PK */
					if(buffer[i] == 7) {
						state = 3;
					} else {
						state = 0;
					}
					break;

				case 3:
					/* Seen PK^G */
					if(buffer[i] == 8) {
						/* Found it! Read the descriptor */
						if(i + 12 < size) {
							current = &buffer[i + 1];
						} else {
							seekResult = j9file_seek(zipFile->fd, zipEntry->dataPointer + dataSize + 1, EsSeekSet);
							if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF))) {
								zipFile->pointer = -1;
								return ZIP_ERR_FILE_READ_ERROR;
							}
							zipFile->pointer = (I_32) seekResult;
							if( j9file_read( zipFile->fd, descriptor, 12 ) != 12) {
								zipFile->pointer = -1;
								return ZIP_ERR_FILE_READ_ERROR;
							}
							zipFile->pointer += 12;
							current = descriptor;
						}
							
						/* Read the data from the descriptor. */
						ZIP_NEXT_U32(zipEntry->crc32, current);
						ZIP_NEXT_U32(zipEntry->compressedSize, current);
						ZIP_NEXT_U32(zipEntry->uncompressedSize, current);

						/* Quick test to ensure that the header isn't bogus. 
							Current dataSize is the number of bytes of data scanned, up to the ^H in the stream. */
						if(dataSize - 3 == zipEntry->compressedSize) {
							return 0;
						}

						/* Header looked bogus. Reset the pointer and continue scanning. */
						seekResult = j9file_seek(zipFile->fd, zipEntry->dataPointer + blockPointer, EsSeekSet);
						if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF))) {
							zipFile->pointer = -1;
							return ZIP_ERR_FILE_READ_ERROR;
						}
						zipFile->pointer = (I_32)seekResult;
					}
					else state = 0;
					break;
			}
		}
	}
}



/*
	Fill in the cache of a given zip file.  This should only be called once during zip_openZipFile!

	Returns 0 on success or one of the following:
			ZIP_ERR_FILE_READ_ERROR
			ZIP_ERR_FILE_OPEN_ERROR
			ZIP_ERR_UNKNOWN_FILE_TYPE
			ZIP_ERR_UNSUPPORTED_FILE_TYPE
			ZIP_ERR_OUT_OF_MEMORY
			ZIP_ERR_INTERNAL_ERROR
*/
static I_32 zip_populateCache(J9PortLibrary* portLib, J9ZipFile *zipFile, J9ZipCentralEnd *endEntry, IDATA startCentralDir)
{
	PORT_ACCESS_FROM_PORT(portLib);

	I_32 result = 0;
	IDATA bufferSize = ZIP_WORK_BUFFER_SIZE;
	IDATA unreadSize = 0;
	IDATA bufferedSize = 0;
	IDATA bytesToRead = 0;
	IDATA filenameCopied;
	J9ZipEntry entry;
	U_8 *buffer = NULL;
	U_8 *filename = NULL;
	IDATA filenameSize = 256;  /* Should be sufficient for most filenames */
	U_8 *current;
	U_32 sig;
	U_32 localHeaderOffset;
	I_64 seekResult;
	J9ZipCachePool *cachePool;
	BOOLEAN freeFilename = FALSE;
	BOOLEAN freeBuffer = FALSE;

	if (!zipFile->cache)  return ZIP_ERR_INTERNAL_ERROR;

	unreadSize = endEntry->dirSize + 4 /* slop */;

	if (zipFile->pointer != startCentralDir)  {
		zipFile->pointer = (I_32)startCentralDir;
	}
	seekResult = j9file_seek(zipFile->fd, zipFile->pointer, EsSeekSet);
	if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF)) || (zipFile->pointer != seekResult)) {
		zipFile->pointer = -1;
		result = ZIP_ERR_FILE_READ_ERROR;
		goto finished;
	}

	/* Allocate some space to hold central directory goo as we eat through it */
	cachePool = zipFile->cachePool;
	if (cachePool != NULL) {
		if (cachePool->allocateWorkBuffer) {
			cachePool->allocateWorkBuffer = FALSE;
			cachePool->workBuffer = j9mem_allocate_memory(ZIP_WORK_BUFFER_SIZE, J9MEM_CATEGORY_VM_JCL);
		}
		if (cachePool->workBuffer != NULL) {
			filename = (U_8*)cachePool->workBuffer;
			buffer = (U_8*)cachePool->workBuffer + filenameSize;
			bufferSize -= filenameSize;
		}
	}

	/* No point in allocating more than we'll actually need.. */
	if (bufferSize > unreadSize)  bufferSize = unreadSize;

	if (buffer == NULL) {
		freeBuffer = freeFilename = TRUE;
		filename = j9mem_allocate_memory(filenameSize, J9MEM_CATEGORY_VM_JCL);
		if (!filename)  {
			result = ZIP_ERR_OUT_OF_MEMORY;
			goto finished;
		}

		buffer = j9mem_allocate_memory(bufferSize, J9MEM_CATEGORY_VM_JCL);
	}
	if(!buffer && (bufferSize > 4096))	 {
		/* Not enough memory, fall back to a smaller buffer! */
		bufferSize = 4096;
		buffer = j9mem_allocate_memory(bufferSize, J9MEM_CATEGORY_VM_JCL);
	}
	if(!buffer)	{
		result = ZIP_ERR_OUT_OF_MEMORY;
		goto finished;
	}
	
	while(unreadSize)  {

		/* Read as much as needed into buffer. */
		bytesToRead = bufferSize-bufferedSize;
		if (bytesToRead > unreadSize)  bytesToRead = unreadSize;
		result = (I_32)j9file_read(zipFile->fd, buffer+bufferedSize, bytesToRead);
		if (result < 0)  {
			result = ZIP_ERR_FILE_READ_ERROR;
			zipFile->pointer = -1;
			goto finished;
		}
		zipFile->pointer += result;
		unreadSize -= result;
		bufferedSize += result;
		current = buffer;

		/* consume entries until we run out. */
		while ( current+46 < buffer+bufferedSize )  {
			IDATA entryPointer;

			entryPointer = zipFile->pointer + (current-(buffer+bufferedSize));

			ZIP_NEXT_U32(sig, current);
			if(sig == ZIP_CentralEnd)  {
				/* We're done here. */
				result = 0;
				goto finished;
			}
			if (sig != ZIP_CentralHeader)  {
				/* This is unexpected. */
				result = ZIP_ERR_FILE_CORRUPT;
				goto finished;
			}

			/* Read ZIP_CentralHeader entry */
			ZIP_NEXT_U16(entry.versionCreated, current);
			ZIP_NEXT_U16(entry.versionNeeded, current);
			ZIP_NEXT_U16(entry.flags, current);
			ZIP_NEXT_U16(entry.compressionMethod, current);
			ZIP_NEXT_U16(entry.lastModTime, current);
			ZIP_NEXT_U16(entry.lastModDate, current);
			ZIP_NEXT_U32(entry.crc32, current);
			ZIP_NEXT_U32(entry.compressedSize, current);
			ZIP_NEXT_U32(entry.uncompressedSize, current);
			ZIP_NEXT_U16(entry.filenameLength, current);
			ZIP_NEXT_U16(entry.extraFieldLength, current);	
			ZIP_NEXT_U16(entry.fileCommentLength, current);	
			current += sizeof(U_16);  /* skip disk number field */
			ZIP_NEXT_U16(entry.internalAttributes, current);
			current += sizeof(U_32);  /* skip external attributes field */
			ZIP_NEXT_U32(localHeaderOffset, current);

			/* Increase filename buffer size if necessary. */
			if (filenameSize < entry.filenameLength + 1)  {
				if (freeFilename) {
					j9mem_free_memory(filename);
				}
				filenameSize = entry.filenameLength + 1;
				freeFilename = TRUE;
				filename = j9mem_allocate_memory(filenameSize, J9MEM_CATEGORY_VM_JCL);
				if (!filename)  {
					result = ZIP_ERR_OUT_OF_MEMORY;
					goto finished;
				}
			}

			filenameCopied = 0;
			while (filenameCopied < entry.filenameLength)  {
				IDATA size;
				/* Copy as much of the filename as we can see in the buffer (probably the whole thing). */
				
				size = entry.filenameLength - filenameCopied;
				if (size > bufferedSize - (current-buffer))  {
					size = bufferedSize - (current-buffer);
				}
				memcpy(filename+filenameCopied, current, size);
				filenameCopied += size;
				current += size;
				if (filenameCopied >= entry.filenameLength)  break;  /* done */

				/* Otherwise, we ran out of source string.  Load another chunk.. */
				bufferedSize = 0;
				if (!unreadSize)  {
					/* Central header is supposedly done?  Bak */
					result = ZIP_ERR_FILE_CORRUPT;
					goto finished;
				}
				bytesToRead = bufferSize-bufferedSize;
				if (bytesToRead > unreadSize)  bytesToRead = unreadSize;
				result = (I_32)j9file_read(zipFile->fd, buffer+bufferedSize, bytesToRead);
				if (result < 0)  {
					result = ZIP_ERR_FILE_READ_ERROR;
					zipFile->pointer = -1;
					goto finished;
				}
				zipFile->pointer += result;
				unreadSize -= result;
				bufferedSize += result;
				current = buffer;
			}
			filename[entry.filenameLength] = '\0';  /* null-terminate */

			if (((entry.compressionMethod == ZIP_CM_Deflated)&&(entry.flags & 0x8))
				|| (entry.fileCommentLength != 0))  {
				/* Either local header doesn't know the compressedSize, or this entry has a file
					comment.  In either case, cache the central header instead of the local header
					so we can find the information we need later. */

				result = (I_32)zipCache_addElement(zipFile->cache, (char*)filename, (IDATA)entry.filenameLength, entryPointer);

			} else  {
				result = (I_32)zipCache_addElement(zipFile->cache, (char*)filename, (IDATA)entry.filenameLength, localHeaderOffset);
			}

			if (!result)  {
				result = ZIP_ERR_OUT_OF_MEMORY;
				goto finished;
			}

			/* Skip the data and comment. */
			bytesToRead = entry.extraFieldLength + entry.fileCommentLength;
			if (bufferedSize - (current-buffer) >= bytesToRead)  {
				current += bytesToRead;
			} else  {
				/* The rest of the buffer is uninteresting.  Skip ahead to where the good stuff is */
				bytesToRead -= (bufferedSize - (current-buffer));
				current = buffer+bufferedSize;
				unreadSize -= bytesToRead;
		
				seekResult = j9file_seek(zipFile->fd, bytesToRead, EsSeekCur);
				if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF))) {
					zipFile->pointer = -1;
					result = ZIP_ERR_FILE_READ_ERROR;
					goto finished;
				}
				zipFile->pointer = (I_32)seekResult;
			}
		}
		bufferedSize -= (current-buffer);
		memmove(buffer, current, bufferedSize);
	}

	result = 0;

finished:
	if (filename && freeFilename) j9mem_free_memory(filename);
	if (buffer && freeBuffer) j9mem_free_memory(buffer);
	return result;
}


/*
	Read the next zip entry for the zipFile into the zipEntry provided.  If filename is non-NULL, it is expected to match
	the filename read for the entry.  If (cachePointer != -1) the filename of the entry will be looked up in the cache (assuming
	there is one) to help detect use of an invalid cache.  If enumerationPointer is non-NULL, sequential access is assumed and
	either a local zip entry or a data descriptor will be accepted, but a central zip entry will cause ZIP_ERR_NO_MORE_ENTRIES
	to be returned.  If enumerationPointer is NULL, random access is assumed and either a local zip entry or a central zip
	entry will be accepted.

	Returns 0 on success or one of the following:
			ZIP_ERR_FILE_READ_ERROR
			ZIP_ERR_FILE_CORRUPT
			ZIP_ERR_OUT_OF_MEMORY
			ZIP_ERR_NO_MORE_ENTRIES
*/
static I_32
readZipEntry(J9PortLibrary * portLib, J9ZipFile * zipFile, J9ZipEntry * zipEntry, const char *filename, IDATA filenameLength,
		IDATA * enumerationPointer, IDATA * entryStart, BOOLEAN findDirectory, BOOLEAN readDataPointer)
{
	PORT_ACCESS_FROM_PORT(portLib);

	I_32 result;
	U_8 buffer[46 + 128];
	U_8 *current;
	U_32 sig;
	IDATA readLength;
	I_64 seekResult;
	U_8 *readBuffer;
	IDATA currentEntryPointer, localEntryPointer;
	IDATA headerSize;

  retry:
	if (entryStart)
		*entryStart = zipFile->pointer;
	readBuffer = NULL;
	/* Guess how many bytes we'll need to read.  If we guess correctly we will do fewer I/O operations */
	headerSize = 30;			/* local zip header size */
	if (zipFile->cache && (zipFile->pointer >= zipCache_getStartCentralDir(zipFile->cache))) {
		headerSize = 46;		/* central zip header size */
	}
	readLength = headerSize + (filename ? filenameLength : 128);
	if (findDirectory) {
		/* Extra byte for possible trailing '/' */
		readLength++;
	}

	/* Allocate some memory if necessary */
	if (readLength <= sizeof(buffer)) {
		current = buffer;
	} else {
		current = readBuffer = j9mem_allocate_memory(readLength, J9MEM_CATEGORY_VM_JCL);
		if (!readBuffer)
			return ZIP_ERR_OUT_OF_MEMORY;
	}

	currentEntryPointer = localEntryPointer = zipFile->pointer;

	result = (I_32)j9file_read(zipFile->fd, current, readLength);
	if ((result < 22) || (filename && !(result == readLength || (findDirectory && result == (readLength-1))))) {
		/* We clearly didn't get enough bytes */
		result = ZIP_ERR_FILE_READ_ERROR;
		goto finished;
	}
	zipFile->pointer += result;
	readLength = result;		/* If it's not enough, we'll catch that later */
	ZIP_NEXT_U32(sig, current);

	if (enumerationPointer) {
		if (sig == ZIP_CentralEnd) {
			result = ZIP_ERR_NO_MORE_ENTRIES;
			goto finished;
		}
	}
	if ((enumerationPointer || (!zipFile->cache)) && (sig == ZIP_DataDescriptor)) {
		/* We failed to predict a data descriptor here.  This should be an error (i.e. only happens in malformed zips?) 
		   but, but we will silently skip over it */
		seekResult = j9file_seek(zipFile->fd, currentEntryPointer + 16, EsSeekSet);
		if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF))) {
			zipFile->pointer = -1;
			result = ZIP_ERR_FILE_READ_ERROR;
			goto finished;
		}
		zipFile->pointer = (I_32)seekResult;

		if (zipFile->pointer == currentEntryPointer + 16) {
			if (readBuffer) {
				j9mem_free_memory(readBuffer);
			}
			goto retry;
		}
		result = ZIP_ERR_FILE_READ_ERROR;
		goto finished;
	}

	if ((sig != ZIP_CentralHeader) && (sig != ZIP_LocalHeader)) {
		/* Unexpected. */
		result = ZIP_ERR_FILE_CORRUPT;
		goto finished;
	}
	headerSize = ((sig == ZIP_CentralHeader) ? 46 : 30);
	if (readLength < headerSize) {
		/* We didn't get the whole header (and none of the filename).. */
		/* NOTE: this could happen in normal use if the assumed filename length above is <16.  Since it's 128, we don't 
		   handle the impossible case where we would have to read more header.  It could also happen if the caller
		   supplied a filename of length <16 but that only happens when we have a cache (so we'll know the header size) 
		 */
		result = ZIP_ERR_FILE_READ_ERROR;
	}
	readLength -= headerSize;

	if (sig == ZIP_CentralHeader) {
		current += 2;			/* skip versionCreated field */
	}
	ZIP_NEXT_U16(zipEntry->versionNeeded, current);
	ZIP_NEXT_U16(zipEntry->flags, current);
	ZIP_NEXT_U16(zipEntry->compressionMethod, current);
	ZIP_NEXT_U16(zipEntry->lastModTime, current);
	ZIP_NEXT_U16(zipEntry->lastModDate, current);
	ZIP_NEXT_U32(zipEntry->crc32, current);
	ZIP_NEXT_U32(zipEntry->compressedSize, current);
	ZIP_NEXT_U32(zipEntry->uncompressedSize, current);
	ZIP_NEXT_U16(zipEntry->filenameLength, current);
	ZIP_NEXT_U16(zipEntry->extraFieldLength, current);
	zipEntry->fileCommentLength = 0;

	if (sig == ZIP_CentralHeader) {
		ZIP_NEXT_U16(zipEntry->fileCommentLength, current);
		current += 8;			/* skip disk number start + internal attrs + external attrs */
		ZIP_NEXT_U32(localEntryPointer, current);
	}

	if (filename) {
		if (zipFile->cache) {
			if (!(readLength == zipEntry->filenameLength || (findDirectory && (readLength-1) == zipEntry->filenameLength))) {
				/* We knew exactly how much we were supposed to read, and this wasn't it */
				result = ZIP_ERR_FILE_CORRUPT;
				goto finished;
			}
		}
	}

	/* Allocate space for filename */
	if (zipEntry->filenameLength >= ZIP_INTERNAL_MAX) {
		zipEntry->filename = j9mem_allocate_memory(zipEntry->filenameLength + 1, J9MEM_CATEGORY_VM_JCL);
		if (!zipEntry->filename) {
			result = ZIP_ERR_OUT_OF_MEMORY;
			goto finished;
		}
	} else {
		zipEntry->filename = zipEntry->internalFilename;
	}
	if (readLength > zipEntry->filenameLength) {
		readLength = zipEntry->filenameLength;
	}
	memcpy(zipEntry->filename, current, readLength);

	/* Read the rest of the filename if necessary.  Allocate space in J9ZipEntry for it! */
	if (readLength < zipEntry->filenameLength) {
		result = (I_32)j9file_read(zipFile->fd, zipEntry->filename + readLength, zipEntry->filenameLength - readLength);
		if (result != (zipEntry->filenameLength - readLength)) {
			result = ZIP_ERR_FILE_READ_ERROR;
			goto finished;
		}
		zipFile->pointer += result;
	}
	zipEntry->filename[zipEntry->filenameLength] = '\0';

	/* If we know what filename is supposed to be, compare it and make sure it matches */
	/* Note: CASE-SENSITIVE COMPARE because filenames in zips are case sensitive (even on platforms with
	   case-insensitive file systems) */
	if (filename) {
		if (!((findDirectory && zipEntry->filenameLength == (filenameLength+1) && zipEntry->filename[filenameLength] == '/' &&
			!strncmp((char*)zipEntry->filename, (const char*)filename, filenameLength)) ||
			!strcmp((const char*)zipEntry->filename, (const char*)filename)))
		{
			/* We seem to have read something totally bogus.. */
			result = ZIP_ERR_FILE_CORRUPT;
			goto finished;
		}
	}

	zipEntry->filenamePointer = (I_32)(currentEntryPointer + headerSize);
	zipEntry->extraFieldPointer = (I_32)(localEntryPointer + 30 + zipEntry->filenameLength);
	/* Must always set the dataPointer as it may be used by scanForDataDescriptor()
	 * when reading a local header.
	 */
	zipEntry->dataPointer = zipEntry->extraFieldPointer + zipEntry->extraFieldLength;
	zipEntry->extraField = NULL;
	zipEntry->fileCommentPointer = 0;
	zipEntry->fileComment = NULL;
	zipEntry->data = NULL;

	if (sig == ZIP_CentralHeader) {
		U_8 buf[2];
		U_8 *buf2 = buf;
		U_16 lost;
		/* Also, we know where the comment is */
		zipEntry->fileCommentPointer = (I_32)(currentEntryPointer + headerSize +
			zipEntry->filenameLength + zipEntry->extraFieldLength);
		/* Fix the dataPointer when reading the central directory. The extraFieldLength in the central
		 * directory gives the size of the extra field data in the central directory, not the size of
		 * the extra field data in the corresponding local entry.
		 */
		if (readDataPointer) {
			if ( j9file_seek( zipFile->fd, localEntryPointer + 28, EsSeekSet ) == localEntryPointer+28 ) {
				if ( j9file_read( zipFile->fd, buf, 2 ) == 2 ) {
					ZIP_NEXT_U16( lost, buf2 );
					zipEntry->dataPointer = zipEntry->extraFieldPointer + lost;
					zipFile->pointer = (I_32)(localEntryPointer + 30);
				}
			}
		}
	}

	if ((sig == ZIP_LocalHeader) && (zipEntry->compressionMethod == ZIP_CM_Deflated)
		&& (zipEntry->flags & 0x8)) {
		/* What we just read doesn't tell us how big the compressed data is.  We have to do a heuristic search for a
		   valid data descriptor at the end of the compressed text */
		result = scanForDataDescriptor(portLib, zipFile, zipEntry);
		if (result < 0)
			goto finished;
	}

	/* Entry read successfully */

	if (enumerationPointer) {
		/* Work out where the next entry is supposed to be */
		*enumerationPointer = zipEntry->fileCommentPointer + zipEntry->fileCommentLength;
	}

	if (readBuffer)
		j9mem_free_memory(readBuffer);
	if (!readDataPointer) {
		zipEntry->dataPointer = 0;
	}
	return 0;

  finished:
	if (readBuffer) {
		j9mem_free_memory(readBuffer);
	}
	if ((zipEntry->filename) && (zipEntry->filename != zipEntry->internalFilename)) {
		j9mem_free_memory(zipEntry->filename);
	}
	zipEntry->filename = NULL;
	if (result == ZIP_ERR_FILE_READ_ERROR) {
		zipFile->pointer = -1;
	}
	return result;
}


/**
 * Attempts to release resources attached to the zipfile.
 * File descriptor is closed here only when cache is not being used.
 * Otherwise, file descriptor is closed only when there is no more reference to this zip file. See zipCache_kill in zipcache.c.
 *
 * @param[in] portLib the port library
 * @param[in] zipFile The zip file to be closed
 *
 * @return 0 on success
 * @return 	ZIP_ERR_FILE_CLOSE_ERROR if there is an error closing the file
 * @return 	ZIP_ERR_INTERNAL_ERROR if there is an internal error
 *
*/
I_32 
zip_releaseZipFile(J9PortLibrary* portLib, struct J9ZipFile* zipFile)
{
	PORT_ACCESS_FROM_PORT(portLib);

	IDATA fd;
	I_32 result = 0;
	J9ZipCachePool *cachePool = NULL; 

	ENTER();

	fd = zipFile->fd;
	cachePool = zipFile->cachePool;
	zipFile->fd = -1;

	if (zipFile->cache && cachePool)  {
		zipCachePool_release(cachePool, zipFile->cache);
		zipFile->cache = NULL;
	} else {
		if (-1 == fd) { 
			result = ZIP_ERR_INTERNAL_ERROR;
			goto finished;
		}
		if (j9file_close(fd)) {
			result = ZIP_ERR_FILE_CLOSE_ERROR;
			goto finished;
		}
	}

finished:
	if (cachePool) {
		TRIGGER_J9HOOK_VM_ZIP_LOAD(cachePool->hookInterface, portLib, cachePool->userData, (const struct J9ZipFile*)zipFile, J9ZIP_STATE_CLOSED, zipFile->filename, result);
	}
	if ((zipFile->filename) && (zipFile->filename != zipFile->internalFilename)) {
		j9mem_free_memory(zipFile->filename);
	}
	zipFile->filename = NULL;
	EXIT();
	return result;
}


/** Search cachePool for a zip file.
 *
 * @param[in] portLib the port library
 * @param[in] filename name of the zip file to be searched in cachePool
 * @param[in] cachePool pool of zip files
 * @param[out] cache cache in cachePool corresponding to zip file
 *
 * @return 0 on success
 * @return	ZIP_ERR_INTERNAL_ERROR if there was an internal error
 */

I_32
zip_searchCache(J9PortLibrary * portLib, char *filename, J9ZipCachePool *cachePool, J9ZipCache **cache) {

	PORT_ACCESS_FROM_PORT(portLib);
	I_32 result = 0;
	I_64 timeStamp, actualFileSize;
	IDATA fileSize, filenameLength;

	*cache = NULL;
	/* Check the cachePool for a suitable cache. */
	filenameLength = strlen((const char*)filename);
	timeStamp = j9file_lastmod((const char*)filename);
	actualFileSize = j9file_length((const char*)filename);
	if ((actualFileSize < 0) || (actualFileSize > J9CONST64(0x7FFFFFFF))) {
		result = ZIP_ERR_INTERNAL_ERROR;
		goto finished;
	}
	fileSize = (IDATA) actualFileSize;
	*cache = zipCachePool_findCache(cachePool, (const char*)filename, filenameLength, fileSize, timeStamp);

finished:
	return result;
}


/** 
 * Called to set up the cache when a zip file is opened with a cachePool. Either uses
 * an existing cache in the cache pool, or create a new cache.
 * If a new cache is created it is not populated, use zip_readCacheData() to populate the cache.
 * On error, the zipFile is closed.
 * The omrthread_global_monitor() must be acquired before calling this function.
 * 
 * @param[in] portLib the port library
 * @param[in] zipFile the zip file for which we want to establish a cache
 * @param[in] cache cache for the zip file if zip file already exists in the cachePool
 * @param[in] cachePool the zip cache pool
 * 
 * @return 0 on success
 * @return 	ZIP_ERR_FILE_READ_ERROR if there is an error reading from zipFile 
 * @return	ZIP_ERR_FILE_OPEN_ERROR if is there is an error opening the file 
 * @return	ZIP_ERR_UNKNOWN_FILE_TYPE if the file type is unknown
 * @return	ZIP_ERR_UNSUPPORTED_FILE_TYPE if the file type is unsupported
 * @return	ZIP_ERR_OUT_OF_MEMORY  if there is not enough memory to complete this call
 * @return	ZIP_ERR_INTERNAL_ERROR if there was an internal error
 */
I_32
zip_setupCache(J9PortLibrary * portLib, J9ZipFile *zipFile, J9ZipCache *cache, J9ZipCachePool *cachePool)
{
	PORT_ACCESS_FROM_PORT(portLib);	
	I_32 result = 0;
	I_64 timeStamp, actualFileSize;
	IDATA fileSize, filenameLength;

	if (zipFile->cache)  {
		if (zipFile->cachePool)  {
			/* Invalidate the cache to keep other people from starting to use it (we will create a new one for them
				to start to use instead).  Once all the current users of the cache have stopped using it, it will go away */
			zipCache_invalidateCache(zipFile->cache);
			zipCachePool_release(zipFile->cachePool, zipFile->cache);
		}
		zipFile->cache = NULL;
	}
	/* The cachePool cannot be NULL. */
	if (!cachePool) {
		result = ZIP_ERR_INTERNAL_ERROR;
		goto finished;
	}
	
	filenameLength = strlen((const char*)zipFile->filename);
	timeStamp = j9file_lastmod((const char*)zipFile->filename);
	actualFileSize = j9file_length((const char*)zipFile->filename);
	if ((actualFileSize < 0) || (actualFileSize > J9CONST64(0x7FFFFFFF))) {
		result = ZIP_ERR_INTERNAL_ERROR;
		goto finished;
	}
	fileSize = (IDATA) actualFileSize;

	zipFile->cachePool = cachePool;

	if (NULL != cache) {
		zipFile->cache = cache;
		/* A cache was found, report success. */
		TRIGGER_J9HOOK_VM_ZIP_LOAD(cachePool->hookInterface, portLib, cachePool->userData, (const struct J9ZipFile*)zipFile, J9ZIP_STATE_OPEN, zipFile->filename, 0);
	} else {
		/* Build a new cache.  Because caller asked for a cache, fail if we can't provide one */
		zipFile->cache = zipCache_new(portLib, (char *)zipFile->filename, filenameLength, fileSize, timeStamp);
		if (!zipFile->cache || !zipCachePool_addCache(zipFile->cachePool, zipFile->cache)) {
			result = ZIP_ERR_OUT_OF_MEMORY;
		} else {
			J9ZipCacheInternal *zci = (J9ZipCacheInternal *)zipFile->cache;
			zci->zipFileFd = zipFile->fd;
			zci->zipFileType = zipFile->type;
		}
	}

finished:
	if (result) {
		/* Only report failure, success is reported by zip_readCacheData(). Call the hook before freeing the zipFile->filename. */
		if (NULL != cachePool) {
			TRIGGER_J9HOOK_VM_ZIP_LOAD(cachePool->hookInterface, portLib, cachePool->userData, (const struct J9ZipFile*)zipFile, J9ZIP_STATE_OPEN, zipFile->filename, result);
		}
		if (zipFile->cache) {
			zipCache_kill(zipFile->cache);
			zipFile->cache = NULL;
		}
		/* NULL the cachePool so zip_releaseZipFile will not call the J9ZIP_STATE_CLOSE hook. */
		zipFile->cachePool = NULL;
		zip_releaseZipFile(portLib, zipFile);
	}
	return result;
}


/** 
 * Called to populate an empty cache and install it in the zip cache pool.
 * On error, the zipFile is closed.  The omrthread_global_monitor() must be
 * acquired before calling this function.
 * 
 * @param[in] portLib the port library
 * @param[in] zipFile the zip file for which we want to establish a cache
 * 
 * @return 0 on success
 * @return 	ZIP_ERR_FILE_READ_ERROR if there is an error reading from zipFile 
 * @return	ZIP_ERR_FILE_OPEN_ERROR if is there is an error opening the file 
 * @return	ZIP_ERR_UNKNOWN_FILE_TYPE if the file type is unknown
 * @return	ZIP_ERR_UNSUPPORTED_FILE_TYPE if the file type is unsupported
 * @return	ZIP_ERR_OUT_OF_MEMORY  if there is not enough memory to complete this call
 * @return	ZIP_ERR_INTERNAL_ERROR if there was an internal error
 */
I_32
zip_readCacheData(J9PortLibrary * portLib, J9ZipFile *zipFile)
{
	I_32 result = 0;
	J9ZipCentralEnd endEntry;
	IDATA startCentralDir;
	
	/* zip_setupCache() must have already been called. */
	if (!zipFile->cachePool || !zipFile->cache) {
		result = ZIP_ERR_INTERNAL_ERROR;
		goto finished;
	}

	/* if the cache has already been populated, just return */
	if (zipCache_hasData(zipFile->cache)){
		return 0;
	}

	/* Find and read the end-of-central-dir record. */
	result = scanForCentralEnd(portLib, zipFile, &endEntry);
	if (result == 0) {
		startCentralDir = (IDATA)((UDATA)endEntry.dirOffset);
		zipCache_setStartCentralDir(zipFile->cache, startCentralDir);
		result = zip_populateCache(portLib, zipFile, &endEntry, startCentralDir);
	}

finished:
	/* Call the hook before freeing the zipFile->filename. */
	if (zipFile->cachePool) {
		J9ZipCachePool *cachePool = zipFile->cachePool;
		TRIGGER_J9HOOK_VM_ZIP_LOAD(cachePool->hookInterface, portLib, cachePool->userData, (const struct J9ZipFile*)zipFile, J9ZIP_STATE_OPEN, zipFile->filename, result);
	}
	if (result)  {
		if (zipFile->cachePool && zipFile->cache) {
			zipCachePool_release(zipFile->cachePool, zipFile->cache);
		}
		zipFile->cache = NULL;
		/* NULL the cachePool so zip_releaseZipFile will not call the J9ZIP_STATE_CLOSE hook */
		zipFile->cachePool = NULL;
		zip_releaseZipFile(portLib, zipFile);
	}
	return result;
}


/**
 * Initialize a zip entry.
 * 
 * Should be called before the entry is passed to any other zip support functions 
 *
 * @param[in] portLib the port library
 * @param[in] entry the zip entry to init
 *
 * @return none
*/

void zip_initZipEntry(J9PortLibrary* portLib, J9ZipEntry* entry)
{
	memset(entry, 0, sizeof(*entry));
}


/**
 * Free any memory associated with a zip entry.
 * 
 * @param[in] portLib the port library
 * @param[in] entry the zip entry we are freeing 
 * 
 * @return none 
 * 
 * @note This does not free the entry itself.
*/

void zip_freeZipEntry(J9PortLibrary * portLib, J9ZipEntry * entry)
{
	PORT_ACCESS_FROM_PORT(portLib);

	if ((entry->filename) && (entry->filename != entry->internalFilename)) {
		j9mem_free_memory(entry->filename);
	}
	entry->filename = NULL;
	if (entry->extraField) {
		j9mem_free_memory(entry->extraField);
		entry->extraField = NULL;
	}
	if (entry->data) {
		j9mem_free_memory(entry->data);
		entry->data = NULL;
	}
	if (entry->fileComment) {
		j9mem_free_memory(entry->fileComment);
		entry->fileComment = NULL;
	}
}



/**
 *	Read the next zip entry at nextEntryPointer into zipEntry.
 *	
 * Any memory held onto by zipEntry may be lost, and therefore
 *	MUST be freed with @ref zip_freeZipEntry first.
 *
 * @param[in] portLib the port library
 * @param[in] zipFile The zip file being read
 * @param[out] zipEntry compressed data is placed here
 * @param[in] nextEntryPointer
 * @param[in] readDataPointer do extra work to read the data pointer, if false the dataPointer will be 0
 * 
 * @return 0 on success
 * @return 	ZIP_ERR_FILE_READ_ERROR if there is an error reading zipFile
 * @return ZIP_ERR_FILE_CORRUPT if zipFile is corrupt
 * @return 	ZIP_ERR_NO_MORE_ENTRIES if there are no more entries in zipFile
 * @return 	ZIP_ERR_OUT_OF_MEMORY if there is not enough memory to complete this call
 *
 * @see zip_freeZipEntry
 *
*/
I_32 zip_getNextZipEntry(J9PortLibrary* portLib, J9ZipFile* zipFile, J9ZipEntry* zipEntry, IDATA* nextEntryPointer, BOOLEAN readDataPointer)
{
	PORT_ACCESS_FROM_PORT(portLib);
	I_32 result;
	BOOLEAN retryAllowed = TRUE;
	IDATA pointer;
	IDATA entryStart;
	I_64 seekResult;

	ENTER();

/*[CMVC 123208] omrthread_monitor_enter() called before checking zip cache
 * the zip cache may be rebuilt as the zip file can be changed by the
 * process that has it open.
 */
retry:
	pointer = *nextEntryPointer;

	/* Seek to the entry's position in the file. */
	if (pointer != zipFile->pointer)  {
		zipFile->pointer = (I_32) pointer;
	}
	seekResult =  j9file_seek(zipFile->fd, zipFile->pointer, EsSeekSet);
	if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF)) || (zipFile->pointer != seekResult)) {
		zipFile->pointer = -1;
		EXIT();
		return ZIP_ERR_FILE_READ_ERROR;
	}

	/* Read the entry */
	entryStart = *nextEntryPointer;
	result = readZipEntry(portLib, zipFile, zipEntry, NULL, 0, &pointer, &entryStart, FALSE, readDataPointer);
	if (result != 0)  {
		if (!retryAllowed || (result == ZIP_ERR_NO_MORE_ENTRIES) || !zipFile->cachePool) {
			EXIT();
			return result;
		}
		/* invalidate existing cache */
		result = zip_setupCache(portLib, zipFile, NULL, zipFile->cachePool);
		if (!result) {
			result = zip_readCacheData(portLib, zipFile);
		}
		if (result) {
			EXIT();
			return result;
		}
		retryAllowed = FALSE;
		goto retry;
	}

	*nextEntryPointer = pointer;
	EXIT();
	return 0;
}


/**
 *	Attempt to find and read the zip entry corresponding to filename.
 *	If found, read the entry into the parameter entry. 
 *
 * If an uncached entry is found, the filename field will be filled in. This
 * memory will have to be freed with @ref zip_freeZipEntry.
 * 
 * @param[in] portLib the port library
 * @param[in] zipFile the file being read from
 * @param[out] entry the zip entry found in zipFile is read to here
 * @param[in] filename the name of the file that corresponds to entry
 * @param[in] fileNameLength length of filename
 * @param[in] flags
 *
 * Valid flags are:
 * J9ZIP_GETENTRY_FIND_DIRECTORY: match a directory even if filename does not end in '/'.
 * J9ZIP_GETENTRY_READ_DATA_POINTER: do extra work to read the data pointer, if false the dataPointer will be 0.
 * J9ZIP_GETENTRY_USE_CENTRAL_DIRECTORY: if there is no cache and this bit is set, look in the central directory , otherwise do a linear scan of the local headers.
 *
 * @return 0 on success or one of the following:
 * @return	ZIP_ERR_FILE_READ_ERROR if there is an error reading from zipFile
 * @return	ZIP_ERR_FILE_CORRUPT if zipFile is corrupt
 * @return	ZIP_ERR_ENTRY_NOT_FOUND if a zip entry with name filename was not found
 * @return 	ZIP_ERR_OUT_OF_MEMORY if there is not enough memory to complete this call
 *
 * @see zip_freeZipEntry
*/

I_32
zip_getZipEntry(J9PortLibrary * portLib, J9ZipFile * zipFile, J9ZipEntry *entry, const char *filename, IDATA fileNameLength, U_32 flags)
{
	PORT_ACCESS_FROM_PORT(portLib);
	I_32 status = 0;
	I_32 result = 0;
	IDATA position = -1;
	BOOLEAN retryAllowed = TRUE;
	I_64 seekResult = -1;
	BOOLEAN findDirectory = J9_ARE_ANY_BITS_SET(flags, J9ZIP_GETENTRY_FIND_DIRECTORY);
	BOOLEAN readDataPointer = J9_ARE_ANY_BITS_SET(flags, J9ZIP_GETENTRY_READ_DATA_POINTER);
	ENTER();

/*[CMVC 123208] omrthread_monitor_enter() called before checking zip cache
 * the zip cache may be rebuilt as the zip file can be changed by the
 * process that has it open.
 */
  retry:
	if (zipFile->cache) {
		/* Look up filename in the cache. */
		position = (IDATA) zipCache_findElement(zipFile->cache, filename, fileNameLength, findDirectory);
		if (-1 == position) {
			/* Note: we assume the cache is still valid here */
			status = ZIP_ERR_ENTRY_NOT_FOUND;
			goto finished;
		}

		/* Seek to the entry's position in the file. */
		if (zipFile->pointer != position) {
			zipFile->pointer = (I_32) position;
		}
		seekResult =  j9file_seek(zipFile->fd, zipFile->pointer, EsSeekSet);
		if (isSeekFailure(seekResult, zipFile->pointer)) {
			zipFile->pointer = -1;
			status = ZIP_ERR_FILE_READ_ERROR;
			goto finished;
		}

		/* Read the entry */
		result = readZipEntry(portLib, zipFile, entry, filename, fileNameLength, NULL, NULL, findDirectory, readDataPointer);
		if (0 != result) {
			if (!retryAllowed) {
				status = result;
				goto finished;
			}
			/* invalidate existing cache */
			result = zip_setupCache(portLib, zipFile, NULL, zipFile->cachePool);
			if (0 == result) {
				result = zip_readCacheData(portLib, zipFile);
			}
			if (0 != result) {
				status = result;
				goto finished;
			}
			retryAllowed = FALSE;
			goto retry;
		}
		status = 0;
	} else if (J9_ARE_ANY_BITS_SET(flags, J9ZIP_GETENTRY_USE_CENTRAL_DIRECTORY)) {
		status = getZipEntryUsingDirectory(portLib, zipFile, entry, filename, fileNameLength, readDataPointer);
	} else {
		/* Linear search of local headers (slow) */
		position = 0;
		zip_resetZipFile(PORTLIB, zipFile, &position);
		while (TRUE) {

			if (zipFile->pointer != position) {
				zipFile->pointer = (I_32)position;
			}
			seekResult = j9file_seek(zipFile->fd, zipFile->pointer, EsSeekSet);
			if (isSeekFailure(seekResult, zipFile->pointer)) {
				zipFile->pointer = -1;
				status = ZIP_ERR_FILE_READ_ERROR;
				break;
			}

			result = readZipEntry(portLib, zipFile, entry, NULL, 0, &position, NULL, FALSE, readDataPointer);
			if ((0 != result) || (0 == strncmp((const char*)entry->filename, filename, fileNameLength))) {
				status = result;
				break;
			}

			/* No match.  Reset for next entry */
			zip_freeZipEntry(portLib, entry);
			zip_initZipEntry(portLib, entry);
		}
	}
finished:
	EXIT();
	return status;
}

/**
 *	Attempt to find and read the zip entry corresponding to fileName
 *	using the central repository. This is intended to be used with zip files
 *	containing shell scripts or other data before the zip file proper.
 *	It assumes that the "end of central directory record" follows
 *	the central directory immediately.
 *
 * @param[in] portLib the port library
 * @param[in] zipFile the file being read from
 * @param[out] entry the zip entry found in zipFile is read to here
 * @param[in] fileName the name of the file that corresponds to entry
 * @param[in] fileNameLength length of filename
 * @param[in] readDataPointer do extra work to read the data pointer, if false the dataPointer will be 0
 * @return 0 on success or one of the following:
 * @return	ZIP_ERR_FILE_READ_ERROR if there is an error reading from zipFile
 * @return	ZIP_ERR_FILE_CORRUPT if zipFile is corrupt
 * @return	ZIP_ERR_ENTRY_NOT_FOUND if a zip entry with name filename was not found
 * @return 	ZIP_ERR_OUT_OF_MEMORY if there is not enough memory to complete this call
 */

/* size of the fixed-size fields */
#define CENTRAL_FILEHEADER_SIZE 46
static I_32
getZipEntryUsingDirectory(J9PortLibrary *portLib, J9ZipFile *zipFile, J9ZipEntry *entry,
		const char *fileName, IDATA fileNameLength, BOOLEAN readDataPointer)
{
	PORT_ACCESS_FROM_PORT(portLib);

	J9ZipCentralEnd endEntry;
	I_32 result = ZIP_ERR_ENTRY_NOT_FOUND;
	U_8 defaultBuffer[128]; /* must be at least CENTRAL_FILEHEADER_SIZE */
	U_8 *readBuffer = defaultBuffer;
	U_16 entryCount = 0;
	int64_t nextEntryPointer = 0;
	int64_t offsetCorrection = 0; /* this is the size of any crud before the first local file header */

	if (fileNameLength > sizeof(defaultBuffer)) {
		readBuffer = j9mem_allocate_memory(fileNameLength, J9MEM_CATEGORY_VM_JCL);
		if (NULL == readBuffer) {
			result = ZIP_ERR_OUT_OF_MEMORY;
			goto finished;
		}
	}
	if (0 !=  scanForCentralEnd(portLib, zipFile, &endEntry)) {
		result = ZIP_ERR_FILE_CORRUPT;
		goto finished;
	}
	nextEntryPointer = endEntry.endCentralDirRecordPosition - endEntry.dirSize; /* assume no ZIP64 records or digital signature */
	offsetCorrection = nextEntryPointer - endEntry.dirOffset;

	for (entryCount = 0; entryCount < endEntry.totalEntries; ++entryCount) {
		U_8 *current = readBuffer;
		U_32 sig = 0;
		I_64 seekResult = j9file_seek(zipFile->fd, nextEntryPointer, EsSeekSet);
		if (isSeekFailure(seekResult, (I_64) nextEntryPointer)) {
			result = ZIP_ERR_FILE_READ_ERROR;
			break;
		}
		if (CENTRAL_FILEHEADER_SIZE != j9file_read(zipFile->fd, readBuffer, CENTRAL_FILEHEADER_SIZE))  {
			zipFile->pointer = -1;
			break;
		}
		ZIP_NEXT_U32(sig, current);
		if (ZIP_CentralEnd == sig)  {
			result = ZIP_ERR_ENTRY_NOT_FOUND;
			break;
		} else if (ZIP_CentralHeader != sig)  {
			/* This is unexpected. */
			result = ZIP_ERR_FILE_CORRUPT;
			break;
		} else {
			U_32 entryNameLength = 0;
			U_32 extraFieldLength = 0;
			U_32 commentLength = 0;
			current += 24; /* skip to filename length */
			/* read the lengths of the file name, extra field, and comment and following fixed-length fields */
			ZIP_NEXT_U16(entryNameLength, current);
			ZIP_NEXT_U16(extraFieldLength, current);
			ZIP_NEXT_U16(commentLength, current);
			if (entryNameLength == fileNameLength) {
				U_32 localHeaderOffset = 0;
				current += 8; /* skip disk number, attributes */
				ZIP_NEXT_U32(localHeaderOffset, current);
				if (fileNameLength != j9file_read(zipFile->fd, readBuffer, fileNameLength))  {
					zipFile->pointer = -1;
					result = ZIP_ERR_FILE_READ_ERROR;
					break;
				}
				if (0 == strncmp(fileName, readBuffer, fileNameLength)) {
					zipFile->pointer = (I_32)(localHeaderOffset + offsetCorrection);
					seekResult = j9file_seek(zipFile->fd,zipFile->pointer, EsSeekSet); /* go to the actual entry */
					if (isSeekFailure(seekResult, zipFile->pointer)) {
						result = ZIP_ERR_FILE_READ_ERROR;
					} else {
						result = readZipEntry(portLib, zipFile, entry, fileName, fileNameLength, NULL, NULL, FALSE, readDataPointer);
					}
					break;
				}
			}
			/* didn't find the file */
			nextEntryPointer += CENTRAL_FILEHEADER_SIZE + entryNameLength + extraFieldLength + commentLength;
		}
	}
	finished:
	if (defaultBuffer != readBuffer) {
		j9mem_free_memory(readBuffer);
	}
	return result;
}

/** 
 *	Attempt to read and uncompress the data for the zip entry entry.
 * 
 *	If buffer is non-NULL it is used, but not explicitly held onto by the entry.
 *	If buffer is NULL, memory is allocated and held onto by the entry, and thus
 *	should later be freed with @ref zip_freeZipEntry.
 *
 * @param[in] portLib the port library
 * @param[in] zipFile the zip file being read from.
 * @param[in,out] entry the zip entry
 * @param[in] buffer may or may not be NULL
 * @param[in] bufferSize

 * @return 0 on success
 * @return	ZIP_ERR_FILE_READ_ERROR if there is an error reading from zipEntry
 * @return	ZIP_ERR_FILE_CORRUPT if zipFile is corrupt
 * @return	ZIP_ERR_ENTRY_NOT_FOUND if entry is not found
 * @return 	ZIP_ERR_OUT_OF_MEMORY  if there is not enough memory to complete this call
 * @return 	ZIP_ERR_BUFFER_TOO_SMALL if buffer is too small to hold the comment for zipFile
 *
 * @see zip_freeZipEntry
 *
*/
I_32 zip_getZipEntryData(J9PortLibrary* portLib, J9ZipFile* zipFile, J9ZipEntry* entry, U_8* buffer, U_32 bufferSize)
{
	PORT_ACCESS_FROM_PORT(portLib);

	I_32 result;
	U_8* dataBuffer;
	struct workBuffer wb;
	I_64 seekResult;

	ENTER();

	wb.portLib = portLib;
	wb.bufferStart = wb.bufferEnd = wb.currentAlloc = 0;

	if(buffer) {
		if(bufferSize < entry->uncompressedSize) {
			EXIT();
			return ZIP_ERR_BUFFER_TOO_SMALL;
		}
		dataBuffer = buffer;
	} else {
		/* Note that this is the first zalloc. This memory must be available to the calling method and is freed explicitly in zip_freeZipEntry. */
		/* Note that other allocs freed in zip_freeZipEntry are not alloc'd using zalloc */
		if (entry->compressionMethod == ZIP_CM_Stored) {
			/* no inflation, so only allocate the uncompressedSize */
			dataBuffer = j9mem_allocate_memory(entry->uncompressedSize, J9MEM_CATEGORY_VM_JCL);
		} else {
			/* inflation required, zdataalloc will allocate extra space */
			dataBuffer = zdataalloc(&wb, 1, entry->uncompressedSize);
		}
		if(!dataBuffer) {
			EXIT(); 
			return ZIP_ERR_OUT_OF_MEMORY;
		}
		entry->data = dataBuffer;
	}

	if(entry->compressionMethod == ZIP_CM_Stored) {
		/* No compression - just read the data in. */
		if (zipFile->pointer != entry->dataPointer)  {
			zipFile->pointer = (I_32) entry->dataPointer;
		}
		seekResult =  j9file_seek(zipFile->fd, zipFile->pointer, EsSeekSet);
		if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF)) || (zipFile->pointer != seekResult)) {
			zipFile->pointer = -1;
			result = ZIP_ERR_FILE_READ_ERROR;
			goto finished;
		}
		result = (I_32)j9file_read(zipFile->fd, dataBuffer, entry->compressedSize);
		if (result != (I_32)entry->compressedSize) {
			result = ZIP_ERR_FILE_READ_ERROR;
			goto finished;
		}
		zipFile->pointer += result;
		EXIT();
		return 0;
	}

	if(entry->compressionMethod == ZIP_CM_Deflated) {
		U_8* readBuffer;

		/* Read the file contents. */
		if (entry->compressedSize < ZIP_WORK_BUFFER_SIZE) {
			J9ZipCachePool *cachePool = zipFile->cachePool;
			if (cachePool != NULL) {
				/* The buffer is allocated in zip_populateCache() */
				if (cachePool->workBuffer != NULL) {
					wb.bufferStart = wb.currentAlloc = (UDATA *)cachePool->workBuffer;
					wb.bufferEnd = (UDATA*)((UDATA)wb.bufferStart + ZIP_WORK_BUFFER_SIZE);
					/* set the cntr to 1 so the memory does not get freed */
					wb.cntr = 1;
				}
			}
		}
		readBuffer = zdataalloc(&wb, 1, entry->compressedSize);
		if(!readBuffer) {
			result = ZIP_ERR_OUT_OF_MEMORY;
			goto finished;
		}
		if (zipFile->pointer != entry->dataPointer)  {
			zipFile->pointer = (I_32)entry->dataPointer;
		}
		seekResult =  j9file_seek(zipFile->fd, zipFile->pointer, EsSeekSet);
		if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF)) || (zipFile->pointer != seekResult)) {
			zipFile->pointer = -1;
			zdatafree(&wb, readBuffer);
			result = ZIP_ERR_FILE_READ_ERROR;
			goto finished;
		}
		if(j9file_read(zipFile->fd, readBuffer, entry->compressedSize) != (I_32)entry->compressedSize) {
			zdatafree(&wb, readBuffer);
			result = ZIP_ERR_FILE_READ_ERROR;
			goto finished;
		}
		zipFile->pointer += (I_32)entry->compressedSize;

		/* Deflate the data. */
		result = inflateData(&wb, readBuffer, entry->compressedSize, dataBuffer, entry->uncompressedSize);
		zdatafree(&wb, readBuffer);
		if(result)  goto finished;
		EXIT();
		return 0;
	}

	/* Whatever this is, we can't decompress it */
	result = ZIP_ERR_UNSUPPORTED_FILE_TYPE;

finished:
	if(!buffer) {
		entry->data = NULL;
		zdatafree(&wb, dataBuffer);
	}
	if (result == ZIP_ERR_FILE_READ_ERROR)  {
		zipFile->pointer = -1;
	}
	EXIT();
	return result;
}

/** 
 *	Attempt to read the raw data for the zip entry entry.
 * 
 * @param[in] portLib the port library
 * @param[in] zipFile the zip file being read from.
 * @param[in,out] entry the zip entry
 * @param[in] buffer may not be NULL
 * @param[in] bufferSize 
 * @param[in] offset from the start of the entry data

 * @return 0 on success
 * @return	ZIP_ERR_FILE_READ_ERROR if there is an error reading from zipEntry
 * @return	ZIP_ERR_FILE_CORRUPT if zipFile is corrupt
 * @return	ZIP_ERR_ENTRY_NOT_FOUND if entry is not found
 * @return 	ZIP_ERR_OUT_OF_MEMORY  if there is not enough memory to complete this call
 * @return 	ZIP_ERR_BUFFER_TOO_SMALL if buffer is too small to hold the comment for zipFile
 *
 * @see zip_freeZipEntry
 *
*/
I_32 zip_getZipEntryRawData(J9PortLibrary* portLib, J9ZipFile* zipFile, J9ZipEntry* entry, U_8* buffer, U_32 bufferSize, U_32 offset)
{
	PORT_ACCESS_FROM_PORT(portLib);

	I_32 result;
	I_64 seekResult;

	ENTER();

	if((offset + bufferSize) > entry->compressedSize) {
		EXIT();
		/* Trying to read past the end of the data. */
		return ZIP_ERR_INTERNAL_ERROR;
	}

	/* Just read the data in. */
	if (zipFile->pointer != (I_32)(entry->dataPointer + offset))  {
		zipFile->pointer = (I_32) (entry->dataPointer + offset);
	}
	seekResult =  j9file_seek(zipFile->fd, zipFile->pointer, EsSeekSet);
	if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF)) || (zipFile->pointer != seekResult)) {
		result = ZIP_ERR_FILE_READ_ERROR;
		goto finished;
	}
	result = (I_32)j9file_read(zipFile->fd, buffer, bufferSize);
	if (result != (IDATA)bufferSize) {
		result = ZIP_ERR_FILE_READ_ERROR;
		goto finished;
	}
	zipFile->pointer += result;
	EXIT();
	return 0;

finished:
	if (result == ZIP_ERR_FILE_READ_ERROR)  {
		zipFile->pointer = -1;
	}
	EXIT();
	return result;
}

/** 
 *	Read the extra field of entry from the zip file filename. 
 *
 * buffer is used if non-NULL, but is not held onto by entry. 
 *
 * If buffer is NULL, memory is allocated and held onto by entry, and MUST be freed later with
 *	@ref zip_freeZipEntry.
 *
 * @param[in] portLib the port library
 * @param[in] zipFile the zip file being read from.
 * @param[in,out] entry the zip entry concerned
 * @param[in] buffer may or may not be NULL
 * @param[in] bufferSize
 *
 * @return 0 on success or one of the following:
 * @return ZIP_ERR_FILE_READ_ERROR if there is an error reading from zipFile
 * @return 	ZIP_ERR_FILE_CORRUPT if zipFile is corrupt
 * @return 	ZIP_ERR_OUT_OF_MEMORY if there is not enough memory to complete this call
 * @return 	ZIP_ERR_BUFFER_TOO_SMALL if the buffer was non-Null but not large enough to hold the contents of entry
 *
 * @see zip_freeZipEntry
*/
I_32 zip_getZipEntryExtraField(J9PortLibrary* portLib, J9ZipFile* zipFile, J9ZipEntry* entry, U_8* buffer, U_32 bufferSize)
{
	PORT_ACCESS_FROM_PORT(portLib);

	I_32 result;
	U_8* extraFieldBuffer;
	I_64 seekResult;

	ENTER();

	if (entry->extraFieldLength == 0)  {
		EXIT();
		return 0;
	}

	if(buffer) {
		if(bufferSize < entry->extraFieldLength) {
			EXIT();
			return ZIP_ERR_BUFFER_TOO_SMALL; 
		}
		extraFieldBuffer = buffer;
	} else {
		extraFieldBuffer = j9mem_allocate_memory(entry->extraFieldLength, J9MEM_CATEGORY_VM_JCL);
		if(!extraFieldBuffer) {
			EXIT();
			return ZIP_ERR_OUT_OF_MEMORY;
		}
		entry->extraField = extraFieldBuffer;
	}

	if (zipFile->pointer != entry->extraFieldPointer)  {
		zipFile->pointer = (I_32) entry->extraFieldPointer;
	}
	seekResult = j9file_seek(zipFile->fd, zipFile->pointer, EsSeekSet);
	if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF)) || (zipFile->pointer != seekResult)) {
		zipFile->pointer = -1;
		result = ZIP_ERR_FILE_READ_ERROR;
		goto finished;
	}
	result = (I_32)j9file_read(zipFile->fd, extraFieldBuffer, entry->extraFieldLength);
	if (result != (I_32)entry->extraFieldLength) {
		result = ZIP_ERR_FILE_READ_ERROR;
		goto finished;
	}
	zipFile->pointer += result;
	EXIT();
	return 0;

finished:
	if(!buffer) {
		entry->extraField = NULL;
		j9mem_free_memory(extraFieldBuffer);
	}
	if (result == ZIP_ERR_FILE_READ_ERROR)  zipFile->pointer = -1;
	EXIT();
	return result;

}



/**
 *	Read the file comment for entry. 
 *
 * If buffer is non-NULL, it is used, but not held onto by entry. 
 *
 * If buffer is NULL, memory is allocated and
 *	held onto by entry, and thus should later be freed with @ref zip_freeZipEntry.
 *
 * @param[in] portLib the port library
 * @param[in] zipFile the zip file concerned
 * @param[in] entry the entry who's comment we want
 * @param[in] buffer may or may not be NULL
 * @param[in] bufferSize

 * @return 0 on success or one of the following
 * @return	ZIP_ERR_FILE_READ_ERROR if there is an error reading the file comment from zipEntry
 * @return	ZIP_ERR_FILE_CORRUPT if zipFile is corrupt
 * @return	ZIP_ERR_ENTRY_NOT_FOUND if entry is not found
 * @return 	ZIP_ERR_OUT_OF_MEMORY  if there is not enough memory to complete this call
 * @return 	ZIP_ERR_BUFFER_TOO_SMALL if buffer is too small to hold the comment for zipFile
*/

I_32 zip_getZipEntryComment(J9PortLibrary * portLib, J9ZipFile * zipFile, J9ZipEntry * entry, U_8 * buffer,
							U_32 bufferSize)
{
	PORT_ACCESS_FROM_PORT(portLib);

	I_32 result;
	U_8 *fileCommentBuffer;
	I_64 seekResult;

	ENTER();

	if (entry->fileCommentLength == 0) {
		if (entry->fileCommentPointer == -1) {
			/* TODO: we don't know where the comment is (or even if there is one)! This only happens if you're running
			   without a cache, so too bad for now */
		}
		EXIT();
		return 0;
	}

	if (buffer) {
		if (bufferSize <= entry->fileCommentLength) {
			EXIT();
			return ZIP_ERR_BUFFER_TOO_SMALL;
		}
		fileCommentBuffer = buffer;
	} else {
		fileCommentBuffer = j9mem_allocate_memory(entry->fileCommentLength+1, J9MEM_CATEGORY_VM_JCL);
		if (!fileCommentBuffer) {
			EXIT();
			return ZIP_ERR_OUT_OF_MEMORY;
		}
		entry->fileComment = fileCommentBuffer;
	}

	if (zipFile->pointer != entry->fileCommentPointer) {
		zipFile->pointer = (I_32) entry->fileCommentPointer;
	}
	seekResult =  j9file_seek(zipFile->fd, zipFile->pointer, EsSeekSet);
	if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF)) || (zipFile->pointer != seekResult)) {
		zipFile->pointer = -1;
		result = ZIP_ERR_FILE_READ_ERROR;
		goto finished;
	}
	result = (I_32)j9file_read(zipFile->fd, fileCommentBuffer, entry->fileCommentLength);
	if (result != (I_32) entry->fileCommentLength) {
		result = ZIP_ERR_FILE_READ_ERROR;
		goto finished;
	}
	fileCommentBuffer[entry->fileCommentLength] = '\0';
	zipFile->pointer += result;
	EXIT();
	return 0;

  finished:
	if (!buffer) {
		entry->fileComment = NULL;
		j9mem_free_memory(fileCommentBuffer);
	}
	if (result == ZIP_ERR_FILE_READ_ERROR) {
		zipFile->pointer = -1;
	}
	EXIT();
	return result;

}

/*
 * initializing the hook interface in J9ZipCachePool
 */
IDATA
zip_initZipCachePoolHookInterface(J9PortLibrary* portLib, J9ZipCachePool *cachePool)
{
		J9HookInterface** mhookInterface = J9_HOOK_INTERFACE(cachePool->hookInterface);

		PORT_ACCESS_FROM_PORT(portLib);
		
		/* init the hook interface in J9ZipCachePool
		 * shutdown is done in jvminit.c freeJavaVM 
		 */	
		if (J9HookInitializeInterface(mhookInterface, OMRPORT_FROM_J9PORT(PORTLIB), sizeof(cachePool->hookInterface))) {
			/* J9ZipCachePool hook interface init failed */
			return -1;
		}
		return 0;
}

/*
 * shut down the hook interface in J9ZipCachePool
 */
void
zip_shutdownZipCachePoolHookInterface(J9ZipCachePool *cachePool)
{
	J9HookInterface** mhookInterface = J9_HOOK_INTERFACE(cachePool->hookInterface);	
	if (*mhookInterface) {
		(*mhookInterface)->J9HookShutdownInterface(mhookInterface);
	}
}

/**
 * Public API for retrieving the hook interface from J9ZipCachePool struct  
*/
J9HookInterface** zip_getVMZipCachePoolHookInterface(J9ZipCachePool *cachePool)
{
	if (cachePool)
		return J9_HOOK_INTERFACE(cachePool->hookInterface);
	return NULL;
}

/**
 * Attempt to open a zip file. 
 *
 * If cachePool is non-NULL, then attempts to find zip cache corresponding to the zip file to be opened.
 * If not found, it opens the zip file.
 *
 * @note If cachePool is NULL, zipFile will be opened without a cache.
 *
 * @param[in] portLib the port library
 * @param[in] filename the zip file to open
 * @param[out] zipFile the zip file structure to populate
 * @param[in] cachePool the cache pool
 * @param[in] flags indicate which options are used
 *
 * Valid flags are:
 * J9ZIP_OPEN_READ_CACHE_DATA: build a cache of the central directory
 * J9ZIP_OPEN_ALLOW_NONSTANDARD_ZIP: open the file even if it does not start with a local header
 *
 * 
 * @return 0 on success
 * @return 	ZIP_ERR_FILE_OPEN_ERROR if is there is an error opening the file
 * @return	ZIP_ERR_FILE_READ_ERROR if there is an error reading the file
 * @return	ZIP_ERR_FILE_CORRUPT if the file is corrupt
 * @return	ZIP_ERR_UNKNOWN_FILE_TYPE if the file type is not known
 * @return	ZIP_ERR_UNSUPPORTED_FILE_TYPE if the file type is unsupported
 * @return	ZIP_ERR_OUT_OF_MEMORY if we are out of memory
*/
I_32
zip_openZipFile(J9PortLibrary *portLib, char *filename, J9ZipFile *zipFile, J9ZipCachePool *cachePool, U_32 flags)
{
	PORT_ACCESS_FROM_PORT(portLib);

	IDATA fd = -1;
	I_32 result = 0;
	I_64 seekResult;
	U_8 buffer[4];
	UDATA len;
	J9ZipCache *cache = NULL;
	BOOLEAN doReadCacheData = J9_ARE_ANY_BITS_SET(flags, J9ZIP_OPEN_READ_CACHE_DATA);

	ENTER();

	len = strlen(filename);
	zipFile->fd = -1;
	zipFile->type = ZIP_Unknown;
	zipFile->cache = NULL;
	zipFile->cachePool = NULL;
	zipFile->pointer = -1;
	/* Allocate space for filename */
	if (len >= ZIP_INTERNAL_MAX) {
		zipFile->filename = j9mem_allocate_memory(len + 1, J9MEM_CATEGORY_VM_JCL);
		if (!zipFile->filename) {
			EXIT();
			return ZIP_ERR_OUT_OF_MEMORY;
		}
	} else {
		zipFile->filename = zipFile->internalFilename;
	}

	strcpy((char*)zipFile->filename, filename);

	if (NULL != cachePool) {
		result = zip_searchCache(PORTLIB, filename, cachePool, &cache);
		if (0 != result) {
			goto finished;
		} else if (NULL != cache) {
			/* If zipFile->cache exists, then zip file is already open. Use the same file descriptor and file type. */
			J9ZipCacheInternal *zci = (J9ZipCacheInternal *)cache;
			/* zci->zipFileFd should be valid */
			zipFile->fd = zci->zipFileFd;
			zipFile->type = zci->zipFileType;
			zipFile->pointer = 0;
		}
	}

	if ((NULL == cachePool) || ((0 == result) && (NULL == cache))) {
		fd = j9file_open(filename, EsOpenRead, 0);
		if (fd == -1)  {
			result = ZIP_ERR_FILE_OPEN_ERROR;
			goto finished;
		}

		if( j9file_read(fd, buffer, 4) != 4) 	{
			result = ZIP_ERR_FILE_READ_ERROR;
			goto finished;
		}


		if (('P' == buffer[0]) && ('K' == buffer[1])) {
			/*[PR 94988] If not the central header or local file header, its corrupt */
			/*[cmvc 167285] adding checks to contain zip file end of central dir record */
			if (!((buffer[2] == 1 && buffer[3] == 2) || (buffer[2] == 3 && buffer[3] == 4) ||
					(buffer[2] == 5 && buffer[3] == 6))
			) {
				result = ZIP_ERR_FILE_CORRUPT;
				goto finished;
			}
			/* PKZIP file. Back up the pointer to the beginning. */
			seekResult = j9file_seek(fd, 0, EsSeekSet);
			if(seekResult != 0)	{
				result = ZIP_ERR_FILE_READ_ERROR;
				goto finished;
			}

			zipFile->fd = fd;
			zipFile->type = ZIP_PKZIP;
			zipFile->pointer = 0;
		} else if (J9_ARE_ANY_BITS_SET(flags, J9ZIP_OPEN_ALLOW_NONSTANDARD_ZIP)) {
			J9ZipCentralEnd endEntry;
			zipFile->fd = fd;
			if (0 != scanForCentralEnd(portLib, zipFile, &endEntry)) {
				result = ZIP_ERR_UNKNOWN_FILE_TYPE;
				goto finished;
			}
			zipFile->fd = fd;
			zipFile->type = ZIP_PKZIP;
			zipFile->pointer = 0;
		}

		if((buffer[0] == 0x1F)&&(buffer[1] == 0x8B)) {
			/* GZIP - currently unsupported.
				zipFile->fd = fd;
				zipFile->type = ZIP_GZIP;
				zipFile->pointer = 2;
			*/
			result = ZIP_ERR_UNSUPPORTED_FILE_TYPE;
			goto finished;
		}

		if (ZIP_Unknown == zipFile->type)  {
			result = ZIP_ERR_UNKNOWN_FILE_TYPE;
			goto finished;
		}
	}

	if (NULL != cachePool) {
		result = zip_setupCache(portLib, zipFile, cache, cachePool);
		fd = zipFile->fd;
		if ((0 == result) && (TRUE == doReadCacheData)) {
			result = zip_readCacheData(portLib, zipFile);
		}
	}

finished:
	if (cachePool) {
		TRIGGER_J9HOOK_VM_ZIP_LOAD(cachePool->hookInterface, portLib, cachePool->userData, (const struct J9ZipFile*)zipFile, J9ZIP_STATE_OPEN, (U_8*)filename, result);
	}
	
	if (result == 0)  {
		EXIT();
		return 0;
	}
	if (fd != -1)  {
		j9file_close(fd);
	}
	if ((zipFile->filename) && (zipFile->filename != zipFile->internalFilename)) {
		j9mem_free_memory(zipFile->filename);
	}
	zipFile->filename = NULL;
	EXIT();
	return result;
}


/**
 * Reset nextEntryPointer to the first entry in the file.
 *
 * @param[in] portLib the port library
 * @param[in] zipFile the zip being read from
 * @param[out] nextEntryPointer will be reset to the first entry in the file
 *
 * @return none
 *
 * 
*/
void zip_resetZipFile(J9PortLibrary* portLib, J9ZipFile* zipFile, IDATA *nextEntryPointer)
{
	*nextEntryPointer = 0;
	if (zipFile) {
		I_32 result = 0;
		J9ZipCachePool *cachePool = zipFile->cachePool;
		
		if (zipFile->cache)
			*nextEntryPointer = zipCache_getStartCentralDir(zipFile->cache);
		else {
			J9ZipCentralEnd endEntry;
			result = scanForCentralEnd(portLib, zipFile, &endEntry);
			if (result == 0) {
				*nextEntryPointer = (IDATA)((UDATA)endEntry.dirOffset);
			}
		}
		if (cachePool) {
			TRIGGER_J9HOOK_VM_ZIP_LOAD(cachePool->hookInterface, portLib, cachePool->userData, zipFile, J9ZIP_STATE_RESET, zipFile->filename, result);
		}
	}
}


/**
 * Attempt to read a zip entry at offset from the zip file provided.
 *	If found, read into entry.
 *
 * @note If an uncached entry is found, the filename field will be filled in. This
 *	memory MUST be freed with @ref zip_freeZipEntry.
 *
 * @param[in] portLib the port library
 * @param[in] zipFile the zip file being read
 * @param[out] entry the compressed data
 * @param[in] offset the offset into the zipFile of the desired zip entry
 * @param[in] readDataPointer do extra work to read the data pointer, if false the dataPointer will be 0
 *
 * @return 0 on success
 * @return ZIP_ERR_FILE_READ_ERROR if there is an error reading from @ref zipFile
 * @return 	ZIP_ERR_FILE_CORRUPT if @ref zipFile is corrupt
 * @return 	ZIP_ERR_ENTRY_NOT_FOUND if the entry is not found 
 * @return 	ZIP_ERR_OUT_OF_MEMORY if there is not enough memory to complete this call
 *
 * @see zip_freeZipEntry
*/
I_32 zip_getZipEntryFromOffset(J9PortLibrary * portLib, J9ZipFile * zipFile, J9ZipEntry * entry, IDATA offset, BOOLEAN readDataPointer)
{
	PORT_ACCESS_FROM_PORT(portLib);
	I_32 result;
	I_64 seekResult;

	ENTER();

	if (zipFile->pointer != offset) {
		zipFile->pointer = (I_32) offset;
	}
	seekResult = j9file_seek(zipFile->fd, zipFile->pointer, EsSeekSet);
	if ((seekResult < 0) || (seekResult > J9CONST64(0x7FFFFFFF)) || (zipFile->pointer != offset)) {
		zipFile->pointer = -1;
		EXIT();
		return ZIP_ERR_FILE_READ_ERROR;
	}

	result = readZipEntry(portLib, zipFile, entry, NULL, 0, NULL, NULL, FALSE, readDataPointer);
	EXIT();
	return result;
}


/*
	cached alloc management for zip_getZipEntryData.
*/
void* zdataalloc(void* opaque, U_32 items, U_32 size)
{
	UDATA* returnVal = 0;
	U_32 byteSize = items * size;
	U_32 allocSize = ZIP_WORK_BUFFER_SIZE;
	struct workBuffer *wb = (struct workBuffer*)opaque;

	PORT_ACCESS_FROM_PORT(wb->portLib);

	/* Round to UDATA multiple */
	byteSize = (byteSize + (sizeof(UDATA) - 1)) & ~(sizeof(UDATA) - 1);

	if (wb->bufferStart == NULL)
	{
		if (byteSize > ZIP_WORK_BUFFER_SIZE) {
			allocSize = byteSize;
		}
		wb->bufferStart = j9mem_allocate_memory(allocSize, J9MEM_CATEGORY_VM_JCL);
		if (wb->bufferStart) {
			wb->bufferEnd = (UDATA*)((UDATA)wb->bufferStart + allocSize);
			wb->currentAlloc = wb->bufferStart;
			wb->cntr = 0;
		}
	}

	if ((wb->bufferStart == NULL) || (((UDATA)wb->currentAlloc + byteSize) > (UDATA)wb->bufferEnd))
	{
		returnVal = j9mem_allocate_memory(byteSize, J9MEM_CATEGORY_VM_JCL);
	} 
	else
	{
		++(wb->cntr);
		returnVal = wb->currentAlloc;
		wb->currentAlloc = (UDATA*)((UDATA)wb->currentAlloc + byteSize);
	}
	return returnVal;
}


/*
	cached alloc management for zip_getZipEntryData.
*/
void zdatafree(void* opaque, void* address)
{
	struct workBuffer *wb = (struct workBuffer*)opaque;

	PORT_ACCESS_FROM_PORT(wb->portLib);

	if ((address < (void*)wb->bufferStart) || (address >= (void*)wb->bufferEnd))
	{
		j9mem_free_memory(address);
	}
	else if (--(wb->cntr) == 0)
	{
		j9mem_free_memory(wb->bufferStart);
		wb->bufferStart = wb->bufferEnd = wb->currentAlloc = 0;
	} 

}

J9ZipFunctionTable *
getZipFunctions(J9PortLibrary* portLib, void *libDir) 
{
	if (0 != initZipLibrary(portLib, (char *) libDir)) {
		return NULL;
	} else {
		return &zipFunctions;
	}
}
