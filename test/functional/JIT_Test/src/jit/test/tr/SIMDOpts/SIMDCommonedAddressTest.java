/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
package jit.test.tr.SIMDOpts;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.util.Random;

@Test(groups = { "level.sanity","component.jit" })
public class SIMDCommonedAddressTest {
	public static final Random rand = new Random();
	public static int LOWER_BOUND = 200;
	public static int UPPER_BOUND = 3000;
	public int [] firstArray;
	public int [] secondArray;

	private void initArray (int [] array, Random random, int lowBound, int upperBound) {
		for(int i=0; i<array.length; i++) {
			array[i] = random.nextInt(upperBound - lowBound + 1) + lowBound;
		}
	}

	void allocate() {
		int size = rand.nextInt(UPPER_BOUND - LOWER_BOUND + 1) + LOWER_BOUND;
		firstArray = new int[size];
		secondArray = new int[size];
	}


	void testSIMDCommonedAddress(int [] first, int [] second, int N) {
		for(int i=0; i<N - 1; i++) {
			second[i] = first[i+1] * 2;
		}
	}

	void verifyArray() {
		for(int i=0; i<firstArray.length - 1; i++) {
			AssertJUnit.assertEquals("Wrong value at index : " + (i+1), firstArray[i+1]*2, secondArray[i]); 
		}
	}

	@Test
	public void testSIMDCommonedAddressTest(){
		for (int i = 0; i < 15; ++i){
			SIMDCommonedAddressTest obj = new SIMDCommonedAddressTest();
			for (int j=0; j < 10; ++j) {
				obj.allocate();
				obj.initArray(obj.firstArray, obj.rand, LOWER_BOUND, UPPER_BOUND);
				obj.testSIMDCommonedAddress(obj.firstArray, obj.secondArray, obj.firstArray.length);
			}
		}
		SIMDCommonedAddressTest obj = new SIMDCommonedAddressTest();
		obj.allocate();
		obj.initArray(obj.firstArray, obj.rand, LOWER_BOUND, UPPER_BOUND);
		obj.testSIMDCommonedAddress(obj.firstArray, obj.secondArray, obj.firstArray.length);
		obj.verifyArray();
	}
}
