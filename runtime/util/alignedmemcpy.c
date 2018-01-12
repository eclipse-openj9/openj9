/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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

#include "j9.h"
#include "util_api.h"


#if defined(_MSC_VER)
#define USE_X86_INTRINSICS
#elif  !defined(WIN32) && defined(J9X86) && defined(__OPTIMIZE__)
#define USE_X86_INTRINSICS
#endif /* !WIN32 && J9X86 && __OPTIMIZE__ */

#if defined(USE_X86_INTRINSICS)
#include <emmintrin.h>
#include <xmmintrin.h>
#elif (defined(J9X86) || (defined(WIN32) && !defined(WIN64)))
typedef long long I_128 __attribute__ ((__vector_size__ (16)));
#endif

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

#else /* 32-bit && !defined(J9X86) */
	double *d = (double *) dest;
	double *s = (double *) source;
	while (0 != count) {
		*d++ = *s++;
		count--;
	}
#endif /* defined(J9VM_ENV_DATA64) */
}

static VMINLINE void copyForwardU32(U_32 *dest, U_32 *source, UDATA count) {
	while (0 != count) {
		*dest++ = *source++;
		count--;
	}
}

static VMINLINE void copyForwardU16(U_16 *dest, U_16 *source, UDATA count) {
	while (0 != count) {
		*dest++ = *source++;
		count--;
	}
}

static VMINLINE void
copyBackwardU64(U_64 *dest, U_64 *source, UDATA count)
{
#if defined(J9VM_ENV_DATA64)
	while (0 != count) {
		*--dest = *--source;
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
			_mm_storeu_si128(--d, _mm_loadu_si128(--s));
#else /* !defined(USE_X86_INTRINSICS) */
			__asm__ __volatile__(
				"movdqu %1, %%xmm0\n"
				"movdqu %%xmm0, %0\n"
				: "=m" (*--d) : "m" (*--s) : "%xmm0");
#endif /* defined(USE_X86_INTRINSICS) */
			halfCount--;
		}
	} else {
		/* Use aligned SSE2 instructions */
		while (0 != halfCount) {
#if defined(USE_X86_INTRINSICS)
			_mm_stream_si128(--d, _mm_load_si128(--s));
#else /* !defined(USE_X86_INTRINSICS) */
			__asm__ __volatile__(
					"movdqa %1, %%xmm0\n"
					"movntdq %%xmm0, %0\n"
					: "=m" (*--d) : "m" (*--s) : "%xmm0");
#endif /* defined(USE_X86_INTRINSICS) */
			halfCount--;
		}
	}

	if (0 != count) {
/* The _mm_storel_epi64 compiler intrinsic is broken on gcc 4.1.2 */
#if (defined(USE_X86_INTRINSICS) && !((4 == __GNUC__) && (1 == __GNUC_MINOR__)))
		d = (__m128i *)((U_64 *)d - 1);
		s = (__m128i *)((U_64 *)s - 1);
		_mm_storel_epi64(d, _mm_loadl_epi64(s));
#else /* !defined(USE_X86_INTRINSICS) || GCC == 4.1 */
		d = (I_128 *)((U_64 *)d - 1);
		s = (I_128 *)((U_64 *)s - 1);
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

#else /* 32-bit && !defined(J9X86) */
	double *d = (double *) dest;
	double *s = (double *) source;
	while (0 != count) {
		*--d = *--s;
		count--;
	}
#endif /* defined(J9VM_ENV_DATA64) */
}

static VMINLINE void copyBackwardU32(U_32 *dest, U_32 *source, UDATA count) {
	while (0 != count) {
		*--dest = *--source;
		count--;
	}
}

static VMINLINE void copyBackwardU16(U_16 *dest, U_16 *source, UDATA count) {
	while (0 != count) {
		*--dest = *--source;
		count--;
	}
}

void alignedMemcpy(J9VMThread *vmStruct, void *dest, void *source, UDATA bytes, UDATA alignment) {
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
		memmove(dest, source, bytes);
		break;
	}
}

void alignedBackwardsMemcpy(J9VMThread *vmStruct, void *dest, void *source, UDATA bytes, UDATA alignment) {
	switch (alignment) {
	case 3:
		copyBackwardU64((U_64 *)dest, (U_64 *)source, bytes >> 3);
		break;
	case 2:
		copyBackwardU32((U_32 *)dest, (U_32 *)source, bytes >> 2);
		break;
	case 1:
		copyBackwardU16((U_16 *)dest, (U_16 *)source, bytes >> 1);
		break;
	default:
		memmove(((U_8 *)dest) - bytes, ((U_8 *)source) - bytes, bytes);
		break;
	}
}

