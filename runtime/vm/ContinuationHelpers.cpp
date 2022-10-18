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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
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

BOOLEAN
enterContinuation(J9VMThread *currentThread, j9object_t continuationObject)
{
	BOOLEAN result = TRUE;
	jboolean started = J9VMJDKINTERNALVMCONTINUATION_STARTED(currentThread, continuationObject);
	J9VMContinuation *continuation = J9VMJDKINTERNALVMCONTINUATION_VMREF(currentThread, continuationObject);

	Assert_VM_Null(currentThread->currentContinuation);

	VM_ContinuationHelpers::swapFieldsWithContinuation(currentThread, continuation);
	continuation->carrierThread = currentThread;
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

	VM_ContinuationHelpers::swapFieldsWithContinuation(currentThread, continuation);
	continuation->carrierThread = NULL;
	currentThread->currentContinuation = NULL;

	/* We need a full fence here to preserve happens-before relationship on PPC and other weakly
	 * ordered architectures since learning/reservation is turned on by default. Since we have the
	 * global pin lock counters we only need to need to address yield points, as thats the
	 * only time a different virtualThread can run on the underlying j9vmthread.
	 */
	VM_AtomicSupport::readWriteBarrier();

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
