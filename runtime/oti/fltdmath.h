/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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


#ifndef fltdmath_h
#define fltdmath_h

#include <math.h>
#include "j9comp.h"

#define DPFRACTION_LENGTH 52
#define DPEXPONENT_LENGTH 11
#define DPSIGN_LENGTH 1
#define DPEXPONENT_BIAS 1023

#define GET_DP_EXPONENT(dblptr) ((HIGH_U32_FROM_DBL_PTR(dblptr) & DOUBLE_EXPONENT_MASK_HI)>>20)
#define SET_DP_EXPONENT(dblptr, e) HIGH_U32_FROM_DBL_PTR(dblptr) = (HIGH_U32_FROM_DBL_PTR(dblptr)&DOUBLE_MANTISSA_MASK_HI) | ((e)<<20)
#define GET_LWDP_EXPONENT(lwptr) ((HIGH_U32_FROM_LONG64_PTR(lwptr) & DOUBLE_EXPONENT_MASK_HI)>>20)
#define SET_LWDP_EXPONENT(lwptr, e) HIGH_U32_FROM_LONG64_PTR(lwptr) = (HIGH_U32_FROM_LONG64_PTR(lwptr)&DOUBLE_MANTISSA_MASK_HI) | ((e)<<20)
#define GET_DP_SIGN(dblptr) ( (HIGH_U32_FROM_DBL_PTR(dblptr) & DOUBLE_SIGN_MASK_HI) != 0  )
#define SET_DP_SIGN(dblptr, s) ( (s) ? (HIGH_U32_FROM_DBL_PTR(dblptr) |= DOUBLE_SIGN_MASK_HI) : (HIGH_U32_FROM_DBL_PTR(dblptr) &= 0x7fffffff))
#define SET_LWDP_SIGN(lwptr, s) ( (s) ? (HIGH_U32_FROM_LONG64_PTR(lwptr) |= DOUBLE_SIGN_MASK_HI) : (HIGH_U32_FROM_LONG64_PTR(lwptr) &= 0x7fffffff) )

/* PR#102641 !!!TBN!!! - 20040419 - Replaced GET_DP_MANTISSA macro and added SET_DP_MANTISSA macro */
#define GET_DP_MANTISSA(dblptr, lwptr) \
	do { \
		LOW_U32_FROM_LONG64_PTR(lwptr) = LOW_U32_FROM_DBL_PTR(dblptr); \
		HIGH_U32_FROM_LONG64_PTR(lwptr) = HIGH_U32_FROM_DBL_PTR(dblptr) & DOUBLE_MANTISSA_MASK_HI; \
	} while(0)

#define SET_DP_MANTISSA(dblptr, lwptr) \
	do { \
		HIGH_U32_FROM_DBL_PTR(dblptr) &= DOUBLE_EXPONENT_MASK_HI; \
		HIGH_U32_FROM_DBL_PTR(dblptr) |= HIGH_U32_FROM_LONG64_PTR(lwptr) & DOUBLE_MANTISSA_MASK_HI; \
		LOW_U32_FROM_DBL_PTR(dblptr) = LOW_U32_FROM_LONG64_PTR(lwptr); \
	} while(0)

#define SET_DPNORMALIZED_BIT(dblptr) HIGH_U32_FROM_DBL_PTR(dblptr) |= 0x00100000
#define CLEAR_DPNORMALIZED_BIT(dblptr) HIGH_U32_FROM_DBL_PTR(dblptr) &= 0xffefffff
#define IS_NORMALIZED_LONG(lwptr) ((HIGH_U32_FROM_LONG64_PTR(lwptr)&0x3ff00000) > 0)
#define SET_LWDPNORMALIZED_BIT(lwptr) HIGH_U32_FROM_LONG64_PTR(lwptr) |= 0x00100000
#define CLEAR_LWDPNORMALIZED_BIT(lwptr) HIGH_U32_FROM_LONG64_PTR(lwptr) &= 0xffefffff

#define DPTRUNCATED_MASK 0x1fffffff
#define GET_TRUNCATED_BITS(dblptr) ( LOW_U32_FROM_DBL_PTR(dblptr) & DPTRUNCATED_MASK )
#define TRUNCATE_LONG_TO_FLOAT(lw)  ( ((HIGH_U32_FROM_LONG64(lw)&0x001fffff) << 3) | ((LOW_U32_FROM_LONG64(lw)&0xe0000000) >> 29) )

#define SPFRACTION_LENGTH 23
#define SPEXPONENT_LENGTH 8
#define SPSIGN_LENGTH 1
#define SPEXPONENT_BIAS 127

#define FLOAT_WORD(f) (*(U_32*)(&f))
#define FLOAT_WORD_P(fp) (*(U_32*)(fp))
#define GET_SP_EXPONENT(f) ( ((*(int *)&f) & 0x7f800000) >>23)
#define SET_SP_EXPONENT(f,e) ( ((*(int *)&f)&0x007fffff) | ((e)<<23))
#define GET_SP_SIGN(f) ( ( (*(int *)&f) & 0x80000000) != 0  )
#define SET_SP_SIGN(f,s) ( (s) ? ( (*(int *)&f) | 0x80000000): ((*(int *)&f) & 0x7fffffff) )
#define SET_SP_SIGN_P(fp,s) ( (s) ? ( (*(int *)fp) | 0x80000000): ((*(int *)fp) & 0x7fffffff) )

#define GET_SP_MANTISSA(f) ( (*(int *)&f) & 0x007fffff )
#define SET_SPNORMALIZED_BIT(i) (*(U_32 *)&i) |= 0x00800000
#define CLEAR_SPNORMALIZED_BIT(i) (i) &= (0xff7fffff)
#define IS_NORMALIZED_INT(i) (((i)&0x7f800000) > 0)

#define IS_SP_ZERO(f) IS_ZERO_SNGL(f)
#define IS_SP_ONE(f) IS_ONE_SNGL(f)
#define IS_SP_INF(f) IS_INF_SNGL(f)
#define IS_SP_POSITIVE(flt) IS_POSITIVE_SNGL(flt)
#define IS_SP_NEGATIVE(flt) IS_NEGATIVE_SNGL(flt)
#define IS_SP_NAN(f) IS_NAN_SNGL(f)
#define IS_SP_NAN_P(fp) IS_NAN_SNGL_PTR(fp)

#define SET_SP_NAN(fp) SET_NAN_SNGL_PTR(fp)
#define SETP_SP_PZERO(fp) SET_PZERO_SNGL_PTR(fp)
#define SETP_SP_NZERO(fp) SET_NZERO_SNGL_PTR(fp)
#define SET_SP_NINF(fp) SET_NINF_SNGL_PTR(fp)
#define SET_SP_PINF(fp) SET_PINF_SNGL_PTR(fp)

#define GET_FLOAT_BITS(dbl) ( ((HIGH_U32_FROM_DBL(dbl) & 0x000fffff) << 3) | ((LOW_U32_FROM_DBL(dbl) & 0xe0000000) >> 29) )
#define SP_TRUNCATION_EXPONENT 23

#define IS_SET(i,m) (((i) & (m)) != 0)
#define IS_CLEAR(i,m) (((i) & (m)) == 0)

#define SET_DP_NAN(dblptr) SET_NAN_DBL_PTR(dblptr)
#define SETP_DP_PZERO(dblptr) SET_PZERO_DBL_PTR(dblptr)
#define SETP_DP_NZERO(dblptr) SET_NZERO_DBL_PTR(dblptr)
#define SET_DP_NINF(dblptr) SET_NINF_DBL_PTR(dblptr)
#define SET_DP_PINF(dblptr) SET_PINF_DBL_PTR(dblptr)

typedef struct canonicalfp_tag {
    I_32    sign;
    I_32    exponent;
    U_64    mantissa;
    U_64    overflow;
} CANONICALFP;

#include "util_api.h"

#endif     /* fltdmath_h */


