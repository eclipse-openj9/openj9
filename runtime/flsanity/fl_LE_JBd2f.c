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


   void group_JBd2f() {
      int i=0, groupNum=0,numTests=20;
      int testsPassed=0, testsFailed=0;
      unsigned int op_0 [] = {
         0x00000000,0x00000000, 
         0x00000000,0x80000000, 
         0x00000000,0x7ff00000, 
         0x00000000,0xfff00000, 
         0xffffffff,0x7fefffff, 
         0xffffffff,0xffefffff, 
         0x00000001,0x00000000, 
         0x00000001,0x80000000, 
         0x00000000,0x7ff80000, 
         0x00000000,0x40590000, 
         0x00000000,0xc0590000, 
         0x00000000,0x40690000, 
         0x00000000,0xc0690000, 
         0x00000000,0x408f4000, 
         0x00000000,0xc08f4000, 
         0x00000000,0xc0600000, 
         0x00000000,0xc0e00000, 
         0x00000000,0xc1e00000, 
         0x00000000,0xc3e00000, 
         0xe0000000,0x369fffff, 
         0x0, 0x0};
      unsigned int result [] = {
         0x00000000,
         0x80000000,
         0x7f800000,
         0xff800000,
         0x7f800000,
         0xff800000,
         0x00000000,
         0x80000000,
         0x7fc00000,
         0x42c80000,
         0xc2c80000,
         0x43480000,
         0xc3480000,
         0x447a0000,
         0xc47a0000,
         0xc3000000,
         0xc7000000,
         0xcf000000,
         0xdf000000,
         0x00000001,
         0x0};
      double *p0=(double *) op_0;
      float *erp=(float*) result, r, *rp=&r;
      for (i=0; i<numTests; i++) {
         r = JBd2f(*(p0));
         if ( IS_SNAN(erp)){
            if ( !(IS_SNAN(&r)) ){
               printf("%d.%d: op_0=0x%08x%08x Expected=0x%08x Actual=0x%08x \n",
                  groupNum, i, 
                  *((int *)p0+1), *(int *)p0, *(int *)erp, *(int *)&r);
               testsFailed++;
            } else {
               testsPassed++;
            }

         } else {
            if ( (FLWORD(&r) != FLWORD(erp)) ){
               printf("%d.%d: op_0=0x%08x%08x Expected=0x%08x Actual=0x%08x \n",
                  groupNum, i, 
                  *((int *)p0+1), *(int *)p0, *(int *)erp, *(int *)&r);
               testsFailed++;
            } else {
               testsPassed++;
            }
         }

         p0++;erp++;
      }
      printf("[%d] %s Passed %d Failed %d Total %d \n", groupNum,"JBd2f", testsPassed, testsFailed, numTests);
      totalNumTest +=numTests; totalTestsPassed +=testsPassed; totalTestsFailed +=testsFailed;
   }

