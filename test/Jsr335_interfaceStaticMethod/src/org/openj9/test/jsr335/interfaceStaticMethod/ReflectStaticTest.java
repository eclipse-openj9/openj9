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
package org.openj9.test.jsr335.interfaceStaticMethod;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.reflect.Method;

public class ReflectStaticTest {

	@Test
	public void test_A_Reflect() {

		Method[] allMethods = A.class.getDeclaredMethods();
		AssertJUnit.assertEquals(1, allMethods.length);

		try {
			A.class.getDeclaredMethod("foo");
		} catch (Exception e) {
			Assert.fail("Exception is not expected!", e);
		}

		try {
			A.class.getDeclaredMethod("bar");
			Assert.fail("NoSuchMethodException expected");
		} catch (NoSuchMethodException e) {
			
		}
	}
	
	@Test
	public void test_B_Reflect() {

		Method[] allMethods = B.class.getDeclaredMethods();
		AssertJUnit.assertEquals(1, allMethods.length);

		try {
			B.class.getDeclaredMethod("bar");
		} catch (Exception e) {
			Assert.fail("Exception is not expected!", e);
		}

		try {
			B.class.getDeclaredMethod("foo");
			Assert.fail("NoSuchMethodException expected");
		} catch (NoSuchMethodException e) {
			
		}
	}
	
	@Test
	public void test_C_Reflect() {

		Method[] allMethods = C.class.getDeclaredMethods();
		AssertJUnit.assertEquals(1, allMethods.length);

		try {
			C.class.getDeclaredMethod("main", String[].class);
		} catch (Exception e) {
			Assert.fail("Exception is not expected!", e);
		}

		try {
			C.class.getDeclaredMethod("foo");
			Assert.fail("NoSuchMethodException expected");
		} catch (NoSuchMethodException e) {
			
		}
	}


}
