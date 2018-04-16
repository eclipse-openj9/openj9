/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.ibm.jvmti.tests.decompileAtMethodResolve;

public class ResolveTest {

	public static boolean globalSuccess = true;
	public boolean localSuccess = true;

	public static void main(String[] args) {
		System.out.println();
		new ResolveTest1().test();
		new ResolveTest2().test();
		if (globalSuccess) {
			System.out.println("ALL decompileAtMethodResolve TESTS PASSED");
			System.exit(0);
		}
		System.out.println("THERE WERE TEST FAILURES");
		System.exit(1);
	}

	public void startTest(String name) {
		System.out.println("Starting test: " + name);
	}

	public void endTest(String name) {
		if (localSuccess) {
			System.out.println("Test passed: " + name);
		} else {
			System.out.println("Test failed: " + name);
		}
		System.out.println();
	}

	public void error(String msg) {
		System.out.println(msg);
		localSuccess = false;
		globalSuccess = false;
	}

	public void assertEquals(String name, int value, int expected) {
		if (value != expected) {
			error(name + " = " + value + " (expected " + expected + ")");
		}
	}

	public void assertEquals(String name, long value, long expected) {
		if (value != expected) {
			error(name + " = " + value + " (expected " + expected + ")");
		}
	}

	public void assertEquals(String name, float value, float expected) {
		if (value != expected) {
			error(name + " = " + value + " (expected " + expected + ")");
		}
	}

	public void assertEquals(String name, double value, double expected) {
		if (value != expected) {
			error(name + " = " + value + " (expected " + expected + ")");
		}
	}

}
