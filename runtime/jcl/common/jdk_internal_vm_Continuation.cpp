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

#include "jni.h"
#include "jcl.h"
#include "jclprots.h"
#include "ut_j9jcl.h"

extern "C" {

/**
 * Increment the counter to pin a continuation.
 *
 * @param env instance of JNIEnv.
 * @param unused.
 * @return void.
 * @throws IllegalStateException for counter overflow.
 */
void JNICALL
Java_jdk_internal_vm_Continuation_pin(JNIEnv *env, jclass unused)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	if (UDATA_MAX == currentThread->continuationPinCount) {
		throwNewIllegalStateException(env, (char *)"Overflow detected");
	} else {
		currentThread->continuationPinCount += 1;
	}
}

/**
 * Decrement the counter to unpin a continuation.
 *
 * @param env instance of JNIEnv.
 * @param unused.
 * @return void.
 * @throws IllegalStateException for counter underflow.
 */
void JNICALL
Java_jdk_internal_vm_Continuation_unpin(JNIEnv *env, jclass unused)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	if (0 == currentThread->continuationPinCount) {
		throwNewIllegalStateException(env, (char *)"Underflow detected");
	} else {
		currentThread->continuationPinCount -= 1;
	}
}

jboolean JNICALL
Java_jdk_internal_vm_Continuation_createContinuationImpl(JNIEnv *env, jobject continuation)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9InternalVMFunctions *vmFuncs = currentThread->javaVM->internalVMFunctions;

	vmFuncs->internalEnterVMFromJNI(currentThread);

	j9object_t continuationObject = J9_JNI_UNWRAP_REFERENCE(continuation);
	BOOLEAN result = vmFuncs->createContinuation(currentThread, continuationObject);

	vmFuncs->internalExitVMToJNI(currentThread);

	return result ? JNI_TRUE : JNI_FALSE;
}

} /* extern "C" */
