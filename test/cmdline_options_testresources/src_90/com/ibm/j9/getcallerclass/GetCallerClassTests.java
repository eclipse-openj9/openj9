/*******************************************************************************
 * Copyright (c) 2014, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/

package com.ibm.j9.getcallerclass;

/**
 * The <code>getCallerClassTests</code> class contains all test cases used by
 * the cmdLineTester_getCallerClassTests to verify the behavior when calling
 * getCallerClass()/ensureCalledFromBootstrapClass() from the
 * application/bootstrap/extension classloader
 */
public class GetCallerClassTests {

	/**
	 * Call getCallerClass() with @CallerSensitive annotation from the
	 * application classloader
	 */
	@jdk.internal.reflect.CallerSensitive
	public static boolean test_getCallerClass_fromAppWithAnnotation() {
		final String TESTCASE_NAME = "test_getCallerClass_fromAppWithAnnotation";

		try {
			jdk.internal.reflect.Reflection.getCallerClass();
			System.out.println(TESTCASE_NAME + ": FAILED 1");
			return false;
		} catch (InternalError e) {
			System.out.println(TESTCASE_NAME + ": PASSED");
			return true;
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED 2");
			e.printStackTrace();
			return false;
		}
	}

	/**
	 * Call getCallerClass() without @CallerSensitive annotation from the
	 * application classloader
	 */
	public static boolean test_getCallerClass_fromAppWithoutAnnotation() {
		final String TESTCASE_NAME = "test_getCallerClass_fromAppWithoutAnnotation";

		try {
			jdk.internal.reflect.Reflection.getCallerClass();
			System.out.println(TESTCASE_NAME + ": FAILED 1");
			return false;
		} catch (InternalError e) {
			System.out.println(TESTCASE_NAME + ": PASSED");
			return true;
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED 2");
			e.printStackTrace();
			return false;
		}
	}

	/**
	 * Call getCallerClass() with @CallerSensitive annotation from the
	 * bootstrap/extension classloader
	 */
	@jdk.internal.reflect.CallerSensitive
	public static boolean test_getCallerClass_fromBootExtWithAnnotation() {
		final String TESTCASE_NAME = "test_getCallerClass_fromBootExtWithAnnotation";

		try {
			jdk.internal.reflect.Reflection.getCallerClass();
			System.out.println(TESTCASE_NAME + ": PASSED");
			return true;
		} catch (InternalError e) {
			System.out.println(TESTCASE_NAME + ": FAILED 1");
			e.printStackTrace();
			return false;
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED 2");
			e.printStackTrace();
			return false;
		}
	}

	/**
	 * Method used when calling getCallerClass() via MethodHandle
	 */
	@jdk.internal.reflect.CallerSensitive
	public static Class<?> test_getCallerClass_MethodHandle() {
		Class<?> cls = null;

		try {
			cls = jdk.internal.reflect.Reflection.getCallerClass();
		} catch (InternalError e) {
			return null;
		} catch (Exception e) {
			return null;
		}
		return cls;
	}

	/**
	 * Empty method used when calling getCallerClass() via MethodHandle using an
	 * ArgumentHelper
	 */
	public static Class<?> test_getCallerClass_ArgumentHelper(Class<?> cls) {
		return cls;
	}

	/**
	 * Call getCallerClass() without @CallerSensitive annotation from the
	 * bootstrap/extension classloader
	 */
	public static boolean test_getCallerClass_fromBootExtWithoutAnnotation() {
		final String TESTCASE_NAME = "test_getCallerClass_fromBootExtWithoutAnnotation";

		try {
			jdk.internal.reflect.Reflection.getCallerClass();
			System.out.println(TESTCASE_NAME + ": FAILED 1");
			return false;
		} catch (InternalError e) {
			System.out.println(TESTCASE_NAME + ": PASSED");
			return true;
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED 2");
			e.printStackTrace();
			return false;
		}
	}

	/**
	 * Call ensureCalledFromBootstrapClass() with @CallerSensitive annotation
	 * from the application classloader
	 */
	@jdk.internal.reflect.CallerSensitive
	public static boolean test_ensureCalledFromBootstrapClass_fromAppWithAnnotation() {
		final String TESTCASE_NAME = "test_ensureCalledFromBootstrapClass_fromAppWithAnnotation";

		try {
			com.ibm.oti.vm.VM.ensureCalledFromBootstrapClass();
			System.out.println(TESTCASE_NAME + ": FAILED 1");
			return false;
		} catch (InternalError e) {
			System.out.println(TESTCASE_NAME + ": PASSED");
			return true;
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED 2");
			e.printStackTrace();
			return false;
		}
	}

	/**
	 * Call ensureCalledFromBootstrapClass() without @CallerSensitive annotation
	 * from the application classloader
	 */
	public static boolean test_ensureCalledFromBootstrapClass_fromAppWithoutAnnotation() {
		final String TESTCASE_NAME = "test_ensureCalledFromBootstrapClass_fromAppWithoutAnnotation";

		try {
			com.ibm.oti.vm.VM.ensureCalledFromBootstrapClass();
			System.out.println(TESTCASE_NAME + ": FAILED 1");
			return false;
		} catch (InternalError e) {
			System.out.println(TESTCASE_NAME + ": PASSED");
			return true;
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED 2");
			e.printStackTrace();
			return false;
		}
	}

	/**
	 * Call ensureCalledFromBootstrapClass() with @CallerSensitive annotation
	 * from the bootstrap classloader
	 */
	@jdk.internal.reflect.CallerSensitive
	public static boolean test_ensureCalledFromBootstrapClass_fromBootWithAnnotation() {
		final String TESTCASE_NAME = "test_ensureCalledFromBootstrapClass_fromBootWithAnnotation";

		try {
			com.ibm.oti.vm.VM.ensureCalledFromBootstrapClass();
			System.out.println(TESTCASE_NAME + ": PASSED");
			return true;
		} catch (InternalError e) {
			System.out.println(TESTCASE_NAME + ": FAILED 1");
			e.printStackTrace();
			return false;
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED 2");
			e.printStackTrace();
			return false;
		}
	}

	/**
	 * Call ensureCalledFromBootstrapClass() with @CallerSensitive annotation
	 * from the extension classloader
	 */
	@jdk.internal.reflect.CallerSensitive
	public static boolean test_ensureCalledFromBootstrapClass_fromExtWithAnnotation() {
		final String TESTCASE_NAME = "test_ensureCalledFromBootstrapClass_fromExtWithAnnotation";

		try {
			com.ibm.oti.vm.VM.ensureCalledFromBootstrapClass();
			System.out.println(TESTCASE_NAME + ": FAILED");
			return false;
		} catch (SecurityException e) {
			/* SecurityException is expected to be thrown out if not called from bootstrap classloader */
			System.out.println(TESTCASE_NAME + ": PASSED");
			return true;
		}
	}

	/**
	 * Call ensureCalledFromBootstrapClass() without @CallerSensitive annotation
	 * from the bootstrap/extension classloader
	 */
	public static boolean test_ensureCalledFromBootstrapClass_fromBootExtWithoutAnnotation() {
		final String TESTCASE_NAME = "test_ensureCalledFromBootstrapClass_fromBootExtWithoutAnnotation";

		try {
			com.ibm.oti.vm.VM.ensureCalledFromBootstrapClass();
			System.out.println(TESTCASE_NAME + ": FAILED 1");
			return false;
		} catch (InternalError e) {
			System.out.println(TESTCASE_NAME + ": PASSED");
			return true;
		} catch (SecurityException e) {
			System.out.println(TESTCASE_NAME + ": FAILED 2");
			e.printStackTrace();
			return false;
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED 3");
			e.printStackTrace();
			return false;
		}
	}
}
