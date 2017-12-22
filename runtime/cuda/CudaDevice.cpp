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
#include "java/com_ibm_cuda_CudaDevice.h"

#ifdef OMR_OPT_CUDA

namespace
{

/**
 * Instances of this class convey the necessary information to trigger a callback
 * to a Java Runnable object.
 */
class Callback {
private:
	static char             threadName[];
	static JavaVMAttachArgs vmAttachArgs;

	JavaVM  * jvm;
	jobject   runnable;

	static void
	handler(J9CudaStream stream, int32_t error, uintptr_t data)
	{
		Trc_cuda_deviceCallbackHandler_entry(stream, error, data);

		Callback * callback = (Callback *)data;
		JNIEnv * env = NULL;
		jint status = callback->jvm->AttachCurrentThreadAsDaemon((void **)&env, &vmAttachArgs);

		if (JNI_OK != status) {
			Trc_cuda_deviceCallbackHandler_exitFail(status);
		} else {
			J9VMThread * thread = (J9VMThread *)env;
			J9CudaGlobals * globals = thread->javaVM->cudaGlobals;

			Assert_cuda_true(NULL != globals);
			Assert_cuda_true(NULL != globals->runnable_run);

			env->CallVoidMethod(callback->runnable, globals->runnable_run);

			jthrowable throwable = env->ExceptionOccurred();

			if (NULL != throwable) {
				// This happens on a thread started by the CUDA runtime: there is no
				// Java caller to catch this exception but we'll note that it occurred.
				Trc_cuda_deviceCallbackHandler_exception(thread, throwable);
				env->ExceptionClear();
			}

			env->DeleteGlobalRef(callback->runnable);

			PORT_ACCESS_FROM_ENV(env);

			J9CUDA_FREE_MEMORY(callback);

			Trc_cuda_deviceCallbackHandler_exit(thread);
		}
	}

public:
	VMINLINE static void
	insert(JNIEnv * env, jint deviceId, jlong stream, jobject runnable)
	{
		J9VMThread * thread = (J9VMThread *)env;

		Trc_cuda_deviceCallbackInsert_entry(thread, deviceId, (J9CudaStream)stream, runnable);

		PORT_ACCESS_FROM_ENV(env);

		Callback * callback = (Callback *)J9CUDA_ALLOCATE_MEMORY(sizeof(Callback));
		int32_t error = J9CUDA_ERROR_MEMORY_ALLOCATION;

		if (NULL == callback) {
			Trc_cuda_deviceCallbackInsert_allocFail(thread);
			goto fail;
		}

		callback->jvm      = (JavaVM *)thread->javaVM;
		callback->runnable = env->NewGlobalRef(runnable);

		if (NULL == callback->runnable) {
			Trc_cuda_deviceCallbackInsert_globalRefFail(thread);
		} else {
			error = j9cuda_streamAddCallback(
					(uint32_t)deviceId,
					(J9CudaStream)stream,
					handler,
					(uintptr_t)callback);
		}

		if (0 != error) {
			if (NULL != callback->runnable) {
				env->DeleteGlobalRef(callback->runnable);
			}

			J9CUDA_FREE_MEMORY(callback);

fail:
			throwCudaException(env, error);
		}

		Trc_cuda_deviceCallbackInsert_exit(thread, error);
	}
};

char             Callback::threadName[] = "cuda4j-callback";
JavaVMAttachArgs Callback::vmAttachArgs = { JNI_VERSION_1_6, threadName, NULL };

} // namespace

#endif /* OMR_OPT_CUDA */

/**
 * Add a callback to a stream or the default stream of a device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    addCallback
 * Signature: (IJLjava/lang/Runnable;)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] stream    the stream, or null for the default stream
 * @param[in] callback  the callback object
 */
void JNICALL
Java_com_ibm_cuda_CudaDevice_addCallback
  (JNIEnv * env, jclass, jint deviceId, jlong stream, jobject callback)
{
	J9VMThread * thread = (J9VMThread *)env;

#ifdef OMR_OPT_CUDA
	Trc_cuda_deviceAddCallback_entry(thread, deviceId, (uintptr_t)stream, (uintptr_t)callback);

	Callback::insert(env, deviceId, stream, callback);
#else /* OMR_OPT_CUDA */
	throwCudaException(env, J9CUDA_ERROR_NO_DEVICE);
#endif /* OMR_OPT_CUDA */

	Trc_cuda_deviceAddCallback_exit(thread);
}

/**
 * Query whether a device can access a peer device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    canAccessPeer
 * Signature: (II)Z
 *
 * @param[in] env           the JNI interface pointer
 * @param[in] (unused)      the class pointer
 * @param[in] deviceId      the device identifier
 * @param[in] peerDeviceId  the peer device identifier
 * @return whether deviceId can access peerDeviceId
 */
jboolean JNICALL
Java_com_ibm_cuda_CudaDevice_canAccessPeer
  (JNIEnv * env, jclass, jint deviceId, jint peerDeviceId)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceCanAccessPeer_entry(thread, deviceId, peerDeviceId);

	BOOLEAN result = 0;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_deviceCanAccessPeer((uint32_t)deviceId, (uint32_t)peerDeviceId, &result);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceCanAccessPeer_exit(thread, 0 != result);

	return (jboolean)(0 != result);
}

/**
 * Disable access to a peer device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    disablePeerAccess
 * Signature: (II)V
 *
 * @param[in] env           the JNI interface pointer
 * @param[in] (unused)      the class pointer
 * @param[in] deviceId      the device identifier
 * @param[in] peerDeviceId  the peer device identifier
 */
void JNICALL
Java_com_ibm_cuda_CudaDevice_disablePeerAccess
  (JNIEnv * env, jclass, jint deviceId, jint peerDeviceId)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceDisablePeerAccess_entry(thread, deviceId, peerDeviceId);

	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_deviceDisablePeerAccess((uint32_t)deviceId, (uint32_t)peerDeviceId);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceDisablePeerAccess_exit(thread);
}

/**
 * Enable access to a peer device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    enablePeerAccess
 * Signature: (II)V
 *
 * @param[in] env           the JNI interface pointer
 * @param[in] (unused)      the class pointer
 * @param[in] deviceId      the device identifier
 * @param[in] peerDeviceId  the peer device identifier
 */
void JNICALL
Java_com_ibm_cuda_CudaDevice_enablePeerAccess
  (JNIEnv * env, jclass, jint deviceId, jint peerDeviceId)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceEnablePeerAccess_entry(thread, deviceId, peerDeviceId);

	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_deviceEnablePeerAccess((uint32_t)deviceId, (uint32_t)peerDeviceId);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceEnablePeerAccess_exit(thread);
}

/**
 * Query a device attribute.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    getAttribute
 * Signature: (II)I
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] attribute  the attribute to query
 * @return the value of the requested attribute
 */
jint JNICALL
Java_com_ibm_cuda_CudaDevice_getAttribute
  (JNIEnv * env, jclass, jint deviceId, jint attribute)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceGetAttribute_entry(thread, deviceId, attribute);

	int32_t value = 0;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = J9CUDA_ERROR_INVALID_VALUE;
	J9CudaDeviceAttribute j9attrib = J9CUDA_DEVICE_ATTRIBUTE_WARP_SIZE;

	switch (attribute) {
	default:
		goto fail;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_ASYNC_ENGINE_COUNT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_ASYNC_ENGINE_COUNT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_CAN_MAP_HOST_MEMORY:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_CLOCK_RATE:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_CLOCK_RATE;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_COMPUTE_CAPABILITY:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_COMPUTE_MODE:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_COMPUTE_MODE;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_CONCURRENT_KERNELS:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_CONCURRENT_KERNELS;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_ECC_ENABLED:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_ECC_ENABLED;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_INTEGRATED:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_INTEGRATED;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_KERNEL_EXEC_TIMEOUT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_L2_CACHE_SIZE:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_L2_CACHE_SIZE;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAX_BLOCK_DIM_X:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAX_BLOCK_DIM_Y:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAX_BLOCK_DIM_Z:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAX_GRID_DIM_X:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAX_GRID_DIM_Y:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Y;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAX_GRID_DIM_Z:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Z;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_LAYERS:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_LAYERS;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_LAYERED_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACE1D_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE1D_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACE2D_HEIGHT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_HEIGHT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_HEIGHT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_HEIGHT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_LAYERS:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_LAYERS;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_LAYERED_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACE2D_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE2D_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACE3D_DEPTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_DEPTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACE3D_HEIGHT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_HEIGHT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACE3D_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACE3D_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_LAYERS;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_LAYERED_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_SURFACECUBEMAP_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_LAYERS:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_LAYERS;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE1D_LINEAR_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LINEAR_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_MIPMAPPED_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE1D_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_HEIGHT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_HEIGHT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_GATHER_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE2D_HEIGHT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_HEIGHT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_HEIGHT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_HEIGHT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_LAYERS:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_LAYERS;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LAYERED_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_HEIGHT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_HEIGHT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_PITCH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_PITCH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_LINEAR_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_HEIGHT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_MIPMAPPED_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE2D_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH_ALTERNATE;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT_ALTERNATE;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH_ALTERNATE;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_LAYERS;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_LAYERED_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_WIDTH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURECUBEMAP_WIDTH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAX_PITCH:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAX_PITCH;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAX_THREADS_PER_BLOCK:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MEMORY_CLOCK_RATE:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_MULTIPROCESSOR_COUNT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_PCI_BUS_ID:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_PCI_BUS_ID;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_PCI_DEVICE_ID:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_PCI_DEVICE_ID;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_PCI_DOMAIN_ID:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_PCI_DOMAIN_ID;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_STREAM_PRIORITIES_SUPPORTED:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_STREAM_PRIORITIES_SUPPORTED;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_SURFACE_ALIGNMENT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_SURFACE_ALIGNMENT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_TCC_DRIVER:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_TCC_DRIVER;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_TEXTURE_ALIGNMENT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_TEXTURE_ALIGNMENT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_TEXTURE_PITCH_ALIGNMENT:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_TEXTURE_PITCH_ALIGNMENT;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_TOTAL_CONSTANT_MEMORY:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_UNIFIED_ADDRESSING:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING;
		break;
	case com_ibm_cuda_CudaDevice_ATTRIBUTE_WARP_SIZE:
		j9attrib = J9CUDA_DEVICE_ATTRIBUTE_WARP_SIZE;
		break;
	}

	error = j9cuda_deviceGetAttribute((uint32_t)deviceId, j9attrib, &value);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
#ifdef OMR_OPT_CUDA
fail:
#endif /* OMR_OPT_CUDA */
		throwCudaException(env, error);
	}

	Trc_cuda_deviceGetAttribute_exit(thread, value);

	return (jint)value;
}

/**
 * Query the cache configuration of a device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    getCacheConfig
 * Signature: (I)I
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @return the device cache configuration
 */
jint JNICALL
Java_com_ibm_cuda_CudaDevice_getCacheConfig
  (JNIEnv * env, jclass, jint deviceId)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceGetCacheConfig_entry(thread, deviceId);

	J9CudaCacheConfig config = J9CUDA_CACHE_CONFIG_PREFER_EQUAL;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_deviceGetCacheConfig((uint32_t)deviceId, &config);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceGetCacheConfig_exit(thread, config);

	return (jint)config;
}

/**
 * Query the amount of free memory on a device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    getFreeMemory
 * Signature: (I)J
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @return the number of bytes of free memory on the device
 */
jlong JNICALL
Java_com_ibm_cuda_CudaDevice_getFreeMemory
  (JNIEnv * env, jclass, jint deviceId)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceGetFreeMemory_entry(thread, deviceId);

	uintptr_t freeBytes = 0;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	uintptr_t totalBytes = 0;
	error = j9cuda_deviceGetMemInfo((uint32_t)deviceId, &freeBytes, &totalBytes);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceGetFreeMemory_exit(thread, freeBytes);

	return (jlong)freeBytes;
}

/**
 * Return the greatest stream priority for a device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    getGreatestStreamPriority
 * Signature: (I)I
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @return the greatest stream priority
 */
jint JNICALL
Java_com_ibm_cuda_CudaDevice_getGreatestStreamPriority
  (JNIEnv * env, jclass, jint deviceId)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceGetGreatestStreamPriority_entry(thread, deviceId);

	int32_t greatestPriority = 0;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	int32_t leastPriority = 0;
	error = j9cuda_deviceGetStreamPriorityRange((uint32_t)deviceId, &leastPriority, &greatestPriority);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceGetGreatestStreamPriority_exit(thread, greatestPriority);

	return (jint)greatestPriority;
}

/**
 * Return the least stream priority for a device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    getLeastStreamPriority
 * Signature: (I)I
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @return the least stream priority
 */
jint JNICALL
Java_com_ibm_cuda_CudaDevice_getLeastStreamPriority
  (JNIEnv * env, jclass, jint deviceId)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceGetLeastStreamPriority_entry(thread, deviceId);

	int32_t leastPriority = 0;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	int32_t greatestPriority = 0;
	error = j9cuda_deviceGetStreamPriorityRange((uint32_t)deviceId, &leastPriority, &greatestPriority);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceGetLeastStreamPriority_exit(thread, leastPriority);

	return (jint)leastPriority;
}

/**
 * Query a limit of a device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    getLimit
 * Signature: (II)J
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] limit      the requested limit
 * @return the value of the requested limit
 */
jlong JNICALL
Java_com_ibm_cuda_CudaDevice_getLimit
  (JNIEnv * env, jclass, jint deviceId, jint limit)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceGetLimit_entry(thread, deviceId, limit);

	uintptr_t value = 0;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_deviceGetLimit((uint32_t)deviceId, (J9CudaDeviceLimit)limit, &value);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceGetLimit_exit(thread, value);

	return (jlong)value;
}

/**
 * Query the name of a device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    getName
 * Signature: (I)Ljava/lang/String;
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @return the name of the device
 */
jstring JNICALL
Java_com_ibm_cuda_CudaDevice_getName
  (JNIEnv * env, jclass, jint deviceId)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceGetName_entry(thread, deviceId);

	jstring result = NULL;
	char name[256];

	name[0] = 0;

	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_deviceGetName((uint32_t)deviceId, (uint32_t)sizeof(name), name);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	} else {
		result = env->NewStringUTF(name);
	}

	Trc_cuda_deviceGetName_exit(thread, result, name);

	return result;
}

/**
 * Query the shared memory configuration of a device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    getSharedMemConfig
 * Signature: (I)I
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @return the shared memory configuration
 */
jint JNICALL
Java_com_ibm_cuda_CudaDevice_getSharedMemConfig
  (JNIEnv * env, jclass, jint deviceId)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceGetSharedMemConfig_entry(thread, deviceId);

	J9CudaSharedMemConfig config = J9CUDA_SHARED_MEM_CONFIG_DEFAULT_BANK_SIZE;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_deviceGetSharedMemConfig((uint32_t)deviceId, &config);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceGetSharedMemConfig_exit(thread, config);

	return (jint)config;
}

/**
 * Query the total amount of memory on a device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    getTotalMemory
 * Signature: (I)J
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @return the number of bytes of memory on the device
 */
jlong JNICALL
Java_com_ibm_cuda_CudaDevice_getTotalMemory
  (JNIEnv * env, jclass, jint deviceId)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceGetTotalMemory_entry(thread, deviceId);

	uintptr_t totalBytes = 0;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	uintptr_t freeBytes = 0;
	error = j9cuda_deviceGetMemInfo((uint32_t)deviceId, &freeBytes, &totalBytes);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceGetTotalMemory_exit(thread, totalBytes);

	return (jlong)totalBytes;
}

/**
 * Adjust the cache configuration of a device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    setCacheConfig
 * Signature: (II)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] config    the requested cache configuration
 */
void JNICALL
Java_com_ibm_cuda_CudaDevice_setCacheConfig
  (JNIEnv * env, jclass, jint deviceId, jint config)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceSetCacheConfig_entry(thread, deviceId, config);

	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_deviceSetCacheConfig((uint32_t)deviceId, (J9CudaCacheConfig)config);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceSetCacheConfig_exit(thread);
}

/**
 * Set a device limit.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    setLimit
 * Signature: (IIJ)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] limit      the limit to be adjusted
 * @param[in] value      the new value of the limit
 */
void JNICALL
Java_com_ibm_cuda_CudaDevice_setLimit
  (JNIEnv * env, jclass, jint deviceId, jint limit, jlong value)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceSetLimit_entry(thread, deviceId, limit, value);

	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_deviceSetLimit(
			(uint32_t)deviceId,
			(J9CudaDeviceLimit)limit,
			(uintptr_t)value);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceSetLimit_exit(thread);
}

/**
 * Set the shared memory configuration of a device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    setSharedMemConfig
 * Signature: (II)V
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] deviceId  the device identifier
 * @param[in] config    the requested shared memory configuration
 */
void JNICALL
Java_com_ibm_cuda_CudaDevice_setSharedMemConfig
  (JNIEnv * env, jclass, jint deviceId, jint config)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceSetSharedMemConfig_entry(thread, deviceId, config);

	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_deviceSetSharedMemConfig((uint32_t)deviceId, (J9CudaSharedMemConfig)config);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceSetSharedMemConfig_exit(thread);
}

/**
 * Synchronize with a device.
 *
 * Class:     com.ibm.cuda.CudaDevice
 * Method:    synchronize
 * Signature: (I)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 */
void JNICALL
Java_com_ibm_cuda_CudaDevice_synchronize
  (JNIEnv * env, jclass, jint deviceId)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_deviceSynchronize_entry(thread, deviceId);

	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_deviceSynchronize((uint32_t)deviceId);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_deviceSynchronize_exit(thread);
}
