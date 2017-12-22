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
#include "java/com_ibm_cuda_CudaFunction.h"

#ifdef OMR_OPT_CUDA

/**
 * Query a function attribute.
 *
 * Class:     com.ibm.cuda.CudaFunction
 * Method:    getAttribute
 * Signature: (IJI)I
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] function   the function pointer
 * @param[in] attribute  the attribute to query
 * @return the value of the requested attribute
 */
jint JNICALL
Java_com_ibm_cuda_CudaFunction_getAttribute
  (JNIEnv * env, jclass, jint deviceId, jlong function, jint attribute)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_functionGetAttribute_entry(thread, deviceId, (J9CudaFunction)function, attribute);

	PORT_ACCESS_FROM_ENV(env);

	int32_t value = 0;
	int32_t error = j9cuda_funcGetAttribute(
			(uint32_t)deviceId,
			(J9CudaFunction)function,
			(J9CudaFunctionAttribute)attribute,
			&value);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_functionGetAttribute_exit(thread, value);

	return (jint)value;
}

/**
 * Launch a grid of threads executing a kernel function.
 *
 * Class:     com.ibm.cuda.CudaFunction
 * Method:    launch
 * Signature: (IJIIIIIIIJ[J)V
 *
 * @param[in] env             the JNI interface pointer
 * @param[in] (unused)        the class pointer
 * @param[in] deviceId        the device identifier
 * @param[in] function        the function pointer
 * @param[in] grdDimX         the grid size in the X dimension
 * @param[in] grdDimY         the grid size in the Y dimension
 * @param[in] grdDimZ         the grid size in the Z dimension
 * @param[in] blkDimX         the block size in the X dimension
 * @param[in] blkDimY         the block size in the Y dimension
 * @param[in] blkDimZ         the block size in the Z dimension
 * @param[in] sharedMemBytes  the number of bytes of dynamic shared memory required
 * @param[in] stream          the stream, or null for the default stream
 * @param[in] argValues       the array of argument values
 */
void JNICALL
Java_com_ibm_cuda_CudaFunction_launch
  (JNIEnv * env, jclass,
		  jint deviceId, jlong function,
		  jint grdDimX, jint grdDimY, jint grdDimZ,
		  jint blkDimX, jint blkDimY, jint blkDimZ,
		  jint sharedMemBytes, jlong stream,
		  jlongArray argValues)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_functionLaunch_entry(
			thread,
			deviceId,
			(J9CudaFunction)function,
			grdDimX, grdDimY, grdDimZ,
	        blkDimX, blkDimY, blkDimZ,
	        sharedMemBytes,
	        (J9CudaStream)stream,
	        argValues);

	int32_t error = J9CUDA_ERROR_OPERATING_SYSTEM;
	jlong * values = env->GetLongArrayElements(argValues, NULL);

	if (NULL == values) {
		Trc_cuda_functionLaunch_getFail(thread);
	} else {
		PORT_ACCESS_FROM_ENV(env);

		jsize    argCount = env->GetArrayLength(argValues);
		void  ** args     = (void **)J9CUDA_ALLOCATE_MEMORY(argCount * sizeof(void *));

		if (NULL == args) {
			Trc_cuda_functionLaunch_allocFail(thread);
			error = J9CUDA_ERROR_MEMORY_ALLOCATION;
		} else {
			// gather addresses of parameters
			for (jsize i = 0; i < argCount; ++i) {
				args[i] = &values[i];
			}

			error = j9cuda_launchKernel(
					(uint32_t)deviceId,
			        (J9CudaFunction)function,
			        (uint32_t)grdDimX, (uint32_t)grdDimY, (uint32_t)grdDimZ,
			        (uint32_t)blkDimX, (uint32_t)blkDimY, (uint32_t)blkDimZ,
			        (uint32_t)sharedMemBytes,
			        (J9CudaStream)stream,
			        (void **)args);

			J9CUDA_FREE_MEMORY(args);
		}

		env->ReleaseLongArrayElements(argValues, values, JNI_ABORT);
	}

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_functionLaunch_exit(thread);
}

/**
 * Set the cache configuration of a kernel function.
 *
 * Class:     com.ibm.cuda.CudaFunction
 * Method:    setCacheConfig
 * Signature: (IJI)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] function  the function pointer
 * @param[in] config    the requested cache configuration
 */
void JNICALL
Java_com_ibm_cuda_CudaFunction_setCacheConfig
  (JNIEnv * env, jclass, jint deviceId, jlong function, jint config)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_functionSetCacheConfig_entry(thread, deviceId, (J9CudaFunction)function, config);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_funcSetCacheConfig(
			(uint32_t)deviceId,
			(J9CudaFunction)function,
			(J9CudaCacheConfig)config);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_functionSetCacheConfig_exit(thread);
}

/**
 * Set the shared memory configuration of a kernel function.
 *
 * Class:     com.ibm.cuda.CudaFunction
 * Method:    setSharedMemConfig
 * Signature: (IJI)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] function  the function pointer
 * @param[in] config    the requested shared memory configuration
 */
void JNICALL
Java_com_ibm_cuda_CudaFunction_setSharedMemConfig
  (JNIEnv * env, jclass, jint deviceId, jlong function, jint config)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_functionSetSharedMemConfig_entry(thread, deviceId, (J9CudaFunction)function, config);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_funcSetSharedMemConfig(
			(uint32_t)deviceId,
			(J9CudaFunction)function,
			(J9CudaSharedMemConfig)config);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_functionSetSharedMemConfig_exit(thread);
}

#endif /* OMR_OPT_CUDA */
