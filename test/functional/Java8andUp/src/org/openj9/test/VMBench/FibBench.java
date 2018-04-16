/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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

package org.openj9.test.VMBench;

import org.testng.Assert;
import org.testng.annotations.Optional;
import org.testng.annotations.Parameters;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class FibBench {

	public static final Logger logger = Logger.getLogger(FibBench.class);

	private static int fibonacci(int num) {
		if (num < 3) {
			return 1;
		}
		return (fibonacci(num - 1) + fibonacci(num - 2));
	}


	private static int fibIteration(int n) {
		int x = 0, y = 1, z = 1;
		for (int i = 0; i < n; i++) {
			x = y;
			y = z;
			z = x + y;
		}
		return x;
	}

	@Parameters({ "ficIterNum" })
	@Test
	public static void testFic(@Optional("12") int fibTimes) {
		int result1 = 0;
		int result2 = -1;
		int iterations = 10000;
		logger.info("Fibonacci test iterations = " + iterations);
		long start = System.currentTimeMillis();
		for (int i = 0; i < iterations; ++i) {
			result1 = fibonacci(12);
			result2 = fibIteration(12);
			Assert.assertTrue((int) 144 == result1, "fibBench(12) == 144 test failed.");
			Assert.assertEquals(result1, result2, "fibBench(12) == 144 == results1 == results2 test failed.");
			result1 = fibonacci(fibTimes);
			result2 = fibIteration(fibTimes);
			Assert.assertEquals(result1, result2, "fibBench(" + fibTimes + ") test failed.");
		}
		long stop = System.currentTimeMillis();
		logger.info("Fibonacci done, time = " + (stop - start));
	}

}
