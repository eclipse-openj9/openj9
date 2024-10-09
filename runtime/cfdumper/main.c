/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "j9.h"
#include "j9port.h"
#include "cfreader.h"
#include "bcnames.h"
#include "j9protos.h"
#include "jimage.h"
#include "jimagereader.h"
#include "zip_api.h"
#include "rommeth.h"
#include "exelib_api.h"
#include "j9cp.h"
#include "bcutil_api.h"
#include "cfdumper_internal.h"
#include "vendor_version.h"

#if defined(J9ZOS390)
#include "atoe.h"
#endif

#define CFDUMP_CLASSFILE_EXTENSION ".class"
#define CFDUMP_CLASSFILE_EXTENSION_WITHOUT_DOT "class"

/* Return values. */
#define RET_SUCCESS                                0
#define RET_ALLOCATE_FAILED                       -1
#define RET_TRANSLATE_FAILED                      -2
#define RET_COMMANDLINE_INCORRECT                 -3
#define RET_UNSUPPORTED_ACTION                    -4
#define RET_UNSUPPORTED_SOURCE                    -5
#define RET_FILE_OPEN_FAILED                     -11
#define RET_FILE_SEEK_FAILED                     -12
#define RET_FILE_READ_FAILED                     -13
#define RET_FILE_WRITE_FAILED                    -14
#define RET_FILE_LOAD_FAILED                     -15
#define RET_ZIP_OPEN_FAILED                      -16
#define RET_ZIP_GETENTRY_FAILED                  -17
#define RET_ZIP_GETNEXTZIPENTRY_FAILED           -18
#define RET_ZIP_GETZIPENTRYFILENAME_FAILED       -19
#define RET_ZIP_GETENTRYDATA_FAILED              -20
#define RET_JIMAGE_LOADJIMAGE_FAILED             -21
#define RET_JIMAGE_CREATEJIMAGELOCATION_FAILED   -22
#define RET_JIMAGE_GETJIMAGERESOURCE_FAILED      -23
#define RET_JIMAGE_INVALIDLOCATION_OFFSET        -24
#define RET_LOADROMCLASSFROMBYTES_FAILED         -25
#define RET_LOADCLASSFROMBYTES_FAILED            -26
#define RET_CREATE_DIR_FAILED                    -27
#define RET_PARSE_CLASS_NAME_FAILED              -28

typedef enum
{
	ACTION_definition = 0,
	ACTION_structure,
	ACTION_bytecodes,
	ACTION_formatClass,
	ACTION_formatFields,
	ACTION_formatMethods,
	ACTION_formatBytecodes,
	ACTION_transformRomClass,
	ACTION_writeRomClassFile,
	ACTION_writeRomClassFileHierarchy,
	ACTION_dumpMaps,
	ACTION_dumpRomClass,
	ACTION_dumpRomClassXML,
	ACTION_queryRomClass,
	ACTION_dumpJImageInfo,
	ACTION_writeJImageResource,
	ACTION_findModuleForPackage
} actionType;

typedef enum
{
	SOURCE_files = 0,
	SOURCE_zip,
	SOURCE_jimage,
	SOURCE_rom
} sourceType;

typedef enum
{
	OPTION_none = 0x0,
	OPTION_check = 0x1,
	OPTION_stripDebugAttributes = 0x2,
	OPTION_quiet = 0x4,
	OPTION_verbose = 0x8,
	OPTION_multi = 0x10,
	OPTION_recursive = 0x20,
	OPTION_translate = 0x40,
	OPTION_littleEndian = 0x80,
	OPTION_bigEndian = 0x100,
	OPTION_dumpPreverifyData = 0x200,
	OPTION_leaveJSRs = 0x400,
	OPTION_stripDebugLines = 0x800,
	OPTION_stripDebugSource = 0x1000,
	OPTION_stripDebugVars = 0x2000,
	OPTION_pedantic = 0x4000,
	OPTION_dumpMaps = 0x8000,
} optionType;

typedef struct
{
	const char *module;
	char *parentString;
	char *baseString;
} JImageMatchInfo;


static void j9_formatMethod (J9ROMClass* romClass, J9ROMMethod* method, char *formatString, IDATA length, U_32 flags);
static void dumpAnnotations (J9CfrClassFile* classfile, J9CfrAnnotation *annotations, U_32 annotationCount, U_32 tabLevel);
static void dumpTypeAnnotations (J9CfrClassFile* classfile, J9CfrTypeAnnotation *annotations, U_32 annotationCount, U_32 tabLevel);
static void printClassFile (J9CfrClassFile* classfile);
static void tty_output_char (J9PortLibrary *portLib, const char c);
static U_32 buildFlags (void);
static void sun_formatBytecode (J9CfrClassFile* classfile, J9CfrMethod* method, BOOLEAN bigEndian, U_8* bcStart, U_8* bytes, U_8 bc, U_32 bytesLength, U_32 decode, char *formatString, IDATA length);
static void sun_formatBytecodes (J9CfrClassFile* classfile, J9CfrMethod* method, BOOLEAN bigEndian, U_8* bytecodes, U_32 bytecodesLength, char *formatString, IDATA stringLength);
static I_32 processSingleFile (char* requestedFile, U_32 flags);
static void j9_formatField (J9ROMClass* romClass, J9ROMFieldShape* field, char *formatString, IDATA length, U_32 flags);
static I_32 processAllFiles (char** files, U_32 flags);
static void dumpStackMap (J9CfrAttributeStackMap * stackMap, J9CfrClassFile* classfile, U_32 tabLevel);
static int parseCommandLine (int argc, char** argv, char** envp);
static void dumpAnnotationElement (J9CfrClassFile* classfile, J9CfrAnnotationElement *element, U_32 tabLevel);
static void sun_formatMethod (J9CfrClassFile* classfile, J9CfrMethod* method, char *formatString, IDATA length);
static I_32 processROMFile (char* requestedFile, U_32 flags);
static BOOLEAN validateROMClassAddressRange (J9ROMClass *romClass, void *address, UDATA length, void *userData);
static I_32 processROMClass (J9ROMClass* romClass, char* requestedFile, U_32 flags);
static void dumpMethod (J9CfrClassFile* classfile, J9CfrMethod* method);
static void sun_formatField (J9CfrClassFile* classfile, J9CfrField* field, char *formatString, IDATA length);
static void printMethod (J9CfrClassFile* classfile, J9CfrMethod* method);
static I_32 processFilesInZIP (char* zipFilename, char** files, U_32 flags);
static I_32 processDirectory (char* dir, BOOLEAN recursive, U_32 flags);
static void dumpField (J9CfrClassFile* classfile, J9CfrField* field);
static void j9_formatBytecode (J9ROMClass* romClass, J9ROMMethod* method, U_8* bcStart, U_8* bytes, U_8 bc, U_32 bytesLength, U_32 decode, char *formatString, IDATA length, U_32 flags);
static void j9_formatBytecodes (J9ROMClass* romClass, J9ROMMethod* method, U_8* bytecodes, U_32 bytecodesLength, char *formatString, IDATA stringLength, U_32 flags);
static void dumpHelpText ( J9PortLibrary *portLib, int argc, char **argv);
static I_32 processAllInZIP (char* zipFilename, U_32 flags);
static J9CfrClassFile* loadClassFile (char* requestedFile, U_32* lengthReturn, U_32 flags);
static void j9_formatClass (J9ROMClass* romClass, char *formatString, IDATA length, U_32 flags);
static J9ROMClass *translateClassBytes (U_8 * data, U_32 dataLength, char *requestedFile, U_32 flags);
static void printDisassembledMethod (J9CfrClassFile* classfile, J9CfrMethod* method, BOOLEAN bigEndian, U_8* bytecodes, U_32 bytecodesLength);
static void printDisassembledMethods (J9CfrClassFile *classfile);
static void dumpClassFile (J9CfrClassFile* classfile);
static U_8 * dumpStackMapSlots (J9CfrClassFile* classfile, U_8 * slotData, U_16 slotCount);
static I_32 processClassFile (J9CfrClassFile* classfile, U_32 dataLength, char* requestedFile, U_32 flags);
static I_32 getBytes (const char* filename, U_8** dataHandle);
static void reportClassLoadError (J9CfrError* error, char* requestedFile);
static void printField (J9CfrClassFile* classfile, J9CfrField* field);
static J9CfrClassFile* loadClassFromBytes (U_8* data, U_32 dataLength, char* requestedFile, U_32 flags);
static U_8* loadBytes (char* requestedFile, U_32* lengthReturn, U_32 flags);
static void sun_formatClass (J9CfrClassFile* classfile, char *formatString, IDATA length);
static I_32 writeOutputFile (U_8 *fileData, U_32 length, char* filename, char *extension);
static void dumpAttribute (J9CfrClassFile* classfile, J9CfrAttribute* attrib, U_32 tabLevel);
static I_32 convertToClassFilename(const char **files, char ***classFiles, I_32 *fileCount);
static I_32 processFilesInJImage(J9JImage *jimage, char* jimageFileName, char** files, U_32 flags);
static I_32 convertToJImageLocations(J9JImage *jimage, char **files, JImageMatchInfo ** output, I_32 *fileCount);
static I_32 processAllInJImage(J9JImage *jimage, char* jimageFileName, U_32 flags);
static void dumpRawData(char *message,	U_32 rawDataLength, U_8 *rawData);

UDATA signalProtectedMain(struct J9PortLibrary *portLibrary, void *arg);


typedef struct
{
	actionType action;
	sourceType source;
	I_32 options;
	I_32 filesStart;
	char *sourceString;
	char *actionString;
} CFDumpOptions;

#define j9tty_output_char(c) tty_output_char(PORTLIB, (c))

#if defined(WIN32) || defined(OS2)
#define PATH_SEP_CHAR '\\'
#else /* defined(WIN32) || defined(OS2) */
#define PATH_SEP_CHAR '/'
#endif /* defined(WIN32) || defined(OS2) */

static J9TranslationBufferSet *translationBuffers;
static J9BytecodeVerificationData *verifyBuffers;
J9PortLibrary* portLib;
CFDumpOptions options;

static I_32 getBytes(const char* filename, U_8** dataHandle)
{
 	U_8* data;
	U_32 fileSize;
	I_64 actualFileSize;
	IDATA fd;

	PORT_ACCESS_FROM_PORT(portLib);

	fd = j9file_open((char*)filename, EsOpenRead, 0);
	if(fd == -1) {
		return RET_FILE_OPEN_FAILED;
	}

	actualFileSize = j9file_seek(fd, 0, EsSeekEnd);
	/* Restrict file size to < 2G */
	if ((actualFileSize == -1) || (actualFileSize > J9CONST64(0x7FFFFFFF))) {
		j9file_close(fd);
		return RET_FILE_SEEK_FAILED;
	}
	fileSize = (U_32) actualFileSize;

	if (j9file_seek(fd, 0, EsSeekSet) == -1) {
		j9file_close(fd);
		return RET_FILE_SEEK_FAILED;
	}

	data = (U_8*)j9mem_allocate_memory(fileSize, J9MEM_CATEGORY_CLASSES);
	if(data == NULL) 	{
		j9file_close(fd);
		return RET_ALLOCATE_FAILED;
	}

	if(j9file_read(fd, data, (IDATA)fileSize) != (IDATA)fileSize)	{
		j9file_close(fd);
		return RET_FILE_READ_FAILED;
	}

	j9file_close(fd);
	*dataHandle = data;
	return (I_32)fileSize;
}

/**
 * Converts list of class names to file names by converting '.' to '/' and
 * by appending ".class" extension if not already present.
 * 
 * @param [in] files list of class names
 * @param [out] classFiles if not NULL then on exit points to list of converted class file names
 * @param [out] fileCount size of the list pointed by classFiles
 * 
 * @return RET_SUCCESS on success, negative error code on failure
 */
static I_32
convertToClassFilename(const char **files, char ***classFiles, I_32 *fileCount) {
	char **convertedFiles = NULL;
	char *currentFile = NULL;
	I_32 count = 0;
	UDATA size = 0;
	I_32 i = 0;
	I_32 result = RET_SUCCESS;
	UDATA classExtLen = sizeof(CFDUMP_CLASSFILE_EXTENSION) - 1;

	PORT_ACCESS_FROM_PORT(portLib);

	/* Convert the various file specifiers to the expected one -- java/lang/Object.class */
	currentFile = (char *)files[count];
	while (NULL != currentFile) {
		count += 1;
		size += strlen(currentFile) + classExtLen + 1;		/* NULL and potentially the ".class" */
		currentFile = (char *)files[count];
	}
	/*
	 * convertedFiles is an array of pointers with each element pointing to the converted file name.
	 * Just after the array we store the strings for converted file names. So the memory layout is:
	 * 
	 * 		pointer to 1st file name string
	 * 		pointer to 2nd file name string
	 * 		...
	 * 		pointer to nth file name string
	 * 		1st file name string
	 * 		2nd file name string
	 * 		...
	 * 		nth file name string
	 */
	convertedFiles = j9mem_allocate_memory((count * sizeof(char*)) + size, J9MEM_CATEGORY_CLASSES);
	if (NULL == convertedFiles) {
		result = RET_ALLOCATE_FAILED;
		goto _end;
	}
	currentFile = (char *)((U_8 *)convertedFiles + (count * sizeof(char*)));
	for (i = 0; i < count; i++) {
		const char *file = files[i];
		UDATA length = strlen(file);
		UDATA j = 0;

		if ((length > classExtLen) && !strncmp(&file[length - classExtLen], CFDUMP_CLASSFILE_EXTENSION, classExtLen)) {
			length -= classExtLen;
		}
		for (j = 0; j < length; j++) {
			if ((file[j] == '.') || (file[j] == '\\')) {
				currentFile[j] = '/';
			} else {
				currentFile[j] = file[j];
			}
		}
		strcpy(&currentFile[length], CFDUMP_CLASSFILE_EXTENSION);
		currentFile[length + classExtLen] = '\0';
		convertedFiles[i] = currentFile;
		currentFile += length + classExtLen + 1; /* ".class" */
	}

_end:
	if (NULL != classFiles) {
		*classFiles = convertedFiles;
	}
	if (NULL != fileCount) {
		*fileCount = count;
	}
	return result;
}


/**
 * Takes a list of class names in the form of package.name.ClassName[.class]
 * and converts to a list of JImageMatchInfo structs.
 *
 * @param [in]  jimage    The jimage file in which the class is going to be looked up
 * @param [in]  files     list of class names to process
 * @param [out] output    if not NULL then on exit points to list of converted class file names. Caller responsible for cleanup
 * @param [out] fileCount If non-null, number of names processed is written here
 *
 * @return RET_SUCCESS on success, negative error code on failure
 */
static I_32
convertToJImageLocations(J9JImage *jimage, char **files, JImageMatchInfo ** output, I_32 *fileCount) {
	char *currentFile = (char *)files[0];
	JImageMatchInfo *matchData = NULL;
	I_32 count = 0;
	UDATA size = 0;
	I_32 result = RET_SUCCESS;
	char * buffer = NULL;
	I_32 i = 0;

	PORT_ACCESS_FROM_PORT(portLib);


	while (NULL != currentFile){
		count += 1;
		/* add room for null char */
		size += strlen(currentFile) + 1;
		currentFile = (char *)files[count];
	}

	/* if caller just wants the class count, then we are done */
	if (NULL == output) {
		goto _end;
	}

	matchData = (JImageMatchInfo*) j9mem_allocate_memory((count * sizeof(JImageMatchInfo) + size), J9MEM_CATEGORY_CLASSES);
	if (NULL == matchData) {
		result = RET_ALLOCATE_FAILED;
		j9tty_printf( PORTLIB, "Insufficient memory to complete operation\n");
		goto _end;
	}
	/* We begin putting strings after Match info structs */
	buffer = (char*) (matchData + count);
	for (i = 0; i < count; ++i) {
		char *lastDot = NULL;
		char * const file = buffer;
		UDATA length = strlen(files[i]);
		I_32 charIdx= 0;
		char currentChar = 0;

		memcpy(file, files[i], length + 1);

		/* Check if input ends with '.class' */
		if (
			(length > sizeof(CFDUMP_CLASSFILE_EXTENSION)) &&
			(0 == strcmp(file + length - sizeof(CFDUMP_CLASSFILE_EXTENSION) + 1, CFDUMP_CLASSFILE_EXTENSION))) {
			/* ignore the .class extension */
			file[length - sizeof(CFDUMP_CLASSFILE_EXTENSION)+1] = '\0';
		}
		/* Split into package & class */
		lastDot = strrchr(file, '.');
		if (NULL == lastDot) {
			j9tty_printf( PORTLIB, "Failed parsing class %s\n", files[i]);
			result = RET_PARSE_CLASS_NAME_FAILED;
			goto _end;
		}

		*lastDot = '\0';
		matchData[i].baseString = lastDot + 1;
		matchData[i].module = j9bcutil_findModuleForPackage(portLib, jimage, file);

		/* replace dots with slashes */
		while (0 != (currentChar = file[charIdx])) {

			if ('.' == currentChar) {
				file[charIdx] = '/';
			}
			++charIdx;
		}
		matchData[i].parentString = file;
		buffer += length + 1;
	}

_end:
	if (RET_SUCCESS == result){
		if (NULL != output) {
			*output = matchData;
		}
		if (NULL != fileCount) {
			*fileCount = count;
		}
	} else {
		/* free copied strings */
		if (matchData != NULL) {
			j9mem_free_memory(matchData);
		}
	}
	return result;
}

static void dumpClassFile(J9CfrClassFile* classfile)
{
	U_32 i;
	U_32 index;

	PORT_ACCESS_FROM_PORT(portLib);

	j9tty_printf( PORTLIB, "Magic: %X\n", classfile->magic);
	j9tty_printf( PORTLIB, "Version: %i.%i\n", classfile->majorVersion, classfile->minorVersion);
	j9tty_printf( PORTLIB, "Constant Pool Size: %i\n", classfile->constantPoolCount);
	j9tty_printf( PORTLIB, "Access Flags: 0x%X ( ", classfile->accessFlags);
	printModifiers(PORTLIB, classfile->accessFlags, INCLUDE_INTERNAL_MODIFIERS, MODIFIERSOURCE_CLASS, J9_IS_CLASSFILE_VALUETYPE(classfile));
	j9tty_printf( PORTLIB, " )\n");
	index = classfile->constantPool[classfile->thisClass].slot1;
	j9tty_printf( PORTLIB, "ThisClass: %i -> %s\n", classfile->thisClass, classfile->constantPool[index].bytes);
	index = classfile->constantPool[classfile->superClass].slot1;
	j9tty_printf( PORTLIB, "SuperClass: %i -> %s\n", classfile->superClass, classfile->constantPool[index].bytes);
	j9tty_printf( PORTLIB, "Interfaces (%i):\n", classfile->interfacesCount);
	for(i = 0; i < classfile->interfacesCount; i++)
	{
		index = classfile->constantPool[classfile->interfaces[i]].slot1;
		j9tty_printf( PORTLIB, "  %i -> %s ", classfile->interfaces[i], classfile->constantPool[index].bytes);
	}
	j9tty_printf( PORTLIB, "\n");
	j9tty_printf( PORTLIB, "Fields (%i):\n", classfile->fieldsCount);
	for(i = 0; i < classfile->fieldsCount; i++)
	{
		dumpField(classfile, &(classfile->fields[i]));
		j9tty_printf( PORTLIB, "\n");
	}
	j9tty_printf( PORTLIB, "Methods (%i):\n", classfile->methodsCount);
	for(i = 0; i < classfile->methodsCount; i++)
	{
		dumpMethod(classfile, &(classfile->methods[i]));
		j9tty_printf( PORTLIB, "\n");
	}
	j9tty_printf( PORTLIB, "Attributes (%i):\n", classfile->attributesCount);
	for(i = 0; i < classfile->attributesCount; i++)
	{
		dumpAttribute(classfile, classfile->attributes[i], 1);
	}
	j9tty_printf( PORTLIB, "\n");
	return;
}


static void dumpField(J9CfrClassFile* classfile, J9CfrField* field)
{
	U_32 i;

	PORT_ACCESS_FROM_PORT(portLib);

	j9tty_printf( PORTLIB, "  Name: %i -> %s\n", field->nameIndex, classfile->constantPool[field->nameIndex].bytes);
	j9tty_printf( PORTLIB, "  Signature: %i -> %s\n", field->descriptorIndex, classfile->constantPool[field->descriptorIndex].bytes);
	j9tty_printf( PORTLIB, "  Access Flags: 0x%X ( ", field->accessFlags);
	printModifiers(PORTLIB, field->accessFlags, INCLUDE_INTERNAL_MODIFIERS, MODIFIERSOURCE_FIELD, FALSE);
	j9tty_printf( PORTLIB, " )\n");
	j9tty_printf( PORTLIB, "  Attributes (%i):\n", field->attributesCount);
	for(i = 0; i < field->attributesCount; i++)
	{
		dumpAttribute(classfile, field->attributes[i], 2);
	}
	return;
}

static void dumpRawData(char *message,	U_32 rawDataLength, U_8 *rawData) {
	PORT_ACCESS_FROM_PORT(portLib);
	U_32 i;
	j9tty_printf(PORTLIB, "%s:\n", message);
	for (i = 0; i < rawDataLength; ++i) {
		j9tty_printf(PORTLIB, "%0x ", rawData[i]);
	}
	j9tty_printf(PORTLIB, "\n");

}


static void dumpMethod(J9CfrClassFile* classfile, J9CfrMethod* method)
{
	U_32 i;

	PORT_ACCESS_FROM_PORT(portLib);

	j9tty_printf( PORTLIB, "  Name: %i -> %s\n", method->nameIndex, classfile->constantPool[method->nameIndex].bytes);
	j9tty_printf( PORTLIB, "  Signature: %i -> %s\n", method->descriptorIndex, classfile->constantPool[method->descriptorIndex].bytes);
	j9tty_printf( PORTLIB, "  Access Flags: 0x%X ( ", method->accessFlags);
	printModifiers(PORTLIB, method->accessFlags, INCLUDE_INTERNAL_MODIFIERS, MODIFIERSOURCE_METHOD, FALSE);
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	if (J9_IS_CLASSFILE_VALUETYPE(classfile)) {
		J9CfrConstantPoolInfo name = classfile->constantPool[method->nameIndex];
		if (J9UTF8_LITERAL_EQUALS(name.bytes, name.slot1, "<init>")) {
			for (i = 0; i < classfile->attributesCount; i++) {
				J9CfrAttribute* attr = classfile->attributes[i];
				if (attr->tag == CFR_ATTRIBUTE_ImplicitCreation) {
					j9tty_printf( PORTLIB, " implicit");
				}
			}
		}
	}
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
	j9tty_printf( PORTLIB, ")\n");
	j9tty_printf( PORTLIB, "  Attributes (%i):\n", method->attributesCount);
	for(i = 0; i < method->attributesCount; i++)
	{
		dumpAttribute(classfile, method->attributes[i], 2);
	}
	return;
}


static void dumpAttribute(J9CfrClassFile* classfile, J9CfrAttribute* attrib, U_32 tabLevel)
{
	U_16 index, index2;
	J9CfrAttributeCode* code;
	J9CfrAttributeInnerClasses* classes;
#if JAVA_SPEC_VERSION >= 11
	J9CfrAttributeNestMembers* nestMembers;
	U_16 nestMemberCount;
#endif /* JAVA_SPEC_VERSION >= 11 */
	J9CfrAttributeExceptions* exceptions;
	U_32 i;
	U_32 j;
	U_32 maxLength;

	PORT_ACCESS_FROM_PORT(portLib);

	for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
	j9tty_printf( PORTLIB, "%s:\n", classfile->constantPool[attrib->nameIndex].bytes);
	switch(attrib->tag)
	{
		case CFR_ATTRIBUTE_SourceFile:
			index = ((J9CfrAttributeSourceFile*)attrib)->sourceFileIndex;
			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf( PORTLIB, "Source File: %i -> %s\n", index, classfile->constantPool[index].bytes);
			break;

		case CFR_ATTRIBUTE_Signature:
			index = ((J9CfrAttributeSignature*)attrib)->signatureIndex;
			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf( PORTLIB, "Signature: %i -> %s\n", index, classfile->constantPool[index].bytes);
			break;

		case CFR_ATTRIBUTE_ConstantValue:
			index = ((J9CfrAttributeConstantValue*)attrib)->constantValueIndex;
			switch(classfile->constantPool[index].tag)
			{
				case CFR_CONSTANT_Integer:
					for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
					j9tty_printf( PORTLIB, "(int) %i\n", classfile->constantPool[index].slot1);
					break;

				case CFR_CONSTANT_Float:
					for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
					j9tty_printf( PORTLIB, "(float) 0x%08X\n", classfile->constantPool[index].slot1);
					break;

				case CFR_CONSTANT_Long:
					for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
					j9tty_printf( PORTLIB, "(long) 0x%08X%08X\n", classfile->constantPool[index].slot1, classfile->constantPool[index].slot2);
					break;

				case CFR_CONSTANT_Double:
					for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
					j9tty_printf( PORTLIB, "(double) 0x%08X%08X\n", classfile->constantPool[index].slot1, classfile->constantPool[index].slot2);
					break;

				case CFR_CONSTANT_String:
					for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
					index = classfile->constantPool[index].slot1;
					j9tty_printf( PORTLIB, "(java.lang.String) %s\n", classfile->constantPool[index].bytes);
					break;
			}
			break;

		case CFR_ATTRIBUTE_Code:
			code = (J9CfrAttributeCode*)attrib;
			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf( PORTLIB, "Max stack: %i\n", code->maxStack);
			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf( PORTLIB, "Max Locals: %i\n", code->maxLocals);
			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf( PORTLIB, "Code Length: %i\n", code->codeLength);

			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf( PORTLIB, "Caught Exceptions (%i):\n", code->exceptionTableLength);
			for(j = 0; j < code->exceptionTableLength; j++)
			{
				for(i = 0; i < tabLevel + 2; i++) j9tty_printf( PORTLIB, "  ");
				index = code->exceptionTable[j].catchType;
				if(index == 0)
					j9tty_printf( PORTLIB, "from %i to %i goto %i when (any) \n", code->exceptionTable[j].startPC, code->exceptionTable[j].endPC, code->exceptionTable[j].handlerPC);
				else
				{
					index = classfile->constantPool[code->exceptionTable[j].catchType].slot1;
					j9tty_printf( PORTLIB, "from %i to %i goto %i when %i -> %s\n", code->exceptionTable[j].startPC, code->exceptionTable[j].endPC, code->exceptionTable[j].handlerPC, index, classfile->constantPool[index].bytes);
				}
			}

			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf( PORTLIB, "Attributes (%i):\n", code->attributesCount);
			for(j = 0; j < code->attributesCount; j++)
				dumpAttribute(classfile, code->attributes[j], tabLevel + 2);
			break;

		case CFR_ATTRIBUTE_Exceptions:
			exceptions = (J9CfrAttributeExceptions*)attrib;
			for(i = 0; i < exceptions->numberOfExceptions; i++)
			{
				for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
				index = classfile->constantPool[exceptions->exceptionIndexTable[i]].slot1;
				j9tty_printf( PORTLIB, "%i -> %s\n", exceptions->exceptionIndexTable[i], classfile->constantPool[index].bytes);
			}
			break;

		case CFR_ATTRIBUTE_InnerClasses:
			classes = (J9CfrAttributeInnerClasses*)attrib;
			for(i = 0; i < classes->numberOfClasses; i++)
			{
				for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
				index = classfile->constantPool[classes->classes[i].innerClassInfoIndex].slot1;
				j9tty_printf( PORTLIB, "Inner Class: %i -> %s\n", classes->classes[i].innerClassInfoIndex, classfile->constantPool[index].bytes);

				for(j = 0; j < tabLevel + 2; j++) j9tty_printf( PORTLIB, "  ");
				if(classes->classes[i].outerClassInfoIndex == 0)
				{
					j9tty_printf( PORTLIB, "Outer Class: (not a member)\n");
				}
				else
				{
					index = classfile->constantPool[classes->classes[i].outerClassInfoIndex].slot1;
					j9tty_printf( PORTLIB, "Outer Class: %i -> %s\n", classes->classes[i].outerClassInfoIndex, classfile->constantPool[index].bytes);
				}

				for(j = 0; j < tabLevel + 2; j++) j9tty_printf( PORTLIB, "  ");
				index = classes->classes[i].innerNameIndex;
				if(index == 0)
				{
					j9tty_printf( PORTLIB, "Inner Name: (anonymous)\n");
				}
				else
				{
					j9tty_printf( PORTLIB, "Inner Name: %i -> %s\n", index, classfile->constantPool[index].bytes);
				}

				for(j = 0; j < tabLevel + 2; j++) j9tty_printf( PORTLIB, "  ");
				j9tty_printf( PORTLIB, "Inner Class Access Flags: 0x%X ( ", classes->classes[i].innerClassAccessFlags);
				printModifiers(PORTLIB, classes->classes[i].innerClassAccessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_CLASS, J9_IS_CLASSFILE_VALUETYPE(classfile));
				j9tty_printf( PORTLIB, " )\n");
			}
			break;

#if JAVA_SPEC_VERSION >= 11
		case CFR_ATTRIBUTE_NestMembers: {
			nestMembers = (J9CfrAttributeNestMembers*)attrib;
			nestMemberCount = nestMembers->numberOfClasses;

			for (j = 0; j < nestMemberCount; j++) {
				for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
				index = nestMembers->classes[j];
				j9tty_printf( PORTLIB, "%i -> %s\n", index, classfile->constantPool[classfile->constantPool[index].slot1].bytes);
			}
			break;
		}

		case CFR_ATTRIBUTE_NestHost: {
			index = ((J9CfrAttributeNestHost*)attrib)->hostClassIndex;
			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf( PORTLIB, "%i -> %s\n", index, classfile->constantPool[classfile->constantPool[index].slot1].bytes);
			break;
		}
#endif /* JAVA_SPEC_VERSION >= 11 */

		case CFR_ATTRIBUTE_LineNumberTable:
			for(i = 0; i < ((J9CfrAttributeLineNumberTable*)attrib)->lineNumberTableLength; i++)
			{
				for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
				j9tty_printf( PORTLIB, "pc: %i line number: %i\n", ((J9CfrAttributeLineNumberTable*)attrib)->lineNumberTable[i].startPC, ((J9CfrAttributeLineNumberTable*)attrib)->lineNumberTable[i].lineNumber);
			}
			break;

		case CFR_ATTRIBUTE_LocalVariableTable:
			for(i = 0; i < ((J9CfrAttributeLocalVariableTable*)attrib)->localVariableTableLength; i++)
			{
				index = ((J9CfrAttributeLocalVariableTable*)attrib)->localVariableTable[i].nameIndex;
				index2 = ((J9CfrAttributeLocalVariableTable*)attrib)->localVariableTable[i].descriptorIndex;

				for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
				j9tty_printf( PORTLIB, "from %i to %i slot %i is %i -> %s type %i -> %s\n",
					((J9CfrAttributeLocalVariableTable*)attrib)->localVariableTable[i].startPC,
					((J9CfrAttributeLocalVariableTable*)attrib)->localVariableTable[i].startPC + ((J9CfrAttributeLocalVariableTable*)attrib)->localVariableTable[i].length,
					((J9CfrAttributeLocalVariableTable*)attrib)->localVariableTable[i].index,
					index,
					classfile->constantPool[index].bytes,
					index2,
					classfile->constantPool[index2].bytes,
					0);
			}
			break;

		case CFR_ATTRIBUTE_LocalVariableTypeTable:
			for(i = 0; i < ((J9CfrAttributeLocalVariableTypeTable*)attrib)->localVariableTypeTableLength; i++)
			{
				index = ((J9CfrAttributeLocalVariableTypeTable*)attrib)->localVariableTypeTable[i].nameIndex;
				index2 = ((J9CfrAttributeLocalVariableTypeTable*)attrib)->localVariableTypeTable[i].signatureIndex;

				for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
				j9tty_printf( PORTLIB, "from %i to %i slot %i is %i -> %s type %i -> %s\n",
					((J9CfrAttributeLocalVariableTypeTable*)attrib)->localVariableTypeTable[i].startPC,
					((J9CfrAttributeLocalVariableTypeTable*)attrib)->localVariableTypeTable[i].startPC
						 + ((J9CfrAttributeLocalVariableTypeTable*)attrib)->localVariableTypeTable[i].length,
					((J9CfrAttributeLocalVariableTypeTable*)attrib)->localVariableTypeTable[i].index,
					index,
					classfile->constantPool[index].bytes,
					index2,
					classfile->constantPool[index2].bytes,
					0);
			}
			break;

		case CFR_ATTRIBUTE_AnnotationDefault:
			dumpAnnotationElement(classfile, ((J9CfrAttributeAnnotationDefault *)attrib)->defaultValue, tabLevel + 1);
			break;

		case CFR_ATTRIBUTE_RuntimeVisibleAnnotations:
			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf( PORTLIB, "Annotations (%i):\n", ((J9CfrAttributeRuntimeVisibleAnnotations *)attrib)->numberOfAnnotations);

			dumpAnnotations(classfile, ((J9CfrAttributeRuntimeVisibleAnnotations *)attrib)->annotations,
					((J9CfrAttributeRuntimeVisibleAnnotations *)attrib)->numberOfAnnotations, tabLevel + 2);
			break;

		case CFR_ATTRIBUTE_RuntimeInvisibleAnnotations:
			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf( PORTLIB, "Annotations (%i):\n", ((J9CfrAttributeRuntimeInvisibleAnnotations *)attrib)->numberOfAnnotations);

			dumpAnnotations(classfile, ((J9CfrAttributeRuntimeInvisibleAnnotations *)attrib)->annotations,
					((J9CfrAttributeRuntimeInvisibleAnnotations *)attrib)->numberOfAnnotations, tabLevel + 1);
			break;

		case CFR_ATTRIBUTE_RuntimeVisibleTypeAnnotations: {
			J9CfrAttributeRuntimeVisibleTypeAnnotations *annotations = (J9CfrAttributeRuntimeVisibleTypeAnnotations *)attrib;
			if (0 == annotations->rawDataLength) { /* the attribute was well-formed */
				dumpTypeAnnotations(classfile, annotations->typeAnnotations,
						annotations->numberOfAnnotations, tabLevel + 1);
			} else {
				dumpRawData("Malformed RuntimeVisibleTypeAnnotations attribute", annotations->rawDataLength,annotations->rawAttributeData);
			}
		}

			break;

		case CFR_ATTRIBUTE_RuntimeInvisibleTypeAnnotations: {
			J9CfrAttributeRuntimeInvisibleTypeAnnotations *annotations = (J9CfrAttributeRuntimeInvisibleTypeAnnotations *)attrib;
			if (0 == annotations->rawDataLength) { /* the attribute was well-formed */
				dumpTypeAnnotations(classfile, annotations->typeAnnotations,
						annotations->numberOfAnnotations, tabLevel + 1);
			} else {
				dumpRawData("Malformed  RuntimeInvisibleTypeAnnotations attribute", annotations->rawDataLength,annotations->rawAttributeData);
			}
		}
			break;

		case CFR_ATTRIBUTE_MethodParameters: {
			const char *noName = "<no name>";

			for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");

			maxLength = 0;
			for (i = 0; i < ((J9CfrAttributeMethodParameters *)attrib)->numberOfMethodParameters; i++) {
				U_8 *bytes = (U_8*)noName;
				U_16 index = ((J9CfrAttributeMethodParameters *)attrib)->methodParametersIndexTable[i];
				U_32 bytesLen = 0;
				if (0 != index) {
					bytes = classfile->constantPool[((J9CfrAttributeMethodParameters *)attrib)->methodParametersIndexTable[i]].bytes;
				}
				bytesLen = (U_32)strlen((const char*)bytes);
				if (maxLength < bytesLen) {
					maxLength = bytesLen;
				}
			}
			j9tty_printf( PORTLIB, "Name");
			/* Flags start point is (maxLength + Name.length) from the start point of Name string
			 * 			Name<------------Space1-->Flags
			 * 			ParameterName<---Space2-->FlagValue
			 *
			 * 			Space1 == maxLength
			 * 			Name.length + Space1 == ParameterName.length + Space2
			 *
			 */
			for(j = 0; j < maxLength; j++) j9tty_printf( PORTLIB, " ");
			j9tty_printf( PORTLIB, "     Flags\n");
			for (i = 0; i < ((J9CfrAttributeMethodParameters *)attrib)->numberOfMethodParameters; i++) {
				U_8 *bytes = (U_8*)noName;
				U_16 index = ((J9CfrAttributeMethodParameters *)attrib)->methodParametersIndexTable[i];
				if (0 != index) {
					bytes = classfile->constantPool[((J9CfrAttributeMethodParameters *)attrib)->methodParametersIndexTable[i]].bytes;
				}
				for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
				j9tty_printf( PORTLIB, "%i -> %s", ((J9CfrAttributeMethodParameters *)attrib)->methodParametersIndexTable[i], bytes);
				if (0 != ((J9CfrAttributeMethodParameters *)attrib)->flags[i]) {
					for(j = 0; j < maxLength - ((U_32)strlen((const char*)bytes)) + ((U_32)strlen("Name")); j++) j9tty_printf( PORTLIB, " ");
					j9tty_printf( PORTLIB, "0x%x ( ", ((J9CfrAttributeMethodParameters *)attrib)->flags[i]);
					printModifiers(PORTLIB, ((J9CfrAttributeMethodParameters *)attrib)->flags[i], ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_METHODPARAMETER, FALSE);
					j9tty_printf( PORTLIB, " )\n");
				} else {
					j9tty_printf( PORTLIB, "\n");
				}
			}
			break;
		}

		case CFR_ATTRIBUTE_RuntimeVisibleParameterAnnotations: {
			J9CfrAttributeRuntimeVisibleParameterAnnotations *annotations = (J9CfrAttributeRuntimeVisibleParameterAnnotations *)attrib;
			if (0 == annotations->rawDataLength) { /* the attribute was well-formed */
				for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
				j9tty_printf( PORTLIB, "Parameter Count (%i):\n", annotations->numberOfParameters);

				for (i = 0; i < annotations->numberOfParameters; i++) {
					for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
					j9tty_printf( PORTLIB, "Annotations (%i):\n", annotations->parameterAnnotations->numberOfAnnotations);
					dumpAnnotations(classfile, annotations->parameterAnnotations->annotations,
							annotations->parameterAnnotations->numberOfAnnotations, tabLevel + 1);
				}
			} else {
				dumpRawData("Malformed RuntimeVisibleParameterAnnotations attribute", annotations->rawDataLength,annotations->rawAttributeData);
			}
		}

			break;

		case CFR_ATTRIBUTE_RuntimeInvisibleParameterAnnotations: {
			J9CfrAttributeRuntimeInvisibleParameterAnnotations *annotations = (J9CfrAttributeRuntimeInvisibleParameterAnnotations *)attrib;
			if (0 == annotations->rawDataLength) { /* the attribute was well-formed */
				for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
				j9tty_printf( PORTLIB, "Parameter Count (%i):\n", annotations->numberOfParameters);

				for (i = 0; i < annotations->numberOfParameters; i++) {
					for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
					j9tty_printf( PORTLIB, "Annotations (%i):\n", annotations->parameterAnnotations->numberOfAnnotations);
					dumpAnnotations(classfile, annotations->parameterAnnotations->annotations,
							annotations->parameterAnnotations->numberOfAnnotations, tabLevel + 1);
				}
			} else {
				dumpRawData("Malformed RuntimeInvisibleParameterAnnotations attribute", annotations->rawDataLength,annotations->rawAttributeData);
			}
		}

			break;

		case CFR_ATTRIBUTE_EnclosingMethod:
			index = ((J9CfrAttributeEnclosingMethod *)attrib)->classIndex;
			index2 = classfile->constantPool[index].slot1;
			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf( PORTLIB, "Class: %i -> %s\n", index, classfile->constantPool[index2].bytes);

			index = ((J9CfrAttributeEnclosingMethod *)attrib)->methodIndex;
			index2 = classfile->constantPool[index].slot1;
			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");

			/* If EnclosingMethod is a static initializer or clinit it will not have a method index */
			if (index2 != 0) {
				j9tty_printf( PORTLIB, "Method: %i -> %s\n", index, classfile->constantPool[index2].bytes);
			} else {
				j9tty_printf( PORTLIB, "Method: (none)\n");
			}
			break;

		case CFR_ATTRIBUTE_StackMap:
		case CFR_ATTRIBUTE_StackMapTable:
			for(i = 0; i < tabLevel + 1; i++) j9tty_printf( PORTLIB, "  ");
			index = ((J9CfrAttributeStackMap *)attrib)->numberOfEntries;
			j9tty_printf( PORTLIB, "Stackmaps (%i):\n", index);
			dumpStackMap((J9CfrAttributeStackMap *)attrib, classfile, tabLevel + 2);
			break;

		case CFR_ATTRIBUTE_Record:
			for(i = 0; i < ((J9CfrAttributeRecord*)attrib)->numberOfRecordComponents; i++) {
				J9CfrRecordComponent* recordComponent = &(((J9CfrAttributeRecord*)attrib)->recordComponents[i]);

				for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
				j9tty_printf( PORTLIB, "Record Component Name: %i -> %s\n", recordComponent->nameIndex, classfile->constantPool[recordComponent->nameIndex].bytes);
				for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
				j9tty_printf( PORTLIB, "Record Component Signature: %i -> %s\n", recordComponent->descriptorIndex, classfile->constantPool[recordComponent->descriptorIndex].bytes);

				for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
				j9tty_printf( PORTLIB, "Attributes (%i):\n", recordComponent->attributesCount);
				for(j = 0; j < recordComponent->attributesCount; j++)
					dumpAttribute(classfile, recordComponent->attributes[j], tabLevel + 2);
			}
			break;

		case CFR_ATTRIBUTE_PermittedSubclasses:
			for(i = 0; i < ((J9CfrAttributePermittedSubclasses*)attrib)->numberOfClasses; i++) {
				U_16 classIndex = ((J9CfrAttributePermittedSubclasses*)attrib)->classes[i];
				U_16 nameIndex = classfile->constantPool[classIndex].slot1;

				for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
				j9tty_printf( PORTLIB, "PermittedSubclass class index, name: %i, %i -> %s\n", classIndex, nameIndex, classfile->constantPool[nameIndex].bytes);
			}
			break;

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
		case CFR_ATTRIBUTE_Preload:
			for(i = 0; i < ((J9CfrAttributePreload*)attrib)->numberOfClasses; i++) {
				U_16 descriptorIndex = ((J9CfrAttributePreload*)attrib)->classes[i];
				for(j = 0; j < tabLevel + 1; j++) {
					j9tty_printf(PORTLIB, "  ");
				}
				j9tty_printf(PORTLIB, "Preload class index, name: %i -> %s\n", descriptorIndex, classfile->constantPool[descriptorIndex].bytes);
			}
			break;
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
		case CFR_ATTRIBUTE_ImplicitCreation:
			for (i = 0; i < tabLevel + 1; i++) {
				j9tty_printf( PORTLIB, "  ");
			}
			j9tty_printf(PORTLIB, "ImplicitCreation flags: 0x%X\n",
				((J9CfrAttributeImplicitCreation*)attrib)->implicitCreationFlags);
			break;
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */
		case CFR_ATTRIBUTE_StrippedLineNumberTable:
		case CFR_ATTRIBUTE_StrippedLocalVariableTable:
		case CFR_ATTRIBUTE_StrippedLocalVariableTypeTable:
		case CFR_ATTRIBUTE_StrippedInnerClasses:
		case CFR_ATTRIBUTE_StrippedUnknown:
			for(j = 0; j < tabLevel + 1; j++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf( PORTLIB, "Attribute data stripped\n");
			break;

		case CFR_ATTRIBUTE_Unknown:
		case CFR_ATTRIBUTE_Synthetic:
		case CFR_ATTRIBUTE_Deprecated:
		default:
			break;
	}
	return;
}


static void printClassFile(J9CfrClassFile* classfile)
{
	U_16 index;
	U_8* string;
	I_32 i, j, k;

	PORT_ACCESS_FROM_PORT(portLib);

	if(classfile->accessFlags & CFR_ACC_PUBLIC) j9tty_printf( PORTLIB, "public ");
	if(classfile->accessFlags & CFR_ACC_PRIVATE) j9tty_printf( PORTLIB, "protected ");
	if(classfile->accessFlags & CFR_ACC_PROTECTED) j9tty_printf( PORTLIB, "private ");
	/* JEP 360: note: non-sealed will not be indicated for subclasses. There's no way of knowing until classes are linked. */
	if(classfile->j9Flags & CFR_J9FLAG_IS_SEALED) j9tty_printf( PORTLIB, "sealed ");
#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
	if (J9_IS_CLASSFILE_VALUETYPE(classfile)) j9tty_printf( PORTLIB, "value ");
#endif /* defined(J9VM_OPT_VALHALLA_VALUE_TYPES) */
	if(classfile->accessFlags & CFR_ACC_INTERFACE)
		j9tty_printf( PORTLIB, "interface ");
	else if (classfile->j9Flags & CFR_J9FLAG_IS_RECORD)
	{
		j9tty_printf( PORTLIB, "record ");
	}
	else
	{
		if(classfile->accessFlags & CFR_ACC_ABSTRACT) j9tty_printf( PORTLIB, "abstract ");
		j9tty_printf( PORTLIB, "class ");
	}

	index = classfile->constantPool[classfile->thisClass].slot1;
	string = classfile->constantPool[index].bytes;
	i = 0;
	while(string[i])
	{
		j9tty_printf( PORTLIB, "%c", (string[i] == '/')?'.':string[i]);
		i++;
	}

	if(classfile->superClass != 0)
	{
		index = classfile->constantPool[classfile->superClass].slot1;
		string = classfile->constantPool[index].bytes;
		j9tty_printf( PORTLIB, " extends ");
		i = 0;
		while(string[i])
		{
			j9tty_printf( PORTLIB, "%c", (string[i] == '/')?'.':string[i]);
			i++;
		}
	}

	if(classfile->interfacesCount > 0)
	{
		j9tty_printf( PORTLIB, " implements ");
		for(i = 0; i < classfile->interfacesCount - 1; i++)
		{
			index = classfile->constantPool[classfile->interfaces[i]].slot1;
			string = classfile->constantPool[index].bytes;
			j = 0;
			while(string[j])
			{
				j9tty_printf( PORTLIB, "%c", (string[j] == '/')?'.':string[j]);
				j++;
			}
			j9tty_printf( PORTLIB, ", ");
		}
		index = classfile->constantPool[classfile->interfaces[i]].slot1;
		string = classfile->constantPool[index].bytes;
		j = 0;
		while(string[j])
		{
			j9tty_printf( PORTLIB, "%c", (string[j] == '/')?'.':string[j]);
			j++;
		}
	}

	/* JEP 360: sealed class permits list */
	if(classfile->j9Flags & CFR_J9FLAG_IS_SEALED) {
		/* find PermittedSubclasses attribute */
		for (i = 0; i < classfile->attributesCount; i++) {
			if (CFR_ATTRIBUTE_PermittedSubclasses == classfile->attributes[i]->tag) {
				/* found attribute, print permitted subclasses */
				j9tty_printf( PORTLIB, " permits ");

				for (j = 0; j < ((J9CfrAttributePermittedSubclasses*)classfile->attributes[i])->numberOfClasses; j++) {
					/* class index */
					index = ((J9CfrAttributePermittedSubclasses*)classfile->attributes[i])->classes[j];
					/* class name index */
					index = classfile->constantPool[index].slot1;
					string = classfile->constantPool[index].bytes;

					k = 0;
					while('\0' != string[k]) {
						j9tty_printf( PORTLIB, "%c", (string[k] == '/') ? '.' : string[k]);
						k++;
					}
					if ((j + 1) != ((J9CfrAttributePermittedSubclasses*)classfile->attributes[i])->numberOfClasses) j9tty_printf( PORTLIB, ", ");
				}
				break;
			}
		}
	}


	j9tty_printf( PORTLIB, "\n{\n");

	for(i = 0; i < classfile->fieldsCount; i++)
		printField(classfile, &(classfile->fields[i]));

	if((classfile->fieldsCount > 0)&&(classfile->methodsCount > 0))
		j9tty_printf( PORTLIB, "\n");

	for(i = 0; i < classfile->methodsCount; i++)
		printMethod(classfile, &(classfile->methods[i]));

	j9tty_printf( PORTLIB, "}\n");
	return;
}


static void printMethod(J9CfrClassFile* classfile, J9CfrMethod* method)
{
	U_8* string;
	J9CfrAttributeExceptions* exceptions;
	U_16 index;
	I_32 arity, i, j;

	PORT_ACCESS_FROM_PORT(portLib);

	j9tty_printf( PORTLIB, "    ");

	/* Access flags. */
	if(method->accessFlags & CFR_ACC_PUBLIC) j9tty_printf( PORTLIB, "public ");
	if(method->accessFlags & CFR_ACC_PRIVATE) j9tty_printf( PORTLIB, "private ");
	if(method->accessFlags & CFR_ACC_PROTECTED) j9tty_printf( PORTLIB, "protected ");
	if(method->accessFlags & CFR_ACC_STATIC) j9tty_printf( PORTLIB, "static ");
	if(method->accessFlags & CFR_ACC_FINAL) j9tty_printf( PORTLIB, "final ");
	if(method->accessFlags & CFR_ACC_SYNCHRONIZED) j9tty_printf( PORTLIB, "synchronized ");
	if(method->accessFlags & CFR_ACC_NATIVE) j9tty_printf( PORTLIB, "native ");
	if(method->accessFlags & CFR_ACC_ABSTRACT) j9tty_printf( PORTLIB, "abstract ");
	if(method->accessFlags & CFR_ACC_STRICT) j9tty_printf( PORTLIB, "strict ");
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	/* ImplicitCreation is triggered by an implicit constructor in a value class */
	if (J9_IS_CLASSFILE_VALUETYPE(classfile)) {
		J9CfrConstantPoolInfo name = classfile->constantPool[method->nameIndex];
		if (J9UTF8_LITERAL_EQUALS(name.bytes, name.slot1, "<init>")) {
			for (i = 0; i < classfile->attributesCount; i++) {
				J9CfrAttribute* attr = classfile->attributes[i];
				if (attr->tag == CFR_ATTRIBUTE_ImplicitCreation) {
					j9tty_printf( PORTLIB, "implicit ");
				}
			}
		}
	}
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

	/* Return type. */
	string = classfile->constantPool[method->descriptorIndex].bytes;
	i = 0;
	while(string[i++] != ')');
	arity = 0;
	while(string[i] == '[') arity++, i++;
	switch(string[i])
	{
		case 'B':
			j9tty_printf( PORTLIB, "byte");
			break;

		case 'C':
			j9tty_printf( PORTLIB, "char");
			break;

		case 'D':
			j9tty_printf( PORTLIB, "double");
			break;

		case 'F':
			j9tty_printf( PORTLIB, "float");
			break;

		case 'I':
			j9tty_printf( PORTLIB, "int");
			break;

		case 'J':
			j9tty_printf( PORTLIB, "long");
			break;

		case 'L':
			i++;
			while(string[i] != ';')
			{
				j9tty_printf( PORTLIB, "%c", (string[i] == '/')?'.':string[i]);
				i++;
			}
			break;

		case 'S':
			j9tty_printf( PORTLIB, "short");
			break;

		case 'V':
			j9tty_printf( PORTLIB, "void");
			break;

		case 'Z':
			j9tty_printf( PORTLIB, "boolean");
			break;
	}
	for(i = 0; i < arity; i++)
		j9tty_printf( PORTLIB, "[]");

	j9tty_printf( PORTLIB, " %s(", classfile->constantPool[method->nameIndex].bytes);

	for(i = 1; string[i] != ')'; i++)
	{
		arity = 0;
		while(string[i] == '[') arity++, i++;
		switch(string[i])
		{
			case 'B':
				j9tty_printf( PORTLIB, "byte");
				break;

			case 'C':
				j9tty_printf( PORTLIB, "char");
				break;

			case 'D':
				j9tty_printf( PORTLIB, "double");
				break;

			case 'F':
				j9tty_printf( PORTLIB, "float");
				break;

			case 'I':
				j9tty_printf( PORTLIB, "int");
				break;

			case 'J':
				j9tty_printf( PORTLIB, "long");
				break;

			case 'L':
				i++;
				while(string[i] != ';')
				{
					j9tty_printf( PORTLIB, "%c", (string[i] == '/')?'.':string[i]);
					i++;
				}
				break;

			case 'S':
				j9tty_printf( PORTLIB, "short");
				break;

			case 'V':
				j9tty_printf( PORTLIB, "void");
				break;

			case 'Z':
				j9tty_printf( PORTLIB, "boolean");
				break;
		}
		for(j = 0; j < arity; j++)
			j9tty_printf( PORTLIB, "[]");

		if(string[i + 1] != ')')
			j9tty_printf( PORTLIB, ", ");
	}

	j9tty_printf( PORTLIB, ")");

	for(i = 0; i < method->attributesCount; i++)
	{
		if(method->attributes[i]->tag == CFR_ATTRIBUTE_Exceptions)
		{
			exceptions = (J9CfrAttributeExceptions*)method->attributes[i];

			if (exceptions && exceptions->numberOfExceptions)
			{
				j9tty_printf( PORTLIB, " throws ");
				for(j = 0; j < exceptions->numberOfExceptions;)
				{
					if(exceptions->exceptionIndexTable[j] != 0)
					{
						index = classfile->constantPool[exceptions->exceptionIndexTable[j]].slot1;
						string = classfile->constantPool[index].bytes;
						while(*string)
						{
							j9tty_printf( PORTLIB, "%c", (*string == '/')?'.':*string);
							string++;
						}
						if (++j != exceptions->numberOfExceptions)
						{
							j9tty_printf( PORTLIB, ", ");
						}
					}
				}
			}

			i = method->attributesCount;
		}
	}

	j9tty_printf( PORTLIB, ";\n");
	return;
}


static void printField(J9CfrClassFile* classfile, J9CfrField* field)
{
	U_8* string;
	U_32 arity, i;

	PORT_ACCESS_FROM_PORT(portLib);

	j9tty_printf( PORTLIB, "    ");

	/* Access flags. */
	if(field->accessFlags & CFR_ACC_PUBLIC) j9tty_printf( PORTLIB, "public ");
	if(field->accessFlags & CFR_ACC_PRIVATE) j9tty_printf( PORTLIB, "private ");
	if(field->accessFlags & CFR_ACC_PROTECTED) j9tty_printf( PORTLIB, "protected ");
	if(field->accessFlags & CFR_ACC_STATIC) j9tty_printf( PORTLIB, "static ");
	if(field->accessFlags & CFR_ACC_FINAL) j9tty_printf( PORTLIB, "final ");
	if(field->accessFlags & CFR_ACC_VOLATILE) j9tty_printf( PORTLIB, "volatile ");
	if(field->accessFlags & CFR_ACC_TRANSIENT) j9tty_printf( PORTLIB, "transient ");
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	if(field->accessFlags & CFR_ACC_STRICT) j9tty_printf( PORTLIB, "strict ");
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

	/* Return type. */
	string = classfile->constantPool[field->descriptorIndex].bytes;
	i = 0;
	arity = 0;
	while(string[i] == '[') arity++, i++;
	switch(string[i])
	{
		case 'B':
			j9tty_printf( PORTLIB, "byte");
			break;

		case 'C':
			j9tty_printf( PORTLIB, "char");
			break;

		case 'D':
			j9tty_printf( PORTLIB, "double");
			break;

		case 'F':
			j9tty_printf( PORTLIB, "float");
			break;

		case 'I':
			j9tty_printf( PORTLIB, "int");
			break;

		case 'J':
			j9tty_printf( PORTLIB, "long");
			break;
		case 'L':
			i++;
			while(string[i] != ';')
			{
				j9tty_printf( PORTLIB, "%c", (string[i] == '/')?'.':string[i]);
				i++;
			}
			break;

		case 'S':
			j9tty_printf( PORTLIB, "short");
			break;

		case 'V':
			j9tty_printf( PORTLIB, "void");
			break;

		case 'Z':
			j9tty_printf( PORTLIB, "boolean");
			break;
	}
	for(i = 0; i < arity; i++)
		j9tty_printf( PORTLIB, "[]");

#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
	for(i = 0; i < field->attributesCount; i++) {
		if (CFR_ATTRIBUTE_NullRestricted == field->attributes[i]->tag) {
			j9tty_printf(PORTLIB, "!");
		}
	}
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

	j9tty_printf( PORTLIB, " %s;\n", classfile->constantPool[field->nameIndex].bytes);

	return;
}


static void printDisassembledMethod(J9CfrClassFile* classfile, J9CfrMethod* method, BOOLEAN bigEndian, U_8* bytecodes, U_32 bytecodesLength)
{
	U_8* string;
	J9CfrAttributeExceptions* exceptions;
	J9CfrAttributeCode* code;
	J9CfrConstantPoolInfo info;
	I_32 arity, i, j;
	U_8* bcIndex;
	I_32 pc, index, target, start;
	I_32 low, high;
	U_32 length, npairs;
	U_8 bc;
	BOOLEAN fieldFlag;
	BOOLEAN wide;

	PORT_ACCESS_FROM_PORT(portLib);

	if(!(code = method->codeAttribute))
	{
		return;
	}

	/* Access flags. */
	if(method->accessFlags & CFR_ACC_PUBLIC) j9tty_printf( PORTLIB, "public ");
	if(method->accessFlags & CFR_ACC_PRIVATE) j9tty_printf( PORTLIB, "private ");
	if(method->accessFlags & CFR_ACC_PROTECTED) j9tty_printf( PORTLIB, "protected ");
	if(method->accessFlags & CFR_ACC_STATIC) j9tty_printf( PORTLIB, "static ");
	if(method->accessFlags & CFR_ACC_FINAL) j9tty_printf( PORTLIB, "final ");
	if(method->accessFlags & CFR_ACC_SYNCHRONIZED) j9tty_printf( PORTLIB, "synchronized ");
	if(method->accessFlags & CFR_ACC_NATIVE) j9tty_printf( PORTLIB, "native ");
	if(method->accessFlags & CFR_ACC_ABSTRACT) j9tty_printf( PORTLIB, "abstract ");

	/* Return type. */
	string = classfile->constantPool[method->descriptorIndex].bytes;
	i = 0;
	while(string[i++] != ')');
	arity = 0;
	while(string[i] == '[') arity++, i++;
	switch(string[i])
	{
		case 'B':
			j9tty_printf( PORTLIB, "byte");
			break;

		case 'C':
			j9tty_printf( PORTLIB, "char");
			break;

		case 'D':
			j9tty_printf( PORTLIB, "double");
			break;

		case 'F':
			j9tty_printf( PORTLIB, "float");
			break;

		case 'I':
			j9tty_printf( PORTLIB, "int");
			break;

		case 'J':
			j9tty_printf( PORTLIB, "long");
			break;

		case 'L':
			i++;
			while(string[i] != ';')
			{
				j9tty_printf( PORTLIB, "%c", (string[i] == '/')?'.':string[i]);
				i++;
			}
			break;

		case 'S':
			j9tty_printf( PORTLIB, "short");
			break;

		case 'V':
			j9tty_printf( PORTLIB, "void");
			break;

		case 'Z':
			j9tty_printf( PORTLIB, "boolean");
			break;
	}
	for(i = 0; i < arity; i++)
		j9tty_printf( PORTLIB, "[]");

	j9tty_printf( PORTLIB, " %s(", classfile->constantPool[method->nameIndex].bytes);

	for(i = 1; string[i] != ')'; i++)
	{
		arity = 0;
		while(string[i] == '[') arity++, i++;
		switch(string[i])
		{
			case 'B':
				j9tty_printf( PORTLIB, "byte");
				break;

			case 'C':
				j9tty_printf( PORTLIB, "char");
				break;

			case 'D':
				j9tty_printf( PORTLIB, "double");
				break;

			case 'F':
				j9tty_printf( PORTLIB, "float");
				break;

			case 'I':
				j9tty_printf( PORTLIB, "int");
				break;

			case 'J':
				j9tty_printf( PORTLIB, "long");
				break;

			case 'L':
				i++;
				while(string[i] != ';')
				{
					j9tty_printf( PORTLIB, "%c", (string[i] == '/')?'.':string[i]);
					i++;
				}
				break;

			case 'S':
				j9tty_printf( PORTLIB, "short");
				break;

			case 'V':
				j9tty_printf( PORTLIB, "void");
				break;

			case 'Z':
				j9tty_printf( PORTLIB, "boolean");
				break;
		}
		for(j = 0; j < arity; j++)
			j9tty_printf( PORTLIB, "[]");

		if(string[i + 1] != ')')
			j9tty_printf( PORTLIB, ", ");
	}

	j9tty_printf( PORTLIB, ")");

	exceptions = method->exceptionsAttribute;
	if(exceptions && exceptions->numberOfExceptions)
	{
		j9tty_printf( PORTLIB, " throws ");
		for(j = 0; j < exceptions->numberOfExceptions;)
		{
			if(exceptions->exceptionIndexTable[j] != 0)
			{
				index = classfile->constantPool[exceptions->exceptionIndexTable[j]].slot1;
				string = classfile->constantPool[index].bytes;
				while(*string)
				{
					j9tty_printf( PORTLIB, "%c", (*string == '/')?'.':*string);
					string++;
				}
				if (++j != exceptions->numberOfExceptions)
				{
					j9tty_printf( PORTLIB, ", ");
				}
			}
		}
	}

	j9tty_printf( PORTLIB, "\n");

	if(bytecodes)
	{
		bcIndex = bytecodes;
		length = bytecodesLength;
	}
	else
	{
		bcIndex = code->code;
		length = code->codeLength;
	}
	pc = 0;
	wide = FALSE;
	fieldFlag = FALSE;
	while((U_32)pc < length)
	{
			NEXT_U8_ENDIAN(bigEndian, bc, bcIndex);
			if(wide)
			{
				j9tty_printf( PORTLIB, "%s", sunJavaBCNames[bc]);
			}
			else
			{
				j9tty_printf( PORTLIB, "%5i %s", pc, sunJavaBCNames[bc]);
			}

			if(0 == strcmp(sunJavaBCNames[bc], "JBunimplemented"))
			{
				j9tty_printf( PORTLIB, "_%d ", bc);
			}
			else
			{
				j9tty_printf( PORTLIB, " ", bc);
			}

			start = pc;
			pc++;
			switch(bc)
			{
				case CFR_BC_bipush:
					NEXT_U8_ENDIAN(bigEndian, index, bcIndex);
					j9tty_printf( PORTLIB, "%i\n", (I_8)index);
					pc++;
					break;

				case CFR_BC_sipush:
					NEXT_U16_ENDIAN(bigEndian, index, bcIndex);
					j9tty_printf( PORTLIB, "%i\n", (I_16)index);
					pc += 2;
					break;

				case CFR_BC_ldc:
				case CFR_BC_ldc_w:
				case CFR_BC_ldc2_w:
					if (bc == CFR_BC_ldc) {
						NEXT_U8_ENDIAN(bigEndian, index, bcIndex);
					} else {
						NEXT_U16_ENDIAN(bigEndian, index, bcIndex);
					}
					j9tty_printf( PORTLIB, "%i ", index);
					info = classfile->constantPool[index];
					switch(info.tag)
					{
						case CFR_CONSTANT_Integer:
							j9tty_printf( PORTLIB, "(int) 0x%08X\n", info.slot1);
							break;
						case CFR_CONSTANT_Float:
							j9tty_printf( PORTLIB, "(float) 0x%08X\n", info.slot1);
							break;
						case CFR_CONSTANT_Long:
#ifdef J9VM_ENV_LITTLE_ENDIAN
							j9tty_printf( PORTLIB, "(long) 0x%08X%08X\n", info.slot2, info.slot1);
#else
							j9tty_printf( PORTLIB, "(long) 0x%08X%08X\n", info.slot1, info.slot2);
#endif
							break;
						case CFR_CONSTANT_Double:
#ifdef J9VM_ENV_LITTLE_ENDIAN
							j9tty_printf( PORTLIB, "(double) 0x%08X%08X\n", info.slot2, info.slot1);
#else
							j9tty_printf( PORTLIB, "(double) 0x%08X%08X\n", info.slot1, info.slot2);
#endif
							j9tty_printf( PORTLIB, "(double) 0x%08X%08X\n", info.slot1, info.slot2);
							break;
						case CFR_CONSTANT_String:
							j9tty_printf( PORTLIB, "(java.lang.String) \"%s\"\n", classfile->constantPool[info.slot1].bytes);
							break;
						case CFR_CONSTANT_Class:
							j9tty_printf( PORTLIB, "(java.lang.Class) ");
							info = classfile->constantPool[info.slot1];
							for(i = 0; i < (I_32)info.slot1; i++)
							{
								j9tty_printf( PORTLIB, "%c", info.bytes[i] == '/' ? '.' : info.bytes[i]);
							}
							j9tty_printf( PORTLIB, "\n");
							break;
						case CFR_CONSTANT_MethodType:
							j9tty_printf( PORTLIB, "(java.dyn.MethodType) \"%s\"\n", classfile->constantPool[info.slot1].bytes);
							break;
						case CFR_CONSTANT_MethodHandle:
							/* TODO - print pretty: kind + field/method ref */
							j9tty_printf( PORTLIB, "(java.dyn.MethodHandle) kind=%x methodOrFieldRef=%x\n", info.slot1, info.slot2);
							break;
						default:
							j9tty_printf( PORTLIB, "<unknown constant type %i>\n", info.tag);
							break;
					}
					pc++;
					if (bc != CFR_BC_ldc) {
						pc++;
					}
					break;

				case CFR_BC_iload:
				case CFR_BC_lload:
				case CFR_BC_fload:
				case CFR_BC_dload:
				case CFR_BC_aload:
				case CFR_BC_istore:
				case CFR_BC_lstore:
				case CFR_BC_fstore:
				case CFR_BC_dstore:
				case CFR_BC_astore:
				case CFR_BC_ret:
					if(wide)
					{
						NEXT_U16_ENDIAN(bigEndian, index, bcIndex);
						pc += 2;
						wide = FALSE;
					}
					else
					{
						NEXT_U8_ENDIAN(bigEndian, index, bcIndex);
						pc++;
					}
					j9tty_printf( PORTLIB, "%i\n", index);
 					break;

				case CFR_BC_iinc:
					if(wide)
					{
						NEXT_U16_ENDIAN(bigEndian, index, bcIndex);
						j9tty_printf( PORTLIB, "%i ", index);
						NEXT_U16_ENDIAN(bigEndian, index, bcIndex);
						j9tty_printf( PORTLIB, "%i\n", (I_16)index);
						pc += 4;
						wide = FALSE;
					}
					else
					{
						NEXT_U8_ENDIAN(bigEndian, index, bcIndex);
						j9tty_printf( PORTLIB, "%i ", index);
						NEXT_U8_ENDIAN(bigEndian, target, bcIndex);
						j9tty_printf( PORTLIB, "%i\n", (I_8)target);
						pc += 2;
					}
					break;

				case CFR_BC_ifeq:
				case CFR_BC_ifne:
				case CFR_BC_iflt:
				case CFR_BC_ifge:
				case CFR_BC_ifgt:
				case CFR_BC_ifle:
				case CFR_BC_if_icmpeq:
				case CFR_BC_if_icmpne:
				case CFR_BC_if_icmplt:
				case CFR_BC_if_icmpge:
				case CFR_BC_if_icmpgt:
				case CFR_BC_if_icmple:
				case CFR_BC_if_acmpeq:
				case CFR_BC_if_acmpne:
				case CFR_BC_goto:
				case CFR_BC_jsr:
				case CFR_BC_ifnull:
				case CFR_BC_ifnonnull:
					NEXT_U16_ENDIAN(bigEndian, index, bcIndex);
					target = start + (I_16)index;
					j9tty_printf( PORTLIB, "%i\n", target);
					pc += 2;
					break;

				case CFR_BC_tableswitch:
					switch(start % 4)
					{
						case 0:
							bcIndex++;
							pc++;
						case 1:
							bcIndex++;
							pc++;
						case 2:
							bcIndex++;
							pc++;
						case 3:
							break;
					}
					NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
					target = start + index;
					NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
					low = (I_32)index;
					NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
					high = (I_32)index;
					pc += 12;
					j9tty_printf( PORTLIB, "low %i high %i\n", low, high);
					j9tty_printf( PORTLIB, "     default %10i\n", target);
					j = high - low + 1;
					for(i = 0; i < j; i++)
					{
						NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
						target = start + index;
						j9tty_printf( PORTLIB, "  %10i %10i\n", low + i, target);
						pc += 4;
					}
					break;

				case CFR_BC_lookupswitch:
					switch(start % 4)
					{
						case 0:
							bcIndex++;
							pc++;
						case 1:
							bcIndex++;
							pc++;
						case 2:
							bcIndex++;
							pc++;
						case 3:
							break;
					}
					NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
					target = start + index;
					NEXT_U32_ENDIAN(bigEndian, npairs, bcIndex);
					j9tty_printf( PORTLIB, "pairs %i\n", npairs);
					j9tty_printf( PORTLIB, "     default %10i\n", target);
					pc += 8;
					for(i = 0; i < (I_32)npairs; i++)
					{
						NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
						j9tty_printf( PORTLIB, "  %10i", index);
						NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
						target = start + index;
						j9tty_printf( PORTLIB, " %10i\n", target);
						pc += 8;
					}
					break;

				case CFR_BC_getstatic:
				case CFR_BC_putstatic:
				case CFR_BC_getfield:
				case CFR_BC_putfield:
					fieldFlag = TRUE;

				case CFR_BC_invokevirtual:
				case CFR_BC_invokespecial:
				case CFR_BC_invokestatic:
					NEXT_U16_ENDIAN(bigEndian, index, bcIndex);
					info = classfile->constantPool[index];
					j9tty_printf( PORTLIB, "%i ", index);
					info = classfile->constantPool[classfile->constantPool[info.slot1].slot1];
					for(i = 0; i < (I_32)info.slot1; i++)
					{
						j9tty_printf( PORTLIB, "%c", info.bytes[i] == '/' ? '.' : info.bytes[i]);
					}
					j9tty_printf( PORTLIB, ".");
					index = classfile->constantPool[index].slot2;
					info = classfile->constantPool[classfile->constantPool[index].slot1];
					for(i = 0; i < (I_32)info.slot1; i++)
					{
						j9tty_printf( PORTLIB, "%c", info.bytes[i] == '/' ? '.' : info.bytes[i]);
					}
					if(fieldFlag) j9tty_printf( PORTLIB, " ");
					info = classfile->constantPool[classfile->constantPool[index].slot2];
					for(i = 0; i < (I_32)info.slot1; i++)
					{
						j9tty_printf( PORTLIB, "%c", info.bytes[i] == '/' ? '.' : info.bytes[i]);
					}
					j9tty_printf( PORTLIB, "\n");
					pc += 2;
					fieldFlag = FALSE;
					break;

				case CFR_BC_invokeinterface:
					NEXT_U16_ENDIAN(bigEndian, index, bcIndex);
					info = classfile->constantPool[index];
					j9tty_printf( PORTLIB, "%i ", index);
					info = classfile->constantPool[classfile->constantPool[info.slot1].slot1];
					for(i = 0; i < (I_32)info.slot1; i++)
					{
						j9tty_printf( PORTLIB, "%c", info.bytes[i] == '/' ? '.' : info.bytes[i]);
					}
					j9tty_printf( PORTLIB, ".");
					index = classfile->constantPool[index].slot2;
					info = classfile->constantPool[classfile->constantPool[index].slot1];
					for(i = 0; i < (I_32)info.slot1; i++)
					{
						j9tty_printf( PORTLIB, "%c", info.bytes[i] == '/' ? '.' : info.bytes[i]);
					}
					info = classfile->constantPool[classfile->constantPool[index].slot2];
					for(i = 0; i < (I_32)info.slot1; i++)
					{
						j9tty_printf( PORTLIB, "%c", info.bytes[i] == '/' ? '.' : info.bytes[i]);
					}
					j9tty_printf( PORTLIB, "\n");
					bcIndex += 2;
					pc += 4;
					break;

				case CFR_BC_new:
				case CFR_BC_anewarray:
				case CFR_BC_checkcast:
				case CFR_BC_instanceof:
					NEXT_U16_ENDIAN(bigEndian, index, bcIndex);
					info = classfile->constantPool[index];
					j9tty_printf( PORTLIB, "%i ", index);
					info = classfile->constantPool[info.slot1];
					for(i = 0; i < (I_32)info.slot1; i++)
					{
						j9tty_printf( PORTLIB, "%c", info.bytes[i] == '/' ? '.' : info.bytes[i]);
					}
					j9tty_printf( PORTLIB, "\n");
					pc += 2;
					break;

				case CFR_BC_newarray:
					NEXT_U8_ENDIAN(bigEndian, index, bcIndex);
					switch(index)
					{
						case /*T_BOOLEAN*/ 4 :
							j9tty_printf( PORTLIB, "boolean\n");
							break;
						case /*T_CHAR*/  5:
							j9tty_printf( PORTLIB, "char\n");
							break;
						case /*T_FLOAT*/  6:
							j9tty_printf( PORTLIB, "float\n");
							break;
						case /*T_DOUBLE*/  7:
							j9tty_printf( PORTLIB, "double\n");
							break;
						case /*T_BYTE*/  8:
							j9tty_printf( PORTLIB, "byte\n");
							break;
						case /*T_SHORT*/  9:
							j9tty_printf( PORTLIB, "short\n");
							break;
						case /*T_INT*/  10:
							j9tty_printf( PORTLIB, "int\n");
							break;
						case /*T_LONG*/  11:
							j9tty_printf( PORTLIB, "long\n");
							break;
						default:
							j9tty_printf( PORTLIB, "(unknown type %i)\n", index);
							break;
					}
					pc++;
					break;

				case CFR_BC_wide:
					wide = TRUE;
					break;

				case CFR_BC_multianewarray:
					NEXT_U16_ENDIAN(bigEndian, index, bcIndex);
					info = classfile->constantPool[index];
					j9tty_printf( PORTLIB, "%i ", index);
					NEXT_U8_ENDIAN(bigEndian, index, bcIndex);
					j9tty_printf( PORTLIB, "dims %i ", index);
					info = classfile->constantPool[info.slot1];
					for(i = 0; i < (I_32)info.slot1; i++)
					{
						j9tty_printf( PORTLIB, "%c", info.bytes[i] == '/' ? '.' : info.bytes[i]);
					}
					j9tty_printf( PORTLIB, "\n");
					pc += 3;
					break;

				case CFR_BC_goto_w:
				case CFR_BC_jsr_w:
					NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
					target = start + index;
					j9tty_printf( PORTLIB, "%i\n", target);
					pc += 4;
					break;

				default:
					j9tty_printf( PORTLIB, "\n");
					break;
			}
	}

	if(code->exceptionTableLength)
	{
		j9tty_printf( PORTLIB, "\nException Table");
		if(bytecodes)
		{
			j9tty_printf( PORTLIB, " - PCs may be wrong\n");
		}
		else
		{
			j9tty_printf( PORTLIB, "\n");
		}
		j9tty_printf( PORTLIB, "start   end   handler   catch type\n");
		j9tty_printf( PORTLIB, "-----   ---   -------   ----------\n");
		for(i = 0; i < code->exceptionTableLength; i++)
		{
			j9tty_printf( PORTLIB, "%5i%6i%10i   ",
				code->exceptionTable[i].startPC,
				code->exceptionTable[i].endPC,
				code->exceptionTable[i].handlerPC,
				 0);
			if(code->exceptionTable[i].catchType)
			{
				int k;
				info = classfile->constantPool[code->exceptionTable[i].catchType];
				info = classfile->constantPool[info.slot1];
				for(k = 0; k < (I_32)info.slot1; k++)
				{
					j9tty_printf( PORTLIB, "%c", info.bytes[k] == '/' ? '.' : info.bytes[k]);
				}
				j9tty_printf( PORTLIB, "\n");
			}
			else
			{
				j9tty_printf( PORTLIB, "(any)\n");
			}
		}
	}

	j9tty_printf( PORTLIB, "\n");
	return;
}





static void dumpHelpText( J9PortLibrary *portLib, int argc, char **argv)
{
	char detailString[1024];
	PORT_ACCESS_FROM_PORT(portLib);

	vmDetailString(portLib, detailString, 1024);
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "\nOpenJ9 Java(TM) Class File Reader, Version " J9JVM_VERSION_STRING);
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "\n" J9_COPYRIGHT_STRING);
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "\nTarget: %s\n", detailString);
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "\nJava and all Java-based marks and logos are trademarks or registered");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "\ntrademarks of Oracle, Inc.\n\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT,  "Usage:\t%s [options] classfile\n\n", argv[0] );
	j9file_printf( PORTLIB, J9PORT_TTY_OUT,  "[options]\n" );
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -b               show bytecodes\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -bi              show bytecodes with jsrs inlined\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -c               check classfile structure\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -d               dump structure details\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -di              dump structure details with jsrs inlined\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -dr              dump J9 ROM Class physical memory layout\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -dr:<depth>      dump J9 ROM Class physical memory layout with specified nesting depth\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -drx             dump J9 ROM Class physical memory layout (XML)\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -drq:q1[,q2,...] query J9 ROM Class physical memory layout\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "                     Query examples:\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "                       -drq:/romHeader\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "                       -drq:/romHeader,/methods\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "                       -drq:/romHeader/className,/romHeader/romSize\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "                       -drq:/methods/method[3]/name,/methods/method[3]/methodBytecodes\n");	
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -h or -?         print usage\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -j:<jimagefile>  load from JImage file\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -jh:<jimagefile> load JImage file and display header and resources metadata\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -jp:<jimagefile> display the module containing the given package in the JImage file\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -jx:<jimagefile> extract resources from JImage file\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -m               dump multiple classes\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -o:<filename>    write out J9 ROM Class to file (use with -x)\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -oh              write out J9 ROM Class to file under the package hierarchy\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -p               pedantic mode (-Xfuture)\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -q               quiet mode (don't print standard class description)\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -r               recurse subdirectories\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -r:<filename>    read in J9 ROM Class from file\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -sd              strip debug attributes\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -t:<filename>    write out recreated .class data to file\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -x               translate to J9 ROM format\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -xm              translate to J9 ROM format with local, debug and stack maps (print only)\n");
	j9file_printf( PORTLIB, J9PORT_TTY_OUT, "  -z:<zipfile>     load from ZIP or JAR\n\n");
}

static I_32
writeOutputFile(U_8 *fileData, U_32 length, char* filename, char *extension)
{
	char* newFilename;
	I_32 extnLen = 0;
	IDATA i;
	IDATA fd;
	BOOLEAN appendExtension = FALSE;

	PORT_ACCESS_FROM_PORT(portLib);

	i = strlen(filename);

	if (NULL != extension) {
		extnLen = (I_32) strlen(extension);
		/* Append extension if the filename doesn't have it already. */
		if ((i < (extnLen + 1)) || (0 != strcmp(filename + i - extnLen, extension))) {
			newFilename = j9mem_allocate_memory(i + extnLen + 1, J9MEM_CATEGORY_CLASSES); /* filename + extension + null */
			appendExtension = TRUE;
		}
	}
	if (TRUE == appendExtension) {
		newFilename = j9mem_allocate_memory(i + extnLen + 1, J9MEM_CATEGORY_CLASSES); /* filename + extension + null */
	} else {
		newFilename = j9mem_allocate_memory(i + 1, J9MEM_CATEGORY_CLASSES); /* filename + null */
	}
	if (!newFilename) {
		return -3;
	}

	memcpy(newFilename, filename, i);
	if (TRUE == appendExtension) {
		memcpy(newFilename + i, extension, extnLen);
		i += extnLen;
	}
	newFilename[i] = '\0';

	fd = j9file_open(newFilename, EsOpenWrite | EsOpenCreate | EsOpenTruncate , 0666);
	if (-1 == fd) {
		return RET_FILE_OPEN_FAILED;
	}

	if ((U_32)j9file_write(fd, fileData, length) != length) {
		j9file_close(fd);
		return RET_FILE_WRITE_FAILED;
	}

	if(!(options.options & OPTION_quiet))
		j9tty_printf( PORTLIB, "Wrote %i bytes to output file %s\n", length, newFilename);

	j9file_close(fd);

	j9mem_free_memory(newFilename);
	return length;
}

static int parseCommandLine(int argc, char** argv, char** envp)
{
	int i;

	PORT_ACCESS_FROM_PORT(portLib);
	memset(&options, 0, sizeof(CFDumpOptions));
	if(argc == 1) goto help;
	for(i = 1; i < argc; i++)
	{
		IDATA optionLen = strlen(argv[i]);

		if(argv[i][0] != '-')
		{
			options.filesStart = i;
			break;
		}
		if (optionLen < 2) {
			goto incomplete;
		}

		switch(argv[i][1])
		{
			case 'b':
				if(options.action) goto incompatible;
				options.action = ACTION_bytecodes;
				if (argv[i][2] != 'i') {
					options.options = OPTION_leaveJSRs;
				}
				break;

			case 'c':
				options.options |= OPTION_check;
				options.options |= OPTION_dumpPreverifyData;
				break;

			case 'd':
				if(options.action) goto incompatible;
				if (argv[i][2] == 'r') {
					options.actionString = "1";
					if (argv[i][3] == 'x') {
						options.action = ACTION_dumpRomClassXML;
						if (argv[i][4] == ':') {
							options.actionString = &argv[i][5];
						}
					} else if (argv[i][3] == 'q') {
						options.action = ACTION_queryRomClass;
						if (argv[i][4] == ':') {
							options.actionString = &argv[i][5];
						} else {
							options.actionString = "";
						}
					} else {
						options.action = ACTION_dumpRomClass;
						if (argv[i][3] == ':') {
							options.actionString = &argv[i][4];
						}
					}
					options.options |= OPTION_translate;
				} else {
					options.action = ACTION_structure;
					if (argv[i][2] != 'i') {
						options.options = OPTION_leaveJSRs;
					}
				}
				break;

			case 'f':
				if(argv[i][3] != ':') goto incomplete;
				switch(argv[i][2])
				{
					case 'c':
						if(options.action) goto incompatible;
						options.action = ACTION_formatClass;
						options.actionString = &argv[i][4];
						break;

					case 'f':
						if(options.action) goto incompatible;
						options.action = ACTION_formatFields;
						options.actionString = &argv[i][4];
						break;

					case 'm':
						if(options.action) goto incompatible;
						options.action = ACTION_formatMethods;
						options.actionString = &argv[i][4];
						break;

					case 'b':
						if(options.action) goto incompatible;
						options.action = ACTION_formatBytecodes;
						options.actionString = &argv[i][4];
						break;

					default:
						goto unknown;
				}
				break;

			case 'h':
			case '?':
				goto help;

			case 'i':
				options.options |= OPTION_verbose;
				break;

			case 'j':
			{
				/* similar to -z option, but for jimage files */
				if (options.source) {
					goto incompatible;
				}
				if (optionLen < 4) {
					/* Minimum size option is -j:[a-z] */
					goto incomplete;
				}
				options.source = SOURCE_jimage;
				if (argv[i][2] != ':') {
					if (optionLen < 5) {
						goto incomplete;
					}
					if (argv[i][3] == ':') {
						if (argv[i][2] == 'x') {
							/* found '-jx:' option */
							options.action = ACTION_writeJImageResource;
							options.sourceString = &argv[i][4];
						} else if (argv[i][2] == 'p') {
							/* found '-jp:' option */
							options.action = ACTION_findModuleForPackage;
							options.sourceString = &argv[i][4];
						} else if (argv[i][2] == 'h') {
							/* found '-jh:' option */
							options.action = ACTION_dumpJImageInfo;
							options.sourceString = &argv[i][4];
						} else {
							goto unknown;
						}
					} else {
						IDATA jimageOptLen = strlen("-jimage:");
						if ((optionLen < jimageOptLen) || strncmp(argv[i], "-jimage:", jimageOptLen)) {
							goto incomplete;
						}
						options.action = ACTION_dumpJImageInfo;
						options.sourceString = &argv[i][jimageOptLen];
					}
				} else {
					options.sourceString = &argv[i][3];
				}
				break;
			}
			case 'm':
				options.options |= OPTION_multi;
				break;

			case 'o':
				if (options.action) {
					goto incompatible;
				}
				options.action = ACTION_writeRomClassFile;
				if (argv[i][2] == 'h') {
					options.action = ACTION_writeRomClassFileHierarchy;
				} else if(argv[i][2] == ':') {
					options.actionString = &argv[i][3];
				}
				options.options |= OPTION_translate;
				break;

			case 'p':
				options.options |= OPTION_pedantic;
				break;

			case 'q':
				options.options |= OPTION_quiet;
				break;

			case 'r':
			case 'R':
				if(argv[i][2] == ':')
				{
					if(options.source) goto incompatible;
					options.source = SOURCE_rom;
					options.options |= OPTION_translate;
					options.sourceString = &argv[i][3];
				}
				else
				{
					options.options |= OPTION_recursive;
				}
				break;

			case 's':
				switch(argv[i][2])
				{
					case 'd':
						options.options |= OPTION_stripDebugAttributes;
						break;
					case 'l':
						options.options |= OPTION_stripDebugLines;
						break;
					case 's':
						options.options |= OPTION_stripDebugSource;
						break;
					case 'v':
						options.options |= OPTION_stripDebugVars;
						break;

					default:
						goto unknown;
						break;
				}
				break;

			case 't':
				if(options.action) {
					goto incompatible;
				}
				options.action = ACTION_transformRomClass;
				if(argv[i][2] == ':') {
					options.actionString = &argv[i][3];
				}
				/* Enable translate by default */
				options.options |= OPTION_translate;
				break;

			case 'v':
				if((strlen(argv[i]) == 8) && !strcmp(argv[i], "-verbose"))
				{
					options.options |= OPTION_verbose;
					break;
				}
				options.options |= OPTION_dumpPreverifyData;
				break;

			case 'x':
				options.options |= OPTION_translate;
				if ((argv[i][2]) == 'm') {
					options.options |= OPTION_dumpMaps;
					if(options.action) {
						goto incompatible;
					}
					options.action = ACTION_dumpMaps;
				}
#ifdef J9VM_ENV_LITTLE_ENDIAN
				options.options |= OPTION_littleEndian;
				options.options &= ~OPTION_bigEndian;
#else
				options.options |= OPTION_bigEndian;
				options.options &= ~OPTION_littleEndian;
#endif
				break;

			case 'z':
				if(options.source) goto incompatible;
				options.source = SOURCE_zip;
				if(argv[i][2] != ':')
				{
					if((strlen(argv[i]) < 5) || strncmp(argv[i], "-zip:", 5)) goto incomplete;
					options.sourceString = &argv[i][5];
				}
				else
				{
					options.sourceString = &argv[i][3];
				}
				break;

			default:
				goto unknown;
				break;
		}
	}

	if (!options.filesStart) {
		options.filesStart = argc;

		/* If no files specified but zip source, go into multi mode. */
		if ((SOURCE_zip == options.source) || (SOURCE_jimage == options.source)) {
			options.options |= OPTION_multi;
		} else if(options.source == SOURCE_rom)	{
			if (options.action) {
				options.options |= OPTION_multi;
			}
		} else {
			/* Must be file mode -- where's the file? */
			j9tty_printf( PORTLIB, "No filename specified\n");
			return 1;
		}
	} else if(options.filesStart < argc - 1) {
		/* If multiple files specified, go into multi mode. */
		options.options |= OPTION_multi;
	}

	/* If doing a multi operation, don't use specified names for output files. */
	if(((options.options & OPTION_multi)
		&& ((options.action == ACTION_writeRomClassFile) || (options.action == ACTION_transformRomClass)))
		|| (ACTION_writeRomClassFileHierarchy == options.action)
	) {
		options.actionString = NULL;
	}

	return 0;

unknown:
	j9tty_printf( PORTLIB, "Unrecognized option(s) on command line. First was: %s\n", argv[i]);
	return RET_COMMANDLINE_INCORRECT;

incomplete:
	j9tty_printf( PORTLIB, "Incomplete option(s) on command line. First was: %s\n", argv[i]);
	return RET_COMMANDLINE_INCORRECT;

incompatible:
	j9tty_printf( PORTLIB, "Incompatible options on command line. First was: %s\n", argv[i]);
	return RET_COMMANDLINE_INCORRECT;

help:
	dumpHelpText(PORTLIB, argc, argv);
	return RET_COMMANDLINE_INCORRECT;
}


static void printDisassembledMethods(J9CfrClassFile *classfile)
{
	I_32 i;

	for(i = 0; i < classfile->methodsCount; i++)
		printDisassembledMethod(classfile, &(classfile->methods[i]), TRUE, NULL, 0);
}



static J9CfrClassFile* loadClassFile(char* requestedFile, U_32* lengthReturn, U_32 flags)
{
	J9CfrClassFile* classfile;
	U_8* data;
	U_32 dataLength;

	PORT_ACCESS_FROM_PORT(portLib);

	data = loadBytes(requestedFile, &dataLength, flags);
	if(data == NULL) return NULL;
	classfile = loadClassFromBytes(data, dataLength, requestedFile, flags);
	j9mem_free_memory(data);
	*lengthReturn = dataLength;
	return classfile;
}

static U_32 buildFlags(void)
{
	U_32 flags;

	flags = BCT_RetainRuntimeInvisibleAttributes;

	/* Set vm flags to the latest version to pass version validation
	 * check in j9bcutil_readClassFileBytes.
	 */
	flags |= BCT_JavaMaxMajorVersionShifted;
	flags |= BCT_AnyPreviewVersion;

	if(options.options & OPTION_stripDebugAttributes) flags |= CFR_StripDebugAttributes;
	if(options.options & OPTION_stripDebugLines) flags |= BCT_StripDebugLines;
	if(options.options & OPTION_stripDebugSource) flags |= BCT_StripDebugSource;
	if(options.options & OPTION_stripDebugVars) flags |= BCT_StripDebugVars;
	if(options.options & OPTION_check) flags |= CFR_StaticVerification;
	if(options.options & OPTION_littleEndian) flags |= BCT_LittleEndianOutput;
	if(options.options & OPTION_bigEndian) flags |= BCT_BigEndianOutput;
	if(options.options & OPTION_leaveJSRs) flags |= CFR_LeaveJSRs;
	if(options.options & OPTION_pedantic) flags |= BCT_Xfuture;
	if(options.options & OPTION_dumpMaps) flags |= BCT_DumpMaps;
	return flags;
}


static I_32 processClassFile(J9CfrClassFile* classfile, U_32 dataLength, char* requestedFile, U_32 flags)
{
	U_32 i;

	PORT_ACCESS_FROM_PORT(portLib);

	switch(options.action)
	{
		case ACTION_definition:
			if(!((options.options & OPTION_check) && (options.options & OPTION_quiet)))
				printClassFile(classfile);
			break;

		case ACTION_structure:
			dumpClassFile(classfile);
			break;

		case ACTION_bytecodes:
			if(!(options.options & OPTION_quiet)) printClassFile(classfile);
			printDisassembledMethods(classfile);
			break;

		case ACTION_formatClass:
			sun_formatClass(classfile, options.actionString, strlen(options.actionString));
			j9tty_printf(PORTLIB, "\n");
			break;

		case ACTION_formatFields:
			for(i = 0; i < classfile->fieldsCount; i++)
			{
				sun_formatField(classfile, &(classfile->fields[i]), options.actionString, strlen(options.actionString));
				j9tty_printf(PORTLIB, "\n");
			}
			break;

		case ACTION_formatMethods:
			for(i = 0; i < classfile->methodsCount; i++)
			{
				sun_formatMethod(classfile, &(classfile->methods[i]), options.actionString, strlen(options.actionString));
				j9tty_printf(PORTLIB, "\n");
			}
			break;

		case ACTION_formatBytecodes:
			for(i = 0; i < classfile->methodsCount; i++)
			{
				sun_formatBytecodes(classfile, &(classfile->methods[i]), TRUE, NULL, 0, options.actionString, strlen(options.actionString));
			}
			break;

		case ACTION_dumpMaps:
			/* Case reserves mapping output to only dump to screen mode for ROM classes */
			break;

		default:
			return RET_UNSUPPORTED_ACTION;
	}
	return 0;
}


static I_32
processAllFiles(char **files, U_32 flags)
{
	I_32 i = 0;

	PORT_ACCESS_FROM_PORT(portLib);

	for (i = 0; ; ++i) {
		char *currentFile = files[i];
		if (NULL == currentFile) {
			break;
		}
		if (j9file_attr(currentFile) != EsIsDir) {
			processSingleFile(currentFile, flags);
		} else {
			IDATA length = strlen(currentFile);
			if ((length > 0) && (PATH_SEP_CHAR == currentFile[length - 1])) {
				processDirectory(currentFile, (BOOLEAN) ((options.options & OPTION_recursive) != 0), flags);
			} else {
				char *dirCopy = j9mem_allocate_memory(length + 2, J9MEM_CATEGORY_CLASSES);
				if (NULL == dirCopy) {
					return RET_ALLOCATE_FAILED;
				}
				memcpy(dirCopy, currentFile, length);
				dirCopy[length] = PATH_SEP_CHAR;
				dirCopy[length + 1] = '\0';
				processDirectory(dirCopy, (BOOLEAN) ((options.options & OPTION_recursive) != 0), flags);
				j9mem_free_memory(dirCopy);
			}
		}
	}
	return 0;
}


static I_32
processAllInZIP(char* zipFilename, U_32 flags)
{
	J9ROMClass* romClass = NULL;
	J9CfrClassFile* classfile = NULL;
	J9ZipFile zipFile;
	J9ZipEntry entry;
	U_8* dataBuffer = NULL;
	U_32 dataBufferSize = 1024;
	I_32 result;
	IDATA nextEntry;

	PORT_ACCESS_FROM_PORT(portLib);

	/* Open the zip file (in non-cached mode) */
	result = zip_openZipFile(PORTLIB, zipFilename, &zipFile, NULL, J9ZIP_OPEN_NO_FLAGS);
	if(result)
	{
		j9tty_printf(PORTLIB, "Could not open or read %s\n", zipFilename);
		return RET_ZIP_OPEN_FAILED;
	}

	dataBuffer = j9mem_allocate_memory(dataBufferSize, J9MEM_CATEGORY_CLASSES);
	if(dataBuffer == NULL)
	{
		j9tty_printf( PORTLIB, "Insufficient memory to complete operation\n");
		result = RET_ALLOCATE_FAILED;
		goto cleanExit;
	}

	zip_resetZipFile(PORTLIB, &zipFile, &nextEntry);

	while(1)
	{
		/* Read the next entry. */
		result = zip_getNextZipEntry(PORTLIB, &zipFile, &entry, &nextEntry, TRUE);
		if(result == ZIP_ERR_NO_MORE_ENTRIES)
		{
			result = 0;
			goto cleanExit;
		}
		if(result)
		{
			result = RET_ZIP_GETNEXTZIPENTRY_FAILED;
			j9tty_printf( PORTLIB, "Error reading %s\n", zipFilename);
			goto cleanExit;
		}

		/* Don't bother with files that can't possibly have a ".class" extension. */
		if(entry.filenameLength > 6)
		{
			/* Check for the ".class" extension. */
			if(!strcmp((const char*)&entry.filename[entry.filenameLength - 6], ".class"))
			{
				/* Looks like a class file. Ensure buffer space. Read the data. */
				if(entry.uncompressedSize > dataBufferSize)
				{
					j9mem_free_memory(dataBuffer);
					dataBufferSize = ((entry.uncompressedSize / 1024) + 1) * 1024;
					dataBuffer = j9mem_allocate_memory(dataBufferSize, J9MEM_CATEGORY_CLASSES);
					if(dataBuffer == NULL)
					{
						j9tty_printf( PORTLIB, "Insufficient memory to complete operation\n");
						result = RET_ALLOCATE_FAILED;
						goto cleanExit;
					}
				}

				result = zip_getZipEntryData(PORTLIB, &zipFile, &entry, dataBuffer, dataBufferSize);
				if(result)
				{
					j9tty_printf( PORTLIB, "Error reading %s from %s\n", entry.filename, zipFilename);
					continue;
				}

				if(options.options  & OPTION_translate)
				{
					romClass = translateClassBytes(dataBuffer, entry.uncompressedSize, (char*)entry.filename, flags);
					if(!romClass) {
						continue;
					}
					result = processROMClass(romClass, (char*)entry.filename, flags);
					if(result) {
						goto cleanExit;
					}
					j9mem_free_memory((U_8*)romClass);
					romClass = NULL;
				}
				else
				{
					classfile = loadClassFromBytes(dataBuffer, entry.uncompressedSize, (char*)entry.filename, flags);
					if(!classfile) continue;
					result = processClassFile(classfile, entry.uncompressedSize, (char*)entry.filename, flags);
					j9mem_free_memory((U_8*)classfile);
					classfile = NULL;
				}
			}
		}
	}
	result = 0;

cleanExit:
	zip_releaseZipFile(PORTLIB, &zipFile);
	if(dataBuffer) j9mem_free_memory(dataBuffer);
	if(classfile) j9mem_free_memory((U_8*)classfile);
	if(romClass) j9mem_free_memory((U_8*)romClass);
	return result;
}

static I_32 processFilesInZIP(char* zipFilename, char** files, U_32 flags)
{
	J9ROMClass* romClass = NULL;
	J9CfrClassFile* classfile = NULL;
	J9ZipFile zipFile;
	J9ZipEntry entry;
	J9ZipCachePool* cachePool = NULL;
	char* requestedFile;
	char** convertedFiles = NULL;
	U_8* dataBuffer = NULL;
	U_32 dataBufferSize = 1024;
	I_32 result, i;
	I_32 fileCount;

	PORT_ACCESS_FROM_PORT(portLib);

	result = convertToClassFilename((const char **)files, &convertedFiles, &fileCount);
	if (RET_SUCCESS != result) {
		j9tty_printf(PORTLIB, "Error in converting file name to .class file name\n");
		goto cleanExit;
	}

	/* make ourselves a pool and just let zipsup manage the cache */
	cachePool = zipCachePool_new(PORTLIB, NULL);
	/* Open the zip file. */
	result = zip_openZipFile(PORTLIB, (char*)zipFilename, &zipFile, cachePool, J9ZIP_OPEN_READ_CACHE_DATA);
	if (0 != result) {
		j9tty_printf(PORTLIB, "Could not open or read %s\n", zipFilename);
		return RET_ZIP_OPEN_FAILED;
	}

	zip_initZipEntry(PORTLIB, &entry);

	dataBuffer = j9mem_allocate_memory(dataBufferSize, J9MEM_CATEGORY_CLASSES);
	if(dataBuffer == NULL)
	{
		j9tty_printf( PORTLIB, "Insufficient memory to complete operation\n");
		result = RET_ALLOCATE_FAILED;
		goto cleanExit;
	}

	for(i = 0; i < fileCount; i++)
	{
		IDATA filenameLength;

		zip_freeZipEntry(PORTLIB, &entry);
		zip_initZipEntry(PORTLIB, &entry);

		requestedFile = convertedFiles[i];
		/* Search for the entry. */
		filenameLength = strlen(requestedFile);
		result = zip_getZipEntry(PORTLIB, &zipFile, &entry, requestedFile, filenameLength, J9ZIP_GETENTRY_READ_DATA_POINTER);
		if(result)
		{
			j9tty_printf(PORTLIB, "File %s not found in %s\n", requestedFile, zipFilename);
			continue;
		}

		/* Ensure buffer space. Read the data. */
		if(entry.uncompressedSize > dataBufferSize)
		{
			j9mem_free_memory(dataBuffer);
			dataBufferSize = ((entry.uncompressedSize / 1024) + 1) * 1024;
			dataBuffer = j9mem_allocate_memory(dataBufferSize, J9MEM_CATEGORY_CLASSES);
			if(dataBuffer == NULL)
			{
				j9tty_printf( PORTLIB, "Insufficient memory to complete operation\n");
				result = RET_ALLOCATE_FAILED;
				goto cleanExit;
			}
		}

		result = zip_getZipEntryData(PORTLIB, &zipFile, &entry, dataBuffer, dataBufferSize);
		if (result) {
			j9tty_printf( PORTLIB, "Error reading %s from %s\n", requestedFile, zipFilename);
			continue;
		}

		if (options.options  & OPTION_translate) {
			romClass = translateClassBytes(dataBuffer, entry.uncompressedSize, requestedFile, flags);
			if(!romClass) continue;
				result = processROMClass(romClass, requestedFile, flags);
			if(result) goto cleanExit;
			j9mem_free_memory((U_8*)romClass);
			romClass = NULL;
		} else {
			classfile = loadClassFromBytes(dataBuffer, entry.uncompressedSize, requestedFile, flags);
			if(!classfile) continue;
			processClassFile(classfile, entry.uncompressedSize, requestedFile, flags);
			j9mem_free_memory((U_8*)classfile);
			classfile = NULL;
		}
	}
	result = 0;

cleanExit:
	zip_freeZipEntry(PORTLIB, &entry);
	j9mem_free_memory(convertedFiles);
	zip_releaseZipFile(PORTLIB, &zipFile);
	if (cachePool)  zipCachePool_kill(cachePool);
	if(dataBuffer) j9mem_free_memory(dataBuffer);
	if(classfile) j9mem_free_memory((U_8*)classfile);
	if(romClass) j9mem_free_memory((U_8*)romClass);
	return result;
}



/**
 * Process jimage resource specified by "j9jimageLocation".
 *
 * @param [in] jimageFileName name of the jimage file
 * @param [in] jimage pointer to J9JImage
 * @param [in] j9jimageLocation pointer to J9JImageLocation storing metadata about the resource
 * @param [in] name of the resource represented by j9jimageLocation
 * @param [in/out] pointer to a buffer for storing resource data; if the buffer is not large enough, it is freed and reallocated to desired amount
 * @param [in/out] pointer to integer specifying input buffer size; if the buffer is reallocated, then it is updated with the new size
 * @param [in] flags build flags used during processing of class files
 *
 * @return RET_SUCCESS on success, negative error code on failure
 */
static I_32
processJImageResource(const char *jimageFileName, J9JImage *jimage, J9JImageLocation *j9jimageLocation, const char *resourceName, U_8 **dataBuffer, UDATA *dataBufferSize, U_32 flags)
{
	U_8 *buffer = *dataBuffer;
	UDATA bufferSize = *dataBufferSize;
	U_64 size = 0;
	I_32 result = RET_SUCCESS;

	PORT_ACCESS_FROM_PORT(portLib);

	size = j9jimageLocation->uncompressedSize;
	if (size > bufferSize) {
		j9mem_free_memory(buffer);
		bufferSize = (UDATA)(((size / 1024) + 1) * 1024);
		buffer = j9mem_allocate_memory(bufferSize, J9MEM_CATEGORY_CLASSES);
		if (NULL == buffer) {
			j9tty_printf(PORTLIB, "Insufficient memory to complete operation\n");
			result = RET_ALLOCATE_FAILED;
			goto _exit;
		}
		*dataBuffer = buffer;
		*dataBufferSize = bufferSize;
	}

	result = j9bcutil_getJImageResource(PORTLIB, jimage, j9jimageLocation, buffer, bufferSize);
	if (J9JIMAGE_NO_ERROR != result) {
		j9tty_printf(PORTLIB, "Error in getting data of the resource %s in jimage file %s. Error code=%d\n", resourceName, jimageFileName, result);
		result = RET_JIMAGE_GETJIMAGERESOURCE_FAILED;
		goto _exit;
	}

	if (0 != (options.options & OPTION_translate)) {
		J9ROMClass *romClass = translateClassBytes(buffer, (U_32)size, (char *)resourceName, flags);
		if (NULL != romClass) {
			result = processROMClass(romClass, (char *)resourceName, flags);
			if (NULL != romClass) {
				j9mem_free_memory((U_8*)romClass);
				romClass = NULL;
			}
			if (RET_SUCCESS != result) {
				goto _exit;
			}
		} else {
			result = RET_LOADROMCLASSFROMBYTES_FAILED;
		}
	} else if (ACTION_writeJImageResource == options.action) {
		char tempPath[EsMaxPath];
		char *current = NULL;
		char jimageFileSeparator = '/';

		/* skip leading '/' if present */
		if ('/' == resourceName[0]) {
			j9str_printf(PORTLIB, tempPath, EsMaxPath, "%s", resourceName + 1);
		} else {
			j9str_printf(PORTLIB, tempPath, EsMaxPath, "%s", resourceName);
		}
		current = strchr(tempPath, jimageFileSeparator);
		while (NULL != current) {
			I_32 rc = 0;

			*current = '\0';
			rc = j9file_mkdir(tempPath);
			if (0 != rc) {
				I_32 errorCode = j9error_last_error_number();

				if (J9PORT_ERROR_FILE_EXIST != errorCode) {
					j9tty_printf(PORTLIB, "Error in creating path %s. Portable error code: %d\n", tempPath, errorCode);
					return RET_CREATE_DIR_FAILED;
				}
			}
			*current = DIR_SEPARATOR;
			current = strchr(current + 1, jimageFileSeparator);
		}
		writeOutputFile(buffer, (U_32)size, tempPath, NULL);
	} else {
		J9CfrClassFile* classfile = loadClassFromBytes(buffer, (U_32)size, (char *)resourceName, flags);
		if (NULL != classfile) {
			result = processClassFile(classfile, (U_32)size, (char *)resourceName, flags);
			if (NULL != classfile) {
				j9mem_free_memory((U_8*)classfile);
				classfile = NULL;
			}
			if (RET_SUCCESS != result) {
				goto _exit;
			}
		} else {
			result = RET_LOADCLASSFROMBYTES_FAILED;
		}
	}

_exit:
	return result;
}

/**
 * Process all ".class" files in the given jimage file.
 * 
 * @param [in] jimage pointer to J9JImage representing the jimage file
 * @param [in] jimageFileName name of the jimage file to be used
 * @param [in] flags build flags used during processing of class files
 * 
 * @return RET_SUCCESS on success, negative error code on failure
 */
static I_32
processAllInJImage(J9JImage *jimage, char *jimageFileName, U_32 flags)
{
	J9JImageHeader *j9jimageHeader = jimage->j9jimageHeader;
	JImageHeader *jimageHeader = j9jimageHeader->jimageHeader;
	U_32 lotIndex = 0;
	U_8 *dataBuffer = NULL;
	UDATA dataBufferSize = 1024;
	I_32 result = RET_SUCCESS;

	PORT_ACCESS_FROM_PORT(portLib);

	dataBuffer = j9mem_allocate_memory(dataBufferSize, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataBuffer) {
		j9tty_printf( PORTLIB, "Insufficient memory to complete operation\n");
		result = RET_ALLOCATE_FAILED;
		goto cleanExit;
	}

	for (lotIndex = 0; lotIndex < jimageHeader->tableLength; lotIndex++) {
		void *imageLocation = NULL;
		J9JImageLocation j9jimageLocation = {0};
		U_32 locationOffset = 0;
		char *resourceName = NULL;

		locationOffset = j9jimageHeader->locationsOffsetTable[lotIndex];
		if (locationOffset >= JIMAGE_LOCATION_DATA_SIZE(*jimageHeader)) {
			j9tty_printf(PORTLIB, "Invalid location offset found at index %u in Locations Offset Table of jimage file %s\n", lotIndex, jimageFileName);
			result = RET_JIMAGE_INVALIDLOCATION_OFFSET;
			goto cleanExit;
		}

		imageLocation = j9jimageHeader->locationsData + j9jimageHeader->locationsOffsetTable[lotIndex];
		result = j9bcutil_createAndVerifyJImageLocation(PORTLIB, jimage, NULL /* resourceName */, imageLocation, &j9jimageLocation);
		if (J9JIMAGE_NO_ERROR != result) {
			j9tty_printf(PORTLIB, "Error in creating JImageLocation. Error code=%d\n", result);
			result = RET_JIMAGE_CREATEJIMAGELOCATION_FAILED;
			goto cleanExit;
		}

		/* Don't bother with files that don't have a ".class" extension. */
		if ((NULL == j9jimageLocation.extensionString)
			|| (strncmp(j9jimageLocation.extensionString, CFDUMP_CLASSFILE_EXTENSION_WITHOUT_DOT, sizeof(CFDUMP_CLASSFILE_EXTENSION_WITHOUT_DOT)))
		) {
			continue;
		}

		result = j9bcutil_getJImageResourceName(PORTLIB, jimage, j9jimageLocation.moduleString, j9jimageLocation.parentString, j9jimageLocation.baseString, j9jimageLocation.extensionString, &resourceName);
		if (J9JIMAGE_NO_ERROR == result) {
			result = processJImageResource(jimageFileName, jimage, &j9jimageLocation, resourceName, &dataBuffer, &dataBufferSize, flags);
			j9mem_free_memory(resourceName);
			if (RET_SUCCESS != result) {
				goto cleanExit;
			}
		} else {
			j9tty_printf(PORTLIB, "Insufficient memory to complete operation\n");
		}
	}

	result = RET_SUCCESS;

cleanExit:
	if (NULL != dataBuffer) {
		j9mem_free_memory(dataBuffer);
	}
	return result;
}

/**
 * Search list of resources in the given jimage file.
 * If the resource is found, then process it as requested by the user.
 * 
 * @param [in] jimage pointer to J9JImage representing the jimage file
 * @param [in] jimageFileName name of the jimage file to be used
 * @param [in] resources list of resources to be searched in the jimage file
 * @param [in] flags build flags used during processing of class files
 * 
 * @return RET_SUCCESS on success, negative error code on failure
 */
static I_32
processFilesInJImage(J9JImage *jimage, char *jimageFileName, char **resources, U_32 flags)
{
	J9JImageHeader *j9jimageHeader = jimage->j9jimageHeader;
	JImageHeader *jimageHeader = j9jimageHeader->jimageHeader;
	I_32 resourceCount = 0;
	U_8* dataBuffer = NULL;
	UDATA dataBufferSize = 1024;
	I_32 i = 0;
	I_32 result =  RET_SUCCESS;
	JImageMatchInfo *matchInfo = NULL;

	PORT_ACCESS_FROM_PORT(portLib);

	if (ACTION_writeJImageResource == options.action) {
		/* Use same resource name(s) as specified by the user */
		char *currentFile = resources[0];
		while (NULL != currentFile) {
			resourceCount += 1;
			currentFile = resources[resourceCount];
		}
	} else {
		result = convertToJImageLocations(jimage, resources, &matchInfo, &resourceCount);
	}

	dataBuffer = j9mem_allocate_memory(dataBufferSize, J9MEM_CATEGORY_CLASSES);
	if (NULL == dataBuffer) {
		j9tty_printf( PORTLIB, "Insufficient memory to complete operation\n");
		result = RET_ALLOCATE_FAILED;
		goto cleanExit;
	}

	for (i = 0; i < resourceCount; i++) {
		J9JImageLocation j9jimageLocation = {0};
		char *resourceName = NULL;
		if (ACTION_writeJImageResource == options.action) {
			resourceName = resources[i];
		} else {
			result = j9bcutil_getJImageResourceName(PORTLIB, jimage,  matchInfo[i].module, matchInfo[i].parentString, matchInfo[i].baseString, CFDUMP_CLASSFILE_EXTENSION_WITHOUT_DOT, &resourceName);
			if (result != J9JIMAGE_NO_ERROR ){
				j9tty_printf(PORTLIB, "Insufficient memory to complete operation\n");
				goto cleanExit;
			}
		}


		result = j9bcutil_lookupJImageResource(PORTLIB, jimage, &j9jimageLocation, resourceName);
		if (J9JIMAGE_RESOURCE_NOT_FOUND == result) {
			j9tty_printf(PORTLIB, "File %s not found in %s\n", resourceName, jimageFileName);
			goto cleanExit;
		}

		result = processJImageResource(jimageFileName, jimage, &j9jimageLocation, resourceName, &dataBuffer, &dataBufferSize, flags);
		if (RET_SUCCESS != result) {
			goto cleanExit;
		}
		if (ACTION_writeJImageResource != options.action) {
			j9mem_free_memory(resourceName);
		}
	}

	result = RET_SUCCESS;

cleanExit:
	if (NULL != dataBuffer) {
		j9mem_free_memory(dataBuffer);
	}
	if (NULL != matchInfo) {
		j9mem_free_memory(matchInfo);
	}
	return result;
}

static J9CfrClassFile* loadClassFromBytes(U_8* data, U_32 dataLength, char* requestedFile, U_32 flags)
{
	J9CfrError* error;
	U_8* segment;
	U_32 segmentLength;
	I_32 result;
	BOOLEAN verbose;

	PORT_ACCESS_FROM_PORT(portLib);
	verbose = (options.options & OPTION_verbose)?TRUE:FALSE;

	segmentLength = ESTIMATE_SIZE(dataLength);

	if(verbose) j9tty_printf( PORTLIB, "<file size: %i bytes>\n", dataLength);
	if(verbose) j9tty_printf( PORTLIB, "<estimated memory size: %i bytes>\n", segmentLength);

	do
	{
		segment = (U_8*)j9mem_allocate_memory(segmentLength, J9MEM_CATEGORY_CLASSES);
		if(segment == NULL) goto outOfMemory;

		result = j9bcutil_readClassFileBytes(PORTLIB, verifyBuffers ? j9bcv_verifyClassStructure : NULL,
			data, dataLength, segment, segmentLength, flags, NULL, NULL, 0, UDATA_MAX);

		if(result == -2)
		{
			/* Not enough space. Retry. */
			if(verbose) j9tty_printf( PORTLIB, "<increasing space to: %i bytes>\n", segmentLength*2);
			segmentLength = segmentLength * 2;
			j9mem_free_memory(segment);
		}
	} while (result == -2);

	if(result == -1)
	{
		error = (J9CfrError*)segment;
		reportClassLoadError(error, requestedFile);
		return NULL;
	}
	else if(result == -2)
	{
outOfMemory:
		j9tty_printf( PORTLIB, "Insufficient memory to complete operation\n");
		return NULL;
	}

	return (J9CfrClassFile*) segment;
}


static J9ROMClass *translateClassBytes(U_8 * data, U_32 dataLength, char *requestedFile, U_32 flags)
{
	U_8 *segment;
	U_64 xl8_startTime;
	U_64 xl8_endTime;
	IDATA result;
	UDATA bufferSize = 4 * dataLength;
	U_8 *classFileBuffer;

	PORT_ACCESS_FROM_PORT(portLib);

retry:
	segment = j9mem_allocate_memory(bufferSize, J9MEM_CATEGORY_CLASSES);
	if (!segment) {
		j9tty_printf(PORTLIB, "Insufficient memory to complete operation\n");
		return NULL;
	}

	classFileBuffer = NULL;

	xl8_startTime = j9time_hires_clock();
	result = j9bcutil_buildRomClassIntoBuffer(data, dataLength, PORTLIB, verifyBuffers, flags,
		translationBuffers->flags, 0, segment, bufferSize, NULL, 0, NULL, 0, &classFileBuffer);
	xl8_endTime = j9time_hires_clock();

	if (0 != (options.options & OPTION_verbose)) {
		j9tty_printf(PORTLIB, "<dynamic load time: %llu us.>\n", j9time_hires_delta(xl8_startTime, xl8_endTime, J9PORT_TIME_DELTA_IN_MICROSECONDS));
	}

	switch (result) {
	case BCT_ERR_NO_ERROR:
		j9mem_free_memory(classFileBuffer);
		return (J9ROMClass *) segment;

	case BCT_ERR_OUT_OF_ROM:
		j9mem_free_memory(classFileBuffer);
		j9mem_free_memory(segment);
		bufferSize *= 2;
		goto retry;

	case BCT_ERR_CLASS_READ:
		reportClassLoadError((J9CfrError *) classFileBuffer, requestedFile);
		break;

	default:
		j9tty_printf(PORTLIB, "<unknown error during translation: %d>\n", result);
		break;
	}

	j9mem_free_memory(classFileBuffer);
	j9mem_free_memory(segment);
	return NULL;
}

static BOOLEAN validateROMClassAddressRange(J9ROMClass *romClass, void *address, UDATA length, void *userData)
{
	return ((UDATA)address >= (UDATA)romClass) && ((UDATA)address + length <= (UDATA)romClass + romClass->romSize);
}

static I_32 processROMClass(J9ROMClass* romClass, char* requestedFile, U_32 flags)
{
	char* filename;
	int i, length;
	U_32 newFlags, j;
	char ch;
	J9ROMFieldWalkState state;
	J9ROMFieldShape * currentField;
	U_8 *data = (U_8 *) romClass;
	U_32 size = romClass->romSize;
	char *extension = ".rom";

	PORT_ACCESS_FROM_PORT(portLib);

	switch(options.action)
	{
		case ACTION_definition:
		case ACTION_structure:
		case ACTION_bytecodes:
		case ACTION_dumpMaps:
			newFlags = flags;
			if (options.options & OPTION_dumpPreverifyData) {
				newFlags |= BCT_DumpPreverifyData;
			}
			j9bcutil_dumpRomClass(romClass, PORTLIB, (J9TranslationBufferSet *)translationBuffers, newFlags);
			break;

		case ACTION_transformRomClass:
			{
				I_32 rc;
				/* we don't have J9JavaVM pointer here */
				rc = (I_32) j9bcutil_transformROMClass(NULL, PORTLIB, romClass, &data, &size);
				if (BCT_ERR_NO_ERROR != rc) {
					j9tty_printf(PORTLIB, "Error in recreating .class data: %d\n", rc);
					return rc;
				}
				extension = ".j9class";
			}
			/* fall through to write recreated .class data to file */
		case ACTION_writeRomClassFile:
			if(options.actionString) {
				writeOutputFile(data, size, options.actionString, extension);
			} else {
				length = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass));
				if(SWAPPING_ENDIAN(flags))
				{
					length =
						((((length) & 0xFF00) >> 8) |
						((length) & 0x00FF) << 8);
				}
				filename = j9mem_allocate_memory(length + 1, J9MEM_CATEGORY_CLASSES);
				for(i = 0; i < length; i++)
				{
					ch = ((U_8*) J9ROMCLASS_CLASSNAME(romClass))[i + 2];
					if(ch == '/') filename[i] = '_';
					else filename[i] = ch;
				}
				filename[i] = '\0';
				writeOutputFile(data, size, filename, extension);
				j9mem_free_memory(filename);
			}
			break;

		case ACTION_writeRomClassFileHierarchy:
			{
				char tempPath[EsMaxPath];
				char *current = NULL;
				U_8 *romClassName = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass));
				I_32 rc = 0;

				length = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass));
				if (SWAPPING_ENDIAN(flags)) {
					length = (((length & 0xFF00) >> 8) | (length & 0x00FF) << 8);
				}

				j9str_printf(PORTLIB, tempPath, EsMaxPath, "%.*s", length, romClassName);
				current = strchr(tempPath, DIR_SEPARATOR);
				while (NULL != current) {
					*current = '\0';
					rc = j9file_mkdir(tempPath);
					if ((0 != rc) && (j9error_last_error_number() != J9PORT_ERROR_FILE_EXIST)) {
						j9tty_printf(PORTLIB, "Error in creating path %s\n", tempPath);
						return RET_CREATE_DIR_FAILED;
					}
					*current = DIR_SEPARATOR;
					current = strchr(current + 1, DIR_SEPARATOR);
				}
				writeOutputFile(data, size, tempPath, extension);
			}
			break;
		case ACTION_formatClass:
			j9_formatClass(romClass, options.actionString, strlen(options.actionString), flags);
			j9tty_printf(PORTLIB, "\n");
			break;

		case ACTION_formatFields:
			currentField = romFieldsStartDo(romClass, &state);
			while (currentField != NULL) {
				j9_formatField(romClass, currentField, options.actionString, strlen(options.actionString), flags);
				j9tty_printf(PORTLIB, "\n");
				currentField = romFieldsNextDo(&state);
			}
			break;

		case ACTION_formatMethods:
			{
				J9ROMMethod * currentMethod = (J9ROMMethod *) J9ROMCLASS_ROMMETHODS(romClass);

				for(j = 0; j < romClass->romMethodCount; j++)
				{
					j9_formatMethod(romClass, currentMethod, options.actionString, strlen(options.actionString), flags);
					j9tty_printf(PORTLIB, "\n");
					currentMethod = nextROMMethod(currentMethod);
				}
			}
			break;

		case ACTION_formatBytecodes:
			{
				J9ROMMethod * currentMethod = (J9ROMMethod *) J9ROMCLASS_ROMMETHODS(romClass);

				for(j = 0; j < romClass->romMethodCount; j++)
				{
					j9_formatBytecodes(romClass, currentMethod, NULL, 0, options.actionString, strlen(options.actionString), flags);
					currentMethod = nextROMMethod(currentMethod);
				}
			}
			break;

		case ACTION_dumpRomClass:
			{
				UDATA nestingThreshold = (UDATA)atoi(options.actionString);
				j9bcutil_linearDumpROMClass(portLib, romClass, NULL, nestingThreshold, validateROMClassAddressRange);
			}
			break;

		case ACTION_dumpRomClassXML:
			j9bcutil_linearDumpROMClassXML(portLib, romClass, validateROMClassAddressRange);
			break;

		case ACTION_queryRomClass:
			j9bcutil_queryROMClassCommaSeparated(portLib, romClass, NULL, options.actionString, validateROMClassAddressRange);
			break;

		default:
			return RET_UNSUPPORTED_ACTION;
	}
	return 0;
}


/*	Print information about the bytecode formatted as per @formatString.
	Format string:
		%<modifiers><specifier>
	Supported specifiers:
		c				short class name
		C				qualified class name
		n				method name
		s				method signature
		i				instruction pointer
		b				bytecode
		B				complete bytecode
		p				parameters
	Supported modifiers:
		%i
			d			decimal (default)
			x			hex, lowercase
			X			hex, uppercase
		%B
			d			decimal
			x			hex, lowercase
			X			hex, uppercase (default)
		%b
			a			ascii (default)
			d			decimal
			x			hex, lowercase
			X			hex, uppercase
		%p
			a			ascii (default)
			d			decimal
			x			hex, lowercase
			X			hex, uppercase
*/
static void sun_formatBytecode(J9CfrClassFile* classfile, J9CfrMethod* method, BOOLEAN bigEndian, U_8* bcStart, U_8* bytes, U_8 bc, U_32 bytesLength, U_32 decode, char *formatString, IDATA length)
{
	J9CfrConstantPoolInfo* info;
	U_8* string;
	U_8* bcIndex;
	U_16 cpIndex;
	UDATA pc, branch;
	U_8 u8, u8_2;
	U_16 u16, u16_2;
	U_32 u32;
	U_32 index, npairs;
	U_32 j;
	I_32 low, high, key;
	I_32 i, k;
	char modifier;
	char pcModifier;
	char ch, ch2;

	PORT_ACCESS_FROM_PORT(portLib);

	pcModifier = 'd';
	pc = bytes - bcStart;

	i = 0;
	modifier = '\0';
	while(i < length)
	{
		ch = formatString[i++];
		if(modifier || ch == '%')
		{
			if(ch == '%')
			{
				modifier = '\0';
				ch = formatString[i++];
			}
			if(ch == '\0' || ch == '%')
			{
				if(modifier) j9tty_printf(PORTLIB, "%%%c", modifier);
				j9tty_output_char(ch);
			}
			else
			{
				switch(ch)
				{
					case 'c':
						/* short class name */
						cpIndex = classfile->constantPool[classfile->thisClass].slot1;
						string = classfile->constantPool[cpIndex].bytes;
						j = 0;
						index = 0;
						while('\0' != (ch2 = string[j++])) if(ch2 == '/') index = j;
						j = index;
						while('\0' != (ch2 = string[j++])) j9tty_output_char(ch2);
						break;

					case 'C':
						/* qualified class name */
						cpIndex = classfile->constantPool[classfile->thisClass].slot1;
						string = classfile->constantPool[cpIndex].bytes;
						j = 0;
						while('\0' != (ch2 = string[j++]))
						{
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'n':
						/* method name */
						string = classfile->constantPool[method->nameIndex].bytes;
						j9tty_printf(PORTLIB, "%s", string);
						break;

					case 's':
						/* method signature */
						string = classfile->constantPool[method->descriptorIndex].bytes;
						j = 0;
						while('\0' != (ch2 = string[j++]))
						{
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'i':
						/* instruction pointer */
						pcModifier = modifier;
						switch(modifier)
						{
							case 'x':
								j9tty_printf(PORTLIB, "%08x", pc);
								break;

							case 'X':
								j9tty_printf(PORTLIB, "%08X", pc);
								break;

							case 'd':
							default:
								j9tty_printf(PORTLIB, "%i", pc);
								break;
						}
						modifier = '\0';
						break;

					case 'b':
						/* bytecode */
						switch(modifier)
						{
							case 'x':
								j9tty_printf(PORTLIB, "%02x", bc);
								break;

							case 'X':
								j9tty_printf(PORTLIB, "%02X", bc);
								break;

							case 'd':
								j9tty_printf(PORTLIB, "%i", bc);
								break;

							default:
								j9tty_printf(PORTLIB, "%s", sunJavaBCNames[bc]);
								break;
						}
						modifier = '\0';
						break;

					case 'B':
						/* bytecode bytes */
						switch(modifier)
						{
							case 'd':
								for(j = 0; j < bytesLength; )
								{
									j9tty_printf(PORTLIB, "%i", bytes[j]);
									if(++j < bytesLength) j9tty_output_char(' ');
								}
								break;

							case 'x':
								for(j = 0; j < bytesLength; j++) j9tty_printf(PORTLIB, "%02x", bytes[j]);
								break;

							default:
							case 'X':
								for(j = 0; j < bytesLength; j++) j9tty_printf(PORTLIB, "%02X", bytes[j]);
								break;
						}
						modifier = '\0';
						break;

					case 'p':
						/* parameters */
						bcIndex = bytes + 1;
						if(bc != *bytes) bcIndex++;
						switch(decode)
						{
							case CFR_DECODE_SIMPLE:
								break;

							case CFR_DECODE_I8:
								NEXT_U8_ENDIAN(bigEndian, u8, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%02x", (I_8)u8);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%02X", (I_8)u8);
										break;

									case 'd':
									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i", (I_8)u8);
										break;
								}
								break;

							case CFR_DECODE_U8:
								NEXT_U8_ENDIAN(bigEndian, u8, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%02x", u8);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%02X", u8);
										break;

									case 'd':
									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i", u8);
										break;
								}
								break;

							case CFR_DECODE_I16:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x", (I_16)u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X", (I_16)u16);
										break;

									case 'd':
									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i", (I_16)u16);
										break;
								}
								break;

							case CFR_DECODE_U16:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x", (I_16)u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X", (I_16)u16);
										break;

									case 'd':
									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i", (I_16)u16);
										break;
								}
								break;

							case CFR_DECODE_U8_I8:
								NEXT_U8_ENDIAN(bigEndian, u8, bcIndex);
								NEXT_U8_ENDIAN(bigEndian, u8_2, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%02x %02x", u8, (I_8)u8_2);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%02X %02X", u8, (I_8)u8_2);
										break;

									case 'd':
									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i %i", u8, (I_8)u8_2);
										break;
								}
								break;

							case CFR_DECODE_U16_I16:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								NEXT_U16_ENDIAN(bigEndian, u16_2, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x %04x", u16, (I_16)u16_2);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X %04X", u16, (I_16)u16_2);
										break;

									case 'd':
									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i %i", u16, (I_16)u16_2);
										break;
								}
								break;

							case CFR_DECODE_CP8:
								NEXT_U8_ENDIAN(bigEndian, u8, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%02x", u8);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%02X", u8);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", u8);
										break;

									case 'a':
									default:
										info = &(classfile->constantPool[u8]);
cpAscii:
										switch(info->tag)
										{
											case CFR_CONSTANT_Integer:
												j9tty_printf( PORTLIB, "(int) 0x%08X", info->slot1);
												break;

											case CFR_CONSTANT_Float:
												j9tty_printf( PORTLIB, "(float) 0x%08X", info->slot1);
												break;

											case CFR_CONSTANT_Long:
#ifdef J9VM_ENV_LITTLE_ENDIAN
												j9tty_printf( PORTLIB, "(long) 0x%08X%08X", info->slot2, info->slot1);
#else
												j9tty_printf( PORTLIB, "(long) 0x%08X%08X", info->slot1, info->slot2);
#endif
												break;

											case CFR_CONSTANT_Double:
#ifdef J9VM_ENV_LITTLE_ENDIAN
												j9tty_printf( PORTLIB, "(double) 0x%08X%08X", info->slot2, info->slot1);
#else
												j9tty_printf( PORTLIB, "(double) 0x%08X%08X", info->slot1, info->slot2);
#endif
												break;

											case CFR_CONSTANT_String:
												string = classfile->constantPool[info->slot1].bytes;
												j9tty_printf(PORTLIB, "(java.lang.String) \"%s\"", string);
												break;
											case CFR_CONSTANT_MethodType:
												string = classfile->constantPool[info->slot1].bytes;
												j9tty_printf(PORTLIB, "(java.dyn.MethodType) \"%s\"", string);
												break;

											case CFR_CONSTANT_Class:
												string = classfile->constantPool[info->slot1].bytes;
												j = 0;
												while('\0' != (ch2 = string[j++]))
												{
													if(ch2 == '/') j9tty_output_char('.');
													else j9tty_output_char(ch2);
												}
												break;

											case CFR_CONSTANT_Fieldref:
											case CFR_CONSTANT_Methodref:
											case CFR_CONSTANT_InterfaceMethodref:
												cpIndex = classfile->constantPool[info->slot1].slot1;
												string = classfile->constantPool[cpIndex].bytes;
												j = 0;
												while('\0' != (ch2 = string[j++]))
												{
													if(ch2 == '/') j9tty_output_char('.');
													else j9tty_output_char(ch2);
												}
												cpIndex = classfile->constantPool[info->slot2].slot1;
												string = classfile->constantPool[cpIndex].bytes;
												j9tty_printf(PORTLIB, ".%s ", string);
												cpIndex = classfile->constantPool[info->slot2].slot2;
												string = classfile->constantPool[cpIndex].bytes;
												j = 0;
												while('\0' != (ch2 = string[j++]))
												{
													if(ch2 == '/') j9tty_output_char('.');
													else j9tty_output_char(ch2);
												}
												break;

											case CFR_CONSTANT_MethodHandle:
												/* TODO - print pretty: kind + field/method ref */
												j9tty_printf(PORTLIB, "<java.dyn.MethodHandle %i>", info->tag);
												break;

											case CFR_CONSTANT_Null:
											case CFR_CONSTANT_Utf8:
											case CFR_CONSTANT_NameAndType:
												j9tty_printf(PORTLIB, "<unexpected constant type %i>", info->tag);
												break;

											default:
												j9tty_printf(PORTLIB, "<unknown constant type %i>", info->tag);
												break;
										}
								}
								break;

							case CFR_DECODE_CP16:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x", u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X", u16);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", u16);
										break;

									case 'a':
									default:
										info = &(classfile->constantPool[u16]);
										goto cpAscii;
								}
								break;

							case CFR_DECODE_MULTIANEWARRAY:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								NEXT_U8_ENDIAN(bigEndian, u8, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%02x %04x", u8, u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%02X %04X", u8, u16);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i %i", u8, u16);
										break;

									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i ", u8);
										info = &(classfile->constantPool[u16]);
										goto cpAscii;
								}
								break;

							case CFR_DECODE_L16:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x", (I_16)u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X", (I_16)u16);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", (I_16)u16);
										break;

									case 'a':
									default:
										branch = pc + (I_16)u16;
										switch(pcModifier)
										{
											case 'x':
												j9tty_printf(PORTLIB, "%08x", branch);
												break;

											case 'X':
												j9tty_printf(PORTLIB, "%08X", branch);
												break;

											case 'd':
												j9tty_printf(PORTLIB, "%i", branch);
												break;
										}
										break;
								}
								break;

							case CFR_DECODE_L32:
								NEXT_U32_ENDIAN(bigEndian, u32, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%08x", (I_32)u32);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%08X", (I_32)u32);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", (I_32)u32);
										break;

									case 'a':
									default:
										branch = pc + (I_32)u32;
										switch(pcModifier)
										{
											case 'x':
												j9tty_printf(PORTLIB, "%08x", branch);
												break;

											case 'X':
												j9tty_printf(PORTLIB, "%08X", branch);
												break;

											case 'd':
												j9tty_printf(PORTLIB, "%i", branch);
												break;
										}
										break;
								}
								break;

							case CFR_DECODE_NEWARRAY:
								NEXT_U8_ENDIAN(bigEndian, u8, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%02x", u8);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%02X", u8);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", u8);
										break;

									case 'a':
									default:
										switch(u8)
										{
											case /*T_BOOLEAN*/ 4 :
												j9tty_printf( PORTLIB, "boolean");
												break;
											case /*T_CHAR*/  5:
												j9tty_printf( PORTLIB, "char");
												break;
											case /*T_FLOAT*/  6:
												j9tty_printf( PORTLIB, "float");
												break;
											case /*T_DOUBLE*/  7:
												j9tty_printf( PORTLIB, "double");
												break;
											case /*T_BYTE*/  8:
												j9tty_printf( PORTLIB, "byte");
												break;
											case /*T_SHORT*/  9:
												j9tty_printf( PORTLIB, "short");
												break;
											case /*T_INT*/  10:
												j9tty_printf( PORTLIB, "int");
												break;
											case /*T_LONG*/  11:
												j9tty_printf( PORTLIB, "long");
												break;
											default:
												j9tty_printf( PORTLIB, "???");
												break;
										}
										break;
								}
								break;

							case CFR_DECODE_TABLESWITCH:
								bcIndex = bytes + 4 - (pc % 4);
								NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
								branch = pc + index;
								NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
								low = (I_32)index;
								NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
								high = (I_32)index;
								j9tty_printf( PORTLIB, "default->%i", branch);
								j = high - low + 1;
								for(k = 0; k <= (I_32) j; k++)
								{
									NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
									branch = pc + index;
									j9tty_printf( PORTLIB, " %i->%i", k + low, branch);
								}
								break;

							case CFR_DECODE_LOOKUPSWITCH:
								bcIndex = bytes + 4 - (pc % 4);
								NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
								branch = pc + index;
								NEXT_U32_ENDIAN(bigEndian, npairs, bcIndex);
								j9tty_printf( PORTLIB, "default->%i", branch);
								for(j = 0; j < npairs; j++)
								{
									NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
									key = (I_32)index;
									NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
									branch = pc + index;
									j9tty_printf( PORTLIB, " %i->%i", key, branch);
								}
								break;
						}
						modifier = '\0';
						break;

					case 'a':
						/* ascii */
						if(formatString[i] == 'b' || formatString[i] == 'p')
						{
							modifier = ch;
						}
						else
						{
							j9tty_output_char('%');
							j9tty_output_char(ch);
						}
						break;

					case 'd':
						/* decimal */
					case 'x':
						/* hex, lowercase */
					case 'X':
						/* hex, uppercase */

						if(formatString[i] == 'i' || formatString[i] == 'B' || formatString[i] == 'b' || formatString[i] == 'p')
						{
							modifier = ch;
							break;
						}

					default:
						j9tty_output_char('%');
						j9tty_output_char(ch);
				}
			}
		}
		else
		{
			j9tty_output_char(ch);
		}
	}
	return;
}


static void sun_formatBytecodes(J9CfrClassFile* classfile, J9CfrMethod* method, BOOLEAN bigEndian, U_8* bytecodes, U_32 bytecodesLength, char *formatString, IDATA stringLength)
{
	J9CfrAttributeCode* code;
	U_8 *bcIndex, *tempIndex;
	I_32 pc, index;
	I_32 low, high;
	U_32 length, npairs, bcLength;
	U_8 bc;
	BOOLEAN wide;

	PORT_ACCESS_FROM_PORT(portLib);

	if(!(code = method->codeAttribute)) return;

	if(bytecodes)
	{
		bcIndex = bytecodes;
		length = bytecodesLength;
	}
	else
	{
		bytecodes = code->code;
		bcIndex = code->code;
		length = code->codeLength;
	}
	pc = 0;
	wide = FALSE;

	while((U_32)pc < length)
	{
			bc = *bcIndex;
			pc++;
			switch(bc)
			{
				case CFR_BC_bipush:
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 2, CFR_DECODE_I8, formatString, stringLength);
					pc++;
					bcIndex += 2;
					break;

				case CFR_BC_sipush:
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 3, CFR_DECODE_I16, formatString, stringLength);
					pc += 2;
					bcIndex += 3;
					break;

				case CFR_BC_ldc:
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 2, CFR_DECODE_CP8, formatString, stringLength);
					pc++;
					bcIndex += 2;
					break;

				case CFR_BC_ldc_w:
				case CFR_BC_ldc2_w:
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 3, CFR_DECODE_CP16, formatString, stringLength);
					pc += 2;
					bcIndex += 3;
					break;

				case CFR_BC_iload:
				case CFR_BC_lload:
				case CFR_BC_fload:
				case CFR_BC_dload:
				case CFR_BC_aload:
				case CFR_BC_istore:
				case CFR_BC_lstore:
				case CFR_BC_fstore:
				case CFR_BC_dstore:
				case CFR_BC_astore:
				case CFR_BC_ret:
					if(wide)
					{
						sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 4, CFR_DECODE_U16, formatString, stringLength);
						pc += 2;
						bcIndex += 3;
						wide = FALSE;
					}
					else
					{
						sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 2, CFR_DECODE_U8, formatString, stringLength);
						pc++;
						bcIndex += 2;
					}
 					break;

				case CFR_BC_iinc:
					if(wide)
					{
						sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 6, CFR_DECODE_U16_I16, formatString, stringLength);
						pc += 4;
						bcIndex += 5;
						wide = FALSE;
					}
					else
					{
						sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 3, CFR_DECODE_U8_I8, formatString, stringLength);
						pc += 2;
						bcIndex += 3;
					}
					break;

				case CFR_BC_ifeq:
				case CFR_BC_ifne:
				case CFR_BC_iflt:
				case CFR_BC_ifge:
				case CFR_BC_ifgt:
				case CFR_BC_ifle:
				case CFR_BC_if_icmpeq:
				case CFR_BC_if_icmpne:
				case CFR_BC_if_icmplt:
				case CFR_BC_if_icmpge:
				case CFR_BC_if_icmpgt:
				case CFR_BC_if_icmple:
				case CFR_BC_if_acmpeq:
				case CFR_BC_if_acmpne:
				case CFR_BC_goto:
				case CFR_BC_jsr:
				case CFR_BC_ifnull:
				case CFR_BC_ifnonnull:
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 3, CFR_DECODE_L16, formatString, stringLength);
					pc += 2;
					bcIndex += 3;
					break;

				case CFR_BC_tableswitch:
					tempIndex = bcIndex + 1;
					switch((pc - 1) % 4)
					{
						case 0:
							tempIndex++;
						case 1:
							tempIndex++;
						case 2:
							tempIndex++;
						case 3:
							break;
					}
					NEXT_U32_ENDIAN(bigEndian, index, tempIndex);
					NEXT_U32_ENDIAN(bigEndian, index, tempIndex);
					low = (I_32)index;
					NEXT_U32_ENDIAN(bigEndian, index, tempIndex);
					high = (I_32)index;
					bcLength = (U_32) (tempIndex - bcIndex) + ((high - low + 1) * 4);
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, bcLength, CFR_DECODE_TABLESWITCH, formatString, stringLength);
					bcIndex += bcLength;
					pc += bcLength - 1;
					break;

				case CFR_BC_lookupswitch:
					tempIndex = bcIndex + 1;
					switch((pc - 1) % 4)
					{
						case 0:
							tempIndex++;
						case 1:
							tempIndex++;
						case 2:
							tempIndex++;
						case 3:
							break;
					}
					NEXT_U32_ENDIAN(bigEndian, index, tempIndex);
					NEXT_U32_ENDIAN(bigEndian, npairs, tempIndex);
					bcLength = (U_32)(tempIndex - bcIndex) + (npairs * 8);
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, bcLength, CFR_DECODE_LOOKUPSWITCH, formatString, stringLength);
					bcIndex += bcLength;
					pc += bcLength - 1;
					break;

				case CFR_BC_getstatic:
				case CFR_BC_putstatic:
				case CFR_BC_getfield:
				case CFR_BC_putfield:
				case CFR_BC_invokevirtual:
				case CFR_BC_invokespecial:
				case CFR_BC_invokestatic:
				case CFR_BC_new:
				case CFR_BC_anewarray:
				case CFR_BC_checkcast:
				case CFR_BC_instanceof:
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 3, CFR_DECODE_CP16, formatString, stringLength);
					pc += 2;
					bcIndex += 3;
					break;

				case CFR_BC_invokeinterface:
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 3, CFR_DECODE_CP16, formatString, stringLength);
					pc += 4;
					bcIndex += 5;
					break;

				case CFR_BC_newarray:
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 2, CFR_DECODE_NEWARRAY, formatString, stringLength);
					pc++;
					bcIndex += 2;
					break;

				case CFR_BC_wide:
					wide = TRUE;
					break;

				case CFR_BC_multianewarray:
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 4, CFR_DECODE_MULTIANEWARRAY, formatString, stringLength);
					pc += 3;
					bcIndex += 4;
					break;

				case CFR_BC_goto_w:
				case CFR_BC_jsr_w:
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 5, CFR_DECODE_L32, formatString, stringLength);
					pc += 4;
					bcIndex += 5;
					break;

				default:
					sun_formatBytecode(classfile, method, bigEndian, bytecodes, bcIndex, bc, 1, CFR_DECODE_SIMPLE, formatString, stringLength);
					bcIndex++;
					break;
			}
			j9tty_printf(PORTLIB, "\n");
	}

	return;
}


/*	Print information about the class formatted as per @formatString.
	Format string:
		%<modifiers><specifier>
	Supported specifiers:
		p				package
		c				short class name
		C				qualified class name
		s				superclass name
		i				interfaces names
		m				modifiers
	Supported modifiers:
		%m
			a			ascii	 (default)
			d			decimal
			x			hex, lowercase
			X			hex, uppercase
*/
static void sun_formatClass(J9CfrClassFile* classfile, char *formatString, IDATA length)
{
	U_16 cpIndex;
	U_8* string;
	U_32 j, k;
	U_32 index;
	int i;
	char ch, ch2;
	char modifier;

	PORT_ACCESS_FROM_PORT(portLib);
	i = 0;
	modifier = '\0';
	while(i < length)
	{
		ch = formatString[i++];
		if(modifier || ch == '%')
		{
			if(ch == '%')
			{
				modifier = '\0';
				ch = formatString[i++];
			}
			if(ch == '\0' || ch == '%')
			{
				if(modifier) j9tty_printf(PORTLIB, "%%%c", modifier);
				j9tty_output_char(ch);
			}
			else
			{
				switch(ch)
				{
					case 'p':
						/* package */
						cpIndex = classfile->constantPool[classfile->thisClass].slot1;
						string = classfile->constantPool[cpIndex].bytes;
						j = 0;
						index = 0;
						while('\0' != (ch2 = string[j++])) if(ch2 == '/') index = j - 1;
						j = 0;
						while(j < index)
						{
							ch2 = string[j++];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'c':
						/* short class name */
						cpIndex = classfile->constantPool[classfile->thisClass].slot1;
						string = classfile->constantPool[cpIndex].bytes;
						j = 0;
						index = 0;
						while('\0' != (ch2 = string[j++])) if(ch2 == '/') index = j;
						j = index;
						while('\0' != (ch2 = string[j++])) j9tty_output_char(ch2);
						break;

					case 'C':
						/* qualified class name */
						cpIndex = classfile->constantPool[classfile->thisClass].slot1;
						string = classfile->constantPool[cpIndex].bytes;
						j = 0;
						while('\0' != (ch2 = string[j++]))
						{
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 's':
						/* superclass name */
						if(classfile->superClass)
						{
							cpIndex = classfile->constantPool[classfile->superClass].slot1;
							string = classfile->constantPool[cpIndex].bytes;
							j = 0;
							while('\0' != (ch2 = string[j++]))
							{
								if(ch2 == '/') j9tty_output_char('.');
								else j9tty_output_char(ch2);
							}
						}
						break;

					case 'i':
						/* interfaces names */
						for(j = 0; j < classfile->interfacesCount;)
						{
							cpIndex = classfile->interfaces[j];
							cpIndex = classfile->constantPool[cpIndex].slot1;
							string = classfile->constantPool[cpIndex].bytes;
							k = 0;
							while('\0' != (ch2 = string[k++]))
							{
								if(ch2 == '/') j9tty_output_char('.');
								else j9tty_output_char(ch2);
							}
							if(++j != classfile->interfacesCount) j9tty_output_char(',');
						}
						break;

					case 'm':
						/* modifiers */
						switch(modifier)
						{
							case 'd':
								j9tty_printf(PORTLIB, "%i ( ", classfile->accessFlags);
								printModifiers(PORTLIB, classfile->accessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_CLASS, J9_IS_CLASSFILE_VALUETYPE(classfile));
								j9tty_printf( PORTLIB, " )");
								break;

							case 'x':
								j9tty_printf(PORTLIB, "%08x ( ", classfile->accessFlags);
								printModifiers(PORTLIB, classfile->accessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_CLASS, J9_IS_CLASSFILE_VALUETYPE(classfile));
								j9tty_printf( PORTLIB, " )");
								break;

							case 'X':
								j9tty_printf(PORTLIB, "%08X ( ", classfile->accessFlags);
								printModifiers(PORTLIB, classfile->accessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_CLASS, J9_IS_CLASSFILE_VALUETYPE(classfile));
								j9tty_printf( PORTLIB, " )");
								break;

							case '\0':
							case 'a':
							default:
								printModifiers(PORTLIB, classfile->accessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_CLASS, J9_IS_CLASSFILE_VALUETYPE(classfile));
								break;
						}
						modifier = '\0';
						break;

					case 'a':
						/* ascii */
					case 'd':
						/* decimal */
					case 'x':
						/* hex, lowercase */
					case 'X':
						/* hex, uppercase */

						if(formatString[i] == 'm')
						{
							modifier = ch;
							break;
						}

					default:
						j9tty_output_char('%');
						j9tty_output_char(ch);
				}
			}
		}
		else
		{
			j9tty_output_char(ch);
		}
	}
	return;
}


/*	Print information about the field formatted as per @formatString.
	Format string:
		%<modifiers><specifier>
	Supported specifiers:
		c				short class name
		C				qualified class name
		n				name
		t				type
		s				signature
		i				inlined value
		m				modifiers
	Supported modifiers:
		%m
			a			ascii	 (default)
			d			decimal
			x			hex, lowercase
			X			hex, uppercase
*/
static void sun_formatField(J9CfrClassFile* classfile, J9CfrField* field, char *formatString, IDATA length)
{
	J9CfrConstantPoolInfo info;
	U_16 cpIndex;
	U_8* string;
	U_32 j;
	U_32 index;
	U_32 arity;
	int i;
	char ch, ch2;
	char modifier;

	PORT_ACCESS_FROM_PORT(portLib);
	i = 0;
	modifier = '\0';
	while(i < length)
	{
		ch = formatString[i++];
		if(modifier || ch == '%')
		{
			if(ch == '%')
			{
				modifier = '\0';
				ch = formatString[i++];
			}
			if(ch == '\0' || ch == '%')
			{
				if(modifier) j9tty_printf(PORTLIB, "%%%c", modifier);
				j9tty_output_char(ch);
			}
			else
			{
				switch(ch)
				{
					case 'c':
						/* short class name */
						cpIndex = classfile->constantPool[classfile->thisClass].slot1;
						string = classfile->constantPool[cpIndex].bytes;
						j = 0;
						index = 0;
						while('\0' != (ch2 = string[j++])) if(ch2 == '/') index = j;
						j = index;
						while('\0' != (ch2 = string[j++])) j9tty_output_char(ch2);
						break;

					case 'C':
						/* qualified class name */
						cpIndex = classfile->constantPool[classfile->thisClass].slot1;
						string = classfile->constantPool[cpIndex].bytes;
						j = 0;
						while('\0' != (ch2 = string[j++]))
						{
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'n':
						/* field name */
						string = classfile->constantPool[field->nameIndex].bytes;
						j9tty_printf(PORTLIB, "%s", string);
						break;

					case 's':
						/* type signature */
						string = classfile->constantPool[field->descriptorIndex].bytes;
						j = 0;
						while('\0' != (ch2 = string[j++]))
						{
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 't':
						/* qualified type name */
						string = classfile->constantPool[field->descriptorIndex].bytes;
						j = 0;
						arity = 0;
						while(string[j] == '[')
						{
							j++;
							arity++;
						}
						switch(string[j++])
						{
							case 'L':
								while((ch2 = string[j++]) != ';')
								{
									if(ch2 == '/') j9tty_output_char('.');
									else j9tty_output_char(ch2);
								}
								break;

							case 'B':
								j9tty_printf(PORTLIB, "byte");
								break;

							case 'C':
								j9tty_printf(PORTLIB, "char");
								break;

							case 'D':
								j9tty_printf(PORTLIB, "double");
								break;

							case 'F':
								j9tty_printf(PORTLIB, "float");
								break;

							case 'I':
								j9tty_printf(PORTLIB, "int");
								break;

							case 'J':
								j9tty_printf(PORTLIB, "long");
								break;

							case 'S':
								j9tty_printf(PORTLIB, "short");
								break;

							case 'Z':
								j9tty_printf(PORTLIB, "boolean");
								break;

							default:
								j9tty_printf(PORTLIB, "???");
								break;
						}
						for(j = 0; j < arity; j++) j9tty_printf(PORTLIB, "[]");
						break;

					case 'i':
						if(field->constantValueAttribute)
						{
							info = classfile->constantPool[field->constantValueAttribute->constantValueIndex];
							switch(info.tag)
							{
								case CFR_CONSTANT_Integer:
									j9tty_printf(PORTLIB, "=(int)0x%08X", info.slot1);
									break;
								case CFR_CONSTANT_Float:
									j9tty_printf(PORTLIB, "=(float)0x%08X", info.slot1);
									break;
								case CFR_CONSTANT_Long:
#ifdef J9VM_ENV_LITTLE_ENDIAN
									j9tty_printf(PORTLIB, "=(long)0x%08X%08X", info.slot2, info.slot1);
#else
									j9tty_printf(PORTLIB, "=(long)0x%08X%08X", info.slot1, info.slot2);
#endif
									break;
								case CFR_CONSTANT_Double:
#ifdef J9VM_ENV_LITTLE_ENDIAN
									j9tty_printf(PORTLIB, "=(double)0x%08X%08X", info.slot2, info.slot1);
#else
									j9tty_printf(PORTLIB, "=(double)0x%08X%08X", info.slot1, info.slot2);
#endif
									break;
								case CFR_CONSTANT_String:
									j9tty_printf(PORTLIB, "=\"%s\"", classfile->constantPool[info.slot1].bytes);
									break;
							}
						}
						break;

					case 'm':
						/* modifiers */
						switch(modifier)
						{
							case 'd':
								j9tty_printf(PORTLIB, "%i ( ", field->accessFlags);
								printModifiers(PORTLIB, field->accessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_FIELD, FALSE);
								j9tty_printf( PORTLIB, " )");
								break;

							case 'x':
								j9tty_printf(PORTLIB, "%08x ( ", field->accessFlags);
								printModifiers(PORTLIB, field->accessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_FIELD, FALSE);
								j9tty_printf( PORTLIB, " )");
								break;

							case 'X':
								j9tty_printf(PORTLIB, "%08X ( ", field->accessFlags);
								printModifiers(PORTLIB, field->accessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_FIELD, FALSE);
								j9tty_printf( PORTLIB, " )");
								break;

							case '\0':
							case 'a':
							default:
								printModifiers(PORTLIB, field->accessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_FIELD, FALSE);
								break;
						}
						modifier = '\0';
						break;

					case 'a':
						/* ascii */
					case 'd':
						/* decimal */
					case 'x':
						/* hex, lowercase */
					case 'X':
						/* hex, uppercase */

						if(formatString[i] == 'm')
						{
							modifier = ch;
							break;
						}

					default:
						j9tty_output_char('%');
						j9tty_output_char(ch);
				}
			}
		}
		else
		{
			j9tty_output_char(ch);
		}
	}
	return;
}


/*	Print information about the method formatted as per @formatString.
	Format string:
		%<modifiers><specifier>
	Supported specifiers:
		c				short class name
		C				qualified class name
		n				name
		r				return type
		p				parameter types
		s				signature
		e				thrown exception types
		m				modifiers
	Supported modifiers:
		%m
			a			ascii	 (default)
			d			decimal
			x			hex, lowercase
			X			hex, uppercase
*/
static void sun_formatMethod(J9CfrClassFile* classfile, J9CfrMethod* method, char *formatString, IDATA length)
{
	J9CfrAttributeExceptions* exceptions;
	U_8* string;
	U_16 cpIndex;
	U_32 j, k;
	U_32 index;
	U_32 arity;
	int i;
	char ch, ch2;
	char modifier;

	PORT_ACCESS_FROM_PORT(portLib);
	i = 0;
	modifier = '\0';
	while(i < length)
	{
		ch = formatString[i++];
		if(modifier || ch == '%')
		{
			if(ch == '%')
			{
				modifier = '\0';
				ch = formatString[i++];
			}
			if(ch == '\0' || ch == '%')
			{
				if(modifier) j9tty_printf(PORTLIB, "%%%c", modifier);
				j9tty_output_char(ch);
			}
			else
			{
				switch(ch)
				{
					case 'c':
						/* short class name */
						cpIndex = classfile->constantPool[classfile->thisClass].slot1;
						string = classfile->constantPool[cpIndex].bytes;
						j = 0;
						index = 0;
						while('\0' != (ch2 = string[j++])) if(ch2 == '/') index = j;
						j = index;
						while('\0' != (ch2 = string[j++])) j9tty_output_char(ch2);
						break;

					case 'C':
						/* qualified class name */
						cpIndex = classfile->constantPool[classfile->thisClass].slot1;
						string = classfile->constantPool[cpIndex].bytes;
						j = 0;
						while('\0' != (ch2 = string[j++]))
						{
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'n':
						/* method name */
						string = classfile->constantPool[method->nameIndex].bytes;
						j9tty_printf(PORTLIB, "%s", string);
						break;

					case 's':
						/* method signature */
						string = classfile->constantPool[method->descriptorIndex].bytes;
						j = 0;
						while('\0' != (ch2 = string[j++]))
						{
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'r':
						/* qualified return type name */
						string = classfile->constantPool[method->descriptorIndex].bytes;
						j = 0;
						arity = 0;
						while(string[j++] != ')');
						while(string[j] == '[')
						{
							j++;
							arity++;
						}
						switch(string[j++])
						{
							case 'L':
								while((ch2 = string[j++]) != ';')
								{
									if(ch2 == '/') j9tty_output_char('.');
									else j9tty_output_char(ch2);
								}
								break;

							case 'B':
								j9tty_printf(PORTLIB, "byte");
								break;

							case 'C':
								j9tty_printf(PORTLIB, "char");
								break;

							case 'D':
								j9tty_printf(PORTLIB, "double");
								break;

							case 'F':
								j9tty_printf(PORTLIB, "float");
								break;

							case 'I':
								j9tty_printf(PORTLIB, "int");
								break;

							case 'J':
								j9tty_printf(PORTLIB, "long");
								break;

							case 'S':
								j9tty_printf(PORTLIB, "short");
								break;

							case 'V':
								j9tty_printf(PORTLIB, "void");
								break;

							case 'Z':
								j9tty_printf(PORTLIB, "boolean");
								break;

							default:
								j9tty_printf(PORTLIB, "???");
								break;
						}
						for(j = 0; j < arity; j++) j9tty_printf(PORTLIB, "[]");
						break;

					case 'p':
						/* qualified parameter type names */
						string = classfile->constantPool[method->descriptorIndex].bytes;
						j = 1;
						while(string[j] != ')')
						{
							arity = 0;
							while(string[j] == '[')
							{
								j++;
								arity++;
							}
							switch(string[j++])
							{
								case 'L':
									while((ch2 = string[j++]) != ';')
									{
										if(ch2 == '/') j9tty_output_char('.');
										else j9tty_output_char(ch2);
									}
									break;

								case 'B':
									j9tty_printf(PORTLIB, "byte");
									break;

								case 'C':
									j9tty_printf(PORTLIB, "char");
									break;

								case 'D':
									j9tty_printf(PORTLIB, "double");
									break;

								case 'F':
									j9tty_printf(PORTLIB, "float");
									break;

								case 'I':
									j9tty_printf(PORTLIB, "int");
									break;

								case 'J':
									j9tty_printf(PORTLIB, "long");
									break;

								case 'S':
									j9tty_printf(PORTLIB, "short");
									break;

								case 'Z':
									j9tty_printf(PORTLIB, "boolean");
									break;

								default:
									j9tty_printf(PORTLIB, "???");
									break;
							}
							for(k = 0; k < arity; k++) j9tty_printf(PORTLIB, "[]");
							if(string[j] != ')') j9tty_output_char(',');
						}
						break;

					case 'e':
						/* qualified thrown exception type names */
						exceptions = method->exceptionsAttribute;
						if(exceptions == NULL) break;
						for(j = 0; j < exceptions->numberOfExceptions;)
						{
							index = classfile->constantPool[exceptions->exceptionIndexTable[j]].slot1;
							string = classfile->constantPool[index].bytes;
							k = 0;
							while('\0' != (ch2 = string[k++]))
							{
								if(ch2 == '/') j9tty_output_char('.');
								else j9tty_output_char(ch2);
							}
							if(++j != exceptions->numberOfExceptions) j9tty_output_char(',');
						}
						break;

					case 'm':
						/* modifiers */
						switch(modifier)
						{
							case 'd':
								j9tty_printf(PORTLIB, "%i ( ", method->accessFlags);
								printModifiers(PORTLIB, method->accessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_METHOD, FALSE);
								j9tty_printf( PORTLIB, " )");
								break;

							case 'x':
								j9tty_printf(PORTLIB, "%08x ( ", method->accessFlags);
								printModifiers(PORTLIB, method->accessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_METHOD, FALSE);
								j9tty_printf( PORTLIB, " )");
								break;

							case 'X':
								j9tty_printf(PORTLIB, "%08X ( ", method->accessFlags);
								printModifiers(PORTLIB, method->accessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_METHOD, FALSE);
								j9tty_printf( PORTLIB, " )");
								break;

							case '\0':
							case 'a':
							default:
								printModifiers(PORTLIB, method->accessFlags, ONLY_SPEC_MODIFIERS, MODIFIERSOURCE_METHOD, FALSE);
								break;
						}
						modifier = '\0';
						break;

					case 'a':
						/* ascii */
					case 'd':
						/* decimal */
					case 'x':
						/* hex, lowercase */
					case 'X':
						/* hex, uppercase */

						if(formatString[i] == 'm')
						{
							modifier = ch;
							break;
						}

					default:
						j9tty_output_char('%');
						j9tty_output_char(ch);
				}
			}
		}
		else
		{
			j9tty_output_char(ch);
		}
	}
	return;
}


/*	Print information about the class formatted as per @formatString.
	Format string:
		%<modifiers><specifier>
	Supported specifiers:
		p				package
		c				short class name
		C				qualified class name
		s				superclass name
		i				interfaces names
		m				sun modifiers
		M				j9  modifiers
	Supported modifiers:
		%m
			a			ascii	 (default)
			d			decimal
			x			hex, lowercase
			X			hex, uppercase
*/
static void j9_formatClass(J9ROMClass* romClass, char *formatString, IDATA length, U_32 flags)
{
	U_8* string;
	U_32 j, k;
	U_32 index;
	U_32 utfLength;
	int i;
	char ch, ch2;
	char modifier;

	PORT_ACCESS_FROM_PORT(portLib);
	i = 0;
	modifier = '\0';
	while(i < length)
	{
		ch = formatString[i++];
		if(modifier || ch == '%')
		{
			if(ch == '%')
			{
				modifier = '\0';
				ch = formatString[i++];
			}
			if(ch == '\0' || ch == '%')
			{
				if(modifier) j9tty_printf(PORTLIB, "%%%c", modifier);
				j9tty_output_char(ch);
			}
			else
			{
				switch(ch)
				{
					case 'p':
						/* package */
						utfLength = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass));
						string = ((U_8*) J9ROMCLASS_CLASSNAME(romClass)) + 2;
						index = 0;
						for(j = 0; j < utfLength; j++)
						{
							if(string[j] == '/') index = j;
						}
						for(j = 0; j < index; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'c':
						/* short class name */
						utfLength = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass));
						string = ((U_8*) J9ROMCLASS_CLASSNAME(romClass)) + 2;
						index = 0;
						for(j = 0; j < utfLength; j++)
						{
							if(string[j] == '/') index = j + 1;
						}
						for(j = index; j < utfLength; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'C':
						/* qualified class name */
						utfLength = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass));
						string = ((U_8*) J9ROMCLASS_CLASSNAME(romClass)) + 2;
						for(j = 0; j < utfLength; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 's':
						/* superclass name */
						utfLength = J9UTF8_LENGTH(J9ROMCLASS_SUPERCLASSNAME(romClass));
						string = ((U_8*) J9ROMCLASS_SUPERCLASSNAME(romClass)) + 2;
						for(j = 0; j < utfLength; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'i': {
						J9SRP * interfaces = J9ROMCLASS_INTERFACES(romClass);

						/* interfaces names */
						for(j = 0; j < romClass->interfaceCount;)
						{
							J9UTF8 * interfaceName = NNSRP_PTR_GET(interfaces, J9UTF8 *);

							utfLength = J9UTF8_LENGTH(interfaceName);
							string = ((U_8*)interfaceName) + 2;
							for(k = 0; k < utfLength; k++)
							{
								ch2 = string[k];
								if(ch2 == '/') j9tty_output_char('.');
								else j9tty_output_char(ch2);
							}
							if(++j != romClass->interfaceCount) j9tty_output_char(',');
							interfaces++;
						}
						break;
					}

					case 'm':
						/* modifiers */
						switch(modifier)
						{
							case 'd':
								j9tty_printf(PORTLIB, "%i", romClass->modifiers);
								break;

							case 'x':
								j9tty_printf(PORTLIB, "%08x", romClass->modifiers);
								break;

							case 'X':
								j9tty_printf(PORTLIB, "%08X", romClass->modifiers);
								break;

							case '\0':
							case 'a':
							default:
								printModifiers(PORTLIB, romClass->modifiers, INCLUDE_INTERNAL_MODIFIERS, MODIFIERSOURCE_CLASS, J9ROMCLASS_IS_VALUE(romClass));
								break;
						}
						modifier = '\0';
						break;

					case 'M':
						/* modifiers */
						switch(modifier)
						{
							case 'd':
								j9tty_printf(PORTLIB, "%i", romClass->extraModifiers);
								break;

							case 'x':
								j9tty_printf(PORTLIB, "%08x", romClass->extraModifiers);
								break;

							case 'X':
								j9tty_printf(PORTLIB, "%08X", romClass->extraModifiers);
								break;

							case '\0':
							case 'a':
							default:
								j9_printClassExtraModifiers(portLib, romClass->extraModifiers);
								break;
						}
						modifier = '\0';
						break;

					case 'a':
						/* ascii */
					case 'd':
						/* decimal */
					case 'x':
						/* hex, lowercase */
					case 'X':
						/* hex, uppercase */

						if(formatString[i] == 'm')
						{
							modifier = ch;
							break;
						}

					default:
						j9tty_output_char('%');
						j9tty_output_char(ch);
				}
			}
		}
		else
		{
			j9tty_output_char(ch);
		}
	}
	return;
}


/*	Print information about the field formatted as per @formatString.
	Format string:
		%<modifiers><specifier>
	Supported specifiers:
		c				short class name
		C				qualified class name
		n				name
		t				type
		s				signature
		i				inlined value
		m				modifiers
	Supported modifiers:
		%m
			a			ascii	 (default)
			d			decimal
			x			hex, lowercase
			X			hex, uppercase
*/
static void j9_formatField(J9ROMClass* romClass, J9ROMFieldShape* field, char *formatString, IDATA length, U_32 flags)
{
	J9ROMConstantPoolItem* info;
	U_8* string;
	U_16 utfLength;
	U_32 fieldType;
	U_32 j;
	U_32 index;
	U_32 arity;
	int i;
	char ch, ch2;
	char modifier;
	U_32 * initialValue = NULL;

	PORT_ACCESS_FROM_PORT(portLib);
	i = 0;
	modifier = '\0';
	while(i < length)
	{
		ch = formatString[i++];
		if(modifier || ch == '%')
		{
			if(ch == '%')
			{
				modifier = '\0';
				ch = formatString[i++];
			}
			if(ch == '\0' || ch == '%')
			{
				if(modifier) j9tty_printf(PORTLIB, "%%%c", modifier);
				j9tty_output_char(ch);
			}
			else
			{
				switch(ch)
				{
					case 'c':
						/* short class name */
						utfLength = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass));
						string = ((U_8*) J9ROMCLASS_CLASSNAME(romClass)) + 2;
						index = 0;
						for(j = 0; j < utfLength; j++)
						{
							if(string[j] == '/') index = j + 1;
						}
						for(j = index; j < utfLength; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'C':
						/* qualified class name */
						utfLength = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass));
						string = ((U_8*) J9ROMCLASS_CLASSNAME(romClass)) + 2;
						for(j = 0; j < utfLength; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'n':
						/* field name */
						utfLength = J9UTF8_LENGTH(J9ROMFIELDSHAPE_NAME(field));
						string = ((U_8*) J9ROMFIELDSHAPE_NAME(field)) + 2;
						for(j = 0; j < utfLength; j++) j9tty_output_char(string[j]);
						break;

					case 's':
						/* type signature */
						utfLength = J9UTF8_LENGTH(J9ROMFIELDSHAPE_SIGNATURE(field));
						string = ((U_8*) J9ROMFIELDSHAPE_SIGNATURE(field)) + 2;
						for(j = 0; j < utfLength; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 't':
						/* qualified type name */
						utfLength = J9UTF8_LENGTH(J9ROMFIELDSHAPE_SIGNATURE(field));
						string = ((U_8*) J9ROMFIELDSHAPE_SIGNATURE(field)) + 2;
						j = 0;
						arity = 0;
						while(string[j] == '[')
						{
							j++;
							arity++;
						}
						switch(string[j++])
						{
							case 'L':
								while((ch2 = string[j++]) != ';')
								{
									if(ch2 == '/') j9tty_output_char('.');
									else j9tty_output_char(ch2);
								}
								break;

							case 'B':
								j9tty_printf(PORTLIB, "byte");
								break;

							case 'C':
								j9tty_printf(PORTLIB, "char");
								break;

							case 'D':
								j9tty_printf(PORTLIB, "double");
								break;

							case 'F':
								j9tty_printf(PORTLIB, "float");
								break;

							case 'I':
								j9tty_printf(PORTLIB, "int");
								break;

							case 'J':
								j9tty_printf(PORTLIB, "long");
								break;

							case 'S':
								j9tty_printf(PORTLIB, "short");
								break;

							case 'Z':
								j9tty_printf(PORTLIB, "boolean");
								break;

							default:
								j9tty_printf(PORTLIB, "???");
								break;
						}
						for(j = 0; j < arity; j++) j9tty_printf(PORTLIB, "[]");
						break;

					case 'i':
						if(initialValue != NULL)
						{
							fieldType = field->modifiers & 0x3A0000; /* FieldTypeMask */
							switch(fieldType)
							{
								case 0: /* char */
									j9tty_printf(PORTLIB, "=(char) 0x%08X", *initialValue);
									break;

								case 0x020000:	/* Object, therefore java.lang.String */
									info = &((J9ROMConstantPoolItem *) (romClass + 1))[*initialValue];
									utfLength = J9UTF8_LENGTH(J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef*) info));
									string = ((U_8*) J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef*) info)) + 2;
									j9tty_printf(PORTLIB, "=(java.lang.String) \"%.*s\"", utfLength, string);
									break;

								case 0x080000: /* boolean */
									j9tty_printf(PORTLIB, "=(boolean) 0x%08X", *initialValue);
									break;

								case 0x100000: /* float */
									j9tty_printf(PORTLIB, "=(float) 0x%08X", *initialValue);
									break;

								case 0x180000: /* double */
#ifdef J9VM_ENV_LITTLE_ENDIAN
									j9tty_printf(PORTLIB, "=(double) 0x%08X%08X", initialValue[1], initialValue[0]);
#else
									j9tty_printf(PORTLIB, "=(double) 0x%08X%08X", initialValue[0], initialValue[1]);
#endif
									break;

								case 0x200000: /* byte */
									j9tty_printf(PORTLIB, "=(byte) 0x%08X", *initialValue);
									break;

								case 0x280000: /* short */
									j9tty_printf(PORTLIB, "=(short) 0x%08X", *initialValue);
									break;

								case 0x300000: /* int */
									j9tty_printf(PORTLIB, "=(int) 0x%08X", *initialValue);
									break;

								case 0x380000: /* long */
#ifdef J9VM_ENV_LITTLE_ENDIAN
									j9tty_printf(PORTLIB, "=(long) 0x%08X%08X", initialValue[1], initialValue[0]);
#else
									j9tty_printf(PORTLIB, "=(long) 0x%08X%08X", initialValue[0], initialValue[1]);
#endif
									break;
							}
						}
						break;

					case 'm':
						/* modifiers */
						switch(modifier)
						{
							case 'd':
								j9tty_printf(PORTLIB, "%i", field->modifiers);
								break;

							case 'x':
								j9tty_printf(PORTLIB, "%08x", field->modifiers);
								break;

							case 'X':
								j9tty_printf(PORTLIB, "%08X", field->modifiers);
								break;

							case '\0':
							case 'a':
							default:
								printModifiers(PORTLIB, field->modifiers, INCLUDE_INTERNAL_MODIFIERS, MODIFIERSOURCE_FIELD, FALSE);
								break;
						}
						modifier = '\0';
						break;

					case 'a':
						/* ascii */
					case 'd':
						/* decimal */
					case 'x':
						/* hex, lowercase */
					case 'X':
						/* hex, uppercase */

						if(formatString[i] == 'm')
						{
							modifier = ch;
							break;
						}

					default:
						j9tty_output_char('%');
						j9tty_output_char(ch);
				}
			}
		}
		else
		{
			j9tty_output_char(ch);
		}
	}
	return;
}


/*	Print information about the method formatted as per @formatString.
	Format string:
		%<modifiers><specifier>
	Supported specifiers:
		c				short class name
		C				qualified class name
		n				name
		r				return type
		p				parameter types
		s				signature
		m				modifiers
	Supported modifiers:
		%m
			a			ascii	 (default)
			d			decimal
			x			hex, lowercase
			X			hex, uppercase
*/
static void j9_formatMethod(J9ROMClass* romClass, J9ROMMethod* method, char *formatString, IDATA length, U_32 flags)
{
	J9SRP * currentThrowName;
	U_8* string;
	U_16 utfLength;
	U_32 j, k;
	U_32 index;
	U_32 arity;
	int i;
	char ch, ch2;
	char modifier;

	PORT_ACCESS_FROM_PORT(portLib);
	i = 0;
	modifier = '\0';
	while(i < length)
	{
		ch = formatString[i++];
		if(modifier || ch == '%')
		{
			if(ch == '%')
			{
				modifier = '\0';
				ch = formatString[i++];
			}
			if(ch == '\0' || ch == '%')
			{
				if(modifier) j9tty_printf(PORTLIB, "%%%c", modifier);
				j9tty_output_char(ch);
			}
			else
			{
				switch(ch)
				{
					case 'c':
						/* short class name */
						utfLength = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass));
						string = ((U_8*) J9ROMCLASS_CLASSNAME(romClass)) + 2;
						index = 0;
						for(j = 0; j < utfLength; j++)
						{
							if(string[j] == '/') index = j + 1;
						}
						for(j = index; j < utfLength; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'C':
						/* qualified class name */
						utfLength = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass));
						string = ((U_8*) J9ROMCLASS_CLASSNAME(romClass)) + 2;
						for(j = 0; j < utfLength; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'n':
						/* method name */
						utfLength = J9UTF8_LENGTH(J9ROMMETHOD_NAME(method));
						string = ((U_8*) J9ROMMETHOD_NAME(method)) + 2;
						for(j = 0; j < utfLength; j++) j9tty_output_char(string[j]);
						break;

					case 's':
						/* type signature */
						utfLength = J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(method));
						string = ((U_8*) J9ROMMETHOD_SIGNATURE(method)) + 2;
						for(j = 0; j < utfLength; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'r':
						/* qualified return type name */
						utfLength = J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(method));
						string = ((U_8*) J9ROMMETHOD_SIGNATURE(method)) + 2;
						j = 0;
						arity = 0;
						while(string[j++] != ')');
						while(string[j] == '[')
						{
							j++;
							arity++;
						}
						switch(string[j++])
						{
							case 'L':
								while((ch2 = string[j++]) != ';')
								{
									if(ch2 == '/') j9tty_output_char('.');
									else j9tty_output_char(ch2);
								}
								break;

							case 'B':
								j9tty_printf(PORTLIB, "byte");
								break;

							case 'C':
								j9tty_printf(PORTLIB, "char");
								break;

							case 'D':
								j9tty_printf(PORTLIB, "double");
								break;

							case 'F':
								j9tty_printf(PORTLIB, "float");
								break;

							case 'I':
								j9tty_printf(PORTLIB, "int");
								break;

							case 'J':
								j9tty_printf(PORTLIB, "long");
								break;

							case 'S':
								j9tty_printf(PORTLIB, "short");
								break;

							case 'V':
								j9tty_printf(PORTLIB, "void");
								break;

							case 'Z':
								j9tty_printf(PORTLIB, "boolean");
								break;

							default:
								j9tty_printf(PORTLIB, "???");
								break;
						}
						for(j = 0; j < arity; j++) j9tty_printf(PORTLIB, "[]");
						break;

					case 'p':
						/* qualified parameter type names */
						utfLength = J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(method));
						string = ((U_8*) J9ROMMETHOD_SIGNATURE(method)) + 2;
						j = 1;
						while(string[j] != ')')
						{
							arity = 0;
							while(string[j] == '[')
							{
								j++;
								arity++;
							}
							switch(string[j++])
							{
								case 'L':
									while((ch2 = string[j++]) != ';')
									{
										if(ch2 == '/') j9tty_output_char('.');
										else j9tty_output_char(ch2);
									}
									break;

								case 'B':
									j9tty_printf(PORTLIB, "byte");
									break;

								case 'C':
									j9tty_printf(PORTLIB, "char");
									break;

								case 'D':
									j9tty_printf(PORTLIB, "double");
									break;

								case 'F':
									j9tty_printf(PORTLIB, "float");
									break;

								case 'I':
									j9tty_printf(PORTLIB, "int");
									break;

								case 'J':
									j9tty_printf(PORTLIB, "long");
									break;

								case 'S':
									j9tty_printf(PORTLIB, "short");
									break;

								case 'Z':
									j9tty_printf(PORTLIB, "boolean");
									break;

								default:
									j9tty_printf(PORTLIB, "???");
									break;
							}
							for(k = 0; k < arity; k++) j9tty_printf(PORTLIB, "[]");
							if(string[j] != ')') j9tty_output_char(',');
						}
						break;

					case 'e': {
						J9ExceptionInfo * exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(method);
						/* qualified thrown exception type names */
						if(J9ROMMETHOD_HAS_EXCEPTION_INFO(method) && (exceptionData->throwCount))
						{
							currentThrowName = J9EXCEPTIONINFO_THROWNAMES(exceptionData);
							for (k=0; k < exceptionData->throwCount; )
							{
								J9UTF8 * currentName = NNSRP_PTR_GET(currentThrowName, J9UTF8 *);
								currentThrowName++;

								utfLength = J9UTF8_LENGTH(currentName);
								string = ((U_8*)currentName) + 2;
								for(j = 0; j < utfLength; j++)
								{
									ch2 = string[j];
									if(ch2 == '/') j9tty_output_char('.');
									else j9tty_output_char(ch2);
								}
								if(++k != exceptionData->throwCount) j9tty_output_char(',');
							}
						}
						break;
					}

					case 'm':
						/* modifiers */
						switch(modifier)
						{
							case 'd':
								j9tty_printf(PORTLIB, "%i", method->modifiers);
								break;

							case 'x':
								j9tty_printf(PORTLIB, "%08x", method->modifiers);
								break;

							case 'X':
								j9tty_printf(PORTLIB, "%08X", method->modifiers);
								break;

							case '\0':
							case 'a':
							default:
								printModifiers(PORTLIB, method->modifiers, INCLUDE_INTERNAL_MODIFIERS, MODIFIERSOURCE_METHOD, FALSE);
								break;
						}
						modifier = '\0';
						break;

					case 'a':
						/* ascii */
					case 'd':
						/* decimal */
					case 'x':
						/* hex, lowercase */
					case 'X':
						/* hex, uppercase */

						if(formatString[i] == 'm')
						{
							modifier = ch;
							break;
						}

					default:
						j9tty_output_char('%');
						j9tty_output_char(ch);
				}
			}
		}
		else
		{
			j9tty_output_char(ch);
		}
	}
	return;
}


/*	Print information about the bytecode formatted as per @formatString.
	Format string:
		%<modifiers><specifier>
	Supported specifiers:
		c				short class name
		C				qualified class name
		n				method name
		s				method signature
		i				instruction pointer
		b				bytecode
		B				complete bytecode
		p				parameters
	Supported modifiers:
		%i
			d			decimal (default)
			x			hex, lowercase
			X			hex, uppercase
		%B
			d			decimal
			x			hex, lowercase
			X			hex, uppercase (default)
		%b
			a			ascii (default)
			d			decimal
			x			hex, lowercase
			X			hex, uppercase
		%p
			a			ascii (default)
			d			decimal
			x			hex, lowercase
			X			hex, uppercase
*/
static void j9_formatBytecode(J9ROMClass* romClass, J9ROMMethod* method, U_8* bcStart, U_8* bytes, U_8 bc, U_32 bytesLength, U_32 decode, char *formatString, IDATA length, U_32 flags)
{
	J9ROMConstantPoolItem* constantPool;
	J9ROMConstantPoolItem *info, *info2;
	J9ROMNameAndSignature *nameAndSig;
	U_8* string;
	U_16 utfLength;
	U_8* bcIndex;
	UDATA pc, branch;
	U_8 u8, u8_2;
	U_16 u16, u16_2;
	U_32 u32;
	U_32 index = 0;
	U_32 npairs;
	U_32 j;
	I_32 low, high, key;
	I_32 i, k;
	BOOLEAN bigEndian;
	char modifier;
	char pcModifier;
	char ch, ch2;

	PORT_ACCESS_FROM_PORT( portLib );

	pcModifier = 'd';
	pc = bytes - bcStart;
	bigEndian = flags & BCT_BigEndianOutput;
	constantPool = (J9ROMConstantPoolItem*)((U_8*)romClass + sizeof(J9ROMClass));

	i = 0;
	modifier = '\0';
	while(i < length)
	{
		ch = formatString[i++];
		if(modifier || ch == '%')
		{
			if(ch == '%')
			{
				modifier = '\0';
				ch = formatString[i++];
			}
			if(ch == '\0' || ch == '%')
			{
				if(modifier) j9tty_printf(PORTLIB, "%%%c", modifier);
				j9tty_output_char(ch);
			}
			else
			{
				switch(ch)
				{
					case 'c':
						/* short class name */
						utfLength = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass));
						string = ((U_8*) J9ROMCLASS_CLASSNAME(romClass)) + 2;
						index = 0;
						for(j = 0; j < utfLength; j++)
						{
							if(string[j] == '/') index = j + 1;
						}
						for(j = index; j < utfLength; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'C':
						/* qualified class name */
						utfLength = J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass));
						string = ((U_8*) J9ROMCLASS_CLASSNAME(romClass)) + 2;
						for(j = 0; j < utfLength; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'n':
						/* method name */
						utfLength = J9UTF8_LENGTH(J9ROMMETHOD_NAME(method));
						string = ((U_8*) J9ROMMETHOD_NAME(method)) + 2;
						for(j = 0; j < utfLength; j++) j9tty_output_char(string[j]);
						break;

					case 's':
						/* type signature */
						utfLength = J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(method));
						string = ((U_8*) J9ROMMETHOD_SIGNATURE(method)) + 2;
						for(j = 0; j < utfLength; j++)
						{
							ch2 = string[j];
							if(ch2 == '/') j9tty_output_char('.');
							else j9tty_output_char(ch2);
						}
						break;

					case 'i':
						/* instruction pointer */
						pcModifier = modifier;
						switch(modifier)
						{
							case 'x':
								j9tty_printf(PORTLIB, "%08x", pc);
								break;

							case 'X':
								j9tty_printf(PORTLIB, "%08X", pc);
								break;

							case 'd':
							default:
								j9tty_printf(PORTLIB, "%i", pc);
								break;
						}
						modifier = '\0';
						break;

					case 'b':
						/* bytecode */
						switch(modifier)
						{
							case 'x':
								j9tty_printf(PORTLIB, "%02x", bc);
								break;

							case 'X':
								j9tty_printf(PORTLIB, "%02X", bc);
								break;

							case 'd':
								j9tty_printf(PORTLIB, "%i", bc);
								break;

							default:
								j9tty_printf(PORTLIB, "%s", JavaBCNames[bc]);
								break;
						}
						modifier = '\0';
						break;

					case 'B':
						/* bytecode bytes */
						switch(modifier)
						{
							case 'd':
								for(j = 0; j < bytesLength; )
								{
									j9tty_printf(PORTLIB, "%i", bytes[j]);
									if(++j < bytesLength) j9tty_output_char(' ');
								}
								break;

							case 'x':
								for(j = 0; j < bytesLength; j++) j9tty_printf(PORTLIB, "%02x", bytes[j]);
								break;

							default:
							case 'X':
								for(j = 0; j < bytesLength; j++) j9tty_printf(PORTLIB, "%02X", bytes[j]);
								break;
						}
						modifier = '\0';
						break;

					case 'p':
						/* parameters */
						bcIndex = bytes + 1;
						if(bc != *bytes) bcIndex++;
						switch(decode)
						{
							case CFR_DECODE_SIMPLE:
								break;

							case CFR_DECODE_I8:
								NEXT_U8_ENDIAN(bigEndian, u8, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%02x", (I_8)u8);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%02X", (I_8)u8);
										break;

									case 'd':
									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i", (I_8)u8);
										break;
								}
								break;

							case CFR_DECODE_U8:
								NEXT_U8_ENDIAN(bigEndian, u8, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%02x", u8);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%02X", u8);
										break;

									case 'd':
									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i", u8);
										break;
								}
								break;

							case CFR_DECODE_I16:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x", (I_16)u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X", (I_16)u16);
										break;

									case 'd':
									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i", (I_16)u16);
										break;
								}
								break;

							case CFR_DECODE_U16:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x", (I_16)u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X", (I_16)u16);
										break;

									case 'd':
									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i", (I_16)u16);
										break;
								}
								break;

							case CFR_DECODE_U8_I8:
								NEXT_U8_ENDIAN(bigEndian, u8, bcIndex);
								NEXT_U8_ENDIAN(bigEndian, u8_2, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%02x %02x", u8, (I_8)u8_2);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%02X %02X", u8, (I_8)u8_2);
										break;

									case 'd':
									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i %i", u8, (I_8)u8_2);
										break;
								}
								break;

							case CFR_DECODE_U16_I16:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								NEXT_U16_ENDIAN(bigEndian, u16_2, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x %04x", u16, (I_16)u16_2);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X %04X", u16, (I_16)u16_2);
										break;

									case 'd':
									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i %i", u16, (I_16)u16_2);
										break;
								}
								break;

							case CFR_DECODE_J9_CLASSREF:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x", u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X", u16);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", u16);
										break;

									case 'a':
									default:
										info = &constantPool[u16];
classrefAscii:
										utfLength = J9UTF8_LENGTH(J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info));
										string = ((U_8*) (J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info))) + 2;
										for(j = 0; j < utfLength; j++)
										{
											ch2 = string[j];
											if(ch2 == '/') j9tty_output_char('.');
											else j9tty_output_char(ch2);
										}
										break;
								}
								break;

							case CFR_DECODE_J9_FIELDREF:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x", u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X", u16);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", u16);
										break;

									case 'a':
									default:
										info = &constantPool[u16];
										utfLength = J9UTF8_LENGTH(J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[((J9ROMFieldRef *) info)->classRefCPIndex]));
										string = ((U_8*) J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[((J9ROMFieldRef *) info)->classRefCPIndex])) + 2;
										for(j = 0; j < utfLength; j++)
										{
											ch2 = string[j];
											if(ch2 == '/') j9tty_output_char('.');
											else j9tty_output_char(ch2);
										}
										j9tty_output_char('.');
										nameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *) info);
										utfLength = J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig));
										string = ((U_8*) J9ROMNAMEANDSIGNATURE_NAME(nameAndSig)) + 2;
										for(j = 0; j < utfLength; j++) j9tty_output_char(string[j]);
										j9tty_output_char(' ');
										utfLength = J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig));
										string = ((U_8*) J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig)) + 2;
										for(j = 0; j < utfLength; j++) j9tty_output_char(string[j]);
										break;
								}
								break;

							case CFR_DECODE_J9_METHODTYPEREF:
								/* fall through: A J9RAMMethodTypeRef has a J9ROMMethodRef
								 * as its ROM Constant pool equivalent
								 */
							case CFR_DECODE_J9_METHODREF:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x", u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X", u16);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", u16);
										break;

									case 'a':
									default:
										info = &constantPool[u16];
										if (JBinvokestaticsplit == bc) {
											U_16 cpIndex = *(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(romClass) + u16);
											info = &constantPool[cpIndex];
										} else if (JBinvokespecialsplit == bc) {
											U_16 cpIndex = *(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(romClass) + u16);
											info = &constantPool[cpIndex];
										}
										info2 = &constantPool[((J9ROMMethodRef *) info)->classRefCPIndex];
										utfLength = J9UTF8_LENGTH(J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info2));
										string = ((U_8*) J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info2)) + 2;
										for(j = 0; j < utfLength; j++)
										{
											ch2 = string[j];
											if(ch2 == '/') j9tty_output_char('.');
											else j9tty_output_char(ch2);
										}
										j9tty_output_char('.');
										nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info);
										utfLength = J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_NAME(nameAndSig));
										string = ((U_8*) J9ROMNAMEANDSIGNATURE_NAME(nameAndSig)) + 2;
										for(j = 0; j < utfLength; j++) j9tty_output_char(string[j]);
										utfLength = J9UTF8_LENGTH(J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig));
										string = ((U_8*) J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig)) + 2;
										for(j = 0; j < utfLength; j++) j9tty_output_char(string[j]);
										break;
								}
								break;

							case CFR_DECODE_J9_LDC:
								NEXT_U8_ENDIAN(bigEndian, u8, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%02x", u8);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%02X", u8);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", u8);
										break;

									case 'a':
									default:
										info = &constantPool[u8];
ldcAscii:
										switch (J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(romClass), index)) {
											case J9CPTYPE_CLASS:
												utfLength = J9UTF8_LENGTH(J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info));
												string = ((U_8*) (J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info))) + 2;
												for(j = 0; j < utfLength; j++)
												{
													ch2 = string[j];
													if(ch2 == '/') j9tty_output_char('.');
													else j9tty_output_char(ch2);
												}
												break;

											case J9CPTYPE_STRING:
											case J9CPTYPE_ANNOTATION_UTF8:
												utfLength = J9UTF8_LENGTH(J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info));
												string = ((U_8*) J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info)) + 2;
												j9tty_printf(PORTLIB, "(java.lang.String) \"%.*s\"", utfLength, string);
												break;

											case J9CPTYPE_INT:
												j9tty_printf(PORTLIB, "(int) 0x%08X", ((J9ROMSingleSlotConstantRef *) info)->data);
												break;

											case J9CPTYPE_FLOAT:
												j9tty_printf(PORTLIB, "(float) 0x%08X", ((J9ROMSingleSlotConstantRef *) info)->data);
												break;

											default:
												j9tty_printf(PORTLIB, "unknown");
												break;
										}
										break;
								}
								break;

							case CFR_DECODE_J9_LDCW:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x", u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X", u16);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", u16);
										break;

									case 'a':
									default:
										info = &constantPool[u16];
										goto ldcAscii;
										break;
								}
								break;

							case CFR_DECODE_J9_LDC2DW:
							case CFR_DECODE_J9_LDC2LW:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x", u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X", u16);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", u16);
										break;

									case 'a':
									default:
										info = &constantPool[u16];
										if(decode == CFR_DECODE_J9_LDC2DW)
										{
											/* double */
#ifdef J9VM_ENV_LITTLE_ENDIAN
											j9tty_printf(PORTLIB, "(double) 0x%08X%08X", info->slot2, info->slot1);
#else
											j9tty_printf(PORTLIB, "(double) 0x%08X%08X", info->slot1, info->slot2);
#endif
										}
										else
										{
#ifdef J9VM_ENV_LITTLE_ENDIAN
											j9tty_printf(PORTLIB, "(long) 0x%08X%08X", info->slot2, info->slot1);
#else
											j9tty_printf(PORTLIB, "(long) 0x%08X%08X", info->slot1, info->slot2);
#endif
										}
										break;
								}
								break;

							case CFR_DECODE_MULTIANEWARRAY:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								NEXT_U8_ENDIAN(bigEndian, u8, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%02x %04x", u8, u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%02X %04X", u8, u16);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i %i", u8, u16);
										break;

									case 'a':
									default:
										j9tty_printf(PORTLIB, "%i ", u8);
										info = &constantPool[u16];
										goto classrefAscii;
										break;
								}
								break;

							case CFR_DECODE_L16:
								NEXT_U16_ENDIAN(bigEndian, u16, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%04x", (I_16)u16);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%04X", (I_16)u16);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", (I_16)u16);
										break;

									case 'a':
									default:
										branch = pc + (I_16)u16;
										switch(pcModifier)
										{
											case 'x':
												j9tty_printf(PORTLIB, "%08x", branch);
												break;

											case 'X':
												j9tty_printf(PORTLIB, "%08X", branch);
												break;

											case 'd':
												j9tty_printf(PORTLIB, "%i", branch);
												break;
										}
										break;
								}
								break;

							case CFR_DECODE_L32:
								NEXT_U32_ENDIAN(bigEndian, u32, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%08x", (I_32)u32);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%08X", (I_32)u32);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", (I_32)u32);
										break;

									case 'a':
									default:
										branch = pc + (I_32)u32;
										switch(pcModifier)
										{
											case 'x':
												j9tty_printf(PORTLIB, "%08x", branch);
												break;

											case 'X':
												j9tty_printf(PORTLIB, "%08X", branch);
												break;

											case 'd':
												j9tty_printf(PORTLIB, "%i", branch);
												break;
										}
										break;
								}
								break;

							case CFR_DECODE_NEWARRAY:
								NEXT_U8_ENDIAN(bigEndian, u8, bcIndex);
								switch(modifier)
								{
									case 'x':
										j9tty_printf(PORTLIB, "%02x", u8);
										break;

									case 'X':
										j9tty_printf(PORTLIB, "%02X", u8);
										break;

									case 'd':
										j9tty_printf(PORTLIB, "%i", u8);
										break;

									case 'a':
									default:
										switch(u8)
										{
											case /*T_BOOLEAN*/ 4 :
												j9tty_printf( PORTLIB, "boolean");
												break;
											case /*T_CHAR*/  5:
												j9tty_printf( PORTLIB, "char");
												break;
											case /*T_FLOAT*/  6:
												j9tty_printf( PORTLIB, "float");
												break;
											case /*T_DOUBLE*/  7:
												j9tty_printf( PORTLIB, "double");
												break;
											case /*T_BYTE*/  8:
												j9tty_printf( PORTLIB, "byte");
												break;
											case /*T_SHORT*/  9:
												j9tty_printf( PORTLIB, "short");
												break;
											case /*T_INT*/  10:
												j9tty_printf( PORTLIB, "int");
												break;
											case /*T_LONG*/  11:
												j9tty_printf( PORTLIB, "long");
												break;
											default:
												j9tty_printf( PORTLIB, "???");
												break;
										}
										break;
								}
								break;

							case CFR_DECODE_TABLESWITCH:
								bcIndex = bytes + 4 - (pc % 4);
								NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
								branch = pc + index;
								NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
								low = (I_32)index;
								NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
								high = (I_32)index;
								j9tty_printf( PORTLIB, "default->%i", branch);
								j = high - low + 1;
								for(k = 0; k <= (I_32) j; k++)
								{
									NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
									branch = pc + index;
									j9tty_printf( PORTLIB, " %i->%i", k + low, branch);
								}
								break;

							case CFR_DECODE_LOOKUPSWITCH:
								bcIndex = bytes + 4 - (pc % 4);
								NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
								branch = pc + index;
								NEXT_U32_ENDIAN(bigEndian, npairs, bcIndex);
								j9tty_printf( PORTLIB, "default->%i", branch);
								for(j = 0; j < npairs; j++)
								{
									NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
									key = (I_32)index;
									NEXT_U32_ENDIAN(bigEndian, index, bcIndex);
									branch = pc + index;
									j9tty_printf( PORTLIB, " %i->%i", key, branch);
								}
								break;
						}
						modifier = '\0';
						break;

					case 'a':
						/* ascii */
						if(formatString[i] == 'b' || formatString[i] == 'p')
						{
							modifier = ch;
						}
						else
						{
							j9tty_output_char('%');
							j9tty_output_char(ch);
						}
						break;

					case 'd':
						/* decimal */
					case 'x':
						/* hex, lowercase */
					case 'X':
						/* hex, uppercase */

						if(formatString[i] == 'i' || formatString[i] == 'B' || formatString[i] == 'b' || formatString[i] == 'p')
						{
							modifier = ch;
							break;
						}

					default:
						j9tty_output_char('%');
						j9tty_output_char(ch);
				}
			}
		}
		else
		{
			j9tty_output_char(ch);
		}
	}
	return;
}


static void j9_formatBytecodes(J9ROMClass* romClass, J9ROMMethod* method, U_8* bytecodes, U_32 bytecodesLength, char *formatString, IDATA stringLength, U_32 flags)
{
	U_8 *bcIndex, *tempIndex;
	I_32 pc, index;
	I_32 low, high;
	UDATA length;
	U_32 npairs, bcLength;
	U_8 bc;
	BOOLEAN bigEndian;

	PORT_ACCESS_FROM_PORT(portLib);

	bigEndian = flags & BCT_BigEndianOutput;

	if((method->modifiers & CFR_ACC_NATIVE) || (method->modifiers & CFR_ACC_ABSTRACT)) return;
	if(bytecodes)
	{
		length = bytecodesLength;
		if(length == 0) return;
		bcIndex = bytecodes;
		pc = 0;
	}
	else
	{
		length = J9_BYTECODE_SIZE_FROM_ROM_METHOD(method);
		if(length == 0) return;
		bytecodes = J9_BYTECODE_START_FROM_ROM_METHOD(method);
		bcIndex = bytecodes;
		pc = 0;
	}

	while((U_32)pc < length)
	{
			bc = *bcIndex;
			pc++;
			switch(bc)
			{
				case JBbipush:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 2, CFR_DECODE_I8, formatString, stringLength, flags);
					pc++;
					bcIndex += 2;
					break;

				case JBsipush:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 3, CFR_DECODE_I16, formatString, stringLength, flags);
					pc += 2;
					bcIndex += 3;
					break;

				case JBldc:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 2, CFR_DECODE_J9_LDC, formatString, stringLength, flags);
					pc++;
					bcIndex += 2;
					break;

				case JBldcw:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 3, CFR_DECODE_J9_LDCW, formatString, stringLength, flags);
					pc += 2;
					bcIndex += 3;
					break;

				case JBldc2dw:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 3, CFR_DECODE_J9_LDC2DW, formatString, stringLength, flags);
					pc += 2;
					bcIndex += 3;
					break;

				case JBldc2lw:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 3, CFR_DECODE_J9_LDC2LW, formatString, stringLength, flags);
					pc += 2;
					bcIndex += 3;
					break;

				case JBiload:
				case JBlload:
				case JBfload:
				case JBdload:
				case JBaload:
				case JBistore:
				case JBlstore:
				case JBfstore:
				case JBdstore:
				case JBastore:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 2, CFR_DECODE_U8, formatString, stringLength, flags);
					pc++;
					bcIndex += 2;
					break;

				case JBiloadw:
				case JBlloadw:
				case JBfloadw:
				case JBdloadw:
				case JBaloadw:
				case JBistorew:
				case JBlstorew:
				case JBfstorew:
				case JBdstorew:
				case JBastorew:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 4, CFR_DECODE_U16, formatString, stringLength, flags);
					pc += 2;
					bcIndex += 3;
 					break;

				case JBiinc:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 3, CFR_DECODE_U8_I8, formatString, stringLength, flags);
					pc += 2;
					bcIndex += 3;
					break;

				case JBiincw:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 6, CFR_DECODE_U16_I16, formatString, stringLength, flags);
					pc += 4;
					bcIndex += 5;
					break;

				case JBifeq:
				case JBifne:
				case JBiflt:
				case JBifge:
				case JBifgt:
				case JBifle:
				case JBificmpeq:
				case JBificmpne:
				case JBificmplt:
				case JBificmpge:
				case JBificmpgt:
				case JBificmple:
				case JBifacmpeq:
				case JBifacmpne:
				case JBgoto:
				case JBifnull:
				case JBifnonnull:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 3, CFR_DECODE_L16, formatString, stringLength, flags);
					pc += 2;
					bcIndex += 3;
					break;

				case JBtableswitch:
					tempIndex = bcIndex + 1;
					switch((pc - 1) % 4)
					{
						case 0:
							tempIndex++;
						case 1:
							tempIndex++;
						case 2:
							tempIndex++;
						case 3:
							break;
					}
					NEXT_U32_ENDIAN(bigEndian, index, tempIndex);
					NEXT_U32_ENDIAN(bigEndian, index, tempIndex);
					low = (I_32)index;
					NEXT_U32_ENDIAN(bigEndian, index, tempIndex);
					high = (I_32)index;
					bcLength = (U_32) (tempIndex - bcIndex) + ((high - low + 1) * 4);
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, bcLength, CFR_DECODE_TABLESWITCH, formatString, stringLength, flags);
					bcIndex += bcLength;
					pc += bcLength - 1;
					break;

				case JBlookupswitch:
					tempIndex = bcIndex + 1;
					switch((pc - 1) % 4)
					{
						case 0:
							tempIndex++;
						case 1:
							tempIndex++;
						case 2:
							tempIndex++;
						case 3:
							break;
					}
					NEXT_U32_ENDIAN(bigEndian, index, tempIndex);
					NEXT_U32_ENDIAN(bigEndian, npairs, tempIndex);
					bcLength = (U_32)(tempIndex - bcIndex) + (npairs * 8);
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, bcLength, CFR_DECODE_LOOKUPSWITCH, formatString, stringLength, flags);
					bcIndex += bcLength;
					pc += bcLength - 1;
					break;

				case JBgetstatic:
				case JBputstatic:
				case JBgetfield:
				case JBputfield:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 3, CFR_DECODE_J9_FIELDREF, formatString, stringLength, flags);
					pc += 2;
					bcIndex += 3;
					break;

				case JBinvokevirtual:
				case JBinvokespecial:
				case JBinvokestatic:
				case JBinvokeinterface:
				case JBinvokehandle:
				case JBinvokehandlegeneric:
				case JBinvokestaticsplit:
				case JBinvokespecialsplit:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 3, CFR_DECODE_J9_METHODREF, formatString, stringLength, flags);
					pc += 2;
					bcIndex += 3;
					break;

				case JBnew:
				case JBanewarray:
				case JBcheckcast:
				case JBinstanceof:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 3, CFR_DECODE_J9_CLASSREF, formatString, stringLength, flags);
					pc += 2;
					bcIndex += 3;
					break;

				case JBnewarray:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 2, CFR_DECODE_NEWARRAY, formatString, stringLength, flags);
					pc++;
					bcIndex += 2;
					break;

				case JBmultianewarray:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 4, CFR_DECODE_MULTIANEWARRAY, formatString, stringLength, flags);
					pc += 3;
					bcIndex += 4;
					break;

				case JBgotow:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 5, CFR_DECODE_L32, formatString, stringLength, flags);
					pc += 4;
					bcIndex += 5;
					break;

				default:
					j9_formatBytecode(romClass, method, bytecodes, bcIndex, bc, 1, CFR_DECODE_SIMPLE, formatString, stringLength, flags);
					bcIndex++;
					break;
			}
			j9tty_printf(PORTLIB, "\n");
	}

	return;
}


static I_32
processDirectory(char *dir, BOOLEAN recursive, U_32 flags)
{
	char *pathBuffer = NULL;
	char *resultBuffer = NULL;
	UDATA handle = 0;
	IDATA result = 0;
	IDATA length = 0;

	PORT_ACCESS_FROM_PORT(portLib);

	pathBuffer = j9mem_allocate_memory(EsMaxPath, J9MEM_CATEGORY_CLASSES);
	if (NULL == pathBuffer) {
		return RET_ALLOCATE_FAILED;
	}

	resultBuffer = j9mem_allocate_memory(EsMaxPath, J9MEM_CATEGORY_CLASSES);
	if (NULL == resultBuffer) {
		j9mem_free_memory(pathBuffer);
		return RET_ALLOCATE_FAILED;
	}

	length = strlen(dir);
	if (length >= EsMaxPath) {
		j9tty_printf(PORTLIB, "Could not open directory %s (path too long)\n", dir);
		goto cleanup;
	}
	memcpy(resultBuffer, dir, length + 1);

	handle = j9file_findfirst(dir, pathBuffer);
	if (~(UDATA)0 == handle) {
		j9tty_printf(PORTLIB, "Could not open directory %s\n", dir);
		goto cleanup;
	}

	result = handle;
	while (-1 != result) {
		/* ignore "." and ".." */
		if ((0 != strcmp(pathBuffer, ".")) && (0 != strcmp(pathBuffer, ".."))) {
			IDATA length2 = length + strlen(pathBuffer);
			if (length2 + 1 >= EsMaxPath) {
				j9tty_printf(PORTLIB, "Could not open %s%s (path too long)\n", dir, pathBuffer);
				break;
			}
			strcpy(resultBuffer + length, pathBuffer);

			if (recursive && (j9file_attr(resultBuffer) == EsIsDir)) {
				resultBuffer[length2] = PATH_SEP_CHAR;
				resultBuffer[length2 + 1] = '\0';
				processDirectory(resultBuffer, TRUE, flags);
			} else {
				if ((length2 > 6) && (0 == strcmp(resultBuffer + length2 - 6, ".class"))) {
					processSingleFile(resultBuffer, flags);
				}
			}
		}
		result = j9file_findnext(handle, pathBuffer);
	}
	j9file_findclose(handle);
cleanup:
	j9mem_free_memory(pathBuffer);
	j9mem_free_memory(resultBuffer);
	return 0;
}


static I_32 processSingleFile(char* requestedFile, U_32 flags)
{
	J9CfrClassFile* classfile;
	J9ROMClass* romClass;
	U_8* data;
	U_32 dataLength;
	int result;

	PORT_ACCESS_FROM_PORT(portLib);

	if(options.options  & OPTION_translate)
	{
		data = loadBytes(requestedFile, &dataLength, flags);
		if(data == NULL) return RET_FILE_LOAD_FAILED;
		romClass = translateClassBytes(data, dataLength, requestedFile, flags);
		if(romClass ==  NULL)
		{
			j9mem_free_memory(data);
			return RET_TRANSLATE_FAILED;
		}
			result = processROMClass(romClass, requestedFile, flags);
		j9mem_free_memory(data);
		j9mem_free_memory(romClass);
		if(result) return result;
	}
	else
	{
		classfile = loadClassFile(requestedFile, &dataLength, flags);
		if(classfile == NULL) return RET_FILE_LOAD_FAILED;
		result = processClassFile(classfile, dataLength, requestedFile, flags);
		j9mem_free_memory((U_8*)classfile);
		if(result) return result;
	}
	return 0;
}

static void
reportClassLoadError(J9CfrError* error, char* requestedFile)
{
	J9CfrConstantPoolInfo *name, *sig;
	PORT_ACCESS_FROM_PORT(portLib);
	const char* errorDescription;

	errorDescription = getJ9CfrErrorDescription(PORTLIB, error);

	if(requestedFile) {
		j9tty_printf( PORTLIB, "\nInvalid class file: %s\n", requestedFile);
	} else {
		j9tty_printf( PORTLIB, "\nInvalid class file\n");
	}

	j9tty_printf( PORTLIB, "Recommended action: ");
	switch(error->errorAction) {
		case CFR_NoAction:
			j9tty_printf( PORTLIB, "no action\n");
			break;

		case CFR_ThrowNoClassDefFoundError:
			j9tty_printf( PORTLIB, "throw java.lang.NoClassDefFoundError\n");
			break;

		case CFR_ThrowClassFormatError:
			j9tty_printf( PORTLIB, "throw java.lang.ClassFormatError\n");
			break;

		case CFR_ThrowVerifyError:
			j9tty_printf( PORTLIB, "throw java.lang.VerifyError\n");
			break;

		case CFR_ThrowUnsupportedClassVersionError:
			j9tty_printf( PORTLIB, "throw java.lang.UnsupportedClassVersionError\n");
			break;

		case CFR_ThrowOutOfMemoryError:
			j9tty_printf( PORTLIB, "throw java.lang.OutOfMemoryError\n");
			break;

		default:
			j9tty_printf( PORTLIB, "<unknown action>\n");
	}

	if(error->errorMethod == -1) {
		j9tty_printf( PORTLIB, "Error: %s at %i\n", errorDescription, error->errorOffset);
	} else {
		name = &(error->constantPool)[error->errorMember->nameIndex];
		sig = &(error->constantPool)[error->errorMember->descriptorIndex];
		j9tty_printf( PORTLIB, "Verification error in method %i (%.*s%.*s) at PC %i\n", error->errorMethod, name->slot1, name->bytes, sig->slot1, sig->bytes, error->errorPC);
		j9tty_printf( PORTLIB, "Error: %s\n", errorDescription);
	}
	return;
}

/* when we removed j9tty_output_char, most of the senders were here in cfdump.
 * This function replaces the portLibrary one
 */
static void tty_output_char(J9PortLibrary *portLib, const char c) {
	PORT_ACCESS_FROM_PORT(portLib);

	j9tty_printf(PORTLIB, "%c", c);
}


UDATA signalProtectedMain(struct J9PortLibrary *portLibrary, void *arg)
{
	struct j9cmdlineOptions *startupOptions = (struct j9cmdlineOptions *) arg;
	int argc = startupOptions->argc;
	char **argv = startupOptions->argv;
	char **envp = startupOptions->envp;
	U_32 flags;
	I_32 result;

	PORT_ACCESS_FROM_PORT(startupOptions->portLibrary);
	portLib = startupOptions->portLibrary;

	main_setNLSCatalog(PORTLIB, argv);

	/* Initialize the thread library -- Zip code now uses monitors. */
	omrthread_attach_ex(NULL, J9THREAD_ATTR_DEFAULT);

	result = initZipLibrary(portLib, NULL);
	if(result) return result;

	result = parseCommandLine(argc, argv, envp);
	if(result) return result;
	flags = buildFlags();

	translationBuffers = j9bcutil_allocTranslationBuffers(PORTLIB);
	if(!translationBuffers)
	{
		j9tty_printf( PORTLIB, "Failed to allocate buffers\n");
		return RET_ALLOCATE_FAILED;
	}
	if(options.options & OPTION_check)
	{
		verifyBuffers = j9mem_allocate_memory( sizeof( J9BytecodeVerificationData ) , J9MEM_CATEGORY_CLASSES);
		if( !verifyBuffers ) {
			j9tty_printf( PORTLIB, "Failed to allocate verify buffers\n");
			return RET_ALLOCATE_FAILED;
		}
	}

	result = 0;
	switch(options.source)
	{
		case SOURCE_files:
			/* Recursive? Directories? */
			result = processAllFiles(&argv[options.filesStart], flags);
			break;

		case SOURCE_zip:
			if ((options.filesStart == argc) && (options.options & OPTION_multi)) {
				result = processAllInZIP(options.sourceString, flags);
			} else {
				result = processFilesInZIP(options.sourceString, &argv[options.filesStart], flags);
			}
			break;

		case SOURCE_jimage:
		{
			J9JImage *jimage = NULL;
			result = j9bcutil_loadJImage(PORTLIB, options.sourceString, &jimage);
			if (J9JIMAGE_NO_ERROR != result) {
				j9tty_printf(PORTLIB, "Error in loading jimage file %s. Error code=%d\n", options.sourceString, result);
				result = RET_JIMAGE_LOADJIMAGE_FAILED;
			} else {
				if (ACTION_findModuleForPackage == options.action) {
					char *module = NULL;
					char **packageName = &argv[options.filesStart];

					while (NULL != *packageName) {
						module = (char *)j9bcutil_findModuleForPackage(PORTLIB, jimage, *packageName);
						if (NULL != module) {
							j9tty_printf(PORTLIB, "Package %s is present in module %s\n", *packageName, module);
						} else {
							j9tty_printf(PORTLIB, "Package %s is not present in any module\n");
						}
						packageName += 1;
					}
				} else if (ACTION_dumpJImageInfo == options.action) {
					j9bcutil_dumpJImageInfo(PORTLIB, jimage);
				} else {
					if ((options.filesStart == argc) && (options.options & OPTION_multi)) {
						result = processAllInJImage(jimage, options.sourceString, flags);
					} else {
						result = processFilesInJImage(jimage, options.sourceString, &argv[options.filesStart], flags);
					}
				}
				if (NULL != jimage) {
					j9bcutil_unloadJImage(PORTLIB, jimage);
					jimage = NULL;
				}
			}
			break;
		}
		case SOURCE_rom:
			result = processROMFile(options.sourceString, flags);
			break;

		default:
			result = RET_UNSUPPORTED_SOURCE;
	}
	return result;
}


static I_32 processROMFile(char* requestedFile, U_32 flags)
{
	U_8* data;
	U_32 dataLength;
	int result;

	PORT_ACCESS_FROM_PORT(portLib);

	data = loadBytes(requestedFile, &dataLength, flags);
	if(data == NULL) return RET_FILE_LOAD_FAILED;

	result = processROMClass((J9ROMClass*)data, requestedFile, flags);
	j9mem_free_memory(data);
	return result;
}


static U_8* loadBytes(char* requestedFile, U_32* lengthReturn, U_32 flags)
{
	U_8* data;
	U_32 dataLength;
	UDATA startTime;
	UDATA endTime;
	BOOLEAN verbose;

	PORT_ACCESS_FROM_PORT(portLib);
	verbose = (options.options & OPTION_verbose)?TRUE:FALSE;

	startTime = j9time_usec_clock( );
	dataLength = getBytes(requestedFile, &data);
	endTime = j9time_usec_clock( );
	translationBuffers->dynamicLoadStats->readStartTime = startTime;
	translationBuffers->dynamicLoadStats->readEndTime = endTime;
	switch(dataLength)
	{
		case RET_FILE_OPEN_FAILED:
				j9tty_printf(PORTLIB, "Could not open %s\n", requestedFile);
				return NULL;

		case RET_FILE_SEEK_FAILED:
		case RET_FILE_READ_FAILED:
				j9tty_printf(PORTLIB, "Could not read %s\n", options.sourceString);
				return NULL;

		case RET_ALLOCATE_FAILED:
				j9tty_printf(PORTLIB, "Insufficient memory to complete operation\n");
				return NULL;
	}
	if(verbose) 	j9tty_printf( PORTLIB, "<file read time: %d us.>\n", endTime - startTime);

	*lengthReturn = dataLength;
	return data;
}


static void dumpAnnotationElement(J9CfrClassFile* classfile, J9CfrAnnotationElement *element, U_32 tabLevel)
{
	J9CfrAnnotationElementArray *array;
	U_16 index, index2;
	U_32 i;

	PORT_ACCESS_FROM_PORT(portLib);

	switch (element->tag) {
		case 'B':
			index = ((J9CfrAnnotationElementPrimitive *)element)->constValueIndex;
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Byte value: %i -> %x\n", index, classfile->constantPool[index].slot1);
			break;

		case 'C':
			index = ((J9CfrAnnotationElementPrimitive *)element)->constValueIndex;
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Char value: %i -> %c\n", index, classfile->constantPool[index].slot1);
			break;

		case 'D':
			index = ((J9CfrAnnotationElementPrimitive *)element)->constValueIndex;
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Double value: %i -> 0x%08X%08X\n", index, classfile->constantPool[index].slot1, classfile->constantPool[index].slot2);
			break;

		case 'F':
			index = ((J9CfrAnnotationElementPrimitive *)element)->constValueIndex;
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Float value: %i -> 0x%08X\n", index, classfile->constantPool[index].slot1);
			break;

		case 'I':
			index = ((J9CfrAnnotationElementPrimitive *)element)->constValueIndex;
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Int value: %i -> %i\n", index, classfile->constantPool[index].slot1);
			break;

		case 'J':
			index = ((J9CfrAnnotationElementPrimitive *)element)->constValueIndex;
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Long value: %i -> 0x%08X%08X\n", index, classfile->constantPool[index].slot1, classfile->constantPool[index].slot2);
			break;

		case 'S':
			index = ((J9CfrAnnotationElementPrimitive *)element)->constValueIndex;
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Short value: %i -> %i\n", index, classfile->constantPool[index].slot1);
			break;

		case 'Z':
			index = ((J9CfrAnnotationElementPrimitive *)element)->constValueIndex;
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Boolean value: %i -> %i\n", index, classfile->constantPool[index].slot1);
			break;

		case 's':
			index = ((J9CfrAnnotationElementPrimitive *)element)->constValueIndex;
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "String value: %i -> %s\n", index, classfile->constantPool[index].bytes);
			break;

		case 'e':
			index = ((J9CfrAnnotationElementEnum *)element)->typeNameIndex;
			index2 = ((J9CfrAnnotationElementEnum *)element)->constNameIndex;
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Enum Type: %i -> %s\n", index, classfile->constantPool[index].bytes);
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Enum Constant: %i -> %s\n", index, classfile->constantPool[index2].bytes);
			break;

		case '@':
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Nested Annotation\n");

			dumpAnnotations(classfile, &((J9CfrAnnotationElementAnnotation *)element)->annotationValue, 1, tabLevel);
			break;

		case '[':
			array = (J9CfrAnnotationElementArray *)element;

			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Array Count: %i\n", array->numberOfValues);

			for (i = 0; i < array->numberOfValues; i++) {
				dumpAnnotationElement(classfile, array->values[i], tabLevel + 1);
			}
			break;

		case 'c':
			index = ((J9CfrAnnotationElementClass *)element)->classInfoIndex;
			j9tty_printf( PORTLIB, "Class: %i %s\n", index, classfile->constantPool[index].bytes);
			break;

		default:
			for(i = 0; i < tabLevel; i++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "???");
			break;
	}
}


static void dumpAnnotations(J9CfrClassFile* classfile, J9CfrAnnotation *annotations, U_32 annotationCount, U_32 tabLevel)
{
	J9CfrAnnotation *annotation;
	J9CfrAnnotationElementPair *elementPair;
	U_16 index;
	U_32 i, j, k;

	PORT_ACCESS_FROM_PORT(portLib);

	annotation = annotations;
	for (i = 0; i < annotationCount; i++, annotation++) {
		index = annotation->typeIndex;
		for(j = 0; j < tabLevel; j++) j9tty_printf( PORTLIB, "  ");
		j9tty_printf(PORTLIB, "Type: %i -> %s\n", index, classfile->constantPool[index].bytes);

		for(j = 0; j < tabLevel; j++) j9tty_printf( PORTLIB, "  ");
		j9tty_printf(PORTLIB, "Element Value Pairs (%i):\n", annotation->numberOfElementValuePairs);

		elementPair = annotation->elementValuePairs;
		for (j = 0; j < annotation->numberOfElementValuePairs; j++, elementPair++) {
			index = elementPair->elementNameIndex;
			for(k = 0; k < tabLevel + 1; k++) j9tty_printf( PORTLIB, "  ");
			j9tty_printf(PORTLIB, "Name: %i -> %s\n", index, classfile->constantPool[index].bytes);

			dumpAnnotationElement(classfile, elementPair->value, tabLevel + 1);
			j9tty_printf(PORTLIB, "\n");
		}
	}
}

#define INDENT(indentation) \
	do { \
		U_32 tl; \
		for (tl = 0; tl < tabLevel; tl++) { \
			j9tty_printf( PORTLIB, "  "); \
		}  \
	} while(FALSE)

static void dumpTypeAnnotations (J9CfrClassFile* classfile, J9CfrTypeAnnotation *annotations, U_32 annotationCount, U_32 tabLevel) {
	char *targetTypeName = "unknown target_type";
	U_32 annotationIndex = 0;

	PORT_ACCESS_FROM_PORT(portLib);

	for (annotationIndex = 0; annotationIndex < annotationCount; ++annotationIndex) {
		J9CfrTypeAnnotation *currentAnnotation = &annotations[annotationIndex];
		U_32 pi = 0;

		switch (currentAnnotation->targetType) {
		case CFR_TARGET_TYPE_TypeParameterGenericClass:
			targetTypeName = "TypeParameterGenericClass";
			break;

		case CFR_TARGET_TYPE_TypeParameterGenericMethod:
			targetTypeName = "TypeParameterGenericMethod";
			break;

		case CFR_TARGET_TYPE_TypeInExtends:
			targetTypeName = "TypeInExtends";
			break;

		case CFR_TARGET_TYPE_TypeInBoundOfGenericClass:
			targetTypeName = "TypeInBoundOfGenericClass";
			break;

		case CFR_TARGET_TYPE_TypeInBoundOfGenericMethod:
			targetTypeName = "TypeInBoundOfGenericMethod";
			break;

		case CFR_TARGET_TYPE_TypeInFieldDecl:
			targetTypeName = "TypeInFieldDecl";
			break;

		case CFR_TARGET_TYPE_ReturnType:
			targetTypeName = "ReturnType";
			break;

		case CFR_TARGET_TYPE_ReceiverType:
			targetTypeName = "ReceiverType";
			break;

		case CFR_TARGET_TYPE_TypeInFormalParam:
			targetTypeName = "TypeInFormalParam";
			break;

		case CFR_TARGET_TYPE_TypeInThrows:
			targetTypeName = "TypeInThrows";
			break;

		case CFR_TARGET_TYPE_TypeInLocalVar:
			targetTypeName = "TypeInLocalVar";
			break;

		case CFR_TARGET_TYPE_TypeInResourceVar:
			targetTypeName = "TypeInResourceVar";
			break;

		case CFR_TARGET_TYPE_TypeInExceptionParam:
			targetTypeName = "TypeInExceptionParam";
			break;

		case CFR_TARGET_TYPE_TypeInInstanceof:
			targetTypeName = "TypeInInstanceof";
			break;

		case CFR_TARGET_TYPE_TypeInNew:
			targetTypeName = "TypeInNew";
			break;

		case CFR_TARGET_TYPE_TypeInMethodrefNew:
			targetTypeName = "TypeInMethodrefNew";
			break;

		case CFR_TARGET_TYPE_TypeInMethodrefIdentifier:
			targetTypeName = "TypeInMethodrefIdentifier";
			break;

		case CFR_TARGET_TYPE_TypeInCast:
			targetTypeName = "TypeInCast";
			break;

		case CFR_TARGET_TYPE_TypeForGenericConstructorInNew:
			targetTypeName = "TypeForGenericConstructorInNew";
			break;

		case CFR_TARGET_TYPE_TypeForGenericMethodInvocation:
			targetTypeName = "TypeForGenericMethodInvocation";
			break;

		case CFR_TARGET_TYPE_TypeForGenericConstructorInMethodRef:
			targetTypeName = "TypeForGenericConstructorInMethodRef";
			break;

		case CFR_TARGET_TYPE_TypeForGenericMethodInvocationInMethodRef:
			targetTypeName = "TypeForGenericMethodInvocationInMethodRef";
			break;

		default:
			targetTypeName = "unknown target_type";
			break;
		}
		INDENT(tabLevel);
		j9tty_printf(PORTLIB, "target_type=0x%0x (%s) ", currentAnnotation->targetType, targetTypeName);

		switch (currentAnnotation->targetType) {

			case CFR_TARGET_TYPE_TypeParameterGenericClass:
			case CFR_TARGET_TYPE_TypeParameterGenericMethod:
				j9tty_printf(PORTLIB, "type_parameter_index=%d", currentAnnotation->targetInfo.typeParameterTarget.typeParameterIndex);
			break;

			case CFR_TARGET_TYPE_TypeInExtends:
				j9tty_printf(PORTLIB, "supertype_index=%d", currentAnnotation->targetInfo.supertypeTarget.supertypeIndex);
			break;

			case CFR_TARGET_TYPE_TypeInBoundOfGenericClass:
			case CFR_TARGET_TYPE_TypeInBoundOfGenericMethod:
				j9tty_printf(PORTLIB, "type_parameter_index=%d bound_index=%d",
						currentAnnotation->targetInfo.typeParameterBoundTarget.typeParameterIndex, currentAnnotation->targetInfo.typeParameterBoundTarget.boundIndex);
			break;

			case CFR_TARGET_TYPE_TypeInFormalParam:
				j9tty_printf(PORTLIB, "formal_parameter_index=%d", currentAnnotation->targetInfo.methodFormalParameterTarget.formalParameterIndex);
			break;

			case CFR_TARGET_TYPE_TypeInThrows:
				j9tty_printf(PORTLIB, "throws_type_index=%d", currentAnnotation->targetInfo.throwsTarget.throwsTypeIndex);
			break;

			case CFR_TARGET_TYPE_TypeInLocalVar:
			case CFR_TARGET_TYPE_TypeInResourceVar: {
				U_32 ti;

				J9CfrLocalvarTarget *t = &(currentAnnotation->targetInfo.localvarTarget);
				for (ti=0; ti < t->tableLength; ++ti) {
					J9CfrLocalvarTargetEntry *te = &(t->table[ti]);
					j9tty_printf(PORTLIB, "\n");
					INDENT(tabLevel);
					j9tty_printf(PORTLIB, "start_pc=%d length=%d index=%d", te->startPC, te->length, te->index);
				}
			}
			break;

			case CFR_TARGET_TYPE_TypeInExceptionParam:
				j9tty_printf(PORTLIB, "exception_table_index=%d", currentAnnotation->targetInfo.catchTarget.exceptiontableIndex);
			break;

			case CFR_TARGET_TYPE_TypeInInstanceof:
			case CFR_TARGET_TYPE_TypeInNew:
			case CFR_TARGET_TYPE_TypeInMethodrefNew:
			case CFR_TARGET_TYPE_TypeInMethodrefIdentifier:
				j9tty_printf(PORTLIB, "offset=%d", currentAnnotation->targetInfo.offsetTarget.offset);
		break;

			case CFR_TARGET_TYPE_TypeInCast:
			case CFR_TARGET_TYPE_TypeForGenericConstructorInNew:
			case CFR_TARGET_TYPE_TypeForGenericMethodInvocation:
			case CFR_TARGET_TYPE_TypeForGenericConstructorInMethodRef:
			case CFR_TARGET_TYPE_TypeForGenericMethodInvocationInMethodRef:
				j9tty_printf(PORTLIB, "offset=%d type_argument_index=%d",
						currentAnnotation->targetInfo.typeArgumentTarget.offset, currentAnnotation->targetInfo.typeArgumentTarget.typeArgumentIndex);
		break;

			default:
				break;
		}
		j9tty_printf(PORTLIB, "\n");

		for (pi=0; pi < currentAnnotation->typePath.pathLength; ++pi) {
			J9CfrTypePathEntry *entry = &currentAnnotation->typePath.path[pi];
			INDENT(tabLevel);
			j9tty_printf(PORTLIB, "type_path_kind=%d type_argument_index=%d\n", entry->typePathKind, entry->typeArgumentIndex);
		}

		dumpAnnotations(classfile, &currentAnnotation->annotation, 1, tabLevel+1);
	}
	return;
}

static void dumpStackMap(J9CfrAttributeStackMap * stackMap, J9CfrClassFile* classfile, U_32 tabLevel)
{
	U_32 i, j;
	U_32 framePC = (U_32) -1;
	U_8 frameType;
	U_8 *framePointer = stackMap->entries;
	U_8 *frameEnd = framePointer+stackMap->mapLength;
	U_16 offset;

	PORT_ACCESS_FROM_PORT(portLib);

	for(j = 0; j < stackMap->numberOfEntries; j++) {
		if (framePointer >= frameEnd) {
			j9tty_printf( PORTLIB, "End of StackMapTable attribute reached before last field\n");
			return;
		}
		for(i = 0; i < tabLevel; i++) {
			j9tty_printf( PORTLIB, "  ");
		}

		if (stackMap->tag == CFR_ATTRIBUTE_StackMap) {
			frameType = 255;
		} else {
			frameType = *framePointer++;
			framePC += 1;
		}

		if (frameType < 64) {
			framePC += (U_32) frameType;
			j9tty_printf( PORTLIB, "pc: %i same\n", framePC);

		} else if (frameType < 128) {
			framePC += (U_32) (frameType - 64);
			j9tty_printf( PORTLIB, "pc: %i same_locals_1_stack_item: ", framePC);
			framePointer = dumpStackMapSlots(classfile, framePointer, 1);
			j9tty_printf( PORTLIB, "\n");

		} else if (frameType < 247) {
			j9tty_printf( PORTLIB, "UNKNOWN FRAME TAG %02x\n", frameType);

		} else if (frameType == 247) {
			offset = (framePointer[0] << 8) + framePointer[1];
			framePointer +=2;
			framePC += (U_32) (offset);
			j9tty_printf( PORTLIB, "pc: %i same_locals_1_stack_item_extended: ", framePC);
			framePointer = dumpStackMapSlots(classfile, framePointer, 1);
			j9tty_printf( PORTLIB, "\n");

		} else if (frameType < 251) {
			offset = (framePointer[0] << 8) + framePointer[1];
			framePointer +=2;
			framePC += (U_32) (offset);
			j9tty_printf( PORTLIB, "pc: %i chop %i\n", framePC, 251 - frameType);

		} else if (frameType == 251) {
			offset = (framePointer[0] << 8) + framePointer[1];
			framePointer +=2;
			framePC += (U_32) (offset);
			j9tty_printf( PORTLIB, "pc: %i same_extended\n", framePC);

		} else if (frameType < 255) {
			offset = (framePointer[0] << 8) + framePointer[1];
			framePointer +=2;
			framePC += (U_32) (offset);
			j9tty_printf( PORTLIB, "pc: %i append: ", framePC);
			framePointer = dumpStackMapSlots(classfile, framePointer, (U_16) (frameType - 251));
			j9tty_printf( PORTLIB, "\n");

		} else if (frameType == 255) {
			offset = (framePointer[0] << 8) + framePointer[1];
			framePointer +=2;
			if (stackMap->tag == CFR_ATTRIBUTE_StackMap) {
				framePC = offset;
			} else {
				framePC += (U_32) (offset);
			}
			j9tty_printf( PORTLIB, "pc: %i full, local(s): ", framePC);
			offset = (framePointer[0] << 8) + framePointer[1];
			framePointer +=2;
			framePointer = dumpStackMapSlots(classfile, framePointer, offset);
			j9tty_printf( PORTLIB, ", stack: ");
			offset = (framePointer[0] << 8) + framePointer[1];
			framePointer +=2;
			framePointer = dumpStackMapSlots(classfile, framePointer, offset);
			j9tty_printf( PORTLIB, "\n");
		}
	}

	return;
}


static U_8 * dumpStackMapSlots(J9CfrClassFile* classfile, U_8 * slotData, U_16 slotCount)
{
	U_16 i;
	U_8 slotType;
	U_16 index;

	PORT_ACCESS_FROM_PORT(portLib);

	const char * slotTypes [] = {
		"top",
		"int",
		"float",
		"double",
		"long",
		"null",
		"uninitialized_this" };

	j9tty_printf( PORTLIB, "(");

	for(i = 0; i < slotCount; i++) {
		slotType = *slotData++;

		if (slotType <= CFR_STACKMAP_TYPE_INIT_OBJECT) {
			j9tty_printf( PORTLIB, "%s", slotTypes[slotType]);

		} else if (slotType == CFR_STACKMAP_TYPE_OBJECT) {
				index = (slotData[0] << 8) + slotData[1];
				index = classfile->constantPool[index].slot1;
				if (classfile->constantPool[index].bytes[0] != '[') {
					j9tty_printf( PORTLIB, "L");
				}
				j9tty_printf( PORTLIB, "%s;", classfile->constantPool[index].bytes);
				slotData += 2;

		} else if (slotType == CFR_STACKMAP_TYPE_NEW_OBJECT) {
			index = (slotData[0] << 8) + slotData[1];
			j9tty_printf( PORTLIB, "this pc:%i", index);
			slotData += 2;

		} else {
			j9tty_printf( PORTLIB, "UNKNOWN SLOT TYPE %x", slotType);
		}

		if (i != (slotCount - 1)) {
			j9tty_printf( PORTLIB, ", ");
		}
	}

	j9tty_printf( PORTLIB, ")");

	return slotData;
}
