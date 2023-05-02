/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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

#include "fastJNI.h"
#include "j9protos.h"
#include "j9vmnls.h"
#include "AtomicSupport.hpp"
#include "VMHelpers.hpp"
#include "ContinuationHelpers.hpp"

extern "C" {

void JNICALL
Fast_java_lang_VirtualThread_notifyJvmtiMountBegin(J9VMThread *currentThread, jobject thread, jboolean firstMount)
{
	VM_ContinuationHelpers::enterVThreadTransitionCritical(currentThread, thread);
}

void JNICALL
Fast_java_lang_VirtualThread_notifyJvmtiMountEnd(J9VMThread *currentThread, j9object_t threadObj, jboolean firstMount)
{
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

	/* Virtual thread is being mounted but it has been suspended. Thus,
	 * set J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND flag. At this
	 * point, virtual thread object is stored in targetThread->threadObject.
	 */
	if (0 != J9OBJECT_U32_LOAD(currentThread, threadObj, vm->isSuspendedInternalOffset)) {
		vmFuncs->setHaltFlag(currentThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
	}

	/* Allow thread to be inspected again. */
	VM_ContinuationHelpers::exitVThreadTransitionCritical(currentThread, threadObj);

	if (firstMount) {
		TRIGGER_J9HOOK_VM_VIRTUAL_THREAD_STARTED(vm->hookInterface, currentThread);
	}
	TRIGGER_J9HOOK_VM_VIRTUAL_THREAD_MOUNT(vm->hookInterface, currentThread);
}

void JNICALL
Fast_java_lang_VirtualThread_notifyJvmtiUnmountBegin(J9VMThread *currentThread, jobject thread, jboolean lastUnmount)
{
	J9JavaVM *vm = currentThread->javaVM;

	j9object_t threadObj = J9_JNI_UNWRAP_REFERENCE(thread);

	TRIGGER_J9HOOK_VM_VIRTUAL_THREAD_UNMOUNT(vm->hookInterface, currentThread);
	if (lastUnmount) {
		TRIGGER_J9HOOK_VM_VIRTUAL_THREAD_END(vm->hookInterface, currentThread);
	}

	VM_ContinuationHelpers::enterVThreadTransitionCritical(currentThread, thread);
	if (lastUnmount) {
		/* Re-fetch reference as enterVThreadTransitionCritical may release VMAccess. */
		threadObj = J9_JNI_UNWRAP_REFERENCE(thread);
		j9object_t continuationObj = J9VMJAVALANGVIRTUALTHREAD_CONT(currentThread, threadObj);
		/* Add reverse link from Continuation object to VirtualThread object, this let JVMTI code */
		J9VMJDKINTERNALVMCONTINUATION_SET_VTHREAD(currentThread, continuationObj, NULL);
	}
}

void JNICALL
Fast_java_lang_VirtualThread_notifyJvmtiUnmountEnd(J9VMThread *currentThread, j9object_t threadObj, jboolean lastUnmount)
{
	J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;

	if (lastUnmount) {
		vmFuncs->freeTLS(currentThread, threadObj);
	}

	j9object_t carrierThreadObject = currentThread->carrierThreadObject;
	/* The J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND will be set for the virtual
	 * thread's carrier thread if it was suspended while the virtual thread was mounted.
	 */
	if (0 != J9OBJECT_U32_LOAD(currentThread, carrierThreadObject, currentThread->javaVM->isSuspendedInternalOffset)) {
		currentThread->javaVM->internalVMFunctions->setHaltFlag(currentThread, J9_PUBLIC_FLAGS_HALT_THREAD_JAVA_SUSPEND);
	}

	/* Allow thread to be inspected again. */
	VM_ContinuationHelpers::exitVThreadTransitionCritical(currentThread, threadObj);

}

J9_FAST_JNI_METHOD_TABLE(java_lang_VirtualThread)
	J9_FAST_JNI_METHOD("notifyJvmtiMountBegin", "(Z)V", Fast_java_lang_VirtualThread_notifyJvmtiMountBegin,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NO_EXCEPTION_THROW)
	J9_FAST_JNI_METHOD("notifyJvmtiMountEnd", "(Z)V", Fast_java_lang_VirtualThread_notifyJvmtiMountEnd,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
	J9_FAST_JNI_METHOD("notifyJvmtiUnmountBegin", "(Z)V", Fast_java_lang_VirtualThread_notifyJvmtiUnmountBegin,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NO_EXCEPTION_THROW)
	J9_FAST_JNI_METHOD("notifyJvmtiUnmountEnd", "(Z)V", Fast_java_lang_VirtualThread_notifyJvmtiUnmountEnd,
		J9_FAST_JNI_RETAIN_VM_ACCESS | J9_FAST_JNI_NOT_GC_POINT | J9_FAST_JNI_NO_NATIVE_METHOD_FRAME | J9_FAST_JNI_NO_EXCEPTION_THROW |
		J9_FAST_JNI_NO_SPECIAL_TEAR_DOWN | J9_FAST_JNI_DO_NOT_WRAP_OBJECTS)
J9_FAST_JNI_METHOD_TABLE_END

}
