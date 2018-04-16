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
// Test file: LongORTest.java
// Testing the OR ("|") operation among long type operand variables, and constants.

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class LongORTest extends jit.test.jitt.Test {

   @Test
   public void testLongORTest() {

        long A, B, C;

        A = 16777216L;
        B = 2147418112L;
        C = tstdoOr(A,B);
        if (C != 2147418112)
                Assert.fail("LongOrTest->run: Incorrect result for test #1!");


        A = 1L;
        B = 32767L;
        C = tstdoOr(A,B);
        if (C != 32767)
                Assert.fail("LongOrTest->run: Incorrect result for test #2!");



        A = 32767L;
        C = tstdoNullOr(A);
        if (C != 32767)
                Assert.fail("LongOrTest->run: Incorrect result for test #3!");



        A = 32767L;
        C = tstdoIdentOr(A);
        if (C != -1)
                Assert.fail("LongOrTest->run: Incorrect result for test #4!");


        A = 32767L;
        C = tstdoShortConstOr(A);
        if (C != 32767)
                Assert.fail("LongOrTest->run: Incorrect result for test #5!");



        A = 32767L;
        C = tstdoIntConstOr(A);
        if (C != 2147450879)
                Assert.fail("LongOrTest->run: Incorrect result for test #6!");



   }

   static long tstdoOr(long A, long B){
    long C;
    C=A|B;

    return C;
   }

   static long tstdoNullOr(long A){
    long C;
    C=A|(0L);

    return C;
   }

   static long tstdoIdentOr(long A){
    long C;
    C=A|(-1L);

    return C;
   }

   static long tstdoShortConstOr(long A){
    long C;
    C=A|(1024L);

    return C;
   }

   static long tstdoIntConstOr(long A){
    long C;
    C=A|(2147418112L);

    return C;
   }

}



