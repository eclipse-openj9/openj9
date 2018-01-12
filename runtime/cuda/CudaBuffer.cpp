/*******************************************************************************
 * Copyright (c) 2013, 2017 IBM Corp. and others
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
#include "java/com_ibm_cuda_CudaBuffer.h"

#ifdef OMR_OPT_CUDA

namespace
{

/**
 * A macro that expands the argument macro for each primitive type.
 */
#define SPECIALIZE_ALL(SPECIALIZATION)  \
		SPECIALIZATION(jbyte,   Byte)   \
		SPECIALIZATION(jchar,   Char)   \
		SPECIALIZATION(jfloat,  Float)  \
		SPECIALIZATION(jdouble, Double) \
		SPECIALIZATION(jint,    Int)    \
		SPECIALIZATION(jlong,   Long)   \
		SPECIALIZATION(jshort,  Short)

/**
 * A template function (specialized for each primitive array type) that
 * answers the size in bytes of an array of the specified length.
 *
 * @param[in] array   the array reference
 * @param[in] length  the length of the array
 * @return the size of the array
 */
template<typename ArrayType> size_t
arraySize(ArrayType array, jint length);

/**
 * A macro that expands to one specialization of the arraySize template.
 *
 * @param[in] Type  the array element type
 * @param[in] Name  the name of the element type
 */
#define SPECIALIZE_ARRAY_SIZE(Type, Name)                      \
		template<> inline size_t                               \
		arraySize<Type##Array>(Type##Array array, jint length) \
		{ return sizeof(Type) * (size_t)length; }

/**
 * Template specializations of arraySize for all primitive array types.
 */
SPECIALIZE_ALL(SPECIALIZE_ARRAY_SIZE)

/**
 * A template function (specialized for each primitive array type) that
 * copies a region of the array into the specified buffer.
 *
 * @param[in] env     the JNI interface pointer
 * @param[in] array   the array reference
 * @param[in] start   the source starting index (inclusive)
 * @param[in] length  the number of elements to be copied
 * @param[in] buffer  the destination buffer
 */
template<typename ArrayType> void
getArrayRegion(JNIEnv * env, ArrayType array, jint start, jint length, void * buffer);

/**
 * A macro that expands to one specialization of the getArrayRegion template.
 *
 * @param[in] Type  the array element type
 * @param[in] Name  the name of the element type
 */
#define SPECIALIZE_GET_ARRAY_REGION(Type, Name)                                                              \
		template<> inline void                                                                               \
		getArrayRegion<Type##Array>(JNIEnv * env, Type##Array array, jint start, jint length, void * buffer) \
		{ env->Get##Name##ArrayRegion(array, start, length, (Type *)buffer); }

/**
 * Template specializations of getArrayRegion for all primitive array types.
 */
SPECIALIZE_ALL(SPECIALIZE_GET_ARRAY_REGION)

/**
 * A template function (specialized for each primitive array type) that
 * sets a region of the array from the specified buffer.
 *
 * @param[in] env     the JNI interface pointer
 * @param[in] array   the array reference
 * @param[in] start   the source starting index (inclusive)
 * @param[in] length  the number of elements to be copied
 * @param[in] buffer  the source buffer
 */
template<typename ArrayType> void
setArrayRegion(JNIEnv * env, ArrayType array, jint start, jint length, void * buffer);

/**
 * A macro that expands to one specialization of the setArrayRegion template.
 *
 * @param[in] Type  the array element type
 * @param[in] Name  the name of the element type
 */
#define SPECIALIZE_SET_ARRAY_REGION(Type, Name)                                                              \
		template<> inline void                                                                               \
		setArrayRegion<Type##Array>(JNIEnv * env, Type##Array array, jint start, jint length, void * buffer) \
		{ env->Set##Name##ArrayRegion(array, start, length, (Type *)buffer); }

/**
 * Template specializations of setArrayRegion for all primitive array types.
 */
SPECIALIZE_ALL(SPECIALIZE_SET_ARRAY_REGION)

/**
 * Copy data from a Java primitive array to a region of device memory.
 * The ArrayType template parameter identifies the type of the Java array.
 * The data are copied to temporary host storage before the final transfer
 * to device memory. The transfer to temporary host storage is required
 * because j9cuda_memcpyHostToDevice may block, in violation of the contract
 * for using GetPrimitiveArrayCritical (were it used instead).
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the source array
 * @param[in] fromIndex  the source starting index (inclusive)
 * @param[in] toIndex    the source ending index (exclusive)
 */
template<typename ArrayType> void
copyFromHost(
		JNIEnv    * env,
		jint        deviceId,
		jlong       devicePtr,
		ArrayType   array,
		jint        fromIndex,
		jint        toIndex)
{
	jint length = toIndex - fromIndex;
	size_t byteCount = arraySize(array, length);

	PORT_ACCESS_FROM_ENV(env);

	void * buffer = J9CUDA_ALLOCATE_MEMORY(byteCount);
	int32_t error = J9CUDA_ERROR_MEMORY_ALLOCATION;

	if (NULL != buffer) {
		getArrayRegion(env, array, fromIndex, length, buffer);

		error = j9cuda_memcpyHostToDevice(
				(uint32_t)deviceId,
				(void *)(uintptr_t)devicePtr,
				buffer,
				byteCount);

		J9CUDA_FREE_MEMORY(buffer);
	}

	if (0 != error) {
		throwCudaException(env, error);
	}
}

/**
 * Copy data to a Java primitive array from a region of device memory.
 * The ArrayType template parameter identifies the type of the Java array.
 * The data are copied to temporary host storage before the final transfer
 * to the array. The transfer to temporary host storage is required because
 * j9cuda_memcpyDeviceToHost may block, in violation of the contract for
 * using GetPrimitiveArrayCritical (were it used instead).
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the target array
 * @param[in] fromIndex  the target starting index (inclusive)
 * @param[in] toIndex    the target ending index (exclusive)
 */
template<typename ArrayType> void
copyToHost(JNIEnv * env, jint deviceId, jlong devicePtr, ArrayType array, jint fromIndex, jint toIndex)
{
	jint length = toIndex - fromIndex;
	size_t byteCount = arraySize(array, length);

	PORT_ACCESS_FROM_ENV(env);

	void * buffer = J9CUDA_ALLOCATE_MEMORY(byteCount);
	int32_t error = J9CUDA_ERROR_MEMORY_ALLOCATION;

	if (NULL != buffer) {
		error = j9cuda_memcpyDeviceToHost(
				(uint32_t)deviceId,
				buffer,
				(void *)(uintptr_t)devicePtr,
				byteCount);

		if (0 == error) {
			setArrayRegion(env, array, fromIndex, length, buffer);
		}

		J9CUDA_FREE_MEMORY(buffer);
	}

	if (0 != error) {
		throwCudaException(env, error);
	}
}

} // namespace

#endif /* OMR_OPT_CUDA */

/**
 * Allocate a block of memory on the specified device.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    allocate
 * Signature: (IJ)J
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] byteCount  the number of bytes requested
 * @return a pointer to the block of storage
 */
jlong JNICALL
Java_com_ibm_cuda_CudaBuffer_allocate
  (JNIEnv * env, jclass, jint deviceId, jlong byteCount)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferAllocate_entry(thread, deviceId, byteCount);

	void * devicePtr = NULL;
	int32_t error = J9CUDA_ERROR_NO_DEVICE;
#ifdef OMR_OPT_CUDA
	PORT_ACCESS_FROM_ENV(env);
	error = j9cuda_deviceAlloc(deviceId, (uintptr_t)byteCount, &devicePtr);
#endif /* OMR_OPT_CUDA */

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_bufferAllocate_exit(thread, devicePtr);

	return (jlong)devicePtr;
}

#ifdef OMR_OPT_CUDA

/**
 * Allocate a direct NIO byte buffer of the specified capacity.
 * The resulting buffer is used only internally within CudaBuffer for transfers
 * involving NIO buffers that are neither direct nor backed by an array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    allocateDirectBuffer
 * Signature: (J)Ljava/nio/ByteBuffer;
 *
 * @param[in] env       the JNI interface pointer
 * @param[in] (unused)  the class pointer
 * @param[in] capacity  the requested buffer capacity in bytes
 * @return a pointer to the byte buffer
 */
jobject JNICALL
Java_com_ibm_cuda_CudaBuffer_allocateDirectBuffer
  (JNIEnv * env, jclass, jlong capacity)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferAllocateDirectBuffer_entry(thread, capacity);

	PORT_ACCESS_FROM_ENV(env);

	void * buffer = J9CUDA_ALLOCATE_MEMORY((size_t)capacity);
	jobject result = NULL;

	if (NULL == buffer) {
		Trc_cuda_bufferAllocateDirectBuffer_allocFail(thread);
		throwCudaException(env, J9CUDA_ERROR_MEMORY_ALLOCATION);
	} else {
		result = env->NewDirectByteBuffer(buffer, capacity);

		if ((NULL == result) || env->ExceptionCheck()) {
			Trc_cuda_bufferAllocateDirectBuffer_newFail(thread);
			J9CUDA_FREE_MEMORY(buffer);
			buffer = NULL;
			result = NULL;
		}
	}

	Trc_cuda_bufferAllocateDirectBuffer_exit(thread, result, buffer);

	return result;
}

/**
 * Copy data to device memory from device memory; the source may be on the same
 * or a different device.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyFromDevice
 * Signature: (IJIJJ)V
 *
 * @param[in] env             the JNI interface pointer
 * @param[in] (unused)        the class pointer
 * @param[in] targetDeviceId  the target device identifier
 * @param[in] targetAddress   the target device pointer
 * @param[in] sourceDeviceId  the source device identifier
 * @param[in] sourceAddress   the source device pointer
 * @param[in] byteCount       the number of bytes to be copied
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyFromDevice
  (JNIEnv * env, jclass, jint targetDeviceId, jlong targetAddress, jint sourceDeviceId, jlong sourceAddress, jlong byteCount)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyFromDevice_entry(
			thread,
			targetDeviceId, (uintptr_t)targetAddress,
			sourceDeviceId, (uintptr_t)sourceAddress, byteCount);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = 0;

	if (targetDeviceId == sourceDeviceId) {
		error = j9cuda_memcpyDeviceToDevice(
				(uint32_t)targetDeviceId,
				(void *)(uintptr_t)targetAddress,
				(const void *)(uintptr_t)sourceAddress,
				(uintptr_t)byteCount);
	} else {
		error = j9cuda_memcpyPeer(
				(uint32_t)targetDeviceId,
				(void *)(uintptr_t)targetAddress,
				(uint32_t)sourceDeviceId,
				(const void *)(uintptr_t)sourceAddress,
				(uintptr_t)byteCount);
	}

	if (0 != error) {
		Trc_cuda_bufferCopyFromDevice_fail(thread, error);
		throwCudaException(env, error);
	}

	Trc_cuda_bufferCopyFromDevice_exit(thread);
}

/**
 * Copy data to device memory from a Java byte array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyFromHostByte
 * Signature: (IJ[BII)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the source array
 * @param[in] fromIndex  the source starting index (inclusive)
 * @param[in] toIndex    the source ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyFromHostByte
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jbyteArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyFromHostByte_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyFromHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyFromHostByte_exit(thread);
}

/**
 * Copy data to device memory from a Java char array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyFromHostChar
 * Signature: (IJ[CII)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the source array
 * @param[in] fromIndex  the source starting index (inclusive)
 * @param[in] toIndex    the source ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyFromHostChar
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jcharArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyFromHostChar_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyFromHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyFromHostChar_exit(thread);
}

/**
 * Copy data to device memory from a direct NIO buffer.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyFromHostDirect
 * Signature: (IJLjava/nio/Buffer;JJ)V
 *
 * @param[in] env         the JNI interface pointer
 * @param[in] (unused)    the class pointer
 * @param[in] deviceId    the device identifier
 * @param[in] devicePtr   the device pointer
 * @param[in] source      the source buffer
 * @param[in] fromOffset  the source starting byte offset (inclusive)
 * @param[in] toOffset    the source ending byte offset (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyFromHostDirect
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jobject source, jlong fromOffset, jlong toOffset)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyFromHostDirect_entry(thread, deviceId, (uintptr_t)devicePtr, source, fromOffset, toOffset);

	jbyte const * buffer = (jbyte const *)env->GetDirectBufferAddress(source);
	int32_t error = J9CUDA_ERROR_OPERATING_SYSTEM;

	if (NULL != buffer) {
		PORT_ACCESS_FROM_ENV(env);

		error = j9cuda_memcpyHostToDevice(
				(uint32_t)deviceId,
				(void *)(uintptr_t)devicePtr,
				&buffer[fromOffset],
				(size_t)(toOffset - fromOffset));
	}

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_bufferCopyFromHostDirect_exit(thread);
}

/**
 * Copy data to device memory from a Java double array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyFromHostDouble
 * Signature: (IJ[DII)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the source array
 * @param[in] fromIndex  the source starting index (inclusive)
 * @param[in] toIndex    the source ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyFromHostDouble
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jdoubleArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyFromHostDouble_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyFromHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyFromHostDouble_exit(thread);
}

/**
 * Copy data to device memory from a Java float array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyFromHostFloat
 * Signature: (IJ[FII)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the source array
 * @param[in] fromIndex  the source starting index (inclusive)
 * @param[in] toIndex    the source ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyFromHostFloat
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jfloatArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyFromHostFloat_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyFromHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyFromHostFloat_exit(thread);
}

/**
 * Copy data to device memory from a Java int array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyFromHostInt
 * Signature: (IJ[III)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the source array
 * @param[in] fromIndex  the source starting index (inclusive)
 * @param[in] toIndex    the source ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyFromHostInt
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jintArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyFromHostInt_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyFromHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyFromHostInt_exit(thread);
}

/**
 * Copy data to device memory from a Java long array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyFromHostLong
 * Signature: (IJ[JII)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the source array
 * @param[in] fromIndex  the source starting index (inclusive)
 * @param[in] toIndex    the source ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyFromHostLong
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jlongArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyFromHostLong_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyFromHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyFromHostLong_exit(thread);
}

/**
 * Copy data to device memory from a Java short array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyFromHostShort
 * Signature: (IJ[SII)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the source array
 * @param[in] fromIndex  the source starting index (inclusive)
 * @param[in] toIndex    the source ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyFromHostShort
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jshortArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyFromHostShort_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyFromHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyFromHostShort_exit(thread);
}

/**
 * Copy data from device memory to a Java byte array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyToHostByte
 * Signature: (IJ[BII)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the target array
 * @param[in] fromIndex  the target starting index (inclusive)
 * @param[in] toIndex    the target ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyToHostByte
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jbyteArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyToHostByte_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyToHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyToHostByte_exit(thread);
}

/**
 * Copy data from device memory to a Java char array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyToHostChar
 * Signature: (IJ[CII)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the target array
 * @param[in] fromIndex  the target starting index (inclusive)
 * @param[in] toIndex    the target ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyToHostChar
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jcharArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyToHostChar_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyToHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyToHostChar_exit(thread);
}

/**
 * Copy data from device memory to a direct NIO buffer.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyToHostDirect
 * Signature: (IJLjava/nio/Buffer;JJ)V
 *
 * @param[in] env         the JNI interface pointer
 * @param[in] (unused)    the class pointer
 * @param[in] deviceId    the device identifier
 * @param[in] devicePtr   the device pointer
 * @param[in] target      the target buffer
 * @param[in] fromOffset  the target starting byte offset (inclusive)
 * @param[in] toOffset    the target ending byte offset (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyToHostDirect
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jobject target, jlong fromOffset, jlong toOffset)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyToHostDirect_entry(thread, deviceId, (uintptr_t)devicePtr, target, fromOffset, toOffset);

	jbyte * buffer = (jbyte *)env->GetDirectBufferAddress(target);
	int32_t error = J9CUDA_ERROR_OPERATING_SYSTEM;

	if (NULL != buffer) {
		PORT_ACCESS_FROM_ENV(env);

		error = j9cuda_memcpyDeviceToHost(
				(uint32_t)deviceId,
				&buffer[fromOffset],
				(void *)(uintptr_t)devicePtr,
				(size_t)(toOffset - fromOffset));
	}

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_bufferCopyToHostDirect_exit(thread);
}

/**
 * Copy data from device memory to a Java double array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyToHostDouble
 * Signature: (IJ[DII)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the target array
 * @param[in] fromIndex  the target starting index (inclusive)
 * @param[in] toIndex    the target ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyToHostDouble
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jdoubleArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyToHostDouble_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyToHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyToHostDouble_exit(thread);
}

/**
 * Copy data from device memory to a Java float array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyToHostFloat
 * Signature: (IJ[FII)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the target array
 * @param[in] fromIndex  the target starting index (inclusive)
 * @param[in] toIndex    the target ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyToHostFloat
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jfloatArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyToHostFloat_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyToHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyToHostFloat_exit(thread);
}

/**
 * Copy data from device memory to a Java int array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyToHostInt
 * Signature: (IJ[III)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the target array
 * @param[in] fromIndex  the target starting index (inclusive)
 * @param[in] toIndex    the target ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyToHostInt
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jintArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyToHostInt_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyToHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyToHostInt_exit(thread);
}

/**
 * Copy data from device memory to a Java long array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyToHostLong
 * Signature: (IJ[JII)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the target array
 * @param[in] fromIndex  the target starting index (inclusive)
 * @param[in] toIndex    the target ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyToHostLong
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jlongArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyToHostLong_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyToHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyToHostLong_exit(thread);
}

/**
 * Copy data from device memory to a Java short array.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    copyToHostShort
 * Signature: (IJ[SII)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] array      the target array
 * @param[in] fromIndex  the target starting index (inclusive)
 * @param[in] toIndex    the target ending index (exclusive)
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_copyToHostShort
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jshortArray array, jint fromIndex, jint toIndex)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferCopyToHostShort_entry(thread, deviceId, (uintptr_t)devicePtr, array, fromIndex, toIndex);

	copyToHost(env, deviceId, devicePtr, array, fromIndex, toIndex);

	Trc_cuda_bufferCopyToHostShort_exit(thread);
}

/**
 * Fill device memory with a repeating value.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    fill
 * Signature: (IJIIJ)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 * @param[in] elemSize   the value size in bytes
 * @param[in] value      the value to be repeated
 * @param[in] count      the number of copies requested
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_fill
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr, jint elemSize, jint value, jlong count)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferFill_entry(thread, deviceId, (uintptr_t)devicePtr, elemSize, value, count);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = J9CUDA_ERROR_INVALID_VALUE;

	switch (elemSize) {
	case 1:
		error = j9cuda_memset8 ((uint32_t)deviceId, (void *)(uintptr_t)devicePtr, (uint8_t) value, (uintptr_t)count);
		break;
	case 2:
		error = j9cuda_memset16((uint32_t)deviceId, (void *)(uintptr_t)devicePtr, (uint16_t)value, (uintptr_t)count);
		break;
	case 4:
		error = j9cuda_memset32((uint32_t)deviceId, (void *)(uintptr_t)devicePtr, (uint32_t)value, (uintptr_t)count);
		break;
	default:
		break;
	}

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_bufferFill_exit(thread);
}

/**
 * Release a direct NIO byte buffer allocated by allocateDirectBuffer.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    allocateDirectBuffer
 * Signature: (J)Ljava/nio/ByteBuffer;
 *
 * @param[in] env         the JNI interface pointer
 * @param[in] (unused)    the class pointer
 * @param[in] byteBuffer  the byte buffer

 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    freeDirectBuffer
 * Signature: (Ljava/nio/Buffer;)V
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_freeDirectBuffer
  (JNIEnv * env, jclass, jobject byteBuffer)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferFreeDirectBuffer_entry(thread, byteBuffer);

	void * buffer = env->GetDirectBufferAddress(byteBuffer);

	if (NULL != buffer) {
		PORT_ACCESS_FROM_ENV(env);

		J9CUDA_FREE_MEMORY(buffer);
	}

	Trc_cuda_bufferFreeDirectBuffer_exit(thread);
}

/**
 * Release a block of memory on the specified device.
 *
 * Class:     com.ibm.cuda.CudaBuffer
 * Method:    release
 * Signature: (IJ)V
 *
 * @param[in] env        the JNI interface pointer
 * @param[in] (unused)   the class pointer
 * @param[in] deviceId   the device identifier
 * @param[in] devicePtr  the device pointer
 */
void JNICALL
Java_com_ibm_cuda_CudaBuffer_release
  (JNIEnv * env, jclass, jint deviceId, jlong devicePtr)
{
	J9VMThread * thread = (J9VMThread *)env;

	Trc_cuda_bufferRelease_entry(thread, deviceId, (uintptr_t)devicePtr);

	PORT_ACCESS_FROM_ENV(env);

	int32_t error = j9cuda_deviceFree(deviceId, (void *)(uintptr_t)devicePtr);

	if (0 != error) {
		throwCudaException(env, error);
	}

	Trc_cuda_bufferRelease_exit(thread);
}

#endif /* OMR_OPT_CUDA */
