/*
 * Copyright IBM Corp. and others 2013
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

import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class Test_J9VMInternalsImpl {

	private static final String ORG_TESTNG = "org.testng"; //$NON-NLS-1$
	static final Logger logger = Logger.getLogger(Test_J9VMInternalsImpl.class);

	public static class NCDFEHelper {
		/* intentionally empty */
	}

	public static class TestNCDFE {
		private static int testCount = 1;

		public static void startTest(String testing) {
			logger.debug("\nTest " + testCount++ + ": " + testing); //$NON-NLS-1$ //$NON-NLS-2$
		}

		public void runTest1() {
			startTest(
					"try instantiating a Class that cannot be loaded because of LinkageError, then enable loading of the class and still fail to load"); //$NON-NLS-1$
			/*
			 * There is magic here. The ClassLoader will:
			 * - throw OutOfMemoryError the first time NCDFEHelper is loaded
			 * - throw LinkageError the second time NCDFEHelper is loaded
			 * - load NCDFEHelper in all subsequent attempts
			 */
			try {
				new NCDFEHelper();
				Assert.fail("FAILED: NCDFEHelper should not exist"); //$NON-NLS-1$
			} catch (OutOfMemoryError e) {
				logger.debug("PASSED: Got "  //$NON-NLS-1$
			+ e.getClass() + " creating instance of NCDFEHelper"); //$NON-NLS-1$
			} catch (Throwable e) {
				Assert.fail("FAILED: Unexpected: " + e); //$NON-NLS-1$
			}
			try {
				new NCDFEHelper();
				Assert.fail("FAILED: NCDFEHelper should still not exist"); //$NON-NLS-1$
			} catch (LinkageError e) {
				logger.debug("PASSED: Got "  //$NON-NLS-1$
			+ e.getClass() + " creating instance of NCDFEHelper"); //$NON-NLS-1$
			} catch (Throwable e) {
				Assert.fail("FAILED: Unexpected: " + e); //$NON-NLS-1$
			}
			try {
				new NCDFEHelper();
				Assert.fail("FAILED: NCDFEHelper should still not exist"); //$NON-NLS-1$
			} catch (LinkageError e) {
				logger.debug("PASSED: Got "  //$NON-NLS-1$
						+ e.getClass() + " creating instance of NCDFEHelper"); //$NON-NLS-1$
			} catch (Throwable e) {
				Assert.fail("FAILED: Unexpected: " + e); //$NON-NLS-1$
			}
		}
	}

	@Test
	public void test_checkPackageAccess() throws Throwable {
		final String className = getClass().getName();

		/*
		 * special ClassLoader that changes behavior every time the
		 * $NCDFEHelper class load is attempted
		 */
		ClassLoader loader1 = new URLClassLoader(
				new URL[] { getClass().getProtectionDomain().getCodeSource().getLocation() }, null) {
			int behaviorLoadingNCDFEHelper = 2;

			@Override
			@SuppressWarnings({ "rawtypes", "unchecked" })
			public Class findClass(String name) throws ClassNotFoundException {
				Class result;
				if (name.startsWith(ORG_TESTNG)) {
					result = Class.forName(name, true, ClassLoader.getSystemClassLoader());
				} else {
					if (name.equals(className + "$NCDFEHelper") && behaviorLoadingNCDFEHelper > 0) { //$NON-NLS-1$
						logger.debug("Trying to load NCDFEHelper"); //$NON-NLS-1$
						behaviorLoadingNCDFEHelper--;
						if (behaviorLoadingNCDFEHelper == 1) {
							throw new OutOfMemoryError();
						}
						throw new LinkageError();
					}
					result = super.findClass(name);
				}
				return result;
			}
		};

		/*
		 * run the $TestNCDFE test
		 */
		@SuppressWarnings("rawtypes")
		Class cl1 = Class.forName(className + "$TestNCDFE", true, loader1); //$NON-NLS-1$
		Object testInstance1 = cl1.newInstance();
		@SuppressWarnings("unchecked")
		Method testMethod1 = cl1.getMethod("runTest1"); //$NON-NLS-1$
		testMethod1.invoke(testInstance1);
	}

}
