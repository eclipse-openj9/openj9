/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

package org.openj9.test.java.lang.management;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.management.ManagementFactory;

import javax.management.MBeanServer;

/**
 * Tests to catch regressions on reported bugs.
 */
@Test(groups = { "level.sanity" })
public class TestRegression_SM {

	@Test
	public final void testBug93006() {
		MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();

		System.setSecurityManager(new SecurityManager());
		AssertJUnit.assertNotNull(System.getSecurityManager());

		try {
			MBeanServer mbs2 = ManagementFactory.getPlatformMBeanServer();
			Assert.fail("Should have thrown a SecurityException");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof SecurityException);
		}
	}
}
