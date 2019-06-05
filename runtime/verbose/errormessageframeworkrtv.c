/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

#include "errormessage_internal.h"
#include "verbose_internal.h"
#include "vrfytbl.h"

/* Mapping the verification encoding in J9JavaBytecodeVerificationTable
 * Note: decodeTable[] was originally declared in bcverify/vrfyconvert.c and
 * can't be used outside of the bcverify directory. So, decodeTable[] has to be duplicated here for internal use.
 */
static const U_32 decodeTable[] = {
	0x0,						/* return index */
	BCV_BASE_TYPE_INT,			/* CFR_STACKMAP_TYPE_INT */
	BCV_BASE_TYPE_FLOAT,		/* CFR_STACKMAP_TYPE_FLOAT */
	BCV_BASE_TYPE_DOUBLE,		/* CFR_STACKMAP_TYPE_DOUBLE */
	BCV_BASE_TYPE_LONG,			/* CFR_STACKMAP_TYPE_LONG */
	BCV_BASE_TYPE_NULL,			/* CFR_STACKMAP_TYPE_NULL */
	0x6,						/* return index */
	BCV_GENERIC_OBJECT,			/* CFR_STACKMAP_TYPE_OBJECT */
	0x8,						/* return index */
	0x9,						/* return index */
	0xA,						/* return index */
	0xB,						/* return index */
	0xC,						/* return index */
	0xD,						/* return index */
	0xE,						/* return index */
	0xF							/* return index */
};

static const char* framePositionName[] = { "locals", "stack" };

static void getJ9RtvExceptionTableEntry(ExceptionTableEntry* buf, void* exceptionTable, UDATA index);
static void getJ9RtvUTF8StringfromCP(J9UTF8Ref* buf, void* constantPool, UDATA index);
static void getJ9RtvStringfromClassNameList(J9UTF8Ref* buf, void* constantPool, void* romClass, UDATA index);

static void constructRtvMethodContextInfo(MethodContextInfo* methodInfo, J9BytecodeVerificationData* verifyData);
static BOOLEAN pushLiveStackToVerificationTypeBuffer(StackMapFrame* stackMapFrame, J9BytecodeVerificationData* verifyData, MethodContextInfo* methodInfo);
static BOOLEAN isWideType2nd(J9UTF8Ref* dataType, StackMapFrame* stackMapFrame, MethodContextInfo* methodInfo, UDATA index, U_8 tag);
static void getNonClassTypeName(J9UTF8Ref* dataType, U_8 typeNameIndex, UDATA bcvType);
static U_8 getBCVDataType(J9BytecodeVerificationData* verifyData, MethodContextInfo* methodInfo, J9UTF8Ref* dataType, UDATA bcvType);

static BOOLEAN compareStackMapFrameFlag(StackMapFrame* CurrentFrame, StackMapFrame* stackMapFrame);
static BOOLEAN printReasonForIncompatibleType(MessageBuffer *buf, J9BytecodeVerificationData *verifyData, MethodContextInfo *methodInfo, StackMapFrame *currentFrame, StackMapFrame *targetFrame);
static BOOLEAN printExpectedTypeFromStackMapFrame(MessageBuffer *msgBuf, J9BytecodeVerificationData *verifyData, MethodContextInfo *methodInfo, StackMapFrame *currentFrame, StackMapFrame *targetFrame, U_32 localStackIndex);
static void printExpectedType(MessageBuffer *msgBuf, J9BytecodeVerificationData *verifyData, MethodContextInfo *methodInfo);
static void printWrongDataType(MessageBuffer *msgBuf, J9BytecodeVerificationData *verifyData, MethodContextInfo *methodInfo);
static BOOLEAN setStackMapFrameWithIndex(J9BytecodeVerificationData *verifyData, MethodContextInfo *methodInfo, StackMapFrame *targetFrame);
static BOOLEAN printReasonForFlagMismatch(MessageBuffer *msgBuf, J9BytecodeVerificationData *verifyData, MethodContextInfo *methodInfo, StackMapFrame *currentFrame, StackMapFrame *targetFrame);
static BOOLEAN adjustStackTopForWrongDataType(J9BytecodeVerificationData *verifyData);


/**
 * Retrieve startPC, endPC and handlerPC from J9ExceptionHandler for runtime verification
 * @param buf - buffer to hold the return value
 * @param exceptionTable - opaque type being cast to J9ExceptionHandler
 * @param index - index to the exceptionTable entry
 */
static void
getJ9RtvExceptionTableEntry(ExceptionTableEntry* buf, void* exceptionTable, UDATA index)
{
	J9ExceptionHandler* table = (J9ExceptionHandler*)exceptionTable;
	buf->startPC = table[index].startPC;
	buf->endPC = table[index].endPC;
	buf->handlerPC = table[index].handlerPC;
}

/**
 * Retrieve name or type constant from J9ROMConstantPoolItem data (runtime verification)
 * @param buf - buffer to hold the return value
 * @param constantPool - opaque type being cast to J9ROMConstantPoolItem
 * @param index - index of UTF8 string in the constant pool
 */
static void
getJ9RtvUTF8StringfromCP(J9UTF8Ref* buf, void* constantPool, UDATA index)
{
	J9ROMConstantPoolItem* items = (J9ROMConstantPoolItem*)constantPool;
	Assert_VRB_notNull(buf);

	if (NULL != items) {
		J9UTF8 *utf8string = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) (&items[index]));
		buf->bytes = J9UTF8_DATA(utf8string);
		buf->length = J9UTF8_LENGTH(utf8string);
	}
}

/**
 * Retrieve name or type constant from verifyData->classNameList data (Refer to getNameAndLengthFromClassNameList at vrfyhelp.c)
 * @param buf - buffer to hold the return value
 * @param classNameListPtr - pointer to classNameListPtr being cast to struct J9UTF8**
 * @param romClass - pointer to verifyData->romClass
 * @param classIndex - class index in the classNameList array
 */
static void
getJ9RtvStringfromClassNameList(J9UTF8Ref* buf, void* classNameListPtr, void* romClass, UDATA classIndex)
{
	struct J9UTF8** classNameList = (struct J9UTF8**)classNameListPtr;
	U_32* offset = (U_32*)classNameList[BCV_INDEX_FROM_TYPE(classIndex)];

	buf->length = J9UTF8_LENGTH(offset + 1);

	/* addClassName() in vrfyhelp.c:
	 * Note: offset[0] is set up in addClassName() to determine where to obtain the class name (classNameSegment or romClass).
	 * So, the layout of class name in the class name list includes:
	 * offset[0] (0 if in classNameSegment or class name offset in romClass), class name (J9UTF8) in classNameSegment (offset[0] == 0), and NULL.
	 */
	if (0 == offset[0]) {
		/* class name exists in the classNameSegment */
		buf->bytes = (U_8*)J9UTF8_DATA(offset + 1);
	} else {
		/* class name exists in the ROM class */
		buf->bytes = (U_8*)((UDATA) offset[0] + (UDATA) romClass);
	}

	buf->arity = (U_8)BCV_ARITY_FROM_TYPE(classIndex);
}

/**
 * Initialize the MethodContextInfo data from J9BytecodeVerificationData
 * to simplify all operations related to J9BytecodeVerificationData.
 * @param methodInfo - pointer to the MethodContextInfo structure under construction
 * @param verifyData - pointer to the verification data used to build the MethodContextInfo object
 */
static void
constructRtvMethodContextInfo(MethodContextInfo* methodInfo, J9BytecodeVerificationData* verifyData)
{
	J9ROMClass * romClass = verifyData->romClass;
	J9ROMMethod *romMethod = verifyData->romMethod;
	U_32 *stackMapMethod = getStackMapInfoForROMMethod(romMethod);
	J9ExceptionInfo *exceptionInfo = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);

	methodInfo->portLib = verifyData->portLib;
	methodInfo->romClass = romClass;
	methodInfo->classNameList = verifyData->classNameList;
	methodInfo->modifier = romMethod->modifiers;

	methodInfo->constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);
	methodInfo->code = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	methodInfo->codeLength = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	methodInfo->maxLocals = (U_16)(J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod));
	methodInfo->maxStack = (U_16)(J9_MAX_STACK_FROM_ROM_METHOD(romMethod));

	/* Class */
	methodInfo->className.bytes = J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass));
	methodInfo->className.length = J9UTF8_LENGTH((J9ROMCLASS_CLASSNAME(romClass)));

	/* Method */
	methodInfo->methodName.bytes = J9UTF8_DATA(J9ROMMETHOD_GET_NAME(romClass, romMethod));
	methodInfo->methodName.length = J9UTF8_LENGTH((J9ROMMETHOD_GET_NAME(romClass, romMethod)));

	/* Signature */
	methodInfo->signature.bytes = J9UTF8_DATA(J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod));
	methodInfo->signature.length = J9UTF8_LENGTH((J9ROMMETHOD_GET_SIGNATURE(romClass, romMethod)));

	/* Exception handler table */
	methodInfo->exceptionTable = NULL;
	methodInfo->exceptionTableLength = 0;

	/* Point to the exception handler table if exists */
	if (J9_ARE_ALL_BITS_SET(methodInfo->modifier, CFR_ACC_HAS_EXCEPTION_INFO)) {
		methodInfo->exceptionTable = J9EXCEPTIONINFO_HANDLERS(exceptionInfo);
		methodInfo->exceptionTableLength = exceptionInfo->catchCount;
	}

	/* Stackmap table */
	methodInfo->stackMapData = NULL;
	methodInfo->stackMapCount = 0;
	methodInfo->stackMapLength = 0;

	/* It is required to use the compressed stackmap table in the class file for the output format of framework
	 * to print out data in the stackmap table so as to match Oracle's behavior.
	 * Note: verifyData->stackMaps points to the decompressed stackmap table (constructed against
	 * the bytecode during runtime verification if the stackmap tale doesn't exist)
	 */
	if (NULL != stackMapMethod) {
		methodInfo->stackMapData = (U_8 *)(stackMapMethod + 1);
		NEXT_U16(methodInfo->stackMapCount, methodInfo->stackMapData);
		methodInfo->stackMapLength = (U_32)((verifyData->stackSize) * (verifyData->stackMapsCount));
	}

	/* Register callback functions dealing with the constant pool, the class name list, and the exception handler table */
	methodInfo->getUTF8StringfromCP = getJ9RtvUTF8StringfromCP;
	methodInfo->getStringfromClassNameList = getJ9RtvStringfromClassNameList;
	methodInfo->getExceptionRange = getJ9RtvExceptionTableEntry;
}

/**
 * Push the converted data type in locals/stacks on verifyData->liveStack to the verification type buffer.
 * @param stackMapFrame - pointer to the specified stackmap frame
 * @param verifyData - pointer to the verification data
 * @param methodInfo - pointer to the MethodContextInfo that holds method-related data and utilities
 * @return TRUE on success; otherwise return FALSE if out-of-memory
 */
static BOOLEAN
pushLiveStackToVerificationTypeBuffer(StackMapFrame* stackMapFrame, J9BytecodeVerificationData* verifyData, MethodContextInfo* methodInfo)
{
	J9BranchTargetStack* liveStack = (J9BranchTargetStack *) verifyData->liveStack;
	IDATA stackBaseIndex = liveStack->stackBaseIndex;
	IDATA stackTopIndex = liveStack->stackTopIndex;
	IDATA errorCurrentFramePosition = (IDATA)verifyData->errorCurrentFramePosition;
	VerificationTypeInfo* currentVerificationTypeEntry = stackMapFrame->entries;
	U_16 maxStack = methodInfo->maxStack;
	U_16 maxLocals = methodInfo->maxLocals;
	IDATA lastIndex = 0;
	IDATA slot = 0;

	IDATA errorPositionInclusive = 1;
	IDATA wideType = DATATYPE_1_SLOT;
	BOOLEAN result = TRUE;
	BOOLEAN nonTopFound = FALSE;

	stackMapFrame->bci = (U_16)liveStack->pc;

	/* 'locals' on liveStack */

	/* Determine the count of local variables on 'locals' (liveStack) starting from the right end of 'locals'.
	 * Note: It is true that placeholders ('top') always occur on the right end of 'locals'.
	 * e.g. data1, data2, data3, top, top, top, ...
	 * However, local variables can be updated at any place of 'locals' in the bytecode
	 * e.g. data1, data2, data3, top, top, data4, top, data5, top, ...
	 * In this case, we need to count all data types from data1 to data5 including placeholders ('top') for later printing
	 * as the verification error may occur on data4 or data5.
	 *
	 * The idea of the algorithm below is to loop from the right end of 'locals' until we find a non-top element
	 * that is either part of long/double or other data type.
	 */
	for (lastIndex = liveStack->stackBaseIndex - 1; (!nonTopFound) && (lastIndex > 0); lastIndex--) {
		switch (liveStack->stackElements[lastIndex]) {
		case BCV_BASE_TYPE_TOP:		/* Check whether the data type is long/double */
			if ((BCV_BASE_TYPE_LONG == liveStack->stackElements[lastIndex - 1])
			|| (BCV_BASE_TYPE_DOUBLE == liveStack->stackElements[lastIndex - 1])
			) {
				nonTopFound = TRUE;
			}
			break;
		default:					/* Other non-top data type */
			nonTopFound = TRUE;
			break;
		}
	}

	/* Step forward by 1 slot to point to the non-top element or the 2nd slot of long/double
	 * as we retreat 1 step when the non-top element was detected in the for loop.
	 */
	if (nonTopFound) {
		lastIndex += 1;
	}

	stackMapFrame->numberOfLocals = (U_16)(lastIndex + 1);

	/* Convert all data type on 'locals' (below liveStack->stackBaseIndex) so as to push them to the verification type buffer for later printing */
	while (slot <= lastIndex) {
		wideType = convertBcvToCfrType(methodInfo, stackMapFrame, &currentVerificationTypeEntry, liveStack->stackElements[slot]);
		if (DATATYPE_OUT_OF_MEMORY == wideType) {
			result = FALSE;
			goto exit;
		}
		slot += wideType;
	}

	/* Fill up 'top' type for the rest of slots on 'locals' to the verification type buffer */
	currentVerificationTypeEntry = pushTopTypeToVerificationTypeBuffer(methodInfo, stackMapFrame, currentVerificationTypeEntry, maxLocals - stackMapFrame->numberOfLocals);
	if (NULL == currentVerificationTypeEntry) {
		result = FALSE;
		goto exit;
	}

	/* 'stack' on liveStack */

	/* Set to the location of wrong data type on 'stack' */
	lastIndex = errorCurrentFramePosition;

	/* The idea here is to ensure it prints out all data types on 'stack' up to the location of wrong data type.
	 * There are 2 cases for this behavior:
	 * 1) If the location of wrong data type is on 'locals', then it uses stackTopIndex 
	 *    or stackBaseIndex (in the case of stack underflow) to locate the existing data types on 'stack'.
	 * 2) If the wrong data type exists on 'stack', then it prints out all data types up to errorCurrentFramePosition.
	 */
	if (lastIndex < stackBaseIndex) {
		lastIndex = (stackTopIndex <= stackBaseIndex) ? stackBaseIndex : stackTopIndex;
	} else {
		lastIndex += errorPositionInclusive;
	}

	/* The 'stack' size should not exceed maxStack */
	if ((U_16)(lastIndex - stackBaseIndex) > maxStack) {
		lastIndex = maxStack + stackBaseIndex;
	}

	currentVerificationTypeEntry = &stackMapFrame->entries[maxLocals];
	stackMapFrame->numberOfStack = (U_16)(lastIndex - stackBaseIndex);

	/* Convert all data type on 'stack' (starting from liveStack->stackBaseIndex) so as to push them to the verification type buffer for later printing */
	slot = stackBaseIndex;
	while (slot < lastIndex) {
		wideType = convertBcvToCfrType(methodInfo, stackMapFrame, &currentVerificationTypeEntry, liveStack->stackElements[slot]);
		if (DATATYPE_OUT_OF_MEMORY == wideType) {
			result = FALSE;
			goto exit;
		}
		slot += wideType;
	}

	/* Fill up 'top' type for the rest of slots on 'stack' to the verification type buffer */
	currentVerificationTypeEntry = pushTopTypeToVerificationTypeBuffer(methodInfo, stackMapFrame, currentVerificationTypeEntry, maxStack - stackMapFrame->numberOfStack);
	if (NULL == currentVerificationTypeEntry) {
		result = FALSE;
	}

exit:
	return result;
}


/**
 * Compare the flag value of the current stackmap frame with that of the target stackmap frame.
 * @param CurrentFrame - pointer to the StackMapFrame that defines current frame state
 * @param stackMapFrame - pointer to the StackMapFrame that defines target frame state
 * @return TRUE if identical or FALSE if not
 */
static BOOLEAN
compareStackMapFrameFlag(StackMapFrame* currentFrame, StackMapFrame* stackMapFrame)
{
	BOOLEAN CurrentflagUnInitThis = FALSE;
	BOOLEAN StackflagUnInitThis = FALSE;
	U_16 numberOfLocals1 = currentFrame->numberOfLocals;
	U_16 numberOfLocals2 = stackMapFrame->numberOfLocals;
	IDATA index = 0;

	/*
	 * According to 4.10.1.4 Stack Map Frame Representation at JVM specification 8,
	 * 'Flags' is a list which may either be empty or have the single element 'flagThisUninit'.
	 * 'flagThisUninit' is used in constructors to mark type states where initialization of this has not yet been completed.
	 * If any local variable in Locals has the type 'uninitializedThis',
	 * then 'Flags' has the single element 'flagThisUninit', otherwise 'Flags' is an empty list.
	 *
	 * The idea is to check whether 'uninitializedThis' exists in local variables on the current stack
	 * and compare it against the flag in the stackmap frame.
	 */
	for (index = 0; index < numberOfLocals1; index++) {
		if (CFR_STACKMAP_TYPE_INIT_OBJECT == currentFrame->entries[index].typeTag) {
			CurrentflagUnInitThis = TRUE;
			break;
		}
	}

	for (index = 0; index < numberOfLocals2; index++) {
		if (CFR_STACKMAP_TYPE_INIT_OBJECT == stackMapFrame->entries[index].typeTag) {
			StackflagUnInitThis = TRUE;
			break;
		}
	}

	return (CurrentflagUnInitThis == StackflagUnInitThis);
}

/**
 * Determine whether the specified type data is the second slot of wide type or not.
 * @param dataType - data type to be printed out
 * @param stackMapFrame - pointer to the StackMapFrame that defines current frame state
 * @param methodInfo - pointer to the MethodContextInfo that holds method related data and utilities
 * @param index - index to the stackmap frame that triggers verifying error
 * @param tag - the type value
 * @return TRUE if the data type is long or double; otherwise return FALSE
 */
static BOOLEAN
isWideType2nd(J9UTF8Ref* dataType, StackMapFrame* stackMapFrame, MethodContextInfo* methodInfo, UDATA index, U_8 tag)
{
	BOOLEAN wideType2ndSlot = FALSE;

	if ((CFR_STACKMAP_TYPE_TOP == tag) && (index > 0)) {
		U_8 wideTypeTag = stackMapFrame->entries[index - 1].typeTag;
		if ((CFR_STACKMAP_TYPE_DOUBLE == wideTypeTag) || (CFR_STACKMAP_TYPE_LONG == wideTypeTag)) {
			mapDataTypeToUTF8String(dataType, stackMapFrame, methodInfo, index - 1);
			wideType2ndSlot = TRUE;
			goto exit;
		}
	}
exit:
	return wideType2ndSlot;
}

/**
 * Obtain the non-class data type name string with the type name index to dataTypeNames[].
 * @param dataType - data type to be printed out
 * @param typeNameIndex - type name index to dataTypeNames[]
 * @param bcvType - data type value
 */
static void
getNonClassTypeName(J9UTF8Ref* dataType, U_8 typeNameIndex, UDATA bcvType)
{
	dataType->bytes = (U_8*)dataTypeNames[typeNameIndex];
	dataType->length = dataTypeLength[typeNameIndex];
	dataType->arity = (U_8)BCV_ARITY_FROM_TYPE(bcvType);
}

/**
 * Obtain the specified data type against the type value
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @param methodInfo - pointer to the MethodContextInfo that holds method related data and utilities
 * @param dataType - data type to be printed out
 * @param bcvType - data type value
 * @return the index to dataTypeNames array for printing
 */
static U_8
getBCVDataType(J9BytecodeVerificationData* verifyData, MethodContextInfo* methodInfo, J9UTF8Ref* dataType, UDATA bcvType)
{
	U_8 typeNameIndex = 0;
	UDATA typeTag = bcvType & BCV_TAG_MASK;

	switch (typeTag) {
	case BCV_OBJECT_OR_ARRAY:  /* FALLTHROUGH */
	{
		U_8  opCode = methodInfo->code[verifyData->errorPC];
		UDATA bytecodeTypeInfo = ((UDATA)J9JavaBytecodeVerificationTable[opCode]) & 0xF;
		UDATA convertedType = (UDATA)decodeTable[bytecodeTypeInfo];
		typeNameIndex = CFR_STACKMAP_TYPE_OBJECT;

		/* Set the expected type to 'reference' if the type data extracted from bytecode is 'reference' type.
		 * Note: according to the JVM Specification, the expected type for aastore is 'java/lang/Object' rather than 'reference'.
		 */
		if ((JBaastore != opCode)
		&& (0 != bytecodeTypeInfo)
		&& (BCV_GENERIC_OBJECT == convertedType)
		&& ((bcvType & (~BCV_ARITY_MASK)) == convertedType)
		) {
			getNonClassTypeName(dataType, typeNameIndex, bcvType);
		} else {
			/* Fetch the class name in classNameList if not 'reference' type */
			methodInfo->getStringfromClassNameList(dataType, methodInfo->classNameList, methodInfo->romClass, bcvType);
		}
	}
		break;
	case BCV_SPECIAL_INIT:
		typeNameIndex = CFR_STACKMAP_TYPE_INIT_OBJECT;
		getNonClassTypeName(dataType, typeNameIndex, bcvType);
		break;
	case BCV_SPECIAL_NEW:
		typeNameIndex = CFR_STACKMAP_TYPE_NEW_OBJECT;
		getNonClassTypeName(dataType, typeNameIndex, bcvType);
		break;
	default:
	{
		typeNameIndex = (U_8)bcvToBaseTypeNameIndex(bcvType);
		getNonClassTypeName(dataType, typeNameIndex, bcvType);

		/* If the 'top' type is the 2nd slot of long/double type, then set the data type string
		 * to long/double type to ensure that this slot is part of long/double type
		 * so as to print out long_2nd/double_2nd string in the error message framework.
		 */
		if ((BCV_BASE_TYPE_TOP == bcvType) && (0 != (verifyData->errorTempData & BCV_WIDE_TYPE_MASK))) {
			U_8 wideTypeNameIndex = (U_8)bcvToBaseTypeNameIndex(verifyData->errorTempData);
			getNonClassTypeName(dataType, wideTypeNameIndex, bcvType);
		}
	}
		break;
	}

	return typeNameIndex;
}

/**
 * Adjust the top of stack so as to print out the wrong data type on stack
 * when the verification error occurs (runtime verification).
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @return TRUE if required to print out the wrong data type on the current stack; otherwise, return FALSE.
 */
static BOOLEAN
adjustStackTopForWrongDataType(J9BytecodeVerificationData *verifyData)
{
	BOOLEAN result = FALSE;

	switch (verifyData->errorDetailCode) {
	case BCV_ERR_FRAMES_INCOMPATIBLE_TYPE:			/* FALLTHROUGH */
	case BCV_ERR_INCOMPATIBLE_TYPE:					/* FALLTHROUGH */
	case BCV_ERR_STACK_SIZE_MISMATCH:				/* FALLTHROUGH */
	case BCV_ERR_STACK_UNDERFLOW:					/* FALLTHROUGH */
	case BCV_ERR_STACK_OVERFLOW:					/* FALLTHROUGH */
	case BCV_ERR_STACKMAP_FRAME_LOCALS_UNDERFLOW:	/* FALLTHROUGH */
	case BCV_ERR_STACKMAP_FRAME_LOCALS_OVERFLOW:	/* FALLTHROUGH */
	case BCV_ERR_STACKMAP_FRAME_STACK_OVERFLOW:		/* FALLTHROUGH */
	case BCV_ERR_INIT_NOT_CALL_INIT:				/* FALLTHROUGH */
	case BCV_ERR_WRONG_TOP_TYPE:					/* FALLTHROUGH */
	case BCV_ERR_INVALID_ARRAY_REFERENCE:			/* FALLTHROUGH */
	case BCV_ERR_BAD_INIT_OBJECT:
		result = TRUE;
		break;
	}

	return result;
}

/**
 * Print the Reason section to buffer in the case of incompatible data type (runtime verification).
 * @param msgBuf - pointer to MessageBuffer
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @param methodInfo - pointer to the MethodContextInfo that holds method-related data and utilities
 * @param currentFrame - pointer to the current frame on stack (liveStack)
 * @param targetFrame - pointer to the stackmap frame
 *
 * @return TRUE if required to print out the stackmap frame; otherwise, return FALSE.
 */
static BOOLEAN
printReasonForIncompatibleType(MessageBuffer *msgBuf, J9BytecodeVerificationData *verifyData, MethodContextInfo *methodInfo, StackMapFrame *currentFrame, StackMapFrame *targetFrame)
{
	U_8 tag = 0;
	J9UTF8Ref dataTypeCurrent;
	U_32 errorCurrentFramePosition = verifyData->errorCurrentFramePosition;
	U_32 localStackIndex = errorCurrentFramePosition;
	U_32 maxLocals = (U_32)methodInfo->maxLocals;
	BOOLEAN wideType2ndSlot = FALSE;
	BOOLEAN result = FALSE;
	const char* errorFramePositionName = NULL;
	UDATA errorFramePositionNameLength = 0;


	/* Determine whether the wrong data type is on 'locals' or 'stack' for the purpose of printing:
	 * The name string points to 'locals' if the error location remains less than the value of maxLocals,
	 * which means the error occurs on 'locals'; otherwise, it points to 'stack'.
	 */
	if (errorCurrentFramePosition < maxLocals) {
		errorFramePositionName = framePositionName[0];   /* 'locals' */
	} else {
		errorFramePositionName = framePositionName[1];   /* 'stack' */
	}
	errorFramePositionNameLength = strlen(errorFramePositionName);

	printMessage(msgBuf, "Type ");

	/* Prepare the wrong data type for the Reason section */
	tag = mapDataTypeToUTF8String(&dataTypeCurrent, currentFrame, methodInfo, (UDATA)errorCurrentFramePosition);

	/* Determine whether the wrong data type is the second slot of long/double */
	wideType2ndSlot = isWideType2nd(&dataTypeCurrent, currentFrame, methodInfo, (UDATA)errorCurrentFramePosition, tag);
	printTypeInfoToBuffer(msgBuf, tag, &dataTypeCurrent, wideType2ndSlot);

	/* Adjust the stack index if greater than/equal to the maximum number of locals */
	if (localStackIndex >= maxLocals) {
		localStackIndex -= maxLocals;
	}

	/* Print the wrong data type to the error message buffer */
	printMessage(msgBuf, " (current frame, %.*s[%d]) is not assignable to ", errorFramePositionNameLength, errorFramePositionName, localStackIndex);

	/* Case 1: if the frame index is specified in errorTargetFrameIndex, the expected data type will be fetched from this stackmap frame */
	if (verifyData->errorTargetFrameIndex >= 0) {
		result = printExpectedTypeFromStackMapFrame(msgBuf, verifyData, methodInfo, currentFrame, targetFrame, localStackIndex);

	/* Case 2: the expected data type is stored in errorTargetType */
	} else {
		printExpectedType(msgBuf, verifyData, methodInfo);
	}

	return result;
}

/**
 * Print the expected data type from the stackmap frame to the error message buffer (runtime verification).
 * @param msgBuf - pointer to MessageBuffer
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @param methodInfo - pointer to the MethodContextInfo that holds method-related data and utilities
 * @param currentFrame - pointer to the current frame on stack (liveStack)
 * @param targetFrame - pointer to the stackmap frame
 * @param localStackIndex - index on stack for the wrong data type
 */
static BOOLEAN
printExpectedTypeFromStackMapFrame(MessageBuffer *msgBuf, J9BytecodeVerificationData *verifyData, MethodContextInfo *methodInfo, StackMapFrame *currentFrame, StackMapFrame *targetFrame, U_32 localStackIndex)
{
	I_32 errorTargetFrameIndex = verifyData->errorTargetFrameIndex;
	U_32 errorCurrentFramePosition = verifyData->errorCurrentFramePosition;
	U_8 tag = 0;
	I_32 stackmapFrameIndex = -1;
	U_8* nextStackmapFrame = NULL;
	BOOLEAN wideType2ndSlot = FALSE;
	const char* errorFramePositionName = NULL;
	UDATA errorFramePositionNameLength = 0;
	J9UTF8Ref dataTypeTarget;
	BOOLEAN result = TRUE;

	/* Determine whether the wrong data type is on 'locals' or 'stacks' for the purpose of printing:
	 * The name string points to 'locals' if the error location remains less than the value of maxLocals,
	 * which means the error occurs on 'locals'; otherwise, it points to 'stack'.
	 */
	if (errorCurrentFramePosition < (U_32)methodInfo->maxLocals) {
		errorFramePositionName = framePositionName[0];    /* 'locals' */
	} else {
		errorFramePositionName = framePositionName[1];    /* 'stack' */
	}
	errorFramePositionNameLength = strlen(errorFramePositionName);

	result = prepareVerificationTypeBuffer(targetFrame, methodInfo);
	if (FALSE == result) {
		goto exit;
	}

	/* Walk through the stackmap table for the specified stackmap frame */
	while (stackmapFrameIndex != errorTargetFrameIndex) {
		stackmapFrameIndex += 1;
		nextStackmapFrame = decodeStackmapFrameData(targetFrame, nextStackmapFrame, stackmapFrameIndex, methodInfo, verifyData);
		/* Return FALSE if out-of-memory during allocating verification buffer for data types in the specified stackmap frame */
		if (NULL == nextStackmapFrame) {
			result = FALSE;
			goto exit;
		}
	}

	/* Decode the expected data type based on the info in the stackmap frame */
	tag = mapDataTypeToUTF8String(&dataTypeTarget, targetFrame, methodInfo, (UDATA)errorCurrentFramePosition);

	/* Determine whether the expected data type is the second slot of long/double */
	wideType2ndSlot = isWideType2nd(&dataTypeTarget, targetFrame, methodInfo, (UDATA)errorCurrentFramePosition, tag);
	printTypeInfoToBuffer(msgBuf, tag, &dataTypeTarget, wideType2ndSlot);

	/* Print the unexpected data type to the error message buffer */
	printMessage(msgBuf, " (stack map, %.*s[%u])", errorFramePositionNameLength, errorFramePositionName, localStackIndex);

exit:
	return result;
}

/**
 * Print the expected data type to the error message buffer (runtime verification).
 * @param msgBuf - pointer to MessageBuffer
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @param methodInfo - pointer to the MethodContextInfo that holds method-related data and utilities
 */
static void
printExpectedType(MessageBuffer *msgBuf, J9BytecodeVerificationData *verifyData, MethodContextInfo *methodInfo)
{
	U_8 tag = 0;
	J9UTF8Ref dataTypeTarget;
	U_8  opCode = methodInfo->code[verifyData->errorPC];

	/* Decode the expected data type based on the data in errorTargetType */
	tag = getBCVDataType(verifyData, methodInfo, &dataTypeTarget, verifyData->errorTargetType);

	/* Print the expected data type to the error message buffer */
	printTypeInfoToBuffer(msgBuf, tag, &dataTypeTarget, FALSE);

	/* Output format: add " type" to the error message buffer if the type data extracted from bytecode is 'reference'.
	 * Note: according to the JVM Specification, the expected type for aastore is 'java/lang/Object' rather than 'reference'.
	 */
	if ((JBaastore != opCode) && (BCV_GENERIC_OBJECT == verifyData->errorTargetType)) {
		UDATA bytecodeTypeInfo = ((UDATA)J9JavaBytecodeVerificationTable[opCode]) & 0xF;
		UDATA convertedType = (UDATA)decodeTable[bytecodeTypeInfo];

		if ((0 != bytecodeTypeInfo) && (BCV_GENERIC_OBJECT == convertedType)) {
			printMessage(msgBuf, " type");
		}
	}
}

/**
 * Print the wrong data type to the error message buffer (runtime verification).
 * @param msgBuf - pointer to MessageBuffer
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @param methodInfo - pointer to the MethodContextInfo that holds method-related data and utilities
 */
static void
printWrongDataType(MessageBuffer *msgBuf, J9BytecodeVerificationData *verifyData, MethodContextInfo *methodInfo)
{
	U_8 tag = 0;
	J9UTF8Ref dataTypeTarget;

	/* Decode the data type based on the data in errorTargetType */
	tag = getBCVDataType(verifyData, methodInfo, &dataTypeTarget, verifyData->errorTempData);

	/* Print the data type to the error message buffer */
	printTypeInfoToBuffer(msgBuf, tag, &dataTypeTarget, FALSE);
}

/**
 * Fetch the stackmap frame from the stackmap table with the frame index specified (runtime verification).
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @param methodInfo - pointer to the MethodContextInfo that holds method-related data and utilities
 * @param targetFrame - pointer to the stackmap frame to be populated
 *
 * @return TRUE if required to print out the stackmap frame; otherwise, return FALSE.
 */
static BOOLEAN
setStackMapFrameWithIndex(J9BytecodeVerificationData *verifyData, MethodContextInfo *methodInfo, StackMapFrame *targetFrame)
{
	BOOLEAN result = FALSE;
	I_32 errorTargetFrameIndex = verifyData->errorTargetFrameIndex;

	/* Set up all data in the specified stackmap frame if not yet populated */
	if (errorTargetFrameIndex >= 0) {
		I_32 stackmapFrameIndex = -1;
		U_8* nextStackmapFrame = NULL;

		if (FALSE == prepareVerificationTypeBuffer(targetFrame, methodInfo)) {
			goto exit;
		}

		/* Walk through the stackmap table for the specified stackmap frame */
		while (stackmapFrameIndex != errorTargetFrameIndex) {
			stackmapFrameIndex += 1;
			nextStackmapFrame = decodeStackmapFrameData(targetFrame, nextStackmapFrame, stackmapFrameIndex, methodInfo, verifyData);
			/* Return FALSE if out-of-memory during allocating verification buffer for data types in the specified stackmap frame */
			if (NULL == nextStackmapFrame) {
				goto exit;
			}
		}

		/* Print the stackmap frame to the error message buffer if found */
		if (stackmapFrameIndex == errorTargetFrameIndex) {
			result = TRUE;
		}
	}

exit:
	return result;
}

/**
 * Print the Reason section to buffer in the case of mismatched flags
 * between the current stack and the stackmap frame (runtime verification).
 * @param msgBuf - pointer to MessageBuffer
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @param methodInfo - pointer to the MethodContextInfo that holds method-related data and utilities
 * @param currentFrame - pointer to the current frame on stack (liveStack)
 * @param targetFrame - pointer to the stackmap frame
 *
 * @return TRUE if required to print out the stackmap frame; otherwise, return FALSE.
 */
static BOOLEAN
printReasonForFlagMismatch(MessageBuffer *msgBuf, J9BytecodeVerificationData *verifyData, MethodContextInfo *methodInfo, StackMapFrame *currentFrame, StackMapFrame *targetFrame)
{
	BOOLEAN result = FALSE;
	U_16 stackMapCount = methodInfo->stackMapCount;

	/* Check the stackmap frame for flag only if the stackmap table exists in the class file */
	if (stackMapCount > 0) {
		I_32 stackmapFrameIndex = 0;
		U_8* nextStackmapFrame = NULL;

		if (FALSE == prepareVerificationTypeBuffer(targetFrame, methodInfo)) {
			goto exit;
		}

		/* Walk through the stackmap table for the specified stackmap frame */
		while (stackmapFrameIndex < (I_32)stackMapCount) {
			nextStackmapFrame = decodeStackmapFrameData(targetFrame, nextStackmapFrame, stackmapFrameIndex, methodInfo, verifyData);
			stackmapFrameIndex += 1;
			/* Return FALSE if out-of-memory during allocating verification buffer for data types in the specified stackmap frame */
			if (NULL == nextStackmapFrame) {
				goto exit;
			}

			/* Compare the flag value if the bci value matches */
			if (currentFrame->bci == targetFrame->bci) {
				BOOLEAN matchFlag = compareStackMapFrameFlag(currentFrame, targetFrame);
				if (!matchFlag) {
					printMessage(msgBuf, "Current frame's flags are not assignable to stack map frame's.");
					result = TRUE;
					goto exit;
				}
			}
		}
	}

	/* Only one error message exist at Reason section */
	printMessage(msgBuf, "Error exists in the bytecode.");

exit:
	return result;
}

U_8*
generateJ9RtvExceptionDetails(J9BytecodeVerificationData* verifyData, U_8* initMsgBuffer, UDATA* msgBufferLength)
{
	PORT_ACCESS_FROM_JAVAVM(verifyData->javaVM);
	MessageBuffer msgBuf;
	MethodContextInfo methodInfo;
	StackMapFrame stackMapFrameCurrent;
	StackMapFrame stackMapFrameTarget;
	const char* bcName = NULL;
	U_8 returnType = 0;
	BOOLEAN printStackFrame = FALSE;
	BOOLEAN printCurrentStack = FALSE;
	UDATA errorPC = verifyData->errorPC;

	/* Initialize the error message buffer */
	initMessageBuffer(PORTLIB, &msgBuf, initMsgBuffer, *msgBufferLength);

	/* The fields of stackMapFrameCurrent will be set up in prepareVerificationTypeBuffer() for the 'Current Frame' section.
	 * The fields of stackMapFrameTarget gets initialized here as prepareVerificationTypeBuffer doesn't get invoked for stackMapFrameTarget
	 * in the majority of cases since there is no 'Stackmap Frame' section in their error message framework.
	 */
	stackMapFrameTarget.bci = (U_16)-1;
	stackMapFrameTarget.entries = NULL;
	stackMapFrameTarget.numberOfLocals = 0;
	stackMapFrameTarget.numberOfStack = 0;

	/* Set up the context information for method with all data available in J9BytecodeVerificationData */
	constructRtvMethodContextInfo(&methodInfo, verifyData);

	/* Exception Details: */
	printMessage(&msgBuf, "\nException Details:");

	/* Location: */
	printMessage(&msgBuf, "\n%*sLocation:\n%*s", INDENT(2), INDENT(4));

	/* Determine the return type based on the method's signature.
	 * It will be used to identify/convert the 'return' opcode in convertToOracleOpcodeString().
	 */
	if ('[' == methodInfo.signature.bytes[methodInfo.signature.length - 2]) {
		returnType = ';';
	} else {
		returnType = methodInfo.signature.bytes[methodInfo.signature.length - 1];;
	}

	bcName = sunJavaBCNames[convertToOracleOpcodeString(methodInfo.code[errorPC], returnType)];

	printMessage(&msgBuf, "%.*s.%.*s%.*s @%u: %s",
				methodInfo.className.length, methodInfo.className.bytes,		/* class name */
				methodInfo.methodName.length, methodInfo.methodName.bytes,		/* method name */
				methodInfo.signature.length, methodInfo.signature.bytes,		/* method's signature */
				errorPC,														/* current pc value when error occurs */
				bcName);														/* Java bytecode name */

	/* Set up the data on the current stack (liveStack) with the context information for method */
	if (FALSE == prepareVerificationTypeBuffer(&stackMapFrameCurrent, &methodInfo)) {
		goto exit;
	}

	/* Adjust the top of stack only when it is required to print out the wrong data (error location) on stack */
	printCurrentStack = adjustStackTopForWrongDataType(verifyData);
	if (printCurrentStack) {
		/* Push the converted data type (locals/stacks) on the current stack to the verification type buffer */
		if (FALSE == pushLiveStackToVerificationTypeBuffer(&stackMapFrameCurrent, verifyData, &methodInfo)) {
			goto exit;
		}
	}

	/* Reason: */
	printMessage(&msgBuf, "\n%*sReason:\n%*s", INDENT(2), INDENT(4));

	/* Print the Reason section to the error message buffer against the error code */
	switch (verifyData->errorDetailCode) {
	case BCV_ERR_FRAMES_INCOMPATIBLE_TYPE:   /* FALLTHROUGH */
	case BCV_ERR_INCOMPATIBLE_TYPE:
		printStackFrame = printReasonForIncompatibleType(&msgBuf, verifyData, &methodInfo, &stackMapFrameCurrent, &stackMapFrameTarget);
		break;
	case BCV_ERR_STACK_SIZE_MISMATCH:
		printMessage(&msgBuf, "Current frame's stack size doesn't match stackmap.");
		printStackFrame = setStackMapFrameWithIndex(verifyData, &methodInfo, &stackMapFrameTarget);
		break;
	case BCV_ERR_INIT_NOT_CALL_INIT:
		printStackFrame = printReasonForFlagMismatch(&msgBuf, verifyData, &methodInfo, &stackMapFrameCurrent, &stackMapFrameTarget);
		printCurrentStack = printStackFrame;
		break;
	case BCV_ERR_STACKMAP_FRAME_LOCALS_UNDERFLOW:
		printMessage(&msgBuf, "The count of local variables in the stackmap frame is less than 0.");
		break;
	case BCV_ERR_STACKMAP_FRAME_LOCALS_OVERFLOW:
		printMessage(&msgBuf, "Exceeded max local size %u in the stackmap frame.", verifyData->errorTempData);
		break;
	case BCV_ERR_STACKMAP_FRAME_STACK_OVERFLOW:
		printMessage(&msgBuf, "Exceeded max stack size %u in the stackmap frame.", verifyData->errorTempData);
		break;
	case BCV_ERR_STACK_UNDERFLOW:
		printMessage(&msgBuf, "Attempt to pop empty stack.");
		break;
	case BCV_ERR_STACK_OVERFLOW:
		printMessage(&msgBuf, "Exceeded max stack size.");
		break;
	case BCV_ERR_WRONG_RETURN_TYPE:
		printMessage(&msgBuf, "Expected return type 'V' in the function.");
		break;
	case BCV_ERR_WRONG_TOP_TYPE:
		printMessage(&msgBuf, "The pair of data types on the top of 'stack' must be long, double or two non-top singles.");
		break;
	case BCV_ERR_INVALID_ARRAY_REFERENCE:
	{
		J9BranchTargetStack* liveStack = (J9BranchTargetStack *) verifyData->liveStack;
		IDATA indexOnStack = verifyData->errorCurrentFramePosition - liveStack->stackBaseIndex;
		printMessage(&msgBuf, "Type ");
		printWrongDataType(&msgBuf, verifyData, &methodInfo);
		printMessage(&msgBuf, " (current frame, stack[%d]) is not an array reference.", indexOnStack);
	}
		break;
	case BCV_ERR_EXPECT_STACKMAP_FRAME:
		printMessage(&msgBuf, "A stackmap frame is expected at branch target %u.", verifyData->errorTempData);
		break;
	case BCV_ERR_WRONG_STACKMAP_FRAME:
		printMessage(&msgBuf, "The stackmap frame (@%u) is not expected at this location.", verifyData->errorTempData);
		break;
	case BCV_ERR_NO_STACKMAP_FRAME:
		printMessage(&msgBuf, "No stackmap frame available at this location. BCI on next stack: @%u", verifyData->errorTempData);
		break;
	case BCV_ERR_DEAD_CODE:
		printMessage(&msgBuf, "Dead code exists in the bytecode.");
		break;
	case BCV_ERR_ARGUMENTS_MISMATCH:
		printMessage(&msgBuf,
					"The number of arguments (%u) in the method's signature mismatches the number (%u) specified in the class file.",
					verifyData->errorTempData,
					verifyData->romMethod->argCount);
		break;
	case BCV_ERR_BAD_INVOKESPECIAL:
		printMessage(&msgBuf, "The method invoked via invokespecial is invalid.");
		break;
	case BCV_ERR_BAD_ACCESS_PROTECTED:
		printMessage(&msgBuf, "Access to the protected member is not permitted.");
		break;
	case BCV_ERR_BAD_INIT_OBJECT:
		printMessage(&msgBuf, "The data type (decoded from stack) ");
		printWrongDataType(&msgBuf, verifyData, &methodInfo);
		printMessage(&msgBuf, " should not be a uninitialized object.");
		break;
	case BCV_ERR_ARRAY_ARITY_OVERFLOW:
		printMessage(&msgBuf, "The arity (0x%x) of array is overflow.", verifyData->errorTempData);
		break;
	case BCV_ERR_ARRAY_DIMENSION_MISMATCH:
		printMessage(&msgBuf, "The dimension of array (%u) is incorrect.", verifyData->errorTempData);
		break;
	case BCV_ERR_BAD_BYTECODE:
		printMessage(&msgBuf, "The bytecode is unrecognizable.");
		break;
	case BCV_ERR_UNEXPECTED_EOF:
		printMessage(&msgBuf, "Unexpected EOF is detected in the class file.");
		break;
	default:
		Assert_VRB_ShouldNeverHappen();
		break;
	}

	/* Current Frame: */
	if (printCurrentStack) {
		printMessage(&msgBuf, "\n%*sCurrent Frame:", INDENT(2));
		printTheStackMapFrame(&msgBuf, &stackMapFrameCurrent, &methodInfo);
	}

	/* Release memory allocated for the current frame (liveStack) */
	releaseVerificationTypeBuffer(&stackMapFrameCurrent, &methodInfo);

	/* Stackmap Frame: */
	if (printStackFrame) {
		/* Specify if the stack frame is from classfile or internal */
		if (verifyData->createdStackMap) {
			printMessage(&msgBuf, "\n%*sStackmap Frame (FallBack):", INDENT(2));
		} else {
			printMessage(&msgBuf, "\n%*sStackmap Frame:", INDENT(2));
		}
		printTheStackMapFrame(&msgBuf, &stackMapFrameTarget, &methodInfo);
	}

	/* Release memory allocated for the stackmap frame */
	releaseVerificationTypeBuffer(&stackMapFrameTarget, &methodInfo);

	/* Exception Handler Table: */
	if (methodInfo.exceptionTableLength > 0) {
		printMessage(&msgBuf, "\n%*sException Handler Table:", INDENT(2));
		printExceptionTable(&msgBuf, &methodInfo);
	}

	/* Stack Map Table:
	 * Only prints the stackmap table if it is non-empty and not internally generated
	 */
	if ((methodInfo.stackMapCount > 0) && (!verifyData->createdStackMap)) {
		printMessage(&msgBuf, "\n%*sStackmap Table:", INDENT(2));
		printSimpleStackMapTable(&msgBuf, &methodInfo);
	}

exit:
	*msgBufferLength = msgBuf.cursor;
	return msgBuf.buffer;
}
