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
#define jitGetOwnedObjectMonitors jitGetOwnedObjectMonitorsVerbose
#define walkBytecodeFrameSlots walkBytecodeFrameSlotsVerbose
#endif
#include "j9protos.h"
#include "MethodMetaData.h"
#include "jitprotos.h"
#include "stackwalk.h"
#include "rommeth.h"
#include "j9cp.h"
#include "jitregmap.h"
#include "j9codertnls.h"
#include "HeapIteratorAPI.h"
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
#include "../vm/linearswalk.h"
#include "ut_j9vrb.h"
#else
#include "ut_j9codertvm.h"
#endif

#define J9JIT_ARTIFACT_SEARCH_CACHE_ENABLE

#ifndef J9VM_INTERP_STACKWALK_TRACING
#define walkFrame (walkState->walkThread->javaVM->walkFrame)
#define walkBytecodeFrameSlots (walkState->walkThread->javaVM->internalVMFunctions->walkBytecodeFrameSlots)
#endif

/* Use the JIT's view of the # of slots to get the argCount rather than asking
 * the J9ROMMethod->argCount.  This is essential as JSR 292 thunk_archetype methods
 * serve only as templates for the generated code and will have an INCORRECT argCount
 */
#define SET_A0_CP_METHOD(walkState) \
	(walkState)->arg0EA = (UDATA *) (((U_8 *) ((walkState)->bp + (walkState)->jitInfo->slots)) + sizeof(J9JITFrame) - sizeof(UDATA)), \
	(walkState)->method = READ_METHOD((walkState)->jitInfo->ramMethod), \
	(walkState)->constantPool = READ_CP((walkState)->jitInfo->constantPool), \
	(walkState)->argCount = (walkState)->jitInfo->slots

#ifdef J9VM_JIT_FULL_SPEED_DEBUG
#define UPDATE_PC_FROM(walkState, pcExpression) \
	(walkState)->pc = MASK_PC(*((walkState)->pcAddress = (U_8 **) &(pcExpression))), \
	(walkState)->decompilationRecord = NULL
#else
#define UPDATE_PC_FROM(walkState, pcExpression) (walkState)->pc = MASK_PC((U_8 *) (pcExpression))
#define jitGetExceptionTable(walkState) jitGetExceptionTableFromPC((walkState)->walkThread, (UDATA) (walkState)->pc)
#endif

#ifdef J9VM_JIT_FULL_SPEED_DEBUG
#define COMMON_UNWIND_TO_NEXT_FRAME(walkState) \
	(walkState)->resolveFrameFlags = 0, \
	UPDATE_PC_FROM(walkState, ((J9JITFrame *) ((walkState)->bp))->returnPC)
#else
#define COMMON_UNWIND_TO_NEXT_FRAME(walkState) \
	UPDATE_PC_FROM(walkState, ((J9JITFrame *) ((walkState)->bp))->returnPC)
#endif

#ifdef J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP

#define UNWIND_TO_NEXT_FRAME(walkState) \
	(walkState)->unwindSP = ((UDATA *) (((J9JITFrame *) ((walkState)->bp)) + 1)) + (walkState)->argCount, \
	COMMON_UNWIND_TO_NEXT_FRAME(walkState)

#else

#define UNWIND_TO_NEXT_FRAME(walkState) \
	(walkState)->unwindSP = ((UDATA *) (((J9JITFrame *) ((walkState)->bp)) + 1)), /* arguments are passed in register, argument area lives in caller frame */ \
	COMMON_UNWIND_TO_NEXT_FRAME(walkState)

#endif

#ifdef J9VM_INTERP_STACKWALK_TRACING
#define CLEAR_LOCAL_REGISTER_MAP_ENTRIES(walkState) \
	{ \
		UDATA clearRegNum; \
		for (clearRegNum = 0; clearRegNum < (J9SW_POTENTIAL_SAVED_REGISTERS - J9SW_JIT_CALLEE_PRESERVED_SIZE); ++clearRegNum) { \
			((UDATA **) &((walkState)->registerEAs))[jitCalleeDestroyedRegisterList[clearRegNum]] = NULL; \
		} \
	}
#else
#define CLEAR_LOCAL_REGISTER_MAP_ENTRIES(walkState)
#endif

#if (defined(J9VM_INTERP_STACKWALK_TRACING)) /* priv. proto (autogen) */

static void jitPrintFrameType(J9StackWalkState * walkState, char * frameType);
#endif /* J9VM_INTERP_STACKWALK_TRACING (autogen) */



static void walkJITFrameSlots(J9StackWalkState * walkState, U_8 * jitDescriptionBits,  U_8 * stackAllocMapBits, U_8 ** jitDescriptionCursor, U_8 ** stackAllocMapCursor, UDATA * jitBitsRemaining, UDATA * mapBytesRemaining, UDATA * scanCursor, UDATA slotsRemaining, void *stackMap, J9JITStackAtlas *gcStackAtlas, char * slotDescription);
#if (!defined(J9VM_OUT_OF_PROCESS)) /* priv. proto (autogen) */

static void jitDropToCurrentFrame(J9StackWalkState * walkState);
#endif /* J9VM_!OUT_OF_PROCESS (autogen) */

#if defined(J9VM_OUT_OF_PROCESS)
#define MM_FN(vm, function) function
#else
#define MM_FN(vm, function) (vm)->memoryManagerFunctions->function
#endif

static UDATA walkTransitionFrame(J9StackWalkState *walkState);
static void jitAddSpilledRegistersForJ2I(J9StackWalkState * walkState);
static void jitAddSpilledRegistersForINL(J9StackWalkState * walkState);
static UDATA jitNextUTFChar(U_8 ** pUtfData);
#if (defined(J9VM_JIT_FULL_SPEED_DEBUG)) /* priv. proto (autogen) */

static J9JITExceptionTable * jitGetExceptionTable(J9StackWalkState * walkState);
#endif /* J9VM_JIT_FULL_SPEED_DEBUG (autogen) */



static void jitAddSpilledRegistersForResolve(J9StackWalkState * walkState);
static void jitWalkFrame(J9StackWalkState *walkState, UDATA walkLocals, void *stackMap);
static void jitWalkResolveMethodFrame(J9StackWalkState *walkState);
static void jitWalkRegisterMap(J9StackWalkState *walkState, void *stackMap, J9JITStackAtlas *gcStackAtlas);
static UDATA jitNextSigChar(U_8 ** utfData);
static void jitWalkStackAllocatedObject(J9StackWalkState * walkState, j9object_t object);
static jvmtiIterationControl stackAllocatedObjectSlotWalkFunction(J9JavaVM *javaVM, J9MM_IterateObjectDescriptor *objectDesc, J9MM_IterateObjectRefDescriptor *refDesc, void *userData);

static UDATA countOwnedObjectMonitors(J9StackWalkState *walkState);
static UDATA walkLiveMonitorSlots(J9StackWalkState *walkState, J9JITStackAtlas *gcStackAtlas,
		U_8 *liveMonitorMap, U_8 *monitorMask, U_16 numberOfMapBits);
static void countLiveMonitorSlots(J9StackWalkState *walkState, J9JITStackAtlas *gcStackAtlas, U_8 *liveMonitorMap, U_8 *monitorMask, U_16 numberOfMapBits);
static j9object_t *getSlotAddress(J9StackWalkState *walkState, J9JITStackAtlas *gcStackAtlas, U_16 slot);
static void jitWalkOSRBuffer(J9StackWalkState *walkState, J9OSRBuffer *osrBuffer);

UDATA  jitWalkStackFrames(J9StackWalkState *walkState)
{
	UDATA rc;
	UDATA * returnSP;
	U_8 * failedPC;
	UDATA i;
	U_8 ** returnTable;
#ifndef J9VM_OUT_OF_PROCESS
	void  (*savedDropToCurrentFrame)(struct J9StackWalkState * walkState) ;
#endif

	if (J9_ARE_ANY_BITS_SET(walkState->flags, J9_STACKWALK_RESUME)) {
		walkState->flags &= ~J9_STACKWALK_RESUME;
		if (0 != walkState->inlineDepth) {
			goto resumeWalkInline;
		}
		goto resumeNonInline;
	}
	if (walkState->flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) {
		memset(&walkState->registerEAs, 0, sizeof(walkState->registerEAs));
	}
	if ((rc = walkTransitionFrame(walkState)) != J9_STACKWALK_KEEP_ITERATING) {
		return rc;
	}

	walkState->frameFlags = 0;
#ifndef J9VM_OUT_OF_PROCESS
	savedDropToCurrentFrame = walkState->dropToCurrentFrame;
	walkState->dropToCurrentFrame = jitDropToCurrentFrame;
#endif

	while ((walkState->jitInfo = jitGetExceptionTable(walkState)) != NULL) {
		walkState->stackMap = NULL;
		walkState->inlineMap = NULL;
		walkState->bp = walkState->unwindSP + getJitTotalFrameSize(walkState->jitInfo);
		
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
		lswRecord(walkState, LSW_TYPE_JIT_BP, REMOTE_ADDR(walkState->bp));
#endif

		/* Point walkState->sp to the last outgoing argument */

#ifdef J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP
		walkState->sp = walkState->unwindSP - walkState->argCount;
#else
		walkState->sp = walkState->unwindSP;
#endif
		walkState->outgoingArgCount = walkState->argCount;

		if ((!(walkState->flags & J9_STACKWALK_SKIP_INLINES)) && getJitInlinedCallInfo(walkState->jitInfo)) {
			jitGetMapsFromPC(walkState->walkThread->javaVM, walkState->jitInfo, (UDATA) walkState->pc, &(walkState->stackMap), &(walkState->inlineMap));
			if (NULL != walkState->inlineMap) {
				walkState->inlinedCallSite = getFirstInlinedCallSite(walkState->jitInfo, walkState->inlineMap);

				if (NULL != walkState->inlinedCallSite) {
					walkState->inlineDepth = getJitInlineDepthFromCallSite(walkState->jitInfo, walkState->inlinedCallSite);
					walkState->arg0EA = NULL;
					do {
						J9Method * inlinedMethod = (J9Method *)getInlinedMethod(walkState->inlinedCallSite);

						walkState->method = READ_METHOD(inlinedMethod);
						walkState->constantPool = UNTAGGED_METHOD_CP(walkState->method);
						walkState->bytecodePCOffset = (IDATA) getCurrentByteCodeIndexAndIsSameReceiver(walkState->jitInfo, walkState->inlineMap, walkState->inlinedCallSite, NULL);
#ifdef J9VM_INTERP_STACKWALK_TRACING
						jitPrintFrameType(walkState, "JIT inline");
#endif						
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING						
						lswFrameNew(walkState->walkThread->javaVM, walkState, LSW_FRAME_TYPE_JIT_INLINE);
						lswRecord(walkState, LSW_TYPE_UNWIND_SP, REMOTE_ADDR(walkState->unwindSP));
						lswRecord(walkState, LSW_TYPE_METHOD, REMOTE_ADDR(walkState->method));
						lswRecord(walkState, LSW_TYPE_JIT_FRAME_INFO, walkState);
#endif
						if ((rc = walkFrame(walkState)) != J9_STACKWALK_KEEP_ITERATING) {
							return rc;
						}
resumeWalkInline:
						walkState->inlinedCallSite = getNextInlinedCallSite(walkState->jitInfo, walkState->inlinedCallSite);
					} while(--walkState->inlineDepth != 0);
				}
			}
		} else if (walkState->flags & J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET) {
			jitGetMapsFromPC(walkState->walkThread->javaVM, walkState->jitInfo, (UDATA) walkState->pc, &(walkState->stackMap), &(walkState->inlineMap));
		}

		SET_A0_CP_METHOD(walkState);
		if (walkState->flags & J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET) {
			walkState->bytecodePCOffset = (NULL == walkState->inlineMap) ? (IDATA) -1 : (IDATA) getCurrentByteCodeIndexAndIsSameReceiver(walkState->jitInfo, walkState->inlineMap, NULL, NULL);
		}

#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
		lswFrameNew(walkState->walkThread->javaVM, walkState, LSW_FRAME_TYPE_JIT);
		lswRecord(walkState, LSW_TYPE_UNWIND_SP, REMOTE_ADDR(walkState->unwindSP));
		lswRecord(walkState, LSW_TYPE_METHOD, REMOTE_ADDR(walkState->method));
#endif	

#ifdef J9VM_INTERP_STACKWALK_TRACING
		jitPrintFrameType(walkState, "JIT");
#endif

		if (walkState->flags & J9_STACKWALK_ITERATE_METHOD_CLASS_SLOTS) {
			markClassesInInlineRanges(walkState->jitInfo, walkState);
		}

		if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
			jitWalkFrame(walkState, TRUE, walkState->stackMap);
		}

#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
		lswRecord(walkState, LSW_TYPE_JIT_FRAME_INFO, walkState);
#endif	 
		if ((rc = walkFrame(walkState)) != J9_STACKWALK_KEEP_ITERATING) {
			return rc;
		}
resumeNonInline:

		if (walkState->flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) {
			CLEAR_LOCAL_REGISTER_MAP_ENTRIES(walkState);
			jitAddSpilledRegisters(walkState, walkState->stackMap);
		}

		UNWIND_TO_NEXT_FRAME(walkState);

	}

	/* JIT pair with no mapping indicates a bytecoded frame */

	failedPC = walkState->pc;
	returnTable = (U_8 **) walkState->walkThread->javaVM->jitConfig->i2jReturnTable;
	if (returnTable) {
		for (i = 0; i < J9SW_JIT_RETURN_TABLE_SIZE; ++i) {
			if (failedPC == returnTable[i]) {
				goto i2jTransition;
			}
		}
#ifdef J9VM_OUT_OF_PROCESS
		dbgError("*** Invalid JIT return address: %p\n", failedPC);
#else
		/* Only report errors if the stack walk caller has allowed it.
		 * If errors are not being reported, the walk will continue at the last
		 * interpreter to JIT transition (skipping an unknown number of JIT frames.
		 * This is safe to do as the I2J values were saved by the interpreter at
		 * the transition point, and can be assumed to be valid.
		 */
		if (walkState->errorMode != J9_STACKWALK_ERROR_MODE_IGNORE) {
			/* Avoid recursive error situations */
			if (0 == (walkState->walkThread->privateFlags & J9_PRIVATE_FLAGS_STACK_CORRUPT)) {
				walkState->walkThread->privateFlags |= J9_PRIVATE_FLAGS_STACK_CORRUPT;
				walkState->walkThread->javaVM->internalVMFunctions->invalidJITReturnAddress(walkState);
			}
		}
#endif
i2jTransition: ;
	}

	UPDATE_PC_FROM(walkState, walkState->i2jState->pc);
	walkState->literals = walkState->i2jState->literals;
	walkState->arg0EA = (UDATA *) LOCAL_ADDR(walkState->i2jState->a0);
	returnSP = walkState->i2jState->returnSP;
	walkState->previousFrameFlags = 0;
	if (((UDATA) returnSP) & J9_STACK_FLAGS_ARGS_ALIGNED) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 2, "I2J args were copied for alignment\n");
#endif
		walkState->previousFrameFlags = J9_STACK_FLAGS_JIT_ARGS_ALIGNED;
	}
	walkState->walkSP = (UDATA *) LOCAL_ADDR(UNTAG2(returnSP, UDATA *));
#ifdef J9VM_INTERP_STACKWALK_TRACING
	swPrintf(walkState, 2, "I2J values: PC = %p, A0 = %p, walkSP = %p, literals = %p, JIT PC = %p, pcAddress = %p, decomp = %p\n", walkState->pc,
		REMOTE_ADDR(walkState->arg0EA), REMOTE_ADDR(walkState->walkSP), walkState->literals, failedPC,
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
		REMOTE_ADDR(walkState->pcAddress), REMOTE_ADDR(walkState->decompilationStack)
#else
		0, 0
#endif
		);
#endif

#ifndef J9VM_OUT_OF_PROCESS
	walkState->dropToCurrentFrame = savedDropToCurrentFrame;
#endif
	return J9_STACKWALK_KEEP_ITERATING;
}

static UDATA walkTransitionFrame(J9StackWalkState *walkState)
{
#ifdef J9VM_OUT_OF_PROCESS
	if (walkState->flags & J9_STACKWALK_START_AT_JIT_FRAME) {
		walkState->flags &= ~J9_STACKWALK_START_AT_JIT_FRAME;
		walkState->unwindSP = walkState->walkSP;
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
		walkState->pcAddress = NULL;
#endif
		return J9_STACKWALK_KEEP_ITERATING;
	}
#endif

	if (walkState->frameFlags & J9_STACK_FLAGS_JIT_RESOLVE_FRAME) {
		J9SFJITResolveFrame * resolveFrame = (J9SFJITResolveFrame *) ((U_8 *) walkState->bp - sizeof(J9SFJITResolveFrame) + sizeof(UDATA));
		UDATA resolveFrameType = walkState->frameFlags & J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK;

#ifdef J9VM_INTERP_STACKWALK_TRACING
		switch (resolveFrameType) {
			case J9_STACK_FLAGS_JIT_GENERIC_RESOLVE:						swPrintf(walkState, 3, "\tGeneric resolve\n");						break;
			case J9_STACK_FLAGS_JIT_STACK_OVERFLOW_RESOLVE_FRAME:			swPrintf(walkState, 3, "\tStack overflow resolve\n");				break;
			case J9_STACK_FLAGS_JIT_DATA_RESOLVE:							swPrintf(walkState, 3, "\tData resolve\n");							break;
			case J9_STACK_FLAGS_JIT_RUNTIME_HELPER_RESOLVE:					swPrintf(walkState, 3, "\tRuntime helper resolve\n");				break;
			case J9_STACK_FLAGS_JIT_LOOKUP_RESOLVE:							swPrintf(walkState, 3, "\tInterface lookup resolve\n");				break;
			case J9_STACK_FLAGS_JIT_INTERFACE_METHOD_RESOLVE:				swPrintf(walkState, 3, "\tInterface method resolve\n");				break;
			case J9_STACK_FLAGS_JIT_SPECIAL_METHOD_RESOLVE:					swPrintf(walkState, 3, "\tSpecial method resolve\n");				break;
			case J9_STACK_FLAGS_JIT_STATIC_METHOD_RESOLVE:					swPrintf(walkState, 3, "\tStatic method resolve\n");				break;
			case J9_STACK_FLAGS_JIT_VIRTUAL_METHOD_RESOLVE:					swPrintf(walkState, 3, "\tVirtual method resolve\n");				break;
			case J9_STACK_FLAGS_JIT_RECOMPILATION_RESOLVE:					swPrintf(walkState, 3, "\tRecompilation resolve\n");				break;
			case J9_STACK_FLAGS_JIT_MONITOR_ENTER_RESOLVE:					swPrintf(walkState, 3, "\tMonitor enter resolve\n");				break;
			case J9_STACK_FLAGS_JIT_METHOD_MONITOR_ENTER_RESOLVE:			swPrintf(walkState, 3, "\tMethod monitor enter resolve\n");         break;
			case J9_STACK_FLAGS_JIT_FAILED_METHOD_MONITOR_ENTER_RESOLVE:	swPrintf(walkState, 3, "\tFailed method monitor enter resolve\n");	break;
			case J9_STACK_FLAGS_JIT_ALLOCATION_RESOLVE:						swPrintf(walkState, 3, "\tAllocation resolve\n");					break;
			case J9_STACK_FLAGS_JIT_BEFORE_ANEWARRAY_RESOLVE:				swPrintf(walkState, 3, "\tBefore anewarray resolve\n");				break;
			case J9_STACK_FLAGS_JIT_BEFORE_MULTIANEWARRAY_RESOLVE:			swPrintf(walkState, 3, "\tBefore multianewarray resolve\n");		break;
			case J9_STACK_FLAGS_JIT_INDUCE_OSR_RESOLVE:						swPrintf(walkState, 3, "\tInduce OSR resolve\n");					break;
			case J9_STACK_FLAGS_JIT_EXCEPTION_CATCH_RESOLVE:				swPrintf(walkState, 3, "\tException catch resolve\n");					break;
			default: swPrintf(walkState, 3, "\tUnknown resolve type %p\n", resolveFrameType);
		}
#endif

		UPDATE_PC_FROM(walkState, resolveFrame->returnAddress);
		if (J9_STACK_FLAGS_JIT_EXCEPTION_CATCH_RESOLVE == resolveFrameType) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
			swPrintf(walkState, 3, "\tAt exception catch - incrementing PC to make map fetching work\n");
#endif
			walkState->pc += 1;
		}
		walkState->resolveFrameFlags = walkState->frameFlags;
		walkState->unwindSP = (UDATA *) LOCAL_ADDR(UNTAG2(resolveFrame->taggedRegularReturnSP, UDATA *));
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 3, "\tunwindSP initialized to %p\n", REMOTE_ADDR(walkState->unwindSP));
#endif
#ifdef J9SW_JIT_HELPERS_PASS_PARAMETERS_ON_STACK
		walkState->unwindSP += resolveFrame->parmCount;
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 3, "\tAdding %d slots of pushed resolve parameters\n", resolveFrame->parmCount);
#endif
#endif
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
		lswRecord(walkState, LSW_TYPE_RESOLVE_FRAME_TYPE, (void*)resolveFrameType);
#endif		

		if (resolveFrameType == J9_STACK_FLAGS_JIT_DATA_RESOLVE) {
			if (walkState->flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) {
				jitAddSpilledRegistersForDataResolve(walkState);
			}

			walkState->unwindSP += getJitDataResolvePushes();
#ifdef J9VM_INTERP_STACKWALK_TRACING
			swPrintf(walkState, 3, "\tData resolve added %d slots\n", getJitDataResolvePushes());
#endif
		} else {
			UDATA inMethodPrologue = TRUE;

			if (walkState->flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) {
				jitAddSpilledRegistersForResolve(walkState);
			}

			switch(resolveFrameType) {
			case J9_STACK_FLAGS_JIT_VIRTUAL_METHOD_RESOLVE:
			case J9_STACK_FLAGS_JIT_INTERFACE_METHOD_RESOLVE:
			case J9_STACK_FLAGS_JIT_SPECIAL_METHOD_RESOLVE:
			case J9_STACK_FLAGS_JIT_STATIC_METHOD_RESOLVE:
			case J9_STACK_FLAGS_JIT_LOOKUP_RESOLVE:
			case J9_STACK_FLAGS_JIT_RECOMPILATION_RESOLVE:
			case J9_STACK_FLAGS_JIT_INDUCE_OSR_RESOLVE:
				jitWalkResolveMethodFrame(walkState);
				break;
			case J9_STACK_FLAGS_JIT_FAILED_METHOD_MONITOR_ENTER_RESOLVE:
				inMethodPrologue = FALSE;
				/* Intentional fall-through */
			case J9_STACK_FLAGS_JIT_STACK_OVERFLOW_RESOLVE_FRAME:
				walkState->jitInfo = jitGetExceptionTable(walkState);
				walkState->bp = walkState->unwindSP;
				if (!inMethodPrologue) {
					walkState->bp += getJitTotalFrameSize(walkState->jitInfo);
				}
				walkState->outgoingArgCount = 0;
				SET_A0_CP_METHOD(walkState);

#ifdef J9VM_INTERP_STACKWALK_TRACING
				jitPrintFrameType(walkState, "JIT (hidden)");
#endif

				if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
					jitWalkFrame(walkState, !inMethodPrologue, NULL);
				}

				if (walkState->flags & J9_STACKWALK_ITERATE_HIDDEN_JIT_FRAMES) {
					UDATA rc;

					walkState->frameFlags = 0;
					if ((rc = walkFrame(walkState)) != J9_STACKWALK_KEEP_ITERATING) {
						return rc;
					}
				}

				if (walkState->flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) {
					CLEAR_LOCAL_REGISTER_MAP_ENTRIES(walkState);
				}

				UNWIND_TO_NEXT_FRAME(walkState);
			}
		}
	} else if (walkState->frameFlags & J9_STACK_FLAGS_JIT_JNI_CALL_OUT_FRAME) {
		J9SFNativeMethodFrame * nativeMethodFrame = (J9SFNativeMethodFrame *) ((U_8 *) walkState->bp - sizeof(J9SFNativeMethodFrame) + sizeof(UDATA));

		UPDATE_PC_FROM(walkState, nativeMethodFrame->savedCP);
#ifdef J9VM_INTERP_GROWABLE_STACKS
		if ((walkState->frameFlags & J9_SSF_JNI_REFS_REDIRECTED) && (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS)) {
			UDATA * currentBP = walkState->bp;

			walkState->jitInfo = jitGetExceptionTable(walkState);
			walkState->unwindSP = ((UDATA *) LOCAL_ADDR(nativeMethodFrame->savedPC)) + 1;
			walkState->bp = walkState->unwindSP + getJitTotalFrameSize(walkState->jitInfo);
			SET_A0_CP_METHOD(walkState);
#ifdef J9VM_INTERP_STACKWALK_TRACING
			jitPrintFrameType(walkState, "JIT (redirected JNI refs)");
#endif
			jitWalkFrame(walkState, TRUE, NULL);
			walkState->bp = currentBP;
		}
#endif
		walkState->unwindSP = walkState->bp + 1;
	} else if (walkState->frameFlags & J9_STACK_FLAGS_JIT_CALL_IN_FRAME) {
		UDATA callInFrameType = walkState->frameFlags & J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK;
		if ((callInFrameType & J9_STACK_FLAGS_JIT_CALL_IN_TYPE_J2_I) == J9_STACK_FLAGS_JIT_CALL_IN_TYPE_J2_I) {
			J9SFJ2IFrame * j2iFrame = (J9SFJ2IFrame *) ((U_8 *) walkState->bp - sizeof(J9SFJ2IFrame) + sizeof(UDATA));

			walkState->i2jState = &(j2iFrame->i2jState);
			walkState->j2iFrame = LOCAL_ADDR(j2iFrame->previousJ2iFrame);

			if (walkState->flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) {
				jitAddSpilledRegistersForJ2I(walkState);
			}

			walkState->unwindSP = (UDATA *) LOCAL_ADDR(UNTAG2(j2iFrame->taggedReturnSP, UDATA *));
			UPDATE_PC_FROM(walkState, j2iFrame->returnAddress);
		} else {
			/* Choke & Die */
#ifndef J9VM_INTERP_STACKWALK_TRACING
			/* Symbol not visible from verbose */
			Assert_JSWalk_invalidFrameType();
#endif
		}	
	} else { /* inl */
		J9SFSpecialFrame * specialFrame = (J9SFSpecialFrame *) ((U_8 *) walkState->bp - sizeof(J9SFSpecialFrame) + sizeof(UDATA));

		if (walkState->flags & J9_STACKWALK_MAINTAIN_REGISTER_MAP) {
			jitAddSpilledRegistersForINL(walkState);
		}
		/* Do not add J9SW_JIT_STACK_SLOTS_USED_BY_CALL as it has been popped from the stack by the call to INL helper */
#ifdef J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP
		walkState->unwindSP = walkState->arg0EA + 1;
#else
		walkState->unwindSP = (UDATA *) LOCAL_ADDR(UNTAG2(specialFrame->savedA0, UDATA *));
#endif
		UPDATE_PC_FROM(walkState, specialFrame->savedCP);
	}

	return J9_STACKWALK_KEEP_ITERATING;
}


#if (defined(J9VM_INTERP_STACKWALK_TRACING)) /* priv. proto (autogen) */

static void jitPrintFrameType(J9StackWalkState * walkState, char * frameType)
{
	swPrintf(walkState, 2, "%s frame: bp = %p, pc = %p, unwindSP = %p, cp = %p, arg0EA = %p, jitInfo = %p\n", 
			 frameType, REMOTE_ADDR(walkState->bp), walkState->pc, REMOTE_ADDR(walkState->unwindSP), 
			 REMOTE_ADDR(walkState->constantPool), REMOTE_ADDR(walkState->arg0EA), REMOTE_ADDR(walkState->jitInfo));
	swPrintMethod(walkState);
	swPrintf(walkState, 3, "\tBytecode index = %d, inlineDepth = %d, PC offset = %p\n", 
			 walkState->bytecodePCOffset, walkState->inlineDepth, walkState->pc - (U_8 *) walkState->method->extra);
}
#endif /* J9VM_INTERP_STACKWALK_TRACING (autogen) */


static void jitWalkFrame(J9StackWalkState *walkState, UDATA walkLocals, void *stackMap)
{
	J9JITStackAtlas * gcStackAtlas;
	U_8 * jitDescriptionCursor;
	U_8 * stackAllocMapCursor;
	UDATA * objectArgScanCursor;
	UDATA * objectTempScanCursor;
	UDATA jitBitsRemaining;
	U_8 jitDescriptionBits = 0;
	U_8 stackAllocMapBits = 0;
	UDATA mapBytesRemaining;
	U_8 variableInternalPtrSize;
	UDATA registerMap;
	J9JITDecompilationInfo *decompilationRecord;

	WALK_METHOD_CLASS(walkState);

	if (stackMap == NULL) {
		stackMap = getStackMapFromJitPC(walkState->walkThread->javaVM, walkState->jitInfo, (UDATA) walkState->pc);
		if (stackMap == NULL) {
#ifdef J9VM_OUT_OF_PROCESS
			dbgError("Unable to locate JIT stack map\n");
#else
			PORT_ACCESS_FROM_WALKSTATE(walkState);
			J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method);
			J9UTF8 * className = J9ROMCLASS_CLASSNAME(J9_CLASS_FROM_METHOD(walkState->method)->romClass);
			J9UTF8 * name = J9ROMMETHOD_GET_NAME(J9_CLASS_FROM_METHOD(walkState->method)->romClass, romMethod);
			J9UTF8 * sig = J9ROMMETHOD_GET_SIGNATURE(J9_CLASS_FROM_METHOD(walkState->method)->romClass, romMethod);

			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_BEGIN_MULTI_LINE, J9NLS_CODERT_UNABLE_TO_LOCATE_JIT_STACKMAP);
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_MULTI_LINE, J9NLS_CODERT_UNABLE_TO_LOCATE_JIT_STACKMAP_METHOD, (U_32) J9UTF8_LENGTH(className), J9UTF8_DATA(className), (U_32) J9UTF8_LENGTH(name), J9UTF8_DATA(name), (U_32) J9UTF8_LENGTH(sig), J9UTF8_DATA(sig), walkState->method);
			j9nls_printf(PORTLIB, J9NLS_ERROR | J9NLS_END_MULTI_LINE, J9NLS_CODERT_UNABLE_TO_LOCATE_JIT_STACKMAP_PC, walkState->pc, walkState->pc - walkState->jitInfo->startPC, walkState->jitInfo);

#ifdef J9VM_INTERP_STACKWALK_TRACING
			Assert_VRB_stackMapNull();
#else
			Assert_Swalk_stackMapNull();
#endif
#endif
		}
	}

	gcStackAtlas = (J9JITStackAtlas *) getJitGCStackAtlas(walkState->jitInfo);

#ifdef J9VM_INTERP_STACKWALK_TRACING
	swPrintf(walkState, 2, "\tstackMap=%p, slots=%d parmBaseOffset=%d, parmSlots=%d, localBaseOffset=%d\n",
					REMOTE_ADDR(stackMap), walkState->jitInfo->slots, gcStackAtlas->parmBaseOffset, gcStackAtlas->numberOfParmSlots, gcStackAtlas->localBaseOffset);
#endif

	objectArgScanCursor = getObjectArgScanCursor(walkState);
	jitBitsRemaining = 0;
	mapBytesRemaining = getJitNumberOfMapBytes(gcStackAtlas);

	variableInternalPtrSize = 0;
	registerMap = getJitRegisterMap(walkState->jitInfo, stackMap);
	jitDescriptionCursor = getJitStackSlots(walkState->jitInfo, stackMap);
	stackAllocMapCursor = getStackAllocMapFromJitPC(walkState->walkThread->javaVM, walkState->jitInfo, (UDATA) walkState->pc, stackMap);

	walkState->slotType = J9_STACKWALK_SLOT_TYPE_METHOD_LOCAL;
	walkState->slotIndex = 0;

	if (getJitNumberOfParmSlots(gcStackAtlas)) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 4, "\tDescribed JIT args starting at %p for %d slots\n", REMOTE_ADDR(objectArgScanCursor), gcStackAtlas->numberOfParmSlots);
#endif
		walkJITFrameSlots(walkState, &jitDescriptionBits, &stackAllocMapBits, &jitDescriptionCursor, &stackAllocMapCursor, &jitBitsRemaining, &mapBytesRemaining,
											objectArgScanCursor, getJitNumberOfParmSlots(gcStackAtlas), stackMap, NULL, ": a");
	}

	if (walkLocals) {
		objectTempScanCursor = getObjectTempScanCursor(walkState);

		if (walkState->bp - objectTempScanCursor) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
			swPrintf(walkState, 4, "\tDescribed JIT temps starting at %p for %d slots\n", REMOTE_ADDR(objectTempScanCursor), walkState->bp - objectTempScanCursor);
#endif
			walkJITFrameSlots(walkState, &jitDescriptionBits, &stackAllocMapBits, &jitDescriptionCursor, &stackAllocMapCursor, &jitBitsRemaining, &mapBytesRemaining,
												objectTempScanCursor, walkState->bp - objectTempScanCursor, stackMap, gcStackAtlas, ": t");
		}
	}

	jitWalkRegisterMap(walkState, stackMap, gcStackAtlas);

	/* If there is an OSR buffer attached to this frame, walk it */

	decompilationRecord = walkState->decompilationRecord;
	if (NULL != decompilationRecord) {
		J9OSRBuffer *osrBuffer = &decompilationRecord->osrBuffer;

		if (0 != osrBuffer->numberOfFrames) {
			jitWalkOSRBuffer(walkState, osrBuffer);
		}
	}
}

static void walkJITFrameSlots(J9StackWalkState * walkState, U_8 * jitDescriptionBits,  U_8 * stackAllocMapBits, U_8 ** jitDescriptionCursor, U_8 ** stackAllocMapCursor, UDATA * jitBitsRemaining, UDATA * mapBytesRemaining, UDATA * scanCursor, UDATA slotsRemaining, void *stackMap, J9JITStackAtlas *gcStackAtlas, char * slotDescription)
{
#ifdef J9VM_INTERP_STACKWALK_TRACING
	char indexedTag[64];
#endif

	/* GC stack atlas passed in to this function is NULL when this method
	   is called for parameters in the frame; when this method is called for
	   autos in the frame, the actual stack atlas is passed in. This
	   works because no internal pointers/pinning arrays can be parameters.
	   We must avoid doing internal pointer fixup twice for a
	   given stack frame as this method is called twice for each
	   stack frame (once for parms and once for autos).

	   If internal pointer map exists in the atlas it means there are internal pointer
	   regs/autos in this method.
	*/

	if (gcStackAtlas && getJitInternalPointerMap(gcStackAtlas))
	{
		walkJITFrameSlotsForInternalPointers(walkState, jitDescriptionCursor, scanCursor, stackMap, gcStackAtlas);
	}

	while (slotsRemaining)
	{
		if (!*jitBitsRemaining)
		{
			if (*mapBytesRemaining)
			{
				*jitDescriptionBits = getNextDescriptionBit(jitDescriptionCursor);
				if (*stackAllocMapCursor != NULL)
				{
					*stackAllocMapBits = getNextDescriptionBit(stackAllocMapCursor);
				}
				--*mapBytesRemaining;
			}
			else
			{
				*jitDescriptionBits = 0;
			}
			*jitBitsRemaining = 8;
		}

		if (*jitDescriptionBits & 1)
		{
#ifdef J9VM_INTERP_STACKWALK_TRACING
			PORT_ACCESS_FROM_WALKSTATE(walkState);
			j9str_printf(PORTLIB, indexedTag, 64, "O-Slot: %s%d", slotDescription, slotsRemaining - 1);
			WALK_NAMED_O_SLOT((j9object_t*) scanCursor, indexedTag);
#else
			WALK_O_SLOT((j9object_t*) scanCursor);
#endif
		}
		else if (*stackAllocMapBits & 1)
		{
			jitWalkStackAllocatedObject(walkState, (j9object_t)scanCursor);
		}
		else
		{
#ifdef J9VM_INTERP_STACKWALK_TRACING
			PORT_ACCESS_FROM_WALKSTATE(walkState);
			j9str_printf(PORTLIB, indexedTag, 64, "I-Slot: %s%d", slotDescription, slotsRemaining - 1);
			WALK_NAMED_I_SLOT(scanCursor, indexedTag);
#else
			WALK_I_SLOT(scanCursor);
#endif

			if (walkState->flags & J9_STACKWALK_CHECK_I_SLOTS_FOR_OBJECTS) {
				/* check for verbose output */

#ifdef J9VM_INTERP_STACKWALK_TRACING
				if (walkState->userData1 == (void*) 8) {
					swPrintf(walkState, 3, "SCANNING I SLOT 0x%x contains: 0x%x, object is in heap 0x%x (0 is heap pointer) \n", scanCursor, *scanCursor, walkState->walkThread->javaVM->memoryManagerFunctions->j9gc_ext_check_is_valid_heap_object(walkState->walkThread->javaVM, *(j9object_t *)scanCursor, 0));
				}
#endif
				if (walkState->walkThread->javaVM->memoryManagerFunctions->j9gc_ext_check_is_valid_heap_object(walkState->walkThread->javaVM, *(j9object_t *)scanCursor, 0) == 0 ) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
					if ((walkState->userData1 == (void*)1) || (walkState->userData1 == (void*)8) ) {
						swPrintf(walkState, 3, "Possible Class Address: 0x%x at search PC 0x%x \n",*((j9object_t *)(*scanCursor)), walkState->pc );
					}
					if ((walkState->userData1 == (void*)1) || (walkState->userData1 == (void*)8) ) {
						swPrintf(walkState, 3, "Uncollected ref SLOT 0x%x pointing at object ref 0x%x for stackmap at seachPC 0x%x: \n",  scanCursor, *scanCursor, walkState->pc);
					}
#endif
					if (walkState->userData2 == (void*)4) {
						walkState->walkThread->javaVM->memoryManagerFunctions->j9gc_modron_global_collect(walkState->currentThread);
					} else {
						walkState->walkThread->javaVM->memoryManagerFunctions->j9gc_modron_local_collect(walkState->currentThread);
						walkState->walkThread->javaVM->memoryManagerFunctions->j9gc_modron_local_collect(walkState->currentThread);
					}
				}
			}
		}

		++(walkState->slotIndex);
		++scanCursor;
		--*jitBitsRemaining;
		*jitDescriptionBits >>= 1;
		*stackAllocMapBits >>= 1;
		--slotsRemaining;
	}
}

static void jitWalkRegisterMap(J9StackWalkState *walkState, void *stackMap, J9JITStackAtlas *gcStackAtlas)
{
	UDATA registerMap = getJitRegisterMap(walkState->jitInfo, stackMap) & J9SW_REGISTER_MAP_MASK;

#ifdef J9VM_INTERP_STACKWALK_TRACING
	swPrintf(walkState, 3, "\tJIT-RegisterMap = %p\n", registerMap);
#endif

	if (gcStackAtlas->internalPointerMap) {
		registerMap &= ~INTERNAL_PTR_REG_MASK;
	}

	if (registerMap) {
		UDATA count = J9SW_POTENTIAL_SAVED_REGISTERS;
		UDATA ** mapCursor;

#ifdef J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH
		mapCursor = (UDATA **) &(walkState->registerEAs);
#else
		mapCursor = ((UDATA **) &(walkState->registerEAs)) + J9SW_POTENTIAL_SAVED_REGISTERS - 1;
#endif

		walkState->slotType = J9_STACKWALK_SLOT_TYPE_JIT_REGISTER_MAP;
		walkState->slotIndex = 0;

		while (count) {
			if (registerMap & 1) {
				j9object_t * targetObject = *((j9object_t **) mapCursor);

#ifdef J9VM_INTERP_STACKWALK_TRACING
#ifdef J9VM_OUT_OF_PROCESS
				j9object_t oldObject = (targetObject == NULL) ? NULL : *targetObject;
#else
				j9object_t oldObject = *targetObject;
#endif
				j9object_t newObject;
				swPrintf(walkState, 4, "\t\tJIT-RegisterMap-O-Slot[%p] = %p (%s)\n", REMOTE_ADDR(targetObject), oldObject, jitRegisterNames[mapCursor - ((UDATA **) &(walkState->registerEAs))]);
#endif
				walkState->objectSlotWalkFunction(walkState->walkThread, walkState, targetObject, REMOTE_ADDR(targetObject));
#ifdef J9VM_INTERP_STACKWALK_TRACING
#ifdef J9VM_OUT_OF_PROCESS
				newObject = (targetObject == NULL) ? NULL : *targetObject;
#else
				newObject = *targetObject;
#endif
				if (oldObject != newObject) {
					swPrintf(walkState, 4, "\t\t\t-> %p\n", newObject);
				}
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
				lswRecordSlot(walkState, REMOTE_ADDR(targetObject), LSW_TYPE_O_SLOT, "O-Slot"); 
#endif

#endif
			}
#ifdef J9VM_INTERP_STACKWALK_TRACING
			else {
				UDATA * targetSlot = *((UDATA **) mapCursor);

				if (targetSlot) {
					swPrintf(walkState, 5, "\t\tJIT-RegisterMap-I-Slot[%p] = %p (%s)\n", REMOTE_ADDR(targetSlot), *targetSlot, jitRegisterNames[mapCursor - ((UDATA **) &(walkState->registerEAs))]);
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
					lswRecordSlot(walkState, REMOTE_ADDR(targetSlot), LSW_TYPE_I_SLOT, "I-Slot");
#endif
				}
			}
#endif

			++(walkState->slotIndex);
			--count;
			registerMap >>= 1;
#ifdef J9SW_REGISTER_MAP_WALK_REGISTERS_LOW_TO_HIGH
			++mapCursor;
#else
			--mapCursor;
#endif
		}
	}
}


#ifdef J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT

#if defined(J9VM_ARCH_S390)

/* On 390, either vector or floating point registers are preserved in the ELS, not both.
 * Matching VRs and FPRs overlap, with the FPR contents in the high-order bits of the VR,
 * meaning that when the VR is saved to memory, the FPR contents are in the lowest-memory
 * address of the VR save memory (due to 390 being big endian).
 *
 * Similarly, float values loaded using the LE instruction occupy the high-order bits of
 * the target FPR, meaning that the float contents are also in the lowest-memory address
 * of the save memory, whether it be VR or FPR.
 *
 * Currently, the 32 128-bit save slots for JIT VRs appear directly after the 16 64-bit
 * save slots for JIT FPRs, so if vector registers are enabled, the save location for a
 * JIT FPR is (base + (16 * 64) + (128 * FPRNumber)), or (base + (64 * (16 + (2 * FPRNumber)))).
 */
#define JIT_FPR_PARM_ADDRESS(walkState, fpParmNumber) \
	(((U_64*)((walkState)->walkedEntryLocalStorage->jitFPRegisterStorageBase)) + \
		(J9_ARE_ANY_BITS_SET((walkState)->walkThread->javaVM->extendedRuntimeFlags, J9_EXTENDED_RUNTIME_USE_VECTOR_REGISTERS) \
				? 16 + (2 * jitFloatArgumentRegisterNumbers[fpParmNumber]) \
				: jitFloatArgumentRegisterNumbers[fpParmNumber]) \
	)
#else
#define JIT_FPR_PARM_ADDRESS(walkState, fpParmNumber) (((U_64*)((walkState)->walkedEntryLocalStorage->jitFPRegisterStorageBase)) + jitFloatArgumentRegisterNumbers[fpParmNumber])
#endif

static void
jitWalkResolveMethodFrame_walkD(J9StackWalkState *walkState, UDATA ** pendingSendScanCursor, UDATA * floatRegistersRemaining)
{
	/* Doubles always occupy two stack slots.  Move the pendingSendScanCursor back to the
	 * beginning of the two slots.
	 */
	--(*pendingSendScanCursor);

	if (*floatRegistersRemaining) {
		UDATA fpParmNumber = J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT - *floatRegistersRemaining;
#ifdef J9VM_INTERP_STACKWALK_TRACING
		if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
#if !defined(J9VM_ENV_DATA64)
			WALK_NAMED_I_SLOT(((UDATA*)JIT_FPR_PARM_ADDRESS(walkState, fpParmNumber)) + 1, "JIT-sig-reg-I-Slot");
#endif
			WALK_NAMED_I_SLOT((UDATA*)JIT_FPR_PARM_ADDRESS(walkState, fpParmNumber), "JIT-sig-reg-I-Slot");
		}
#endif
		if (walkState->flags & J9_STACKWALK_SAVE_STACKED_REGISTERS) {
			*(U_64*)*pendingSendScanCursor = *JIT_FPR_PARM_ADDRESS(walkState, fpParmNumber);
		}
		--(*floatRegistersRemaining);
	}
#ifdef J9VM_INTERP_STACKWALK_TRACING
	else {
		if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
#if !defined(J9VM_ENV_DATA64)
			WALK_NAMED_I_SLOT((*pendingSendScanCursor) + 1, "JIT-sig-stk-I-Slot");
#endif
			WALK_NAMED_I_SLOT(*pendingSendScanCursor, "JIT-sig-stk-I-Slot");
		}
	}
#endif
}


static void
jitWalkResolveMethodFrame_walkF(J9StackWalkState *walkState, UDATA ** pendingSendScanCursor, UDATA * floatRegistersRemaining)
{
	if (*floatRegistersRemaining) {
		UDATA fpParmNumber = J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT - *floatRegistersRemaining;
#ifdef J9VM_INTERP_STACKWALK_TRACING
		if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
#if defined(J9SW_JIT_FLOATS_PASSED_AS_DOUBLES) && !defined(J9VM_ENV_DATA64)
			WALK_NAMED_I_SLOT(((UDATA*)JIT_FPR_PARM_ADDRESS(walkState, fpParmNumber)) + 1, "JIT-sig-reg-I-Slot");
#endif
			WALK_NAMED_I_SLOT((UDATA*)JIT_FPR_PARM_ADDRESS(walkState, fpParmNumber), "JIT-sig-reg-I-Slot");
		}
#endif
		if (walkState->flags & J9_STACKWALK_SAVE_STACKED_REGISTERS) {
#if defined(J9SW_JIT_FLOATS_PASSED_AS_DOUBLES)
			jitConvertStoredDoubleRegisterToSingle(JIT_FPR_PARM_ADDRESS(walkState, fpParmNumber),(U_32*)*pendingSendScanCursor);
#else
			*(U_32*)*pendingSendScanCursor = *(U_32*)JIT_FPR_PARM_ADDRESS(walkState, fpParmNumber);
#endif
		}
		--(*floatRegistersRemaining);
	}
#ifdef J9VM_INTERP_STACKWALK_TRACING
	else {
		if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
			WALK_NAMED_I_SLOT(*pendingSendScanCursor, "JIT-sig-stk-I-Slot");
		}
	}
#endif
}
#endif


static void
jitWalkResolveMethodFrame_walkJ(J9StackWalkState *walkState, UDATA ** pendingSendScanCursor, UDATA ** stackSpillCursor, UDATA * stackSpillCount)
{
	/* Longs always take 2 stack slots */

	--(*pendingSendScanCursor);

#if defined(J9SW_ARGUMENT_REGISTER_COUNT)
	if (*stackSpillCount) {
		/* On 64-bit, the long is in a single register.
		 *
		 * On 32-bit, parameter registers are used for longs such that the lower-numbered register
		 * holds the lower-memory half of the long (so that a store multiple on the register pair
		 * could be used to put the long back in memory).  This remains true for the case where only
		 * one parameter register remains.
		 */
#ifdef J9VM_INTERP_STACKWALK_TRACING
		if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
			WALK_NAMED_I_SLOT(*stackSpillCursor, "JIT-sig-reg-I-Slot");
		}
#endif
		if (walkState->flags & J9_STACKWALK_SAVE_STACKED_REGISTERS) {
			**pendingSendScanCursor = **stackSpillCursor;
		}
		--(*stackSpillCount);
		--(*stackSpillCursor);
#if !defined(J9VM_ENV_DATA64)
		if (*stackSpillCount) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
			if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
				WALK_NAMED_I_SLOT(*stackSpillCursor, "JIT-sig-reg-I-Slot");
			}
#endif
			if (walkState->flags & J9_STACKWALK_SAVE_STACKED_REGISTERS) {
				*((*pendingSendScanCursor) + 1) = **stackSpillCursor;
			}
			--(*stackSpillCount);
			--(*stackSpillCursor);
		} else
#endif
		{
#ifdef J9VM_INTERP_STACKWALK_TRACING
			if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
				WALK_NAMED_I_SLOT((*pendingSendScanCursor) + 1, "JIT-sig-stk-I-Slot");
			}
#endif
		}
	} else
#endif
	{
#ifdef J9VM_INTERP_STACKWALK_TRACING
		if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
			/* Long is entirely in memory */
			WALK_NAMED_I_SLOT((*pendingSendScanCursor) + 1, "JIT-sig-stk-I-Slot");
			WALK_NAMED_I_SLOT(*pendingSendScanCursor, "JIT-sig-stk-I-Slot");
		}
#endif
	}
}

static void
jitWalkResolveMethodFrame_walkI(J9StackWalkState *walkState, UDATA ** pendingSendScanCursor, UDATA ** stackSpillCursor,
								UDATA * stackSpillCount)
{
#ifdef J9SW_ARGUMENT_REGISTER_COUNT
	if (*stackSpillCount) {

		if (walkState->flags & J9_STACKWALK_SAVE_STACKED_REGISTERS) {
			*(U_32*)*pendingSendScanCursor = (U_32)**stackSpillCursor;
		}
#ifdef J9VM_INTERP_STACKWALK_TRACING
		if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
			WALK_NAMED_I_SLOT(*stackSpillCursor, "JIT-sig-reg-I-Slot");
		}
#endif
		--(*stackSpillCount);
		--(*stackSpillCursor);
	} else
#endif
	 {
#ifdef J9VM_INTERP_STACKWALK_TRACING
		if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
			WALK_NAMED_I_SLOT(*pendingSendScanCursor, "JIT-sig-stk-I-Slot");
		}
#endif
	}
}


static void jitWalkResolveMethodFrame(J9StackWalkState *walkState)
{
	UDATA walkStackedReceiver = FALSE;
	J9UTF8 * signature;
	UDATA pendingSendSlots;
	U_8 * sigData;
	UDATA sigChar;
#ifdef J9SW_ARGUMENT_REGISTER_COUNT
	UDATA * stackSpillCursor = NULL;
	UDATA stackSpillCount = 0;
#endif
#ifdef J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT
	UDATA floatRegistersRemaining = J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT;
#endif
	UDATA resolveFrameType = walkState->frameFlags & J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK;

	walkState->slotType = J9_STACKWALK_SLOT_TYPE_INTERNAL;
	walkState->slotIndex = -1;

	if (resolveFrameType == J9_STACK_FLAGS_JIT_RECOMPILATION_RESOLVE) {
		J9Method * method = READ_METHOD((J9Method *) JIT_RESOLVE_PARM(1));
		J9ROMMethod * romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);

		signature = J9ROMMETHOD_GET_SIGNATURE(J9_CLASS_FROM_METHOD(method)->romClass,romMethod);
		pendingSendSlots = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod); /* receiver is already included in this arg count */
		walkStackedReceiver = ((romMethod->modifiers & J9AccStatic) == 0);
#ifdef J9SW_ARGUMENT_REGISTER_COUNT
		stackSpillCount = J9SW_ARGUMENT_REGISTER_COUNT;
		walkState->unwindSP += J9SW_ARGUMENT_REGISTER_COUNT;
		stackSpillCursor = walkState->unwindSP - 1;
#endif
		walkState->unwindSP += getJitRecompilationResolvePushes();
	} else if (resolveFrameType == J9_STACK_FLAGS_JIT_LOOKUP_RESOLVE) {
		UDATA *interfaceObjectAndISlot = (UDATA *) JIT_RESOLVE_PARM(2);
		J9Class *resolvedClass = READ_CLASS((J9Class *) READ_UDATA(interfaceObjectAndISlot));
		UDATA iTableOffset = READ_UDATA(interfaceObjectAndISlot + 1);
		J9Method *ramMethod = NULL;
		J9ROMMethod *romMethod = NULL;
		if (J9_ARE_ANY_BITS_SET(iTableOffset, J9_ITABLE_OFFSET_DIRECT)) {
			ramMethod = (J9Method*)(iTableOffset & ~J9_ITABLE_OFFSET_TAG_BITS);
		} else if (J9_ARE_ANY_BITS_SET(iTableOffset, J9_ITABLE_OFFSET_VIRTUAL)) {
			UDATA vTableOffset = iTableOffset & ~J9_ITABLE_OFFSET_TAG_BITS;
			J9Class *jlObject = J9VMJAVALANGOBJECT_OR_NULL(walkState->walkThread->javaVM);
			ramMethod = *(J9Method**)((UDATA)jlObject + vTableOffset);
		} else {
			UDATA methodIndex = (iTableOffset - sizeof(J9ITable)) / sizeof(UDATA);
			/* Find the appropriate segment for the referenced method within the
			 * resolvedClass iTable.
			 */
			J9ITable *allInterfaces = (J9ITable*)resolvedClass->iTable;
			for(;;) {
				J9Class *interfaceClass = allInterfaces->interfaceClass;
				UDATA methodCount = J9INTERFACECLASS_ITABLEMETHODCOUNT(interfaceClass);
				if (methodIndex < methodCount) {
					/* iTable segment located */
					ramMethod = iTableMethodAtIndex(interfaceClass, methodIndex);
					break;
				}
				methodIndex -= methodCount;
				allInterfaces = allInterfaces->next;
			}
		}
		romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(ramMethod);
		signature = J9ROMMETHOD_GET_SIGNATURE(interfaceClass->romClass, romMethod);
		pendingSendSlots = J9_ARG_COUNT_FROM_ROM_METHOD(romMethod); /* receiver is already included in this arg count */
		walkStackedReceiver = TRUE;
#ifdef J9SW_ARGUMENT_REGISTER_COUNT
		stackSpillCount = J9SW_ARGUMENT_REGISTER_COUNT;
		walkState->unwindSP += J9SW_ARGUMENT_REGISTER_COUNT;
		stackSpillCursor = walkState->unwindSP - 1;
#endif

#ifdef J9SW_JIT_LOOKUP_INTERFACE_RESOLVE_OFFSET_TO_SAVED_RECEIVER
		if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
			swPrintf(walkState, 4, "\tObject push (picBuilder interface saved receiver)\n");
#endif
			WALK_O_SLOT((j9object_t*) (walkState->unwindSP + J9SW_JIT_LOOKUP_INTERFACE_RESOLVE_OFFSET_TO_SAVED_RECEIVER));
		}
#endif
		walkState->unwindSP += getJitVirtualMethodResolvePushes();
	} else if (resolveFrameType == J9_STACK_FLAGS_JIT_INDUCE_OSR_RESOLVE) {
		/* walkState->pc has been populated from the resolve frame */
		J9JITExceptionTable *metaData = jitGetExceptionTable(walkState);
		void *stackMap = NULL;
		void *inlineMap = NULL;
		J9Method *callingMethod = NULL;
		IDATA bytecodePCOffset = -1;
		void *inlinedCallSite = NULL;
		U_8 *invokeBytecode = NULL;
		U_8 bytecode = 0;
		U_16 cpIndex = 0;

		/* Being unable to find the metadata or stack maps is a fatal error */
		if (NULL == metaData) {
#ifdef J9VM_OUT_OF_PROCESS
			dbgError("*** OSR invalid JIT return address: %p\n", walkState->pc);
#else
			/* Only report errors if the stack walk caller has allowed it.
			 * If errors are not being reported, stop walking this frame,
			 * which will likely lead to further errors or crashes.
			 */
			if (walkState->errorMode != J9_STACKWALK_ERROR_MODE_IGNORE) {
				/* Avoid recursive error situations */
				if (0 == (walkState->walkThread->privateFlags & J9_PRIVATE_FLAGS_STACK_CORRUPT)) {
					walkState->walkThread->privateFlags |= J9_PRIVATE_FLAGS_STACK_CORRUPT;
					walkState->walkThread->javaVM->internalVMFunctions->invalidJITReturnAddress(walkState);
				}
			}
#endif
			return;
		}
		jitGetMapsFromPC(walkState->walkThread->javaVM, metaData, (UDATA)walkState->pc, &stackMap, &inlineMap);

		/* If there are no inlines, use the outer method.  Otherwise, use the innermost inline at the current PC */

		callingMethod = metaData->ramMethod;
		if (NULL != inlineMap) {
			if (NULL != getJitInlinedCallInfo(metaData)) {
				inlinedCallSite = getFirstInlinedCallSite(metaData, inlineMap);
				if (NULL != inlinedCallSite) {
					callingMethod = (J9Method*)getInlinedMethod(inlinedCallSite);
				}
			}
		}
		bytecodePCOffset = (IDATA)getCurrentByteCodeIndexAndIsSameReceiver(metaData, inlineMap, inlinedCallSite, NULL);
		invokeBytecode = J9_BYTECODE_START_FROM_RAM_METHOD(callingMethod) + bytecodePCOffset;
		bytecode = invokeBytecode[0];
		cpIndex = *(U_16*)(invokeBytecode + 1);
		switch(bytecode) {
		case JBinvokeinterface2: {
			/* invokeinterface2 is a 5-byte bytecode: [invokeinterface2, 0, invokeinterface, idx, idx] */
			cpIndex = *(U_16*)(invokeBytecode + 3);
			/* Intentional fall-through */
		}
		case JBinvokevirtual:
		case JBinvokespecial:
		case JBinvokeinterface:
		case JBinvokehandle:
		case JBinvokehandlegeneric:
			walkStackedReceiver = TRUE;
			/* Intentional fall-through */
		case JBinvokestatic: {
			J9ROMConstantPoolItem *romCPItem = &(J9_ROM_CP_FROM_CP(J9_CP_FROM_METHOD(callingMethod))[cpIndex]);
			signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef*)romCPItem));
			break;
		}
		case JBinvokespecialsplit: {
			/* cpIndex is actually an index into split table. Access the split table to get real constant pool index */
			J9ROMClass *romClass = J9_CLASS_FROM_METHOD(callingMethod)->romClass;
			U_16 realCPIndex = *(J9ROMCLASS_SPECIALSPLITMETHODREFINDEXES(romClass) + cpIndex);
			J9ROMConstantPoolItem *romCPItem = &(J9_ROM_CP_FROM_CP(J9_CP_FROM_METHOD(callingMethod))[realCPIndex]);
			signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef*)romCPItem));
			walkStackedReceiver = TRUE;
			break;
		}
		case JBinvokestaticsplit: {
			/* cpIndex is actually an index into split table. Access the split table to get real constant pool index */
			J9ROMClass *romClass = J9_CLASS_FROM_METHOD(callingMethod)->romClass;
			U_16 realCPIndex = *(J9ROMCLASS_STATICSPLITMETHODREFINDEXES(romClass) + cpIndex);
			J9ROMConstantPoolItem *romCPItem = &(J9_ROM_CP_FROM_CP(J9_CP_FROM_METHOD(callingMethod))[realCPIndex]);
			signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMMETHODREF_NAMEANDSIGNATURE((J9ROMMethodRef*)romCPItem));
			break;
		}
		case JBinvokedynamic: {
			/* invokedynamic has no receiver, all pushed arguments are described by the signature
			 * in the call site data.
			 */
			J9ROMClass *romClass = J9_CLASS_FROM_METHOD(callingMethod)->romClass;
			J9SRP *callSiteData = (J9SRP*)J9ROMCLASS_CALLSITEDATA(romClass);
			J9ROMNameAndSignature *nameAndSig = SRP_PTR_GET(callSiteData + cpIndex, J9ROMNameAndSignature*);
			signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(nameAndSig);
			break;
		}
		default:
			/* Not an invoke, no special handling required */
			return;
		}
		pendingSendSlots = getSendSlotsFromSignature(J9UTF8_DATA(signature)) + (walkStackedReceiver ? 1 : 0);
		/* Nothing is in registers at this point - the JIT has populated the outgoing argument area in the stack */
#ifdef J9SW_ARGUMENT_REGISTER_COUNT
		stackSpillCount = 0;
#endif
#ifdef J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT
		floatRegistersRemaining = 0;
#endif
	} else {
		UDATA cpIndex;
		J9ConstantPool * constantPool;
		J9ROMMethodRef * romMethodRef;

		if ((resolveFrameType == J9_STACK_FLAGS_JIT_STATIC_METHOD_RESOLVE) || (resolveFrameType == J9_STACK_FLAGS_JIT_SPECIAL_METHOD_RESOLVE)) {
#ifdef J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT
			floatRegistersRemaining = 0;
#endif
			constantPool = READ_CP((J9ConstantPool *) JIT_RESOLVE_PARM(2));
			cpIndex = JIT_RESOLVE_PARM(3);
			walkState->unwindSP += getJitStaticMethodResolvePushes();
			walkStackedReceiver = (resolveFrameType == J9_STACK_FLAGS_JIT_SPECIAL_METHOD_RESOLVE);
#ifdef J9SW_ARGUMENT_REGISTER_COUNT
			stackSpillCount = 0;
#endif
			cpIndex = jitGetRealCPIndex(walkState->currentThread, J9_CLASS_FROM_CP(constantPool)->romClass, cpIndex);
		} else {
			UDATA * indexAndLiterals = (UDATA *) JIT_RESOLVE_PARM(1);

			constantPool = READ_CP((J9ConstantPool *) READ_UDATA(indexAndLiterals));
			cpIndex = READ_UDATA(indexAndLiterals + 1);

			walkStackedReceiver = TRUE;
#ifdef J9SW_ARGUMENT_REGISTER_COUNT
			stackSpillCount = J9SW_ARGUMENT_REGISTER_COUNT;
			walkState->unwindSP += J9SW_ARGUMENT_REGISTER_COUNT;
			stackSpillCursor = walkState->unwindSP - 1;
#endif

#ifdef J9SW_JIT_VIRTUAL_METHOD_RESOLVE_OFFSET_TO_SAVED_RECEIVER
			if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
				swPrintf(walkState, 4, "\tObject push (picBuilder virtual saved receiver)\n");
#endif
				WALK_O_SLOT((j9object_t*) (walkState->unwindSP + J9SW_JIT_VIRTUAL_METHOD_RESOLVE_OFFSET_TO_SAVED_RECEIVER));
			}
#endif
			walkState->unwindSP += getJitVirtualMethodResolvePushes();
		}

		romMethodRef = (J9ROMMethodRef *) &(constantPool->romConstantPool[cpIndex]);
		signature = J9ROMNAMEANDSIGNATURE_SIGNATURE(J9ROMMETHODREF_NAMEANDSIGNATURE(romMethodRef));
		pendingSendSlots = getSendSlotsFromSignature(J9UTF8_DATA(signature)) + (walkStackedReceiver ? 1 : 0);
	}

	if ((walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) || (walkState->flags & J9_STACKWALK_SAVE_STACKED_REGISTERS)) {
		UDATA * pendingSendScanCursor = walkState->unwindSP + pendingSendSlots - 1;

#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 3, "\tPending send scan cursor initialized to %p\n", REMOTE_ADDR(pendingSendScanCursor));
#endif
		if (walkStackedReceiver) {
#ifdef J9SW_ARGUMENT_REGISTER_COUNT
			if (stackSpillCount) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
				swPrintf(walkState, 4, "\tObject push (receiver in register spill area)\n");
#endif
				if (walkState->flags & J9_STACKWALK_SAVE_STACKED_REGISTERS) {
					/* Restore stacked receiver */
					*pendingSendScanCursor = *stackSpillCursor;
				}
				if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
					WALK_O_SLOT((j9object_t*) stackSpillCursor);
				}
				--stackSpillCount;
				--stackSpillCursor;
			} else {
#endif
#ifdef J9VM_INTERP_STACKWALK_TRACING
				swPrintf(walkState, 4, "\tObject push (receiver in stack)\n");
#endif
				if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
					WALK_O_SLOT((j9object_t*) pendingSendScanCursor);
				}
#ifdef J9SW_ARGUMENT_REGISTER_COUNT
			}
#endif
			--pendingSendScanCursor;
		}

#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 3, "\tMethod signature: %.*s\n", (U_32) J9UTF8_LENGTH(signature), J9UTF8_DATA(signature));
#endif

		sigData = J9UTF8_DATA(signature);
		jitNextUTFChar(&sigData); /* skip the opening ( */
		while ((sigChar = jitNextSigChar(&sigData)) != ')') {

			switch (sigChar) {
				case 'L':
#ifdef J9SW_ARGUMENT_REGISTER_COUNT
					if (stackSpillCount) {

						if (walkState->flags & J9_STACKWALK_SAVE_STACKED_REGISTERS) {
							/* Restore an object */
							*pendingSendScanCursor = *stackSpillCursor;
						}
						if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
							WALK_NAMED_O_SLOT((j9object_t*) stackSpillCursor, "JIT-sig-reg-O-Slot");
						}
						--stackSpillCount;
						--stackSpillCursor;
					} else {
#endif
						if (walkState->flags & J9_STACKWALK_ITERATE_O_SLOTS) {
							WALK_NAMED_O_SLOT((j9object_t*) pendingSendScanCursor, "JIT-sig-stk-O-Slot");
						}
#ifdef J9SW_ARGUMENT_REGISTER_COUNT
					}
#endif
					break;

				case 'D':
#ifdef J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT
					jitWalkResolveMethodFrame_walkD(walkState, &pendingSendScanCursor, &floatRegistersRemaining);
					break;
#else
					/* no float regs - double treated as long */
#endif

				case 'J':
#if defined(J9SW_ARGUMENT_REGISTER_COUNT)
					jitWalkResolveMethodFrame_walkJ(walkState, &pendingSendScanCursor, &stackSpillCursor, &stackSpillCount);
#else
					jitWalkResolveMethodFrame_walkJ(walkState, &pendingSendScanCursor, NULL, NULL);
#endif
					break;

				case 'F':
#ifdef J9SW_JIT_FLOAT_ARGUMENT_REGISTER_COUNT
					jitWalkResolveMethodFrame_walkF(walkState, &pendingSendScanCursor, &floatRegistersRemaining);
					break;
#else
					/* no float regs - float treated as int */
#endif

				case 'I':
				default:
#ifdef J9SW_ARGUMENT_REGISTER_COUNT
					jitWalkResolveMethodFrame_walkI(walkState, &pendingSendScanCursor, &stackSpillCursor, &stackSpillCount);
#else
					jitWalkResolveMethodFrame_walkI(walkState, &pendingSendScanCursor, NULL, NULL);
#endif
					break;

			}

			--pendingSendScanCursor;
		}

	}

#ifdef J9SW_NEEDS_JIT_2_INTERP_CALLEE_ARG_POP
	walkState->unwindSP += pendingSendSlots;
#endif
	walkState->argCount = pendingSendSlots;
}


static UDATA jitNextUTFChar(U_8 ** pUtfData)
{
	U_8 * utfData = *pUtfData;
	UDATA utfChar;

	utfChar = (UDATA) *(utfData++);
	if (utfChar & 0x80) {
		if (utfChar & 0x20) {
			utfChar = (utfChar & 0x0F) << 12;
			utfChar |= ((((UDATA) *(utfData++)) & 0x3F) << 6);
			utfChar |= (((UDATA) *(utfData++)) & 0x3F);
		} else {
			utfChar = (utfChar & 0x1F) << 6;
			utfChar |= (((UDATA) *(utfData++)) & 0x3F);
		}
	}

	*pUtfData = utfData;
	return utfChar;
}
static UDATA jitNextSigChar(U_8 ** utfData)
{
	UDATA utfChar;

	utfChar = jitNextUTFChar(utfData);

	switch (utfChar) {
		case '[':
			do {
				utfChar = jitNextUTFChar(utfData);
			} while (utfChar == '[');
			if (utfChar != 'L') {
				utfChar = 'L';
				break;
			}
			/* Fall through to consume type name, utfChar == 'L' for return value */

		case 'L':
			while (jitNextUTFChar(utfData) != ';') ;
	}

	return utfChar;
}

#if (defined(J9VM_INTERP_STACKWALK_TRACING)) /* priv. proto (autogen) */

void jitPrintRegisterMapArray(J9StackWalkState * walkState, char * description)
{
	UDATA i;
	UDATA ** mapCursor = (UDATA **) &(walkState->registerEAs);

	for (i = 0; i < J9SW_POTENTIAL_SAVED_REGISTERS; ++i) {
		UDATA * registerSaveAddress = mapCursor[i];

		if (registerSaveAddress) {
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING
			lswRecordSlot(walkState, REMOTE_ADDR(registerSaveAddress), LSW_TYPE_JIT_REG_SLOT, "%s: %s", description, jitRegisterNames[i]);
#endif
			swPrintf(walkState, 3, "\tJIT-%s-RegisterMap[%p] = %p (%s)\n", description, REMOTE_ADDR(registerSaveAddress), 
					 *registerSaveAddress, jitRegisterNames[i]);
		}
	}
}

#endif /* J9VM_INTERP_STACKWALK_TRACING (autogen) */




static void jitAddSpilledRegistersForResolve(J9StackWalkState * walkState)
{
	J9STACKSLOT * slotCursor = walkState->walkedEntryLocalStorage->jitGlobalStorageBase;
	UDATA ** mapCursor = (UDATA **) &(walkState->registerEAs);
	UDATA i;

	for (i = 0; i < J9SW_POTENTIAL_SAVED_REGISTERS; ++i) {
		*mapCursor++ = (UDATA *) slotCursor++;
	}

#ifdef J9VM_INTERP_STACKWALK_TRACING
	jitPrintRegisterMapArray(walkState, "Resolve");
#endif
}

static void jitAddSpilledRegistersForINL(J9StackWalkState * walkState)
{
	J9STACKSLOT * slotCursor = walkState->walkedEntryLocalStorage->jitGlobalStorageBase;
	UDATA ** mapCursor = (UDATA **) &(walkState->registerEAs);
	UDATA i;

	for (i = 0; i < J9SW_JIT_CALLEE_PRESERVED_SIZE; ++i) {
		UDATA regNumber = jitCalleeSavedRegisterList[i];

		mapCursor[regNumber] = (UDATA *) &slotCursor[regNumber];
	}

#ifdef J9VM_INTERP_STACKWALK_TRACING
	jitPrintRegisterMapArray(walkState, "INL");
#endif
}

static void jitAddSpilledRegistersForJ2I(J9StackWalkState * walkState)
{
	J9SFJ2IFrame * j2iFrame = (J9SFJ2IFrame *) ((U_8 *) walkState->bp - sizeof(J9SFJ2IFrame) + sizeof(UDATA));
	UDATA * slotCursor = ((UDATA *) &(j2iFrame->previousJ2iFrame)) + 1;
	UDATA ** mapCursor = (UDATA **) &(walkState->registerEAs);
	UDATA i;

	for (i = 0; i < J9SW_JIT_CALLEE_PRESERVED_SIZE; ++i) {
		UDATA regNumber = jitCalleeSavedRegisterList[i];

		mapCursor[regNumber] = slotCursor++;
	}

#ifdef J9VM_INTERP_STACKWALK_TRACING
	jitPrintRegisterMapArray(walkState, "J2I");
#endif
}

#if (defined(J9VM_JIT_FULL_SPEED_DEBUG)) /* priv. proto (autogen) */

static J9JITExceptionTable * jitGetExceptionTable(J9StackWalkState * walkState)
{
#ifdef J9VM_INTERP_STACKWALK_TRACING
	J9JITDecompilationInfo * stack;
#endif
	J9JITExceptionTable * result = jitGetExceptionTableFromPC(walkState->walkThread, (UDATA) walkState->pc);

	if (result) return result;

	/* Check to see if the PC is a decompilation return point and if so, use the real PC for finding the metaData */

	if (walkState->decompilationStack) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
/*		swPrintf(walkState, 1, "(ws pcaddr = %p, dc tos = %p, pcaddr = %p, pc = %p)\n", walkState->pcAddress, walkState->decompilationStack, walkState->decompilationStack->pcAddress, walkState->decompilationStack->pc); */
#endif
		if (walkState->pcAddress == walkState->decompilationStack->pcAddress) {
			walkState->pc = walkState->decompilationStack->pc;
			if (J9_STACK_FLAGS_JIT_EXCEPTION_CATCH_RESOLVE == (walkState->resolveFrameFlags & J9_STACK_FLAGS_JIT_FRAME_SUB_TYPE_MASK)) {
				walkState->pc += 1;
			}
			walkState->decompilationRecord = walkState->decompilationStack;
			walkState->decompilationStack = walkState->decompilationStack->next;
			return jitGetExceptionTableFromPC(walkState->walkThread, (UDATA) walkState->pc);
		}
#ifdef J9VM_INTERP_STACKWALK_TRACING
		stack = walkState->decompilationStack;
		while ((stack = stack->next) != NULL) {
			if (walkState->pcAddress == walkState->decompilationStack->pcAddress) {
				swPrintf(walkState, 0, "\n");
				swPrintf(walkState, 0, "\n");
				swPrintf(walkState, 0, "**** decomp found not on TOS! ****\n");
				swPrintf(walkState, 0, "\n");
				swPrintf(walkState, 0, "\n");
			}
		}
#endif
	}

	return NULL;
}
#endif /* J9VM_JIT_FULL_SPEED_DEBUG (autogen) */


#define JIT_ARTIFACT_SEARCH_CACHE_SIZE 256
#define JIT_ARTIFACT_SEARCH_CACHE_DIMENSION 8
/* hash values for 64 bit and 32 bit platform. Just selected a random prime number, may need to modify later */
#if defined(J9VM_ENV_DATA64)
#define JIT_ARTIFACT_SEARCH_CACHE_HASH_VALUE ((UDATA)J9CONST_U64(17446744073709553729))
#define BITS_IN_INTEGER 64
#else
#define JIT_ARTIFACT_SEARCH_CACHE_HASH_VALUE ((UDATA)4102541685U)
#define BITS_IN_INTEGER 32
#endif

#define JIT_ARTIFACT_SEARCH_CACHE_HASH_RESULT(key) \
    (((key) * JIT_ARTIFACT_SEARCH_CACHE_HASH_VALUE) >> (BITS_IN_INTEGER - JIT_ARTIFACT_SEARCH_CACHE_DIMENSION))

typedef struct TR_jit_artifact_search_cache
{
	UDATA searchValue;
	J9JITExceptionTable * exceptionTable;
} TR_jit_artifact_search_cache;

J9JITExceptionTable * jitGetExceptionTableFromPC(J9VMThread * vmThread, UDATA jitPC)
{
	UDATA maskedPC = (UDATA)MASK_PC(jitPC);
#ifdef J9VM_OUT_OF_PROCESS
	J9JITExceptionTable* remoteMetaData = dbgFindJITMetaData(vmThread->javaVM->jitConfig->translationArtifacts, maskedPC);
	return dbgReadJITMetaData(remoteMetaData);
#else
#ifdef J9JIT_ARTIFACT_SEARCH_CACHE_ENABLE
	J9JITExceptionTable *exceptionTable = NULL;
	TR_jit_artifact_search_cache *artifactSearchCache = vmThread->jitArtifactSearchCache;
	TR_jit_artifact_search_cache *cacheEntry = NULL;
	if (NULL == artifactSearchCache) {
		PORT_ACCESS_FROM_JAVAVM(vmThread->javaVM);
		artifactSearchCache = j9mem_allocate_memory(JIT_ARTIFACT_SEARCH_CACHE_SIZE * sizeof (TR_jit_artifact_search_cache), OMRMEM_CATEGORY_JIT);
		if (NULL == artifactSearchCache) {
			return jit_artifact_search(vmThread->javaVM->jitConfig->translationArtifacts, maskedPC);
		}
		memset(artifactSearchCache, 0, JIT_ARTIFACT_SEARCH_CACHE_SIZE * sizeof(TR_jit_artifact_search_cache));
		vmThread->jitArtifactSearchCache = artifactSearchCache;
	}
	cacheEntry = &(artifactSearchCache[JIT_ARTIFACT_SEARCH_CACHE_HASH_RESULT(maskedPC)]);
	if (cacheEntry->searchValue == maskedPC) {
		exceptionTable = cacheEntry->exceptionTable;
	} else {
		exceptionTable = jit_artifact_search(vmThread->javaVM->jitConfig->translationArtifacts, maskedPC);
		if (NULL != exceptionTable) {
			cacheEntry->searchValue = maskedPC;
			cacheEntry->exceptionTable = exceptionTable;
		}
	}
	return exceptionTable;
#else
	return jit_artifact_search(vmThread->javaVM->jitConfig->translationArtifacts, maskedPC);
#endif /* J9JIT_ARTIFACT_SEARCH_CACHE_ENABLE */
#endif /* J9VM_OUT_OF_PROCESS */
}



#if (!defined(J9VM_OUT_OF_PROCESS)) /* priv. proto (autogen) */

/* Only callable from inside a visible-only walk on the current thread (with VM access) */

static void
jitDropToCurrentFrame(J9StackWalkState * walkState)
{
	J9VMThread * vmThread = walkState->walkThread;
	J9STACKSLOT * elsSaveArea = walkState->walkedEntryLocalStorage->jitGlobalStorageBase;
	UDATA ** registerMap = (UDATA **) &(walkState->registerEAs);
	UDATA i;
	J9SFJITResolveFrame * resolveFrame;
	J9I2JState * currentState;
	U_8 * pc = walkState->pc;

#ifdef J9VM_JIT_FULL_SPEED_DEBUG
	J9JITDecompilationInfo * currentFrameDecompilation = NULL;

	if (J9_FSD_ENABLED(vmThread->javaVM)) {

		/* If there's a decompilation for the current frame, fix it to point back to the new resolve frame */

		currentFrameDecompilation = vmThread->javaVM->jitConfig->jitCleanUpDecompilationStack(vmThread, walkState, FALSE);
		if (currentFrameDecompilation != NULL) {
			pc = *(walkState->pcAddress);
		}
	}
#endif

	/* Copy mapped regs into ELS - modify register map to reflect this */

	for (i = 0; i < J9SW_POTENTIAL_SAVED_REGISTERS; ++i) {
		UDATA * newSaveSlot = (UDATA *) (&elsSaveArea[i]);
		UDATA * registerSaveAddress = registerMap[i];

		if (registerSaveAddress != NULL) {
			*newSaveSlot = *registerSaveAddress;
		}
		registerMap[i] = newSaveSlot;
	}

	/* Copy current I2J state back to ELS */

	currentState = walkState->i2jState;
	if (currentState != NULL) {
		J9I2JState * elsState = &(vmThread->entryLocalStorage->i2jState);

		elsState->pc = currentState->pc;
		elsState->literals = currentState->literals;
		elsState->a0 = currentState->a0;
		elsState->returnSP = currentState->returnSP;
		walkState->i2jState = elsState;
	}

	/* Build generic resolve frame */
	resolveFrame = jitPushResolveFrame(vmThread, walkState->unwindSP, pc);
#ifdef J9VM_JIT_FULL_SPEED_DEBUG
	if (currentFrameDecompilation != NULL) {
		currentFrameDecompilation->pcAddress = (U_8 **) &(resolveFrame->returnAddress);
	}
#endif

	vmThread->j2iFrame = walkState->j2iFrame;
}

#endif /* J9VM_!OUT_OF_PROCESS (autogen) */

/* This function is invoked for each stack allocated object. Unless a specific callback
 * has been provided for stack allocated objects it creates synthetic slots for the object's
 * fields and reports them using WALK_O_SLOT()
 */
static void
jitWalkStackAllocatedObject(J9StackWalkState * walkState, j9object_t object)
{
	J9JavaVM* vm = walkState->walkThread->javaVM;
	J9MM_IterateObjectDescriptor descriptor;
	UDATA iterateObjectSlotsFlags = 0;

	if (J9_STACKWALK_INCLUDE_ARRAYLET_LEAVES == (walkState->flags & J9_STACKWALK_INCLUDE_ARRAYLET_LEAVES)) {
		iterateObjectSlotsFlags |= j9mm_iterator_flag_include_arraylet_leaves;
	}
	
#if defined (J9VM_INTERP_STACKWALK_TRACING)
	swPrintf(walkState, 4, "\t\tSA-Obj[%p]\n", REMOTE_ADDR(object));

#endif

	MM_FN(vm, j9mm_initialize_object_descriptor)(REMOTE_ADDR(vm), &descriptor, REMOTE_ADDR(object));

	MM_FN(vm, j9mm_iterate_object_slots)(
		REMOTE_ADDR(vm),
		vm->portLibrary,
		&descriptor,
		iterateObjectSlotsFlags,
		stackAllocatedObjectSlotWalkFunction,
		walkState);
}

static jvmtiIterationControl
stackAllocatedObjectSlotWalkFunction(J9JavaVM *javaVM, J9MM_IterateObjectDescriptor *objectDesc, J9MM_IterateObjectRefDescriptor *refDesc, void *userData)
{
	J9StackWalkState * walkState = userData; /* used implicitly by WALK_NAMED_INDIRECT_O_SLOT */

#if defined (J9VM_INTERP_STACKWALK_TRACING)
	j9object_t oldValue = refDesc->object;

	swPrintf(walkState, 4, "\t\t\tF-Slot[%p] = %p\n", refDesc->fieldAddress, refDesc->object);
#ifdef J9VM_INTERP_LINEAR_STACKWALK_TRACING	
	lswRecordSlot(walkState, refDesc->fieldAddress, LSW_TYPE_F_SLOT, "F-Slot");
#endif	
#if !defined(J9VM_OUT_OF_PROCESS)
	swMarkSlotAsObject(walkState, (j9object_t*)(((UDATA)refDesc->fieldAddress) & ~(UDATA)(sizeof(UDATA) - 1)));
#endif /* J9VM_OUT_OF_PROCESS */
#endif /* J9VM_INTERP_STACKWALK_TRACING */

	walkState->objectSlotWalkFunction(walkState->currentThread, walkState, &refDesc->object, refDesc->fieldAddress);

#if defined (J9VM_INTERP_STACKWALK_TRACING)
	if (oldValue != refDesc->object) {
		swPrintf(walkState, 4, "\t\t\t\t-> %p\n", refDesc->object);
	}
#endif /* J9VM_INTERP_STACKWALK_TRACING */

	return JVMTI_ITERATION_CONTINUE;
}

/*
 * userData1 = curInfo
 * userData2 = lastInfo / monitorCount
 * userData3 = monitorEnterRecords
 * userData4 = stack depth, including inlines
 */
UDATA 
jitGetOwnedObjectMonitors(J9StackWalkState *walkState)
{
	J9JITStackAtlas *gcStackAtlas;
	void *stackMap;
	void *inlineMap;
	U_8 *liveMonitorMap;
	U_16 numberOfMapBits;

	if (NULL == walkState->userData1) {
		return countOwnedObjectMonitors(walkState);
	}

	/* get the stackmap and inline map for the given pc (this is a single walk of jit metadata) */
	jitGetMapsFromPC(walkState->walkThread->javaVM, walkState->jitInfo, (UDATA)walkState->pc, &stackMap, &inlineMap);

	/* get a slot map of all live monitors on the JIT frame.  May include slots from inlined methods */
	liveMonitorMap = getJitLiveMonitors(walkState->jitInfo, stackMap);
	gcStackAtlas = (J9JITStackAtlas *)getJitGCStackAtlas(walkState->jitInfo);
	numberOfMapBits = getJitNumberOfMapBytes(gcStackAtlas) << 3;

	/*
	 * walk the inlined methods and use the new monitor mask interface to find live
	 * monitors corresponding to each inlined frame
	 */
	if (inlineMap) {
		void *inlinedCallSite;

		for (
			inlinedCallSite = getFirstInlinedCallSite(walkState->jitInfo, inlineMap);
			inlinedCallSite != NULL;
			inlinedCallSite = getNextInlinedCallSite(walkState->jitInfo, inlinedCallSite)
			) {

			if (liveMonitorMap) {
				U_8 *inlineMonitorMask = getMonitorMask(gcStackAtlas, inlinedCallSite);
				if (NULL != inlineMonitorMask) {
					walkLiveMonitorSlots(walkState, gcStackAtlas, liveMonitorMap, inlineMonitorMask, numberOfMapBits);
				}
			}
			/* increment stack depth */
			walkState->userData4 = (void *)(((UDATA)walkState->userData4) + 1);
		}
	}

	/* Get the live monitors for the outer frame */
	if (liveMonitorMap) {
		walkLiveMonitorSlots(walkState, gcStackAtlas, liveMonitorMap, getMonitorMask(gcStackAtlas, NULL), numberOfMapBits);
	}

	return J9_STACKWALK_KEEP_ITERATING;
}

/*
 * userData4 (stack depth) is ignored in this case
 */
static UDATA
countOwnedObjectMonitors(J9StackWalkState *walkState)
{
	J9JITStackAtlas *gcStackAtlas;
	void *stackMap;
	void *inlineMap;
	U_8 *liveMonitorMap;
	U_16 numberOfMapBits;

	/* get the stackmap and inline map for the given pc (this is a single walk of jit metadata) */
	jitGetMapsFromPC(walkState->walkThread->javaVM, walkState->jitInfo, (UDATA)walkState->pc, &stackMap, &inlineMap);

	/* get a slot map of all live monitors on the JIT frame.  May include slots from inlined methods */
	liveMonitorMap = getJitLiveMonitors(walkState->jitInfo, stackMap);
	gcStackAtlas = (J9JITStackAtlas *)getJitGCStackAtlas(walkState->jitInfo);
	numberOfMapBits = getJitNumberOfMapBytes(gcStackAtlas) << 3;

	/*
	 * walk the inlined methods and use the new monitor mask interface to find live
	 * monitors corresponding to each inlined frame
	 */
	if (inlineMap) {
		void *inlinedCallSite;

		for (
			inlinedCallSite = getFirstInlinedCallSite(walkState->jitInfo, inlineMap);
			inlinedCallSite != NULL;
			inlinedCallSite = getNextInlinedCallSite(walkState->jitInfo, inlinedCallSite)
			) {

			if (liveMonitorMap) {
				U_8 *inlineMonitorMask = getMonitorMask(gcStackAtlas, inlinedCallSite);
				if (NULL != inlineMonitorMask) {
					countLiveMonitorSlots(walkState, gcStackAtlas, liveMonitorMap, inlineMonitorMask, numberOfMapBits);
				}
			}
		}
	}

	/* Get the live monitors for the outer frame */
	if (liveMonitorMap) {
		countLiveMonitorSlots(walkState, gcStackAtlas, liveMonitorMap, getMonitorMask(gcStackAtlas, 0), numberOfMapBits);
	}
	return J9_STACKWALK_KEEP_ITERATING;
}

static UDATA
walkLiveMonitorSlots(J9StackWalkState *walkState, J9JITStackAtlas *gcStackAtlas,
		U_8 *liveMonitorMap, U_8 *monitorMask, U_16 numberOfMapBits)
{
	J9ObjectMonitorInfo *info = walkState->userData1;
	J9ObjectMonitorInfo *lastInfo = walkState->userData2;
	j9object_t *objAddress;
	U_16 i;
	U_8 bit;

	for (i = 0; i < numberOfMapBits; ++i) {
		bit = liveMonitorMap[i >> 3] & monitorMask[i >> 3] & (1 << (i & 7));
		if (bit) {
			if (info > lastInfo) {
				/* don't overflow buffer */
				return J9_STACKWALK_STOP_ITERATING;
			}

			objAddress = getSlotAddress(walkState, gcStackAtlas, i);

			/* CMVC 188386 : if the object is stack allocates and the object is discontiguous on stack,
			 * the jit just stores a null in the slot. Skip this slot.
			 */
			if (NULL != objAddress) {
				j9object_t obj = *objAddress;

				if (NULL != obj) {
					info->object = obj;
					info->count = 1;
					info->depth = (UDATA)walkState->userData4;
					++info;
				}
			}
		}
	}
	walkState->userData1 = info;

	return J9_STACKWALK_KEEP_ITERATING;
}

static void
countLiveMonitorSlots(J9StackWalkState *walkState, J9JITStackAtlas *gcStackAtlas, U_8 *liveMonitorMap, U_8 *monitorMask, U_16 numberOfMapBits)
{
	IDATA monitorCount = (IDATA)walkState->userData2;
	U_16 i;
	U_8 bit;

	for (i = 0; i < numberOfMapBits; ++i) {
		bit = liveMonitorMap[i >> 3] & monitorMask[i >> 3];
		bit >>= i & 7;

		if (bit & 1) {
			j9object_t *objAddress = getSlotAddress(walkState, gcStackAtlas, i);
			/* CMVC 188386 : if the object is stack allocates and the object is discontiguous on stack,
			 * the jit stores a null in the slot. Skip this slot.
			 */
			if ((NULL != objAddress) && (NULL != *objAddress)) {
				monitorCount += 1;
			}
		}
	}
	walkState->userData2 = (void *)monitorCount;
}

static j9object_t *
getSlotAddress(J9StackWalkState *walkState, J9JITStackAtlas *gcStackAtlas, U_16 slot)
{
	UDATA *slotAddress;
	j9object_t *retobj;
	U_16 numParms;

	/* The base address depends on the range of the slot index */
	numParms = getJitNumberOfParmSlots(gcStackAtlas);
	if (slot < numParms) {
		slotAddress = getObjectArgScanCursor(walkState);
	} else {
		slotAddress = getObjectTempScanCursor(walkState);
		slot -= numParms;
	}

	slotAddress += slot;
	retobj = (j9object_t *)slotAddress;

	return retobj;
}


/**
 * Walk the stack slots in a single OSR frame.
 *
 * @param[in] *walkState current J9StackWalkState pointer
 * @param[in] *osrFrame the current OSR frame
 *
 * @return pointer to the next OSR frame in the buffer
 */
static J9OSRFrame*
jitWalkOSRFrame(J9StackWalkState *walkState, J9OSRFrame *osrFrame)
{
	J9Method *method = osrFrame->method;
	U_8 *bytecodePC = osrFrame->bytecodePCOffset + J9_BYTECODE_START_FROM_RAM_METHOD(osrFrame->method);
	UDATA numberOfLocals = osrFrame->numberOfLocals;
	UDATA maxStack = osrFrame->maxStack;
	UDATA pendingStackHeight = osrFrame->pendingStackHeight;
	UDATA offsetPC = bytecodePC - J9_BYTECODE_START_FROM_RAM_METHOD(method);
	UDATA *localSlots = ((UDATA*)(osrFrame + 1)) + maxStack;
	UDATA *nextFrame = localSlots + numberOfLocals;
	J9MonitorEnterRecord *enterRecord = osrFrame->monitorEnterRecords;

#ifdef J9VM_INTERP_STACKWALK_TRACING
	{
		J9Method *stateMethod = walkState->method;
		swPrintf(walkState, 3, "\tJIT-OSRFrame = %p, bytecodePC = %p, numberOfLocals = %d, maxStack = %d, pendingStackHeight = %d\n", osrFrame, bytecodePC, numberOfLocals, maxStack, pendingStackHeight);
		walkState->method = method;
		swPrintMethod(walkState);
		walkState->method = stateMethod;
	}
#endif
	walkBytecodeFrameSlots(walkState, method, offsetPC,
			localSlots - 1, pendingStackHeight,
			nextFrame - 1, numberOfLocals, TRUE);
	while (NULL != enterRecord) {
#ifdef J9VM_INTERP_STACKWALK_TRACING
		swPrintf(walkState, 3, "\tJIT-OSR-monitorEnterRecord = %p\n", enterRecord);
#endif
		WALK_O_SLOT(&enterRecord->object);
		enterRecord = enterRecord->next;
	}
	return (J9OSRFrame*)nextFrame;
}


/**
 * Walk the stack slots in all frames in the OSR buffer.
 *
 * @param[in] *walkState current J9StackWalkState pointer
 * @param[in] *osrBuffer the OSR buffer
 */
static void
jitWalkOSRBuffer(J9StackWalkState *walkState, J9OSRBuffer *osrBuffer)
{
	UDATA numberOfFrames = osrBuffer->numberOfFrames;
	J9OSRFrame *currentFrame = (J9OSRFrame*)(osrBuffer + 1);

#ifdef J9VM_INTERP_STACKWALK_TRACING
	swPrintf(walkState, 3, "\tJIT-OSRBuffer = %p, numberOfFrames = %d\n", osrBuffer, numberOfFrames);
#endif

	/* Walk all of the frames (assume there is at least one) */

	do {
		currentFrame = jitWalkOSRFrame(walkState, currentFrame);
		numberOfFrames -= 1;
	} while (0 != numberOfFrames);
}
