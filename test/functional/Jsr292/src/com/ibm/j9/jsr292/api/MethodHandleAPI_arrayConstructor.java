/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
import java.util.Arrays;

import static java.lang.invoke.MethodType.methodType;

public class MethodHandleAPI_arrayConstructor {
	/**
	 * arrayConstructor test for a null array type.
	 */
	@Test(expectedExceptions = NullPointerException.class, groups = { "level.extended" })
	public static void test_arrayConstructor_NullArray() {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(null);
		Assert.fail("The test case failed to detect a null array type");
	}
	
	/**
	 * arrayConstructor test for a non-array type
	 */
	@Test(expectedExceptions = IllegalArgumentException.class, groups = { "level.extended" })
	public static void test_arrayConstructor_NonArray() {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(int.class);
		Assert.fail("The test case failed to detect that the passed-in argument is not an array type");
	}
	
	/**
	 * arrayConstructor test for an empty array (primitive type)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_EmptyPrimitiveArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(int[].class);
		Assert.assertEquals(methodType(int[].class, int.class), mhArrayConstructor.type());
		int[] intArray = (int[])mhArrayConstructor.invokeExact(0);
		Assert.assertEquals(0, intArray.length);
	}
	
	/**
	 * arrayConstructor test for a boolean array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_BooleanArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(boolean[].class);
		Assert.assertEquals(methodType(boolean[].class, int.class), mhArrayConstructor.type());
		boolean[] boolArray = (boolean[])mhArrayConstructor.invokeExact(1);
		Assert.assertEquals(1, boolArray.length);
	}
	
	/**
	 * arrayConstructor test for a char array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_CharArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(char[].class);
		Assert.assertEquals(methodType(char[].class, int.class), mhArrayConstructor.type());
		char[] charArray = (char[])mhArrayConstructor.invokeExact(2);
		Assert.assertEquals(2, charArray.length);
	}
	
	/**
	 * arrayConstructor test for a byte array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_ByteArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(byte[].class);
		Assert.assertEquals(methodType(byte[].class, int.class), mhArrayConstructor.type());
		byte[] byteArray = (byte[])mhArrayConstructor.invokeExact(3);
		Assert.assertEquals(3, byteArray.length);
	}
	
	/**
	 * arrayConstructor test for a short array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_ShortArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(short[].class);
		Assert.assertEquals(methodType(short[].class, int.class), mhArrayConstructor.type());
		short[] shortArray = (short[])mhArrayConstructor.invokeExact(4);
		Assert.assertEquals(4, shortArray.length);
	}
	
	/**
	 * arrayConstructor test for an int array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_IntArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(int[].class);
		Assert.assertEquals(methodType(int[].class, int.class), mhArrayConstructor.type());
		int[] intArray = (int[])mhArrayConstructor.invokeExact(5);
		Assert.assertEquals(5, intArray.length);
	}
	
	/**
	 * arrayConstructor test for a long array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_LongArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(long[].class);
		Assert.assertEquals(methodType(long[].class, int.class), mhArrayConstructor.type());
		long[] longArray = (long[])mhArrayConstructor.invokeExact(6);
		Assert.assertEquals(6, longArray.length);
	}
	
	/**
	 * arrayConstructor test for a float array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_FloatArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(float[].class);
		Assert.assertEquals(methodType(float[].class, int.class), mhArrayConstructor.type());
		float[] floatArray = (float[])mhArrayConstructor.invokeExact(7);
		Assert.assertEquals(7, floatArray.length);
	}
	
	/**
	 * arrayConstructor test for a double array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_DoubleArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(double[].class);
		Assert.assertEquals(methodType(double[].class, int.class), mhArrayConstructor.type());
		double[] doubleArray = (double[])mhArrayConstructor.invokeExact(7);
		Assert.assertEquals(7, doubleArray.length);
	}
	
	/**
	 * arrayConstructor test for an empty array (String type)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_EmptyStringArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(String[].class);
		Assert.assertEquals(methodType(String[].class, int.class), mhArrayConstructor.type());
		String[] stringArray = (String[])mhArrayConstructor.invokeExact(0);
		Assert.assertEquals(0, stringArray.length);
	}
	
	/**
	 * arrayConstructor test for a String array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_StringArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(String[].class);
		Assert.assertEquals(methodType(String[].class, int.class), mhArrayConstructor.type());
		String[] stringArray = (String[])mhArrayConstructor.invokeExact(3);
		Assert.assertEquals(3, stringArray.length);
	}
	
	/**
	 * arrayConstructor test for a StringBuffer array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_StringBufferArray() throws Throwable {
		MethodHandle arrayLength = MethodHandles.arrayLength(StringBuffer[].class);
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(StringBuffer[].class);
		Assert.assertEquals(mhArrayConstructor.type(), methodType(StringBuffer[].class, int.class));
		StringBuffer[] stringBufferArray = (StringBuffer[])mhArrayConstructor.invokeExact(10);
		Assert.assertEquals((int)arrayLength.invokeExact(stringBufferArray), 10);
	}
	
	/**
	 * arrayConstructor test for an empty array (Object type)
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_EmptyObjectArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(Object[].class);
		Assert.assertEquals(methodType(Object[].class, int.class), mhArrayConstructor.type());
		Object[] objectArray = (Object[])mhArrayConstructor.invokeExact(0);
		Assert.assertEquals(0, objectArray.length);
	}
	
	/**
	 * arrayConstructor test for a Object array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_ObjectArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(Object[].class);
		Assert.assertEquals(methodType(Object[].class, int.class), mhArrayConstructor.type());
		Object[] objectArray = (Object[])mhArrayConstructor.invokeExact(4);
		Assert.assertEquals(4, objectArray.length);
	}
	
	/**
	 * arrayConstructor test for a 2-dimensional int array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_2D_IntArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(int[][].class);
		Assert.assertEquals(methodType(int[][].class, int.class), mhArrayConstructor.type());
		int[][] intArray = (int[][])mhArrayConstructor.invokeExact(4);
		Assert.assertEquals(4, intArray.length);
	}
	
	/**
	 * arrayConstructor test for a 2-dimensional String array
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount = 2)
	public static void test_arrayConstructor_2D_StringArray() throws Throwable {
		MethodHandle mhArrayConstructor = MethodHandles.arrayConstructor(String[][].class);
		Assert.assertEquals(methodType(String[][].class, int.class), mhArrayConstructor.type());
		String[][] stringArray = (String[][])mhArrayConstructor.invokeExact(4);
		Assert.assertEquals(4, stringArray.length);
	}
}
