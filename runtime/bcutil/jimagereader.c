/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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

#include "bcutil_api.h"
#include "jimage.h"
#include "jimagereader.h"
#include "ut_j9bcu.h"
#include "util_api.h"

VMINLINE static U_32 hashFn(const char *name, I_32 baseValue);
VMINLINE static I_32 getRedirectTableValue(const char *name, I_32 *redirectTable, U_32 redirectTableSize);
static I_32 verifyJImageHeader(const char *fileName, JImageHeader *header);

/**
 * Computes hash code for given "name" using "baseValue".
 * If baseValue is 0, uses JIMAGE_LOOKUP_HASH_SEED as the base value for computation of hash code.
 *
 * @param [in] name
 * @param [in] baseValue
 *
 * @return unsigned 32-bit hash code
 */
VMINLINE static U_32
hashFn(const char *name, I_32 baseValue)
{
	I_32 hashcode = baseValue;
	UDATA i = 0;

	if (0 == baseValue) {
		hashcode = JIMAGE_LOOKUP_HASH_SEED;
	}

	for (i = 0; i < strlen(name); i++) {
		hashcode = (hashcode * JIMAGE_LOOKUP_HASH_SEED) ^ name[i];
	}

	return hashcode & 0x7FFFFFFF;
}

/**
 * Returns the value in the redirect table using given name has the key.
 * 
 * @param [in] name	used as key to get the value in redirectTable
 * @param [in] redirectTable
 * @param [in] redirectTableSize number of entries in redirectTable
 *
 * @return value in the redirect table
 */
VMINLINE static I_32
getRedirectTableValue(const char *name, I_32 *redirectTable, U_32 redirectTableSize)
{
	U_32 hash = hashFn(name, 0);
	return redirectTable[hash % redirectTableSize];
}

/**
 * Verifies jimage header is valid.
 *
 * @param [in] fileName name of jimage file
 * @param [in] header pointer to J9ImageHeader
 *
 * @return J9JIMAGE_NO_ERROR if jimage header is valid, J9JIMAGE_INVALID_HEADER otherwise
 */
static I_32
verifyJImageHeader(const char *fileName, JImageHeader *header)
{
	I_32 rc = J9JIMAGE_INVALID_HEADER;

	if (JIMAGE_HEADER_MAGIC != header->magic) {
		Trc_BCU_verifyJImageHeader_BadMagic(fileName, header->magic, header);
		goto _end;
	}
	if (JIMAGE_MAJOR_VERSION != header->majorVersion) {
		Trc_BCU_verifyJImageHeader_BadMajorVersion(fileName, header->majorVersion, header);
		goto _end;
	}
	if (JIMAGE_MINOR_VERSION != header->minorVersion) {
		Trc_BCU_verifyJImageHeader_BadMinorVersion(fileName, header->minorVersion, header);
		goto _end;
	}
	rc = J9JIMAGE_NO_ERROR;

_end:
	return rc;
}

I_32
j9bcutil_loadJImage(J9PortLibrary *portlib, const char *fileName, J9JImage **pjimage)
{
	IDATA jimagefd = -1;
	UDATA pageSize = 0;
	UDATA mapSize = 0;
	I_64 fileSize = 0;
	U_8 *cursor = NULL;
	JImageHeader header;
	J9JImageHeader *j9jimageHeader = NULL;
	J9JImage *jimage = NULL;
	UDATA fileNameLen = strlen(fileName);
	IDATA bytesRead = 0;
	UDATA byteAmount = 0;
	I_32 rc = J9JIMAGE_NO_ERROR;

	PORT_ACCESS_FROM_PORT(portlib);

	Trc_BCU_loadJImage_Entry(fileName);

	jimagefd = j9file_open(fileName, EsOpenRead, 0644 /* irrelevant since EsOpenCreate is not used */);
	if (-1 == jimagefd) {
		I_32 portlibErrCode = j9error_last_error_number();
		const char *portlibErrMsg = j9error_last_error_message();
		Trc_BCU_loadJImage_JImageOpenFailed(fileName, portlibErrCode, portlibErrMsg);
		rc = J9JIMAGE_FILE_OPEN_ERROR;
		goto _end;
	}

	fileSize = j9file_length(fileName);
	if (fileSize < 0) {
		I_32 portlibErrCode = j9error_last_error_number();
		const char *portlibErrMsg = j9error_last_error_message();
		Trc_BCU_loadJImage_JImageFileLenFailed(fileName, portlibErrCode, portlibErrMsg);
		rc = J9JIMAGE_FILE_LENGTH_ERROR;
		goto _end;
	}

	/* Read only the header to verify it and to find out how much we need to mmap */
	bytesRead = j9file_read(jimagefd, &header, sizeof(JImageHeader));
	if (sizeof(JImageHeader) != bytesRead) {
		I_32 portlibErrCode = j9error_last_error_number();
		const char *portlibErrMsg = j9error_last_error_message();
		Trc_BCU_loadJImage_JImageReadHeaderFailed(fileName, portlibErrCode, portlibErrMsg, bytesRead, sizeof(JImageHeader));
		rc = J9JIMAGE_FILE_READ_ERROR;
		goto _end;
	}

	rc = verifyJImageHeader(fileName, &header);
	if (J9JIMAGE_INVALID_HEADER == rc) {
		goto _end;
	}

	byteAmount = sizeof(J9JImage) + (fileNameLen + 1) + sizeof(J9JImageHeader);
	jimage = (J9JImage *)j9mem_allocate_memory(byteAmount, J9MEM_CATEGORY_CLASSES);
	if (NULL == jimage) {
		Trc_BCU_loadJImage_MemoryAllocationFailed(byteAmount);
		rc = J9JIMAGE_OUT_OF_MEMORY;
		goto _end;
	}
	memset(jimage, 0, sizeof(J9JImage));
	jimage->fd = jimagefd;
	jimage->fileName = (char *)(jimage + 1);
	j9str_printf(PORTLIB, jimage->fileName, fileNameLen + 1, "%s", fileName);
	jimage->fileLength = fileSize;
	j9jimageHeader = jimage->j9jimageHeader = (J9JImageHeader *)((U_8 *)jimage + sizeof(J9JImage) + (fileNameLen + 1));

	/* Map JImage header, redirect table, locationsOffsetTable, ImageLocations and Strings
	 * Format of the above structures in jimage is:
	 * 	| JImageHeader | Redirect Table (s4*tableLength) | LocationsOffsetTable (u4*tableLength) | Locations | Strings | ... |
	 */
	mapSize = JIMAGE_RESOURCE_AREA_OFFSET(header);
	pageSize = j9mmap_get_region_granularity(j9jimageHeader);
	if (0 != pageSize) {
		mapSize = ROUND_UP_TO(pageSize, mapSize);
	}

	jimage->jimageMmap = j9mmap_map_file(jimagefd, 0, mapSize, fileName, J9PORT_MMAP_FLAG_READ, J9MEM_CATEGORY_CLASSES);
	if (NULL == jimage->jimageMmap) {
		I_32 portlibErrCode = j9error_last_error_number();
		const char *portlibErrMsg = j9error_last_error_message();
		Trc_BCU_loadJImage_JImageMmapFailed(mapSize, fileName, portlibErrCode, portlibErrMsg);
		rc = J9JIMAGE_MAP_FAILED;
		goto _end;
	}

	cursor = (U_8 *)jimage->jimageMmap->pointer;
	j9jimageHeader->jimageHeader = (JImageHeader *)cursor;

	cursor += JIMAGE_HEADER_SIZE(header);
	j9jimageHeader->redirectTable = (I_32 *)cursor;

	cursor += JIMAGE_REDIRECT_TABLE_SIZE(header);
	j9jimageHeader->locationsOffsetTable = (U_32 *)cursor;

	cursor += JIMAGE_LOCATION_OFFSETS_TABLE_SIZE(header);
	j9jimageHeader->locationsData = cursor;

	cursor += JIMAGE_LOCATION_DATA_SIZE(header);
	j9jimageHeader->stringsData = cursor;

	cursor += JIMAGE_STRING_DATA_SIZE(header);
	j9jimageHeader->resources = cursor - (U_8 *)j9jimageHeader->jimageHeader;

	*pjimage = jimage;

	rc = J9JIMAGE_NO_ERROR;

_end:
	if (J9JIMAGE_NO_ERROR != rc) {
		if (NULL != jimage) {
			j9bcutil_unloadJImage(PORTLIB, jimage);
			}
		if (-1 != jimagefd) {
			j9file_close(jimagefd);
		}
		*pjimage = NULL;
	}

	Trc_BCU_loadJImage_Exit(rc, *pjimage);
	return rc;
}

void
j9bcutil_dumpJImageInfo(J9PortLibrary *portlib, J9JImage *jimage)
{
	J9JImageHeader *j9jimageHeader = NULL;
	JImageHeader *jimageHeader = NULL;
	U_32 locationOffset = 0;
	void *imageLocation = NULL;
	U_32 i = 0;

	PORT_ACCESS_FROM_PORT(portlib);

	Trc_BCU_Assert_NotEquals(NULL, jimage);
	Trc_BCU_Assert_NotEquals(NULL, jimage->j9jimageHeader);
	Trc_BCU_Assert_NotEquals(NULL, jimage->j9jimageHeader->jimageHeader);

	j9jimageHeader = jimage->j9jimageHeader;
	jimageHeader = j9jimageHeader->jimageHeader;

	j9tty_printf(PORTLIB, "jimage file: %s\n", jimage->fileName);
	j9tty_printf(PORTLIB, "\n");
	j9tty_printf(PORTLIB, "--- Header ---\n");
	j9tty_printf(PORTLIB, "major version: %x\n", jimageHeader->majorVersion);
	j9tty_printf(PORTLIB, "minor version: %x\n", jimageHeader->minorVersion);
	j9tty_printf(PORTLIB, "flags: %x\n", jimageHeader->flags);
	j9tty_printf(PORTLIB, "resourceCount: %u\n", jimageHeader->resourceCount);
	j9tty_printf(PORTLIB, "tableLength: %u\n", jimageHeader->tableLength);
	j9tty_printf(PORTLIB, "locationsSize: %u\n", jimageHeader->locationsSize);
	j9tty_printf(PORTLIB, "stringsSize: %u\n", jimageHeader->stringsSize);
	j9tty_printf(PORTLIB, "\n");
	j9tty_printf(PORTLIB, "--- Location Data ---\n");
	j9tty_printf(PORTLIB, "%-8s", "Index");
	j9tty_printf(PORTLIB, "| %-16s", "LOT Offset");
	j9tty_printf(PORTLIB, "| %-16s", "Resource Offset");
	j9tty_printf(PORTLIB, "| %-16s", "Compressed size");
	j9tty_printf(PORTLIB, "| %-16s", "Uncompressed size");
	j9tty_printf(PORTLIB, "| %-s", "Module|Parent|Base|Extension");

	j9tty_printf(PORTLIB, "\n");
	for (i = 0; i < jimageHeader->tableLength; i++) {
		J9JImageLocation j9jimageLocation = {0};

		locationOffset = j9jimageHeader->locationsOffsetTable[i];
		imageLocation = j9jimageHeader->locationsData + locationOffset;

		j9bcutil_createAndVerifyJImageLocation(portlib, jimage, NULL, imageLocation, &j9jimageLocation);

		j9tty_printf(PORTLIB, "%-8u| %-16x| %-16x| %-16llu| %-16llu| %s|%s|%s|%s\n", i, locationOffset, j9jimageLocation.resourceOffset,
					j9jimageLocation.compressedSize, j9jimageLocation.uncompressedSize,
					(NULL == j9jimageLocation.moduleString) ? "-" : j9jimageLocation.moduleString,
					(NULL == j9jimageLocation.parentString) ? "-" : j9jimageLocation.parentString,
					(NULL == j9jimageLocation.baseString) ? "-" : j9jimageLocation.baseString,
					(NULL == j9jimageLocation.extensionString) ? "-" : j9jimageLocation.extensionString);
	}
}

I_32
j9bcutil_lookupJImageResource(J9PortLibrary *portlib, J9JImage *jimage, J9JImageLocation *j9jimageLocation, const char *resourceName)
{
	J9JImageHeader *j9jimageHeader = NULL;
	JImageHeader *jimageHeader = NULL;
	I_32 redirectValue = 0;
	I_32 rc = J9JIMAGE_NO_ERROR;

	PORT_ACCESS_FROM_PORT(portlib);

	Trc_BCU_Assert_NotEquals(NULL, jimage);
	Trc_BCU_Assert_NotEquals(NULL, jimage->j9jimageHeader);
	Trc_BCU_Assert_NotEquals(NULL, jimage->j9jimageHeader->jimageHeader);

	Trc_BCU_lookupJImageResource_Entry(jimage->fileName, resourceName, jimage);

	j9jimageHeader = jimage->j9jimageHeader;
	jimageHeader = j9jimageHeader->jimageHeader;

	redirectValue = getRedirectTableValue(resourceName, j9jimageHeader->redirectTable, jimageHeader->tableLength);

	if (0 == redirectValue) {
		/* No resource of this name exists in the jimage */
		Trc_BCU_lookupJImageResource_ResourceNotFound(jimage->fileName, resourceName, jimage);
		rc = J9JIMAGE_RESOURCE_NOT_FOUND;
		goto _end;
	} else {
		U_32 lotIndex = 0;
		U_32 locationOffset = 0;
		void *imageLocation = NULL;

		if (redirectValue < 0) {
			lotIndex = -1 - redirectValue;
		} else {
			U_32 hash = hashFn(resourceName, redirectValue);
			lotIndex = hash % jimageHeader->tableLength;
		}

		if (lotIndex >= jimageHeader->tableLength) {
			Trc_BCU_lookupJImageResource_InvalidLocationOffsetsTableIndex(jimage->fileName, lotIndex, jimageHeader->tableLength, jimage);
			rc = J9JIMAGE_INVALID_LOT_INDEX;
			goto _end;
		}
		
		locationOffset = j9jimageHeader->locationsOffsetTable[lotIndex];
		if (locationOffset >= JIMAGE_LOCATION_DATA_SIZE(*jimageHeader)) {
			Trc_BCU_lookupJImageResource_InvalidLocationOffset(jimage->fileName, locationOffset, JIMAGE_LOCATION_DATA_SIZE(*jimageHeader), jimage);
			rc = J9JIMAGE_INVALID_LOCATION_OFFSET;
			goto _end;
		}

		imageLocation = j9jimageHeader->locationsData + locationOffset;

		rc = j9bcutil_createAndVerifyJImageLocation(portlib, jimage, resourceName, imageLocation, j9jimageLocation);
		if (J9JIMAGE_LOCATION_VERIFICATION_FAIL == rc) {
			rc = J9JIMAGE_RESOURCE_NOT_FOUND;
		}
	}

_end:

	Trc_BCU_lookupJImageResource_Exit(jimage->fileName, rc);
	return rc;
}

I_32
j9bcutil_createAndVerifyJImageLocation(J9PortLibrary *portlib, J9JImage *jimage, const char *resourceName, void *imageLocation, J9JImageLocation *j9jimageLocation)
{
	U_8 *cursor = NULL;
	J9JImageHeader *j9jimageHeader = NULL;
	JImageHeader *jimageHeader = NULL;
	char *moduleString = NULL;
	char *parentString = NULL;
	char *baseString = NULL;
	char *extensionString = NULL;
	U_32 extensionOffset = 0;
	U_64 resourceOffset = 0;
	U_64 compressedSize = 0;
	U_64 uncompressedSize = 0;
	U_8 type = J9JIMAGE_ATTRIBUTE_TYPE_END;
	I_32 rc = J9JIMAGE_NO_ERROR;

	PORT_ACCESS_FROM_PORT(portlib);

	Trc_BCU_Assert_NotEquals(NULL, jimage);
	Trc_BCU_Assert_NotEquals(NULL, jimage->j9jimageHeader);
	Trc_BCU_Assert_NotEquals(NULL, jimage->j9jimageHeader->jimageHeader);

	Trc_BCU_createAndVerifyJImageLocation_Entry(jimage->fileName, resourceName, imageLocation);

	j9jimageHeader = jimage->j9jimageHeader;
	jimageHeader = j9jimageHeader->jimageHeader;

	/* If there is no attribute of type Offset, assume its offset to be 0 */
	resourceOffset = j9jimageHeader->resources;

	cursor = (U_8 *)imageLocation;
	do {
		U_8 length = 0;
		U_64 offset = 0;

		J9JIMAGE_READ_U8(type, cursor);
		length = (type & JIMAGE_TYPE_BYTE_LENGTH_MASK) + 1;
		type = (type & JIMAGE_TYPE_BYTE_TYPE_MASK) >> JIMAGE_TYPE_BYTE_TYPE_SHIFT;

		/* Note: uniquely, END attribute is not followed by a length and data field */
		if (J9JIMAGE_ATTRIBUTE_TYPE_END != type) {
			switch(length) {
			case 1:
				J9JIMAGE_READ_U8(offset, cursor);
				break;
			case 2:
				J9JIMAGE_READ_U16_BIGENDIAN(offset, cursor);
				break;
			case 3:
				J9JIMAGE_READ_U24_BIGENDIAN(offset, cursor);
				break;
			case 4:
				J9JIMAGE_READ_U32_BIGENDIAN(offset, cursor);
				break;
			case 5:
			case 6:
			case 7:
			case 8:
				J9JIMAGE_READ_BYTES_BIGENDIAN(offset, cursor, length);
				break;
			default:
				Trc_BCU_Assert_ShouldNeverHappen();
				break;
			}
		}

		switch(type) {
		case J9JIMAGE_ATTRIBUTE_TYPE_END:
			/* end of Image Location */
			break;

		case J9JIMAGE_ATTRIBUTE_TYPE_MODULE:
			/* Offset 0 is special for empty string. */
			if ((0 == offset) || (offset >= JIMAGE_STRING_DATA_SIZE(*jimageHeader))) {
				Trc_BCU_createAndVerifyJImageLocation_InvalidModuleStringOffset(jimage->fileName, offset, imageLocation, JIMAGE_STRING_DATA_SIZE(*jimageHeader));
				rc = J9JIMAGE_INVALID_MODULE_OFFSET;
				goto _end;
			}
			moduleString = (char *)(j9jimageHeader->stringsData + offset);
			break;

		case J9JIMAGE_ATTRIBUTE_TYPE_PARENT:
			/* Offset 0 is special for empty string. */
			if ((0 == offset) || (offset >= JIMAGE_STRING_DATA_SIZE(*jimageHeader))) {
				Trc_BCU_createAndVerifyJImageLocation_InvalidParentStringOffset(jimage->fileName, offset, imageLocation, JIMAGE_STRING_DATA_SIZE(*jimageHeader));
				rc = J9JIMAGE_INVALID_PARENT_OFFSET;
				goto _end;
			}
			parentString = (char *)(j9jimageHeader->stringsData + offset);
			break;

		case J9JIMAGE_ATTRIBUTE_TYPE_BASE:
			/* Offset 0 is special for empty string. */
			if ((0 == offset) || (offset >= JIMAGE_STRING_DATA_SIZE(*jimageHeader))) {
				Trc_BCU_createAndVerifyJImageLocation_InvalidBaseStringOffset(jimage->fileName, offset, imageLocation, JIMAGE_STRING_DATA_SIZE(*jimageHeader));
				rc = J9JIMAGE_INVALID_BASE_OFFSET;
				goto _end;
			}
			baseString = (char *)(j9jimageHeader->stringsData + offset);
			break;

		case J9JIMAGE_ATTRIBUTE_TYPE_EXTENSION:
			/* Offset 0 is special for empty string. */
			if ((0 == offset) || (offset >= JIMAGE_STRING_DATA_SIZE(*jimageHeader))) {
				Trc_BCU_createAndVerifyJImageLocation_InvalidExtensionStringOffset(jimage->fileName, offset, imageLocation, JIMAGE_STRING_DATA_SIZE(*jimageHeader));
				rc = J9JIMAGE_INVALID_EXTENSION_OFFSET;
				goto _end;
			}
			extensionString = (char *)(j9jimageHeader->stringsData + offset);
			break;

		case J9JIMAGE_ATTRIBUTE_TYPE_OFFSET:
			if (offset >= (jimage->fileLength - JIMAGE_RESOURCE_AREA_OFFSET(*jimageHeader))) {
				Trc_BCU_createAndVerifyJImageLocation_InvalidResourceOffset(jimage->fileName, offset, imageLocation, jimage->fileLength - JIMAGE_RESOURCE_AREA_OFFSET(*jimageHeader));
				rc = J9JIMAGE_INVALID_RESOURCE_OFFSET;
				goto _end;
			}
			resourceOffset = j9jimageHeader->resources + offset;
			break;

		case J9JIMAGE_ATTRIBUTE_TYPE_COMPRESSED:
			compressedSize = offset;
			if (compressedSize >= (jimage->fileLength - JIMAGE_RESOURCE_AREA_OFFSET(*jimageHeader))) {
				Trc_BCU_createAndVerifyJImageLocation_InvalidCompressedSize(jimage->fileName, compressedSize, imageLocation, jimage->fileLength - JIMAGE_RESOURCE_AREA_OFFSET(*jimageHeader));
				rc = J9JIMAGE_INVALID_COMPRESSED_SIZE;
			}
			break;

		case J9JIMAGE_ATTRIBUTE_TYPE_UNCOMPRESSED:
			uncompressedSize = offset;
			if (uncompressedSize >= (jimage->fileLength - JIMAGE_RESOURCE_AREA_OFFSET(*jimageHeader))) {
				Trc_BCU_createAndVerifyJImageLocation_InvalidUncompressedSize(jimage->fileName, uncompressedSize, imageLocation, jimage->fileLength - JIMAGE_RESOURCE_AREA_OFFSET(*jimageHeader));
				rc = J9JIMAGE_INVALID_UNCOMPRESSED_SIZE;
			}
			break;

		default:
			Trc_BCU_Assert_ShouldNeverHappen();
			break;
		}

	} while (J9JIMAGE_ATTRIBUTE_TYPE_END != type);

	/* assert that baseString is non-null */
	Trc_BCU_Assert_NotEquals(NULL, baseString);
	
	/* verify image location using resource name */
	if (NULL != resourceName) {
		char *name = NULL;
		IDATA resourceNameLen = strlen(resourceName) + 1; /* +1 to include terminating NULL character */

		rc = j9bcutil_getJImageResourceName(PORTLIB, jimage, moduleString, parentString, baseString, extensionString, &name);
		if (J9JIMAGE_NO_ERROR != rc) {
			goto _end;
		}

		if (0 != strncmp(name, resourceName, resourceNameLen)) {
			Trc_BCU_createAndVerifyJImageLocation_ResourceNameMismatch(jimage->fileName, resourceName, name, imageLocation);
			rc = J9JIMAGE_LOCATION_VERIFICATION_FAIL;
			j9mem_free_memory(name);
			goto _end;
		}

		j9mem_free_memory(name);
	}
	
	if (NULL != j9jimageLocation) {
		j9jimageLocation->moduleString = moduleString;
		j9jimageLocation->parentString = parentString;
		j9jimageLocation->baseString = baseString;
		j9jimageLocation->extensionString = extensionString;
		j9jimageLocation->compressedSize = compressedSize;
		j9jimageLocation->uncompressedSize = uncompressedSize;
		j9jimageLocation->resourceOffset = resourceOffset;
	}

_end:

	Trc_BCU_createAndVerifyJImageLocation_Exit(jimage->fileName, rc);
	return rc;
}

I_32
j9bcutil_getJImageResource(J9PortLibrary *portlib, J9JImage *jimage, J9JImageLocation *j9jimageLocation, void *dataBuffer, U_64 dataBufferSize)
{
	J9JImageHeader *j9jimageHeader = NULL;
	JImageHeader *jimageHeader = NULL;
	I_64 seekResult = -1;
	IDATA bytesRead = 0;
	U_8 *inputBuffer = NULL;
	U_8 *outputBuffer = NULL;
	I_32 rc = J9JIMAGE_NO_ERROR;

	PORT_ACCESS_FROM_PORT(portlib);

	Trc_BCU_Assert_NotEquals(NULL, jimage);
	Trc_BCU_Assert_NotEquals(NULL, jimage->j9jimageHeader);
	Trc_BCU_Assert_NotEquals(NULL, jimage->j9jimageHeader->jimageHeader);
	Trc_BCU_Assert_True(dataBufferSize >= j9jimageLocation->uncompressedSize);

	j9jimageHeader = jimage->j9jimageHeader;
	jimageHeader = j9jimageHeader->jimageHeader;

	seekResult = j9file_seek(jimage->fd, (I_64)j9jimageLocation->resourceOffset, EsSeekSet);
	if (-1 == seekResult) {
		I_32 portlibErrCode = j9error_last_error_number();
		const char *portlibErrMsg = j9error_last_error_message();
		Trc_BCU_getJImageResource_JImageFileSeekFailed(jimage->fileName, j9jimageLocation->resourceOffset, portlibErrCode, portlibErrMsg);
		rc = J9JIMAGE_FILE_SEEK_ERROR;
		goto _end;
	}

	if (0 != j9jimageLocation->compressedSize) {
		DecompressorInfo *decompressorInfo = NULL;
		char *decompressorName = NULL;

		inputBuffer = j9mem_allocate_memory((UDATA)j9jimageLocation->compressedSize, J9MEM_CATEGORY_CLASSES);
		if (NULL == inputBuffer) {
			Trc_BCU_getJImageResource_MemoryAllocationFailed(jimage->fileName, j9jimageLocation->compressedSize);
			rc = J9JIMAGE_OUT_OF_MEMORY;
			goto _end;
		}
		bytesRead = j9file_read(jimage->fd, inputBuffer, (UDATA)j9jimageLocation->compressedSize);
		if (j9jimageLocation->compressedSize != bytesRead) {
			I_32 portlibErrCode = j9error_last_error_number();
			const char *portlibErrMsg = j9error_last_error_message();
			Trc_BCU_getJImageResource_JImageReadResourceDataFailed(jimage->fileName, bytesRead, j9jimageLocation->compressedSize, portlibErrCode, portlibErrMsg);
			rc = J9JIMAGE_FILE_READ_ERROR;
			goto _end;
		}

		do {
			decompressorInfo = (DecompressorInfo *)inputBuffer;

			if (decompressorInfo->magic != JIMAGE_DECOMPRESSOR_MAGIC_BYTE) {
				Trc_BCU_getJImageResource_JImageInvalidDecompressor(jimage->fileName);
				rc = J9JIMAGE_DECOMPRESSOR_INVALID;
				goto _end;
			}

			/* Skip decompressor header to reach compressed data */
			inputBuffer = (U_8 *)(decompressorInfo + 1);
			outputBuffer = (U_8 *)j9mem_allocate_memory(decompressorInfo->uncompressedSize, J9MEM_CATEGORY_CLASSES);
			if (NULL == outputBuffer) {
				Trc_BCU_getJImageResource_MemoryAllocationFailed(jimage->fileName, decompressorInfo->uncompressedSize);
				rc = J9JIMAGE_OUT_OF_MEMORY;
				goto _end;
			}

			decompressorName = (char *)((U_8 *)jimageHeader + JIMAGE_STRING_DATA_OFFSET(*jimageHeader) + decompressorInfo->decompressorNameOffset);
			if (0 == strncmp(decompressorName, "zip", sizeof("zip"))) {
#if 0
				/* Inflate data using zip library.
				 * Can re-use zipsup.c:inflateData() for this purpose.
				 */
#else /* if 0 */
				j9tty_printf(PORTLIB, "Decompressor %s is not supported\n", decompressorName);
				Trc_BCU_getJImageResource_JImageDecompressorNotSupported(jimage->fileName, decompressorName);
				rc = J9JIMAGE_DECOMPRESSOR_NOT_SUPPORTED;
				goto _end;
#endif /* if 0 */
			} else {
				j9tty_printf(PORTLIB, "Decompressor %s is not supported\n", decompressorName);
				Trc_BCU_getJImageResource_JImageDecompressorNotSupported(jimage->fileName, decompressorName);
				rc = J9JIMAGE_DECOMPRESSOR_NOT_SUPPORTED;
				goto _end;
			}

			/* If decompressorFlag is 0 then the inflated data itself is in compressed format.
			 * Loop until we find decompressorFlag set to 1 which would mean no more decompression is required.
			 */
			if (0 == decompressorInfo->decompressorFlag) {
				inputBuffer = outputBuffer;
			} else {
				/* This is the last decompressor. */
				IDATA bytesToCopy = (dataBufferSize < decompressorInfo->uncompressedSize) ? (IDATA)dataBufferSize : (IDATA)decompressorInfo->uncompressedSize;
				memcpy(dataBuffer, outputBuffer, bytesToCopy);
				if (dataBufferSize < decompressorInfo->uncompressedSize) {
					rc = J9JIMAGE_RESOURCE_TRUNCATED;
				}
				j9mem_free_memory(outputBuffer);
			}
			j9mem_free_memory(decompressorInfo);
		} while (0 == decompressorInfo->decompressorFlag);
	} else {
		IDATA bytesToRead = (dataBufferSize < j9jimageLocation->uncompressedSize) ? (IDATA)dataBufferSize : (IDATA)j9jimageLocation->uncompressedSize;
		bytesRead = j9file_read(jimage->fd, dataBuffer, bytesToRead);
		if (bytesToRead != bytesRead) {
			I_32 portlibErrCode = j9error_last_error_number();
			const char *portlibErrMsg = j9error_last_error_message();
			Trc_BCU_getJImageResource_JImageReadResourceDataFailed(jimage->fileName, bytesRead, bytesToRead, portlibErrCode, portlibErrMsg);
			rc = J9JIMAGE_FILE_READ_ERROR;
			goto _end;
		} else {
			if (dataBufferSize < j9jimageLocation->uncompressedSize) {
				rc = J9JIMAGE_RESOURCE_TRUNCATED;
			}
		}
	}

_end:
	if (NULL != inputBuffer) {
		j9mem_free_memory(inputBuffer);
	}
	if ((NULL != outputBuffer) && (outputBuffer != dataBuffer)) {
		j9mem_free_memory(outputBuffer);
	}
	Trc_BCU_getJImageResource_Exit(jimage->fileName, rc);
	return rc;
}

I_32
j9bcutil_getJImageResourceName(J9PortLibrary *portlib, J9JImage *jimage, const char *module, const char *parent, const char *base, const char *extension, char **resourceName)
{
	char *fullName = NULL;
	UDATA fullNameLen = 0;
	char *cursor = NULL;
	UDATA count = 0;
	UDATA spaceLeft = 0;
	I_32 rc = J9JIMAGE_NO_ERROR;

	PORT_ACCESS_FROM_PORT(portlib);

	/* assert that baseString is non-null */
	Trc_BCU_Assert_NotEquals(NULL, base);
	Trc_BCU_Assert_NotEquals(NULL, resourceName);
	
	fullNameLen = strlen(base);

	if (NULL != module) {
		fullNameLen += 1 + strlen(module) + 1;	/* add 1 each for leading and trailing '/' */
	}
	if (NULL != parent) {
		fullNameLen += strlen(parent) + 1;	/* add 1 for trailing '/' */
	}
	if (NULL != extension) {
		fullNameLen += strlen(extension) + 1;	/* add 1 for '.' before extension */
	}
	fullNameLen += 1;

	fullName = j9mem_allocate_memory(fullNameLen, J9MEM_CATEGORY_CLASSES);	/* add 1 for '\0' character */
	if (NULL == fullName) {
		Trc_BCU_getJImageResourceName_NameAllocationFailed(jimage->fileName, fullNameLen + 1);
		rc = J9JIMAGE_OUT_OF_MEMORY;
		goto _end;
	}

	cursor = fullName;
	spaceLeft = fullNameLen;
	if (NULL != module) {
		count = j9str_printf(PORTLIB, cursor, spaceLeft, "/%s/", module);
		cursor += count;
		spaceLeft -= count;
	}
	if (NULL != parent) {
		count = j9str_printf(PORTLIB, cursor, spaceLeft, "%s/", parent);
		cursor += count;
		spaceLeft -= count;
	}
	count = j9str_printf(PORTLIB, cursor, spaceLeft, "%s", base);
	cursor += count;
	spaceLeft -= count;
	if (NULL != extension) {
		count = j9str_printf(PORTLIB, cursor, spaceLeft, ".%s", extension);
	}

	*resourceName = fullName;

_end:
	if (J9JIMAGE_NO_ERROR != rc) {
		*resourceName = NULL;
	}
	return rc;
}

const char *
j9bcutil_findModuleForPackage(J9PortLibrary *portlib, J9JImage *jimage, const char *package)
{
	J9JImageHeader *j9jimageHeader = NULL;
	JImageHeader *jimageHeader = NULL;
	J9JImageLocation j9jimageLocation = {0};
	const char *packagePrefix = "/packages/";
	const IDATA pkgPrefixLen = sizeof("/packages/") - 1;
	char *packageName = NULL;
	IDATA packageNameLen = 0;
	UDATA i = 0;
	U_8 *dataBuffer = NULL;
	U_8 *cursor = NULL;
	U_32 isEmpty = 0;
	U_32 moduleNameOffset = 0;
	char *moduleName = NULL;
	I_32 rc = J9JIMAGE_NO_ERROR;

	PORT_ACCESS_FROM_PORT(portlib);

	Trc_BCU_Assert_NotEquals(NULL, jimage);
	Trc_BCU_Assert_NotEquals(NULL, jimage->j9jimageHeader);
	Trc_BCU_Assert_NotEquals(NULL, jimage->j9jimageHeader->jimageHeader);

	/* It appears package to module mapping is stored in a resource named "/packages/<package name>".
	 * Eg resource named "/packages/java.lang" contains offset of the name of the module that contains java.lang package.
	 */
	packageNameLen = pkgPrefixLen + strlen(package) + 1; /* add 1 for '\0' character */
	packageName = j9mem_allocate_memory(packageNameLen, J9MEM_CATEGORY_CLASSES);
	if (NULL == packageName) {
		goto _end;
	}

	j9str_printf(PORTLIB, packageName, packageNameLen, "%s", packagePrefix);

	for (i = 0; i <= strlen(package); i++) { /* include '\0' character as well */
		/* convert any '/' to '.' */
		if ('/' == package[i]) {
			packageName[pkgPrefixLen + i] = '.';
		} else {
			packageName[pkgPrefixLen + i] = package[i];
		}
	}

	rc = j9bcutil_lookupJImageResource(portlib, jimage, &j9jimageLocation, packageName);
	if (J9JIMAGE_NO_ERROR != rc) {
		goto _end;
	}

	dataBuffer = (U_8 *)j9mem_allocate_memory((UDATA)j9jimageLocation.uncompressedSize, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataBuffer) {
		goto _end;
	}
	rc = j9bcutil_getJImageResource(portlib, jimage, &j9jimageLocation, dataBuffer, j9jimageLocation.uncompressedSize);
	if (J9JIMAGE_NO_ERROR != rc) {
		goto _end;
	}

	cursor = (U_8 *)dataBuffer;
	/* "/packages/<package name>" contains 2 tuples of 4 bytes each: isEmpty|Offset. Use the first module that is not empty. */
	while (cursor < (dataBuffer + j9jimageLocation.uncompressedSize)) {
		isEmpty = *(U_32 *)cursor;
		cursor += sizeof(U_32);
		if (0 == isEmpty) {
			moduleNameOffset = *(U_32 *)cursor;
			cursor += sizeof(U_32);
			break;
		} else {
			cursor += sizeof(U_32);
		}
	}

	j9jimageHeader = jimage->j9jimageHeader;
	jimageHeader = j9jimageHeader->jimageHeader;

	moduleName = (char *)((U_8 *)jimageHeader + JIMAGE_STRING_DATA_OFFSET(*jimageHeader) + moduleNameOffset);

_end:
	if (NULL != dataBuffer) {
		j9mem_free_memory(dataBuffer);
	}
	if (NULL != packageName) {
		j9mem_free_memory(packageName);
	}
	return moduleName;
}

void
j9bcutil_unloadJImage(J9PortLibrary *portlib, J9JImage *jimage)
{
	PORT_ACCESS_FROM_PORT(portlib);

	Trc_BCU_unloadJImage_Entry(jimage);

	if (NULL != jimage) {
		if (NULL != jimage->jimageMmap) {
			j9mmap_unmap_file(jimage->jimageMmap);
			jimage->jimageMmap = NULL;
		}
		if (-1 != jimage->fd) {
			j9file_close(jimage->fd);
			jimage->fd = -1;
		}
		jimage->fileName = NULL;
		jimage->fileLength = 0;
		jimage->j9jimageHeader = NULL;
		j9mem_free_memory(jimage);
	}
	Trc_BCU_unloadJImage_Exit();
}
