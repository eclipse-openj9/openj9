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
package j9vm.test.arraylets;

/*
 * Test allocation of arrays near powers of two.
 * This can expose errors in arraylet calculations, resulting in crashes (such as CMVC 168253).
 */
public class ArrayletAllocateTest {
	private static final int rangeSize = 32;
	
	//these are public statics to avoid compiler optimizations removing them
	public static byte[] bytes = null;
	public static short[] shorts = null;
	public static int[] ints = null;
	public static long[] longs = null;
	public static Object[] objects = null;
	
	public static void main(String[] args) {
		long maxMemory = Runtime.getRuntime().freeMemory();
		
		/* first, run the traditional version of the test */
		System.out.println("Testing array allocation...");
		long maxMemoryForFirstTest = maxMemory / 8 / 2;
		for (int logSize = 0; logSize < 31; logSize++) {
			int maxSize = 1 << logSize;
			/* maxMemory / 8 is approximately the largest long array we could allocate */
			if ((maxSize >= rangeSize) && (maxSize < maxMemoryForFirstTest)) {
				int minSize = Math.max(1, maxSize - rangeSize);
				System.out.println("\tTesting arrays from " + minSize + " to " + maxSize + "...");
				for (int size = minSize; size <= maxSize; size++) {
					test(new byte[size]);
					test(new short[size]);
					test(new int[size]);
					test(new long[size]);
					test(new Object[size]);
				}
			}
		}
		System.out.println("Array allocation tests passed.");
		/* second, run the variant where we hash each array and hold on to it for a moment in order to intermittently test that growing an object doesn't cause a problem in copy-forward */
		System.out.println("Testing hashed array allocation and growth...");
		long maxMemoryForSecondTest = maxMemoryForFirstTest / 2;
		for (int logSize = 0; logSize < 31; logSize++) {
			int maxSize = 1 << logSize;
			/* maxMemory / 8 is approximately the largest long array we could allocate */
			if ((maxSize >= rangeSize) && (maxSize < maxMemoryForSecondTest)) {
				int minSize = Math.max(1, maxSize - rangeSize);
				System.out.println("\tTesting arrays from " + minSize + " to " + maxSize + "...");
				for (int size = minSize; size <= maxSize; size++) {
					bytes = new byte[size];
					bytes.hashCode();
					test(bytes);
					shorts = new short[size];
					shorts.hashCode();
					test(shorts);
					ints = new int[size];
					ints.hashCode();
					test(ints);
					longs = new long[size];
					longs.hashCode();
					test(longs);
					objects = new Object[size];
					objects.hashCode();
					test(objects);
				}
			}
		}
		System.out.println("Hashed array allocation and growth tests passed.");
	}

	private static void test(byte[] array) {
		array[0] = 1;
		array[array.length - 1] = 1;
	}

	private static void test(short[] array) {
		array[0] = 1;
		array[array.length - 1] = 1;
	}
	
	private static void test(int[] array) {
		array[0] = 1;
		array[array.length - 1] = 1;
	}

	private static void test(long[] array) {
		array[0] = 1;
		array[array.length - 1] = 1;
	}

	private static void test(Object[] array) {
		array[0] = "1";
		array[array.length - 1] = "1";
	}
}
