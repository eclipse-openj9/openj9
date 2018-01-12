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

#if defined(J9ZTPF)
#define J9_NO_DENORMAL_FLOAT_SUPPORT 1
#endif


I_32
helperCDoubleCompareDouble(jdouble a, jdouble b)
{
	I_32 result;
	
#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT) && defined(ARMGNU))
    if (IS_DENORMAL_DBL(a) || IS_DENORMAL_DBL(b)) {
		return compareDD(a, b);
	}
#endif

	if(IS_NAN_DBL(a) || IS_NAN_DBL(b)) {
		result = -2;
	} else {
		if(a > b) {
			result = 1;
		} else {
			if(a < b) {
				result = -1;
			} else
				result = 0;
		}
	}
	return result;
}


int 
helperDoubleCompareDouble(jdouble *a, jdouble *b)
{
	return helperCDoubleCompareDouble(*a, *b);
}


jdouble 
helperCDoubleDivideDouble(jdouble a, jdouble b)
{
	jdouble result;

#if defined(ARMGNU)&&defined(HARDHAT)
	if (IS_NAN_DBL(a) || IS_NAN_DBL(b))  {
		SET_NAN_DBL_PTR(&result);	
		return result;
	}
#endif

#if defined(ARMGNU) || defined(J9ZTPF)
	divideDD(a, b, &result);
#else
#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT)&&defined(LINUXPPC))
 	if (IS_DENORMAL_DBL(a) || IS_DENORMAL_DBL(b)) {
		divideDD(a, b,&result);
	} else 
#endif
	{
		result = a / b;
	}

#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT)&&(defined(ARMGNU)||defined(LINUXPPC)))
 	if (IS_ZERO_DBL(result) && IS_FINITE_DBL(a) && IS_FINITE_DBL(b) && !IS_ZERO_DBL(a)) {
		divideDD(a, b,&result);
	}
#if defined(HARDHAT)&&(defined(ARMGNU)||defined(LINUXPPC))
	else {
		if(IS_DENORMAL_DBL(result) && !IS_DENORMAL_DBL(a) && !IS_DENORMAL_DBL(b)) {
			divideDD(a, b,&result);
		}
	}
#endif
#endif
#endif /* defined(ARMGNU) */

	return result;
}


I_32 
helperDoubleDivideDouble(jdouble *a, jdouble *b, jdouble *c)
{
	*c = helperCDoubleDivideDouble(*a, *b);
	return 0;
}


jdouble 
helperCDoubleMinusDouble(jdouble a, jdouble b)
{
	jdouble result;
	
#if defined(ARMGNU)
    subDD(a, b, &result);
#else
#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT)
	if (IS_DENORMAL_DBL(a) || IS_DENORMAL_DBL(b))  {
        subDD(a, b, &result);
		goto exit;
	}
#endif /* defined(J9_NO_DENORMAL_FLOAT_SUPPORT) */

	result = a - b;

#endif /* defined(ARMGNU) */

	goto exit; /* avoid warning */
exit:
	
	return result;
}


I_32 
helperDoubleMinusDouble(jdouble *a, jdouble *b, jdouble *c)
{
	*c = helperCDoubleMinusDouble(*a, *b);
	return 0;	
}


jdouble 
helperCDoubleMultiplyDouble(jdouble a, jdouble b)
{
	jdouble result;

#if defined(WIN32) || defined(J9X86)
	int e1, e2, re;
#endif
#if defined(ARMGNU) && defined(HARDHAT)
	if (IS_NAN_DBL(a) || IS_NAN_DBL(b))  {
		SET_NAN_DBL_PTR(&result);	/* return an incorrect, but non-crashing answer (NaN) */
		goto exit;
	}
#endif

#if defined(ARMGNU)
	multiplyDD(a, b, &result);
#else
#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT) && defined(LINUXPPC))
    if (IS_DENORMAL_DBL(a) || IS_DENORMAL_DBL(b)) {
		multiplyDD(a, b,&result);
	} else {
#endif
#if defined(WIN32) || defined(J9X86)
	e1 = GET_DP_EXPONENT(&a);
	e2 = GET_DP_EXPONENT(&b);
	re = e1+e2-DPEXPONENT_BIAS;
	if(re <= 0) {
		multiplyDD(a, b, &result);
	} else 
#endif
	{
		result = a * b;
	}
#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT) && defined(LINUXPPC))
	}
#endif
	
#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT) && (defined(ARMGNU) || defined(LINUXPPC)))
	if (IS_ZERO_DBL(result) &&  !IS_ZERO_DBL(a) && !IS_ZERO_DBL(b)) {
		multiplyDD(a, b, &result);
	}
#if defined(HARDHAT) && (defined(ARMGNU)||defined(LINUXPPC))
	else {
		if(IS_DENORMAL_DBL(result) && !IS_DENORMAL_DBL(a) && !IS_DENORMAL_DBL(b)) {
			multiplyDD(a, b, &result);
		}
	}
#endif /* defined(HARDHAT) && (defined(ARMGNU)||defined(LINUXPPC))  */

#endif /* defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT) && (defined(ARMGNU) || defined(LINUXPPC))) */

#endif /* defined(ARMGNU) */
	
	goto exit; /* avoid warning */
exit:
	
	return result;
}


I_32 
helperDoubleMultiplyDouble(jdouble *a, jdouble *b, jdouble *c)
{
	*c = helperCDoubleMultiplyDouble(*a, *b);
	return 0;
}


jdouble 
helperCDoublePlusDouble(jdouble a, jdouble b)
{
	jdouble result;

#if defined(ARMGNU)
	addDD(a, b, &result);
#else
#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT)
	if (IS_DENORMAL_DBL(a) || IS_DENORMAL_DBL(b))  {
        addDD(a, b, &result);
		goto exit;
	}
#endif

	result = a + b;

#endif /* defined(ARMGNU) */

	goto exit; /* avoid warning */
exit:

	return result;
}


I_32 
helperDoublePlusDouble(jdouble *a, jdouble *b, jdouble *c)
{
	*c = helperCDoublePlusDouble(*a, *b);
	return 0;
}


I_32 
helperCFloatCompareFloat(jfloat a, jfloat b)
{
	I_32 result;

#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT)&&defined(ARMGNU))
   	if (IS_DENORMAL_SNGL(a) || IS_DENORMAL_SNGL(b)) {
        result = compareDF(a, b);
		return result;
   	}
#endif

	if(IS_NAN_SNGL(a) || IS_NAN_SNGL(b)) {
		result = -2;
	} else {
		if(a > b) {
			result = 1;
		} else {
			if(a < b) {
				result = -1;
			} else {
				result = 0;
			}
		}
	}
	return result;
}


I_32 
helperFloatCompareFloat(jfloat *a, jfloat *b)
{
	jfloat tmpA, tmpB;

	PTR_SINGLE_VALUE(a, &tmpA);
	PTR_SINGLE_VALUE(b, &tmpB);
	
	return helperCFloatCompareFloat(tmpA, tmpB);
}


jfloat 
helperCFloatDivideFloat(jfloat a, jfloat b)
{
	jfloat result;

#if defined(ARMGNU) && defined(HARDHAT)
	
	/*
	 * If either input number is NaN, then
	 * return Nan, an incorrect but non-crashing
	 * number.
	 */
    if (IS_NAN_SNGL(a) || IS_NAN_SNGL(b)) {
	    SET_NAN_SNGL_PTR(&result);
	    return result;
    }
#endif

#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT)&&(defined(ARMGNU)||defined(LINUXPPC)))
	if (IS_DENORMAL_SNGL(a) || IS_DENORMAL_SNGL(b))  {
        divideDF(a, b, &result);
		return result;
	}
#endif

#if defined(ARMGNU) || defined(J9ZTPF)
    divideDF(a, b, &result);
    return result;
#else
	result = a / b;
#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT)&&(defined(ARMGNU)||defined(LINUXPPC)))
	if (IS_ZERO_SNGL(result) && IS_FINITE_SNGL(a) && IS_FINITE_SNGL(b) && !IS_ZERO_SNGL(a)) {
		divideDF(a, b,&result);
	} 
#if defined(HARDHAT)&&(defined(ARMGNU)||defined(LINUXPPC))
	else {
		if(IS_DENORMAL_SNGL(result) && !IS_DENORMAL_SNGL(a) && !IS_DENORMAL_SNGL(b)) {
			divideDF(a, b,&result);
		}
	}
#endif
#endif

#endif /* defined(ARMGNU) */

	return result;
}


I_32 
helperFloatDivideFloat(jfloat * a, jfloat * b, jfloat * c)
{
	jfloat tmpA, tmpB, tmpC;

	PTR_SINGLE_VALUE(a, &tmpA);
	PTR_SINGLE_VALUE(b, &tmpB);
	tmpC = helperCFloatDivideFloat(tmpA, tmpB);
	PTR_SINGLE_STORE(c, &tmpC);

	return 0;
}


jfloat 
helperCFloatMinusFloat(jfloat a, jfloat b)
{
	jfloat result;

#if defined(ARMGNU)
    subDF(a, b, &result);
#else
#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT)
	if (IS_DENORMAL_SNGL(a) || IS_DENORMAL_SNGL(b))  {
        subDF(a, b, &result);
		goto exit;
	}
#endif

	result = a - b;

#endif /* defined(ARMGNU) */

	goto exit; /* avoid warning */
exit:
	
	return result;
}


I_32 
helperFloatMinusFloat(jfloat * a, jfloat * b, jfloat * c)
{
	jfloat tmpA, tmpB, tmpC;

	PTR_SINGLE_VALUE(a, &tmpA);
	PTR_SINGLE_VALUE(b, &tmpB);
	tmpC = helperCFloatMinusFloat(tmpA, tmpB);
	PTR_SINGLE_STORE(c, &tmpC);

	return 0;
}


jfloat 
helperCFloatMultiplyFloat(jfloat a, jfloat b)
{
	jfloat result;

#if defined(ARMGNU)
	multiplyDF(a, b, &result);
#else
#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT)&&(defined(ARMGNU)||defined(LINUXPPC)))
   	if (IS_DENORMAL_SNGL(a) || IS_DENORMAL_SNGL(b))  {
        multiplyDF(a, b, &result);
        goto exit;
	}
#endif

	result = a * b;

#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT)&&(defined(ARMGNU)||defined(LINUXPPC)))
	if (IS_ZERO_SNGL(result) &&  !IS_ZERO_SNGL(a) && !IS_ZERO_SNGL(b)) {
		multiplyDF(a, b,&result);
	}
#if defined(HARDHAT)&&(defined(ARMGNU)||defined(LINUXPPC))
	else {
		if(IS_DENORMAL_SNGL(result) && !IS_DENORMAL_SNGL(a) && !IS_DENORMAL_SNGL(b)) {
			multiplyDF(a, b,&result);
		}
	}
#endif
#endif

#endif /* defined(ARMGNU) */

	goto exit; /* avoid warning */
exit:
	
	return result;
}


I_32 
helperFloatMultiplyFloat(jfloat *a, jfloat *b, jfloat *c)
{
	jfloat tmpA, tmpB, tmpC;
    
	PTR_SINGLE_VALUE(a, &tmpA);
	PTR_SINGLE_VALUE(b, &tmpB);
	tmpC = helperCFloatMultiplyFloat(tmpA, tmpB);
	PTR_SINGLE_STORE(c, &tmpC);
	
	return 0;
}


jfloat 
helperCFloatPlusFloat(jfloat a, jfloat b)
{
	jfloat result;

#if defined(J9_NO_DENORMAL_FLOAT_SUPPORT) || (defined(HARDHAT)&&defined(ARMGNU))
    if (IS_DENORMAL_SNGL(a) || IS_DENORMAL_SNGL(b))  {
        addDF(a, b, &result);
		goto exit;
	}
#endif

	result = a + b;

	goto exit; /* avoid warning */
exit:
	
	return result;
}


I_32 
helperFloatPlusFloat(jfloat *a, jfloat *b, jfloat *c)
{
	jfloat tmpA, tmpB, tmpC;

	PTR_SINGLE_VALUE(a, &tmpA);
	PTR_SINGLE_VALUE(b, &tmpB);
	tmpC = helperCFloatPlusFloat(tmpA, tmpB);
	PTR_SINGLE_STORE(c, &tmpC);
	
	return 0;
}


I_32 
helperNegateDouble(jdouble *a, jdouble *b)
{
	HIGH_U32_FROM_DBL_PTR(b) = HIGH_U32_FROM_DBL_PTR(a) ^ 0x80000000;
	LOW_U32_FROM_DBL_PTR(b) = LOW_U32_FROM_DBL_PTR(a);

	return 0;
}


I_32 
helperNegateFloat(jfloat *a, jfloat *b)
{

	*(U_32*) b = (*(U_32 *)a) ^ 0x80000000;

	return 0;
}


#endif /* J9VM_INTERP_FLOAT_SUPPORT */ /* End File Level Build Flags */


