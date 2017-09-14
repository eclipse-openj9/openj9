/*******************************************************************************
 * Copyright (c) 2002, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

#include "jni.h"
#include "floatsanityg.h"
#include "j9protos.h"
#include "fltconst.h"
#include "fltdmath.h"
#include "helpers.h"

#define PI J9_POS_PI 

int initializeFPU(void)
{
#if (defined(WIN32)   || defined(J9X86) || defined(J9HAMMER)) && !defined(J9_SOFT_FLOAT) && !defined(WIN64)
	helperInitializeFPU();
#endif
	return 0;
}


float java_lang_Float_intBitsToFloat(I_32 i) {
	float result;
	result = *(float *) &i;
	return  result;
}
double java_lang_Double_longBitsToDouble(I_64 l) {
	double result;

	HIGH_U32_FROM_DBL(result) = HIGH_U32_FROM_DBL(l);
	LOW_U32_FROM_DBL(result) = LOW_U32_FROM_DBL(l);

	return result;
}
I_32 java_lang_Float_floatToIntBits(float f) {
	I_32 result;
	result = *(int *) &f;
	return  result;
}
I_64 java_lang_Double_doubleToLongBits(double d) {
	I_64 result;

	HIGH_U32_FROM_DBL(result) = HIGH_U32_FROM_DBL(d);
	LOW_U32_FROM_DBL(result) = LOW_U32_FROM_DBL(d);

	return result;
}


float JBfmul ( float op_0, float op_1 ) { 
	float result;
	helperFloatMultiplyFloat(&op_0, &op_1, &result);
	return result;
}

float JBfadd ( float op_0, float op_1 ) { 
	float result;
	helperFloatPlusFloat(&op_0, &op_1, &result);
	return result;
}

double JBddiv ( double op_0, double op_1 ) { 
	double result;
	helperDoubleDivideDouble(&op_0, &op_1, &result);
	return result;
}

float JBfneg ( float op_0 ) { 
	float result;
	helperNegateFloat(&op_0, &result);
	return result;
}

double JBdmul ( double op_0, double op_1 ) { 
	double result;
	helperDoubleMultiplyDouble(&op_0, &op_1, &result);
	return result;
}

double JBdadd ( double op_0, double op_1 ) { 
	double result;
	helperDoublePlusDouble(&op_0, &op_1, &result);
	return result;
}

float JBi2f ( int op_0 ) { 
	float result;
	helperConvertIntegerToFloat (&op_0, &result);
	return result;
}

double JBi2d ( int op_0 ) { 
	double result;
	helperConvertIntegerToDouble(&op_0, &result);
	return result;
}

double JBdneg ( double op_0 ) { 
	double result;
	helperNegateDouble(&op_0, &result);
	return result;
}

float JBfsub ( float op_0, float op_1 ) { 
	float result;
	helperFloatMinusFloat(&op_0, &op_1, &result);
	return result;
}

I_64 JBf2l ( float op_0 ) { 
	I_64 result;
	I_64 tmp;

#if defined(WIN32) && !defined(WIN32_IBMC)
	I_64 l;
	l = (I_64) op_0;
	if ( l == 0x8000000000000000 ) {
		if ( op_0 > 0 )
			l = 0x7fffffffffffffff;
	}
	return l;
#endif

	helperConvertFloatToLong(&op_0, &tmp);
	HIGH_U32_FROM_DBL(result) = HIGH_U32_FROM_DBL(tmp);
	LOW_U32_FROM_DBL(result) = LOW_U32_FROM_DBL(tmp);

	return result;
}

int JBf2i ( float op_0 ) { 
	I_32 result;

#if defined(WIN32) && !defined(WIN32_IBMC)
	I_64 l = (I_64) op_0;

	result = (I_32) op_0;
	if ( l ==0x8000000000000000) {
		if (op_0 > 0) {
			result = 0x7fffffff;
		} else {
			result = 0x80000000;
		}
	}
#else
	helperConvertFloatToInteger(&op_0, &result);
#endif

	return result;
}

double JBdsub ( double op_0, double op_1 ) { 
	double result;
	helperDoubleMinusDouble(&op_0, &op_1, &result);
	return result;
}

double JBf2d ( float op_0 ) { 
	double result;
	helperConvertFloatToDouble(&op_0, &result);
	return result;
}

float JBd2f ( double op_0 ) { 
	float result;
	helperConvertDoubleToFloat(&op_0, &result);
	return result;
}

double JBdrem ( double op_0, double op_1 ) { 
	double result;
	helperDoubleRemainderDouble(&op_0, &op_1, &result);
	return result;
}

float JBfdiv ( float op_0, float op_1 ) { 
	float result;
	helperFloatDivideFloat(&op_0, &op_1, &result);
	return result;
}

float JBl2f ( I_64 op_0 ) { 
	float result;
	I_64 tmp;

	HIGH_U32_FROM_DBL(tmp) = HIGH_U32_FROM_DBL(op_0);
	LOW_U32_FROM_DBL(tmp) = LOW_U32_FROM_DBL(op_0);

	helperConvertLongToFloat(&tmp, &result);
	return result;
}

double JBl2d ( I_64 op_0 ) { 
	double result;
	I_64 tmp;

	HIGH_U32_FROM_DBL(tmp) = HIGH_U32_FROM_DBL(op_0);
	LOW_U32_FROM_DBL(tmp) = LOW_U32_FROM_DBL(op_0);
 
	helperConvertLongToDouble(&tmp, &result);
	return result;
}

I_64 JBd2l ( double op_0 ) { 
	I_64 result;
	I_64 tmp;

#if defined(WIN32) && !defined(WIN32_IBMC)
	I_64 l;
	l = (I_64) op_0;
	if ( l == 0x8000000000000000 ) {
		if ( op_0 > 0 )
			l = 0x7fffffffffffffff;
	}
	return l;
#endif

	helperConvertDoubleToLong(&op_0, &tmp);

	HIGH_U32_FROM_DBL(result) = HIGH_U32_FROM_DBL(tmp);
	LOW_U32_FROM_DBL(result) = LOW_U32_FROM_DBL(tmp);

	return result;
}

int JBd2i ( double op_0 ) { 
	I_32 result;

#if defined(WIN32) && !defined(WIN32_IBMC) && !defined(WIN64)
	I_64 l;
	IDATA i = (IDATA) op_0;
	l = (I_64) op_0;
	if (i==0 &&  l!=0) {
		if (op_0 > 0)
			i = 0x7fffffff;
		else
			i = 0x80000000;
		}
	return i;
#endif

	helperConvertDoubleToInteger(&op_0, &result);
	return result;
}


float JBfrem ( float op_0, float op_1 ) { 
	float result;
	helperFloatRemainderFloat(&op_0, &op_1, &result);
	return result;
}

int JBdcmpl ( double op_0, double op_1 ) { 
	if ( IS_NAN_DBL(op_0) || IS_NAN_DBL(op_1) ) return -1;
	return helperDoubleCompareDouble(&op_0, &op_1);
}

int JBdcmpg ( double op_0, double op_1 ) { 
	if ( IS_NAN_DBL(op_0) || IS_NAN_DBL(op_1) ) return 1;
	return helperDoubleCompareDouble(&op_0, &op_1);
}

int JBfcmpl ( float op_0, float op_1 ) { 
	if ( IS_NAN_SNGL(op_0) || IS_NAN_SNGL(op_1) ) return -1;
	return (int) helperFloatCompareFloat(&op_0, &op_1);
}

int JBfcmpg ( float op_0, float op_1 ) { 
	if ( IS_NAN_SNGL(op_0) || IS_NAN_SNGL(op_1) ) return 1;
	return (int) helperFloatCompareFloat(&op_0, &op_1);
}

float java_lang_Math_abs_float ( float op_0 ) { 
	I_32 l;
	float result;
	l = java_lang_Float_floatToIntBits(op_0);
	l &= 0x7fffffff;
	result = java_lang_Float_intBitsToFloat(l);
	return result;
}

double java_lang_Math_abs_double ( double op_0 ) { 
	I_64 l;
	double result;
	l = java_lang_Double_doubleToLongBits(op_0);
	HIGH_U32_FROM_DBL(l) = HIGH_U32_FROM_DBL(l) & 0x7fffffff;
	result = java_lang_Double_longBitsToDouble(l);
	return result;
}

double java_lang_Math_rint_double ( double op_0 ) { 
	double result;
	helperDoubleRint(&op_0, &result);
	return result;
}

double java_lang_Math_floor_double ( double op_0 ) { 
	double result;
	helperDoubleFloor(&op_0, &result);
	return result;
}

double java_lang_Math_IEEEremainder_double ( double op_0, double op_1 ) { 
	double result;
	helperDoubleIEEERemainderDouble(&op_0, &op_1, &result);
	return result;
}

double java_lang_Math_ceil_double ( double op_0 ) { 
	double result;
	helperDoubleCeil(&op_0, &result);
	return result;
}

I_64 java_lang_Math_round_double ( double op_0 ) { 
	if ( helperDoubleCompareDouble(&op_0, &op_0) == -2 )
		return 0;
	return JBd2l(java_lang_Math_floor_double(JBdadd (op_0, (double) 0.5)));
}

int java_lang_Math_round_float ( float op_0 ) { 
	if ( helperFloatCompareFloat(&op_0, &op_0) == -2 )
		return 0;
	return JBd2i(java_lang_Math_floor_double(JBdadd ((double)op_0, (double) 0.5)));
}

double java_lang_Math_min_double_double ( double op_0, double op_1 ) { 
	I_64 l = ((I_64)0x80000000) << 32;
	double result;

	if ( JBdcmpl ( op_0, op_1 ) == 1) {
		return op_1;
	} 

	if ( JBdcmpl ( op_1, op_0 ) ==1) {
		return op_0;
	} 

	if ( JBdcmpl ( op_0, op_1 ) != 0) {
		SET_NAN_DBL_PTR(&result);
		return result;
	}

	 if ( (JBdcmpl ( op_0, 0.0 ) == 0) &&
#if defined(WIN32_IBMC) || defined(AIXPPC)
	(((HIWORD(&op_0) | HIWORD(&op_1)) &  0x80000000) != 0)){
#else
    (((java_lang_Double_doubleToLongBits(op_0) | java_lang_Double_doubleToLongBits(op_1)) &  l) != 0)){
#endif
		return JBdmul(0.0, -1.0);
	}

	return op_0;
}

float java_lang_Math_min_float_float ( float op_0, float op_1 ) { 
	I_32 l = 0x80000000;
	float result;

	if ( JBfcmpl ( op_0, op_1 ) == 1) {
		return op_1;
	} 

	if ( JBfcmpl ( op_1, op_0 ) ==1) {
		return op_0;
	} 

	if ( JBfcmpl ( op_0, op_1 ) != 0) {
		SET_NAN_SNGL_PTR(&result);
		return result;
	}

    if ( (JBfcmpl ( op_0, 0.0 ) == 0) && (((java_lang_Float_floatToIntBits(op_0) | java_lang_Float_floatToIntBits(op_1)) &  l) != 0)){
		return JBfmul(0.0, -1.0);
	}

	return op_0;
}

double java_lang_Math_max_double_double ( double op_0, double op_1 ) { 
	I_64 l = ((I_64)0x80000000) << 32;
	double result;

	if ( JBdcmpl ( op_0, op_1 ) == 1) {
		return op_0;
	} 

	if ( JBdcmpl ( op_1, op_0 ) ==1) {
		return op_1;
	} 

	if ( JBdcmpl ( op_0, op_1 ) != 0) {
		SET_NAN_DBL_PTR(&result);
		return result;
	}

    if ( (JBdcmpl ( op_0, 0.0 ) == 0) && 
#if defined(WIN32_IBMC) || defined(AIXPPC)
	(((HIWORD(&op_0) | HIWORD(&op_1)) &  0x80000000) != 0)){
#else
	(((java_lang_Double_doubleToLongBits(op_0) | java_lang_Double_doubleToLongBits(op_1)) &  l) != 0)){
#endif
		return 0.0;
	}

	return op_0;

}
float java_lang_Math_max_float_float ( float op_0, float op_1 ) { 
	I_32 l = 0x80000000;
	float result;

	if ( JBfcmpl ( op_0, op_1 ) == 1) {
		return op_0;
	} 

	if ( JBfcmpl ( op_1, op_0 ) ==1) {
		return op_1;
	} 

	if ( JBfcmpl ( op_0, op_1 ) != 0) {
		SET_NAN_SNGL_PTR(&result);
		return result;
	}

    if ( (JBfcmpl ( op_0, 0.0 ) == 0) && (((java_lang_Float_floatToIntBits(op_0) | java_lang_Float_floatToIntBits(op_1)) &  l) != 0)){
		return 0.0;
	}

	return op_0;
}

double java_lang_Math_toDegrees_double ( double op_0 ) { 
	return JBdmul(PI, JBddiv(op_0, 90));
}

double java_lang_Math_toRadians_double ( double op_0 ) { 
	return JBdmul(90, JBddiv(op_0, PI));
}

double java_lang_Math_sqrt_double ( double op_0 ) { 
	double result;

#if defined(WIN32_IBMC)
	if ( op_0 < 0) {
		SET_NAN_DBL_PTR(&result);
		return result;
	}
#endif

	helperDoubleSqrt(&op_0, &result);
	return result;
}

double java_lang_Math_acos ( double op_0 ) { 
	double result;
	helperDoubleArcCos(&op_0, &result);
	return result;
}

double java_lang_Math_asin ( double op_0 ) { 
	double result;
	helperDoubleArcSin(&op_0, &result);
	return result;
}

double java_lang_Math_tan ( double op_0 ) { 
	double result;
	helperDoubleTan(&op_0, &result);
	return result;
}

double java_lang_Math_pow ( double op_0, double op_1 ) { 
	double result;
	helperDoublePowDouble(&op_0, &op_1, &result);
	return result;
}

double java_lang_Math_sin ( double op_0 ) { 
	double result;
	helperDoubleSin(&op_0, &result);
	return result;
}

double java_lang_Math_exp ( double op_0 ) { 
	double result;
	helperDoubleExp(&op_0, &result);
	return result;
}

double java_lang_Math_cos ( double op_0 ) { 
	double result;
	helperDoubleCos(&op_0, &result);
	return result;
}

double java_lang_Math_log ( double op_0 ) { 
	double result;
	helperDoubleLn(&op_0, &result);
	return result;
}

double java_lang_Math_atan ( double op_0 ) { 
	double result;
	helperDoubleArcTan(&op_0, &result);
	return result;
}

double java_lang_Math_atan2 ( double op_0, double op_1 ) { 
	double result;
	helperDoubleArcTan2Double(&op_0, &op_1, &result);
	return result;
}
