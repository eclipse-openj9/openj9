/*******************************************************************************
 * Copyright (c) 2020, 2021 IBM Corp. and others
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

#include "jni.h"
#include "jcl.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jcl_internal.h"
#include "rommeth.h"
#include "omrlinkedlist.h"
#include "VMHelpers.hpp"

extern "C" {

#if JAVA_SPEC_VERSION >= 16
void JNICALL
Java_jdk_internal_misc_ScopedMemoryAccess_registerNatives(JNIEnv *env, jclass clazz)
{
}

typedef enum FrameCheckResult {
	FrameCheckErrorOpaqueFrame,
	FrameCheckErrorOutOfMemory,
	FrameCheckWaitSleepPark,
	FrameCheckErrorNoMoreFrames,
	FrameCheckNoError
} FrameCheckResult;

J9_DECLARE_CONSTANT_UTF8(waitMethod, "wait");
J9_DECLARE_CONSTANT_UTF8(sleepMethod, "sleep");
J9_DECLARE_CONSTANT_UTF8(parkMethod, "park");

static UDATA
frameCheckIterator(J9VMThread *currentThread, J9StackWalkState *walkState)
{
	// J9JavaVM *vm = currentThread->javaVM;
	J9Method *method = walkState->method;
	J9ROMMethod *romMethod = NULL;

	/* If there's no method, this must be a call-in frame - fail immediately */
	if (NULL == method) {
		*(FrameCheckResult *)walkState->userData1 = FrameCheckErrorOpaqueFrame;
		return J9_STACKWALK_STOP_ITERATING;
	}

	/* Check if thread is in wait/sleep/park (should not happen) */
	romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
	if (1 == walkState->framesWalked) {
		J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);
		if (J9UTF8_LENGTH(methodName) > 0) {
			if ((0 == memcmp(J9UTF8_DATA(methodName), J9UTF8_DATA(&waitMethod), J9UTF8_LENGTH(&waitMethod)))
				|| (0 == memcmp(J9UTF8_DATA(methodName), J9UTF8_DATA(&sleepMethod), J9UTF8_LENGTH(&sleepMethod)))
				|| (0 == memcmp(J9UTF8_DATA(methodName), J9UTF8_DATA(&parkMethod), J9UTF8_LENGTH(&parkMethod)))
			) {
				*(FrameCheckResult *)walkState->userData1 = FrameCheckWaitSleepPark;
				return J9_STACKWALK_STOP_ITERATING;
			}
		}
	}

	// Temporary (TODO: Async check after jni)
	if (romMethod->modifiers & J9AccNative) {
		*(FrameCheckResult *)walkState->userData1 = FrameCheckErrorOpaqueFrame;
		return J9_STACKWALK_STOP_ITERATING;
	}

	/* If the frame being dropped is <clinit>, disallow it */
	if (1 == walkState->framesWalked) {
		J9UTF8 *methodName = J9ROMMETHOD_NAME(romMethod);
		if ((romMethod->modifiers & J9AccStatic) && ('<' == J9UTF8_DATA(methodName)[0])) {
			*(FrameCheckResult *)walkState->userData1 = FrameCheckErrorOpaqueFrame;
			return J9_STACKWALK_STOP_ITERATING;
		}
	}

	if (NULL == walkState->jitInfo) {
		/* Interpreted frame, no need for decompilation */
	} else {
		/* Outer JIT frame */
		// jitAddDecompilationForFramePop hanging?
		// if (NULL == vm->jitConfig->jitAddDecompilationForFramePop(currentThread, walkState)) {
		// 	*(FrameCheckResult *)walkState->userData1 = FrameCheckErrorOutOfMemory;
		// 	return J9_STACKWALK_STOP_ITERATING;
		// }
		*(FrameCheckResult *)walkState->userData1 = FrameCheckErrorOpaqueFrame;
		return J9_STACKWALK_STOP_ITERATING;
	}

	*(FrameCheckResult *)walkState->userData1 = FrameCheckNoError;
	return J9_STACKWALK_STOP_ITERATING;
}

static void
exceptionThrowAsyncHandler(J9VMThread *currentThread, IDATA handlerKey, void *userData)
{
	VM_VMHelpers::setExceptionPending(currentThread, J9_JNI_UNWRAP_REFERENCE(userData));
}

static UDATA
closeScope0FrameWalkFunction(J9VMThread *vmThread, J9StackWalkState *walkState)
{
	if (JNI_TRUE == *(jboolean *)walkState->userData2) {
		/* scope has been found */
		return J9_STACKWALK_STOP_ITERATING;
	}
	return J9_STACKWALK_KEEP_ITERATING;
}

static void
closeScope0OSlotWalkFunction(J9VMThread *vmThread, J9StackWalkState *walkState, j9object_t *slot, const void *stackLocation)
{
	J9Method *method = walkState->method;
	if (NULL != method) {
		J9ROMMethod *romMethod = J9_ROM_METHOD_FROM_RAM_METHOD(method);
		if (NULL != romMethod && J9ROMMETHOD_HAS_EXTENDED_MODIFIERS(romMethod)) {
			U_32 extraModifiers = getExtendedModifiersDataFromROMMethod(romMethod);
			if (J9ROMMETHOD_HAS_SCOPED_ANNOTATION(extraModifiers)) {
				if (*slot == J9_JNI_UNWRAP_REFERENCE(walkState->userData1)) {
					*(jboolean *)walkState->userData2 = JNI_TRUE;
				}
			}
		}
	}
}

typedef struct LinkedThreads {
	J9VMThread *thread;
	struct LinkedThreads *next;
} LinkedThreads;

/**
 * For each thread, walk Java stack and look for the scope instance. Methods that can take and access a scope
 * instance are marked with the "@Scoped" extended modifier. If the scope instance is found in a method, that
 * method is accessing the memory segment associated with the scope and thus closing the scope will fail. The
 * scope accessing methods will be interrupted with a ScopedMemoryAccess.Scope.ScopedAccessError.
 */
jboolean JNICALL
Java_jdk_internal_misc_ScopedMemoryAccess_closeScope0(JNIEnv *env, jobject instance, jobject scope, jthrowable exception)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jboolean scopeNotFound = JNI_TRUE;
	LinkedThreads *current = NULL;
	LinkedThreads *head = NULL;
	PORT_ACCESS_FROM_VMC(currentThread);
	vmFuncs->internalEnterVMFromJNI(currentThread);
	vmFuncs->acquireExclusiveVMAccess(currentThread);

	if (NULL == scope) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		J9VMThread *walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
		while (NULL != walkThread) {
			jboolean threadHasScope = JNI_FALSE;
			J9StackWalkState walkState;
			walkState.walkThread = walkThread;
			walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_ITERATE_O_SLOTS;
			walkState.skipCount = 0;
			walkState.userData1 = (void *)scope;
			walkState.userData2 = (void *)&threadHasScope;
			walkState.frameWalkFunction = closeScope0FrameWalkFunction;
			walkState.objectSlotWalkFunction = closeScope0OSlotWalkFunction;

			vm->walkStackFrames(walkThread, &walkState);
			if (JNI_TRUE == *(jboolean *)walkState.userData2) {
				/* scope found, add to linked list */
				scopeNotFound = JNI_FALSE;
				if (NULL == head) {
					current = (LinkedThreads *)j9mem_allocate_memory(sizeof(LinkedThreads), J9MEM_CATEGORY_VM);
					if (NULL != current) {
						current->thread = walkThread;
						current->next = NULL;
						head = current;
					}
				} else {
					current->next = (LinkedThreads *)j9mem_allocate_memory(sizeof(LinkedThreads), J9MEM_CATEGORY_VM);
					if (NULL != current->next) {
						current->next->thread = walkThread;
						current->next->next = NULL;
						current = current->next;
					}
				}
			}

			walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
		}
	}

	current = head;
	while (NULL != current) {
		/* Scope is being accessed by a thread */
		J9VMThread *targetThread = current->thread;
		LinkedThreads *prev = NULL;
		static IDATA exceptionThrowHandlerKey = -1;
		J9StackWalkState walkState;
		FrameCheckResult result = FrameCheckErrorNoMoreFrames;

		if (exceptionThrowHandlerKey < 0) {
			exceptionThrowHandlerKey = vmFuncs->J9RegisterAsyncEvent(vm, exceptionThrowAsyncHandler, (void *)exception);
		}

		walkState.walkThread = targetThread;
		walkState.userData1 = (void *)&result;
		walkState.frameWalkFunction = frameCheckIterator;
		walkState.skipCount = 0;
		walkState.flags = J9_STACKWALK_INCLUDE_CALL_IN_FRAMES | J9_STACKWALK_INCLUDE_NATIVES | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_MAINTAIN_REGISTER_MAP;
		vm->walkStackFrames(currentThread, &walkState);
		switch (result) {
		case FrameCheckErrorOutOfMemory: /* fall through */
			vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGOUTOFMEMORYERROR, NULL);
		case FrameCheckErrorOpaqueFrame: /* fall through */
		case FrameCheckErrorNoMoreFrames:
			break;
		case FrameCheckWaitSleepPark:
			Assert_JCL_unreachable();
			break;
		case FrameCheckNoError:
			/* vm->jitConfig->jitDecompileMethodForFramePop? */
			vmFuncs->J9SignalAsyncEvent(vm, targetThread, exceptionThrowHandlerKey);
			break;
		}

		prev = current;
		current = current->next;
		j9mem_free_memory(prev);
	}

	vmFuncs->releaseExclusiveVMAccess(currentThread);
	vmFuncs->internalExitVMToJNI(currentThread);

	return scopeNotFound;
}
#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
