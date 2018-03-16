package org.openj9.test.jsr335.interfaceStaticMethod;

/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
import static org.objectweb.asm.Opcodes.ARETURN;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.util.ArrayList;
import java.util.Arrays;

@Test(groups = { "level.sanity" })
public class InvokeStaticTest {

	/*
	 * Interface A { static void foo() { ... } }
	 * Interface B extends A { static void bar() { ... } }
	 * Interface C extends B { public static void main(String[] args) { ... } }
	 */

	// test invokestatic A.foo() should succeed
	@Test
	public void test_A_foo() {
		String s = org.openj9.test.jsr335.interfaceStaticMethod.A.foo();
		if (!s.equals("foo")) {
			Assert.fail("Wrong string returned'" + s + "'");
		}
	}

	// test invokestatic B.foo() should fail with IncompatibleClassChange
	@Test
	public void test_B_foo() {
		try {
			String s = org.openj9.test.jsr335.interfaceStaticMethod.GenInvokeStatic.test_B_foo();
			Assert.fail("incompatibleClassChangeErrorThrown not thrown");
		} catch (IncompatibleClassChangeError e) {
			// do nothing
		}
	}

	// test invokestatic B.bar() should succeed
	@Test
	public void test_B_bar() {
		String s = org.openj9.test.jsr335.interfaceStaticMethod.B.bar();
		if (!s.equals("bar")) {
			Assert.fail("Wrong string returned'" + s + "'");
		}
	}

	// test invokestatic A.bar() should fail with IncompatibleClassChangeError
	@Test
	public void test_A_bar() {
		try {
			String s = org.openj9.test.jsr335.interfaceStaticMethod.GenInvokeStatic.test_A_bar();
			Assert.fail("IncompatibleClassChangeError not thrown");
		} catch (IncompatibleClassChangeError e) {
			// do nothing
		}
	}
	
	
	// test invokestatic C.bar() should fail with IncompatibleClassChange
	@Test
	public void test_C_bar() {
		try {
			String s = org.openj9.test.jsr335.interfaceStaticMethod.GenInvokeStatic.test_C_bar();
			Assert.fail("incompatibleClassChangeErrorThrown not thrown");
		} catch (IncompatibleClassChangeError e) {
			// do nothing
		}
	}
	
	// test invokestatic C.foo() should fail with IncompatibleClassChange
	@Test
	public void test_C_foo() {
		try {
			String s = org.openj9.test.jsr335.interfaceStaticMethod.GenInvokeStatic.test_C_foo();
			Assert.fail("incompatibleClassChangeErrorThrown not thrown");
		} catch (IncompatibleClassChangeError e) {
			// do nothing
		}
	}

	// test invokestatic C.main() should succeed public void test_C_main() {
	@Test
	public void test_C_main() {
		try {
			org.openj9.test.jsr335.interfaceStaticMethod.C.main(new String[]{"1"});
		} catch (Exception e) {
			Assert.fail("exception caught", e);
		}
	}
}
