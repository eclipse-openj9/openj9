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

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * Test cases intended for reflection and MethodHandle
 */
public class RefectionMHTests {

	/**
	 * Call getCallerClass() with a helper method via reflection from the bootstrap/extension
	 * classloader
	 */
	public static boolean test_getCallerClass_Helper_Reflection_fromBootExtWithAnnotation() {
		final String TESTCASE_NAME = "test_getCallerClass_Helper_Reflection_fromBootExtWithAnnotation";
		Method method = null;
		Class<?> cls = null;

		try {
			method = GetCallerClassTests.class.getDeclaredMethod("test_getCallerClass_MethodHandle");
			cls = (Class<?>) method.invoke(null, new Object[0]);

			if (cls == RefectionMHTests.class) {
				System.out.println(TESTCASE_NAME + ": PASSED: return " + cls.getName());
				return true;
			} else {
				System.out.println(TESTCASE_NAME + ": FAILED: return " + cls.getName());
				return false;
			}
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED");
			e.printStackTrace();
			return false;
		}
	}

	/**
	 * Call getCallerClass() directly via reflection from the bootstrap/extension
	 * classloader
	 */
	public static boolean test_getCallerClass_Direct_Reflection_fromBootExtClassLoader() {
		final String TESTCASE_NAME = "test_getCallerClass_Direct_Reflection_fromBootExtClassLoader";
		Method method = null;

		try {
			method = jdk.internal.reflect.Reflection.class.getDeclaredMethod("getCallerClass");
			method.invoke(jdk.internal.reflect.Reflection.class);
			System.out.println(TESTCASE_NAME + ": FAILED 1");
			return false;
		} catch (InvocationTargetException e) {
			if (java.lang.InternalError.class == e.getCause().getClass()) {
				System.out.println(TESTCASE_NAME + ": PASSED");
				return true;
			} else {
				System.out.println(TESTCASE_NAME + ": FAILED 2");
				e.printStackTrace();
				return false;
			}
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED 3");
			e.printStackTrace();
			return false;
		} catch (Throwable e) {
			System.out.println(TESTCASE_NAME + ": FAILED 4");
			e.printStackTrace();
			return false;
		}
	}

	/**
	 * Call getCallerClass() with a helper method via MethodHandle from the bootstrap/extension
	 * classloader
	 */
	public static boolean test_getCallerClass_Helper_MethodHandle_fromBootExtWithAnnotation() {
		final String TESTCASE_NAME = "test_getCallerClass_Helper_MethodHandle_fromBootExtWithAnnotation";
		MethodHandles.Lookup lookup = MethodHandles.lookup();
		MethodType methodType = MethodType.methodType(Class.class);
		MethodHandle methodHandle = null;
		Class<?> cls = null;

		try {
			methodHandle = lookup.findStatic(GetCallerClassTests.class, "test_getCallerClass_MethodHandle", methodType);
			cls = (Class<?>) methodHandle.invoke();

			if ("java.lang.invoke.SecurityFrame" == cls.getName()) {
				System.out.println(TESTCASE_NAME + ": PASSED: return " + cls.getName());
				return true;
			} else {
				System.out.println(TESTCASE_NAME + ": FAILED: return " + cls.getName());
				return false;
			}
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED 2");
			e.printStackTrace();
			return false;
		} catch (Throwable e) {
			System.out.println(TESTCASE_NAME + ": FAILED 3");
			e.printStackTrace();
			return false;
		}
	}

	/**
	 * Call getCallerClass() directly via MethodHandle from the bootstrap/extension
	 * classloader
	 */
	public static boolean test_getCallerClass_Direct_MethodHandle_fromBootExtClassLoader() {
		final String TESTCASE_NAME = "test_getCallerClass_Direct_MethodHandle_fromBootExtClassLoader";
		MethodHandles.Lookup lookup = MethodHandles.lookup();
		MethodType methodType = MethodType.methodType(Class.class);
		MethodHandle methodHandle;

		try {
			methodHandle = lookup.findStatic(jdk.internal.reflect.Reflection.class, "getCallerClass", methodType);
			methodHandle.invoke();
			System.out.println(TESTCASE_NAME + ": FAILED 1");
			return false;
		} catch (InternalError e) {
			System.out.println(TESTCASE_NAME + ": PASSED");
			return true;
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED 2");
			e.printStackTrace();
			return false;
		} catch (Throwable e) {
			System.out.println(TESTCASE_NAME + ": FAILED 3");
			e.printStackTrace();
			return false;
		}
	}

	/**
	 * Call getCallerClass() via MethodHandle using an ArgumentHelper from the
	 * bootstrap/extension classloader
	 */
	public static boolean test_getCallerClass_MethodHandle_ArgumentHelper() {
		final String TESTCASE_NAME = "test_getCallerClass_MethodHandle_ArgumentHelper";
		MethodHandles.Lookup lookup = MethodHandles.lookup();
		Class<?> cls = null;

		try {
			MethodType typeCombiner = MethodType.methodType(Class.class);
			MethodHandle mhCombiner = lookup.findStatic(GetCallerClassTests.class, "test_getCallerClass_MethodHandle", typeCombiner);

			MethodType typeTarget = MethodType.methodType(Class.class, Class.class);
			MethodHandle mhTarget = lookup.findStatic(GetCallerClassTests.class, "test_getCallerClass_ArgumentHelper", typeTarget);

			MethodHandle mhResult = MethodHandles.foldArguments(mhTarget, mhCombiner);
			cls = (Class<?>) mhResult.invoke();

			if ("java.lang.invoke.SecurityFrame" == cls.getName()) {
				System.out.println(TESTCASE_NAME + ": PASSED: return " + cls.getName());
				return true;
			} else {
				System.out.println(TESTCASE_NAME + ": FAILED: return " + cls.getName());
				return false;
			}
		} catch (Exception e) {
			System.out.println(TESTCASE_NAME + ": FAILED 2");
			e.printStackTrace();
			return false;
		} catch (Throwable e) {
			System.out.println(TESTCASE_NAME + ": FAILED 3");
			e.printStackTrace();
			return false;
		}
	}
}
