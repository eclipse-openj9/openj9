/*******************************************************************************
 * Copyright IBM Corp. and others 1991
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

#include <string.h>
#include "j9.h"
#include "j9port.h"
#include "rommeth.h"
#include "ut_j9vm.h"
#include "vm_internal.h"
#include "j9cp.h"
#include "j9consts.h"
#include "VMHelpers.hpp"
#include "AtomicSupport.hpp"

extern "C" {

#if JAVA_SPEC_VERSION >= 16
/**
 * Frame walk function, which is used with hasMemoryScope.
 *
 * @param[in] vmThread the J9VMThread
 * @param[in] walkState the stack walk state
 *
 * @return J9_STACKWALK_STOP_ITERATING to stop iterating, and
 *         J9_STACKWALK_KEEP_ITERATING to continue iterating
 */
static UDATA
closeScope0FrameWalkFunction(J9VMThread *vmThread, J9StackWalkState *walkState)
{
	if (*(bool *)walkState->userData2) {
		/* Scope has been found. */
		return J9_STACKWALK_STOP_ITERATING;
	}
	return J9_STACKWALK_KEEP_ITERATING;
}

/**
 * O-slot walk function, which is used with hasMemoryScope.
 *
 * @param[in] vmThread the J9VMThread
 * @param[in] walkState the stack walk state
 * @param[in] slot the O-slot pointer
 * @param[in] stackLocation the stack location
 */
static void
closeScope0OSlotWalkFunction(J9VMThread *vmThread, J9StackWalkState *walkState, j9object_t *slot, const void *stackLocation)
{
	J9Method *method = walkState->method;
	if (NULL != method) {
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		if (NULL != romMethod && J9ROMMETHOD_HAS_EXTENDED_MODIFIERS(romMethod)) {
			U_32 extraModifiers = getExtendedModifiersDataFromROMMethod(romMethod);
			if (J9ROMMETHOD_HAS_SCOPED_ANNOTATION(extraModifiers)) {
				if (*slot == walkState->userData1) {
					*(bool *)walkState->userData2 = true;
				}
			}
		}
	}
}

BOOLEAN
hasMemoryScope(J9VMThread *walkThread, j9object_t scope)
{
	bool scopeFound = false;

	if (NULL != scope) {
		J9StackWalkState walkState;

		walkState.walkThread = walkThread;
		walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_ITERATE_O_SLOTS;
		walkState.skipCount = 0;
		walkState.userData1 = (void *)scope;
		walkState.userData2 = (void *)&scopeFound;
		walkState.frameWalkFunction = closeScope0FrameWalkFunction;
		walkState.objectSlotWalkFunction = closeScope0OSlotWalkFunction;

		walkThread->javaVM->walkStackFrames(walkThread, &walkState);
	}

	return scopeFound;
}
#endif /* JAVA_SPEC_VERSION >= 16 */

void
clearAsyncEventFlags(J9VMThread *vmThread, UDATA flags)
{
	VM_AtomicSupport::bitAnd(&vmThread->asyncEventFlags, ~flags);
}

void
setAsyncEventFlags(J9VMThread *vmThread, UDATA flags, UDATA indicateEvent)
{
	VM_AtomicSupport::bitOr(&vmThread->asyncEventFlags, flags);
	if (indicateEvent) {
		VM_VMHelpers::indicateAsyncMessagePending(vmThread);
	}
}

UDATA
javaCheckAsyncMessages(J9VMThread *currentThread, UDATA throwExceptions)
{
	UDATA result = J9_CHECK_ASYNC_NO_ACTION;
	/* Indicate that all current asyncs have been seen */
	currentThread->stackOverflowMark = currentThread->stackOverflowMark2;
	/* Process the hookable async events */
	VM_AtomicSupport::readBarrier();
	UDATA asyncEventFlags = VM_AtomicSupport::set(&currentThread->asyncEventFlags, 0);
	if (0 != asyncEventFlags) {
		dispatchAsyncEvents(currentThread, asyncEventFlags);
	}
	/* Start the async check loop */
	omrthread_monitor_enter(currentThread->publicFlagsMutex);
	UDATA volatile *flagsPtr = (UDATA volatile*)&currentThread->publicFlags;
	for (;;) {
		UDATA const publicFlags = *flagsPtr;
		/* Check for a pop frames request */
		if (J9_ARE_ANY_BITS_SET(publicFlags, J9_PUBLIC_FLAGS_POP_FRAMES_INTERRUPT)) {
			/* Do not clear the drop flag yet (clear it once the drop is complete) */
			VM_VMHelpers::indicateAsyncMessagePending(currentThread);
			result = J9_CHECK_ASYNC_POP_FRAMES;
			break;
		}
#if JAVA_SPEC_VERSION >= 22
		/* Process pending close-scope requests queued on this thread. */
		J9CloseScopeListNode *head = currentThread->closeScopeList;
		if (NULL != head) {
			J9JavaVM *vm = currentThread->javaVM;
			j9object_t closeScopeObj = J9_JNI_UNWRAP_REFERENCE(head->closeScope);
			bool deleteNode = true;
			I_64 closeScopeCount = J9OBJECT_I64_LOAD(currentThread, closeScopeObj, vm->closeScopeCountOffset);

			Assert_VM_true(closeScopeCount > 0);

			/* Re-check whether this thread is still accessing the session in @Scoped code. */
			if (hasMemoryScope(currentThread, closeScopeObj)) {
				if (throwExceptions) {
					/* Deliver scoped exception for this close request. */
					currentThread->currentException = J9_JNI_UNWRAP_REFERENCE(head->scopeError);
					Assert_VM_true(NULL != currentThread->currentException);
					result = J9_CHECK_ASYNC_THROW_EXCEPTION;
				} else {
					/* Retry later when exceptions can be thrown. */
					VM_VMHelpers::indicateAsyncMessagePending(currentThread);
					deleteNode = false;
				}
			}

			if (deleteNode) {
				PORT_ACCESS_FROM_JAVAVM(vm);

				/* Decrement per-session close counter; last thread releases shared global refs. */
				MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);
				while (!objectAccessBarrier.inlineMixedObjectCompareAndSwapU64(
						currentThread,
						closeScopeObj,
						vm->closeScopeCountOffset,
						(U_64)closeScopeCount,
						((U_64)(closeScopeCount - 1)))
				) {
					/* Field updated by another thread, try again. */
					closeScopeCount = J9OBJECT_I64_LOAD(currentThread, closeScopeObj, vm->closeScopeCountOffset);
				}

				/* CAS succeeded. Update the local copy since CAS does not modify the local variable. */
				closeScopeCount -= 1;

				Assert_VM_true(closeScopeCount >= 0);

				/* Last thread to process this close deletes the shared global refs. */
				if (0 == closeScopeCount) {
					j9jni_deleteGlobalRef((JNIEnv *)currentThread, head->closeScope, JNI_FALSE);
					j9jni_deleteGlobalRef((JNIEnv *)currentThread, head->scopeError, JNI_FALSE);
				}

				/* Pop and free the processed node. */
				currentThread->closeScopeList = head->next;
				j9mem_free_memory(head);
			}
			break;
		}
#endif /* JAVA_SPEC_VERSION >= 22 */
		/* Check for a thread halt request */
		if (J9_ARE_ANY_BITS_SET(publicFlags, J9_PUBLIC_FLAGS_RELEASE_ACCESS_REQUIRED_MASK)) {
			Assert_VM_false(J9_ARE_ANY_BITS_SET(publicFlags, J9_PUBLIC_FLAGS_NOT_AT_SAFE_POINT));
			internalReleaseVMAccessNoMutex(currentThread);
			internalAcquireVMAccessNoMutex(currentThread);
			continue;
		}
		/* Check for stop request.  Do this last so that the currentException does not get a chance
		 * to be overwritten when access is released.
		 */
		if (J9_ARE_ANY_BITS_SET(publicFlags, J9_PUBLIC_FLAGS_STOP)) {
			if (throwExceptions) {
				currentThread->currentException = currentThread->stopThrowable;
				currentThread->stopThrowable = NULL;
				clearEventFlag(currentThread, J9_PUBLIC_FLAGS_STOP);
				omrthread_clear_priority_interrupted();
				result = J9_CHECK_ASYNC_THROW_EXCEPTION;
			} else {
				VM_VMHelpers::indicateAsyncMessagePending(currentThread);
			}
		}
		break;
	}
	omrthread_monitor_exit(currentThread->publicFlagsMutex);
	return result;
}

}
