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

#define J9_EXTERNAL_TO_VM

#include "env/ArithEnv.hpp"
#include "j9.h"
#include "j9cfg.h"
#include "jilconsts.h"
#include "j9protos.h"

float
J9::ArithEnv::floatAddFloat(float a, float b)
   {
   return helperCFloatPlusFloat(a, b);
   }

float
J9::ArithEnv::floatSubtractFloat(float a, float b)
   {
   return helperCFloatMinusFloat(a, b);
   }

float
J9::ArithEnv::floatMultiplyFloat(float a, float b)
   {
   return helperCFloatMultiplyFloat(a, b);
   }

float
J9::ArithEnv::floatDivideFloat(float a, float b)
   {
   return helperCFloatDivideFloat(a, b);
   }
float
J9::ArithEnv::floatRemainderFloat(float a, float b)
   {
   return helperCFloatRemainderFloat(a, b);
   }

float
J9::ArithEnv::floatNegate(float a)
   {
   float c;
   helperNegateFloat(&a, &c);
   return c;
   }

double
J9::ArithEnv::doubleAddDouble(double a, double b)
   {
   return helperCDoublePlusDouble(a, b);
   }

double
J9::ArithEnv::doubleSubtractDouble(double a, double b)
   {
   return helperCDoubleMinusDouble(a, b);
   }

double
J9::ArithEnv::doubleMultiplyDouble(double a, double b)
   {
   return helperCDoubleMultiplyDouble(a, b);
   }

double
J9::ArithEnv::doubleDivideDouble(double a, double b)
   {
   return helperCDoubleDivideDouble(a, b);
   }
double
J9::ArithEnv::doubleRemainderDouble(double a, double b)
   {
   return helperCDoubleRemainderDouble(a, b);
   }

double
J9::ArithEnv::doubleNegate(double a)
   {
   double c;
   helperNegateDouble(&a, &c);
   return c;
   }

double
J9::ArithEnv::floatToDouble(float a)
   {
   return helperCConvertFloatToDouble(a);
   }

float
J9::ArithEnv::doubleToFloat(double a)
   {
   return helperCConvertDoubleToFloat(a);
   }

int64_t
J9::ArithEnv::longRemainderLong(int64_t a, int64_t b)
   {
   return helperCLongRemainderLong(a, b);
   }

int64_t
J9::ArithEnv::longDivideLong(int64_t a, int64_t b)
   {
   return helperCLongDivideLong(a, b);
   }

int64_t
J9::ArithEnv::longMultiplyLong(int64_t a, int64_t b)
   {
   return helperCLongMultiplyLong(a, b);
   }
