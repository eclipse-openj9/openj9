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
#include "java/com_ibm_cuda_CudaLinker.h"

#ifdef OMR_OPT_CUDA

/**
 * Add a module fragment to the set of linker inputs.
 *
 * Class:     com.ibm.cuda.CudaLinker
 * Method:    add
 * Signature: (IJI[BLjava/lang/String;J)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] linker    the linker state pointer
 * @param[in] type      the type of input in image
 * @param[in] image     the input content
 * @param[in] name      an optional name for use in log messages
 * @param[in] options   an optional options pointer
 */
void JNICALL
Java_com_ibm_cuda_CudaLinker_add
  (JNIEnv * env, jclass, jint deviceId, jlong linker, jint type, jbyteArray image, jstring name, jlong options)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_linkerAdd_entry(thread, deviceId, (J9CudaLinker)linker, type, image, name, (J9CudaJitOptions *)options);

	int32_t error = J9CUDA_ERROR_OPERATING_SYSTEM;
	jbyte * imageData = env->GetByteArrayElements(image, NULL);
	const char * nameData = NULL;

	if (NULL != name) {
		nameData = env->GetStringUTFChars(name, NULL);
	}

	if ((NULL == imageData) || ((NULL != name) && (NULL == nameData))) {
		Trc_cuda_linkerAdd_fail(thread);
	} else {
		PORT_ACCESS_FROM_ENV(env);

		error = j9cuda_linkerAddData(
				(uint32_t)deviceId,
				(J9CudaLinker)linker,
				(J9CudaJitInputType)type,
				imageData,
				(uintptr_t)env->GetArrayLength(image),
				nameData,
				(J9CudaJitOptions *)options);
	}

	if (NULL != nameData) {
		env->ReleaseStringUTFChars(name, nameData);
	}

	if (NULL != imageData) {
		env->ReleaseByteArrayElements(image, imageData, JNI_ABORT);
	}

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_linkerAdd_exit(thread);
}

/**
 * Complete linking the module.
 *
 * Class:     com.ibm.cuda.CudaLinker
 * Method:    complete
 * Signature: (IJ)[B
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] linker    the linker state pointer
 * @return the completed module ready for loading
 */
jbyteArray JNICALL
Java_com_ibm_cuda_CudaLinker_complete
  (JNIEnv * env, jclass, jint deviceId, jlong linker)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_linkerComplete_entry(thread, deviceId, (J9CudaLinker)linker);

	PORT_ACCESS_FROM_ENV(env);

	void * data = NULL;
	uintptr_t size = 0;
	int32_t error = j9cuda_linkerComplete((uint32_t)deviceId, (J9CudaLinker)linker, &data, &size);
	jbyteArray result = NULL;

	if (0 != error) {
		throwCudaException(env, error);
	} else {
		result = env->NewByteArray((jsize)size);

		if (env->ExceptionCheck()) {
			Trc_cuda_linkerComplete_exception(thread);
			result = NULL;
		} else if (NULL == result) {
			Trc_cuda_linkerComplete_fail(thread);
		} else {
			env->SetByteArrayRegion(result, 0, (jsize)size, (jbyte *)data);
		}
	}

	Trc_cuda_linkerComplete_exit(thread, result);

	return result;
}

#endif /* OMR_OPT_CUDA */

/**
 * Create a new linker session.
 *
 * Class:     com.ibm.cuda.CudaLinker
 * Method:    create
 * Signature: (IJ)J
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] options   an optional options pointer
 * @return the linker state pointer
 */
jlong JNICALL
Java_com_ibm_cuda_CudaLinker_create
  (JNIEnv * env, jclass, jint deviceId, jlong options)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_linkerCreate_entry(thread, deviceId, (J9CudaJitOptions *)options);

	J9CudaLinker linker = NULL;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_linkerCreate((uint32_t)deviceId, (J9CudaJitOptions *)options, &linker);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_linkerCreate_exit(thread, linker);

	return (jlong)linker;
}

#ifdef OMR_OPT_CUDA

/**
 * Destroy a linker session.
 *
 * Class:     com.ibm.cuda.CudaLinker
 * Method:    destroy
 * Signature: (IJ)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] linker    the linker state pointer
 */
void JNICALL
Java_com_ibm_cuda_CudaLinker_destroy
  (JNIEnv * env, jclass, jint deviceId, jlong linker)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_linkerDestroy_entry(thread, deviceId, (J9CudaLinker)linker);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_linkerDestroy((uint32_t)deviceId, (J9CudaLinker)linker);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_linkerDestroy_exit(thread);
}

#endif /* OMR_OPT_CUDA */
