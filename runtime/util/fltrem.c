/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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

#include <stdio.h>
#include <math.h>

#include <float.h>

#include "j9port.h"
#include "j9comp.h"
#include "j9protos.h"
#include "fltconst.h"
#include "fltdmath.h"
#include "util_internal.h"

#if (defined(J9VM_INTERP_FLOAT_SUPPORT))  /* File Level Build Flags */

/* The following is built to behave as the JAVA drem bytecode is expected to.
 */
jdouble 
helperCDoubleRemainderDouble(jdouble a, jdouble b)
{
	jdouble result = 0;

	/* 
	 * If either value is NaN, the result is NaN. 
	 */
	 if (IS_NAN_DBL(a) || IS_NAN_DBL(b)) {
		SET_NAN_DBL_PTR(&result);
		return result;
	}

	/* If the dividend is an infinity, or the divisor is a zero, 
	 * or both, the result is NaN. 
	 */
	if (IS_INF_DBL(a) || IS_ZERO_DBL(b)) {
		SET_NAN_DBL_PTR(&result);
		return result;
	}

	/* If the dividend is finite and the divisor is an infinity, 
	 * the result equals the dividend. 
	 */
	if (IS_INF_DBL(b)) {
		return a;
		
	}

	/* If the dividend is a zero and the divisor is finite, 
	 * the result equals the dividend. 
	 */
	if (IS_ZERO_DBL(a)) {
		return a;
		
	}

	/* 
	 * If the divisor is denormal, then the result equals zero  
	 * with the same sign as the dividend. 
	 */
	if( IS_DENORMAL_DBL(b) ) {
		if( IS_POSITIVE_DBL_PTR(&a) ) {
			SETP_DP_PZERO(&result);
		} else {
			SETP_DP_NZERO(&result);
		}
		return result;            
	}

	/* In the remaining cases, (JVM spec page 193).... 
	 */

#if defined(ARMGNU)
	remDD(a, b, &result);
	return result;
#else
#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT)
	if (IS_DENORMAL_DBL(a) || IS_DENORMAL_DBL(b)) {
		remDD(a, b, &result);
		return result;
	}
#endif

#if defined(AIXPPC)
		
		/* This platform has problems with the case: (denormalNum % numWithBiggestExponent). 
		 */
		if (fabs(a) < fabs(b)) {
			result = fabs(a);
		} else {
			result = fabs(fmod(a, b));
		}
#else
		result = fabs(fmod(a, b));
#endif

		/* Swizzle the sign of the result based on the
		 * 1.3 spec on 15.17.3 Remainder Operator % which
		 * says the sign of the remainder will be the same
		 * as the dividend
		 */
		if (IS_NEGATIVE_DBL(a)) {
			HIGH_U32_FROM_DBL(result) |= DOUBLE_SIGN_MASK_HI;
		}

#endif /* defined(ARMGNU) */

	return result;
}


I_32 
helperDoubleRemainderDouble(jdouble *a, jdouble *b, jdouble *c)
{
	*c = helperCDoubleRemainderDouble(*a, *b);
	return RETURN_FINITE;
}


/* The following is built to behave as the JAVA frem bytecode is expected to.
 */
jfloat 
helperCFloatRemainderFloat(jfloat a, jfloat b)
{
	jfloat result;

	/* If either value is NaN then the result is NaN. 
	 */
	if (IS_NAN_SNGL(a) || IS_NAN_SNGL(b)) {
		SET_NAN_SNGL_PTR(&result);
		return result;
	}

	/* If the dividend is an infinity, or the divisor is a zero, 
	 * or both, the result is NaN. 
	 */
	if (IS_INF_SNGL(a) || IS_ZERO_SNGL(b)) {
		SET_NAN_SNGL_PTR(&result);
		return result;
	}

	/* If the dividend is finite and the divisor is an infinity, 
	 * the result equals the dividend. 
	 */
	if (IS_INF_SNGL(b)) {
		return a;
	}

	/* If the dividend is a zero and the divisor is finite,
	 * the result equals the dividend. 
	 */
	if (IS_ZERO_SNGL(a)) {
		return a;
	}

	/* In the remaining cases, (JVM spec page 193).... 
	 */
#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT)
	if (IS_DENORMAL_SNGL(a) || IS_DENORMAL_SNGL(b)) {
		remDF(a, b, &result);
		return result;
	}
#endif

#if defined(AIXPPC)
		
		/* This platform has problems with the case: (denormalNum % numWithBiggestExponent). 
		 */
		if (fabs(a) < fabs(b)) {
			result = (jfloat) fabs((jdouble)a);
		} else {
			result = (jfloat) fabs(fmod((jdouble)a, (jdouble)b));
		}
#else 
		result = (jfloat) fabs(fmod((jdouble)a, (jdouble)b));
#endif

		/* Swizzle the sign of the result based on the
		 * 1.3 spec on 15.17.3 Remainder Operator % which
		 * says the sign of the remainder will be the same
		 * as the dividend
		 */
		if (IS_NEGATIVE_SNGL(a)) {
			*U32P(&result) |= SINGLE_SIGN_MASK;
		}

	return result;
}


I_32 
helperFloatRemainderFloat(jfloat *a, jfloat *b, jfloat *c)
{
	jfloat tmpA, tmpB, tmpC;
    
	PTR_SINGLE_VALUE(a, &tmpA);
	PTR_SINGLE_VALUE(b, &tmpB);
	tmpC = helperCFloatRemainderFloat(tmpA, tmpB);
	PTR_SINGLE_STORE(c, &tmpC);

	return RETURN_FINITE;
}

#endif /* J9VM_INTERP_FLOAT_SUPPORT */ /* End File Level Build Flags */


