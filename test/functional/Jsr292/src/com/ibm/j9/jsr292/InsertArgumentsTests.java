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
package com.ibm.j9.jsr292;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import static java.lang.invoke.MethodHandles.*;
import java.lang.invoke.MethodHandle;

public class InsertArgumentsTests {

	public static MethodHandle handle = identity(Object[].class);
	
	@Test(groups = { "level.extended" })
	public static void test_insert_no_values() throws Throwable{
		MethodHandle mh = insertArguments(handle, 0);
		AssertJUnit.assertTrue("inserting no args should be a no op", mh == handle);
		Object[] value = (Object[]) mh.invokeExact(new Object[0]);
		AssertJUnit.assertNotNull(value);
	}

	@Test(groups = { "level.extended" })
	public static void test_insert_location_IAE() {
		boolean IAE = false;
		try {
			insertArguments(handle, 5 /* invalid index */);
		} catch (IllegalArgumentException e) {
			IAE = true;
		}
		AssertJUnit.assertTrue(IAE);
	}

	@Test(groups = { "level.extended" })
	public static void test_insert_values_NPE() {
		boolean NPE = false;
		try {
			/* Ensure NPE for null values occurs before IAE for invalid index to match RI */
			insertArguments(handle, 5 /* invalid index */, (Object[])null);
		} catch (NullPointerException e) {
			NPE = true;
		}
		AssertJUnit.assertTrue(NPE);
	}

	@Test(groups = { "level.extended" })
	public static void test_insert_handle_NPE() {
		boolean NPE = false;
		try {
			/* Ensure NPE for null handle occurs before IAE for invalid index to match RI */
			insertArguments(null, 5 /* invalid index */, (Object[])null);
		} catch (NullPointerException e) {
			NPE = true;
		}
		AssertJUnit.assertTrue(NPE);
	}
}
