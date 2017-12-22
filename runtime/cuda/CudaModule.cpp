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
#include "java/com_ibm_cuda_CudaModule.h"

#ifdef OMR_OPT_CUDA

/**
 * Find a kernel function in a module.
 *
 * Class:     com.ibm.cuda.CudaModule
 * Method:    getFunction
 * Signature: (IJLjava/lang/String;)J
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] module    the module pointer
 * @param[in] name      the requested function name
 * @return the function pointer
 */
jlong JNICALL
Java_com_ibm_cuda_CudaModule_getFunction
  (JNIEnv * env, jclass, jint deviceId, jlong module, jstring name)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_moduleGetFunction_entry(thread, deviceId, (J9CudaModule)module, name);

	const char * cName = env->GetStringUTFChars(name, NULL);
	int32_t error = J9CUDA_ERROR_OPERATING_SYSTEM;
	J9CudaFunction function = 0;

	if (NULL == cName) {
		Trc_cuda_module_getStringFail(thread);
	} else {
		PORT_ACCESS_FROM_ENV(env);

		error = j9cuda_moduleGetFunction(
				(uint32_t)deviceId,
				(J9CudaModule)module,
				cName,
				&function);

		env->ReleaseStringUTFChars(name, cName);
	}

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_moduleGetFunction_exit(thread, function);

	return (jlong)function;
}

/**
 * Find a global symbol in a module.
 *
 * Class:     com.ibm.cuda.CudaModule
 * Method:    getGlobal
 * Signature: (IJLjava/lang/String;)J
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] module    the module pointer
 * @param[in] name      the requested global symbol name
 * @return the global pointer
 */
jlong JNICALL
Java_com_ibm_cuda_CudaModule_getGlobal
  (JNIEnv * env, jclass, jint deviceId, jlong module, jstring name)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_moduleGetGlobal_entry(thread, deviceId, (J9CudaModule)module, name);

	const char * cName = env->GetStringUTFChars(name, NULL);
	int32_t error = J9CUDA_ERROR_OPERATING_SYSTEM;
	uintptr_t size = 0;
	uintptr_t symbol = 0;

	if (NULL == cName) {
		Trc_cuda_module_getStringFail(thread);
	} else {
		PORT_ACCESS_FROM_ENV(env);

		error = j9cuda_moduleGetGlobal(
				(uint32_t)deviceId,
				(J9CudaModule)module,
				cName,
				&symbol,
				&size);

		env->ReleaseStringUTFChars(name, cName);
	}

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_moduleGetGlobal_exit(thread, (uintptr_t)symbol);

	return (jlong)symbol;
}

/**
 * Find a surface in a module.
 *
 * Class:     com.ibm.cuda.CudaModule
 * Method:    getSurface
 * Signature: (IJLjava/lang/String;)J
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] module    the module pointer
 * @param[in] name      the requested surface name
 * @return the surface pointer
 */
jlong JNICALL
Java_com_ibm_cuda_CudaModule_getSurface
  (JNIEnv * env, jclass, jint deviceId, jlong module, jstring name)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_moduleGetSurface_entry(thread, deviceId, (J9CudaModule)module, name);

	const char * cName = env->GetStringUTFChars(name, NULL);
	int32_t error = J9CUDA_ERROR_OPERATING_SYSTEM;
	uintptr_t surface = 0;

	if (NULL == cName) {
		Trc_cuda_module_getStringFail(thread);
	} else {
		PORT_ACCESS_FROM_ENV(env);

		error = j9cuda_moduleGetSurfaceRef(
				(uint32_t)deviceId,
				(J9CudaModule)module,
				cName,
				&surface);

		env->ReleaseStringUTFChars(name, cName);
	}

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_moduleGetSurface_exit(thread, (uintptr_t)surface);

	return (jlong)surface;
}

/**
 * Find a texture in a module.
 *
 * Class:     com.ibm.cuda.CudaModule
 * Method:    getTexture
 * Signature: (IJLjava/lang/String;)J
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] module    the module pointer
 * @param[in] name      the requested texture name
 * @return the texture pointer
 */
jlong JNICALL
Java_com_ibm_cuda_CudaModule_getTexture
  (JNIEnv * env, jclass, jint deviceId, jlong module, jstring name)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_moduleGetTexture_entry(thread, deviceId, (J9CudaModule)module, name);

	const char * cName = env->GetStringUTFChars(name, NULL);
	int32_t error = J9CUDA_ERROR_OPERATING_SYSTEM;
	uintptr_t texture = 0;

	if (NULL == cName) {
		Trc_cuda_module_getStringFail(thread);
	} else {
		PORT_ACCESS_FROM_ENV(env);

		error = j9cuda_moduleGetTextureRef(
				(uint32_t)deviceId,
				(J9CudaModule)module,
				cName,
				&texture);

		env->ReleaseStringUTFChars(name, cName);
	}

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_moduleGetTexture_exit(thread, (uintptr_t)texture);

	return (jlong)texture;
}

#endif /* OMR_OPT_CUDA */

/**
 * Load a module onto a device.
 *
 * Class:     com.ibm.cuda.CudaModule
 * Method:    load
 * Signature: (I[BJ)J
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] image     the module image
 * @param[in] options   Fan optional options pointer
 * @return the module pointer
 */
jlong JNICALL
Java_com_ibm_cuda_CudaModule_load
  (JNIEnv * env, jclass, jint deviceId, jbyteArray image, jlong options)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_moduleLoad_entry(thread, deviceId, image, (J9CudaJitOptions *)options);

	J9CudaModule module = NULL;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	jbyte * imageData = env->GetByteArrayElements(image, NULL);

	if (NULL == imageData) {
		error = J9CUDA_ERROR_OPERATING_SYSTEM;
		Trc_cuda_moduleLoad_fail(thread);
	} else {
		PORT_ACCESS_FROM_ENV(env);

		error = j9cuda_moduleLoad(
				(uint32_t)deviceId,
				imageData,
		        (J9CudaJitOptions *)options,
		        &module);

		env->ReleaseByteArrayElements(image, imageData, JNI_ABORT);
	}
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_moduleLoad_exit(thread, module);

	return (jlong)module;
}

#ifdef OMR_OPT_CUDA

/**
 * Unload a module from a device.
 *
 * Class:     com.ibm.cuda.CudaModule
 * Method:    unload
 * Signature: (IJ)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] module    the module pointer
 */
void JNICALL
Java_com_ibm_cuda_CudaModule_unload
  (JNIEnv * env, jclass, jint deviceId, jlong module)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_moduleUnload_entry(thread, deviceId, (J9CudaModule)module);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_moduleUnload((uint32_t)deviceId, (J9CudaModule)module);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_moduleUnload_exit(thread);
}

#endif /* OMR_OPT_CUDA */
