/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package com.ibm.j9.jsr292.api;

import org.testng.annotations.Test;
import org.testng.Assert;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import static java.lang.invoke.MethodType.methodType;

public class MethodHandleAPI_arrayLength {
	/**
	 * arrayLength test for a null array type.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.extended" })
	public static void test_arrayLength_NullArray() {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(null);
		Assert.fail("The test case failed to detect a null array type");
	}
	
	/**
	 * arrayLength test for a non-array type
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_arrayLength_NonArray() {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(int.class);
		Assert.fail("The test case failed to detect that the passed-in argument is not an array type");
	}
	
	/**
	 * arrayLength test for an empty array (primitive type)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_EmptyPrimitiveArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(int[].class);
		Assert.assertEquals(methodType(int.class, int[].class), mhArrayLength.type());
		int[] intArray = new int[]{};
		Assert.assertEquals(0, (int)mhArrayLength.invokeExact(intArray));
	}
	
	/**
	 * arrayLength test for a boolean array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_BooleanArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(boolean[].class);
		Assert.assertEquals(methodType(int.class, boolean[].class), mhArrayLength.type());
		boolean[] boolArray = new boolean[]{true, false, true};
		Assert.assertEquals(3, (int)mhArrayLength.invokeExact(boolArray));
	}
	
	/**
	 * arrayLength test for a char array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_CharArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(char[].class);
		Assert.assertEquals(methodType(int.class, char[].class), mhArrayLength.type());
		char[] charArray = new char[]{'A', 'B', 'C', 'D'};
		Assert.assertEquals(4, (int)mhArrayLength.invokeExact(charArray));
	}
	
	/**
	 * arrayLength test for a byte array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_ByteArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(byte[].class);
		Assert.assertEquals(methodType(int.class, byte[].class), mhArrayLength.type());
		byte[] byteArray = new byte[]{(byte)1, (byte)2, (byte)3, (byte)4, (byte)5};
		Assert.assertEquals(5, (int)mhArrayLength.invokeExact(byteArray));
	}
	
	/**
	 * arrayLength test for a short array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_ShortArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(short[].class);
		Assert.assertEquals(methodType(int.class, short[].class), mhArrayLength.type());
		short[] shortArray = new short[]{(short)10, (short)20, (short)30, (short)40, (short)50, (short)60};
		Assert.assertEquals(6, (int)mhArrayLength.invokeExact(shortArray));
	}
	
	/**
	 * arrayLength test for an int array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_IntArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(int[].class);
		Assert.assertEquals(methodType(int.class, int[].class), mhArrayLength.type());
		int[] intArray = new int[]{111, 222, 333, 444, 555, 666, 777};
		Assert.assertEquals(7, (int)mhArrayLength.invokeExact(intArray));
	}
	
	/**
	 * arrayLength test for a long array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_LongArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(long[].class);
		Assert.assertEquals(methodType(int.class, long[].class), mhArrayLength.type());
		long[] longArray = new long[]{1000l, 2000l, 3000l, 4000l, 5000l, 6000l, 7000l, 8000l};
		Assert.assertEquals(8, (int)mhArrayLength.invokeExact(longArray));
	}
	
	/**
	 * arrayLength test for a float array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_FloatArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(float[].class);
		Assert.assertEquals(methodType(int.class, float[].class), mhArrayLength.type());
		float[] floatArray = new float[]{1.1f, 2.2f, 3.3f, 4.4f};
		Assert.assertEquals(4, (int)mhArrayLength.invokeExact(floatArray));
	}
	
	/**
	 * arrayLength test for a double array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_DoubleArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(double[].class);
		Assert.assertEquals(methodType(int.class, double[].class), mhArrayLength.type());
		double[] doubleArray = new double[]{1111.22d, 2222.33d, 3333.44d, 4444.55d, 5555.66d, 6666.77d};
		Assert.assertEquals(6, (int)mhArrayLength.invokeExact(doubleArray));
	}
	
	/**
	 * arrayLength test for an empty array (String type)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_EmptyStringArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(String[].class);
		Assert.assertEquals(methodType(int.class, String[].class), mhArrayLength.type());
		String[] stringArray = new String[]{};
		Assert.assertEquals(0, (int)mhArrayLength.invokeExact(stringArray));
	}
	
	/**
	 * arrayLength test for a String array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_StringArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(String[].class);
		Assert.assertEquals(methodType(int.class, String[].class), mhArrayLength.type());
		String[] stringArray = new String[]{"str1", "str2", "str3", "str4", "str5"};
		Assert.assertEquals(5, (int)mhArrayLength.invokeExact(stringArray));
	}
	
	/**
	 * arrayLength test for an empty array (Object type)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_EmptyObjectArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(Object[].class);
		Assert.assertEquals(methodType(int.class, Object[].class), mhArrayLength.type());
		Object[] objectArray = new Object[]{};
		Assert.assertEquals(0, (int)mhArrayLength.invokeExact(objectArray));
	}
	
	/**
	 * arrayLength test for a Object array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_ObjectArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(Object[].class);
		Assert.assertEquals(methodType(int.class, Object[].class), mhArrayLength.type());
		Object[] objectArray = new Object[]{new Object(), new Object(), new Object(), new Object()};
		Assert.assertEquals(4, (int)mhArrayLength.invokeExact(objectArray));
	}
	
	/**
	 * arrayLength test for a 2-dimensional int array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_2D_IntArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(int[][].class);
		Assert.assertEquals(methodType(int.class, int[][].class), mhArrayLength.type());
		int[][] intArray = new int[][]{{1, 2, 3}, {4, 5, 6}, {7, 8, 9}};
		Assert.assertEquals(3, (int)mhArrayLength.invokeExact(intArray));
	}
	
	/**
	 * arrayLength test for a 2-dimensional String array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayLength_2D_StringArray() throws Throwable {
		MethodHandle mhArrayLength = MethodHandles.arrayLength(String[][].class);
		Assert.assertEquals(methodType(int.class, String[][].class), mhArrayLength.type());
		String[][] stringArray = new String[][]{{"str1", "str2", "str3"}, {"str4", "str5", "str6"}, {"str7", "str8", "str9"}};
		Assert.assertEquals(3, (int)mhArrayLength.invokeExact(stringArray));
	}
}
