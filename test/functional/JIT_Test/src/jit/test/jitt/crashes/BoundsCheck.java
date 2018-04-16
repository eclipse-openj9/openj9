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
package jit.test.jitt.crashes;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class BoundsCheck extends jit.test.jitt.Test {

	static byte sArrayOfByte[] = new byte[] {1, 2, 3};
	static char sArrayOfChar[] = new char[] {1, 2, 3};
	static short sArrayOfShort[] = {1, 2, 3};
	static int sArrayOfInt[] = {1, 2, 3};
	static long sArrayOfLong[] = {1, 2, 3};
//	static float sArrayOfFloat[] = {1.0F, 2.0F, 3.0F};
//	static double sArrayOfDouble[] = {1.0, 2.0, 3.0};

	int sIntegerResult = 0;
	double sFpResult = 0.0;

	void tstTriggerBoundsCheck(boolean triggerNow, int indexPassed) {

		int passCount = 0;
		
		if (!triggerNow)
			return;
		
		try {
			sIntegerResult += sArrayOfByte[indexPassed];
		} catch (ArrayIndexOutOfBoundsException e1) {
			passCount++;
			try {
				sIntegerResult += sArrayOfChar[indexPassed];
			} catch (ArrayIndexOutOfBoundsException e2) {
				passCount++;
				try {
					sIntegerResult += sArrayOfShort[indexPassed];
				} catch (ArrayIndexOutOfBoundsException e3) {
					passCount++;
					try {
						sIntegerResult += sArrayOfInt[indexPassed];
					} catch (ArrayIndexOutOfBoundsException e4) {
						passCount++;
						try {
							sIntegerResult += sArrayOfLong[indexPassed];
						} catch (ArrayIndexOutOfBoundsException e5) {
							return;
						}
					}
				}
			}
		}
		Assert.fail("Should have generated ArrayIndexOutOfBounds, passed = " + passCount);
	}

	@Test
	public void testnestedExceptions() {
		tstTriggerBoundsCheck(true, 4);
		for (int i = 0; i < sJitThreshold; i++) {
			tstTriggerBoundsCheck(false, 4);
		}
		tstTriggerBoundsCheck(true, 4);
	}
}
