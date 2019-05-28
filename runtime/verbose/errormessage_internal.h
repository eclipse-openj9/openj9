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

#ifndef errormessage_internal_h
#define errormessage_internal_h

/**
* @file errormessage_internal.h
* @brief Internal prototypes and type/constant definitions used in generating the error message framework.
*
* This file contains internal functions and type/constant definitions intended
* for the error message framework during static/runtime verification.
*
*/

#include "j9.h"
#include "j9comp.h"
#include "bcverify.h"
#include "j9protos.h"
#include "argcount.h"
#include "bcnames.h"
#include "cfreader.h"
#include "rommeth.h"
#include "ut_j9vrb.h"
#include "cfrerrnls.h"
#include "j9bcvnls.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MSGBUF_OK					0
#define MSGBUF_ERROR				-1
#define MSGBUF_OUT_OF_MEMORY		-2

#define DATATYPE_2_SLOTS			2
#define DATATYPE_1_SLOT				1
#define DATATYPE_OUT_OF_MEMORY		0

#define INDEX_CONSTANTPOOL			0x01
#define INDEX_SIGNATURE				0x02
#define INDEX_CLASSNAME				0x03
#define	INDEX_CLASSNAMELIST			0x04

#define INDENT(n) (n), " "

/* Name and length of data type string */
extern const char* dataTypeNames[];
extern const UDATA dataTypeLength[];

typedef struct MessageBuffer MessageBuffer;

/* Contain all data of the error message buffer */
struct MessageBuffer {
	UDATA size;
	UDATA cursor;
	BOOLEAN bufEmpty;
	U_8* _bufOnStack;	/* Only for internal use. It is normally initialized to point to a 1K byte array on caller's stack */
	U_8* buffer;
	J9PortLibrary* portLib;
};


/* Hold the information of UTF8 style data, including the data length, data in bytes, and arity */
typedef struct J9UTF8Ref {
	UDATA length;
	const U_8* bytes;
	U_8 arity;
} J9UTF8Ref;


/* Hold the common information from J9ExceptionHandler (j9nonbuilder.h) or J9CfrExceptionTableEntry (cfr.h) */
typedef struct ExceptionTableEntry {
    U_16 startPC;
    U_16 endPC;
    U_16 handlerPC;
} ExceptionTableEntry;


/**
 * Callback function that fetches UTF8 string from the constant pool.
 * @param buf - buffer to hold the return value
 * @param constantPool - opaque type being cast to either J9CfrConstantPoolInfo or J9ROMConstantPoolItem accordingly
 * @param cpIndex - constant pool index
 */
typedef void (*getUTF8StringfromCPFunc)(J9UTF8Ref* buf, void* constantPool, UDATA cpIndex);


/**
 * Callback function that fetches class name string from verifyData->classNameList
 * @param buf - buffer to hold the return value
 * @param classNameListPtr - pointer to classNameListPtr being cast to struct J9UTF8**
 * @param romClass - pointer to verifyData->romClass
 * @param classIndex - class index in verifyData->classNameList
 */
typedef void (*getStringfromClassNameListFunc)(J9UTF8Ref* buf, void* classNameListPtr, void* romClass, UDATA classIndex);


/**
 * Retrieve startPC, endPC and handlerPC from either J9CfrExceptionTableEntry or J9ExceptionHandler
 * @param buf - buffer to hold the return value
 * @param exceptionTable - opaque type being cast to either J9CfrExceptionTableEntry or J9ExceptionHandler
 * @param index - index to the exceptionTable entry
 */
typedef void (*getExceptionTableEntryFunc)(ExceptionTableEntry* buf, void* exceptionTable, UDATA index);


/* Hold all required information for the specified method from J9BytecodeVerificationData or J9CfrError */
typedef struct MethodContextInfo {
	J9UTF8Ref className;
	J9UTF8Ref methodName;
	J9UTF8Ref signature;
	U_16 maxLocals;
	U_16 maxStack;
	U_32 modifier;
	void* constantPool;
	void* classNameList;
	U_8* code;
	U_32 codeLength;
	void* exceptionTable;
	U_16 exceptionTableLength;
	U_8* stackMapData;
	U_16 stackMapCount;
	U_32 stackMapLength;
	void* romClass;
	J9PortLibrary* portLib;
	getUTF8StringfromCPFunc getUTF8StringfromCP;
	getStringfromClassNameListFunc getStringfromClassNameList;
	getExceptionTableEntryFunc getExceptionRange;
} MethodContextInfo;


/*
 * Hold data type information which includes tag of type, 
 * attribute of type value (such as constant pool index, method's signature, index of class name, etc),
 * and the type value in the verification type buffer.
 */
typedef struct VerificationTypeInfo {
	U_8 typeTag;
	U_8 typeValueAttribute;
	U_32 typeValue;
} VerificationTypeInfo;


/* Hold basic information for stackmap frame */
typedef struct StackMapFrame {
	U_8 frameType;
	U_16 bci;
	U_16 numberOfLocals;
	U_16 numberOfStack;
	UDATA numberOfSlots;
	VerificationTypeInfo* entries;
} StackMapFrame;


/* ---------------- errormessagebuffer.c ---------------- */

/**
 * Initialize the message buffer
 * @param portLibrary - the portLibrary
 * @param buf - the message buffer to be initialized (must not be NULL)
 * @param byteArray - the buffer defined on stack (must not be NULL)
 * @param size - the initial size of the message buffer (must be > 0)
 */
void
initMessageBuffer(J9PortLibrary* portLibrary, MessageBuffer* buf, U_8* byteArray, UDATA size);


/**
 * Print the formatted message to the error message buffer
 * @param buf - pointer to the MessageBuffer
 * @param msgFormat - message format
 */
void
printMessage(MessageBuffer* buf, const char* msgFormat, ...);


/* ---------------- errormessagehelper.c ---------------- */

/**
 * Convert internal J9 opcode to Oracle opcode so as to match the Oracle' output
 * @param j9Opcode - opcode internally defined in J9 (bcnames.h)
 * @param returnType - value on return to determine the return opcode (e.g. CFR_BC_lreturn if type 'long')
 * @param index - index to the exceptionTable entry
 * @return Oracle opcode if defined
 */
U_8
convertToOracleOpcodeString(U_8 j9Opcode, U_8 returnType);

/**
 * Push the 'top' type on liveStack or stackmap frame to the verification type buffer
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 * @param stackMapFrame - pointer to the specified stackmap frame data
 * @param currentVerificationTypeEntry - pointer to the VerificationTypeInfo data
 * @param slotCount - the required count of slots to be populated on 'locals'/'stack'
 * @return the next VerificationTypeInfo entry or NULL if out-of-memory
 */
VerificationTypeInfo*
pushTopTypeToVerificationTypeBuffer(MethodContextInfo* methodInfo, StackMapFrame* stackMapFrame, VerificationTypeInfo* currentVerificationTypeEntry, UDATA slotCount);


/**
 * Push the information of data type on liveStack or stackmap frame to the verification type buffer
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 * @param stackMapFrame - pointer to the specified stackmap frame data
 * @param currentVerificationTypeEntry - pointer to the VerificationTypeInfo data
 * @param typeTag - tag value of type
 * @param typeValueAttribute - attribute of type value (e.g. index in constant pool, index of class name, etc)
 * @param typeValue - the value of data type according to typeValueAttribute
 * @return the next VerificationTypeInfo entry or NULL if out-of-memory
 */
VerificationTypeInfo*
pushVerificationTypeInfo(MethodContextInfo* methodInfo, StackMapFrame* stackMapFrame, VerificationTypeInfo* currentVerificationTypeEntry, U_8 typeTag, U_8 typeValueAttribute, UDATA typeValue);


/**
 * Pop up the data of the current entry from the verification type buffer
 * @param currentVerificationTypeEntry - pointer to the current VerificationTypeInfo entry
 * @return pointer to the previous VerificationTypeInfo entry
 */
VerificationTypeInfo*
popVerificationTypeInfo(VerificationTypeInfo* currentVerificationTypeEntry);


/**
 * Print the base type name directly to the error message buffer
 * @param buf - pointer to the MessageBuffer
 * @param cursor - pointer to the current frame in methodInfo->stackMapData
 * @param itemCount - the total number of items to be show up
 * @param remainingItemSlots - the remaining slots occupied by items in stack map
 */
U_8*
printVerificationTypeInfo(MessageBuffer* buf, U_8* cursor, UDATA itemCount, I_32* remainingItemSlots);


/**
 * Print the data of full_frame type to the error message buffer
 * @param buf - pointer to the MessageBuffer
 * @param cursor - pointer to the current frame in methodInfo->stackMapData
 * @param remainingItemSlots - the remaining slots occupied by items in stack map
 */
U_8*
printFullStackFrameInfo(MessageBuffer* buf, U_8* cursor, I_32* remainingItemSlots);


/**
 * Push or pop up numOfEntries in 'locals' and 'stacks'
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 * @param stackMapFrame - pointer to the specified stackmap frame data
 * @param topIndex - pointer to the index of the top entry on 'locals' or 'stack' in the stackmap frame
 * @param entries - pointer to the current frame of stackmap frame
 * @param numEntries - the count of entries to be handled
 * @return pointer to the next frame
 */
U_8* adjustLocalsAndStack(MethodContextInfo* methodInfo, StackMapFrame* stackMapFrame, U_16* topIndex, U_8* entries, IDATA numEntries);


/**
 * Release memory allocated in the verification type buffer on liveStack or stackmap frame
 * @param stackMapFrame - pointer to the specified stackmap frame data
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 */
void
releaseVerificationTypeBuffer(StackMapFrame* stackMapFrame, MethodContextInfo* methodInfo);


/**
 * Decode all data of the specified stackmap frame from the stackmap table or verifyData->stackMaps
 * and push the information of all data type to the verification type buffer.
 * @param stackMapFrame - pointer to the current stackmap frame data
 * @param nextStackmapFrame - pointer to the next stackmap frame data
 * @param stackmapFrameIndex - index to the stackmap frame
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @return the pointer to the next stackmap frame
 */
U_8*
decodeStackmapFrameData(StackMapFrame* stackMapFrame, U_8* nextStackmapFrame, I_32 stackmapFrameIndex, MethodContextInfo* methodInfo, J9BytecodeVerificationData *verifyData);


/**
 * Decode all data of the specified stackmap frame in the stackmap table if exists
 * and push the information of all data type to the verification type buffer.
 * @param stackMapFrame - pointer to the current stackmap frame data
 * @param nextStackmapFrame - pointer to the next stackmap frame data
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 * @return the pointer to the next stackmap frame
 */
U_8*
decodeStackFrameDataFromStackMapTable(StackMapFrame* stackMapFrame, U_8* nextStackmapFrame, MethodContextInfo* methodInfo);


/**
 * Decode all data of the specified stackmap frame from verifyData->stackMaps (the decompressed stackmap table)
 * as the stackmap table doesn't exist in the class file, and push the information of all data type
 * to the verification type buffer (Only used for the 'Stackmap Frame' section).
 * @param stackMapFrame - pointer to the current stackmap frame data
 * @param nextStackmapFrame - pointer to the next stackmap frame data
 * @param stackmapFrameIndex - index to the stackmap frame
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @return the pointer to the next stackmap frame
 */
U_8*
decodeConstuctedStackMapFrameData(StackMapFrame* stackMapFrame, U_8* nextStackmapFrame, I_32 stackmapFrameIndex, MethodContextInfo* methodInfo, J9BytecodeVerificationData *verifyData);


/**
 * Push the converted information of data type (from BCV_XXX tag to CFR_STACKMAP_TYPE_XXX tag) on liveStack or stackmap frame
 * to the verification type buffer based on the tag of data type.
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 * @param stackMapFrame - pointer to the specified stack kmap frame
 * @param currentVerificationTypeEntry - pointer to the current VerificationTypeInfo entry
 * @param bcvType - data type value
 * @return 2 if it is wide type or 1 if not; otherwise, return -1 if out-of-memory
 */
IDATA
convertBcvToCfrType(MethodContextInfo* methodInfo, StackMapFrame* stackMapFrame, VerificationTypeInfo** currentVerificationTypeEntry, UDATA bcvType);


/**
 * Determine whether the specified data type is base type or array against the index in dataTypeNames[]
 * @param bcvType - data type value
 * @return the index to dataTypeNames[]
 */
IDATA
bcvToBaseTypeNameIndex(UDATA bcvType);


/**
 * Allocate memory for the verification type buffer to store the information of data type
 * on 'locals'/'stack' in liveStack or stackmap frame.
 * @param stackMapFrame - pointer to the current frame (liveStack) or the specified stackmap frame
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 * @return TRUE on success or FALSE if out-of-memory
 */
BOOLEAN
prepareVerificationTypeBuffer(StackMapFrame* stackMapFrame, MethodContextInfo* methodInfo);


/**
 * Map the data type to UTF8 string data
 * @param dataType - returned type value
 * @param stackMapFrame - pointer to the current frame (liveStack) or the specified stackmap frame
 * @param methodInfo - pointer to the MethodContextInfo that holds method related data and utilities
 * @param index - index to the array of VerificationTypeInfo data
 * @return CFR_STACKMAP_TYPE_??? or -1
 */
U_8
mapDataTypeToUTF8String(J9UTF8Ref* dataType, StackMapFrame* stackMapFrame, MethodContextInfo* methodInfo, UDATA index);


/**
 * Print the information of verification type (full name string, arity, etc) to the error message buffer
 * @param buf - pointer to the MessageBuffer
 * @param tag - type value in the dataType
 * @param dataType - pointer to the data type of stackmap frame
 * @param print2nd - flag regarding whether to print out wide data type
 * @return 2 if the data type is long/double; otherwise return 1.
 */
IDATA
printTypeInfoToBuffer(MessageBuffer* buf, U_8 tag, J9UTF8Ref* dataType, BOOLEAN print2nd);


/**
 * Print out all data of the current frame (liveStack) or the specified stackmap frame to the error message buffer
 * @param buf - pointer to the MessageBuffer
 * @param stackMapFrame - pointer to the specified stackmap frame
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 */
void
printTheStackMapFrame(MessageBuffer* buf, StackMapFrame* stackMapFrame, MethodContextInfo* methodInfo);


/**
 * Print the exception table contents to the error message buffer
 * @param buf - pointer to the MessageBuffer
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 */
void
printExceptionTable(MessageBuffer* buf, MethodContextInfo* methodInfo);


/**
 * Print the decompressed stackmap table to the error message buffer
 * @param buf - pointer to the MessageBuffer
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 */
void
printSimpleStackMapTable(MessageBuffer* buf, MethodContextInfo* methodInfo);


/**
 * Print the bytecode index in the current frame (liveStack) or the specified stackmap frame to the error message buffer
 * @param buf - pointer to the MessageBuffer
 * @param stackMapFrame - pointer to the current frame (liveStack) or the specified stackmap frame
 */
void
printStackMapFrameBCI(MessageBuffer* buf, StackMapFrame* stackMapFrame);


/**
 * Print the flag in the current frame (liveStack) or the specified stackmap frame to the error message buffer.
 * @param buf - pointer to the MessageBuffer
 * @param stackMapFrame - pointer to the current frame (liveStack) or the specified stackmap frame
 */
void
printStackMapFrameFlag(MessageBuffer* buf, StackMapFrame* stackMapFrame);


/**
 * Print out data type on 'locals' and 'stack' in the current frame (liveStack)
 * or the specified stackmap frame to the error message buffer.
 * @param buf - pointer to the MessageBuffer
 * @param stackMapFrame - pointer to the current frame (liveStack) or the specified stackmap frame
 * @param methodInfo - pointer to the MethodContextInfo that holds all method-related data and utilities
 * @param indent - indentation space at the beginning of each line
 * @param title - title name to be printed out ('locals' or 'stack')
 * @param titleLength - the length of title name
 * @param startFrom - the first index to StackMapFrame->entries
 * @param numOfItems - the total number of entries to be printed
 */
void
printStackMapFrameData(MessageBuffer* buf, StackMapFrame* stackMapFrame, MethodContextInfo* methodInfo, const char* title, UDATA titleLength, UDATA startFrom, UDATA numOfItems);


#define PRINT_LOCALS(buf, stackMapFrame, methodInfo) \
		printStackMapFrameData((buf), (stackMapFrame), (methodInfo), "locals", strlen("locals"), 0, (stackMapFrame)->numberOfLocals)

#define PRINT_STACK(buf, stackMapFrame, methodInfo) \
		printStackMapFrameData((buf), (stackMapFrame), (methodInfo), "stack", strlen("stack"), (methodInfo)->maxLocals, (stackMapFrame)->numberOfStack)

#define ADJUST_LOCALS(stackMapFrame, frame, numEntries) \
	do { \
		(frame) = adjustLocalsAndStack((methodInfo), (stackMapFrame), &(stackMapFrame)->numberOfLocals, (frame), (numEntries)); \
	} while (0)

#define ADJUST_STACK(stackMapFrame, stackBase, frame, numEntries) \
	do { \
		(stackMapFrame)->numberOfStack += (U_16)(stackBase); \
		(frame) = adjustLocalsAndStack((methodInfo), (stackMapFrame), &(stackMapFrame)->numberOfStack, (frame), (numEntries)); \
		(stackMapFrame)->numberOfStack -= (U_16)(stackBase); \
	} while (0)

#ifdef __cplusplus
}
#endif

#endif /* errormessage_internal_h */
