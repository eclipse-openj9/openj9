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
#include "java/com_ibm_cuda_CudaEvent.h"

/**
 * Create an event with the specified flags.
 *
 * Class:     com.ibm.cuda.CudaEvent
 * Method:    create
 * Signature: (II)J
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] flags     the event creation flags
 * @return a new event
 */
jlong JNICALL
Java_com_ibm_cuda_CudaEvent_create
  (JNIEnv * env, jclass, jint deviceId, jint flags)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_eventCreate_entry(thread, deviceId, flags);

	J9CudaEvent event = NULL;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_eventCreate((uint32_t)deviceId, (uint32_t)flags, &event);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_eventCreate_exit(thread, event);

	return (jlong)event;
}

#ifdef OMR_OPT_CUDA

/**
 * Destroy an event.
 *
 * Class:     com.ibm.cuda.CudaEvent
 * Method:    destroy
 * Signature: (IJ)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] event     the event
 */
void JNICALL
Java_com_ibm_cuda_CudaEvent_destroy
  (JNIEnv * env, jclass, jint deviceId, jlong event)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_eventDestroy_entry(thread, deviceId, (J9CudaEvent)event);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_eventDestroy((uint32_t)deviceId, (J9CudaEvent)event);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_eventDestroy_exit(thread, error);
}

/**
 * Return the elapsed time in milliseconds between two events.
 *
 * Class:     com.ibm.cuda.CudaEvent
 * Method:    elapsedTimeSince
 * Signature: (JJ)F
 *
 * @param[in] env         the JNI interface pointer
 * @param[in] (unused)    the class pointer
 * @param[in] event       the reference event
 * @param[in] priorEvent  the earlier event
 * @return the elapsed time in milliseconds between the two events
 */
jfloat JNICALL
Java_com_ibm_cuda_CudaEvent_elapsedTimeSince
  (JNIEnv * env, jclass, jlong event, jlong priorEvent)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_eventElapsedTimeSince_entry(thread, (J9CudaEvent)priorEvent, (J9CudaEvent)event);

	PORT_ACCESS_FROM_ENV(env);

	float elapsed = 0.0f;
	int32_t error = j9cuda_eventElapsedTime((J9CudaEvent)priorEvent, (J9CudaEvent)event, &elapsed);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_eventElapsedTimeSince_exit(thread, (jint)error, (jfloat)elapsed);

	return (jfloat)elapsed;
}

/**
 * Query the status of an event.
 *
 * Class:     com.ibm.cuda.CudaEvent
 * Method:    query
 * Signature: (J)I
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] event     the event
 * @return 0 if the event has been recorded; else cudaErrorNotReady or another error
 */
jint JNICALL
Java_com_ibm_cuda_CudaEvent_query
  (JNIEnv * env, jclass, jlong event)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_eventQuery_entry(thread, (J9CudaEvent)event);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_eventQuery((J9CudaEvent)event);

	Trc_cuda_eventQuery_exit(thread, error);

	return (jint)error;
}

/**
 * Record an event on a stream or the default stream.
 *
 * Class:     com.ibm.cuda.CudaEvent
 * Method:    record
 * Signature: (IJJ)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] stream    the stream, or null for the default stream
 * @param[in] event     the event to be recorded
 */
void JNICALL
Java_com_ibm_cuda_CudaEvent_record
  (JNIEnv * env, jclass, jint deviceId, jlong stream, jlong event)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_eventRecord_entry(thread, deviceId, (J9CudaStream)stream, (J9CudaEvent)event);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_eventRecord((uint32_t)deviceId, (J9CudaEvent)event, (J9CudaStream)stream);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_eventRecord_exit(thread);
}

/**
 * Synchronize with an event.
 *
 * Class:     com.ibm.cuda.CudaEvent
 * Method:    synchronize
 * Signature: (J)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] event     the event to be recorded
 */
void JNICALL
Java_com_ibm_cuda_CudaEvent_synchronize
  (JNIEnv * env, jclass, jlong event)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_eventSynchronize_entry(thread, (J9CudaEvent)event);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_eventSynchronize((J9CudaEvent)event);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_eventSynchronize_exit(thread, (jint)error);
}

#endif /* OMR_OPT_CUDA */
