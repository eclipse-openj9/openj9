/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
#include "float.h"
#include "emfloat.h"
#include "j9protos.h"



/* NOTE: arguments reversed to all helpers! */
#ifdef ZAURUS
FLOAT_RESULT jitMathHelperFloatAddFloat(float arg1, float arg2)
{
	float floatResult = helperCFloatPlusFloat(arg1, arg2);
   return *(FLOAT_RESULT*)&floatResult;
}

FLOAT_RESULT jitMathHelperFloatSubtractFloat(float arg1, float arg2)
{
	float floatResult = helperCFloatMinusFloat(arg1, arg2);
   return *(FLOAT_RESULT*)&floatResult;
}

FLOAT_RESULT jitMathHelperFloatMultiplyFloat(float arg1, float arg2)
{
	float floatResult = helperCFloatMultiplyFloat(arg1, arg2);
	return *(FLOAT_RESULT*)&floatResult;
}

FLOAT_RESULT jitMathHelperFloatDivideFloat(float arg1, float arg2)
{
	float floatResult = helperCFloatDivideFloat(arg1, arg2);
	return *(FLOAT_RESULT*)&floatResult;
}

FLOAT_RESULT jitMathHelperFloatRemainderFloat(float arg1, float arg2)
{
	float floatResult = helperCFloatRemainderFloat(arg1, arg2);
	return *(FLOAT_RESULT*)&floatResult;
}

DOUBLE_RESULT jitMathHelperDoubleAddDouble(double arg1, double arg2)
{
	double doubleResult = helperCDoublePlusDouble(arg1, arg2);
	return *(DOUBLE_RESULT*)&doubleResult;
}

DOUBLE_RESULT jitMathHelperDoubleSubtractDouble(double arg1, double arg2)
{
	double doubleResult = helperCDoubleMinusDouble(arg1, arg2);
	return *(DOUBLE_RESULT*)&doubleResult;
}

DOUBLE_RESULT jitMathHelperDoubleMultiplyDouble(double arg1, double arg2)
{
	double doubleResult = helperCDoubleMultiplyDouble(arg1, arg2);
	return *(DOUBLE_RESULT*)&doubleResult;
}

DOUBLE_RESULT jitMathHelperDoubleDivideDouble(double arg1, double arg2)
{
	double doubleResult = helperCDoubleDivideDouble(arg1, arg2);
	return *(DOUBLE_RESULT*)&doubleResult;
}

DOUBLE_RESULT jitMathHelperDoubleRemainderDouble(double arg1, double arg2)
{
	double doubleResult = helperCDoubleRemainderDouble(arg1, arg2);
	return *(DOUBLE_RESULT*)&doubleResult;
}

FLOAT_RESULT jitMathHelperConvertDoubleToFloat(double arg1)
{
	float floatResult = helperCConvertDoubleToFloat(arg1);
	return *(FLOAT_RESULT*)&floatResult;
}

DOUBLE_RESULT jitMathHelperConvertFloatToDouble(float arg1)
{
	double doubleResult = helperCConvertFloatToDouble(arg1);
	return *(DOUBLE_RESULT*)&doubleResult;
}

FLOAT_RESULT jitMathHelperConvertIntToFloat(INT_ARG arg1)
{
	float floatResult = helperCConvertIntegerToFloat(arg1);
	return *(FLOAT_RESULT*)&floatResult;
}

DOUBLE_RESULT jitMathHelperConvertIntToDouble(INT_ARG arg1)
{
	double doubleResult = helperCConvertIntegerToDouble(arg1);
	return *(DOUBLE_RESULT*)&doubleResult;
}

FLOAT_RESULT jitMathHelperConvertLongToFloat(LONG_ARG arg1)
{
	float floatResult = helperCConvertLongToFloat(arg1);
	return *(FLOAT_RESULT*)&floatResult;
}

DOUBLE_RESULT jitMathHelperConvertLongToDouble(LONG_ARG arg1)
{
	double doubleResult = helperCConvertLongToDouble(arg1);
	return *(DOUBLE_RESULT*)&doubleResult;
}
#endif

FLOAT_RESULT jitMathHelperFloatNegate(float arg1)
{
   U_32 result = (*(U_32 *)&arg1) ^ 0x80000000;
   return *(FLOAT_RESULT*)&result;
}

DOUBLE_RESULT jitMathHelperDoubleNegate(double arg1)
{
	DOUBLE_RESULT result;
	helperNegateDouble(&arg1, (double*)&result);
	return result;
}

/* NOTE: ??U comparators return 1 if either argument is NaN.
    Other non-U versions return 0 if either argument is NaN
*/
int jitMathHelperFloatCompareEQ(float arg1, float arg2) 
{
	return helperCFloatCompareFloat(arg1, arg2) == 0;
}

int jitMathHelperFloatCompareEQU(float arg1, float arg2) 
{	
	int result = helperCFloatCompareFloat(arg1, arg2);
	return result == 0 || result == -2;
}

int jitMathHelperFloatCompareNE(float arg1, float arg2) 
{
	int result = helperCFloatCompareFloat(arg1, arg2);
	return result != -2 && result != 0;
}

int jitMathHelperFloatCompareNEU(float arg1, float arg2) 
{
	return helperCFloatCompareFloat(arg1, arg2) != 0;
}

int jitMathHelperFloatCompareLT(float arg1, float arg2) 
{
	return helperCFloatCompareFloat(arg1, arg2) == -1;
}

int jitMathHelperFloatCompareLTU(float arg1, float arg2) 
{
	return helperCFloatCompareFloat(arg1, arg2) < 0;
}

int jitMathHelperFloatCompareLE(float arg1, float arg2) 
{
	int result = helperCFloatCompareFloat(arg1, arg2);
	return result == 0 || result == -1;
}

int jitMathHelperFloatCompareLEU(float arg1, float arg2) 
{
	return helperCFloatCompareFloat(arg1, arg2) <= 0;
}

int jitMathHelperFloatCompareGT(float arg1, float arg2) 
{
	return helperCFloatCompareFloat(arg1, arg2) > 0;
}

int jitMathHelperFloatCompareGTU(float arg1, float arg2) 
{
	int result = helperCFloatCompareFloat(arg1, arg2);
	return result == 1 || result == -2;
}

int jitMathHelperFloatCompareGE(float arg1, float arg2) 
{
	return helperCFloatCompareFloat(arg1, arg2) >= 0;
}

int jitMathHelperFloatCompareGEU(float arg1, float arg2) 
{
	return helperCFloatCompareFloat(arg1, arg2) != -1;
}

int jitMathHelperDoubleCompareEQ(double arg1, double arg2) 
{
	return helperCDoubleCompareDouble(arg1, arg2) == 0;
}

int jitMathHelperDoubleCompareEQU(double arg1, double arg2) 
{
	int result = helperCDoubleCompareDouble(arg1, arg2);
	return result == 0 || result == -2;
}

int jitMathHelperDoubleCompareNE(double arg1, double arg2) 
{
	int result = helperCDoubleCompareDouble(arg1, arg2);
	return result != -2 && result != 0;
}

int jitMathHelperDoubleCompareNEU(double arg1, double arg2) 
{
	return helperCDoubleCompareDouble(arg1, arg2) != 0;
}

int jitMathHelperDoubleCompareLT(double arg1, double arg2) 
{
	return helperCDoubleCompareDouble(arg1, arg2) == -1;
}

int jitMathHelperDoubleCompareLTU(double arg1, double arg2) 
{
	return helperCDoubleCompareDouble(arg1, arg2) < 0;
}

int jitMathHelperDoubleCompareLE(double arg1, double arg2) 
{
	int result = helperCDoubleCompareDouble(arg1, arg2);
	return result == 0 || result == -1;
}

int jitMathHelperDoubleCompareLEU(double arg1, double arg2) 
{
	return helperCDoubleCompareDouble(arg1, arg2) <= 0;
}

int jitMathHelperDoubleCompareGT(double arg1, double arg2) 
{
	return helperCDoubleCompareDouble(arg1, arg2) > 0;
}

int jitMathHelperDoubleCompareGTU(double arg1, double arg2) 
{
	int result = helperCDoubleCompareDouble(arg1, arg2);
	return result == -2 || result == 1;
}

int jitMathHelperDoubleCompareGE(double arg1, double arg2) 
{
	return helperCDoubleCompareDouble(arg1, arg2) >= 0;
}

int jitMathHelperDoubleCompareGEU(double arg1, double arg2) 
{
	return helperCDoubleCompareDouble(arg1, arg2) != -1;
}

