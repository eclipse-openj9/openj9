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
// Test LongArgTest4.java
// Testing the combination of passing 2 long arguments  and testing the return
// value of a long 
// For example: argcall1(long , long), argcall2(long)

package jit.test.jitt.os390.linkage;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class LongArgTest4 extends jit.test.jitt.Test {


	private static long tstFoo(long arg1, long arg2) {
		if (arg1 != 1L) {
			Assert.fail("linkTest3->tstFoo: Incorrect integer argument passing");
		}

                if (arg2 != 3L) {
                        Assert.fail("linkTest3->tstFoo: Incorrect integer argument passing");
                }
		return arg1;
}


        private long tstDoo(long arg1) {
                if (arg1 != 1L) {
                        Assert.fail("linkTest3->tstDoo: Incorrect long argument passing");
                }
                return arg1;
}



	@Test
	public void testLongArgTest4() {
		long long_result=0L;

		for (int j = 0; j < sJitThreshold; j++) {
			long_result=0L;
			long_result=tstFoo(1L,3L);
			if (long_result != 1L)
        	                Assert.fail("LongArgTest4->run: Incorrect long result return value!");
			long_result=0L;
			long_result=tstDoo(1L);
		
			if (long_result != 1L)
                                Assert.fail("LongArgTest4->run: Incorrect long result return value!");

		}
	}

}
