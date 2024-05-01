/*
 * Copyright IBM Corp. and others 1998
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.java.lang;

import org.openj9.test.util.CompilerAccess;
import org.openj9.test.util.VersionCheck;
import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity" })
public class Test_Compiler {

	/**
	 * @tests java.lang.Compiler#Compiler()
	 */
	@Test
	public void test_Constructor() {
		if (VersionCheck.major() >= 21) {
			// java.lang.Compiler is not available
			return;
		}
		Class<?> compilerClass;
		try {
			compilerClass = Class.forName("java.lang.Compiler");
		} catch (ClassNotFoundException e) {
			Assert.fail("Cannot find class java.lang.Compiler", e);
			return;
		}
		try {
			compilerClass.newInstance();
			Assert.fail("Constructor should have failed.");
		} catch (Exception e) {
			// correct
		}
	}

	/**
	 * @tests java.lang.Compiler#command(java.lang.Object)
	 */
	@Test
	public void test_command() {
		try {
			Assert.assertNull(CompilerAccess.command(new Object()), "Incorrect behavior.");
		} catch (Exception e) {
			Assert.fail("Exception during test.", e);
		}
	}

	/**
	 * @tests java.lang.Compiler#compileClass(java.lang.Class)
	 */
	@Test
	public void test_compileClass() {
		try {
			// Do not test return value, may return true or false depending on
			// if the jit is enabled. Make the call to ensure it doesn't crash.
			CompilerAccess.compileClass(CompilerAccess.class);
		} catch (Exception e) {
			Assert.fail("Exception during test.", e);
		}
	}

	/**
	 * @tests java.lang.Compiler#compileClasses(java.lang.String)
	 */
	@Test
	public void test_compileClasses() {
		try {
			// Do not test return value, may return true or false depending on
			// if the jit is enabled. Make the call to ensure it doesn't crash.
			CompilerAccess.compileClasses("Integer");
		} catch (Exception e) {
			Assert.fail("Exception during test.", e);
		}
	}

	/**
	 * @tests java.lang.Compiler#disable()
	 */
	@Test
	public void test_disable() {
		try {
			CompilerAccess.disable();
			CompilerAccess.compileClass(CompilerAccess.class);
			// correct behavior
		} catch (Exception e) {
			Assert.fail("Exception during test.", e);
		}
	}

	/**
	 * @tests java.lang.Compiler#enable()
	 */
	@Test
	public void test_enable() {
		try {
			CompilerAccess.disable();
			CompilerAccess.enable();
			CompilerAccess.compileClass(CompilerAccess.class);
			// correct behavior
		} catch (Exception e) {
			Assert.fail("Exception during test.", e);
		}
	}
}
