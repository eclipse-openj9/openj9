/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
// Test file: NegationTest.java
// Testing the negation of byte, short, integer, and long type values.

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class NegationTest extends jit.test.jitt.Test {

   @Test
   public void testNegationTest() {

       byte A;
       short B;
       int C;
       long D;

       A=5;
       A = tstNeg(A);
        if (A != -5)
                Assert.fail("NegationTest->run: Incorrect negation for test #1!");


       A=-5;
       A = tstNeg(A);
        if (A != 5)
                Assert.fail("NegationTest->run: Incorrect negation for test #2!");


       B=5;
       B = tstNeg(B);
        if (B != -5)
                Assert.fail("NegationTest->run: Incorrect negation for test #3!");



       C=5;
       C = tstNeg(C);
        if (C != -5)
                Assert.fail("NegationTest->run: Incorrect negation for test #4!");


       D=5;
       D = tstLNeg(D);
        if (D != -5)
                Assert.fail("NegationTest->run: Incorrect negation for test #5!");



       D=-5;
       D = tstLNeg(D);
        if (D != 5)
                Assert.fail("NegationTest->run: Incorrect negation for test #6!");



       D=0xffff0000L;
       D = tstLNeg(D);
        if (D != -4294901760L)
                Assert.fail("NegationTest->run: Incorrect negation for test #7!");



       D=0xffff000000000000L;
       D = tstLNeg(D);
        if (D != 281474976710656L)
                Assert.fail("NegationTest->run: Incorrect negation for test #2!");


   }

   static byte tstNeg(byte A){
    return (byte)(-A);
   }

   static short tstNeg(short A){
    return (short)(-A);
   }

   static int tstNeg(int A) {
    return -A;
   }

   static long tstLNeg(long A) {
    return -A;
   }

}



