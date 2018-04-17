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
package j9vm.test.hashCode.generator;

/**
 * Linear feedback shift register: XOR based random number generator
 *
 */
public class Lfsr {
	boolean shiftLeft = true;
	static final int GENERATOR = 0x04C11DB7;
	private static final int SHIFT_RIGHT_GENERATOR = 0xEDB88320;

	public int runLfsr(int initial, int iterations) {
		int newValue = initial;
		for (int i = 0; i < iterations; ++i) {
			if (shiftLeft) {
				if ((newValue & 0x80000000) != 0) {
					newValue <<= 1;
					newValue ^= GENERATOR;
				} else {
					newValue <<= 1;
				}
			} else {
				if ((newValue & 0x1) != 0) {
					newValue = (newValue >> 1) & 0x7fffffff;
					newValue ^= SHIFT_RIGHT_GENERATOR;
				} else {
					newValue = (newValue >> 1) & 0x7fffffff;
				}
			}
		}
		return newValue;
	}

	public int scrambleSeed(int initialValue) {
		int seed = runLfsr(initialValue, 40);
		return seed;
	}

	public void setShiftLeft(boolean doShiftLeft) {
		this.shiftLeft = doShiftLeft;
	}
}
