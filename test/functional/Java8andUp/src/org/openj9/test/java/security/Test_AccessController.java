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

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Method;
import java.net.URL;
import java.security.AccessControlContext;
import java.security.AccessControlException;
import java.security.AccessController;
import java.security.AllPermission;
import java.security.CodeSource;
import java.security.DomainCombiner;
import java.security.PermissionCollection;
import java.security.Permissions;
import java.security.PrivilegedAction;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import java.security.ProtectionDomain;
import java.security.SecurityPermission;
import java.util.PropertyPermission;

import org.testng.annotations.AfterMethod;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity" })
public class Test_AccessController {

	static final String PASSED_SECURITY_EXCEPTION_EXPECTED = "PASSED: security exception expected."; //$NON-NLS-1$
	static final String PASSED_ACCESS_CONTROL_EXCEPTION_EXPECTED = "PASSED: expected AccessControlException thrown"; //$NON-NLS-1$
	static Logger logger = Logger.getLogger(Test_AccessController.class);
	private SecurityManager oldSecurityManager;

	/**
	 *  java.security.AccessController#AccessController()
	 */
	@Test
	public void test_Constructor() {
		try {
			AccessController ac = (AccessController)AccessController.class.newInstance();
		} catch (IllegalAccessException e) {
			return;
		} catch (Exception e) {
			return;
		}
		AssertJUnit.assertTrue("Class instantiation should not be possible.", false);
	}

	/**
	 *  java.security.AccessController#doPrivileged(java.security.
	 *        PrivilegedAction)
	 */
	@Test
	public void test_doPrivileged() {
		// Test for method java.lang.Object
		// java.security.AccessController.doPrivileged(java.security.PrivilegedAction)

		// Pass fail flag...
		Boolean pass;

		// First test 1 argument version (TBD).

		// Then test 2 argument version. These belong in one of the
		// other test methods, but since they haven't been generated,
		// they live here.

		pass = (Boolean)AccessController.doPrivileged(new PrivilegedAction() {
			public Object run() {
				try {
					AccessController.checkPermission(new AllPermission());
					return new Boolean(false);
				} catch (SecurityException ex) {
					return new Boolean(true);
				}
			}
		}, null);
		AssertJUnit.assertTrue("Got AllPermission by passing in a null PD", pass.booleanValue());

	}

	/**
	 *
	 *        java.security.AccessController#doPrivilegedWithCombiner(java.security
	 *        .PrivilegedAction)
	 */
	@Test
	public void test_doPrivilegedWithCombiner() {
		class MyDomainCombiner implements DomainCombiner {
			public ProtectionDomain[] combine(ProtectionDomain[] executionDomains, ProtectionDomain[] parentDomains) {
				Permissions perms = new Permissions();
				perms.add(new RuntimePermission("checking"));
				return new ProtectionDomain[] { new ProtectionDomain(null, perms) };
			}
		}

		AccessControlContext acc1 = new AccessControlContext(
				new ProtectionDomain[] { new ProtectionDomain(null, new Permissions()) });
		AccessControlContext acc = new AccessControlContext(acc1, new MyDomainCombiner());
		AccessController.doPrivileged(new PrivilegedAction() {
			public Object run() {
				AccessController.doPrivilegedWithCombiner(new PrivilegedAction() {
					public Object run() {
						AccessController.checkPermission(new RuntimePermission("checking"));
						return null;
					}
				});
				try {
					AccessController.doPrivileged(new PrivilegedAction() {
						public Object run() {
							AccessController.checkPermission(new RuntimePermission("checking"));
							return null;
						}
					});
					Assert.fail("Expected SecurityException");
				} catch (SecurityException e) {
					// expected
				}
				return null;
			}
		}, acc);
	}

	/**
	 *
	 *        java.security.AccessController#doPrivilegedWithCombiner(java.security
	 *        .PrivilegedExceptionAction)
	 */
	@Test
	public void test_doPrivilegedWithCombiner2() {
		class MyDomainCombiner implements DomainCombiner {
			public ProtectionDomain[] combine(ProtectionDomain[] executionDomains, ProtectionDomain[] parentDomains) {
				Permissions perms = new Permissions();
				perms.add(new RuntimePermission("checking"));
				return new ProtectionDomain[] { new ProtectionDomain(null, perms) };
			}
		}

		AccessControlContext acc1 = new AccessControlContext(
				new ProtectionDomain[] { new ProtectionDomain(null, new Permissions()) });
		AccessControlContext acc = new AccessControlContext(acc1, new MyDomainCombiner());
		AccessController.doPrivileged(new PrivilegedAction() {
			public Object run() {
				try {
					AccessController.doPrivilegedWithCombiner(new PrivilegedExceptionAction() {
						public Object run() {
							AccessController.checkPermission(new RuntimePermission("checking"));
							return null;
						}
					});
				} catch (PrivilegedActionException e) {
					Assert.fail("Unexpected1: " + e);
				}
				try {
					AccessController.doPrivileged(new PrivilegedExceptionAction() {
						public Object run() {
							AccessController.checkPermission(new RuntimePermission("checking"));
							return null;
						}
					});
					Assert.fail("Expected SecurityException");
				} catch (SecurityException e) {
					// expected
				} catch (PrivilegedActionException e) {
					Assert.fail("Unexpected2: " + e);
				}
				return null;
			}
		}, acc);
	}

	private final static String PROP_USER = "user.dir";

	/**
	 *
	 *        java.security.AccessController#doPrivilegedWithCombiner(java.security.PrivilegedAction)
	 *
	 */
	@Test
	public void test_doPrivilegedWithCombiner4() {
		ClassLoader cl = new TestURLClassLoader(
				new URL[] { getClass().getProtectionDomain().getCodeSource().getLocation() }, null) {
			public PermissionCollection getPermissions(CodeSource cs) {
				PermissionCollection pc = super.getPermissions(cs);
				pc.add(new PropertyPermission(PROP_USER, "read"));
				return pc;
			}
		};
		try {
			Class<?> c = Class.forName("org.openj9.test.java.security.Test_AccessController$TestClass", true, cl);
			Object o = c.newInstance();
			Method m = c.getMethod("test", AccessControlContext.class);
			Boolean result = (Boolean)(m.invoke(o, AccessController.getContext()));
			if (!result) {
				Assert.fail("test_doPrivilegedWithCombiner4 failed!");
			}
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception thrown: " + e);
		}
	}

	public static class TestClass {
		private final static PropertyPermission READ_PROP_USER_DIR = new PropertyPermission(PROP_USER, "read");

		public Boolean test(AccessControlContext accNoPerm) {
			return AccessController.doPrivileged(new PrivilegedAction<Boolean>() {
				public Boolean run() {
					try {
						try {
							AccessControlContext accNoMH = AccessController.getContext();
							AccessControlContext accViaMH = (AccessControlContext)MethodHandles.lookup()
									.findStatic(AccessController.class, "getContext",
											MethodType.methodType(AccessControlContext.class))
									.invoke();
							if (!accNoMH.equals(accViaMH)) {
								logger.error("AccessControlContext returned from AccessController.getContext() should be equal w/o MethodHandles.");
								return false;
							}
						} catch (Throwable e) {
							logger.error("FAIL: unexpected exception." + e);
							return false;
						}

						AccessController.checkPermission(READ_PROP_USER_DIR);
						logger.error("FAILED: checkPermission should NOT succeed!");
						return false;
					} catch (AccessControlException ace) {
						logger.debug(PASSED_ACCESS_CONTROL_EXCEPTION_EXPECTED);
					}
					return AccessController.doPrivilegedWithCombiner(new PrivilegedAction<Boolean>() {
						public Boolean run() {
							try {
								try {
									AccessControlContext accNoMH = AccessController.getContext();
									AccessControlContext accViaMH = (AccessControlContext)MethodHandles.lookup()
											.findStatic(AccessController.class, "getContext",
													MethodType.methodType(AccessControlContext.class))
											.invoke();
									if (!accNoMH.equals(accViaMH)) {
										logger.error("AccessControlContext returned from AccessController.getContext() should be equal w/o MethodHandles.");
										return false;
									}
								} catch (Throwable e) {
									logger.error("FAIL: unexpected exception." + e);
									return false;
								}

								AccessController.checkPermission(READ_PROP_USER_DIR);
								logger.error("FAILED: checkPermission should NOT succeed!");
								return false;
							} catch (AccessControlException ace) {
								logger.debug(PASSED_ACCESS_CONTROL_EXCEPTION_EXPECTED);
							}
							return true;
						}
					});
				}
			}, accNoPerm);
		}
	}

	/**
	 *  java.security.AccessController#getContext()
	 */
	@Test
	public void test_getContext() {
		/* [PR CMVC 137464] Null pointer exception in AccessController */
		class MyCombiner implements DomainCombiner {
			public ProtectionDomain[] combine(ProtectionDomain[] executionDomains, ProtectionDomain[] parentDomains) {
				return null;
			}
		}

		AccessController.doPrivileged(new PrivilegedAction() {
			public Object run() {
				AccessController.getContext();
				return null;
			}
		}, new AccessControlContext(AccessController.getContext(), new MyCombiner()));
	}

	/**
	 *  java.security.AccessController#doPrivilegedWithCombiner(java.security.PrivilegedAction)
	 *  java.security.AccessController#doPrivilegedWithCombiner(java.security.PrivilegedExceptionAction)
	 */
	@Test
	public void test_doPrivilegedWithCombiner5() {
		Boolean pass = false;

		System.setSecurityManager(new SecurityManager());
		TestPrivilegedAction tpa = new TestPrivilegedAction();
		TestPrivilegedExceptionAction tpea = new TestPrivilegedExceptionAction();

		try {
			AccessController.doPrivilegedWithCombiner(tpa);
			Assert.fail("Expected AccessControlException from doPrivilegedWithCombiner(PrivilegedAction<T> action)");
		} catch (AccessControlException e) {
			pass = true;
		}
		AssertJUnit.assertTrue("Expected AccessControlException", pass);
		pass = false;

		try {
			AccessController.doPrivilegedWithCombiner(tpea);
			Assert.fail(
					"Expected AccessControlException from doPrivilegedWithCombiner(PrivilegedExceptionAction<T> action)");
		} catch (AccessControlException e) {
			pass = true;
		} catch (PrivilegedActionException e) {
			Assert.fail("Expected AccessControlException, got PrivilegedActionException instead");
		}

		AssertJUnit.assertTrue("Expected AccessControlException", pass);
	}

	static class TestPrintOut {
		public static void printout(Object obj, PrivilegedAction action) {
			logger.debug("obj = " + obj + "; action = " + action);
		}
	}

	/**
	 *  java.security.AccessController#doPrivileged(java.security.PrivilegedAction)
	 */
	@Test
	public void test_doPrivilegedFrameStackWalking() {
		final String PROP_JAVA_HOME = "java.home";

		com.ibm.oti.util.PriviAction pa = new com.ibm.oti.util.PriviAction(PROP_JAVA_HOME);
		System.setSecurityManager(new SecurityManager());
		try {
			String result = (String)java.security.AccessController.doPrivileged(pa);
			Assert.fail("FAIL: security exception NOT thrown, get system property " + PROP_JAVA_HOME + " : " + result);
		} catch (java.security.AccessControlException e) {
			logger.debug(PASSED_SECURITY_EXCEPTION_EXPECTED);
		}

		/*
		 * Loop 20 time here such that the reflection object are optimized and bytecodes are created in a new class
		 * (after calling invoke() about 15 times), which introduces a slightly different reflection frame
		 */
		for (int i = 0; i < 20; i++) {
			try {
				Method m = AccessController.class.getMethod("doPrivileged", PrivilegedAction.class);
				String result = (String)m.invoke(null, pa);
				Assert.fail(
						"FAIL: security exception NOT thrown, get system property " + PROP_JAVA_HOME + " : " + result);
			} catch (java.lang.reflect.InvocationTargetException e) {
				if (e.getCause() instanceof java.security.AccessControlException) {
					logger.debug(PASSED_SECURITY_EXCEPTION_EXPECTED);
				} else {
					e.printStackTrace();
					Assert.fail("FAIL: unexpected exception.");
				}
			} catch (Exception e) {
				e.printStackTrace();
				Assert.fail("FAIL: unexpected exception.");
			}
		}

		try {
			String result = (String)MethodHandles.lookup().findStatic(AccessController.class, "doPrivileged",
					MethodType.methodType(Object.class, PrivilegedAction.class)).invoke(pa);
			Assert.fail("FAIL: security exception NOT thrown, get system property " + PROP_JAVA_HOME + " : " + result);
		} catch (java.security.AccessControlException e) {
			logger.debug(PASSED_SECURITY_EXCEPTION_EXPECTED);
		} catch (Throwable e) {
			e.printStackTrace();
			Assert.fail("FAIL: unexpected exception.", e);
		}

		try {
			MethodHandles
					.foldArguments(
							MethodHandles.lookup().findStatic(TestPrintOut.class, "printout",
									MethodType.methodType(void.class, Object.class, PrivilegedAction.class)),
							MethodHandles.lookup().findStatic(AccessController.class, "doPrivileged",
									MethodType.methodType(Object.class, PrivilegedAction.class)))
					.invokeExact((PrivilegedAction)pa);
			Assert.fail("FAIL: security exception NOT thrown");
		} catch (java.security.AccessControlException e) {
			logger.debug(PASSED_SECURITY_EXCEPTION_EXPECTED);
		} catch (Throwable e) {
			e.printStackTrace();
			Assert.fail("FAIL: unexpected exception.");
		}
	}

	/**
	 *  java.security.AccessController#doPrivileged(java.security.PrivilegedAction, AccessControlContext)
	 */
	@Test
	public void test_doPrivileged_createAccessControlContext() {
		/*
		 * Classes loaded by this Classloader withPermCL have the Permission JAVA_HOME_READ & CREATE_ACC
		 */
		ClassLoader withPermCL = new TestURLClassLoader(
				new URL[] { this.getClass().getProtectionDomain().getCodeSource().getLocation() }, null) {
			public PermissionCollection getPermissions(CodeSource cs) {
				PermissionCollection pc = super.getPermissions(cs);
				pc.add(new PropertyPermission("java.home", "read"));
				pc.add(new SecurityPermission("createAccessControlContext"));
				return pc;
			}
		};
		try {
			Class<?> mwp = Class.forName("org.openj9.test.java.security.Test_AccessController$MainWithPerm", true,
					withPermCL);
			Object mwpObj = mwp.newInstance();
			Method m = mwp.getDeclaredMethod("testCreateACC");
			m.setAccessible(true);
			m.invoke(mwpObj);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("FAIL: TEST FAILED, probably setup issue.");
		}
	}

	public static class MainWithPerm {
		private static final PropertyPermission JAVA_HOME_READ = new PropertyPermission("java.home", "read");
		private static final PropertyPermission USER_DIR_READ = new PropertyPermission("user.dir", "read");
		private static final PermissionCollection PERMISSIONS = new Permissions();
		private static final AccessControlContext ACC_JAVA_HOME;
		static {
			PERMISSIONS.add(JAVA_HOME_READ);
			ACC_JAVA_HOME = new AccessControlContext(
					new ProtectionDomain[] { new ProtectionDomain(null, PERMISSIONS) });
		}

		@Test
		public void testCreateACC() {
			// Classes loaded by this Classloader noCreateACCPermCL only have the Permission JAVA_HOME_READ but NOT CREATE_ACC
			ClassLoader noCreateACCPermCL = new TestURLClassLoader(
					new URL[] { this.getClass().getProtectionDomain().getCodeSource().getLocation() }, null) {
				public PermissionCollection getPermissions(CodeSource cs) {
					PermissionCollection pc = super.getPermissions(cs);
					pc.add(JAVA_HOME_READ);
					return pc;
				}
			};

			try {
				Class<?> ancap = Class.forName("org.openj9.test.java.security.Test_AccessController$AppNoCreateACCPriv",
						true, noCreateACCPermCL);
				final Object ancapObj = ancap.newInstance();
				final Method m2 = ancap.getDeclaredMethod("doSomething");
				m2.setAccessible(true);
				m2.invoke(ancapObj);
				final Method methodDoSomethingWithdoPrivilegedV1 = ancap
						.getDeclaredMethod("doSomethingWithdoPrivilegedV1");
				methodDoSomethingWithdoPrivilegedV1.setAccessible(true);
				final Method methodDoSomethingWithdoPrivilegedV2 = ancap
						.getDeclaredMethod("doSomethingWithdoPrivilegedV2");
				methodDoSomethingWithdoPrivilegedV2.setAccessible(true);
				final Method methodDoSomethingWithdoPrivilegedV3 = ancap
						.getDeclaredMethod("doSomethingWithdoPrivilegedV3");
				methodDoSomethingWithdoPrivilegedV3.setAccessible(true);

				System.setSecurityManager(new SecurityManager());

				//	Negative tests show that doPrivileged caller without permission "createAccessControlContext" is not allowed
				//	Verification check were performed within methods of AppNoCreateACCPriv
				methodDoSomethingWithdoPrivilegedV1.invoke(ancapObj);
				methodDoSomethingWithdoPrivilegedV2.invoke(ancapObj);
				methodDoSomethingWithdoPrivilegedV3.invoke(ancapObj);

				//	Positive tests show that doPrivileged caller with permission "createAccessControlContext" is allowed
				Object ret = AccessController.doPrivileged(new PrivilegedAction<Object>() {
					public Object run() {
						try {
							/* This tests a use scenario with following stacktrace from a standalone testcase
							 * that there is no security exception thrown.
							 * 	TestCreateACCPriv$AppNoCreateACCPriv.doSomething()	---> NO SecurityPermission("createAccessControlContext")
							 * 	NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
							 * 	NativeMethodAccessorImpl.invoke(Object, Object[])
							 * 	DelegatingMethodAccessorImpl.invoke(Object, Object[])
							 * 	Method.invoke(Object, Object...)
							 * 	TestCreateACCPriv$MainWithPerm$2.run()
							 * 	AccessController.doPrivileged(PrivilegedAction<T>, AccessControlContext)
							 * 	TestCreateACCPriv$MainWithPerm.testCreateACC()	 --> WITH SecurityPermission("createAccessControlContext")
							 */
							m2.invoke(ancapObj);
							return ancapObj;
						} catch (Exception e) {
							e.printStackTrace();
							Assert.fail("FAIL: TEST FAILED, probably setup issue.");
						}
						return null;
					}
				}, ACC_JAVA_HOME);
				if (null == ret) {
					Assert.fail("FAIL: TEST FAILED");
				}
				/*
				 * Stacktrace from a stand-alone testcase
				 * AccessController.checkPermission(Permission)
				 * SecurityManager.checkPermission(Permission)
				 * SecurityManager.checkPropertyAccess(String)
				 * System.getProperty(String, String)
				 * System.getProperty(String)
				 * TestCreateACCPriv$AppNoCreateACCPriv.doSomething()	---> With PropertyPermission("java.home", "read") but NO SecurityPermission("createAccessControlContext")
				 * NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
				 * NativeMethodAccessorImpl.invoke(Object, Object[])
				 * DelegatingMethodAccessorImpl.invoke(Object, Object[])
				 * Method.invoke(Object, Object...)
				 * TestCreateACCPriv$MainWithPerm$3.run()
				 * AccessController.doPrivileged(PrivilegedAction<T>, AccessControlContext, Permission...)
				 * TestCreateACCPriv$MainWithPerm.testCreateACC()	---> With PropertyPermission("java.home", "read") AND SecurityPermission("createAccessControlContext")
				 * NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
				 * NativeMethodAccessorImpl.invoke(Object, Object[])
				 * DelegatingMethodAccessorImpl.invoke(Object, Object[])
				 * Method.invoke(Object, Object...)
				 * TestCreateACCPriv.test()
				 * TestCreateACCPriv.main(String[])
				 */
				ret = AccessController.doPrivileged(new PrivilegedAction<Object>() {
					public Object run() {
						try {
							m2.invoke(ancapObj);
							return ancapObj;
						} catch (Exception e) {
						}
						return null;
					}
				}, ACC_JAVA_HOME, JAVA_HOME_READ);
				if (null == ret) {
					Assert.fail("FAIL: TEST FAILED");
				}
				/*
				 * Stacktrace from a stand-alone testcase
				 * AccessController.checkPermission(Permission)
				 * SecurityManager.checkPermission(Permission)
				 * SecurityManager.checkPropertyAccess(String)
				 * System.getProperty(String, String)
				 * System.getProperty(String)
				 * TestCreateACCPriv$AppNoCreateACCPriv.doSomething()	---> With PropertyPermission("java.home", "read") but NO SecurityPermission("createAccessControlContext")
				 * NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
				 * NativeMethodAccessorImpl.invoke(Object, Object[])
				 * DelegatingMethodAccessorImpl.invoke(Object, Object[])
				 * Method.invoke(Object, Object...)
				 * TestCreateACCPriv$MainWithPerm$4$1.run()
				 * AccessController.doPrivileged(PrivilegedAction<T>, AccessControlContext, Permission...)	---> With PropertyPermission("user.dir", "read")
				 * TestCreateACCPriv$MainWithPerm$4.run()
				 * AccessController.doPrivileged(PrivilegedAction<T>, AccessControlContext, Permission...)	---> With PropertyPermission("java.home", "read")
				 * TestCreateACCPriv$MainWithPerm.testCreateACC()	---> With PropertyPermission("java.home", "read") AND SecurityPermission("createAccessControlContext")
				 * NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
				 * NativeMethodAccessorImpl.invoke(Object, Object[])
				 * DelegatingMethodAccessorImpl.invoke(Object, Object[])
				 * Method.invoke(Object, Object...)
				 * TestCreateACCPriv.test()
				 * TestCreateACCPriv.main(String[])
				 */
				ret = AccessController.doPrivileged(new PrivilegedAction<Object>() {
					public Object run() {
						try {
							return AccessController.doPrivileged(new PrivilegedAction<Object>() {
								public Object run() {
									try {
										m2.invoke(ancapObj);
										return ancapObj;
									} catch (Exception e) {
									}
									return null;
								}
							}, ACC_JAVA_HOME, USER_DIR_READ);
						} catch (Exception e) {
						}
						return null;
					}
				}, ACC_JAVA_HOME, JAVA_HOME_READ);
				if (null == ret) {
					Assert.fail("FAIL: TEST FAILED");
				}
				/*
				 * Stacktrace from a stand-alone testcase
				 * AccessController.checkPermission(Permission)
				 * SecurityManager.checkPermission(Permission)
				 * SecurityManager.checkPropertyAccess(String)
				 * System.getProperty(String, String)
				 * System.getProperty(String)
				 * TestCreateACCPriv$AppNoCreateACCPriv.doSomething()	---> With PropertyPermission("java.home", "read") but NO SecurityPermission("createAccessControlContext")
				 * NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
				 * NativeMethodAccessorImpl.invoke(Object, Object[])
				 * DelegatingMethodAccessorImpl.invoke(Object, Object[])
				 * Method.invoke(Object, Object...)
				 * TestCreateACCPriv$MainWithPerm$5$1.run()
				 * AccessController.doPrivileged(PrivilegedAction<T>, AccessControlContext, Permission...)	---> With PropertyPermission("user.dir", "read")
				 * TestCreateACCPriv$MainWithPerm$5.run()
				 * AccessController.doPrivileged(PrivilegedAction<T>, AccessControlContext)	---> full doPrivileged frame
				 * TestCreateACCPriv$MainWithPerm.testCreateACC()	---> With PropertyPermission("java.home", "read") AND SecurityPermission("createAccessControlContext")
				 * NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
				 * NativeMethodAccessorImpl.invoke(Object, Object[])
				 * DelegatingMethodAccessorImpl.invoke(Object, Object[])
				 * Method.invoke(Object, Object...)
				 * TestCreateACCPriv.test()
				 * TestCreateACCPriv.main(String[])
				 */
				ret = AccessController.doPrivileged(new PrivilegedAction<Object>() {
					public Object run() {
						try {
							return AccessController.doPrivileged(new PrivilegedAction<Object>() {
								public Object run() {
									try {
										m2.invoke(ancapObj);
										return ancapObj;
									} catch (Exception e) {
									}
									return null;
								}
							}, ACC_JAVA_HOME, USER_DIR_READ);
						} catch (Exception e) {
						}
						return null;
					}
				}, ACC_JAVA_HOME);
				if (null == ret) {
					Assert.fail("FAIL: TEST FAILED");
				}
				logger.debug("TEST PASSED");
			} catch (Exception e) {
				e.printStackTrace();
				Assert.fail("FAIL: TEST FAILED, probably setup issue.", e);
			}
		}
	}

	public static class AppNoCreateACCPriv {
		private static final String PASSED_EXPECTED_ACCESS_CONTROL_EXCEPTION_THROWN = "PASSED: Expected AccessControlException thrown";
		private static final PropertyPermission JAVA_HOME_READ = new PropertyPermission("java.home", "read");
		private static final PropertyPermission USER_DIR_READ = new PropertyPermission("user.dir", "read");
		private static final PermissionCollection PERMISSIONS = new Permissions();
		private static final AccessControlContext ACC_JAVA_HOME;
		static {
			PERMISSIONS.add(JAVA_HOME_READ);
			ACC_JAVA_HOME = new AccessControlContext(
					new ProtectionDomain[] { new ProtectionDomain(null, PERMISSIONS) });
		}

		public void doSomething() {
			logger.debug("Here is AppNoCreateACCPriv.doSomething()");
			logger.debug("System.property: java.home = " + System.getProperty("java.home"));
		}

		public void doSomethingWithdoPrivilegedV1() {
			logger.debug("Here is AppNoCreateACCPriv.doSomethingWithdoPrivileged1()");
			AccessController.doPrivileged(new PrivilegedAction<Object>() {
				public Object run() {
					try {
						String propValue = System.getProperty("java.home");
						logger.debug("System.property: java.home = " + propValue);
						Assert.fail("FAIL: Expected AccessControlException");
					} catch (AccessControlException e) {
						// AccessControlException expected
						/* Stacktrace from a stand-alone testcase
						 * AccessController.checkPermission(Permission)
						 * SecurityManager.checkPermission(Permission)
						 * SecurityManager.checkPropertyAccess(String)
						 * System.getProperty(String, String)
						 * System.getProperty(String)
						 * TestCreateACCPriv$AppNoCreateACCPriv$1.run()
						 * AccessController.doPrivileged(PrivilegedAction<T>, AccessControlContext)
						 * TestCreateACCPriv$AppNoCreateACCPriv.doSomethingWithdoPrivilegedV1()	---> With PropertyPermission("java.home", "read") but NO SecurityPermission("createAccessControlContext")
						 * NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
						 * NativeMethodAccessorImpl.invoke(Object, Object[])
						 * DelegatingMethodAccessorImpl.invoke(Object, Object[])
						 * Method.invoke(Object, Object...)
						 * TestCreateACCPriv$MainWithPerm$2.run()
						 * AccessController.doPrivileged(PrivilegedAction<T>, AccessControlContext)
						 * TestCreateACCPriv$MainWithPerm.testCreateACC()	---> With PropertyPermission("java.home", "read") AND SecurityPermission("createAccessControlContext")
						 * NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
						 * NativeMethodAccessorImpl.invoke(Object, Object[])
						 * DelegatingMethodAccessorImpl.invoke(Object, Object[])
						 * Method.invoke(Object, Object...)
						 * TestCreateACCPriv.test()
						 */
						logger.debug(PASSED_EXPECTED_ACCESS_CONTROL_EXCEPTION_THROWN);											
					} catch (Exception e) {
						Assert.fail("FAIL: Unexpected exception");
					}
					return null;
				}
			}, ACC_JAVA_HOME);
		}

		public void doSomethingWithdoPrivilegedV2() {
			logger.debug("Here is AppNoCreateACCPriv.doSomethingWithdoPrivileged2()");
			AccessController.doPrivileged(new PrivilegedAction<Object>() {
				public Object run() {
					try {
						String propValue = System.getProperty("java.home");
						logger.debug("System.property: java.home = " + propValue);
						Assert.fail("FAIL: Expected AccessControlException");
					} catch (AccessControlException e) {
						// AccessControlException expected
						/* Stacktrace from a stand-alone testcase
						 * AccessController.checkPermission(Permission)
						 * SecurityManager.checkPermission(Permission)
						 * SecurityManager.checkPropertyAccess(String)
						 * System.getProperty(String, String)
						 * System.getProperty(String)
						 * TestCreateACCPriv$AppNoCreateACCPriv$2.run()
						 * AccessController.doPrivileged(PrivilegedAction<T>, AccessControlContext, Permission...)
						 * TestCreateACCPriv$AppNoCreateACCPriv.doSomethingWithdoPrivilegedV2()	---> With PropertyPermission("java.home", "read") but NO SecurityPermission("createAccessControlContext")
						 * NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
						 * NativeMethodAccessorImpl.invoke(Object, Object[])
						 * DelegatingMethodAccessorImpl.invoke(Object, Object[])
						 * Method.invoke(Object, Object...)
						 * TestCreateACCPriv$MainWithPerm$3.run()
						 * AccessController.doPrivileged(PrivilegedAction<T>, AccessControlContext)
						 * TestCreateACCPriv$MainWithPerm.testCreateACC()	---> With PropertyPermission("java.home", "read") AND SecurityPermission("createAccessControlContext")
						 * NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
						 * NativeMethodAccessorImpl.invoke(Object, Object[])
						 * DelegatingMethodAccessorImpl.invoke(Object, Object[])
						 * Method.invoke(Object, Object...)
						 * TestCreateACCPriv.test()
						 */
						logger.debug(PASSED_EXPECTED_ACCESS_CONTROL_EXCEPTION_THROWN);						
					} catch (Exception e) {
						Assert.fail("FAIL: Unexpected exception");
					}
					return null;
				}
			}, ACC_JAVA_HOME, USER_DIR_READ);
		}

		public void doSomethingWithdoPrivilegedV3() {
			logger.debug("Here is AppNoCreateACCPriv.doSomethingWithdoPrivileged2()");
			AccessController.doPrivileged(new PrivilegedAction<Object>() {
				public Object run() {
					try {
						String propValue = System.getProperty("java.home");
						logger.debug("System.property: java.home = " + propValue);
						Assert.fail("FAIL: Expected AccessControlException");
					} catch (AccessControlException e) {
						// AccessControlException expected
						/* Stacktrace from a stand-alone testcase
						 * AccessController.checkPermission(Permission)
						 * SecurityManager.checkPermission(Permission)
						 * SecurityManager.checkPropertyAccess(String)
						 * System.getProperty(String, String)
						 * System.getProperty(String)
						 * TestCreateACCPriv$AppNoCreateACCPriv$3.run()
						 * AccessController.doPrivileged(PrivilegedAction<T>, AccessControlContext, Permission...)
						 * TestCreateACCPriv$AppNoCreateACCPriv.doSomethingWithdoPrivilegedV3()	---> With PropertyPermission("java.home", "read") but NO SecurityPermission("createAccessControlContext")
						 * NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
						 * NativeMethodAccessorImpl.invoke(Object, Object[])
						 * DelegatingMethodAccessorImpl.invoke(Object, Object[])
						 * Method.invoke(Object, Object...)
						 * TestCreateACCPriv$MainWithPerm$4.run()
						 * AccessController.doPrivileged(PrivilegedAction<T>, AccessControlContext)
						 * TestCreateACCPriv$MainWithPerm.testCreateACC()	---> With PropertyPermission("java.home", "read") AND SecurityPermission("createAccessControlContext")
						 * NativeMethodAccessorImpl.invoke0(Method, Object, Object[])
						 * NativeMethodAccessorImpl.invoke(Object, Object[])
						 * DelegatingMethodAccessorImpl.invoke(Object, Object[])
						 * Method.invoke(Object, Object...)
						 * TestCreateACCPriv.test()
						 */
					} catch (Exception e) {
						Assert.fail("FAIL: Unexpected exception");
					}
					return null;
				}
			}, ACC_JAVA_HOME, JAVA_HOME_READ);
		}
	}
	
	@BeforeMethod
	public void printTestEntry(Method testMethod) {
		logger.debug("> Enter "+testMethod.getName());
		oldSecurityManager = System.getSecurityManager();
	}
	
	@AfterMethod
	public void printTestExit(Method testMethod) {
		System.setSecurityManager(oldSecurityManager);
		logger.debug("< Exit "+testMethod.getName());
	}
}
