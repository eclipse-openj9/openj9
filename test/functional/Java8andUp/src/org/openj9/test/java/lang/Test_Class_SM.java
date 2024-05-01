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
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.lang.ClassLoader;
import java.lang.reflect.Member;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.security.*;

import org.openj9.test.support.resource.Support_Resources;

/**
 *
 * @requiredClass org.openj9.test.support.resource.Support_Resources
 * @requiredResource org/openj9/resources/openj9tr_compressD.txt
 * @requiredResource org/openj9/resources/openj9tr_general.jar
 * @requiredResource org/openj9/resources/openj9tr_Foo.c
 */
@Test(groups = { "level.sanity" })
public class Test_Class_SM {
	private static final Logger logger = Logger.getLogger(Test_Class_SM.class);

	@Test
	public void test_getMethods_subtest2() {
		PrivilegedAction<Object> action = new PrivilegedAction<Object>() {
			@Override
			public Object run() {
				try {
					File resources = Support_Resources.createTempFolder();
					String[] expected_E = new String[] {
							"org.openj9.resources.classinheritance.E.method_N(java.lang.String)",
							"org.openj9.resources.classinheritance.E.method_M(java.lang.String)",
							"org.openj9.resources.classinheritance.A.method_K(java.lang.String)",
							"org.openj9.resources.classinheritance.E.method_L(java.lang.String)",
							"org.openj9.resources.classinheritance.E.method_O(java.lang.String)",
							"org.openj9.resources.classinheritance.B.method_K(java.lang.String)"};

					Support_Resources.copyFile(resources, null, "openj9tr_general.jar");
					File file = new File(resources.toString() + "/openj9tr_general.jar");
					URL url = new URL("file:" + file.getPath());
					ClassLoader loader = new URLClassLoader(new URL[] { url }, null);

					Class<?> cls_E = Class.forName("org.openj9.resources.classinheritance.E", false, loader);
					Method[] methodNames_E = cls_E.getMethods();
					AssertJUnit.assertEquals("Returned incorrect number of methods for cls_E", expected_E.length + Object.class.getMethods().length, methodNames_E.length);
					for (int i = 0; i < expected_E.length; i++) {
						boolean match = false;
						for (int j = 0; j < methodNames_E.length; j++) {
							if (methodNames_E[j].toString().indexOf(expected_E[i]) != -1) {
								match = true;
							}
						}
						AssertJUnit.assertTrue("Expected method " + expected_E[i] + " not found.", match);
					}
				} catch (MalformedURLException e) {
					logger.error("Unexpected exception: " + e);
				} catch (ClassNotFoundException e) {
					logger.error("Unexpected exception: " + e);
				}

				return null;
			}
		};
		AccessController.doPrivileged(action);
	}

	/**
	 * @tests java.lang.Class#getClasses()
	 */
	@Test
	public void test_getClasses2() {
		final Permission privCheckPermission = new BasicPermission("Privilege check") {
		};
		class MyCombiner implements DomainCombiner {
			boolean combine;
			public ProtectionDomain[] combine(ProtectionDomain[] executionDomains, ProtectionDomain[] parentDomains) {
				combine = true;
				return new ProtectionDomain[0];
			}
			public boolean isPriviledged() {
				combine = false;
				try {
					AccessController.checkPermission(privCheckPermission);
				} catch (SecurityException e) {
				}
				return !combine;
			}
		};
		final MyCombiner combiner = new MyCombiner();
		class SecurityManagerCheck extends SecurityManager {
			String reason;
			Class<?> checkClass;
			int checkType;
			int checkPermission = 0;
			int checkMemberAccess = 0;
			int checkPackageAccess = 0;

			public void setExpected(String reason, Class<?> cls, int type) {
				this.reason = reason;
				checkClass = cls;
				checkType = type;
				checkPermission = 0;
				checkMemberAccess = 0;
				checkPackageAccess = 0;
			}
			public void checkPermission(Permission perm) {
				if (combiner.isPriviledged())
					return;
				checkPermission++;
			}
			public void checkPackageAccess(String packageName) {
				if (packageName.startsWith("java.") || packageName.startsWith("org.openj9.test.java.lang")) {
					return;
				}
				if (combiner.isPriviledged())
					return;
				checkPackageAccess++;
				String name = checkClass.getName();
				int index = name.lastIndexOf('.');
				String checkPackage = name.substring(0, index);
				AssertJUnit.assertTrue(reason + " unexpected package: " + packageName, packageName.equals(checkPackage));
			}
			public void assertProperCalls() {
				int declared = checkType == Member.DECLARED ? 1 : 0;
				AssertJUnit.assertTrue(reason + " unexpected checkPermission count: " + checkPermission, checkPermission == declared);
				AssertJUnit.assertTrue(reason + " unexpected checkPackageAccess count: " + checkPackageAccess, checkPackageAccess == 1);
			}
		}

		final SecurityManagerCheck sm = new SecurityManagerCheck();

		AccessControlContext acc = new AccessControlContext(new ProtectionDomain[0]);
		AccessControlContext acc2 = new AccessControlContext(acc, combiner);

		PrivilegedAction action = new PrivilegedAction() {
			public Object run() {
				File resources = Support_Resources.createTempFolder();
				try {
					Support_Resources.copyFile(resources, null, "openj9tr_general.jar");
					File file = new File(resources.toString() + "/openj9tr_general.jar");
					URL url = new URL("file:" + file.getPath());
					ClassLoader loader = new URLClassLoader(new URL[]{url}, null);
					Class<?> cls = Class.forName("org.openj9.resources.security.SecurityTestSub", false, loader);
					// must preload junit.framework.Assert before installing SecurityManager
					// otherwise loading it inside checkPackageAccess() is recursive
					AssertJUnit.assertTrue("preload assertions", true);
					System.setSecurityManager(sm);
					try {
						sm.setExpected("getClasses", cls, java.lang.reflect.Member.PUBLIC);
						cls.getClasses();
						sm.assertProperCalls();

						sm.setExpected("getDeclaredClasses", cls, java.lang.reflect.Member.DECLARED);
						cls.getDeclaredClasses();
						sm.assertProperCalls();

						sm.setExpected("getConstructor", cls, java.lang.reflect.Member.PUBLIC);
						cls.getConstructor(new Class[0]);
						sm.assertProperCalls();

						sm.setExpected("getConstructors", cls, java.lang.reflect.Member.PUBLIC);
						cls.getConstructors();
						sm.assertProperCalls();

						sm.setExpected("getDeclaredConstructor", cls, java.lang.reflect.Member.DECLARED);
						cls.getDeclaredConstructor(new Class[0]);
						sm.assertProperCalls();

						sm.setExpected("getDeclaredConstructors", cls, java.lang.reflect.Member.DECLARED);
						cls.getDeclaredConstructors();
						sm.assertProperCalls();

						sm.setExpected("getField", cls, java.lang.reflect.Member.PUBLIC);
						cls.getField("publicField");
						sm.assertProperCalls();

						sm.setExpected("getFields", cls, java.lang.reflect.Member.PUBLIC);
						cls.getFields();
						sm.assertProperCalls();

						sm.setExpected("getDeclaredField", cls, java.lang.reflect.Member.DECLARED);
						cls.getDeclaredField("publicField");
						sm.assertProperCalls();

						sm.setExpected("getDeclaredFields", cls, java.lang.reflect.Member.DECLARED);
						cls.getDeclaredFields();
						sm.assertProperCalls();

						sm.setExpected("getDeclaredMethod", cls, java.lang.reflect.Member.DECLARED);
						cls.getDeclaredMethod("publicMethod", new Class[0]);
						sm.assertProperCalls();

						sm.setExpected("getDeclaredMethods", cls, java.lang.reflect.Member.DECLARED);
						cls.getDeclaredMethods();
						sm.assertProperCalls();

						sm.setExpected("getMethod", cls, java.lang.reflect.Member.PUBLIC);
						cls.getMethod("publicMethod", new Class[0]);
						sm.assertProperCalls();

						sm.setExpected("getMethods", cls, java.lang.reflect.Member.PUBLIC);
						cls.getMethods();
						sm.assertProperCalls();

						sm.setExpected("newInstance", cls, java.lang.reflect.Member.PUBLIC);
						cls.newInstance();
						sm.assertProperCalls();
					} finally {
						System.setSecurityManager(null);
					}
				} catch (Exception e) {
					if (e instanceof RuntimeException)
						throw (RuntimeException)e;
					Assert.fail("unexpected exception: " + e);
				}
				return null;
			}
		};
		AccessController.doPrivileged(action, acc2);
	}

	/**
	 * @tests java.lang.Class#getResource(java.lang.String)
	 */
	@Test
	public void test_getResource() {
		// Test for method java.net.URL java.lang.Class.getResource(java.lang.String)
		System.setSecurityManager(new SecurityManager());
		try {
			URL res = Object.class.getResource("Object.class");
			AssertJUnit.assertTrue("Object.class should not be found", res == null);
		} finally {
			System.setSecurityManager(null);
		}

		String name = "/org/openj9/resources/openj9tr_Foo.c";

		// find resource from object
		AssertJUnit.assertTrue("directory of this class can be found",
				Test_Class_SM.class.getResource(name) != null);

		// find resource from array of objects
		AssertJUnit.assertTrue("directory of array of this class can be found",
				Test_Class_SM[].class.getResource(name) != null);
	}

	/**
	 * @tests java.lang.Class#getResourceAsStream(java.lang.String)
	 */
	@Test
	public void test_getResourceAsStream() {
		// Test for method java.io.InputStream java.lang.Class.getResourceAsStream(java.lang.String)
		String name = Support_Resources.RESOURCE_PACKAGE + "openj9tr_compressD.txt";
		Class<?> clazz = null;
		try {
			clazz = Class.forName("org.openj9.test.java.lang.Test_Class_SM");
		} catch (ClassNotFoundException e) {
			AssertJUnit.assertTrue("Should be able to find the class org.openj9.test.java.lang.Test_Class_SM", false);
		}
		AssertJUnit.assertTrue("the file " + name + " can not be found in this directory",
			clazz.getResourceAsStream(name) != null);

		System.setSecurityManager(new SecurityManager());
		try {
			InputStream res = Object.class.getResourceAsStream("Object.class");
			AssertJUnit.assertTrue("Object.class should not be found", res == null);
		} finally {
			System.setSecurityManager(null);
		}

		name = "org/openj9/resources/openj9tr_Foo.c";
		AssertJUnit.assertTrue("the file " + name + " should not be found in this directory",
			clazz.getResourceAsStream(name) == null);
		AssertJUnit.assertTrue("the file " + name + " can be found in the root directory",
			clazz.getResourceAsStream("/" + name) != null);
		// find resource from array of objects
		AssertJUnit.assertTrue("the file " + name + " can be found in the root directory where the class is an array",
				Test_Class_SM[].class.getResourceAsStream("/" + name) != null);

		try {
			clazz = Class.forName("java.lang.Object");
		} catch (ClassNotFoundException e) {
			AssertJUnit.assertTrue("Should be able to find the class java.lang.Object", false);
		}
		InputStream str = clazz.getResourceAsStream("Class.class");
		AssertJUnit.assertTrue(
			"java.lang.Object couldn't find its class with getResource...",
			str != null);
		try {
			AssertJUnit.assertTrue("Cannot read single byte", str.read() != -1);
			AssertJUnit.assertTrue("Cannot read multiple bytes", str.read(new byte[5]) == 5);
			str.close();
		} catch (IOException e) {
			AssertJUnit.assertTrue("Exception while closing resource stream 1.", false);
		}

		InputStream str2 = getClass().getResourceAsStream(Support_Resources.RESOURCE_PACKAGE + "openj9tr_compressD.txt");
		AssertJUnit.assertTrue("Can't find resource", str2 != null);
		try {
			AssertJUnit.assertTrue("Cannot read single byte", str2.read() != -1);
			AssertJUnit.assertTrue("Cannot read multiple bytes", str2.read(new byte[5]) == 5);
			str2.close();
		} catch (IOException e) {
			AssertJUnit.assertTrue("Exception while closing resource stream 2.", false);
		}
	}

}
