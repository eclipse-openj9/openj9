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
#include "java/com_ibm_cuda_CudaStream.h"

/**
 * Create a stream with the default priority and flags.
 *
 * Class:     com.ibm.cuda.CudaStream
 * Method:    create
 * Signature: (I)J
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @return a new stream
 */
jlong JNICALL
Java_com_ibm_cuda_CudaStream_create
  (JNIEnv * env, jclass, jint deviceId)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_streamCreate_entry(thread, deviceId);

	J9CudaStream stream = NULL;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_streamCreate((uint32_t)deviceId, &stream);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_streamCreate_exit(thread, stream);

	return (jlong)stream;
}

/**
 * Create a stream with the specified flags and priority.
 *
 * Class:     com.ibm.cuda.CudaStream
 * Method:    createWithPriority
 * Signature: (III)J
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] flags     the stream creation flags
 * @param[in] priority  the requested stream priority
 * @return a new stream
 */
jlong JNICALL
Java_com_ibm_cuda_CudaStream_createWithPriority
  (JNIEnv * env, jclass, jint deviceId, jint flags, jint priority)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_streamCreateWithPriority_entry(thread, deviceId, flags, priority);

	J9CudaStream stream = NULL;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_streamCreateWithPriority(
			(uint32_t)deviceId,
			priority,
			(uint32_t)flags,
			&stream);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_streamCreateWithPriority_exit(thread, stream);

	return (jlong)stream;
}

#ifdef OMR_OPT_CUDA

/**
 * Destroy a stream.
 *
 * Class:     com.ibm.cuda.CudaStream
 * Method:    destroy
 * Signature: (IJ)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] stream    the stream to be destroyed
 */
void JNICALL
Java_com_ibm_cuda_CudaStream_destroy
  (JNIEnv * env, jclass, jint deviceId, jlong stream)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_streamDestroy_entry(thread, deviceId, (J9CudaStream)stream);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_streamDestroy((uint32_t)deviceId, (J9CudaStream)stream);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_streamDestroy_exit(thread, error);
}

/**
 * Query the flags of a stream.
 *
 * Class:     com.ibm.cuda.CudaStream
 * Method:    getFlags
 * Signature: (IJ)I
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] stream    the stream
 * @return the flags of the stream
 */
jint JNICALL
Java_com_ibm_cuda_CudaStream_getFlags
  (JNIEnv * env, jclass, jint deviceId, jlong stream)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_streamGetFlags_entry(thread, deviceId, (J9CudaStream)stream);

	PORT_ACCESS_FROM_ENV(env);

	uint32_t flags = 0;
	int32_t error = j9cuda_streamGetFlags((uint32_t)deviceId, (J9CudaStream)stream, &flags);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_streamGetFlags_exit(thread, error, flags);

	return (jint)flags;
}

/**
 * Query the priority of a stream.
 *
 * Class:     com.ibm.cuda.CudaStream
 * Method:    getPriority
 * Signature: (IJ)I
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] stream    the stream
 * @return the priority of the stream
 */
jint JNICALL
Java_com_ibm_cuda_CudaStream_getPriority
  (JNIEnv * env, jclass, jint deviceId, jlong stream)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_streamGetPriority_entry(thread, deviceId, (J9CudaStream)stream);

	PORT_ACCESS_FROM_ENV(env);

	int32_t priority = 0;
	int32_t error = j9cuda_streamGetPriority((uint32_t)deviceId, (J9CudaStream)stream, &priority);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_streamGetPriority_exit(thread, error, priority);

	return priority;
}

/**
 * Query the completion status of a stream.
 *
 * Class:     com.ibm.cuda.CudaStream
 * Method:    query
 * Signature: (IJ)I
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] stream    the stream
 * @return 0 if the stream is complete; else cudaErrorNotReady or another error
 */
jint JNICALL
Java_com_ibm_cuda_CudaStream_query
  (JNIEnv * env, jclass, jint deviceId, jlong stream)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_streamQuery_entry(thread, deviceId, (J9CudaStream)stream);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_streamQuery((uint32_t)deviceId, (J9CudaStream)stream);

	Trc_cuda_streamQuery_exit(thread, error);

	return error;
}

/**
 * Synchronize with a stream.
 *
 * Class:     com.ibm.cuda.CudaStream
 * Method:    synchronize
 * Signature: (IJ)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] stream    the stream
 */
void JNICALL
Java_com_ibm_cuda_CudaStream_synchronize
  (JNIEnv * env, jclass, jint deviceId, jlong stream)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_streamSynchronize_entry(thread, deviceId, (J9CudaStream)stream);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_streamSynchronize((uint32_t)deviceId, (J9CudaStream)stream);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_streamSynchronize_exit(thread, error);
}

/**
 * Cause a stream to wait for an event.
 *
 * Class:     com.ibm.cuda.CudaStream
 * Method:    waitFor
 * Signature: (IJJ)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] stream    the stream
 * @param[in] event     the event to wait for
 */
void JNICALL
Java_com_ibm_cuda_CudaStream_waitFor
  (JNIEnv * env, jclass, jint deviceId, jlong stream, jlong event)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_streamWaitFor_entry(thread, deviceId, (J9CudaStream)stream, (J9CudaEvent)event);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_streamWaitEvent((uint32_t)deviceId, (J9CudaStream)stream, (J9CudaEvent)event);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_streamWaitFor_exit(thread, error);
}

#endif /* OMR_OPT_CUDA */
