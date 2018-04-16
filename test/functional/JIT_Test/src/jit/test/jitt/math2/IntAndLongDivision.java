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
// Test file: IntAndLongDivision.java
// Testing the division operation among integer and long type operand variables, and constants.

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class IntAndLongDivision extends jit.test.jitt.Test {

   @Test
   public void testIntAndLongDivision() {

        int A, B;
        long C;
        int D;

        A = 1;
        B = 32767;
        D = tstdoDiv(A,B);
        if (D != 0)
                Assert.fail("IntAndLongDivision->run: Incorrect division for test #1!");


        A = 32767;
        B = 32767;
        D = tstdoDiv(A,B);
        if (D != 1)
                Assert.fail("IntAndLongDivision->run: Incorrect division for test #2!");




        A = 32767;
        D = tstdoIdentDiv(A);
        if (D != -32767)
                Assert.fail("IntAndLongDivision->run: Incorrect division for test #3!");




        A = 32767;
        C = tstdoShortConstDiv(A);
        if (C != 31L)
                Assert.fail("IntAndLongDivision->run: Incorrect division for test #4!");




        A = 32767;
        C = tstdoIntConstDiv(A);
        if (C != 0L)
                Assert.fail("IntAndLongDivision->run: Incorrect division for test #5!");


   }

   static int tstdoDiv(int A, int B){
    int C;
    C=A/B;

    return C;
   }

   static long tstdoNullDiv(int A){
    long C;
    C=A/(0);

    return C;
   }

   static int tstdoIdentDiv(int A){
    int C;
    C=A/(-1);

    return C;
   }

   static long tstdoShortConstDiv(int A){
    long C;
    C=A/(1024);

    return C;
   }

   static long tstdoIntConstDiv(int A){
    long C;
    C=A/(2147418112);

    return C;
   }

}



