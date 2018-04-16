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
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;

public class MethodHandleAPI_zero {
	/**
	 * zero test for the boolean type to check
	 * whether it returns false as the default value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public static void test_zero_BooleanType() throws Throwable {
		MethodHandle mhZero = MethodHandles.zero(boolean.class);
		AssertJUnit.assertEquals(false, (boolean)mhZero.invokeExact());
	}
	
	/**
	 * zero test for the char type to check
	 * whether it returns '\0' as the default value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public static void test_zero_CharType() throws Throwable {
		MethodHandle mhZero = MethodHandles.zero(char.class);
		AssertJUnit.assertEquals('\0', (char)mhZero.invokeExact());
	}
	
	/**
	 * zero test for the byte type to check
	 * whether it returns 0 as the default value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public static void test_zero_ByteType() throws Throwable {
		MethodHandle mhZero = MethodHandles.zero(byte.class);
		AssertJUnit.assertEquals(0, (byte)mhZero.invokeExact());
	}
	
	/**
	 * zero test for the short type to check
	 * whether it returns 0 as the default value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public static void test_zero_ShortType() throws Throwable {
		MethodHandle mhZero = MethodHandles.zero(short.class);
		AssertJUnit.assertEquals(0, (short)mhZero.invokeExact());
	}
	
	/**
	 * zero test for the int type to check
	 * whether it returns 0 as the default value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public static void test_zero_IntType() throws Throwable {
		MethodHandle mhZero = MethodHandles.zero(int.class);
		AssertJUnit.assertEquals(0, (int)mhZero.invokeExact());
	}
	
	/**
	 * zero test for the long type to check
	 * whether it returns 0 as the default value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public static void test_zero_LongType() throws Throwable {
		MethodHandle mhZero = MethodHandles.zero(long.class);
		AssertJUnit.assertEquals(0L, (long)mhZero.invokeExact());
	}
	
	/**
	 * zero test for the float type to check
	 * whether it returns 0 as the default value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public static void test_zero_FloatType() throws Throwable {
		MethodHandle mhZero = MethodHandles.zero(float.class);
		AssertJUnit.assertEquals(0F, (float)mhZero.invokeExact());
	}
	
	/**
	 * zero test for the double type to check
	 * whether it returns 0 as the default value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public static void test_zero_DoubleType() throws Throwable {
		MethodHandle mhZero = MethodHandles.zero(double.class);
		AssertJUnit.assertEquals(0D, (double)mhZero.invokeExact());
	}
	
	/**
	 * zero test for the void type to check
	 * whether it returns null as the default value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public static void test_zero_VoidType() throws Throwable {
		MethodHandle mhZero = MethodHandles.zero(void.class);
		mhZero.invokeExact();
	}
	
	/**
	 * zero test for the Object type to check
	 * whether it returns null as the default value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public static void test_zero_ObjectType() throws Throwable {
		MethodHandle mhZero = MethodHandles.zero(Object.class);
		AssertJUnit.assertEquals(null, (Object)mhZero.invokeExact());
	}
	
	/**
	 * zero test for the String type to check
	 * whether it returns null as the default value.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public static void test_zero_StringType() throws Throwable {
		MethodHandle mhZero = MethodHandles.zero(String.class);
		AssertJUnit.assertEquals(null, (String)mhZero.invokeExact());
	}
}
