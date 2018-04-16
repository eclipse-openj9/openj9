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

@Test(groups = { "level.sanity","component.jit" })
public class Div07 extends jit.test.jitt.Test {

	private static int tstDiv(int p1, int p2) {
		return p1 / p2;
	}
	
	private static int tstRem(int p1, int p2) {
		return p1 % p2;
	}
	
	@Test
	public void testDiv07() {
		int p1 = 0x80000000;
		int p2 = -1;

		for (int j = 0; j < sJitThreshold; j++) {
			if (tstDiv(72, 5) != 14) throw new ArithmeticException("int 72 / 5");
			if (tstRem(72, 5) != 2) throw new ArithmeticException("int 72 % 5");
			if (tstDiv(p1, p2) != p1) throw new ArithmeticException("int boundary divide");
			if (tstRem(p1, p2) != 0) throw new ArithmeticException("int boundary remainder");
		}
	}

}
