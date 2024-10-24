/*******************************************************************************
 * Copyright IBM Corp. and others 2020
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

#include "bcverify.h"
#include "cfreader.h"
#include "pcstack.h"
#include "rommeth.h"
#include "stackwalk.h"
#include "ut_j9vm.h"
#include "vrfytbl.h"

extern "C" {

/* Define for debug
#define DEBUG_BCV
*/

static IDATA buildBranchMap(J9NPEMessageData *npeMsgData);
static void computeNPEMsgAtPC(J9VMThread *vmThread, J9ROMMethod *romMethod, J9ROMClass *romClass, UDATA npePC,
	bool npeFinalFlag, char **npeMsg, bool *isMethodFlag, UDATA *temps, J9BytecodeOffset *bytecodeOffset);
static char* convertToJavaFullyQualifiedName(J9VMThread *vmThread, J9UTF8 *fullyQualifiedNameUTF);
static char* convertMethodSignature(J9VMThread *vmThread, J9UTF8 *methodSig);
static IDATA copyToTargetStack(J9NPEMessageData *npeMsgData, UDATA target);
static char* getCompleteNPEMessage(J9VMThread *vmThread, U_8 *bcCurrentPtr, J9ROMClass *romClass, char *npeCauseMsg, bool isMethodFlag);
static char* getFullyQualifiedMethodName(J9VMThread *vmThread, J9ROMClass *romClass, U_8 *bcIndex);
static char* getLocalsName(J9VMThread *vmThread, J9ROMMethod *romMethod, U_16 localVar, UDATA bcCausePos, UDATA *temps);
static char* getMsgWithAllocation(J9VMThread *vmThread, const char *msgTemplate, ...);
static IDATA initializeNPEMessageData(J9NPEMessageData *npeMsgData);
static void initStackFromMethodSignature(J9VMThread *vmThread, J9ROMMethod *romMethod, UDATA **stackTopPtr);
static UDATA* pushViaSiganature(J9VMThread *vmThread, U_8 *signature, UDATA *stackTop, UDATA bcPos);
static void setSrcBytecodeOffset(J9BytecodeOffset *bytecodeOffset, UDATA bcPos, UDATA first, UDATA second);
static char* simulateStack(J9NPEMessageData *npeMsgData);

#define BCV_INTERNAL_DEFAULT_SIZE (32*1024)
#define BYTECODE_BRANCH_TARGET 0xFEFEFEFE
#define BYTECODE_TEMP_CHANGED 0xFDFDFDFD
#define BYTECODE_OFFSET_SHIFT 16
#define BYTECODE_OFFSET_MASK 0xFFFF

#define NPEMSG_FIRST_STACK() \
	((J9BranchTargetStack *) (npeMsgData->stackMaps))

#define NPEMSG_NEXT_STACK(thisStack) \
	((J9BranchTargetStack *) (((U_8 *) (thisStack)) + (npeMsgData->stackSize)))

#define NPEMSG_INDEX_STACK(stackCount) \
	((J9BranchTargetStack *) (((U_8 *) NPEMSG_FIRST_STACK()) + (npeMsgData->stackSize * (stackCount))))

#define NPEMSG_CHECK_STACK_UNDERFLOW \
	if( stackTop < stackBase ) { \
		goto _internalError; \
	}

#define NPEMSG_DROP( x )	\
	stackTop -= x;	\
	NPEMSG_CHECK_STACK_UNDERFLOW

#define NPEMSG_POP	\
	*(--stackTop); \
	NPEMSG_CHECK_STACK_UNDERFLOW

#define ALLOC_BUFFER(name, needed, typecast) \
	if (needed > name##Size) { \
		j9mem_free_memory(name); \
		name = typecast j9mem_allocate_memory(needed, OMRMEM_CATEGORY_VM); \
		if (NULL != name) { \
			memset(name, 0, needed); \
			name##Size = needed; \
		} else { \
			name##Size = 0; \
			result = BCV_ERR_INSUFFICIENT_MEMORY; \
		} \
	} else { \
		memset(name, 0, name##Size); \
	}

/* Following two macro arguments must be variables, not expressions */
#define GETNEXT_U16(value, index) (value = *(U_16*)index, index += 2, value)
#define GETNEXT_U8(value, index) (value = *(index++))

/* Mapping the verification encoding in J9JavaBytecodeVerificationTable */
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

/**
 * Replace '/' with '.' for an internal fully qualified name
 *
 * Note: the caller is responsible for freeing the returned string if it is not NULL.
 *
 * @param[in] vmThread current J9VMThread
 * @param[in] fullyQualifiedNameUTF pointer to J9UTF8 containing the fully qualified name
 *
 * @return a char pointer to the fully qualified name,
 *         NULL if not successful, but keep application exception instead of throwing OOM.
 */
static char*
convertToJavaFullyQualifiedName(J9VMThread *vmThread, J9UTF8 *fullyQualifiedNameUTF)
{
	PORT_ACCESS_FROM_VMC(vmThread);
	UDATA length = J9UTF8_LENGTH(fullyQualifiedNameUTF);
	char *result =  (char *)j9mem_allocate_memory(length + 1, OMRMEM_CATEGORY_VM);

	if (NULL != result) {
		char *cursor = result;
		char *end = result + length;

		memcpy(result, J9UTF8_DATA(fullyQualifiedNameUTF), length);
		while (cursor < end) {
			if ('/' == *cursor) {
				*cursor = '.';
			}
			cursor += 1;
		}
		*end = '\0';
	}

	Trc_VM_ConvertToJavaFullyQualifiedName_Get_ClassName(vmThread, result, length, fullyQualifiedNameUTF);

	return result;
}

/**
 * Convert a signature string
 * from
 *  ([[[Ljava/lang/String;Ljava/lang/Void;BLjava/lang/String;ICDFJLjava/lang/Object;SZ)Ljava/lang/String;
 * to
 * (java.lang.String[][][], java.lang.Void, byte, java.lang.String, int, char, double, float, long, java.lang.Object, short, boolean)
 *
 * Note:
 * The return type is ignored.
 * Assuming the method signature is well formed.
 * The caller is responsible for freeing the returned string if it is not NULL.
 *
 * @param[in] vmThread current J9VMThread
 * @param[in] methodSig pointer to J9UTF8 containing the method signature
 *
 * @return a char pointer to the class name within fullQualifiedUTF,
 *         NULL if not successful, but keep application exception instead of OOM.
 */
static char*
convertMethodSignature(J9VMThread *vmThread, J9UTF8 *methodSig)
{
	UDATA i = 0;
	UDATA j = 0;
	U_8 *string = J9UTF8_DATA(methodSig);
	UDATA stringLength = J9UTF8_LENGTH(methodSig);
	UDATA bufferSize = 0;
	char *result = NULL;

	PORT_ACCESS_FROM_VMC(vmThread);

	/* first scan to calculate the buffer size required */
	/* first character is '(' */
	bufferSize += 1;
	for (i = 1; (')' != string[i]); i++) {
		UDATA arity = 0;
		while ('[' == string[i]) {
			arity += 1;
			i += 1;
		}
		switch (string[i]) {
		case 'B': /* FALLTHROUGH */
		case 'C': /* FALLTHROUGH */
		case 'J':
			/* byte, char, long */
			bufferSize += 4;
			break;
		case 'D':
			/* double */
			bufferSize += 6;
			break;
		case 'F': /* FALLTHROUGH */
		case 'S':
			/* float, short */
			bufferSize += 5;
			break;
		case 'I':
			/* int */
			bufferSize += 3;
			break;
		case 'L':
		{
			i += 1;
			UDATA objSize = 0;
			while (';' != string[i]) {
				objSize += 1;
				i += 1;
			}
			bufferSize += objSize;
			break;
		}
		case 'Z':
			/* boolean */
			bufferSize += 7;
			break;
		default:
			Trc_VM_ConvertMethodSignature_Malformed_Signature(vmThread, stringLength, string, i);
			break;
		}
		bufferSize += (2 * arity);
		if (')' != string[i + 1]) {
			/* ", " */
			bufferSize += 2;
		}
	}
	/* ')' and the extra byte for '\0' */
	bufferSize += 2;
	Trc_VM_ConvertMethodSignature_Signature_BufferSize(vmThread, stringLength, string, bufferSize);
	result = (char *)j9mem_allocate_memory(bufferSize, OMRMEM_CATEGORY_VM);
	if (NULL != result) {
		char *cursor = result;
		UDATA availableSize = bufferSize;

		memset(result, 0, bufferSize);
		/* first character is '(' */
		j9str_printf(PORTLIB, cursor, availableSize, "(");
		cursor += 1;
		availableSize -= 1;
		for (i = 1; (')' != string[i]); i++) {
			UDATA arity = 0;
			while ('[' == string[i]) {
				arity += 1;
				i += 1;
			}
			const char *elementType = NULL;
			if (IS_CLASS_SIGNATURE(string[i])) {
				i += 1;

				UDATA objSize = 0;
				while (';' != string[i]) {
					*cursor = string[i];
					if ('/' == string[i]) {
						*cursor = '.';
					}
					objSize += 1;
					cursor += 1;
					i += 1;
				}
				availableSize -= objSize;
			} else {
				switch (string[i]) {
				case 'B':
					elementType = "byte";
					break;
				case 'C':
					elementType = "char";
					break;
				case 'D':
					elementType = "double";
					break;
				case 'F':
					elementType = "float";
					break;
				case 'I':
					elementType = "int";
					break;
				case 'J':
					elementType = "long";
					break;
				case 'S':
					elementType = "short";
					break;
				case 'Z':
					elementType = "boolean";
					break;
				default:
					Trc_VM_ConvertMethodSignature_Malformed_Signature(vmThread, stringLength, string, i);
					break;
				}
				UDATA elementLength = strlen(elementType);
				j9str_printf(PORTLIB, cursor, availableSize, elementType);
				cursor += elementLength;
				availableSize -= elementLength;
			}
			for(j = 0; j < arity; j++) {
				j9str_printf(PORTLIB, cursor, availableSize, "[]");
				cursor += 2;
				availableSize -= 2;
			}

			if (')' != string[i + 1]) {
				j9str_printf(PORTLIB, cursor, availableSize, ", ");
				cursor += 2;
				availableSize -= 2;
			}
		}
		j9str_printf(PORTLIB, cursor, availableSize, ")");
	} else {
		bufferSize = 0;
	}
	Trc_VM_ConvertMethodSignature_Signature_Result(vmThread, result, bufferSize);

	return result;
}

/**
 * Return an extended NPE message.
 *
 * Note: the caller is responsible for freeing the returned string if it is not NULL.
 *
 * @param[in] vmThread The current J9VMThread
 * @param[in] bcCurrentPtr The pointer to the bytecode being executed and caused the NPE
 * @param[in] romClass The romClass of the bytecode
 * @param[in] npeCauseMsg The cause of NPE
 * @param[in] isMethodFlag The flag indicates if "the return value of" should be added into the message
 *
 * @return an extended NPE message or NULL if such a message can't be generated
 */
static char*
getCompleteNPEMessage(J9VMThread *vmThread, U_8 *bcCurrentPtr, J9ROMClass *romClass, char *npeCauseMsg, bool isMethodFlag)
{
	char *npeMsg = NULL;
	const char *msgTemplate = NULL;
	U_8 bcCurrent = *bcCurrentPtr;

	PORT_ACCESS_FROM_VMC(vmThread);

	Trc_VM_GetCompleteNPEMessage_Entry(vmThread, bcCurrentPtr, romClass, bcCurrent, npeCauseMsg, isMethodFlag);
	if (((bcCurrent >= JBiaload) && (bcCurrent <= JBsaload))
		|| ((bcCurrent >= JBiastore) && (bcCurrent <= JBsastore))
	) {
		const char *elementType = NULL;
		switch (bcCurrent) {
		case JBiaload: /* FALLTHROUGH */
		case JBiastore:
			elementType = "int";
			break;
		case JBlaload: /* FALLTHROUGH */
		case JBlastore:
			elementType = "long";
			break;
		case JBfaload: /* FALLTHROUGH */
		case JBfastore:
			elementType = "float";
			break;
		case JBdaload: /* FALLTHROUGH */
		case JBdastore:
			elementType = "double";
			break;
		case JBaaload: /* FALLTHROUGH */
		case JBaastore:
			elementType = "object";
			break;
		case JBbaload: /* FALLTHROUGH */
		case JBbastore:
			elementType = "byte/boolean";
			break;
		case JBcaload: /* FALLTHROUGH */
		case JBcastore:
			elementType = "char";
			break;
		case JBsaload: /* FALLTHROUGH */
		case JBsastore:
			elementType = "short";
			break;
		default:
			Trc_VM_GetCompleteNPEMessage_Unreachable(vmThread, bcCurrent);
		}
		if (NULL == npeCauseMsg) {
			if ((bcCurrent >= JBiaload) && (bcCurrent <= JBsaload)) {
				msgTemplate = "Cannot load from %s array";
			} else {
				msgTemplate = "Cannot store to %s array";
			}
			npeMsg = getMsgWithAllocation(vmThread, msgTemplate, elementType);
		} else {
			if ((bcCurrent >= JBiaload) && (bcCurrent <= JBsaload)) {
				if (isMethodFlag) {
					msgTemplate = "Cannot load from %s array because the return value of \"%s\" is null";
				} else {
					msgTemplate = "Cannot load from %s array because \"%s\" is null";
				}
			} else {
				if (isMethodFlag) {
					msgTemplate = "Cannot store to %s array because the return value of \"%s\" is null";
				} else {
					msgTemplate = "Cannot store to %s array because \"%s\" is null";
				}
			}
			npeMsg = getMsgWithAllocation(vmThread, msgTemplate, elementType, npeCauseMsg);
		}
	} else {
		switch (bcCurrent) {
		case JBarraylength: {
			if (NULL == npeCauseMsg) {
				npeMsg = getMsgWithAllocation(vmThread, msgTemplate, "Cannot read the array length");
			} else {
				if (isMethodFlag) {
					msgTemplate = "Cannot read the array length because the return value of \"%s\" is null";
				} else {
					msgTemplate = "Cannot read the array length because \"%s\" is null";
				}
				npeMsg = getMsgWithAllocation(vmThread, msgTemplate, npeCauseMsg);
			}
			break;
		}
		case JBathrow: {
			if (NULL == npeCauseMsg) {
				npeMsg = getMsgWithAllocation(vmThread, "Cannot throw exception");
			} else {
				if (isMethodFlag) {
					msgTemplate = "Cannot throw exception because the return value of \"%s\" is null";
				} else {
					msgTemplate = "Cannot throw exception because \"%s\" is null";
				}
				npeMsg = getMsgWithAllocation(vmThread, msgTemplate, npeCauseMsg);
			}
			break;
		}
		case JBmonitorenter: {
			if (NULL == npeCauseMsg) {
				npeMsg = getMsgWithAllocation(vmThread, "Cannot enter synchronized block");
			} else {
				if (isMethodFlag) {
					msgTemplate = "Cannot enter synchronized block because the return value of \"%s\" is null";
				} else {
					msgTemplate = "Cannot enter synchronized block because \"%s\" is null";
				}
				npeMsg = getMsgWithAllocation(vmThread, msgTemplate, npeCauseMsg);
			}
			break;
		}
		case JBmonitorexit: {
			if (NULL == npeCauseMsg) {
				npeMsg = getMsgWithAllocation(vmThread, "Cannot exit synchronized block");
			} else {
				if (isMethodFlag) {
					msgTemplate = "Cannot exit synchronized block because the return value of \"%s\" is null";
				} else {
					msgTemplate = "Cannot exit synchronized block because \"%s\" is null";
				}
				npeMsg = getMsgWithAllocation(vmThread, msgTemplate, npeCauseMsg);
			}
			break;
		}
		case JBgetfield: /* FALLTHROUGH */
		case JBputfield: {
			U_16 index = PARAM_16(bcCurrentPtr, 1);
			UDATA cpType = J9_CP_TYPE(J9ROMCLASS_CPSHAPEDESCRIPTION(romClass), index);

			Trc_VM_GetCompleteNPEMessage_Field_Index(vmThread, index);
			if (J9CPTYPE_FIELD == cpType) {
				J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);
				J9ROMConstantPoolItem *cpItem = constantPool + index;
				J9ROMNameAndSignature *fieldNameAndSig = J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *)cpItem);
				J9UTF8 *fieldName = J9ROMNAMEANDSIGNATURE_NAME(fieldNameAndSig);

				if (NULL == npeCauseMsg) {
					if (JBputfield == bcCurrent) {
						msgTemplate = "Cannot assign field \"%.*s\"";
					} else {
						msgTemplate = "Cannot read field \"%.*s\"";
					}
					npeMsg = getMsgWithAllocation(vmThread, msgTemplate, J9UTF8_LENGTH(fieldName), J9UTF8_DATA(fieldName));
				} else {
					if (JBputfield == bcCurrent) {
						if (isMethodFlag) {
							msgTemplate = "Cannot assign field \"%.*s\" because the return value of \"%s\" is null";
						} else {
							msgTemplate = "Cannot assign field \"%.*s\" because \"%s\" is null";
						}
					} else {
						if (isMethodFlag) {
							msgTemplate = "Cannot read field \"%.*s\" because the return value of \"%s\" is null";
						} else {
							msgTemplate = "Cannot read field \"%.*s\" because \"%s\" is null";
						}
					}
					npeMsg = getMsgWithAllocation(vmThread, msgTemplate, J9UTF8_LENGTH(fieldName), J9UTF8_DATA(fieldName), npeCauseMsg);
				}
			} else {
				Trc_VM_GetCompleteNPEMessage_UnexpectedCPType(vmThread, cpType, bcCurrent);
			}
			break;
		}
		case JBinvokehandle: /* FALLTHROUGH */
		case JBinvokehandlegeneric: /* FALLTHROUGH */
		case JBinvokeinterface2: /* FALLTHROUGH */
		case JBinvokeinterface: /* FALLTHROUGH */
		case JBinvokespecial: /* FALLTHROUGH */
		case JBinvokestatic: /* FALLTHROUGH */
		case JBinvokevirtual: {
			U_16 index = 0;
			J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);

			if (JBinvokeinterface2 == bcCurrent) {
				index = PARAM_16(bcCurrentPtr + 2, 1); /* get JBinvokeinterface instead */
			} else {
				index = PARAM_16(bcCurrentPtr, 1);
			}
			Trc_VM_GetCompleteNPEMessage_Invoke_Index(vmThread, index);

			J9ROMMethodRef *romMethodRef = (J9ROMMethodRef *)&constantPool[index];
			J9ROMNameAndSignature *methodNameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef);
			J9UTF8 *methodName = J9ROMNAMEANDSIGNATURE_NAME(methodNameAndSig);
			J9UTF8 *definingClassFullQualifiedName = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *)&constantPool[romMethodRef->classRefCPIndex]);
			J9UTF8* methodSig = J9ROMNAMEANDSIGNATURE_SIGNATURE(methodNameAndSig);
			bool npeMsgRequired = true;

#if JAVA_SPEC_VERSION >= 18
			if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(definingClassFullQualifiedName), J9UTF8_LENGTH(definingClassFullQualifiedName), "jdk/internal/reflect/DirectConstructorHandleAccessor")) {
				if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodName), "invokeImpl")) {
					/* JEP416 - jdk.internal.reflect.DirectConstructorHandleAccessor.invokeImpl() is invoked by DirectConstructorHandleAccessor.newInstance().
					 * No message generated for new NullPointerException().getMessage() or new NullPointerException(null).getMessage()
					 */
					npeMsgRequired = false;
					Trc_VM_GetCompleteNPEMessage_Not_Required(vmThread);
				}
			} else
#endif /* JAVA_SPEC_VERSION >= 18 */
			if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(definingClassFullQualifiedName), J9UTF8_LENGTH(definingClassFullQualifiedName), "java/lang/NullPointerException")) {
				if (J9UTF8_LITERAL_EQUALS(J9UTF8_DATA(methodName), J9UTF8_LENGTH(methodName), "<init>")) {
					/* No message generated for new NullPointerException().getMessage() or new NullPointerException(null).getMessage() */
					npeMsgRequired = false;
					Trc_VM_GetCompleteNPEMessage_Not_Required(vmThread);
				}

			}
			if (npeMsgRequired) {
				char *fullyQualifiedClassName = convertToJavaFullyQualifiedName(vmThread, definingClassFullQualifiedName);
				char *methodSigParameters = convertMethodSignature(vmThread, methodSig);
				if (NULL == npeCauseMsg) {
					npeMsg = getMsgWithAllocation(vmThread, "Cannot invoke \"%s.%.*s%s\"",
						fullyQualifiedClassName, J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), methodSigParameters);
				} else {
					if (isMethodFlag) {
						msgTemplate = "Cannot invoke \"%s.%.*s%s\" because the return value of \"%s\" is null";
					} else {
						msgTemplate = "Cannot invoke \"%s.%.*s%s\" because \"%s\" is null";
					}
					npeMsg = getMsgWithAllocation(vmThread, msgTemplate,
							fullyQualifiedClassName, J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), methodSigParameters, npeCauseMsg);
				}
				j9mem_free_memory(fullyQualifiedClassName);
				j9mem_free_memory(methodSigParameters);
			}
			break;
		}
		default:
			Trc_VM_GetCompleteNPEMessage_MissedBytecode(vmThread, bcCurrent);
		}
	}
	j9mem_free_memory(npeCauseMsg);
	Trc_VM_GetCompleteNPEMessage_Exit(vmThread, npeMsg);

	return npeMsg;
}

/*
 * Initialize npeMsgData with incoming romMethod
 * @param[in] npeMsgData - the J9NPEMessageData structure holding romClass/romMethod/npePC
 *
 * @return IDATA BCV_SUCCESS if it succeeded, BCV_ERR_INTERNAL_ERROR for any unexpected error
 */
static IDATA
initializeNPEMessageData(J9NPEMessageData *npeMsgData)
{
	PORT_ACCESS_FROM_VMC(npeMsgData->vmThread);
	J9ROMMethod *romMethod = npeMsgData->romMethod;
	IDATA result = BCV_SUCCESS;

	/* BCV_TARGET_STACK_HEADER_UDATA_SIZE for pc/stackBase/stackEnd in J9BranchTargetStack and
	 * BCV_STACK_OVERFLOW_BUFFER_UDATA_SIZE for late overflow detection of longs/doubles
	 */
	npeMsgData->stackSize = (J9_MAX_STACK_FROM_ROM_METHOD(romMethod)
							+ J9_ARG_COUNT_FROM_ROM_METHOD(romMethod)
							+ J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod)
							+ BCV_TARGET_STACK_HEADER_UDATA_SIZE
							+ BCV_STACK_OVERFLOW_BUFFER_UDATA_SIZE) * sizeof(UDATA);
	ALLOC_BUFFER(npeMsgData->liveStack, npeMsgData->stackSize, (J9BranchTargetStack *));

	UDATA bytecodeSize = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	UDATA bytecodeBufferSize = bytecodeSize * sizeof(U_32) * 2;
	ALLOC_BUFFER(npeMsgData->bytecodeOffset, bytecodeBufferSize, (J9BytecodeOffset *));
	ALLOC_BUFFER(npeMsgData->bytecodeMap, bytecodeBufferSize, (U_32 *));

	result = buildBranchMap(npeMsgData);
	if (result >= 0) {
		J9BranchTargetStack *liveStack = NULL;

		if (result > 0) {
			npeMsgData->stackMapsCount = result;

			UDATA stackMapsSize = (npeMsgData->stackSize) * (npeMsgData->stackMapsCount);
			ALLOC_BUFFER(npeMsgData->stackMaps, stackMapsSize, (UDATA *));
			UDATA mapIndex = 0;
			UDATA j = 0;
			U_32 *bytecodeMap = npeMsgData->bytecodeMap;

			liveStack = NPEMSG_FIRST_STACK ();
			for (j = 0; j < bytecodeSize; j++) {
				if ((bytecodeMap)[j] & BRANCH_TARGET) {
					liveStack->pc = j;		/* offset of the branch target */
					liveStack->stackBaseIndex = -1;
					liveStack->stackTopIndex = -1;
					liveStack = NPEMSG_NEXT_STACK(liveStack);
					bytecodeMap[j] |= (mapIndex << BRANCH_INDEX_SHIFT); /* adding the stack # */
					mapIndex++;
				}
			}

			UDATA unwalkedQueueSize = (npeMsgData->stackMapsCount + 1) * sizeof(UDATA);
			ALLOC_BUFFER(npeMsgData->unwalkedQueue, unwalkedQueueSize, (UDATA *));
		}

		liveStack = (J9BranchTargetStack *)npeMsgData->liveStack;
		UDATA *stackTop = &(liveStack->stackElements[0]);
		initStackFromMethodSignature(npeMsgData->vmThread, romMethod, &stackTop);
		SAVE_STACKTOP(liveStack, stackTop);
		liveStack->stackBaseIndex = liveStack->stackTopIndex;
		result = BCV_SUCCESS;
	}

	return result;
}

/*
 * Initialize bytecodeMap with BRANCH_TARGET or BRANCH_EXCEPTION_START
 * @param[in] npeMsgData - the J9NPEMessageData structure holding romClass/romMethod/npePC
 *
 * @return IDATA the number of branch targets and exception handler starts
 *         BCV_ERR_INTERNAL_ERROR for any unexpected error
 */
static IDATA
buildBranchMap(J9NPEMessageData *npeMsgData)
{
	J9ROMMethod *romMethod = npeMsgData->romMethod;
	U_32 *bytecodeMap = npeMsgData->bytecodeMap;
	UDATA count = 0;
	U_8 *bcStart = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	U_8 *bcIndex = bcStart;
	U_8 *bcEnd = bcStart + J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);

	while (bcIndex < bcEnd) {
		IDATA start = 0;
		I_32 longBranch = 0;
		UDATA bc = *bcIndex;
		/* high nybble = branch action, low nybble = instruction size */
		UDATA size = J9JavaInstructionSizeAndBranchActionTable[bc];
		if (0 == size) {
			return BCV_ERR_INTERNAL_ERROR;
		}

		switch (size >> 4) { /* branch action */
		case 5: { /* switches */
			start = bcIndex - bcStart;
			IDATA pc = (start + 4) & ~3;
			bcIndex = bcStart + pc;
			longBranch = (I_32) PARAM_32(bcIndex, 0);
			bcIndex += 4;
			if (0 == bytecodeMap[start + longBranch]) {
				bytecodeMap[start + longBranch] = BRANCH_TARGET;
				count++;
			}

			UDATA npairs = 0;
			IDATA pcs = 0;
			IDATA low = (I_32) PARAM_32(bcIndex, 0);
			bcIndex += 4;
			if (JBtableswitch == bc) {
				IDATA high = (I_32) PARAM_32(bcIndex, 0);
				bcIndex += 4;
				npairs = (UDATA) (high - low + 1);
				pcs = 0;
			} else {
				npairs = (UDATA) low;
				pcs = 4;
			}

			UDATA temp = 0;
			for (temp = 0; temp < npairs; temp++) {
				bcIndex += pcs;
				longBranch = (I_32) PARAM_32(bcIndex, 0);
				bcIndex += 4;
				if (0 == bytecodeMap[start + longBranch]) {
					bytecodeMap[start + longBranch] = BRANCH_TARGET;
					count++;
				}
			}
			continue;
		}

		case 2: /* gotos */
			if (JBgotow == bc) {
				start = bcIndex - bcStart;
				longBranch = (I_32) PARAM_32(bcIndex, 1);
				if (0 == bytecodeMap[start + longBranch]) {
					bytecodeMap[start + longBranch] = BRANCH_TARGET;
					count++;
				}
				break;
			} /* fall through for JBgoto */

		case 1: /* ifs */
			I_16 shortBranch = (I_16) PARAM_16(bcIndex, 1);
			start = bcIndex - bcStart;
			if (0 == bytecodeMap[start + shortBranch]) {
				bytecodeMap[start + shortBranch] = BRANCH_TARGET;
				count++;
			}
			break;
		}
		bcIndex += size & 7;
	}

	/* need to walk exceptions as well, since they are branch targets */
	if (romMethod->modifiers & J9AccMethodHasExceptionInfo) {
		J9ExceptionInfo *exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
		J9ExceptionHandler *handler = NULL;

		if (exceptionData->catchCount) {
			UDATA temp = 0;
			IDATA pc = 0;
			IDATA pcs = 0;

			handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);
			for (temp = 0; temp < (U_32) exceptionData->catchCount; temp++) {
				pc = (IDATA) handler->startPC;
				pcs = (IDATA) handler->handlerPC;
				/* Avoid re-walking a handler that handles itself */
				if (pc != pcs) {
					bytecodeMap[pc] |= BRANCH_EXCEPTION_START;
				}
				if (0 == (bytecodeMap[pcs] & BRANCH_TARGET)) {
					bytecodeMap[pcs] |= BRANCH_TARGET;
					count++;
				}
				handler++;
			}
		}
	}

	return count;
}

/**
 * Compute NPE message at npePC supplied. This can be invoked recursively.
 *
 * @param[in] vmThread The current J9VMThread
 * @param[in] bcCurrentPtr The pointer to the bytecode being executed and caused the NPE
 * @param[in] romMethod The romMethod of the bytecode
 * @param[in] romClass The romClass of the bytecode
 * @param[in] npePC The bytecode offset where the NPE message is to be generated
 * @param[in] npeFinalFlag The flag indicates if getCompleteNPEMessage() is invoked
 * @param[in,out] npeMsg An extended NPE message generated so far
 * @param[in,out] isMethodFlag The flag indicates if "the return value of" should be added into the message
 * @param[in] temps The location of bytecode temps supplied via liveStack->stackElements
 * @param[in] bytecodeOffset The array storing the bytecode offset
 * @param[in] bytecodeOffset The cause of NPE
 */
static void
computeNPEMsgAtPC(J9VMThread *vmThread, J9ROMMethod *romMethod, J9ROMClass *romClass, UDATA npePC,
		bool npeFinalFlag, char **npeMsg, bool *isMethodFlag, UDATA *temps, J9BytecodeOffset *bytecodeOffset)
{
	Trc_VM_ComputeNPEMsgAtPC_Entry(vmThread, romClass, romMethod, temps, bytecodeOffset, npePC, npeFinalFlag, *isMethodFlag, *npeMsg);
	PORT_ACCESS_FROM_VMC(vmThread);
	if (NULL != *npeMsg) {
		j9mem_free_memory(*npeMsg);
		*npeMsg = NULL;
	}
	if (BYTECODE_TEMP_CHANGED == npePC) {
		/* *npeMsg is NULL */
	} else {
		J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);
		U_8 *code = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
		U_8 *bcCurrentPtr = code + npePC;
		U_8 bcCurrent = *bcCurrentPtr;

		Trc_VM_ComputeNPEMsgAtPC_start(vmThread, romClass, romMethod, temps, bytecodeOffset, bcCurrent, npePC, npeFinalFlag, *isMethodFlag, *npeMsg);
		if ((bcCurrent >= JBiconstm1) && (bcCurrent <= JBdconst1)) {
			/*
			 * JBiconstm1, JBiconst0, JBiconst1, JBiconst2, JBiconst3, JBiconst4, JBiconst5
			 * JBlconst0, JBlconst1
			 * JBfconst0, JBfconst1, JBfconst2
			 * JBdconst0, JBdconst1
			 */
			I_8 constNum = 0;
			bool missedBCFlag = false;

			if (JBiconstm1 == bcCurrent) {
				constNum = -1;
			} else {
				if ((bcCurrent >= JBiconst0) && (bcCurrent <= JBiconst5)) {
					constNum = bcCurrent - 3; /* iconst_0 = 3 (0x3) */
				} else if ((JBlconst0 == bcCurrent) || (JBfconst0 == bcCurrent) || (JBdconst0 == bcCurrent)) {
					constNum = 0;
				} else if ((JBlconst1 == bcCurrent) || (JBfconst1 == bcCurrent) || (JBdconst1 == bcCurrent)) {
					constNum = 1;
				} else if (JBfconst2 == bcCurrent) {
					constNum = 2;
				} else {
					missedBCFlag = true;
					Trc_VM_ComputeNPEMsgAtPC_Constants_UnexpectedBC(vmThread, bcCurrent);
				}
			}
			if (!missedBCFlag) {
				*npeMsg = getMsgWithAllocation(vmThread, "%d", constNum);
			}
		} else if (((bcCurrent >= JBiadd) && (bcCurrent <= JBlxor))
			|| ((bcCurrent >= JBlcmp) && (bcCurrent <= JBdcmpg))
		) {
			/* JBiadd, JBladd, JBfadd, JBdadd, JBisub, JBlsub, JBfsub, JBdsub, JBimul, JBlmul, JBfmul, JBdmul
			 * JBidiv, JBldiv, JBfdiv, JBddiv, JBirem, JBlrem, JBfrem, JBdrem, JBineg, JBlneg, JBfneg, JBdneg
			 * JBishl, JBlshl, JBishr, JBlshr, JBiushr, JBlushr, JBiand, JBland, JBior, JBlor, JBixor, JBlxor
			 *
			 * JBlcmp, JBfcmpl, JBfcmpg, JBdcmpl, JBdcmpg
			 */
			*npeMsg = getMsgWithAllocation(vmThread, "...");
		} else {
			switch (bcCurrent) {
			case JBaconstnull:
				*npeMsg = getMsgWithAllocation(vmThread, "null");
				break;

			case JBbipush:
				*npeMsg = getMsgWithAllocation(vmThread, "%lu", *(bcCurrentPtr + 1));
				break;

			case JBsipush: {
				UDATA sipushIndex = 0;
				U_8 *tmpbcPtr = bcCurrentPtr + 1;

				GETNEXT_U16(sipushIndex, tmpbcPtr);
				*npeMsg = getMsgWithAllocation(vmThread, "%lu", sipushIndex);
				break;
			}

			case JBldc:		/* Fall through case !!! */
			case JBldcw:	/* Fall through case !!! */
			case JBldc2dw:	/* Fall through case !!! */
			case JBldc2lw: {
				UDATA ldcIndex = 0;
				U_8 *tmpbcPtr = bcCurrentPtr + 1;

				if (JBldc == bcCurrent) {
					GETNEXT_U8(ldcIndex, tmpbcPtr);
				} else if (JBldcw == bcCurrent) {
					GETNEXT_U16(ldcIndex, tmpbcPtr);
				} else {
					/* bcCurrent is JBldc2lw or JBldc2dw */
					GETNEXT_U8(ldcIndex, tmpbcPtr);
				}

				J9ROMConstantPoolItem *info = &constantPool[ldcIndex];
				if (BCT_J9DescriptionCpTypeScalar == ((J9ROMSingleSlotConstantRef *) info)->cpType) {
					/* this is a float/int constant */
					ldcIndex = ((J9ROMSingleSlotConstantRef *) info)->data;
					*npeMsg = getMsgWithAllocation(vmThread, "%lu", ldcIndex);
				} else {
					 Trc_VM_ComputeNPEMsgAtPC_NotScalarType(vmThread, romClass, romMethod, constantPool, ldcIndex, info, ((J9ROMSingleSlotConstantRef *) info)->cpType);
				}
				break;
			}

			case JBanewarray: {
				UDATA index = PARAM_16(bcCurrentPtr, 1);
				J9ROMConstantPoolItem *info = &constantPool[index];
				J9UTF8 *className = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info);
				char *fullyQualifiedClassName = convertToJavaFullyQualifiedName(vmThread, className);

				*npeMsg = getMsgWithAllocation(vmThread, fullyQualifiedClassName);
				j9mem_free_memory(fullyQualifiedClassName);
				break;
			}

			case JBinvokeinterface2:	/* Fall through case !!! */
			case JBinvokevirtual:		/* Fall through case !!! */
			case JBinvokespecial:		/* Fall through case !!! */
			case JBinvokeinterface:		/* Fall through case !!! */
			case JBinvokestatic: {
				if (npeFinalFlag) {
					UDATA objectrefPos = bytecodeOffset[npePC].first;

					if (BYTECODE_BRANCH_TARGET == objectrefPos) {
						/* *npeMsg is NULL */
					} else {
						computeNPEMsgAtPC(vmThread, romMethod, romClass, objectrefPos, false, npeMsg, isMethodFlag, temps, bytecodeOffset);
					}
				} else {
					U_8 *bcPtrTemp = (JBinvokeinterface2 == bcCurrent) ? bcCurrentPtr + 2 : bcCurrentPtr;
					char *methodName = getFullyQualifiedMethodName(vmThread, romClass, bcPtrTemp);

					*npeMsg = getMsgWithAllocation(vmThread, methodName);
					j9mem_free_memory(methodName);
					*isMethodFlag = true;
				}
				break;
			}

			case JBiload0:	/* Fall through case !!! */
			case JBiload1:	/* Fall through case !!! */
			case JBiload2:	/* Fall through case !!! */
			case JBiload3:	/* Fall through case !!! */
			case JBlload0:	/* Fall through case !!! */
			case JBlload1:	/* Fall through case !!! */
			case JBlload2:	/* Fall through case !!! */
			case JBlload3:	/* Fall through case !!! */
			case JBfload0:	/* Fall through case !!! */
			case JBfload1:	/* Fall through case !!! */
			case JBfload2:	/* Fall through case !!! */
			case JBfload3:	/* Fall through case !!! */
			case JBdload0:	/* Fall through case !!! */
			case JBdload1:	/* Fall through case !!! */
			case JBdload2:	/* Fall through case !!! */
			case JBdload3:	/* Fall through case !!! */
			case JBaload0:	/* Fall through case !!! */
			case JBaload1:	/* Fall through case !!! */
			case JBaload2:	/* Fall through case !!! */
			case JBaload3:	/* Fall through case !!! */
			case JBaload0getfield: {
				U_16 localVar = 0;
				if (bcCurrent >= JBiload0 && bcCurrent <= JBiload3) {
					localVar = bcCurrent - JBiload0;
				} else if (bcCurrent >= JBlload0 && bcCurrent <= JBlload3) {
					localVar = bcCurrent - JBlload0;
				} else if (bcCurrent >= JBfload0 && bcCurrent <= JBfload3) {
					localVar = bcCurrent - JBfload0;
				} else if (bcCurrent >= JBdload0 && bcCurrent <= JBdload3) {
					localVar = bcCurrent - JBdload0;
				} else if (bcCurrent >= JBaload0 && bcCurrent <= JBaload3) {
					localVar = bcCurrent - JBaload0;
				} else if (JBaload0getfield == bcCurrent) {
					localVar = 0;
				}
				*npeMsg = getLocalsName(vmThread, romMethod, localVar, npePC, temps);
				break;
			}

			case JBiload:	/* Fall through case !!! */
			case JBlload:	/* Fall through case !!! */
			case JBfload:	/* Fall through case !!! */
			case JBdload:	/* Fall through case !!! */
			case JBaload: {
				/* The index is an unsigned byte that must be an index into the
				 * local variable array of the current frame (section 2.6).
				 * The local variable at index must contain a reference.
				 */
				*npeMsg = getLocalsName(vmThread, romMethod, *(bcCurrentPtr + 1), npePC, temps);
				break;
			}

			case JBiloadw:	/* Fall through case !!! */
			case JBlloadw:	/* Fall through case !!! */
			case JBfloadw:	/* Fall through case !!! */
			case JBdloadw:	/* Fall through case !!! */
			case JBaloadw: {
				U_16 localVar = 0;
				U_8 *tmpbcPtr = bcCurrentPtr + 1;

				GETNEXT_U16(localVar, tmpbcPtr);
				*npeMsg = getLocalsName(vmThread, romMethod, localVar, npePC, temps);
				break;
			}

			case JBi2l:	/* Fall through case !!! */
			case JBi2f:	/* Fall through case !!! */
			case JBi2d:	/* Fall through case !!! */
			case JBl2i:	/* Fall through case !!! */
			case JBl2f:	/* Fall through case !!! */
			case JBl2d:	/* Fall through case !!! */
			case JBf2i:	/* Fall through case !!! */
			case JBf2l:	/* Fall through case !!! */
			case JBf2d:	/* Fall through case !!! */
			case JBd2i:	/* Fall through case !!! */
			case JBd2l:	/* Fall through case !!! */
			case JBd2f:	/* Fall through case !!! */
			case JBi2b:	/* Fall through case !!! */
			case JBi2c:	/* Fall through case !!! */
			case JBi2s:	/* Fall through case !!! */
			case JBarraylength:	/* Fall through case !!! */
			case JBathrow:	/* Fall through case !!! */
			case JBmonitorenter:	/* Fall through case !!! */
			case JBmonitorexit:	/* Fall through case !!! */
			case JBdup:	/* Fall through case !!! */
			case JBdupx1:	/* Fall through case !!! */
			case JBdupx2:	/* Fall through case !!! */
			case JBdup2:	/* Fall through case !!! */
			case JBdup2x1:	/* Fall through case !!! */
			case JBdup2x2:
				computeNPEMsgAtPC(vmThread, romMethod, romClass, bytecodeOffset[npePC].first, false, npeMsg, isMethodFlag, temps, bytecodeOffset);
				break;

			case JBiastore:	/* Fall through case !!! */
			case JBlastore:	/* Fall through case !!! */
			case JBfastore:	/* Fall through case !!! */
			case JBdastore:	/* Fall through case !!! */
			case JBbastore:	/* Fall through case !!! */
			case JBcastore:	/* Fall through case !!! */
			case JBsastore:	/* Fall through case !!! */
			case JBaastore: {
				UDATA bcCausePos2 = bytecodeOffset[npePC].second;
				UDATA bcCausePos = bytecodeOffset[npePC].first;

				if ((BYTECODE_BRANCH_TARGET == bcCausePos)
					|| (BYTECODE_BRANCH_TARGET == bcCausePos2)
				) {
					/* *npeMsg is NULL */
				} else {
					computeNPEMsgAtPC(vmThread, romMethod, romClass, bcCausePos, false, npeMsg, isMethodFlag, temps, bytecodeOffset);
				}
				break;
			}

			case JBiaload:	/* Fall through case !!! */
			case JBlaload:	/* Fall through case !!! */
			case JBfaload:	/* Fall through case !!! */
			case JBdaload:	/* Fall through case !!! */
			case JBbaload:	/* Fall through case !!! */
			case JBcaload:	/* Fall through case !!! */
			case JBsaload:	/* Fall through case !!! */
			case JBaaload: {
				UDATA bcCausePos = bytecodeOffset[npePC].first;

				char *npeMsgObjref = NULL;
				UDATA aaloadIndexPos = 0;
				if (BYTECODE_BRANCH_TARGET == bcCausePos) {
					if (!npeFinalFlag) {
						npeMsgObjref = getMsgWithAllocation(vmThread, "<array>");
					}
				} else {
					computeNPEMsgAtPC(vmThread, romMethod, romClass, bcCausePos, false, &npeMsgObjref, isMethodFlag, temps, bytecodeOffset);
				}
				aaloadIndexPos = bytecodeOffset[npePC].second;
				if (npeFinalFlag || (NULL == npeMsgObjref)) {
					*npeMsg = npeMsgObjref;
				} else {
					char *npeMsgIndex = NULL;
					if (BYTECODE_BRANCH_TARGET == aaloadIndexPos) {
						npeMsgIndex = getMsgWithAllocation(vmThread, "...");
					} else {
						computeNPEMsgAtPC(vmThread, romMethod, romClass, aaloadIndexPos, false, &npeMsgIndex, isMethodFlag, temps, bytecodeOffset);
					}

					if (NULL != npeMsgIndex) {
						*npeMsg = getMsgWithAllocation(vmThread, "%s[%s]", npeMsgObjref, npeMsgIndex);
						*isMethodFlag = false;
						j9mem_free_memory(npeMsgIndex);
					}
					j9mem_free_memory(npeMsgObjref);
				}
				break;
			}

			case JBgetstatic: {
				UDATA index = PARAM_16(bcCurrentPtr, 1);
				J9ROMConstantPoolItem *info = &constantPool[index];
				J9UTF8 *className = J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[((J9ROMFieldRef *) info)->classRefCPIndex]);
				char *fullyQualifiedClassName = convertToJavaFullyQualifiedName(vmThread, className);
				J9UTF8 *fieldName = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_NAME(J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *) info))));

				if (NULL != fullyQualifiedClassName) {
					*npeMsg = getMsgWithAllocation(vmThread, "%s.%.*s", fullyQualifiedClassName, J9UTF8_LENGTH(fieldName), J9UTF8_DATA(fieldName));
				}
				j9mem_free_memory(fullyQualifiedClassName);
				break;
			}

			case JBgetfield: {
				UDATA bcCausePos = bytecodeOffset[npePC].first;
				char *npeMsgObjref = NULL;
				if (BYTECODE_BRANCH_TARGET != bcCausePos) {
					computeNPEMsgAtPC(vmThread, romMethod, romClass, bcCausePos, false, &npeMsgObjref, isMethodFlag, temps, bytecodeOffset);
				}

				if (npeFinalFlag) {
					*npeMsg = npeMsgObjref;
				} else {
					UDATA index = PARAM_16(bcCurrentPtr, 1);
					J9ROMConstantPoolItem *info = &constantPool[index];
					J9UTF8 *fieldName = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_NAME(J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *) info))));

					if (NULL == npeMsgObjref) {
						*npeMsg = getMsgWithAllocation(vmThread, "%.*s", J9UTF8_LENGTH(fieldName), J9UTF8_DATA(fieldName));
					} else {
						*npeMsg = getMsgWithAllocation(vmThread, "%s.%.*s", npeMsgObjref, J9UTF8_LENGTH(fieldName), J9UTF8_DATA(fieldName));
						j9mem_free_memory(npeMsgObjref);
					}
					*isMethodFlag = false;
				}
				break;
			}

			case JBputfield: {
				UDATA bcCausePos = bytecodeOffset[npePC].first;
				char *npeMsgObjref = NULL;

				computeNPEMsgAtPC(vmThread, romMethod, romClass, bcCausePos, false, &npeMsgObjref, isMethodFlag, temps, bytecodeOffset);
				if (npeFinalFlag || (NULL == npeMsgObjref)) {
					*npeMsg = npeMsgObjref;
				} else {
					UDATA index = PARAM_16(bcCurrentPtr, 1);
					J9ROMConstantPoolItem *info = &constantPool[index];
					J9UTF8 *fieldName = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_NAME(J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *) info))));

					*npeMsg = getMsgWithAllocation(vmThread, "%s.%.*s", npeMsgObjref, J9UTF8_LENGTH(fieldName), J9UTF8_DATA(fieldName));
					j9mem_free_memory(npeMsgObjref);
				}

				break;
			}

			default:
				Trc_VM_ComputeNPEMsgAtPC_SkippedBC(vmThread, bcCurrent, npeFinalFlag);
				break;
			}
		}

		if (npeFinalFlag) {
			*npeMsg = getCompleteNPEMessage(vmThread, bcCurrentPtr, romClass, *npeMsg, *isMethodFlag);
		}
		Trc_VM_ComputeNPEMsgAtPC_end(vmThread, bcCurrent, npePC, npeFinalFlag, *isMethodFlag, *npeMsg);
	}

	Trc_VM_ComputeNPEMsgAtPC_Exit(vmThread, npePC, npeFinalFlag, *isMethodFlag, *npeMsg);
}

/**
 * Get a fully qualified method name.
 *
 * Note: the caller is responsible for freeing the returned string if it is not NULL.
 *
 * @param[in] vmThread current J9VMThread
 * @param[in] romClass the romClass containing the method
 * @param[in] bcIndex the bytecode byte stream where the method is to be computed
 *
 * @return a char pointer to a fully qualified method name,
 *         NULL if not successful, but keep application exception instead of throwing OOM.
 */
static char*
getFullyQualifiedMethodName(J9VMThread *vmThread, J9ROMClass *romClass, U_8 *bcIndex)
{
	UDATA bc = *bcIndex;
	UDATA index = PARAM_16(bcIndex, 1);
	J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);

	PORT_ACCESS_FROM_VMC(vmThread);

	Trc_VM_GetFullyQualifiedMethodName_Entry(vmThread, bc, index, bcIndex);
	if (JBinvokestaticsplit == bc) {
		index = *(U_16 *)(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(romClass) + index);
	} else if (JBinvokespecialsplit == bc) {
		index = *(U_16 *)(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(romClass) + index);
	}

	J9ROMMethodRef *info = (J9ROMMethodRef *) &constantPool[index];
	J9ROMNameAndSignature *nameAndSig = J9ROMMETHODREF_NAMEANDSIGNATURE(info);
	J9UTF8 *methodName = J9ROMNAMEANDSIGNATURE_NAME(nameAndSig);
	J9UTF8 *methodSig = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
	J9UTF8 *className = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) &constantPool[info->classRefCPIndex]);
	char *fullyQualifiedClassName = convertToJavaFullyQualifiedName(vmThread, className);
	char *methodSigParameters = convertMethodSignature(vmThread, methodSig);
	char *result = NULL;

	if ((NULL != fullyQualifiedClassName) && (NULL != methodSigParameters)) {
		result = getMsgWithAllocation(vmThread, "%s.%.*s%s",
			fullyQualifiedClassName, J9UTF8_LENGTH(methodName), J9UTF8_DATA(methodName), methodSigParameters);
	}
	j9mem_free_memory(fullyQualifiedClassName);
	j9mem_free_memory(methodSigParameters);

	Trc_VM_GetFullyQualifiedMethodName_Exit(vmThread, index, info, fullyQualifiedClassName, methodSigParameters, result);
	return result;
}

/**
 * Get a local name, the actual variable name,
 * parameter# if the temp wasn't changed, or local# if the temp has been changed.
 *
 * Note: the caller is responsible for freeing the returned string if it is not NULL.
 *
 * @param[in] vmThread current J9VMThread
 * @param[in] romMethod the romMethod containing the local
 * @param[in] localVar the local variable index
 * @param[in] bcIndex the bytecode index
 * @param[in] temps the location of bytecode temps supplied via liveStack->stackElements
 *
 * @return a char pointer to a local name, NULL if not successful
 */
static char*
getLocalsName(J9VMThread *vmThread, J9ROMMethod *romMethod, U_16 localVar, UDATA bcIndex, UDATA *temps)
{
	J9MethodDebugInfo *methodInfo = getMethodDebugInfoFromROMMethod(romMethod);
	char *result = NULL;
	bool foundName = false;

	if (NULL != methodInfo) {
		J9VariableInfoWalkState state = {0};
		J9VariableInfoValues *values = variableInfoStartDo(methodInfo, &state);

		while (NULL != values) {
			if ((values->slotNumber == localVar)
				&& (bcIndex >= values->startVisibility)
				&& (bcIndex < (values->startVisibility + values->visibilityLength))
			) {
				if ((NULL != values->signature) && (NULL != values->name)) {
					result = getMsgWithAllocation(vmThread, "%.*s", J9UTF8_LENGTH(values->name), J9UTF8_DATA(values->name));
					foundName = true;
				}
				break;
			}
			values = variableInfoNextDo(&state);
		}
	}

	if (!foundName) {
		/* debug info not available */
		if ((0 == localVar)
			&& !(romMethod->modifiers & J9AccStatic)
		) {
			result = getMsgWithAllocation(vmThread, "this");
		} else {
			UDATA temp = temps[localVar];
			if (BYTECODE_TEMP_CHANGED == temp) {
				/* there was a change to the temp, use <local%d> instead */
				result = getMsgWithAllocation(vmThread, "<local%d>", localVar);
			} else {
				/* there was no change to the temp, use <parameter%d>, and the number was saved within temps[localVar] */
				Trc_VM_GetLocalsName_LocalVar(vmThread, localVar, temps[localVar]);
				result = getMsgWithAllocation(vmThread, "<parameter%d>", temp);
			}
		}
	}

	Trc_VM_GetLocalsName_Exit(vmThread, result, localVar, bcIndex, temps, methodInfo);
	return result;
}

/**
 * Push the bytecode offset onto the simulation stack according to the signature
 *
 * @param[in] vmThread current J9VMThread
 * @param[in] signature class/field signature
 * @param[in,out] stackTop the pointer to the simulation stack top
 * @param[in] bcPos the bytecode offset to be pushed onto the stack
 *
 * @return the stackTop
 */
static UDATA *
pushViaSiganature(J9VMThread *vmThread, U_8 *signature, UDATA *stackTop, UDATA bcPos)
{
	if ('V' != *signature) {
		PUSH(bcPos);
		if (('J' == *signature) || ('D' == *signature)) {
			PUSH(bcPos);
		}
	}
	Trc_VM_PushViaSiganature_Exit(vmThread, *signature, stackTop, bcPos);
	return stackTop;
}

/**
 * Create a message according to the template and incoming arguments
 *
 * Note: the caller is responsible for freeing the returned string if it is not NULL.
 *
 * @param[in] vmThread current J9VMThread
 * @param[in] msgTemplate the message template
 * @param[in] ... arguments to be substituted into the message
 *
 * @return a message string, NULL if not successful
 */
static char *
getMsgWithAllocation(J9VMThread *vmThread, const char *msgTemplate, ...)
{
	va_list args;
	PORT_ACCESS_FROM_VMC(vmThread);

	va_start(args, msgTemplate);
	UDATA msgLen = j9str_vprintf(NULL, 0, msgTemplate, args);
	char *resultMsg = (char *)j9mem_allocate_memory(msgLen, OMRMEM_CATEGORY_VM);
	/* NULL check omitted since j9str_vprintf accepts NULL (as above) */
	j9str_vprintf(resultMsg, msgLen, msgTemplate, args);
	va_end(args);

	Trc_VM_GetMsgWithAllocation_Exit(vmThread, msgTemplate, resultMsg);

	return resultMsg;
}

/**
 * Set bytecodeOffset[bcPos] value to first << BYTECODE_OFFSET_SHIFT + second
 *
 * Note: Usually first is the offset of a bytecode which puts an arrayref/objectref onto the operand stack,
 * 		and the second is the offset of a bytecode which puts an index value onto the operand stack.
 * 		When either first or second is BYTECODE_BRANCH_TARGET which means there are more than one path to put
 * 		such a value onto the stack, a BYTECODE_BRANCH_TARGET will be saved instead.
 *
 * @param[in] bytecodeOffset the array storing the bytecode offset
 * @param[in] bcPos the location of the array bytecodeOffset
 * @param[in] first the first bytecode offset stored at bytecodeOffset[bcPos] higher 16 bits
 * @param[in] second the second bytecode offset stored at bytecodeOffset[bcPos] lower 16 bits
 */
static void
setSrcBytecodeOffset(J9BytecodeOffset *bytecodeOffset, UDATA bcPos, UDATA first, UDATA second)
{
	if (BYTECODE_BRANCH_TARGET == first) {
		bytecodeOffset[bcPos].first = BYTECODE_BRANCH_TARGET;
	} else {
		bytecodeOffset[bcPos].first = (U_32)first;
	}
	if (BYTECODE_BRANCH_TARGET == second) {
		bytecodeOffset[bcPos].second = BYTECODE_BRANCH_TARGET;
	} else {
		bytecodeOffset[bcPos].second = (U_32)second;
	}
}

char*
getNPEMessage(J9NPEMessageData *npeMsgData)
{
	char *npeMsg = NULL;
	if (BCV_SUCCESS == initializeNPEMessageData(npeMsgData)) {
		npeMsg = simulateStack(npeMsgData);
	}

	return npeMsg;
}

/**
 * Simulate stack push/pop with incoming romMethod bytecode until the offset specified via npePC,
 * then invoke computeNPEMsgAtPC() to generate NPE extended message.
 *
 * Note: the caller is responsible for freeing the returned string if it is not NULL.
 *
 * @param[in] npeMsgData - the J9NPEMessageData structure holding romClass/romMethod/npePC
 *
 * @return an extended NPE message or NULL if such a message can't be generated
 */
static char*
simulateStack(J9NPEMessageData *npeMsgData)
{
	char *npeMsg = NULL;
	UDATA npePC = npeMsgData->npePC;
	J9ROMClass *romClass = npeMsgData->romClass;
	J9ROMMethod *romMethod = npeMsgData->romMethod;
	J9VMThread *vmThread = npeMsgData->vmThread;
	U_32 *bytecodeMap = npeMsgData->bytecodeMap;
	J9ROMConstantPoolItem *constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);
	bool checkIfInsideException = (0 != (romMethod->modifiers & J9AccMethodHasExceptionInfo));
	U_8 *bytecode = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	UDATA bytecodeLength = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	J9ExceptionInfo *exceptionData = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
	J9BytecodeOffset *bytecodeOffset = npeMsgData->bytecodeOffset;
	J9BranchTargetStack *liveStack = npeMsgData->liveStack;

	/* required by macro NPEMSG_POP/DROP/RELOAD_LIVESTACK etc. */
	UDATA *stackBase = NULL;
	UDATA *stackTop = NULL;
	UDATA *temps = NULL;
	UDATA temp1 = 0;
	RELOAD_LIVESTACK;

	U_8 *bcIndex = bytecode;
	UDATA pc = 0;
	UDATA currentBytecode = 0;
	UDATA bcPos = 0;

	bool foundFlag = false;
	bool justLoadedStack = false;
	bool wideIndex = false;
	UDATA popCount = 0;

	while (pc < bytecodeLength) {
		bcIndex = bytecode + pc;
		currentBytecode = *bcIndex;
		if (pc == npePC) {
			/* mark the NPE PC is found, need proceed to prepare the data structures while the pc value could be updated later */
			foundFlag = true;
		} else if (JBinvokeinterface2 == currentBytecode) {
			if ((pc + 2) == npePC) {
				foundFlag = true;
			}
		}
		bcPos = pc;

		/* If exception start PC, or possible branch to inside an exception range,
		 * copy the existing stack shape into the exception stack.
		 */
		if ((bytecodeMap[pc] & BRANCH_EXCEPTION_START) || (justLoadedStack && checkIfInsideException)) {
			UDATA exception = 0;
			UDATA *originalStackTop = NULL;
			UDATA originalStackZeroEntry = 0;
			J9ExceptionHandler *handler = J9EXCEPTIONINFO_HANDLERS(exceptionData);

			SAVE_STACKTOP(liveStack, stackTop);
			/* Save the current liveStack element zero,
			 * Reset the stack pointer to push the exception on the empty stack
			 */
			originalStackTop = stackTop;
			originalStackZeroEntry = liveStack->stackElements[liveStack->stackBaseIndex];
			for (exception = 0; exception < (UDATA) exceptionData->catchCount; exception++) {
				/* find the matching branch target, and copy/merge the stack with the exception object */
				if ((pc >= handler->startPC) && (pc < handler->endPC)) {
					/* Empty the stack */
					stackTop = &(liveStack->stackElements[liveStack->stackBaseIndex]);
					PUSH(bcPos); /* the equivalent stack operation of PUSH the exception object */
					SAVE_STACKTOP(liveStack, stackTop);
					if (BCV_SUCCESS != copyToTargetStack(npeMsgData, handler->handlerPC)) {
						goto _internalError;
					}
				}
				handler++;
			}
			/* Restore liveStack */
			liveStack->stackElements[liveStack->stackBaseIndex] = originalStackZeroEntry;
			stackTop = originalStackTop;
			SAVE_STACKTOP(liveStack, stackTop);
		}

		/* Format: 8bits action, 4bits type Y (type 2), 4bits type X (type 1) */
		UDATA typeTableEntry = (UDATA) J9JavaBytecodeVerificationTable[currentBytecode];
		UDATA action = typeTableEntry >> 8;
		UDATA type1 = decodeTable[typeTableEntry & 0xF];
		UDATA type2 = decodeTable[(typeTableEntry >> 4) & 0xF];

		/* Merge all branchTargets encountered */
		if (bytecodeMap[pc] & BRANCH_TARGET) {
			/* Skip the stack just loaded */
			if (!justLoadedStack) {
				SAVE_STACKTOP(liveStack, stackTop);
				Trc_VM_SimulateStack_BranchTarget(vmThread, pc, bytecodeMap[pc], bcPos);
				if (BCV_SUCCESS != copyToTargetStack(npeMsgData, bcPos)) {
					goto _internalError;
				}
				goto _checkFinished;
			}
		}
		justLoadedStack = false;

		/* high nybble = branch action, low nybble = instruction size */
		pc += (J9JavaInstructionSizeAndBranchActionTable[currentBytecode] & 7);

		/* Layout:
		 * bit 7:	special
		 * bit 6:	int/object pushes 0=int, 1=object.
		 * bit 4-5:	pushes.
		 * bit 0-3: pops
		 */
		popCount = JavaStackActionTable[currentBytecode] & 0x07;
		if ((stackTop - popCount) < stackBase) {
			Trc_VM_SimulateStack_MalformedStack_One(vmThread, stackTop, popCount, stackBase);
			goto _internalError;
		}

		switch (action) {
		case RTV_ARRAY_FETCH_PUSH: {
			UDATA indexBytecodeOffset = NPEMSG_POP; /* the offset of the bytecode putting index onto the operand stack */
			UDATA arrayrefBytecodeOffset = NPEMSG_POP; /* the offset of the bytecode putting arrayref onto the operand stack */
			setSrcBytecodeOffset(bytecodeOffset, bcPos, arrayrefBytecodeOffset, indexBytecodeOffset);
		}	/* Fall through case !!! */

		case RTV_WIDE_LOAD_TEMP_PUSH:	/* Fall through case !!! */
		case RTV_LOAD_TEMP_PUSH:		/* Fall through case !!! */
		case RTV_PUSH_CONSTANT:			/* Fall through case !!! */
		case RTV_PUSH_CONSTANT_POOL_ITEM:

		_pushConstant:
			PUSH(bcPos);
			if ((type1 & BCV_WIDE_TYPE_MASK)
				|| (JBldc2lw == currentBytecode)
				|| (JBldc2dw == currentBytecode)
			) {
				PUSH(bcPos);
			}
			break;

		case RTV_WIDE_POP_STORE_TEMP:
			wideIndex = true;
			/* Fall through case !!! */

		case RTV_POP_STORE_TEMP: {
			UDATA index = type2 & 0x7;
			if (0 == type2) {
				index = PARAM_8(bcIndex, 1);
				if (wideIndex) {
					index = PARAM_16(bcIndex, 1);
					wideIndex = false;
				}
			}
			if (BCV_GENERIC_OBJECT == type1) {
				/* astore family */
				NPEMSG_DROP(1);
				temps[index] = BYTECODE_TEMP_CHANGED;
			} else {
				NPEMSG_DROP(popCount);
				if (type1 & BCV_WIDE_TYPE_MASK) {
					temps[index + 1] = BYTECODE_TEMP_CHANGED;
				}
				temps[index] = BYTECODE_TEMP_CHANGED;
			}
			break;
		}

		case RTV_POP_X_PUSH_X:
			popCount = 0;
			if (0 != type2) {
				/* shift family */
				popCount = 1;
			}	/* Fall through case !!! */

		case RTV_ARRAY_STORE:
			/*  *(stackTop - popCount + 1) - the offset of the bytecode putting index onto the operand stack */
			/* *(stackTop - popCount) - the offset of the bytecode putting arrayref onto the operand stack */
			setSrcBytecodeOffset(bytecodeOffset, bcPos, *(stackTop - popCount), *(stackTop - popCount + 1));
			NPEMSG_DROP(popCount);
			break;

		case RTV_POP_X_PUSH_Y:
			type1 = type2;
			/* Fall through case !!! */

		case RTV_POP_2_PUSH:
			/* the offset of the bytecode putting the value onto the operand stack is *(stackTop - 1) */
			setSrcBytecodeOffset(bytecodeOffset, bcPos, *(stackTop - 1), 0);
			NPEMSG_DROP(popCount);
			goto _pushConstant;

		case RTV_BRANCH: {
			UDATA target = 0;

			popCount = type2 & 0x07;
			stackTop -= popCount;
			if (JBgotow == currentBytecode) {
				I_32 offset32 = (I_32) PARAM_32(bcIndex, 1);
				target = bcPos + offset32;
			} else {
				I_16 offset16 = (I_16) PARAM_16(bcIndex, 1);
				target = bcPos + offset16;
			}
			SAVE_STACKTOP(liveStack, stackTop);
			if (BCV_SUCCESS != copyToTargetStack(npeMsgData, target)) {
				goto _internalError;
			}
			/* Unconditional branch (goto family) */
			if (0 == popCount) {
				goto _checkFinished;
			}
			break;
		}

		case RTV_RETURN:
			goto _checkFinished;

		case RTV_STATIC_FIELD_ACCESS: {
			UDATA index = PARAM_16(bcIndex, 1);
			J9ROMConstantPoolItem *info = &constantPool[index];
			J9UTF8 *fieldSig = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *) info))));
			if (currentBytecode >= JBgetfield) {
				/* the offset of the bytecode putting the field receiver onto the operand stack is *(stackTop - 1) */
				setSrcBytecodeOffset(bytecodeOffset, bcPos, *(stackTop - 1), 0);
				/* field bytecode receiver */
				NPEMSG_DROP(1);
			}
			if (currentBytecode & 1) {
				if (JBputfield == currentBytecode) {
					setSrcBytecodeOffset(bytecodeOffset, bcPos, *(stackTop - 1), 0);
				}
				/* JBputfield/JBpustatic - odd currentBytecode's */
				NPEMSG_DROP(1);
				if ((*J9UTF8_DATA(fieldSig) == 'D') || (*J9UTF8_DATA(fieldSig) == 'J')) {
					NPEMSG_DROP(1);
				}
			} else {
				/* JBgetfield/JBgetstatic - even currentBytecode's */
				stackTop = pushViaSiganature(vmThread, J9UTF8_DATA(fieldSig), stackTop, bcPos);
			}
			break;
		}

		case RTV_SEND: {
			J9UTF8 *classSig = NULL;

			Trc_VM_simulateStack_RTVSEND_bcIndex(vmThread, currentBytecode, bcIndex);
			if (JBinvokeinterface2 == currentBytecode) {
				/* Set to point to JBinvokeinterface. */
				bcIndex += 2;
				Trc_VM_simulateStack_RTVSEND_bcIndex2(vmThread, bcIndex);
			}
			UDATA index = PARAM_16(bcIndex, 1);
			Trc_VM_simulateStack_RTVSEND_index(vmThread, index);
			if (JBinvokestaticsplit == currentBytecode) {
				index = *(U_16 *)(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(romClass) + index);
				Trc_VM_simulateStack_RTVSEND_JBinvokestaticsplit_index(vmThread, index);
			} else if (JBinvokespecialsplit == currentBytecode) {
				index = *(U_16 *)(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(romClass) + index);
				Trc_VM_simulateStack_RTVSEND_JBinvokespecialsplit_index(vmThread, index);
			}
			if (JBinvokedynamic == currentBytecode) {
				J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
				classSig = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(SRP_PTR_GET(callSiteData + index, J9ROMNameAndSignature*))));
				Trc_VM_simulateStack_RTVSEND_JBinvokedynamic_classSig(vmThread, J9UTF8_LENGTH(classSig), J9UTF8_DATA(classSig));
			} else {
				J9ROMConstantPoolItem *info = &constantPool[index];
				classSig = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info))));
				Trc_VM_simulateStack_RTVSEND_others_classSig(vmThread, J9UTF8_LENGTH(classSig), J9UTF8_DATA(classSig));
			}
			stackTop -= getSendSlotsFromSignature(J9UTF8_DATA(classSig));

			if ((JBinvokestatic != currentBytecode)
				&& (JBinvokedynamic != currentBytecode)
				&& (JBinvokestaticsplit != currentBytecode)
			) {
				if ((JBinvokespecial == currentBytecode)
					|| (JBinvokespecialsplit == currentBytecode)
				) {
					setSrcBytecodeOffset(bytecodeOffset, bcPos, *(stackTop - 1), 0);
				} else { /* virtual or interface */
					/* the offset of the bytecode putting the objectref onto the operand stack is *(stackTop - 1) */
					setSrcBytecodeOffset(bytecodeOffset, bcPos, *(stackTop - 1), 0);
					if (JBinvokeinterface2 == currentBytecode) {
						/* setSrcBytecodeOffset for JBinvokeinterface as well */
						setSrcBytecodeOffset(bytecodeOffset + 2, bcPos, *(stackTop - 1), 0);
					}
				}
				NPEMSG_DROP(1);
			}

			U_8 *signature = J9UTF8_DATA(classSig);
			while (*signature++ != ')');
			stackTop = pushViaSiganature(vmThread, signature, stackTop, bcPos);

			break;
		}

		case RTV_PUSH_NEW:
			switch (currentBytecode) {
			case JBnew:	/* Fall through case !!! */
			case JBnewdup:
				break;

			case JBnewarray:	/* Fall through case !!! */
			case JBanewarray:
				NPEMSG_DROP(1);	/* pop the size of the array */
				break;

			case JBmultianewarray: {
				IDATA i1 = PARAM_8(bcIndex, 3);
				NPEMSG_DROP(i1);
				if (stackTop < stackBase) {
					Trc_VM_SimulateStack_MalformedStack_Two(vmThread, stackTop, stackBase);
					goto _internalError;
				}
				break;
			}

			default:
				Trc_VM_SimulateStack_RtvPushNew_SkippedBC(vmThread, currentBytecode);
				break;
			}
			PUSH(bcPos);
			break;

		case RTV_MISC:
			switch (currentBytecode) {
			case JBathrow:
				/* the offset of the bytecode putting the objectref onto the operand stack is *(stackTop - 1) */
				setSrcBytecodeOffset(bytecodeOffset, bcPos, *(stackTop - 1), 0);
				goto _checkFinished;

			case JBarraylength:	/* Fall through case !!! */
			case JBinstanceof:
				/* the offset of the bytecode putting the objectref onto the operand stack is *(stackTop - 1) */
				setSrcBytecodeOffset(bytecodeOffset, bcPos, *(stackTop - 1), 0);
				NPEMSG_DROP(1);
				PUSH(bcPos);
				break;

			case JBtableswitch:	/* Fall through case !!! */
			case JBlookupswitch: {
				IDATA i1 = 0;
				I_32 offset32 = 0;

				NPEMSG_DROP(1);
				UDATA index = (UDATA) ((4 - (pc & 3)) & 3);	/* consume padding */
				pc += index;
				bcIndex += index;
				pc += 8;
				offset32 = (I_32) PARAM_32(bcIndex, 1);
				bcIndex += 4;
				UDATA target = offset32 + bcPos;
				SAVE_STACKTOP(liveStack, stackTop);
				if (BCV_SUCCESS != copyToTargetStack(npeMsgData, target)) {
					goto _internalError;
				}

				if (JBtableswitch == currentBytecode) {
					i1 = (I_32) PARAM_32(bcIndex, 1);
					bcIndex += 4;
					pc += 4;
					IDATA i2 = (I_32) PARAM_32(bcIndex, 1);
					bcIndex += 4;
					pc += ((I_32)i2 - (I_32)i1 + 1) * 4;
					/* Add the table switch destinations in reverse order to more closely mimic the order that people
					   (ie: the TCKs) expect you to load classes */
					bcIndex += (((I_32)i2 - (I_32)i1) * 4);	/* point at the last table switch entry */
					/* Count the entries */
					i2 = (I_32)i2 - (I_32)i1 + 1;
					for (i1 = 0; (I_32)i1 < (I_32)i2; i1++) {
						offset32 = (I_32) PARAM_32(bcIndex, 1);
						bcIndex -= 4;	/* back up to point at the previous table switch entry */
						target = offset32 + bcPos;
						if (BCV_SUCCESS != copyToTargetStack(npeMsgData, target)) {
							goto _internalError;
						}
					}
				} else {
					IDATA i2 = (I_32) PARAM_32(bcIndex, 1);
					bcIndex += 4;
					pc += (I_32)i2 * 8;
					for (i1 = 0; (I_32)i1 < (I_32)i2; i1++) {
						bcIndex += 4;
						offset32 = (I_32) PARAM_32(bcIndex, 1);
						bcIndex += 4;
						target = offset32 + bcPos;
						if (BCV_SUCCESS != copyToTargetStack(npeMsgData, target)) {
							goto _internalError;
						}
					}
				}
				goto _checkFinished;
			}

			case JBmonitorenter:	/* Fall through case !!! */
			case JBmonitorexit:
				/* the offset of the bytecode putting the objectref onto the operand stack is *(stackTop - 1) */
				setSrcBytecodeOffset(bytecodeOffset, bcPos, *(stackTop - 1), 0);
				NPEMSG_DROP(1);
				break;
			}
			break;

		case RTV_POP_2_PUSH_INT:
			NPEMSG_DROP(popCount);
			PUSH(bcPos);
			break;

		case RTV_BYTECODE_POP:
		case RTV_BYTECODE_POP2:
			NPEMSG_DROP(popCount);
			break;

		case RTV_BYTECODE_DUP: {
			UDATA type = NPEMSG_POP;
			PUSH(type);
			PUSH(bcPos);
			/* the offset of the bytecode putting the value onto the operand stack is type (POPed from stack) */
			setSrcBytecodeOffset(bytecodeOffset, bcPos, type, 0);
			break;
		}

		case RTV_BYTECODE_DUPX1: {
			UDATA type = NPEMSG_POP;
			temp1 = NPEMSG_POP;
			PUSH(type);
			PUSH(temp1);
			PUSH(bcPos);
			setSrcBytecodeOffset(bytecodeOffset, bcPos, type, 0);
			break;
		}

		case RTV_BYTECODE_DUPX2: {
			UDATA type = NPEMSG_POP;
			temp1 = NPEMSG_POP;
			UDATA temp2 = NPEMSG_POP;
			PUSH(type);
			PUSH(temp2);
			PUSH(temp1);
			PUSH(bcPos);
			setSrcBytecodeOffset(bytecodeOffset, bcPos, type, 0);
			break;
		}

		case RTV_BYTECODE_DUP2: {
			temp1 = NPEMSG_POP;
			UDATA temp2 = NPEMSG_POP;
			PUSH(temp2);
			PUSH(temp1);
			PUSH(bcPos);
			PUSH(bcPos);
			setSrcBytecodeOffset(bytecodeOffset, bcPos, temp1, 0);
			break;
		}

		case RTV_BYTECODE_DUP2X1: {
			UDATA type = NPEMSG_POP;
			temp1 = NPEMSG_POP;
			UDATA temp2 = NPEMSG_POP;
			PUSH(temp1);
			PUSH(type);
			PUSH(temp2);
			PUSH(bcPos);
			PUSH(bcPos);
			setSrcBytecodeOffset(bytecodeOffset, bcPos, type, 0);
			break;
		}

		case RTV_BYTECODE_DUP2X2: {
			UDATA type = NPEMSG_POP;
			temp1 = NPEMSG_POP;
			UDATA temp2 = NPEMSG_POP;
			UDATA temp3 = NPEMSG_POP;
			PUSH(temp1);
			PUSH(type);
			PUSH(temp3);
			PUSH(temp2);
			PUSH(bcPos);
			PUSH(bcPos);
			setSrcBytecodeOffset(bytecodeOffset, bcPos, type, 0);
			break;
		}

		case RTV_BYTECODE_SWAP: {
			UDATA type = NPEMSG_POP;
			temp1 = NPEMSG_POP;
			PUSH(type);
			PUSH(temp1);
			break;
		}

		case RTV_UNIMPLEMENTED:
			Trc_VM_SimulateStack_UnimplementedBC(vmThread, currentBytecode, pc, action);
			goto _internalError;

		/* JBnop - 0 */
		default:
			Trc_VM_SimulateStack_SkippedBC(vmThread, currentBytecode, pc, action);
			break;
		}

		if (foundFlag) {
			break;
		} else {
			continue;
		}

_checkFinished:
		justLoadedStack = true;
		if (npeMsgData->unwalkedQueueHead != npeMsgData->unwalkedQueueTail) {
			pc = npeMsgData->unwalkedQueue[npeMsgData->unwalkedQueueHead++];
			npeMsgData->unwalkedQueueHead %= (npeMsgData->unwalkedQueueSize / sizeof(UDATA));
			if ((bytecodeMap[pc] & BRANCH_ON_UNWALKED_QUEUE) == 0) {
				Trc_VM_SimulateStack_AlreadyWalked(vmThread, pc, bytecodeMap[pc], currentBytecode, npePC);
				goto _checkFinished; /* this is already walked, try next one in the queue */
			}
			bytecodeMap[pc] &= ~BRANCH_ON_UNWALKED_QUEUE; /* clear this bit */
			bcIndex = bytecode + pc;

			UDATA stackIndex = bytecodeMap[pc] >> BRANCH_INDEX_SHIFT;
			J9BranchTargetStack *branch = NPEMSG_INDEX_STACK (stackIndex);

			Trc_VM_SimulateStack_JustLoadedStack(vmThread, pc, bytecodeMap[pc], stackIndex, bcIndex, currentBytecode, npePC);
			memcpy((UDATA *)liveStack, (UDATA *)branch, (branch->stackTopIndex + BCV_TARGET_STACK_HEADER_UDATA_SIZE) * sizeof(UDATA));
			RELOAD_LIVESTACK;
		}
	}

	if (foundFlag) {
		bool isMethodFlag = false;

		computeNPEMsgAtPC(vmThread, romMethod, romClass, npePC, true, &npeMsg, &isMethodFlag, temps, bytecodeOffset);
	}

_internalError: /* required by macro POP etc. */
	Trc_VM_SimulateStack_Exit(vmThread, npeMsg);
	return npeMsg;
}

/*
 * Decorated with DEBUG_BCV (not enabled by default)
 * Dump the stack elements up to the stackTopIndex
 *
 * @param[in] stack the stack to be dumped
 */
static void
dumpStack(J9BranchTargetStack *stack)
{
#if defined(DEBUG_BCV)
	IDATA indexTemp = 0;
	for (indexTemp = 0; indexTemp < stack->stackTopIndex; indexTemp++) {
		printf(" 0x%lx", stack->stackElements[indexTemp]);
	}
	printf("\n");
#endif /* defined(DEBUG_BCV) */
}

/*
 * Copy current live stack to a target stack specified via bytecodeMap[target] >> BRANCH_INDEX_SHIFT
 *
 * @param[in] npeMsgData the J9NPEMessageData structure holding romClass/romMethod/npePC
 * @param[in] target the target stack to be copied from the live stack
 *
 * @return IDATA return BCV_SUCCESS on success, BCV_ERR_INTERNAL_ERROR for any unexpected error
 */
static IDATA
copyToTargetStack(J9NPEMessageData *npeMsgData, UDATA target)
{
	J9VMThread *vmThread = npeMsgData->vmThread;
	U_32 *bytecodeMap = npeMsgData->bytecodeMap;
	J9BranchTargetStack *liveStack = (J9BranchTargetStack *)npeMsgData->liveStack;
	UDATA stackIndex = bytecodeMap[target] >> BRANCH_INDEX_SHIFT;
	J9BranchTargetStack *targetStack = NPEMSG_INDEX_STACK(stackIndex);
	IDATA result = BCV_SUCCESS;

	if (TrcEnabled_Trc_VM_CopyToTargetStack_DumpLiveStack) {
		Trc_VM_CopyToTargetStack_DumpLiveStack(vmThread, liveStack->stackBaseIndex, liveStack->stackTopIndex);
		dumpStack(liveStack);
	}
	if (-1 == targetStack->stackBaseIndex) {
		/* Target location does not have a stack, so give the target our current stack */
		Trc_VM_CopyToTargetStack_Copy(vmThread, npeMsgData->unwalkedQueueTail, target, bytecodeMap[target]);

		/* Put it into unwalkedQueue */
		npeMsgData->unwalkedQueue[npeMsgData->unwalkedQueueTail++] = target;
		npeMsgData->unwalkedQueueTail %= (npeMsgData->unwalkedQueueSize / sizeof(UDATA));
		bytecodeMap[target] |= BRANCH_ON_UNWALKED_QUEUE;

		UDATA numCpy = (liveStack->stackTopIndex + BCV_TARGET_STACK_HEADER_UDATA_SIZE) * sizeof(UDATA);
		memcpy(targetStack, liveStack, numCpy);
	} else {
		if (TrcEnabled_Trc_VM_CopyToTargetStack_DumpTargetStack) {
			Trc_VM_CopyToTargetStack_DumpTargetStack(vmThread, targetStack->stackBaseIndex, targetStack->stackTopIndex);
			dumpStack(targetStack);
		}
		if ((liveStack->stackBaseIndex == targetStack->stackBaseIndex)
			&& (liveStack->stackTopIndex == targetStack->stackTopIndex)
		) {
			IDATA liveStackBaseIndexTemp = liveStack->stackBaseIndex;

			for (liveStackBaseIndexTemp = liveStack->stackBaseIndex; liveStackBaseIndexTemp < liveStack->stackTopIndex; liveStackBaseIndexTemp++) {
				UDATA liveStackTmp = liveStack->stackElements[liveStackBaseIndexTemp];
				UDATA targetStackTmp = targetStack->stackElements[liveStackBaseIndexTemp];

				if (liveStackTmp != targetStackTmp) {
					liveStack->stackElements[liveStackBaseIndexTemp] = BYTECODE_BRANCH_TARGET;
					targetStack->stackElements[liveStackBaseIndexTemp] = BYTECODE_BRANCH_TARGET;
					Trc_VM_CopyToTargetStack_BranchTarget(vmThread, liveStackBaseIndexTemp, liveStack->stackElements[liveStackBaseIndexTemp], targetStack->stackElements[liveStackBaseIndexTemp]);
				} else {
					targetStack->stackElements[liveStackBaseIndexTemp] = liveStack->stackElements[liveStackBaseIndexTemp];
				}
			}

			UDATA numCpy = (liveStack->stackBaseIndex + BCV_TARGET_STACK_HEADER_UDATA_SIZE) * sizeof(UDATA);
			memcpy(targetStack, liveStack, numCpy);
		} else {
			Trc_VM_CopyToTargetStack_Mismatched_StackShape(vmThread, liveStack->stackBaseIndex, liveStack->stackTopIndex, targetStack->stackBaseIndex, targetStack->stackTopIndex);
			result = BCV_ERR_INTERNAL_ERROR;
		}
		if (TrcEnabled_Trc_VM_CopyToTargetStack_DumpTargetStack) {
			dumpStack(liveStack);
			dumpStack(targetStack);
		}
	}
	Trc_VM_CopyToTargetStack_Exit(vmThread, target, bytecodeMap[target], targetStack, liveStack, targetStack->stackTopIndex, liveStack->stackTopIndex);

	return result;
}

/*
 * Initialize the stack with the method signature.
 * The arguments are pushed as current argCount, if there is no variable name information,
 * and if it is changed to BYTECODE_TEMP_CHANGED later, a 'locali' message will be generated
 * to denote this slot, otherwise, a 'parameteri' message is applied instead.
 *
 * @param[in] vmThread current J9VMThread
 * @param[in] romMethod the current ROM method
 * @param[in] stackTopPtr a pointer to a pointer to the start of the stack (grows towards larger values)
 */
static void
initStackFromMethodSignature(J9VMThread *vmThread, J9ROMMethod *romMethod, UDATA **stackTopPtr)
{
	const UDATA argMax = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod);
	U_8 *args = J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod));
	UDATA *stackTop = *stackTopPtr;
	UDATA argCount = 0;
	UDATA i = 0;
	bool isArray = false;

	/* if this is a virtual method, push the receiver */
	if ((!(romMethod->modifiers & J9AccStatic)) && (argMax > 0)) {
		PUSH(BYTECODE_TEMP_CHANGED); /* push BYTECODE_TEMP_CHANGED as the receiver */
	}

	/* Walk the signature of the method to determine the arg shape */
	for (i = 1; args[i] != ')'; i++) {
		argCount++;
		if (argCount > argMax) { /* match verifier behavior */
			break;
		}

		while ('[' == args[i]) {
			i++;
			isArray = true;
		}
		PUSH(argCount); /* push the argCount as the argument */
		if ('L' == args[i]) {
			i++;
			while (';' != args[i]) {
				i++;
			}
		} else {
			if (!isArray && (('J' == args[i]) || ('D' == args[i]))) {
				PUSH(argCount);
			}
		}
	}

	for (i = 0; i < J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod); i++) {
		/* Now push a bunch of variables which are not arguments */
		PUSH(BYTECODE_TEMP_CHANGED);
	}
	Trc_VM_InitStackFromMethodSignature_Result(vmThread, *stackTopPtr, stackTop, argCount);
	*stackTopPtr = stackTop;
}

}
