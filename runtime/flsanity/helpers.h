/*******************************************************************************
 * Copyright (c) 2002, 2014 IBM Corp. and others
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
#include "j9comp.h"

extern J9_CFUNC void  helperInitializeFPU ();

int helperDoubleCompareDouble(ESDOUBLE *a, ESDOUBLE *b);
I_32 helperDoubleDivideDouble(ESDOUBLE *a, ESDOUBLE *b, ESDOUBLE *c);
I_32 helperDoubleMinusDouble(ESDOUBLE *a, ESDOUBLE *b, ESDOUBLE *c);
I_32 helperDoubleMultiplyDouble(ESDOUBLE *a, ESDOUBLE *b, ESDOUBLE *c);
I_32 helperDoublePlusDouble(ESDOUBLE *a, ESDOUBLE *b, ESDOUBLE *c);
I_32 helperFloatCompareFloat(ESSINGLE *a, ESSINGLE *b);
I_32 helperFloatDivideFloat(ESSINGLE * a, ESSINGLE * b, ESSINGLE * c);
I_32 helperFloatMinusFloat(ESSINGLE * a, ESSINGLE * b, ESSINGLE * c);
I_32 helperFloatMultiplyFloat(ESSINGLE * a, ESSINGLE * b, ESSINGLE * c);
I_32 helperFloatPlusFloat(ESSINGLE * a, ESSINGLE * b, ESSINGLE * c);
I_32 helperNegateFloat(ESSINGLE *a, ESSINGLE *b);
I_32 helperNegateDouble(ESDOUBLE *a, ESDOUBLE *b);

void helperConvertDoubleToFloat(ESDOUBLE *src, ESSINGLE *dst);
void helperConvertDoubleToInteger(ESDOUBLE * a, I_32 * anInt);
void helperConvertDoubleToLong(ESDOUBLE * a, I_64 * aLong);
void helperConvertFloatToDouble(ESSINGLE * src, ESDOUBLE * dst);
void helperConvertFloatToInteger(ESSINGLE * a, I_32 * anInt);
void helperConvertFloatToLong(ESSINGLE * a, I_64 * aLong);
void helperConvertIntegerToDouble(I_32 *anInt, ESDOUBLE *a);
void helperConvertIntegerToFloat(I_32 * anInt, ESSINGLE * a);
void helperConvertLongToDouble(I_64 * aLong, ESDOUBLE *a);
void helperConvertLongToFloat(I_64 * aLong, ESSINGLE * a);

I_32 helperDoubleRemainderDouble(ESDOUBLE * a, ESDOUBLE * b, ESDOUBLE * c);
I_32 helperFloatRemainderFloat(ESSINGLE * a, ESSINGLE * b, ESSINGLE * c);

I_32 helperDoubleArcCos(ESDOUBLE *a, ESDOUBLE *b);
I_32 helperDoubleArcSin(ESDOUBLE *a, ESDOUBLE *b);
I_32 helperDoubleArcTan(ESDOUBLE *a, ESDOUBLE *b);
I_32 helperDoubleArcTan2Double(ESDOUBLE *a, ESDOUBLE *b, ESDOUBLE *c);
I_32 helperDoubleCeil(ESDOUBLE *a, ESDOUBLE *b);
I_32 helperDoubleCos(ESDOUBLE * a, ESDOUBLE * b);
I_32 helperDoubleExp(ESDOUBLE *a, ESDOUBLE *b);
I_32 helperDoubleFloor(ESDOUBLE *a, ESDOUBLE *b);
I_32 helperDoubleIEEERemainderDouble(ESDOUBLE *a, ESDOUBLE *b, ESDOUBLE *c);
I_32 helperDoubleIEEERemainderDouble(ESDOUBLE *a, ESDOUBLE *b, ESDOUBLE *c);
I_32 helperDoubleLn(ESDOUBLE *a, ESDOUBLE *b);
I_32 helperDoublePowDouble(ESDOUBLE * a, ESDOUBLE * b, ESDOUBLE * c);
I_32 helperDoubleRint(ESDOUBLE * a, ESDOUBLE * b);
I_32 helperDoubleSin(ESDOUBLE *a, ESDOUBLE *b);
I_32 helperDoubleSqrt(ESDOUBLE *a, ESDOUBLE *b);
I_32 helperDoubleTan(ESDOUBLE * a, ESDOUBLE * b);
