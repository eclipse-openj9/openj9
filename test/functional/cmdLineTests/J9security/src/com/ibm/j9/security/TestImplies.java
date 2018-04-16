/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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

package com.ibm.j9.security;

import java.io.*;
import java.lang.reflect.*;
import java.net.*;
import java.security.*;
import java.util.*;

public class TestImplies {

	public static class RestrictedClass implements TestRunner {
		static boolean failed = false;
		static int testCount = 1;
		static int pass;
		static boolean calledImplies = false;

		public static void failed(String output) {
			failed = true;
			System.out.println("FAILED: " + output);
		}
		public static void startTest(String testing) {
			System.out.println("\nTest " + pass + " / " + testCount++ + ": " + testing);
		}
		public static boolean passed() {
			return !failed;
		}

		public void run(final int hint) {
			pass = hint;
			// stop security checks here, as this class has RuntimePermission("setSecurityManager") but the calling class does not
			AccessController.doPrivileged(new PrivilegedAction() {
				public Object run() {
					runTest(hint);
					return null;
				}
			});
		}

		public void runTest(final int hint) {
			startTest("check assumptions, new AccessControlContext(new ProtectionDomain[]) does not require special permission");
			try {
				new AccessControlContext(new ProtectionDomain[]{});
				System.out.println("GOOD: created ACC");
			} catch (SecurityException e) {
				failed("unexpected: " + e);
			}

			// ------------------------------------------------------------------------------
			startTest("check assumptions, new AccessControlContext(new AccessControlContext(new ProtectionDomain[]), DomainCombiner) does require special permission");
			CustomSecurityManager.enableLogging = false;
			try {
				new AccessControlContext(new AccessControlContext(new ProtectionDomain[]{}), (DomainCombiner)null);
				if (hint == 1) {
					failed("exepected AccessControlException checking createAccessControlContext");
				} else {
					System.out.println("GOOD: creation is allowed when createAccessControlContext is granted");
				}
			} catch (SecurityException e) {
				if (hint == 1) {
					if  (e.getMessage().indexOf("createAccessControlContext") == -1) {
						failed("could have failed for the wrong reason");
						e.printStackTrace();
					} else {
						System.out.println("GOOD: got expected: " + e);
					}
				} else {
					failed("unexpected: " + e);
				}
			}
			CustomSecurityManager.enableLogging = true;

			// ------------------------------------------------------------------------------
			startTest("check assumptions, new AccessControlContext(AccessController.getContext(), DomainCombiner) does require special permission");
			CustomSecurityManager.enableLogging = false;
			try {
				new AccessControlContext(AccessController.getContext(), (DomainCombiner)null);
				if (hint == 1) {
					failed("exepected AccessControlException checking createAccessControlContext");
				} else {
					System.out.println("GOOD: creation is allowed when createAccessControlContext is granted");
				}
			} catch (SecurityException e) {
				if (hint == 1) {
					if  (e.getMessage().indexOf("createAccessControlContext") == -1) {
						failed("could have failed for the wrong reason");
						e.printStackTrace();
					} else {
						System.out.println("GOOD: got expected: " + e);
					}
				} else {
					failed("unexpected: " + e);
				}
			}
			CustomSecurityManager.enableLogging = true;

			// ------------------------------------------------------------------------------
			startTest("Cannot call an untrusted implies() method with restricted action");
			calledImplies = false;
			ProtectionDomain pd1 = new ProtectionDomain(null, null) {
				public boolean implies(Permission perm) {
					if (hint == 1) {
						failed("should not be called");
					} else {
						calledImplies = true;
					}
					return true;
				}
			};
			try {
				AccessController.doPrivileged(new PrivilegedAction() {
					public Object run() {
						new File("abc").exists();
						return null;
					}
				}, new AccessControlContext(new ProtectionDomain[]{pd1}));
				failed("untrusted action should not be permitted");
			} catch (SecurityException e) {
				System.out.println("GOOD: expected: " + e);
			}
			if (hint == 2 && !calledImplies) {
				failed("untrusted implies() should be called");
			}

			// ------------------------------------------------------------------------------
			startTest("Cannot call an untrusted implies() method with allowed action");
			calledImplies = false;
			ProtectionDomain pd2 = new ProtectionDomain(null, null) {
				public boolean implies(Permission perm) {
					if (hint == 1) {
						failed("should not be called");
					} else {
						calledImplies = true;
					}
					return true;
				}
			};
			try {
				AccessController.doPrivileged(new PrivilegedAction() {
					public Object run() {
						return System.getProperty("java.version");
					}
				}, new AccessControlContext(new ProtectionDomain[]{pd1}));
				if (hint == 1) {
					failed("trusted action is not allowed when using an untrusted pd");
				} else {
					System.out.println("GOOD: permitted action is allowed");
				}
			} catch (SecurityException e) {
				if (hint == 1) {
					System.out.println("GOOD: expected: " + e);
					e.printStackTrace();
				} else {
					failed("unexpected: " + e);
				}
			}
			if (hint == 2 && !calledImplies) {
				failed("untrusted implies() should be called");
			}

			// ------------------------------------------------------------------------------
			startTest("Cannot execute restricted action with AccessController.getContext()");
			try {
				String value = (String)AccessController.doPrivileged(new PrivilegedAction() {
					public Object run() {
						return System.getProperty("user.home");
					}
				}, AccessController.getContext());
				failed("untrusted action should not be permitted");
			} catch (SecurityException e) {
				System.out.println("GOOD: expected: " + e);
			}

			// ------------------------------------------------------------------------------
			startTest("Can execute allowed action with AccessControlContext(AccessControlContext, DomainCombiner)");
			CustomSecurityManager.enableLogging = false;
			try {
				AccessController.doPrivileged(new PrivilegedAction() {
					public Object run() {
						return System.getProperty("java.version");
					}
				}, new AccessControlContext(AccessController.getContext(), null));
				if (hint == 1) {
					failed("trusted action is not allowed when using an untrusted pd");
				} else {
					System.out.println("GOOD: permitted action is allowed");
				}
			} catch (SecurityException e) {
				if (hint == 1) {
					System.out.println("GOOD: expected: " + e);
					e.printStackTrace();
				} else {
					failed("unexpected: " + e);
				}
			}
			CustomSecurityManager.enableLogging = true;
		}
	}

	public static interface TestRunner {
		public void run(int hint);
	}

	public static class CustomSecurityManager extends SecurityManager {
		public static boolean enableLogging = false;
		public static Permission lastPermission;
		public static int count = 0;
		public void checkPermission(Permission perm) {
			if (enableLogging) {
				count++;
				lastPermission = perm;
				if (perm.getName().equals("createAccessControlContext")) {
					throw new Error("found check " + count);
				}
			}
			super.checkPermission(perm);
		}
	}

public static void main(String[] args) throws Throwable {
	new TestImplies().runTest();
}

public void runTest() throws Throwable {
	final String baseName = getClass().getName();
	ClassLoader parentPrivilegedLoader = new URLClassLoader(new URL[]{getClass().getProtectionDomain().getCodeSource().getLocation()}, null) {
		public Class findClass(String name) throws ClassNotFoundException {
			if (!name.equals(baseName + "$CustomSecurityManager")
					&& !name.equals(baseName + "$TestRunner")
			) {
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
	ClassLoader loader1 = new URLClassLoader(new URL[]{getClass().getProtectionDomain().getCodeSource().getLocation()}, parentPrivilegedLoader) {
		public PermissionCollection getPermissions(CodeSource csource) {
			PermissionCollection permCollection = super.getPermissions(csource);
			permCollection.add(new RuntimePermission("setSecurityManager"));
			return permCollection;
		}
	};
	ClassLoader loader2 = new URLClassLoader(new URL[]{getClass().getProtectionDomain().getCodeSource().getLocation()}, parentPrivilegedLoader) {
		public PermissionCollection getPermissions(CodeSource csource) {
			PermissionCollection permCollection = super.getPermissions(csource);
			permCollection.add(new RuntimePermission("setSecurityManager"));
			permCollection.add(new SecurityPermission("createAccessControlContext"));
			return permCollection;
		}
	};

	Class securityClass = Class.forName(baseName + "$CustomSecurityManager", true, parentPrivilegedLoader);
	System.setSecurityManager((SecurityManager)securityClass.newInstance());

	Class cl1 = Class.forName(baseName + "$RestrictedClass", true, loader1);
	Object testInstance1 = cl1.newInstance();
	Method testMethod1 = cl1.getMethod("run", Integer.TYPE);
	testMethod1.invoke(testInstance1, 1);

	Class cl2 = Class.forName(baseName + "$RestrictedClass", true, loader2);
	Object testInstance2 = cl2.newInstance();
	Method testMethod2 = cl2.getMethod("run", Integer.TYPE);
	testMethod2.invoke(testInstance2, 2);

	Method passedMethod1 = cl1.getMethod("passed");
	Method passedMethod2 = cl2.getMethod("passed");
	boolean passed = ((Boolean)passedMethod1.invoke(testInstance1)).booleanValue()
			&& ((Boolean)passedMethod2.invoke(testInstance2)).booleanValue();
	if (passed) {
		System.out.println("\nALL TESTS COMPLETED AND PASSED");
	}

}

}
