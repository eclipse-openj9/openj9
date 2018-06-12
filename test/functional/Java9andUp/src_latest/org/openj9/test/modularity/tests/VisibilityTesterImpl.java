/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

package org.openj9.test.modularity.tests;
import static java.util.Objects.isNull;
import static java.util.Objects.nonNull;
import static org.openj9.test.modularity.tests.MethodVisibilityTests.logger;
import static org.testng.Assert.assertEquals;
import static org.testng.Assert.fail;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.util.Objects;

import org.openj9.test.modularity.common.VisibilityTester;
import org.testng.annotations.Test;

public class VisibilityTesterImpl implements VisibilityTester  {
	private static final String NON_REQUIRED_PACKAGE = "pkgD";
	private static final Class<IllegalAccessException> IAEXC_CLASS = IllegalAccessException.class;
	private static final String NO_THROWABLE_THROWN = "No exception or error thrown";
	private static final Class<ClassNotFoundException> CNFE_CLASS = ClassNotFoundException.class;
	private static final Class<IllegalAccessError> IAERR_CLASS = IllegalAccessError.class;
	private static final String LORG_OPENJ9_TEST_MODULARITY = "()Lorg/openj9/test/modularity/";
	private static final String ORG_OPENJ9_TEST_MODULARITY = "org.openj9.test.modularity";
	private static final String packages[] = new String[] {"pkgAe", "pkgBe", "pkgCe", "pkgDe", 
			"pkgAo", "pkgBo", "pkgCo", "pkgDo", 
			"pkgAr", "pkgBr", "pkgCr", "pkgDr"};

	@Override
	public void testClassForNameAndFindVirtual() {
		logger.debug("\n=================================\ntest Class.forName() and findVirtual()");
		ClassLoader myClassLoader = getClassLoader();
		for (String pkg: packages) {
			logger.debug("test " + pkg);
			Class<?> resultClass = loadClass(myClassLoader, pkg);

			if (Objects.nonNull(resultClass)) {
				Lookup looker = MethodHandles.lookup();
				Throwable actualFVException = null;
				logger.debug("* test Class.forName()");
				Class<? extends Exception> expectedFVThrowable;
				if (pkg.startsWith(NON_REQUIRED_PACKAGE)) {
					expectedFVThrowable = CNFE_CLASS;
				} else if (isInaccessible(pkg)) {
					expectedFVThrowable = IAEXC_CLASS;
				} else {
					expectedFVThrowable = null;
				}
				logger.debug(NO_THROWABLE_THROWN);
				logger.debug("* test MethodHandles.lookup().findVirtual()");
				try {
					looker.findVirtual(resultClass, "meth", MethodType.methodType(Integer.class));
					logger.debug(NO_THROWABLE_THROWN);
				} catch (Throwable e) {
					actualFVException = e;
					reportException(e);
				}
				checkException("MethodHandles.lookup().findVirtual()", expectedFVThrowable, actualFVException);
			}
			logger.debug("-----------------------------------\n");
		}
	}

	@Test(groups = { "level.extended" })
	public void testFromMethodDesc() {
		logger.debug("\n=================================\ntest MethodType.fromMethodDescriptorString()");
		ClassLoader myClassLoader = getClassLoader();
		for (String pkg: packages) {
			Class<? extends Exception> expectedExceptionType;
			Exception actualException = null;
			logger.debug("test " + pkg);

			if (pkg.startsWith(NON_REQUIRED_PACKAGE)) {
				expectedExceptionType = TypeNotPresentException.class;
			} else {
				expectedExceptionType = null;
			}
			try {	
				logger.debug("fromMethodDescriptorString for " + pkg + " = " +
						MethodType.fromMethodDescriptorString(LORG_OPENJ9_TEST_MODULARITY
								+ pkg + "/Klass;", myClassLoader));
				logger.debug(NO_THROWABLE_THROWN);
			} catch (Exception e) {
				actualException = e;
				reportException(e);		
			}
			checkException("fromMethodDescriptorString()", expectedExceptionType, actualException);

			logger.debug("-----------------------------------\n");
		}
	}

	@Override
	public void testFindClass() {
		logger.debug("\n=================================\ntest MethodHandles.lookup().findClass()");
		for (String pkg: packages) {
			Class<? extends Exception> expectedExceptionType;
			Throwable actualException = null;
			logger.debug("test " + pkg);

			Lookup looker = MethodHandles.lookup();
			if (pkg.startsWith(NON_REQUIRED_PACKAGE)) {
				expectedExceptionType = CNFE_CLASS;
			} else if (isInaccessible(pkg)) {
				expectedExceptionType = IAEXC_CLASS;
			} else {
				expectedExceptionType = null;
			}
			try {
				looker.findClass(ORG_OPENJ9_TEST_MODULARITY + "." + pkg + ".Klass");
				logger.debug(NO_THROWABLE_THROWN);
			} catch (Throwable e) {
				actualException = e;
				reportException(e);
			}
			checkException("MethodHandles.lookup().findClass()", expectedExceptionType, actualException);
			logger.debug("-----------------------------------\n");
		}
	}

	@Override
	public void testLoadMethodHandle() {
		String UNEXPORTED_PACKAGE = "ldc with package from unexported package";
		String UNREAD_MODULE = "ldc with package from unread module";
		Throwable actualThrown = null;
		logger.debug("\n=================================\ntest method handles with invisible types");
		/* these are visible via reflection */
		logger.debug("Test visible packages");
		MethodHandleTester.pkgAe();
		MethodHandleTester.pkgAo();
		MethodHandleTester.pkgBe();
		MethodHandleTester.pkgBo(); 
		
		logger.debug("Test unexported packages");
		try {
			MethodHandleTester.pkgAr();
		} catch (Throwable e) {
			actualThrown = e;
			reportException(e);		
		}
		checkException(UNEXPORTED_PACKAGE+": pkgAr", IAERR_CLASS, actualThrown);
		
		try {
			MethodHandleTester.pkgBr();
		} catch (Throwable e) {
			actualThrown = e;
			reportException(e);		
		}
		checkException(UNEXPORTED_PACKAGE+": pkgBr", IAERR_CLASS, actualThrown);

		logger.debug("Test unread module");
		try {
			MethodHandleTester.pkgCe();
		} catch (Throwable e) {
			actualThrown = e;
			reportException(e);		
		}
		checkException(UNREAD_MODULE+": pkgCe", IAERR_CLASS, actualThrown);
		
		try {
			MethodHandleTester.pkgCo();
		} catch (Throwable e) {
			reportException(e);		
			actualThrown = e;
		}
		checkException(UNREAD_MODULE+": pkgCo", IAERR_CLASS, actualThrown);
		
		try {
			MethodHandleTester.pkgCr();
		} catch (Throwable e) {
			reportException(e);		
			actualThrown = e;
		}
		checkException(UNREAD_MODULE+": pkgCr", IAERR_CLASS, actualThrown);
	}

	@Override
	public void testFindStatic() {
		logger.debug("\n=================================\ntest findStatic()");
		ClassLoader myClassLoader = getClassLoader();
		for (String pkg: packages) {
			logger.debug("test " + pkg);

			Class<?> resultClass = loadClass(myClassLoader, pkg);

			if (Objects.nonNull(resultClass)) {
				Lookup looker = MethodHandles.lookup();
				Lookup pubLookup = MethodHandles.publicLookup();
				Throwable actualFSThrowable = null;
				Class<? extends Exception> expectedFSThrowable;
				if (pkg.startsWith(NON_REQUIRED_PACKAGE)) {
					expectedFSThrowable = CNFE_CLASS;
				} else if (isInaccessible(pkg)) {
					expectedFSThrowable = IAEXC_CLASS;
				} else {
					expectedFSThrowable = null;
				}
				logger.debug("* test MethodHandles.lookup().findStatic()");
				try {
					looker.findStatic(resultClass, "stat", MethodType.methodType(Integer.class));
					logger.debug(NO_THROWABLE_THROWN);
				} catch (Throwable e) {
					actualFSThrowable = e;
					reportException(e);
				}
				checkException("MethodHandles.lookup().findStatic()", expectedFSThrowable, actualFSThrowable);

				logger.debug("* test MethodHandles.publicLookup().findStatic()");
				if (pkg.startsWith(NON_REQUIRED_PACKAGE)) {
					expectedFSThrowable = CNFE_CLASS;
				} else if (pkg.endsWith("r")) {
					expectedFSThrowable = IAEXC_CLASS;
				} else {
					expectedFSThrowable = null;
				}
				actualFSThrowable = null;
				try {
					pubLookup.findStatic(resultClass, "stat", MethodType.methodType(Integer.class));
					logger.debug(NO_THROWABLE_THROWN);
				} catch (Throwable e) {
					actualFSThrowable = e;
					reportException(e);
				}
				checkException("MethodHandles.publicLookup().findStatic()", expectedFSThrowable, actualFSThrowable);
			}
			logger.debug("-----------------------------------\n");
		}
	}

	ClassLoader getClassLoader() {
		Class<? extends VisibilityTesterImpl> myClass = this.getClass();
		Module myModule = myClass.getModule();
		ClassLoader myClassLoader = myModule.getClassLoader();
		logger.debug("myModule="+myModule.getName()+" module classloader ="+myClassLoader);
		return myClassLoader;
	}

	boolean isInaccessible(String pkg) {
		return pkg.startsWith("pkgC") || pkg.endsWith("r");
	}

	/**
	 * Tries to load a test class in the named package.  Catch and check exceptions.
	 * @param myClassLoader Classloader to use
	 * @param pkg Enclosing package of the test class
	 * @return class object, or null if an expected throwable was thrown
	 */
	Class<?> loadClass(ClassLoader myClassLoader, String pkg) {
		Class<? extends Exception> expectedCFNThrowable;
		Throwable actualCFNException = null;
		if (pkg.startsWith(NON_REQUIRED_PACKAGE)) {
			expectedCFNThrowable = CNFE_CLASS;
		} else {
			expectedCFNThrowable = null;
		}
		Class<?> resultClass = null;
		try {
			resultClass = Class.forName(ORG_OPENJ9_TEST_MODULARITY + "." + pkg + ".Klass", false, myClassLoader);
		} catch (Throwable t) {
			actualCFNException = t;
			reportException(t);
		}
		checkException("Class.forName()", expectedCFNThrowable, actualCFNException);
		return resultClass;
	}

	void reportException(Throwable e) {
		logger.debug("Exception " + e.getClass().getName() + " thrown with message \""+e.getMessage()+"\"");
	}

	static void checkException(String msg, Class<? extends Throwable> expectedExceptionType, Throwable actualException) {
		if (nonNull(actualException)) {
			if (isNull(expectedExceptionType)) {
				fail("unexpected exception or error thrown for "+msg, actualException);
			} else {
				assertEquals(actualException.getClass(), expectedExceptionType, 
						"Wrong type of throwable thrown for "+msg);
			}
			logger.debug("exception check passed");
		} else 	if (nonNull(expectedExceptionType)) {
			fail("expected throwable " +expectedExceptionType.getName() + " not thrown for "+msg);
		}
	}
}
