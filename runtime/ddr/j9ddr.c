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

#include <string.h>
#include "j9protos.h"
#include "j9port.h"

#include "j9ddr.h"
#include "ddrtable.h"

extern U_8 sizeofBool(void);

typedef struct {
	U_32 x1 : 1;
	U_32 x2 : 15;
	U_32 x3 : 6;
	U_32 x4 : 10;

	U_32 x5 : 7;
	U_32 x6 : 9;
	U_32 x7 : 4;
	U_32 x8 : 12;
} bitField;

#define DEBUG

#define J9DDR_CORE_FILE_DATA_VERSION 1

typedef struct {
	U_32 version;
	U_8 sizeofBool;
	U_8 sizeofUDATA;
	U_8 bitfieldFormat;
	U_8 unused;
	U_32 structDataSize;
	U_32 stringDataSize;
	U_32 structureCount;
} J9DDRCoreFileData;

typedef struct {
	OMRPortLibrary * portLibrary;
	J9DDRCoreFileData * coreFileData;
	const J9DDRStructDefinition ** structTables;
	UDATA totalSize;
	J9HashTable * stringTable;
	void * cursor;
	char * filename;
} J9DDRTranslationData;

typedef struct {
	const char * cString;
	U_32 offset;
} J9DDRStringTableEntry;

typedef struct {
	U_32 nameOffset;
	U_32 superOffset;
	U_32 structSize;
	U_32 fieldCount;
	U_32 constantCount;
} J9DDRCoreFileStruct;

typedef struct {
	U_32 nameOffset;
	U_32 typeOffset;
	U_32 offset;
} J9DDRCoreFileField;

typedef struct {
	U_32 nameOffset;
	U_32 value[2];
} J9DDRCoreFileConstant;

static jint initializeBitfieldEncoding PROTOTYPE((J9DDRTranslationData * data));
static jint processDDRStructTableList PROTOTYPE((J9DDRTranslationData * data));
static jint writeDataFile PROTOTYPE((J9DDRTranslationData * data));
static jint processDDRStructTable PROTOTYPE((J9DDRTranslationData * data, const J9DDRStructDefinition * ddrStruct));
static U_32 stringTableOffset PROTOTYPE((J9DDRTranslationData * data, const char * cString));
static void copyStringTable PROTOTYPE((J9DDRTranslationData * data));
static jint processDDRField PROTOTYPE((J9DDRTranslationData * data, const J9DDRFieldDeclaration * ddrField, U_32 * size));
static UDATA stringTableHash PROTOTYPE((void *key, void *userData));
static UDATA stringTableEquals PROTOTYPE((void *leftKey, void *rightKey, void *userData));
static jint processDDRConstant PROTOTYPE((J9DDRTranslationData * data, const J9DDRConstantDeclaration * ddrConstant, U_32 * size));
static jint createDDRCoreData PROTOTYPE((J9DDRTranslationData * data));

/* Try to initialize the NLS catalog; otherwise use defaults   */
static void
ddr_setNLSCatalog(OMRPortLibrary * portLib, char **argv)
{
	char *exename = NULL;

	OMRPORT_ACCESS_FROM_OMRPORT(portLib);

	/* Look for java.home in the executable's directory */
	if ( omrsysinfo_get_executable_name( argv[0], &exename ) == 0 ) {

		/* find the last separator and add the "java" string */
		omrnls_set_catalog( (const char**)&exename, 1, "java", "properties" );
	}
	/* Do /not/ delete executable name string; system-owned. */
}


static jint
createDDRCoreData(J9DDRTranslationData * data)
{
	OMRPORT_ACCESS_FROM_OMRPORT(data->portLibrary);
	J9HashTable * stringTable = NULL;
	jint rc = JNI_OK;
	J9DDRCoreFileData pass1Header;
	J9DDRCoreFileData * coreFileData;

	omrtty_printf("Encoding struct data\n");

	/* Create an empty string table */

	stringTable = hashTableNew(OMRPORTLIB, J9_GET_CALLSITE(), 0, sizeof(J9DDRStringTableEntry), 0, 0, OMRMEM_CATEGORY_VM, stringTableHash, stringTableEquals, NULL, NULL);
	if (stringTable == NULL) {
		omrtty_printf("ERROR: Unable to allocate string hash table\n");
		rc = JNI_ENOMEM;
		goto done;
	}

	/* Run the conversion once without writing to compute the size */

	data->stringTable = stringTable;
	data->coreFileData = &pass1Header;
	memset(&pass1Header, 0, sizeof(pass1Header));
	rc = processDDRStructTableList(data);
	if (rc != JNI_OK) {
		omrtty_printf("ERROR: Pass 1 encoding failed\n");
		data->coreFileData = NULL;
		rc = JNI_ENOMEM;
		goto done;
	}

	/* Allocate memory for the header, struct data and string table and copy in the header */

	data->totalSize = sizeof(pass1Header) + pass1Header.structDataSize + pass1Header.stringDataSize;
	coreFileData = omrmem_allocate_memory(data->totalSize, OMRMEM_CATEGORY_VM);
	if (coreFileData == NULL) {
		omrtty_printf("ERROR: Unable to allocate temp space of size %d for encoded data\n", data->totalSize);
		rc = JNI_ENOMEM;
		goto done;
	}
	data->coreFileData = coreFileData;
	*coreFileData = pass1Header;

	/* Run the conversion again, writing the struct data to the allocated memory */

	memset(coreFileData, 0, data->totalSize);
	coreFileData->stringDataSize = pass1Header.stringDataSize;
	data->cursor = coreFileData + 1;
	rc = processDDRStructTableList(data);
	if (rc != JNI_OK) {
		omrtty_printf("ERROR: Pass 2 encoding failed\n");
		rc = JNI_ERR;
		goto done;
	}

	/* Sanity check to make sure the second pass computations match the first pass */

	if (memcmp(&pass1Header, coreFileData, sizeof(pass1Header)) != 0) {
		omrtty_printf("ERROR: Pass 2 header does not match pass 1\n");
		rc = JNI_EINVAL;
		goto done;
	}
	if ((((U_8 *)(coreFileData  +1)) + coreFileData->structDataSize) != data->cursor) {
		omrtty_printf("ERROR: Pass 2 data size does not match pass 1\n");
		rc = JNI_EINVAL;
		goto done;
	}

	/* Write the string table after the struct data */

	copyStringTable(data);

	/* Sanity check to make sure the second pass computations match the first pass */

	if ((((U_8 *)(coreFileData  +1)) + coreFileData->structDataSize + coreFileData->stringDataSize) != data->cursor) {
		omrtty_printf("ERROR: Pass 2 string table size does not match pass 1\n");
		rc = JNI_EINVAL;
		goto done;
	}

	/* Fill in compile-time constants */

	coreFileData->version = J9DDR_CORE_FILE_DATA_VERSION;
	coreFileData->sizeofBool = sizeofBool();
	coreFileData->sizeofUDATA = sizeof(UDATA);
	rc = initializeBitfieldEncoding(data);

done:
	if (stringTable != NULL) {
		hashTableFree(stringTable);
	}

	return rc;
}

static jint
processDDRStructTableList(J9DDRTranslationData * data)
{
	const J9DDRStructDefinition ** list = data->structTables;

	/* Process each struct table in the list */
	while(NULL != *list) {
		jint rc = processDDRStructTable(data, *list++);

		if (rc != JNI_OK) {
			return rc;
		}
	}

	/* Round the string data size up to U_32 to maintain alignment of anything which comes after it */

	data->coreFileData->stringDataSize = (data->coreFileData->stringDataSize + 3) & ~(U_32)3;

	return JNI_OK;
}


static void
copyStringTable(J9DDRTranslationData * data)
{
	U_8 * stringData = data->cursor;
	J9HashTableState state;
	J9DDRStringTableEntry * entry;

	/* Iterate the table and copy each string to its already-assigned offset */

	entry = hashTableStartDo(data->stringTable, &state);
	while (entry != NULL) {
		J9UTF8 * utf = (J9UTF8 *)(stringData + entry->offset);
		const char * cString = entry->cString;
		size_t len = strlen(cString);

		J9UTF8_SET_LENGTH(utf, (U_16)len);
		memcpy(J9UTF8_DATA(utf), cString, len);

		entry = hashTableNextDo(&state);
	}

	/* Update cursor (stringDataSize is U_32 aligned already) */

	data->cursor = stringData + data->coreFileData->stringDataSize;
}

static UDATA
stringTableHash(void *key, void *userData)
{
	const char *s = ((J9DDRStringTableEntry *)key)->cString;
	size_t length = strlen(s);
	UDATA hash = 0;
	U_8 * data = (U_8 *) s;
	const U_8 * end = data + length;

	while (data < end) {
		U_16 c;

		data += decodeUTF8Char(data, &c);
		hash = (hash << 5) - hash + c;
	}
	return hash;
}
static UDATA
stringTableEquals(void *leftKey, void *rightKey, void *userData) 
{
	const char *left_s = ((J9DDRStringTableEntry *)leftKey)->cString;
	const char *right_s = ((J9DDRStringTableEntry *)rightKey)->cString;

	return (left_s == right_s) || (0 == strcmp(left_s, right_s));
}
static U_32
stringTableOffset(J9DDRTranslationData * data, const char * cString)
{
	J9DDRStringTableEntry exemplar;
	J9DDRStringTableEntry * entry;

	exemplar.cString = cString;
	exemplar.offset = U_32_MAX;

	/* Add will return an existing entry if one matches, NULL on allocation failure, or the new node */

	entry = hashTableAdd(data->stringTable, &exemplar);

	/* NULL means the string was new, but memory allocation failed */

	if (entry == NULL) {
		OMRPORT_ACCESS_FROM_OMRPORT(data->portLibrary);
		omrtty_printf("ERROR: Unable to allocate memory for string table entry: %s", cString);
		return U_32_MAX;
	}

	/* If the offset is U_32_MAX, this indicates that a new entry was added */

	if (entry->offset == U_32_MAX) {
		entry->offset = data->coreFileData->stringDataSize;

		/* Reserve space for the U_16 size, the string data, and the padding byte, if necessary */

		data->coreFileData->stringDataSize += (U_32)((sizeof(U_16) + strlen(cString) + 1) & ~(size_t)1);
	}

	return entry->offset;
}
static jint
processDDRStructTable(J9DDRTranslationData * data, const J9DDRStructDefinition * ddrStruct)
{
	/* Process each struct definition in the array (table is terminated with an all-zero entry */

	while (ddrStruct->name != NULL) {
		J9DDRCoreFileStruct * coreFileStruct = data->cursor;
		const J9DDRFieldDeclaration * ddrField;
		const J9DDRConstantDeclaration * ddrConstant;
		U_32 nameOffset;
		U_32 superOffset;
		U_32 fieldCount = 0;
		U_32 constantCount = 0;
		jint rc;

		if (data->cursor != NULL) {
			data->cursor = coreFileStruct + 1;
		}
		data->coreFileData->structDataSize += sizeof(J9DDRCoreFileStruct);

		/* Process fields */

		ddrField = ddrStruct->fields;
		if (ddrField != NULL) {
			while (ddrField->name != NULL) {
				rc = processDDRField(data, ddrField, &(data->coreFileData->structDataSize));
				if (rc != JNI_OK) {
					return rc;
				}
				++fieldCount;
				++ddrField;
			}
		}

		/* Process constants */

		ddrConstant = ddrStruct->constants;
		if (ddrConstant != NULL) {
			while (ddrConstant->name != NULL) {
				rc = processDDRConstant(data, ddrConstant, &(data->coreFileData->structDataSize));
				if (rc != JNI_OK) {
					return rc;
				}
				++constantCount;
				++ddrConstant;
			}
		}

		/* Create the strings */

		nameOffset = stringTableOffset(data, ddrStruct->name);
		if (nameOffset == U_32_MAX) {
			return JNI_ENOMEM;
		}
		if (ddrStruct->superName == NULL) {
			superOffset = U_32_MAX;
		} else {
			superOffset = stringTableOffset(data, ddrStruct->superName);
			if (superOffset == U_32_MAX) {
				return JNI_ENOMEM;
			}
		}

		/* Fill in the header */

		if (data->cursor != NULL) {
			coreFileStruct->nameOffset = nameOffset;
			coreFileStruct->superOffset = superOffset;
			coreFileStruct->structSize = ddrStruct->size;
			coreFileStruct->fieldCount = fieldCount;
			coreFileStruct->constantCount = constantCount;
		}

		data->coreFileData->structureCount += 1;
		++ddrStruct;
	}

	return JNI_OK;
}

static jint
processDDRField(J9DDRTranslationData * data, const J9DDRFieldDeclaration * ddrField, U_32 * size)
{
	U_32 nameOffset;
	U_32 typeOffset;

	/* Create the strings */

	nameOffset = stringTableOffset(data, ddrField->name);
	if (nameOffset == U_32_MAX) {
		return JNI_ENOMEM;
	}
	typeOffset = stringTableOffset(data, ddrField->type);
	if (typeOffset == U_32_MAX) {
		return JNI_ENOMEM;
	}

	/* Fill in the struct */

	if (data->cursor != NULL) {
		J9DDRCoreFileField * coreFileField = data->cursor;

		coreFileField->nameOffset = nameOffset;
		coreFileField->typeOffset = typeOffset;
		coreFileField->offset = ddrField->offset;

		data->cursor = coreFileField + 1;
	}

	/* Update required size */

	*size += sizeof(J9DDRCoreFileField);

	return JNI_OK;
}

static jint
processDDRConstant(J9DDRTranslationData * data, const J9DDRConstantDeclaration * ddrConstant, U_32 * size)
{
	U_32 nameOffset;

	/* Create the strings */

	nameOffset = stringTableOffset(data, ddrConstant->name);
	if (nameOffset == U_32_MAX) {
		return JNI_ENOMEM;
	}

	/* Fill in the struct */

	if (data->cursor != NULL) {
		J9DDRCoreFileConstant * coreFileConstant = data->cursor;

		memcpy(coreFileConstant->value, &ddrConstant->value, sizeof(U_64));
		coreFileConstant->nameOffset = nameOffset;

		data->cursor = coreFileConstant + 1;
	}

	/* Update required size */

	*size += sizeof(J9DDRCoreFileConstant);

	return JNI_OK;
}

UDATA
ddrSignalProtectedMain(struct OMRPortLibrary *portLibrary, void *arg)
{
	J9DDRCmdlineOptions *startupOptions = arg;
	int argc = startupOptions->argc;
	char **argv = startupOptions->argv;
	char **envp = startupOptions->envp;
	OMRPORT_ACCESS_FROM_OMRPORT(startupOptions->portLibrary);
	J9DDRTranslationData data;
	UDATA rc = JNI_OK;

	memset(&data, 0, sizeof(data));
	data.portLibrary = OMRPORTLIB;
	data.filename = J9DDR_DAT_FILE;

	ddr_setNLSCatalog(OMRPORTLIB, argv);

	/* Initialize the thread library -- Zip code now uses monitors. */
	omrthread_attach_ex(NULL, J9THREAD_ATTR_DEFAULT);

	data.structTables = initializeDDRComponents(data.portLibrary);
	if (NULL == data.structTables) {
		rc = JNI_ENOMEM;
		goto done;
	}

	rc = createDDRCoreData(&data);
	if (rc != JNI_OK) {
		goto done;
	}

	rc = writeDataFile(&data);
	if (rc != JNI_OK) {
		goto done;
	}

	omrtty_printf("Completed with no errors\n");

done:
	omrmem_free_memory(data.coreFileData);
	omrmem_free_memory((void*) data.structTables);
	return (UDATA) rc;
}

static jint
writeDataFile(J9DDRTranslationData * data)
{
	OMRPORT_ACCESS_FROM_OMRPORT(data->portLibrary);
	IDATA fd;
	IDATA length = data->totalSize;

	omrtty_printf("Creating output file %s of size %i\n", data->filename, length);
	fd = omrfile_open(data->filename, EsOpenWrite | EsOpenCreate | EsOpenTruncate, 0666);
	if (fd == -1) {
		omrtty_printf("ERROR: Failed to open output file\n");
		return JNI_ERR;
	}

	if (omrfile_write(fd, data->coreFileData, length) != length) {
		omrtty_printf("ERROR: Failed to write to output file\n");
		omrfile_close(fd);
		return JNI_ERR;
	}

	if (omrfile_close(fd) != 0) {
		omrtty_printf("ERROR: Failed to close output file\n");
		return JNI_ERR;
	}

	return JNI_OK;
}



static jint
initializeBitfieldEncoding(J9DDRTranslationData * data)
{
	bitField bf;
	jint rc = JNI_OK;
	U_32 slot0;
	U_32 slot1;

	memset(&bf, 0, sizeof(bf));

	bf.x1 = 1;				/* 1 */
	bf.x2 = 31768;		/* 111110000011000 */
	bf.x3 = 3;				/* 000011 */
	bf.x4 = 777;			/* 1100001001 */

	bf.x5 = 43;			/* 0101011 */
	bf.x6 = 298;			/* 100101010 */
	bf.x7 = 12;			/* 1100 */
	bf.x8 = 1654;		/* 011001110110 */

	slot0 = ((U_32*)&bf)[0];
	slot1 = ((U_32*)&bf)[1];
	if ((slot0 ==0xC243F831) && (slot1 == 0x676C952B)) {
		data->coreFileData->bitfieldFormat = 1;
	} else if ((slot0 ==0xFC180F09) && (slot1 == 0x572AC676)) {
		data->coreFileData->bitfieldFormat = 2;
	} else {
		OMRPORT_ACCESS_FROM_OMRPORT(data->portLibrary);

		omrtty_printf("ERROR: Unable to determine bitfield format from %08X %08X\n", slot0, slot1);
		rc = JNI_ERR;
	}

	return rc;
}

