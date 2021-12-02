/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

#include <string.h>
#include "jni.h"
#include "jcl.h"
#include "jclglob.h"
#include "jclprots.h"
#include "jcl_internal.h"
#include "util_api.h"

/* private static native byte[][] getVMArgsImpl(); */

jobjectArray JNICALL
Java_com_ibm_oti_vm_VM_getVMArgsImpl(JNIEnv *env, jobject recv)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	J9JavaVM *vm = vmThread->javaVM;
	JavaVMInitArgs *vmOptionsStruct = vm->vmArgsArray->actualVMArgs;
	jint originalSize = vmOptionsStruct->nOptions;
	jint resultSize = 0;
	JavaVMOption *options = vmOptionsStruct->options;
	jobjectArray result = NULL;
	jint i;
	jclass byteArrayClass;

	/* Count only options that begin with "-" */

	for (i = 0; i < originalSize; ++i) {
		if ('-' == options[i].optionString[0]) {
			resultSize += 1;
		}
	}

	/* Create the result array and fill it in, including only options that begin with "-" */

	byteArrayClass = (*env)->FindClass(env, "[B");
	if (NULL != byteArrayClass) {
		result = (*env)->NewObjectArray(env, resultSize, byteArrayClass, NULL);
		if (NULL != result) {
			jint writeIndex = 0;

			for (i = 0; i < originalSize; ++i) {
				char * optionString = options[i].optionString;

				if ('-' == optionString[0]) {
					jint optionLength = (jint) strlen(optionString);
					jbyteArray option = (*env)->NewByteArray(env, optionLength);

					if (NULL == option) {
						/* Don't use break here to avoid triggering the assertion below */
						return NULL;
					}
					(*env)->SetByteArrayRegion(env, option, 0, optionLength, (jbyte*)optionString);
					(*env)->SetObjectArrayElement(env, result, writeIndex, option);
					(*env)->DeleteLocalRef(env, option);
					writeIndex += 1;
				}
			}
			Assert_JCL_true(writeIndex == resultSize);
		}
	}

	return result;
}

/**
 * @return process ID of the caller.  This is upcast from UDATA.
 */
jlong JNICALL
Java_com_ibm_oti_vm_VM_getProcessId(JNIEnv *env, jclass clazz)
{

	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );
	jlong pid;
	pid =  (jlong) j9sysinfo_get_pid();
	return pid;
}

/**
 * @return numeric user ID of the caller. This is upcast from a UDATA.
 */
jlong JNICALL
Java_com_ibm_oti_vm_VM_getUid(JNIEnv *env, jclass clazz)
{

	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );
	jlong uid;
	uid =  (jlong) j9sysinfo_get_euid();
	return uid;
}

jobject JNICALL
Java_com_ibm_oti_vm_VM_getNonBootstrapClassLoader(JNIEnv *env, jclass jlClass)
{
	J9VMThread *currentThread = (J9VMThread*)env;
	J9JavaVM *vm = currentThread->javaVM;
	J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;
	J9ClassLoader *bootstrapLoader = vm->systemClassLoader;
	jobject result = NULL;
	J9StackWalkState walkState;

	vmFuncs->internalEnterVMFromJNI(currentThread);
	walkState.skipCount = 2; /* Skip this native and its caller */
	walkState.flags = J9_STACKWALK_CACHE_CPS | J9_STACKWALK_VISIBLE_ONLY | J9_STACKWALK_INCLUDE_NATIVES;
	walkState.walkThread = currentThread;
	if (J9_STACKWALK_RC_NONE != vm->walkStackFrames(currentThread, &walkState)) {
		vmFuncs->setNativeOutOfMemoryError(currentThread, 0, 0);
	} else {
		J9ConstantPool **cacheCursor = (J9ConstantPool**)walkState.cache;
		UDATA i = 0;
		for (i = 0; i < walkState.framesWalked; ++i, ++cacheCursor) {
			J9ClassLoader *classLoader = J9_CLASS_FROM_CP(*cacheCursor)->classLoader;
			if (classLoader != bootstrapLoader) {
				result = vmFuncs->j9jni_createLocalRef(env, classLoader->classLoaderObject);
				break;
			}
		}
		vmFuncs->freeStackWalkCaches(currentThread, &walkState);
	}
	vmFuncs->internalExitVMToJNI(currentThread);
	return result;
}


/**
 * Set the thread as a JVM System thread type
 * @return 0 if successful, -1 if failed
 */
jint JNICALL
Java_com_ibm_oti_vm_VM_markCurrentThreadAsSystemImpl(JNIEnv *env)
{
	J9VMThread *vmThread = (J9VMThread *) env;
	jint rc = 0;

	rc = (jint) omrthread_set_category(vmThread->osThread, J9THREAD_CATEGORY_SYSTEM_THREAD, J9THREAD_TYPE_SET_CREATE);

	return rc;
}
