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

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity" })
public class Test_OutOfMemoryError {

	/**
	 * @tests java.lang.OutOfMemoryError#getMessage()
	 */
	@Test
	public void test_getMessage() {
		try {
			long[][] l = new long[Integer.MAX_VALUE][Integer.MAX_VALUE];
			l[Integer.MAX_VALUE-1][Integer.MAX_VALUE-1] = 1;
			AssertJUnit.assertTrue("Failed to generate OutOfMemoryError", false);
		} catch (OutOfMemoryError e) {
			String detailMessage = e.getMessage();
			AssertJUnit.assertTrue("Incorrect detail message: \"" + detailMessage + "\"",
					detailMessage.equals("Java heap space"));
		} catch (Throwable e) {
			e.printStackTrace();
			AssertJUnit.assertTrue("Unexpected exception during test", false);
		}
	}

	/**
	 * Sets up the fixture, for example, open a network connection.
	 * This method is called before a test is executed.
	 */
	@BeforeMethod
	protected void setUp() {
	}

	/**
	 * Tears down the fixture, for example, close a network connection.
	 * This method is called after a test is executed.
	 */
	@AfterMethod
	protected void tearDown() {
	}
}
