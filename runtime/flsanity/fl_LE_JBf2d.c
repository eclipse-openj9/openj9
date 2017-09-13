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


   void group_JBf2d() {
      int i=0, groupNum=11,numTests=29;
      int testsPassed=0, testsFailed=0;
      unsigned int op_0 [] = {
         0x00000000,
         0x80000000,
         0x7f800000,
         0xff800000,
         0x7fc00000,
         0x3f800000,
         0xbf800000,
         0x40000000,
         0xc0000000,
         0x43480000,
         0xc3480000,
         0xc3000000,
         0xc7000000,
         0xcf000000,
         0xdf000000,
         0x3f000000,
         0x3fc00000,
         0xbf000000,
         0x3fcccccd,
         0xbf19999a,
         0x40200000,
         0xbfc00000,
         0x402ccccd,
         0xbfd9999a,
         0x3f0147ae,
         0x3efd70a4,
         0x447a2000,
         0xc479e000,
         0xffc00000,
         0x0};
      unsigned int result [] = {
         0x00000000,0x00000000, 
         0x00000000,0x80000000, 
         0x00000000,0x7ff00000, 
         0x00000000,0xfff00000, 
         0x00000000,0x7ff80000, 
         0x00000000,0x3ff00000, 
         0x00000000,0xbff00000, 
         0x00000000,0x40000000, 
         0x00000000,0xc0000000, 
         0x00000000,0x40690000, 
         0x00000000,0xc0690000, 
         0x00000000,0xc0600000, 
         0x00000000,0xc0e00000, 
         0x00000000,0xc1e00000, 
         0x00000000,0xc3e00000, 
         0x00000000,0x3fe00000, 
         0x00000000,0x3ff80000, 
         0x00000000,0xbfe00000, 
         0xa0000000,0x3ff99999, 
         0x40000000,0xbfe33333, 
         0x00000000,0x40040000, 
         0x00000000,0xbff80000, 
         0xa0000000,0x40059999, 
         0x40000000,0xbffb3333, 
         0xc0000000,0x3fe028f5, 
         0x80000000,0x3fdfae14, 
         0x00000000,0x408f4400, 
         0x00000000,0xc08f3c00, 
         0x00000000,0xfff80000, 
         0x0, 0x0};
      float *p0=(float *) op_0;
      double *erp=(double*) result, r, *rp=&r;
      for (i=0; i<numTests; i++) {
         r = JBf2d(*(p0));
         if ( IS_DNAN(erp) ){
            if ( !(IS_DNAN(&r)) ){
               printf("%d.%d: op_0=0x%08x Expected=0x%08x%08x Actual=0x%08x%08x \n",
                  groupNum, i, 
                  *(int *)p0, *((int *) erp + 1), *(int *)erp, *((int *) &r + 1), *(int *)&r);
               testsFailed++;
            } else {
               testsPassed++;
            }

         } else {
            if ( (HIWORD(&r) != HIWORD(erp)) || (LOWORD(&r) != LOWORD(erp)) ){
               printf("%d.%d: op_0=0x%08x Expected=0x%08x%08x Actual=0x%08x%08x \n",
                  groupNum, i, 
                  *(int *)p0, *((int *) erp + 1), *(int *)erp, *((int *) &r + 1), *(int *)&r);
               testsFailed++;
            } else {
               testsPassed++;
            }
         }

         p0++;erp++;
      }
      printf("[%d] %s Passed %d Failed %d Total %d \n", groupNum,"JBf2d", testsPassed, testsFailed, numTests);
      totalNumTest +=numTests; totalTestsPassed +=testsPassed; totalTestsFailed +=testsFailed;
   }

