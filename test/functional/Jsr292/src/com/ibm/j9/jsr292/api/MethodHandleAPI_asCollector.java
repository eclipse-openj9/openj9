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

public class MethodHandleAPI_asCollector {

	/**
	 * asCollector test for a non-zero sized array at the head of the argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_asCollector_NonZeroSizedArray_AtHead() throws Throwable {
		SamePackageExample classSamePackage = new SamePackageExample();

		/* test with int array */
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addIntArrayToSum", MethodType.methodType(int.class, int[].class, String.class, long.class, double.class));
		mhIntArray = mhIntArray.bindTo(classSamePackage);
		mhIntArray = mhIntArray.asCollector(0, int[].class, 3);
		AssertJUnit.assertEquals(6, (int)mhIntArray.invokeExact(1, 2, 3, "argument2", 300L, 400.5D));

		/* test with long array */
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addLongArrayToSum", MethodType.methodType(long.class, long[].class, String.class, long.class, double.class));
		mhLongArray = mhLongArray.bindTo(classSamePackage);
		mhLongArray = mhLongArray.asCollector(0, long[].class, 2);
		AssertJUnit.assertEquals(6000000003L, (long)mhLongArray.invokeExact(3000000001L, 3000000002L, "argument2", 300L, 400.5D));

		/* test with String array */
		MethodHandle mhStringArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addStringArrayToString", MethodType.methodType(String.class, String[].class, String.class, long.class, double.class));
		mhStringArray = mhStringArray.bindTo(classSamePackage);
		mhStringArray = mhStringArray.asCollector(0, String[].class, 5);
		AssertJUnit.assertEquals("[This is a String array]", (String)mhStringArray.invokeExact("This ", "is ", "a ", "String ", "array", "argument2", 300L, 400.5D));
	}

	/**
	 * asCollector test for a non-zero sized array in the middle of the argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_asCollector_NonZeroSizedArray_InMiddle() throws Throwable {
		SamePackageExample classSamePackage = new SamePackageExample();

		/* test with int array */
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addIntArrayToSum", MethodType.methodType(int.class, long.class, double.class, int[].class, String.class));
		mhIntArray = mhIntArray.bindTo(classSamePackage);
		mhIntArray = mhIntArray.asCollector(2, int[].class, 3);
		AssertJUnit.assertEquals(6, (int)mhIntArray.invokeExact(100L,200.5D, 1, 2, 3, "argument4"));

		/* test with long array */
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addLongArrayToSum", MethodType.methodType(long.class, int.class, double.class, long[].class, String.class));
		mhLongArray = mhLongArray.bindTo(classSamePackage);
		mhLongArray = mhLongArray.asCollector(2, long[].class, 2);
		AssertJUnit.assertEquals(6000000003L, (long)mhLongArray.invokeExact(100, 200.5D, 3000000001L, 3000000002L, "argument4"));

		/* test with String array */
		MethodHandle mhStringArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addStringArrayToString", MethodType.methodType(String.class, long.class, int.class, String[].class, int.class));
		mhStringArray = mhStringArray.bindTo(classSamePackage);
		mhStringArray = mhStringArray.asCollector(2, String[].class, 5);
		AssertJUnit.assertEquals("[This is a String array]", (String)mhStringArray.invokeExact(100L, 200, "This ", "is ", "a ", "String ", "array", 3));
	}

	/**
	 * asCollector test for a non-zero sized array at the end of the argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_asCollector_NonZeroSizedArray_AtEnd() throws Throwable {
		SamePackageExample classSamePackage = new SamePackageExample();

		/* test with int array */
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addIntArrayToSum", MethodType.methodType(int.class, String.class, long.class, double.class, int[].class));
		mhIntArray = mhIntArray.bindTo(classSamePackage);
		mhIntArray = mhIntArray.asCollector(3, int[].class, 3);
		AssertJUnit.assertEquals(6, (int)mhIntArray.invokeExact("argument1", 100L, 200.5D, 1, 2, 3));

		/* test with long array */
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addLongArrayToSum", MethodType.methodType(long.class, int.class, String.class, double.class, long[].class));
		mhLongArray = mhLongArray.bindTo(classSamePackage);
		mhLongArray = mhLongArray.asCollector(3, long[].class, 2);
		AssertJUnit.assertEquals(6000000003L, (long)mhLongArray.invokeExact(100, "argument2", 200.5D, 3000000001L, 3000000002L));

		/* test with String array */
		MethodHandle mhStringArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addStringArrayToString", MethodType.methodType(String.class, double.class, long.class, int.class, String[].class));
		mhStringArray = mhStringArray.bindTo(classSamePackage);
		mhStringArray = mhStringArray.asCollector(3, String[].class, 5);
		AssertJUnit.assertEquals("[This is a String array]", (String)mhStringArray.invokeExact(100.5D, 200L, 300, "This ", "is ", "a ", "String ", "array"));
	}

	/**
	 * asCollector test for a zero sized array at the head of the argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_asCollector_ZeroSizedArray_AtHead() throws Throwable {
		SamePackageExample classSamePackage = new SamePackageExample();

		/* test with int array */
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addIntArrayToSum", MethodType.methodType(int.class, int[].class, String.class, long.class, double.class));
		mhIntArray = mhIntArray.bindTo(classSamePackage);
		mhIntArray = mhIntArray.asCollector(0, int[].class, 0);
		AssertJUnit.assertEquals(0, (int)mhIntArray.invokeExact("argument2", 300L, 400.5D));

		/* test with long array */
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addLongArrayToSum", MethodType.methodType(long.class, long[].class, String.class, long.class, double.class));
		mhLongArray = mhLongArray.bindTo(classSamePackage);
		mhLongArray = mhLongArray.asCollector(0, long[].class, 0);
		AssertJUnit.assertEquals(0, (long)mhLongArray.invokeExact("argument2", 300L, 400.5D));

		/* test with String array */
		MethodHandle mhStringArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addStringArrayToString", MethodType.methodType(String.class, String[].class, String.class, long.class, double.class));
		mhStringArray = mhStringArray.bindTo(classSamePackage);
		mhStringArray = mhStringArray.asCollector(0, String[].class, 0);
		AssertJUnit.assertEquals("[]", (String)mhStringArray.invokeExact("argument2", 300L, 400.5D));

	}

	/**
	 * asCollector test for a zero sized array in the middle of the argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_asCollector_ZeroSizedArray_InMiddle() throws Throwable {
		SamePackageExample classSamePackage = new SamePackageExample();

		/* test with int array */
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addIntArrayToSum", MethodType.methodType(int.class, long.class, double.class, int[].class, String.class));
		mhIntArray = mhIntArray.bindTo(classSamePackage);
		mhIntArray = mhIntArray.asCollector(2, int[].class, 0);
		AssertJUnit.assertEquals(0, (int)mhIntArray.invokeExact(100L, 200.5D, "argument4"));

		/* test with long array */
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addLongArrayToSum", MethodType.methodType(long.class, int.class, double.class, long[].class, String.class));
		mhLongArray = mhLongArray.bindTo(classSamePackage);
		mhLongArray = mhLongArray.asCollector(2, long[].class, 0);
		AssertJUnit.assertEquals(0, (long)mhLongArray.invokeExact(100, 200.5D, "argument4"));

		/* test with String array */
		MethodHandle mhStringArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addStringArrayToString", MethodType.methodType(String.class, long.class, int.class, String[].class, int.class));
		mhStringArray = mhStringArray.bindTo(classSamePackage);
		mhStringArray = mhStringArray.asCollector(2, String[].class, 0);
		AssertJUnit.assertEquals("[]", (String)mhStringArray.invokeExact(100L, 200, 400));
	}

	/**
	 * asCollector test for a zero sized array at the end of the argument list
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" }, invocationCount=2)
	public void test_asCollector_ZeroSizedArray_AtEnd() throws Throwable {
		SamePackageExample classSamePackage = new SamePackageExample();

		/* test with int array */
		MethodHandle mhIntArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addIntArrayToSum", MethodType.methodType(int.class, String.class, long.class, double.class, int[].class));
		mhIntArray = mhIntArray.bindTo(classSamePackage);
		mhIntArray = mhIntArray.asCollector(3, int[].class, 0);
		AssertJUnit.assertEquals(0, (int)mhIntArray.invokeExact("argument1", 200L, 300.5D));

		/* test with long array */
		MethodHandle mhLongArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addLongArrayToSum", MethodType.methodType(long.class, int.class, String.class, double.class, long[].class));
		mhLongArray = mhLongArray.bindTo(classSamePackage);
		mhLongArray = mhLongArray.asCollector(3, long[].class, 0);
		AssertJUnit.assertEquals(0, (long)mhLongArray.invokeExact(100, "argument2", 300.5D));

		/* test with String array */
		MethodHandle mhStringArray = MethodHandles.lookup().findVirtual(SamePackageExample.class, "addStringArrayToString", MethodType.methodType(String.class, double.class, long.class, int.class, String[].class));
		mhStringArray = mhStringArray.bindTo(classSamePackage);
		mhStringArray = mhStringArray.asCollector(3, String[].class, 0);
		AssertJUnit.assertEquals("[]", (String)mhStringArray.invokeExact(100.5D, 200L, 300));
	}
}
