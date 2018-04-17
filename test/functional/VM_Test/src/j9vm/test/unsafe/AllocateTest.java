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
package j9vm.test.unsafe;

import sun.misc.Unsafe;

/* CMVC 190093 - this was formerly known as Allocate, but the class name must end in "Test" */
public class AllocateTest {
	static Unsafe myUnsafe;
	static final int memBuffSize = 100;
	final int allocSizeMax = 100;
	static long memBuff;
	static boolean suiteFail = false;
	static final int allocSizes[] = {0,1,2,3,4,5,7,8,9, 14,15,16, 31,32,33,48, 100, 1000, 10000, 100000};
	
	/**
	 * try allocating different numbers and sizes of blocks.
	 * Deallocate in different orders.
	 */
	public void testAllocate() {
		System.out.println("testAllocate");
		for (int i = 0; i < allocSizeMax; ++i) {
			memBuff = myUnsafe.allocateMemory(i);
			myUnsafe.freeMemory(memBuff);
		}
	}

	/**
	 * @testName: testRealloc1
	 * @testMethod: allocate memory, reallocate it, free it
	 * @testExpect: no exceptions
	 * @testVariants:
	 */
	
	public void testRealloc1() {
		final int minSize = 0;
		
		System.out.println("testReallocate: allocateMemory");
		long membuff2[] = new long[memBuffSize];
		for (int i = minSize; i < allocSizeMax; ++i) {
			System.out.println("testReallocate: allocate buffer "+i+ " new size "+i);
			membuff2[i] = myUnsafe.allocateMemory(i);
		}
		System.out.println("testReallocate: reallocateMemory");
		try {
			for (int i = minSize; i < allocSizeMax; ++i) {
				int newSize = i + 10;
				System.out.println("testReallocate: reallocate buffer "+i+" = "+Long.toHexString(membuff2[i])+ " new size "+newSize);
				membuff2[i] = myUnsafe.reallocateMemory(membuff2[i], newSize);
				if ((0 != i) && (0 == membuff2[i])) { /* should have got an exception */
					suiteFail = true;
				}
			} 
		} catch (Throwable e) {
			System.out.println("testReallocate: caught exception");
		}


		System.out.println("testReallocate: freeMemory");
		for (int i = minSize; i < allocSizeMax; ++i) {
			myUnsafe.freeMemory(membuff2[i]);
		}
	}

	/**
	 * @testName: testRealloc2
	 * @testMethod: allocate different sizes of memory, reallocate  to different sizes, don't free it
	 * @testExpect: no exceptions
	 * @testVariants:
	 */
	
	public void testRealloc2() {
		int asize, rsize;
		
		System.out.println("testRealloc2");
		for (int a=0; a < allocSizes.length; ++a) {
			for (int r=0; r < allocSizes.length; ++r) {
				asize = allocSizes[a];
				rsize = allocSizes[r];
				memBuff = myUnsafe.allocateMemory(asize);
				memBuff = myUnsafe.reallocateMemory(memBuff, rsize);
			}
		}
		
	}

	/** 
	 * @testName: testMultipleUnsafes
	  * @testMethod: create multiple sun.misc.Unsafe objects and do interleaved allocates and frees
		@testExpect: no exceptions or crashes
		@testVariants: none
		@throws IllegalAccessException 
	 */
	
	public void testMultipleUnsafes() throws IllegalAccessException {
		sun.misc.Unsafe[] unsafes = new Unsafe[memBuffSize];
		long membuff2[] = new long[memBuffSize];
		final int iterations = 20;

		System.out.println("testMultipleUnsafes");
		for (int i = 0; i < memBuffSize; ++i) {
			unsafes[i] = UnsafeObjectGetTest.getUnsafeInstance();
		}
		for (int j = 0; j < iterations; ++j) {
			System.out.println("testMultipleUnsafes: iteration "+j);
			for (int i = 0; i < memBuffSize; ++i) {
				int sizeSelect = (i + j ) % allocSizes.length;
				int victim = (i + j) % memBuffSize;
				membuff2[i] = unsafes[i].allocateMemory(allocSizes[sizeSelect]);
				if (0 != membuff2[victim]) {
					unsafes[victim].freeMemory(membuff2[victim]);
					membuff2[victim] = 0L;
				}
			}
		}
		for (int i = 0; i < memBuffSize; ++i) {
			if (0 != membuff2[i]) {
				unsafes[i].freeMemory(membuff2[i]);
			}
		}
	}
	/**
	 * test not freeing all memory
	 */
	public void testResidue1() {
		System.out.println("testResidue1");
		memBuff = myUnsafe.allocateMemory(10000);
	}
	
	public void testNegative1() {
		boolean fail = true;
		
		System.out.println("testNegative1");
		try {
			memBuff = myUnsafe.allocateMemory(-1000);
		} catch (Exception e) {
			fail = false;
			System.out.println("testNegative1: exception thrown");
		}
		suiteFail |= fail;
	}
	
	public static void main(String[] args) throws Exception {
		myUnsafe = UnsafeObjectGetTest.getUnsafeInstance();
		AllocateTest tester = new AllocateTest();
		tester.testAllocate();
		tester.testRealloc1();
		tester.testRealloc2();
		tester.testResidue1();
		tester.testNegative1();
		tester.testMultipleUnsafes();
		if (suiteFail) {
			System.err.println("One or more tests failed. Ignore memcheck errors");
			System.exit(1);
		}
	}
	
}
