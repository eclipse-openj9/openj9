/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
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
#include "j9.h"
#include "rommeth.h"
#include "jclprots.h"

static void**
extractArguments(JNIEnv *env, jintArray argSizes, jlongArray argValues) {
	PORT_ACCESS_FROM_VMC( ((J9VMThread *) env) );

#if !defined(J9VM_ENV_LITTLE_ENDIAN)
	jint  * sizes  = (*env)->GetIntArrayElements (env, argSizes, (jboolean *)0);
#endif
	jlong * values = (*env)->GetLongArrayElements(env, argValues, (jboolean *)0);

	if (
#if !defined(J9VM_ENV_LITTLE_ENDIAN)
		(NULL == sizes) ||
#endif
		(NULL == values)
	) {
		/* TODO throwCudaException(env, cudaErrorOperatingSystem); */
		return NULL;
	} else {
		jsize argCount	= (*env)->GetArrayLength(env, argSizes);
		void ** args	= (void **)j9mem_allocate_memory(argCount * sizeof(void**), J9MEM_CATEGORY_VM_JCL);
		jsize i = 0;

		if (NULL == args) {
			((J9VMThread *)env)->javaVM->internalVMFunctions->throwNativeOOMError(env, 0, 0);
			return NULL;
		}
		/* gather addresses of parameters */
		for (i = 0; i < argCount; ++i) {
			char * value = (char *)&values[i];

#if !defined(J9VM_ENV_LITTLE_ENDIAN)
			size_t size = (size_t)sizes[i];

			if (size != 0) {
				value += sizeof(jlong) - size;
			} else {
				value += sizeof(jlong) - sizeof(void *);
			}
#endif
			args[i] = value;
		}
		return args;
    }
}


jint JNICALL
Java_com_ibm_gpu_Kernel_launch(JNIEnv *env,
				jobject thisObject,  /* Kernel object */
				jobject invokeObject, /* object on which the method will be invoked */
				jobject mymethod, jint deviceId,
				jint gridDimX, jint gridDimY, jint gridDimZ,
				jint blockDimX, jint blockDimY, jint blockDimZ,
				jintArray argSizes, jlongArray argValues)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JavaVM *jvm = vmThread->javaVM;
	IDATA result = 1; /* launchGPU() returns 1 if it fails */

	if (NULL != jvm->jitConfig) {
		jmethodID methodID = 0;
		J9Method *ramMethod = NULL;
		void **args = NULL;

		PORT_ACCESS_FROM_JAVAVM(jvm);

		if (argSizes != NULL &&  argValues != NULL) {
			args = extractArguments(env, argSizes, argValues);
			if (NULL == args) {
				goto done;
			}
		}

		jvm->internalVMFunctions->internalEnterVMFromJNI(vmThread);
		methodID = jvm->reflectFunctions.reflectMethodToID(vmThread, mymethod);
		ramMethod = ((J9JNIMethodID *) methodID)->method;

		result = jvm->jitConfig->launchGPU(vmThread, invokeObject,
						ramMethod, deviceId,
						gridDimX, gridDimY, gridDimZ,
						blockDimX, blockDimY, blockDimZ,
						args);

		jvm->internalVMFunctions->internalExitVMToJNI(vmThread);

		if (NULL != args) {
			j9mem_free_memory(args);
		}
	}

done:
	return (jint) result;
}


void JNICALL
Java_java_util_stream_IntPipeline_promoteGPUCompile(JNIEnv *env, jclass clazz)
{
	J9VMThread *vmThread = (J9VMThread *)env;
	J9JavaVM *vm = vmThread->javaVM;

	if (NULL != vm->jitConfig) {
		J9InternalVMFunctions *vmFuncs = vm->internalVMFunctions;

		vmFuncs->internalEnterVMFromJNI(vmThread);
		vm->jitConfig->promoteGPUCompile(vmThread);
		vmFuncs->internalExitVMToJNI(vmThread);
	}
}

