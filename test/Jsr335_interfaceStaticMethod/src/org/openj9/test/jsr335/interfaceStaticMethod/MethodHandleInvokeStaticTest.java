package org.openj9.test.jsr335.interfaceStaticMethod;

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

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.util.ArrayList;
import java.util.Arrays;

@Test(groups = { "level.sanity" })
public class MethodHandleInvokeStaticTest {

	/*
	 * Interface A { static void foo() { ... } }
	 * Interface B extends A { static void bar() { ... } }
	 * Interface C extends B { public static void main(String[] args) { ... } }
	 */

	// test invokestatic A.foo() should succeed
	@Test
	public void test_A_foo() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(A.class, "foo", MethodType.methodType(String.class));
		AssertJUnit.assertEquals("foo", (String)mh.invoke());
	}

	
	// test invokestatic B.foo() should fail with IncompatibleClassChange
	@Test
	public void test_B_foo() throws Throwable {
		try {
			MethodHandle mh = MethodHandles.lookup().findStatic(B.class, "foo", MethodType.methodType(String.class));
			AssertJUnit.assertEquals("foo", (String)mh.invoke());
			Assert.fail("NoSuchMethodException not thrown");
		} catch (java.lang.NoSuchMethodException e) {
			// do nothing
		} 
	}

	
	// test invokestatic B.bar() should succeed
	@Test
	public void test_B_bar() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findStatic(B.class, "bar", MethodType.methodType(String.class));
		AssertJUnit.assertEquals("bar", (String)mh.invoke());
	}

	
	// test invokestatic A.bar() should fail with NoSuchMethodError public
	@Test
	public void test_A_bar() throws Throwable {
		try {
			MethodHandle mh = MethodHandles.lookup().findStatic(A.class, "bar", MethodType.methodType(String.class));
			AssertJUnit.assertEquals("bar", (String)mh.invoke());
			Assert.fail("noSuchMethodErrorThrown not thrown");
		} catch (NoSuchMethodException e) {
			// do nothing
		}
	}
	
	// test invokestatic C.bar() should fail with IncompatibleClassChange
	@Test
	public void test_C_bar() throws Throwable {
		try {
			MethodHandle mh = MethodHandles.lookup().findStatic(C.class, "bar", MethodType.methodType(String.class));
			AssertJUnit.assertEquals("bar", (String)mh.invoke());
			Assert.fail("NoSuchMethodException not thrown");
		} catch (java.lang.NoSuchMethodException e) {
			// do nothing
		} 
	}
	
	// test invokestatic C.foo() should fail with IncompatibleClassChange
	@Test
	public void test_C_foo() throws Throwable {
		try {
			MethodHandle mh = MethodHandles.lookup().findStatic(C.class, "foo", MethodType.methodType(String.class));
			AssertJUnit.assertEquals("foo", (String)mh.invoke());
			Assert.fail("NoSuchMethodException not thrown");
		} catch (java.lang.NoSuchMethodException e) {
			// do nothing
		} 
	}
	
	// test invokestatic C.main() should succeed public void test_C_main() {
	@Test
	public void test_C_main() throws Throwable {
		try {
			MethodHandle mh = MethodHandles.lookup().findStatic(C.class, "main", MethodType.methodType(void.class, String[].class));
			mh.invoke(new String[0]);
		} catch (java.lang.NoSuchMethodException e){
			Assert.fail("NoSuchMethodException caught", e);
		}
	}
}
