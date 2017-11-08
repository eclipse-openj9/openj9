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

#include "cfreader.h"
#include "j9protos.h"
#include "j9user.h"
#include <string.h>
#include "ut_j9bcu.h"
#include "j9bcvnls.h"
#include "romphase.h"
#include "bcutil_internal.h"
#include "util_api.h"

/*
 * Note: The attrlookup.h file contains a generated perfect hash table
 * for fast lookup of known attributes. It is used by attributeTagFor().
 *
 * It was generated using the gperf utility via the following command line:
 *
 *    gperf -CD -t attrlookup.gperf
 *
 * (The gperf utility is a perfect hash function generator that is readily available on Linux.)
 *
 */
#include "attrlookup.h"

static I_32 checkAttributes (J9CfrClassFile* classfile, J9CfrAttribute** attributes, U_32 attributesCount, U_8* segment, I_32 maxBootstrapMethodIndex, U_32 extra, U_32 flags);
static I_32 readAttributes (J9CfrClassFile * classfile, J9CfrAttribute *** pAttributes, U_32 attributesCount, U_8 * data, U_8 * dataEnd, U_8 * segment, U_8 * segmentEnd, U_8 ** pIndex, U_8 ** pFreePointer, U_32 flags, UDATA * syntheticFound);
static I_32 checkFields (J9CfrClassFile * classfile, U_8 * segment, U_32 flags);
static U_8 attributeTagFor (J9CfrConstantPoolInfo *utf8, BOOLEAN stripDebugAttributes);
static I_32 readAnnotations (J9CfrClassFile * classfile, J9CfrAnnotation * pAnnotations, U_32 annotationCount, U_8 * data, U_8 * dataEnd, U_8 * segment, U_8 * segmentEnd, U_8 ** pIndex, U_8 ** pFreePointer, U_32 flags);
static I_32 readTypeAnnotation (J9CfrClassFile * classfile, J9CfrTypeAnnotation * pAnnotations, U_8 * data, U_8 * dataEnd, U_8 * segment, U_8 * segmentEnd, U_8 ** pIndex, U_8 ** pFreePointer, U_32 flags);
static I_32 readAnnotationElement (J9CfrClassFile * classfile, J9CfrAnnotationElement ** pAnnotationElement, U_8 * data, U_8 * dataEnd, U_8 * segment, U_8 * segmentEnd, U_8 ** pIndex, U_8 ** pFreePointer, U_32 flags);
static I_32 checkClassVersion (J9CfrClassFile* classfile, U_8* segment, U_32 flags);
static BOOLEAN utf8EqualUtf8 (J9CfrConstantPoolInfo *utf8a, J9CfrConstantPoolInfo *utf8b);
static BOOLEAN utf8Equal (J9CfrConstantPoolInfo* utf8, char* string, UDATA length);
static I_32 readMethods (J9CfrClassFile* classfile, U_8* data, U_8* dataEnd, U_8* segment, U_8* segmentEnd, U_8** pIndex, U_8** pFreePointer, U_32 flags);
static I_32 readPool (J9CfrClassFile* classfile, U_8* data, U_8* dataEnd, U_8* segment, U_8* segmentEnd, U_8** pIndex, U_8** pFreePointer);
static I_32 checkDuplicateMembers (J9PortLibrary* portLib, J9CfrClassFile * classfile, U_8 * segment, U_32 flags, UDATA memberSize);
static I_32 checkPool (J9CfrClassFile* classfile, U_8* segment, U_8* poolStart, I_32 *maxBootstrapMethodIndex, U_32 vmVersionShifted, U_32 flags);
static I_32 checkClass (J9PortLibrary *portLib, J9CfrClassFile* classfile, U_8* segment, U_32 endOfConstantPool, U_32 vmVersionShifted, U_32 flags);
static I_32 readFields (J9CfrClassFile* classfile, U_8* data, U_8* dataEnd, U_8* segment, U_8* segmentEnd, U_8** pIndex, U_8** pFreePointer, U_32 flags);
static I_32 checkMethods (J9CfrClassFile* classfile, U_8* segment, U_32 vmVersionShifted, U_32 flags);
static BOOLEAN memberEqual (J9CfrClassFile * classfile, J9CfrMember* a, J9CfrMember* b);
static void sortMethodIndex(J9CfrConstantPoolInfo* constantPool, J9CfrMethod *list, IDATA start, IDATA end);
static IDATA compareMethodIDs(J9CfrConstantPoolInfo* constantPool, J9CfrMethod *a, J9CfrMethod *b);
static U_8* getUTF8Data(J9CfrConstantPoolInfo* constantPool, U_16 cpIndex);
static U_16 getUTF8Length(J9CfrConstantPoolInfo* constantPool, U_16 cpIndex);

#define OUTSIDE_CODE	((U_32) -1)

/* set DUP_TIMING to 1 to determine the best value 
 * for DUP_HASH_THRESHOLD on a particular 
 * platform 
 */
#define DUP_TIMING 0
#define DUP_HASH_THRESHOLD 30

/* mapping characters A..Z */
static const U_8 cpTypeCharConversion[] = {
0,										CFR_CONSTANT_Integer,	CFR_CONSTANT_Integer,	CFR_CONSTANT_Double,
0,										CFR_CONSTANT_Float,		0,										0,
CFR_CONSTANT_Integer,	CFR_CONSTANT_Long,		0,										CFR_CONSTANT_String,
0,										0,										0,										0,
0,										0,										CFR_CONSTANT_Integer,	0, 
0,										0,										0,										0,
0,										CFR_CONSTANT_Integer}; 

/*
	Compare a J9CfrConstantUtf8 to a C string.
*/

static BOOLEAN 
utf8Equal(J9CfrConstantPoolInfo* utf8, char* string, UDATA length)
{
	if ((utf8->tag !=CFR_CONSTANT_Utf8) || (utf8->slot1 != length)) {
		return FALSE;
	}

	return memcmp(utf8->bytes, string, length) == 0;
}	


/*
	Compare an J9CfrConstantPoolInfo to the known attribute
	names. Return a tag value for any known attribute,
	0 for an unknown one.
*/
static U_8 
attributeTagFor(J9CfrConstantPoolInfo *utf8, BOOLEAN stripDebugAttributes)
{
	const struct AttribType *attribType = lookupKnownAttribute((const char *)utf8->bytes, (unsigned int)utf8->slot1);

	if (NULL != attribType) {
		return (stripDebugAttributes ? attribType->strippedAttribCode : attribType->attribCode);
	}

	return (U_8) (stripDebugAttributes
		? CFR_ATTRIBUTE_StrippedUnknown
 		: CFR_ATTRIBUTE_Unknown);
}


/*
	Read the attributes from the bytes in @data.
	Returns 0 on success, non-zero on failure.
*/

static I_32 
readAttributes(J9CfrClassFile * classfile, J9CfrAttribute *** pAttributes, U_32 attributesCount, U_8 * data,
						   U_8 * dataEnd, U_8 * segment, U_8 * segmentEnd, U_8 ** pIndex, U_8 ** pFreePointer, U_32 flags, UDATA * syntheticFound)
{
	J9CfrAttribute **attributes = *pAttributes;
	U_8 *index = *pIndex;
	U_8 *freePointer = *pFreePointer;
	J9CfrConstantPoolInfo *info;
	J9CfrAttribute *attrib;
	J9CfrAttributeCode *code;
	J9CfrAttributeExceptions *exceptions;
	J9CfrExceptionTableEntry *exception;
	J9CfrAttributeInnerClasses *classes;
	J9CfrParameterAnnotations *parameterAnnotations;
	J9CfrTypeAnnotation *typeAnnotations = NULL;
	J9CfrAttributeStackMap *stackMap;
	J9CfrAttributeBootstrapMethods *bootstrapMethods;
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
	J9CfrAttributeMemberOfNest *memberOfNest;
	J9CfrAttributeNestMembers *nestMembers;
#endif /* J9VM_OPT_VALHALLA_NESTMATES */
	U_32 name, length;
	U_32 tag, errorCode, offset;
	U_8 *end;
	U_32 address;
	U_32 i, j, k;
	I_32 result;
	BOOLEAN sourceFileAttributeRead = FALSE;
	BOOLEAN bootstrapMethodAttributeRead = FALSE;
	BOOLEAN visibleTypeAttributeRead = FALSE;
	BOOLEAN invisibleTypeAttributeRead = FALSE;
	BOOLEAN sourceDebugExtensionRead  = FALSE;
	BOOLEAN annotationDefaultRead  = FALSE;
	BOOLEAN visibleAnnotationsRead  = FALSE;
	BOOLEAN invisibleAnnotationsRead  = FALSE;
	BOOLEAN visibleParameterAnnotationsRead  = FALSE;
	BOOLEAN invisibleParameterAnnotationsRead  = FALSE;
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
	BOOLEAN nestAttributeRead = FALSE;
#endif /* J9VM_OPT_VALHALLA_NESTMATES */

	if (NULL != syntheticFound) {
		*syntheticFound = FALSE;
	}
	for (i = 0; i < attributesCount; i++) {
		address = (U_32) (index - data);

		CHECK_EOF(6);
		NEXT_U16(name, index);
		NEXT_U32(length, index);
		end = index + length;

		if ((!name) || (name > classfile->constantPoolCount)) {
			errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
			offset = address;
			goto _errorFound;
		}
		info = &classfile->constantPool[name];
		if (info->tag != CFR_CONSTANT_Utf8) {
			errorCode = J9NLS_CFR_ERR_BAD_NAME_INDEX__ID;
			offset = address;
			goto _errorFound;
		}
		tag = attributeTagFor(info, (BOOLEAN) (flags & CFR_StripDebugAttributes));

		switch (tag) {
		case CFR_ATTRIBUTE_SourceFile:
			if (sourceFileAttributeRead){
				errorCode = J9NLS_CFR_ERR_FOUND_MULTIPLE_SOURCE_FILE_ATTRIBUTES__ID;
				offset = address;
				goto _errorFound;
			}
			sourceFileAttributeRead = TRUE;
			if (!ALLOC_CAST(attrib, J9CfrAttributeSourceFile, J9CfrAttribute)) {
				return -2;
			}
			CHECK_EOF(2);
			NEXT_U16(((J9CfrAttributeSourceFile *) attrib)->sourceFileIndex, index);
			break;

		case CFR_ATTRIBUTE_Signature:
			if (!ALLOC_CAST(attrib, J9CfrAttributeSignature, J9CfrAttribute)) {
				return -2;
			}
			CHECK_EOF(2);
			NEXT_U16(((J9CfrAttributeSignature *) attrib)->signatureIndex, index);
			break;

		case CFR_ATTRIBUTE_ConstantValue:
			if (!ALLOC_CAST(attrib, J9CfrAttributeConstantValue, J9CfrAttribute)) {
				return -2;
			}
			CHECK_EOF(2);
			NEXT_U16(((J9CfrAttributeConstantValue *) attrib)->constantValueIndex, index);
			break;

		case CFR_ATTRIBUTE_Code:
			if (!ALLOC(code, J9CfrAttributeCode)) {
				return -2;
			}
			attrib = (J9CfrAttribute*)code;

			CHECK_EOF(8);
			NEXT_U16(code->maxStack, index);
			NEXT_U16(code->maxLocals, index);
			NEXT_U32(code->codeLength, index);

			CHECK_EOF(code->codeLength);
			if (!ALLOC_ARRAY(code->code, code->codeLength, U_8)) {
				return -2;
			}
			code->originalCode = index;
			memcpy (code->code, index, code->codeLength);
			index += code->codeLength;

			CHECK_EOF(2);
			NEXT_U16(code->exceptionTableLength, index);
			if (!ALLOC_ARRAY(code->exceptionTable, code->exceptionTableLength, J9CfrExceptionTableEntry)) {
				return -2;
			}
			CHECK_EOF(code->exceptionTableLength << 3);
			for (j = 0; j < code->exceptionTableLength; j++) {
				exception = &(code->exceptionTable[j]);
				NEXT_U16(exception->startPC, index);
				NEXT_U16(exception->endPC, index);
				NEXT_U16(exception->handlerPC, index);
				NEXT_U16(exception->catchType, index);
			}

			CHECK_EOF(2);
			NEXT_U16(code->attributesCount, index);
			if (!ALLOC_ARRAY(code->attributes, code->attributesCount, J9CfrAttribute *)) {
				return -2;
			}
			if ((result = readAttributes(classfile, &(code->attributes), code->attributesCount, data, dataEnd, segment, segmentEnd, &index, &freePointer, flags, NULL)) != 0) {
				return result;
			}
			break;

		case CFR_ATTRIBUTE_Exceptions:
			if (!ALLOC(exceptions, J9CfrAttributeExceptions)) {
				return -2;
			}
			attrib = (J9CfrAttribute*)exceptions;
			CHECK_EOF(2);
			NEXT_U16(exceptions->numberOfExceptions, index);
			if (!ALLOC_ARRAY(exceptions->exceptionIndexTable, exceptions->numberOfExceptions, U_16)) {
				return -2;
			}
			CHECK_EOF(exceptions->numberOfExceptions << 1);
			for (j = 0; j < exceptions->numberOfExceptions; j++) {
				NEXT_U16(exceptions->exceptionIndexTable[j], index);
			}
			break;

		case CFR_ATTRIBUTE_LineNumberTable:
			if (!ALLOC_CAST(attrib, J9CfrAttributeLineNumberTable, J9CfrAttribute)) {
				return -2;
			}
			CHECK_EOF(2);
			NEXT_U16(((J9CfrAttributeLineNumberTable *) attrib)->lineNumberTableLength, index);
			if (!ALLOC_ARRAY(
				 ((J9CfrAttributeLineNumberTable *) attrib)->lineNumberTable, 
				 ((J9CfrAttributeLineNumberTable *) attrib)->lineNumberTableLength, 
				 J9CfrLineNumberTableEntry)) { 
				return -2;
			}

			CHECK_EOF(((J9CfrAttributeLineNumberTable *) attrib)->lineNumberTableLength << 2);
			for (j = 0; j < ((J9CfrAttributeLineNumberTable *) attrib)->lineNumberTableLength; j++) {
				NEXT_U16(((J9CfrAttributeLineNumberTable *) attrib)->lineNumberTable[j].startPC, index);
				NEXT_U16(((J9CfrAttributeLineNumberTable *) attrib)->lineNumberTable[j].lineNumber, index);
			}
			break;

		case CFR_ATTRIBUTE_LocalVariableTable:
			if (!ALLOC_CAST(attrib, J9CfrAttributeLocalVariableTable, J9CfrAttribute)) {
				return -2;
			}
			CHECK_EOF(2);
			NEXT_U16(((J9CfrAttributeLocalVariableTable *) attrib)->localVariableTableLength, index);
			if (!ALLOC_ARRAY(
				 ((J9CfrAttributeLocalVariableTable *) attrib)->localVariableTable, 
				 ((J9CfrAttributeLocalVariableTable *) attrib)->localVariableTableLength,
				 J9CfrLocalVariableTableEntry)) {
				return -2;
			}

			CHECK_EOF(((J9CfrAttributeLocalVariableTable *) attrib)->localVariableTableLength * 10);
			for (j = 0; j < ((J9CfrAttributeLocalVariableTable *) attrib)->localVariableTableLength; j++) {
				NEXT_U16(((J9CfrAttributeLocalVariableTable *) attrib)->localVariableTable[j].startPC, index);
				NEXT_U16(((J9CfrAttributeLocalVariableTable *) attrib)->localVariableTable[j].length, index);
				NEXT_U16(((J9CfrAttributeLocalVariableTable *) attrib)->localVariableTable[j].nameIndex, index);
				NEXT_U16(((J9CfrAttributeLocalVariableTable *) attrib)->localVariableTable[j].descriptorIndex, index);
				NEXT_U16(((J9CfrAttributeLocalVariableTable *) attrib)->localVariableTable[j].index, index);
			}
			break;

		case CFR_ATTRIBUTE_LocalVariableTypeTable:
			if (!ALLOC_CAST(attrib, J9CfrAttributeLocalVariableTypeTable, J9CfrAttribute)) {
				return -2;
			}
			CHECK_EOF(2);
			NEXT_U16(((J9CfrAttributeLocalVariableTypeTable *) attrib)->localVariableTypeTableLength, index);
			if (!ALLOC_ARRAY(
				 ((J9CfrAttributeLocalVariableTypeTable *) attrib)->localVariableTypeTable, 
				 ((J9CfrAttributeLocalVariableTypeTable *) attrib)->localVariableTypeTableLength,
				 J9CfrLocalVariableTypeTableEntry)) {
				return -2;
			}

			CHECK_EOF(((J9CfrAttributeLocalVariableTypeTable *) attrib)->localVariableTypeTableLength * 10);
			for (j = 0; j < ((J9CfrAttributeLocalVariableTypeTable *) attrib)->localVariableTypeTableLength; j++) {
				NEXT_U16(((J9CfrAttributeLocalVariableTypeTable *) attrib)->localVariableTypeTable[j].startPC, index);
				NEXT_U16(((J9CfrAttributeLocalVariableTypeTable *) attrib)->localVariableTypeTable[j].length, index);
				NEXT_U16(((J9CfrAttributeLocalVariableTypeTable *) attrib)->localVariableTypeTable[j].nameIndex, index);
				NEXT_U16(((J9CfrAttributeLocalVariableTypeTable *) attrib)->localVariableTypeTable[j].signatureIndex, index);
				NEXT_U16(((J9CfrAttributeLocalVariableTypeTable *) attrib)->localVariableTypeTable[j].index, index);
			}
			break;

		case CFR_ATTRIBUTE_Synthetic:
			if (!ALLOC_CAST(attrib, J9CfrAttributeSynthetic, J9CfrAttribute)) {
				return -2;
			}
			CHECK_EOF(length);
			index += length;
			if (NULL != syntheticFound) {
				*syntheticFound = TRUE;
			}
			break;

		case CFR_ATTRIBUTE_AnnotationDefault: {
			if (annotationDefaultRead) {
				/* found a redundant attribute */
				errorCode = J9NLS_CFR_ERR_MULTIPLE_ANNOTATION_DEFAULT_ATTRIBUTES__ID;
				offset = address;
				goto _errorFound;

			} else {
				annotationDefaultRead = TRUE;
			}
			if (!ALLOC_CAST(attrib, J9CfrAttributeAnnotationDefault, J9CfrAttribute)) {
				return -2;
			}

			result = readAnnotationElement(classfile, &((J9CfrAttributeAnnotationDefault *)attrib)->defaultValue,
					data, dataEnd, segment, segmentEnd, &index, &freePointer, flags);
			if (result != 0) {
				return result;
			}
		}
		break;

		case CFR_ATTRIBUTE_RuntimeInvisibleAnnotations:
			if (invisibleAnnotationsRead) {
				/* found a redundant attribute */
				errorCode = J9NLS_CFR_ERR_MULTIPLE_ANNOTATION_ATTRIBUTES__ID;
				offset = address;
				goto _errorFound;

			} else {
				invisibleAnnotationsRead = TRUE;
			}
			if (J9_ARE_NO_BITS_SET(flags, BCT_RetainRuntimeInvisibleAttributes)) {
				if (!ALLOC(attrib, J9CfrAttribute)) { /* create dummy attribute and skip */
					return BCT_ERR_OUT_OF_ROM;
				}
				CHECK_EOF(length);
				index += length;
				break;
			} /* else FALLTHROUGH */
		case CFR_ATTRIBUTE_RuntimeVisibleAnnotations: {
			U_8 *attributeStart = index;
			J9CfrAttributeRuntimeVisibleAnnotations *annotations = NULL;
			if (CFR_ATTRIBUTE_RuntimeVisibleAnnotations == tag) {
				if (visibleAnnotationsRead) {
					/* found a redundant attribute */
					errorCode = J9NLS_CFR_ERR_MULTIPLE_ANNOTATION_ATTRIBUTES__ID;
					offset = address;
					goto _errorFound;
				}
				visibleAnnotationsRead = TRUE;
			}
			if (!ALLOC_CAST(attrib, J9CfrAttributeRuntimeVisibleAnnotations, J9CfrAttribute)) {
				return BCT_ERR_OUT_OF_ROM;
			}

			annotations = (J9CfrAttributeRuntimeVisibleAnnotations *)attrib;
			annotations->rawAttributeData = NULL;
			annotations->rawDataLength = 0; /* 0 indicates attribute is well-formed */
			CHECK_EOF(2);
			NEXT_U16(annotations->numberOfAnnotations, index);

			if (!ALLOC_ARRAY(annotations->annotations, annotations->numberOfAnnotations, J9CfrAnnotation))
			{
				return BCT_ERR_OUT_OF_ROM;
			}

			result = readAnnotations(classfile, annotations->annotations, annotations->numberOfAnnotations, data, dataEnd, segment, segmentEnd, &index, &freePointer, flags);
			if ((BCT_ERR_NO_ERROR != result) || (index != end)) {
				U_32 cursor = 0;
				Trc_BCU_MalformedAnnotation(address);

				annotations->rawDataLength = length;
				if (!ALLOC_ARRAY(annotations->rawAttributeData, length, U_8)) {
					return BCT_ERR_OUT_OF_ROM;
				}
				index = attributeStart; /* rewind to the start of the attribute */
				for (cursor = 0; cursor < annotations->rawDataLength; ++cursor) {
					CHECK_EOF(1);
					NEXT_U8(annotations->rawAttributeData[cursor], index);
				}
			}
		}
		break;

		case CFR_ATTRIBUTE_MethodParameters:
			if (!ALLOC_CAST(attrib, J9CfrAttributeMethodParameters, J9CfrAttribute)) {
				return -2;
			}
			CHECK_EOF(1);
			NEXT_U8(((J9CfrAttributeMethodParameters *) attrib)->numberOfMethodParameters, index);

			if (!ALLOC_ARRAY(
						 ((J9CfrAttributeMethodParameters *) attrib)->methodParametersIndexTable,
						 ((J9CfrAttributeMethodParameters *) attrib)->numberOfMethodParameters,
						 U_16)) {
						return -2;
					}

			if (!ALLOC_ARRAY(
						 ((J9CfrAttributeMethodParameters *) attrib)->flags,
						 ((J9CfrAttributeMethodParameters *) attrib)->numberOfMethodParameters,
						 U_16)) {
						return -2;
					}

			/* We need 4 bytes for each methodParameter (2 for flags, 2 formethodParametersIndexTable ) */
			CHECK_EOF(((J9CfrAttributeMethodParameters *) attrib)->numberOfMethodParameters * 4);
			for (j = 0; j < ((J9CfrAttributeMethodParameters *) attrib)->numberOfMethodParameters; j++) {
				NEXT_U16(((J9CfrAttributeMethodParameters *) attrib)->methodParametersIndexTable[j], index);
				NEXT_U16(((J9CfrAttributeMethodParameters *) attrib)->flags[j], index);
			}
			break;

		case CFR_ATTRIBUTE_RuntimeInvisibleParameterAnnotations:
			if (invisibleParameterAnnotationsRead) {
				/* found a redundant attribute */
				errorCode = J9NLS_CFR_ERR_MULTIPLE_PARAMETER_ANNOTATION_ATTRIBUTES__ID;
				offset = address;
				goto _errorFound;

			} else {
				invisibleParameterAnnotationsRead = TRUE;
			}
			if (J9_ARE_NO_BITS_SET(flags, BCT_RetainRuntimeInvisibleAttributes)) {
				if (!ALLOC(attrib, J9CfrAttribute)) { /* create dummy attribute and skip */
					return -2;
				}
				CHECK_EOF(length);
				index += length;
				break;
			} /* else FALLTHROUGH */
		case CFR_ATTRIBUTE_RuntimeVisibleParameterAnnotations: {
			U_8 *attributeStart = index;
			J9CfrAttributeRuntimeVisibleParameterAnnotations *annotations = NULL;
			if (CFR_ATTRIBUTE_RuntimeVisibleParameterAnnotations == tag) {
				if (visibleParameterAnnotationsRead) {
					/* found a redundant attribute */
					errorCode = J9NLS_CFR_ERR_MULTIPLE_PARAMETER_ANNOTATION_ATTRIBUTES__ID;
					offset = address;
					goto _errorFound;

				}
				visibleParameterAnnotationsRead = TRUE;
			}
			if (!ALLOC_CAST(attrib, J9CfrAttributeRuntimeVisibleParameterAnnotations, J9CfrAttribute)) {
				return -2;
			}
			annotations = (J9CfrAttributeRuntimeVisibleParameterAnnotations *)attrib;
			annotations->rawAttributeData = NULL;
			annotations->rawDataLength = 0; /* 0 indicates attribute is well-formed */
			CHECK_EOF(1);
			NEXT_U8(annotations->numberOfParameters, index);

			if (!ALLOC_ARRAY(annotations->parameterAnnotations,	annotations->numberOfParameters, J9CfrParameterAnnotations)) {
				return -2;
			}

			parameterAnnotations = annotations->parameterAnnotations;
			for (j = 0; j < annotations->numberOfParameters; j++, parameterAnnotations++) {
				CHECK_EOF(2);
				NEXT_U16(parameterAnnotations->numberOfAnnotations, index);

				if (!ALLOC_ARRAY(parameterAnnotations->annotations, parameterAnnotations->numberOfAnnotations, J9CfrAnnotation)) {
					return -2;
				}

				result = readAnnotations(classfile, parameterAnnotations->annotations,	parameterAnnotations->numberOfAnnotations, data, dataEnd,
						segment, segmentEnd, &index, &freePointer, flags);
				if (result != 0) {
					break;
				}
			}
			/* Jazz 104170: According to Java SE 8V M Spec at 4.7.18 The RuntimeVisibleParameterAnnotations Attribute:
			 * num_parameters == 0 means there is no parameter for this method and naturally no annotations 
			 * for these parameters, in which case the code should treat this attribute as bad and ignore it.
			 */
			if ((BCT_ERR_NO_ERROR != result) || (index != end) || (0 == annotations->numberOfParameters)) {
				U_32 cursor = 0;
				/*
				 * give up parsing.
				 * Copy the raw data, a 0 where the num_parameters should be, followed by the rest of the data.
				 * This is forces an error because if num_parameters=0, there should be no more data in the attribute.
				 */
				Trc_BCU_MalformedParameterAnnotation(address);
				if (!ALLOC_ARRAY(annotations->rawAttributeData, length + 1, U_8)) {
					return -2;
				}
				index = attributeStart; /* rewind to the start of the attribute */
				NEXT_U8(annotations->rawAttributeData[0], index); /* put in the num_parameters */
				if (0 != annotations->rawAttributeData[0]) {
					/* insert an error marker */
					annotations->rawAttributeData[1] = annotations->rawAttributeData[0];
					annotations->rawAttributeData[0] = 0;
					annotations->rawDataLength = length + 1;
				} else { /* the attribute is already marked bad */
					if (length > 1) {
						NEXT_U8(annotations->rawAttributeData[1], index);
					}
					annotations->rawDataLength = length;
				}

				/* Jazz 104170: Given that annotations->rawAttributeData[0] is set to 0
				 * in the case of malformed annotation attribute, we should also
				 * set num_parameters to 0 so as to skip this attribute when
				 * walking through attributes in ClassFileOracle::walkMethodAttributes().
				 */
				annotations->numberOfParameters = annotations->rawAttributeData[0];

				for (cursor = 2; cursor < annotations->rawDataLength; ++cursor) {
					CHECK_EOF(1);
					NEXT_U8(annotations->rawAttributeData[cursor], index);
				}
			}
		}

			break;

		case CFR_ATTRIBUTE_RuntimeInvisibleTypeAnnotations: {
			if (invisibleTypeAttributeRead) {
				/* found a second typeAnnotation attribute */
				errorCode = J9NLS_CFR_ERR_MULTIPLE_TYPE_ANNOTATIONS_ATTRIBUTES__ID;
				offset = address;
				goto _errorFound;
			}
			invisibleTypeAttributeRead = TRUE;
			if (J9_ARE_NO_BITS_SET(flags, BCT_RetainRuntimeInvisibleAttributes)) {
				if (!ALLOC(attrib, J9CfrAttribute)) { /* create dummy attribute and skip */
					return -2;
				}
				CHECK_EOF(length);
				index += length;
				break;
			} /* else FALLTHROUGH */
		}
		case CFR_ATTRIBUTE_RuntimeVisibleTypeAnnotations: {
			J9CfrAttributeRuntimeVisibleTypeAnnotations *annotations = NULL;
			U_8 *attributeStart = index;
			BOOLEAN foundError = FALSE;
			{
				if (CFR_ATTRIBUTE_RuntimeVisibleTypeAnnotations == tag) {
					if (visibleTypeAttributeRead) {
						/* found a second typeAnnotation attribute */
						errorCode = J9NLS_CFR_ERR_MULTIPLE_TYPE_ANNOTATIONS_ATTRIBUTES__ID;
						offset = address;
						goto _errorFound;
					}
					visibleTypeAttributeRead = TRUE;
				}
			}
			if (!ALLOC_CAST(attrib, J9CfrAttributeRuntimeVisibleTypeAnnotations, J9CfrAttribute)) {
				return -2;
			}
			annotations = (J9CfrAttributeRuntimeVisibleTypeAnnotations *)attrib;
			annotations->rawAttributeData = NULL;
			annotations->rawDataLength = 0; /* 0 indicates attribute is well-formed */
			CHECK_EOF(2);
			NEXT_U16(annotations->numberOfAnnotations, index);
			if (!ALLOC_ARRAY(annotations->typeAnnotations, annotations->numberOfAnnotations, J9CfrTypeAnnotation))
			{
				return -2;
			}
			typeAnnotations = annotations->typeAnnotations;
			/*
			 * we are now at the start of the first type_annotation
			 * Silently ignore errors.
			 */
			for (j = 0; j < annotations->numberOfAnnotations; j++, typeAnnotations++) {
				result = readTypeAnnotation(classfile, typeAnnotations, data, dataEnd, segment, segmentEnd, &index, &freePointer, flags);
				if (result != 0) {
					break;
				}
			}
			if ((BCT_ERR_NO_ERROR != result) || (index != end)) {
				U_32 cursor = 0;
				/*
				 * give up parsing.
				 * Copy the raw data, insert a bogus type_annotation,
				 * i.e. an illegal target_type byte immediately after the num_annotations field,
				 * to indicate that the attribute is malformed.
				 */
				Trc_BCU_MalformedTypeAnnotation(address);
				if (!ALLOC_ARRAY(annotations->rawAttributeData, length + 1, U_8)) {
					return -2;
				}
				index = attributeStart; /* rewind to the start of the attribute */
				NEXT_U16(annotations->rawAttributeData[0], index); /* put in the num_annotations */
				NEXT_U8(annotations->rawAttributeData[2], index);
				if (CFR_TARGET_TYPE_ErrorInAttribute != annotations->rawAttributeData[2]) {
					/* insert an error marker */
					annotations->rawAttributeData[3] = annotations->rawAttributeData[2];
					annotations->rawAttributeData[2] = CFR_TARGET_TYPE_ErrorInAttribute;
					annotations->rawDataLength = length + 1;
				} else { /* the attribute is already marked bad */
					NEXT_U8(annotations->rawAttributeData[3], index);
					annotations->rawDataLength = length;
				}
				for (cursor = 4; cursor < annotations->rawDataLength; ++cursor) {
					CHECK_EOF(1);
					NEXT_U8(annotations->rawAttributeData[cursor], index);
				}
			}

			result = 0;
		}
			break;

		case CFR_ATTRIBUTE_InnerClasses:
			if (!ALLOC(classes, J9CfrAttributeInnerClasses)) {
				return -2;
			}
			attrib = (J9CfrAttribute*)classes;
			CHECK_EOF(2);
			NEXT_U16(classes->numberOfClasses, index);
			if (!ALLOC_ARRAY(classes->classes, classes->numberOfClasses, J9CfrClassesEntry)) {
				return -2;
			}
			CHECK_EOF(classes->numberOfClasses << 3);
			for (j = 0; j < classes->numberOfClasses; j++) {
				NEXT_U16(classes->classes[j].innerClassInfoIndex, index);
				NEXT_U16(classes->classes[j].outerClassInfoIndex, index);
				NEXT_U16(classes->classes[j].innerNameIndex, index);
				NEXT_U16(classes->classes[j].innerClassAccessFlags, index);
			}
			break;

		case CFR_ATTRIBUTE_BootstrapMethods:
			if (bootstrapMethodAttributeRead) {
				errorCode = J9NLS_CFR_ERR_FOUND_MULTIPLE_BOOTSTRAP_METHODS_ATTRIBUTES__ID;
				offset = address;
				goto _errorFound;
			}
			bootstrapMethodAttributeRead = TRUE;
			if (!ALLOC(bootstrapMethods, J9CfrAttributeBootstrapMethods)) {
				return -2;
			}
			attrib = (J9CfrAttribute*)bootstrapMethods;
			CHECK_EOF(2);
			NEXT_U16(bootstrapMethods->numberOfBootstrapMethods, index);
			if (!ALLOC_ARRAY(bootstrapMethods->bootstrapMethods, bootstrapMethods->numberOfBootstrapMethods, J9CfrBootstrapMethod)) {
				return -2;
			}
			for (j = 0; j < bootstrapMethods->numberOfBootstrapMethods; j++) {
				CHECK_EOF(4);
				NEXT_U16(bootstrapMethods->bootstrapMethods[j].bootstrapMethodIndex, index);
				NEXT_U16(bootstrapMethods->bootstrapMethods[j].numberOfBootstrapArguments, index);
				if (!ALLOC_ARRAY(bootstrapMethods->bootstrapMethods[j].bootstrapArguments, bootstrapMethods->bootstrapMethods[j].numberOfBootstrapArguments, U_16)) {
					return -2;
				}
				CHECK_EOF(bootstrapMethods->bootstrapMethods[j].numberOfBootstrapArguments << 1);
				for (k = 0; k < bootstrapMethods->bootstrapMethods[j].numberOfBootstrapArguments; k++) {
					NEXT_U16(bootstrapMethods->bootstrapMethods[j].bootstrapArguments[k], index);
				}
			}
			break;

		case CFR_ATTRIBUTE_EnclosingMethod:
			if (!ALLOC_CAST(attrib, J9CfrAttributeEnclosingMethod, J9CfrAttribute)) {
				return -2;
			}
			CHECK_EOF(4);
			NEXT_U16(((J9CfrAttributeEnclosingMethod *) attrib)->classIndex, index);
			NEXT_U16(((J9CfrAttributeEnclosingMethod *) attrib)->methodIndex, index);
			break;

		case CFR_ATTRIBUTE_Deprecated:
			if (!ALLOC_CAST(attrib, J9CfrAttributeDeprecated, J9CfrAttribute)) {
				return -2;
			}
			CHECK_EOF(length);
			index += length;
			break;

		case CFR_ATTRIBUTE_StackMapTable:
			if (!ALLOC(stackMap, J9CfrAttributeStackMap)) {
				return -2;
			}
			attrib = (J9CfrAttribute *)stackMap;

			CHECK_EOF(2);
			NEXT_U16(stackMap->numberOfEntries, index);
			stackMap->mapLength = 0;
			if (length > 1) {
				stackMap->mapLength = length - 2;
			}

			if (!ALLOC_ARRAY(stackMap->entries, stackMap->mapLength, U_8)) {
				return -2;
			}

			CHECK_EOF(stackMap->mapLength);
			memcpy (stackMap->entries, index, stackMap->mapLength);
			index += stackMap->mapLength;
			
			break;
#if defined(J9VM_OPT_VALHALLA_NESTMATES)
		case CFR_ATTRIBUTE_MemberOfNest:
			if (nestAttributeRead) {
				errorCode = J9NLS_CFR_ERR_MULTIPLE_NEST_ATTRIBUTES__ID;
				offset = address;
				goto _errorFound;
			}

			if (!ALLOC(memberOfNest, J9CfrAttributeMemberOfNest)) {
				return -2;
			}
			nestAttributeRead = TRUE;
			attrib = (J9CfrAttribute*)memberOfNest;

			CHECK_EOF(2);
			NEXT_U16(memberOfNest->hostClassIndex, index);
			break;

		case CFR_ATTRIBUTE_NestMembers: {
			U_16 numberOfClasses;
			if (nestAttributeRead) {
				errorCode = J9NLS_CFR_ERR_MULTIPLE_NEST_ATTRIBUTES__ID;
				offset = address;
				goto _errorFound;
			}

			if (!ALLOC(nestMembers, J9CfrAttributeNestMembers)) {
				return -2;
			}
			nestAttributeRead = TRUE;
			attrib = (J9CfrAttribute*)nestMembers;

			CHECK_EOF(2);
			NEXT_U16(numberOfClasses, index);
			nestMembers->numberOfClasses = numberOfClasses;

			if (!ALLOC_ARRAY(nestMembers->classes, numberOfClasses, U_16)) {
				return -2;
			}

			CHECK_EOF(2 * numberOfClasses);

			for (j = 0; j < numberOfClasses; j++) {
				NEXT_U16(nestMembers->classes[j], index);
			}
			break;
		}
#endif /* J9VM_OPT_VALHALLA_NESTMATES */

		case CFR_ATTRIBUTE_StrippedLineNumberTable:
		case CFR_ATTRIBUTE_StrippedLocalVariableTable:
		case CFR_ATTRIBUTE_StrippedLocalVariableTypeTable:
		case CFR_ATTRIBUTE_StrippedInnerClasses:
		case CFR_ATTRIBUTE_StrippedSourceDebugExtension:
		case CFR_ATTRIBUTE_StrippedUnknown:
			if (!ALLOC(attrib, J9CfrAttribute)) {
				return -2;
			}
			CHECK_EOF(length);
			index += length;
			break;

		case CFR_ATTRIBUTE_SourceDebugExtension:
			if (sourceDebugExtensionRead) {
				/* found a redundant attribute */
				errorCode = J9NLS_CFR_ERR_MULTIPLE_SOURCE_DEBUG_EXTENSION_ATTRIBUTES__ID;
				offset = address;
				goto _errorFound;

			} else {
				sourceDebugExtensionRead = TRUE;
			}
			/* FALLTHROUGH */
		case CFR_ATTRIBUTE_StackMap: /* CLDC StackMap - Not supported in J2SE */
		case CFR_ATTRIBUTE_Unknown:
		default:
			if (!ALLOC_CAST(attrib, J9CfrAttributeUnknown, J9CfrAttribute)) {
				return -2;
			}
			if (!ALLOC_ARRAY(((J9CfrAttributeUnknown*) attrib)->value, length, U_8)) {
				return -2;
			}
			CHECK_EOF(length);
			memcpy (((J9CfrAttributeUnknown *) attrib)->value, index, length);
			index += length;
			break;
		}

		attrib->tag = tag;
		attrib->nameIndex = (U_16) name;
		attrib->length = length;
		attrib->romAddress = address;
		attributes[i] = (J9CfrAttribute *) attrib;

		if (index != end) {
			errorCode = (U_32) ((index < end) ? J9NLS_CFR_ERR_LENGTH_TOO_SMALL__ID : J9NLS_CFR_ERR_LENGTH_TOO_BIG__ID);
			offset = address + 2;
			goto _errorFound;
		}
	}

	*pAttributes = attributes;
	*pIndex = index;
	*pFreePointer = freePointer;
	return 0;

_errorFound:
	buildError((J9CfrError *) segment, errorCode, CFR_ThrowClassFormatError, offset);
	return -1;
}



/*
	Read the methods from the bytes in @data.
	Returns 0 on success, non-zero on failure.
*/

static I_32 
readMethods(J9CfrClassFile* classfile, U_8* data, U_8* dataEnd, U_8* segment, U_8* segmentEnd, U_8** pIndex, U_8** pFreePointer, U_32 flags)
{
	U_8* index = *pIndex;
	U_8* freePointer = *pFreePointer;
	J9CfrMethod* method;
	UDATA errorCode, offset;
	U_32 i;
	I_32 result;

	for (i = 0; i < classfile->methodsCount; i++) {
		U_32 j;
		UDATA syntheticFound = FALSE;

		method = &(classfile->methods[i]);
		method->romAddress = (UDATA) (index - data);

		CHECK_EOF(8);
		method->accessFlags = NEXT_U16(method->accessFlags, index) & CFR_METHOD_ACCESS_MASK;
		method->j9Flags = 0;
		NEXT_U16(method->nameIndex, index);
		NEXT_U16(method->descriptorIndex, index);
		NEXT_U16(method->attributesCount, index);

		if (!ALLOC_ARRAY(method->attributes, method->attributesCount, J9CfrAttribute*)) {
			return -2;
		}

		/* Read the attributes. */
		if ((result = readAttributes(classfile, &(method->attributes), method->attributesCount, data, dataEnd, segment, segmentEnd, &index, &freePointer, flags, &syntheticFound))!= 0) {
			return result;
		}
		if (syntheticFound) {
			method->accessFlags |= CFR_ACC_SYNTHETIC;
		}

		method->codeAttribute = NULL;
		method->exceptionsAttribute = NULL;
		method->methodParametersAttribute = NULL;
		for (j = 0; j < method->attributesCount; j++) {
			switch (method->attributes[j]->tag) {
			case CFR_ATTRIBUTE_Code:
				if (method->codeAttribute) {
					/* found a second one! */
					errorCode = J9NLS_CFR_ERR_TWO_CODE_ATTRIBUTES__ID;
					offset = method->attributes[j]->romAddress;
					goto _errorFound;
				}
				method->codeAttribute = (J9CfrAttributeCode*)(method->attributes[j]);
				break;
			case CFR_ATTRIBUTE_Exceptions:
				if (method->exceptionsAttribute) {
					/* found a second one! */
					errorCode = J9NLS_CFR_ERR_TWO_EXCEPTIONS_ATTRIBUTES__ID;
					offset = method->attributes[j]->romAddress;
					goto _errorFound;
				}
				method->exceptionsAttribute = (J9CfrAttributeExceptions*)(method->attributes[j]);
				break;
			case CFR_ATTRIBUTE_MethodParameters:
				if (method->methodParametersAttribute) {
					/* found a second one! */
					errorCode = J9NLS_CFR_ERR_TWO_METHOD_PARAMETERS_ATTRIBUTES__ID;
					offset = method->attributes[j]->romAddress;
					goto _errorFound;
				}
				method->methodParametersAttribute = (J9CfrAttributeMethodParameters*)(method->attributes[j]);
			}
		}
	}

	*pIndex = index;
	*pFreePointer = freePointer;
	return 0;

_errorFound:
	buildError((J9CfrError *) segment, errorCode, CFR_ThrowClassFormatError, offset);
	return -1;
}


/*
	Read the methods from the bytes in @data.
	Returns 0 on success, non-zero on failure.
*/

static I_32 
readFields(J9CfrClassFile* classfile, U_8* data, U_8* dataEnd, U_8* segment, U_8* segmentEnd, U_8** pIndex, U_8** pFreePointer, U_32 flags)
{
	U_8* index = *pIndex;
	U_8* freePointer = *pFreePointer;
	J9CfrField* field;
	UDATA errorCode, offset;
	U_32 i, j;
	I_32 result;

	field = classfile->fields;
	for (i = 0; i < classfile->fieldsCount; i++, field++) {
		UDATA syntheticFound = FALSE;

		field->romAddress = (UDATA) (index - data);

		CHECK_EOF(8);
		field->accessFlags = NEXT_U16(field->accessFlags, index) & CFR_FIELD_ACCESS_MASK;
		NEXT_U16(field->nameIndex, index);
		NEXT_U16(field->descriptorIndex, index);
		NEXT_U16(field->attributesCount, index);

		if (!ALLOC_ARRAY(field->attributes, field->attributesCount, J9CfrAttribute*)) {
			return -2;
		}

		/* Read the attributes. */
		if ((result = readAttributes(classfile, &(field->attributes), field->attributesCount, data, dataEnd, segment, segmentEnd, &index, &freePointer, flags, &syntheticFound)) != 0) {
			return result;
		}
		if (syntheticFound) {
			field->accessFlags |= CFR_ACC_SYNTHETIC;
		}

		field->constantValueAttribute = NULL;
		for (j = 0; j < field->attributesCount; j++) {
			if (field->attributes[j]->tag == CFR_ATTRIBUTE_ConstantValue) {
				if (field->constantValueAttribute) {
					/* found a second one! */
					errorCode = J9NLS_CFR_ERR_TWO_CONSTANT_VALUE_ATTRIBUTES__ID;
					offset = field->attributes[j]->romAddress;
					goto _errorFound;
				}
				field->constantValueAttribute = (J9CfrAttributeConstantValue*)(field->attributes[j]);
			}
		}
	}

	*pIndex = index;
	*pFreePointer = freePointer;
	return 0;

_errorFound:
	buildError((J9CfrError *) segment, errorCode, CFR_ThrowClassFormatError, offset);
	return -1;
}


/*
	Read the constant pool from the bytes in @data.
	Returns 0 on success, non-zero on failure.
*/

static I_32 
readPool(J9CfrClassFile* classfile, U_8* data, U_8* dataEnd, U_8* segment, U_8* segmentEnd, U_8** pIndex, U_8** pFreePointer)
{
	U_8* index = *pIndex;
	U_8* freePointer = *pFreePointer;
	J9CfrConstantPoolInfo* info;
	J9CfrConstantPoolInfo* previousUTF8;
	J9CfrConstantPoolInfo* previousNAT;
	U_32 size, errorCode, offset;
	U_32 i;
	I_32 verifyResult;

	/* Explicitly zero the null entry */
	info = &(classfile->constantPool[0]);
	info->tag = CFR_CONSTANT_Null;
	info->flags1 = 0;
	info->nextCPIndex = 0;
	info->slot1 = 0;
	info->slot2 = 0;
	info->bytes = 0;
	info->romAddress = 0;
	classfile->firstUTF8CPIndex = 0;
	classfile->firstNATCPIndex = 0;

	/* Read each entry. */
	for (i = 1; i < classfile->constantPoolCount;) {
		info = &(classfile->constantPool[i]);
		CHECK_EOF(1);
		NEXT_U8(info->tag, index);
		info->flags1 = 0;
		info->nextCPIndex = 0;
		info->romAddress = 0;
		switch (info->tag) {
		case CFR_CONSTANT_Utf8:
			CHECK_EOF(2);
			NEXT_U16(size, index);
			info->slot2 = 0;

			if (!ALLOC_ARRAY(info->bytes, size + 1, U_8)) {
				return -2;
			}
			CHECK_EOF(size);
			verifyResult = j9bcutil_verifyCanonisizeAndCopyUTF8(info->bytes, index, size);
			info->slot1 = (U_32) verifyResult;
			if (verifyResult < 0) {
				errorCode = J9NLS_CFR_ERR_BAD_UTF8__ID;
				offset = (U_32) (index - data - 1);
				goto _errorFound;
			}
			info->bytes[info->slot1] = (U_8) '\0';
			/* correct for compression */
			freePointer -= (size - info->slot1);
			index += size;
			if (0 == classfile->firstUTF8CPIndex) {
				classfile->firstUTF8CPIndex = i;
				previousUTF8 = info;
			} else {
				previousUTF8->nextCPIndex = (U_16)i;
				previousUTF8 = info;
			}
			i++;
			break;

		case CFR_CONSTANT_Integer:
		case CFR_CONSTANT_Float:
			CHECK_EOF(4);
			NEXT_U32(info->slot1, index);
			info->slot2 = 0;
			i++;
			break;
	
		case CFR_CONSTANT_Long:
		case CFR_CONSTANT_Double:
			CHECK_EOF(8);
#ifdef J9VM_ENV_LITTLE_ENDIAN
			NEXT_U32(info->slot2, index);
			NEXT_U32(info->slot1, index);
#else
			NEXT_U32(info->slot1, index);
			NEXT_U32(info->slot2, index);
#endif
			i++;
			classfile->constantPool[i].tag = CFR_CONSTANT_Null;
			i++;
			if (i > classfile->constantPoolCount) {
				/* Incomplete long or double constant. This means that n+1 is out of range. */
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				offset = (U_32) (index - data - 1);
				goto _errorFound;
			}
			break;

		case CFR_CONSTANT_MethodType:
			if (classfile->majorVersion < 51) {
				errorCode = J9NLS_CFR_ERR_CP_ENTRY_INVALID_BEFORE_V51__ID;
				offset = (U_32) (index - data - 1);
				goto _errorFound;
			}
		case CFR_CONSTANT_Class:
		case CFR_CONSTANT_String:
			CHECK_EOF(2);
			NEXT_U16(info->slot1, index);
			info->slot2 = 0;
			i++;
			break;

		case CFR_CONSTANT_InvokeDynamic:
			if (classfile->majorVersion < 51) {
				errorCode = J9NLS_CFR_ERR_CP_ENTRY_INVALID_BEFORE_V51__ID;
				offset = (U_32) (index - data - 1);
				goto _errorFound;
			}
		case CFR_CONSTANT_Fieldref:
		case CFR_CONSTANT_Methodref:
		case CFR_CONSTANT_InterfaceMethodref:
			CHECK_EOF(4);
			NEXT_U16(info->slot1, index);
			NEXT_U16(info->slot2, index);
			i++;
			break;

		case CFR_CONSTANT_NameAndType:
			CHECK_EOF(4);
			NEXT_U16(info->slot1, index);
			NEXT_U16(info->slot2, index);
			if (0 == classfile->firstNATCPIndex) {
				classfile->firstNATCPIndex = i;
				previousNAT = info;
			} else {
				previousNAT->nextCPIndex = (U_16)i;
				previousNAT = info;
			}
			i++;
			break;

		case CFR_CONSTANT_MethodHandle:
			if (classfile->majorVersion < 51) {
				errorCode = J9NLS_CFR_ERR_CP_ENTRY_INVALID_BEFORE_V51__ID;
				offset = (U_32) (index - data - 1);
				goto _errorFound;
			}
			CHECK_EOF(3);
			NEXT_U8(info->slot1, index);
			NEXT_U16(info->slot2, index);
			i++;
			break;

		case CFR_CONSTANT_Module:
		case CFR_CONSTANT_Package:
			if (classfile->majorVersion < 53) {
				errorCode = J9NLS_CFR_ERR_CP_ENTRY_INVALID_BEFORE_V53__ID;
				offset = (U_32)(index - data - 1);
				goto _errorFound;
			}

			if (!J9_ARE_ALL_BITS_SET(classfile->accessFlags, CFR_ACC_MODULE)) {
				if (J9_ARE_ALL_BITS_SET(info->tag, CFR_CONSTANT_Module)) {
					errorCode = J9NLS_CFR_ERR_CONSTANT_MODULE_OUTSIDE_MODULE__ID;
				} else {
					errorCode = J9NLS_CFR_ERR_CONSTANT_PACKAGE_OUTSIDE_MODULE__ID;
				}
				offset = (U_32)(index - data - 1);
				goto _errorFound;
			}
			CHECK_EOF(2);
			NEXT_U16(info->slot1, index);
			i++;
			break;

		default:
			errorCode = J9NLS_CFR_ERR_UNKNOWN_CONSTANT__ID;
			offset = (U_32) (index - data - 1);
			goto _errorFound;
		}
	}
	*pIndex = index;
	*pFreePointer = freePointer;
	return 0;

_errorFound:
	buildError((J9CfrError *) segment, errorCode, CFR_ThrowClassFormatError, offset);
	return -1;
}


/*
	Check the validity of the constant pool.
	@param maxBootstrapMethodIndex (out) Pass back the largest bootstrap table index used by invokeDynamic
	@return 0 on success, non-zero on failure.
*/

static I_32 
checkPool(J9CfrClassFile* classfile, U_8* segment, U_8* poolStart, I_32 *maxBootstrapMethodIndex, U_32 vmVersionShifted, U_32 flags)
{
	J9CfrConstantPoolInfo* info;
	J9CfrConstantPoolInfo* utf8; 
	J9CfrConstantPoolInfo* cpBase; 
	U_32 count, i, errorCode;
	U_8* index;

	cpBase = classfile->constantPool;
	count = classfile->constantPoolCount;
	index = poolStart;

	info = &cpBase[1];

	for (i = 1; i < count; i++, info++) {

		switch (info->tag) {

		case CFR_CONSTANT_Utf8:
			/* Check format? */
			index += info->slot1 + 3;
			break;

		case CFR_CONSTANT_Integer:
		case CFR_CONSTANT_Float:
			index += 5;
			break;

		case CFR_CONSTANT_Long:
		case CFR_CONSTANT_Double:
			index += 9;
			i++;
			info++;
			break;

		case CFR_CONSTANT_Class:
			/* Must be a UTF8. */
			if ((!(info->slot1)) || (info->slot1 > count)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}

			utf8 = &cpBase[info->slot1];
			if (utf8->tag != CFR_CONSTANT_Utf8) {
				errorCode = J9NLS_CFR_ERR_BAD_NAME_INDEX__ID;
				goto _errorFound;
			}

			index += 3;
			break;
		
		case CFR_CONSTANT_String:
			/* Must be a UTF8. */
			if ((!(info->slot1)) || (info->slot1 > count)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			if (cpBase[info->slot1].tag != CFR_CONSTANT_Utf8) {
				errorCode = J9NLS_CFR_ERR_BAD_STRING_INDEX__ID;
				goto _errorFound;
			}					
			index += 3;
			break;

		case CFR_CONSTANT_Fieldref:
		case CFR_CONSTANT_Methodref:
		case CFR_CONSTANT_InterfaceMethodref:
			if (!(info->slot1) || (info->slot1 > count)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			if (cpBase[info->slot1].tag != CFR_CONSTANT_Class) {
				errorCode = J9NLS_CFR_ERR_BAD_CLASS_INDEX__ID;
				goto _errorFound;
			}					
					
			if (!(info->slot2) || (info->slot2 > count)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			if (cpBase[info->slot2].tag != CFR_CONSTANT_NameAndType) {
				errorCode = J9NLS_CFR_ERR_BAD_NAME_AND_TYPE_INDEX__ID;
				goto _errorFound;
			}					
			index += 5;
			break;

		case CFR_CONSTANT_NameAndType:
			if ((!(info->slot1)) || (info->slot1 > count)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			if (cpBase[info->slot1].tag != CFR_CONSTANT_Utf8) {
				errorCode = J9NLS_CFR_ERR_BAD_NAME_INDEX__ID;
				goto _errorFound;
			}					
					
			if ((!(info->slot2)) || (info->slot2 > count)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			if (cpBase[info->slot2].tag != CFR_CONSTANT_Utf8) {
				errorCode = J9NLS_CFR_ERR_BAD_DESCRIPTOR_INDEX__ID;
				goto _errorFound;
			}					
			index += 5;
			break;


		case CFR_CONSTANT_MethodType:
			if (!(info->slot1) || (info->slot1 > count)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			if (cpBase[info->slot1].tag != CFR_CONSTANT_Utf8) {
				errorCode = J9NLS_CFR_ERR_BAD_DESCRIPTOR_INDEX__ID;
				goto _errorFound;
			}
			index += 3;
			break;

		case CFR_CONSTANT_MethodHandle:
			/* Format requires that slot1 is between 
			 * 1 and 9 and that slot2 is either a field
			 * or method ref or interface method ref.
			 * Checking for the correct 'kind' (slot1) occurs
			 * in the check for the correct 'ref' (slot2).
			 */
			 
			/* Ensure the ref field is a valid cpEntry index */
			if (!(info->slot2) || (info->slot2 > count)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			/* Must be either FieldRef or MethodRef or InterfaceMethodRef */
			switch(cpBase[info->slot2].tag){
				case CFR_CONSTANT_Fieldref:
					/* Kinds 1 - 4 must reference fields */
					if ((info->slot1 < 1) || (info->slot1 > 4)) {
						errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
						goto _errorFound;
					}
					break;
				case CFR_CONSTANT_Methodref:
					/* Kinds 5 - 8 must reference methodrefs */
					if ((info->slot1 < MH_REF_INVOKEVIRTUAL) || (info->slot1 > MH_REF_NEWINVOKESPECIAL)) {
						errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
						goto _errorFound;
					}
					break;
				case CFR_CONSTANT_InterfaceMethodref:
					/* Kind 9 must reference interface methodrefs.
					 * If major version >= 52.0, Kind 6 and 7 can also reference interface methodrefs.
					 */
					if ((info->slot1 != MH_REF_INVOKEINTERFACE)
						&& (!((classfile->majorVersion >= 52)
							&& ((info->slot1 == MH_REF_INVOKESTATIC) || (info->slot1 == MH_REF_INVOKESPECIAL))))
					) {
						errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
						goto _errorFound;
					}
					break;
				default:
					errorCode = J9NLS_CFR_ERR_BAD_METHODHANDLE_REF_INDEX__ID;
					goto _errorFound;
			}

			index += 4;
			break;

		case CFR_CONSTANT_InvokeDynamic:
			/* Slot1 is an index into the BootstrapMethods_attribute - can't be validated yet */
			if (((I_32) info->slot1) > *maxBootstrapMethodIndex) {
				*maxBootstrapMethodIndex = info->slot1;
			}

			/* Slot2 is a NAS */
			if (!(info->slot2) || (info->slot2 > count)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			if (cpBase[info->slot2].tag != CFR_CONSTANT_NameAndType) {
				errorCode = J9NLS_CFR_ERR_BAD_NAME_AND_TYPE_INDEX__ID;
				goto _errorFound;
			}
			index += 5;
			break;

		case CFR_CONSTANT_Module:
		case CFR_CONSTANT_Package:
			index += 3;
			break;

		default:
			errorCode = J9NLS_CFR_ERR_UNKNOWN_CONSTANT__ID;
			goto _errorFound;
		}
	}

	return 0;

_errorFound:

	buildError((J9CfrError *) segment, errorCode, CFR_ThrowClassFormatError, (UDATA) (index - poolStart + 10));
	return -1;
}

/*
	Verify (pass 1) the fields in @classfile.
	Returns 0 on success, non-zero on failure.
*/
static I_32 
checkFields(J9CfrClassFile * classfile, U_8 * segment, U_32 flags)
{
	J9CfrField *field;
	U_32 value, maskedValue, errorCode, offset = 0;
	U_32 i;
	U_8 sigChar, sigTag, constantTag;

	for (i = 0; i < classfile->fieldsCount; i++) {
		field = &(classfile->fields[i]);
		value = field->accessFlags & CFR_FIELD_ACCESS_MASK;

		if ((flags & BCT_MajorClassFileVersionMask) < BCT_Java5MajorVersionShifted) {
			value &= ~CFR_FIELD_ACCESS_NEWJDK5_MASK;
		}


		if (classfile->accessFlags & CFR_ACC_INTERFACE) {
			/* check that there are no flags set that are not allowed AND the required flags are set */
			if ((value & ~CFR_INTERFACE_FIELD_ACCESS_MASK) ||
				 ((value & CFR_INTERFACE_FIELD_ACCESS_REQUIRED) != CFR_INTERFACE_FIELD_ACCESS_REQUIRED)) {
				errorCode = J9NLS_CFR_ERR_INTERFACE_FIELD__ID;
				goto _errorFound;
			}
		}

		maskedValue = value & CFR_PUBLIC_PRIVATE_PROTECTED_MASK;
		/* Check none or only one of the access flags set - result is 0 if no bits or only 1 bit is set */
		if (maskedValue & (maskedValue - 1)) {
			errorCode = J9NLS_CFR_ERR_ACCESS_CONFLICT_FIELD__ID;
			goto _errorFound;
		}

		if ((value & (U_32) (CFR_ACC_FINAL | CFR_ACC_VOLATILE)) == (U_32) (CFR_ACC_FINAL | CFR_ACC_VOLATILE)) {
			errorCode = J9NLS_CFR_ERR_FINAL_VOLATILE_FIELD__ID;
			goto _errorFound;
		}

		offset = 2;
		value = field->nameIndex;
		if ((!value) || (value > classfile->constantPoolCount)) {
			errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
			goto _errorFound;
		}
		if (classfile->constantPool[value].tag != CFR_CONSTANT_Utf8) {
			errorCode = J9NLS_CFR_ERR_BAD_NAME_INDEX__ID;
			goto _errorFound;
		}
		/* field names are checked for well-formedness by the static verifier */

		offset = 4;
		value = field->descriptorIndex;
		if ((!value) || (value > classfile->constantPoolCount)) {
			errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
			goto _errorFound;
		}
		if (classfile->constantPool[value].tag != CFR_CONSTANT_Utf8) {
			errorCode = J9NLS_CFR_ERR_BAD_DESCRIPTOR_INDEX__ID;
			goto _errorFound;
		}
		/* field signature checked in j9bcv_verifyClassStructure */
		
		/* Check the attributes. */
		if (checkAttributes(classfile, field->attributes, field->attributesCount, segment, -1, OUTSIDE_CODE, flags)) {
			return -1;
		}

		/* Ensure that the constantValueAttribute is appropriate. */
		if (field->constantValueAttribute && (field->accessFlags & CFR_ACC_FINAL) &&
			(field->accessFlags & CFR_ACC_STATIC)) {
			sigChar = classfile->constantPool[field->descriptorIndex].bytes[0];
			constantTag = classfile->constantPool[field->constantValueAttribute->constantValueIndex].tag;
			if ((sigChar < 'A') || (sigChar > 'Z')) {
				errorCode = J9NLS_CFR_ERR_INCOMPATIBLE_CONSTANT__ID;
				goto _errorFound;
			}
			sigTag = (U_32) cpTypeCharConversion[sigChar - 'A'];
			if (sigTag == constantTag) {
				if (sigTag == CFR_CONSTANT_String) {
					/* Reject a String ConstantValue for anything but java.lang.String. */
					if (!utf8Equal(&classfile->constantPool[field->descriptorIndex], "Ljava/lang/String;", 18)) {
						errorCode = J9NLS_CFR_ERR_INCOMPATIBLE_CONSTANT__ID;
						goto _errorFound;
					}
				}
			} else {
				errorCode = J9NLS_CFR_ERR_INCOMPATIBLE_CONSTANT__ID;
				goto _errorFound;
			}
		}
	}

	return 0;

_errorFound:

	buildError((J9CfrError *) segment, errorCode, CFR_ThrowClassFormatError, field->romAddress + offset);
	return -1;
}



/*
	Check the methods in @classfile.
	Returns 0 on success, non-zero on failure.
*/

static I_32 
checkMethods(J9CfrClassFile* classfile, U_8* segment, U_32 vmVersionShifted, U_32 flags)
{
	J9CfrMethod* method;
	U_32 value = 0;
	U_32 maskedValue = 0;
	U_32 errorCode = 0;
	U_32 offset = 0;
	U_32 i;
	BOOLEAN nameIndexOK;
	U_32 classfileVersion = flags & BCT_MajorClassFileVersionMask;

	for (i = 0; i < classfile->methodsCount; i++) {
		method = &(classfile->methods[i]);

		/* Can we trust the name index? */
		nameIndexOK = TRUE;
		value = method->nameIndex;
		if ((!value) || (value > classfile->constantPoolCount)) {
			nameIndexOK = FALSE;
		} else if (classfile->constantPool[value].tag != CFR_CONSTANT_Utf8) {
			nameIndexOK = FALSE;
		}

		value = method->accessFlags & CFR_METHOD_ACCESS_MASK;

		if (classfileVersion < BCT_Java5MajorVersionShifted) {
			value &= ~CFR_METHOD_ACCESS_NEWJDK5_MASK;
		}

		/* ACC_STRICT doesn't exist for older .class files prior to Java 2 */
		/* Xfuture qualification added to pass CLDC TCK mthacc00601m5_1 - wants 45.3 class file rejected with ACC_STRICT set */
		/* Test exclusion request pending - will revert on success */
		if ((classfileVersion < BCT_Java2MajorVersionShifted) && ((flags & CFR_Xfuture) == 0)) {
			value &= ~CFR_ACC_STRICT;
		}

		/* the rules don't apply to <clinit> (see JVMS 4.6)  */
		if (nameIndexOK && utf8Equal(&classfile->constantPool[method->nameIndex], "<clinit>", 8)) {
			/* JVMS 4.6: Methods: access_flags: 
			 * 		the only flag that we're supposed to honour on <clinit> is ACC_STRICT.
			 * JVMS 2.9: Specially named initialization Methods
			 * 		"initialization method... is static, takes no args..."  Footnote states other
			 * 		methods named <clinit> are ignored.
			 */
			/*
			 * JVMS 2.9 Java SE 8 Edition:
			 * In a class file whose version number is 51.0 or above, the method must
			 * additionally have its ACC_STATIC flag (Section 4.6 Methods) set in order to be the class or interface
			 * initialization method.
			 */
			if (classfileVersion < BCT_Java7MajorVersionShifted) {
				method->accessFlags |= CFR_ACC_STATIC;

			/* Leave this here to find usages of the following check:
			 * if (J2SE_VERSION(vm) >= J2SE_19) { 
			 */
			} else if (vmVersionShifted >= BCT_Java9MajorVersionShifted) {
				if (J9_ARE_NO_BITS_SET(method->accessFlags, CFR_ACC_STATIC)) {
					/* missing code attribute  detected elsewhere */
					errorCode = J9NLS_CFR_ERR_CLINIT_NOT_STATIC__ID;
					goto _errorFound;
				}
			}

			method->accessFlags &= CFR_CLINIT_METHOD_ACCESS_MASK;
			goto _nameCheck;

		} 

		if (nameIndexOK && utf8Equal(&classfile->constantPool[method->nameIndex], "<init>", 6)) {

			/* check no invalid flags set */
			if (value & ~CFR_INIT_METHOD_ACCESS_MASK) {
				errorCode = J9NLS_CFR_ERR_INIT_METHOD__ID;
				goto _errorFound;
			}

			/* Java SE 9 Edition:
			 * A method is an instance initialization method if
			 * it is defined in a class (not an interface).
			 */
			if (vmVersionShifted >= BCT_Java9MajorVersionShifted) {
				if (classfile->accessFlags & CFR_ACC_INTERFACE) {
					errorCode = J9NLS_CFR_ERR_INIT_ILLEGAL_IN_INTERFACE__ID;
					goto _errorFound;
				}
			}
		}

		/* Check interface-method-only access flag constraints. */
		if (classfile->accessFlags & CFR_ACC_INTERFACE) {
			if (classfileVersion < BCT_Java8MajorVersionShifted) {
				U_32 mask = CFR_INTERFACE_METHOD_ACCESS_MASK;
				/* Xfuture maintains checks for all the illegal qualifiers for interface methods */
				/* Make the normal mode skip checks for some interface method flags */
				if ((flags & CFR_Xfuture) == 0) {
					/* Contrary to the spec, ACC_STRICT is allowed by Sun for all classes up to and including Java 1.5 */
					/* ACC_SYNCHRONIZED is also allowed by Sun, but only for classes before Java 1.5. */
					mask |= (CFR_ACC_STRICT | CFR_ACC_SYNCHRONIZED);
				}
				/* check that there are no flags set that are not allowed AND the required flags are set */
				if ((value & ~mask) || ((value & CFR_INTERFACE_METHOD_ACCESS_REQUIRED) != CFR_INTERFACE_METHOD_ACCESS_REQUIRED)) {
					errorCode = J9NLS_CFR_ERR_INTERFACE_METHOD__ID;
					goto _errorFound;
				}
			} else {
				/* class file version >= 52 (Java 8) */
				const U_32 illegalMask = CFR_ACC_PROTECTED | CFR_ACC_FINAL | CFR_ACC_NATIVE | CFR_ACC_SYNCHRONIZED;
				const U_32 publicPrivateMask = CFR_ACC_PUBLIC | CFR_ACC_PRIVATE;
				/* Use 'n & (n - 1)' to determine if multiple bits are set which test if this is a power of two */
				const U_32 maskedValue = value & publicPrivateMask;
				BOOLEAN oneOfPublicOrPrivate = (maskedValue != 0) && (((maskedValue) & ((maskedValue) - 1)) == 0);

				if ((0 != (value & illegalMask)) || !oneOfPublicOrPrivate) {
					errorCode = J9NLS_CFR_ERR_INTERFACE_METHOD_JAVA_8__ID;
					goto _errorFound;
				}
				if ((CFR_ACC_ABSTRACT == (value & CFR_ACC_ABSTRACT)) && (0 != (value & ~CFR_ABSTRACT_METHOD_ACCESS_MASK))) {
					errorCode = J9NLS_CFR_ERR_INTERFACE_METHOD_JAVA_8__ID;
					goto _errorFound;
				}
			}
			goto _nameCheck;
		}

		/* handling method of generic classes (non-interface) */
		if (value & CFR_ACC_ABSTRACT) {
 			/* check abstract methods */
			if (value & ~CFR_ABSTRACT_METHOD_ACCESS_MASK) {
				errorCode = J9NLS_CFR_ERR_ABSTRACT_METHOD__ID;
				goto _errorFound;
			}
		}

		/* Check access flag constraints. */
		maskedValue = value & CFR_PUBLIC_PRIVATE_PROTECTED_MASK;

		/* Check none or only one of the access flags set - result is 0 if no bits or only 1 bit is set */
		if (maskedValue & (maskedValue - 1)) {
			errorCode = J9NLS_CFR_ERR_ACCESS_CONFLICT_METHOD__ID;
			goto _errorFound;
		}
		
_nameCheck:

		offset = 2;
		/* Name check. */
		value = method->nameIndex;
		if (!nameIndexOK) {
			if ((!value) || (value > classfile->constantPoolCount)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			if (classfile->constantPool[value].tag != CFR_CONSTANT_Utf8) {
				errorCode = J9NLS_CFR_ERR_BAD_NAME_INDEX__ID;
				goto _errorFound;
			}				
		}
		/* method names are checked for well-formedness by the static verifier */

		offset = 4;
		/* Check signature. */
		value = method->descriptorIndex;
		if ((!value) || (value > classfile->constantPoolCount)) {
			errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
			goto _errorFound;
		}
		if (classfile->constantPool[value].tag != CFR_CONSTANT_Utf8) {
			errorCode = J9NLS_CFR_ERR_BAD_DESCRIPTOR_INDEX__ID;
			goto _errorFound;
		}	
		/* staticverify.c checks the method signature */
		/* Check the attributes. */
		if (checkAttributes(classfile, method->attributes, method->attributesCount, segment, -1, OUTSIDE_CODE, flags)) {
			return -1;
		}

		if (method->codeAttribute) {
			if (method->codeAttribute->codeLength > 65535) {
				errorCode = J9NLS_CFR_ERR_CODE_ARRAY_TOO_LARGE__ID;
				goto _errorFound;
			}
			if (J9_ARE_ANY_BITS_SET(method->accessFlags, CFR_ACC_NATIVE | CFR_ACC_ABSTRACT)) {
				errorCode = J9NLS_CFR_ERR_CODE_FOR_ABSTRACT_OR_NATIVE__ID;
				goto _errorFound;
			}
		}

		/* 97986 check the validity in ClassFileOracle::walkMethodMethodParametersAttribute */
	}
	return 0;

_errorFound:

	buildError((J9CfrError *) segment, errorCode, CFR_ThrowClassFormatError, method->romAddress + offset);
	return -1;
}


/*
	Read the attributes from the bytes in @data.
	@param maxBootstrapMethodIndex maximum bootstrap method index used by InvokeDynamic.  Set to -1 to ignore.
	@return 0 on success, non-zero on failure.
*/
static I_32 
checkAttributes(J9CfrClassFile* classfile, J9CfrAttribute** attributes, U_32 attributesCount, U_8* segment, I_32 maxBootstrapMethodIndex, U_32 extra, U_32 flags)
{
	J9CfrAttribute* attrib;
	J9CfrAttributeCode* code;
	J9CfrAttributeExceptions* exceptions;
	J9CfrExceptionTableEntry* exception;
	J9CfrAttributeInnerClasses* classes = NULL;
	J9CfrAttributeEnclosingMethod* enclosing = NULL;
	J9CfrConstantPoolInfo* cpBase;
	U_32 value, errorCode, errorType, cpCount;
	U_32 i, j, k;
	UDATA foundStackMap = FALSE;
	BOOLEAN bootstrapMethodAttributeRead = FALSE;

	errorType = CFR_ThrowClassFormatError;
	cpBase = classfile->constantPool;
	cpCount = (U_32) classfile->constantPoolCount;

	for(i = 0; i < attributesCount; i++) {
		attrib = attributes[i];
		switch(attrib->tag) {
		case CFR_ATTRIBUTE_SourceFile:
			value = ((J9CfrAttributeSourceFile*)attrib)->sourceFileIndex;
			if((!value)||(value > cpCount)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}				
			if(cpBase[value].tag != CFR_CONSTANT_Utf8) {
				errorCode = J9NLS_CFR_ERR_SOURCE_FILE_INDEX__ID;
				goto _errorFound;
			}
			break;

		case CFR_ATTRIBUTE_Signature:
			value = ((J9CfrAttributeSignature*)attrib)->signatureIndex;
			if((0 == value)||(value > cpCount)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}				
			if(cpBase[value].tag != CFR_CONSTANT_Utf8) {
				errorCode = J9NLS_CFR_ERR_BC_INVALID_SIG__ID;
				goto _errorFound;
			}
			break;

		case CFR_ATTRIBUTE_ConstantValue:
			value = ((J9CfrAttributeConstantValue*)attrib)->constantValueIndex;
			if((0 == value)||(value > cpCount)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			switch(cpBase[value].tag) {
			case CFR_CONSTANT_Integer:
			case CFR_CONSTANT_Float:
			case CFR_CONSTANT_Long:
			case CFR_CONSTANT_Double:
			case CFR_CONSTANT_String:
				break;

			default:
				errorCode = J9NLS_CFR_ERR_CONSTANT_VALUE_INDEX__ID;
				goto _errorFound;
			}
			break;

		case CFR_ATTRIBUTE_Code:
			code = (J9CfrAttributeCode*)attrib;
			if(code->codeLength == 0) {
				errorCode = J9NLS_CFR_ERR_CODE_ARRAY_EMPTY__ID;
				goto _errorFound;
			}					

			for(j = 0; j < code->exceptionTableLength; j++) {
				exception = &(code->exceptionTable[j]);
				value = exception->catchType;
				if(value > cpCount) {
					errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
					goto _errorFound;
				}
				if((0 != value) && (cpBase[value].tag != CFR_CONSTANT_Class)) {
					errorCode = J9NLS_CFR_ERR_CATCH_NOT_CLASS__ID;
					goto _errorFound;
				}
			}
					
			if(checkAttributes(classfile, code->attributes, code->attributesCount, segment, -1, code->codeLength, flags)) {
					return -1;
			}
			break;

		case CFR_ATTRIBUTE_Exceptions:
			exceptions = (J9CfrAttributeExceptions*)attrib;
			for(j = 0; j < exceptions->numberOfExceptions; j++) {
				value = exceptions->exceptionIndexTable[j];
				if((0 == value)||(value > cpCount)) {
					errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
					goto _errorFound;
				}
				if(cpBase[value].tag != CFR_CONSTANT_Class) {
					errorCode = J9NLS_CFR_ERR_EXCEPTION_NOT_CLASS__ID;
					goto _errorFound;
				}
			}
			break;

		case CFR_ATTRIBUTE_MethodParameters:
		{
			/* 97986 Errors here are not fatal. Check the validity in ClassFileOracle::walkMethodMethodParametersAttribute */
			break;
		}
		case CFR_ATTRIBUTE_LineNumberTable:
			value = ((J9CfrAttributeLineNumberTable*)attrib)->lineNumberTableLength;
			for(j = 0; j < value; j++) {
				if(((J9CfrAttributeLineNumberTable*)attrib)->lineNumberTable[j].startPC >= extra) {
					errorCode = J9NLS_CFR_ERR_LINE_NUMBER_PC__ID;
					goto _errorFound;
				}
			}
			break;

		case CFR_ATTRIBUTE_LocalVariableTable:
			for(j = 0; j < ((J9CfrAttributeLocalVariableTable*)attrib)->localVariableTableLength; j++) {
				value = ((J9CfrAttributeLocalVariableTable*)attrib)->localVariableTable[j].startPC;
				if(value > extra) {
					errorCode = J9NLS_CFR_ERR_LOCAL_VARIABLE_START__ID;
					goto _errorFound;
				}							

				if(((J9CfrAttributeLocalVariableTable*)attrib)->localVariableTable[j].length + value > extra) {
					errorCode = J9NLS_CFR_ERR_LOCAL_VARIABLE_LENGTH__ID;
					goto _errorFound;
				}							

				value = ((J9CfrAttributeLocalVariableTable*)attrib)->localVariableTable[j].nameIndex;
				if((0 == value)||(value > cpCount)) {
					errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
					goto _errorFound;
				}
				if((cpBase)&&(cpBase[value].tag != CFR_CONSTANT_Utf8)) {
					errorCode = J9NLS_CFR_ERR_LOCAL_VARIABLE_NAME_NOT_UTF8__ID;
					goto _errorFound;
				}

				if ((flags & CFR_Xfuture) && (bcvCheckName(&cpBase[value]))) {
					errorCode = J9NLS_CFR_ERR_LOCAL_VARIABLE_NAME__ID;
					goto _errorFound;
				}

				value = ((J9CfrAttributeLocalVariableTable*)attrib)->localVariableTable[j].descriptorIndex;
				if((0 == value)||(value > cpCount)) {
					errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
					goto _errorFound;
				}
				if((cpBase)&&(cpBase[value].tag != CFR_CONSTANT_Utf8)) {
					errorCode = J9NLS_CFR_ERR_LOCAL_VARIABLE_SIGNATURE_NOT_UTF8__ID;
					goto _errorFound;
				}
				if(j9bcv_checkFieldSignature(&cpBase[value], 0)) {
					errorCode = J9NLS_CFR_ERR_LOCAL_VARIABLE_SIGNATURE_INVALID__ID;
					goto _errorFound;
				}
			}
			break;

		case CFR_ATTRIBUTE_LocalVariableTypeTable:
			for(j = 0; j < ((J9CfrAttributeLocalVariableTypeTable*)attrib)->localVariableTypeTableLength; j++) {
				value = ((J9CfrAttributeLocalVariableTypeTable*)attrib)->localVariableTypeTable[j].startPC;
				if(value > extra) {
					errorCode = J9NLS_CFR_ERR_LOCAL_VARIABLE_START__ID;
					goto _errorFound;
				}							

				if(((J9CfrAttributeLocalVariableTypeTable*)attrib)->localVariableTypeTable[j].length + value > extra) {
					errorCode = J9NLS_CFR_ERR_LOCAL_VARIABLE_LENGTH__ID;
					goto _errorFound;
				}							

				value = ((J9CfrAttributeLocalVariableTypeTable*)attrib)->localVariableTypeTable[j].nameIndex;
				if((0 == value)||(value > cpCount)) {
					errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
					goto _errorFound;
				}
				if((cpBase)&&(cpBase[value].tag != CFR_CONSTANT_Utf8)) {
					errorCode = J9NLS_CFR_ERR_LOCAL_VARIABLE_NAME_NOT_UTF8__ID;
					goto _errorFound;
				}

				value = ((J9CfrAttributeLocalVariableTypeTable*)attrib)->localVariableTypeTable[j].signatureIndex;
				if((0 == value)||(value > cpCount)) {
					errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
					goto _errorFound;
				}
				if((cpBase)&&(cpBase[value].tag != CFR_CONSTANT_Utf8)) {
					errorCode = J9NLS_CFR_ERR_LOCAL_VARIABLE_SIGNATURE_NOT_UTF8__ID;
					goto _errorFound;
				}
			}
			break;

		case CFR_ATTRIBUTE_InnerClasses:
			if (classes != NULL) {
				errorCode = J9NLS_CFR_ERR_MULTIPLE_INNER_CLASS_ATTRIBUTES__ID;
				goto _errorFound;
			}
			classes = (J9CfrAttributeInnerClasses*)attrib;
			for(j = 0; j < classes->numberOfClasses; j++) {
				value = classes->classes[j].innerClassInfoIndex;
				if((0 == value)||(value > cpCount)) {
					errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
					goto _errorFound;
				}
				if(cpBase[value].tag != CFR_CONSTANT_Class) {
					errorCode = J9NLS_CFR_ERR_INNER_CLASS_NOT_CLASS__ID;
					goto _errorFound;
				}
				/* Check class name integrity? */

				value = classes->classes[j].outerClassInfoIndex;
				if((0 != value) && (value > cpCount)) {
					errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
					goto _errorFound;
				}
				if(value&&(cpBase[value].tag != CFR_CONSTANT_Class)) {
					errorCode = J9NLS_CFR_ERR_OUTER_CLASS_NOT_CLASS__ID;
					goto _errorFound;
				}
				/* Check class name integrity? */

				value = classes->classes[j].innerNameIndex;
				if((0 != value) && (value > cpCount)) {
					errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
					goto _errorFound;
				}
				if((0 != value) && (cpBase[value].tag != CFR_CONSTANT_Utf8)) {
					errorCode = J9NLS_CFR_ERR_INNER_NAME_NOT_UTF8__ID;
					goto _errorFound;
				}
				/* Check class name integrity? */
			}

			/* Duplicate class check */
			for(j = 0; j < classes->numberOfClasses; j++) {
				for(k = j + 1; k < classes->numberOfClasses; k++) {
					if (classes->classes[j].innerClassInfoIndex == classes->classes[k].innerClassInfoIndex) {
						errorCode = J9NLS_CFR_ERR_DUPLICATE_INNER_CLASS_ENTRY__ID;
						goto _errorFound;
					}
				}
			}
			break;

		case CFR_ATTRIBUTE_EnclosingMethod:
			if (enclosing != NULL) {
				errorCode = J9NLS_CFR_ERR_MULTIPLE_ENCLOSING_METHOD_ATTRIBUTES__ID;
				goto _errorFound;
			}
			enclosing = (J9CfrAttributeEnclosingMethod*)attrib;
			value = enclosing->classIndex;
			if((0 == value) || (value > cpCount)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			if(cpBase[value].tag != CFR_CONSTANT_Class) {
				errorCode = J9NLS_CFR_ERR_ENCLOSING_METHOD_CLASS_INDEX_NOT_CLASS__ID;
				goto _errorFound;
			}

			value = enclosing->methodIndex;
			if(value > cpCount) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			if((0 != value) && (cpBase[value].tag != CFR_CONSTANT_NameAndType)) {
				errorCode = J9NLS_CFR_ERR_ENCLOSING_METHOD_METHOD_INDEX_NOT_NAME_AND_TYPE__ID;
				goto _errorFound;
			}
			break;

		case CFR_ATTRIBUTE_BootstrapMethods:
			{
				J9CfrAttributeBootstrapMethods* bootstrapMethods = (J9CfrAttributeBootstrapMethods *) attrib;

				if (maxBootstrapMethodIndex >= (I_32) bootstrapMethods->numberOfBootstrapMethods) {
					errorCode = J9NLS_CFR_ERR_BOOTSTRAP_METHOD_TABLE_ABSENT_OR_WRONG_SIZE__ID;
					goto _errorFound;
				}
				bootstrapMethodAttributeRead = TRUE;
				for (j = 0; j < bootstrapMethods->numberOfBootstrapMethods; j++) {
					value = bootstrapMethods->bootstrapMethods[j].bootstrapMethodIndex;
					if((0 == value) || (value > cpCount)) {
						errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
						goto _errorFound;
					}
					if(cpBase[value].tag != CFR_CONSTANT_MethodHandle) {
						errorCode = J9NLS_CFR_ERR_BOOTSTRAP_METHODHANDLE__ID;
						goto _errorFound;
					}
				}
			}
			break;

		case CFR_ATTRIBUTE_StackMapTable:
			if (extra == OUTSIDE_CODE) {
				errorCode = J9NLS_CFR_ERR_STACK_MAP_ATTRIBUTE_OUTSIDE_CODE_ATTRIBUTE__ID;
				goto _errorFound;
			}
			if (foundStackMap) {
				errorCode = J9NLS_CFR_ERR_MULTIPLE_STACK_MAP_ATTRIBUTES__ID;
				goto _errorFound;
			}
			foundStackMap = TRUE;

			break;

#if defined(J9VM_OPT_VALHALLA_NESTMATES)
		case CFR_ATTRIBUTE_MemberOfNest:
			value = ((J9CfrAttributeMemberOfNest*)attrib)->hostClassIndex;
			if ((!value) || (value > cpCount)) {
				errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
				goto _errorFound;
			}
			if (CFR_CONSTANT_Class != cpBase[value].tag) {
				errorCode = J9NLS_CFR_ERR_BAD_NEST_TOP_INDEX__ID;
				goto _errorFound;
			}
			break;

		case CFR_ATTRIBUTE_NestMembers: {
			U_16 nestMembersCount = ((J9CfrAttributeNestMembers*)attrib)->numberOfClasses;
			for (j = 0; j < nestMembersCount; j++) {
				value = ((J9CfrAttributeNestMembers*)attrib)->classes[j];
				if ((0 == value) || (value > cpCount)) {
					errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
					goto _errorFound;
				}
				if (CFR_CONSTANT_Class != cpBase[value].tag) {
					errorCode = J9NLS_CFR_ERR_NEST_MEMBERS_NAME_NOT_CONSTANT_CLASS__ID;
					goto _errorFound;
				}
			}
			break;
		}
#endif /* J9VM_OPT_VALHALLA_NESTMATES */

		case CFR_ATTRIBUTE_StackMap: /* CLDC StackMap attribute - not supported in J2SE */
		case CFR_ATTRIBUTE_Synthetic:
		case CFR_ATTRIBUTE_Deprecated:
		case CFR_ATTRIBUTE_StrippedLineNumberTable:
		case CFR_ATTRIBUTE_StrippedLocalVariableTable:
		case CFR_ATTRIBUTE_StrippedInnerClasses:
		case CFR_ATTRIBUTE_StrippedUnknown:
		case CFR_ATTRIBUTE_Unknown:
			break;			
		}
	}

	if (!bootstrapMethodAttributeRead && (maxBootstrapMethodIndex >= 0)) {
		errorCode = J9NLS_CFR_ERR_BOOTSTRAP_METHOD_TABLE_ABSENT_OR_WRONG_SIZE__ID;
		buildError((J9CfrError *) segment, errorCode, errorType, 0);
		return -1;
	}
			
	return 0;

_errorFound:

	buildError((J9CfrError *) segment, errorCode, errorType, attrib->romAddress);
	return -1;
}


/*
	Check the class file in @classfile.

	According the the JVMS 2nd ed: (1.2)
		"Implementations of version 1.2 of the Java 2 platform can support
		class file formats of versions in the range 45.0 through 46.0 inclusive."

	According to http://access1.sun.com/SRDs/srd_repository/tools.pdf (1.3)
		"The Java virtual machine (JVM) now accepts class files with version
		numbers 45.3 through 47.0, inclusive."

	According to http://home.ott.oti.com/teams/bluebird/doc/cldcng-f/CLDCSpecification1.1.pdf
		"The class file format numbers used by different JDK versions are as follows:
		- The 45.* (usually 45.3) version number identified JDK 1.1 class files
		- The 46.* version number identifies JDK 1.2 class files.
		- The 47.* version number identifies JDK 1.3 class files.
		- The 48.* version number identifies JDK 1.4 class files."

	- 49.* are JDK 5.0 class files (aka JDK 1.5)

	Returns -1 on error, 0 on success.
*/

static I_32 
checkClassVersion(J9CfrClassFile* classfile, U_8* segment, U_32 flags)
{
	U_32 errorCode = J9NLS_CFR_ERR_MAJOR_VERSION__ID;
	U_32 offset = 6;
	U_16 max_allowed_version = flags >> BCT_MajorClassFileVersionMaskShift;

	/* Support versions 45.0 -> <whatever is legal for this VM> */
	if((classfile->majorVersion >= 45) && (classfile->majorVersion <= max_allowed_version)) {
		/* check minor version numbers */
		if (classfile->majorVersion < max_allowed_version) {
			return 0;
		}

		if (0 == classfile->minorVersion) {
			/* only .0 is a valid minor version for max class major version */
			return 0;
		}
		errorCode = J9NLS_CFR_ERR_MINOR_VERSION__ID;
	}

	buildError((J9CfrError *) segment, errorCode, CFR_ThrowUnsupportedClassVersionError, offset);
	return -1;
}


/*
	Check the class file in @classfile.
	Returns -1 on error, 0 on success.
*/

static I_32 
checkClass(J9PortLibrary *portLib, J9CfrClassFile* classfile, U_8* segment, U_32 endOfConstantPool, U_32 vmVersionShifted, U_32 flags)
{
	U_32 value, errorCode, offset;
	U_32 i;
	I_32 maxBootstrapMethodIndex = -1;

	if(checkPool(classfile, segment, (U_8*)10, &maxBootstrapMethodIndex, vmVersionShifted, flags)) {
		return -1;
	}

	if (vmVersionShifted >= BCT_Java9MajorVersionShifted) {
		value = classfile->accessFlags & CFR_CLASS_ACCESS_MASK_9;
	} else {
		value = classfile->accessFlags & CFR_CLASS_ACCESS_MASK;
	}

	if ((flags & BCT_MajorClassFileVersionMask) < BCT_Java5MajorVersionShifted) {
		value &= ~CFR_CLASS_ACCESS_NEWJDK5_MASK;
		
		/* Remove ACC_SUPER from interface tests on older classes */
		/* ACC_SUPER is often set in older interfaces */
		if (value & CFR_ACC_INTERFACE) {
			value &= ~CFR_ACC_SUPER;
		}
	}

	/* the check for ACC_INTERFACE & ACC_ABSTRACT done in j9bcutil_readClassFileBytes */

	if ((value & CFR_ACC_ANNOTATION) && (! (value & CFR_ACC_INTERFACE))) {
		errorCode = J9NLS_CFR_ERR_ANNOTATION_NOT_INTERFACE__ID;
		offset = endOfConstantPool;
		goto _errorFound;
	}

	if((value & CFR_ACC_FINAL)&&(value & CFR_ACC_ABSTRACT)) {
		errorCode = J9NLS_CFR_ERR_FINAL_ABSTRACT_CLASS__ID;
		offset = endOfConstantPool;
		goto _errorFound;
	}

	/* check annotations and interfaces for invalid flags */
	if ((value & CFR_ACC_INTERFACE) && (value & ~CFR_INTERFACE_CLASS_ACCESS_MASK))
	{
		errorCode = J9NLS_CFR_ERR_INTERFACE_CLASS__ID;
		offset = endOfConstantPool;
		goto _errorFound;
	}

	value = classfile->thisClass;
	if((!value)||(value > classfile->constantPoolCount)) {
		errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
		offset = endOfConstantPool + 2;
		goto _errorFound;
	}

	if((classfile->constantPool)&&(classfile->constantPool[value].tag != CFR_CONSTANT_Class)) {
		errorCode = J9NLS_CFR_ERR_NOT_CLASS__ID;
		offset = endOfConstantPool + 2;
		goto _errorFound;
	}

	value = classfile->superClass;
	if(value > classfile->constantPoolCount) {
		errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
		offset = endOfConstantPool + 4;
		goto _errorFound;
	}

	if(value == 0) {
		/* Check against j.l.O. */
		if(!utf8Equal(&classfile->constantPool[classfile->constantPool[classfile->thisClass].slot1], "java/lang/Object", 16)) {
			errorCode = J9NLS_CFR_ERR_NULL_SUPER__ID;
			offset = endOfConstantPool + 4;
			goto _errorFound;
		}
	} else if(classfile->constantPool[value].tag != CFR_CONSTANT_Class) {
		errorCode = J9NLS_CFR_ERR_SUPER_NOT_CLASS__ID;
		offset = endOfConstantPool + 4;
		goto _errorFound;
	} 

	for(i = 0; i < classfile->interfacesCount; i++) {
		U_32 j;
		J9CfrConstantPoolInfo* cpInfo;
		value = classfile->interfaces[i];
		if((!value)||(value > classfile->constantPoolCount)) {
			errorCode = J9NLS_CFR_ERR_BAD_INDEX__ID;
			offset = endOfConstantPool + 4 + (i << 1);
			goto _errorFound;
		}
		cpInfo = &classfile->constantPool[value];
		if(cpInfo->tag != CFR_CONSTANT_Class) {
			errorCode = J9NLS_CFR_ERR_INTERFACE_NOT_CLASS__ID;
			offset = endOfConstantPool + 4 + (i << 1);
			goto _errorFound;
		}
		/* check that this interface isn't a duplicate */
		for (j = 0; j < i; j++) {
			U_32 otherValue = classfile->interfaces[j];
			J9CfrConstantPoolInfo* otherCpInfo = &classfile->constantPool[otherValue];
			if(utf8EqualUtf8(&classfile->constantPool[cpInfo->slot1], &classfile->constantPool[otherCpInfo->slot1])) {
				errorCode = J9NLS_CFR_ERR_DUPLICATE_INTERFACE__ID;
				offset = endOfConstantPool + 4 + (i << 1);
				goto _errorFound;
			}
		}
	}

	/* Check that interfaces subclass object. */
	if(classfile->accessFlags & CFR_ACC_INTERFACE) {
		/* Check against j.l.O. */
		if(!utf8Equal(&classfile->constantPool[classfile->constantPool[classfile->superClass].slot1], "java/lang/Object", 16)) {
			errorCode = J9NLS_CFR_ERR_INTERFACE_SUPER_NOT_OBJECT__ID;
			offset = endOfConstantPool + 4 + (i << 1);
			goto _errorFound;
		}
	}

	if(checkFields(classfile, segment, flags)) {
		return -1;
	}

	if(checkMethods(classfile, segment, vmVersionShifted, flags)) {
		return -1;
	}

	if (checkDuplicateMembers(portLib, classfile, segment, flags, (UDATA) sizeof(J9CfrField))) {
		return -1;
	}

	if (checkDuplicateMembers(portLib, classfile, segment, flags, (UDATA) sizeof(J9CfrMethod))) {
		return -1;
	}

	if(checkAttributes(classfile, classfile->attributes, classfile->attributesCount, segment, maxBootstrapMethodIndex, OUTSIDE_CODE, flags)) {
		return -1;
	}

	return 0;

	_errorFound:

	buildError((J9CfrError *) segment, errorCode, CFR_ThrowClassFormatError, offset);
	return -1;
}


/*
	Read the class file from the bytes in @data.
	Returns:
		-2 on insufficent space
		-1 on error
		0 on success
		(required segment size)

	On success, the class file structure is written into the memory segment
	provided.
	
	On failure an error structure is written.
	
	If the segment was too small, the data will represent a partial class file and
	should not be used.

	If the segment was too small for a single class file struct (26 + 4 * sizeof(void*))
	a different error is returned.

	Supported flags:
		CFR_StripDebugAttributes	-	do not store data for debug attributes
		CFR_StaticVerification			-	verify static constraints for each code array
		BCT_BigEndianOutput				-	ignored
		BCT_LittleEndianOutput			-	ignored
*/

#define VERBOSE_START(phase) \
	do { \
		if (NULL != verboseContext) { \
			romVerboseRecordPhaseStart(verboseContext, phase); \
		} \
	} while (0)

#define VERBOSE_END(phase) \
	do { \
		if (NULL != verboseContext) { \
			romVerboseRecordPhaseEnd(verboseContext, phase); \
		} \
	} while (0)

I_32
j9bcutil_readClassFileBytes(J9PortLibrary *portLib,
	IDATA (*verifyFunction) (J9PortLibrary *aPortLib, J9CfrClassFile* classfile, U_8* segment, U_8* segmentLength, U_8* freePointer, U_32 vmVersionShifted, U_32 flags, I_32 *hasRET),
	U_8* data, UDATA dataLength, U_8* segment, UDATA segmentLength, U_32 flags, U_8** segmentFreePointer, void *verboseContext, UDATA findClassFlags, UDATA romClassSortingThreshold)
{
	U_8* dataEnd;
	U_8* segmentEnd;
	U_8* index;
	U_8* freePointer;
	U_8* endOfConstantPool;
	J9CfrClassFile* classfile;
	I_32 result;
	UDATA errorCode;
	UDATA offset = 0;
	UDATA i;
	I_32 hasRET = 0;
	UDATA syntheticFound = FALSE;
	U_32 vmVersionShifted = flags & BCT_MajorClassFileVersionMask;

	Trc_BCU_j9bcutil_readClassFileBytes_Entry();

	/* There must be at least enough space for the classfile struct. */
	if(segmentLength < (UDATA) sizeof(J9CfrClassFile)) {
		Trc_BCU_j9bcutil_readClassFileBytes_Exit(-2);
		return -2;
	}

	/* protect the callers with default values */
	if ((flags & BCT_MajorClassFileVersionMask) == 0) {
		flags |= BCT_Java9MajorVersionShifted;
	}

	index = data;
	dataEnd = data + dataLength;
	freePointer = segment;
	segmentEnd = segment + segmentLength;

	ALLOC(classfile, J9CfrClassFile);

	CHECK_EOF(10);
	NEXT_U32(classfile->magic, index);
	if(classfile->magic != (U_32) CFR_MAGIC) {
		errorCode = J9NLS_CFR_ERR_MAGIC__ID;
		offset = index - data - 4;
		goto _errorFound;
	}

	NEXT_U16(classfile->minorVersion, index);
	NEXT_U16(classfile->majorVersion, index);
	NEXT_U16(classfile->constantPoolCount, index);

	if(classfile->constantPoolCount < 1) {
		errorCode = J9NLS_CFR_ERR_CONSTANT_POOL_EMPTY__ID;
		offset = index - data - 2;
		goto _errorFound;
	}

	VERBOSE_START(ParseClassFileConstantPool);
	/* Space for the constant pool */
	if(!ALLOC_ARRAY(classfile->constantPool, classfile->constantPoolCount, J9CfrConstantPoolInfo)) {
		Trc_BCU_j9bcutil_readClassFileBytes_Exit(-2);
		return -2;
	}

	/* Read the pool. */
	if((result = readPool(classfile, data, dataEnd, segment, segmentEnd, &index, &freePointer)) != 0) {
		Trc_BCU_j9bcutil_readClassFileBytes_Exit(result);
		return result;
	}
	endOfConstantPool = index;
	VERBOSE_END(ParseClassFileConstantPool);

	CHECK_EOF(8);
	if (vmVersionShifted >= BCT_Java9MajorVersionShifted) {
		classfile->accessFlags = NEXT_U16(classfile->accessFlags, index) & CFR_CLASS_ACCESS_MASK_9;
	} else {
		classfile->accessFlags = NEXT_U16(classfile->accessFlags, index) & CFR_CLASS_ACCESS_MASK;
	}
	classfile->j9Flags = 0;

	/* Certain versions of the Java compiler forget to tag interfaces as abstract. Fix it. */
	/* See 1GCC5T2: J9VM:ALL - JDK accepts non-abstract interfaces */
	if( (classfile->accessFlags & (CFR_ACC_INTERFACE | CFR_ACC_ABSTRACT)) == CFR_ACC_INTERFACE ) {

		/* Only enforce the check if -Xfuture is specified */
		if (flags & CFR_Xfuture) { 
			errorCode = J9NLS_CFR_ERR_INTERFACE_NOT_ABSTRACT__ID;
			offset = index - data - 2;
			goto _errorFound;
		}

		classfile->accessFlags |= CFR_ACC_ABSTRACT;
	}

	if ((vmVersionShifted >= BCT_Java9MajorVersionShifted)
			&& (J9_ARE_ALL_BITS_SET(classfile->accessFlags, CFR_ACC_MODULE))) {
		errorCode = J9NLS_CFR_ERR_MODULE_IS_INVALID_CLASS__ID;
		offset = index - data - 2;
		goto _errorFound;
	}

	NEXT_U16(classfile->thisClass, index);

	/* if anonClass create an array for the new class name */
	if (J9_ARE_ANY_BITS_SET(findClassFlags, J9_FINDCLASS_FLAG_ANON)) {
		/* find the UTF8 thisClass name slot */
		U_32 cpThisClassUTF8Slot = classfile->constantPool[classfile->thisClass].slot1;
		U_32 originalStringLength = classfile->constantPool[cpThisClassUTF8Slot].slot1;
		const char* originalStringBytes = (const char*)classfile->constantPool[cpThisClassUTF8Slot].bytes;
		U_32 i = 0;
		/*
		 * alloc an array for the new name with the following format:
		 * [className]/[ROMClassAddress]\0
		 */
		if (!ALLOC_ARRAY(classfile->constantPool[cpThisClassUTF8Slot].bytes, originalStringLength + 1 + ROM_ADDRESS_LENGTH + 1, U_8)) {
			Trc_BCU_j9bcutil_readClassFileBytes_Exit(-2);
			return -2;
		}
		
		/* update the size */
		classfile->constantPool[cpThisClassUTF8Slot].slot1 += ROM_ADDRESS_LENGTH + 1;

		/* copy the name into the new location and add the special character, fill the rest with zeroes */
		memcpy (classfile->constantPool[cpThisClassUTF8Slot].bytes, originalStringBytes, originalStringLength);
		*(U_8*)((UDATA) classfile->constantPool[cpThisClassUTF8Slot].bytes + originalStringLength) = ANON_CLASSNAME_CHARACTER_SEPARATOR;
		memset(classfile->constantPool[cpThisClassUTF8Slot].bytes + originalStringLength + 1, '0', ROM_ADDRESS_LENGTH);
		*(U_8*)((UDATA) classfile->constantPool[cpThisClassUTF8Slot].bytes + originalStringLength + 1 + ROM_ADDRESS_LENGTH) = '\0';

		/* search constpool for all other identical classRefs TODO untested */
		for (i = 0; i < classfile->constantPoolCount; i++) {
			if (CFR_CONSTANT_Class == classfile->constantPool[i].tag) {
				U_32 classNameSlot = classfile->constantPool[i].slot1;
				if (classNameSlot != cpThisClassUTF8Slot) {
					U_32 classNameLength = classfile->constantPool[classNameSlot].slot1;
					if ((classNameLength == originalStringLength)
						&& (0 == strncmp(originalStringBytes, (const char*)classfile->constantPool[classNameSlot].bytes, originalStringLength)))
					{
						/* if it is the same class, point to original class name slot */
						classfile->constantPool[i].slot1 = cpThisClassUTF8Slot;
					}
				}
			}
		}
	}

	NEXT_U16(classfile->superClass, index);
	NEXT_U16(classfile->interfacesCount, index);

	/* Space for the interfaces */
	if(!ALLOC_ARRAY(classfile->interfaces, classfile->interfacesCount, U_16)) {
		Trc_BCU_j9bcutil_readClassFileBytes_Exit(-2);
		return -2;
	}

	/* Read the interfaces. */
	CHECK_EOF(classfile->interfacesCount << 1);
	for(i = 0; i < classfile->interfacesCount; i++) {
		NEXT_U16(classfile->interfaces[i], index);
	}

	CHECK_EOF(2);
	NEXT_U16(classfile->fieldsCount, index);

	VERBOSE_START(ParseClassFileFields);
	/* Space for the fields. */
	if(!ALLOC_ARRAY(classfile->fields, classfile->fieldsCount, J9CfrField)) {
		Trc_BCU_j9bcutil_readClassFileBytes_Exit(-2);
		return -2;
	}

	/* Read the fields. */
	if((result = readFields(classfile, data, dataEnd, segment, segmentEnd, &index, &freePointer, flags)) != 0) {
		Trc_BCU_j9bcutil_readClassFileBytes_Exit(result);
		return result;
	}
	VERBOSE_END(ParseClassFileFields);

	CHECK_EOF(2);
	NEXT_U16(classfile->methodsCount, index);

	VERBOSE_START(ParseClassFileMethods);
	/* Space for the methods. */
	if(!ALLOC_ARRAY(classfile->methods, classfile->methodsCount, J9CfrMethod)) {
		Trc_BCU_j9bcutil_readClassFileBytes_Exit(-2);
		return -2;
	}
			
	/* Read the methods. */
	if((result = readMethods(classfile, data, dataEnd, segment, segmentEnd, &index, &freePointer, flags)) != 0) {
		Trc_BCU_j9bcutil_readClassFileBytes_Exit(result);
		return result;
	}
	VERBOSE_END(ParseClassFileMethods);

	/* sort methods */
	if (classfile->methodsCount >= romClassSortingThreshold) {
		sortMethodIndex(classfile->constantPool, classfile->methods, 0, classfile->methodsCount - 1);
	}

	CHECK_EOF(2);
	NEXT_U16(classfile->attributesCount, index);

	VERBOSE_START(ParseClassFileAttributes);
	/* Space for the attributes. */
	if(!ALLOC_ARRAY(classfile->attributes, classfile->attributesCount, J9CfrAttribute*)) {
		Trc_BCU_j9bcutil_readClassFileBytes_Exit(-2);
		return -2;
	}
			
	/* Read the attributes. */
	if((result = readAttributes(classfile, &(classfile->attributes), classfile->attributesCount, data, dataEnd, segment, segmentEnd, &index, &freePointer, flags, &syntheticFound)) != 0) {
		Trc_BCU_j9bcutil_readClassFileBytes_Exit(result);
		return result;
	}
	if (syntheticFound) {
		classfile->accessFlags |= CFR_ACC_SYNTHETIC;
	}

	VERBOSE_END(ParseClassFileAttributes);

	VERBOSE_START(ParseClassFileCheckClass);
	/* Check the pointer vs. end of file. */
	if(index != dataEnd) {
		offset = index - data;
		if (dataLength < offset) {
			errorCode = J9NLS_CFR_ERR_UNEXPECTED_EOF__ID;
			offset = dataLength;
		} else {
			/* Already know file is not too short */
			errorCode = J9NLS_CFR_ERR_EXPECTED_EOF__ID;
		}
		goto _errorFound;
	}

	classfile->classFileSize = (U_32) dataLength;
	if (J9_ARE_ALL_BITS_SET(findClassFlags, J9_FINDCLASS_FLAG_ANON)) {
		/* additional bytes added for the unique name */
		classfile->classFileSize += 1 + ROM_ADDRESS_LENGTH;
	}
	/* Ensure that this is a supported class file version. */
	if(checkClassVersion(classfile, segment, flags)) {
		Trc_BCU_j9bcutil_readClassFileBytes_Exit(-1);
		return -1;
	}

	/* Make sure that following verification uses the class file version number */
	flags &= ~BCT_MajorClassFileVersionMask;
	flags |= ((UDATA) classfile->majorVersion) << BCT_MajorClassFileVersionMaskShift;
	
	/* Structure verification. This is "Pass 1". */
	if (0 != (flags & CFR_StaticVerification)) {
		if(checkClass(portLib, classfile, segment, (U_32) (endOfConstantPool - data), vmVersionShifted, flags)) {
			Trc_BCU_j9bcutil_readClassFileBytes_Exit(-1);
			return -1;
		}
	}
	VERBOSE_END(ParseClassFileCheckClass);

	VERBOSE_START(ParseClassFileVerifyClass);
	/* Static verification. This is the first half of "Pass 2". */
	if (0 != (flags & CFR_StaticVerification)) {
		if((result = (I_32)(verifyFunction)(portLib, classfile, segment, segmentEnd, freePointer, vmVersionShifted, flags, &hasRET)) != 0) {
			Trc_BCU_j9bcutil_readClassFileBytes_Exit(result);
			return result;
		}
	} else {
		/* special checking to look for jsr's if -noverify - the verifyFunction normally scans and tags 
			for inlining methods and classes that contain jsr's */
		hasRET = checkForJsrs(classfile);
	}
	VERBOSE_END(ParseClassFileVerifyClass);

	VERBOSE_START(ParseClassFileInlineJSRs);
	/* perform jsr inlining in necessary */
	if (((classfile->j9Flags & CFR_J9FLAG_HAS_JSR) == CFR_J9FLAG_HAS_JSR) && ((flags & CFR_LeaveJSRs) == 0)) {
		J9JSRIData inlineBuffers;
		J9CfrMethod *method;

		/*
		 * JSR 202: Section 4.10.1 - class file version # is 51.0 or greater,
		 * or the version is 50 and nofallback is specified, neither jsr or jsr_w can be in code array
		 */
		if ((classfile->majorVersion >= 51)
		|| (((flags & J9_VERIFY_NO_FALLBACK) == J9_VERIFY_NO_FALLBACK) && (classfile->majorVersion == 50))
		) {
			errorCode = J9NLS_CFR_ERR_FOUND_JSR_IN_CLASS_VER_51__ID;
			/* Jazz 82615: Ignore the specific method as it can't be determined under such circumstance */
			buildMethodErrorWithExceptionDetails((J9CfrError *) segment, errorCode, 0, CFR_ThrowVerifyError, 0, 0, &(classfile->methods[0]), classfile->constantPool, 0, -1, 0);
			Trc_BCU_j9bcutil_readClassFileBytes_VerifyError_Exception(((J9CfrError *) segment)->errorCode, getJ9CfrErrorDescription(portLib, (J9CfrError *) segment));
			Trc_BCU_j9bcutil_readClassFileBytes_Exit(-1);
			return -1;
		}

		memset (&inlineBuffers, 0, sizeof(J9JSRIData));
		inlineBuffers.portLib = portLib;
		J9_CFR_ALIGN(freePointer, UDATA);
		inlineBuffers.freePointer = freePointer;
		inlineBuffers.segmentEnd = segmentEnd;

		for (i = 0; i < classfile->methodsCount; i++) {
			method = &(classfile->methods[i]);
			if ((method->j9Flags & CFR_J9FLAG_HAS_JSR) == CFR_J9FLAG_HAS_JSR) {
				inlineBuffers.codeAttribute = method->codeAttribute;
				inlineBuffers.sourceBuffer = method->codeAttribute->code;
				inlineBuffers.sourceBufferSize = method->codeAttribute->codeLength;
				inlineBuffers.constantPool = classfile->constantPool;
				inlineBuffers.maxStack = method->codeAttribute->maxStack;
				inlineBuffers.maxLocals = method->codeAttribute->maxLocals;
				inlineBuffers.flags = flags;
				inlineBuffers.bytesAddedByJSRInliner = 0;

				Trc_BCU_inlineJsrs_Entry((classfile->constantPool[classfile->constantPool[classfile->thisClass].slot1]).slot1, (classfile->constantPool[classfile->constantPool[classfile->thisClass].slot1]).bytes,
																 (classfile->constantPool[method->nameIndex]).slot1, (classfile->constantPool[method->nameIndex]).bytes,
																 (classfile->constantPool[method->descriptorIndex]).slot1, (classfile->constantPool[method->descriptorIndex]).bytes);
				inlineJsrs(hasRET, classfile, &inlineBuffers);

				if (inlineBuffers.errorCode) {
					result = (I_32) inlineBuffers.errorCode;
					if (inlineBuffers.errorCode == BCT_ERR_VERIFY_ERROR_INLINING) {
						/* Jazz 82615: Set the verbose error type and the local variable index if used for the error message framework */
						buildMethodErrorWithExceptionDetails((J9CfrError *)segment, inlineBuffers.verifyError, inlineBuffers.verboseErrorType, CFR_ThrowVerifyError,
										 (I_32) i, (U_16) inlineBuffers.verifyErrorPC, method, classfile->constantPool, inlineBuffers.errorLocalIndex, -1, 0);

						/* Over-ride the default message catalog - inlineJsrs() errors are in the BCV catalog */
						((J9CfrError *)segment)->errorCatalog = (U_32) J9NLS_BCV_ERR_NO_ERROR__MODULE;

						Trc_BCU_j9bcutil_readClassFileBytes_VerifyError_Exception(((J9CfrError *) segment)->errorCode, getJ9CfrErrorDescription(portLib, (J9CfrError *) segment));
						Trc_BCU_inlineJsrs_Exit();

						result = -1;
					}
					releaseInlineBuffers (&inlineBuffers);
					Trc_BCU_j9bcutil_readClassFileBytes_Exit(result);
					return result;
				}

				/* Update class file size with amount of bytes added by JSR inliner */
				classfile->classFileSize += inlineBuffers.bytesAddedByJSRInliner;

				Trc_BCU_inlineJsrs_Exit();
			}
		}

		freePointer = inlineBuffers.freePointer;
		releaseInlineBuffers (&inlineBuffers);

		/* Backup method bytecodes so that in-place fixup will not ruin the JSR'd versions. */
		for (i = 0; i < classfile->methodsCount; i++) {
			method = &(classfile->methods[i]);

			if (CFR_J9FLAG_HAS_JSR == (method->j9Flags & CFR_J9FLAG_HAS_JSR)) {
				/* clear the flag - not needed anymore */
				method->j9Flags &= ~CFR_J9FLAG_HAS_JSR;

				if (!ALLOC_ARRAY(method->codeAttribute->code, method->codeAttribute->codeLength, U_8)) {
					Trc_BCU_inlineJsrs_Exit();
					return -2;
				}
				memcpy(method->codeAttribute->code, method->codeAttribute->originalCode, method->codeAttribute->codeLength);
			}
		}

		/* clear the flag - not needed anymore */
		classfile->j9Flags &= ~CFR_J9FLAG_HAS_JSR;
	}
	VERBOSE_END(ParseClassFileInlineJSRs);

	Trc_BCU_j9bcutil_readClassFileBytes_Exit(0);
	if (segmentFreePointer != NULL) {
		*segmentFreePointer = freePointer;
	}
	return 0;

_errorFound:

	buildError((J9CfrError *) segment, errorCode, CFR_ThrowClassFormatError, offset);

	Trc_BCU_j9bcutil_readClassFileBytes_ClassFormatError_Exception(((J9CfrError *) segment)->errorCode, getJ9CfrErrorDescription(portLib, (J9CfrError *) segment));

	Trc_BCU_j9bcutil_readClassFileBytes_Exit(-1);
	return -1;
}

#undef VERBOSE_START
#undef VERBOSE_END

/*
	Compare a J9CfrConstantUtf8 to another J9CfrConstantUtf8.
*/

static BOOLEAN 
utf8EqualUtf8(J9CfrConstantPoolInfo *utf8a, J9CfrConstantPoolInfo *utf8b)
{
	if (utf8a->tag !=CFR_CONSTANT_Utf8) {
		return FALSE;
	}

	if (utf8a == utf8b) {
		return TRUE;
	}

	/* slot1 contains the length */
	if ((utf8b->tag !=CFR_CONSTANT_Utf8) || (utf8a->slot1 != utf8b->slot1)) {
		return FALSE;
	}

	return memcmp (utf8a->bytes, utf8b->bytes, utf8a->slot1) == 0;
}	


/*
	Verify (pass 1) that fields/members aren't duplicated in @classfile.
	Returns 0 on success, non-zero on failure.

	This function implements two different algorithms. For a small number
	of members (<30) each member is compared to every other member.
	For a larger number of members, a hash table is allocated and each
	member is placed in the hashtable. When collisions occur, the colliding
	elements are checked to see if they are duplicates.

	On tiny configurations, only the O(n^2) algorithm is used, as these VMs
	are unlikely to encounter very large classes.
*/

static I_32 
checkDuplicateMembers (J9PortLibrary* portLib, J9CfrClassFile * classfile, U_8 * segment, U_32 flags, UDATA memberSize)
{
	if (flags & CFR_StaticVerification) {
#if DUP_TIMING
		PORT_ACCESS_FROM_PORT(portLib); 
		U_64 startTime;
#endif
		J9CfrMember *member;
		U_32 i, j, errorCode;
		UDATA count;
		U_8* data;

		if (memberSize == (UDATA) sizeof(J9CfrField)) {
			count =classfile->fieldsCount;
			data = (U_8*)classfile->fields;
			errorCode = J9NLS_CFR_ERR_DUPLICATE_FIELD__ID;
		} else { 
			count = classfile->methodsCount;
			data = (U_8*)classfile->methods;
			errorCode = J9NLS_CFR_ERR_DUPLICATE_METHOD__ID;
		}

		if (count >= DUP_HASH_THRESHOLD || DUP_TIMING) {
			PORT_ACCESS_FROM_PORT(portLib);
			U_16 *sortedIndices;
			UDATA tableSize = findSmallestPrimeGreaterThanOrEqualTo(count * 2);

#if DUP_TIMING
			startTime = j9time_hires_clock();
#endif
			sortedIndices = j9mem_allocate_memory(tableSize * sizeof(U_16), J9MEM_CATEGORY_CLASSES);
			/* if the allocate failed, fall through to the in-place search */
			if (sortedIndices != NULL) {
				I_32 k;

				memset(sortedIndices, 0, tableSize * sizeof(U_16));

				/* count down, so that 0 is the last index added to the table. This allows
				 * us to use 0 as the free tag
				 */
				for (k = (I_32) (count - 1); k >= 0; k--) {
					U_32 hash = 0;
					J9CfrConstantPoolInfo *name, *sig;
					member = (J9CfrMember*)&data[memberSize * k];
					name = &classfile->constantPool[member->nameIndex];
					sig = &classfile->constantPool[member->descriptorIndex];

					/* a 24-bit hash is good enough, since the maximum number of members is 65535 */
					for (j = 0; j < name->slot1; j++) {
						hash ^= (U_32)RandomValues[name->bytes[j]] << 8;
						if (++j < name->slot1) hash ^= (U_32)RandomValues[name->bytes[j]] << 8;
						if (++j < name->slot1) hash ^= (U_32)RandomValues[name->bytes[j]] << 16;
					}
					for (j = 0; j < sig->slot1; j++) {
						hash ^= (U_32)RandomValues[sig->bytes[j]] << 8;
						if (++j < sig->slot1) hash ^= (U_32)RandomValues[sig->bytes[j]] << 8;
						if (++j < sig->slot1) hash ^= (U_32)RandomValues[sig->bytes[j]] << 16;
					}
					hash %= tableSize;
					for (j = hash; sortedIndices[j];) {
						J9CfrMember* dup = (J9CfrMember*)&data[memberSize * sortedIndices[j]];
						if (memberEqual(classfile, member, dup)) {
							goto fail;
						}
						j = j + 1;
						if (j == tableSize) {
							j = 0;
						}
					}
					sortedIndices[j] = (U_16) k;
				}

				j9mem_free_memory(sortedIndices);
#if DUP_TIMING
				j9tty_printf(PORTLIB, "Hash table for %6d elements: %lluus\n", count,  j9time_hires_delta(startTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MICROSECONDS));
#else
				return 0;
#endif
			}
		}

		/* do an in-place, O(n^2) search */
#if DUP_TIMING
		startTime = j9time_hires_clock();
#endif
		for (i = 0; i < count; i++) {
			member = (J9CfrMember*)&data[memberSize * i];
			for (j = 0; j < i; j++) {
				J9CfrMember *dup = (J9CfrMember*)&data[memberSize * j];
				if (memberEqual(classfile, member, dup)) {
fail:
					buildError((J9CfrError *) segment, errorCode, CFR_ThrowClassFormatError, member->romAddress);
					return -1;
				}
			}
		}
#if DUP_TIMING
		j9tty_printf(PORTLIB, "In-place for %8d elements: %lluus\n", count, j9time_hires_delta(startTime, j9time_hires_clock(), J9PORT_TIME_DELTA_IN_MICROSECONDS));
#endif
	}

	return 0;
}



static BOOLEAN 
memberEqual(J9CfrClassFile * classfile, J9CfrMember* a, J9CfrMember* b) 
{
	return 
		utf8EqualUtf8 (&classfile->constantPool[a->nameIndex], &classfile->constantPool[b->nameIndex]) 
		&& utf8EqualUtf8 (&classfile->constantPool[a->descriptorIndex], &classfile->constantPool[b->descriptorIndex]);
}

/*
	Read the attributes from the bytes in @data.
	Returns 0 on success, non-zero on failure.
*/

static I_32 
readAnnotations(J9CfrClassFile * classfile, J9CfrAnnotation * pAnnotations, U_32 annotationCount, U_8 * data,
						   U_8 * dataEnd, U_8 * segment, U_8 * segmentEnd, U_8 ** pIndex, U_8 ** pFreePointer, U_32 flags)
{
	J9CfrAnnotation *annotationList = pAnnotations;
	U_8 *index = *pIndex;
	U_8 *freePointer = *pFreePointer;
	U_32 errorCode, offset;
	U_32 i, j;
	I_32 result;

	for (i = 0; i < annotationCount; i++) {
		CHECK_EOF(4);
		NEXT_U16(annotationList[i].typeIndex, index);
		if (annotationList[i].typeIndex >= classfile->constantPoolCount ) {
			annotationList[i].typeIndex = 0; /* put in safe dummy values in case of error */
			annotationList[i].numberOfElementValuePairs = 0;
			return BCT_ERR_INVALID_ANNOTATION;
		}
		NEXT_U16(annotationList[i].numberOfElementValuePairs, index);

		if (!ALLOC_ARRAY(annotationList[i].elementValuePairs, annotationList[i].numberOfElementValuePairs, J9CfrAnnotationElementPair)) {
			return BCT_ERR_OUT_OF_ROM;
		}

		for (j = 0; j < annotationList[i].numberOfElementValuePairs; j++) {
			CHECK_EOF(2);
			NEXT_U16(annotationList[i].elementValuePairs[j].elementNameIndex, index);

			result = readAnnotationElement(classfile, &annotationList[i].elementValuePairs[j].value,
					data, dataEnd, segment, segmentEnd, &index, &freePointer, flags);
			if (result != 0) {
				return result;
			}
		}
	}

	*pIndex = index;
	*pFreePointer = freePointer;
	return 0;

_errorFound:
	return -1;
}

/*
	Read one type_annotation attribute from the bytes in @data.
	Precondition: classfile reader is at the start of a type_annotation
	Postcondition: classfile reader is at the byte following the end of a type_annotation
	@return 0 on success, negative on failure
*/

static I_32
readTypeAnnotation(J9CfrClassFile * classfile, J9CfrTypeAnnotation * tAnn, U_8 * data,
						   U_8 * dataEnd, U_8 * segment, U_8 * segmentEnd, U_8 ** pIndex, U_8 ** pFreePointer, U_32 flags)
{
	U_8 targetType = 0;
	U_8 *index = *pIndex;
	U_32 errorCode = 0; /* required by CHECK_EOF */
	U_32 offset = 0; /* required by CHECK_EOF */
	U_32 i = 0;
	I_32 result = 0;
	U_8 *freePointer = *pFreePointer;

	CHECK_EOF(1);
	NEXT_U8(targetType, index);
	tAnn->targetType = targetType;
	Trc_BCU_TypeAnnotation(targetType);
	switch (targetType) { /* use values from the JVM spec. Java SE 8 Edition, table 4.7.20-A&B */
	case CFR_TARGET_TYPE_TypeParameterGenericClass:
	case CFR_TARGET_TYPE_TypeParameterGenericMethod: {
		J9CfrTypeParameterTarget *t = &(tAnn->targetInfo.typeParameterTarget);
		CHECK_EOF(1);
		NEXT_U8(t->typeParameterIndex, index);
	};
	break;

	case CFR_TARGET_TYPE_TypeInExtends: {
		J9CfrSupertypeTarget *t = &(tAnn->targetInfo.supertypeTarget);
		CHECK_EOF(2);
		NEXT_U16(t->supertypeIndex, index);
	}
	break;

	case CFR_TARGET_TYPE_TypeInBoundOfGenericClass:
	case CFR_TARGET_TYPE_TypeInBoundOfGenericMethod: {
		J9CfrTypeParameterBoundTarget *t = &(tAnn->targetInfo.typeParameterBoundTarget);
		CHECK_EOF(2);
		NEXT_U8(t->typeParameterIndex, index);
		NEXT_U8(t->boundIndex, index);
	}
	break;

	case CFR_TARGET_TYPE_TypeInFieldDecl:
	case CFR_TARGET_TYPE_ReturnType:
	case CFR_TARGET_TYPE_ReceiverType: /* empty_target */
	break;

	case CFR_TARGET_TYPE_TypeInFormalParam: {
		J9CfrMethodFormalParameterTarget *t = &(tAnn->targetInfo.methodFormalParameterTarget);
		CHECK_EOF(1);
		NEXT_U8(t->formalParameterIndex, index);
	};
	break;

	case CFR_TARGET_TYPE_TypeInThrows: 	{
		J9CfrThrowsTarget *t = &(tAnn->targetInfo.throwsTarget);
		CHECK_EOF(2);
		NEXT_U16(t->throwsTypeIndex, index);
	}
	break;

	case CFR_TARGET_TYPE_TypeInLocalVar:
	case CFR_TARGET_TYPE_TypeInResourceVar: {
		U_32 ti;
		J9CfrLocalvarTarget *t = &(tAnn->targetInfo.localvarTarget);
		CHECK_EOF(2);
		NEXT_U16(t->tableLength, index);
		if (!ALLOC_ARRAY(t->table, t->tableLength, J9CfrLocalvarTargetEntry)) {
			return -2;
		}
		for (ti=0; ti < t->tableLength; ++ti) {
			J9CfrLocalvarTargetEntry *te = &(t->table[ti]);
			CHECK_EOF(6);
			NEXT_U16(te->startPC, index);
			NEXT_U16(te->length, index);
			NEXT_U16(te->index, index);
		}
	};
	break;
	case CFR_TARGET_TYPE_TypeInExceptionParam: {
		J9CfrCatchTarget *t = &(tAnn->targetInfo.catchTarget);
		CHECK_EOF(2);
		NEXT_U16(t->exceptiontableIndex, index);
	}
	break;

	case CFR_TARGET_TYPE_TypeInInstanceof:
	case CFR_TARGET_TYPE_TypeInNew:
	case CFR_TARGET_TYPE_TypeInMethodrefNew:
	case CFR_TARGET_TYPE_TypeInMethodrefIdentifier: {
		J9CfrOffsetTarget *t = &(tAnn->targetInfo.offsetTarget);
		CHECK_EOF(2);
		NEXT_U16(t->offset, index);
	}
	break;

	case CFR_TARGET_TYPE_TypeInCast:
	case CFR_TARGET_TYPE_TypeForGenericConstructorInNew:
	case CFR_TARGET_TYPE_TypeForGenericMethodInvocation:
	case CFR_TARGET_TYPE_TypeForGenericConstructorInMethodRef:
	case CFR_TARGET_TYPE_TypeForGenericMethodInvocationInMethodRef: {
		J9CfrTypeArgumentTarget  *t = &(tAnn->targetInfo.typeArgumentTarget);
		CHECK_EOF(3);
		NEXT_U16(t->offset, index);
		NEXT_U8(t->typeArgumentIndex, index);
	}
	break;

	default:
		errorCode = J9NLS_CFR_ERR_BAD_TARGET_TYPE_TAG;
		offset = (U_32) (index - data - 1);
		goto _errorFound;
	}

	/* now parse the target_path */
	CHECK_EOF(1);
	NEXT_U8(tAnn->typePath.pathLength, index);
	if (!ALLOC_ARRAY(tAnn->typePath.path, tAnn->typePath.pathLength, J9CfrTypePathEntry)) {
		return -2;
	}
	for (i=0; i < tAnn->typePath.pathLength; ++i) {
		J9CfrTypePathEntry *entry = &tAnn->typePath.path[i];
		CHECK_EOF(2);
		NEXT_U8(entry->typePathKind, index);
		NEXT_U8(entry->typeArgumentIndex, index);
	}

	/* read the annotation text */
	result = readAnnotations(classfile, &(tAnn->annotation), 1, data,
			dataEnd, segment, segmentEnd, &index, &freePointer, flags);
	*pFreePointer = freePointer;
	*pIndex = index;
	return result;

_errorFound:
	return BCT_ERR_INVALID_ANNOTATION;
}


/*
	Read the annotations from the bytes in @data.
	Returns 0 on success, non-zero on failure.
*/

static I_32 
readAnnotationElement(J9CfrClassFile * classfile, J9CfrAnnotationElement ** pAnnotationElement, U_8 * data,
						   U_8 * dataEnd, U_8 * segment, U_8 * segmentEnd, U_8 ** pIndex, U_8 ** pFreePointer, U_32 flags)
{
	J9CfrAnnotationElement *element;
	U_8 *index = *pIndex; /* used by CHECK_EOF macro */
	U_8 *freePointer = *pFreePointer; /* used by ALLOC_* macros */
	J9CfrAnnotation *annotation;
	J9CfrAnnotationElementArray *array;
	J9CfrAnnotationElement **arrayWalk;
	U_32 errorCode, offset;
	U_32 j;
	I_32 result; /* used by ALLOC_* macros */
	U_32 annotationTag;

	CHECK_EOF(1);
	NEXT_U8(annotationTag, index);
 
	switch (annotationTag) {
	case 'B':
	case 'C':
	case 'D':
	case 'F':
	case 'I':
	case 'J':
	case 'S':
	case 'Z':
	case 's':
		/* primitive */
		if (!ALLOC_CAST(element, J9CfrAnnotationElementPrimitive, J9CfrAnnotationElement)) {
			return -2;
		}

		CHECK_EOF(2);
		NEXT_U16(((J9CfrAnnotationElementPrimitive *)element)->constValueIndex, index);
		break;
			
	case 'e':
		if (!ALLOC_CAST(element, J9CfrAnnotationElementEnum, J9CfrAnnotationElement)) {
			return -2;
		}

		CHECK_EOF(4);
		NEXT_U16(((J9CfrAnnotationElementEnum *)element)->typeNameIndex, index);
		NEXT_U16(((J9CfrAnnotationElementEnum *)element)->constNameIndex, index);
		break;

	case 'c':
		if (!ALLOC_CAST(element, J9CfrAnnotationElementClass, J9CfrAnnotationElement)) {
			return -2;
		}

		CHECK_EOF(2);
		NEXT_U16(((J9CfrAnnotationElementClass *)element)->classInfoIndex, index);
		break;

	case '@':
		if (!ALLOC_CAST(element, J9CfrAnnotationElementAnnotation, J9CfrAnnotationElement)) {
			return -2;
		}

		annotation = &((J9CfrAnnotationElementAnnotation *)element)->annotationValue;

		result = readAnnotations(classfile, annotation, 1, data, dataEnd, segment, segmentEnd, &index, &freePointer, flags);
		if (result != 0) {
			return result;
		}

		break;

	case '[':
		if (!ALLOC_CAST(element, J9CfrAnnotationElementArray, J9CfrAnnotationElement)) {
			return -2;
		}
		array = (J9CfrAnnotationElementArray *)element;

		CHECK_EOF(2);
		NEXT_U16(array->numberOfValues, index);

		if (!ALLOC_ARRAY(array->values, array->numberOfValues, J9CfrAnnotationElement *)) {
			return -2;
		}

		arrayWalk = array->values;
		for (j = 0; j < array->numberOfValues; j++, arrayWalk++) {
			result = readAnnotationElement(classfile, arrayWalk, data, dataEnd, segment, segmentEnd, &index, &freePointer, flags);
			if (result != 0) {
				return result;
			}
		}

		break;

	default:
		/* error */
		errorCode = J9NLS_CFR_ERR_BAD_ANNOTATION_TAG__ID;
		offset = (U_32) (index - data - 1);
		goto _errorFound;
	}

	element->tag = annotationTag;

	*pAnnotationElement = element;
	*pIndex = index;
	*pFreePointer = freePointer;
	return 0;

_errorFound:
	return BCT_ERR_INVALID_ANNOTATION;
}

static VMINLINE U_16
getUTF8Length(J9CfrConstantPoolInfo* constantPool, U_16 cpIndex)
{
	return (U_16) constantPool[cpIndex].slot1;
}

static VMINLINE U_8*
getUTF8Data(J9CfrConstantPoolInfo* constantPool, U_16 cpIndex)
{
	return constantPool[cpIndex].bytes;
}

static IDATA
compareMethodIDs(J9CfrConstantPoolInfo* constantPool, J9CfrMethod *a, J9CfrMethod *b)
{
	U_16 aNameIndex = a->nameIndex;
	U_16 bNameIndex = b->nameIndex;
	U_16 aSigIndex = a->descriptorIndex;
	U_16 bSigIndex = b->descriptorIndex;
	U_8 *aNameData = getUTF8Data(constantPool, aNameIndex);
	U_8 *bNameData = getUTF8Data(constantPool, bNameIndex);
	U_8 *aSigData = getUTF8Data(constantPool, aSigIndex);
	U_8 *bSigData = getUTF8Data(constantPool, bSigIndex);
	U_16 aNameLength = getUTF8Length(constantPool, aNameIndex);
	U_16 bNameLength = getUTF8Length(constantPool, bNameIndex);
	U_16 aSigLength = getUTF8Length(constantPool, aSigIndex);
	U_16 bSigLength = getUTF8Length(constantPool, bSigIndex);

	return compareMethodNameAndSignature(aNameData, aNameLength, aSigData, aSigLength, bNameData, bNameLength, bSigData, bSigLength);
}

/**
 * Quicksort the list of methods, via one level of indirection: the array of structs is
 * unsorted, but the pointers to the struct are in sorted order. This function assumes all 
 * items in the list are unique, and start come before end
 * 
 * @param constantPool class constant pool
 * @param Vector of pointers to method descriptors
 * @start index of the first element to be sorted
 * @end index of the last element to be sorted
 */
static void
sortMethodIndex(J9CfrConstantPoolInfo* constantPool, J9CfrMethod *list, IDATA start, IDATA end)
{
	IDATA scanUp = start;
	IDATA scanDown = end;
	IDATA result = 0;
	J9CfrMethod pivot = list[(start+end)/2];

	do {
		while (scanUp < scanDown) {
			result = compareMethodIDs(constantPool, &pivot, &list[scanUp]);
			if (result <=  0) {
				break;
			}
			scanUp += 1;
		}
		while (scanUp < scanDown) {
			result = compareMethodIDs(constantPool, &pivot, &list[scanDown]);
			if (result >= 0) {
				break;
			}
			scanDown -= 1;
		}
		if (scanUp < scanDown) {
			J9CfrMethod swap = list[scanDown];
			list[scanDown] = list[scanUp];
			list[scanUp] = swap;
		}
	}  while (scanUp < scanDown);
	/*
	 * invariant: scanUp == scanDown
	 * invariant: result contains the comparison of the value at scanUp
	 * invariant: if scanUp == end, the entire list, with the possible exception of the last element, is less than or equal to the pivot
	 * invariant: everything from scanUp+1 and up is greater than the pivot value
	 * the value at scanUp may be greater, less, or same as the pivot
	 */
	Trc_BCU_Assert_Equals(scanUp, scanDown);
	if ((end - start) >= 2) { /* list of length 1 or 2 is already sorted */
		if (result <= 0) { /* scanUp value is greater than or equal to pivot */
			scanUp -= 1;
		}
		if (result >= 0) { /* scanUp value is less than or equal to pivot */
			scanDown += 1;
		}
		if (scanUp > start) {
			sortMethodIndex(constantPool, list, start, scanUp);
		}
		if (scanDown < end) {
			sortMethodIndex(constantPool, list, scanDown, end);
		}
	}
}



