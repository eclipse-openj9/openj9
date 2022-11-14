/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#include "j9.h"
#include "j9comp.h"
#include "j9protos.h"
#include "j9vmnls.h"
#include "ut_j9vm.h"
#include "vm_api.h"
#include "AtomicSupport.hpp"
#include "ContinuationHelpers.hpp"
#include "OutOfLineINL.hpp"
#include "VMHelpers.hpp"


extern "C" {

BOOLEAN
createContinuation(J9VMThread *currentThread, j9object_t continuationObject)
{
	J9JavaVM *vm = currentThread->javaVM;
	PORT_ACCESS_FROM_VMC(currentThread);
	BOOLEAN result = TRUE;
	J9JavaStack *stack = NULL;
	J9SFJNINativeMethodFrame *frame = NULL;

	J9VMContinuation *continuation = (J9VMContinuation *)j9mem_allocate_memory(sizeof(J9VMContinuation), OMRMEM_CATEGORY_THREADS);
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

	J9VMJDKINTERNALVMCONTINUATION_SET_VMREF(currentThread, continuationObject, continuation);

	/* GC Hook to register Continuation object */
end:
	return result;
}

void
synchronizeWithConcurrentGCScan(J9VMThread *currentThread, J9VMContinuation *continuation)
{
	volatile uintptr_t *localAddr = &continuation->state;
	/* atomically 'or' (not 'set') continuation->state  with currentThread */
	uintptr_t oldContinuationState = VM_AtomicSupport::bitOr(localAddr, (uintptr_t)currentThread);

	Assert_VM_Null(VM_VMHelpers::getCarrierThreadFromContinuationState(oldContinuationState));

	if (VM_VMHelpers::isConcurrentlyScannedFromContinuationState(oldContinuationState)) {
		/* currentThread was low tagged (GC was already in progress), but by 'or'-ing our ID, we let GC know there is a pending mount */
		internalReleaseVMAccess(currentThread);

		omrthread_monitor_enter(currentThread->publicFlagsMutex);
		while (VM_VMHelpers::isConcurrentlyScannedFromContinuationState(*localAddr)) {
			/* GC is still concurrently scanning the continuation(currentThread was still low tagged), wait for GC thread to notify us when it's done. */
			omrthread_monitor_wait(currentThread->publicFlagsMutex);
		}
		omrthread_monitor_exit(currentThread->publicFlagsMutex);

		internalAcquireVMAccess(currentThread);
	}
}

BOOLEAN
enterContinuation(J9VMThread *currentThread, j9object_t continuationObject)
{
	BOOLEAN result = TRUE;
	jboolean started = J9VMJDKINTERNALVMCONTINUATION_STARTED(currentThread, continuationObject);
	J9VMContinuation *continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, continuationObject);
	Assert_VM_Null(currentThread->currentContinuation);
	Assert_VM_notNull(continuation);

	/* let GC know we are mounting, so they don't need to scan us, or if there is already ongoing scan wait till it's complete. */
	synchronizeWithConcurrentGCScan(currentThread, continuation);

	VM_ContinuationHelpers::swapFieldsWithContinuation(currentThread, continuation, started);

	currentThread->currentContinuation = continuation;

	/* Reset counters which determine if the current continuation is pinned. */
	currentThread->continuationPinCount = 0;
	currentThread->ownedMonitorCount = 0;
	currentThread->callOutCount = 0;

	if (started) {
		/* resuming Continuation from yieldImpl */
		VM_OutOfLineINL_Helpers::restoreInternalNativeStackFrame(currentThread);
		VM_OutOfLineINL_Helpers::returnSingle(currentThread, JNI_TRUE, 0);
		result = FALSE;
	} else {
		/* start new Continuation execution */
		J9VMJDKINTERNALVMCONTINUATION_SET_STARTED(currentThread, continuationObject, JNI_TRUE);
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
yieldContinuation(J9VMThread *currentThread)
{
	BOOLEAN result = TRUE;
	J9VMContinuation *continuation = currentThread->currentContinuation;
	Assert_VM_notNull(currentThread->currentContinuation);

	currentThread->currentContinuation = NULL;
	VM_ContinuationHelpers::swapFieldsWithContinuation(currentThread, continuation);

	/* We need a full fence here to preserve happens-before relationship on PPC and other weakly
	 * ordered architectures since learning/reservation is turned on by default. Since we have the
	 * global pin lock counters we only need to need to address yield points, as thats the
	 * only time a different virtualThread can run on the underlying j9vmthread.
	 */
	VM_AtomicSupport::readWriteBarrier();
	/* we don't need atomic here, since no GC thread should be able to start scanning while continuation is mounted,
	 * nor should another carrier thread be able to mount before we complete the unmount (hence no risk to overwrite anything in a race).
	 * Order
	 *
	 *	swap-stacks
	 *	writeBarrier
	 *	state initial
	 *
	 * must be maintained for weakly ordered CPUs, to unsure that once the continuation is again available for GC scan (on potentially remote CPUs), all CPUs see up-to-date stack .
	 */
	Assert_VM_true((uintptr_t)currentThread == continuation->state);
	continuation->state = J9_GC_CONTINUATION_STATE_INITIAL;

	return result;
}

void
freeContinuation(J9VMThread *currentThread, j9object_t continuationObject)
{
	J9VMContinuation *continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, continuationObject);
	if (NULL != continuation) {
		PORT_ACCESS_FROM_VMC(currentThread);
		/* Free all stack used by continuation */
		J9JavaStack *currentStack = continuation->stackObject;
		do {
			J9JavaStack *previous = currentStack->previous;

			freeJavaStack(currentThread->javaVM, currentStack);
			currentStack = previous;
		} while (NULL != currentStack);

		/* Free the J9VMContinuation struct */
		j9mem_free_memory(continuation);

		/* Update Continuation object's vmRef field */
		J9VMJDKINTERNALVMCONTINUATION_SET_VMREF(currentThread, continuationObject, NULL);
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
}

UDATA
walkContinuationStackFrames(J9VMThread *currentThread, J9VMContinuation *continuation, J9StackWalkState *walkState)
{
	Assert_VM_notNull(currentThread);

	UDATA rc = J9_STACKWALK_RC_NONE;

	if (NULL != continuation) {
		J9VMThread stackThread = {0};
		J9VMEntryLocalStorage els = {0};

		copyFieldsFromContinuation(currentThread, &stackThread, &els, continuation);

		walkState->walkThread = &stackThread;
		rc = currentThread->javaVM->walkStackFrames(currentThread, walkState);
	}

	return rc;
}
} /* extern "C" */
