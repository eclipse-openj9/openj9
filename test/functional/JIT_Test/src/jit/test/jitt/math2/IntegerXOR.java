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
// Test file: IntegerXOR.java
// Testing the XOR operation among integer type operand variables, and constants.

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class IntegerXOR extends jit.test.jitt.Test {

   @Test
   public void testIntegerXOR() {

        int A, B, C;

        A = 1;
        B = 32767;
        C = tstdoOp(A,B);
        if (C != 32766)
                Assert.fail("IntegerXOR->run: Incorrect result for test #1!");


        A = 32767;
        C = tstdoNull(A);
        if (C != 32767)
                Assert.fail("IntegerXOR->run: Incorrect result for test #2!");


        A = 32767;
        C = tstdoIdent(A);
        if (C != -32768)
                Assert.fail("IntegerXOR->run: Incorrect result for test #3!");


        A = 32767;
        C = tstdoShortConst(A);
        if (C != 31743)
                Assert.fail("IntegerXOR->run: Incorrect result for test #4!");


        A = 32767;
        C = tstdoConst(A);
        if (C != 2147450879)
                Assert.fail("IntegerXOR->run: Incorrect result for test #5!");


   }

   static int tstdoOp(int A, int B){
    int C;
    C=A^B;

    return C;
   }

   static int tstdoNull(int A){
    int C;
    C=A^(0);

    return C;
   }

   static int tstdoIdent(int A){
    int C;
    C=A^(-1);

    return C;
   }

   static int tstdoShortConst(int A){
    int C;
    C=A^(1024);

    return C;
   }

   static int tstdoConst(int A){
    int C;
    C=A^(2147418112);

    return C;
   }

}



