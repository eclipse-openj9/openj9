/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#include "j9.h"
#ifdef J9VM_INTERP_STACKWALK_TRACING
#define walkFrame walkFrameVerbose
#define walkStackFrames walkStackFramesVerbose
#define jitWalkStackFrames jitWalkStackFramesVerbose
#define getLocalsMap getLocalsMapVerbose
#define walkBytecodeFrameSlots walkBytecodeFrameSlotsVerbose
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
#include "linearswalk.h"
#endif
#ifndef J9VM_OUT_OF_PROCESS
#include "j9modron.h"
#include "ut_j9vrb.h"
#endif
#else
#include "ut_j9vm.h"
#endif

#include "j9protos.h"
#ifdef J9VM_INTERP_NATIVE_SUPPORT
#include "jitprotos.h"
#endif

#ifndef J9VM_INTERP_STACKWALK_TRACING
#define jitWalkStackFrames walkState->walkThread->javaVM->jitWalkStackFrames
#endif

#include "rommeth.h"
#include "j9consts.h"
#include "stackwalk.h"
#include "j9cp.h"
#include "j9vmnls.h"
#include "vm_internal.h"
#include "stackmap_api.h"

#include <string.h>

#if (defined(J9VM_INTERP_STACKWALK_TRACING))
static void printFrameType (J9StackWalkState * walkState, char * frameType);

#if (!defined(J9VM_OUT_OF_PROCESS))
static void initializeObjectSlotBitVector (J9StackWalkState * walkState);
static void sniffAndWhack (J9StackWalkState * walkState);
static void sniffAndWhackPointer (J9StackWalkState * walkState, j9object_t *slotPointer);
static void sniffAndWhackIterator (J9VMThread * currentThread, J9StackWalkState * walkState, j9object_t * objectSlot, const void * stackLocation);
static void emptySniffAndWhackIterator (J9VMThread * currentThread, J9StackWalkState * walkState, j9object_t * objectSlot, const void * stackLocation);
#if (defined(J9VM_INTERP_NATIVE_SUPPORT))
static void sniffAndWhackELS (J9StackWalkState * walkState);
#endif /* INTERP_NATIVE_SUPPORT */
#endif /* J9VM_!OUT_OF_PROCESS */
#endif /* J9VM_INTERP_STACKWALK_TRACING */

#if (defined(J9VM_INTERP_NATIVE_SUPPORT))
static void walkJITResolveFrame (J9StackWalkState * walkState);
static void walkJITJNICalloutFrame (J9StackWalkState * walkState);
#endif /* J9VM_INTERP_NATIVE_SUPPORT */

static void walkJNICallInFrame (J9StackWalkState * walkState);
static void walkGenericSpecialFrame (J9StackWalkState * walkState);
static void walkIndirectDescribedPushes (J9StackWalkState * walkState, UDATA * highestIndirectSlot, UDATA slotCount, U_32 * descriptionSlots);
static void walkMethodFrame (J9StackWalkState * walkState);
static void walkMethodTypeFrame(J9StackWalkState * walkState);
static void walkJNIRefs (J9StackWalkState * walkState, UDATA * currentRef, UDATA refCount);
static void walkBytecodeFrame (J9StackWalkState * walkState);
static void walkDescribedPushes (J9StackWalkState * walkState, UDATA * highestSlot, UDATA slotCount, U_32 * descriptionSlots, UDATA argCount);
static void walkObjectPushes (J9StackWalkState * walkState);
static void walkPushedJNIRefs (J9StackWalkState * walkState);
static void getStackMap (J9StackWalkState * walkState, J9ROMClass * romClass, J9ROMMethod * romMethod, UDATA offsetPC, UDATA pushCount, U_32 *result);
static void getLocalsMap (J9StackWalkState * walkState, J9ROMClass * romClass, J9ROMMethod * romMethod, UDATA offsetPC, U_32 * result, UDATA argTempCount, UDATA alwaysLocalMap);
#if (!defined(J9VM_OUT_OF_PROCESS)) 
static UDATA allocateCache (J9StackWalkState * walkState);
static void dropToCurrentFrame (J9StackWalkState * walkState);
#endif /* J9VM_!OUT_OF_PROCESS */

/* The minimum number of stack slots that a stack frame can occupy */
#define J9_STACKWALK_MIN_FRAME_SLOTS (OMR_MIN(sizeof(J9JITFrame), sizeof(J9SFStackFrame)) / sizeof(UDATA))

UDATA  walkStackFrames(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	UDATA rc = (walkState->walkThread->privateFlags & J9_PRIVATE_FLAGS_STACK_CORRUPT) ? J9_STACKWALK_RC_STACK_CORRUPT : J9_STACKWALK_RC_NONE;
	J9Method * nextLiterals;
	UDATA * nextA0;
	UDATA savedFlags = walkState->flags;
	J9StackWalkState * oldState = NULL;
#if defined(J9VM_INTERP_STACKWALK_TRACING) && !defined(J9VM_OUT_OF_PROCESS)
	void  (*savedOSlotIterator) (struct J9VMThread * vmThread, struct J9StackWalkState * walkState, j9object_t * objectSlot, const void * stackLocation) =
		walkState->objectSlotWalkFunction;

	Trc_VRB_WalkStackFrames_Entry(currentThread, walkState->walkThread, walkState->flags);
#endif

	if (J9_ARE_ANY_BITS_SET(walkState->flags, J9_STACKWALK_RESUME)) {
		if (NULL != walkState->jitInfo) {
			goto resumeJitWalk;
		}
		walkState->flags &= ~J9_STACKWALK_RESUME;
		goto resumeInterpreterWalk;
	}
	if (currentThread != NULL) {
		oldState = currentThread->activeWalkState;
		currentThread->activeWalkState = walkState;
	}

	walkState->currentThread = currentThread;
	walkState->cache = NULL;
	walkState->framesWalked = 0;
	walkState->previousFrameFlags = 0;
	walkState->arg0EA = walkState->walkThread->arg0EA;
	walkState->pcAddress = &(walkState->walkThread->pc);
	walkState->pc = walkState->walkThread->pc;
	walkState->nextPC = NULL;
	walkState->walkSP = walkState->walkThread->sp;
	walkState->literals = walkState->walkThread->literals;
	walkState->argCount = 0;
	walkState->linearSlotWalker = NULL;
	walkState->stackMap = NULL;
	walkState->inlineMap = NULL;
	walkState->inlinedCallSite = NULL;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	walkState->jitInfo = NULL;
	walkState->inlineDepth = 0;
	walkState->inlinerMap = NULL;
	walkState->walkedEntryLocalStorage = walkState->walkThread->entryLocalStorage;
	walkState->i2jState = walkState->walkedEntryLocalStorage ? &(walkState->walkedEntryLocalStorage->i2jState) : NULL;
	walkState->j2iFrame = walkState->walkThread->j2iFrame;
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
	walkState->decompilationStack = walkState->walkThread->decompilationStack;
	walkState->decompilationRecord = NULL;
	walkState->resolveFrameFlags = 0;
#endif
#endif


#ifdef J9VM_INTERP_STACKWALK_TRACING
	swPrintf(walkState, 1, "\n");
	swPrintf(walkState, 1, "*** BEGIN STACK WALK, flags = %p, walkThread = %p, walkState = %p ***\n", walkState->flags, REMOTE_ADDR(walkState->walkThread), walkState);

	if (walkState->flags & J9_STACKWALK_RESUME) swPrintf(walkState, 2, "\tJ9_STACKWALK_RESUME\n");
	if (walkState->flags & J9_STACKWALK_START_AT_JIT_FRAME) swPrintf(walkState, 2, "\tSTART_AT_JIT_FRAME\n");
	if (walkState->flags & J9_STACKWALK_CACHE_CPS) swPrintf(walkState, 2, "\tCACHE_CPS\n");
	if (walkState->flags & J9_STACKWALK_CACHE_PCS) swPrintf(walkState, 2, "\tCACHE_PCS\n");
	if (walkState->flags & J9_STACKWALK_COUNT_SPECIFIED) swPrintf(walkState, 2, "\tCOUNT_SPECIFIED\n");
	if (walkState->flags & J9_STACKWALK_INCLUDE_ARRAYLET_LEAVES) swPrintf(walkState, 2, "\tINCLUDE_ARRAYLET_LEAVES\n");
	if (walkState->flags & J9_STACKWALK_INCLUDE_NATIVES) swPrintf(walkState, 2, "\tINCLUDE_NATIVES\n");
	if (walkState->flags & J9_STACKWALK_INCLUDE_CALL_IN_FRAMES) swPrintf(walkState, 2, "\tINCLUDE_CALL_IN_FRAMES\n");
	if (walkState->flags & J9_STACKWALK_ITERATE_FRAMES) swPrintf(walkState, 2, "\tITERATE_FRAMES\n");
	if (walkState->flags & J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES) swPrintf(walkState, 2, "\tITERATE_HIDDEN_JIT_FRAMES\n");
	if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) swPrintf(walkState, 2, "\tITERATE_O_SLOTS\n");
	if (walkState->flags & J9_STACKWALK_ITERATE_METHOD_CLASS_SLOTS) swPrintf(walkState, 2, "\tITERATE_METHOD_CLASS_SLOTS\n");
	if (walkState->flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) swPrintf(walkState, 2, "\tMAINTAIN_REGISTER_MAP\n");
	if (walkState->flags & J9_STACKWALK_SKIP_INLINES) swPrintf(walkState, 2, "\tSKIP_INLINES\n");
	if (walkState->flags & J9_STACKWALK_VISIBLE_ONLY) swPrintf(walkState, 2, "\tVISIBLE_ONLY\n");
	if (walkState->flags & J9_STACKWALK_WALK_TRANSLATE_PC) swPrintf(walkState, 2, "\tWALK_TRANSLATE_PC\n");
	if (walkState->flags & J9_STACKWALK_HIDE_EXCEPTION_FRAMES) swPrintf(walkState, 2, "\tHIDE_EXCEPTION_FRAMES\n");
	if (walkState->flags & J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET) swPrintf(walkState, 2, "\tRECORD_BYTECODE_PC_OFFSET\n");
	if (walkState->flags & J9_STACKWALK_DO_NOT_SNIFF_AND_WHACK) swPrintf(walkState, 2, "\tDO_NOT_SNIFF_AND_WHACK\n");
	if (walkState->flags & J9_STACKWALK_CHECK_I_SLOTS_FOR_OBJECTS) swPrintf(walkState, 2, "\tCHECK_I_SLOTS_FOR_OBJECTS\n");
	if (walkState->flags & J9_STACKWALK_SAVE_STACKED_REGISTERS) swPrintf(walkState, 2, "\tSAVE_STACKED_REGISTERS\n");

#ifndef J9VM_OUT_OF_PROCESS
	if ((walkState->flags & (J9_STACKWALK_MAINTAIN_REGISTER_MAP | J9_STACKWALK_INCLUDE_CALL_IN_FRAMES)) == (J9_STACKWALK_MAINTAIN_REGISTER_MAP | J9_STACKWALK_INCLUDE_CALL_IN_FRAMES)) {
		PORT_ACCESS_FROM_WALKSTATE(walkState);
		J9UTF8 * className = J9ROMCLASS_CLASSNAME(((J9Class *) walkState->userData4)->romClass);
		char detailStackBuffer[J9VM_PACKAGE_NAME_BUFFER_LENGTH];
		char * detail = NULL;
		j9object_t detailMessage = J9VMJAVALANGTHROWABLE_DETAILMESSAGE(currentThread, walkState->restartException);

		if (NULL != detailMessage) {
			detail = walkState->walkThread->javaVM->internalVMFunctions->copyStringToUTF8WithMemAlloc(currentThread, detailMessage, J9_STR_NULL_TERMINATE_RESULT, ": ", 2, detailStackBuffer, J9VM_PACKAGE_NAME_BUFFER_LENGTH, NULL);
		}
		swPrintf(walkState, 2, "\tThrowing exception: %.*s%s\n", J9UTF8_LENGTH(className), J9UTF8_DATA(className), detail ? detail : "");
		if (detailStackBuffer != detail) {
			j9mem_free_memory(detail);
		}
	}
#endif

	swPrintf(walkState, 2, "Initial values: walkSP = %p, PC = %p, literals = %p, A0 = %p, j2iFrame = %p, ELS = %p, decomp = %p\n", REMOTE_ADDR(walkState->walkSP), walkState->pc, walkState->literals, REMOTE_ADDR(walkState->arg0EA),
#ifdef J9VM_INTERP_NATIVE_SUPPORT
		REMOTE_ADDR(walkState->j2iFrame),
		REMOTE_ADDR(walkState->walkedEntryLocalStorage),
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
		REMOTE_ADDR(walkState->decompilationStack)
#else
		0
#endif
#else
		0, 0, 0
#endif
	);
#endif

	if ((walkState->flags & J9_STACKWALK_COUNT_SPECIFIED) && (walkState->maxFrames == 0)) {
		goto terminationPoint;
	}

	if (J9_PRIVATE_FLAGS_STACKS_OUT_OF_SYNC == (walkState->walkThread->privateFlags & J9_PRIVATE_FLAGS_STACKS_OUT_OF_SYNC)) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 1, "\tStacks out of sync - terminating walk immediately\n");
#endif
		goto terminationPoint;
	} 

#ifndef J9VM_OUT_OF_PROCESS
	walkState->dropToCurrentFrame = dropToCurrentFrame;
	if (walkState->flags & J9_STACKWALK_CACHE_MASK) {
		rc = allocateCache(walkState);
		if (rc != J9_STACKWALK_RC_NONE) {
			goto terminationPoint;
		}
	}
#endif

#if defined(J9VM_INTERP_STACKWALK_TRACING) && !defined(J9VM_OUT_OF_PROCESS)
	initializeObjectSlotBitVector(walkState);
#endif

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (walkState->flags == J9_STACKWALK_ITERATE_O_SLOTS) {
		walkState->flags |= J9_STACKWALK_SKIP_INLINES;
	}
	if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
		walkState->flags |= J9_STACKWALK_MAINTAIN_REGISTER_MAP;
	}
#ifdef J9VM_OUT_OF_PROCESS
	if (walkState->flags & J9_STACKWALK_START_AT_JIT_FRAME) {
		goto jitFrame;
	}
#endif
#endif

#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
	if (walkState->flags & J9_STACKWALK_LINEAR) {
		rc = lswInitialize(walkState->walkThread->javaVM, walkState);
		if (rc != SW_ERROR_NONE) {
			rc = J9_STACKWALK_RC_NO_MEMORY;
			goto terminationPoint;
		}
	}
#endif 

	while(1) {
		J9SFStackFrame * fixedStackFrame;

		walkState->constantPool = NULL;
		walkState->unwindSP = NULL;
		walkState->method = NULL;
		walkState->sp = walkState->walkSP - walkState->argCount;
		walkState->outgoingArgCount = walkState->argCount;
		walkState->bytecodePCOffset = -1;

#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
		lswFrameNew(walkState->walkThread->javaVM, walkState, (UDATA)walkState->pc);
#endif	

		switch((UDATA) walkState->pc) {
			case J9SF_FRAME_TYPE_END_OF_STACK:
				goto endOfStack;

			case J9SF_FRAME_TYPE_GENERIC_SPECIAL:
				walkGenericSpecialFrame(walkState);
				break;

#ifdef J9VM_INTERP_NATIVE_SUPPORT
			case J9SF_FRAME_TYPE_JIT_RESOLVE:
				walkJITResolveFrame(walkState);
				break;

			case J9SF_FRAME_TYPE_JIT_JNI_CALLOUT:
				walkJITJNICalloutFrame(walkState);
				break;
#endif

			case J9SF_FRAME_TYPE_METHOD:
			case J9SF_FRAME_TYPE_NATIVE_METHOD:
			case J9SF_FRAME_TYPE_JNI_NATIVE_METHOD:
				walkMethodFrame(walkState);
				break;

			case J9SF_FRAME_TYPE_METHODTYPE:
				walkMethodTypeFrame(walkState);
				break;

			default:
#ifdef J9VM_INTERP_STACKWALK_TRACING
				if ((UDATA) walkState->pc <= J9SF_MAX_SPECIAL_FRAME_TYPE) {
					printFrameType(walkState, "Unknown");
#ifdef J9VM_OUT_OF_PROCESS
					swPrintf(walkState, 0, "Aborting walk due to unknown special frame type\n");
					goto terminationPoint;
#endif
				}
#endif

#ifdef J9VM_OUT_OF_PROCESS
				if ((walkState->pc == walkState->walkThread->javaVM->callInReturnPC) || (walkState->pc == (walkState->walkThread->javaVM->callInReturnPC + 3)))
#else
				if (READ_BYTE(walkState->pc) == 0xFF) /* impdep2 = 0xFF - indicates a JNI call-in frame */
#endif
				{
					walkJNICallInFrame(walkState);
				} else {
					walkBytecodeFrame(walkState);
				}
				break;
		}

#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
		lswRecord(walkState, LSW_TYPE_BP, REMOTE_ADDR(walkState->bp));
		lswRecord(walkState, LSW_TYPE_ARG_COUNT, (void*)(UDATA)walkState->argCount);
		lswRecord(walkState, LSW_TYPE_METHOD, REMOTE_ADDR(walkState->method));
		lswRecord(walkState, LSW_TYPE_UNWIND_SP, REMOTE_ADDR(walkState->unwindSP)); 
		if ((UDATA) walkState->pc != J9SF_FRAME_TYPE_JIT_RESOLVE) {
			lswRecord(walkState, LSW_TYPE_FIXED_SLOTS, REMOTE_ADDR(walkState->bp));
		}
#endif

		/* Fetch the PC value before calling the frame walker since the debugger modifies PC values when breakpointing methods */

		walkState->nextPC = ((J9SFStackFrame *) ((U_8 *) walkState->bp - sizeof(J9SFStackFrame) + sizeof(UDATA)))->savedPC;

		/* Walk the frame */

		if (walkFrame(walkState) != J9_STACKWALK_KEEP_ITERATING) {
			goto terminationPoint;
		}
resumeInterpreterWalk:
		walkState->previousFrameFlags = walkState->frameFlags;
		walkState->resolveFrameFlags = 0;

		/* Call the JIT walker if the current frame is a transition from the JIT to the interpreter */

#ifdef J9VM_INTERP_NATIVE_SUPPORT
		if (walkState->frameFlags & J9_STACK_FLAGS_JIT_TRANSITION_TO_INTERPRETER_MASK) {
#ifdef J9VM_OUT_OF_PROCESS
jitFrame:
#endif
resumeJitWalk:
			if (jitWalkStackFrames(walkState) != J9_STACKWALK_KEEP_ITERATING) {
				goto terminationPoint;
			}
			walkState->decompilationRecord = NULL;
			continue;
		}
#endif

		/* Fetch the remaining frame values after calling the frame walker since the stack could grow during the iterator, changing all stack pointer values */

		fixedStackFrame = (J9SFStackFrame *) ((U_8 *) walkState->bp - sizeof(J9SFStackFrame) + sizeof(UDATA));
		nextLiterals = fixedStackFrame->savedCP;
		nextA0 = (UDATA *) LOCAL_ADDR(UNTAGGED_A0(fixedStackFrame));

		/* Move to next frame */

		walkState->pcAddress = &(fixedStackFrame->savedPC);
		walkState->walkSP = walkState->arg0EA + 1;
		walkState->pc = walkState->nextPC;
		walkState->literals = nextLiterals;
		walkState->arg0EA = nextA0;
	}

endOfStack:

#ifdef J9VM_INTERP_STACKWALK_TRACING
	swPrintf(walkState, 2, "<end of stack>\n");
#endif

#if defined(J9VM_INTERP_LINEAR_STACKWALK_TRACING)
	if (walkState->linearSlotWalker) {
		lswPrintFrames(walkState->walkThread, walkState);
		lswCleanup(walkState->walkThread->javaVM, walkState);
	}
#endif	

terminationPoint:

#if defined(J9VM_INTERP_STACKWALK_TRACING) && !defined(J9VM_OUT_OF_PROCESS)
	sniffAndWhack(walkState);
#endif

#ifdef J9VM_INTERP_STACKWALK_TRACING
	swPrintf(walkState, 1, "*** END STACK WALK (rc = %d) ***\n", rc);
#endif

	walkState->flags = savedFlags | (walkState->flags & J9_STACKWALK_CACHE_ALLOCATED);
#if defined(J9VM_INTERP_STACKWALK_TRACING) && !defined(J9VM_OUT_OF_PROCESS)
	walkState->objectSlotWalkFunction = savedOSlotIterator;
	Trc_VRB_WalkStackFrames_Exit(currentThread, walkState->walkThread, rc);
#endif

	if (currentThread != NULL) {
		currentThread->activeWalkState = oldState;
	}

	return rc;
}


UDATA walkFrame(J9StackWalkState * walkState)
{
	if (walkState->flags & J9_STACKWALK_VISIBLE_ONLY) {

		if ((((UDATA) walkState->pc == J9SF_FRAME_TYPE_NATIVE_METHOD) || ((UDATA) walkState->pc == J9SF_FRAME_TYPE_JNI_NATIVE_METHOD)) && !(walkState->flags & J9_STACKWALK_INCLUDE_NATIVES)) {
			return J9_STACKWALK_KEEP_ITERATING;
		}

#ifdef J9VM_INTERP_NATIVE_SUPPORT
		if (!walkState->jitInfo) {
#endif
			if (*walkState->bp & J9SF_A0_INVISIBLE_TAG) {
				if (!(walkState->flags & J9_STACKWALK_INCLUDE_CALL_IN_FRAMES) || (walkState->pc != walkState->walkThread->javaVM->callInReturnPC)) {
					return J9_STACKWALK_KEEP_ITERATING;
				}
			}
#ifdef J9VM_INTERP_NATIVE_SUPPORT
		}
#endif

		if (walkState->skipCount) {
			--walkState->skipCount;
			return J9_STACKWALK_KEEP_ITERATING;
		}

		if (walkState->flags & J9_STACKWALK_HIDE_EXCEPTION_FRAMES) {
			J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method);

			if (!(romMethod->modifiers & J9AccStatic)) {
				if (J9UTF8_DATA(J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(walkState->method)->ramClass->romClass, romMethod))[0] == '<') {
					if (*walkState->arg0EA == (UDATA) walkState->restartException) {
						return J9_STACKWALK_KEEP_ITERATING;
					}
				}
				walkState->flags &= ~J9_STACKWALK_HIDE_EXCEPTION_FRAMES;
			}
		}
	}

	/* Cache values */

	if (walkState->flags & J9_STACKWALK_CACHE_MASK) {
		if (walkState->flags & J9_STACKWALK_CACHE_PCS) {
			U_8 * cachePC = (U_8 *) walkState->pc;

			/* Translate PC if necessary (assume that only visible frames are being walked, i.e. a valid method exists) */

			if (walkState->flags & J9_STACKWALK_WALK_TRANSLATE_PC) {
				if ((UDATA) cachePC <= J9SF_MAX_SPECIAL_FRAME_TYPE) {
					/* Assume native method */
					cachePC = J9_BYTECODE_START_FROM_RAM_METHOD(walkState->method) - 1;
				}

				/* All non-native PCs are correct if the debugger is not running (no breakpointed methods) */

#ifdef J9VM_INTERP_NATIVE_SUPPORT
				/* All JIT PCs are correct, since breakpoints are added only to bytecodes */

				if (!walkState->jitInfo)
#endif
				{
					J9ROMClass * romClass = J9_CLASS_FROM_METHOD(walkState->method)->romClass;

					/* If the PC is within the romClass of the method, it's correct (not breakpointed) */

					if ((cachePC < (U_8 *) romClass) || (cachePC >= (((U_8 *) romClass) + romClass->romSize))) {
						TRIGGER_J9HOOK_VM_PERMANENT_PC(walkState->walkThread->javaVM->hookInterface, walkState->walkThread, cachePC);
					}
				}
			}
			*walkState->cacheCursor++ = (UDATA) cachePC;
		}
		if (walkState->flags & J9_STACKWALK_CACHE_CPS) *walkState->cacheCursor++ = (UDATA) walkState->constantPool;
		if (walkState->flags & J9_STACKWALK_CACHE_METHODS) *walkState->cacheCursor++ = (UDATA) walkState->method;
	}

	++walkState->framesWalked;
	if (walkState->flags & J9_STACKWALK_COUNT_SPECIFIED) {
		if (walkState->framesWalked == walkState->maxFrames)
			goto walkIt;
	}

	if (walkState->flags & J9_STACKWALK_ITERATE_FRAMES) {
		goto walkIt;
	}

	return J9_STACKWALK_KEEP_ITERATING;

walkIt:

	if (walkState->flags & J9_STACKWALK_ITERATE_FRAMES) {
		UDATA rc = walkState->frameWalkFunction(walkState->currentThread, walkState);

		if (walkState->flags & J9_STACKWALK_COUNT_SPECIFIED) {
			if (walkState->framesWalked == walkState->maxFrames) {
				rc = J9_STACKWALK_STOP_ITERATING;
			}
		}

		return rc;
	}

	return J9_STACKWALK_STOP_ITERATING;
}



#if (!defined(J9VM_OUT_OF_PROCESS)) 
static UDATA allocateCache(J9StackWalkState * walkState)
{
	PORT_ACCESS_FROM_WALKSTATE(walkState);
	UDATA * endOfStack;
	UDATA framesPresent;
	UDATA cacheElementSize;
	UDATA cacheSize;
	UDATA * stackStart;

	endOfStack = walkState->walkThread->stackObject->end;
	framesPresent = (endOfStack - walkState->walkThread->sp) / J9_STACKWALK_MIN_FRAME_SLOTS;

	cacheElementSize = 0;
	if (walkState->flags & J9_STACKWALK_CACHE_PCS) ++cacheElementSize;
	if (walkState->flags & J9_STACKWALK_CACHE_CPS) ++cacheElementSize;
	if (walkState->flags & J9_STACKWALK_CACHE_METHODS) ++cacheElementSize;

	cacheSize = framesPresent * cacheElementSize;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (walkState->walkThread->javaVM->jitConfig) {
		if (!(walkState->flags & J9_STACKWALK_SKIP_INLINES)) {
			/* computations above assume 1 cacheElement per frame, in reality there may be (maxInlinedMethods + 1) elements per frame */
			cacheSize *= (walkState->walkThread->javaVM->jitConfig->maxInlineDepth + 1);
		}
	}
#endif

	stackStart = J9_LOWEST_STACK_SLOT(walkState->walkThread);
	if ((walkState != walkState->walkThread->stackWalkState) || ((UDATA) (walkState->walkThread->sp - stackStart) < cacheSize)
#if defined (J9VM_INTERP_VERBOSE) || defined (J9VM_PROF_EVENT_REPORTING)
		|| (walkState->walkThread->javaVM->runtimeFlags & J9_RUNTIME_PAINT_STACK)
#endif
	) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
		if (walkState != walkState->walkThread->stackWalkState)
			swPrintf(walkState, 2, "  <cannot use primary walk buffer>\n");
#endif
		walkState->cache = j9mem_allocate_memory(cacheSize * sizeof(UDATA), OMRMEM_CATEGORY_VM);
		if (!walkState->cache) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
			swPrintf(walkState, 2, "  <failed to allocate cache (%d slots)>\n", cacheSize);
#endif
			return J9_STACKWALK_RC_NO_MEMORY;
		}
		walkState->flags |= J9_STACKWALK_CACHE_ALLOCATED;
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 2, "  <cache allocated @ 0x%x frames=%d, cacheSlots=%d>\n", walkState->cache, framesPresent, cacheSize);
#endif
	} else {
		/* Unused stack contains enough room for entire cache */
		walkState->cache = stackStart;
	}

	walkState->cacheCursor = walkState->cache;
	return J9_STACKWALK_RC_NONE;
}

#endif /* J9VM_!OUT_OF_PROCESS */


#if (!defined(J9VM_INTERP_STACKWALK_TRACING)) 
void freeStackWalkCaches(J9VMThread * currentThread, J9StackWalkState * walkState)
{
	if (walkState->cache && (walkState->flags & J9_STACKWALK_CACHE_ALLOCATED)) {
		PORT_ACCESS_FROM_VMC(currentThread);

		j9mem_free_memory(walkState->cache);
	}
	walkState->cache = NULL;
	walkState->flags &= ~J9_STACKWALK_CACHE_ALLOCATED;
}

#endif /* J9VM_!INTERP_STACKWALK_TRACING */


static void walkObjectPushes(J9StackWalkState * walkState)
{
	UDATA byteCount = (UDATA) walkState->literals;
	j9object_t * currentSlot = (j9object_t*) walkState->walkSP;

#ifdef J9VM_INTERP_STACKWALK_TRACING
	swPrintf(walkState, 4, "\tObject pushes starting at %p for %d slots\n", REMOTE_ADDR(currentSlot), byteCount / sizeof(UDATA));
#endif

	walkState->slotType = J9_STACKWALK_SLOT_TYPE_INTERNAL;
	walkState->slotIndex = 0;

	while (byteCount) {
		WALK_NAMED_O_SLOT(currentSlot, "Push");
		++currentSlot;
		byteCount -= sizeof(UDATA);
		++(walkState->slotIndex);
	}
}



static void walkDescribedPushes(J9StackWalkState * walkState, UDATA * highestSlot, UDATA slotCount, U_32 * descriptionSlots, UDATA argCount)
{
	UDATA descriptionBitsRemaining = 0;
	U_32 description = 0;

	while (slotCount) {
		if (!descriptionBitsRemaining) {
			description = *descriptionSlots++;
			descriptionBitsRemaining = 32;
		}

#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
		{
			char indexedTag[64];
			PORT_ACCESS_FROM_WALKSTATE(walkState);
			if (walkState->slotType == J9_STACKWALK_SLOT_TYPE_METHOD_LOCAL) {
				j9str_printf(PORTLIB, indexedTag, 64, "%s-Slot: %s%d",
						 (description & 1) ? "O" : "I", (walkState->slotIndex >= (IDATA)argCount) ? "t" : "a", walkState->slotIndex);
			} else {
				j9str_printf(PORTLIB, indexedTag, 64, "%s-Slot: p%d", (description & 1) ? "O" : "I", walkState->slotIndex);
			}

			if (description & 1) {
				WALK_NAMED_O_SLOT((j9object_t*) highestSlot, indexedTag);
			} else {
				WALK_NAMED_I_SLOT(highestSlot, indexedTag);
			}
		}
#else
		if (description & 1) {
			WALK_O_SLOT((j9object_t*) highestSlot);
		} else { 
			WALK_I_SLOT(highestSlot);
		}
#endif

		description >>= 1;
		--descriptionBitsRemaining;
		--highestSlot;
		--slotCount;
		++(walkState->slotIndex);
	}
}



static void 
walkMethodFrame(J9StackWalkState * walkState)
{
	J9SFMethodFrame * methodFrame = (J9SFMethodFrame *) ((U_8*) walkState->walkSP + (UDATA) walkState->literals);

	walkState->bp = (UDATA *) &(methodFrame->savedA0);
	walkState->frameFlags = methodFrame->specialFrameFlags;
	MARK_SLOT_AS_OBJECT(walkState, (j9object_t*) &(methodFrame->specialFrameFlags));
	walkState->method = READ_METHOD(methodFrame->method);
	walkState->unwindSP = (UDATA *) methodFrame;

#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
	{
		J9SFMethodFrame * mf = (J9SFMethodFrame *) REMOTE_ADDR(methodFrame);
		lswRecordSlot(walkState, &mf->specialFrameFlags, LSW_TYPE_FLAGS, "Special Flags");
		lswRecordSlot(walkState, &mf->method, LSW_TYPE_METHOD, "Method");
	}
#endif 

#ifdef J9VM_INTERP_STACKWALK_TRACING
	switch((UDATA) walkState->pc) {
		case J9SF_FRAME_TYPE_METHOD:
			printFrameType(walkState, (walkState->frameFlags & J9_STACK_FLAGS_JIT_NATIVE_TRANSITION) ? "JIT generic transition" : "Generic method");
			break;
		case J9SF_FRAME_TYPE_NATIVE_METHOD:
			printFrameType(walkState, (walkState->frameFlags & J9_STACK_FLAGS_JIT_NATIVE_TRANSITION) ? "JIT INL transition" : "INL native method");
			break;
		case J9SF_FRAME_TYPE_JNI_NATIVE_METHOD:
			printFrameType(walkState, (walkState->frameFlags & J9_STACK_FLAGS_JIT_NATIVE_TRANSITION) ? "JIT JNI transition" : "JNI native method");
			break;
		default:
			printFrameType(walkState, (walkState->frameFlags & J9_STACK_FLAGS_JIT_NATIVE_TRANSITION) ? "JIT unknown transition" : "Unknown method");
			break;
	}
#endif

	if ((walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) && walkState->literals) {
		if (walkState->frameFlags & J9_SSF_JNI_REFS_REDIRECTED) {
			walkPushedJNIRefs(walkState);
		} else {
			walkObjectPushes(walkState);
		}
	}
	if (walkState->method) {
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method);

		walkState->constantPool = UNTAGGED_METHOD_CP(walkState->method);
		walkState->argCount = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod);

		if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
			WALK_METHOD_CLASS(walkState);

			if (walkState->argCount) {
				/* Max size as argCount always <= 255 */
				U_32 result[8];

#ifdef J9VM_INTERP_STACKWALK_TRACING
				swPrintf(walkState, 4, "\tUsing signature mapper\n");
#endif
				j9localmap_ArgBitsForPC0(UNTAGGED_METHOD_CP(walkState->method)->ramClass->romClass, romMethod, result);

#ifdef J9VM_INTERP_STACKWALK_TRACING
				swPrintf(walkState, 4, "\tArguments starting at %p for %d slots\n", REMOTE_ADDR(walkState->arg0EA), walkState->argCount);
#endif
				walkState->slotType = J9_STACKWALK_SLOT_TYPE_METHOD_LOCAL;
				walkState->slotIndex = 0;
				if (walkState->frameFlags & J9_SSF_JNI_REFS_REDIRECTED) {
					walkIndirectDescribedPushes(walkState, walkState->arg0EA, walkState->argCount, result);
				} else {
					walkDescribedPushes(walkState, walkState->arg0EA, walkState->argCount, result, walkState->argCount);
				}
			}
		}
	} else {
		if ((walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) && (walkState->arg0EA != walkState->bp)) {
			walkJNIRefs(walkState, walkState->bp + 1, walkState->arg0EA - walkState->bp);
		}
		walkState->constantPool = NULL;
		walkState->argCount = 0;
	}


}

/**
 * Walk a J9SFMethodTypeFrame.  Format:
 *
 * typedef struct J9SFMethodTypeFrame {
 *  +0x0:     j9object_t methodType;
 *  +0x4:     UDATA argStackSlots;
 *  +0x8:     UDATA descriptionIntCount;
 *  +0xC:     UDATA specialFrameFlags;
 *  +0x10:    struct J9Method* savedCP;
 *  +0x14:    U_8* savedPC;
 *  +0x18:    UDATA* savedA0;
 * } J9SFMethodTypeFrame;
 * stackDescriptionInts
 * args described by frame
 *
 */
static void
walkMethodTypeFrame(J9StackWalkState * walkState)
{
	J9SFMethodTypeFrame * methodTypeFrame = (J9SFMethodTypeFrame *) ((U_8*) walkState->walkSP + (UDATA) walkState->literals);

	walkState->bp = (UDATA *) &(methodTypeFrame->savedA0);
	walkState->frameFlags = methodTypeFrame->specialFrameFlags;
	/* Tell Sniff-n-whack to skip these slots */
	MARK_SLOT_AS_OBJECT(walkState, (j9object_t*) &(methodTypeFrame->specialFrameFlags));
	MARK_SLOT_AS_OBJECT(walkState, (j9object_t*) &(methodTypeFrame->argStackSlots));
	MARK_SLOT_AS_OBJECT(walkState, (j9object_t*) &(methodTypeFrame->descriptionIntCount));
	walkState->method = NULL;
	walkState->unwindSP = (UDATA *) methodTypeFrame;

#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
	{
		J9SFMethodTypeFrame * mf = (J9SFMethodTypeFrame *) REMOTE_ADDR(methodTypeFrame);
		lswRecordSlot(walkState, &mf->specialFrameFlags, LSW_TYPE_FLAGS, "Special Flags");
		lswRecordSlot(walkState, &mf->descriptionIntCount, LSW_TYPE_DESCRIPTION_INT_COUNT, "descriptionIntCount");
		lswRecordSlot(walkState, &mf->argStackSlots, LSW_TYPE_ARG_COUNT, "argStackSlots");
		lswRecordSlot(walkState, &mf->methodType, LSW_TYPE_METHODTYPE, "MethodType");
	}
#endif

#ifdef J9VM_INTERP_STACKWALK_TRACING
	printFrameType(walkState, "JSR 292 MethodType");
#endif

	if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
		U_32 *description = (U_32*)(walkState->bp + 1);
	
		if (walkState->literals) {
			/* Special frames overload literals to be the number of bytes of pushed objects */
			walkObjectPushes(walkState);
		}
		
		/* Ensure the MethodType object gets walked */
		WALK_O_SLOT((j9object_t*) &(methodTypeFrame->methodType));
		
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 4, "\tUsing array mapper\n");
#endif

		/* Add 1 for MH or NULL "receiver" that will always be on the stack */
		walkState->argCount = methodTypeFrame->argStackSlots + 1;
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 4, "\tArguments starting at %p for %d slots\n", REMOTE_ADDR(walkState->arg0EA), walkState->argCount);
#endif
		walkState->slotType = J9_STACKWALK_SLOT_TYPE_METHOD_LOCAL;
		walkState->slotIndex = 0;
		walkDescribedPushes(walkState, walkState->arg0EA, walkState->argCount, description, walkState->argCount);
	}
}


static void walkGenericSpecialFrame(J9StackWalkState * walkState)
{
	J9SFSpecialFrame * specialFrame = (J9SFSpecialFrame *) ((U_8*) walkState->walkSP + (UDATA) walkState->literals);

	walkState->bp = (UDATA *) &(specialFrame->savedA0);
	walkState->frameFlags = specialFrame->specialFrameFlags;
	MARK_SLOT_AS_OBJECT(walkState, (j9object_t*) &(specialFrame->specialFrameFlags));
#ifdef J9VM_INTERP_STACKWALK_TRACING
	printFrameType(walkState, "Generic special");
#endif
	if ((walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) && walkState->literals) {
		walkObjectPushes(walkState);
	}
	walkState->argCount = 0;

}


void
walkBytecodeFrameSlots(J9StackWalkState *walkState, J9Method *method, UDATA offsetPC, UDATA *pendingBase, UDATA pendingStackHeight, UDATA *localBase, UDATA numberOfLocals, UDATA alwaysLocalMap)
{
	J9JavaVM *vm = walkState->walkThread->javaVM;
	PORT_ACCESS_FROM_JAVAVM(vm);
	UDATA *bp = localBase - numberOfLocals;
	J9Class *ramClass = J9_CLASS_FROM_METHOD(method);
	J9ROMClass *romClass = ramClass->romClass;
	J9ROMMethod *romMethod = getOriginalROMMethod(method);
	U_32 smallResult = 0;
	U_32 *result = &smallResult;
	U_32 *globalBuffer = NULL;
	UDATA numberOfMappedLocals = numberOfLocals;

#ifdef J9VM_INTERP_STACKWALK_TRACING
	swPrintf(walkState, 3, "\tBytecode index = %d\n", offsetPC);
#endif


	if (romMethod->modifiers & J9AccSynchronized) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 4, "\tSync object for synchronized method\n");
#endif
		walkState->slotType = J9_STACKWALK_SLOT_TYPE_INTERNAL;
		walkState->slotIndex = -1;
		WALK_NAMED_O_SLOT((j9object_t*) (bp + 1), "Sync O-Slot");
		numberOfMappedLocals -= 1;
	} else if (J9ROMMETHOD_IS_NON_EMPTY_OBJECT_CONSTRUCTOR(romMethod)) {
		/* Non-empty java.lang.Object.<init> has one hidden temp to hold a copy of the receiver */
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 4, "\tReceiver object for java.lang.Object.<init>\n");
#endif
		walkState->slotType = J9_STACKWALK_SLOT_TYPE_INTERNAL;
		walkState->slotIndex = -1;
		WALK_NAMED_O_SLOT((j9object_t*) (bp + 1), "Receiver O-Slot");
		numberOfMappedLocals -= 1;
	}

	if ((numberOfMappedLocals > 32) || (pendingStackHeight > 32)) {
		UDATA maxCount = (numberOfMappedLocals > pendingStackHeight) ? numberOfMappedLocals : pendingStackHeight;
		result = j9mem_allocate_memory(((maxCount + 31) >> 5) * sizeof(U_32), OMRMEM_CATEGORY_VM);
		if (NULL == result) {
			globalBuffer = j9mapmemory_GetResultsBuffer(vm);
			result = globalBuffer;
		}
	}

	if (0 != numberOfMappedLocals) {
		getLocalsMap(walkState, romClass, romMethod, offsetPC, result, numberOfMappedLocals, alwaysLocalMap);
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 4, "\tLocals starting at %p for %d slots\n", REMOTE_ADDR(localBase), numberOfMappedLocals);
#endif
		walkState->slotType = J9_STACKWALK_SLOT_TYPE_METHOD_LOCAL;
		walkState->slotIndex = 0;
		walkDescribedPushes(walkState, localBase, numberOfMappedLocals, result, romMethod->argCount);
	}

	if (0 != pendingStackHeight) {
		getStackMap(walkState, romClass, romMethod, offsetPC, pendingStackHeight, result);
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 4, "\tPending stack starting at %p for %d slots\n", REMOTE_ADDR(pendingBase), pendingStackHeight);
#endif
		walkState->slotType = J9_STACKWALK_SLOT_TYPE_PENDING;
		walkState->slotIndex = 0;
		walkDescribedPushes(walkState, pendingBase, pendingStackHeight, result, 0);
	}

	if (result != &smallResult) {
		if (NULL == globalBuffer) {
			j9mem_free_memory(result);
		} else {
			j9mapmemory_ReleaseResultsBuffer(vm);
		}
	}
}


static void 
walkBytecodeFrame(J9StackWalkState * walkState)
{
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
	lswRecord(walkState, LSW_TYPE_FRAME_TYPE, (void*)(UDATA)LSW_FRAME_TYPE_BYTECODE);
#endif

	walkState->method = READ_METHOD(walkState->literals);
	if (NULL == walkState->method) {
		walkState->constantPool = NULL;
		walkState->bytecodePCOffset = -1;
		walkState->argCount = 0;
		if (walkState->arg0EA == walkState->j2iFrame) {
			walkState->bp = walkState->arg0EA;
			walkState->unwindSP = (UDATA *) ((U_8 *) walkState->bp - sizeof(J9SFJ2IFrame) + sizeof(UDATA));
			walkState->frameFlags = ((J9SFJ2IFrame *) walkState->unwindSP)->specialFrameFlags;
			MARK_SLOT_AS_OBJECT(walkState, (j9object_t*) &(((J9SFJ2IFrame *) walkState->unwindSP)->specialFrameFlags));
#ifdef J9VM_INTERP_STACKWALK_TRACING
			printFrameType(walkState, "invokeExact J2I");
#endif
		} else {
			walkState->bp = NULL;
			walkState->unwindSP = NULL;
			walkState->frameFlags = 0;
#ifdef J9VM_INTERP_STACKWALK_TRACING
			printFrameType(walkState, "BAD bytecode (expect crash)");
#endif
		}
	} else {
		J9JavaVM *vm = walkState->walkThread->javaVM;
		UDATA argTempCount = 0;
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method);

		walkState->constantPool = UNTAGGED_METHOD_CP(walkState->method);

		/* If PC = impdep1, we haven't run the method (and never will) its just a placeholder */
		if ((walkState->pc != vm->impdep1PC) && (walkState->pc != (vm->impdep1PC + 3))) {
			walkState->bytecodePCOffset = walkState->pc - (U_8 *) REMOTE_ADDR(J9_BYTECODE_START_FROM_RAM_METHOD(walkState->method));
		} else {
			walkState->bytecodePCOffset = 0;
		}

		walkState->argCount = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod);
		argTempCount = J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod) + walkState->argCount;
		walkState->bp = walkState->arg0EA - argTempCount;
		if (romMethod->modifiers & J9AccSynchronized) {
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
			lswRecordSlot(walkState, REMOTE_ADDR(walkState->bp), LSW_TYPE_O_SLOT, "Sync Object");
#endif
			argTempCount += 1;
			walkState->bp -= 1;
		} else if (J9ROMMETHOD_IS_NON_EMPTY_OBJECT_CONSTRUCTOR(romMethod)) {
			/* Non-empty java.lang.Object.<init> has one hidden temp to hold a copy of the receiver */
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
			lswRecordSlot(walkState, REMOTE_ADDR(walkState->bp), LSW_TYPE_O_SLOT, "Receiver Object");
#endif
			argTempCount += 1;
			walkState->bp -= 1;
		}

#ifdef J9VM_INTERP_NATIVE_SUPPORT
		if (walkState->bp == walkState->j2iFrame) {
			walkState->unwindSP = (UDATA *) ((U_8 *) walkState->bp - sizeof(J9SFJ2IFrame) + sizeof(UDATA));
			walkState->frameFlags = ((J9SFJ2IFrame *) walkState->unwindSP)->specialFrameFlags;
			MARK_SLOT_AS_OBJECT(walkState, (j9object_t*) &(((J9SFJ2IFrame *) walkState->unwindSP)->specialFrameFlags));
		} else {
#endif
			walkState->unwindSP = (UDATA *) ((U_8 *) walkState->bp - sizeof(J9SFStackFrame) + sizeof(UDATA));
			walkState->frameFlags = 0;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
		}
#endif
#ifdef J9VM_INTERP_STACKWALK_TRACING
		printFrameType(walkState, walkState->frameFlags ? "J2I" : "Bytecode");
#endif

		if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
			WALK_METHOD_CLASS(walkState);
			walkBytecodeFrameSlots(walkState, walkState->method, walkState->bytecodePCOffset,
					walkState->unwindSP - 1, walkState->unwindSP - walkState->walkSP,
					walkState->arg0EA, argTempCount, FALSE);
		}
	}
}


static void walkJNICallInFrame(J9StackWalkState * walkState)
{
	J9SFJNICallInFrame * callInFrame;

	walkState->bp = walkState->arg0EA;
	callInFrame = (J9SFJNICallInFrame *) ((U_8 *) walkState->bp - sizeof(J9SFJNICallInFrame) + sizeof(UDATA));

	/* Retain any non-argument object pushes after the call-in frame (the exit point may want them) */
	walkState->unwindSP = (UDATA *) ((U_8 *) callInFrame - (UDATA) walkState->literals);

	walkState->frameFlags = callInFrame->specialFrameFlags;
	MARK_SLOT_AS_OBJECT(walkState, (j9object_t*) &(callInFrame->specialFrameFlags));
#ifdef J9VM_INTERP_STACKWALK_TRACING
	printFrameType(walkState, "JNI call-in");
#endif	
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING	
	{
		J9SFJNICallInFrame * jf = REMOTE_ADDR(callInFrame);
		lswRecordSlot(walkState, &jf->specialFrameFlags, LSW_TYPE_FLAGS, "Special Flags");
		lswRecordSlot(walkState, &jf->exitAddress, LSW_TYPE_ADDRESS, "Exit Address"); 
		lswRecord(walkState, LSW_TYPE_ELS, walkState->walkedEntryLocalStorage->oldEntryLocalStorage); 
		lswRecord(walkState, LSW_TYPE_FRAME_TYPE, (void*)(UDATA)LSW_FRAME_TYPE_JNI_CALL_IN);
	}
#endif
	if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {

		/* The following can only be true if the call-in method has returned (removed args and pushed return value) */

		if (walkState->walkSP != walkState->unwindSP) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
			if (walkState->pc != (walkState->walkThread->javaVM->callInReturnPC + 3)) {
				swPrintf(walkState, 0, "Error: PC should have been advanced in order to push return value, pc = %p, cipc = %p !!!\n", walkState->pc, walkState->walkThread->javaVM->callInReturnPC);
			}
#endif
			if (walkState->frameFlags & J9_SSF_RETURNS_OBJECT) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
				swPrintf(walkState, 4, "\tObject push (return value from call-in method)\n");
#endif
				WALK_O_SLOT((j9object_t*) walkState->walkSP);
			}
#ifdef J9VM_INTERP_STACKWALK_TRACING
			else {
				swPrintf(walkState, 2, "\tCall-in return value (non-object) takes %d slots at %p\n", walkState->unwindSP - walkState->walkSP, REMOTE_ADDR(walkState->walkSP));
			}
#endif
			walkState->walkSP = walkState->unwindSP;	/* so walkObjectPushes works correctly */
		}
		if (walkState->literals) {
			walkObjectPushes(walkState);
		}
	}
#ifdef J9VM_INTERP_NATIVE_SUPPORT
#if defined(J9VM_INTERP_STACKWALK_TRACING) && !defined(J9VM_OUT_OF_PROCESS)
	sniffAndWhackELS(walkState);
#endif
	walkState->walkedEntryLocalStorage = walkState->walkedEntryLocalStorage->oldEntryLocalStorage;
	walkState->i2jState = walkState->walkedEntryLocalStorage ? &(walkState->walkedEntryLocalStorage->i2jState) : NULL;
#ifdef J9VM_INTERP_STACKWALK_TRACING
	swPrintf(walkState, 2, "\tNew ELS = %p\n", REMOTE_ADDR(walkState->walkedEntryLocalStorage));
#endif
#endif
	walkState->argCount = 0;
}

#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
static void walkJITResolveFrame(J9StackWalkState * walkState)
{
	J9SFJITResolveFrame * jitResolveFrame = (J9SFJITResolveFrame *) ((U_8*) walkState->walkSP + (UDATA) walkState->literals);

	walkState->argCount = 0;
	walkState->bp = (UDATA *) &(jitResolveFrame->taggedRegularReturnSP);
	walkState->frameFlags = jitResolveFrame->specialFrameFlags;
	MARK_SLOT_AS_OBJECT(walkState, (j9object_t*) &(jitResolveFrame->specialFrameFlags));
#ifdef J9VM_INTERP_STACKWALK_TRACING
	printFrameType(walkState, "JIT resolve");
#endif
	if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
		j9object_t * savedJITExceptionSlot = &jitResolveFrame->savedJITException;

#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 4, "\tObject push (savedJITException)\n");
#endif
		WALK_O_SLOT(savedJITExceptionSlot);

		if (walkState->literals) {
			walkObjectPushes(walkState);
		}
	}

#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING 
	{
		J9SFJITResolveFrame * rf = REMOTE_ADDR(jitResolveFrame);
		lswRecordSlot(walkState, &rf->savedJITException, LSW_TYPE_O_SLOT, "Saved JIT Excptn");
		lswRecordSlot(walkState, &rf->specialFrameFlags, LSW_TYPE_FLAGS, "Flags");
		lswRecordSlot(walkState, &rf->parmCount, LSW_TYPE_ARG_COUNT, "Arg Count");
		lswRecordSlot(walkState, &rf->returnAddress, LSW_TYPE_ADDRESS, "Ret Address");
		lswRecordSlot(walkState, &rf->taggedRegularReturnSP, LSW_TYPE_ADDRESS, "Return SP");
	} 
#endif	

}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */


#if (defined(J9VM_INTERP_STACKWALK_TRACING)) 
static void printFrameType(J9StackWalkState * walkState, char * frameType)
{
	swPrintf(walkState, 2, "%s frame: bp = %p, sp = %p, pc = %p, cp = %p, arg0EA = %p, flags = %p\n", frameType, REMOTE_ADDR(walkState->bp), REMOTE_ADDR(walkState->walkSP), walkState->pc, REMOTE_ADDR(walkState->constantPool), REMOTE_ADDR(walkState->arg0EA), walkState->frameFlags);
	swPrintMethod(walkState);

#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING	
	lswRecord(walkState, LSW_TYPE_FRAME_NAME, frameType);
	lswRecord(walkState, LSW_TYPE_FRAME_INFO, walkState);
#endif	
}
#endif /* J9VM_INTERP_STACKWALK_TRACING */


#if (defined(J9VM_INTERP_STACKWALK_TRACING)) 
void swPrintMethod(J9StackWalkState * walkState) {
	J9Method * method = walkState->method;

	if (method) {
		PORT_ACCESS_FROM_WALKSTATE(walkState);
		J9UTF8 * className = J9ROMCLASS_CLASSNAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass);
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		J9UTF8 * name = J9ROMMETHOD_GET_NAME(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);
		J9UTF8 * sig = J9ROMMETHOD_GET_SIGNATURE(UNTAGGED_METHOD_CP(method)->ramClass->romClass, romMethod);

		swPrintf(walkState, 2, "\tMethod: %.*s.%.*s%.*s "
#ifdef J9VM_OUT_OF_PROCESS
			"!j9method  0x%zx"
#else
			"(%p)"
#endif
			"\n", (U_32) J9UTF8_LENGTH(className), J9UTF8_DATA(className), (U_32) J9UTF8_LENGTH(name), J9UTF8_DATA(name), (U_32) J9UTF8_LENGTH(sig), J9UTF8_DATA(sig), REMOTE_ADDR(walkState->method));
	}
}
#endif /* J9VM_INTERP_STACKWALK_TRACING */


#if (defined(J9VM_INTERP_NATIVE_SUPPORT)) 
static void walkJITJNICalloutFrame(J9StackWalkState * walkState)
{
	J9SFMethodFrame * methodFrame = (J9SFMethodFrame *) ((U_8*) walkState->walkSP + (UDATA) walkState->literals);

	walkState->argCount = 0;
	walkState->bp = (UDATA *) &(methodFrame->savedA0);
	walkState->frameFlags = methodFrame->specialFrameFlags;
	MARK_SLOT_AS_OBJECT(walkState, (j9object_t*) &(methodFrame->specialFrameFlags));
	walkState->method = READ_METHOD(methodFrame->method);
	walkState->constantPool = UNTAGGED_METHOD_CP(walkState->method);
#ifdef J9VM_INTERP_STACKWALK_TRACING
	printFrameType(walkState, "JIT JNI call-out");
#endif
	if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
		WALK_METHOD_CLASS(walkState);
		if (walkState->literals) {
			walkPushedJNIRefs(walkState);
		}
	}
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
	{
		J9SFMethodFrame * mf = REMOTE_ADDR(methodFrame);
		lswRecordSlot(walkState, &mf->specialFrameFlags, LSW_TYPE_FLAGS, "Special Flags");
		lswRecordSlot(walkState, &mf->method, LSW_TYPE_METHOD, "Method");
		lswRecord(walkState, LSW_TYPE_FRAME_BOTTOM, walkState->bp);
	}
#endif
}
#endif /* J9VM_INTERP_NATIVE_SUPPORT */


static void walkPushedJNIRefs(J9StackWalkState * walkState)
{
	UDATA refCount = walkState->frameFlags & J9_SSF_JNI_PUSHED_REF_COUNT_MASK;
	UDATA pushCount = (((UDATA) walkState->literals) / sizeof(UDATA)) - refCount;

	if (pushCount) {
		walkState->literals = (J9Method *) (pushCount * sizeof(UDATA));
		walkObjectPushes(walkState);
	}

	if (refCount) {
		walkJNIRefs(walkState, walkState->walkSP + pushCount, refCount);
	}
}



static void walkIndirectDescribedPushes(J9StackWalkState * walkState, UDATA * highestIndirectSlot, UDATA slotCount, U_32 * descriptionSlots)
{
	UDATA descriptionBitsRemaining = 0;
	U_32 description = 0;

	while (slotCount) {
		if (!descriptionBitsRemaining) {
			description = *descriptionSlots++;
			descriptionBitsRemaining = 32;
		}
		if (description & 1) {
			/* There's no need to mark indirect slots as object since they will never point to valid objects (so will not be whacked) */
			WALK_INDIRECT_O_SLOT((j9object_t*) (*highestIndirectSlot & ~1), highestIndirectSlot);
		} else {
			WALK_I_SLOT(highestIndirectSlot);
		}

		description >>= 1;
		--descriptionBitsRemaining;
		--highestIndirectSlot;
		--slotCount;
		++(walkState->slotIndex);
	}
}



#if (defined(J9VM_INTERP_STACKWALK_TRACING)) 
void swPrintf(J9StackWalkState * walkState, UDATA level, char * format, ...)
{
	if (walkState->walkThread->javaVM->stackWalkVerboseLevel >= level) {
		PORT_ACCESS_FROM_WALKSTATE(walkState);
		va_list args;
#ifdef J9VM_OUT_OF_PROCESS
		static char buf[4096];
#else
		char buf[1024];
#endif

		va_start(args, format);
		j9str_vprintf(buf, sizeof(buf), format, args);

#if defined(J9VM_INTERP_STACKWALK_TRACING) && !defined(J9VM_OUT_OF_PROCESS)
/*	Trc_VRB_WalkStackFrames_swPrintf(walkState->walkThread, buf); */
#endif

		j9tty_printf(PORTLIB, "<%p> %s", REMOTE_ADDR(walkState->walkThread), buf);
		va_end(args);
	}
}

#endif /* J9VM_INTERP_STACKWALK_TRACING */



#if (defined(J9VM_INTERP_STACKWALK_TRACING)) 
void 
swWalkObjectSlot(J9StackWalkState * walkState, j9object_t * objectSlot, void * indirectSlot, void * tag) 
{
	UDATA oldValue;
	UDATA newValue;
	j9object_t * localObjectSlot;

	/* If indirectSlot is NULL, then objectSlot is a pointer onto the local copy of the stack.
	 * If it is non-NULL, then objectSlot is a pointer into remote memory, and indirectSlot is a pointer onto the local stack
	 */
	if (indirectSlot == NULL) {
		localObjectSlot = objectSlot;
		objectSlot = REMOTE_ADDR(objectSlot);
	} else {
		localObjectSlot = LOCAL_ADDR(objectSlot);
	}

	oldValue = READ_UDATA((UDATA *) objectSlot);

	if (indirectSlot != NULL) {
		swPrintf(walkState, 4, "\t\t%s[%p -> %p] = %p\n", (tag ? tag : "O-Slot"), REMOTE_ADDR(indirectSlot), objectSlot, oldValue);
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING 
		lswRecordSlot(walkState, REMOTE_ADDR((void*)(((UDATA) indirectSlot) & ~(UDATA)1)), LSW_TYPE_INDIRECT_O_SLOT, tag ? tag : "O-Slot");
#endif 
	} else {
		swPrintf(walkState, 4, "\t\t%s[%p] = %p\n", (tag ? tag : "O-Slot"), objectSlot, oldValue);
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING 
		lswRecordSlot(walkState, objectSlot, LSW_TYPE_O_SLOT, tag ? tag : "O-Slot");
#endif  
	}
	walkState->objectSlotWalkFunction(walkState->currentThread, walkState, localObjectSlot, objectSlot);
	newValue = READ_UDATA((UDATA *) objectSlot);
	if (oldValue != newValue) {
		swPrintf(walkState, 4, "\t\t\t-> %p\n", newValue);
	}
}

#endif /* J9VM_INTERP_STACKWALK_TRACING */


#if (defined(J9VM_INTERP_STACKWALK_TRACING)) 
void swWalkIntSlot(J9StackWalkState * walkState, UDATA * intSlot, void * indirectSlot, void * tag) {
	if (indirectSlot) {
		swPrintf(walkState, 5, "\t\t%s[%p -> %p] = %p\n", (tag ? tag : "I-Slot"), REMOTE_ADDR(indirectSlot), REMOTE_ADDR(intSlot), *intSlot);
	} else {
		swPrintf(walkState, 5, "\t\t%s[%p] = %p\n", (tag ? tag : "I-Slot"), REMOTE_ADDR(intSlot), *intSlot);
	}
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING	
	lswRecordSlot(walkState, REMOTE_ADDR(intSlot), LSW_TYPE_I_SLOT, tag ? tag : "I-Slot");
#endif
}

#endif /* J9VM_INTERP_STACKWALK_TRACING */


#if (defined(J9VM_INTERP_STACKWALK_TRACING)  && !defined(J9VM_OUT_OF_PROCESS)) 
static void initializeObjectSlotBitVector(J9StackWalkState * walkState)
{
	walkState->objectSlotBitVector = NULL;
	walkState->elsBitVector = 0;

	if ((walkState->walkThread->javaVM->runtimeFlags & J9_RUNTIME_SNIFF_AND_WHACK) && !(walkState->flags & J9_STACKWALK_DO_NOT_SNIFF_AND_WHACK)) {
		PORT_ACCESS_FROM_WALKSTATE(walkState);
		UDATA slotCount = walkState->walkThread->stackObject->end - walkState->walkThread->sp;
		UDATA byteCount = (slotCount + 7) >> 3;

		walkState->objectSlotBitVector = j9mem_allocate_memory(byteCount, OMRMEM_CATEGORY_VM);
		if (walkState->objectSlotBitVector == NULL) {
			swPrintf(walkState, 1, "Unable to allocate bit vector for Sniff'n'whack - continuing without it\n");
		} else {
			swPrintf(walkState, 1, "Sniff'n'whack enabled\n");
			memset(walkState->objectSlotBitVector, 0, byteCount);
			if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
				walkState->savedObjectSlotWalkFunction = walkState->objectSlotWalkFunction;
			} else {
				walkState->savedObjectSlotWalkFunction = emptySniffAndWhackIterator;
				walkState->flags |=J9_STACKWALK_ITERATE_O_SLOTS;
			}
			walkState->objectSlotWalkFunction = sniffAndWhackIterator;
		}
	}
}

#endif /* J9VM_INTERP_STACKWALK_TRACING && !OUT_OF_PROCESS */


#if (defined(J9VM_INTERP_STACKWALK_TRACING)  && !defined(J9VM_OUT_OF_PROCESS)) 
static void sniffAndWhack(J9StackWalkState * walkState)
{
	if (walkState->objectSlotBitVector) {
		PORT_ACCESS_FROM_WALKSTATE(walkState);

		if (walkState->pc == (U_8 *) J9SF_FRAME_TYPE_END_OF_STACK) {
			UDATA slotCount = walkState->walkThread->stackObject->end - walkState->walkThread->sp;
			U_8 * currentBytePtr;
			U_8 currentByte;
			UDATA bitsLeft;
			UDATA * currentSlot;

			currentBytePtr = walkState->objectSlotBitVector;
			bitsLeft = 0;
			currentSlot = walkState->walkThread->stackObject->end;
			while (slotCount) {
				--currentSlot;
				if (!bitsLeft) {
					currentByte = *(currentBytePtr++);
					bitsLeft = 8;
				}
				if ((currentByte & 1) == 0) {
					sniffAndWhackPointer(walkState, (j9object_t*) currentSlot);
				}
				currentByte >>= 1;
				--bitsLeft;
				--slotCount;
			}
		} else {
			swPrintf(walkState, 1, "Entire stack not walked - skipping whack stage\n");
		}

		j9mem_free_memory(walkState->objectSlotBitVector);
		walkState->objectSlotBitVector = NULL;
	}
}

#endif /* J9VM_INTERP_STACKWALK_TRACING && !OUT_OF_PROCESS */


#if (defined(J9VM_INTERP_STACKWALK_TRACING)  && !defined(J9VM_OUT_OF_PROCESS)) 
static void sniffAndWhackIterator(J9VMThread * currentThread, J9StackWalkState * walkState, j9object_t * objectSlot, const void * stackLocation)
{
	MARK_SLOT_AS_OBJECT(walkState, (j9object_t *)stackLocation);
	walkState->savedObjectSlotWalkFunction(currentThread, walkState, objectSlot, stackLocation);
}


#endif /* J9VM_INTERP_STACKWALK_TRACING && !OUT_OF_PROCESS */


#if (defined(J9VM_INTERP_STACKWALK_TRACING)  && !defined(J9VM_OUT_OF_PROCESS)) 
static void sniffAndWhackPointer(J9StackWalkState * walkState, j9object_t *slotPointer)
{
	j9object_t object = *slotPointer;

	if (object) {
		J9JavaVM * vm = walkState->walkThread->javaVM;
		UDATA rc = vm->memoryManagerFunctions->j9gc_ext_check_is_valid_heap_object(vm, object, 0);

		if ((rc == J9OBJECTCHECK_OBJECT) || (rc == J9OBJECTCHECK_FORWARDED)) {
#ifdef J9VM_ENV_DATA64
			((U_32 *) slotPointer)[1] = (U_32) 0xDEAD0000 + (U_16) (walkState->walkThread->javaVM->whackedPointerCounter);
#else
			((U_32 *) slotPointer)[0] = (U_32) 0xDEAD0000 + (U_16) (walkState->walkThread->javaVM->whackedPointerCounter);
#endif
			swPrintf(walkState, 1, "WHACKING I-Slot[%p] = %p --now--> %p\n", slotPointer, object, *slotPointer);
			walkState->walkThread->javaVM->whackedPointerCounter += 2;
		}
	}
}

#endif /* J9VM_INTERP_STACKWALK_TRACING && !OUT_OF_PROCESS */


#if (defined(J9VM_INTERP_STACKWALK_TRACING)  && !defined(J9VM_OUT_OF_PROCESS)  && defined(J9VM_INTERP_NATIVE_SUPPORT)) 
static void sniffAndWhackELS(J9StackWalkState * walkState)
{
	if (walkState->objectSlotBitVector) {
		UDATA registerNumber;

		for (registerNumber = 0; registerNumber < J9SW_POTENTIAL_SAVED_REGISTERS; ++registerNumber) {
			if ((walkState->elsBitVector & ((UDATA)1 << registerNumber)) == 0) {
				sniffAndWhackPointer(walkState, (j9object_t*) (walkState->walkedEntryLocalStorage->jitGlobalStorageBase + registerNumber));
			}
		}
	}

	walkState->elsBitVector = 0;
}

#endif /* J9VM_INTERP_STACKWALK_TRACING && !OUT_OF_PROCESS && INTERP_NATIVE_SUPPORT */


#if (defined(J9VM_INTERP_STACKWALK_TRACING)  && !defined(J9VM_OUT_OF_PROCESS)) 
void swMarkSlotAsObject(J9StackWalkState * walkState, j9object_t * objectSlot)
{
	if (!walkState->objectSlotBitVector) return;

	if ((((UDATA *) objectSlot) >= walkState->walkThread->sp) && (((UDATA *) objectSlot) < walkState->walkThread->stackObject->end)) {
		UDATA slotNumber = (walkState->walkThread->stackObject->end - 1) - (UDATA *) objectSlot;

		if (walkState->objectSlotBitVector[slotNumber >> 3] & (1 << (slotNumber & 7))) {
			swPrintf(walkState, 0, "\t\tError: slot already walked: %p\n", objectSlot);
		} else {
			walkState->objectSlotBitVector[slotNumber >> 3] |= (1 << (slotNumber & 7));
		}
		return;
	}

#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (walkState->walkedEntryLocalStorage) {
		if (((J9STACKSLOT *) objectSlot) >= walkState->walkedEntryLocalStorage->jitGlobalStorageBase) {
			UDATA slotNumber = ((J9STACKSLOT *) objectSlot) - walkState->walkedEntryLocalStorage->jitGlobalStorageBase;

			if (slotNumber < J9SW_POTENTIAL_SAVED_REGISTERS) {
				if (walkState->elsBitVector & ((UDATA)1 << slotNumber)) {
					swPrintf(walkState, 0, "\t\tError: slot already walked: %p\n", objectSlot);
				} else {
					walkState->elsBitVector |= ((UDATA)1 << slotNumber);
				}
			}
		}
	}
#endif
}

#endif /* J9VM_INTERP_STACKWALK_TRACING && !OUT_OF_PROCESS */


#if (defined(J9VM_INTERP_STACKWALK_TRACING)  && !defined(J9VM_OUT_OF_PROCESS)) 
static void emptySniffAndWhackIterator(J9VMThread * currentThread, J9StackWalkState * walkState, j9object_t * objectSlot, const void * stackLocation)
{
}


#endif /* J9VM_INTERP_STACKWALK_TRACING && !OUT_OF_PROCESS */


static void 
getLocalsMap(J9StackWalkState * walkState, J9ROMClass * romClass, J9ROMMethod * romMethod, UDATA offsetPC, U_32 * result, UDATA argTempCount, UDATA alwaysLocalMap)
{
	PORT_ACCESS_FROM_WALKSTATE(walkState);
	IDATA errorCode;
	J9JavaVM *vm = walkState->walkThread->javaVM;

	if (!alwaysLocalMap) {
		/*	Detect method entry vs simply executing at PC 0.  If the bytecode frame is invisible (method monitor enter or
			stack growth) or the previous frame was the special frame indicating method entry (reporting method enter), then
			use the signature mapper instead of the local mapper.  This keeps the receiver alive for use within method enter
			events, even if the receiver is never used (in which case the local mapper would mark it as int).
		*/

		if ((*walkState->bp & J9SF_A0_INVISIBLE_TAG) || (walkState->previousFrameFlags & J9_SSF_METHOD_ENTRY)) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
			if (*walkState->bp & J9SF_A0_INVISIBLE_TAG) {
				swPrintf(walkState, 4, "\tAt method entry (hidden bytecode frame = monitor enter/stack grow), using signature mapper\n");
			} else {
				swPrintf(walkState, 4, "\tAt method entry (previous frame = report monitor enter), using signature mapper\n");
			}
#endif

			/* j9localmap_ArgBitsForPC0 only deals with args, so zero out the result array to make sure the temps are non-object */

			memset(result, 0, ((argTempCount + 31) / 32) * sizeof(U_32));

			j9localmap_ArgBitsForPC0(romClass, romMethod, result);
			return;
		}
	}

#ifdef J9VM_INTERP_STACKWALK_TRACING
	swPrintf(walkState, 4, "\tUsing local mapper\n");
#endif
	errorCode = vm->localMapFunction(PORTLIB, romClass, romMethod, offsetPC, result, vm, j9mapmemory_GetBuffer, j9mapmemory_ReleaseBuffer);

	if (errorCode < 0) {
#ifdef J9VM_OUT_OF_PROCESS
		dbgError("Local map failed, result = %p\n", errorCode);
#else
		/* Local map failed, result = %p - aborting VM - needs new message TBD */
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VM_STACK_MAP_FAILED, errorCode);
#if defined(J9VM_INTERP_STACKWALK_TRACING)
		Assert_VRB_stackMapFailed();
#else
		Assert_VM_stackMapFailed();
#endif
#endif
	}

	return;
}


static void 
getStackMap(J9StackWalkState * walkState, J9ROMClass * romClass, J9ROMMethod * romMethod, UDATA offsetPC, UDATA pushCount, U_32 *result) 
{
	PORT_ACCESS_FROM_WALKSTATE(walkState);
	IDATA errorCode;

	errorCode = j9stackmap_StackBitsForPC(PORTLIB, offsetPC, romClass, romMethod, result, pushCount, walkState->walkThread->javaVM, j9mapmemory_GetBuffer, j9mapmemory_ReleaseBuffer);
	if (errorCode < 0) {
#ifdef J9VM_OUT_OF_PROCESS
		dbgError("Stack map failed, result = %p\n", errorCode);
#else
		/* Local map failed, result = %p - aborting VM */
		j9nls_printf(PORTLIB, J9NLS_ERROR, J9NLS_VM_STACK_MAP_FAILED, errorCode);
#if defined(J9VM_INTERP_STACKWALK_TRACING)
		Assert_VRB_stackMapFailed();
#else
		Assert_VM_stackMapFailed();
#endif
#endif
	}
	return;
}


static void walkJNIRefs(J9StackWalkState * walkState, UDATA * currentRef, UDATA refCount)
{
#ifdef J9VM_INTERP_STACKWALK_TRACING
	swPrintf(walkState, 4, "\tJNI local ref pushes starting at %p for %d slots\n", REMOTE_ADDR(currentRef), refCount);
#endif

	walkState->slotType = J9_STACKWALK_SLOT_TYPE_JNI_LOCAL;
	walkState->slotIndex = 0;

	do {
		if (*currentRef & J9_REDIRECTED_REFERENCE) {
			j9object_t *realRef = (j9object_t*) (*currentRef & ~J9_REDIRECTED_REFERENCE);

			/* There's no need to mark indirect slots as object since they will never point to valid objects (so will not be whacked) */
			WALK_NAMED_INDIRECT_O_SLOT(realRef, currentRef, "Indir-Lcl-JNI-Ref");
		} else {
			WALK_NAMED_O_SLOT((j9object_t*) currentRef, "Lcl-JNI-Ref");
		}
		++currentRef;
		++(walkState->slotIndex);
	} while (--refCount);
}



#if (!defined(J9VM_OUT_OF_PROCESS)) 
/* Only callable from inside a visible-only walk on the current thread (with VM access) */
/* Must not be called when a native callout frame is on TOS */

static void
dropToCurrentFrame(J9StackWalkState * walkState)
{
	J9VMThread * vmThread = walkState->walkThread;
	J9SFSpecialFrame * specialFrame = (J9SFSpecialFrame *) (((U_8 *) walkState->unwindSP) - sizeof(J9SFSpecialFrame));

	specialFrame->specialFrameFlags = 0;
	specialFrame->savedCP = walkState->literals;
	specialFrame->savedPC = walkState->pc;
	specialFrame->savedA0 = (UDATA *) (((U_8 *) (walkState->arg0EA)) + J9SF_A0_INVISIBLE_TAG);

	vmThread->arg0EA = (UDATA *) &(specialFrame->savedA0);
	vmThread->literals = NULL;
	vmThread->pc = (U_8 *) J9SF_FRAME_TYPE_GENERIC_SPECIAL;
	vmThread->sp = (UDATA *) specialFrame;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	vmThread->j2iFrame = walkState->j2iFrame;
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
	if (J9_FSD_ENABLED(vmThread->javaVM)) {
		vmThread->javaVM->jitConfig->jitCleanUpDecompilationStack(vmThread, walkState, TRUE);
	}
#endif
#endif
}

#endif /* J9VM_!OUT_OF_PROCESS */

/* Ensure that this assertion code only appears in the VM DLL */
#if !defined(J9VM_INTERP_STACKWALK_TRACING)
void
invalidJITReturnAddress(J9StackWalkState *walkState)
{
	PORT_ACCESS_FROM_WALKSTATE(walkState);
	j9tty_printf(PORTLIB, "\n\n*** Invalid JIT return address %p in %p\n\n", walkState->pc, walkState);
	Assert_VM_unreachable();
}
#endif

