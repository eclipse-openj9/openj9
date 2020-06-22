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
public class SIMDOptTest{
	public static final Random rand = new Random();
	private int[] offsets = new int[0];
	private int cachedRandomNumber = -1;

	private void setCachedRandomNumber (int val){
		this.cachedRandomNumber = val;
	}

	public static int[] grow(int[] array, int minSize){
		int totalLength = array.length + minSize;
		int[] returnArray = new int[totalLength];
		System.arraycopy(array, 0, returnArray, 0, array.length);
		return returnArray;
	}

	private int returnCachedRandomNumber(){
		if (cachedRandomNumber == - 1)
			cachedRandomNumber = 100+rand.nextInt(100);
		return cachedRandomNumber;
	}
	/**
	 * One of the optimization applied to following method when JIT compiled at scorching optimization level
	 * is SIMD Optimization named SPMDKernelParallelization which vectorizes the loop in the method,
	 * Optimization was generating incorrect trees which was causing IBM Z evaluator to throw ASSERT.
	 * Apart from the ASSERT. This test aims to verify that specific transformation done by the
	 * SIMD Optimization is functionally correct.
	 *
	 * For more details see eclipse/openj9#9446
	 */
	private void testSimpleLoopSIMD(){
		final int token = returnCachedRandomNumber();
		int addnArrayLength = token >>> 1;
		offsets = grow(offsets, addnArrayLength);
		for (int i = 0; i < addnArrayLength-1; ++i)
			offsets[1 + i] = (1 + i) * token;
	}

	@Test
	public void testSIMDOptLoop(){
		for (int i = 0; i < 20; ++i){
			SIMDOptTest obj = new SIMDOptTest();
			for (int j=0; j < 10; ++j)
				obj.testSimpleLoopSIMD();
		}
		int token = 0B01000000;
		SIMDOptTest obj = new SIMDOptTest();
		obj.setCachedRandomNumber(token);
		obj.testSimpleLoopSIMD();
		AssertJUnit.assertEquals("Wrong value at offset 0",0, obj.offsets[0]);
		for (int i = 0; i < (token >>> 1)-1; ++i)
			AssertJUnit.assertEquals("Wrong value at offset "+(i+1),(1+i)*token, obj.offsets[1 + i]);
	}
}
