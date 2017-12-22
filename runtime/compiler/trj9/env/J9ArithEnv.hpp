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

#ifndef J9_ARITHENV_INCL
#define J9_ARITHENV_INCL

/*
 * The following #define and typedef must appear before any #includes in this file
 */
#ifndef J9_ARITHENV_CONNECTOR
#define J9_ARITHENV_CONNECTOR
namespace J9 { class ArithEnv; }
namespace J9 { typedef J9::ArithEnv ArithEnvConnector; }
#endif

#include "env/OMRArithEnv.hpp"
#include "infra/Annotations.hpp"
#include "env/jittypes.h"


namespace J9
{

class OMR_EXTENSIBLE ArithEnv : public OMR::ArithEnvConnector
   {
public:

   float floatAddFloat(float a, float b);
   float floatSubtractFloat(float a, float b);
   float floatMultiplyFloat(float a, float b);
   float floatDivideFloat(float a, float b);
   float floatRemainderFloat(float a, float b);
   float floatNegate(float a);
   double doubleAddDouble(double a, double b);
   double doubleSubtractDouble(double a, double b);
   double doubleMultiplyDouble(double a, double b);
   double doubleDivideDouble(double a, double b);
   double doubleRemainderDouble(double a, double b);
   double doubleNegate(double a);
   double floatToDouble(float a);
   float doubleToFloat(double a);
   int64_t longRemainderLong(int64_t a, int64_t b);
   int64_t longDivideLong(int64_t a, int64_t b);
   int64_t longMultiplyLong(int64_t a, int64_t b);
   };

}

#endif
