/*******************************************************************************
 * Copyright (c) 2013, 2015 IBM Corp. and others
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
#include "java/com_ibm_cuda_CudaJitOptions.h"

namespace
{

/**
 * Destroy a JIT options instance.
 */
void
destroyOptions(JNIEnv * env, J9CudaJitOptions * options)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_jitOptionsDestroy0_entry(thread, options);

	PORT_ACCESS_FROM_ENV(env);

	void * errorLog = options->errorLogBuffer;

	if (NULL != errorLog) {
		J9CUDA_FREE_MEMORY(errorLog);
	}

	void * infoLog = options->infoLogBuffer;

	if (NULL != infoLog) {
		J9CUDA_FREE_MEMORY(infoLog);
	}

	J9CUDA_FREE_MEMORY(options);

	Trc_cuda_jitOptionsDestroy0_exit(thread);
}

} // namespace

/**
 * Create and initialize a JIT options object.
 *
 * Class:     com.ibm.cuda.CudaJitOptions
 * Method:    create
 * Signature: ([I)J
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] pairs     an array of option/value pairs
 * @return a pointer to a new JIT options object
 */
jlong JNICALL
Java_com_ibm_cuda_CudaJitOptions_create
  (JNIEnv * env, jclass, jintArray pairs)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_jitOptionsCreate_entry(thread, pairs);

	jsize const   pairLength = env->GetArrayLength(pairs) & ~1;
	jint        * pairData   = NULL;

	if (0 != pairLength) {
		pairData = env->GetIntArrayElements(pairs, NULL);

		if (NULL == pairData) {
			Trc_cuda_jitOptionsCreate_getFail(thread);
			throwCudaException(env, J9CUDA_ERROR_OPERATING_SYSTEM);
			Trc_cuda_jitOptionsCreate_exit(thread, NULL);
			return 0;
		}
	}

	PORT_ACCESS_FROM_ENV(env);

	int32_t            error   = 0;
	J9CudaJitOptions * options = (J9CudaJitOptions *)J9CUDA_ALLOCATE_MEMORY(sizeof(J9CudaJitOptions));

	if (NULL == options) {
		Trc_cuda_jitOptionsCreate_allocFail(thread);
		error = J9CUDA_ERROR_MEMORY_ALLOCATION;
	} else {
		memset(options, 0, sizeof(J9CudaJitOptions));

		for (jint index = 0; (0 == error) && (index < pairLength); index += 2) {
			jint key   = pairData[index];
			jint value = pairData[index + 1];

			switch (key) {
			case com_ibm_cuda_CudaJitOptions_OPT_CACHE_MODE:
				options->cacheMode = (J9CudaJitCacheMode)value;
				break;

			case com_ibm_cuda_CudaJitOptions_OPT_ERROR_LOG_BUFFER_SIZE_BYTES:
				if (NULL != options->errorLogBuffer) {
					J9CUDA_FREE_MEMORY(options->errorLogBuffer);
				}

				options->errorLogBuffer = (char *)J9CUDA_ALLOCATE_MEMORY((size_t)value);
				options->errorLogBufferSize = (uintptr_t)value;

				if (NULL == options->errorLogBuffer) {
					Trc_cuda_jitOptionsCreate_allocFail(thread);
					options->errorLogBufferSize = 0;
					error = J9CUDA_ERROR_MEMORY_ALLOCATION;
				}
				break;

			case com_ibm_cuda_CudaJitOptions_OPT_FALLBACK_STRATEGY:
				options->fallbackStrategy = (J9CudaJitFallbackStrategy)value;
				break;

			case com_ibm_cuda_CudaJitOptions_OPT_GENERATE_DEBUG_INFO:
				options->generateDebugInfo = value ? J9CUDA_JIT_FLAG_ENABLED : J9CUDA_JIT_FLAG_DISABLED;
				break;

			case com_ibm_cuda_CudaJitOptions_OPT_GENERATE_LINE_INFO:
				options->generateLineInfo = value ? J9CUDA_JIT_FLAG_ENABLED : J9CUDA_JIT_FLAG_DISABLED;
				break;

			case com_ibm_cuda_CudaJitOptions_OPT_INFO_LOG_BUFFER_SIZE_BYTES:
				if (NULL != options->infoLogBuffer) {
					J9CUDA_FREE_MEMORY(options->infoLogBuffer);
				}

				options->infoLogBuffer = (char *)J9CUDA_ALLOCATE_MEMORY((size_t)value);
				options->infoLogBufferSize = (uintptr_t)value;

				if (NULL == options->infoLogBuffer) {
					Trc_cuda_jitOptionsCreate_allocFail(thread);
					options->infoLogBufferSize = 0;
					error = J9CUDA_ERROR_MEMORY_ALLOCATION;
				}
				break;

			case com_ibm_cuda_CudaJitOptions_OPT_LOG_VERBOSE:
				options->verboseLogging = value ? J9CUDA_JIT_FLAG_ENABLED : J9CUDA_JIT_FLAG_DISABLED;
				break;

			case com_ibm_cuda_CudaJitOptions_OPT_MAX_REGISTERS:
				options->maxRegisters = (uint32_t)value;
				break;

			case com_ibm_cuda_CudaJitOptions_OPT_OPTIMIZATION_LEVEL:
				switch (value) {
				case 0:
					options->optimizationLevel = J9CUDA_JIT_OPTIMIZATION_LEVEL_0;
					break;
				case 1:
					options->optimizationLevel = J9CUDA_JIT_OPTIMIZATION_LEVEL_1;
					break;
				case 2:
					options->optimizationLevel = J9CUDA_JIT_OPTIMIZATION_LEVEL_2;
					break;
				case 3:
					options->optimizationLevel = J9CUDA_JIT_OPTIMIZATION_LEVEL_3;
					break;
				case 4:
					options->optimizationLevel = J9CUDA_JIT_OPTIMIZATION_LEVEL_4;
					break;
				default:
					error = J9CUDA_ERROR_INVALID_VALUE;
					break;
				}
				break;

			case com_ibm_cuda_CudaJitOptions_OPT_TARGET:
				options->target = (uint32_t)value;
				break;

			case com_ibm_cuda_CudaJitOptions_OPT_TARGET_FROM_CUCONTEXT:
				options->targetFromContext = value ? J9CUDA_JIT_FLAG_ENABLED : J9CUDA_JIT_FLAG_DISABLED;
				break;

			case com_ibm_cuda_CudaJitOptions_OPT_THREADS_PER_BLOCK:
				if (0 != value) {
					options->threadsPerBlock = (uint32_t)value;
				} else {
					error = J9CUDA_ERROR_INVALID_VALUE;
				}
				break;

			case com_ibm_cuda_CudaJitOptions_OPT_WALL_TIME:
				options->recordWallTime = value ? J9CUDA_JIT_FLAG_ENABLED : J9CUDA_JIT_FLAG_DISABLED;
				break;

			default:
				Trc_cuda_jitOptionsCreate_badOption(thread, key);
				error = J9CUDA_ERROR_INVALID_VALUE;
				break;
			}
		}
	}

	if (NULL != pairData) {
		env->ReleaseIntArrayElements(pairs, pairData, JNI_ABORT);
	}

	if (0 != error) {
		throwCudaException(env, error);
	}

	if (env->ExceptionCheck() && (NULL != options)) {
		destroyOptions(env, options);
		options = NULL;
	}

	Trc_cuda_jitOptionsCreate_exit(thread, options);

	return (jlong)options;
}

/**
 * Destroy a JIT options object.
 *
 * Class:     com.ibm.cuda.CudaJitOptions
 * Method:    destroy
 * Signature: (J)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] handle    the options pointer
 */
void JNICALL
Java_com_ibm_cuda_CudaJitOptions_destroy
  (JNIEnv * env, jclass, jlong handle)
{
	J9VMThread       * thread  = (J9VMThread *)env;
	J9CudaJitOptions * options = (J9CudaJitOptions *)handle;

	Trc_cuda_jitOptionsDestroy_entry(thread, options);

	destroyOptions(env, options);

	Trc_cuda_jitOptionsDestroy_exit(thread);
}

/**
 * Return the contents of the error log.
 *
 * Class:     com.ibm.cuda.CudaJitOptions
 * Method:    getErrorLogBuffer
 * Signature: (J)Ljava/lang/String;
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] handle    the options pointer
 * @return a string containing the error log
 */
jstring JNICALL
Java_com_ibm_cuda_CudaJitOptions_getErrorLogBuffer
  (JNIEnv * env, jclass, jlong handle)
{
	J9VMThread       * thread  = (J9VMThread *)env;
	J9CudaJitOptions * options = (J9CudaJitOptions *)handle;

	Trc_cuda_jitOptionsGetErrorLogBuffer_entry(thread, options);

	const char * buffer = ((J9CudaJitOptions *)handle)->errorLogBuffer;
	jstring      result = env->NewStringUTF((NULL == buffer) ? "" : buffer);

	Trc_cuda_jitOptionsGetErrorLogBuffer_exit(thread, result);

	return result;
}

/**
 * Return the contents of the information log.
 *
 * Class:     com.ibm.cuda.CudaJitOptions
 * Method:    getInfoLogBuffer
 * Signature: (J)Ljava/lang/String;
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] handle    the options pointer
 * @return a string containing the information log
 */
jstring JNICALL
Java_com_ibm_cuda_CudaJitOptions_getInfoLogBuffer
  (JNIEnv * env, jclass, jlong handle)
{
	J9VMThread         * thread   = (J9VMThread *)env;
	J9CudaJitOptions * options = (J9CudaJitOptions *)handle;

	Trc_cuda_jitOptionsGetInfoLogBuffer_entry(thread, options);

	const char * buffer = options->infoLogBuffer;
	jstring      result = env->NewStringUTF((NULL == buffer) ? "" : buffer);

	Trc_cuda_jitOptionsGetInfoLogBuffer_exit(thread, result);

	return result;
}

/**
 * Return the number of threads the compiler actually targeted per block.
 *
 * Class:     com.ibm.cuda.CudaJitOptions
 * Method:    getThreadsPerBlock
 * Signature: (J)I
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] handle    the options pointer
 * @return the number of threads per block
 */
jint JNICALL
Java_com_ibm_cuda_CudaJitOptions_getThreadsPerBlock
  (JNIEnv * env, jclass, jlong handle)
{
	J9VMThread         * thread   = (J9VMThread *)env;
	J9CudaJitOptions * options = (J9CudaJitOptions *)handle;

	Trc_cuda_jitOptionsGetThreadsPerBlock_entry(thread, options);

	jint result = (jint)options->threadsPerBlock;

	Trc_cuda_jitOptionsGetThreadsPerBlock_exit(thread, result);

	return result;
}

/**
 * Return the elapsed compile or link time in milliseconds.
 *
 * Class:     com.ibm.cuda.CudaJitOptions
 * Method:    getWallTime
 * Signature: (J)F
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] handle    the options pointer
 * @return the elapsed compile/link time
 */
jfloat JNICALL
Java_com_ibm_cuda_CudaJitOptions_getWallTime
  (JNIEnv * env, jclass, jlong handle)
{
	J9VMThread         * thread   = (J9VMThread *)env;
	J9CudaJitOptions * options = (J9CudaJitOptions *)handle;

	Trc_cuda_jitOptionsGetWallTime_entry(thread, options);

	jfloat result = (jfloat)options->wallTime;

	Trc_cuda_jitOptionsGetWallTime_exit(thread, result);

	return result;
}
