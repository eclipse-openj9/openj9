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

/*
* ARM Testarossa and Cinquecento use integer registers for float results
* All other Cinquecento use integer registers for float results
* All other Testarossa builds using fp emulation use float registers for float results
*/

#if defined(TR_HOST_ARM)
#define FLOAT_RESULT    I_32
#define	DOUBLE_RESULT   I_64
#else
#define	FLOAT_RESULT    ESSINGLE
#define	DOUBLE_RESULT   ESDOUBLE
#endif
#define	INT_RESULT      I_32
#define	INT_ARG         I_32
#define LONG_RESULT     I_64
#define LONG_ARG        I_64

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ZAURUS
FLOAT_RESULT jitMathHelperFloatAddFloat(float arg1, float arg2);
FLOAT_RESULT jitMathHelperFloatSubtractFloat(float arg1, float arg2);
FLOAT_RESULT jitMathHelperFloatMultiplyFloat(float arg1, float arg2);
FLOAT_RESULT jitMathHelperFloatDivideFloat(float arg1, float arg2);
FLOAT_RESULT jitMathHelperFloatRemainderFloat(float arg1, float arg2);
DOUBLE_RESULT jitMathHelperDoubleAddDouble(double arg1, double arg2);
DOUBLE_RESULT jitMathHelperDoubleSubtractDouble(double arg1, double arg2);
DOUBLE_RESULT jitMathHelperDoubleMultiplyDouble(double arg1, double arg2);
DOUBLE_RESULT jitMathHelperDoubleDivideDouble(double arg1, double arg2);
DOUBLE_RESULT jitMathHelperDoubleRemainderDouble(double arg1, double arg2);
FLOAT_RESULT jitMathHelperConvertDoubleToFloat(double arg1);
DOUBLE_RESULT jitMathHelperConvertFloatToDouble(float arg1);
FLOAT_RESULT jitMathHelperConvertIntToFloat(INT_ARG arg1);
DOUBLE_RESULT jitMathHelperConvertIntToDouble(INT_ARG arg1);
FLOAT_RESULT jitMathHelperConvertLongToFloat(LONG_ARG arg1);
DOUBLE_RESULT jitMathHelperConvertLongToDouble(LONG_ARG arg1);
#endif

FLOAT_RESULT jitMathHelperFloatNegate(float arg1);
DOUBLE_RESULT jitMathHelperDoubleNegate(double arg1);
int jitMathHelperFloatCompareEQ(float arg1, float arg2);
int jitMathHelperFloatCompareEQU(float arg1, float arg2);
int jitMathHelperFloatCompareNE(float arg1, float arg2);
int jitMathHelperFloatCompareNEU(float arg1, float arg2);
int jitMathHelperFloatCompareLT(float arg1, float arg2);
int jitMathHelperFloatCompareLTU(float arg1, float arg2);
int jitMathHelperFloatCompareLE(float arg1, float arg2);
int jitMathHelperFloatCompareLEU(float arg1, float arg2);
int jitMathHelperFloatCompareGT(float arg1, float arg2);
int jitMathHelperFloatCompareGTU(float arg1, float arg2);
int jitMathHelperFloatCompareGE(float arg1, float arg2);
int jitMathHelperFloatCompareGEU(float arg1, float arg2);
int jitMathHelperDoubleCompareEQ(double arg1, double arg2);
int jitMathHelperDoubleCompareEQU(double arg1, double arg2);
int jitMathHelperDoubleCompareNE(double arg1, double arg2);
int jitMathHelperDoubleCompareNEU(double arg1, double arg2);
int jitMathHelperDoubleCompareLT(double arg1, double arg2);
int jitMathHelperDoubleCompareLTU(double arg1, double arg2);
int jitMathHelperDoubleCompareLE(double arg1, double arg2);
int jitMathHelperDoubleCompareLEU(double arg1, double arg2);
int jitMathHelperDoubleCompareGT(double arg1, double arg2);
int jitMathHelperDoubleCompareGTU(double arg1, double arg2);
int jitMathHelperDoubleCompareGE(double arg1, double arg2);
int jitMathHelperDoubleCompareGEU(double arg1, double arg2);

#ifdef __cplusplus
}
#endif

