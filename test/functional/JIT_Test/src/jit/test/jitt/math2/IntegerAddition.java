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
// Test IntegerAddition.java
// Testing the addition of integers, variable integers, and constant integers

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class IntegerAddition extends jit.test.jitt.Test {
   @Test
   public void testIntegerAddition() {
        int A, B, C;

        A = 1;
        B = 2;
        C = tstdoAdd(A,B);
	if (C != 3)
		Assert.fail("IntegerAddition->run: Incorrect addition #1!");

        A = 3;
        C = tstdoConstAdd(A);
        if (C != 4)
                Assert.fail("IntegerAddition->run: Incorrect addition #2!");

        A = 4;
        C = tstdoBigConstAdd(A);
        if (C != 8323076)
                Assert.fail("IntegerAddition->run: Incorrect addition #3!");

        A = 4;
        C = tstdoBigConstAdd2(A);
        if (C != 32772)
                Assert.fail("IntegerAddition->run: Incorrect addition#4!");

   }

   private static int tstdoAdd(int A, int B){
    int C;
    C=A+B;

    return C;
   }

   private static int tstdoConstAdd(int A){
    int C;
    C=A+1;

    return C;
   }

   private static int tstdoBigConstAdd(int A) {
    int C;
    C=A+0x7F0000;

    return C;
   }


   private static int tstdoBigConstAdd2(int A) {
    int C;
    C=A+0x8000;

    return C;
   }

}



