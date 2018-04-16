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
// Test file: LongToInteger.java
// Testing the conversion from long to integer type.

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class LongToInteger extends jit.test.jitt.Test {

   @Test
   public void testLongToInteger() {

        int A;
        long B;

        B = -1;
        A = tstdoL2I(B);
        if (A != -1)
                Assert.fail("LongToInteger->run: Incorrect conversion outcome for test #1!");


        B = 1;
        A = tstdoL2I(B);
        if (A != 1)
                Assert.fail("LongToInteger->run: Incorrect conversion outcome for test #2!");


        B = 0x7ffff00000000L;
        A = tstdoL2I(B);
        if (A != 0)
                Assert.fail("LongToInteger->run: Incorrect conversion outcome for test #3!");



   }

   static int tstdoL2I(long A){
    int C=(int)A;

    return C;
   }

}



