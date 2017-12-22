
package org.openj9.test.java.lang;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import java.lang.reflect.*;
import java.net.*;
import java.security.*;
import java.util.*;

/*******************************************************************************
 * Copyright (c) 2013, 2017 IBM Corp. and others
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
@Test(groups = { "level.sanity" })
public class Test_J9VMInternals {

	private static final Logger logger = Logger.getLogger(Test_J9VMInternals.class);

	public static class NCDFEHelper {
	}

	public static class SuperclassRestricted extends com.ibm.oti.shared.Shared {
		static {
			logger.debug("#####################################");
			logger.debug("clinit of SuperclassRestricted called");
		}
	}

	public static class InterfaceRestricted implements com.ibm.oti.shared.SharedClassFilter {
		static {
			logger.debug("####################################");
			logger.debug("clinit of InterfaceRestricted called");
		}

		public boolean acceptStore(String className) {
			return true;
		}

		public boolean acceptFind(String className) {
			return true;
		}
	}

	public static class TestNCDFE {
		private static int testCount = 1;

		public static void startTest(String testing) {
			logger.debug("\nTest " + testCount++ + ": " + testing);
		}

		public static void fail(String output) {
			throw new Error(output);
		}

		public void runTest1() {
			startTest(
					"try instantiating a Class that cannot be loaded because of LinkageError, then enable loading of the class and still fail to load");
			// There is magic here. The ClassLoader will
			// 	throw OutOfMemoryErorr the first time NCDFEHelper is loaded
			// 	throw LinkageError the second time NCDFEHelper is loaded
			//	load NCDFEHelper in all subsequent attempts
			try {
				new NCDFEHelper();
				Assert.fail("NCDFEHelper should not exist");
			} catch (OutOfMemoryError e) {
				logger.debug("GOOD Got " + e.getClass() + " creating instance of NCDFEHelper");
			} catch (Throwable e) {
				Assert.fail("Unexpected: " + e);
			}
			try {
				new NCDFEHelper();
				Assert.fail("NCDFEHelper should still not exist");
				//			Not supported on J9, see CMVC 198574
				//			} catch (NoClassDefFoundError e) {
				//				fail("Unexpected: " + e);
			} catch (LinkageError e) {
				logger.debug("GOOD Got " + e.getClass() + " creating instance of NCDFEHelper");
			} catch (Throwable e) {
				Assert.fail("Unexpected: " + e);
			}
			try {
				new NCDFEHelper();
				Assert.fail("NCDFEHelper should 2nd time still not exist");
				//			Not supported on J9, see CMVC 198574
				//			} catch (NoClassDefFoundError e) {
				//				fail("Unexpected: " + e);
			} catch (LinkageError e) {
				logger.debug("GOOD Got " + e.getClass() + " creating instance of NCDFEHelper");
			} catch (Throwable e) {
				Assert.fail("Unexpected: " + e);
			}
		}
	}

	// The test must be performed in a different ClassLoader, because the default application ClassLoader
	// has a built in check for package access.
	public static class TestRestricted {
		private static int testCount = 2;

		public static void startTest(String testing) {
			logger.debug("\nTest " + testCount++ + ": " + testing);
		}

		public static void fail(String output) {
			throw new Error(output);
		}

		public static void assertTrue(String output, boolean result) {
			if (!result) {
				Assert.fail(output);
			}
		}

		public void runTest2(final String className) throws Exception {
			// stop security checks here
			AccessController.doPrivileged(new PrivilegedExceptionAction() {
				public Object run() throws Exception {
					// must create the classloader in an environment with sufficient privilege, as it remembers its context
					ClassLoader loader = new URLClassLoader(
							new URL[] { getClass().getProtectionDomain().getCodeSource().getLocation() },
							getClass().getClassLoader());
					System.setSecurityManager(new SecurityManager());
					try {

						startTest("try instantiating from a restricted package 1st time");
						Class cl = Class.forName(className + "$TestHelper1", true, loader);
						Runnable testRunner = (Runnable)cl.newInstance();
						try {
							testRunner.run();
							Assert.fail("successfully created an instance of a restricted package");
						} catch (SecurityException e) {
							// expected
						}

						startTest("try instantiating from a restricted package 2nd time");
						try {
							testRunner.run();
							Assert.fail("successfully created an instance of a restricted package 2nd time");
						} catch (SecurityException e) {
							// expected
						}

						Class cl2 = Class.forName(className + "$TestHelper2", true, loader);
						Runnable testRunner2 = (Runnable)cl2.newInstance();
						testRunner2.run();

					} finally {
						System.setSecurityManager(null);
					}
					return null;
				}
			});
		}
	}

	public static class TestHelper1 implements Runnable {
		public void run() {
			new com.ibm.oti.shared.Shared();
		}
	}

	public static class TestHelper2 extends TestRestricted implements Runnable {
		public void run() {
			startTest("try loading class from a restricted package using Class.forName()");
			try {
				Class cl2 = Class.forName("com.ibm.oti.shared.Shared");
				Assert.fail("unexpected behavior: successfully loaded com.ibm.oti.shared.Shared using forName");
			} catch (SecurityException e) {
				logger.debug("GOOD forName of com.ibm.oti.shared.Shared: " + e);
			} catch (ClassNotFoundException e) {
				Assert.fail("unexpected: " + e);
			}

			startTest("try casting to a class in a restricted package");
			try {
				Object o = new Object();
				com.ibm.oti.shared.Shared encoder = (com.ibm.oti.shared.Shared)o;
				Assert.fail("unexpected behavior: successfully cast to com.ibm.oti.shared.Shared");
			} catch (ClassCastException e) {
				Assert.fail("cast to com.ibm.oti.shared.Shared: " + e);
			} catch (SecurityException e) {
				logger.debug("GOOD casting to com.ibm.oti.shared.Shared: " + e);
			}

			startTest("try testing instanceof a restricted package");
			try {
				Object o = new Object();
				if (o instanceof com.ibm.oti.shared.Shared) {
				}
				Assert.fail("unexpected behavior: successfully tested instanceof com.ibm.oti.shared.Shared");
			} catch (SecurityException e) {
				logger.debug("GOOD instanceof com.ibm.oti.shared.Shared: " + e);
			}

			startTest("try calling a static method in a restricted package");
			try {
				com.ibm.oti.shared.Shared.isSharingEnabled();
				Assert.fail("called static method on com.ibm.oti.shared.Shared");
			} catch (SecurityException e) {
				logger.debug("GOOD calling static method on com.ibm.oti.shared.Shared: " + e);
			}

			startTest("try accessing a static field in a restricted package");
			try {
				int flag = com.ibm.oti.shared.SharedClassUtilities.NO_FLAGS;
				Assert.fail("accessed static field on com.ibm.oti.shared.SharedClassUtilities");
			} catch (SecurityException e) {
				logger.debug("GOOD accessing static field on com.ibm.oti.shared.SharedClassUtilities: " + e);
			}
		}
	}

	public static class TestHelper3 extends TestRestricted implements Runnable {
		public void run() {
		}
	}

	@Test
	public void test_checkPackageAccess() throws Throwable {
		final String className = getClass().getName();

		System.setSecurityManager(new SecurityManager());
		try {
			// these tests are not dependent on the ClassLoader behavior
			try {
				Class cl = Class.forName(className + "$SuperclassRestricted");
				Assert.fail("successfully loaded " + cl + " with restricted superclass");
			} catch (SecurityException e) {
				// expected
			}

			try {
				Class cl = Class.forName(className + "$InterfaceRestricted");
				Assert.fail("successfully loaded " + cl + " with restricted interface");
			} catch (SecurityException e) {
				// expected
			}

		} finally {
			System.setSecurityManager(null);
		}

		// special ClassLoader that changes behavior every time the $NCDFEHelper class load is attempted
		ClassLoader loader1 = new URLClassLoader(
				new URL[] { getClass().getProtectionDomain().getCodeSource().getLocation() }, null) {
			int behaviorLoadingNCDFEHelper = 2;

			public Class findClass(String name) throws ClassNotFoundException {
				if (name.equals(className + "$NCDFEHelper") && behaviorLoadingNCDFEHelper > 0) {
					logger.debug("Trying to load NCDFEHelper");
					behaviorLoadingNCDFEHelper--;
					if (behaviorLoadingNCDFEHelper == 1) {
						throw new OutOfMemoryError();
					}
					throw new LinkageError();
				}
				return super.findClass(name);
			}
		};

		// run the $TestNCDFE test
		Class cl1 = Class.forName(className + "$TestNCDFE", true, loader1);
		Object testInstance1 = cl1.newInstance();
		Method testMethod1 = cl1.getMethod("runTest1");
		testMethod1.invoke(testInstance1);

		// create a ClassLoader with AllPermission so classes loaded by this loader are not restricted by
		// the default permissions of the application ClassLoader
		ClassLoader privilegedLoader = new URLClassLoader(
				new URL[] { getClass().getProtectionDomain().getCodeSource().getLocation() }, null) {
			public Class findClass(String name) throws ClassNotFoundException {
				if (!name.startsWith(className + "$TestRestricted")) {
					throw new ClassNotFoundException(name);
				}
				return super.findClass(name);
			}

			public PermissionCollection getPermissions(CodeSource csource) {
				PermissionCollection permCollection = super.getPermissions(csource);
				// Allow AllPermission so we can use this classloader to define a custom SecurityManager
				permCollection.add(new AllPermission());
				return permCollection;
			}
		};

		// these tests are dependent on the ClassLoader behavior. The default application ClassLoader
		// checks package access of the classes being loaded. Other ClassLoaders don't have this behavior
		// and depend on the VM to do the check. Its the VM behavior we want to test
		Class cl2 = Class.forName(className + "$TestRestricted", true, privilegedLoader);
		Object testInstance2 = cl2.newInstance();
		Method testMethod2 = cl2.getMethod("runTest2", String.class);
		testMethod2.invoke(testInstance2, className);
	}

}
