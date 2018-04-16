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
// Test file: LongSubtraction.java
// Testing the subtraction operation among long type operand variables, and constants.

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class LongSubtraction extends jit.test.jitt.Test {

   @Test
   public void testLongSubtraction() {

        long A, B, C;

        A = 1;
        B = 2;
        C = tstdoSub(A,B);
        if (C != -1)
                Assert.fail("LongSubtraction->run: Incorrect subtraction for test #1!");


        A = -3;
        C = tstdoConstSub(A);
        if (C != -4)
                Assert.fail("LongSubtraction->run: Incorrect subtraction for test #2!");



        A = 3;
        C = tstdoConstSub(A);
        if (C != 2)
                Assert.fail("LongSubtraction->run: Incorrect subtraction for test #3!");


        A = 4;
        C = tstdoBigConstSub(A);
        if (C != -8323068)
                Assert.fail("LongSubtraction->run: Incorrect subtraction for test #4!");


   }

   static long tstdoSub(long A, long B){
    long C;
    C=A-B;

    return C;
   }

   static long tstdoConstSub(long A){
    long C;
    C=A-1;

    return C;
   }

   static long tstdoBigConstSub(long A) {
    long C;
    C=A-0x7F0000;

    return C;
   }

}



