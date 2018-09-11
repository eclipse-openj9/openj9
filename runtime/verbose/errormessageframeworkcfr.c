/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

static void constructCfrMethodContextInfo(J9PortLibrary* portLib, MethodContextInfo* methodInfo, J9CfrError* error, U_8* className, UDATA classNameLength);
static void getJ9CfrExceptionTableEntry(ExceptionTableEntry* buf, void* exceptionTable, UDATA index);
static void getJ9CfrUTF8StringfromCP(J9UTF8Ref* buf, void* constantPool, UDATA index);
static BOOLEAN printReasonForInvalidStackMapAttribute(MessageBuffer* msgBuf, J9CfrError* error, MethodContextInfo* methodInfo, J9CfrAttributeCode* code, StackMapFrame* stackMapFrame);
static void printJ9BcvErrorMessages(MessageBuffer* msgBuf, J9CfrError* error);
static void printJ9CfrErrorMessages(MessageBuffer* msgBuf, J9CfrError* error, MethodContextInfo* methodInfo, J9CfrAttributeCode* code);


/**
 * Retrieve startPC, endPC and handlerPC from J9CfrExceptionTableEntry (static verification).
 * @param buf - buffer to hold the return value
 * @param exceptionTable - opaque type being cast to J9CfrExceptionTableEntry
 * @param index - index to the exceptionTable entry
 */
static void
getJ9CfrExceptionTableEntry(ExceptionTableEntry* buf, void* exceptionTable, UDATA index)
{
	J9CfrExceptionTableEntry* table = (J9CfrExceptionTableEntry*)exceptionTable;
	buf->startPC = table[index].startPC;
	buf->endPC = table[index].endPC;
	buf->handlerPC = table[index].handlerPC;
}

/**
 * Fetch the UTF8 string via index from the constant pool.
 * @param buf - buffer to hold the requested UTF8 string.
 * @param constantPool - opaque type being cast to J9CfrConstantPoolInfo
 * @param index - index of UTF8 string in the constant pool
 */
static void
getJ9CfrUTF8StringfromCP(J9UTF8Ref* buf, void* constantPool, UDATA index)
{
	J9CfrConstantPoolInfo* info = (J9CfrConstantPoolInfo*)constantPool;
	UDATA utf8StringIndex = 0;
	Assert_VRB_notNull(buf);

	if (NULL != info) {
		/* 1) The UTF8 string can be retrieved directly from the constant pool when the index of UTF8 string
		 *    is specified (e.g. class name index, descriptor index).
		 * 2) In the case of CFR_CONSTANT_Class, slot1 stores the index of requested UTF8 string in the constant pool.
		 */
		switch(info[index].tag) {
		case CFR_CONSTANT_Class:
			utf8StringIndex = info[index].slot1;
			break;
		case CFR_CONSTANT_Utf8:
			utf8StringIndex = index;
			break;
		default:
			Assert_VRB_ShouldNeverHappen();
			break;
		}

		buf->bytes = info[utf8StringIndex].bytes;
		buf->length = info[utf8StringIndex].slot1;
	}
}

/**
 * Initialize the MethodContextInfo data from J9CfrError so as to simplify all operations related to J9CfrError.
 * @param portLib - the port library
 * @param methodInfo - pointer to the MethodContextInfo structure under construction
 * @param error - error data being used to build the MethodContextInfo object
 * @param className - the name of class
 * @param classNameLength - the length of the className
 */
static void
constructCfrMethodContextInfo(J9PortLibrary* portLib, MethodContextInfo* methodInfo, J9CfrError* error, U_8* className, UDATA classNameLength)
{
	J9CfrAttributeCode* codeAttribute = NULL;
	IDATA attrIndex = 0;

	/* error->errorMember is set up in buildMethodError and buildMethodErrorWithExceptionDetails (cfrerr.c)
	 * when these functions get invoked (staticverify.c).
	 * constructCfrMethodContextInfo() is called in generateJ9CfrExceptionDetails(), where error->errorMember->codeAttribute
	 * is confirmed to be a non-NULL value.
	 */
	Assert_VRB_notNull(error->errorMember);
	Assert_VRB_notNull(error->errorMember->codeAttribute);
	codeAttribute = error->errorMember->codeAttribute;

	methodInfo->portLib = portLib;
	methodInfo->maxLocals = codeAttribute->maxLocals;
	methodInfo->maxStack = codeAttribute->maxStack;
	methodInfo->code = codeAttribute->code;
	methodInfo->codeLength = codeAttribute->codeLength;
	methodInfo->exceptionTable = codeAttribute->exceptionTable;
	methodInfo->exceptionTableLength = codeAttribute->exceptionTableLength;
	methodInfo->className.bytes = className;
	methodInfo->className.length = classNameLength;
	methodInfo->modifier = error->errorMember->accessFlags;
	methodInfo->constantPool = error->constantPool;
	methodInfo->stackMapData = NULL;
	methodInfo->stackMapCount = 0;

	/* Refer to ClassFileOracle::walkMethodCodeAttributeAttributes (ClassFileOracle.cpp)
	 * to set up stackMap related data.
	 */
	for (attrIndex = 0; attrIndex < codeAttribute->attributesCount; attrIndex++) {
		J9CfrAttribute* attribute = codeAttribute->attributes[attrIndex];
		if (CFR_ATTRIBUTE_StackMapTable == attribute->tag){
			J9CfrAttributeStackMap * stackMap = (J9CfrAttributeStackMap *) attribute;
			methodInfo->stackMapData = stackMap->entries;
			methodInfo->stackMapCount = stackMap->numberOfEntries;
			methodInfo->stackMapLength = stackMap->mapLength;
			break;
		}
	}

	/* Register callback functions for retrieving name string in the constant pool and data in the exception handler table */
	methodInfo->getUTF8StringfromCP = getJ9CfrUTF8StringfromCP;
	methodInfo->getExceptionRange = getJ9CfrExceptionTableEntry;

	/* Obtain the method name/signature string from the constant pool */
	getJ9CfrUTF8StringfromCP(&methodInfo->methodName, error->constantPool, error->errorMember->nameIndex);
	getJ9CfrUTF8StringfromCP(&methodInfo->signature, error->constantPool, error->errorMember->descriptorIndex);

	/* Unused fields in static verification */
	methodInfo->romClass = NULL;
	methodInfo->classNameList = NULL;
	methodInfo->getStringfromClassNameList = NULL;
}

/**
 * Print the Reason section to the error message buffer
 * in the case of invalid stackmap attribute (static verification)
 * @param msgBuf - pointer to MessageBuffer
 * @param error - pointer to J9CfrError
 * @param methodInfo - pointer to the MethodContextInfo that holds method-related data and utilities
 * @param code - pointer to J9CfrAttributeCode
 * @param stackMapFrame - pointer to the stackmap frame
 * @return TRUE if required to print the data for 'Current frame' to the message buffer; otherwise return FALSE.
 */
static BOOLEAN
printReasonForInvalidStackMapAttribute(MessageBuffer* msgBuf, J9CfrError* error, MethodContextInfo* methodInfo, J9CfrAttributeCode* code, StackMapFrame* stackMapFrame)
{
	BOOLEAN result = FALSE;
	printMessage(msgBuf, "Invalid stackmap specification");

	/* errorFrameIndex is initialized with -1 in buildMethodErrorWithExceptionDetails (cfrerr.c) */
	if (-1 != error->errorFrameIndex) {
		I_32 frameIndex = -1;
		U_8* nextFrame = NULL;
		I_32 errorFrameIndex = error->errorFrameIndex;
		U_32 errorFrameBCI = error->errorFrameBCI;
		U_32 codeLength = code->codeLength;

		if (errorFrameBCI < codeLength) {
			/* Obtain opcode string from the Oracle bytecode instruction set (sunJavaBCNames[] defined in bcnames.h) with the opcode number.
			 * Note: errorFrameBCI (exceptionDetails->stackmapFrameBCI) is set to the offset in code (the opcode number is stored in there)
			 * when calling checkStackMap (staticverify.c).
			 */
			const char* bcName = sunJavaBCNames[code->code[errorFrameBCI]];
			printMessage(msgBuf, "(Stack Map Frame offset invalid. bci=%u, bytecode=%s)", errorFrameBCI, bcName);
		} else {
			printMessage(msgBuf, "(Stack Map Frame offset overflow. bci=%u, bytecode length=%u)", errorFrameBCI, codeLength);
		}

		/* Walk through the stackmap for the specified stackmap frame and set alll data to stackMapFrame for 'Current Frame'.
		 * Note: it only prints out the content existing in the error message uffer even if out-out-memory.
		 */
		if (FALSE == prepareVerificationTypeBuffer(stackMapFrame, methodInfo)) {
			goto exit;
		}

		while (frameIndex != errorFrameIndex) {
			nextFrame = decodeStackFrameDataFromStackMapTable(stackMapFrame, nextFrame, methodInfo);
			if (NULL == nextFrame) {
				goto exit;
			}
			frameIndex += 1;
		}

exit:
		result = TRUE;
	}

	return result;
}

/**
 * Print the J9NLS_BCV error messages to the error message buffer
 * @param msgBuf - pointer to MessageBuffer
 * @param error - pointer to J9CfrError
 */
static void
printJ9BcvErrorMessages(MessageBuffer* msgBuf, J9CfrError* error)
{
	Assert_VRB_notNull(msgBuf);
	Assert_VRB_notNull(error);

	switch (error->errorCode) {
	/* Determine the specific verification error in the case of inconsistent stack shape */
	case J9NLS_BCV_ERR_INCONSISTENT_STACK__ID:
	{
		switch(error->verboseErrorType){
		case BCV_ERR_JSR_STACK_UNDERFLOW:
			printMessage(msgBuf, "Attempt to pop empty stack in the jsr code block.");
			break;
		case BCV_ERR_JSR_STACK_OVERFLOW:
			printMessage(msgBuf, "Exceeded max stack size in the jsr code block.");
			break;
		case BCV_ERR_JSR_RET_ADDRESS_ON_STACK:
			printMessage(msgBuf, "Detected the jsr return address on the stack rather than the expected data type.");
			break;
		case BCV_ERR_JSR_ILLEGAL_LOAD_OPERATION:
			printMessage(msgBuf, "Loading return address from local variable index %u is illegal.", error->errorDataIndex);
			break;
		default:
			Assert_VRB_ShouldNeverHappen();
			break;
		}
	}
		break;
	default:
		printMessage(msgBuf, "Error exists in the bytecode.");
		break;
	}
}

/**
 * Print the J9NLS_CFR error messages to the error message buffer
 * @param msgBuf - pointer to MessageBuffer
 * @param error - pointer to J9CfrError
 * @param methodInfo - pointer to the MethodContextInfo that holds method-related data and utilities
 * @param code - pointer to J9CfrAttributeCode
 */
static void
printJ9CfrErrorMessages(MessageBuffer* msgBuf, J9CfrError* error, MethodContextInfo* methodInfo, J9CfrAttributeCode* code)
{
	Assert_VRB_notNull(msgBuf);
	Assert_VRB_notNull(error);

	switch (error->errorCode) {
		case J9NLS_CFR_ERR_INVALID_STACK_MAP_ATTRIBUTE__ID:
		{
			StackMapFrame stackMapFrame;
			BOOLEAN printStackFrame = printReasonForInvalidStackMapAttribute(msgBuf, error, methodInfo, code, &stackMapFrame);
			/* Current Frame: */
			if (printStackFrame) {
				printMessage(msgBuf, "\n%*sCurrent Frame:", INDENT(2));
				printTheStackMapFrame(msgBuf, &stackMapFrame, methodInfo);

				/* Release resources allocated for building the verification info in 'Current Frame' */
				releaseVerificationTypeBuffer(&stackMapFrame, methodInfo);
			}
		}
			break;
		case J9NLS_CFR_ERR_BC_SWITCH_OFFSET__ID:
			printMessage(msgBuf, "Target in switch bytecode exceeds the code length.");
			break;
		case J9NLS_CFR_ERR_BC_JUMP_OFFSET__ID:
			printMessage(msgBuf, "Target in jump bytecode exceeds the code length.");
			break;
		case J9NLS_CFR_ERR_BC_SWITCH_TARGET__ID:
			printMessage(msgBuf, "Target in switch bytecode doesn't exist.");
			break;
		case J9NLS_CFR_ERR_BC_JUMP_TARGET__ID:
			printMessage(msgBuf, "Target in jump bytecode doesn't exist.");
			break;
		case J9NLS_CFR_ERR_BC_LOAD_INDEX__ID:					/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_STORE_INDEX__ID:					/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_IINC_INDEX__ID:
			printMessage(msgBuf, "Local variable index %u is invalid.", error->errorDataIndex);
			break;
		case J9NLS_CFR_ERR_BAD_INDEX__ID:
			printMessage(msgBuf, "Constant pool index %u is invalid.", error->errorDataIndex);
			break;
		case J9NLS_CFR_ERR_BC_LDC_NOT_CONSTANT_OR_CONSTANT_DYNAMIC__ID:			/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_LDC2W_NOT_CONSTANT_OR_CONSTANT_DYNAMIC__ID:		/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_LDC_CONSTANT_DYNAMIC_RETURNS_LONG_OR_DOUBLE__ID:	/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_LDC2W_CONSTANT_DYNAMIC_NOT_LONG_OR_DOUBLE__ID:		/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_NEW_NOT_CLASS__ID:				/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_ANEWARRAY_NOT_CLASS__ID:			/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_CHECKCAST_NOT_CLASS__ID:			/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_INSTANCEOF_NOT_CLASS__ID:			/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_NOT_FIELDREF__ID:					/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_NOT_INTERFACEMETHODREF__ID:		/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_NOT_INVOKEDYNAMIC__ID:			/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_MULTI_NOT_CLASS__ID:				/* FALLTHROUGH */
		case J9NLS_CFR_ERR_BC_NOT_METHODREF__ID:
			printMessage(msgBuf, "Wrong type at the index %u of constant pool", error->errorDataIndex);
			break;
		default:
			printMessage(msgBuf, "Error exists in the bytecode.");
			break;
		}
}

U_8*
generateJ9CfrExceptionDetails(J9JavaVM *javaVM, J9CfrError *error, U_8* className, UDATA classNameLength, U_8* initMsgBuffer, UDATA* msgBufferLength)
{
	PORT_ACCESS_FROM_JAVAVM(javaVM);
	MessageBuffer msgBuf;
	J9CfrAttributeCode *code = NULL;
	U_32 errorPC = error->errorPC;

	/* error->errorMember is set up in buildMethodError and buildMethodErrorWithExceptionDetails (cfrerr.c)
	 * when these functions get invoked (staticverify.c).
	 * error->errorMember->codeAttribute can be NULL during static verification.
	 * In such case, there is nothing left to do after initializing the message buffer.
	 */
	Assert_VRB_notNull(error->errorMember);
	code = error->errorMember->codeAttribute;

	/* Prepare the error message buffer for the framework */
	initMessageBuffer(PORTLIB, &msgBuf, initMsgBuffer, *msgBufferLength);

	/* Do nothing in the case that error->errorMember->codeAttribute is NULL */
	if (NULL != code) {
		const char* bcName = sunJavaBCNames[code->code[errorPC]];
		MethodContextInfo methodInfo;

		/* Set up the context information for method with all data available in J9CfrError */
		constructCfrMethodContextInfo(PORTLIB, &methodInfo, error, className, classNameLength);

		/* Exception Details: */
		printMessage(&msgBuf, "\nException Details:");

		/* Location: */
		printMessage(&msgBuf, "\n%*sLocation:\n%*s%.*s.%.*s%.*s @%u: %s",
					INDENT(2), INDENT(4),
					classNameLength, className,										/* class name */
					methodInfo.methodName.length, methodInfo.methodName.bytes,		/* method name */
					methodInfo.signature.length, methodInfo.signature.bytes,		/* method's signature */
					errorPC,														/* current pc value when error occurs */
					bcName);														/* Java bytecode name */

		/* Reason: */
		printMessage(&msgBuf, "\n%*sReason:\n%*s", INDENT(2), INDENT(4));

		/* Use errorCatalog to differentiate the NLS error code (BCV and CFR) to avoid the numbering conflict between these error codes */
		switch(error->errorCatalog){
		case J9NLS_BCV_ERR_NO_ERROR__MODULE:
			printJ9BcvErrorMessages(&msgBuf, error);
			break;
		case J9NLS_CFR_ERR_NO_ERROR__MODULE:
			printJ9CfrErrorMessages(&msgBuf, error, &methodInfo, code);
			break;
		default:
			printMessage(&msgBuf, "CFR: Error exists in the bytecode.");
			break;
		}


		/* Exception Handler Table: */
		if (code->exceptionTableLength > 0) {
			printMessage(&msgBuf, "\n%*sException Handler Table:", INDENT(2));
			printExceptionTable(&msgBuf, &methodInfo);
		}

		/* Stack Map Table: */
		if (methodInfo.stackMapCount > 0) {
			printMessage(&msgBuf, "\n%*sStackmap Table:", INDENT(2));
			printSimpleStackMapTable(&msgBuf, &methodInfo);
		}
	}

	*msgBufferLength = msgBuf.cursor;
	return msgBuf.buffer;
}
