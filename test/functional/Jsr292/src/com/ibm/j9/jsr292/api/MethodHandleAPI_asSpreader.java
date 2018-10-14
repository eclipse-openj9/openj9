/*******************************************************************************
 * Copyright (c) 2016, 2018 IBM Corp. and others
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
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

public class MethodHandleAPI_asSpreader {

	/**
	 * asSpreader test for a non-zero sized array to spread at the head of the argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_asSpreader_NonZeroSizedArray_AtHead() throws Throwable {
		SamePackageExample classSamePackage = new SamePackageExample();

		/* test with int array */
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addIntArrayToSum", MethodType.methodType(int.class, int.class, int.class, int.class, String.class, long.class, double.class));
		mhIntArray = mhIntArray.bindTo(classSamePackage);
		mhIntArray = mhIntArray.asSpreader(0, int[].class, 3);
		AssertJUnit.assertEquals(6, (int)mhIntArray.invokeExact(new int[] {1, 2, 3}, "argument4", 500L, 600.5D));

		/* test with long array */
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addLongArrayToSum", MethodType.methodType(long.class, long.class, long.class, String.class, long.class, double.class));
		mhLongArray = mhLongArray.bindTo(classSamePackage);
		mhLongArray = mhLongArray.asSpreader(0, long[].class, 2);
		AssertJUnit.assertEquals(6000000003L, (long)mhLongArray.invokeExact(new long[] {3000000001L, 3000000002L}, "argument3", 400L, 500.5D));

		/* test with String array */
		MethodHandle mhStringArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addStringArrayToString", MethodType.methodType(String.class, String.class, String.class, String.class, long.class, double.class));
		mhStringArray = mhStringArray.bindTo(classSamePackage);
		mhStringArray = mhStringArray.asSpreader(0, String[].class, 3);
		AssertJUnit.assertEquals("[a String array]", (String)mhStringArray.invokeExact(new String[] {"a ", "String ", "array"}, 400L, 500.5D));
	}

	/**
	 * asSpreader test for a non-zero sized array to spread in the middle of the argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_asSpreader_NonZeroSizedArray_InMiddle() throws Throwable {
		SamePackageExample classSamePackage = new SamePackageExample();

		/* test with int array */
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addIntArrayToSum", MethodType.methodType(int.class, long.class, double.class, int.class, int.class, int.class, String.class));
		mhIntArray = mhIntArray.bindTo(classSamePackage);
		mhIntArray = mhIntArray.asSpreader(2, int[].class, 3);
		AssertJUnit.assertEquals(12, (int)mhIntArray.invokeExact(100L,200.5D, new int[] {3, 4, 5}, "argument6"));

		/* test with long array */
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addLongArrayToSum", MethodType.methodType(long.class, int.class, double.class, long.class, long.class, String.class));
		mhLongArray = mhLongArray.bindTo(classSamePackage);
		mhLongArray = mhLongArray.asSpreader(2, long[].class, 2);
		AssertJUnit.assertEquals(6000000003L, (long)mhLongArray.invokeExact(100, 200.5D, new long[] {3000000001L, 3000000002L}, "argument5"));

		/* test with String array */
		MethodHandle mhStringArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addStringArrayToString", MethodType.methodType(String.class, long.class, int.class, String.class, String.class, String.class, int.class));
		mhStringArray = mhStringArray.bindTo(classSamePackage);
		mhStringArray = mhStringArray.asSpreader(2, String[].class, 3);
		AssertJUnit.assertEquals("[a String array]", (String)mhStringArray.invokeExact(1000000001L, 20, new String[] {"a ", "String ", "array"}, 60));
	}

	/**
	 * asSpreader test for a non-zero sized array to spread at the end of the argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_asSpreader_NonZeroSizedArray_AtEnd() throws Throwable {
		SamePackageExample classSamePackage = new SamePackageExample();

		/* test with int array */
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addIntArrayToSum", MethodType.methodType(int.class, String.class, long.class, double.class, int.class, int.class, int.class));
		mhIntArray = mhIntArray.bindTo(classSamePackage);
		mhIntArray = mhIntArray.asSpreader(3, int[].class, 3);
		AssertJUnit.assertEquals(15, (int)mhIntArray.invokeExact("argument1", 200L, 300.5D, new int[] {4, 5, 6}));

		/* test with long array */
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addLongArrayToSum", MethodType.methodType(long.class, int.class, String.class, double.class, long.class, long.class));
		mhLongArray = mhLongArray.bindTo(classSamePackage);
		mhLongArray = mhLongArray.asSpreader(3, long[].class, 2);
		AssertJUnit.assertEquals(6000000003L, (long)mhLongArray.invokeExact(100, "argument2", 300.5D, new long[] {3000000001L, 3000000002L}));

		/* test with String array */
		MethodHandle mhStringArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addStringArrayToString", MethodType.methodType(String.class, double.class, long.class, int.class, String.class, String.class, String.class));
		mhStringArray = mhStringArray.bindTo(classSamePackage);
		mhStringArray = mhStringArray.asSpreader(3, String[].class, 3);
		AssertJUnit.assertEquals("[a String array]", (String)mhStringArray.invokeExact(100.5D, 200L, 300, new String[] {"a ", "String ", "array"}));
	}

	/**
	 * asSpreader test for a zero sized array at the head of the argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_asSpreader_ZeroSizedArray_AtHead() throws Throwable {
		SamePackageExample classSamePackage = new SamePackageExample();

		/* test with int array */
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addIntArrayToSum", MethodType.methodType(int.class, int.class, int.class, int.class, String.class, long.class, double.class));
		mhIntArray = mhIntArray.bindTo(classSamePackage);
		mhIntArray = mhIntArray.asSpreader(0, int[].class, 0);
		AssertJUnit.assertEquals(9, (int)mhIntArray.invokeExact(new int[] {}, 2, 3, 4, "argument4", 500L, 600.5D));

		/* test with long array */
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addLongArrayToSum", MethodType.methodType(long.class, long.class, long.class, String.class, long.class, double.class));
		mhLongArray = mhLongArray.bindTo(classSamePackage);
		mhLongArray = mhLongArray.asSpreader(0, long[].class, 0);
		AssertJUnit.assertEquals(500L, (long)mhLongArray.invokeExact(new long[] {}, 200L, 300L, "argument4", 500L, 600.5D));

		/* test with String array */
		MethodHandle mhStringArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addStringArrayToString", MethodType.methodType(String.class, String.class, String.class, String.class, long.class, double.class));
		mhStringArray = mhStringArray.bindTo(classSamePackage);
		mhStringArray = mhStringArray.asSpreader(0, String[].class, 0);
		AssertJUnit.assertEquals("[a String array]", (String)mhStringArray.invokeExact(new String[] {}, "a ", "String ", "array", 500L, 600.5D));
	}

	/**
	 * asSpreader test for a zero sized array in the middle of the argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_asSpreader_ZeroSizedArray_InMiddle() throws Throwable {
		SamePackageExample classSamePackage = new SamePackageExample();

		/* test with int array */
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addIntArrayToSum", MethodType.methodType(int.class, long.class, double.class, int.class, int.class, int.class, String.class));
		mhIntArray = mhIntArray.bindTo(classSamePackage);
		mhIntArray = mhIntArray.asSpreader(2, int[].class, 0);
		AssertJUnit.assertEquals(12, (int)mhIntArray.invokeExact(100L,200.5D, new int[] {}, 3, 4, 5, "argument6"));

		/* test with long array */
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addLongArrayToSum", MethodType.methodType(long.class, int.class, double.class, long.class, long.class, String.class));
		mhLongArray = mhLongArray.bindTo(classSamePackage);
		mhLongArray = mhLongArray.asSpreader(2, long[].class, 0);
		AssertJUnit.assertEquals(6000000003L, (long)mhLongArray.invokeExact(100, 200.5D, new long[] {}, 3000000001L, 3000000002L, "argument5"));

		/* test with String array */
		MethodHandle mhStringArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addStringArrayToString", MethodType.methodType(String.class, long.class, int.class, String.class, String.class, String.class, int.class));
		mhStringArray = mhStringArray.bindTo(classSamePackage);
		mhStringArray = mhStringArray.asSpreader(2, String[].class, 0);
		AssertJUnit.assertEquals("[a String array]", (String)mhStringArray.invokeExact(1000000001L, 20, new String[] {}, "a ", "String ", "array", 60));
	}

	/**
	 * asSpreader test for a zero sized array at the end of the argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_asSpreader_ZeroSizedArray_AtEnd() throws Throwable {
		SamePackageExample classSamePackage = new SamePackageExample();

		/* test with int array */
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addIntArrayToSum", MethodType.methodType(int.class, String.class, long.class, double.class, int.class, int.class, int.class));
		mhIntArray = mhIntArray.bindTo(classSamePackage);
		mhIntArray = mhIntArray.asSpreader(6, int[].class, 0);
		AssertJUnit.assertEquals(15, (int)mhIntArray.invokeExact("argument1", 200L, 300.5D, 4, 5, 6, new int[] {}));

		/* test with long array */
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addLongArrayToSum", MethodType.methodType(long.class, int.class, String.class, double.class, long.class, long.class));
		mhLongArray = mhLongArray.bindTo(classSamePackage);
		mhLongArray = mhLongArray.asSpreader(5, long[].class, 0);
		AssertJUnit.assertEquals(9000000003L, (long)mhLongArray.invokeExact(100, "argument2", 300.5D, 4000000001L, 5000000002L, new long[] {}));

		/* test with String array */
		MethodHandle mhStringArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addStringArrayToString", MethodType.methodType(String.class, double.class, long.class, int.class, String.class, String.class, String.class));
		mhStringArray = mhStringArray.bindTo(classSamePackage);
		mhStringArray = mhStringArray.asSpreader(6, String[].class, 0);
		AssertJUnit.assertEquals("[a String array]", (String)mhStringArray.invokeExact(100.5D, 200L, 300, "a ", "String ", "array", new String[] {}));
	}
}
