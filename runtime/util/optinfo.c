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


#include "j9.h"
#include "j9cp.h"
#include "rommeth.h"
#include "util_internal.h"
#include "ut_j9vmutil.h"

#define BE_INT_AT(address) \
	(((U_32)((U_8 *)address)[3])+\
	((U_32)((U_8 *)address)[2] << 8)+\
	((U_32)((U_8 *)address)[1] << 16)+\
	((U_32)((U_8 *)address)[0] << 24))

#define COUNT_MASK(value, mask) ((value) & ((mask) << 1) - 1)

#define READ_U8(ptr) *(U_8 *)ptr
#define READ_U16(ptr) *(U_16 *)ptr
#define READ_I32(ptr) *(I_32 *)ptr
#define READ_SRP(ptr, type)  SRP_GET(*((J9SRP *)ptr), type)

static U_32* getSRPPtr (U_32 *ptr, U_32 flags, U_32 option);
static UDATA reloadClass (J9VMThread *vmThread, J9Class *originalClass, U_8 *classFileBytes, UDATA classFileSize, J9ROMClass **result);
static U_32 countBits (U_32 x);


static U_32*
getSRPPtr(U_32 *ptr, U_32 flags, U_32 option)
{
	if ((!(flags & option)) || (ptr == NULL)) {
		return NULL;
	}
	return (U_32*)((U_8*)ptr + (countBits(COUNT_MASK(flags,option)) - 1)*sizeof(U_32)); 
}


static U_32
countBits(U_32 x)
{
	U_32 count;

	for (count = 0; x!=0; x>>=1) {
		if (x & 1) {
			count++;
		}
	}

	return count;
}

U_32 *
getClassAnnotationsDataForROMClass(J9ROMClass *romClass)
{
	U_32 *ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_CLASS_ANNOTATION_INFO);
	if (NULL == ptr) {
		return NULL;
	}
	return SRP_PTR_GET(ptr, U_32 *);
}

U_32 *
getClassTypeAnnotationsDataForROMClass(J9ROMClass *romClass)
{
    U_32 *ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_TYPE_ANNOTATION_INFO);
    if (NULL == ptr) {
        return NULL;
    }
    return SRP_PTR_GET(ptr, U_32 *);
}


J9UTF8 *
getGenericSignatureForROMClass(J9JavaVM *vm, J9ClassLoader *classLoader, J9ROMClass *romClass)
{
#if defined(J9VM_OPT_DEBUG_INFO_SERVER)
	U_32 *ptr;

	ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_GENERIC_SIGNATURE);

	if(ptr == NULL) {
		return NULL;
	}
	return SRP_PTR_GET(ptr, J9UTF8 *);
#else
	return NULL;
#endif
}


UDATA
getLineNumberForROMClass(J9JavaVM *vm, J9Method *method, UDATA relativePC)
{
	UDATA bytecodeSize = J9_BYTECODE_SIZE_FROM_ROM_METHOD(J9_ROM_METHOD_FROM_RAM_METHOD(method));
	UDATA number = (UDATA)-1;
	J9LineNumber lineNumber;
	lineNumber.lineNumber = 0;
	lineNumber.location = 0;

	/* bug 113600 - bytecodeSize == 0 means that the method has been AOTd and the bytecodes have been stripped.
     So we can allow a relativePC that is >= bytecodeSize in that case */
	if ((relativePC < bytecodeSize) || (bytecodeSize == 0)) {
		J9MethodDebugInfo *methodInfo = getMethodDebugInfoForROMClass(vm, method);

		if (methodInfo) {
			U_8 *currentLineNumber= getLineNumberTable(methodInfo);
			UDATA i;
			U_32 lineNumberCount = getLineNumberCount(methodInfo);

			for (i = 0; i < lineNumberCount; i++) {
				if (!getNextLineNumberFromTable(&currentLineNumber, &lineNumber)) {
					return (UDATA)-1;
				}
				if (relativePC < lineNumber.location) {
					break;
				}
				number = lineNumber.lineNumber;
			}
			releaseOptInfoBuffer(vm, J9_CP_FROM_METHOD(method)->ramClass->romClass);
		}
	}

	return number;
}

U_32
getMethodDebugInfoStructureSize(J9MethodDebugInfo *methodInfo)
{
	if (1 == (methodInfo->lineNumberCount & 0x1)) {
		return sizeof(J9MethodDebugInfo) + sizeof(U_32);
	}
	return sizeof(J9MethodDebugInfo);
}

U_8 *
getLineNumberTable(J9MethodDebugInfo *methodInfo)
{
	if (0 != methodInfo->lineNumberCount) {
		return ((U_8 *)methodInfo) + getMethodDebugInfoStructureSize(methodInfo);
	}
	return NULL;
}


J9MethodDebugInfo *
getMethodDebugInfoForROMClass(J9JavaVM *vm, J9Method *method)
{
	/* 
	 * This is poorly named function, with an unneeded parameter (vm)
	 * TODO: create new signature
	 *  J9MethodDebugInfo *
	 *    getMethodDebugInfoForMethod(J9Method *method);
	 */
	return getMethodDebugInfoFromROMMethod(getOriginalROMMethod(method));
}


BOOLEAN
compressLineNumbers(J9CfrLineNumberTableEntry * lineNumberTableEntryArray, U_16 lineNumberTableEntryCount, J9CfrLineNumberTableEntry * lastLineNumberTableEntry, U_8 ** buffer)
{
	UDATA i;
	U_32 lastPC = 0;
	U_32 lastLineNumber = 0;
	if (NULL != lastLineNumberTableEntry) {
		lastPC = lastLineNumberTableEntry->startPC;
		lastLineNumber = lastLineNumberTableEntry->lineNumber;
	}
	for (i = 0; i < lineNumberTableEntryCount; i++) {
		I_32 pcOffset = lineNumberTableEntryArray[i].startPC - lastPC;
		I_32 lineNumberOffset = lineNumberTableEntryArray[i].lineNumber - lastLineNumber;
		U_32 result;
		U_8 * lineNumbersInfoCompressed;

		if (pcOffset < 0) {
			return FALSE;
		}
		lineNumbersInfoCompressed = *buffer;
		/* if pcOffset fits in xxxxx and lineNumberOffset fits in yy */
		if ((lineNumberOffset <= 0x3) && (pcOffset <= 0x1F) && (lineNumberOffset >= 0)) {
			/* 1 byte encoded : 0xxxxxyy */
			result = (((pcOffset << 2) & 0x7C)) | (lineNumberOffset & 0x3);
			*lineNumbersInfoCompressed = result & 0xFF;
			lineNumbersInfoCompressed++;
		} else {
			/* if pcOffset fits in xxxxx and lineNumberOffset fits in YYYYYYYYY */
			if ((pcOffset <= 0x1F) && (lineNumberOffset <= 0xFF) && (lineNumberOffset >= -256)) {
				/* 2 bytes encoded : 10xxxxxY YYYYYYYY */
				result = 0x8000 | ((pcOffset << 9) & 0x3E00) | (lineNumberOffset & 0x1FF);
				*lineNumbersInfoCompressed = (result>>8) & 0xFF;
				lineNumbersInfoCompressed++;
				*lineNumbersInfoCompressed = result & 0xFF;
				lineNumbersInfoCompressed++;
			} else {
				/* if pcOffset fits in xxxxxxx and lineNumberOffset fits in YYYYYYYYYYYYYY */
				if ((pcOffset <= 0x7F) && (lineNumberOffset <= 0x1FFF) && (lineNumberOffset >= -8192)) {
					/* 3 bytes encoded : 110xxxxx xxYYYYYY YYYYYYYY */
					U_16 *u16LineNumbersInfoCompressed;
					result = 0xC00000 | ((pcOffset << 14) & 0x1FC000) | (lineNumberOffset & 0x3FFF);
					*lineNumbersInfoCompressed = (result >> 16) & 0xFF;
					lineNumbersInfoCompressed++;
					u16LineNumbersInfoCompressed = (U_16 *)(lineNumbersInfoCompressed);
					*u16LineNumbersInfoCompressed = result & 0xFFFF;
					lineNumbersInfoCompressed += 2;
				} else {
					/* 5 bytes encoded : 1110000Y xxxxxxxx xxxxxxxx YYYYYYYY YYYYYYYY */
					U_16 *u16LineNumbersInfoCompressed;
					if (lineNumberOffset >= 0) {
						*lineNumbersInfoCompressed = 0xE0;
					} else {
						*lineNumbersInfoCompressed = 0xE0 | 1;
					}
					lineNumbersInfoCompressed++;
					u16LineNumbersInfoCompressed = (U_16 *)lineNumbersInfoCompressed;
					*u16LineNumbersInfoCompressed = pcOffset & 0xFFFF;
					u16LineNumbersInfoCompressed++;
					*u16LineNumbersInfoCompressed = lineNumberOffset & 0xFFFF;
					u16LineNumbersInfoCompressed++;
					lineNumbersInfoCompressed = (U_8 *)u16LineNumbersInfoCompressed;
				}
			}
		}
		*buffer = lineNumbersInfoCompressed;

		lastPC = lineNumberTableEntryArray[i].startPC;
		lastLineNumber = lineNumberTableEntryArray[i].lineNumber;
	}
	return TRUE;
}

BOOLEAN
getNextLineNumberFromTable(U_8 **currentLineNumber, J9LineNumber *lineNumber)
{
	U_8 *currentLineNumberPtr = *currentLineNumber;
	if (0 == (*currentLineNumberPtr & 0x80)) {
		/* 1 byte encoded : 0xxxxxyy */
		lineNumber->location += (*currentLineNumberPtr >> 2) & 0x1F;
		lineNumber->lineNumber += (*currentLineNumberPtr & 0x3);
		currentLineNumberPtr++; /* advance the pointer by one byte */
	} else if (0x80 == (*currentLineNumberPtr & 0xC0)) {
		U_32 const m = 1U << (9 - 1);

		/* 2 bytes encoded : 10xxxxxY YYYYYYYY */
		U_32 ulineNumber;
		I_32 ilineNumber;
		U_16 encoded = *currentLineNumberPtr << 8;
		currentLineNumberPtr++;
		encoded |= *currentLineNumberPtr;

		lineNumber->location += (encoded >> 9) & 0x1F;

		ulineNumber = encoded & 0x1FF;
		ilineNumber = (ulineNumber ^ m) - m; /* sign extend from 9bit */
		lineNumber->lineNumber += ilineNumber;
		currentLineNumberPtr++;
	} else if (0xC0 == (*currentLineNumberPtr & 0xE0)) { /* Comparing the 3 most significant bits to 110 */
		U_32 const m = 1U << (14 - 1);
		U_32 ulineNumber;
		I_32 ilineNumber;

		/* 3 bytes encoded : 110xxxxx xxYYYYYY YYYYYYYY */
		U_32 encoded = *currentLineNumberPtr << 16;
		currentLineNumberPtr++;
		encoded |= *((U_16*)currentLineNumberPtr);

		lineNumber->location += (encoded >> 14) & 0x7F;

		ulineNumber = encoded & 0x3FFF;
		ilineNumber = (ulineNumber ^ m) - m; /* sign extend from 14bit */
		lineNumber->lineNumber += ilineNumber;
		currentLineNumberPtr += 2;
	} else if (0xE0 == (*currentLineNumberPtr & 0xF0)) { /* Comparing the 4 most significant bits to 1110 */
		I_32 lineNumberOffset;
		U_8 firstByte = *currentLineNumberPtr;
		/* 5 bytes encoded : 1110000Y xxxxxxxx xxxxxxxx YYYYYYYY YYYYYYYY */
		currentLineNumberPtr++;
		lineNumber->location += *((U_16*)currentLineNumberPtr);
		currentLineNumberPtr += 2;

		lineNumberOffset = (I_32)(*((U_16*)currentLineNumberPtr));
		if (0x1 == (firstByte & 0x1)) {
			/* If the Y bit in 1110000Y is 1, then the lineNumberOffset is negative
			 * set the sign bit to 1
			 */
			lineNumberOffset = 0xFFFF & lineNumberOffset;
		}
		lineNumber->lineNumber += lineNumberOffset;

		currentLineNumberPtr += 2;
	} else {
		return FALSE;
	}
	*currentLineNumber = currentLineNumberPtr;
	return TRUE;
}


UDATA
compressLocalVariableTableEntry(I_32 deltaIndex, I_32 deltaStartPC, I_32 deltaLength, U_8 * buffer)
{
	/* Compression schema
	 * x = slotNumber (index)
	 * y = startVisibility (startPC)
	 * z = visibilityLength (length)
	 */
	if ((deltaIndex >= 0 && deltaIndex<=1)
		&& (deltaStartPC == 0)
		&& ((deltaLength >= -32) && (deltaLength <= 31))) { /* 6 bit */
		/* 0xZZZZZZ */
		U_8 result = 0;
		result |= deltaIndex << 6;
		result |= deltaLength & 0x3F;

		*buffer = result;
		return 1;
	} else if ((deltaIndex >= 0 && deltaIndex<=1)
			&& ((deltaStartPC >= -16) && (deltaStartPC <= 15)) /* 5 bit */
			&& ((deltaLength >= -128) && (deltaLength <= 127))) { /* 8 bit */
			/* 10xYYYYY ZZZZZZZZ */
			U_8 result = 0x80;
			result |= deltaIndex << 5;
			result |= deltaStartPC & 0x1F;

			*buffer = result;
			buffer++;

			*buffer = deltaLength & 0xFF;
			return 2;
	} else if ((deltaIndex >= 0 && deltaIndex<=1)
			&& ((deltaStartPC >= -256) && (deltaStartPC <= 255)) /* 9 bit */
			&& ((deltaLength >= -1024) && (deltaLength <= 1023))) { /* 11 bit */
			/* 110xYYYY YYYYYZZZ ZZZZZZZZ */
			U_8 result1 = 0xC0;
			U_16 result2;
			result1 |= deltaIndex << 4;
			result1 |= (deltaStartPC >> 5) & 0xF;

			*buffer = result1;
			buffer++;

			result2 = (deltaStartPC << 11) & 0xFFFF;
			result2 |= deltaLength & 0x7FF;

			*(U_16*)buffer = result2;
			return 3;
	} else if ((deltaIndex >= 0 && deltaIndex<=3)
			&& ((deltaStartPC >= -32768) && (deltaStartPC <= 32767)) /* 16 bit */
			&& ((deltaLength >= -131072) && (deltaLength <= 131071))) { /* 18 bit */
			/* 1110xxZZ ZZZZZZZZ ZZZZZZZZ YYYYYYYY YYYYYYYY */
			U_8 result1 = 0xE0;
			result1 |= deltaIndex << 2;
			result1 |= (deltaLength >> 16) & 0x3;

			*buffer = result1;
			buffer++;

			*(U_16*)buffer = deltaLength & 0xFFFF;
			buffer += sizeof(U_16);

			*(U_16*)buffer = deltaStartPC & 0xFFFF;
			return 5;
	} else {
		/* 11110000 FULL DATA in case it overflow in some classes */
		U_8 result1 = 0xF0;
		*buffer = result1;
		buffer++;

		*(U_32*)buffer = deltaIndex;
		buffer += sizeof(U_32);

		*(U_32*)buffer = deltaStartPC;
		buffer += sizeof(U_32);

		*(U_32*)buffer = deltaLength;
		return 13;
	}
}

J9SourceDebugExtension *
getSourceDebugExtensionForROMClass(J9JavaVM *vm, J9ClassLoader *classLoader, J9ROMClass *romClass)
{
#if defined(J9VM_OPT_DEBUG_INFO_SERVER)
	U_32 *ptr;

	ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_SOURCE_DEBUG_EXTENSION);

	if(ptr == NULL) {
		return NULL;
	}
	return SRP_PTR_GET(ptr, J9SourceDebugExtension *);
#else
	return NULL;
#endif
}


J9UTF8 *
getSourceFileNameForROMClass(J9JavaVM *vm, J9ClassLoader *classLoader, J9ROMClass *romClass)
{
#if defined(J9VM_OPT_DEBUG_INFO_SERVER)
	U_32 *ptr;

	ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_SOURCE_FILE_NAME);

	if(ptr == NULL) {
		return NULL;
	}
	return SRP_PTR_GET(ptr, J9UTF8 *);
#else
	return NULL;
#endif
}


U_32
getLineNumberCount(J9MethodDebugInfo *methodInfo) {
	if (0 == (methodInfo->lineNumberCount & 1)) {
		return (methodInfo->lineNumberCount >> 1) & 0x7FFF;
	} else {
		return methodInfo->lineNumberCount >> 1;
	}
}
U_32
getLineNumberCompressedSize(J9MethodDebugInfo *methodInfo) {
	if ((methodInfo->lineNumberCount & 1) == 0) {
		return (methodInfo->lineNumberCount >> 16) & 0xFFFF;
	} else {
		return *((U_32*)(methodInfo + 1));
	}
}

U_8 *
getVariableTableForMethodDebugInfo(J9MethodDebugInfo *methodInfo) {
	if (0 != methodInfo->varInfoCount) {
		if (1 == (methodInfo->srpToVarInfo & 1)) {
			/* 
			 * low tag indicates that debug information is in line 
			 * skip over J9MethodDebugInfo header and the J9LineNumber table
			 */
			U_32 lineNumberTableCompressedSize = getLineNumberCompressedSize(methodInfo);
			if (0 == methodInfo->lineNumberCount) {
				return ((U_8 *)(methodInfo + 1));
			} else {
				return getLineNumberTable(methodInfo) + lineNumberTableCompressedSize;
			}
		} else {
			/* 
			 * debug information is out of line, this slot is an SRP to the
			 * J9VariableInfo table
			 */
			return (SRP_GET(methodInfo->srpToVarInfo, U_8 *));
		}
	}
	return NULL;
}

J9VariableInfoValues * 
variableInfoStartDo(J9MethodDebugInfo * methodInfo, J9VariableInfoWalkState* state)
{
	state->variablesLeft = methodInfo->varInfoCount;
	if (state->variablesLeft == 0) {
		return NULL;
	}
	
	state->variableTablePtr = getVariableTableForMethodDebugInfo(methodInfo);
	state->values.slotNumber = 0;
	state->values.startVisibility = 0;
	state->values.visibilityLength = 0;

	return variableInfoNextDo(state);
}

J9VariableInfoValues *
variableInfoNextDo(J9VariableInfoWalkState *state)
{
	U_8 firstByte;
	if (state->variablesLeft == 0) {
		return NULL;
	}

	/* Compression schema
	 * x = slotNumber
	 * y = startVisibility
	 * z = visibilityLength
	 */

	firstByte = READ_U8(state->variableTablePtr);

	if (0 == (firstByte & 0x80)) {
		/* 0xZZZZZZ */
		U_32 const m = 1U << (6 - 1);
		U_8 result = firstByte;
		state->variableTablePtr += sizeof(U_8);

		state->values.slotNumber += (result >> 6);
		state->values.visibilityLength += (((result & 0x3F) ^ m) - m); /* sign extend from 6bit */;
	} else if (0x80 == (firstByte & 0xC0)) {
		/* 10xYYYYY ZZZZZZZZ */
		U_32 const m5 = 1U << (5 - 1);
		U_32 const m8 = 1U << (8 - 1);
		U_8 result = firstByte;
		state->variableTablePtr += sizeof(U_8);

		state->values.slotNumber += ((result >> 5) & 1);
		state->values.startVisibility += (((result & 0x1F) ^ m5) - m5); /* sign extend from 5bit */;
		firstByte = READ_U8(state->variableTablePtr);
		state->values.visibilityLength += ((firstByte ^ m8) - m8); /* sign extend from 8bit */;
		state->variableTablePtr += sizeof(U_8);
	} else if (0xC0 == (firstByte & 0xE0)) { /* Comparing the 3 most significant bits to 110 */
		/* 110xYYYY YYYYYZZZ ZZZZZZZZ */
		U_32 const m9 = 1U << (9 - 1);
		U_32 const m11 = 1U << (11 - 1);
		U_32 result = firstByte;
		U_32 startVisibilityUnsigned;
		U_16 twoBytes;
		state->variableTablePtr += sizeof(U_8);

		state->values.slotNumber += ((result >> 4) & 1);
		result = result << 16;
		twoBytes = READ_U16(state->variableTablePtr);
		result |= twoBytes;
		state->variableTablePtr += sizeof(U_16);

		startVisibilityUnsigned = (result >> 11) & 0x1FF;
		state->values.startVisibility += ((startVisibilityUnsigned ^ m9) - m9); /* sign extend from 9bit */;
		state->values.visibilityLength += (((result & 0x7FF) ^ m11) - m11); /* sign extend from 11bit */;
	} else if (0xE0 == (firstByte & 0xF0)) { /* Comparing the 4 most significant bits to 1110 */
		/* 1110xxZZ ZZZZZZZZ ZZZZZZZZ YYYYYYYY YYYYYYYY */
		U_32 const m16 = 1U << (16 - 1);
		U_32 const m18 = 1U << (18 - 1);
		U_32 result = firstByte;
		U_32 visibilityLengthUnsigned, startVisibilityUnsigned;
		U_16 twoBytes;
		state->variableTablePtr += sizeof(U_8);

		state->values.slotNumber += ((result >> 2) & 3);
		visibilityLengthUnsigned = ((result & 3) << 16);
		twoBytes = READ_U16(state->variableTablePtr);
		visibilityLengthUnsigned |= twoBytes;
		state->variableTablePtr += sizeof(U_16);
		state->values.visibilityLength += ((visibilityLengthUnsigned ^ m18) - m18); /* sign extend from 18bit */;

		twoBytes = READ_U16(state->variableTablePtr);
		startVisibilityUnsigned = twoBytes;
		state->variableTablePtr += sizeof(U_16);
		state->values.startVisibility += ((startVisibilityUnsigned ^ m16) - m16); /* sign extend from 16bit */;
	} else if (0xF0 == (firstByte)) { /* Comparing the 8 most significant bits to 0xF0 */
		/* 11110000	FULL DATA in case it overflow in some classes */
		I_32 integerValue;
		state->variableTablePtr += sizeof(U_8);
		integerValue = READ_I32(state->variableTablePtr);
		state->values.slotNumber += integerValue;
		state->variableTablePtr += sizeof(I_32);
		integerValue = READ_I32(state->variableTablePtr);
		state->values.startVisibility += integerValue;
		state->variableTablePtr += sizeof(I_32);
		integerValue = READ_I32(state->variableTablePtr);
		state->values.visibilityLength += integerValue;
		state->variableTablePtr += sizeof(I_32);
	} else {
		return NULL;
	}

	state->values.nameSrp = (J9SRP *)state->variableTablePtr;
	state->values.name = READ_SRP(state->variableTablePtr, J9UTF8*);
	state->variableTablePtr += sizeof(J9SRP);
	state->values.signatureSrp = (J9SRP *)state->variableTablePtr;
	state->values.signature = READ_SRP(state->variableTablePtr, J9UTF8*);
	state->variableTablePtr += sizeof(J9SRP);

	if (state->values.visibilityLength & J9_ROMCLASS_OPTINFO_VARIABLE_TABLE_HAS_GENERIC) {
		state->values.genericSignatureSrp = (J9SRP *)state->variableTablePtr;
		state->values.genericSignature = READ_SRP(state->variableTablePtr, J9UTF8*);
		state->variableTablePtr += sizeof(J9SRP);
	} else {
		state->values.genericSignatureSrp = 0;
		state->values.genericSignature = NULL;
	}

	state->values.visibilityLength = state->values.visibilityLength & ~J9_ROMCLASS_OPTINFO_VARIABLE_TABLE_HAS_GENERIC;
	--(state->variablesLeft);
	return &state->values;
}

void
releaseOptInfoBuffer(J9JavaVM *vm, J9ROMClass *romClass)
{
}


J9EnclosingObject *
getEnclosingMethodForROMClass(J9JavaVM *vm, J9ClassLoader *classLoader, J9ROMClass *romClass)
{
#if defined(J9VM_OPT_DEBUG_INFO_SERVER)
	U_32 *ptr;

	ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_ENCLOSING_METHOD);

	if(ptr == NULL) {
		return NULL;
	}
	return SRP_PTR_GET(ptr, J9EnclosingObject *);
#else
	return NULL;
#endif
}


J9UTF8 *
getSimpleNameForROMClass(J9JavaVM *vm, J9ClassLoader *classLoader, J9ROMClass *romClass)
{
#if defined(J9VM_OPT_DEBUG_INFO_SERVER)
	U_32 *ptr;

	ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_SIMPLE_NAME);

	if(ptr == NULL) {
		return NULL;
	}
	return SRP_PTR_GET(ptr, J9UTF8 *);
#else
	return NULL;
#endif
}

UDATA
getLineNumberForROMClassFromROMMethod(J9JavaVM *vm, J9ROMMethod *romMethod, J9ROMClass *romClass, J9ClassLoader *classLoader, UDATA relativePC)
{
	UDATA bytecodeSize = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	UDATA number = (UDATA)-1;
	J9LineNumber lineNumber;
	lineNumber.lineNumber = 0;
	lineNumber.location = 0;

	/* bug 113600 - bytecodeSize == 0 means that the method has been AOTd and the bytecodes have been stripped.
     So we can allow a relativePC that is >= bytecodeSize in that case */
	if ((relativePC < bytecodeSize) || (bytecodeSize == 0)) {
		J9MethodDebugInfo *methodInfo = getMethodDebugInfoFromROMMethod(romMethod);

		if (methodInfo) {
			U_8 *currentLineNumber= getLineNumberTable(methodInfo);
			UDATA i;
			U_32 lineNumberCount = getLineNumberCount(methodInfo);

			for (i = 0; i < lineNumberCount; i++) {
				if (!getNextLineNumberFromTable(&currentLineNumber, &lineNumber)) {
					return (UDATA)-1;
				}
				if (relativePC < lineNumber.location) {
					break;
				}
				number = lineNumber.lineNumber;
			}
			releaseOptInfoBuffer(vm, romClass);
		}
	}

	return number;
}

U_32*
getNumberOfPermittedSubclassesPtr(J9ROMClass *romClass)
{
	U_32 *ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_PERMITTEDSUBCLASSES_ATTRIBUTE);

	Assert_VMUtil_true(ptr != NULL);

	return SRP_PTR_GET(ptr, U_32*);
}

J9UTF8*
permittedSubclassesNameAtIndex(U_32* permittedSubclassesCountPtr, U_32 index)
{
	/* SRPs to PermittedSubclass name constant pool entries start after the permittedSubclassCountPtr */
	U_32* permittedSubclassPtr = permittedSubclassesCountPtr + 1 + index;

	return NNSRP_PTR_GET(permittedSubclassPtr, J9UTF8*);
}

U_32
getNumberOfRecordComponents(J9ROMClass *romClass)
{
	U_32 *ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_RECORD_ATTRIBUTE);

	Assert_VMUtil_true(ptr != NULL);

	return *SRP_PTR_GET(ptr, U_32*);
}

#if defined(J9VM_OPT_VALHALLA_VALUE_TYPES)
U_32
getNumberOfInjectedInterfaces(J9ROMClass *romClass) {
	U_32 *ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_INJECTED_INTERFACE_INFO);

	Assert_VMUtil_true(ptr != NULL);

	return *SRP_PTR_GET(ptr, U_32 *);
}

U_32 *
getLoadableDescriptorsInfoPtr(J9ROMClass *romClass)
{
	U_32 *ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_LOADABLEDESCRIPTORS_ATTRIBUTE);

	Assert_VMUtil_true(ptr != NULL);

	return SRP_PTR_GET(ptr, U_32 *);
}

J9UTF8 *
loadableDescriptorAtIndex(U_32 *loadableDescriptorsInfoPtr, U_32 index)
{
	/* SRPs to loadable descriptors constant pool entries start after the count. */
	U_32 *loadableDescriptorsPtr = loadableDescriptorsInfoPtr + 1 + index;

	return NNSRP_PTR_GET(loadableDescriptorsPtr, J9UTF8 *);
}
#endif /* J9VM_OPT_VALHALLA_VALUE_TYPES */
#if defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES)
U_16
getImplicitCreationFlags(J9ROMClass *romClass)
{
	U_32 *ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_IMPLICITCREATION_ATTRIBUTE);
	U_32* implicitCreationInfo = NULL;

	Assert_VMUtil_true(ptr != NULL);
	implicitCreationInfo = SRP_PTR_GET(ptr, U_32*);

	return (U_16)*implicitCreationInfo;
}
#endif /* defined(J9VM_OPT_VALHALLA_FLATTENABLE_VALUE_TYPES) */

BOOLEAN
recordComponentHasSignature(J9ROMRecordComponentShape* recordComponent)
{
	return (recordComponent->attributeFlags & J9RecordComponentFlagHasGenericSignature);
}

BOOLEAN
recordComponentHasAnnotations(J9ROMRecordComponentShape* recordComponent)
{
	return (recordComponent->attributeFlags & J9RecordComponentFlagHasAnnotations);
}

BOOLEAN
recordComponentHasTypeAnnotations(J9ROMRecordComponentShape* recordComponent)
{
	return (recordComponent->attributeFlags & J9RecordComponentFlagHasTypeAnnotations);
}

J9UTF8*
getRecordComponentGenericSignature(J9ROMRecordComponentShape* recordComponent)
{
	if (recordComponentHasSignature(recordComponent)) {	
		U_32* signaturePtr = (U_32*) ((UDATA)recordComponent + sizeof(J9ROMRecordComponentShape));
		return NNSRP_PTR_GET(signaturePtr, J9UTF8*);
	}
	return NULL;
}

U_32*
getRecordComponentAnnotationData(J9ROMRecordComponentShape* recordComponent)
{
	U_32* result = NULL;
	if (recordComponentHasAnnotations(recordComponent)) {
		/* calculate offset from start of record component */
		UDATA offset = sizeof(J9ROMRecordComponentShape);
		/* U_32 for generic signature if it exists */
		if (recordComponentHasSignature(recordComponent)) {
			offset += sizeof(U_32);
		}
		result = (U_32*)((UDATA)recordComponent + offset);
	}
	return result;
}

static UDATA
annotationAttributeSize(U_8* annotationAttribute) {
	UDATA size = 0;
	Assert_VMUtil_true(((UDATA)annotationAttribute % sizeof(U_32)) == 0);
	size = sizeof(U_32);		/* size of length */
	size += *((U_32 *)annotationAttribute);	/* length of attribute */
	return ROUND_UP_TO_POWEROF2(size, sizeof(U_32));	/* padding */
} 

U_32*
getRecordComponentTypeAnnotationData(J9ROMRecordComponentShape* recordComponent)
{
	U_32* result = NULL;
	if (recordComponentHasTypeAnnotations(recordComponent)) {
		if (recordComponentHasAnnotations(recordComponent)) {
			/* use previous annotation result to calculate offset since size is not known */
			U_8 *recordComponentAnnotationData = (U_8 *)getRecordComponentAnnotationData(recordComponent);
			result = (U_32*)(recordComponentAnnotationData + annotationAttributeSize(recordComponentAnnotationData));
		} else {
			UDATA offset = sizeof(J9ROMRecordComponentShape);
			if (recordComponentHasSignature(recordComponent)) {
				offset += sizeof(U_32);
			}
			result = (U_32*)((UDATA)recordComponent + offset);
		}
	}
	return result;
}

J9ROMRecordComponentShape* 
recordComponentStartDo(J9ROMClass *romClass)
{
	U_32 *srp = NULL;
	U_32 *ptr = getSRPPtr(J9ROMCLASS_OPTIONALINFO(romClass), romClass->optionalFlags, J9_ROMCLASS_OPTINFO_RECORD_ATTRIBUTE);

	Assert_VMUtil_true(ptr != NULL);

	/* first 4 bytes of record entry is the size */
	srp = SRP_PTR_GET(ptr, U_32*);
	srp += 1;
	return (J9ROMRecordComponentShape *)srp;
}

J9ROMRecordComponentShape* 
recordComponentNextDo(J9ROMRecordComponentShape* recordComponent)
{
	UDATA recordComponentSize = sizeof(J9ROMRecordComponentShape);

	if (recordComponentHasSignature(recordComponent)) {
		recordComponentSize += sizeof(U_32);
	}
	if (recordComponentHasAnnotations(recordComponent)) {
		recordComponentSize += annotationAttributeSize((U_8*)recordComponent + recordComponentSize);
	}
	if (recordComponentHasTypeAnnotations(recordComponent)) {
		recordComponentSize += annotationAttributeSize((U_8*)recordComponent + recordComponentSize);
	}
	return (J9ROMRecordComponentShape*)((U_8*)recordComponent + recordComponentSize);
}

