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

static UDATA
closeScope0FrameWalkFunction(J9VMThread *vmThread, J9StackWalkState *walkState)
{
	if (JNI_FALSE == *(jboolean *)walkState->userData2) {
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
					*(jboolean *)walkState->userData2 = JNI_FALSE;
				}
			}
		}
	}
}

/**
 * For each thread, walk Java stack and look for the scope instance. Methods that can take and access a scope
 * instance are marked with the "@Scoped" extended modifier. If the scope instance is found in a method, that
 * method is accessing the memory segment associated with the scope and thus closing the scope will fail.
 */
jboolean JNICALL
#if JAVA_SPEC_VERSION >= 19
Java_jdk_internal_misc_ScopedMemoryAccess_closeScope0(JNIEnv *env, jobject instance, jobject scope)
#else /* JAVA_SPEC_VERSION >= 19 */
Java_jdk_internal_misc_ScopedMemoryAccess_closeScope0(JNIEnv *env, jobject instance, jobject scope, jobject exception)
#endif /* JAVA_SPEC_VERSION >= 19 */
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	const J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jboolean scopeNotFound = JNI_TRUE;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	if (NULL == scope) {
		vmFuncs->setCurrentExceptionUTF(currentThread, J9VMCONSTANTPOOL_JAVALANGNULLPOINTEREXCEPTION, NULL);
	} else {
		vmFuncs->acquireExclusiveVMAccess(currentThread);
		J9VMThread *walkThread = J9_LINKED_LIST_START_DO(vm->mainThread);
		while (NULL != walkThread) {
			if (VM_VMHelpers::threadCanRunJavaCode(walkThread)) {
				J9StackWalkState walkState;
				walkState.walkThread = walkThread;
				walkState.flags = J9_STACKWALK_ITERATE_FRAMES | J9_STACKWALK_ITERATE_O_SLOTS;
				walkState.skipCount = 0;
				walkState.userData1 = (void *)scope;
				walkState.userData2 = (void *)&scopeNotFound;
				walkState.frameWalkFunction = closeScope0FrameWalkFunction;
				walkState.objectSlotWalkFunction = closeScope0OSlotWalkFunction;

				vm->walkStackFrames(walkThread, &walkState);
				if (JNI_FALSE == *(jboolean *)walkState.userData2) {
					/* scope found */
					break;
				}
			}

			walkThread = J9_LINKED_LIST_NEXT_DO(vm->mainThread, walkThread);
		}
		vmFuncs->releaseExclusiveVMAccess(currentThread);
	}

	vmFuncs->internalExitVMToJNI(currentThread);
	return scopeNotFound;
}
#endif /* JAVA_SPEC_VERSION >= 16 */

} /* extern "C" */
