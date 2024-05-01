package org.openj9.test.java.lang;

/*
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */

import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.security.Permission;

import org.openj9.test.support.Support_Exec;
import org.openj9.test.util.VersionCheck;

@Test(groups = { "level.sanity" })
public class Test_System_SM {

	/**
	 * @tests java.lang.System#getSecurityManager()
	 */
	@Test
	public void test_getSecurityManager() {
		AssertJUnit.assertTrue("Returned incorrect SecurityManager", System.getSecurityManager() == null);
	}

	/**
	 * @tests java.lang.System#setSecurityManager(java.lang.SecurityManager)
	 */
	@Test
	public void test_setSecurityManager() {
		/* [PR 97686, 123113] Improve System.setSecurityManager() initialization */
		class MySecurityManager extends SecurityManager {
			public void preloadClasses(Permission perm) {
				if (!(perm instanceof RuntimePermission)
					|| !(perm.getName().toLowerCase().equals("setsecuritymanager"))
				) {
					// do nothing, required classes are now loaded
				}
			}

			public void checkPermission(Permission perm) {
				if (!(perm instanceof RuntimePermission)
					|| !(perm.getName().toLowerCase().equals("setsecuritymanager"))
				) {
					super.checkPermission(perm);
				}
			}
		}

		MySecurityManager newManager = new MySecurityManager();
		// preload the required classes without calling implies()
		try {
			newManager.preloadClasses(new RuntimePermission("anything"));
		} catch (SecurityException e) {
		}
		try {
			System.setSecurityManager(new SecurityManager());
			System.getProperty("someProperty");
			Assert.fail("should cause SecurityException");
		} catch (SecurityException e) {
			// expected
		} finally {
			System.setSecurityManager(null);
		}
	}

	/**
	 * @tests java.lang.System#setSecurityManager(java.lang.SecurityManager)
	 */
	@Test
	public void test_setSecurityManager2() {
		try {
			String helperName = "org.openj9.test.java.lang.Test_System_SM$TestSecurityManager";
			String output = Support_Exec.execJava(new String[] {
					"-Djava.security.manager=" + helperName, helperName },
					null, true);
			AssertJUnit.assertTrue("not correct SecurityManager: " + output, output.startsWith(helperName));
		} catch (Exception e) {
			Assert.fail("Unexpected: " + e);
		}
	}

	/**
	 * @tests java.lang.System#setSecurityManager(java.lang.SecurityManager)
	 */
	public static class TestSecurityManager extends SecurityManager {
		public static void main(String[] args) {
			System.out
					.println(System.getSecurityManager().getClass().getName());
		}
	}

	/**
	 * @tests java.lang.System#setSecurityManager(java.lang.SecurityManager)
	 */
	@Test
	public void test_setSecurityManager3() {
		/* https://github.com/eclipse-openj9/openj9/issues/6661 */
		if (VersionCheck.major() >= 11) {
			try {
				String helperName = "org.openj9.test.java.lang.Test_System_SM$TestSecurityManagerNonPublicConstructor";
				String output = Support_Exec.execJava(new String[] { "-Djava.security.manager=" + helperName, helperName },
						null, true,
						"java.lang.NoSuchMethodException: org.openj9.test.java.lang.Test_System_SM$TestSecurityManagerNonPublicConstructor.<init>()");
				// if the expected error string was not found, Assert.fail() will be invoked
				// within Support_Exec.execJava() above.
			} catch (Exception e) {
				Assert.fail("Unexpected: " + e);
			}
		}
	}

	/**
	 * @tests java.lang.System#setSecurityManager(java.lang.SecurityManager)
	 */
	public static class TestSecurityManagerNonPublicConstructor extends SecurityManager {
		TestSecurityManagerNonPublicConstructor() {
		}

		public static void main(String[] args) {
			System.out.println(System.getSecurityManager().getClass().getName());
		}
	}

}
