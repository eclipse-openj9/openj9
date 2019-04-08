/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package com.ibm.jvmti.tests.redefineClasses;

import com.ibm.jvmti.tests.util.Util;

/*
 * The purpose of this test is to verify that the resolved state for invokedynamic is propagated
 * to equivalent methods in a redefined class.
 * 
 * Every time an invokedynamic instruction is called with a bootstrap method in the IndyBSMs class, 
 * a counter will be incremented. If after invokedynamic calls the counter value is greater than expected,
 * the invokedynamic bytecodes are not being propagated.
 */
public class rc020 {
	private static byte[] resetClassBytes;

	public static native boolean redefineClass(Class name, int classBytesSize, byte[] classBytes);

	public boolean testIndyRedefinition() throws Throwable {
		resetClassBytes = new rc020_testIndyAsmGenerator().generateExistingIndyClass();

		/* Redefine class with one indy call. */
		if (!runTest("testIndyRedefinition (basic test)", new rc020_testIndyAsmGenerator().generateBasicIndyClass(), 1, 0)) {
			return false;
		}

		/* Redefine class with one indy call where bsm has an argument. */
		if (!runTest("testIndyRedefinition (basic test, bsm args)", new rc020_testIndyAsmGenerator().generateBasicIndyClassWithArgs(), 1, 0)) {
			return false;
		}

		/* Redefine class with one indy call where bsm has two additional arguments. */
		if (!runTest("testIndyRedefinition (basic test, bsm two args)", new rc020_testIndyAsmGenerator().generateBasicIndyClassWithTwoArgs(), 1, 0)) {
			return false;
		}

		/* Redefine class with one indy call where bsm has an additional double slot argument. */
		if (!runTest("testIndyRedefinition (basic test, bsm double slot args)", new rc020_testIndyAsmGenerator().generateBasicIndyClassWithDoubleSlotArgs(), 1, 0)) {
			return false;
		}

		/* Redefine class with one indy call where bsm has three args with a double slot in the middle */
		if (!runTest("testIndyRedefinition (basic test, bsm double slot with three args)", new rc020_testIndyAsmGenerator().generateBasicIndyClassWithDoubleSlotWithThreeArgs(), 1, 0)) {
			return false;
		}

		/* Redefine class with two indy calls referring to the same BSMs. */
		if (!runTest("testIndyRedefinition (double indy test, same bsms)", new rc020_testIndyAsmGenerator().generateTwoSameIndyCallClass(), 2, 0)) {
			return false;
		}

		/* Redefine class with two indy calls referring to different BSMs. */
		if (!runTest("testIndyRedefinition (double indy test, diff bsms)", new rc020_testIndyAsmGenerator().generateTwoDiffIndyCallClass(), 2, 0)) {
			return false;
		}

		/* Redefine class with two indy calls referring to different BSMs with arguments. */
		if (!runTest("testIndyRedefinition (double indy test, diff bsms with args)", new rc020_testIndyAsmGenerator().generateTwoDiffIndyCallClassWithArgs(), 2, 0)) {
			return false;
		}

		/* Redefine class with three indy calls where two are referring to the same BSM. */
		if (!runTest("testIndyRedefinition (triple indy test)", new rc020_testIndyAsmGenerator().generateThreeIndyCallClass(), 3, 0)) {
			return false;
		}

		/* Redefine class with more than one method with an indy call. */
		if (!runTest("testIndyRedefinition (multi class test)", new rc020_testIndyAsmGenerator().generateMultiMethodClass(), 1, 0)) {
		 	return false;
		}

		/*
		 * Tests where redefined class changes and callsites should not copied.
		 */
		/* Change NAS of bootstrap method, callsite should not be copied. */
		if (!runTest("testIndyRedefinition (basic test, change of NAS, callsite should not be copied)", 
			new rc020_testIndyAsmGenerator().generateBasicIndyClass(), 
			new rc020_testIndyAsmGenerator().generateBasicIndyClassWithArgs(), 1, 1)) {
			return false;
		}

		/* Redefine class with one indy call where bsm has three args with a double slot in the middle. If the static arguments
		 * to the BSM change, the output will also change and BSM should be reinvoked. Verify that values are checked correctly.
		 */
		if (!runTest("testIndyRedefinition (basic test, bsm double slot with three args parameter change, callsite should not be copied)", 
			new rc020_testIndyAsmGenerator().generateBasicIndyClassWithDoubleSlotWithThreeArgs(), 
			new rc020_testIndyAsmGenerator().generateBasicIndyClassWithDoubleSlotWithThreeArgs2(), 1, 1)) {
			return false;
		}

		return true; /* success */
	}

	private boolean runTest(String testName, byte[] classBytes, int expectedOriginalBSMCount, int expectedBSMCountAfterRedefine) {
		return runTest(testName, classBytes, classBytes, expectedOriginalBSMCount, expectedBSMCountAfterRedefine);
	}

	private boolean runTest(String testName, byte[] classBytesO, byte[] classBytesR, int expectedOriginalBSMCount, int expectedBSMCountAfterRedefine) {
		/* define original class */	
		boolean redefineWithOriginal = redefineClass(rc020_testIndyAsmGenerator.testClass, classBytesO.length, classBytesO);
		if (! redefineWithOriginal) {
			System.out.println(testName + ": Class " + rc020_testIndyAsmGenerator.testClassName + " could not be redefined to original.");
			return false; /* fail */
		}

		/* call test method that runs invokedynamic. This should invoke the BSM to resolve the callsite */
		rc020_Source.testCall();

		/* verify that BSM was invoked */
		if (expectedOriginalBSMCount != rc020_testIndyBSMs.bsmCallCount()) {
			System.out.println(testName + ": BSM call count was expected to be " + expectedOriginalBSMCount 
				+ " after original indy call, count is actually " + rc020_testIndyBSMs.bsmCallCount());
			return false; /* fail */
		}

		/* redefine class from original to new */
		boolean redefineWithNew = redefineClass(rc020_testIndyAsmGenerator.testClass, classBytesR.length, classBytesR);
		if (! redefineWithNew) {
			System.out.println(testName + ": Class " + rc020_testIndyAsmGenerator.testClassName + " could not be redefined to new class.");
			return false; /* fail */
		}

		/* call redefined test method and verify that callsite information was propagated to redefinition. */
		rc020_Source.testCall();

		/* verify that BSM count is correct */
		int expectedTotalBSMCount = expectedOriginalBSMCount + expectedBSMCountAfterRedefine;
		if (expectedTotalBSMCount != rc020_testIndyBSMs.bsmCallCount()) {
			System.out.println(testName + ": BSM call count was expected to be " + expectedTotalBSMCount 
				+ " after redefined indy call, actually " + rc020_testIndyBSMs.bsmCallCount());
			return false; /* fail */
		}

		/* reset for next test */
		return resetTest(testName);
	}

	private boolean resetTest(String testName) {
		/* reset BSM count to zero */
		rc020_testIndyBSMs.resetBsmCallCount();

		/* reset test class to have no indy calls, resolved callsites will be reset */
		boolean resetResult = redefineClass(rc020_testIndyAsmGenerator.testClass, resetClassBytes.length, resetClassBytes);
		if (! resetResult) {
			System.out.println(testName + ": reset redefine failed.");
			return false; /* fail */
		}
		return true;
	}

	public String helpIndyRedefinition() {
		return "Tests if indy MethodHandle resolution is propagated over redefined classes.";
	}
}
