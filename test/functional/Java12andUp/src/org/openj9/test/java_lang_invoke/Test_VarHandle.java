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
package org.openj9.test.java_lang_invoke;

import org.openj9.test.java_lang_invoke.helpers.Jep334MHHelperImpl;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import java.lang.invoke.MethodHandles;
import java.lang.invoke.VarHandle;
import java.lang.invoke.VarHandle.VarHandleDesc;
import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;

/**
 * This test Java.lang.invoke.VarHandle API added in Java 12 and later version.
 *
 * new methods: 
 * - describeConstable 
 * - equals 
 * - hashCode 
 * - toString
 */
public class Test_VarHandle {
	public static Logger logger = Logger.getLogger(Test_VarHandle.class);

	/* test VarHandles */
	private static VarHandle instanceTest;
	private static VarHandle staticTest;
	private static VarHandle arrayTest;
	private static VarHandle arrayTest2;
	/* hash tests */
	private static VarHandle instanceTestCopy;
	private static VarHandle staticTestCopy;
	private static VarHandle arrayTestCopy;
	private static VarHandle arrayTest2Copy;
	static {
		try {
			instanceTest = Jep334MHHelperImpl.getInstanceTest();
			staticTest = Jep334MHHelperImpl.getStaticTest();
			arrayTest = Jep334MHHelperImpl.getArrayTest();
			arrayTest2 = Jep334MHHelperImpl.getArrayTest2();
			/* used for hash tests */
			instanceTestCopy = Jep334MHHelperImpl.getInstanceTest();
			staticTestCopy = Jep334MHHelperImpl.getStaticTest();
			arrayTestCopy = Jep334MHHelperImpl.getArrayTest();
			arrayTest2Copy = Jep334MHHelperImpl.getArrayTest2();
		} catch (Throwable e) {
			Assert.fail("Test variables could not be initialized for Test_VarHandle.");
		}
	};

	/*
	 * Test Java 12 API VarHandle.describeConstable() for instance field type
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleDescribeConstableInstanceField() throws Throwable {
		describeConstableTestGeneral("testVarHandleDescribeConstableInstanceField", instanceTest);
	}

	/*
	 * Test Java 12 API VarHandle.describeConstable() for static field type
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleDescribeConstableStaticField() throws Throwable {
		describeConstableTestGeneral("testVarHandleDescribeConstableStaticField", staticTest);
	}

	/*
	 * Test Java 12 API VarHandle.describeConstable() for array type
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleDescribeConstableArray() throws Throwable {
		describeConstableTestGeneral("testVarHandleDescribeConstableArray (1)", arrayTest);
		describeConstableTestGeneral("testVarHandleDescribeConstableArray (2)", arrayTest2);
	}

	private void describeConstableTestGeneral(String testName, VarHandle handle) throws Throwable {
		VarHandleDesc desc = handle.describeConstable().orElseThrow();

		/*
		 * verify that descriptor can be resolved. Otherwise will throw
		 * ReflectiveOperationException
		 */
		VarHandle resolvedHandle = (VarHandle) desc.resolveConstantDesc(MethodHandles.lookup());

		logger.debug(testName + ": Descriptor of original class varType is: " + handle.varType().descriptorString());
		logger.debug(testName + ": Descriptor of VarHandleDesc varType is: " + resolvedHandle.varType().descriptorString());
		Assert.assertTrue(handle.equals(resolvedHandle));
	}

	/*
	 * Test Java 12 API VarHandle.equals()
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleEquals() {
		testVarHandleEqualsSub("testVarHandleEquals instance test: ", instanceTest, arrayTest2);
		testVarHandleEqualsSub("testVarHandleEquals static test: ", staticTest, arrayTest2);
		testVarHandleEqualsSub("testVarHandleEquals array test: ", arrayTest, arrayTest2);
		testVarHandleEqualsSub("testVarHandleEquals array test: ", arrayTest2, instanceTest);
	}

	private void testVarHandleEqualsSub(String testName, VarHandle tester, VarHandle diffHandle) {
		logger.debug(testName + " the same VarHandle should be equal to itself: " + tester.equals(tester));
		Assert.assertTrue(tester.equals(tester));
		logger.debug(testName + " different VarHandle instances should not be equal: " + !tester.equals(diffHandle));
		Assert.assertTrue(!tester.equals(diffHandle));
	}

	/*
	 * Test Java 12 API VarHandle.hashCode()
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleHashCode() {
		testVarHandleHashCodeSub("testVarHandleHashCode instance test: ", instanceTest, instanceTestCopy);
		testVarHandleHashCodeSub("testVarHandleHashCode static test: ", staticTest, staticTestCopy);
		testVarHandleHashCodeSub("testVarHandleHashCode array test: ", arrayTest, arrayTestCopy);
		testVarHandleHashCodeSub("testVarHandleHashCode array test 2: ", arrayTest2, arrayTest2Copy);
	}

	private void testVarHandleHashCodeSub(String testName, VarHandle tester, VarHandle copy) {
		/*
		 * The hashcode is stored for each instance once the method is called. Use a
		 * replica of the handle for tests so its computed twice
		 */
		int testerHash = tester.hashCode();
		int copyHash = copy.hashCode();
		logger.debug(testName + " The same VarHandle should produce the same hash. " + (testerHash == copyHash));
		Assert.assertTrue(testerHash == copyHash);
	}

	/*
	 * Test Java 12 API VarHandle.toString()
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleToString() {
		testHandleString("testVarHandleToString (instance)", instanceTest);
		testHandleString("testVarHandleToString (static)", staticTest);
		testHandleString("testVarHandleToString (array)", arrayTest);
		testHandleString("testVarHandleToString (array 2)", arrayTest2);
	}

	private void testHandleString(String testName, VarHandle handle) {
		String handleString = handle.toString();

		/* the general format is: VarHandle[varType=, coord=[]] */
		Assert.assertTrue(handleString.startsWith("VarHandle["));
		Assert.assertTrue(handleString.contains("varType=" + handle.varType().getName()));

		List<Class<?>> coordList = handle.coordinateTypes();
		String coordString = coordList.stream().map(coord -> String.valueOf(coord)).collect(Collectors.joining(", "));

		logger.debug(testName + ": var handle string is " + handleString + " string should include coords of: " + coordString);
		Assert.assertTrue(handleString.contains("coord=[" + coordString + "]"));
	}
}