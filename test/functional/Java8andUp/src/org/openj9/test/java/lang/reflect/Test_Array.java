package org.openj9.test.java.lang.reflect;

/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.reflect.Array;

@Test(groups = { "level.sanity" })
public class Test_Array {

	/**
	 * @tests java.lang.reflect.Array#get(java.lang.Object, int)
	 */
	@Test
	public void test_get() {
		int[] x = { 1 };
		Object ret = null;
		boolean thrown = false;
		try {
			ret = Array.get(x, 0);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", ((Integer)ret).intValue() == 1);
		try {
			ret = Array.get(new Object(), 0);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			ret = Array.get(x, 4);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#getBoolean(java.lang.Object, int)
	 */
	@Test
	public void test_getBoolean() {
		boolean[] x = { true };
		boolean ret = false;
		boolean thrown = false;
		try {
			ret = Array.getBoolean(x, 0);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", ret);
		try {
			ret = Array.getBoolean(new Object(), 0);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			ret = Array.getBoolean(x, 4);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#getByte(java.lang.Object, int)
	 */
	@Test
	public void test_getByte() {
		byte[] x = { 1 };
		byte ret = 0;
		boolean thrown = false;
		try {
			ret = Array.getByte(x, 0);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", ret == 1);
		try {
			ret = Array.getByte(new Object(), 0);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			ret = Array.getByte(x, 4);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#getChar(java.lang.Object, int)
	 */
	@Test
	public void test_getChar() {
		char[] x = { 1 };
		char ret = 0;
		boolean thrown = false;
		try {
			ret = Array.getChar(x, 0);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", ret == 1);
		try {
			ret = Array.getChar(new Object(), 0);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			ret = Array.getChar(x, 4);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#getDouble(java.lang.Object, int)
	 */
	@Test
	public void test_getDouble() {
		double[] x = { 1 };
		double ret = 0;
		boolean thrown = false;
		try {
			ret = Array.getDouble(x, 0);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", ret == 1);
		try {
			ret = Array.getDouble(new Object(), 0);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			ret = Array.getDouble(x, 4);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#getFloat(java.lang.Object, int)
	 */
	@Test
	public void test_getFloat() {
		float[] x = { 1 };
		float ret = 0;
		boolean thrown = false;
		try {
			ret = Array.getFloat(x, 0);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", ret == 1);
		try {
			ret = Array.getFloat(new Object(), 0);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			ret = Array.getFloat(x, 4);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#getInt(java.lang.Object, int)
	 */
	@Test
	public void test_getInt() {
		int[] x = { 1 };
		int ret = 0;
		boolean thrown = false;
		try {
			ret = Array.getInt(x, 0);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", ret == 1);
		try {
			ret = Array.getInt(new Object(), 0);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			ret = Array.getInt(x, 4);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#getLength(java.lang.Object)
	 */
	@Test
	public void test_getLength() {
		long[] x = { 1 };

		AssertJUnit.assertTrue("Returned incorrect length", Array.getLength(x) == 1);
		AssertJUnit.assertTrue("Returned incorrect length", Array.getLength(new Object[10000]) == 10000);
		try {
			Array.getLength(new Object());
		} catch (IllegalArgumentException e) {
			// Correct
			return;
		}
		AssertJUnit.assertTrue("Failed to throw exception when passed non-array", false);
	}

	/**
	 * @tests java.lang.reflect.Array#getLong(java.lang.Object, int)
	 */
	@Test
	public void test_getLong() {
		long[] x = { 1 };
		long ret = 0;
		boolean thrown = false;
		try {
			ret = Array.getLong(x, 0);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", ret == 1);
		try {
			ret = Array.getLong(new Object(), 0);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			ret = Array.getLong(x, 4);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#getShort(java.lang.Object, int)
	 */
	@Test
	public void test_getShort() {
		short[] x = { 1 };
		short ret = 0;
		boolean thrown = false;
		try {
			ret = Array.getShort(x, 0);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", ret == 1);
		try {
			ret = Array.getShort(new Object(), 0);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			ret = Array.getShort(x, 4);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#newInstance(java.lang.Class, int[])
	 */
	@Test
	public void test_newInstance() {
		int[][] x;
		int[] y = { 2 };
		int[][] z;

		x = (int[][])Array.newInstance(int[].class, y);
		AssertJUnit.assertTrue("Failed to instantiate array properly", x.length == 2);

		try {
			Array.newInstance(Void.TYPE, new int[10]);
			Assert.fail("No IllegalArgumentException thrown when Void.Type is passed as componentType");
		} catch (IllegalArgumentException e) {
			// Correct behavior
		}

		try {
			Array.newInstance(Integer.TYPE, new int[10]);
		} catch (IllegalArgumentException e) {
			Assert.fail("IllegalArgumentException thrown when Integer.Type is passed as componentType");
		}

		Class clazz = null;

		try {
			/* Incrementally creating an array with 255 dimensions */
			clazz = Array.newInstance(Object.class, new int[1]).getClass();
			for (int i = 0; i < 254; i++) {
				clazz = Array.newInstance(clazz, new int[1]).getClass();
			}
		} catch (IllegalArgumentException e) {
			Assert.fail("IllegalArgumentException should not be thrown when the array dimension doesn't exceed 255");
		}

		try {
			/* Incrementally creating an array with 256 dimensions */
			clazz = Array.newInstance(clazz, new int[1]).getClass();
			Assert.fail("IllegalArgumentException should be thrown when the array dimension exceeds 255");
		} catch (IllegalArgumentException e) {
			// Correct behavior
		}

		try {
			/* Directly creating an array with 255 dimensions */
			Array.newInstance(Object.class, new int[255]);
		} catch (IllegalArgumentException e) {
			Assert.fail("IllegalArgumentException should not be thrown when array dimension doesn't exceed 255");
		}

		try {
			/* Directly creating an array with 256 dimensions */
			Array.newInstance(Object.class, new int[256]);
			Assert.fail("IllegalArgumentException should be thrown when the array dimension exceeds 255");
		} catch (IllegalArgumentException e) {
			// Correct behavior
		}

		try {
			Array.newInstance(Object.class, new int[0]);
			Assert.fail(
					"IllegalArgumentException should be thrown when the specified dimensions argument is a zero-dimensional array");
		} catch (IllegalArgumentException e) {
			// Correct behavior
		}

		try {
			Array.newInstance(Object.class, new int[] { 0, Integer.MAX_VALUE });
		} catch (NegativeArraySizeException e) {
			Assert.fail(
					"NegativeArraySizeException should not be thrown when the specified array lengths are not negative");
		}

		try {
			Array.newInstance(Object.class, new int[] { -1 });
			Assert.fail("NegativeArraySizeException should be thrown when a specified array length is negative");
		} catch (NegativeArraySizeException e) {
			// Correct behavior
		}

		try {
			Array.newInstance(Object.class, new int[] { Integer.MIN_VALUE });
			Assert.fail("NegativeArraySizeException should be thrown when a specified array length is negative");
		} catch (NegativeArraySizeException e) {
			// Correct behavior
		}
	}

	/**
	 * @tests java.lang.reflect.Array#newInstance(java.lang.Class, int)
	 */
	@Test
	public void test_newInstance2() {
		int[] x;
		int[] y = { 5 };
		int[][] z;

		x = (int[])Array.newInstance(int.class, 100);
		AssertJUnit.assertTrue("Failed to instantiate array properly", x.length == 100);

		try {
			Array.newInstance(Void.TYPE, 10);
			Assert.fail("IllegalArgumentException should be thrown when Void.Type is passed as componentType");
		} catch (IllegalArgumentException e) {
			// Correct behavior
		}

		try {
			Array.newInstance(Integer.TYPE, 10);
		} catch (IllegalArgumentException e) {
			Assert.fail("IllegalArgumentException should not be thrown when Integer.Type is passed as componentType");
		}

		Class clazz = null;

		try {
			/* Incrementally creating an array with 255 dimensions */
			clazz = Array.newInstance(Object.class, 1).getClass();
			for (int i = 0; i < 254; i++) {
				clazz = Array.newInstance(clazz, 1).getClass();
			}
		} catch (IllegalArgumentException e) {
			Assert.fail("IllegalArgumentException should not be thrown when the array dimension doesn't exceed 255");
		}

		try {
			/* Incrementally creating an array with 256 dimensions */
			clazz = Array.newInstance(clazz, 1).getClass();
			Assert.fail("IllegalArgumentException should be thrown when the array dimension exceeds 255");
		} catch (IllegalArgumentException e) {
			// Correct behavior
		}

		try {
			/* Creating an array with negative array length */
			Array.newInstance(Object.class, -10);
			Assert.fail("NegativeArraySizeException should be thrown when the specified array length is negative");
		} catch (NegativeArraySizeException e) {
			// Correct behavior
		}
	}

	/**
	 * @tests java.lang.reflect.Array#set(java.lang.Object, int,
	 *        java.lang.Object)
	 */
	@Test
	public void test_set() {
		int[] x = { 0 };
		Object ret = null;
		boolean thrown = false;
		try {
			Array.set(x, 0, new Integer(1));
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", ((Integer)Array.get(x, 0)).intValue() == 1);
		try {
			Array.set(new Object(), 0, new Object());
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			Array.set(x, 4, new Integer(1));
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);

		thrown = false;
		try {
			Array.set(x, 0, new Object());
		} catch (IllegalArgumentException e) {
			/* Correct behaviour */
			thrown = true;
		}
		if (!thrown) {
			Assert.fail("Setting non-primitive element failed to throw exception");
		}

		/* [PR CMVC 87782] Behavior changed in 1.5 */
		// trying to put null in a primitive array causes
		// IllegalArgumentException in 1.5
		// but a NullPointerException in 1.4
		boolean exception = false;
		try {
			Array.set(new int[1], 0, null);

		} catch (IllegalArgumentException e) {
			exception = true;
		}
		AssertJUnit.assertTrue("expected exception not thrown", exception);
	}

	/**
	 * @tests java.lang.reflect.Array#setBoolean(java.lang.Object, int, boolean)
	 */
	@Test
	public void test_setBoolean() {
		boolean[] x = { false };
		Object ret = null;
		boolean thrown = false;
		try {
			Array.setBoolean(x, 0, true);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Failed to set correct value", Array.getBoolean(x, 0));
		try {
			Array.setBoolean(new Object(), 0, false);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			Array.setBoolean(x, 4, false);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#setByte(java.lang.Object, int, byte)
	 */
	@Test
	public void test_setByte() {
		byte[] x = { 0 };
		Object ret = null;
		boolean thrown = false;
		try {
			Array.setByte(x, 0, (byte)1);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", Array.getByte(x, 0) == 1);
		try {
			Array.setByte(new Object(), 0, (byte)9);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			Array.setByte(x, 4, (byte)9);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#setChar(java.lang.Object, int, char)
	 */
	@Test
	public void test_setChar() {
		char[] x = { 0 };
		Object ret = null;
		boolean thrown = false;
		try {
			Array.setChar(x, 0, (char)1);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", Array.getChar(x, 0) == 1);
		try {
			Array.setChar(new Object(), 0, (char)9);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			Array.setChar(x, 4, (char)9);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#setDouble(java.lang.Object, int, double)
	 */
	@Test
	public void test_setDouble() {
		double[] x = { 0 };
		Object ret = null;
		boolean thrown = false;
		try {
			Array.setDouble(x, 0, (double)1);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", Array.getDouble(x, 0) == 1);
		try {
			Array.setDouble(new Object(), 0, (double)9);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			Array.setDouble(x, 4, (double)9);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#setFloat(java.lang.Object, int, float)
	 */
	@Test
	public void test_setFloat() {
		float[] x = { 0.0f };
		Object ret = null;
		boolean thrown = false;
		try {
			Array.setFloat(x, 0, (float)1);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", Array.getFloat(x, 0) == 1);
		try {
			Array.setFloat(new Object(), 0, (float)9);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			Array.setFloat(x, 4, (float)9);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#setInt(java.lang.Object, int, int)
	 */
	@Test
	public void test_setInt() {
		int[] x = { 0 };
		Object ret = null;
		boolean thrown = false;
		try {
			Array.setInt(x, 0, (int)1);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", Array.getInt(x, 0) == 1);
		try {
			Array.setInt(new Object(), 0, (int)9);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			Array.setInt(x, 4, (int)9);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#setLong(java.lang.Object, int, long)
	 */
	@Test
	public void test_setLong() {
		long[] x = { 0 };
		Object ret = null;
		boolean thrown = false;
		try {
			Array.setLong(x, 0, (long)1);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", Array.getLong(x, 0) == 1);
		try {
			Array.setLong(new Object(), 0, (long)9);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			Array.setLong(x, 4, (long)9);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}

	/**
	 * @tests java.lang.reflect.Array#setShort(java.lang.Object, int, short)
	 */
	@Test
	public void test_setShort() {
		short[] x = { 0 };
		Object ret = null;
		boolean thrown = false;
		try {
			Array.setShort(x, 0, (short)1);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during get test: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Get returned incorrect value", Array.getShort(x, 0) == 1);
		try {
			Array.setShort(new Object(), 0, (short)9);
		} catch (IllegalArgumentException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Passing non-array failed to throw exception", false);
		thrown = false;
		try {
			Array.setShort(x, 4, (short)9);
		} catch (ArrayIndexOutOfBoundsException e) {
			// Correct behaviour
			thrown = true;
		}
		if (!thrown)
			AssertJUnit.assertTrue("Invalid index failed to throw exception", false);
	}
}
