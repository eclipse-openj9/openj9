/*******************************************************************************
 * Copyright (c) 1991, 2020 IBM Corp. and others
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

#include "bcvcfr.h"
#include "j9bcvnls.h"

#include "cfreader.h"
#include "bcnames.h"
#include "pcstack.h"
#include "j9protos.h"
#include "j9consts.h"
#include "omrthread.h"
#include "jvminit.h"
#include "vrfyconvert.h"
#include "bcverify.h"
#include "bcverify_internal.h"
#include "vrfytbl.h"
#include "j9cp.h"

#include "ut_j9bcverify.h"


static IDATA verifyBytecodes (J9BytecodeVerificationData * verifyData);
static IDATA matchStack (J9BytecodeVerificationData * verifyData, J9BranchTargetStack *liveStack,  J9BranchTargetStack * targetStack, UDATA inlineMatch);
static IDATA findAndMatchStack (J9BytecodeVerificationData *verifyData, IDATA targetPC, IDATA currentPC);
static IDATA verifyExceptions (J9BytecodeVerificationData *verifyData);
static J9BranchTargetStack * nextStack (J9BytecodeVerificationData *verifyData, UDATA *nextMapIndex, IDATA *nextStackPC);
static IDATA nextExceptionStart (J9BytecodeVerificationData *verifyData, J9ROMMethod *romMethod, IDATA lastPC);
static void storeArgumentErrorData (J9BytecodeVerificationData * verifyData, U_32 errorCurrentFramePosition, U_16 errorArgumentIndex);
static void storeMethodInfo (J9BytecodeVerificationData * verifyData, J9UTF8* errorClassString, J9UTF8* errorMethodString, J9UTF8* errorSignatureString, IDATA currentPC);

/*
 * returns J9Class * on success
 * returns NULL on error
 * 	set reasonCode to BCV_ERR_INSUFFICIENT_MEMORY on OOM
 */
J9Class *
j9rtv_verifierGetRAMClass( J9BytecodeVerificationData *verifyData, J9ClassLoader* classLoader, U_8 *className, UDATA nameLength, IDATA *reasonCode)
{
	J9Class *found = NULL;
	JavaVM* jniVM = (JavaVM*)verifyData->javaVM;
	J9ThreadEnv* threadEnv = NULL;
	J9JavaVM *vm = verifyData->vmStruct->javaVM;
	(*jniVM)->GetEnv(jniVM, (void**)&threadEnv, J9THREAD_VERSION_1_1);

#ifdef J9VM_THR_PREEMPTIVE
	threadEnv->monitor_enter(vm->classTableMutex);
#endif

	/* Sniff the class table to see if already loaded */
	Trc_RTV_j9rtv_verifierGetRAMClass_Entry(verifyData->vmStruct, classLoader, nameLength, className);
	found = vm->internalVMFunctions->hashClassTableAt (classLoader, className, nameLength);

#ifdef J9VM_THR_PREEMPTIVE
	threadEnv->monitor_exit(vm->classTableMutex);
#endif

	if (!found) {
		/* Set reasonCode to BCV_ERR_CLASS_RELATIONSHIP_RECORD_REQUIRED if -XX:+ClassRelationshipVerifier is used, the class is not already loaded and if the classfile major version is at least 51 (Java 7) */
		if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags2, J9_EXTENDED_RUNTIME2_ENABLE_CLASS_RELATIONSHIP_VERIFIER) && (verifyData->romClass->majorVersion >= CFR_MAJOR_VERSION_REQUIRING_STACKMAPS)) {
			*reasonCode = BCV_ERR_CLASS_RELATIONSHIP_RECORD_REQUIRED;
			return NULL;
		} else {
			J9BytecodeVerificationData savedVerifyData;
			UDATA *currentAlloc;
			UDATA *internalBufferStart;
			UDATA *internalBufferEnd;
			J9VMThread *tmpVMC = verifyData->vmStruct;

			Trc_RTV_j9rtv_verifierGetRAMClass_notFound(verifyData->vmStruct);

			/* Nest class loading */
			memcpy(&savedVerifyData, verifyData, sizeof(savedVerifyData));
			verifyData->vmStruct = NULL;

			if (BCV_ERR_INSUFFICIENT_MEMORY == allocateVerifyBuffers (tmpVMC->javaVM->portLibrary, verifyData)) {
				/* returning BCV_ERR_INSUFFICIENT_MEMORY for OOM condition */
				Trc_RTV_j9rtv_verifierGetRAMClass_OutOfMemoryException(verifyData->vmStruct, classLoader, nameLength, className);
				*reasonCode = BCV_ERR_INSUFFICIENT_MEMORY;
				return NULL;
			}

#ifdef J9VM_THR_PREEMPTIVE
			threadEnv->monitor_exit(verifyData->verifierMutex);
#endif

			/* Find the requested class, fully loading it, but not initializing it. */

			found = tmpVMC->javaVM->internalVMFunctions->internalFindClassUTF8(
				tmpVMC,
				className,
				nameLength,
				classLoader,
				J9_FINDCLASS_FLAG_THROW_ON_FAIL);

			if (NULL == found) {
				*reasonCode = BCV_ERR_INACCESSIBLE_CLASS;
			}

#ifdef J9VM_THR_PREEMPTIVE
			/*
			 * Note: if locking both verifierMutex and classTableMutex, they must be entered in that order (CMVC 186043).
			 */

			threadEnv->monitor_enter(verifyData->verifierMutex);
#endif

			freeVerifyBuffers (tmpVMC->javaVM->portLibrary, verifyData);

			/* The currentAlloc, internalBufferStart, internalBufferEnd fields are NOT nested */
			/* used in bcvalloc/bcvfree - avoid hammering it */
			/* This should probably be moved out of the struct, but where? - split the struct by scope */
			currentAlloc = verifyData->currentAlloc;
			internalBufferStart = verifyData->internalBufferStart;
			internalBufferEnd = verifyData->internalBufferEnd;

			memcpy(verifyData, &savedVerifyData, sizeof(savedVerifyData));

			verifyData->currentAlloc = currentAlloc;
			verifyData->internalBufferStart = internalBufferStart;
			verifyData->internalBufferEnd = internalBufferEnd;
		} 
	} else {
		Trc_RTV_j9rtv_verifierGetRAMClass_found(verifyData->vmStruct);
	}

	Trc_RTV_j9rtv_verifierGetRAMClass_Exit(verifyData->vmStruct);

	return found;
}


/*
 * returns BCV_SUCCESS on success
 * returns BCV_ERR_INSUFFICIENT_MEMORY on OOM
 * returns BCV_ERR_INTERNAL_ERROR on error
 */
IDATA
j9rtv_verifyBytecodes (J9BytecodeVerificationData *verifyData)
{
	J9ROMClass *romClass = verifyData->romClass;
	J9ROMMethod *romMethod = verifyData->romMethod;

	UDATA oldState = verifyData->vmStruct->omrVMThread->vmState;
	IDATA result;

	verifyData->vmStruct->omrVMThread->vmState = J9VMSTATE_RTVERIFY;

	Trc_RTV_j9rtv_verifyBytecodes_Entry(verifyData->vmStruct, 
			(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)),
			J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)));

	/* Go walk the bytecodes */
	result = verifyBytecodes (verifyData);

	if ((result == BCV_SUCCESS) && (romMethod->modifiers & CFR_ACC_HAS_EXCEPTION_INFO)) {
		result = verifyExceptions (verifyData);

		if (BCV_SUCCESS != result) {
			if (BCV_ERR_INSUFFICIENT_MEMORY == result) {
				Trc_RTV_j9rtv_verifyBytecodes_OutOfMemoryException(verifyData->vmStruct,
							(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(romClass)),
							J9UTF8_DATA(J9ROMCLASS_CLASSNAME(romClass)));

				verifyData->errorModule = J9NLS_BCV_ERR_VERIFY_OUT_OF_MEMORY__MODULE;
				verifyData->errorCode = J9NLS_BCV_ERR_VERIFY_OUT_OF_MEMORY__ID;
			} else {
				verifyData->errorModule = J9NLS_BCV_ERR_NOT_THROWABLE__MODULE;
				verifyData->errorCode = J9NLS_BCV_ERR_NOT_THROWABLE__ID;
			}
		}
	}

	if (BCV_SUCCESS != result) {
		Trc_RTV_j9rtv_verifyBytecodes_VerifyError(verifyData->vmStruct, verifyData->errorCode, verifyData->errorPC);
	}

	verifyData->vmStruct->omrVMThread->vmState = oldState;
	Trc_RTV_j9rtv_verifyBytecodes_Exit(verifyData->vmStruct);

	return result;
}


/* 
	REMOVED: ( Check for backwards branching and if found, look for uninitialized objects. )
	Find the target stack and then match.
	This is only called to match out-of-line stacks - branches and switches.
	return BCV_SUCCESS on finding matching stack
	return BCV_FAIL on stack mismatch
	return BCV_ERR_INSUFFICIENT_MEMORY on OOM
*/

static IDATA 
findAndMatchStack (J9BytecodeVerificationData *verifyData, IDATA targetPC, IDATA currentPC)
{
	U_32 *bytecodeMap = verifyData->bytecodeMap;
	J9BranchTargetStack *liveStack = (J9BranchTargetStack *) verifyData->liveStack;
	J9BranchTargetStack *targetStack;
	UDATA stackIndex;
	IDATA rc = BCV_SUCCESS;

	Trc_RTV_findAndMatchStack_Entry(verifyData->vmStruct, currentPC, targetPC);

	if (bytecodeMap[targetPC] & BRANCH_TARGET) {
		stackIndex = bytecodeMap[targetPC] >> BRANCH_INDEX_SHIFT;
		targetStack = BCV_INDEX_STACK (stackIndex);

		/* backwards branch? */
		if (targetPC < currentPC) {
			/* Ensure we never backwards branch to code that has an initialized this when this code doesn't.
			 * Remember: the verifier does a *linear* walk of the bytecodes.
			 */
			if (TRUE == liveStack->uninitializedThis) {
				if (TRUE != targetStack->uninitializedThis) {
					rc = BCV_FAIL;
					goto exit;
				}
			}
		}

		/* Never considered an inline match if we perform a find */
		rc = matchStack (verifyData, liveStack, targetStack, FALSE);
	} else {
		/* failed to find map */
		Trc_RTV_findAndMatchStack_StackNotFound(verifyData->vmStruct, 
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				targetPC);
		rc = BCV_FAIL;
		verifyData->errorDetailCode = BCV_ERR_EXPECT_STACKMAP_FRAME;
		/* Set the branch target PC value to show up in the error message framework */
		verifyData->errorTempData = targetPC;
	}
exit:
	/* Jazz 82615: errorDetailCode has been initialized with 0 (SUCCESS).
	 * liveStack->pc should be updated to the current pc value when any
	 * verification error is detected above.
	 */
	if (verifyData->errorDetailCode < 0) {
		liveStack->pc = (UDATA)currentPC;
	}
	Trc_RTV_findAndMatchStack_Exit(verifyData->vmStruct, rc);
	return rc;
}


/*
	Returns 
	BCV_SUCCESS on success,
	BCV_FAIL on stack mismatch.
	BCV_ERR_INSUFFICIENT_MEMORY on OOM.
*/
static IDATA 
matchStack(J9BytecodeVerificationData * verifyData, J9BranchTargetStack *liveStack, J9BranchTargetStack * targetStack, UDATA inlineMatch)
{
	UDATA *livePtr = liveStack->stackElements;
	UDATA *liveTop = RELOAD_STACKTOP(liveStack);
	UDATA *targetPtr = targetStack->stackElements;
	UDATA *targetTop = RELOAD_STACKTOP(targetStack);
	UDATA size = liveStack->stackTopIndex;
	IDATA rc = BCV_SUCCESS;
	IDATA reasonCode = 0;

	Trc_RTV_matchStack_Entry(verifyData->vmStruct, inlineMatch);

	if (size != (UDATA) targetStack->stackTopIndex) {
		Trc_RTV_matchStack_DepthMismatchException(verifyData->vmStruct,
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				size, targetStack->stackTopIndex);
		rc = BCV_FAIL; /* fail - stack depth mismatch */
		verifyData->errorDetailCode = BCV_ERR_STACK_SIZE_MISMATCH;
		/* There are two situations for setting the location of data type on 'stack':
		 * 1) size == 0: it means (U_32)(liveTop - liveStack->stackElements) == 0 
		 *    and only show up the 1st data type on 'stack'.
		 * 2) size > 0: given the stackTop pointer always points to the next slot 
		 *    after pushing data type onto 'stack', it needs to step back 
		 *    by 1 slot to the latest valid data type.
		 */
		livePtr = (size > 0) ? (liveTop - 1) : liveTop;
		goto _errorLocation;
	}

	/* The check on the uninitializedThis flag is only applied to the class files
	 * with stackmaps (class version >= 51) which was introduced since Java 7.
	 * For the old class files without stackmaps (class version < 51), such check
	 * on the generated stackmaps is ignored so as to match the RI's behavior.
	 * (See Jazz103: 120689 for details)
	 */
	if (!verifyData->createdStackMap) {
		/* Target stack frame flag needs to be subset of ours. See JVM sepc 4.10.1.4 */
		if (liveStack->uninitializedThis && !targetStack->uninitializedThis) {
			rc = BCV_FAIL;
			goto _finished;
		}
	}

	while (livePtr != liveTop) {

		if (*livePtr != *targetPtr) {
			if ((*targetPtr & BCV_BASE_OR_SPECIAL) == 0) {
				rc = isClassCompatible (verifyData, *livePtr, *targetPtr, &reasonCode);

				if (FALSE == rc) {
					if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
						Trc_RTV_matchStack_OutOfMemoryException(verifyData->vmStruct,
							(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
							J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
							(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
							J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
							(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
							J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)));
						rc = BCV_ERR_INSUFFICIENT_MEMORY;
						goto _finished;
					} else {
						Trc_RTV_matchStack_IncompatibleClassException(verifyData->vmStruct,
								(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
								J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
								(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
								J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
								(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
								J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
								(livePtr - liveStack->stackElements), *livePtr, *targetPtr);
						rc = BCV_FAIL; /* fail - object type mismatch*/
						goto _incompatibleType;
					}
				}
			} else if (*targetPtr != BCV_BASE_TYPE_TOP) {
				Trc_RTV_matchStack_PrimitiveMismatchException(verifyData->vmStruct,
						(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
						(livePtr - liveStack->stackElements), *livePtr, *targetPtr);
				rc = BCV_FAIL; /* fail - primitive or special mismatch */
				goto _incompatibleType;
			}
		}
		livePtr++;
		targetPtr++;
	}

	if (inlineMatch) {
		/* hammer the live stack to be the target stack */
		Trc_RTV_matchStack_InlineMatchEvent(verifyData->vmStruct);
		livePtr = liveStack->stackElements;
		targetPtr = targetStack->stackElements;
		memcpy (livePtr, targetPtr, size * sizeof(UDATA));

		/* Propagate the uninitialized_this flag to targetStack if liveStack is uninitialized */
		if (((UDATA)TRUE == liveStack->uninitializedThis) || ((UDATA)TRUE == targetStack->uninitializedThis)) {
			liveStack->uninitializedThis = targetStack->uninitializedThis = TRUE;
		}
	}

	rc = BCV_SUCCESS;
	
_finished:
	Trc_RTV_matchStack_Exit(verifyData->vmStruct, rc);
	return rc;

_incompatibleType:
	/* errorDetailCode has been initialized with 0 (SUCCESS).
	 * Verification error data for incompatible data type should be saved here when any verification error occurs is detected above.
	 */
	verifyData->errorDetailCode = BCV_ERR_FRAMES_INCOMPATIBLE_TYPE;
	verifyData->errorTargetType = *targetPtr;
_errorLocation:
	verifyData->errorCurrentFramePosition = (U_32)(livePtr - liveStack->stackElements);
	verifyData->errorTargetFrameIndex = (U_32)BCV_STACK_INDEX(targetStack);
	goto _finished;
}


/* 
	Walk the bytecodes linearly and verify that the recorded stack maps match.

	returns BCV_SUCCESS on success
	returns BCV_ERR_INTERNAL_ERROR on verification error
	returns BCV_ERR_INSUFFICIENT_MEMORY on OOM
*/

static IDATA 
verifyBytecodes (J9BytecodeVerificationData * verifyData)
{
#define CHECK_END \
	if (pc > length) { \
		errorType = J9NLS_BCV_ERR_UNEXPECTED_EOF__ID;	\
		verboseErrorCode = BCV_ERR_UNEXPECTED_EOF;	\
		index = (UDATA)-1;	\
		goto _miscError; \
	}

	J9ROMClass * romClass = verifyData->romClass;
	J9ROMMethod * romMethod = verifyData->romMethod;
	J9BranchTargetStack *liveStack;
	J9BranchTargetStack *currentMapData = NULL;
	UDATA nextMapIndex = 0;
	IDATA start = 0;
	UDATA pc, length, index;
	J9ROMConstantPoolItem *constantPool;
	J9ROMConstantPoolItem *info;
	J9ROMStringRef *classRef;
	J9UTF8 *utf8string;
	U_8 *code, *temp, *bcIndex;
	UDATA returnChar;
	UDATA bc = 0;
	UDATA i;
	UDATA inconsistentStack = FALSE;
	UDATA inconsistentStack2 = FALSE;
	UDATA type, type1, type2, arrayType, temp1, temp2, temp3, popCount, receiver, cpIndex;
	UDATA action = RTV_NOP;
	UDATA *stackBase, *stackTop, *temps;
	UDATA *ptr;
	IDATA target;
	IDATA i1, i2;
	U_8 *className;
	UDATA classIndex, maxStack;
	UDATA wideIndex = FALSE;
	IDATA nextStackPC;
	IDATA nextExceptionStartPC;
	IDATA rc = 0;
	U_8 returnBytecode;
	UDATA errorModule = J9NLS_BCV_ERR_BYTECODES_INVALID__MODULE; /* defaults to BCV NLS catalog */
	U_16 errorType;
	I_16 offset16;
	I_32 offset32;
	UDATA argCount;
	UDATA checkIfInsideException = romMethod->modifiers & J9AccMethodHasExceptionInfo;
	UDATA tempStoreChange;
	J9ExceptionInfo *exceptionInfo = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
	J9ExceptionHandler *handler;
	J9UTF8 *catchName;
	UDATA catchClass;
	J9SRP *callSiteData = (J9SRP *) J9ROMCLASS_CALLSITEDATA(romClass);
	UDATA* originalStackTop;
	UDATA originalStackZeroEntry;
	BOOLEAN isInitMethod;
	IDATA verboseErrorCode = 0;
	/* Jazz 82615: Initialized by default if unused */
	UDATA errorTargetType = (UDATA)-1;
	UDATA errorStackIndex = (UDATA)-1;
	UDATA errorTempData = (UDATA)-1;
	BOOLEAN isNextStack = FALSE;

	Trc_RTV_verifyBytecodes_Entry(verifyData->vmStruct, 
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)));

	pc = 0;

	liveStack = (J9BranchTargetStack *) verifyData->liveStack;
	stackTop = &(liveStack->stackElements[0]);

	/* Determine the initial stack map from the method signature */
	isInitMethod = liveStack->uninitializedThis = buildStackFromMethodSignature (verifyData, &stackTop, &argCount);

	code = J9_BYTECODE_START_FROM_ROM_METHOD(romMethod);
	length = (UDATA) J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);
	maxStack = J9_MAX_STACK_FROM_ROM_METHOD(romMethod);

	if (argCount != romMethod->argCount) {
		errorType = J9NLS_BCV_ERR_ARGUMENTS_INCOMPATIBLE__ID; /* correct error code ??? */
		/* Jazz 82615: Set the error code and the mismatched argument calculated from the method's signature */
		verboseErrorCode = BCV_ERR_ARGUMENTS_MISMATCH;
		errorTempData = argCount;
		goto _miscError;
	}

	/* Set the base just past the temps - only args and temps are on the stack after building the signature */
	/* args and temps are accessed relative to liveStack->stackElements */
	SAVE_STACKTOP(liveStack, stackTop);
	liveStack->stackBaseIndex = liveStack->stackTopIndex;

	/* Jazz 105041: Initialize the 1st data slot on 'stack' with 'top' (placeholdler)
	 * to avoid storing garbage data type in the error message buffer
	 * when stack underflow occurs.
	 */
	liveStack->stackElements[liveStack->stackBaseIndex] = BCV_BASE_TYPE_TOP;

	RELOAD_LIVESTACK;

	/* result in temp1 is ignored */
	returnBytecode = getReturnBytecode (romClass, romMethod, &temp1);

	bcIndex = code;

	constantPool = J9_ROM_CP_FROM_ROM_CLASS(romClass);

	currentMapData = nextStack (verifyData, &nextMapIndex, &nextStackPC);

	/* Determine where the first region of bytecodes covered by an exception handler is */
	nextExceptionStartPC = nextExceptionStart (verifyData, romMethod, -1);

	/* walk the bytecodes linearly */
	while (pc < length) {
		if (inconsistentStack) {
_inconsistentStack:
			/* Jazz 82615: Only used for cases when errorType has not yet been set up for the verification error */
			errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
_inconsistentStack2:
			/* Jazz 82615: Store all verification error data here when the incompatible type issue is detected.
			 * Note: errorTempData is set to -1 by default if unused when verification error occurs.
			 */
			storeVerifyErrorData(verifyData, BCV_ERR_INCOMPATIBLE_TYPE, (U_32)errorStackIndex, errorTargetType, errorTempData, start);
			goto _verifyError;
		}

		if ((UDATA) (stackTop - stackBase) > maxStack) {
			errorType = J9NLS_BCV_ERR_STACK_OVERFLOW__ID;
			/* Jazz 82615: Set the error code and the location of wrong data type on stack (only keep the maximum size for stack) */
			verboseErrorCode = BCV_ERR_STACK_OVERFLOW;
			errorStackIndex = stackBase - liveStack->stackElements;
			if (maxStack > 0) {
				errorStackIndex += maxStack - 1;
			}
			goto _miscError;
		}

		/* If we are at the point of the next stack map, ensure that the current stack matches the mapped stack */
		if (pc == (UDATA) nextStackPC) {
			SAVE_STACKTOP(liveStack, stackTop);
			rc = matchStack (verifyData, liveStack, currentMapData, TRUE);
			if (BCV_SUCCESS != rc) {
				if (BCV_ERR_INSUFFICIENT_MEMORY == rc) {
					goto _outOfMemoryError;
				}
				/* Jazz 82615: Set liveStack->pc to the next pc value rather than the current pc value (start)
				 * in the case of the matched stack frame in the current frame (liveStack)
				 * of the detailed error message
			 	 */
				liveStack->pc = pc;
				goto _mapError;
			}

			/* Matched the expected next stack, find a new next stack */
			currentMapData = nextStack (verifyData, &nextMapIndex, &nextStackPC);
		}

		/* Check the stack against the exception handler stack */
		if (pc == (UDATA) nextExceptionStartPC) {
			handler = J9EXCEPTIONINFO_HANDLERS(exceptionInfo);
			SAVE_STACKTOP(liveStack, stackTop);

			/* Save the current liveStack element zero */
			/* Reset the stack pointer and push the exception on the empty stack */
			originalStackTop = stackTop;
			originalStackZeroEntry = liveStack->stackElements[liveStack->stackBaseIndex];

			/* Find all exception handlers from here */
			for (i = exceptionInfo->catchCount; i; i--, handler++) {
				if (handler->startPC == pc) {
					/* Check the maps at the handler PC */
					/* Modify the liveStack temporarily to contain the handler exception */
					if (handler->exceptionClassIndex) {
						catchName = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *)(&constantPool [handler->exceptionClassIndex]));
						catchClass = convertClassNameToStackMapType(verifyData, J9UTF8_DATA(catchName), J9UTF8_LENGTH(catchName), 0, 0);
					} else {
						catchClass = BCV_JAVA_LANG_THROWABLE_INDEX;
						catchClass <<= BCV_CLASS_INDEX_SHIFT;
					}

					/* Empty the stack */
					stackTop = &(liveStack->stackElements[liveStack->stackBaseIndex]);
					PUSH(catchClass);
					SAVE_STACKTOP(liveStack, stackTop);

					rc = findAndMatchStack (verifyData, handler->handlerPC, start);
					if (BCV_SUCCESS != rc) {
						if (BCV_ERR_INSUFFICIENT_MEMORY == rc) {
							goto _outOfMemoryError;
						}
						goto _mapError;
					}
				}
			}

			/* Restore liveStack */
			liveStack->stackElements[liveStack->stackBaseIndex] = originalStackZeroEntry;
			stackTop = originalStackTop;

			/* Get next exception start PC of interest */
			nextExceptionStartPC = nextExceptionStart (verifyData, romMethod, nextExceptionStartPC);
		}

		bcIndex = code + pc;

		bc = *bcIndex;

		start = pc;
		pc += (J9JavaInstructionSizeAndBranchActionTable[bc] & 7);
		CHECK_END;

		if ((stackTop - (JavaStackActionTable[bc] & 7)) < stackBase) {
			errorType = J9NLS_BCV_ERR_STACK_UNDERFLOW__ID;
			/* Jazz 82615: Set the error code and the location of data type involved 
			 * when the verification error related to stack underflow occurs.
			 */
			verboseErrorCode = BCV_ERR_STACK_UNDERFLOW;
			/* Given that the actual data type involved has not yet been located 
			 * through pop operation when stack underflow occurs,
			 * it needs to step back by 1 slot to the actual data type to be manipulated by the opcode.
			 */
			errorStackIndex = (stackTop - liveStack->stackElements) - 1;
			/* Always set to the location of the 1st data type on 'stack' to show up if stackTop <= stackBase
			 * Note: this setup is disabled to avoid decoding the garbage data in the error messages
			 * if the current stack frame is loaded from the next stack (without data on the stack)
			 * of the stackmaps.
			 */
			if ((stackTop <= stackBase) && !isNextStack) {
				errorStackIndex = stackBase - liveStack->stackElements;
			}
			goto _miscError;
		}
		/* Reset as the flag is only used for the current bytecode */
		isNextStack = FALSE;

		/* Format: 8bits action, 4bits type Y, 4bits type X */
		type1 = (UDATA) J9JavaBytecodeVerificationTable[bc];
		action = type1 >> 8;
		type2 = (type1 >> 4) & 0xF;
		type1 = (UDATA) decodeTable[type1 & 0xF];
		type2 = (UDATA) decodeTable[type2];

		/* Many bytecodes are isomorphic - ie have the same actions on the stack.  Walk
		 * based on the action type, rather than the individual bytecode.
		 */
		switch (action) {
		case RTV_NOP:
			break;

		case RTV_WIDE_LOAD_TEMP_PUSH:
			wideIndex = TRUE;	/* Fall through case !!! */

		case RTV_LOAD_TEMP_PUSH:
			index = type2 & 0x7;
			if (type2 == 0) {
				index = PARAM_8(bcIndex, 1);
				if (wideIndex) {
					index = PARAM_16(bcIndex, 1);
					wideIndex = FALSE;
				}
			}
			if (temps[index] != type1) {
				if ((type1 != BCV_GENERIC_OBJECT) || (temps[index] & BCV_TAG_BASE_TYPE_OR_TOP)) {
					inconsistentStack = TRUE;
					/* Jazz 82615: Set the expected data type and the location of wrong data type
					 * on stack (already in index) when the verification error occurs.
					 */
					errorTargetType = type1;
					errorStackIndex = index;
					goto _inconsistentStack;
				}
			}
			if (type1 == BCV_GENERIC_OBJECT) {
				PUSH(temps[index]);
				break;
			}
			if (type1 & BCV_WIDE_TYPE_MASK) {
				CHECK_TEMP((index + 1), BCV_BASE_TYPE_TOP);
				if (inconsistentStack) {
					/* Jazz 82615: Set the expected long/double type (already in type1)
					 * and the location of wrong data type on stack when the verification error occurs.
					 */
					errorTargetType = BCV_BASE_TYPE_TOP;
					/* Save the long/double type to errorTempData (verifyData->errorTempData)
					 * so as to ensure the wrong data type is the 2nd slot of long/double type
					 * in the error message framework rather than a 'top' type.
					 */
					errorTempData = type1;
					errorStackIndex = index + 1;
					goto _inconsistentStack;
				}
			}	/* Fall through case !!! */

		case RTV_PUSH_CONSTANT:
		
		_pushConstant:
			PUSH(type1);
			if (type1 & BCV_WIDE_TYPE_MASK) {
				PUSH(BCV_BASE_TYPE_TOP);
			}
			break;

		case RTV_PUSH_CONSTANT_POOL_ITEM:
			switch (bc) {
			case JBldc:
			case JBldcw:
				if (bc == JBldc) {
					index = PARAM_8(bcIndex, 1);
				} else {
					index = PARAM_16(bcIndex, 1);
				}
				stackTop = pushLdcType(verifyData, romClass, index, stackTop);
				break;

			/* Change lookup table to generate constant of correct type */
			case JBldc2lw:
				PUSH_LONG_CONSTANT;
				break;

			case JBldc2dw:
				PUSH_DOUBLE_CONSTANT;
				break;
			}
			break;

		case RTV_ARRAY_FETCH_PUSH:
			POP_TOS_INTEGER;
			if (inconsistentStack) {
				/* Jazz 82615: Set the expected data type and the location of wrong data type
				 * on stack when the verification error occurs.
				 */
				errorTargetType = BCV_BASE_TYPE_INT;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _inconsistentStack;
			}
			arrayType = POP;
			if (arrayType != BCV_BASE_TYPE_NULL) {
				if (bc == JBaaload) {
					inconsistentStack |= arrayType & BCV_BASE_OR_SPECIAL;
					inconsistentStack |= (arrayType & BCV_ARITY_MASK) == 0;
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type (presumably object array with 1 dimension)
						 * and the location of wrong data type on stack when the verification error occurs.
						 */
						errorTargetType = (UDATA)(BCV_GENERIC_OBJECT | (1 << BCV_ARITY_SHIFT));
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack;
					}
					type1 = arrayType - 0x01000000;	/* reduce types arity by one */
					PUSH(type1);
					break;
				} else {
					type = (UDATA) J9JavaBytecodeArrayTypeTable[bc - JBiaload];
					/* The operand checking for baload needs to cover the case of boolean arrays */
					CHECK_BOOL_ARRAY(JBbaload, bc, type);
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type (already in type) and the location of wrong data type
						 * on stack when the verification error occurs.
						 */
						errorTargetType = type;
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack;
					}
				}
			}
			goto _pushConstant;
			break;

		case RTV_WIDE_POP_STORE_TEMP:
			wideIndex = TRUE;	/* Fall through case !!! */

		case RTV_POP_STORE_TEMP:
			index = type2 & 0x7;
			if (type2 == 0) {
				index = PARAM_8(bcIndex, 1);
				if (wideIndex) {
					index = PARAM_16(bcIndex, 1);
					wideIndex = FALSE;
				}
			}
			POP_TOS_TYPE( type, type1 );
			if (inconsistentStack) {
				/* Jazz 82615: Set the the 2nd slot of long/double type (already in type1) and
				 * the location of wrong data type on stack when the verification error occurs.
				 */
				errorTargetType = BCV_BASE_TYPE_TOP;
				/* Save the long/double type to errorTempData (verifyData->errorTempData)
				 * so as to ensure the wrong data type is the 2nd slot of long/double type
				 * in the error message framework rather than a 'top' type.
				 */
				errorTempData = type1;
				/* The location of wrong data type needs to adjusted back to the right place
				 * because it pops twice from the stack for the wide type.
				 */
				errorStackIndex = (stackTop - liveStack->stackElements) + 1;
				goto _inconsistentStack;
			}

			tempStoreChange = FALSE;

			if (type != type1) {
				if ((type1 != BCV_GENERIC_OBJECT) || (type & BCV_TAG_BASE_TYPE_OR_TOP)) {
					inconsistentStack = TRUE;
					/* Jazz 82615: Set the expected data type and the location of wrong data type
					 * on stack when the verification error occurs.
					 */
					errorTargetType = type1;
					errorStackIndex = stackTop - liveStack->stackElements;
					goto _inconsistentStack;
				}
			}

			/* because of pre-index local clearing - the order here matters */
			if (type1 & BCV_WIDE_TYPE_MASK) {
				tempStoreChange = (temps[index + 1] != BCV_BASE_TYPE_TOP);
				STORE_TEMP((index + 1), BCV_BASE_TYPE_TOP);
			}
			tempStoreChange |= (type != temps[index]);
			STORE_TEMP(index, type);

			if (checkIfInsideException && tempStoreChange) {
				/* If we've stored a value into an arg/local, and it's of a different type than was
				 * originally there, we need to ensure that we are still compatible with all our
				 * exception handlers.
				 * 
				 * For all exception handlers covering this instruction
				 */
				handler = J9EXCEPTIONINFO_HANDLERS(exceptionInfo);
				SAVE_STACKTOP(liveStack, stackTop);

				/* Save the current liveStack element zero */
				/* Reset the stack pointer to push the exception on the empty stack */
				originalStackTop = stackTop;
				originalStackZeroEntry = liveStack->stackElements[liveStack->stackBaseIndex];

				/* Find all exception handlers from here */
				for (i = exceptionInfo->catchCount; i; i--, handler++) {
					if (((UDATA) start >= handler->startPC) && ((UDATA) start < handler->endPC)) {
#ifdef DEBUG_BCV
						printf("exception map change check at startPC: %d\n", handler->startPC);
#endif
						/* Check the maps at the handler PC */
						/* Modify the liveStack temporarily to contain the handler exception */
						if (handler->exceptionClassIndex) {
							catchName = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *)(&constantPool [handler->exceptionClassIndex]));
							catchClass = convertClassNameToStackMapType(verifyData, J9UTF8_DATA(catchName), J9UTF8_LENGTH(catchName), 0, 0);
						} else {
							catchClass = BCV_JAVA_LANG_THROWABLE_INDEX;
							catchClass <<= BCV_CLASS_INDEX_SHIFT;
						}
	
						stackTop = &(liveStack->stackElements[liveStack->stackBaseIndex]);
						PUSH(catchClass);
						SAVE_STACKTOP(liveStack, stackTop);
	
						rc = findAndMatchStack (verifyData, handler->handlerPC, start);
						if (BCV_SUCCESS != rc) {
							if (BCV_ERR_INSUFFICIENT_MEMORY == rc) {
								goto _outOfMemoryError;
							}
							goto _mapError;
						}
					}
				}
	
				/* Restore liveStack */
				liveStack->stackElements[liveStack->stackBaseIndex] = originalStackZeroEntry;
				stackTop = originalStackTop;
			}
			break;

		case RTV_ARRAY_STORE:
			POP_TOS_TYPE( type, type1 );
			if (inconsistentStack) {
				/* Jazz 82615: Set the 2nd slot of long/double type (already in type1) and
				 * the location of wrong data type on stack when the verification error occurs.
				 */
				errorTargetType = BCV_BASE_TYPE_TOP;
				/* Save the long/double type to errorTempData (verifyData->errorTempData)
				 * so as to ensure the wrong data type is the 2nd slot of long/double type
				 * in the error message framework rather than a 'top' type.
				 */
				errorTempData = type1;
				/* The location of wrong data type needs to adjusted back to the right place
				 * because it pops twice from the stack for the wide type.
				 */
				errorStackIndex = (stackTop - liveStack->stackElements) + 1;
				goto _inconsistentStack;
			}
			POP_TOS_INTEGER;
			if (inconsistentStack) {
				/* Jazz 82615: Set the expected data type and the location of wrong data type
				 * on stack when the verification error occurs.
				 */
				errorTargetType = BCV_BASE_TYPE_INT;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _inconsistentStack;
			}
			arrayType = POP;
			if (arrayType != BCV_BASE_TYPE_NULL) {
				if (bc == JBaastore) {
					inconsistentStack |= (arrayType | type) & BCV_BASE_OR_SPECIAL;
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type and the location of wrong data type on stack when the verification error occurs.
						 * Note: Given that the current location on stack is for arrayType, it needs to move forward by two slots
						 * to skip over the arrayType plus the index of array so as to reach the location of wrong type (value stored in the array).
						 */
						errorTargetType = type1;
						errorStackIndex = (stackTop - liveStack->stackElements) + 2;
						goto _inconsistentStack;
					}
					inconsistentStack |= (arrayType & BCV_ARITY_MASK) == 0;
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type (array of object reference)
						 * and the location of wrong data type on stack when the verification error occurs.
						 */
						errorTargetType = type1 | (1 << BCV_ARITY_SHIFT);
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack;
					}
				} else {
					inconsistentStack |= (type != type1);
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type and the location of wrong data type on stack when the verification error occurs.
						 * Note: Given that the current location on stack is for arrayType, it needs to move forward by two slots
						 * to skip over the index of array and get to the location of wrong type (value stored in the array).
						 */
						errorTargetType = type1;
						errorStackIndex = (stackTop - liveStack->stackElements) + 2;
						goto _inconsistentStack;
					}

					type2 = (UDATA) J9JavaBytecodeArrayTypeTable[bc - JBiastore];
					/* The operand checking for bastore needs to cover the case of boolean arrays */
					CHECK_BOOL_ARRAY(JBbastore, bc, type2);
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type and the location of wrong data type 
						 * on stack when the verification error occurs.
						 */
						errorTargetType = type2;
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack;
					}
				}
			}
			break;

		case RTV_INCREMENT:
			index = PARAM_8(bcIndex, 1);
			if (bc == JBiincw) {
				index = PARAM_16(bcIndex, 1);
			}
			CHECK_TEMP_INTEGER(index);
			if (inconsistentStack) {
				/* Jazz 82615: Set the expected data type and the location of wrong data type on stack
				 * (already in index) when the verification error occurs.
				 */
				errorTargetType = BCV_BASE_TYPE_INT;
				errorStackIndex = index;
				goto _inconsistentStack;
			}
			break;

		case RTV_POP_2_PUSH:
			POP_TOS_TYPE_EQUAL( type, type1 );
			if (inconsistentStack) {
				/* Jazz 82615: Set the 2nd slot of long/double type (already in type1) and
				 * the location of wrong data type on stack when the verification error occurs.
				 */
				errorTargetType = BCV_BASE_TYPE_TOP;
				/* Save the long/double type to errorTempData (verifyData->errorTempData)
				 * so as to ensure the wrong data type is the 2nd slot of long/double type
				 * in the error message framework rather than a 'top' type.
				 */
				errorTempData = type1;
				/* The location of wrong data type needs to adjusted back to the right place
				 * because it pops twice from the stack for the wide type.
				 */
				errorStackIndex = (stackTop - liveStack->stackElements) + 1;
				goto _inconsistentStack;
				/* Jazz 82615: the second inconsistent stack case in POP_TOS_TYPE_EQUAL should be addressed here */
			} else if (inconsistentStack2) {
				errorTargetType = type1;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _inconsistentStack;
			}
			POP_TOS_TYPE_EQUAL( type, type1 );
			if (inconsistentStack) {
				/* Jazz 82615: Set the 2nd slot of long/double type (already in type1) and
				 * the location of wrong data type on stack when the verification error occurs.
				 */
				errorTargetType = BCV_BASE_TYPE_TOP;
				/* Save the long/double type to errorTempData (verifyData->errorTempData)
				 * so as to ensure the wrong data type is the 2nd slot of long/double type
				 * in the error message framework rather than a 'top' type.
				 */
				errorTempData = type1;
				/* The location of wrong data type needs to adjusted back to the right place
				 * because it pops twice from the stack for the wide type.
				 */
				errorStackIndex = (stackTop - liveStack->stackElements) + 1;
				goto _inconsistentStack;
				/* Jazz 82615: the second inconsistent stack case in POP_TOS_TYPE_EQUAL should be addressed here */
			} else if (inconsistentStack2) {
				errorTargetType = type1;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _inconsistentStack;
			}
			goto _pushConstant;
			break;

		case RTV_POP_X_PUSH_X:
			/* Shifts only have non-zero type2 */
			if (type2) { 
				POP_TOS_INTEGER;
				if (inconsistentStack) {
					/* Jazz 82615: Set the expected data type and the location of wrong data type
					 * on stack when the verification error occurs.
					 */
					errorTargetType = BCV_BASE_TYPE_INT;
					errorStackIndex = stackTop - liveStack->stackElements;
					goto _inconsistentStack;
				}
			}
			POP_TOS_TYPE_EQUAL( type, type1 );
			if (inconsistentStack) {
				/* Jazz 82615: Set the 2nd slot of long/double type (already in type1) and
				 * the location of wrong data type on stack when the verification error occurs.
				 */
				errorTargetType = BCV_BASE_TYPE_TOP;
				/* Save the long/double type to errorTempData (verifyData->errorTempData)
				 * so as to ensure the wrong data type is the 2nd slot of long/double type
				 * in the error message framework rather than a 'top' type.
				 */
				errorTempData = type1;
				/* The location of wrong data type needs to adjusted back to the right place
				 * because it pops twice from the stack for the wide type.
				 */
				errorStackIndex = (stackTop - liveStack->stackElements) + 1;
				goto _inconsistentStack;
				/* Jazz 82615: the second inconsistent stack case in POP_TOS_TYPE_EQUAL should be addressed here */
			} else if (inconsistentStack2) {
				errorTargetType = type1;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _inconsistentStack;
			}
			goto _pushConstant;
			break;

		case RTV_POP_X_PUSH_Y:
			POP_TOS_TYPE_EQUAL( type, type1 );
			if (inconsistentStack) {
				/* Jazz 82615: Set the 2nd slot of long/double type (already in type1) and
				 * the location of wrong data type on stack when the verification error occurs.
				 */
				errorTargetType = BCV_BASE_TYPE_TOP;
				/* Save the long/double type to errorTempData (verifyData->errorTempData)
				 * so as to ensure the wrong data type is the 2nd slot of long/double type
				 * in the error message framework rather than a 'top' type.
				 */
				errorTempData = type1;
				/* The location of wrong data type needs to adjusted back to the right place
				 * because it pops twice from the stack for the wide type.
				 */
				errorStackIndex = (stackTop - liveStack->stackElements) + 1;
				goto _inconsistentStack;
				/* Jazz 82615: the second inconsistent stack case in POP_TOS_TYPE_EQUAL should be addressed here */
			} else if (inconsistentStack2) {
				errorTargetType = type1;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _inconsistentStack;
			}
			type1 = type2;
			goto _pushConstant;
			break;

		case RTV_BRANCH:
			popCount = type2 - 8;
			for (i = 0; i < popCount; i++) {
				if (type1 == BCV_GENERIC_OBJECT) {
					type = POP;
					/* Jazz 89060: According to the explanation of the Verification type hierarchy/rules
					 * at section 4.10.1.2 Verification Type System (The JVM Specification Java SE 8 Edition):
					 * isAssignable(uninitialized, X) :- isAssignable(reference, X).
					 * isAssignable(uninitializedThis, X) :- isAssignable(uninitialized, X).
					 * isAssignable(uninitialized(_), X) :- isAssignable(uninitialized, X).
					 *
					 * It means that 'uninitializedThis' is a subtype  of 'reference',
					 * and then 'uninitializedThis' is compatible with BCV_GENERIC_OBJECT ( 'reference' type )
					 */
					if (J9_ARE_NO_BITS_SET(type, BCV_SPECIAL)) {
						IDATA reasonCode = 0;
						rc = isClassCompatible (verifyData, type, BCV_GENERIC_OBJECT, &reasonCode);
						if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
							goto _outOfMemoryError;
						}

						inconsistentStack |= (FALSE == rc);
						if (inconsistentStack) {
							/* Jazz 82615: Set the expected data type and the location of wrong data type
							 * on stack when the verification error occurs.
							 */
							errorTargetType = BCV_GENERIC_OBJECT;
							errorStackIndex = stackTop - liveStack->stackElements;
							goto _inconsistentStack;
						}
					}
				} else {
					POP_TOS_INTEGER;
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type and the location of wrong data type
						 * on stack when the verification error occurs.
						 */
						errorTargetType = BCV_BASE_TYPE_INT;
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack;
					}
				}
			}

			if (bc == JBgotow) {
				offset32 = (I_32) PARAM_32(bcIndex, 1);
				target = start + offset32;
			} else {
				offset16 = (I_16) PARAM_16(bcIndex, 1);
				target = start + offset16;
			}

			SAVE_STACKTOP(liveStack, stackTop);
			/* Merge our stack to the target */
			rc = findAndMatchStack (verifyData, target, start);
			if (BCV_SUCCESS != rc) {
				if (BCV_ERR_INSUFFICIENT_MEMORY == rc) {
					goto _outOfMemoryError;
				}
				goto _mapError;
			}

			/* Unconditional branch */
			if (popCount == 0) {
				goto _newStack;
			}
			break;

		case RTV_RETURN:
			utf8string = J9ROMMETHOD_SIGNATURE(romMethod);
			temp = &J9UTF8_DATA(utf8string)[J9UTF8_LENGTH(utf8string) - 2];
			returnChar = (UDATA) temp[1];
			if (temp[0] != ')') {
				returnChar = 0;
				className = J9UTF8_DATA(utf8string);
				while (*className++ != ')');
				temp1 = parseObjectOrArrayName(verifyData, className);
			}
				
			switch (bc) {

			case JBreturn0:
			case JBsyncReturn0:
			case JBreturnFromConstructor:
				inconsistentStack |= (returnChar != 'V');
				if (inconsistentStack) {
_illegalPrimitiveReturn:
					/* Jazz 82615: Set the error code in the case of verification error on return. */
					errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
					verboseErrorCode = BCV_ERR_WRONG_RETURN_TYPE;
					goto _miscError;
				}
				break;

			case JBreturn1:
			case JBreturnB:
			case JBreturnC:
			case JBreturnS:
			case JBreturnZ:
			case JBsyncReturn1:
				/* Note: for synchronized return{B,C,S,Z}, JBgenericReturn is used */
				if (returnChar) {
					U_32 returnType = 0;
					/* single character return description */
					if ((returnChar < 'A') || (returnChar > 'Z')) {
						inconsistentStack = TRUE;
						goto _illegalPrimitiveReturn;
					}
					type = (UDATA) oneArgTypeCharConversion[returnChar - 'A'];
					POP_TOS(type);

					/* check methods that return char, byte, short, or bool use the right opcode */
					if (BCV_BASE_TYPE_INT == type){
						switch(bc) {
						case JBreturnB:
							returnType = BCV_BASE_TYPE_BYTE_BIT;
							break;

						case JBreturnC:
							returnType = BCV_BASE_TYPE_CHAR_BIT;
							break;

						case JBreturnS:
							returnType = BCV_BASE_TYPE_SHORT_BIT;
							break;

						case JBreturnZ:
							returnType = BCV_BASE_TYPE_BOOL_BIT;
							break;
						/* Note: synchronized return of b,c,s,z are handled as a JBgenericReturn */
						case JBreturn1:
						case JBsyncReturn1:
							returnType = BCV_BASE_TYPE_INT_BIT;
							break;
						default:
							Trc_RTV_j9rtv_verifyBytecodes_Unreachable(verifyData->vmStruct,
								(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
								J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
								(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
								J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
								(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
								J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
								__LINE__);
							break;
						}
						inconsistentStack |= returnType != baseTypeCharConversion[returnChar - 'A'];
					}
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type (already in type) and
						 * the location of wrong data type on stack when the verification error occurs.
						 */
						errorTargetType = type;
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack;
					}

				} else {
					IDATA reasonCode = 0;

					/* Object */
					POP_TOS_OBJECT(type);
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type and the location of wrong data type on stack when the verification error occurs. */
						errorTargetType = BCV_GENERIC_OBJECT;
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack;
					}

					rc = isClassCompatible(verifyData, type, temp1, &reasonCode);
					if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
						goto _outOfMemoryError;
					}
					inconsistentStack |= (FALSE == rc);
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type and the location of wrong data type
						 * on stack when the verification error occurs.
						 */
						errorTargetType = temp1;
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack;
					}
				}
				break;

			case JBreturn2:
			case JBsyncReturn2:
				/* single character return description */
				if ((returnChar < 'A') || (returnChar > 'Z')) {
					inconsistentStack = TRUE;
					goto _illegalPrimitiveReturn;
				}
				type = (UDATA) argTypeCharConversion[returnChar - 'A'];
				POP_TOS_2(type);
				if (inconsistentStack) {
					/* Jazz 82615: Set the expected data type (already in type)
					 * and the location of wrong data type on stack when the verification error occurs.
					 */
					errorTargetType = type;
					errorStackIndex = stackTop - liveStack->stackElements;
					goto _inconsistentStack;
				}
				break;

			case JBgenericReturn:
				/* Decide based on the signature of this method what the return is supposed to be */
				if (returnChar) {
					/* single character return description */
					if ((returnChar < 'A') || (returnChar > 'Z')) {
						inconsistentStack = TRUE;
						goto _illegalPrimitiveReturn;
					}
					if (returnChar != 'V') {
						type = (UDATA) argTypeCharConversion[returnChar - 'A'];
						if ((returnChar == 'J') || (returnChar == 'D')) {
							POP_TOS(BCV_BASE_TYPE_TOP);
							if (inconsistentStack) {
								/* Jazz 82615: Set the expected long/double type and the location of wrong data type on stack when the verification error occurs. */
								errorTempData = ('J' == returnChar) ? BCV_BASE_TYPE_LONG : BCV_BASE_TYPE_DOUBLE;
								errorTargetType = BCV_BASE_TYPE_TOP;
								errorStackIndex = stackTop - liveStack->stackElements;
								goto _inconsistentStack;
							}
						}
						POP_TOS(type);
						if (inconsistentStack) {
							/* Jazz 82615: Set the expected data type (already in type) and
							 * the location of wrong data type on stack when the verification error occurs.
							 */
							errorTargetType = type;
							errorStackIndex = stackTop - liveStack->stackElements;
							goto _inconsistentStack;
						}
					}
				} else {
					IDATA reasonCode = 0;

					/* Object */
					POP_TOS_OBJECT(type);
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type and the location of wrong data type
						 * on stack when the verification error occurs.
						 */
						errorTargetType = BCV_GENERIC_OBJECT;
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack;
					}

					rc = isClassCompatible(verifyData, type, temp1, &reasonCode);
					if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
						goto _outOfMemoryError;
					}
					inconsistentStack |= (FALSE == rc);
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type and the location of wrong data type
						 * on stack when the verification error occurs.
						 */
						errorTargetType = temp1;
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack;
					}
				}

				/* Don't rewrite the return bytecode for j.l.Object.<init>()V or if the romClass is in the shared classes cache*/
				if ((stackTop == stackBase)
				&& ((argCount != 1) || (returnBytecode != JBreturnFromConstructor) || (J9ROMCLASS_SUPERCLASSNAME(romClass) != NULL))
				&& (FALSE == verifyData->romClassInSharedClasses)
				) {
					/* Rewrite clean return if empty stack - needed for StackMap/StackMapTable attribute methods */
					*bcIndex = returnBytecode;
				}

				break;
			}

			/* If this is <init>, need to ensure that the uninitialized_this
			 * has been initialized.  Can be determined from the stack map's
			 * flag
			 */
			if (isInitMethod) {
				if (liveStack->uninitializedThis == (UDATA)TRUE){
					errorType = J9NLS_BCV_ERR_INIT_NOT_CALL_INIT__ID;
					/* Jazz 82615: Set the error code and the location of data type on stack in the case of the verification error unrelated to incompatible type.*/
					verboseErrorCode = BCV_ERR_INIT_NOT_CALL_INIT;
					/* Given that there is no real wrong data type on stack in such case, the stackTop pointer needs to step back by one slot
					 * to point to the current data type on stack so as to print all data types correctly in the error message framework.
					 */
					errorStackIndex = (stackTop - liveStack->stackElements) - 1;
					/* Always set to the location of the 1st data type on 'stack' to show up if stackTop <= stackBase */
					if (stackTop <= stackBase) {
						errorStackIndex = stackBase - liveStack->stackElements;
					}
					goto _miscError;
				}
			}

			goto _newStack;
			break;

		case RTV_STATIC_FIELD_ACCESS:
		{
			IDATA reasonCode = 0;
			/* Jazz 82615: Save the location of wrong data type (receiver) */
			UDATA *receiverPtr = NULL;

			index = PARAM_16(bcIndex, 1);
			info = &constantPool[index];
			utf8string = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMFIELDREF_NAMEANDSIGNATURE((J9ROMFieldRef *) info))));

			receiver = BCV_BASE_TYPE_NULL; /* makes class compare work with statics */

			if (bc & 1) {
				/* JBputfield/JBpustatic - odd bc's */
				type = POP;
				if ((*J9UTF8_DATA(utf8string) == 'D') || (*J9UTF8_DATA(utf8string) == 'J')) {
					inconsistentStack |= (type != BCV_BASE_TYPE_TOP);
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected long/double type and the location of wrong data type
						 * on stack when the verification error occurs.
						 */
						errorTempData = ('J' == *J9UTF8_DATA(utf8string)) ? BCV_BASE_TYPE_LONG : BCV_BASE_TYPE_DOUBLE;
						errorTargetType = BCV_BASE_TYPE_TOP;
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack;
					}
					type = POP;
				}
				/* HACK: Pushes the return type without moving the stack pointer */
				pushFieldType(verifyData, utf8string, stackTop);
				if (*stackTop & BCV_TAG_BASE_TYPE_OR_TOP) {
					inconsistentStack |= (*stackTop != type);
				} else {
					rc = isClassCompatible(verifyData, type, *stackTop, &reasonCode);
					if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
						goto _outOfMemoryError;
					}

					inconsistentStack |= (FALSE == rc);
				}
				if (inconsistentStack) {
					/* Jazz 82615: Set the expected data type and the location of wrong data type on stack when the verification error occurs. */
					errorTargetType = *stackTop;
					errorStackIndex = stackTop - liveStack->stackElements;

					/* Jazz 82615: This is an exception when storing the verification error data, which was originally caused by the hack (pushFieldType) above.
					 * The stackTop pointer doesn't get updated after calling pushFieldType() to store the return type, so *stackTop stores
					 * the expected data type rather than the wrong data type which has already been popped up from stack.
					 *
					 * According to the output format of error message framework, only the the wrong data type is expected to occur at *stackTop on stack
					 * rather than the expected data type. Thus, the wrong data type must be pushed back to *stackTop after saving the expected data type
					 * somewhere else.
					 */
					PUSH(type);
					goto _inconsistentStack;
				}

				if (bc == JBputfield) {
					receiver = POP;
					/* Jazz 82615: Save the location of receiver */
					receiverPtr = stackTop;
				}
			} else {
				/* JBgetfield/JBgetstatic - even bc's */
				if (bc == JBgetfield) {
					receiver = POP;
					/* Jazz 82615: Save the location of receiver */
					receiverPtr = stackTop;
				}
				stackTop = pushFieldType(verifyData, utf8string, stackTop);
			}

			rc = isFieldAccessCompatible (verifyData, (J9ROMFieldRef *) info, bc, receiver, &reasonCode);
			if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
				goto _outOfMemoryError;
			}
			inconsistentStack |= (FALSE == rc);
			if (inconsistentStack) {
				constantPool = (J9ROMConstantPoolItem *) (romClass + 1);
				utf8string = J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[((J9ROMFieldRef *) info)->classRefCPIndex]);

				/* Jazz 82615: Store the expected data type for later retrieval in classNameList. */
				errorTargetType = convertClassNameToStackMapType(verifyData, J9UTF8_DATA(utf8string), J9UTF8_LENGTH(utf8string), 0, 0);

				/* If the error occurs in isFieldAccessCompatible(), the stackTop pointer needs
				 * to be restored to point to the previous location of receiver (wrong data type) and
				 * restore the value of receiver if overridden by other data.
				 */
				if (NULL != receiverPtr) {
					stackTop = receiverPtr;
				}
				*stackTop = receiver;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _inconsistentStack;
			}

			if (receiver != BCV_BASE_TYPE_NULL) {
				utf8string = J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[((J9ROMFieldRef *) info)->classRefCPIndex]);

				rc = isProtectedAccessPermitted (verifyData, utf8string, receiver, (void *) info, TRUE, &reasonCode);
				if (FALSE == rc) {
					if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
						goto _outOfMemoryError;
					}
					errorType = J9NLS_BCV_ERR_BAD_ACCESS_PROTECTED__ID;
					/* Jazz 82615: Set the error code when error occurs in checking the protected member access. */
					verboseErrorCode = BCV_ERR_BAD_ACCESS_PROTECTED;
					goto _miscError;
				}
			}
			break;
		}

		case RTV_SEND:
			if ((bc == JBinvokehandle) || (bc == JBinvokehandlegeneric)) {
				IDATA reasonCode = 0;

				index = PARAM_16(bcIndex, 1);
				info = &constantPool[index];
				utf8string = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info))));
				/* Removes the args from the stack and verify the stack shape is consistent */
				rc = j9rtv_verifyArguments(verifyData, utf8string, &stackTop);
				CHECK_STACK_UNDERFLOW;
				if (BCV_ERR_INSUFFICIENT_MEMORY == rc) {
					goto _outOfMemoryError;
				}
				inconsistentStack |= (BCV_SUCCESS != rc);
				if (inconsistentStack) {
					/* errorDetailCode has been initialized with 0 (SUCCESS) before any verification error is detected
					 * For the return code from j9rtv_verifyArguments(), there is no need to print out error message framework
					 * in the case of BCV_FAIL for bad signature (should never happen - caught by cfreader.c in checkMethodSignature)
					 * and BCV_ERR_INSUFFICIENT_MEMORY for OOM as there is nothing meaningful to show up.
					 */
					if (verifyData->errorDetailCode < 0) {
						J9UTF8 * utf8ClassString = J9ROMCLASSREF_NAME((J9ROMClassRef *) &constantPool[((J9ROMMethodRef *) info)->classRefCPIndex]);
						J9UTF8 * utf8MethodString = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_NAME(J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info))));
						storeMethodInfo(verifyData, utf8ClassString, utf8MethodString, utf8string, start);
					}
					errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
					goto _verifyError;
				}

				/* Don't need to do protected access check here - both of these are special
				 * cases of invokevirtual which only invoke public methods of a public class.
				 */

				/* Receiver compatible with MethodHandle? */
				type = POP;		/* Remove the receiver from the stack */
				rc = isClassCompatibleByName (verifyData, type, (U_8 *)"java/lang/invoke/MethodHandle", sizeof("java/lang/invoke/MethodHandle") - 1, &reasonCode);
				if (FALSE == rc) {
					if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
						goto _outOfMemoryError;
					}
					errorType = J9NLS_BCV_ERR_RECEIVER_NOT_COMPATIBLE__ID;
					/* Jazz 82615: Store the index of the expected data type 'java/lang/invoke/MethodHandle' for later retrieval in classNameList */
					errorTargetType = (UDATA)(BCV_JAVA_LANG_INVOKE_METHODHANDLE_INDEX << BCV_CLASS_INDEX_SHIFT);
					errorStackIndex = stackTop - liveStack->stackElements;
					goto _inconsistentStack2;
				}
				stackTop = pushReturnType(verifyData, utf8string, stackTop);

				break;
			}

			if (bc == JBinvokedynamic) {
				index = PARAM_16(bcIndex, 1); /* TODO 3 byte index */
				utf8string = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(SRP_PTR_GET(callSiteData + index, J9ROMNameAndSignature*))));
				/* Removes the args from the stack and verify the stack shape is consistent */
				rc = j9rtv_verifyArguments(verifyData, utf8string, &stackTop);
				CHECK_STACK_UNDERFLOW;
				if (BCV_ERR_INSUFFICIENT_MEMORY == rc) {
					goto _outOfMemoryError;
				}
				inconsistentStack |= (BCV_SUCCESS != rc);
				if (inconsistentStack) {
					/* errorDetailCode has been initialized with 0 (SUCCESS) before any verification error is detected
					 * For the return code from j9rtv_verifyArguments(), there is no need to print out error message framework
					 * in the case of BCV_FAIL for bad signature (should never happen - caught by cfreader.c in checkMethodSignature)
					 * and BCV_ERR_INSUFFICIENT_MEMORY for OOM as there is nothing meaningful to show up.
					 */
					if (verifyData->errorDetailCode < 0) {
						J9UTF8 * utf8ClassString = J9ROMCLASS_CLASSNAME(romClass);
						J9UTF8 * utf8MethodString = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_NAME(SRP_PTR_GET(callSiteData + index, J9ROMNameAndSignature*))));
						storeMethodInfo(verifyData, utf8ClassString, utf8MethodString, utf8string, start);
					}
					errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
					goto _verifyError;
				}
				stackTop = pushReturnType(verifyData, utf8string, stackTop);
				break;
			}

			if (bc == JBinvokeinterface2) {
				bcIndex += 2;
				bc = JBinvokeinterface;
			}
			index = PARAM_16(bcIndex, 1);
			if (JBinvokestaticsplit == bc) {
				index = *(U_16 *)(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(romClass) + index);
			} else if (JBinvokespecialsplit == bc) {
				index = *(U_16 *)(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(romClass) + index);
			}
			info = &constantPool[index];
			utf8string = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info))));
			cpIndex = ((J9ROMMethodRef *) info)->classRefCPIndex;
			classRef = (J9ROMStringRef *) &constantPool[cpIndex];
			rc = j9rtv_verifyArguments(verifyData, utf8string, &stackTop);	/* Removes the args from the stack */
			CHECK_STACK_UNDERFLOW;
			if (BCV_ERR_INSUFFICIENT_MEMORY == rc) {
				goto _outOfMemoryError;
			}
			inconsistentStack |= (BCV_SUCCESS != rc);
			if (inconsistentStack) {
				/* errorDetailCode has been initialized with 0 (SUCCESS) before any verification error is detected
				 * For the return code from j9rtv_verifyArguments(), there is no need to print out error message framework
				 * in the case of BCV_FAIL for bad signature (should never happen - caught by cfreader.c in checkMethodSignature)
				 * and BCV_ERR_INSUFFICIENT_MEMORY for OOM as there is nothing meaningful to show up.
				 */
				if (verifyData->errorDetailCode < 0) {
					J9UTF8 * utf8ClassString = J9ROMSTRINGREF_UTF8DATA(classRef);
					J9UTF8 * utf8MethodString = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_NAME(J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info))));
					storeMethodInfo(verifyData, utf8ClassString, utf8MethodString, utf8string, start);
				}
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				goto _verifyError;
			}
			
			if ((JBinvokestatic != bc)
				&& (JBinvokestaticsplit != bc)
			) {
				IDATA reasonCode = 0;

				type = POP;		/* Remove the receiver from the stack */
				switch (bc) {
				case JBinvokespecial:
				case JBinvokespecialsplit:
					CHECK_STACK_UNDERFLOW;
					/* cannot use isInitMethod here b/c invokespecial may be invoking a different <init> method then the one we are in */
					if (J9UTF8_DATA(J9ROMNAMEANDSIGNATURE_NAME(J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info)))[0] == '<') {

						/* This is <init>, verify that this is a NEW or INIT object */
						if ((type & BCV_SPECIAL) == 0) {
							/* Fail - not an init object */
							errorType = J9NLS_BCV_ERR_BAD_INIT__ID;
							/* Jazz 82615: Set the expected data type and the location of wrong data type
							 * on stack when the verification error occurs.
							 */
							errorTargetType = BCV_SPECIAL_INIT;
							errorStackIndex = stackTop - liveStack->stackElements;
							goto _inconsistentStack2;
						} else {
							temp1 = getSpecialType(verifyData, type, code);
							if (temp1 & BCV_SPECIAL_INIT) {
								/* fail if getSpecialType didn't decode to a non-special type */
								errorType = J9NLS_BCV_ERR_ARGUMENTS_INCOMPATIBLE__ID;
								/* Jazz 82615: Set the error code, decoded data type (already in temp1) and
								 * update the stackTopIndex to include the wrong type on stack (already popped out of stack).
								 */
								verboseErrorCode = BCV_ERR_BAD_INIT_OBJECT;
								errorTempData = temp1;
								errorStackIndex = stackTop - liveStack->stackElements;
								goto _miscError;
							}

							/* For uninitialized(x), ensure that class @ 'new X' and class of <init> are identical */
							if (type & BCV_SPECIAL_NEW) {
								utf8string = J9ROMSTRINGREF_UTF8DATA(classRef);
								classIndex = convertClassNameToStackMapType(verifyData, J9UTF8_DATA(utf8string), J9UTF8_LENGTH(utf8string), 0, 0);
								if (classIndex != temp1) {
									errorType = J9NLS_BCV_ERR_WRONG_INIT_METHOD__ID;
									/* Jazz 82615: Set the expected data type, the wrong type on stack and
									 * the location of wrong data type on stack when the verification error occurs.
									 */
									errorTargetType = temp1;
									*stackTop = classIndex;
									errorStackIndex = stackTop - liveStack->stackElements;
									goto _inconsistentStack2;
								}
							}

							/* Ensure the <init> method is either for this class or its direct super class */
							if (type & BCV_SPECIAL_INIT) {
								UDATA superClassIndex = 0;

								utf8string = J9ROMSTRINGREF_UTF8DATA(classRef);
								classIndex = convertClassNameToStackMapType(verifyData, J9UTF8_DATA(utf8string), J9UTF8_LENGTH(utf8string), 0, 0);

								utf8string = J9ROMCLASS_SUPERCLASSNAME(romClass);
								superClassIndex = convertClassNameToStackMapType(verifyData, J9UTF8_DATA(utf8string), J9UTF8_LENGTH(utf8string), 0, 0);
								if ((classIndex != temp1) && (classIndex != superClassIndex)) {
									errorType = J9NLS_BCV_ERR_WRONG_INIT_METHOD__ID;
									/* Jazz 82615: Set the expected data type, the wrong type on stack and
									 * the location of wrong data type on stack when the verification error occurs.
									 */
									errorTargetType = temp1;
									*stackTop = classIndex;
									errorStackIndex = stackTop - liveStack->stackElements;
									goto _inconsistentStack2;
								}
							}

							/* This invoke special will make all copies of this "type" on the stack a real
							   object, find all copies of this object and initialize them */
							ptr = temps;	/* assumption that stack follows temps */
							/* we don't strictly need to walk temps here, the pending stack would be enough */
							while (ptr != stackTop) {
								if (*ptr == type) {
									*ptr = temp1;
								}
								ptr++;
							}
							/* Flag the initialization occurred only if its uninitialized_this */
							if (type & BCV_SPECIAL_INIT) {
								liveStack->uninitializedThis = FALSE;
							}
							type = temp1;
						}

					} else { /* non <init> */
						/* The JVM converts invokevirtual of final methods in Object to invokespecial.
						 * For verification purposes, treat these as invokevirtual by falling through.
						 */
						J9ROMNameAndSignature *nas = J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info);
						J9UTF8 *name = J9ROMNAMEANDSIGNATURE_NAME(nas);
						J9UTF8 *sig = J9ROMNAMEANDSIGNATURE_SIGNATURE(nas);
						if (!methodIsFinalInObject(J9UTF8_LENGTH(name), J9UTF8_DATA(name), J9UTF8_LENGTH(sig), J9UTF8_DATA(sig))) {
							utf8string = J9ROMCLASS_CLASSNAME(romClass);
							classIndex = convertClassNameToStackMapType(verifyData, J9UTF8_DATA(utf8string), J9UTF8_LENGTH(utf8string), 0, 0);
							utf8string = J9ROMSTRINGREF_UTF8DATA(classRef);

							/* the lazy evaluation would guarantee reasonCode reflects the first failure.
							 * In all three functions, returned boolean value would indicate an error occurred and the reasonCode is set to BCV_ERR_INSUFFICIENT_MEMORY in OOM cases.
							 * Hence, if any of the 3 conditions should fail, if it is not on OOM as reasonCode would indicate, it must be a verification error.
							 */
							if ((FALSE == isClassCompatibleByName(verifyData, classIndex, J9UTF8_DATA(utf8string), J9UTF8_LENGTH(utf8string), &reasonCode))
							|| (FALSE == isClassCompatible(verifyData, type, classIndex, &reasonCode))
							) {
								if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
									goto _outOfMemoryError;
								}
								errorType = J9NLS_BCV_ERR_BAD_INVOKESPECIAL__ID;
								/* Jazz 82615: Set the error code when error occurs in checking the final method. */
								verboseErrorCode = BCV_ERR_BAD_INVOKESPECIAL;
								goto _miscError;
							}
						}
					} /* fall through */

				case JBinvokevirtual:
					rc = isProtectedAccessPermitted (verifyData, J9ROMSTRINGREF_UTF8DATA(classRef), type,
													J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info), FALSE, &reasonCode);

					if (FALSE == rc) {
						if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
							goto _outOfMemoryError;
						}
						errorType = J9NLS_BCV_ERR_BAD_ACCESS_PROTECTED__ID;
						/* Jazz 82615: Set the error code when error occurs in checking the protected member access. */
						verboseErrorCode = BCV_ERR_BAD_ACCESS_PROTECTED;
						goto _miscError;
					}
					break;
				}

				if (bc != JBinvokeinterface) {
					utf8string = J9ROMSTRINGREF_UTF8DATA(classRef);

					rc = isClassCompatibleByName (verifyData, type, J9UTF8_DATA(utf8string), J9UTF8_LENGTH(utf8string), &reasonCode);
					if (FALSE == rc) {
						if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
							goto _outOfMemoryError;
						}
						errorType = J9NLS_BCV_ERR_RECEIVER_NOT_COMPATIBLE__ID;
						/* Jazz 82615: Store the index of the expected data type for later retrieval in classNameList */
						errorTargetType = convertClassNameToStackMapType(verifyData, J9UTF8_DATA(utf8string), J9UTF8_LENGTH(utf8string), 0, 0);
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack2;
					}
				} else {
					/* Need to ensure that there is at least an Object reference on the stack for the 
					 * invokeinterface receiver.  If the top of stack is a base type or TOP, then 
					 * throw a verify error.  The check for the receiver to be an interface occurs in
					 * the invokeinterface bytecode.
					 * Note: we need to check whether the Object reference on the stack is initialized
					 * so as to stop an uninitialized object from being addressed here by invokeinterface.
					 */
					if ((BCV_TAG_BASE_TYPE_OR_TOP == (type & BCV_TAG_MASK))
						|| J9_ARE_ANY_BITS_SET(type, BCV_SPECIAL)
					) {
						errorType = J9NLS_BCV_ERR_RECEIVER_NOT_COMPATIBLE__ID;
						/* Jazz 82615: Set the expected data type and the location of wrong data type
						 * on stack when the verification error occurs.
						 */
						errorTargetType = BCV_GENERIC_OBJECT;
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack2;
					}
				}
			}

			utf8string = ((J9UTF8 *) (J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef *) info))));
			stackTop = pushReturnType(verifyData, utf8string, stackTop);
			break;

		case RTV_PUSH_NEW:
			switch (bc) {
			case JBnew:
			case JBnewdup:
				/* put a uninitialized object of the correct type on the stack */
				PUSH(BCV_SPECIAL_NEW | (start << BCV_CLASS_INDEX_SHIFT));
				break;

			case JBnewarray:
				POP_TOS_INTEGER;	/* pop the size of the array */
				if (inconsistentStack) {
					/* Jazz 82615: Set the expected data type and the location of wrong data type
					 * on stack when the verification error occurs.
					 */
					errorTargetType = BCV_BASE_TYPE_INT;
					errorStackIndex = stackTop - liveStack->stackElements;
					goto _inconsistentStack;
				}

				index = PARAM_8(bcIndex, 1);
				type = (UDATA) newArrayParamConversion[index];
				PUSH(type);	/* arity of one implicit */
				break;

			case JBanewarray:
				POP_TOS_INTEGER;	/* pop the size of the array */
				if (inconsistentStack) {
					/* Jazz 82615: Set the expected data type and the location of wrong data type
					 * on stack when the verification error occurs.
					 */
					errorTargetType = BCV_BASE_TYPE_INT;
					errorStackIndex = stackTop - liveStack->stackElements;
					goto _inconsistentStack;
				}

				index = PARAM_16(bcIndex, 1);
				info = &constantPool[index];
				utf8string = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info);

				stackTop = pushClassType(verifyData, utf8string, stackTop);
				/* arity is one greater than signature */
				type = POP;
				/* Check for arity overflow */
				if ((type & BCV_ARITY_MASK) == BCV_ARITY_MASK) {
					errorType = J9NLS_BCV_ERR_ARRAY_TYPE_MISMATCH__ID;
					/* Jazz 82615: Set the error code and the arity in the case of arity overflow. */
					verboseErrorCode = BCV_ERR_ARRAY_ARITY_OVERFLOW;
					errorTempData = (type & BCV_ARITY_MASK);
					goto _miscError;
				}
				PUSH(( (UDATA)1 << BCV_ARITY_SHIFT) + type);
				break;

			case JBmultianewarray:
				/* points to cp entry for class of object to create */
				index = PARAM_16(bcIndex, 1);
				i1 = PARAM_8(bcIndex, 3);

				if (i1 == 0) {
					errorType = J9NLS_BCV_ERR_ARRAY_DIMENSION_MISMATCH__ID;
					/* Jazz 82615: Set the error code and the dimension of array if incorrect. */
					verboseErrorCode = BCV_ERR_ARRAY_DIMENSION_MISMATCH;
					errorTempData = i1;
					goto _miscError;
				}

				if ((stackTop - i1) < stackBase) {
					errorType = J9NLS_BCV_ERR_STACK_UNDERFLOW__ID;
					/* Jazz 82615: Set the error code when the verification error related to stack underflow occurs */
					verboseErrorCode = BCV_ERR_STACK_UNDERFLOW;
					/* Given that the actual data type involved has not yet been located through pop operation when stack underflow occurs,
					 * step back by one slot to the actual data type to be manipulated by the opcode.
					 */
					errorStackIndex = (stackTop - liveStack->stackElements) - 1;
					/* Always set to the location of the 1st data type on 'stack' to show up if stackTop <= stackBase */
					if (stackTop <= stackBase) {
						errorStackIndex = stackBase - liveStack->stackElements;
					}
					goto _miscError;
				}

				for (i2 = 0; i2 < i1; i2++) {
					POP_TOS_INTEGER;
					if (inconsistentStack) {
						/* Jazz 82615: Set the expected data type and the location of wrong data type
						 * on stack when the verification error occurs.
						 */
						errorTargetType = BCV_BASE_TYPE_INT;
						errorStackIndex = stackTop - liveStack->stackElements;
						goto _inconsistentStack;
					}
				}

				info = &constantPool[index];
				utf8string = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info);

				stackTop = pushClassType(verifyData, utf8string, stackTop);
				/* Silly test? */
				type = POP;
				if (type & BCV_TAG_BASE_ARRAY_OR_NULL) {
					i1--;
				} 
				if ((type >> BCV_ARITY_SHIFT) < (UDATA) i1) {
					errorType = J9NLS_BCV_ERR_ARRAY_DIMENSION_MISMATCH__ID;
					/* Jazz 82615: Set the error code when the dimension of array is incorrect. */
					verboseErrorCode = BCV_ERR_ARRAY_DIMENSION_MISMATCH;
					errorTempData = (type >> BCV_ARITY_SHIFT);
					goto _miscError;
				}
				PUSH(type);
				break;
			}
			break;

		case RTV_MISC:
		{
			IDATA reasonCode = 0;
			switch (bc) {
			case JBathrow:
				POP_TOS_OBJECT(type);
				rc = isClassCompatible(verifyData, type, (UDATA) (BCV_JAVA_LANG_THROWABLE_INDEX << BCV_CLASS_INDEX_SHIFT), &reasonCode);
				if (FALSE == rc) {
					if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
						goto _outOfMemoryError;
					}
					errorType = J9NLS_BCV_ERR_NOT_THROWABLE__ID;
					/* Jazz 82615: Store the index of the expected data type 'java/lang/invoke/MethodHandle' for later retrieval in classNameList */
					errorTargetType = (UDATA)(BCV_JAVA_LANG_THROWABLE_INDEX << BCV_CLASS_INDEX_SHIFT);
					errorStackIndex = stackTop - liveStack->stackElements;

					/* Jazz 82615: errorType may change in the case of _compatibleError */
					if (BCV_ERR_INTERNAL_ERROR == rc) {
						goto _compatibleError;
					} else {
						goto _inconsistentStack2;
					}
				}
				goto _newStack;
				break;

			case JBarraylength:
				POP_TOS_ARRAY(type);
				if (inconsistentStack) {
					/* Jazz 82615: Given that there is no way to know the information of the expected array type
					 * (e.g. base/object type, arity, etc), so set the error code (invalid array reference) and
					 * the location of wrong data type on stack when the verification error occurs.
					 */
					errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
					verboseErrorCode = BCV_ERR_INVALID_ARRAY_REFERENCE;
					errorTempData = type;
					errorStackIndex = stackTop - liveStack->stackElements;
					goto _miscError;
				}
				PUSH_INTEGER_CONSTANT;
				break;

			case JBtableswitch:
			case JBlookupswitch:
				POP_TOS_INTEGER;
				if (inconsistentStack) {
					/* Jazz 82615: Set the expected data type and the location of wrong data type
					 * on stack when the verification error occurs.
					 */
					errorTargetType = BCV_BASE_TYPE_INT;
					errorStackIndex = stackTop - liveStack->stackElements;
					goto _inconsistentStack;
				}
				index = (UDATA) ((4 - (pc & 3)) & 3);	/* consume padding */
				pc += index;
				bcIndex += index;
				pc += 8;
				CHECK_END;
				offset32 = (I_32) PARAM_32(bcIndex, 1);
				bcIndex += 4;
				target = offset32 + start;
				SAVE_STACKTOP(liveStack, stackTop);
				rc = findAndMatchStack (verifyData, target, start);
				if (BCV_SUCCESS != rc) {
					if (BCV_ERR_INSUFFICIENT_MEMORY == rc) {
						goto _outOfMemoryError;
					}
					goto _mapError;
				}

				if (bc == JBtableswitch) {
					i1 = (I_32) PARAM_32(bcIndex, 1);
					bcIndex += 4;
					pc += 4;
					i2 = (I_32) PARAM_32(bcIndex, 1);
					bcIndex += 4;

					pc += ((I_32)i2 - (I_32)i1 + 1) * 4;
					CHECK_END;

					/* Add the table switch destinations in reverse order to more closely mimic the order that people
					   (ie: the TCKs) expect you to load classes */
					bcIndex += (((I_32)i2 - (I_32)i1) * 4);	/* point at the last table switch entry */

					/* Count the entries */
					i2 = (I_32)i2 - (I_32)i1 + 1;
					for (i1 = 0; (I_32)i1 < (I_32)i2; i1++) {
						offset32 = (I_32) PARAM_32(bcIndex, 1);
						bcIndex -= 4;	/* back up to point at the previous table switch entry */
						target = offset32 + start;
						rc = findAndMatchStack (verifyData, target, start);
						if (BCV_SUCCESS != rc) {
							if (BCV_ERR_INSUFFICIENT_MEMORY == rc) {
								goto _outOfMemoryError;
							}
							goto _mapError;
						}
					}
				} else {
					i2 = (I_32) PARAM_32(bcIndex, 1);
					bcIndex += 4;

					pc += (I_32)i2 * 8;
					CHECK_END;
					for (i1 = 0; (I_32)i1 < (I_32)i2; i1++) {
						bcIndex += 4;
						offset32 = (I_32) PARAM_32(bcIndex, 1);
						bcIndex += 4;
						target = offset32 + start;
						rc = findAndMatchStack (verifyData, target, start);
						if (BCV_SUCCESS != rc) {
							if (BCV_ERR_INSUFFICIENT_MEMORY == rc) {
								goto _outOfMemoryError;
							}
							goto _mapError;
						}
					}
				}
				goto _newStack;
				break;

			case JBmonitorenter:
			case JBmonitorexit:
				/* Monitor instructions are allowed to accept an uninitialized object */
				POP_TOS_OBJECT_IN_MONITOR(type);
				if (inconsistentStack) {
					/* Jazz 82615: Set the expected data type and the location of wrong data type
					 * on stack when the verification error occurs.
					 */
					errorTargetType = BCV_GENERIC_OBJECT;
					errorStackIndex = stackTop - liveStack->stackElements;
					goto _inconsistentStack;
				}
				break;

			case JBcheckcast:
				index = PARAM_16(bcIndex, 1);
				POP_TOS_OBJECT(type);
				if (inconsistentStack) {
					/* Jazz 82615: Set the expected data type and the location of wrong data type
					 * on stack when the verification error occurs.
					 */
					errorTargetType = BCV_GENERIC_OBJECT;
					errorStackIndex = stackTop - liveStack->stackElements;
					goto _inconsistentStack;
				}
				info = &constantPool[index];
				utf8string = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *) info);
				stackTop = pushClassType(verifyData, utf8string, stackTop);
				break;

			case JBinstanceof:
				POP_TOS_OBJECT(type);
				if (inconsistentStack) {
					/* Jazz 82615: Set the expected data type and the location of wrong data type
					 * on stack when the verification error occurs.
					 */
					errorTargetType = BCV_GENERIC_OBJECT;
					errorStackIndex = stackTop - liveStack->stackElements;
					goto _inconsistentStack;
				}
				PUSH_INTEGER_CONSTANT;
				break;
			}
			break;
		}
		case RTV_POP_2_PUSH_INT:
			POP_TOS_TYPE_EQUAL( type, type1 );
			if (inconsistentStack) {
				/* Jazz 82615: Set the 2nd slot of long/double type (already in type1) and
				 * the location of wrong data type on stack when the verification error occurs.
				 */
				errorTargetType = BCV_BASE_TYPE_TOP;
				/* Save the long/double type to errorTempData (verifyData->errorTempData)
				 * so as to ensure the wrong data type is the 2nd slot of long/double type
				 * in the error message framework rather than a 'top' type.
				 */
				errorTempData = type1;
				/* The location of wrong data type needs to adjusted back to the right place
				 * because it pops twice from the stack for the wide type.
				 */
				errorStackIndex = (stackTop - liveStack->stackElements) + 1;
				goto _inconsistentStack;
				/* Jazz 82615: the second inconsistent stack case in POP_TOS_TYPE_EQUAL should be addressed here */
			} else if (inconsistentStack2) {
				errorTargetType = type1;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _inconsistentStack;
			}
			POP_TOS_TYPE_EQUAL( type, type1 );
			if (inconsistentStack) {
				/* Jazz 82615: Set the 2nd slot of long/double type (already in type1) and
				 * the location of wrong data type on stack when the verification error occurs.
				 */
				errorTargetType = BCV_BASE_TYPE_TOP;
				/* Save the long/double type to errorTempData (verifyData->errorTempData)
				 * so as to ensure the wrong data type is the 2nd slot of long/double type
				 * in the error message framework rather than a 'top' type.
				 */
				errorTempData = type1;
				/* The location of wrong data type needs to adjusted back to the right place
				 * because it pops twice from the stack for the wide type.
				 */
				errorStackIndex = (stackTop - liveStack->stackElements) + 1;
				goto _inconsistentStack;
				/* Jazz 82615: the second inconsistent stack case in POP_TOS_TYPE_EQUAL should be addressed here */
			} else if (inconsistentStack2) {
				errorTargetType = type1;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _inconsistentStack;
			}
			PUSH_INTEGER_CONSTANT;
			break;

		case RTV_BYTECODE_POP:
			POP_TOS_SINGLE(type);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _miscError;
			}
			break;

		case RTV_BYTECODE_POP2:
			POP_TOS_PAIR(temp1, temp2);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				/* The pair of data types should be printed out once detected as invalid */
				errorStackIndex = stackTop - liveStack->stackElements + 1;
				goto _miscError;
			}
			break;

		case RTV_BYTECODE_DUP:
			POP_TOS_SINGLE(type);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _miscError;
			}
			PUSH(type);
			PUSH(type);
			break;

		case RTV_BYTECODE_DUPX1:
			POP_TOS_SINGLE(type);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _miscError;
			}
			POP_TOS_SINGLE(temp1);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _miscError;
			}
			PUSH(type);
			PUSH(temp1);
			PUSH(type);
			break;

		case RTV_BYTECODE_DUPX2:
			POP_TOS_SINGLE(type);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _miscError;
			}
			POP_TOS_PAIR(temp1, temp2); /* use nverifyPop2 ?? */
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				/* The pair of data types should be printed out once detected as invalid */
				errorStackIndex = stackTop - liveStack->stackElements + 1;
				goto _miscError;
			}
			PUSH(type);
			PUSH(temp2);
			PUSH(temp1);
			PUSH(type);
			break;

		case RTV_BYTECODE_DUP2:
			POP_TOS_PAIR(temp1, temp2);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				/* The pair of data types should be printed out once detected as invalid */
				errorStackIndex = stackTop - liveStack->stackElements + 1;
				goto _miscError;
			}
			PUSH(temp2);
			PUSH(temp1);
			PUSH(temp2);
			PUSH(temp1);
			break;

		case RTV_BYTECODE_DUP2X1:
			POP_TOS_PAIR(type, temp1);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				/* The pair of data types should be printed out once detected as invalid */
				errorStackIndex = stackTop - liveStack->stackElements + 1;
				goto _miscError;
			}
			POP_TOS_SINGLE(temp2);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _miscError;
			}
			PUSH(temp1);
			PUSH(type);
			PUSH(temp2);
			PUSH(temp1);
			PUSH(type);
			break;

		case RTV_BYTECODE_DUP2X2:
			POP_TOS_PAIR(type, temp1);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				/* The pair of data types should be printed out once detected as invalid */
				errorStackIndex = stackTop - liveStack->stackElements + 1;
				goto _miscError;
			}
			POP_TOS_PAIR(temp2, temp3);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				/* The pair of data types should be printed out once detected as invalid */
				errorStackIndex = stackTop - liveStack->stackElements + 1;
				goto _miscError;
			}
			/* should probably do more checking to avoid bogus dup's of long/double pairs */
			PUSH(temp1);
			PUSH(type);
			PUSH(temp3);
			PUSH(temp2);
			PUSH(temp1);
			PUSH(type);
			break;

		case RTV_BYTECODE_SWAP:
			POP_TOS_SINGLE(type);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _miscError;
			}
			POP_TOS_SINGLE(temp1);
			if (inconsistentStack) {
				/* Jazz 82615: Set the error code (a non-top type is expected on stack). */
				errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
				verboseErrorCode = BCV_ERR_WRONG_TOP_TYPE;
				errorStackIndex = stackTop - liveStack->stackElements;
				goto _miscError;
			}
			PUSH(type);
			PUSH(temp1);
			break;

		default:
			errorType = J9NLS_BCV_ERR_BC_UNKNOWN__ID;
			/* Jazz 82615: Set the error code in the case of unrecognized opcode. */
			verboseErrorCode = BCV_ERR_BAD_BYTECODE;
			goto _miscError;
		}
		continue;

_newStack:

		/* Do I need a new stack? */
		if (pc < length) {

			/* Do I have more stacks */
			if ((UDATA) nextStackPC < length) {

				/* Is the next stack for the next bytecode */
				if (pc != (UDATA) nextStackPC) {
					if (J9ROMCLASS_HAS_VERIFY_DATA(romClass) && (!verifyData->ignoreStackMaps)) {
						/* the nextStack is not the wanted stack */
						/* Flow verification allows dead code without a map */
						/* Jazz 82615: Set the error code, the next pc value, and the pc value of unexpected stackmap frame. */
						errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
						verboseErrorCode = BCV_ERR_WRONG_STACKMAP_FRAME;
						errorTempData = nextStackPC;
						start = pc;
						goto _miscError;
					} else {
						/* skip dead code to nextStackPC */
						pc = nextStackPC;
					}
				}

				/* Load the next stack into the live stack */
				SAVE_STACKTOP(liveStack, stackTop);
				memcpy((UDATA *) liveStack, (UDATA *) currentMapData, verifyData->stackSize);
				isNextStack = TRUE;
				RELOAD_LIVESTACK;

				/* Start with uninitialized_this flag for the current map data */
				liveStack->uninitializedThis = currentMapData->uninitializedThis;

				/* New next stack */
				currentMapData = nextStack (verifyData, &nextMapIndex, &nextStackPC);
			} else {
				/* no map available */
				if (J9ROMCLASS_HAS_VERIFY_DATA(romClass) && (!verifyData->ignoreStackMaps)) {
					/* Flow verification allows dead code without a map */
					/* Jazz 82615: Set the error code when nextStackPC goes beyond the stackmap table. */
					errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
					verboseErrorCode = BCV_ERR_NO_STACKMAP_FRAME;
					errorTempData = (UDATA)nextStackPC;
					goto _miscError;
				} else {
					if ((action != RTV_RETURN) 
					&& (bc !=JBathrow) 
					&& (bc !=JBtableswitch) 
					&& (bc !=JBlookupswitch)
					&& (bc !=JBgoto)
					&& (bc !=JBgotow)
					) {
						/* Jazz 82615: Set the error code when it reaches unterminated dead code. */
						errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
						verboseErrorCode = BCV_ERR_DEAD_CODE;
						goto _miscError;
					}
					/* no more maps, skip remaining code as dead */
					pc = length;
				}
			}
		}
	}

	/* StackMap/StackMapTable attribute treat all code as live */
	/* Flow verification allows unterminated dead code */
	if ((action != RTV_RETURN) 
	&& (bc !=JBathrow) 
	&& (bc !=JBtableswitch) 
	&& (bc !=JBlookupswitch)
	&& (bc !=JBgoto)
	&& (bc !=JBgotow)
	) {
		/* Jazz 82615: Set the error code when it reaches unterminated dead code. */
		errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
		verboseErrorCode = BCV_ERR_DEAD_CODE;
		goto _miscError;
	}

	Trc_RTV_verifyBytecodes_Exit(verifyData->vmStruct);
	
	return BCV_SUCCESS;
#undef CHECK_END

	errorType = J9NLS_BCV_ERR_UNEXPECTED_EOF__ID;	/* should never reach here */
	goto _verifyError;

_mapError:
	/* TODO: add tracepoint that indicates a map error occured:
	 * 	class, method, pc
	 */

	errorType = J9NLS_BCV_ERR_INCONSISTENT_STACK__ID;
_compatibleError:
	if (rc == BCV_ERR_INTERNAL_ERROR) {
		errorType = J9NLS_BCV_ERR_CLASS_LOAD_FAILED__ID;
	}

_verifyError:
	/* Jazz 82615: Store the error code in the case of CHECK_STACK_UNDERFLOW */
	if ((stackTop < stackBase) && (J9NLS_BCV_ERR_STACK_UNDERFLOW__ID == errorType)) {
		/* Reset to the location of the 1st data type on 'stack' in the case of stack underflow to show up */
		errorStackIndex = stackBase - liveStack->stackElements;
		storeVerifyErrorData(verifyData, BCV_ERR_STACK_UNDERFLOW, (U_32)errorStackIndex, (UDATA)-1, (UDATA)-1, start);
	}

	Trc_RTV_verifyBytecodes_VerifyError(verifyData->vmStruct,
		errorType, 
		(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
		J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
		(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
		J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
		start);

	Trc_RTV_verifyBytecodes_VerifyErrorBytecode(verifyData->vmStruct, 
		(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
		J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
		(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
		J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
		(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
		J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
		errorType, start, start, *(code + start));
	BUILD_VERIFY_ERROR(errorModule, errorType);

	return BCV_ERR_INTERNAL_ERROR;

_miscError:
	/* Jazz 82615: Store the error code and the location of wrong data type on stack in the case of the verification error unrelated to incompatible type */
	storeVerifyErrorData(verifyData, (I_16)verboseErrorCode, (U_32)errorStackIndex, (UDATA)-1, errorTempData, start);
	goto _verifyError;

_outOfMemoryError:
	errorType = J9NLS_BCV_ERR_VERIFY_OUT_OF_MEMORY__ID;
	Trc_RTV_verifyBytecodes_OutOfMemoryException(verifyData->vmStruct,
		errorType,
		(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
		J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
		(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
		J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
		start);
	BUILD_VERIFY_ERROR(errorModule, errorType);
	return BCV_ERR_INSUFFICIENT_MEMORY;
}


/*
 * returns BCV_SUCCESS on success
 * returns BCV_FAIL on error
 * returns BCV_ERR_INSUFFICIENT_MEMORY on OOM
 */
static IDATA
verifyExceptions (J9BytecodeVerificationData *verifyData)
{
	J9ROMMethod *romMethod = verifyData->romMethod;
	J9ROMClass *romClass = verifyData->romClass;
	J9ExceptionInfo *exceptionInfo;
	J9ExceptionHandler *handler;
	J9ROMConstantPoolItem *romConstantPool;
	J9UTF8 *catchName;
	UDATA i, catchClass;
	IDATA rc = BCV_SUCCESS;
	IDATA reasonCode = 0;

	Trc_RTV_verifyExceptions_Entry(verifyData->vmStruct, 
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)));
	
	/* Verify catch types are throwable */
	exceptionInfo = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
	handler = J9EXCEPTIONINFO_HANDLERS(exceptionInfo);
	romConstantPool = J9_ROM_CP_FROM_ROM_CLASS(verifyData->romClass);

	for (i = 0; i < exceptionInfo->catchCount; i++, handler++) {
		if (handler->exceptionClassIndex) {
			IDATA tempRC = 0;

			catchName = J9ROMSTRINGREF_UTF8DATA((J9ROMStringRef *)(&romConstantPool [handler->exceptionClassIndex]));
			catchClass = convertClassNameToStackMapType(verifyData, J9UTF8_DATA(catchName), J9UTF8_LENGTH(catchName), 0, 0);
			tempRC = isClassCompatible (verifyData, catchClass, (UDATA) (BCV_JAVA_LANG_THROWABLE_INDEX << BCV_CLASS_INDEX_SHIFT), &reasonCode);
			if (FALSE == tempRC) {
				if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
					Trc_RTV_verifyExceptions_OutOfMemoryException(verifyData->vmStruct,
						(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)));
					rc = BCV_ERR_INSUFFICIENT_MEMORY;
					break;
				} else {
					Trc_RTV_verifyExceptions_VerifyError(verifyData->vmStruct,
						(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_NAME(romMethod)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(romMethod)),
						i, J9UTF8_LENGTH(catchName), J9UTF8_DATA(catchName));
					rc = BCV_FAIL;
					break;
				}
			}
		}
	}

	Trc_RTV_verifyExceptions_Exit(verifyData->vmStruct);
	
	return rc;
}



/*
 * returns BCV_SUCCESS on success
 * returns BCV_FAIL on errors
 * returns BCV_ERR_INSUFFICIENT_MEMORY on OOM
 * TODO: get rid of all the "this should never happen, checked by cfreader.c checks" */
IDATA
j9rtv_verifyArguments (J9BytecodeVerificationData *verifyData, J9UTF8 * utf8string, UDATA ** pStackTop)
{
	IDATA argCount, index;
	UDATA arity, objectType, baseType;
	UDATA *stackTop;
	U_8 *signature;
	U_16 length;
	U_8 *string;
	IDATA rc;
	IDATA mrc = BCV_SUCCESS;
	BOOLEAN boolType = FALSE;
	J9BranchTargetStack *liveStack = (J9BranchTargetStack *) verifyData->liveStack;

	Trc_RTV_j9rtv_verifyArguments_Entry(verifyData->vmStruct, J9UTF8_LENGTH(utf8string), J9UTF8_DATA(utf8string));

	stackTop = *pStackTop;

	/* Arguments are in reverse order on the stack, we must pre-pop the stack */
	argCount = (IDATA) getSendSlotsFromSignature(J9UTF8_DATA(utf8string));

	/*
	 * Jazz 82615: When the number of arguments is greater than the whole stack size,
	 * Set stackTop with the address at liveStack->stackElements to avoid compare against garbage that doesn't exist on stack.
	 */
	if ((IDATA)(stackTop - liveStack->stackElements) < argCount) {
		stackTop = liveStack->stackElements;
	} else {
		stackTop = stackTop - argCount;
	}

	signature = J9UTF8_DATA(utf8string) + 1;		/* skip open bracket */

	/* Walk the arguments and verify the stack */
	for (index = 0; index < argCount; index++) {

		arity = 0;
		objectType = 0;
		boolType = FALSE;

		/* Look for an array */
		if (*signature == '[') {

			while (*signature == '[') {
				signature++;
				arity++;
			}

		}

		/* Object or array */
		if ((*signature == 'L') || arity) {
			IDATA reasonCode = 0;

			/* Object array */
			if (*signature == 'L') {
				signature++;
				string = signature;	/* remember the start of the string */
				while (*signature++ != ';');
				length = (U_16) (signature - string - 1);
				objectType = convertClassNameToStackMapType(verifyData, string, length, 0, arity);

			/* Base type array */
			} else {

				if ((*signature < 'A') || (*signature > 'Z')) {
					/* Should never happen - bad sig. caught by cfreader.c checkMethodSignature */
					Trc_RTV_j9rtv_verifyArguments_Unreachable(verifyData->vmStruct, 
							(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
							J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
							(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
							J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
							(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
							J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
							__LINE__);
					mrc = BCV_FAIL;
					break;
				}

				objectType = (UDATA) baseTypeCharConversion[*signature - 'A'];
				signature++;

				if (!objectType) {
					/* Should never happen - bad sig. caught by cfreader.c checkMethodSignature */
					Trc_RTV_j9rtv_verifyArguments_Unreachable(verifyData->vmStruct, 
							(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
							J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
							(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
							J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
							(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
							J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
							__LINE__);
					mrc = BCV_FAIL;
					break;
				}

				arity--;
				objectType |= BCV_TAG_BASE_ARRAY_OR_NULL;
			}

			/* Check the object  */
			objectType |= (arity << BCV_ARITY_SHIFT);
			rc = isClassCompatible (verifyData, stackTop[index], objectType, &reasonCode);

			if (FALSE == rc) {
				if (BCV_ERR_INSUFFICIENT_MEMORY == reasonCode) {
					Trc_RTV_j9rtv_verifyArguments_OutOfMemoryException(verifyData->vmStruct,
						(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)));
					mrc = BCV_ERR_INSUFFICIENT_MEMORY;
					break;
				} else if (BCV_ERR_INACCESSIBLE_CLASS == reasonCode) {
					Trc_RTV_j9rtv_verifyArguments_InaccessibleClass(verifyData->vmStruct,
						(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)));
					mrc = BCV_ERR_INACCESSIBLE_CLASS;
					break;
				} else {
					Trc_RTV_j9rtv_verifyArguments_ObjectMismatch(verifyData->vmStruct,
						(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
						index, J9UTF8_LENGTH(utf8string), J9UTF8_DATA(utf8string), stackTop[index]);
					mrc = BCV_FAIL;
					verifyData->errorDetailCode = BCV_ERR_INCOMPATIBLE_TYPE; /* failure - object type mismatch */
					verifyData->errorTargetType = objectType;
					break;
				}
			}

		/* Base type */
		} else {

			if ((*signature < 'A') || (*signature > 'Z')) {
				/* Should never happen - bad sig. caught by cfreader.c checkMethodSignature */
				Trc_RTV_j9rtv_verifyArguments_Unreachable(verifyData->vmStruct,
						(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
						__LINE__);
				mrc = BCV_FAIL;
				break;
			}

			baseType = (UDATA) argTypeCharConversion[*signature - 'A'];
			/* Jazz 82615: Tagged as 'bool' when argTypeCharConversion returns BCV_BASE_TYPE_INT for type 'Z'
			 * Note: for base type, type B (byte) and type Z (boolean) correspond to the verification type int.
			 * In such case, there is no way to determine whether the original type is byte or boolean after the
			 * conversion of argument type. To ensure type Z is correctly generated to the error messages, we need
			 * to filter it out here by checking the type in signature.
			 */
			if ('Z' == *signature) {
				boolType = TRUE;
			}
			signature++;

			if (!baseType) {
				/* Should never happen - bad sig. caught by cfreader.c checkMethodSignature */
				Trc_RTV_j9rtv_verifyArguments_Unreachable(verifyData->vmStruct, 
						(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
						__LINE__);
				mrc = BCV_FAIL;
				break;
			}

			if (stackTop[index] != baseType) {
				Trc_RTV_j9rtv_verifyArguments_PrimitiveMismatch(verifyData->vmStruct, 
						(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
						(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
						J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
						index, J9UTF8_LENGTH(utf8string), J9UTF8_DATA(utf8string), stackTop[index]);
				mrc = BCV_FAIL;
				verifyData->errorDetailCode = BCV_ERR_INCOMPATIBLE_TYPE; /* failure - primitive mismatch */
				verifyData->errorTargetType = (TRUE == boolType) ? BCV_BASE_TYPE_BOOL : baseType;
				break;
			}

			if (baseType & BCV_WIDE_TYPE_MASK) {
				index++;
				if (stackTop[index] != BCV_BASE_TYPE_TOP) {
					Trc_RTV_j9rtv_verifyArguments_WidePrimitiveMismatch(verifyData->vmStruct, 
							(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
							J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
							(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
							J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
							(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
							J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
							index, J9UTF8_LENGTH(utf8string), J9UTF8_DATA(utf8string));
					mrc = BCV_FAIL;
					verifyData->errorDetailCode = BCV_ERR_INCOMPATIBLE_TYPE; /* failure - wide primitive mismatch */
					verifyData->errorTargetType = (TRUE == boolType) ? BCV_BASE_TYPE_BOOL : baseType;
					break;
				}
			}
		}
	}

	/* errorDetailCode has been initialized with 0 (SUCCESS).
	 * Verification error data related to argument mismatch should be saved here when any verification error is detected above.
	 */
	if (verifyData->errorDetailCode < 0) {
		storeArgumentErrorData(verifyData, (U_32)(&stackTop[index] - liveStack->stackElements), (U_16)index);
	}

	*pStackTop = stackTop;

	Trc_RTV_j9rtv_verifyArguments_Exit(verifyData->vmStruct, mrc);

	return mrc;
}

/* Return a pointer to the next stack, update the nextMapIndex and the nextStackPC */

static J9BranchTargetStack * 
nextStack (J9BytecodeVerificationData *verifyData, UDATA *nextMapIndex, IDATA *nextStackPC)
{
	J9BranchTargetStack * returnStack = NULL;
	/* Default nextStackPC is the end of the method */
	*nextStackPC = J9_BYTECODE_SIZE_FROM_ROM_METHOD(verifyData->romMethod);
	
	while (*nextMapIndex < (UDATA) verifyData->stackMapsCount) {
		returnStack = BCV_INDEX_STACK (*nextMapIndex);
		(*nextMapIndex)++;
		
		/* skip unused stack maps */  
		/* simulateStack can have unused stack map targets - they result from being in dead code */
		if (returnStack->stackBaseIndex != -1) {
			*nextStackPC = returnStack->pc;
			break;
		}
	}
	Trc_RTV_nextStack_Result(verifyData->vmStruct, 
			(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
			J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
			(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
			J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
			verifyData->stackMapsCount, *nextMapIndex, *nextStackPC, 
			J9_BYTECODE_SIZE_FROM_ROM_METHOD(verifyData->romMethod));
	return returnStack;
}

/* Return the next pc in the method that is an exception handler start address */

static IDATA
nextExceptionStart (J9BytecodeVerificationData *verifyData, J9ROMMethod *romMethod, IDATA lastPC)
{
	/* Method size */
	IDATA nextPC = J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod);

	if (romMethod->modifiers & CFR_ACC_HAS_EXCEPTION_INFO) {
		J9ExceptionInfo *exceptionInfo = J9_EXCEPTION_DATA_FROM_ROM_METHOD(romMethod);
		J9ExceptionHandler *handler = J9EXCEPTIONINFO_HANDLERS(exceptionInfo);
		UDATA i;

		for (i = exceptionInfo->catchCount; i; i--, handler++) {
			if (((IDATA) handler->startPC) > lastPC) {
				if (handler->startPC < (UDATA) nextPC) {
					nextPC = handler->startPC;
				}
			}
		}
		Trc_RTV_nextExceptionStart_Result(verifyData->vmStruct, 
				(UDATA) J9UTF8_LENGTH(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				J9UTF8_DATA(J9ROMCLASS_CLASSNAME(verifyData->romClass)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_NAME(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_NAME(verifyData->romMethod)),
				(UDATA) J9UTF8_LENGTH(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				J9UTF8_DATA(J9ROMMETHOD_SIGNATURE(verifyData->romMethod)),
				exceptionInfo->catchCount, lastPC, nextPC, 
				J9_BYTECODE_SIZE_FROM_ROM_METHOD(romMethod));
	}
	return nextPC;
}


/**
 * Store the failure info regarding mismatched argument in method signature to the
 * J9BytecodeVerificationData structure for outputting detailed error message
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @param errorCurrentFramePosition - the location of type data in the current frame when error occurs
 * @param errorArgumentIndex - index to the argument of method signature when error occurs
 */
static void
storeArgumentErrorData (J9BytecodeVerificationData * verifyData, U_32 errorCurrentFramePosition, U_16 errorArgumentIndex)
{
	verifyData->errorCurrentFramePosition = errorCurrentFramePosition;
	verifyData->errorArgumentIndex = errorArgumentIndex;
}

/**
 * Postpone storing the method info to the checking of return code after the mismatched argument in method signature is detected
 * J9BytecodeVerificationData structure for outputting detailed error message
 * @param verifyData - pointer to J9BytecodeVerificationData
 * @param errorClassString - pointer to class name
 * @param errorMethodString - pointer to method name
 * @param errorSignatureString - pointer to signature string
 * @param currentPC - current pc value
 */
static void
storeMethodInfo (J9BytecodeVerificationData * verifyData, J9UTF8* errorClassString, J9UTF8* errorMethodString, J9UTF8* errorSignatureString, IDATA currentPC)
{
	J9BranchTargetStack *liveStack = (J9BranchTargetStack *) verifyData->liveStack;
	verifyData->errorClassString = errorClassString;
	verifyData->errorMethodString = errorMethodString;
	verifyData->errorSignatureString = errorSignatureString;

	/* Jazz 82615: Set liveStack->pc to the current pc value in the current frame (liveStack)
	 * info of the detailed error message.
	 */
	liveStack->pc = (UDATA)currentPC;
}
