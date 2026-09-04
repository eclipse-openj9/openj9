/*******************************************************************************
 * Copyright IBM Corp. and others 2024
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
#include "j9.h"
#include "j9vmconstantpool.h"
#include "ut_j9jcl.h"
#include "ObjectAccessBarrierAPI.hpp"

extern "C" {

J9_DECLARE_CONSTANT_UTF8(eventHandlerName, "eventHandler");
J9_DECLARE_CONSTANT_UTF8(eventHandlerSig, "Ljava/lang/Object;");

void JNICALL
Java_jdk_jfr_internal_JVM_setSampleThreads(JNIEnv *env, jobject obj, jboolean sampleThreads)
{
	// TODO: implementation
}

jboolean JNICALL
Java_jdk_jfr_internal_JVM_setHandler(JNIEnv *env, jobject obj, jclass eventClass, jobject handler)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jboolean rc = JNI_FALSE;

	Assert_JCL_notNull(eventClass);

	vmFuncs->internalEnterVMFromJNI(currentThread);
	J9Class *definingClass = NULL;
	void *eventHandlerAddr = vmFuncs->staticFieldAddress(currentThread,
															J9VMJAVALANGCLASS_VMREF(currentThread,
															J9_JNI_UNWRAP_REFERENCE(eventClass)),
															(U_8 *)J9UTF8_DATA(&eventHandlerName),
															J9UTF8_LENGTH(&eventHandlerName),
															(U_8 *)J9UTF8_DATA(&eventHandlerSig),
															J9UTF8_LENGTH(&eventHandlerSig),
															&definingClass,
															NULL,
															0,
															NULL);
	if (NULL != eventHandlerAddr) {
		MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);
		objectAccessBarrier.inlineStaticStoreObject(currentThread, definingClass, (j9object_t*)eventHandlerAddr, J9_JNI_UNWRAP_REFERENCE(handler), FALSE);
		rc = JNI_TRUE;
	}
	vmFuncs->internalExitVMToJNI(currentThread);

	return rc;
}

jobject JNICALL
Java_jdk_jfr_internal_JVM_getHandler(JNIEnv *env, jobject obj, jclass eventClass)
{
	J9VMThread *currentThread = (J9VMThread *)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	jobject handler = NULL;

	Assert_JCL_notNull(eventClass);

	vmFuncs->internalEnterVMFromJNI(currentThread);
	J9Class *definingClass = NULL;
	void *eventHandlerAddr = vmFuncs->staticFieldAddress(currentThread,
															J9VMJAVALANGCLASS_VMREF(currentThread,
															J9_JNI_UNWRAP_REFERENCE(eventClass)),
															(U_8 *)J9UTF8_DATA(&eventHandlerName),
															J9UTF8_LENGTH(&eventHandlerName),
															(U_8 *)J9UTF8_DATA(&eventHandlerSig),
															J9UTF8_LENGTH(&eventHandlerSig),
															&definingClass,
															NULL,
															0,
															NULL);
	if (NULL != eventHandlerAddr) {
		MM_ObjectAccessBarrierAPI objectAccessBarrier = MM_ObjectAccessBarrierAPI(currentThread);
		j9object_t eventHandler = objectAccessBarrier.inlineStaticReadObject(currentThread, definingClass, (j9object_t*)eventHandlerAddr, FALSE);
		handler = vmFuncs->j9jni_createLocalRef(env, eventHandler);
	}
	vmFuncs->internalExitVMToJNI(currentThread);

	return handler;
}

} /* extern "C" */
