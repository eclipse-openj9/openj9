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
// Test LongArgTest3.java
// Testing the combination of passing 3 integer arguments and a long argument and testing the return
// value of an int and long 
// For example: argcall1(int, int, int, long), argcall2(int, long, int, int), and argcall3(long, int, int, int)

package jit.test.jitt.os390.linkage;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class LongArgTest3 extends jit.test.jitt.Test {


	private static int tstFoo(int arg1, int arg2, int arg3, long arg4) {
		if (arg1 != 1) {
			Assert.fail("LongArgTest3->tstFoo: Incorrect integer argument passing");
		}

                if (arg2 != 3) {
                        Assert.fail("LongArgTest3->tstFoo: Incorrect integer argument passing");
                }

	        if (arg3 != 5) {
                        Assert.fail("LongArgTest3->tstFoo: Incorrect int argument passing");
                }

                if (arg4 != 7) {
                        Assert.fail("LongArgTest3->tstFoo: Incorrect long argument passing");
                }

		return arg1;

}


        private long tstMoo(int arg1, long arg2, int arg3, int arg4) {

		// just some stuff for compiler
		int arg1_;
		long arg2_;

		arg1_=(int) arg2;
		arg2_= arg1;
	
		arg1_=arg1_++;
		arg2_=arg2_++;
	
		if (arg1_ != arg2_)
			arg2_=arg1_*arg1*arg2_*arg2;

		// end of some stuff for compiler

                if (arg1 != 1) {
                        Assert.fail("LongArgTest3->tstMoo: Incorrect integer argument passing");
                }

                if (arg2 != 3) {
                        Assert.fail("LongArgTest3->tstMoo: Incorrect long argument passing");
                }

		if (arg3 != 5) {
                        Assert.fail("LongArgTest3->tstMoo: Incorrect integer argument passing");
                }

                if (arg4 != 7) {
                        Assert.fail("LongArgTest3->tstMoo: Incorrect integer argument passing");
                }

                return arg2;
        }


        private int tstDoo(long arg1, int arg2, int arg3, int arg4) {
                if (arg1 != 1) {
                        Assert.fail("LongArgTest3->tstDoo: Incorrect long argument passing");
                }

                if (arg2 != 3) {
                        Assert.fail("LongArgTest3->tstDoo: Incorrect integer argument passing");
                }

                if (arg3 != 5) {
                        Assert.fail("LongArgTest3->tstDoo: Incorrect integer argument passing");
                }

                if (arg4 != 7) {
                        Assert.fail("LongArgTest3->tstDoo: Incorrect integer argument passing");
                }

                return arg2;

}

	@Test
	public void testLongArgTest3() {
		int result1, result2=0;
		long long_result=0L;


		for (int j = 0; j < sJitThreshold; j++) {
			result1=0;
			result1=tstFoo(1,3,5,7L);
			if (result1 != 1)
        	                Assert.fail("LongArgTest3->run: Incorrect int result1 return value!");

			long_result=0L;
			long_result=tstMoo(1,3L,5,7);

                        if (long_result != 3L)
                                Assert.fail("LongArgTest3->run: Incorrect long return value!");
			

			result2=0;
			result2=tstDoo(1L,3,5,7);
		
			if (result2 != 3)
                                Assert.fail("LongArgTest3->run: Incorrect int result2 return value!");

		}
	}

}
