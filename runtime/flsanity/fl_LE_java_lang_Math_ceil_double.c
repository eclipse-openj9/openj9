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


   void group_java_lang_Math_ceil_double() {
      int i=0, groupNum=33,numTests=19;
      int testsPassed=0, testsFailed=0;
      unsigned int op_0 [] = {
         0x0,0x0, 
         0x00000000,0x3ff00000, 
         0x00000000,0xbff00000, 
         0x9999999a,0x3ff19999, 
         0x9999999a,0xbff19999, 
         0x47ae147b,0x3f747ae1, 
         0x00000000,0x408f4000, 
         0x00000000,0xc08f4000, 
         0x00000000,0x7ff80000, 
         0x00000000,0x3fe00000, 
         0x00000000,0xbfe00000, 
         0x00000000,0x3ff80000, 
         0x00000000,0xbff80000, 
         0x00000000,0x7ff00000, 
         0x00000000,0xfff00000, 
         0xffffffff,0x433fffff, 
         0xffffffff,0xc33fffff, 
         0xffffffff,0x7fefffff, 
         0xffffffff,0xffefffff, 
         0x0, 0x0};
      unsigned int result [] = {
         0x0,0x0, 
         0x00000000,0x3ff00000, 
         0x00000000,0xbff00000, 
         0x00000000,0x40000000, 
         0x00000000,0xbff00000, 
         0x00000000,0x3ff00000, 
         0x00000000,0x408f4000, 
         0x00000000,0xc08f4000, 
         0x00000000,0x7ff80000, 
         0x00000000,0x3ff00000, 
         0x00000000,0x80000000, 
         0x00000000,0x40000000, 
         0x00000000,0xbff00000, 
         0x00000000,0x7ff00000, 
         0x00000000,0xfff00000, 
         0xffffffff,0x433fffff, 
         0xffffffff,0xc33fffff, 
         0xffffffff,0x7fefffff, 
         0xffffffff,0xffefffff, 
         0x0, 0x0};
      double *p0=(double *) op_0;
      double *erp=(double*) result, r, *rp=&r;
      for (i=0; i<numTests; i++) {
         r = java_lang_Math_ceil_double(*(p0));
         if ( IS_DNAN(erp) ){
            if ( !(IS_DNAN(&r)) ){
               printf("%d.%d: op_0=0x%08x%08x Expected=0x%08x%08x Actual=0x%08x%08x \n",
                  groupNum, i, 
                  *((int *)p0+1), *(int *)p0, *((int *) erp + 1), *(int *)erp, *((int *) &r + 1), *(int *)&r);
               testsFailed++;
            } else {
               testsPassed++;
            }

         } else {
            if ( (HIWORD(&r) != HIWORD(erp)) || (LOWORD(&r) != LOWORD(erp)) ){
               printf("%d.%d: op_0=0x%08x%08x Expected=0x%08x%08x Actual=0x%08x%08x \n",
                  groupNum, i, 
                  *((int *)p0+1), *(int *)p0, *((int *) erp + 1), *(int *)erp, *((int *) &r + 1), *(int *)&r);
               testsFailed++;
            } else {
               testsPassed++;
            }
         }

         p0++;erp++;
      }
      printf("[%d] %s Passed %d Failed %d Total %d \n", groupNum,"java_lang_Math_ceil_double", testsPassed, testsFailed, numTests);
      totalNumTest +=numTests; totalTestsPassed +=testsPassed; totalTestsFailed +=testsFailed;
   }

