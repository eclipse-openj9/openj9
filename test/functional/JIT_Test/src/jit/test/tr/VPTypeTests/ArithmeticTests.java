/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package jit.test.tr.VPTypeTests;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity","component.jit" })
public class ArithmeticTests {

        public static int BIGKONST = 0x87a23; 

        private static Logger logger = Logger.getLogger(ArithmeticTests.class);
        
        // check ranges created for div operations
        //
        @Test
        public void testDiv() {
               subTestDiv(6, 2);
               subTestDiv(6, 3);
               subTestDiv(6, 4);
               subTestDiv(7, 2);
               subTestDiv(7, 3);
               subTestDiv(7, 4);
               subTestDiv(8, 2);
               subTestDiv(8, 3);
               subTestDiv(8, 4);
        }
        private void subTestDiv(int numerator, int denominator) {
                int quotient = numerator/denominator;
                if (6 <= numerator && numerator <= 8)
                        if (2 <= denominator && denominator <= 4)
                                AssertJUnit.assertEquals(numerator/denominator, quotient);
        }

        // check equality relationships created between 2 valuenumbers
        // 
        @Test
        public void testEquals() {
               // try adds/subtracts first
               // S = x - const   ==> x = S + const
               // A = x + const   ==> x = A - const
               // 
               subTestEquals001(100);
               // try muls next 
               // 
               subTestEquals002(100);
        }
        private void subTestEquals001(int N) {
               int s;
               int x = 101;
               for (int i = 0; i < N; i++) {
                   s = 100 - x;
                   if (s > 0) break;
                   if (x >= 100) break;
                   if ((100 - x) > ArithmeticTests.BIGKONST) break;
                   if (x < 0) break;
                   if ((100 - x) + 10 > 200) break; // inconsistent ranges for s at this point 
                   x = x - i; 
               } 
        }
        private void subTestEquals002(int N) {
               int m;
               int x = 3;
               for (int i = 0; i < N; i++) {
                   m = 100 * x;
                   if (m < -100) break;
                   if (m > 10000) break;
                   if (x < 100) break;
                   if ((100 * x) > ArithmeticTests.BIGKONST) break;
                   if (x < 20000) break;
                   if (x > 250000) break;
                   if ((100 * x) + 10 > 200) break; // inconsistent ranges for m at this point
                   x = x - i; 
               } 
        }      
        @Test
		public void testLoops() {
                subTestLoops(true, 500);
        }  
        private void subTestLoops(boolean val, int x) {
                int N;
                int y;
                for (y = 0; y < x; y+=2);
                if (val) {
                        if (y <= 0 || y > 1000) return;
                        N = y;         
                } else N = -1;
                
                // merge point
                // constraints for N []        
                int count = 0;
                do {
                   count++;
                } while(count < N);
                if (count != 500)
                        logger.debug("--test failed-- count: "+count);       
                else
                        logger.debug("--test passed-- count: "+count);
        }
}

