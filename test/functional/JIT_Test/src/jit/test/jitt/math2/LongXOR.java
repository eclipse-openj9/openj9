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
// Test file: LongXOR.java
// Testing the XOR operation among long type operand variables, and constants.

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class LongXOR extends jit.test.jitt.Test {

   @Test
   public void testLongXOR() {

        long A, B, C;

        A = 16777216L;
        B = 2147418112L;
        C = tstdoXor(A,B);
        if (C != 2130640896L)
                Assert.fail("LongXOR->run: Incorrect XOR for test #1!");


        A = 1L;
        B = 32767L;
        C = tstdoXor(A,B);
        if (C != 32766L)
                Assert.fail("LongXOR->run: Incorrect XOR for test #2!");



        A = 32767L;
        C = tstdoNullXor(A);
        if (C != 32767L)
                Assert.fail("LongXOR->run: Incorrect XOR for test #3!");



        A = 32767L;
        C = tstdoIdentXor(A);
        if (C != -32768L)
                Assert.fail("LongXOR->run: Incorrect XOR for test #4!");



        A = 32767L;
        C = tstdoShortConstXor(A);
        if (C != 31743L)
                Assert.fail("LongXOR->run: Incorrect XOR for test #5!");



        A = 32767L;
        C = tstdoIntConstXor(A);
        if (C != 2147450879L)
                Assert.fail("LongXOR->run: Incorrect XOR for test #6!");


   }

   static long tstdoXor(long A, long B){
    long C;
    C=A^B;

    return C;
   }

   static long tstdoNullXor(long A){
    long C;
    C=A^(0L);

    return C;
   }

   static long tstdoIdentXor(long A){
    long C;
    C=A^(-1L);

    return C;
   }

   static long tstdoShortConstXor(long A){
    long C;
    C=A^(1024L);

    return C;
   }

   static long tstdoIntConstXor(long A){
    long C;
    C=A^(2147418112L);

    return C;
   }

}



