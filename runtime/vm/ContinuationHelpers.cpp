/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
#include "j9comp.h"
#include "j9protos.h"
#include "j9vmnls.h"
#include "objhelp.h"
#include "ut_j9vm.h"
#include "vm_api.h"
#include "AtomicSupport.hpp"
#include "ContinuationHelpers.hpp"
#include "HeapIteratorAPI.h"
#include "OutOfLineINL.hpp"
#include "VMHelpers.hpp"


extern "C" {

BOOLEAN
createContinuation(J9VMThread *currentThread, j9object_t continuationObject)
{
	J9JavaVM *vm = currentThread->javaVM;
	PORT_ACCESS_FROM_VMC(currentThread);
	BOOLEAN result = TRUE;
	J9VMContinuation *continuation = NULL;
#if defined(J9VM_PROF_CONTINUATION_ALLOCATION)
	I_64 start = j9time_hires_clock();
#endif /* defined(J9VM_PROF_CONTINUATION_ALLOCATION) */

	/* First check if local cache is available. */
	if (NULL != currentThread->continuationT1Cache) {
		for (U_32 i = 0; i < vm->continuationT1Size; i++) {
			if (NULL != currentThread->continuationT1Cache[i]) {
				continuation = currentThread->continuationT1Cache[i];
				currentThread->continuationT1Cache[i] = NULL;
#if defined(J9VM_PROF_CONTINUATION_ALLOCATION)
				vm->t1CacheHit += 1;
#endif /* defined(J9VM_PROF_CONTINUATION_ALLOCATION) */
				break;
			}
		}
	}
	if (NULL == continuation) {
		/* Greedily try to use first available cache from global array. */
		for (U_32 i = 0; i < vm->continuationT2Size; i++) {
			continuation = vm->continuationT2Cache[i];
			if ((NULL != continuation)
			&& (continuation == (J9VMContinuation*)VM_AtomicSupport::lockCompareExchange(
																		(uintptr_t*)&(vm->continuationT2Cache[i]),
																		(uintptr_t)continuation,
																		(uintptr_t)NULL))
			) {
#if defined(J9VM_PROF_CONTINUATION_ALLOCATION)
				vm->t2CacheHit += 1;
#endif /* defined(J9VM_PROF_CONTINUATION_ALLOCATION) */
				break;
			}
			continuation = NULL;
		}
	}
#if defined(J9VM_PROF_CONTINUATION_ALLOCATION)
		vm->avgCacheLookupTime += (I_64)j9time_hires_delta(start, j9time_hires_clock(), OMRPORT_TIME_DELTA_IN_NANOSECONDS);
#endif /* defined(J9VM_PROF_CONTINUATION_ALLOCATION) */

	/* No cache found, allocate new continuation structure. */
	if (NULL == continuation) {
#if defined(J9VM_PROF_CONTINUATION_ALLOCATION)
		start = j9time_hires_clock();
#endif /* defined(J9VM_PROF_CONTINUATION_ALLOCATION) */
		J9JavaStack *stack = NULL;
		J9SFJNINativeMethodFrame *frame = NULL;
		continuation = (J9VMContinuation*)j9mem_allocate_memory(sizeof(J9VMContinuation), OMRMEM_CATEGORY_THREADS);
		if (NULL == continuation) {
			vm->internalVMFunctions->setNativeOutOfMemoryError(currentThread, 0, 0);
			result = FALSE;
			goto end;
		}

		memset(continuation, 0, sizeof(J9VMContinuation));

#ifdef J9VM_INTERP_GROWABLE_STACKS
#define VMTHR_INITIAL_STACK_SIZE ((vm->initialStackSize > (UDATA) vm->stackSize) ? vm->stackSize : vm->initialStackSize)
#else
#define VMTHR_INITIAL_STACK_SIZE vm->stackSize
#endif

		if ((stack = allocateJavaStack(vm, VMTHR_INITIAL_STACK_SIZE, NULL)) == NULL) {
			vm->internalVMFunctions->setNativeOutOfMemoryError(currentThread, 0, 0);
			j9mem_free_memory(continuation);
			result = FALSE;
			goto end;
		}

#undef VMTHR_INITIAL_STACK_SIZE

		continuation->stackObject = stack;
		continuation->stackOverflowMark2 = J9JAVASTACK_STACKOVERFLOWMARK(stack);
		continuation->stackOverflowMark = continuation->stackOverflowMark2;

		frame = ((J9SFJNINativeMethodFrame*)stack->end) - 1;
		frame->method = NULL;
		frame->specialFrameFlags = 0;
		frame->savedCP = NULL;
		frame->savedPC = (U_8*)(UDATA)J9SF_FRAME_TYPE_END_OF_STACK;
		frame->savedA0 = (UDATA*)(UDATA)J9SF_A0_INVISIBLE_TAG;
		continuation->sp = (UDATA*)frame;
		continuation->literals = NULL;
		continuation->pc = (U_8*)J9SF_FRAME_TYPE_JNI_NATIVE_METHOD;
		continuation->arg0EA = (UDATA*)&frame->savedA0;
		continuation->stackObject->isVirtual = TRUE;

#if defined(J9VM_PROF_CONTINUATION_ALLOCATION)
		I_64 totalTime = (I_64)j9time_hires_delta(start, j9time_hires_clock(), OMRPORT_TIME_DELTA_IN_NANOSECONDS);
		if (totalTime > 10000) {
			vm->slowAlloc += 1;
			vm->slowAllocAvgTime += totalTime;
		} else {
			vm->fastAlloc += 1;
			vm->fastAllocAvgTime += totalTime;
		}
#endif /* defined(J9VM_PROF_CONTINUATION_ALLOCATION) */
	}

	J9VMJDKINTERNALVMCONTINUATION_SET_VMREF(currentThread, continuationObject, continuation);

	/* GC Hook to register Continuation object. */
end:
	return result;
}

j9object_t
synchronizeWithConcurrentGCScan(J9VMThread *currentThread, j9object_t continuationObject, ContinuationState volatile *continuationStatePtr)
{
	ContinuationState oldContinuationState = 0;
	ContinuationState returnContinuationState = 0;
	/* Set pending state and carrier id, GC need to distinguish between fully mounted (don't scan) vs pending to be mounted (do scan) cases. */
	do {
		oldContinuationState = *continuationStatePtr;
		ContinuationState newContinuationState = oldContinuationState;
		VM_ContinuationHelpers::settingCarrierAndPendingState(&newContinuationState, currentThread);
		returnContinuationState = VM_AtomicSupport::lockCompareExchange(continuationStatePtr, oldContinuationState, newContinuationState);
	} while (returnContinuationState != oldContinuationState);
	Assert_VM_false(VM_ContinuationHelpers::isPendingToBeMounted(returnContinuationState));
	Assert_VM_Null(VM_ContinuationHelpers::getCarrierThread(returnContinuationState));

	/* These loops should iterate only a few times, at worst. We might have up to 2 overlapping concurrent GC,
	 * and each of them typically scans the object once. Some GCs (for example card dirtying based) may do rescans,
	 * but only up to small finite number of times.
	 */
	do {
		if (VM_ContinuationHelpers::isConcurrentlyScanned(returnContinuationState)) {
			omrthread_monitor_enter(currentThread->publicFlagsMutex);
			/* Wait till potentially concurrent GC scans are complete */
			do {
				oldContinuationState = *continuationStatePtr;
				if (VM_ContinuationHelpers::isConcurrentlyScanned(oldContinuationState)) {
					PUSH_OBJECT_IN_SPECIAL_FRAME(currentThread, continuationObject);
					internalReleaseVMAccess(currentThread);

					/* Wait for GC thread to notify us when it's done. */
					omrthread_monitor_wait(currentThread->publicFlagsMutex);

					internalAcquireVMAccess(currentThread);
					continuationObject = POP_OBJECT_IN_SPECIAL_FRAME(currentThread);
					/* Object might have moved - update its address and the address of the state slot. */
					continuationStatePtr = VM_ContinuationHelpers::getContinuationStateAddress(currentThread, continuationObject);
					continue;
				}
			} while (false);
			omrthread_monitor_exit(currentThread->publicFlagsMutex);
		}
		/* Remove pending state */
		oldContinuationState = *continuationStatePtr;
		Assert_VM_true(VM_ContinuationHelpers::isMountedWithCarrierThread(oldContinuationState, currentThread));
		Assert_VM_true(VM_ContinuationHelpers::isPendingToBeMounted(oldContinuationState));
		ContinuationState newContinuationState = oldContinuationState;
		VM_ContinuationHelpers::resetPendingState(&newContinuationState);
		returnContinuationState = VM_AtomicSupport::lockCompareExchange(continuationStatePtr, oldContinuationState, newContinuationState);
	} while (oldContinuationState != returnContinuationState);
	Assert_VM_false(VM_ContinuationHelpers::isPendingToBeMounted(*continuationStatePtr));

	return continuationObject;
}

BOOLEAN
enterContinuation(J9VMThread *currentThread, j9object_t continuationObject)
{
	BOOLEAN result = TRUE;
	J9VMContinuation *continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, continuationObject);
	ContinuationState volatile *continuationStatePtr = VM_ContinuationHelpers::getContinuationStateAddress(currentThread, continuationObject);
	bool started = VM_ContinuationHelpers::isStarted(*continuationStatePtr);
	Assert_VM_Null(currentThread->currentContinuation);

	if ((!started) && (NULL == continuation)) {
		result = createContinuation(currentThread, continuationObject);
		if (!result) {
			/* Directly return result if the create code failed, exception is already set. */
			return result;
		}
		currentThread->javaVM->memoryManagerFunctions->continuationObjectStarted(currentThread, continuationObject);

		continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, continuationObject);
	}
	Assert_VM_notNull(continuation);

	/* let GC know we are mounting, so they don't need to scan us, or if there is already ongoing scan wait till it's complete. */
	continuationObject = synchronizeWithConcurrentGCScan(currentThread, continuationObject, continuationStatePtr);

	/* defer preMountContinuation() after synchronizeWithConcurrentGCScan() to compensate potential missing concurrent scan
	 * between synchronizeWithConcurrentGCScan() to swapFieldsWithContinuation().
	 */
	if (started) {
		/* Notify GC of Continuation stack swap */
		currentThread->javaVM->memoryManagerFunctions->preMountContinuation(currentThread, continuationObject);
	}

	VM_ContinuationHelpers::swapFieldsWithContinuation(currentThread, continuation, continuationObject, started);

	currentThread->currentContinuation = continuation;
	/* Reset counters which determine if the current continuation is pinned. */
	currentThread->continuationPinCount = 0;
	currentThread->ownedMonitorCount = 0;
	currentThread->callOutCount = 0;

	if (started) {
		/* resuming Continuation from yieldImpl */
		VM_OutOfLineINL_Helpers::restoreInternalNativeStackFrame(currentThread);
		VM_OutOfLineINL_Helpers::returnSingle(currentThread, JNI_TRUE, 1);
		result = FALSE;
	} else {
		/* start new Continuation execution */
		VM_ContinuationHelpers::setStarted(continuationStatePtr);

		/* prepare callin frame, send method will be set by interpreter */
		J9SFJNICallInFrame *frame = ((J9SFJNICallInFrame*)currentThread->sp) - 1;

		frame->exitAddress = NULL;
		frame->specialFrameFlags = 0;
		frame->savedCP = currentThread->literals;
		frame->savedPC = currentThread->pc;
		frame->savedA0 = (UDATA*)((UDATA)currentThread->arg0EA | J9SF_A0_INVISIBLE_TAG);
		currentThread->sp = (UDATA*)frame;
		currentThread->pc = currentThread->javaVM->callInReturnPC;
		currentThread->literals = 0;
		currentThread->arg0EA = (UDATA*)&frame->savedA0;

		/* push argument to stack */
		*--currentThread->sp = (UDATA)continuationObject;
	}

	return result;
}

BOOLEAN
yieldContinuation(J9VMThread *currentThread, BOOLEAN isFinished)
{
	BOOLEAN result = TRUE;
	J9VMContinuation *continuation = currentThread->currentContinuation;
	j9object_t continuationObject = J9VMJAVALANGTHREAD_CONT(currentThread, currentThread->carrierThreadObject);
	ContinuationState volatile *continuationStatePtr = VM_ContinuationHelpers::getContinuationStateAddress(currentThread, continuationObject);
	Assert_VM_notNull(currentThread->currentContinuation);
	Assert_VM_false(VM_ContinuationHelpers::isPendingToBeMounted(*continuationStatePtr));

	if (isFinished) {
		VM_ContinuationHelpers::setFinished(continuationStatePtr);
	}

	currentThread->currentContinuation = NULL;
	VM_ContinuationHelpers::swapFieldsWithContinuation(currentThread, continuation, continuationObject);

	/* We need a full fence here to preserve happens-before relationship on PPC and other weakly
	 * ordered architectures since learning/reservation is turned on by default. Since we have the
	 * global pin lock counters we only need to need to address yield points, as thats the
	 * only time a different virtualThread can run on the underlying j9vmthread.
	 */
	VM_AtomicSupport::readWriteBarrier();
	/* We don't need atomic to set to state to unmounted (reset carrier ID), since no GC thread should be able to start scanning while continuation is mounted,
	 * nor should another carrier thread be able to mount before we complete the unmount (hence no risk to overwrite anything in a race).
	 * Order
	 *
	 * swap-stacks and set finished
	 * writeBarrier
	 * set unmounted (reset carrier ID)
	 *
	 * must be maintained for weakly ordered CPUs, to ensure that once the continuation is again available for GC scan or unavailable because it's finished,
	 * all remote CPUs see up-to-date stack and finished flag .
	 */
	Assert_VM_true(VM_ContinuationHelpers::isMountedWithCarrierThread(*continuationStatePtr, currentThread));
	VM_ContinuationHelpers::resetCarrierID(continuationStatePtr);
	/* Logically postUnmountContinuation(), which add the related continuation Object to the rememberedSet or dirty the Card for concurrent marking for future scanning, should be called
	 * before resetCarrierID(), but the scan might happened before resetCarrierID() if concurrent card clean happens, then the related compensating scan might be
	 * missed due to the continuation still is stated as mounted(we don't scan any mounted continuation, it should be scanned during root scanning via J9VMThread->currentContinuation).
	 * so calling postUnmountContinuation() after resetCarrierID() to avoid the missing scan case.
	 */
	if (isFinished) {
		/* Cleanup the native structure allocated */
		freeContinuation(currentThread, continuationObject, FALSE);
	} else {
		/* Notify GC of Continuation stack swap */
		currentThread->javaVM->memoryManagerFunctions->postUnmountContinuation(currentThread, continuationObject);
	}

	return result;
}

void
freeContinuation(J9VMThread *currentThread, j9object_t continuationObject, BOOLEAN skipLocalCache)
{
	J9VMContinuation *continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, continuationObject);
	if (NULL != continuation) {
		ContinuationState continuationState = *VM_ContinuationHelpers::getContinuationStateAddress(currentThread, continuationObject);
		Assert_VM_true(
					!VM_ContinuationHelpers::isConcurrentlyScanned(continuationState)
					&& (NULL == VM_ContinuationHelpers::getCarrierThread(continuationState)));

		/* Free old stack used by continuation. */
		J9JavaStack *currentStack = continuation->stackObject->previous;
		while (NULL != currentStack) {
			J9JavaStack *previous = currentStack->previous;

			freeJavaStack(currentThread->javaVM, currentStack);
			currentStack = previous;
		}

		/* Update Continuation object's vmRef field. */
		J9VMJDKINTERNALVMCONTINUATION_SET_VMREF(currentThread, continuationObject, NULL);
		J9VMJDKINTERNALVMCONTINUATION_SET_VTHREAD(currentThread, continuationObject, NULL);

		recycleContinuation(currentThread->javaVM, currentThread, continuation, skipLocalCache);
	}
}

void
recycleContinuation(J9JavaVM *vm, J9VMThread *vmThread, J9VMContinuation* continuation, BOOLEAN skipLocalCache)
{
	PORT_ACCESS_FROM_JAVAVM(vm);
	bool cached = false;
	/* Prepare continuationState for recycle, and reset stack initial state. */
	J9SFJNINativeMethodFrame *frame = ((J9SFJNINativeMethodFrame*)continuation->stackObject->end) - 1;
	frame->method = NULL;
	frame->specialFrameFlags = 0;
	frame->savedCP = NULL;
	frame->savedPC = (U_8*)(UDATA)J9SF_FRAME_TYPE_END_OF_STACK;
	frame->savedA0 = (UDATA*)(UDATA)J9SF_A0_INVISIBLE_TAG;
	continuation->sp = (UDATA*)frame;
	continuation->literals = NULL;
	continuation->pc = (U_8*)J9SF_FRAME_TYPE_JNI_NATIVE_METHOD;
	continuation->arg0EA = (UDATA*)&frame->savedA0;

	if (!skipLocalCache && (0 < vm->continuationT1Size)) {
		/* If called by carrier thread (not global), try to store in local cache first.
		 * Allocate cacheArray if it doesn't exist.
		 */
		if (NULL == vmThread->continuationT1Cache) {
			UDATA cacheSize = sizeof(J9VMContinuation*) * vm->continuationT1Size;
			vmThread->continuationT1Cache = (J9VMContinuation**)j9mem_allocate_memory(cacheSize, J9MEM_CATEGORY_VM);
			if (NULL == vmThread->continuationT1Cache) {
				vm->internalVMFunctions->setNativeOutOfMemoryError(vmThread, 0, 0);
				goto T2;
			}
			memset(vmThread->continuationT1Cache, 0, cacheSize);
		}
		for (U_32 i = 0; i < vm->continuationT1Size; i++) {
			if (NULL == vmThread->continuationT1Cache[i]) {
				vmThread->continuationT1Cache[i] = continuation;
				cached = true;
				break;
			}
		}
	}
T2:
	if (!cached) {
		/* Greedily try to cache continuation struct in global array. */
		for (U_32 i = 0; i < vm->continuationT2Size; i++) {
			if ((NULL == vm->continuationT2Cache[i])
			&& (NULL == (UDATA*)VM_AtomicSupport::lockCompareExchange(
													(uintptr_t*)&(vm->continuationT2Cache[i]),
													(uintptr_t)NULL,
													(uintptr_t)continuation))
			) {
				cached = true;
				break;
			}
		}

		if (!cached) {
			/* Caching failed, free the J9VMContinuation struct. */
			freeJavaStack(vm, continuation->stackObject);
			j9mem_free_memory(continuation);
		}
	}
}

jint
isPinnedContinuation(J9VMThread *currentThread)
{
	jint result = 0;

	if (currentThread->continuationPinCount > 0) {
		result = J9VM_CONTINUATION_PINNED_REASON_CRITICAL_SECTION;
	} else if (currentThread->ownedMonitorCount > 0) {
		result = J9VM_CONTINUATION_PINNED_REASON_MONITOR;
	} else if (currentThread->callOutCount > 0) {
		/* TODO: This check should be changed from > 1 to > 0 once the call-ins are no
		 * longer used and the new design for single cInterpreter is implemented.
		 */
		result = J9VM_CONTINUATION_PINNED_REASON_NATIVE;
	} else {
		/* Do nothing. */
	}

	return result;
}

void
copyFieldsFromContinuation(J9VMThread *currentThread, J9VMThread *vmThread, J9VMEntryLocalStorage *els, J9VMContinuation *continuation)
{
	vmThread->javaVM = currentThread->javaVM;
	vmThread->arg0EA = continuation->arg0EA;
	vmThread->bytecodes = continuation->bytecodes;
	vmThread->sp = continuation->sp;
	vmThread->pc = continuation->pc;
	vmThread->literals = continuation->literals;
	vmThread->stackOverflowMark = continuation->stackOverflowMark;
	vmThread->stackOverflowMark2 = continuation->stackOverflowMark2;
	vmThread->stackObject = continuation->stackObject;
	vmThread->j2iFrame = continuation->j2iFrame;
	vmThread->decompilationStack = continuation->decompilationStack;
	els->oldEntryLocalStorage = continuation->oldEntryLocalStorage;
	els->jitGlobalStorageBase = (UDATA*)&continuation->jitGPRs;
	els->i2jState = continuation->i2jState;
	vmThread->entryLocalStorage = els;

	/* Disable the JIT artifact cache on the walk thread.  It provides little performance
	 * benefit to a single walk and the cache memory must be managed.
	 */
	vmThread->jitArtifactSearchCache = (void*)((UDATA)vmThread->jitArtifactSearchCache | J9_STACKWALK_NO_JIT_CACHE);
}

UDATA
walkContinuationStackFrames(J9VMThread *currentThread, J9VMContinuation *continuation, j9object_t threadObject, J9StackWalkState *walkState)
{
	Assert_VM_notNull(currentThread);

	UDATA rc = J9_STACKWALK_RC_NONE;

	if (NULL != continuation) {
		J9VMThread stackThread = {0};
		J9VMEntryLocalStorage els = {0};

		copyFieldsFromContinuation(currentThread, &stackThread, &els, continuation);
		stackThread.threadObject = threadObject;
		walkState->walkThread = &stackThread;
		rc = currentThread->javaVM->walkStackFrames(currentThread, walkState);
	}

	return rc;
}


jvmtiIterationControl
walkContinuationCallBack(J9VMThread *vmThread, J9MM_IterateObjectDescriptor *object, void *userData)
{
	j9object_t continuationObj = object->object;
	J9VMContinuation *continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(vmThread, continuationObj);
	if (NULL != continuation) {
		J9StackWalkState localWalkState = *(J9StackWalkState*)userData;
		/* Walk non-null continuation's stack */
		j9object_t threadObject = VM_ContinuationHelpers::getThreadObjectForContinuation(vmThread, continuation, continuationObj);
		walkContinuationStackFrames(vmThread, continuation, threadObject, &localWalkState);
	}
	return JVMTI_ITERATION_CONTINUE;
}

UDATA
walkAllStackFrames(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9StackWalkState localWalkState = {0};
	UDATA rc = J9_STACKWALK_RC_NONE;

	Assert_VM_true((J9_XACCESS_EXCLUSIVE == vm->exclusiveAccessState) || (J9_XACCESS_EXCLUSIVE == vm->safePointState));

	/* Walk all vmThread stacks */
	J9VMThread *targetThread = vm->mainThread;
	do {
		/* Reset localWalkState */
		localWalkState = *walkState;
		localWalkState.walkThread = targetThread;
		rc = vm->walkStackFrames(currentThread, &localWalkState);

		targetThread = targetThread->linkNext;
	} while (targetThread != vm->mainThread);

	/* for non realtime GC case j9mm_iterate_all_continuation_objects has been done by pallells GC threads during GC clearable phase */
	/* Walk all live continuation stacks using the GC Continuation object iterator */
/*	PORT_ACCESS_FROM_VMC(currentThread);
	vm->memoryManagerFunctions->j9gc_flush_nonAllocationCaches_for_walk(vm);
	vm->memoryManagerFunctions->j9mm_iterate_all_continuation_objects(
									currentThread,
									PORTLIB,
									0,
									walkContinuationCallBack,
									(void*)walkState);
*/
	return rc;
}

BOOLEAN
acquireVThreadInspector(J9VMThread *currentThread, jobject thread, BOOLEAN spin)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);
	j9object_t threadObj = NULL;
	I_64 vthreadInspectorCount = 0;
retry:
	/* Consistently re-fetch threadObj for all the cases below. */
	threadObj = J9_JNI_UNWRAP_REFERENCE(thread);
	vthreadInspectorCount = J9OBJECT_I64_LOAD(currentThread, threadObj, vm->virtualThreadInspectorCountOffset);
	if (vthreadInspectorCount < 0) {
		/* Thread is in transition, wait. */
		vmFuncs->internalExitVMToJNI(currentThread);
		VM_AtomicSupport::yieldCPU();
		/* After wait, the thread may suspend here. */
		vmFuncs->internalEnterVMFromJNI(currentThread);
		if (spin) {
			goto retry;
		} else {
			return FALSE;
		}
	} else if (!objectAccessBarrier.inlineMixedObjectCompareAndSwapU64(
															currentThread,
															threadObj,
															vm->virtualThreadInspectorCountOffset,
															(U_64)vthreadInspectorCount,
															((U_64)(vthreadInspectorCount + 1)))
	) {
		/* Field updated by another thread, try again. */
		if (spin) {
			goto retry;
		} else {
			return FALSE;
		}
	}
	return TRUE;
}

void
releaseVThreadInspector(J9VMThread *currentThread, jobject thread)
{
	J9JavaVM *vm = currentThread->javaVM;
	MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);
	j9object_t threadObj = J9_JNI_UNWRAP_REFERENCE(thread);
	I_64 vthreadInspectorCount = J9OBJECT_I64_LOAD(currentThread, threadObj, vm->virtualThreadInspectorCountOffset);
	/* vthreadInspectorCount must be greater than 0 before decrement. */
	Assert_VM_true(vthreadInspectorCount > 0);
	while (!objectAccessBarrier.inlineMixedObjectCompareAndSwapU64(
														currentThread,
														threadObj,
														vm->virtualThreadInspectorCountOffset,
														(U_64)vthreadInspectorCount,
														((U_64)(vthreadInspectorCount - 1)))
	) {
		/* Field updated by another thread, try again. */
		vthreadInspectorCount = J9OBJECT_I64_LOAD(currentThread, threadObj, vm->virtualThreadInspectorCountOffset);
	}
}
} /* extern "C" */
