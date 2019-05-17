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
#include "bcverify.h"
#include "verutil_api.h"

#define MAX_ARRAY_PREFIX_LENGTH 256

#define TYPE_LONG		"long"
#define TYPE_DOUBLE		"double"

/* Set '[' for array type. It will be printed to the message buffer depending upon the arity value of data type.
 * Only used in printTypeInfoToBuffer().
 * Note: VM specification says that the number of dimensions in an array is limited to 255.
 */
static const U_8 arrayPrefix[MAX_ARRAY_PREFIX_LENGTH] = {
'[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[',
'[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[',
'[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[',
'[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[',
'[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[',
'[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[',
'[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[',
'[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[',
'[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '[', '\0'};

static IDATA obtainArgumentInfo(U_8* signatureBytes, UDATA signatureLength, UDATA* argumentStart, UDATA* argumentLength, U_8* argumentType);

/**
 * Obtain the information of arguments in the method's signature, which includes
 * including the starting address, length, slot count and data type of argument.
 * @param signatureBytes - pointer to the method signature
 * @param signatureLength - length of the signature string
 * @param argumentStart - starting point to the argument
 * @param argumentLength - length of the argument
 * @param argumentType - data type of the argument
 * @return the slot number of the argument; otherwise return 0 if no more argument to be parsed
 * Note: return value < 0 if any error is detected
 */
static IDATA
obtainArgumentInfo(U_8* signatureBytes, UDATA signatureLength, UDATA* argumentStart, UDATA* argumentLength, U_8* argumentType)
{
	UDATA argumentEnd = 0;

	if (*argumentStart > signatureLength) {
		return -1;
	}

	/* Skip over '(' to the arguments of method and start counting from out there */
	if ('(' == signatureBytes[*argumentStart]) {
		*argumentStart += 1;
	}

	argumentEnd = *argumentStart;

	/* Return 0 if no more parameters */
	if (')' == signatureBytes[argumentEnd]) {
		*argumentLength = 0;
		return 0;
	} else {
		/* Fetch the argument of signature (double/long type takes 2 slots, the others take 1 slot)
		 * to obtain the slot count of argument plus its data type.
		 */
		IDATA argumentSlotCount = fetchArgumentOfSignature(signatureBytes, signatureLength, &argumentEnd, argumentType);
		*argumentLength = argumentEnd - *argumentStart;
		return argumentSlotCount;
	}
}

U_8
convertToOracleOpcodeString(U_8 j9Opcode, U_8 returnType)
{
	U_8 oracleOpcode = 0;

	switch (j9Opcode) {
	case JBldc2dw: /* FALLTHROUGH */
	case JBldc2lw:
		oracleOpcode = CFR_BC_ldc2_w;
		break;

	case JBiloadw:	/* FALLTHROUGH */
	case JBlloadw:	/* FALLTHROUGH */
	case JBfloadw:	/* FALLTHROUGH */
	case JBdloadw:	/* FALLTHROUGH */
	case JBaloadw:	/* FALLTHROUGH */
	case JBistorew:	/* FALLTHROUGH */
	case JBlstorew:	/* FALLTHROUGH */
	case JBfstorew:	/* FALLTHROUGH */
	case JBdstorew:	/* FALLTHROUGH */
	case JBastorew:	/* FALLTHROUGH */
	case JBiincw:
		oracleOpcode = CFR_BC_wide;
		break;

	case JBnewdup:
		oracleOpcode = CFR_BC_new;
		break;

	case JBinvokehandle:	/* FALLTHROUGH */
	case JBinvokehandlegeneric:
		oracleOpcode = CFR_BC_invokevirtual;
		break;

	case JBaload0getfield:
		oracleOpcode = CFR_BC_aload_0;
		break;

	case JBinvokeinterface2:
		oracleOpcode = CFR_BC_invokeinterface;
		break;

	case JBgenericReturn:			/* FALLTHROUGH */
	case JBreturnFromConstructor:	/* FALLTHROUGH */
	case JBreturn0:					/* FALLTHROUGH */
	case JBreturn1:					/* FALLTHROUGH */
	case JBreturn2:					/* FALLTHROUGH */
	case JBreturnC:					/* FALLTHROUGH */
	case JBreturnS:					/* FALLTHROUGH */
	case JBreturnB:					/* FALLTHROUGH */
	case JBreturnZ:					/* FALLTHROUGH */
	case JBsyncReturn0:				/* FALLTHROUGH */
	case JBsyncReturn1:				/* FALLTHROUGH */
	case JBsyncReturn2:
		switch (returnType) {
		case 'V':
			oracleOpcode = CFR_BC_return;
			break;
		case 'J':	/* LONG */
			oracleOpcode = CFR_BC_lreturn;
			break;
		case 'D':	/* DOUBLE */
			oracleOpcode = CFR_BC_dreturn;
			break;
		case 'F':	/* FLOAT */
			oracleOpcode = CFR_BC_freturn;
			break;
		case ';':	/* OBJECT */
			oracleOpcode = CFR_BC_areturn;
			break;
		default:	/* OTHERS */
			oracleOpcode = CFR_BC_ireturn;
			break;
		}
		break;

	default:
		oracleOpcode = j9Opcode;
		break;
	}

	return oracleOpcode;
}

/**
 * Allocate memory to the verification type buffer for storing the information of data type.
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 * @param stackMapFrame - pointer to the specified stackmap frame data
 * @param currentVerificationTypeEntry - pointer to the VerificationTypeInfo data
 * @param slotCount - the required count of slots to be populated on 'locals'/stack
 * @return the current VerificationTypeInfo entry or NULL if out-of-memory
 */
static VerificationTypeInfo*
allocateMemoryToVerificationTypeBuffer(MethodContextInfo* methodInfo, StackMapFrame* stackMapFrame, VerificationTypeInfo* currentVerificationTypeEntry, UDATA slotCount)
{
	PORT_ACCESS_FROM_PORT(methodInfo->portLib);
	UDATA numberOfSlots = stackMapFrame->numberOfSlots;
	UDATA slotIndex = 0;

	Assert_VRB_notNull(currentVerificationTypeEntry);

	/* Increase the buffer size of entries if full or the existing buffer size is less than the required slot count */
	slotIndex = currentVerificationTypeEntry - stackMapFrame->entries;
	if ((numberOfSlots - slotIndex) <= slotCount) {
		VerificationTypeInfo* newEntries = NULL;
		numberOfSlots = slotIndex + slotCount + 1;

		newEntries = (VerificationTypeInfo *)j9mem_reallocate_memory(stackMapFrame->entries, (sizeof(VerificationTypeInfo) * numberOfSlots), OMRMEM_CATEGORY_VM);
		if (NULL == newEntries) {
			Trc_VRB_Reallocate_Memory_Failed(slotIndex, numberOfSlots);
			currentVerificationTypeEntry = NULL;
			goto exit;
		}
		stackMapFrame->entries = newEntries;
		currentVerificationTypeEntry = &stackMapFrame->entries[slotIndex];
		stackMapFrame->numberOfSlots = numberOfSlots;
	}

	exit:
		return currentVerificationTypeEntry;
}


VerificationTypeInfo*
pushTopTypeToVerificationTypeBuffer(MethodContextInfo* methodInfo, StackMapFrame* stackMapFrame, VerificationTypeInfo* currentVerificationTypeEntry, UDATA slotCount)
{
	Assert_VRB_notNull(currentVerificationTypeEntry);

	/* Increase the buffer size of entries if full or the existing buffer size is less than the required slot count */
	currentVerificationTypeEntry = allocateMemoryToVerificationTypeBuffer(methodInfo, stackMapFrame, currentVerificationTypeEntry, slotCount);
	if (NULL == currentVerificationTypeEntry) {
		goto exit;
	}

	/* Fill up the required number of 'top' type to the verification type buffer and move forward to the next entry in stackMapFrame->entries after that.
	 * Data of 'top' type: (typeTag, typeValueAttribute, typeValue) = (CFR_STACKMAP_TYPE_TOP (0x00), 0, 0)
	 */
	memset(currentVerificationTypeEntry, 0, sizeof(VerificationTypeInfo) * slotCount);
	currentVerificationTypeEntry += slotCount;

exit:
	return currentVerificationTypeEntry;
}

VerificationTypeInfo*
pushVerificationTypeInfo(MethodContextInfo* methodInfo, StackMapFrame* stackMapFrame, VerificationTypeInfo* currentVerificationTypeEntry, U_8 typeTag, U_8 typeValueAttribute, UDATA typeValue)
{
	Assert_VRB_notNull(currentVerificationTypeEntry);

	/* Increase the buffer size of entries if full */
	currentVerificationTypeEntry = allocateMemoryToVerificationTypeBuffer(methodInfo, stackMapFrame, currentVerificationTypeEntry, 1);
	if (NULL == currentVerificationTypeEntry) {
		goto exit;
	}

	/* Set up the information of data type in the entry and then move forward to the next entry in stackMapFrame->entries. */
	currentVerificationTypeEntry->typeTag = typeTag;
	currentVerificationTypeEntry->typeValueAttribute = typeValueAttribute;
	currentVerificationTypeEntry->typeValue = (U_32)typeValue;
	currentVerificationTypeEntry += 1;

	/* Push the 'top' type when the data type is long or double */
	if ((CFR_STACKMAP_TYPE_LONG == typeTag) || (CFR_STACKMAP_TYPE_DOUBLE == typeTag)) {
		currentVerificationTypeEntry = pushVerificationTypeInfo(methodInfo, stackMapFrame, currentVerificationTypeEntry, CFR_STACKMAP_TYPE_TOP, 0, 0);
	}

exit:
	return currentVerificationTypeEntry;
}

VerificationTypeInfo*
popVerificationTypeInfo(VerificationTypeInfo* currentVerificationTypeEntry)
{
	Assert_VRB_notNull(currentVerificationTypeEntry);

	currentVerificationTypeEntry -= 1;
	/* Pop up 2 slots for long/double data type, including 'top' */
	if (CFR_STACKMAP_TYPE_TOP == currentVerificationTypeEntry->typeTag) {
		VerificationTypeInfo* currentVerificationTypeEntry1 = currentVerificationTypeEntry - 1;
		U_8 vrfyType = currentVerificationTypeEntry1->typeTag;

		if ((CFR_STACKMAP_TYPE_LONG == vrfyType) || (CFR_STACKMAP_TYPE_DOUBLE == vrfyType)) {
			currentVerificationTypeEntry -= 1;
		}
	}
	return currentVerificationTypeEntry;
}

U_8*
adjustLocalsAndStack(MethodContextInfo* methodInfo, StackMapFrame* stackMapFrame, U_16* topIndex, U_8* entries, IDATA numEntries)
{
	PORT_ACCESS_FROM_PORT(methodInfo->portLib);
	VerificationTypeInfo* currentVerificationTypeEntry = &stackMapFrame->entries[*topIndex];
	IDATA index;

	if (numEntries < 0) { /* POP */
		/* The last k local variables are absent and that the operand stack is empty for CHOP frame (numEntries = -k).
		 * The value of k is given by the formula 251 - frame_type.
		 * So, pop up the all information for these k local variables from the verification type buffer.
		 */
		for (index = numEntries; index < 0; index++) {
			currentVerificationTypeEntry = popVerificationTypeInfo(currentVerificationTypeEntry);
		}
	} else {
		/* Push all information for entries on locals/stacks to the verification type buffer (numEntries > 0) */
		for (index = 0; index < numEntries; index++) {
			U_16 typeValue = 0;
			U_8 tag = *entries;

			entries += 1;
			if (tag <= CFR_STACKMAP_TYPE_INIT_OBJECT) {
				/* Ignore typeValue for base type and init object */
				currentVerificationTypeEntry = pushVerificationTypeInfo(methodInfo, stackMapFrame, currentVerificationTypeEntry, tag, 0, 0);
			} else {
				/* Set up typeValue for new object, object reference and base array type */
				NEXT_U16(typeValue, entries);
				currentVerificationTypeEntry = pushVerificationTypeInfo(methodInfo, stackMapFrame, currentVerificationTypeEntry, tag, INDEX_CONSTANTPOOL, typeValue);
			}

			if (NULL == currentVerificationTypeEntry) {
				entries = NULL;
				goto exit;
			}
		}
	}

	/* Adjust the number of data type on 'locals' or 'stack' in stackMapFrame->entries */
	*topIndex = (U_16)(currentVerificationTypeEntry - stackMapFrame->entries);
exit:
	return entries;
}

BOOLEAN
prepareVerificationTypeBuffer(StackMapFrame* stackMapFrame, MethodContextInfo* methodInfo)
{
	PORT_ACCESS_FROM_PORT(methodInfo->portLib);
	VerificationTypeInfo* verificationTypeEntries = NULL;
	VerificationTypeInfo* currentVerificationTypeEntry = NULL;
	IDATA localCount = 0;
	IDATA argumentSlotCount = 0;
	UDATA typeValue = 0;
	UDATA typeLength = 0;
	UDATA maxSlot = 0;
	U_8 vrfyType = (U_8)-1;
	BOOLEAN result = TRUE;

	stackMapFrame->entries = NULL;

	/* Allocate a verification type buffer (maximum size) for data type from locals/stacks on liveStack or stackmap frame.
	 * Each entry of stackMapFrame structure stores the information of one data type coming from locals/stacks
	 */
	maxSlot = methodInfo->maxLocals + methodInfo->maxStack;
	verificationTypeEntries = (VerificationTypeInfo *)j9mem_allocate_memory((sizeof(VerificationTypeInfo) * maxSlot), J9MEM_CATEGORY_CLASSES);
	if (NULL == verificationTypeEntries) {
		Trc_VRB_Allocate_Memory_Failed(sizeof(VerificationTypeInfo) * maxSlot);
		result = FALSE;
		goto exit;
	}

	stackMapFrame->numberOfSlots = maxSlot;
	stackMapFrame->entries = verificationTypeEntries;
	currentVerificationTypeEntry = verificationTypeEntries;

	/* Check the method itself and push all related data of method to the verification type buffer */
	if (!J9_ARE_ALL_BITS_SET(methodInfo->modifier, CFR_ACC_STATIC)) {
		J9CfrConstantPoolInfo cpInfo;
		cpInfo.bytes = (U_8*)methodInfo->signature.bytes;
		cpInfo.slot1 = (U_32)methodInfo->signature.length;

		/* Calls isInitOrClinitImpl() to determine whether the method is either "<init>" or "<clinit>".
		 * It returns 1) 0 if name is a normal name 2) 1 if '<init>' or 3) 2 if '<clinit>'
		 */
		if (CFR_METHOD_NAME_INIT == bcvIsInitOrClinit(&cpInfo)) {
			vrfyType = CFR_STACKMAP_TYPE_INIT_OBJECT;  /* "this" of an <init> method (cfreader.h) */
		} else {
			vrfyType = CFR_STACKMAP_TYPE_OBJECT;
		}
		currentVerificationTypeEntry = pushVerificationTypeInfo(methodInfo, stackMapFrame, currentVerificationTypeEntry, vrfyType, INDEX_CLASSNAME, typeValue);
		if (NULL == currentVerificationTypeEntry) {
			result = FALSE;
			goto exit;
		}
		localCount += 1;
	}

	/* Check each argument of method and push all related data to the verification type buffer */
	for (;;) {
		/* Obtain all related data for each argument, which includes the slot count of argument,
		 * the location of argument in the method's signature, the length of argument, data type of argument.
		 */
		argumentSlotCount = obtainArgumentInfo((U_8*)methodInfo->signature.bytes, methodInfo->signature.length, &typeValue, &typeLength, &vrfyType);
		Assert_VRB_false(argumentSlotCount < 0);

		/* No more argument detected */
		if (0 == argumentSlotCount) {
			break;
		}

		/* Push the information of argument to the verification type buffer */
		currentVerificationTypeEntry = pushVerificationTypeInfo(methodInfo,
												stackMapFrame,
												currentVerificationTypeEntry,
												vrfyType,
												INDEX_SIGNATURE,
												typeValue);

		if (NULL == currentVerificationTypeEntry) {
			result = FALSE;
			goto exit;
		}

		/* Count the slots occupied by arguments on 'locals' and move to the next argument */
		localCount += argumentSlotCount;
		typeValue += typeLength;
	}

	stackMapFrame->frameType = CFR_STACKMAP_FULL;
	stackMapFrame->bci = (U_16)-1;
	stackMapFrame->numberOfLocals = (U_16)localCount;
	stackMapFrame->numberOfStack = 0;

exit:
	return result;
}

U_8*
decodeStackmapFrameData(StackMapFrame* stackMapFrame, U_8* nextStackmapFrame, I_32 stackmapFrameIndex, MethodContextInfo* methodInfo, J9BytecodeVerificationData* verifyData)
{
	if (verifyData->createdStackMap) {
		/* Decode the specified 'Stackmap frame' data from verifyData->stackMaps (the decompressed stackmap table)
		 * as the stackmap table doesn't exist in the class file or currently in fallback verification
		 * so the frame is pointing to internal created stackMap
		 */
		nextStackmapFrame = decodeConstuctedStackMapFrameData(stackMapFrame, nextStackmapFrame, stackmapFrameIndex, methodInfo, verifyData);
	} else {
		/* Decode the specified 'Stackmap Frame' data from the compressed stackmap table in the class file */
		nextStackmapFrame = decodeStackFrameDataFromStackMapTable(stackMapFrame, nextStackmapFrame, methodInfo);
	}

	return nextStackmapFrame;
}

/*
 * This function refers to checkStackMap (staticverify.c)/ClassFileOracle::walkMethodCodeAttributeAttributes (ClassFileOracle.cpp)
 * against VM specification to decode all data of the specified stackmap frame to stackMapFrame for later printing out to the error message buffer.
 */
U_8*
decodeStackFrameDataFromStackMapTable(StackMapFrame* stackMapFrame, U_8* nextStackmapFrame, MethodContextInfo* methodInfo)
{
	U_8 frameType = 0;
	UDATA offset_delta = 0;
	IDATA numEntries = 0;

	/* Set up frame for the first time */
	if (NULL == nextStackmapFrame) {
		nextStackmapFrame = methodInfo->stackMapData;
	}

	frameType = *nextStackmapFrame;
	nextStackmapFrame += 1;

	/* Set up the bytecode offset for the stackmap frame.
	 * Note: stackMapFrame->bci is initialized to -1 at prepareVerificationTypeBuffer().
	 */
	stackMapFrame->bci += 1;

	if (frameType < CFR_STACKMAP_SAME_LOCALS_1_STACK) {
		/* Section 4.7.4 The StackMapTable Attribute in Java 8 VM specification:
		 * The frame type same_frame is represented by tags in the range [0-63].
		 * The offset_delta value for same_frame is the value of the tag item, frame_type.
		 */
		stackMapFrame->bci += (U_16)frameType;
	} else if (frameType < CFR_STACKMAP_SAME_LOCALS_1_STACK_END) {
		/* Section 4.7.4 The StackMapTable Attribute in Java 8 VM specification:
		 * The frame type same_locals_1_stack_item_frame is represented by tags in the range [64, 127].
		 * The offset_delta value for same_locals_1_stack_item_frame is given by the formula frame_type - 64.
		 */
		stackMapFrame->bci += (U_16)(frameType - CFR_STACKMAP_SAME_LOCALS_1_STACK);
	} else if (frameType >= CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED) {
		/* Section 4.7.4 The StackMapTable Attribute in Java 8 VM specification:
		 * For same_locals_1_stack_item_frame_extended, chop_frame, append_frame, and full_frame,
		 * the offset_delta value is given explicitly in the stackmap entry.
		 */
		stackMapFrame->bci += (U_16)NEXT_U16(offset_delta, nextStackmapFrame);
	}

	/* Set up the verification type information for each entry on locals/stacks of the stackmap frame against the frame type */
	stackMapFrame->numberOfStack = 0;
	if ((frameType >= CFR_STACKMAP_SAME_LOCALS_1_STACK) && (frameType <= CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED)) {
		/* VM specification says that the operand stack has one entry for SAME_LOCALS_1_STACK_ITEM
		 * or SAME_LOCALS_1_STACK_ITEM_EXTENDED. So, push all information of this entry to the verification type buffer.
		 */
		ADJUST_STACK(stackMapFrame, methodInfo->maxLocals, nextStackmapFrame, 1);
		if (NULL == nextStackmapFrame) {
			goto exit;
		}
	} else if ((frameType >= CFR_STACKMAP_CHOP_3) && (frameType < CFR_STACKMAP_FULL)) {
		/* VM specification says that
		 * 1) the last k local variables are absent and that the operand stack is empty for CHOP frame (numEntries < 0).
		 *    The value of k is given by the formula 251 - frame_type.
		 *    So, pop up the all information for these k local variables from the verification type buffer.
		 * 2) the k additional locals are defined and that the operand stack is empty for APPEND frame (numEntries > 0).
		 *    The value of k is given by the formula frame_type - 251.
		 *    So, push the all information for these k local variables into the verification type buffer.
		 */
		numEntries = (IDATA)frameType - CFR_STACKMAP_APPEND_BASE;
		ADJUST_LOCALS(stackMapFrame, nextStackmapFrame, numEntries);
		if (NULL == nextStackmapFrame) {
			goto exit;
		}
	} else if (CFR_STACKMAP_FULL == frameType) {
		/* VM specification shows that the full_frame structure includes
		 * frame_type, offset_delta, verification type of data on 'locals' and 'stack'.
		 */
		stackMapFrame->numberOfLocals = 0;

		/* Add items on 'locals' to the verification type buffer */
		NEXT_U16(numEntries, nextStackmapFrame);
		ADJUST_LOCALS(stackMapFrame, nextStackmapFrame, numEntries);
		if (NULL == nextStackmapFrame) {
			goto exit;
		}

		/* Add items on 'stack' to the verification type buffer */
		NEXT_U16(numEntries, nextStackmapFrame);
		ADJUST_STACK(stackMapFrame, methodInfo->maxLocals, nextStackmapFrame, numEntries);
		if (NULL == nextStackmapFrame) {
			goto exit;
		}
	}

exit:
	return nextStackmapFrame;
}


U_8*
decodeConstuctedStackMapFrameData(StackMapFrame* stackMapFrame, U_8* nextStackmapFrame, I_32 stackmapFrameIndex, MethodContextInfo* methodInfo, J9BytecodeVerificationData* verifyData)
{
	J9BranchTargetStack * targetStackmapFrame = BCV_INDEX_STACK((UDATA)stackmapFrameIndex);
	IDATA stackBaseIndex = targetStackmapFrame->stackBaseIndex;
	IDATA stackTopIndex = targetStackmapFrame->stackTopIndex;
	VerificationTypeInfo* currentVerificationTypeEntry = stackMapFrame->entries;
	U_16 maxStack = methodInfo->maxStack;
	U_16 maxLocals = methodInfo->maxLocals;
	IDATA lastIndex = stackBaseIndex - 1;
	IDATA slot = 0;
	IDATA dataTypeCode = DATATYPE_1_SLOT;
	BOOLEAN nonTopFound = FALSE;

	nextStackmapFrame = (U_8*)BCV_NEXT_STACK(targetStackmapFrame);

	stackMapFrame->bci = (U_16)targetStackmapFrame->pc;

	/* 'locals' on 'Stackmap Frame' */
	stackMapFrame->numberOfLocals = (U_16)(lastIndex + 1);

	/* Convert all data type on 'locals' (below stackBaseIndex) so as to push them to the verification type buffer for later printing */
	while (slot <= lastIndex) {
		dataTypeCode = convertBcvToCfrType(methodInfo, stackMapFrame, &currentVerificationTypeEntry, targetStackmapFrame->stackElements[slot]);
		if (DATATYPE_OUT_OF_MEMORY == dataTypeCode) {
			nextStackmapFrame = NULL;
			goto exit;
		}
		slot += dataTypeCode;
	}

	/* Fill up 'top' type for the rest of slots on 'locals' to the verification type buffer */
	currentVerificationTypeEntry = pushTopTypeToVerificationTypeBuffer(methodInfo, stackMapFrame, currentVerificationTypeEntry, maxLocals - stackMapFrame->numberOfLocals);
	if (NULL == currentVerificationTypeEntry) {
		nextStackmapFrame = NULL;
		goto exit;
	}

	/* 'stack' on 'Stackmap Frame' */

	/* Adjust the 'stack' size in the case of stack underflow */
	if (stackTopIndex >= stackBaseIndex) {
		lastIndex = stackTopIndex;
	} else {
		lastIndex = stackBaseIndex;
	}

	/* The 'stack' size should not exceed maxStack */
	if ((U_16)(lastIndex - stackBaseIndex) > maxStack) {
		lastIndex = maxStack + stackBaseIndex;
	}

	currentVerificationTypeEntry = &stackMapFrame->entries[maxLocals];
	stackMapFrame->numberOfStack = (U_16)(lastIndex - stackBaseIndex);

	/* Convert all data type on 'stack' (starting from stackBaseIndex) so as to push them to the verification type buffer for later printing */
	slot = stackBaseIndex;
	while (slot < lastIndex) {
		dataTypeCode = convertBcvToCfrType(methodInfo, stackMapFrame, &currentVerificationTypeEntry, targetStackmapFrame->stackElements[slot]);
		if (DATATYPE_OUT_OF_MEMORY == dataTypeCode) {
			nextStackmapFrame = NULL;
			goto exit;
		}
		slot += dataTypeCode;
	}

	/* Fill up 'top' type for the rest of slots on 'stack' to the verification type buffer */
	currentVerificationTypeEntry = pushTopTypeToVerificationTypeBuffer(methodInfo, stackMapFrame, currentVerificationTypeEntry, maxStack - stackMapFrame->numberOfStack);
	if (NULL == currentVerificationTypeEntry) {
		nextStackmapFrame = NULL;
	}

exit:
	return nextStackmapFrame;
}


IDATA
convertBcvToCfrType(MethodContextInfo* methodInfo, StackMapFrame* stackMapFrame, VerificationTypeInfo** currentVerificationTypeEntry, UDATA bcvType)
{
	UDATA typeTag = bcvType & BCV_TAG_MASK;
	UDATA dataTypeCode = DATATYPE_1_SLOT;
	UDATA arity = 0;
	U_8 typeNameIndex = 0;

	/* Convert BCV_XXX tag to CFR_STACKMAP_TYPE_XXX tag for later printing to the error message buffer */
	switch (typeTag) {
	case BCV_TAG_BASE_TYPE_OR_TOP:
		/* Determine whether the data type is long or double */
		dataTypeCode = ((0 != (bcvType & BCV_WIDE_TYPE_MASK)) ? DATATYPE_2_SLOTS : DATATYPE_1_SLOT);
		typeNameIndex = (U_8)bcvToBaseTypeNameIndex(bcvType);
		*currentVerificationTypeEntry = pushVerificationTypeInfo(methodInfo, stackMapFrame, *currentVerificationTypeEntry, typeNameIndex, 0, 0);
		break;
	case BCV_TAG_BASE_ARRAY_OR_NULL:
		arity = BCV_ARITY_FROM_TYPE(bcvType);
		typeNameIndex = (U_8)bcvToBaseTypeNameIndex(bcvType);
		*currentVerificationTypeEntry = pushVerificationTypeInfo(methodInfo, stackMapFrame, *currentVerificationTypeEntry, typeNameIndex, 0, arity);
		break;
	case BCV_SPECIAL_NEW:
		*currentVerificationTypeEntry = pushVerificationTypeInfo(methodInfo, stackMapFrame, *currentVerificationTypeEntry, CFR_STACKMAP_TYPE_NEW_OBJECT, 0, BCV_INDEX_FROM_TYPE(bcvType));
		break;
	case BCV_SPECIAL_INIT:
		*currentVerificationTypeEntry = pushVerificationTypeInfo(methodInfo, stackMapFrame, *currentVerificationTypeEntry, CFR_STACKMAP_TYPE_INIT_OBJECT, 0, 0);
		break;
	case BCV_OBJECT_OR_ARRAY:
	default:
		*currentVerificationTypeEntry = pushVerificationTypeInfo(methodInfo, stackMapFrame, *currentVerificationTypeEntry, CFR_STACKMAP_TYPE_OBJECT, INDEX_CLASSNAMELIST, bcvType);
		break;
	}

	/* Set to out-of-memory if unable to allocate memory for data type in the verification type buffer  */
	if (NULL == *currentVerificationTypeEntry) {
		dataTypeCode = DATATYPE_OUT_OF_MEMORY;
	}

	return dataTypeCode;
}


IDATA
bcvToBaseTypeNameIndex(UDATA bcvType)
{
	IDATA index = 0;
	BOOLEAN isArray = (BCV_TAG_BASE_ARRAY_OR_NULL == (bcvType & BCV_TAG_MASK));

	switch (bcvType & BCV_BASE_TYPE_MASK) {
	case BCV_BASE_TYPE_INT_BIT:
		index = isArray ? CFR_STACKMAP_TYPE_INT_ARRAY : CFR_STACKMAP_TYPE_INT;
		break;
	case BCV_BASE_TYPE_FLOAT_BIT:
		index = isArray ? CFR_STACKMAP_TYPE_FLOAT_ARRAY : CFR_STACKMAP_TYPE_FLOAT;
		break;
	case BCV_BASE_TYPE_LONG_BIT:
		index = isArray ? CFR_STACKMAP_TYPE_LONG_ARRAY : CFR_STACKMAP_TYPE_LONG;
		break;
	case BCV_BASE_TYPE_DOUBLE_BIT:
		index = isArray ? CFR_STACKMAP_TYPE_DOUBLE_ARRAY : CFR_STACKMAP_TYPE_DOUBLE;
		break;
	case BCV_BASE_TYPE_SHORT_BIT:
		index = isArray ? CFR_STACKMAP_TYPE_SHORT_ARRAY : CFR_STACKMAP_TYPE_INT;
		break;
	case BCV_BASE_TYPE_BYTE_BIT:
		index = isArray ? CFR_STACKMAP_TYPE_BYTE_ARRAY : CFR_STACKMAP_TYPE_INT;
		break;
	case BCV_BASE_TYPE_CHAR_BIT:
		index = isArray ? CFR_STACKMAP_TYPE_CHAR_ARRAY : CFR_STACKMAP_TYPE_INT;
		break;
	case BCV_BASE_TYPE_BOOL_BIT:
		index = isArray ? CFR_STACKMAP_TYPE_BOOL_ARRAY : CFR_STACKMAP_TYPE_INT;
		break;
	default:
		index = isArray ? CFR_STACKMAP_TYPE_NULL : CFR_STACKMAP_TYPE_TOP;
		break;
	}
	return index;
}

/*
 * This function refers to checkStackMap (staticverify.c)/ClassFileOracle::walkMethodCodeAttributeAttributes (ClassFileOracle.cpp)
 * against VM specification to walk through the stackmap table so as to print out data type of all stackmap frames to the error message buffer.
 */
void
printSimpleStackMapTable(MessageBuffer* buf, MethodContextInfo* methodInfo)
{
	U_8* entry = methodInfo->stackMapData;
	U_16 stackMapCount = methodInfo->stackMapCount;
	I_32 remainingItemSlots = (I_32)methodInfo->stackMapLength;
	U_16 entryCount = 0;
	U_32 bci = (U_32)-1;
	U_16 offset_delta = 0;

	while ((entryCount < stackMapCount) && (remainingItemSlots > 0)) {
		U_8 frameType = *entry;
		entry += 1;
		bci += 1;
		remainingItemSlots -= 1;

		printMessage(buf, "\n%*s", INDENT(4));

		if (frameType < CFR_STACKMAP_SAME_LOCALS_1_STACK) {
			/* Section 4.7.4 The StackMapTable Attribute in Java 8 VM specification:
			 * The frame type same_frame is represented by tags in the range [0-63].
			 * The offset_delta value for same_frame is the value of the tag item, frame_type.
			 */
			bci += frameType;
			printMessage(buf,"same_frame(@%u", bci);
		} else if (frameType < CFR_STACKMAP_SAME_LOCALS_1_STACK_END) {
			/* Section 4.7.4 The StackMapTable Attribute in Java 8 VM specification:
			 * The frame type same_locals_1_stack_item_frame is represented by tags in the range [64, 127].
			 * The offset_delta value for same_locals_1_stack_item_frame is given by the formula frame_type - 64.
			 */
			bci += frameType - CFR_STACKMAP_SAME_LOCALS_1_STACK;
			printMessage(buf,"same_locals_1_stack_item_frame(@%u", bci);

			if (remainingItemSlots > 0) {
				/* VM specification says that the operand stack has one entry for SAME_LOCALS_1_STACK_ITEM
				 * So, print all information of this entry to the message buffer.
				 */
				printMessage(buf,",");
				entry = printVerificationTypeInfo(buf, entry, 1, &remainingItemSlots);
			}
		} else if (frameType >= CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED) {
			/* Section 4.7.4 The StackMapTable Attribute in Java 8 VM specification:
			 * For same_locals_1_stack_item_frame_extended, chop_frame, append_frame, and full_frame,
			 * the offset_delta value is given explicitly in the stackmap entry.
			 */
			remainingItemSlots -= 2;
			/* Exit immediately if the data in the stackmap already got corrupted from here */
			if (remainingItemSlots < 0) {
				goto nextEntry;
			}
			bci += NEXT_U16(offset_delta, entry);

			if (CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED == frameType) {
				printMessage(buf,"same_locals_1_stack_item_frame_extended(@%u", bci);

				if (remainingItemSlots > 0) {
					/* VM specification says that the operand stack has one entry for SAME_LOCALS_1_STACK_ITEM_EXTENDED.
					 * So, print all information of this entry to the message buffer.
					 */
					printMessage(buf,",");
					entry = printVerificationTypeInfo(buf, entry, 1, &remainingItemSlots);
				}
			} else if ((frameType > CFR_STACKMAP_SAME_LOCALS_1_STACK_EXTENDED) && (frameType < CFR_STACKMAP_CHOP_BASE)) { /* CFR_STACKMAP_CHOP_1/3 */
				/* VM specification says that the last k local variables are absent and that the operand stack is empty for CHOP frame.
				 * The value of k is given by the formula 251 - frame_type.
				 */
				printMessage(buf,"chop_frame(@%u,%u", bci, (CFR_STACKMAP_CHOP_BASE - frameType));
			} else if (CFR_STACKMAP_SAME_EXTENDED == frameType) {
				printMessage(buf,"same_frame_extended(@%u", bci);
			} else if ((frameType > CFR_STACKMAP_APPEND_BASE) && (frameType < CFR_STACKMAP_FULL)) { /* CFR_STACKMAP_APPEND_1/3 */
				/* VM specification says that the k additional locals are defined and that the operand stack is empty for APPEND frame.
				 * The value of k is given by the formula frame_type - 251.
				 * So, print all information for these k local variables into the message buffer.
				 */
				I_32 itemCount = frameType - CFR_STACKMAP_APPEND_BASE;
				printMessage(buf,"append_frame(@%u", bci);
				/* Only print the remaining data in the stackmap to the error message buffer
				 * if the value of itemCount is invalid (greater than the count of remaining
				 * data in the stackmap).
				 */
				if (remainingItemSlots < itemCount) {
					itemCount = remainingItemSlots;
				}
				if (itemCount > 0) {
					printMessage(buf,",");
					entry = printVerificationTypeInfo(buf, entry, itemCount, &remainingItemSlots);
				}
			} else if (CFR_STACKMAP_FULL == frameType) {
				/* VM specification shows that the full_frame structure includes
				 * frame_type, offset_delta, verification type of data on locals and stacks.
				 */
				printMessage(buf,"full_frame(@%u", bci);

				/* Print data on 'locals' to the message buffer */
				entry = printFullStackFrameInfo(buf, entry, &remainingItemSlots);

				/* Print data on 'stack' to the message buffer */
				entry = printFullStackFrameInfo(buf, entry, &remainingItemSlots);
			} else {
				Assert_VRB_ShouldNeverHappen();
			}
		}

nextEntry:
		printMessage(buf, ")");
		entryCount++;
	}
}

void
releaseVerificationTypeBuffer(StackMapFrame* stackMapFrame, MethodContextInfo* methodInfo)
{
	if (NULL != stackMapFrame->entries) {
		PORT_ACCESS_FROM_PORT(methodInfo->portLib);
		j9mem_free_memory(stackMapFrame->entries);
	}
}

U_8
mapDataTypeToUTF8String(J9UTF8Ref* dataType, StackMapFrame* stackMapFrame, MethodContextInfo* methodInfo, UDATA index)
{
	U_8 tag = stackMapFrame->entries[index].typeTag;
	UDATA typeValue = stackMapFrame->entries[index].typeValue;

	dataType->arity = 0;

	switch(tag) {
	case CFR_STACKMAP_TYPE_TOP:				/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_INT:				/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_FLOAT:			/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_DOUBLE:			/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_LONG:			/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_NULL:			/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_INIT_OBJECT:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_NEW_OBJECT:
		/* Set the name string for base type, init/new object.
		 * Note: The numbering of type tags is set up in dataTypeNames[]
		 */
		dataType->bytes = (U_8*)dataTypeNames[tag];
		dataType->length = dataTypeLength[tag];
		break;
	case CFR_STACKMAP_TYPE_INT_ARRAY:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_FLOAT_ARRAY:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_DOUBLE_ARRAY:	/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_LONG_ARRAY:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_SHORT_ARRAY:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_BYTE_ARRAY:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_CHAR_ARRAY:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_BOOL_ARRAY:
		/* Set the name string for array type (only used for runtime verification) */
		dataType->bytes = (U_8*)dataTypeNames[tag];
		dataType->length = dataTypeLength[tag];
		dataType->arity = (U_8)typeValue;
		break;
	case CFR_STACKMAP_TYPE_OBJECT:
	{
		/* Set the name string for object reference.
		 * Identify what the type index value is according to the index attribute.
		 * The type index value could be index in constant pool, location of the method's signature,
		 * class name, index in classNameList, etc.
		 */
		switch (stackMapFrame->entries[index].typeValueAttribute) {
		case INDEX_CONSTANTPOOL:
			methodInfo->getUTF8StringfromCP(dataType, methodInfo->constantPool, typeValue);
			break;
		case INDEX_SIGNATURE:
		{
			UDATA typeLength = 0;
			IDATA argumentSlotCount = obtainArgumentInfo((U_8*)methodInfo->signature.bytes, methodInfo->signature.length, &typeValue, &typeLength, NULL);
			Assert_VRB_false(argumentSlotCount < 0);

			dataType->bytes = methodInfo->signature.bytes + typeValue;
			dataType->length = typeLength;
			/* Ignore 'L' and ';' to get the full string of argument in signature */
			if ('L' == *dataType->bytes) {
				dataType->bytes += 1;
				dataType->length -= 2;
			}
			break;
		}
		case INDEX_CLASSNAME:
			dataType->bytes = methodInfo->className.bytes;
			dataType->length = methodInfo->className.length;
			break;
		case INDEX_CLASSNAMELIST: /* Only used for runtime verification */
			methodInfo->getStringfromClassNameList(dataType, methodInfo->classNameList, methodInfo->romClass, typeValue);
			break;
		default:
			Assert_VRB_ShouldNeverHappen();
			break;
		}
	}
		break;
	default:
		Assert_VRB_ShouldNeverHappen();
		break;
	}

	return tag;
}

void
printStackMapFrameBCI(MessageBuffer* buf, StackMapFrame* stackMapFrame)
{
	printMessage(buf, "\n%*sbci: @%d", INDENT(4), stackMapFrame->bci);
}

void
printStackMapFrameFlag(MessageBuffer* buf, StackMapFrame* stackMapFrame)
{
	IDATA index = 0;
	BOOLEAN flagUnInitThis = FALSE;
	U_16 numberOfLocals = stackMapFrame->numberOfLocals;

	/* Check whether data type 'uninitializedThis' exists on 'locals' of this stackmap frame.
	 * VM specification says:
	 * Flags is a list which may either be empty or have the single element 'flagThisUninit'.
	 * If any local variable on 'locals' has the type 'uninitializedThis',
	 * then Flags has the single element flagThisUninit, otherwise Flags is an empty list.
	 */
	for (index = 0; index < numberOfLocals; index++) {
		if (CFR_STACKMAP_TYPE_INIT_OBJECT == stackMapFrame->entries[index].typeTag) {
			flagUnInitThis = TRUE;
			break;
		}
	}
	printMessage(buf, "\n%*sflags: {%s }", INDENT(4), (flagUnInitThis) ? " flagThisUninit" : "");
}

IDATA
printTypeInfoToBuffer(MessageBuffer* buf, U_8 tag, J9UTF8Ref* dataType, BOOLEAN print2nd)
{
	IDATA wide = 1;
	UDATA arity = 0;

	switch(tag) {
	case CFR_STACKMAP_TYPE_TOP:
		/* Only print the 2nd slot of long/double type to the error message buffer for the "Reason" section in the error message framework. */
		if ((0 == strncmp((char*)dataType->bytes, TYPE_LONG, sizeof(TYPE_LONG) - 1))
		|| (0 == strncmp((char*)dataType->bytes, TYPE_DOUBLE, sizeof(TYPE_DOUBLE) - 1))
		) {
			printMessage(buf, "%.*s_2nd", dataType->length, dataType->bytes);
		} else {
			/* Print 'top' to the error message buffer if not the 2nd slot of long/double type */
			printMessage(buf, "%.*s", dataType->length, dataType->bytes);
		}
		break;
	case CFR_STACKMAP_TYPE_LONG:			/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_DOUBLE:
	{
		/* Print long/double (e.g. "long" or "double") or long/double plus the 2nd slot (e.g. "long, long_2nd" or "double, double_2nd")
		 * to the error message buffer for other section in the error message framework if required.
		 */
		printMessage(buf, "%.*s", dataType->length, dataType->bytes);

		if (print2nd) {
			/* Print the 2nd slot of long/double type to the error message buffer */
			printMessage(buf, ", %.*s_2nd", dataType->length, dataType->bytes);
		}
		wide = 2;
	}
		break;
	case CFR_STACKMAP_TYPE_INT:				/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_FLOAT:			/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_NULL:
		printMessage(buf, "%.*s", dataType->length, dataType->bytes);
		break;
	case CFR_STACKMAP_TYPE_INT_ARRAY:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_FLOAT_ARRAY:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_DOUBLE_ARRAY:	/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_LONG_ARRAY:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_SHORT_ARRAY:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_BYTE_ARRAY:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_CHAR_ARRAY:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_BOOL_ARRAY:
		/* Base type array.
		 * Note: arity 1 is implicitly set to 0 for base type array during verification, which means
		 * 0 is used for arity 1, 1 for arity 2, 2 for arity 3 and so forth.
		 * So, we need to add 1 to arity explicitly here so as to print out '[' correctly to the message buffer.
		 */
		dataType->arity += 1;
		printMessage(buf, "'%.*s%.*s'", dataType->arity, arrayPrefix, dataType->length, dataType->bytes);
		break;
	case CFR_STACKMAP_TYPE_INIT_OBJECT:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_NEW_OBJECT:		/* FALLTHROUGH */
	case CFR_STACKMAP_TYPE_OBJECT:
		/* Object, init/new object and object 'reference' */
		printMessage(buf, "'%.*s%.*s%.*s%.*s'",
				dataType->arity, arrayPrefix,		/* array prefix '[' */
				(dataType->arity > 0) ? 1 : 0, "L",	/* class prefix 'L' only when arity > 0 */
				dataType->length, dataType->bytes,	/* class name		*/
				(dataType->arity > 0) ? 1 : 0, ";"	/* class suffix ';' only when arity > 0 */
				);
		break;
	default:
		Assert_VRB_ShouldNeverHappen();
		break;
	}

	return wide;
}

void
printStackMapFrameData(MessageBuffer* buf, StackMapFrame* stackMapFrame, MethodContextInfo* methodInfo, const char* title, UDATA titleLength, UDATA startFrom, UDATA numOfItems)
{
	IDATA index = startFrom;
	IDATA startTo = startFrom + numOfItems;
	BOOLEAN firstEntry = TRUE;

	printMessage(buf, "\n%*s%.*s: {", INDENT(4), titleLength, title);

	while (index < startTo) {
		J9UTF8Ref dataType;
		U_8 tag = (U_8)-1;

		/* Print ',' after the first entry */
		printMessage(buf, (firstEntry ? " " : ", "));

		tag = mapDataTypeToUTF8String(&dataType, stackMapFrame, methodInfo, index);
		index += printTypeInfoToBuffer(buf, tag, &dataType, TRUE);

		if (firstEntry) {
			firstEntry = FALSE;
		}
	}

	printMessage(buf, " }");
}

void
printTheStackMapFrame(MessageBuffer* buf, StackMapFrame* stackMapFrame, MethodContextInfo* methodInfo)
{
	printStackMapFrameBCI(buf, stackMapFrame);
	printStackMapFrameFlag(buf, stackMapFrame);
	PRINT_LOCALS(buf, stackMapFrame, methodInfo);
	PRINT_STACK(buf, stackMapFrame, methodInfo);
}

void
printExceptionTable(MessageBuffer* buf, MethodContextInfo* methodInfo)
{
	U_16 exceptionTableLength = methodInfo->exceptionTableLength;
	UDATA index;

	for (index = 0; index < exceptionTableLength; index++) {
		ExceptionTableEntry entry;
		methodInfo->getExceptionRange(&entry, methodInfo->exceptionTable, index);
		printMessage(buf, "\n%*sbci [%u, %u] => handler: %u", INDENT(4), entry.startPC, entry.endPC, entry.handlerPC);
	}
}

U_8*
printVerificationTypeInfo(MessageBuffer* buf, U_8* cursor, UDATA itemCount, I_32* remainingItemSlots)
{
	UDATA index = 0;
	U_8 itemType = 0;
	BOOLEAN firstItem = TRUE;

	while ((index < itemCount) && (*remainingItemSlots > 0)) {
		itemType = *cursor;
		cursor += 1;
		*remainingItemSlots -= 1;

		/* Print ',' after the first item */
		if (firstItem) {
			firstItem = FALSE;
		} else {
			printMessage(buf, ",");
		}

		/* Print name string for base type and init object */
		if (itemType < CFR_STACKMAP_TYPE_OBJECT) {
			const U_8* name = (U_8*)dataTypeNames[itemType];
			UDATA len = dataTypeLength[itemType];
			printMessage(buf, "%.*s", len, name);

		/* Print constant poll index for other types */
		} else {
			U_16 cpIndex = 0;

			/* Exit immediately if the data in stackmap already got corrupted from here */
			*remainingItemSlots -= 2;
			if (*remainingItemSlots < 0) {
				break;
			}
			NEXT_U16(cpIndex, cursor);
			printMessage(buf, "Object[#%u]", cpIndex);
		}

		index += 1;
	}
	return cursor;
}

U_8*
printFullStackFrameInfo(MessageBuffer* buf, U_8* cursor, I_32* remainingItemSlots)
{
	U_16 itemCount = 0;

	/* Exit immediately if
	 * 1) *remainingItemSlots < 0: the value of itemCount in the stackmap already got corrupted.
	 * 2) *remainingItemSlots == 0: there is no data left after itemCount (2 bytes) in the stackmap.
	 */
	*remainingItemSlots -= 2;
	if (*remainingItemSlots <= 0) {
		return cursor;
	}
	NEXT_U16(itemCount, cursor);

	printMessage(buf, ",{");
	/* Only print the remaining data in stackmap if the value of
	 * itemCount >= the count of remaining data in the stackmap.
	 * Note: By doing so, it avoids storing garbage data to the error
	 * message buffer because the value of itemCount is literally
	 * invalid and the data of stackmap gets corrupted from there.
	 */
	if (*remainingItemSlots < (I_32)itemCount) {
		itemCount = (U_16)*remainingItemSlots;
	}
	cursor = printVerificationTypeInfo(buf, cursor, itemCount, remainingItemSlots);
	printMessage(buf, "}");

	return cursor;
}
