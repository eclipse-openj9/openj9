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
// Test LongAddition.java
// Testing the addition of long type variables and constants

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class LongAddition extends jit.test.jitt.Test {
   @Test
   public void testLongAddition() {
        long A, B, C;

        A = 16777216L;
        B = 2147418112L;
        C = tstdoAdds(A,B);
	if (C != 2164195328L)
                Assert.fail("LongAddition->run: Incorrect addition #1!");

        A = 1L;
        B = 32767L;
        C = tstdoAdds(A,B);
        if (C != 32768L)
                Assert.fail("LongAddition->run: Incorrect addition #2!");


        A = 32767L;
        C = tstdoNullAdd(A);
        if (C != 32767L)
                Assert.fail("LongAddition->run: Incorrect addition #3!");


        A = 32767L;
        C = tstdoIdentAdd(A);
        if (C != 32766L)
                Assert.fail("LongAddition->run: Incorrect addition #4!");


        A = 32767L;
        C = tstdoShortConstAdd(A);
        if (C != 33791L)
                Assert.fail("LongAddition->run: Incorrect addition #5!");


        A = 32767L;
        C = tstdoIntConstAdd(A);
        if (C != 2147450879L)
                Assert.fail("LongAddition->run: Incorrect addition #6!");


        A = 32767L;
        C = tstdoLongConstAdd(A);
        if (C != 140733193420799L)
                Assert.fail("LongAddition->run: Incorrect addition #7!");


   }

   static long tstdoAdds(long A, long B){
    long C;
    C=A+B;

    return C;
   }

   static long tstdoNullAdd(long A){
    long C;
    C=A+(0L);

    return C;
   }

   static long tstdoIdentAdd(long A){
    long C;
    C=A+(-1L);

    return C;
   }

   static long tstdoShortConstAdd(long A){
    long C;
    C=A+(1024L);

    return C;
   }

   static long tstdoIntConstAdd(long A){
    long C;
    C=A+(2147418112L);

    return C;
   }

   static long tstdoLongConstAdd(long A){
    long C;
    C=A+(140733193388032L);

    return C;
   }

}



