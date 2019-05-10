/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

#if !defined(ARRAYCOPYHELPERS_HPP_)
#define ARRAYCOPYHELPERS_HPP_

/* Hide the compiler specific bits of this file from DDR since it parses with GCC. */
/* TODO : there should be a better #define for this! */
#if defined(TYPESTUBS_H)
#define ARRAYCOPYHELPERS_STUB
#endif /* defined(TYPESTUBS_H) */

#include "j9.h"
#include "j9protos.h"
#include "j9consts.h"
#include <string.h>


#undef USE_X86_INTRINSICS
#if !defined(ARRAYCOPYHELPERS_STUB)
#if defined(_MSC_VER)
#define USE_X86_INTRINSICS
#elif  !defined(WIN32) && defined(J9X86) && defined(__OPTIMIZE__)
#define USE_X86_INTRINSICS
#endif /* !WIN32 && J9X86 && __OPTIMIZE__ */
#endif /* !defined(ARRAYCOPYHELPERS_STUB) */

#if defined(USE_X86_INTRINSICS)
#include <emmintrin.h>
#include <xmmintrin.h>
#elif (defined(J9X86) || (defined(WIN32) && !defined(WIN64)))
typedef long long I_128 __attribute__ ((__vector_size__ (16)));
#endif

class VM_ArrayCopyHelpers
{
/*
 * Data members
 */
private:
protected:
public:

/*
 * Function members
 */
private:
	static VMINLINE void
	copyForwardU64(U_64 *dest, U_64 *source, UDATA count)
	{
#if defined(J9VM_ENV_DATA64)
		while (0 != count) {
			*dest++ = *source++;
			count--;
		}
#elif (defined(J9X86) || (defined(WIN32) && !defined(WIN64)))

#if defined(USE_X86_INTRINSICS)
		__m128i *d = (__m128i *) dest;
		__m128i *s = (__m128i *) source;
#else
		I_128 *d = (I_128 *) dest;
		I_128 *s = (I_128 *) source;
#endif /* defined(USE_X86_INTRINSICS) */

		UDATA halfCount = count >> 1;
		count -= halfCount * 2;
		/* Check for 16-byte alignment */
		if (0 != (((UDATA)s | (UDATA)d) & 0x0f)) {
			/* Use unaligned SSE2 instructions */
			while (0 != halfCount) {
#if defined(USE_X86_INTRINSICS)
				_mm_storeu_si128(d++, _mm_loadu_si128(s++));
#else /* !defined(USE_X86_INTRINSICS) */
				__asm__ __volatile__(
						"movdqu %1, %%xmm0\n"
						"movdqu %%xmm0, %0\n"
						: "=m" (*d++) : "m" (*s++) : "%xmm0");
#endif /* defined(USE_X86_INTRINSICS) */
				halfCount--;
			}
		} else {
			/* Use aligned SSE2 instructions */
			while (0 != halfCount) {
#if defined(USE_X86_INTRINSICS)
				_mm_stream_si128(d++, _mm_load_si128(s++));
#else /* !defined(USE_X86_INTRINSICS) */
				__asm__ __volatile__(
						"movdqa %1, %%xmm0\n"
						"movntdq %%xmm0, %0\n"
						: "=m" (*d++) : "m" (*s++) : "%xmm0");
#endif /* defined(USE_X86_INTRINSICS) */
				halfCount--;
			}
		}

		if (0 != count) {
/* The _mm_storel_epi64 compiler intrinsic is broken on gcc 4.1.2 */
#if (defined(USE_X86_INTRINSICS) && !((4 == __GNUC__) && (1 == __GNUC_MINOR__)))
			_mm_storel_epi64(d, _mm_loadl_epi64(s));
#else /* !defined(USE_X86_INTRINSICS) || GCC == 4.1 */
			__asm__ __volatile__(
				"movq %1, %%xmm0\n"
				"movq %%xmm0, %0\n"
				: "=m" (*d) : "m" (*s) : "%xmm0");
#endif /* (defined(USE_X86_INTRINSICS) && GCC != 4.1 */
		}
#if defined(USE_X86_INTRINSICS)
		_mm_sfence();
#else /* !defined(USE_X86_INTRINSICS) */
		__asm__ __volatile__("sfence\n");
#endif /* defined(USE_X86_INTRINSICS) */

#else /* 32-bit && x86 */
		double *d = (double *) dest;
		double *s = (double *) source;
		while (0 != count) {
			*d++ = *s++;
			count--;
		}
#endif /* defined(J9VM_ENV_DATA64) */
	}

	static VMINLINE void
	copyForwardU32(U_32 *dest, U_32 *source, UDATA count)
	{
		while (0 != count) {
			*dest++ = *source++;
			count--;
		}
	}

	static VMINLINE void
	copyForwardU16(U_16 *dest, U_16 *source, UDATA count)
	{
		while (0 != count) {
			*dest++ = *source++;
			count--;
		}
	}

	static VMINLINE void
	copyForwardU8(U_8 *dest, U_8 *source, UDATA count)
	{
		/* Use memmove instead of memcpy because memcpy does not work on Ubuntu 10.04 */
		memmove((void *)dest, (void *)source, count);
	}
	
	static VMINLINE void
	inlineAlignedMemcpy(void *dest, void *source, UDATA bytes, UDATA alignment)
	{
		switch (alignment) {
		case 3:
			copyForwardU64((U_64 *)dest, (U_64 *)source, bytes >> 3);
			break;
		case 2:
			copyForwardU32((U_32 *)dest, (U_32 *)source, bytes >> 2);
			break;
		case 1:
			copyForwardU16((U_16 *)dest, (U_16 *)source, bytes >> 1);
			break;
		default:
			copyForwardU8((U_8 *)dest, (U_8 *)source, bytes);
			break;
		}
	}

	static VMINLINE UDATA
	calculateElementsToCopyFromCurrentLeafForwardCopy(UDATA arrayletLeafSizeInElements, UDATA index)
	{
		UDATA leaf = index / arrayletLeafSizeInElements;
		UDATA leafIndex = leaf * arrayletLeafSizeInElements;
		UDATA leafCount = arrayletLeafSizeInElements - (index - leafIndex);
		return leafCount;
	}

	static VMINLINE UDATA
	calculateElementsToCopyFromCurrentLeafBackwardCopy(UDATA arrayletLeafSizeInElements, UDATA index)
	{
		UDATA leaf = index / arrayletLeafSizeInElements;
		UDATA leafIndex = leaf * arrayletLeafSizeInElements;
		UDATA leafCount = index - leafIndex;
		if (leafCount == 0) {
			/* if there are 0 elements left in this leaf just answer arrayleaf size in elements */
			leafCount = arrayletLeafSizeInElements;
		}
		return leafCount;
	}

	static VMINLINE bool
	isContiguousRange(J9VMThread *currentThread, UDATA index, UDATA count, UDATA logElementSize)
	{
		UDATA arrayletLeafSizeInElements = currentThread->javaVM->arrayletLeafSize;
		if (0 != logElementSize) {
			arrayletLeafSizeInElements = arrayletLeafSizeInElements >> logElementSize;
		}

		UDATA lastIndex = index;
		lastIndex += count;
		lastIndex -= 1;

		lastIndex ^= index;

		return lastIndex < arrayletLeafSizeInElements;
	}

	static VMINLINE void
	primitiveDiscontiguousBackwardArrayCopy(J9VMThread *currentThread, j9object_t srcObject, UDATA srcStart, j9object_t destObject, UDATA destStart, UDATA elementCount, UDATA arrayletLeafSizeInElements, UDATA logElementSize)
	{
		UDATA srcEndIndex = srcStart + elementCount;
		UDATA destEndIndex = destStart + elementCount;
		UDATA elementsToCopy = elementCount;

		while (elementsToCopy > 0) {
			/* calculate SRC information */
			UDATA srcLeafCount = calculateElementsToCopyFromCurrentLeafBackwardCopy(arrayletLeafSizeInElements, srcEndIndex);
			/* calculate DEST information */
			UDATA destLeafCount = calculateElementsToCopyFromCurrentLeafBackwardCopy(arrayletLeafSizeInElements, destEndIndex);

			/* Choose the smallest of the two since that is the most that can be copied this time through the loop */
			UDATA copyCount = srcLeafCount;
			if (copyCount > destLeafCount) {
				copyCount = destLeafCount;
			}
			/* limit copyCount to the number of elements left to copy */
			if (copyCount > elementsToCopy) {
				copyCount = elementsToCopy;
			}

			void *srcAddress = NULL;
			void *destAddress = NULL;
			UDATA bytesToCopy = 0;

			/* Calculate the srcAddress, destAddress and bytesToCopy based on the type of array being copied */
			switch(logElementSize) {
			case 3:
				srcAddress = J9JAVAARRAY_EA(currentThread, srcObject, srcEndIndex - 1, U_64) + 1;
				destAddress = J9JAVAARRAY_EA(currentThread, destObject, destEndIndex - 1, U_64) + 1;

				bytesToCopy = sizeof(U_64) * copyCount;
				break;
			case 2:
				srcAddress = J9JAVAARRAY_EA(currentThread, srcObject, srcEndIndex - 1, U_32) + 1;
				destAddress = J9JAVAARRAY_EA(currentThread, destObject, destEndIndex - 1, U_32) + 1;

				bytesToCopy = sizeof(U_32) * copyCount;
				break;
			case 1:
				srcAddress = J9JAVAARRAY_EA(currentThread, srcObject, srcEndIndex - 1, U_16) + 1;
				destAddress = J9JAVAARRAY_EA(currentThread, destObject, destEndIndex - 1, U_16) + 1;

				bytesToCopy = sizeof(U_16) * copyCount;
				break;
			default:
				srcAddress = J9JAVAARRAY_EA(currentThread, srcObject, srcEndIndex - 1, U_8) + 1;
				destAddress = J9JAVAARRAY_EA(currentThread, destObject, destEndIndex - 1, U_8) + 1;

				bytesToCopy = sizeof(U_8) * copyCount;
				break;
			}

			alignedBackwardsMemcpy(currentThread, destAddress, srcAddress, bytesToCopy, logElementSize);

			srcEndIndex -= copyCount;
			destEndIndex -= copyCount;
			elementsToCopy -= copyCount;
		}
	}

	static VMINLINE void
	primitiveDiscontiguousForwardArrayCopy(J9VMThread *currentThread, j9object_t srcObject, UDATA srcStart, j9object_t destObject, UDATA destStart, UDATA elementCount, UDATA arrayletLeafSizeInElements, UDATA logElementSize)
	{
		UDATA srcIndex = srcStart;
		UDATA destIndex = destStart;
		UDATA elementsToCopy = elementCount;

		while (elementsToCopy > 0) {
			/* calculate SRC information */
			UDATA srcLeafCount = calculateElementsToCopyFromCurrentLeafForwardCopy(arrayletLeafSizeInElements, srcIndex);
			/* calculate DEST information */
			UDATA destLeafCount = calculateElementsToCopyFromCurrentLeafForwardCopy(arrayletLeafSizeInElements, destIndex);

			/* Choose the smallest of the two counts since that is the most that can be copied this time through the loop */
			UDATA copyCount = srcLeafCount;
			if (copyCount > destLeafCount) {
				copyCount = destLeafCount;
			}
			/* limit copyCount to the number of elements left to copy */
			if (copyCount > elementsToCopy) {
				copyCount = elementsToCopy;
			}

			void *srcAddress = NULL;
			void *destAddress = NULL;
			UDATA bytesToCopy = 0;

			/* Calculate the srcAddress, destAddress and bytesToCopy based on the type of array being copied */
			switch(logElementSize) {
			case 3:
				srcAddress = J9JAVAARRAY_EA(currentThread, srcObject, srcIndex, U_64);
				destAddress = J9JAVAARRAY_EA(currentThread, destObject, destIndex, U_64);

				bytesToCopy = sizeof(U_64) * copyCount;
				break;
			case 2:
				srcAddress = J9JAVAARRAY_EA(currentThread, srcObject, srcIndex, U_32);
				destAddress = J9JAVAARRAY_EA(currentThread, destObject, destIndex, U_32);

				bytesToCopy = sizeof(U_32) * copyCount;
				break;
			case 1:
				srcAddress = J9JAVAARRAY_EA(currentThread, srcObject, srcIndex, U_16);
				destAddress = J9JAVAARRAY_EA(currentThread, destObject, destIndex, U_16);

				bytesToCopy = sizeof(U_16) * copyCount;
				break;
			default:
				srcAddress = J9JAVAARRAY_EA(currentThread, srcObject, srcIndex, U_8);
				destAddress = J9JAVAARRAY_EA(currentThread, destObject, destIndex, U_8);

				bytesToCopy = sizeof(U_8) * copyCount;
				break;
			}

			inlineAlignedMemcpy(destAddress, srcAddress, bytesToCopy, logElementSize);

			srcIndex += copyCount;
			destIndex += copyCount;
			elementsToCopy -= copyCount;
		}
	}

	static VMINLINE void
	primitiveContiguousArrayCopy(J9VMThread *currentThread, j9object_t srcObject, UDATA srcStart, j9object_t destObject, UDATA destStart, UDATA elementCount, UDATA logElementSize)
	{
		void *srcAddress = NULL;
		void *destAddress = NULL;
		UDATA bytesToCopy = 0;

		/* Calculate the srcAddress, destAddress and bytesToCopy based on the type of array being copied */
		switch(logElementSize) {
		case 3:
			srcAddress = J9JAVAARRAY_EA(currentThread, srcObject, srcStart, U_64);
			destAddress = J9JAVAARRAY_EA(currentThread, destObject, destStart, U_64);

			bytesToCopy = sizeof(U_64) * elementCount;
			break;
		case 2:
			srcAddress = J9JAVAARRAY_EA(currentThread, srcObject, srcStart, U_32);
			destAddress = J9JAVAARRAY_EA(currentThread, destObject, destStart, U_32);

			bytesToCopy = sizeof(U_32) * elementCount;
			break;
		case 1:
			srcAddress = J9JAVAARRAY_EA(currentThread, srcObject, srcStart, U_16);
			destAddress = J9JAVAARRAY_EA(currentThread, destObject, destStart, U_16);

			bytesToCopy = sizeof(U_16) * elementCount;
			break;
		default:
			srcAddress = J9JAVAARRAY_EA(currentThread, srcObject, srcStart, U_8);
			destAddress = J9JAVAARRAY_EA(currentThread, destObject, destStart, U_8);

			bytesToCopy = sizeof(U_8) * elementCount;
			break;
		}

		if ((srcObject != destObject) || (destAddress <= srcAddress)) {
			/* Do a forward copy */
			inlineAlignedMemcpy(destAddress, srcAddress, bytesToCopy, logElementSize);
		} else {
			/* Do a backward copy */
			alignedBackwardsMemcpy(currentThread, (U_8*)destAddress + bytesToCopy, (U_8*)srcAddress + bytesToCopy, bytesToCopy, logElementSize);
		}
	}

	static VMINLINE void
	primitiveDiscontiguousArrayCopy(J9VMThread * currentThread, j9object_t srcObject, UDATA srcStart, j9object_t destObject, UDATA destStart, UDATA elementCount, UDATA logElementSize)
	{
		UDATA arrayletLeafSizeInElements = currentThread->javaVM->arrayletLeafSize >> logElementSize;

		if ((srcObject != destObject) || (destStart <= srcStart)) {
			/* Do a forward copy */
			primitiveDiscontiguousForwardArrayCopy(currentThread, srcObject, srcStart, destObject, destStart, elementCount, arrayletLeafSizeInElements, logElementSize);
		} else {
			/* Do a backward copy */
			primitiveDiscontiguousBackwardArrayCopy(currentThread, srcObject, srcStart, destObject, destStart, elementCount, arrayletLeafSizeInElements, logElementSize);
		}
	}

	static VMINLINE void
	primitiveContiguousArrayFill(J9VMThread *currentThread, j9object_t object, UDATA start, UDATA elementCount, U_8 value)
	{
		memset(J9JAVAARRAY_EA(currentThread, object, start, U_8), value, elementCount);
	}

	static VMINLINE void
	primitiveDiscontiguousArrayFill(J9VMThread *currentThread, j9object_t object, UDATA start, UDATA elementCount, U_8 value)
	{
		UDATA index = start;
		UDATA elementsToSet = elementCount;
		UDATA arrayletLeafSizeInBytes = currentThread->javaVM->arrayletLeafSize;

		while (elementsToSet > 0) {
			/* calculate leaf information */
			UDATA setCount = calculateElementsToCopyFromCurrentLeafForwardCopy(arrayletLeafSizeInBytes, index);
			/* limit setCount to the number of elements left to set */
			if (setCount > elementsToSet) {
				setCount = elementsToSet;
			}
			memset(J9JAVAARRAY_EA(currentThread, object, index, U_8), value, setCount);
			index += setCount;
			elementsToSet -= setCount;
		}
	}

	static VMINLINE void
	memcpyToOrFromArrayContiguous(J9VMThread *currentThread, j9object_t array, UDATA index, UDATA count, void *nativePtr, UDATA logElementSize, bool from)
	{
		void *arrayAddress = NULL;
		UDATA bytesToCopy = 0;

		/* Calculate the srcAddress, destAddress and bytesToCopy based on the type of array being copied */
		switch(logElementSize) {
		case 3:
			arrayAddress = J9JAVAARRAY_EA(currentThread, array, index, U_64);
			bytesToCopy = sizeof(U_64) * count;
			break;
		case 2:
			arrayAddress = J9JAVAARRAY_EA(currentThread, array, index, U_32);
			bytesToCopy = sizeof(U_32) * count;
			break;
		case 1:
			arrayAddress = J9JAVAARRAY_EA(currentThread, array, index, U_16);
			bytesToCopy = sizeof(U_16) * count;
			break;
		default:
			arrayAddress = J9JAVAARRAY_EA(currentThread, array, index, U_8);
			bytesToCopy = sizeof(U_8) * count;
			break;
		}

		if (from) {
			inlineAlignedMemcpy(nativePtr, arrayAddress, bytesToCopy, logElementSize);
		} else {
			inlineAlignedMemcpy(arrayAddress, nativePtr, bytesToCopy, logElementSize);
		}
	}

	static VMINLINE void
	memcpyToOrFromArrayDiscontiguous(J9VMThread *currentThread, j9object_t array, UDATA index, UDATA count, void *nativePtr, UDATA logElementSize, bool from)
	{
		UDATA elementsToCopy = count;
		UDATA arrayIndex = index;
		void *nativeAddress = nativePtr;
		UDATA arrayletLeafSizeInElements = currentThread->javaVM->arrayletLeafSize >> logElementSize;

		while (elementsToCopy > 0) {
			/* calculate SRC information */
			UDATA copyCount = calculateElementsToCopyFromCurrentLeafForwardCopy(arrayletLeafSizeInElements, arrayIndex);

			/* limit copyCount to the number of elements left to copy */
			if (copyCount > elementsToCopy) {
				copyCount = elementsToCopy;
			}

			void *arrayAddress = NULL;
			UDATA bytesToCopy = 0;

			/* Calculate the srcAddress, destAddress and bytesToCopy based on the type of array being copied */
			switch(logElementSize) {
			case 3:
				arrayAddress = J9JAVAARRAY_EA(currentThread, array, arrayIndex, U_64);
				bytesToCopy = sizeof(U_64) * copyCount;
				break;
			case 2:
				arrayAddress = J9JAVAARRAY_EA(currentThread, array, arrayIndex, U_32);
				bytesToCopy = sizeof(U_32) * copyCount;
				break;
			case 1:
				arrayAddress = J9JAVAARRAY_EA(currentThread, array, arrayIndex, U_16);
				bytesToCopy = sizeof(U_16) * copyCount;
				break;
			default:
				arrayAddress = J9JAVAARRAY_EA(currentThread, array, arrayIndex, U_8);
				bytesToCopy = sizeof(U_8) * copyCount;
				break;
			}

			if (from) {
				inlineAlignedMemcpy(nativeAddress, arrayAddress, bytesToCopy, logElementSize);
			} else {
				inlineAlignedMemcpy(arrayAddress, nativeAddress, bytesToCopy, logElementSize);
			}

			arrayIndex += copyCount;
			elementsToCopy -= copyCount;
			nativeAddress = (void *)((UDATA)nativeAddress + bytesToCopy);
		}
	}

	static VMINLINE void
	memcpyToArrayHelper(J9VMThread *currentThread, j9object_t array, UDATA index, UDATA count, void *srcPtr, UDATA logElementSize)
	{
		if (isContiguousRange(currentThread, index, count, logElementSize)) {
			memcpyToOrFromArrayContiguous(currentThread, array, index, count, srcPtr, logElementSize, false);
		} else {
			memcpyToOrFromArrayDiscontiguous(currentThread, array, index, count, srcPtr, logElementSize, false);
		}
	}

	static VMINLINE void
	memcpyFromArrayHelper(J9VMThread *currentThread, j9object_t array, UDATA index, UDATA count, void *destPtr, UDATA logElementSize)
	{
		if (isContiguousRange(currentThread, index, count, logElementSize)) {
			memcpyToOrFromArrayContiguous(currentThread, array, index, count, destPtr, logElementSize, true);
		} else {
			memcpyToOrFromArrayDiscontiguous(currentThread, array, index, count, destPtr, logElementSize, true);
		}
	}

protected:
public:
	/**
	 * Perform a memcpy to an object array from the native array
	 *
	 * @param currentThread - The thread performing the arraycopy
	 * @param array - the object array to copy into
	 * @param logElementSize - the log of the elementSize to copy the array elements in
	 * @param index - the index to start the copy from
	 * @param count - the number of elements to copy
	 * @param destPtr - the address of the native array to copy from
	 *
	 */
	static VMINLINE void
	memcpyToArray(J9VMThread *currentThread, j9object_t array, UDATA logElementSize, UDATA index, UDATA count, void *srcPtr)
	{
		memcpyToArrayHelper(currentThread, array, index, count, srcPtr, logElementSize);
	}

	/**
	 * Perform a memcpy to an object array from the native array
	 *
	 * @param currentThread - The thread performing the arraycopy
	 * @param array - the object array to copy into
	 * @param clazz - the class to treat array as for the copy
	 * @param index - the index to start the copy from
	 * @param count - the number of elements to copy
	 * @param destPtr - the address of the native array to copy from
	 *
	 */
	static VMINLINE void
	memcpyToArray(J9VMThread *currentThread, j9object_t array, J9Class *clazz, UDATA index, UDATA count, void *srcPtr)
	{
		UDATA logElementSize = (((J9ROMArrayClass*)clazz->romClass)->arrayShape & 0x0000FFFF);

		memcpyToArrayHelper(currentThread, array, index, count, srcPtr, logElementSize);
	}

	/**
	 * Perform a memcpy to an object array from the native array
	 *
	 * @param currentThread - The thread performing the arraycopy
	 * @param array - the object array to copy into
	 * @param index - the index to start the copy from
	 * @param count - the number of elements to copy
	 * @param destPtr - the address of the native array to copy from
	 *
	 */
	static VMINLINE void
	memcpyToArray(J9VMThread *currentThread, j9object_t array, UDATA index, UDATA count, void *srcPtr)
	{
		J9Class *objectClass = J9OBJECT_CLAZZ(currentThread, array);
		UDATA logElementSize = (((J9ROMArrayClass*)objectClass->romClass)->arrayShape & 0x0000FFFF);

		memcpyToArrayHelper(currentThread, array, index, count, srcPtr, logElementSize);
	}

	/**
	 * Perform a memcpy from an object array to the native array
	 *
	 * @param currentThread - The thread performing the arraycopy
	 * @param array - the object array to copy from
	 * @param logElementSize - the log of the elementSize to copy the array elements in
	 * @param index - the index to start the copy from
	 * @param count - the number of elements to copy
	 * @param destPtr - the address of the native array to copy into
	 *
	 */
	static VMINLINE void
	memcpyFromArray(J9VMThread *currentThread, j9object_t array, UDATA logElementSize, UDATA index, UDATA count, void *destPtr)
	{
		memcpyFromArrayHelper(currentThread, array, index, count, destPtr, logElementSize);
	}

	/**
	 * Perform a memcpy from an object array to the native array
	 *
	 * @param currentThread - The thread performing the arraycopy
	 * @param array - the object array to copy from
	 * @param clazz - the class to treat array as for the copy
	 * @param index - the index to start the copy from
	 * @param count - the number of elements to copy
	 * @param destPtr - the address of the native array to copy into
	 *
	 */
	static VMINLINE void
	memcpyFromArray(J9VMThread *currentThread, j9object_t array, J9Class *clazz, UDATA index, UDATA count, void *destPtr)
	{
		UDATA logElementSize = (((J9ROMArrayClass*)clazz->romClass)->arrayShape & 0x0000FFFF);

		memcpyFromArrayHelper(currentThread, array, index, count, destPtr, logElementSize);
	}

	/**
	 * Perform a memcpy from an object array to the native array
	 *
	 * @param currentThread - The thread performing the arraycopy
	 * @param array - the object array to copy from
	 * @param index - the index to start the copy from
	 * @param count - the number of elements to copy
	 * @param destPtr - the address of the native array to copy into
	 *
	 */
	static VMINLINE void
	memcpyFromArray(J9VMThread *currentThread, j9object_t array, UDATA index, UDATA count, void *destPtr)
	{
		J9Class *objectClass = J9OBJECT_CLAZZ(currentThread, array);
		UDATA logElementSize = (((J9ROMArrayClass*)objectClass->romClass)->arrayShape & 0x0000FFFF);

		memcpyFromArrayHelper(currentThread, array, index, count, destPtr, logElementSize);
	}

	/**
	 * Perform an arraycopy on the reference arrays provided.
	 * Copy elementCount elements from srcObject starting at srcStart to destObject starting at destStart
	 *
	 * @param currentThread - The thread performing the arraycopy
	 * @param srcObject - the source object for the array copy
	 * @param srcStart - the start index to copy elements from srcObject
	 * @param destObject - the destination object for the array copy
	 * @param destStart - the start index to copy elements into destObject
	 * @param elementCount - the number of elements to copy
	 *
	 * @return -1 if the copy succeeded, else the index where the ArrayStoreException occurred
	 *
	 */
	static VMINLINE I_32
	referenceArrayCopy(J9VMThread * currentThread, j9object_t srcObject, I_32 srcStart, j9object_t destObject, I_32 destStart, I_32 elementCount)
	{
		return currentThread->javaVM->memoryManagerFunctions->referenceArrayCopyIndex(currentThread, (J9IndexableObject *)srcObject, (J9IndexableObject *)destObject, srcStart, destStart, elementCount);
	}

	/**
	 * Perform an arraycopy on the primitive arrays provided.
	 * Copy elementCount elements from srcObject starting at srcStart to destObject starting at destStart
	 *
	 * @param currentThread - The thread performing the arraycopy
	 * @param srcObject - the source object for the array copy
	 * @param srcStart - the start index to copy elements from srcObject
	 * @param destObject - the destination object for the array copy
	 * @param destStart - the start index to copy elements into destObject
	 * @param elementCount - the number of elements to copy
	 * @param logElementSize - The log of the size of the elements contained in the array
	 *
	 */
	static VMINLINE void
	primitiveArrayCopy(J9VMThread * currentThread, j9object_t srcObject, UDATA srcStart, j9object_t destObject, UDATA destStart, UDATA elementCount, UDATA logElementSize)
	{
		if (isContiguousRange(currentThread, srcStart, elementCount, logElementSize) && isContiguousRange(currentThread, destStart, elementCount, logElementSize)) {
			primitiveContiguousArrayCopy(currentThread, srcObject, srcStart, destObject, destStart, elementCount, logElementSize);
		} else {
			primitiveDiscontiguousArrayCopy(currentThread, srcObject, srcStart, destObject, destStart, elementCount, logElementSize);
		}
	}

	/**
	 * Sets a range of bytes in a primitive array to a value.
	 *
	 * @param currentThread - The thread performing the arraycopy
	 * @param object - the primitive array to fill
	 * @param start - the start index
	 * @param elementCount - the number of elements to fill
	 * @param value - the value to set
	 *
	 */
	static VMINLINE void
	primitiveArrayFill(J9VMThread * currentThread, j9object_t object, UDATA start, UDATA elementCount, U_8 value)
	{
		if (isContiguousRange(currentThread, start, elementCount, 0)) {
			primitiveContiguousArrayFill(currentThread, object, start, elementCount, value);
		} else {
			primitiveDiscontiguousArrayFill(currentThread, object, start, elementCount, value);
		}
	}

	/**
	 * Check to see if the arraycopy range is within the array.
	 * startIndex can not be less than 0 or larger than the size of the array.
	 * startIndex + length has to be less than or equal to the size of the array
	 *
	 * @param currentThread - The thread performing the arraycopy
	 * @param array - array taking part in the arraycopy
	 * @param startIndex - the start index for the arraycopy
	 * @param copyCount - the number of elements to copy
	 * @param[out] invalidIndex - on failure, set to the index to use for the AIOB
	 *
	 * @return true if the range is valid or false if invalid (and set invalidIndex)
	 */
	static VMINLINE bool
	rangeCheck(J9VMThread * currentThread, j9object_t array, I_32 startIndex, I_32 copyCount, I_32 *invalidIndex)
	{
		bool inRange = true;

		if (startIndex < 0) {
			inRange = false;
			*invalidIndex = startIndex;
		} else {
			I_32 size = J9INDEXABLEOBJECT_SIZE(currentThread, array);
			if (startIndex > size) {
				inRange = false;
				*invalidIndex = startIndex;
			} else if (copyCount > (size - startIndex)) {
				inRange = false;
				*invalidIndex = size;
			}
		}
		return inRange;
	}
};

#endif /* ARRAYCOPYHELPERS_HPP_ */
