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
package jit.test.jitt.math;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class IntegerMultiply extends jit.test.jitt.Test {

	private static void tstFoo(int i) {
		int a = i * 7;
		if (a != 14)
			Assert.fail("bad math: 7");
			
		int b = i * -2;
		if (b != -4)
			Assert.fail("bad math: -2");

		int c = i * 1;
		if (c != 2)
			Assert.fail("bad math: 1");

		int d = i * 9;
		if (d != 18)
			Assert.fail("bad math: 9");

		int e = i * 64;
		if (e != 128)
			Assert.fail("bad math: 64");

		int f = i * 0;
		if (f != 0)
			Assert.fail("bad math: 0");

		int g = i * 1;
		if (g != 2)
			Assert.fail("bad math: 1");

		int h = i * -127;
		if (h != -254)
			Assert.fail("bad math: -127");

		int j = i * -129;
		if (j != -258)
			Assert.fail("bad math: -129");

		int k = i * 101;
		if (k != 202)
			Assert.fail("bad math: 101");

	}

	@Test
	public void testIntegerMultiply() {
		for (int j = 0; j < sJitThreshold; j++) {
			IntegerMultiply.tstFoo(2);
		}

	}
}
