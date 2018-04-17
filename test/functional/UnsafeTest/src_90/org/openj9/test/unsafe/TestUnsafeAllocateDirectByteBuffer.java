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
package org.openj9.test.unsafe;

import org.testng.AssertJUnit;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import jdk.internal.misc.Unsafe;

@Test(groups = { "level.sanity" })
public class TestUnsafeAllocateDirectByteBuffer extends UnsafeTestBase {
	private static Logger logger = Logger.getLogger(TestUnsafeAllocateDirectByteBuffer.class);
	
	protected TestUnsafeAllocateDirectByteBuffer(String scenario) {
		super(scenario);
	}
	
	/* get logger to use, for child classes to report with their class name instead of UnsafeTestBase */
	protected Logger getLogger() {
		return logger;
	}

	@Override
	@BeforeMethod
	protected void setUp() throws Exception {
		myUnsafe = getUnsafeInstance2();
	}

	/**
	 * try allocating different numbers and sizes of blocks. Deallocate in
	 * different orders.
	 */
	public void testAllocateDBB() throws Exception {
		int allocSizeMax = 100;
		long memBuff;
		logger.debug("testAllocateDBB");
		for (int i = 0; i < allocSizeMax; ++i) {
			memBuff = myUnsafe.allocateDBBMemory(i);
			AssertJUnit.assertTrue(memBuff != 0);
			myUnsafe.freeDBBMemory(memBuff);
		}
	}

	/**
	 * @testName: testReallocDBB1
	 * @testMethod: allocate memory, reallocate it, free it
	 * @testExpect: no exceptions
	 * @testVariants:
	 */

	public void testReallocDBB1() throws Exception {
		final int minSize = 0;
		int memBuffSize = 100;
		int allocSizeMax = 100;

		myUnsafe = Unsafe.getUnsafe();
		logger.debug("testReallocateDBB1: allocateDBBMemory");
		long membuff2[] = new long[memBuffSize];
		for (int i = minSize; i < allocSizeMax; ++i) {
			logger.debug("testReallocateDBB1: allocate buffer " + i + " new size "
					+ i);
			membuff2[i] = myUnsafe.allocateDBBMemory(i);
			AssertJUnit.assertTrue(membuff2[i] != 0);
		}
		logger.debug("testReallocateDBB: reallocateDBBMemory");
		for (int i = minSize; i < allocSizeMax; ++i) {
			logger.debug("testReallocateDBB: reallocate buffer " + i + " = "
					+ Long.toHexString(membuff2[i]) + " new size " + i);
			membuff2[i] = myUnsafe.reallocateDBBMemory(membuff2[i], i);
			
			if (minSize == i) {
				AssertJUnit.assertTrue(0 == membuff2[i]);
			} else {
				AssertJUnit.assertTrue(membuff2[i] != 0);
			}
		}

		logger.debug("testReallocateDBB: freeDBBMemory");
		for (int i = minSize; i < allocSizeMax; ++i) {
			myUnsafe.freeDBBMemory(membuff2[i]);
		}
	}

	/**
	 * @testName: testReallocDBB2
	 * @testMethod: allocate different sizes of memory, reallocate to different
	 *              sizes, don't free it
	 * @testExpect: no exceptions
	 * @testVariants:
	 */

	public void testReallocDBB2() throws Exception {
		int asize, rsize;
		long memBuff;
		int allocSizes[] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 14, 15, 16, 31, 32, 33,
				48, 100, 1000, 10000, 100000 };

		logger.debug("testReallocDBB2");
		myUnsafe = Unsafe.getUnsafe();
		for (int a = 0; a < allocSizes.length; ++a) {
			for (int r = 0; r < allocSizes.length; ++r) {
				asize = allocSizes[a];
				rsize = allocSizes[r];
				memBuff = myUnsafe.allocateDBBMemory(asize);
				AssertJUnit.assertTrue(memBuff != 0);
				
				memBuff = myUnsafe.reallocateDBBMemory(memBuff, rsize);
				if (0 == rsize) {
					AssertJUnit.assertTrue(0 == memBuff);
				} else {
					AssertJUnit.assertTrue(memBuff != 0);
				}
				
				myUnsafe.freeDBBMemory(memBuff);
			}
		}

	}

	/**
	 * @testName: testMultipleUnsafes
	 * @testMethod: create multiple sun.misc.Unsafe objects and do interleaved
	 *              allocates and frees
	 * @testExpect: no exceptions or crashes
	 * @testVariants: none
	 */

	public void testMultipleUnsafes() throws Exception {
		int memBuffSize = 100;
		jdk.internal.misc.Unsafe[] unsafes = new Unsafe[memBuffSize];
		long membuff2[] = new long[memBuffSize];
		final int iterations = 20;

		int allocSizes[] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 14, 15, 16, 31, 32, 33,
				48, 100, 1000, 10000, 100000 };

		logger.debug("testMultipleUnsafes");
		for (int i = 0; i < memBuffSize; ++i) {
			unsafes[i] = Unsafe.getUnsafe();
		}
		for (int j = 0; j < iterations; ++j) {
			logger.debug("testMultipleUnsafes: iteration " + j);
			for (int i = 0; i < memBuffSize; ++i) {
				int sizeSelect = (i + j) % allocSizes.length;
				int victim = (i + j) % memBuffSize;
				membuff2[i] = unsafes[i]
						.allocateDBBMemory(allocSizes[sizeSelect]);
				AssertJUnit.assertTrue(membuff2[i] != 0);
				if (0 != membuff2[victim]) {
					unsafes[victim].freeDBBMemory(membuff2[victim]);
					membuff2[victim] = 0L;
				}
			}
		}
		for (int i = 0; i < memBuffSize; ++i) {
			if (0 != membuff2[i]) {
				unsafes[i].freeDBBMemory(membuff2[i]);
			}
		}
	}

	/**
	 * test not freeing all memory
	 */
	public void testResidue1() throws Exception {
		myUnsafe = Unsafe.getUnsafe();
		logger.debug("testResidue1");
		myUnsafe.allocateDBBMemory(10000);
	}

	public void testNegative1() throws Exception {
		myUnsafe = Unsafe.getUnsafe();
		logger.debug("testNegative1");
		try {
			myUnsafe.allocateDBBMemory(-1000);
		} catch (java.lang.IllegalArgumentException e) {
			logger.debug("testNegative1: exception thrown", e);
		}
	}

}
