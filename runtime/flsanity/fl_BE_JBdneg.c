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
#define true 1
#define false 0
#include "floatsanityg.h"


   void group_JBdneg() {
      int i=0, groupNum=8,numTests=100;
      int testsPassed=0, testsFailed=0;
      unsigned int op_0 [] = {
         0x4005bf0a, 0x8b145769,
         0x400921fb, 0x54442d18,
         0x401921fb, 0x54442d18,
         0x3ff921fb, 0x54442d18,
         0x4012d97c, 0x7f3321d2,
         0x3fe921fb, 0x54442d18,
         0x4002d97c, 0x7f3321d2,
         0x3fd0c152, 0x382d7365,
         0x3fe0c152, 0x382d7365,
         0x3ff0c152, 0x382d7365,
         0x3ff4f1a6, 0xc638d03f,
         0x3ffd524f, 0xe24f89f2,
         0x4000c152, 0x382d7365,
         0x4004f1a6, 0xc638d03f,
         0x400709d1, 0x0d3e7eab,
         0xbff00000, 0x00000000,
         0x3ff00000, 0x00000000,
         0xbff19999, 0x9999999a,
         0x3ff19999, 0x9999999a,
         0xc0590000, 0x00000000,
         0x40590000, 0x00000000,
         0x40000000, 0x00000000,
         0xc0000000, 0x00000000,
         0x40019999, 0x9999999a,
         0xc0019999, 0x9999999a,
         0x40690000, 0x00000000,
         0xc0690000, 0x00000000,
         0x00000000, 0x00000000,
         0x80000000, 0x00000000,
         0x00000000, 0x00000001,
         0x80000000, 0x00000001,
         0x00040000, 0x00000000,
         0x80040000, 0x00000000,
         0x3f1a36e2, 0xeb1c432d,
         0xbf1a36e2, 0xeb1c432d,
         0x3f2a36e2, 0xeb1c432d,
         0xbf2a36e2, 0xeb1c432d,
         0x3f747ae1, 0x47ae147b,
         0xbf747ae1, 0x47ae147b,
         0x3f847ae1, 0x47ae147b,
         0xbf847ae1, 0x47ae147b,
         0x408f4000, 0x00000000,
         0xc08f4000, 0x00000000,
         0x7fefffff, 0xffffffff,
         0xffefffff, 0xffffffff,
         0x7ff00000, 0x00000000,
         0xfff00000, 0x00000000,
         0x7ff80000, 0x00000000,
         0xbfd0907d, 0xc1930690,
         0x3fd0907d, 0xc1930690,
         0xbfd0c152, 0x382d7365,
         0xbfdfffff, 0xffffffff,
         0x3fe00000, 0x00000000,
         0xbfe0c152, 0x382d7366,
         0xbfe6a09e, 0x667f3bcc,
         0x3fe6a09e, 0x667f3bcc,
         0xbfe921fb, 0x54442d18,
         0xbfebb67a, 0xe8584caa,
         0x3febb67a, 0xe8584caa,
         0xbff0c152, 0x382d7365,
         0xbfeee8dd, 0x4748bf15,
         0x3feee8dd, 0x4748bf15,
         0xbff4f1a6, 0xc638d040,
         0xbff921fb, 0x54442d18,
         0xbfebb67a, 0xe8584cac,
         0xbfe6a09e, 0x667f3bcd,
         0xbfd0907d, 0xc1930695,
         0xbca1a626, 0x33145c07,
         0x400b3a25, 0x9b49db84,
         0x3fd0907d, 0xc1930689,
         0x400d524f, 0xe24f89f2,
         0x3fe00000, 0x00000001,
         0xbfe00000, 0x00000000,
         0x3fe0c152, 0x382d7366,
         0x400f6a7a, 0x2955385e,
         0x3fe6a09e, 0x667f3bcb,
         0x4010c152, 0x382d7365,
         0x3febb67a, 0xe8584ca8,
         0x4011cd67, 0x5bb04a9c,
         0x3ff4f1a6, 0xc638d040,
         0x4013e591, 0xa2b5f909,
         0x3feee8dd, 0x4748bf14,
         0x4014f1a6, 0xc638d03f,
         0x4015fdbb, 0xe9bba775,
         0x3fe6a09e, 0x667f3bce,
         0x401709d1, 0x0d3e7eab,
         0x3fe00000, 0x00000004,
         0x401815e6, 0x30c155e2,
         0x3fd0907d, 0xc193068f,
         0x3fd12614, 0x5e9ecd56,
         0xbfd19240, 0x70008291,
         0x3fe279a7, 0x4590331d,
         0xbfe4d82b, 0x68cac1a1,
         0xbff8eb24, 0x5cbee3a6,
         0x3ffbb67a, 0xe8584caa,
         0x40189712, 0xeeca32b5,
         0x400ddb3d, 0x742c2656,
         0xbfe5726f, 0xf2545a31,
         0xbff4f1a6, 0xc638d03f,
         0xfff80000, 0x00000000,
         0x0, 0x0};
      unsigned int result [] = {
         0xc005bf0a, 0x8b145769,
         0xc00921fb, 0x54442d18,
         0xc01921fb, 0x54442d18,
         0xbff921fb, 0x54442d18,
         0xc012d97c, 0x7f3321d2,
         0xbfe921fb, 0x54442d18,
         0xc002d97c, 0x7f3321d2,
         0xbfd0c152, 0x382d7365,
         0xbfe0c152, 0x382d7365,
         0xbff0c152, 0x382d7365,
         0xbff4f1a6, 0xc638d03f,
         0xbffd524f, 0xe24f89f2,
         0xc000c152, 0x382d7365,
         0xc004f1a6, 0xc638d03f,
         0xc00709d1, 0x0d3e7eab,
         0x3ff00000, 0x00000000,
         0xbff00000, 0x00000000,
         0x3ff19999, 0x9999999a,
         0xbff19999, 0x9999999a,
         0x40590000, 0x00000000,
         0xc0590000, 0x00000000,
         0xc0000000, 0x00000000,
         0x40000000, 0x00000000,
         0xc0019999, 0x9999999a,
         0x40019999, 0x9999999a,
         0xc0690000, 0x00000000,
         0x40690000, 0x00000000,
         0x80000000, 0x00000000,
         0x00000000, 0x00000000,
         0x80000000, 0x00000001,
         0x00000000, 0x00000001,
         0x80040000, 0x00000000,
         0x00040000, 0x00000000,
         0xbf1a36e2, 0xeb1c432d,
         0x3f1a36e2, 0xeb1c432d,
         0xbf2a36e2, 0xeb1c432d,
         0x3f2a36e2, 0xeb1c432d,
         0xbf747ae1, 0x47ae147b,
         0x3f747ae1, 0x47ae147b,
         0xbf847ae1, 0x47ae147b,
         0x3f847ae1, 0x47ae147b,
         0xc08f4000, 0x00000000,
         0x408f4000, 0x00000000,
         0xffefffff, 0xffffffff,
         0x7fefffff, 0xffffffff,
         0xfff00000, 0x00000000,
         0x7ff00000, 0x00000000,
         0xfff80000, 0x00000000,
         0x3fd0907d, 0xc1930690,
         0xbfd0907d, 0xc1930690,
         0x3fd0c152, 0x382d7365,
         0x3fdfffff, 0xffffffff,
         0xbfe00000, 0x00000000,
         0x3fe0c152, 0x382d7366,
         0x3fe6a09e, 0x667f3bcc,
         0xbfe6a09e, 0x667f3bcc,
         0x3fe921fb, 0x54442d18,
         0x3febb67a, 0xe8584caa,
         0xbfebb67a, 0xe8584caa,
         0x3ff0c152, 0x382d7365,
         0x3feee8dd, 0x4748bf15,
         0xbfeee8dd, 0x4748bf15,
         0x3ff4f1a6, 0xc638d040,
         0x3ff921fb, 0x54442d18,
         0x3febb67a, 0xe8584cac,
         0x3fe6a09e, 0x667f3bcd,
         0x3fd0907d, 0xc1930695,
         0x3ca1a626, 0x33145c07,
         0xc00b3a25, 0x9b49db84,
         0xbfd0907d, 0xc1930689,
         0xc00d524f, 0xe24f89f2,
         0xbfe00000, 0x00000001,
         0x3fe00000, 0x00000000,
         0xbfe0c152, 0x382d7366,
         0xc00f6a7a, 0x2955385e,
         0xbfe6a09e, 0x667f3bcb,
         0xc010c152, 0x382d7365,
         0xbfebb67a, 0xe8584ca8,
         0xc011cd67, 0x5bb04a9c,
         0xbff4f1a6, 0xc638d040,
         0xc013e591, 0xa2b5f909,
         0xbfeee8dd, 0x4748bf14,
         0xc014f1a6, 0xc638d03f,
         0xc015fdbb, 0xe9bba775,
         0xbfe6a09e, 0x667f3bce,
         0xc01709d1, 0x0d3e7eab,
         0xbfe00000, 0x00000004,
         0xc01815e6, 0x30c155e2,
         0xbfd0907d, 0xc193068f,
         0xbfd12614, 0x5e9ecd56,
         0x3fd19240, 0x70008291,
         0xbfe279a7, 0x4590331d,
         0x3fe4d82b, 0x68cac1a1,
         0x3ff8eb24, 0x5cbee3a6,
         0xbffbb67a, 0xe8584caa,
         0xc0189712, 0xeeca32b5,
         0xc00ddb3d, 0x742c2656,
         0x3fe5726f, 0xf2545a31,
         0x3ff4f1a6, 0xc638d03f,
         0x7ff80000, 0x00000000,
         0x0, 0x0};
      double *p0=(double *) op_0;
      double *erp=(double*) result, r, *rp=&r;
      for (i=0; i<numTests; i++) {
         r = JBdneg(*(p0));
         if ( IS_DNAN(erp) ){
            if ( !(IS_DNAN(&r)) ){
               printf("%d.%d: op_0=0x%08x%08x Expected=0x%08x%08x Actual=0x%08x%08x \n",
                  groupNum, i, 
                  *(int *)p0, *((int *)p0+1), *(int *)erp, *((int *) erp + 1), *(int *)&r, *((int *) &r + 1));
               testsFailed++;
            } else {
               testsPassed++;
            }

         } else {
            if ( (HIWORD(&r) != HIWORD(erp)) || (LOWORD(&r) != LOWORD(erp)) ){
               printf("%d.%d: op_0=0x%08x%08x Expected=0x%08x%08x Actual=0x%08x%08x \n",
                  groupNum, i, 
                  *(int *)p0, *((int *)p0+1), *(int *)erp, *((int *) erp + 1), *(int *)&r, *((int *) &r + 1));
               testsFailed++;
            } else {
               testsPassed++;
            }
         }

         p0++;erp++;
      }
      printf("[%d] %s Passed %d Failed %d Total %d \n", groupNum,"JBdneg", testsPassed, testsFailed, numTests);
      totalNumTest +=numTests; totalTestsPassed +=testsPassed; totalTestsFailed +=testsFailed;
   }

