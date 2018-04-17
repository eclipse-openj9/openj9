/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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

package org.openj9.test.java.lang.management;

import org.testng.annotations.AfterClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeClass;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.management.ManagementPermission;

@Test(groups = { "level.sanity" })
public class TestManagementPermission {

	private static Logger logger = Logger.getLogger(TestManagementPermission.class);

	@BeforeClass
	protected void setUp() throws Exception {
		logger.info("Starting TestManagementPermission tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
	}

	/*
	 * Class under test for void ManagementPermission(String)
	 */
	@Test
	public final void testManagementPermissionString() {
		// Normal cases.
		ManagementPermission mp = new ManagementPermission("monitor");
		AssertJUnit.assertNotNull(mp);
		mp = new ManagementPermission("control");
		AssertJUnit.assertNotNull(mp);

		// Bad input - null name
		try {
			mp = new ManagementPermission(null);
			Assert.fail("Should have thrown NPE for null name.");
		} catch (Exception e) {
		}

		// Bad input - unwanted name
		try {
			mp = new ManagementPermission("Sunset");
			Assert.fail("Should have thrown IllegalArgumentException for incorrect name.");
		} catch (Exception e) {
		}

		// Bad input - correct name but in incorrect type
		try {
			mp = new ManagementPermission("Monitor");
			Assert.fail("Should have thrown IllegalArgumentException for upper-case name.");
		} catch (Exception e) {
		}
	}

	/*
	 * Class under test for void ManagementPermission(String, String)
	 */
	@Test
	public final void testManagementPermissionStringString() {
		// Normal cases.
		ManagementPermission mp = new ManagementPermission("monitor", "");
		AssertJUnit.assertNotNull(mp);
		mp = new ManagementPermission("control", null);
		AssertJUnit.assertNotNull(mp);

		// Bad input - null name
		try {
			mp = new ManagementPermission(null, null);
			Assert.fail("Should have thrown NPE for null name.");
		} catch (Exception e) {
		}

		// Bad input - unwanted name
		try {
			mp = new ManagementPermission("Sunset", null);
			Assert.fail("Should have thrown IllegalArgumentException for incorrect name.");
		} catch (Exception e) {
		}

		// Bad input - correct name but in incorrect type
		try {
			mp = new ManagementPermission("Monitor", null);
			Assert.fail("Should have thrown IllegalArgumentException for upper-case name.");
		} catch (Exception e) {
		}

		// Bad input - action not one of "" or null
		try {
			mp = new ManagementPermission("monitor", "You broke my heart Fredo.");
			Assert.fail("Should have thrown IllegalArgumentException for bad action.");
		} catch (Exception e) {
		}
	}
}
