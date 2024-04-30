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
package org.openj9.test.java.lang;

import java.lang.reflect.Method;
import java.net.URL;
import java.net.URLClassLoader;
import java.security.AccessController;
import java.security.AllPermission;
import java.security.CodeSource;
import java.security.PermissionCollection;
import java.security.PrivilegedExceptionAction;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity" })
public class Test_J9VMInternalsImpl_SM {

	private static final String ORG_TESTNG = "org.testng"; //$NON-NLS-1$
	private static final String TEST_RESTRICTED = "$TestRestricted"; //$NON-NLS-1$
	static final Logger logger = Logger.getLogger(Test_J9VMInternalsImpl_SM.class);

	public static class SuperclassRestricted extends sun.misc.test.RestrictedClass {
		static {
			logger.debug("FAILED: clinit of SuperclassRestricted called"); //$NON-NLS-1$
		}
	}

	public static class InterfaceRestricted implements sun.misc.test.RestrictedInterface {
		static {
			logger.debug("FAILED: clinit of InterfaceRestricted called"); //$NON-NLS-1$
		}
	}

	/*
	 * The test must be performed in a different ClassLoader,
	 * because the default application ClassLoader has a built in check for package access.
	 */
	public static class TestRestricted {
		private static int testCount = 2;

		/*
		 * We need a local logger because referring to the logger refers to the outer class,
		 * which our special classloader cannot load.
		 */
		private static final Logger restrictedLogger = Logger.getLogger(TestRestricted.class);

		public static void startTest(String testing) {
			restrictedLogger.debug("\nTest " + testCount++ + ": " + testing); //$NON-NLS-1$ //$NON-NLS-2$
		}

		public void runTest2(final String className) throws Exception {
			/* stop security checks here */
			AccessController.doPrivileged(new PrivilegedExceptionAction<Object>() {
				@Override
				public Object run() throws Exception {
					// must create the classloader in an environment with sufficient privilege, as it remembers its context
					ClassLoader loader = new URLClassLoader(
							new URL[] { getClass().getProtectionDomain().getCodeSource().getLocation() },
							getClass().getClassLoader());
					System.setSecurityManager(new SecurityManager());
					try {

						startTest("try instantiating from a restricted package 1st time"); //$NON-NLS-1$
						@SuppressWarnings("rawtypes")
						Class cl = Class.forName(className + "$TestHelper1", true, loader); //$NON-NLS-1$
						Runnable testRunner = (Runnable)cl.newInstance();
						try {
							testRunner.run();
							Assert.fail("FAILED: successfully created an instance of a restricted package"); //$NON-NLS-1$
						} catch (SecurityException e) {
							/* intentionally empty */
						}

						startTest("try instantiating from a restricted package 2nd time"); //$NON-NLS-1$
						try {
							testRunner.run();
							Assert.fail("FAILED: successfully created an instance of a restricted package 2nd time"); //$NON-NLS-1$
						} catch (SecurityException e) {
							/* intentionally empty */
						}

						@SuppressWarnings("rawtypes")
						Class cl2 = Class.forName(className + "$TestHelper2", true, loader); //$NON-NLS-1$
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
		@Override
		public void run() {
			new sun.misc.test.RestrictedClass();
		}
	}

	public static class TestHelper2 extends TestRestricted implements Runnable {
		private static final String RESTRICTED_CLASS = "sun.misc.test.RestrictedClass"; //$NON-NLS-1$

		@Override
		public void run() {
			startTest("try loading class from a restricted package using Class.forName()"); //$NON-NLS-1$
			try {
				@SuppressWarnings({ "unused", "rawtypes" })
				Class cl2 = Class.forName(RESTRICTED_CLASS);
				Assert.fail("FAILED: unexpected behavior: successfully loaded " //$NON-NLS-1$
						+ RESTRICTED_CLASS + " using forName"); //$NON-NLS-1$
			} catch (SecurityException e) {
				logger.debug("PASSED: forName of " //$NON-NLS-1$
						+ RESTRICTED_CLASS + ": " + e); //$NON-NLS-1$
			} catch (ClassNotFoundException e) {
				Assert.fail("FAILED: unexpected: " + e); //$NON-NLS-1$
			}

			startTest("try casting to a class in a restricted package"); //$NON-NLS-1$
			try {
				Object o = new Object();
				@SuppressWarnings("unused")
				sun.misc.test.RestrictedClass encoder = (sun.misc.test.RestrictedClass)o;
				Assert.fail("FAILED: successfully cast to " + RESTRICTED_CLASS); //$NON-NLS-1$
			} catch (ClassCastException e) {
				Assert.fail("FAILED: cast to " //$NON-NLS-1$
						+ RESTRICTED_CLASS + ": " + e); //$NON-NLS-1$
			} catch (SecurityException e) {
				logger.debug("PASSED: casting to " //$NON-NLS-1$
						+ RESTRICTED_CLASS + ": " + e); //$NON-NLS-1$
			}

			startTest("try testing instanceof a restricted package"); //$NON-NLS-1$
			try {
				Object o = new Object();
				if (o instanceof sun.misc.test.RestrictedClass) {
					/* intentionally empty */
				}
				Assert.fail("FAILED: successfully tested instanceof " //$NON-NLS-1$
						+ RESTRICTED_CLASS);
			} catch (SecurityException e) {
				logger.debug("PASSED: instanceof " //$NON-NLS-1$
						+ RESTRICTED_CLASS + ": " + e); //$NON-NLS-1$
			}

			startTest("try calling a static method in a restricted package"); //$NON-NLS-1$
			try {
				sun.misc.test.RestrictedClass.myStaticMethod();
				Assert.fail("FAILED: called static method on " + RESTRICTED_CLASS); //$NON-NLS-1$
			} catch (SecurityException e) {
				logger.debug("PASSED: calling static method on " //$NON-NLS-1$
						+ RESTRICTED_CLASS + " : " + e); //$NON-NLS-1$
			}

			startTest("try accessing a static field in a restricted package"); //$NON-NLS-1$
			try {
				@SuppressWarnings({ "unused" })
				Object result = com.ibm.oti.util.ReflectPermissions.permissionSuppressAccessChecks;
				Assert.fail("FAILED: accessed static field in com.ibm.oti.util.ReflectPermissions"); //$NON-NLS-1$
			} catch (SecurityException e) {
				logger.debug("PASSED: accessing static field on sun.misc.ASCIICaseInsensitiveComparator: " + e); //$NON-NLS-1$
			}
		}
	}

	public static class TestHelper3 extends TestRestricted implements Runnable {
		@Override
		public void run() {
			/* intentionally empty */
		}
	}

	@Test
	public void test_checkPackageAccess() throws Throwable {
		final String className = getClass().getName();

		System.setSecurityManager(new SecurityManager());
		try {
			/*
			 * these tests are not dependent on the ClassLoader behaviour
			 */
			try {
				@SuppressWarnings("rawtypes")
				Class cl = Class.forName(className + "$SuperclassRestricted"); //$NON-NLS-1$
				Assert.fail("FAILED: successfully loaded " //$NON-NLS-1$
						+ cl + " with restricted superclass"); //$NON-NLS-1$
			} catch (SecurityException e) {
				/* intentionally empty */
			}

			try {
				@SuppressWarnings("rawtypes")
				Class cl = Class.forName(className + "$InterfaceRestricted"); //$NON-NLS-1$
				Assert.fail("FAILED: successfully loaded " //$NON-NLS-1$
						+ cl + " with restricted interface"); //$NON-NLS-1$
			} catch (SecurityException e) {
				/* intentionally empty */
			}

		} finally {
			System.setSecurityManager(null);
		}

		/*
		 * Create a ClassLoader with AllPermission so classes loaded by this loader
		 * are not restricted by the default permissions of the application ClassLoader.
		 */
		ClassLoader privilegedLoader = new URLClassLoader(
				new URL[] { getClass().getProtectionDomain().getCodeSource().getLocation() }, null) {
			@SuppressWarnings({ "rawtypes", "unchecked" })
			@Override
			public Class findClass(String name) throws ClassNotFoundException {
				Class result;
				if (name.startsWith(ORG_TESTNG)) {
					result = Class.forName(name, true, ClassLoader.getSystemClassLoader());
				} else {
					if (!name.startsWith(className + TEST_RESTRICTED)) {
						throw new ClassNotFoundException(name);
					}
					result = super.findClass(name);
				}
				return result;
			}

			@Override
			public PermissionCollection getPermissions(CodeSource csource) {
				PermissionCollection permCollection = super.getPermissions(csource);
				/*
				 * Allow AllPermission so we can use this classloader
				 * to define a custom SecurityManager.
				 */
				permCollection.add(new AllPermission());
				return permCollection;
			}
		};

		/*
		 * These tests are dependent on the ClassLoader behaviour.
		 * The default application ClassLoader checks package access of the classes being loaded.
		 * Other ClassLoaders don't have this behaviour
		 * and depend on the VM to do the check. We wish to test the VM behavior.
		 */
		@SuppressWarnings("rawtypes")
		Class cl2 = Class.forName(className + TEST_RESTRICTED, true, privilegedLoader);
		Object testInstance2 = cl2.newInstance();
		@SuppressWarnings("unchecked")
		Method testMethod2 = cl2.getMethod("runTest2", String.class); //$NON-NLS-1$
		testMethod2.invoke(testInstance2, className);
	}

}
