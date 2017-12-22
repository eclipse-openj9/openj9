/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
/*
 * Created on Jul 9, 2007
 *
 * To change the template for this generated file go to
 * Window>Preferences>Java>Code Generation>Code and Comments
 */
package j9vm.test.hash;

import java.util.Random;

/*
 * Perform a simple test for randomness of hash codes.
 * Tests how many bits in the hash code are on or off approximately
 * the same number of times.
 * This test can easily be fooled by any pattern that gives a normal
 * distribution of bits, but it can pick up some simple errors and
 * give a rough measure of hash quality.
 */
public class HashCodeDistributionTest extends HashCodeTestParent {

	static final int n = 1000000;
	
	/* Define the acceptable range for a ratio to be normally distributed.
	 * Ideally, each bit should have 0.50 probability.
	 * We'll accept anything between 0.40 and 0.60.
	 */
	static final double LOW_RATIO = 0.40;
	static final double HIGH_RATIO = 0.60;
	
	/* storing the temporary in a static makes it less likely that it will be optimized away */
	static Object obj;
	
	public HashCodeDistributionTest(int mode) {
		super(mode);
	}


	public static void main(String[] args) {
		new HashCodeDistributionTest(MODE_SYSTEM_GC).test();
	}
	
	public void test() {

		Random random = new Random(483948954L);
		
		int[] bitCount = new int[32];
		
		for (int i = 0; i < n; i++) {
			/* use a variety of sizes to mix things up */
			obj = new byte[random.nextInt(64)];
			int hash = obj.hashCode();
			
			for (int b = 0; b < bitCount.length; b++) {
				if (0 != (hash & (1 << b))) {
					bitCount[b] += 1;
				}
			}
		}
		
		int randomBits = 0;
		
		System.out.println("Hash code distribution");
		for (int i = 0; i < bitCount.length; i++) {
			double ratio = (double)bitCount[i] / (double)n;
			System.out.println("\tBit " + i + " = " + bitCount[i] + " (" + ratio + ")");
			
			if (ratio > LOW_RATIO && ratio < HIGH_RATIO) {
				randomBits += 1;
			}
		}
		System.out.println("" + randomBits + " bits are within the acceptable range (" + LOW_RATIO + " < x < " + HIGH_RATIO + ")");
		
		if (randomBits < 16) {
			throw new Error("Insufficiently random hash code (only " + randomBits + " bits appear to be random)");
		}
	
	}
	
}
