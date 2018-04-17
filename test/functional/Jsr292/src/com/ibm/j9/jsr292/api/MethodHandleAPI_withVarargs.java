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
import static java.lang.invoke.MethodHandles.Lookup;
import static java.lang.invoke.MethodHandles.lookup;
import static java.lang.invoke.MethodType.methodType;
import java.lang.invoke.WrongMethodTypeException;


public class MethodHandleAPI_withVarargs {
	final Lookup lookup = lookup();

	/**
	 * withVarargs test in the case of a trailing non-array parameter in the handle.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_withVarargs_InvalidTrailingParameter() throws Throwable {
		MethodHandle mhResult = lookup.findStatic(SamePackageExample.class,"nonVarArityMethod", methodType(int.class, int.class, int.class));
		try {
			mhResult = mhResult.withVarargs(true);
			AssertJUnit.fail("IllegalArgumentException should be thrown out as there is no trailing array paramter in the handle");
		} catch (IllegalArgumentException e) {
			/* Success */
		}
	}

	/**
	 * withVarargs test for a handle with fixed arity to keep the original arity mode
	 * if not variable varity.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_withVarargs_KeepFixedArity() throws Throwable {
		MethodHandle mhResult = lookup.findStatic(SamePackageExample.class,"fixedArityMethod", methodType(String.class, int.class, String[].class));
		try {
			mhResult = mhResult.withVarargs(mhResult.isVarargsCollector());
			AssertJUnit.assertFalse(mhResult.isVarargsCollector());
			AssertJUnit.assertEquals(methodType(String.class, int.class, String[].class),mhResult.type());
			AssertJUnit.assertEquals("[arg0, arg1, arg2]", (String)mhResult.invokeExact(1, new String[] {"arg0", "arg1", "arg2"}));
		} catch (Exception e) {
			AssertJUnit.fail("No exception should be thrown out as there is no conversion for the handle's varity mode");
		}

		try {
			AssertJUnit.assertEquals("[arg0, arg1, arg2]", (String)mhResult.invoke(1, "arg0", "arg1", "arg2"));
			AssertJUnit.fail("WrongMethodTypeException should be thrown out as the varity mode of the handle remains unchanged");
		} catch (WrongMethodTypeException e) {
			/* Success */
		}

	}

	/**
	 * withVarargs test for a handle with fixed arity to be converted to variable varity as requested.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_withVarargs_ConvertToVarArity() throws Throwable {
		MethodHandle mhResult = lookup.findStatic(SamePackageExample.class,"fixedArityMethod", methodType(String.class, int.class, String[].class));
		try {
			AssertJUnit.assertFalse(mhResult.isVarargsCollector());
			AssertJUnit.assertEquals("[arg0, arg1, arg2]", (String)mhResult.invokeExact(1, new String[] {"arg0", "arg1", "arg2"}));

			mhResult = mhResult.withVarargs(true);
			AssertJUnit.assertTrue(mhResult.isVarargsCollector());
			AssertJUnit.assertEquals(methodType(String.class, int.class, String[].class),mhResult.type());
			AssertJUnit.assertEquals("[arg0, arg1, arg2]", (String)mhResult.invoke(1, "arg0", "arg1", "arg2"));
		} catch (Exception e) {
			AssertJUnit.fail("No exception should be thrown out in terms of the conversion of varity mode");
		}
	}

	/**
	 * withVarargs test for a handle with variable arity to keep the original arity mode
	 * if not fixed varity.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_withVarargs_KeepVarArity() throws Throwable {
		MethodHandle mhResult = lookup.findStatic(SamePackageExample.class,"varArityMethod", methodType(String.class, int.class, String[].class));
		try {
			mhResult = mhResult.withVarargs(mhResult.isVarargsCollector());
			AssertJUnit.assertTrue(mhResult.isVarargsCollector());
			AssertJUnit.assertEquals(methodType(String.class, int.class, String[].class),mhResult.type());
			AssertJUnit.assertEquals("[arg0, arg1, arg2]", (String)mhResult.invoke(1, "arg0", "arg1", "arg2"));
		} catch (Exception e) {
			AssertJUnit.fail("No exception should be thrown out as the varity mode of the handle remains unchanged");
		}
	}

	/**
	 * withVarargs test for a handle with variable arity to be converted to fixed varity as requested.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_withVarargs_ConvertToFixedArity() throws Throwable {
		MethodHandle mhResult = lookup.findStatic(SamePackageExample.class,"varArityMethod", methodType(String.class, int.class, String[].class));
		try {
			AssertJUnit.assertTrue(mhResult.isVarargsCollector());
			AssertJUnit.assertEquals("[arg0, arg1, arg2]", (String)mhResult.invoke(1, "arg0", "arg1", "arg2"));

			mhResult = mhResult.withVarargs(false);
			AssertJUnit.assertFalse(mhResult.isVarargsCollector());
			AssertJUnit.assertEquals(methodType(String.class, int.class, String[].class),mhResult.type());

			AssertJUnit.assertEquals("[arg0, arg1, arg2]", (String)mhResult.invokeExact(1, new String[] {"arg0", "arg1", "arg2"}));
		} catch (Exception e) {
			AssertJUnit.fail("No exception should be thrown out in terms of the conversion of varity mode");
		}

		try {
			AssertJUnit.assertEquals("[arg0, arg1, arg2]", (String)mhResult.invoke(1, "arg0", "arg1", "arg2"));
			AssertJUnit.fail("WrongMethodTypeException should be thrown out as the handle has changed to be fixed varity");
		} catch (WrongMethodTypeException e) {
			/* Success */
		}
	}

	/**
	 * withVarargs test for a handle with variable arity to keep the original arity mode
	 * after a non-trailing parameter gets changed.
	 * @throws Throwable
	 */
	@Test(groups = { "level.extended" })
	public void test_withVarargs_VarArity_ReplaceParamterType() throws Throwable {
		MethodHandle mhResult = lookup.findStatic(SamePackageExample.class,"varArityMethod", methodType(String.class, int.class, String[].class));
		try {
			AssertJUnit.assertTrue(mhResult.isVarargsCollector());
			AssertJUnit.assertEquals("[arg0, arg1, arg2]", (String)mhResult.invoke(1, "arg0", "arg1", "arg2"));

			mhResult = mhResult.asType(mhResult.type().changeParameterType(0, byte.class)).withVarargs(mhResult.isVarargsCollector());
			AssertJUnit.assertTrue(mhResult.isVarargsCollector());
			AssertJUnit.assertEquals(methodType(String.class, byte.class, String[].class),mhResult.type());

			AssertJUnit.assertEquals("[arg0, arg1, arg2]", (String)mhResult.invoke((byte)1, "arg0", "arg1", "arg2"));
		} catch (Exception e) {
			AssertJUnit.fail("No exception should be thrown out as the varity mode of the handle remains unchanged");
		}
	}
}
