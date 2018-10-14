package org.openj9.test.java.lang.invoke;

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
import org.testng.AssertJUnit;
import java.lang.invoke.*;
import static java.lang.invoke.MethodHandles.*;
import static java.lang.invoke.MethodType.*;

public class Test_AdaptorTests {
	public static int subtract(int x, int y) { return x - y; }
	public static int argStringReturnInt1(String s) { return 1; }
	public static int noArgReturnInt1() { return 1; }
	public static void argStringReturnVoid(String s) { return; }
	public static void noArgReturnVoid() { return; }
	public static int argIntStringReturnInt1(int i, String s) { return 1; }

	@Test(groups = { "level.sanity" })
	public void test_CollectArguments() throws Throwable {
		MethodType II_I_type = methodType(int.class, int.class, int.class);
		MethodType S_I_type = methodType(int.class, String.class);
		MethodType V_I_type = methodType(int.class);

		MethodHandle target = publicLookup().findStatic(Test_AdaptorTests.class, "subtract", II_I_type);
		MethodHandle filter = publicLookup().findStatic(Test_AdaptorTests.class, "argStringReturnInt1", S_I_type);
		MethodHandle filter2 = publicLookup().findStatic(Test_AdaptorTests.class, "noArgReturnInt1", V_I_type);

		// Valid pos (0)
		{
			MethodHandle adapter = collectArguments(target, 0, filter);
			AssertJUnit.assertEquals(adapter.type(), methodType(int.class, String.class, int.class));
			int result = (int)adapter.invokeExact("Some string", 1);
			AssertJUnit.assertEquals(0, result);
		}
		// Valid pos (1)
		{
			MethodHandle adapter = collectArguments(target, 1, filter);
			AssertJUnit.assertEquals(adapter.type(), methodType(int.class, int.class, String.class));
			int result = (int)adapter.invokeExact(2, "Some string");
			AssertJUnit.assertEquals(1, result);
		}
		// No arguments filter
		{
			MethodHandle adapter = collectArguments(target, 1, filter2);
			AssertJUnit.assertEquals(adapter.type(), methodType(int.class, int.class));
			int result = (int)adapter.invokeExact(2);
			AssertJUnit.assertEquals(1, result);
		}
	}

	@Test(groups = { "level.sanity" })
	public void test_CollectArgumentsNegative() throws Throwable {
		MethodType II_I_type = methodType(int.class, int.class, int.class);
		MethodType IS_I_type = methodType(int.class, int.class, String.class);
		MethodType S_I_type = methodType(int.class, String.class);
		MethodType V_I_type = methodType(int.class);

		MethodHandle target = publicLookup().findStatic(Test_AdaptorTests.class, "subtract", II_I_type);
		MethodHandle target2 = publicLookup().findStatic(Test_AdaptorTests.class, "argIntStringReturnInt1", IS_I_type);
		MethodHandle filter = publicLookup().findStatic(Test_AdaptorTests.class, "argStringReturnInt1", S_I_type);
		MethodHandle filter2 = publicLookup().findStatic(Test_AdaptorTests.class, "noArgReturnInt1", V_I_type);

		// Invalid pos (2)
		{
			boolean correctExceptionThrown = false;
			try {
				collectArguments(target, 2, filter);
			} catch (IllegalArgumentException x) {
				correctExceptionThrown = true;
			}
			AssertJUnit.assertTrue(correctExceptionThrown);
		}
		// Invalid negative pos (-1)
		{
			boolean IAEThrown = false;
			try {
				collectArguments(target, -1, filter);
			} catch (IllegalArgumentException x) {
				IAEThrown = true;
			}
			AssertJUnit.assertTrue(IAEThrown);
		}
		// Non-matching filter return type and target arg
		{
			boolean IAEThrown = false;
			try {
				collectArguments(target2, 1, filter);
			} catch (IllegalArgumentException x) {
				IAEThrown = true;
			}
			AssertJUnit.assertTrue(IAEThrown);
		}
		// target is null
		{
			boolean NPEThrown = false;
			try {
				collectArguments(null, 0, filter);
			} catch (NullPointerException x) {
				NPEThrown = true;
			}
			AssertJUnit.assertTrue(NPEThrown);
		}
		// filter is null
		{
			boolean NPEThrown = false;
			try {
				collectArguments(target, 0, null);
			} catch (NullPointerException x) {
				NPEThrown = true;
			}
			AssertJUnit.assertTrue(NPEThrown);
		}
		// No target arguments
		{
			boolean IAEThrown = false;
			try {
				collectArguments(filter2 /*target*/, 0, filter2);
			} catch (IllegalArgumentException x) {
				IAEThrown = true;
			}
			AssertJUnit.assertTrue(IAEThrown);
		}
	}

	@Test(groups = { "level.sanity" })
	public void test_VoidCollectArguments() throws Throwable {
		MethodType II_I_type = methodType(int.class, int.class, int.class);
		MethodType S_V_type = methodType(void.class, String.class);
		MethodType V_V_type = methodType(void.class);

		MethodHandle target = publicLookup().findStatic(Test_AdaptorTests.class, "subtract", II_I_type);
		MethodHandle filter = publicLookup().findStatic(Test_AdaptorTests.class, "argStringReturnVoid", S_V_type);
		MethodHandle filter2 = publicLookup().findStatic(Test_AdaptorTests.class, "noArgReturnVoid", V_V_type);

		// Valid pos (0)
		{
			MethodHandle adapter = collectArguments(target, 0, filter);
			AssertJUnit.assertEquals(adapter.type(), methodType(int.class, String.class, int.class, int.class));
			int result = (int)adapter.invokeExact("Some string", 2, 1);
			AssertJUnit.assertEquals(1, result);
		}
		// Valid pos (1)
		{
			MethodHandle adapter = collectArguments(target, 1, filter);
			AssertJUnit.assertEquals(adapter.type(), methodType(int.class, int.class, String.class, int.class));
			int result = (int)adapter.invokeExact(2, "Some string", 2);
			AssertJUnit.assertEquals(0, result);
		}
		// Valid pos (2)
		{
			MethodHandle adapter = collectArguments(target, 2, filter);
			AssertJUnit.assertEquals(adapter.type(), methodType(int.class, int.class, int.class, String.class));
			int result = (int)adapter.invokeExact(2, 2, "Some string");
			AssertJUnit.assertEquals(0, result);
		}
		// No filter arguments
		{
			MethodHandle adapter = collectArguments(target, 1, filter2);
			AssertJUnit.assertEquals(adapter.type(), methodType(int.class, int.class, int.class));
			int result = (int)adapter.invokeExact(2, 1);
			AssertJUnit.assertEquals(1, result);
		}
		// No target arguments
		{
			MethodHandle adapter = collectArguments(filter2 /*target*/, 0, filter);
			AssertJUnit.assertEquals(adapter.type(), methodType(void.class, String.class));
			adapter.invokeExact("Some string");
		}
	}

	@Test(groups = { "level.sanity" })
	public void test_VoidCollectArgumentsNegative() throws Throwable {
		MethodType II_I_type = methodType(int.class, int.class, int.class);
		MethodType S_V_type = methodType(void.class, String.class);

		MethodHandle target = publicLookup().findStatic(Test_AdaptorTests.class, "subtract", II_I_type);
		MethodHandle filter = publicLookup().findStatic(Test_AdaptorTests.class, "argStringReturnVoid", S_V_type);

		// Invalid pos (3)
		{
			boolean IAEThrown = false;
			try {
				collectArguments(target, 3, filter);
			} catch (IllegalArgumentException x) {
				IAEThrown = true;
			}
			AssertJUnit.assertTrue(IAEThrown);
		}
		// Invalid negative pos (-1)
		{
			boolean IAEThrown = false;
			try {
				collectArguments(target, -1, filter);
			} catch (IllegalArgumentException x) {
				IAEThrown = true;
			}
			AssertJUnit.assertTrue(IAEThrown);
		}
	}
}
