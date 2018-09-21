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
package org.openj9.test.java.lang;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity" })
public class Test_Compiler {

	/**
	* @tests java.lang.Compiler#Compiler()
	*/
	@Test
	public void test_Constructor() {
		try {
			Compiler c = (Compiler)Compiler.class.newInstance();
			AssertJUnit.assertTrue("Constructor should have failed.", false);
		} catch (Exception e) {//Correct.
		}
	}

	/**
	 * @tests java.lang.Compiler#command(java.lang.Object)
	 */
	@Test
	public void test_command() {
		try {
			AssertJUnit.assertTrue("Incorrect behavior.", Compiler.command(new Object()) == null);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during test.", false);
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
			Compiler.compileClass(Compiler.class);
		} catch (Exception e) {
			Assert.fail("Exception during test.");
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
			Compiler.compileClasses("Compiler");
		} catch (Exception e) {
			Assert.fail("Exception during test.");
		}
	}

	/**
	 * @tests java.lang.Compiler#disable()
	 */
	@Test
	public void test_disable() {
		try {
			Compiler.disable();
			Compiler.compileClass(Compiler.class);
			AssertJUnit.assertTrue("Correct behavior.", true);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during test.", false);
		}
	}

	/**
	 * @tests java.lang.Compiler#enable()
	 */
	@Test
	public void test_enable() {
		try {
			Compiler.disable();
			Compiler.enable();
			Compiler.compileClass(Compiler.class);
			AssertJUnit.assertTrue("Correct behavior.", true);
		} catch (Exception e) {
			AssertJUnit.assertTrue("Exception during test.", false);
		}
	}
}
