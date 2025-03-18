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
#if JAVA_SPEC_VERSION >= 24
#include "thrtypes.h"
#endif /* JAVA_SPEC_VERSION >= 24 */
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
				vm->t1CacheHit += 1;
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
				vm->t2CacheHit += 1;
				break;
			}
			continuation = NULL;
		}
	}
#if defined(J9VM_PROF_CONTINUATION_ALLOCATION)
		vm->avgCacheLookupTime += (I_64)j9time_hires_delta(start, j9time_hires_clock(), OMRPORT_TIME_DELTA_IN_NANOSECONDS);
#endif /* defined(J9VM_PROF_CONTINUATION_ALLOCATION) */

	J9JavaStack *stack = NULL;
	J9SFJNINativeMethodFrame *frame = NULL;
	/* No cache found, allocate new continuation structure. */
	if (NULL == continuation) {
#if defined(J9VM_PROF_CONTINUATION_ALLOCATION)
		start = j9time_hires_clock();
#endif /* defined(J9VM_PROF_CONTINUATION_ALLOCATION) */
		continuation = (J9VMContinuation*)j9mem_allocate_memory(sizeof(J9VMContinuation), OMRMEM_CATEGORY_THREADS);
		if (NULL == continuation) {
			vm->internalVMFunctions->setNativeOutOfMemoryError(currentThread, 0, 0);
			result = FALSE;
			goto end;
		}

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
		vm->cacheMiss += 1;
	} else {
		/* Reset and reuse the stack in the recycled continuation. */
		stack = continuation->stackObject;
		stack->previous = NULL;
		stack->firstReferenceFrame = 0;
	}

	/* Reset all fields in the new or recycled continuation. */
	memset(continuation, 0, sizeof(J9VMContinuation));

#if JAVA_SPEC_VERSION >= 24
		continuation->nextWaitingContinuation = NULL;
		if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags3, J9_EXTENDED_RUNTIME3_YIELD_PINNED_CONTINUATION)) {
			continuation->monitorEnterRecordPool = pool_new(sizeof(J9MonitorEnterRecord), 0, 0, 0, J9_GET_CALLSITE(), OMRMEM_CATEGORY_VM, POOL_FOR_PORT(PORTLIB));
		}
#endif /* JAVA_SPEC_VERSION >= 24 */

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
		oldContinuationState = *continuationStatePtr;
		if (VM_ContinuationHelpers::isConcurrentlyScanned(oldContinuationState)) {
			omrthread_monitor_enter(currentThread->publicFlagsMutex);
			/* Wait till potentially concurrent GC scans are complete */
			for(;;) {
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
				} else {
					break;
				}
			}
			omrthread_monitor_exit(currentThread->publicFlagsMutex);
		}
		/* Remove pending state */
		Assert_VM_true(VM_ContinuationHelpers::isMountedWithCarrierThread(oldContinuationState, currentThread));
		Assert_VM_true(VM_ContinuationHelpers::isPendingToBeMounted(oldContinuationState));
		ContinuationState newContinuationState = oldContinuationState;
		VM_ContinuationHelpers::resetPendingState(&newContinuationState);
		returnContinuationState = VM_AtomicSupport::lockCompareExchange(continuationStatePtr, oldContinuationState, newContinuationState);
	} while (oldContinuationState != returnContinuationState);
	Assert_VM_false(VM_ContinuationHelpers::isPendingToBeMounted(*continuationStatePtr));
	Assert_VM_false(VM_ContinuationHelpers::isConcurrentlyScanned(*continuationStatePtr));

	return continuationObject;
}

BOOLEAN
enterContinuation(J9VMThread *currentThread, j9object_t continuationObject)
{
	J9JavaVM *vm = currentThread->javaVM;
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
		vm->memoryManagerFunctions->continuationObjectStarted(currentThread, continuationObject);

		continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, continuationObject);
#if JAVA_SPEC_VERSION >= 24
		continuation->vthread = currentThread->threadObject;
#endif /* JAVA_SPEC_VERSION >= 24 */
	}
	Assert_VM_notNull(continuation);

#if JAVA_SPEC_VERSION >= 24
	if (NULL != continuation->nextWaitingContinuation) {
		/* Continuation is still in a blocked list. This can happen with TIMED_WAIT.
		 * It must be removed from the waiting list.
		 */
		bool foundInBlockedContinuationList = false;
		bool foundInMonitorList = false;

		omrthread_monitor_enter(vm->blockedVirtualThreadsMutex);

		foundInBlockedContinuationList = VM_ContinuationHelpers::removeContinuationFromList(
				&vm->blockedContinuations, continuation);

		foundInMonitorList = VM_ContinuationHelpers::removeContinuationFromList(
				&continuation->objectWaitMonitor->waitingContinuations, continuation);

		omrthread_monitor_exit(vm->blockedVirtualThreadsMutex);

		Assert_VM_true(foundInMonitorList || foundInMonitorList);

		/* Virtual can only be in one list at a time. */
		Assert_VM_false(foundInBlockedContinuationList && foundInMonitorList);

		continuation->objectWaitMonitor = NULL;
	}
#endif /* JAVA_SPEC_VERSION >= 24 */

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
	currentThread->callOutCount = 0;

	if (started) {
#if JAVA_SPEC_VERSION >= 24
		if (J9_ARE_ANY_BITS_SET(currentThread->javaVM->extendedRuntimeFlags3, J9_EXTENDED_RUNTIME3_YIELD_PINNED_CONTINUATION)) {
			preparePinnedVirtualThreadForMount(currentThread, continuationObject, (J9VM_CONTINUATION_RETURN_FROM_OBJECT_WAIT == continuation->returnState));
		}
		/* InternalNative frame only build for non-jit calls. */
		if (J9VM_CONTINUATION_RETURN_FROM_JIT_MONITOR_ENTER != continuation->returnState) {
			VM_OutOfLineINL_Helpers::restoreInternalNativeStackFrame(currentThread);
		}
		result = FALSE;
#else /* JAVA_SPEC_VERSION >= 24 */
		/* resuming Continuation from yieldImpl */
		VM_OutOfLineINL_Helpers::restoreInternalNativeStackFrame(currentThread);
		VM_OutOfLineINL_Helpers::returnSingle(currentThread, JNI_TRUE, 1);
		result = FALSE;
#endif /* JAVA_SPEC_VERSION >= 24 */
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
yieldContinuation(J9VMThread *currentThread, BOOLEAN isFinished, UDATA returnState)
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
		/* Only set returnState if continuation is not finished. */
		continuation->returnState = returnState;
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

#if JAVA_SPEC_VERSION >= 24
		/* Remove reverse link to vthread object. */
		continuation->vthread = NULL;
		Assert_VM_true(NULL == continuation->nextWaitingContinuation);
#endif /* JAVA_SPEC_VERSION >= 24 */
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
	vm->totalContinuationStackSize += continuation->stackObject->size;

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
				vm->t2store += 1;
				break;
			}
		}

		if (!cached) {
			vm->cacheFree += 1;
			/* Caching failed, free the J9VMContinuation struct. */
			freeJavaStack(vm, continuation->stackObject);
#if JAVA_SPEC_VERSION >= 24
			if (J9_ARE_ANY_BITS_SET(vm->extendedRuntimeFlags3, J9_EXTENDED_RUNTIME3_YIELD_PINNED_CONTINUATION)) {
				pool_kill(continuation->monitorEnterRecordPool);
			}
#endif /* JAVA_SPEC_VERSION >= 24 */
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
	} else if (currentThread->callOutCount > 0) {
		/* TODO: This check should be changed from > 1 to > 0 once the call-ins are no
		 * longer used and the new design for single cInterpreter is implemented.
		 */
		result = J9VM_CONTINUATION_PINNED_REASON_NATIVE;
	} else if ((currentThread->ownedMonitorCount > 0)
#if JAVA_SPEC_VERSION >= 24
	&& J9_ARE_NO_BITS_SET(currentThread->javaVM->extendedRuntimeFlags3, J9_EXTENDED_RUNTIME3_YIELD_PINNED_CONTINUATION)
#endif /* JAVA_SPEC_VERSION >= 24 */
	) {
		result = J9VM_CONTINUATION_PINNED_REASON_MONITOR;
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

#if JAVA_SPEC_VERSION >= 24
	if (J9_ARE_ANY_BITS_SET(currentThread->javaVM->extendedRuntimeFlags3, J9_EXTENDED_RUNTIME3_YIELD_PINNED_CONTINUATION)) {
		vmThread->ownedMonitorCount = continuation->ownedMonitorCount;
		vmThread->monitorEnterRecords = continuation->monitorEnterRecords;
		vmThread->jniMonitorEnterRecords = continuation->jniMonitorEnterRecords;
	}
#endif /* JAVA_SPEC_VERSION >= 24 */
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

	/* -1 indicates vthread is in transition and doesn't allow any inspector.
	 * Count less than -1 indicates vthread was suspended in transition and every inspector acquired in this state
	 * will continue to decrement the count by 1 to keep track of total inspector access granted.
	 * Once the suspended vthread is resumed, no inspector should be allowed to acquire access before transition ends.
	 */
	if (vthreadInspectorCount < 0) {
		if ((vthreadInspectorCount < -1) && VM_VMHelpers::isThreadSuspended(currentThread, threadObj)) {
			if (!objectAccessBarrier.inlineMixedObjectCompareAndSwapU64(
					currentThread,
					threadObj,
					vm->virtualThreadInspectorCountOffset,
					(U_64)vthreadInspectorCount,
					((U_64)(vthreadInspectorCount - 1)))
			) {
				goto retry;
			} else {
				return TRUE;
			}
		}
		/* Thread is in transition, wait. */
		vmFuncs->internalReleaseVMAccess(currentThread);
		VM_AtomicSupport::yieldCPU();
		/* After wait, the thread may suspend here. */
		vmFuncs->internalAcquireVMAccess(currentThread);
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
	BOOLEAN inTransition = (NULL != VM_VMHelpers::getCarrierVMThread(currentThread, threadObj));

	/* If a thread is in transition, then access maybe allowed during this phase with negative inspector count. */
	if (inTransition) {
		Assert_VM_true(vthreadInspectorCount < -1);
		while (!objectAccessBarrier.inlineMixedObjectCompareAndSwapU64(
				currentThread,
				threadObj,
				vm->virtualThreadInspectorCountOffset,
				(U_64)vthreadInspectorCount,
				((U_64)(vthreadInspectorCount + 1)))
		) {
			/* Field updated by another thread, try again. */
			vthreadInspectorCount = J9OBJECT_I64_LOAD(currentThread, threadObj, vm->virtualThreadInspectorCountOffset);
		}
	} else {
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
}

void
enterVThreadTransitionCritical(J9VMThread *currentThread, jobject thread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);
	j9object_t threadObj = J9_JNI_UNWRAP_REFERENCE(thread);

retry:
	if (!VM_VMHelpers::isThreadSuspended(currentThread, threadObj)) {
		while (!objectAccessBarrier.inlineMixedObjectCompareAndSwapU64(currentThread, threadObj, vm->virtualThreadInspectorCountOffset, 0, ~(U_64)0)) {
			/* Thread is being inspected or unmounted, wait. */
			vmFuncs->internalReleaseVMAccess(currentThread);
			VM_AtomicSupport::yieldCPU();
			/* After wait, the thread may suspend here. */
			vmFuncs->internalAcquireVMAccess(currentThread);
			threadObj = J9_JNI_UNWRAP_REFERENCE(thread);
		}

		/* Now we have locked access to virtualThreadInspectorCount, check if the vthread is suspended.
		* If suspended, release the access and spin-wait until the vthread is resumed.
		* If not suspended, link the current J9VMThread with the virtual thread object.
		*/
		if (!VM_VMHelpers::isThreadSuspended(currentThread, threadObj)
		&& objectAccessBarrier.inlineMixedObjectCompareAndSwapU64(currentThread, threadObj, vm->internalSuspendStateOffset, J9_VIRTUALTHREAD_INTERNAL_STATE_NONE, (U_64)currentThread)
		) {
			return;
		}
		J9OBJECT_I64_STORE(currentThread, threadObj, vm->virtualThreadInspectorCountOffset, 0);
	}
	vmFuncs->internalReleaseVMAccess(currentThread);
	/* Spin is used instead of the halt flag as we cannot guarantee suspend flag is still set now.
	*
	* TODO: Dynamically increase the sleep time to a bounded maximum.
	*/
	j9thread_sleep(10);
	/* After wait, the thread may suspend here. */
	vmFuncs->internalAcquireVMAccess(currentThread);
	threadObj = J9_JNI_UNWRAP_REFERENCE(thread);
	goto retry;
}

void
exitVThreadTransitionCritical(J9VMThread *currentThread, jobject thread)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	j9object_t vthread = J9_JNI_UNWRAP_REFERENCE(thread);
	MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);

	/* Remove J9VMThread address from internalSuspendedState field, as the thread state is no longer in a transition. */
	while (!objectAccessBarrier.inlineMixedObjectCompareAndSwapU64(currentThread, vthread, vm->internalSuspendStateOffset, (U_64)currentThread, J9_VIRTUALTHREAD_INTERNAL_STATE_NONE)) {
		/* Wait if the suspend flag is set. */
		vmFuncs->internalReleaseVMAccess(currentThread);
		VM_AtomicSupport::yieldCPU();
		/* After wait, the thread may suspend here. */
		vmFuncs->internalAcquireVMAccess(currentThread);
		vthread = J9_JNI_UNWRAP_REFERENCE(thread);
	}

	/* Update to virtualThreadInspectorCount must be after clearing isSuspendedInternal field to retain sync ordering. */
	Assert_VM_true(-1 == J9OBJECT_I64_LOAD(currentThread, vthread, vm->virtualThreadInspectorCountOffset));
	J9OBJECT_I64_STORE(currentThread, vthread, vm->virtualThreadInspectorCountOffset, 0);
}

#if JAVA_SPEC_VERSION >= 24
J9ObjectMonitor *
detachMonitorInfo(J9VMThread *currentThread, j9object_t lockObject)
{
	J9ObjectMonitor *objectMonitor = NULL;
	j9objectmonitor_t lock = 0;

	if (!LN_HAS_LOCKWORD(currentThread, lockObject)) {
		objectMonitor = monitorTablePeek(currentThread->javaVM, lockObject);
		if (NULL != objectMonitor) {
			lock = J9_LOAD_LOCKWORD(currentThread, &objectMonitor->alternateLockword);
		} else {
			lock = 0;
		}
	} else {
		lock = J9OBJECT_MONITOR(currentThread, lockObject);
	}

	if (!J9_LOCK_IS_INFLATED(lock)) {
		objectMonitor = objectMonitorInflate(currentThread, lockObject, lock);
		if (NULL == objectMonitor) {
			return NULL;
		}
	} else {
		objectMonitor = J9_INFLLOCK_OBJECT_MONITOR(lock);
	}

	J9ThreadAbstractMonitor *monitor = (J9ThreadAbstractMonitor *)objectMonitor->monitor;
	monitor->owner = (J9Thread*)1;
	objectMonitor->ownerContinuation = currentThread->currentContinuation;

	return objectMonitor;
}

void
updateMonitorInfo(J9VMThread *currentThread, J9ObjectMonitor *objectMonitor)
{
	J9ThreadAbstractMonitor *monitor = (J9ThreadAbstractMonitor *)objectMonitor->monitor;
	monitor->owner = currentThread->osThread;
	objectMonitor->ownerContinuation = NULL;
}

UDATA
walkFrameMonitorEnterRecords(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	J9MonitorEnterRecord *monitorEnterRecords = (J9MonitorEnterRecord*)walkState->userData3;
	J9ObjectMonitor *objMonitorHead = (J9ObjectMonitor*)walkState->userData1;
	j9object_t targetSyncObject = (j9object_t)walkState->userData2;
	UDATA monitorCount = (UDATA)walkState->userData4;
	U_32 modifiers;
	UDATA *frameID;

	frameID = walkState->arg0EA;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (walkState->jitInfo != NULL) {
		frameID = walkState->unwindSP;
	}
#endif

	while (monitorEnterRecords &&
			(frameID == CONVERT_FROM_RELATIVE_STACK_OFFSET(walkState->walkThread, monitorEnterRecords->arg0EA))
		) {
		j9object_t obj = monitorEnterRecords->object;
		if (obj != targetSyncObject) {
			J9ObjectMonitor *mon = detachMonitorInfo(currentThread, obj);
			if (NULL == mon) {
				return J9_STACKWALK_RC_NO_MEMORY;
			}
			mon->next = objMonitorHead;
			objMonitorHead = mon;
			monitorCount++;
		}
		monitorEnterRecords = monitorEnterRecords->next;
	}

	/* If the current method is synchronized, add the syncObject to the array. */
	modifiers = J9_ROM_METHOD_FROM_RAM_METHOD(walkState->method)->modifiers;
	if (modifiers & J9AccSynchronized) {
		j9object_t syncObject;

		if ((modifiers & J9AccNative)
#ifdef J9VM_INTERP_NATIVE_SUPPORT
			|| (walkState->jitInfo != NULL)
#endif
		) {
			if (modifiers & J9AccStatic) {
				syncObject = J9VM_J9CLASS_TO_HEAPCLASS(walkState->constantPool->ramClass);
			} else {
				syncObject = *((j9object_t *) walkState->arg0EA);
			}
		} else {
			syncObject = (j9object_t) (walkState->bp[1]);
		}

		if (syncObject != targetSyncObject) {
			J9ObjectMonitor *mon = detachMonitorInfo(currentThread, syncObject);
			if (NULL == mon) {
				return J9_STACKWALK_RC_NO_MEMORY;
			}
			mon->next = objMonitorHead;
			objMonitorHead = mon;
			monitorCount++;
		}
	}

	walkState->userData1 = objMonitorHead;
	walkState->userData3 = monitorEnterRecords;
	walkState->userData4 = (void*)monitorCount;
	return J9_STACKWALK_KEEP_ITERATING;
}

UDATA
ownedMonitorsIterator(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	UDATA rc = J9_STACKWALK_KEEP_ITERATING;

	/* Take the J9JavaVM from the targetThread as currentThread may be null. */
	J9JavaVM* javaVM = walkState->walkThread->javaVM;
#ifdef J9VM_INTERP_NATIVE_SUPPORT
	if (walkState->jitInfo) {
		/* The jit walk may increment/decrement the stack depth */
		rc = javaVM->jitGetOwnedObjectMonitors(walkState);
	} else
#endif
	{
		/* The walk function may decrement the stack depth if a hidden frame is skipped */
		rc = walkFrameMonitorEnterRecords(currentThread, walkState);
	}

	return rc;
}

void
preparePinnedVirtualThreadForMount(J9VMThread *currentThread, j9object_t continuationObject, BOOLEAN isObjectWait)
{
	UDATA monitorCount = 0;

	if (currentThread->ownedMonitorCount > 0) {
		/* Update all owned monitors. */
		J9ObjectMonitor *head = currentThread->currentContinuation->enteredMonitors;
		while (NULL != head) {
			updateMonitorInfo(currentThread, head);
			monitorCount++;
			J9ObjectMonitor *next = head->next;
			head->next = NULL;
			head = next;
		}
		currentThread->currentContinuation->enteredMonitors = NULL;
	}

	/* Add the attached monitor to the carrier thread's lockedmonitorcount. */
	currentThread->osThread->lockedmonitorcount += monitorCount;
	if (J9VM_CONTINUATION_RETURN_FROM_YIELD != currentThread->currentContinuation->returnState) {
		exitVThreadTransitionCritical(currentThread, (jobject)&currentThread->threadObject);
	}
}

UDATA
preparePinnedVirtualThreadForUnmount(J9VMThread *currentThread, j9object_t syncObj, BOOLEAN isObjectWait)
{
	UDATA result = J9_OBJECT_MONITOR_YIELD_VIRTUAL;
	J9ObjectMonitor *syncObjectMonitor = NULL;
	J9ObjectMonitor *enteredMonitorsList = NULL;
	j9objectmonitor_t lock = 0;
	j9object_t continuationObj = NULL;
	UDATA monitorCount = 0;
	J9JavaVM *vm = currentThread->javaVM;

	if (NULL != syncObj) {
		enterVThreadTransitionCritical(currentThread, (jobject)&currentThread->threadObject);
	}
	if (currentThread->ownedMonitorCount > 0) {
		J9StackWalkState walkState;

		walkState.userData1 = NULL;
		walkState.userData2 = syncObj;

		walkState.userData3 = currentThread->monitorEnterRecords;
		walkState.userData4 = (void *)1;
		walkState.walkThread = currentThread;
		walkState.skipCount = 0;
		walkState.flags = J9_STACKWALK_VISIBLE_ONLY
			| J9_STACKWALK_SKIP_INLINES
			| J9_STACKWALK_ITERATE_FRAMES
			| J9_STACKWALK_NO_ERROR_REPORT
			| J9_STACKWALK_PREPARE_FOR_YIELD;

		walkState.frameWalkFunction = ownedMonitorsIterator;

		if (vm->walkStackFrames(currentThread, &walkState) != J9_STACKWALK_RC_NONE) {
			result = J9_OBJECT_MONITOR_OOM;
			goto done;
		}

		enteredMonitorsList = (J9ObjectMonitor*)walkState.userData1;
		monitorCount = (UDATA)walkState.userData4;

		/* Inflate all owned monitors. */
		/* Repeat for JNI monitor records. */
		J9MonitorEnterRecord *monitorRecords = currentThread->jniMonitorEnterRecords;
		while (NULL != monitorRecords) {
			j9object_t object = monitorRecords->object;

			if (syncObj != object) {
				J9ObjectMonitor *objectMonitor = detachMonitorInfo(currentThread, object);
				if (NULL == objectMonitor) {
					result = J9_OBJECT_MONITOR_OOM;
					goto done;
				}
				objectMonitor->next = enteredMonitorsList;
				enteredMonitorsList = objectMonitor;
				monitorCount++;
			}
			monitorRecords = monitorRecords->next;
		}
		currentThread->currentContinuation->enteredMonitors = enteredMonitorsList;
	}

	if (NULL != syncObj) {
		if (!LN_HAS_LOCKWORD(currentThread, syncObj)) {
			syncObjectMonitor = monitorTablePeek(vm, syncObj);
			if (NULL != syncObjectMonitor) {
				lock = J9_LOAD_LOCKWORD(currentThread, &syncObjectMonitor->alternateLockword);
			} else {
				lock = 0;
			}
		} else {
			lock = J9OBJECT_MONITOR(currentThread, syncObj);
		}

		if (!J9_LOCK_IS_INFLATED(lock)) {
			syncObjectMonitor = objectMonitorInflate(currentThread, syncObj, lock);
			if (NULL == syncObjectMonitor) {
				result = J9_OBJECT_MONITOR_OOM;
				goto done;
			}
		} else {
			syncObjectMonitor = J9_INFLLOCK_OBJECT_MONITOR(lock);
		}

		continuationObj = J9VMJAVALANGVIRTUALTHREAD_CONT(currentThread, currentThread->threadObject);
		J9VMJDKINTERNALVMCONTINUATION_SET_BLOCKER(currentThread, continuationObj, syncObj);

		if (isObjectWait) {
			J9VMContinuation *continuation = currentThread->currentContinuation;
			omrthread_monitor_t monitor = syncObjectMonitor->monitor;

			/* Record wait monitor state. */
			continuation->waitingMonitorEnterCount = monitor->count;

			/* Reset monitor entry count to 1.*/
			monitor->count = 1;
			/* Reset monitor state to pre-detach state so omrthread_monitor_exit behave correctly. */
			monitor->owner = currentThread->osThread;
			syncObjectMonitor->ownerContinuation = NULL;

			/* Add Continuation struct to the monitor's waiting list. */
			omrthread_monitor_exit(monitor);
			omrthread_monitor_enter(vm->blockedVirtualThreadsMutex);
			currentThread->currentContinuation->nextWaitingContinuation = syncObjectMonitor->waitingContinuations;
			syncObjectMonitor->waitingContinuations = currentThread->currentContinuation;
			currentThread->currentContinuation->objectWaitMonitor = syncObjectMonitor;
			omrthread_monitor_exit(vm->blockedVirtualThreadsMutex);
		} else {
			VM_AtomicSupport::addU32(&syncObjectMonitor->virtualThreadWaitCount, 1);
		}

		/* Clear the blocking object on the carrier thread. */
		J9VMTHREAD_SET_BLOCKINGENTEROBJECT(currentThread, currentThread, NULL);
	}

	/* Subtract the detached monitor from the carrier thread's lockedmonitorcount. */
	currentThread->osThread->lockedmonitorcount -= monitorCount;

done:
	if ((NULL != syncObj) && (J9_OBJECT_MONITOR_YIELD_VIRTUAL != result)) {
		exitVThreadTransitionCritical(currentThread, (jobject)&currentThread->threadObject);
	}
	return result;
}

jobject
takeVirtualThreadListToUnblock(J9VMThread *currentThread)
{
	j9object_t unblockedList = NULL;
	jobject result = NULL;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions const * const vmFuncs = vm->internalVMFunctions;

	if (J9_ARE_NO_BITS_SET(vm->extendedRuntimeFlags3, J9_EXTENDED_RUNTIME3_YIELD_PINNED_CONTINUATION)
	|| (NULL == vm->blockedContinuations)
	) {
		return NULL;
	}

	omrthread_monitor_enter(vm->blockedVirtualThreadsMutex);
	vmFuncs->internalEnterVMFromJNI(currentThread);
	while (NULL == unblockedList) {
		if (NULL != vm->blockedContinuations) {
restart:
			J9VMContinuation *previous = NULL;
			J9VMContinuation *current = vm->blockedContinuations;
			J9VMContinuation *next = NULL;
			while (NULL != current) {
				bool unblocked = false;
				U_32 state = J9VMJAVALANGVIRTUALTHREAD_STATE(currentThread, current->vthread);
				next = current->nextWaitingContinuation;
				/* Skip vthreads that are still in transition. */
				switch (state) {
				case JAVA_LANG_VIRTUALTHREAD_BLOCKING:
				case JAVA_LANG_VIRTUALTHREAD_WAITING:
				case JAVA_LANG_VIRTUALTHREAD_TIMED_WAITING:
					previous = current;
					current = next;
					continue;
				case JAVA_LANG_VIRTUALTHREAD_WAIT:
				case JAVA_LANG_VIRTUALTHREAD_TIMED_WAIT:
					J9VMJAVALANGVIRTUALTHREAD_SET_STATE(currentThread, current->vthread, JAVA_LANG_VIRTUALTHREAD_BLOCKED);
					/* FALLTHROUGH */
				default:
					break;
				}
				if (J9VMJAVALANGVIRTUALTHREAD_ONWAITINGLIST(currentThread, current->vthread)) {
					unblocked = true;
				} else {
					j9object_t continuationObj = J9VMJAVALANGVIRTUALTHREAD_CONT(currentThread, current->vthread);
					j9object_t syncObject = J9VMJDKINTERNALVMCONTINUATION_BLOCKER(currentThread, continuationObj);
					J9ObjectMonitor *syncObjectMonitor = NULL;
					j9objectmonitor_t lock = 0;
					if (!LN_HAS_LOCKWORD(currentThread, syncObject)) {
						syncObjectMonitor = vmFuncs->monitorTablePeek(vm, syncObject);
						if (NULL != syncObjectMonitor){
							lock = J9_LOAD_LOCKWORD_VM(vm, &syncObjectMonitor->alternateLockword);
						}
					} else {
						lock = J9OBJECT_MONITOR(currentThread, syncObject);
						if (J9_LOCK_IS_INFLATED(lock)) {
							syncObjectMonitor = J9_INFLLOCK_OBJECT_MONITOR(lock);
						}
					}
					/* Only perform the below operations for inflated monitors. */
					if (NULL != syncObjectMonitor) {
						omrthread_monitor_t monitor = syncObjectMonitor->monitor;
						if (0 == monitor->count) {
							unblocked = true;
							if (syncObjectMonitor->virtualThreadWaitCount >= 1) {
								VM_AtomicSupport::subtractU32(&syncObjectMonitor->virtualThreadWaitCount, 1);
							}
							J9VMJAVALANGVIRTUALTHREAD_SET_ONWAITINGLIST(currentThread, current->vthread, JNI_TRUE);
						}
					}
				}

				if (unblocked) {
					/* Add to Java unblock list. */
					J9VMJAVALANGVIRTUALTHREAD_SET_NEXT(currentThread, current->vthread, unblockedList);
					unblockedList = current->vthread;

					/* Remove from native blocking list. */
					current->nextWaitingContinuation = NULL;

					if (NULL == previous) {
						vm->blockedContinuations = next;
					} else {
						previous->nextWaitingContinuation = next;
					}
				} else {
					/* Keep in native blocking list. */
					previous = current;
				}

				current = next;
			}
			if (NULL == unblockedList) {
				vmFuncs->internalExitVMToJNI(currentThread);
				omrthread_monitor_wait(vm->blockedVirtualThreadsMutex);
				vmFuncs->internalEnterVMFromJNI(currentThread);
				goto restart;
			} else {
				result = vmFuncs->j9jni_createLocalRef((JNIEnv *)currentThread, unblockedList);
				break;
			}
		}
	}
	vmFuncs->internalExitVMToJNI(currentThread);
	omrthread_monitor_exit(vm->blockedVirtualThreadsMutex);

	return result;
}
#endif /* JAVA_SPEC_VERSION >= 24 */
} /* extern "C" */
