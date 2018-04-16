/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
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

package com.ibm.j9.getcallerclass;

/**
 * Test cases are grouped for the application/bootstrap/extension classloader
 */
public class TestGroup {
	static final String APP_CLASSLOADER = "appClassLoader";
	static final String BOOT_CLASSLOADER = "bootClassLoader";
	static final String EXT_CLASSLOADER = "extClassLoader";

	public static void main(String[] args) {
		TestGroup.classLoaderTest(args[0]);
	}

	public static void classLoaderTest(String classloaderType) {
		boolean testResult = true;

		switch(classloaderType) {
		case APP_CLASSLOADER:
			testResult = TestGroup.fromAppClassLoader();
			break;
		case BOOT_CLASSLOADER:
			testResult = TestGroup.fromBootClassLoader();
			break;
		case EXT_CLASSLOADER:
			testResult = TestGroup.fromExtClassLoader();
			break;
		default:
				System.out.println("Please specify the correct classloader: [appClassLoader, bootClassLoader, extClassLoader]: " + classloaderType);
			return;
		}

		if (true == testResult) {
			System.out.println("All Tests Completed and Passed");
		}
	}

	public static boolean fromAppClassLoader() {
		boolean subTestResult = true;

		subTestResult &= GetCallerClassTests.test_getCallerClass_fromAppWithAnnotation();
		subTestResult &= GetCallerClassTests.test_ensureCalledFromBootstrapClass_fromAppWithAnnotation();

		subTestResult &= GetCallerClassTests.test_getCallerClass_fromAppWithoutAnnotation();
		subTestResult &= GetCallerClassTests.test_ensureCalledFromBootstrapClass_fromAppWithoutAnnotation();

		return subTestResult;
	}

	public static boolean fromBootClassLoader() {
		boolean subTestResult = true;

		subTestResult &= GetCallerClassTests.test_getCallerClass_fromBootExtWithAnnotation();
		subTestResult &= GetCallerClassTests.test_ensureCalledFromBootstrapClass_fromBootWithAnnotation();
		subTestResult &= RefectionMHTests.test_getCallerClass_Helper_Reflection_fromBootExtWithAnnotation();
		subTestResult &= RefectionMHTests.test_getCallerClass_Direct_Reflection_fromBootExtClassLoader();
		subTestResult &= RefectionMHTests.test_getCallerClass_Helper_MethodHandle_fromBootExtWithAnnotation();
		subTestResult &= RefectionMHTests.test_getCallerClass_Direct_MethodHandle_fromBootExtClassLoader();
		subTestResult &= RefectionMHTests.test_getCallerClass_MethodHandle_ArgumentHelper();

		subTestResult &= GetCallerClassTests.test_getCallerClass_fromBootExtWithoutAnnotation();
		subTestResult &= GetCallerClassTests.test_ensureCalledFromBootstrapClass_fromBootExtWithoutAnnotation();

		return subTestResult;
	}

	public static boolean fromExtClassLoader() {
		boolean subTestResult = true;

		subTestResult &= GetCallerClassTests.test_getCallerClass_fromBootExtWithAnnotation();
		subTestResult &= GetCallerClassTests.test_ensureCalledFromBootstrapClass_fromExtWithAnnotation();
		subTestResult &= RefectionMHTests.test_getCallerClass_Helper_Reflection_fromBootExtWithAnnotation();
		subTestResult &= RefectionMHTests.test_getCallerClass_Direct_Reflection_fromBootExtClassLoader();
		subTestResult &= RefectionMHTests.test_getCallerClass_Helper_MethodHandle_fromBootExtWithAnnotation();
		subTestResult &= RefectionMHTests.test_getCallerClass_Direct_MethodHandle_fromBootExtClassLoader();
		subTestResult &= RefectionMHTests.test_getCallerClass_MethodHandle_ArgumentHelper();

		subTestResult &= GetCallerClassTests.test_getCallerClass_fromBootExtWithoutAnnotation();
		subTestResult &= GetCallerClassTests.test_ensureCalledFromBootstrapClass_fromBootExtWithoutAnnotation();

		return subTestResult;
	}
}
