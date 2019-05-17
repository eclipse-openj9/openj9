/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package jit.test.jitt.math;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class longCompare extends jit.test.jitt.Test {

	private static final long[][] pairs = {
		{ -1L, 0L },
		{ 0L, 1L },
		{ 999L, 1000L },
		{ 9999999999L, 10000000000L },
		{ -1000L, -999L },
		{ -10000000000L, -9999999999L },
		{ Long.MIN_VALUE, Long.MAX_VALUE },
		{ Long.MIN_VALUE, Long.MIN_VALUE + 1},
		{ Long.MAX_VALUE - 1, Long.MAX_VALUE },
		{ 0L, 0x00000000FFFFFFFFL }, // long with sign bit set in low word
		{ 0x00000000FFFFFFFFL, 0x00000000FFFFFFFFL + 1 }
	};

	private static void tstFoo(long i) {
		long a = 0L;
		if (i != 2)
			Assert.fail("bad math");
		a = i + 2;//a == 4
		if (a == 5)
			Assert.fail("bad math");
		if (a >= 6)
			Assert.fail("bad math");
		if (a > 7)
			Assert.fail("bad math");
		if (a < 3)
			Assert.fail("bad math");
		if (a <= 1)
			Assert.fail("bad math");

	}

	/* these tests are designed to tests the lcmp?? 
	 * evaluators, at least when run at lowOpt
	 */	
	private static boolean tstCmpLT(long l1, long l2) {
		boolean result = l1 < l2;
		return result;
	}

	private static boolean tstCmpLE(long l1, long l2) {
		boolean result = l1 <= l2;
		return result;
	}

	private static boolean tstCmpGT(long l1, long l2) {
		boolean result = l1 > l2;
		return result;
	}

	private static boolean tstCmpGE(long l1, long l2) {
		boolean result = l1 >= l2;
		return result;
	}

	private static boolean tstCmpEQ(long l1, long l2) {
		boolean result = l1 == l2;
		return result;
	}

	private static boolean tstCmpNE(long l1, long l2) {
		boolean result = l1 != l2;
		return result;
	}

	private static void runTstCmpLT(long p1, long p2) {
		if (tstCmpLT(p1, p2) != true)
			Assert.fail(p1 + " < " + p2);
		if (tstCmpLT(p2, p1) != false)
			Assert.fail(p2 + " < " + p1);
		if (tstCmpLT(p1, p1) != false)
			Assert.fail(p1 + " < " + p1);
	}

	private static void runTstCmpLE(long p1, long p2) {
		if (tstCmpLE(p1, p2) != true)
			Assert.fail(p1 + " <= " + p2);
		if (tstCmpLE(p2, p1) != false)
			Assert.fail(p2 + " <= " + p1);
		if (tstCmpLE(p1, p1) != true)
			Assert.fail(p1 + " <= " + p1);
	}

	private static void runTstCmpGT(long p1, long p2) {
		if (tstCmpGT(p1, p2) != false)
			Assert.fail(p1 + " > " + p2);
		if (tstCmpGT(p2, p1) != true)
			Assert.fail(p2 + " > " + p1);
		if (tstCmpGT(p1, p1) != false)
			Assert.fail(p1 + " > " + p1);
	}

	private static void runTstCmpGE(long p1, long p2) {
		if (tstCmpGE(p1, p2) != false)
			Assert.fail(p1 + " >= " + p2);
		if (tstCmpGE(p2, p1) != true)
			Assert.fail(p2 + " >= " + p1);
		if (tstCmpGE(p1, p1) != true)
			Assert.fail(p1 + " >= " + p1);
	}

	private static void runTstCmpEQ(long p1, long p2) {
		if (tstCmpEQ(p1, p2) != false)
			Assert.fail(p1 + " == " + p2);
		if (tstCmpEQ(p2, p1) != false)
			Assert.fail(p2 + " == " + p1);
		if (tstCmpEQ(p1, p1) != true)
			Assert.fail(p1 + " == " + p1);
	}

	private static void runTstCmpNE(long p1, long p2) {
		if (tstCmpNE(p1, p2) != true)
			Assert.fail(p1 + " != " + p2);
		if (tstCmpNE(p2, p1) != true)
			Assert.fail(p2 + " != " + p1);
		if (tstCmpNE(p1, p1) != false)
			Assert.fail(p1 + " != " + p1);
	}

	@Test
	public void testLongCompare() {
		for (int j = 0; j < sJitThreshold; j++) {
			tstFoo(2L);

			for (int i = 0; i < pairs.length; i++) {
				runTstCmpLT(pairs[i][0], pairs[i][1]);
				runTstCmpLE(pairs[i][0], pairs[i][1]);
				runTstCmpGT(pairs[i][0], pairs[i][1]);
				runTstCmpGE(pairs[i][0], pairs[i][1]);
				runTstCmpEQ(pairs[i][0], pairs[i][1]);
				runTstCmpNE(pairs[i][0], pairs[i][1]);
			}
			
		}
	}
}
