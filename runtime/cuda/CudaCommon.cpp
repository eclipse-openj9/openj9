/*******************************************************************************
 * Copyright (c) 2013, 2016 IBM Corp. and others
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

#include "CudaCommon.hpp"
#include "java/com_ibm_cuda_Cuda.h"
#include "java/com_ibm_cuda_Cuda_Cleaner.h"

/**
 * Unless there is an outstanding exception, create and throw a CudaException.
 *
 * @param[in] env    the JNI interface pointer
 * @param[in] error  the CUDA error code to be included in the exception
 */
void
throwCudaException(JNIEnv * env, int32_t error)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_throwCudaException_entry(thread, error);

	J9JavaVM * javaVM = thread->javaVM;
	J9CudaGlobals * globals = javaVM->cudaGlobals;

	Assert_cuda_true(NULL != globals);
	Assert_cuda_true(NULL != globals->exception_init);

	if (env->ExceptionCheck()) {
		Trc_cuda_throwCudaException_suppressed(thread);
	} else {
		jobject exception = env->NewObject(globals->exceptionClass, globals->exception_init, error);

		if (NULL != exception) {
			env->Throw((jthrowable)exception);
		}
	}

	Trc_cuda_throwCudaException_exit(thread);
}

/**
 * Allocate a block of pinned host memory. Transfers between pinned memory and
 * devices avoids the intermediate copy via a pinned buffer local to the device
 * driver.
 *
 * Class:     com.ibm.cuda.Cuda
 * Method:    allocatePinnedBuffer
 * Signature: (J)J
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] byteCount  the number of bytes requested
 * @return a pointer to the pinned block of storage
 */
jlong JNICALL
Java_com_ibm_cuda_Cuda_allocatePinnedBuffer
  (JNIEnv * env, jclass, jlong byteCount)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_allocatePinnedBuffer_entry(thread, byteCount);

	void * address = NULL;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_hostAlloc((size_t)byteCount, J9CUDA_HOST_ALLOC_DEFAULT, &address);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_allocatePinnedBuffer_exit(thread, address);

	return (jlong)address;
}

/**
 * Return the number of devices visible to this process.
 *
 * Class:     com.ibm.cuda.Cuda
 * Method:    getDeviceCount
 * Signature: ()I
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @return the number of devices visible to this process
 */
jint JNICALL
Java_com_ibm_cuda_Cuda_getDeviceCount
  (JNIEnv * env, jclass)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_getDeviceCount_entry(thread);

	uint32_t count = 0;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	int32_t error = j9cuda_deviceGetCount(&count);

	if (0 != error) {
		throwCudaException(env, error);
	}
#endif /* OMR_OPT_CUDA */

	Trc_cuda_getDeviceCount_exit(thread, count);

	return (jint)count;
}

/**
 * Return the version number of the device driver.
 *
 * Class:     com.ibm.cuda.Cuda
 * Method:    getDriverVersion
 * Signature: ()I
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @return the driver version number
 */
jint JNICALL
Java_com_ibm_cuda_Cuda_getDriverVersion
  (JNIEnv * env, jclass)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_getDriverVersion_entry(thread);

	uint32_t version = 0;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_driverGetVersion(&version);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_getDriverVersion_exit(thread, version);

	return (jint)version;
}

/**
 * Return a string describing the given error code.
 *
 * Class:     com.ibm.cuda.Cuda
 * Method:    getErrorMessage
 * Signature: (I)Ljava/lang/String;
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] code      the error code
 * @return a string describing the error
 */
jstring JNICALL
Java_com_ibm_cuda_Cuda_getErrorMessage
  (JNIEnv * env, jclass, jint code)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_getErrorMessage_entry(thread, code);

	const char * message = NULL;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	message = j9cuda_getErrorString(code);
#else /* OMR_OPT_CUDA */
	/* Without CUDA support from OMR, these are the only error codes in use. */
	switch (code) {
	case J9CUDA_NO_ERROR:
		message = "no error";
		break;
	case J9CUDA_ERROR_MEMORY_ALLOCATION:
		message = "memory allocation failed";
		break;
	case J9CUDA_ERROR_NO_DEVICE:
		message = "no CUDA-capable device is detected";
		break;
	default:
		break;
	}
#endif /* OMR_OPT_CUDA */

	Trc_cuda_getErrorMessage_exit(thread, (NULL == message) ? "(null)" : message);

	return (NULL == message) ? NULL : env->NewStringUTF(message);
}

/**
 * Return the version number of the CUDA runtime.
 *
 * Class:     com.ibm.cuda.Cuda
 * Method:    getRuntimeVersion
 * Signature: ()I
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @return the runtime version number
 */
jint JNICALL
Java_com_ibm_cuda_Cuda_getRuntimeVersion
  (JNIEnv * env, jclass)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_getRuntimeVersion_entry(thread);

	uint32_t version = 0;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_runtimeGetVersion(&version);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_getRuntimeVersion_exit(thread, version);

	return (jint)version;
}

/**
 * Perform initialization required by native code.
 *
 * Class:     com.ibm.cuda.Cuda
 * Method:    initialize
 * Signature: (Ljava/lang/Class;Ljava/lang/reflect/Method;)I
 *
 * @param[in] env             the JNI interface pointer
 * @param[in] (unused)        the class pointer
 * @param[in] exceptionClass  the class pointer for CudaException
 * @param[in] runMethod       the method handle for Runnable.run()
 * @return a CUDA error code
 */
jint JNICALL
Java_com_ibm_cuda_Cuda_initialize
  (JNIEnv * env, jclass, jclass exceptionClass, jobject runMethod)
{
	jint            result  = 0;
	J9CudaGlobals * globals = NULL;
	J9VMThread    * thread  = (J9VMThread *)env;
	J9JavaVM      * javaVM  = thread->javaVM;

#if defined(UT_DIRECT_TRACE_REGISTRATION)
	UT_MODULE_LOADED(J9_UTINTERFACE_FROM_VM(javaVM));
#else
	UT_MODULE_LOADED(javaVM);
#endif

	Trc_cuda_initialize_entry(thread);

	Assert_cuda_true(NULL != exceptionClass);
	Assert_cuda_true(NULL != runMethod);

	PORT_ACCESS_FROM_JAVAVM(javaVM);

	globals = (J9CudaGlobals *)J9CUDA_ALLOCATE_MEMORY(sizeof(J9CudaGlobals));

	if (NULL == globals) {
		Trc_cuda_initialize_fail(thread, "allocate globals");
		result = J9CUDA_ERROR_MEMORY_ALLOCATION;
		goto done;
	}

	memset(globals, 0, sizeof(J9CudaGlobals));

	// Cache handles for classes and methods we use.

	globals->exceptionClass = (jclass)env->NewGlobalRef(exceptionClass);

	if (NULL == globals->exceptionClass) {
		Trc_cuda_initialize_fail(thread, "create NewGlobalRef for CudaException");
		result = J9CUDA_ERROR_MEMORY_ALLOCATION;
		goto error1;
	}

	globals->exception_init = env->GetMethodID(globals->exceptionClass, "<init>", "(I)V");

	if (NULL == globals->exception_init) {
		Trc_cuda_initialize_fail(thread, "find CudaException constructor");
		result = J9CUDA_ERROR_INITIALIZATION_ERROR;
		goto error2;
	}

	globals->runnable_run = env->FromReflectedMethod(runMethod);

	if (NULL == globals->runnable_run) {
		Trc_cuda_initialize_fail(thread, "get method handle");
		result = J9CUDA_ERROR_INITIALIZATION_ERROR;

error2:
		env->DeleteGlobalRef(globals->exceptionClass);

error1:
		J9CUDA_FREE_MEMORY(globals);
		globals = NULL;
	}

	javaVM->cudaGlobals = globals;

done:
	Trc_cuda_initialize_exit(thread, result);

	return result;
}

#ifdef OMR_OPT_CUDA

/**
 * Wrap the given native host buffer in a direct NIO byte buffer.
 *
 * Class:     com.ibm.cuda.Cuda
 * Method:    wrapDirectBuffer
 * Signature: (JJ)Ljava/nio/ByteBuffer;
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] buffer    the buffer pointer
 * @param[in] capacity  the buffer size in bytes
 * @return a direct byte buffer
 */
jobject JNICALL
Java_com_ibm_cuda_Cuda_wrapDirectBuffer
  (JNIEnv * env, jclass, jlong buffer, jlong capacity)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_wrapDirectBuffer_entry(thread, (uintptr_t)buffer, capacity);

	jobject wrapper = env->NewDirectByteBuffer((void *)(uintptr_t)buffer, capacity);

	Trc_cuda_wrapDirectBuffer_exit(thread, wrapper);

	return wrapper;
}

/**
 * Release a block of pinned host memory.
 *
 * Class:     com.ibm.cuda.Cuda.Cleaner
 * Method:    releasePinnedBuffer
 * Signature: (J)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] address   the buffer pointer
 */
void JNICALL
Java_com_ibm_cuda_Cuda_00024Cleaner_releasePinnedBuffer
  (JNIEnv * env, jclass, jlong address)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_releasePinnedBuffer_entry(thread, (uintptr_t)address);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_hostFree((void *)(uintptr_t)address);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_releasePinnedBuffer_exit(thread);
}

#endif /* OMR_OPT_CUDA */

/**
 * Called immediately before our shared library is unloaded: Release resources we allocated.
 */
void JNICALL
JNI_OnUnload(JavaVM * jvm, void *)
{
	J9JavaVM * javaVM = (J9JavaVM *)jvm;
	J9CudaGlobals * globals = javaVM->cudaGlobals;

	PORT_ACCESS_FROM_JAVAVM(javaVM);

	if (NULL != globals) {
		if (NULL != globals->exceptionClass) {
			JNIEnv * env = NULL;

			/*
			 * It's not serious if we can't get the JNIEnv - we're likely shutting down anyway.
			 */
			if (JNI_OK == jvm->GetEnv((void **)&env, JNI_VERSION_1_2)) {
				env->DeleteGlobalRef(globals->exceptionClass);
			}
		}

		J9CUDA_FREE_MEMORY(globals);
		javaVM->cudaGlobals = NULL;
	}

#if defined(UT_DIRECT_TRACE_REGISTRATION)
	UT_MODULE_UNLOADED(J9_UTINTERFACE_FROM_VM(javaVM));
#else
	UT_MODULE_UNLOADED(javaVM);
#endif
}
