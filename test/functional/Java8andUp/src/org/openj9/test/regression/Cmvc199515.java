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
package org.openj9.test.regression;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.lang.reflect.Field;
import java.util.Random;
import sun.misc.Unsafe;

/*
 * This tests cmvc 199515 where copying an array of longs failed on x86-32 
 * due to the use of floating point operations to copy each long atomically.
 * When the longs were treated as doubles and contained 0x7ff or 0x000 in the exponent
 * the highest bit of the fraction (bit 51) was flipped when popped from the fpu stack.
 */

@Test(groups = {"level.extended", "req.cmvc.199515"})
public class Cmvc199515 {
	
	@Test
	public void testCmvc199515() throws Exception {

		Random rng = new Random(0);
		for (int arraySize = 1; arraySize <= 128; arraySize++) {
			long[] dataset = new long[arraySize];

			for (int i = 0; i < arraySize; i++) {
				if ((i % 2) == 0) {
					/* NaN doubles (0x7FF exponent) */
					dataset[i] = 0x7FF0000000000000L | rng.nextLong();
				} else {
					/* signed zero and subnormal doubles (0x000 exponent) */
					dataset[i] = 0x000FFFFFFFFFFFFFL & rng.nextLong();
				}
			}

			
			/* test arraycopy */
			long[] testLongArrayCopy = new long[arraySize];
			System.arraycopy(dataset, 0, testLongArrayCopy, 0, arraySize);

			for (int i = 0; i < arraySize; i++) {
				AssertJUnit.assertEquals("System.arraycopy failed dataset[" + i + "] != testLongArrayCopy[" + i + "]",
						dataset[i], testLongArrayCopy[i]);
			}
			
			
			/* test arraycopy copyBackwardU64 */
			if ((arraySize % 2) == 0) {
				System.arraycopy(testLongArrayCopy, 0, testLongArrayCopy, arraySize / 2, arraySize / 2);
				for (int i = 0; i < arraySize / 2; i++) {
					int j = arraySize / 2 + i;
					AssertJUnit.assertEquals("System.arraycopy failed testLongArrayCopy[" + i + "] != testLongArrayCopy[" + j + "]",
							testLongArrayCopy[i], testLongArrayCopy[j]);
				}
			}
			

			/* test array clone */
			long[] testLongArrayClone = dataset.clone();

			for (int i = 0; i < arraySize; i++) {
				AssertJUnit.assertEquals("clone array failed dataset[" + i + "] != testLongArrayClone[" + i + "]",
						dataset[i], testLongArrayClone[i]);
			}

			
			/* test Unsafe copyMemory */
			long[] testLongArrayUnsafeCopy = new long[arraySize];

			Unsafe myUnsafe = getUnsafeInstance();
			myUnsafe.copyMemory(dataset, myUnsafe.arrayBaseOffset(dataset.getClass()), 
					testLongArrayUnsafeCopy, myUnsafe.arrayBaseOffset(testLongArrayUnsafeCopy.getClass()), 
					8 * arraySize);

			for (int i = 0; i < arraySize; i++) {
				AssertJUnit.assertEquals("Unsafe copyMemory dataset[" + i + "] != testLongArrayUnsafeCopy[" + i + "]",
						dataset[i], testLongArrayUnsafeCopy[i]);
			}

		}
	}

	/* use reflection to bypass the security manager */
	protected static Unsafe getUnsafeInstance() throws IllegalAccessException {
		Field[] staticFields = Unsafe.class.getDeclaredFields();
		for (Field field : staticFields) {
			if (field.getType() == Unsafe.class) {
				field.setAccessible(true);
				return (Unsafe) field.get(Unsafe.class);
			}
		}
		throw new Error("Unable to find an instance of Unsafe");
	}
}
