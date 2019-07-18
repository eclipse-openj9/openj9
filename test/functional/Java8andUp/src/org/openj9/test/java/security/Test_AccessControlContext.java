package org.openj9.test.java.security;

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
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.reflect.Field;
import java.security.AccessControlContext;
import java.security.AccessController;
import java.security.Permission;
import java.security.PermissionCollection;
import java.security.ProtectionDomain;
import java.util.PropertyPermission;

@Test(groups = { "level.sanity" })
public class Test_AccessControlContext {

	/**
	 * @tests
	 *        java.security.AccessControlContext#AccessControlContext(java.security
	 *        .ProtectionDomain[])
	 */
	@Test
	public void test_Constructor() {
		final Permission perm = new PropertyPermission("java.class.path", "read");
		PermissionCollection col = perm.newPermissionCollection();
		col.add(perm);
		final ProtectionDomain pd = new ProtectionDomain(null, col);
		AccessControlContext acc = new AccessControlContext(new ProtectionDomain[] { pd });
		try {
			acc.checkPermission(perm);
		} catch (SecurityException e) {
			AssertJUnit.assertTrue("Should have permission", false);
		}

		final boolean[] result = new boolean[] { false };
		Thread th = new Thread(new Runnable() {
			public void run() {
				AccessControlContext acc = new AccessControlContext(new ProtectionDomain[] { pd });
				try {
					acc.checkPermission(perm);
					result[0] = true;
				} catch (SecurityException e) {
				}
			}
		});
		th.start();
		try {
			th.join();
		} catch (InterruptedException e) {
		}
		AssertJUnit.assertTrue("Thread should have permission", result[0]);
	}

	/**
	 * @tests
	 *        java.security.AccessControlContext#AccessControlContext(java.security
	 *        .AccessControlContext, java.security.DomainCombiner)
	 */
	@Test
	public void test_Constructor2() {
		/* [PR 110588] Spec changed between 1.3 and 1.4 */
		AccessControlContext context = AccessController.getContext();
		boolean exception = false;
		try {
			new AccessControlContext(context, null);
		} catch (NullPointerException e) {
			exception = true;
		}
		AssertJUnit.assertTrue("should not throw NullPointerException", !exception);
	}

	/**
	 * @tests
	 *        java.security.AccessControlContext#AccessControlContext(java.security
	 *        .ProtectionDomain[])
	 */
	@Test
	public void test_Constructor3() {
		try {
			Field contextField = AccessControlContext.class.getDeclaredField("context");
			contextField.setAccessible(true);
			AccessControlContext acc1 = new AccessControlContext(new ProtectionDomain[] { null });
			AssertJUnit.assertTrue("context should be null", contextField.get(acc1) == null);
			AccessControlContext acc2 = new AccessControlContext(new ProtectionDomain[] { null, null, null });
			AssertJUnit.assertTrue("context should be null", contextField.get(acc2) == null);
			AccessControlContext acc3 = new AccessControlContext(
					new ProtectionDomain[] { new ProtectionDomain(null, null), null });
			AssertJUnit.assertTrue("context should have length 1",
					((ProtectionDomain[])contextField.get(acc3)).length == 1);
			AccessControlContext acc4 = new AccessControlContext(
					new ProtectionDomain[] { null, new ProtectionDomain(null, null) });
			AssertJUnit.assertTrue("context should have length 1",
					((ProtectionDomain[])contextField.get(acc4)).length == 1);
			AccessControlContext acc5 = new AccessControlContext(
					new ProtectionDomain[] { null, new ProtectionDomain(null, null), null });
			AssertJUnit.assertTrue("context should have length 1",
					((ProtectionDomain[])contextField.get(acc5)).length == 1);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("unexpected: " + e);
		}

	}
}
