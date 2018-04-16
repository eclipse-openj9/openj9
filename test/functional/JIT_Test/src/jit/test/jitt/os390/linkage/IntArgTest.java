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
// Test IntArgTest.java
// Testing the passing of integer arguments and the returning of an integer value
// For example: argcall(int, int, int)

package jit.test.jitt.os390.linkage;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class IntArgTest extends jit.test.jitt.Test {


	private static int tstFoo(int arg1, int arg2, int arg3) {
		if ((arg1+arg2+arg3) != 6) {
			Assert.fail("IntArgTest-> Incorrect integer argument passing");
		}

		return arg1+arg2+arg3;
	}

	@Test
	public void testIntArgTest() {
		int result=0;

		result = tstFoo(1,2,3);
		if (result != 6)
			Assert.fail("IntArgTest-> Incorrect return value!");
		for (int j = 0; j < sJitThreshold; j++) {
			result=0;
			result=tstFoo(1,2,3);
			if (result != 6)
        	                Assert.fail("IntArgTest-> Incorrect return value!");

		}
	}

}
