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

package com.ibm.j9.zostest;

/**
 *
 * This is the superclass of all test classes in this package.
 * Its function is simply to load the native library.
 */
public class ZosNativesTest {

	static {
		System.loadLibrary("zostesting");
	}

	public native int ifaSwitchTest1();
	public native int ifaSwitchTest2();
	public native int ifaSwitchTest3();
	public native int ifaSwitchTest4();
	public native int ifaSwitchTest5();
	public native int ifaSwitchTest6();
	public native int ifaSwitchTest7();
	public native int ifaSwitchTest8();
	public native boolean registerNative(Class klass);

	public int testJniNatives() {
		int rc = 0;
		int value = 0;
		TestClass obj = new TestClass();

		try {
			registerNative(obj.getClass());
			value = obj.registeredNativeMethod();

			System.out.print("Registered native method returned: ");
			if (value != 100) {
				System.out.println("FAIL");
				rc = 1;
			} else {
				System.out.println("PASS");
			}
		} catch (UnsatisfiedLinkError e) {
			e.printStackTrace();
			System.out.print("Registered native method returned: FAIL");
			return 1;
		}

		return rc;
	}

	public static void main(String[] args) {
		int rc = 1;
		int testCase = 0;
		String testCaseName = null;

		if (args.length == 1) {
			testCase = Integer.parseInt(args[0]);
			testCaseName = "ifaSwitchTest" + args[0];
		} else {
			System.out.println("Usage : com.ibm.j9.zostest.ZosNativesTest <testCase_number>");
		}
		ZosNativesTest test = new ZosNativesTest();

		switch (testCase) {
			case 1 :
				rc = test.ifaSwitchTest1();
				break;
			case 2 :
				rc = test.ifaSwitchTest2();
				break;
			case 3 :
				rc = test.ifaSwitchTest3();
				break;
			case 4 :
				rc = test.ifaSwitchTest4();
				break;
			case 5 :
				rc = test.ifaSwitchTest5();
				break;
			case 6 :
				rc = test.ifaSwitchTest6();
				break;
			case 7 :
				rc = test.ifaSwitchTest7();
				break;
			case 8 :
				rc = test.ifaSwitchTest8();
				break;
			case 9 :
				rc = test.testJniNatives();
				testCaseName = "testJniNatives";
				break;
			default :
				System.out.println("Invalid test case = " + testCase);
		}

		if (1 == rc) {
			System.out.println("com.ibm.j9.zostest.ZosNativesTest." + testCaseName +" failed");
		} else {
			System.out.println("com.ibm.j9.zostest.ZosNativesTest." + testCaseName +" succeeded");
		}
	}
}
