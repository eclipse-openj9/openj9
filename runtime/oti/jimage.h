/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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

#ifndef jimage_h
#define jimage_h

#include "j9.h"
#include "j9comp.h"
#include "j9pool.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Magic value in jimage header */
#define JIMAGE_HEADER_MAGIC 0xCAFEDADA

/* Major and minor versions in jimage header */
#define JIMAGE_MAJOR_VERSION 0
#define JIMAGE_MINOR_VERSION 1

/* Macros to get type and length from "type" byte in attribute */
#define JIMAGE_TYPE_BYTE_TYPE_MASK 0xF8	/* highest five bits in "type" byte represent type */
#define JIMAGE_TYPE_BYTE_TYPE_SHIFT 3
#define JIMAGE_TYPE_BYTE_LENGTH_MASK 0x7		/* lowest three bits in "type" byte represent date length */

/* Macros representing attribute type */
#define J9JIMAGE_ATTRIBUTE_TYPE_END 0
#define J9JIMAGE_ATTRIBUTE_TYPE_MODULE 1
#define J9JIMAGE_ATTRIBUTE_TYPE_PARENT 2
#define J9JIMAGE_ATTRIBUTE_TYPE_BASE 3
#define J9JIMAGE_ATTRIBUTE_TYPE_EXTENSION 4
#define J9JIMAGE_ATTRIBUTE_TYPE_OFFSET 5
#define J9JIMAGE_ATTRIBUTE_TYPE_COMPRESSED 6
#define J9JIMAGE_ATTRIBUTE_TYPE_UNCOMPRESSED 7

/* Initial seed used during hashing */
#define JIMAGE_LOOKUP_HASH_SEED 0x01000193

#define JIMAGE_HEADER_SIZE(header) sizeof(JImageHeader)

#define JIMAGE_REDIRECT_TABLE_OFFSET(header) JIMAGE_HEADER_SIZE(header)
#define JIMAGE_REDIRECT_TABLE_SIZE(header) ((header).tableLength * sizeof(I_32))

#define JIMAGE_LOCATION_OFFSETS_TABLE_OFFSET(header) (JIMAGE_REDIRECT_TABLE_OFFSET(header) + JIMAGE_REDIRECT_TABLE_SIZE(header))
#define JIMAGE_LOCATION_OFFSETS_TABLE_SIZE(header) ((header).tableLength * sizeof(U_32))

#define JIMAGE_LOCATION_DATA_OFFSET(header) (JIMAGE_LOCATION_OFFSETS_TABLE_OFFSET(header) + JIMAGE_LOCATION_OFFSETS_TABLE_SIZE(header))
#define JIMAGE_LOCATION_DATA_SIZE(header) ((header).locationsSize)

#define JIMAGE_STRING_DATA_OFFSET(header) (JIMAGE_LOCATION_DATA_OFFSET(header) + JIMAGE_LOCATION_DATA_SIZE(header))
#define JIMAGE_STRING_DATA_SIZE(header) ((header).stringsSize)

#define JIMAGE_RESOURCE_AREA_OFFSET(header) (JIMAGE_STRING_DATA_OFFSET(header) + JIMAGE_STRING_DATA_SIZE(header))

#define JIMAGE_DECOMPRESSOR_MAGIC_BYTE 0xCAFEFAFA

typedef struct JImageHeader {
	U_32 magic;
	U_16 majorVersion;
	U_16 minorVersion;
	U_32 flags;
	U_32 resourceCount;
	U_32 tableLength;
	U_32 locationsSize;
	U_32 stringsSize;
} JImageHeader;

typedef struct J9JImageHeader {
	struct JImageHeader *jimageHeader;	/* pointer to header in jimage file */
	I_32 *redirectTable; 				/* pointer to redirect table of jimage file */
	U_32 *locationsOffsetTable;			/* pointer to locations offset table in jimage file */
	U_8 *locationsData;					/* pointer to locations data in jimage file */
	U_8 *stringsData;					/* pointer to strings data in jimage file */
	U_64 resources;						/* offset of resources data in jimage file */
} J9JImageHeader;

typedef struct J9JImage {
	IDATA fd;
	char *fileName;
	U_64 fileLength;
	struct J9JImageHeader *j9jimageHeader;
	J9MmapHandle *jimageMmap;
} J9JImage;

typedef struct DecompressorInfo {
	U_32 magic;
	U_32 resourceLength;
	U_32 uncompressedSize;
	U_32 decompressorNameOffset;
	U_32 decompressorConfigNameOffset;
	U_8 decompressorFlag;
} DecompressorInfo;

typedef struct J9JImageLocation {
	char *moduleString;
	char *parentString;
	char *baseString;
	char *extensionString;
	U_64 compressedSize;
	U_64 uncompressedSize;
	U_64 resourceOffset;
} J9JImageLocation;

#ifdef __cplusplus
}
#endif

#endif /* jimage_h */
