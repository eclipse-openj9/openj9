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

#include <math.h>
#include "j9comp.h"

I_32 __intDivideC(I_32 p1, I_32 p2) { return p1 / p2; }
I_32 __intRemainderC(I_32 p1, I_32 p2) { return p1 % p2; }
I_64 __longDivideC(I_64 p1, I_64 p2) { return p1 / p2; }
I_64 __longRemainderC(I_64 p1, I_64 p2) { return p1 % p2; }
I_64 __multi64(I_64 p1, I_64 p2) { return p1 * p2; }

#if (defined(__VFP_FP__) && !defined(__SOFTFP__))

I_64 __convertDouble2Long(double a)
   {
   /* NaN, max long, and min long cases are handled in Math.s:_double2Long */
   return (I_64) a;
   }

I_64 __convertFloat2Long(float a)
   {
   /* NaN, max long, and min long cases are handled in Math.s:_float2Long */
   return (I_64) a;
   }
   
double __convertLong2Double(I_64 a)
   {
   return (double) a;
   }

float __convertLong2Float(I_64 a)
   {
   return (float) a;
   }
   
double __doubleRemainderC(double p1, double p2)
   {
   if (isnan(p1))
      return p1;
   else if (isnan(p2))
      return p2;
   else
      return fmod(p1, p2);
   }

float __floatRemainderC(float p1, float p2)
   {
   if (isnan(p1)) 
      return p1;
   else if (isnan(p2))
      return p2;
   else
      return fmodf(p1, p2);
   }
#endif

