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
// Test file: IntegerANDTest.java
// Testing the AND operation among integer type operand variables, and constants.

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class IntegerANDTest extends jit.test.jitt.Test {

   @Test
   public void testIntegerANDTest() {
        int A, B, C;

        A = 1;
        B = 32767;
        C = tstdoAnds(A,B);
        if (C != 1)
                Assert.fail("IntegerANDTest->run: Incorrect AND result #1!");

        A = 32767;
        C = tstdoNullAnd(A);
        if (C != 0)
                Assert.fail("IntegerANDTest->run: Incorrect AND result #2!");

        A = 32767;
        C = tstdoIdentAnd(A);
        if (C != 32767)
                Assert.fail("IntegerANDTest->run: Incorrect AND result #3!");

        A = 32767;
        C = tstdoShortConstAnd(A);
        if (C != 1024)
                Assert.fail("IntegerANDTest->run: Incorrect AND result #4!");

        A = 32767;
        C = tstdoIntConstAnd(A);
        if (C != 0)
                Assert.fail("IntegerANDTest->run: Incorrect AND result #5!");

}

   static int tstdoAnds(int A, int B){
    int C;
    C=A&B;

    return C;
   }

   static int tstdoNullAnd(int A){
    int C;
    C=A&(0);

    return C;
   }

   static int tstdoIdentAnd(int A){
    int C;
    C=A&(-1);

    return C;
   }

   static int tstdoShortConstAnd(int A){
    int C;
    C=A&(1024);

    return C;
   }

   static int tstdoIntConstAnd(int A){
    int C;
    C=A&(2147418112);

    return C;
   }

}



