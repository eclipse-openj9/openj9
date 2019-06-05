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

import java.lang.constant.ClassDesc;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.VarHandle;
import java.lang.invoke.VarHandle.VarHandleDesc;
import java.util.Optional;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

/**
 * This test Java.lang.invoke.VarHandleDesc API added in Java 12 and later
 * version.
 *
 * new methods: 
 * - ofArray 
 * - ofField 
 * - ofStaticField 
 * - resolveConstantDesc 
 * - toString 
 * - varType: covered in ofArray / ofField / ofStaticField tests
 */
public class Test_VarHandleDesc {
	public static Logger logger = Logger.getLogger(Test_VarHandleDesc.class);

	private static VarHandle instanceTest;
	private static VarHandle staticTest;
	private static VarHandle arrayTest;
	private static VarHandle arrayTest2;
	private static ClassDesc declaringClassDesc;
	static {
		try {
			instanceTest = Jep334MHHelperImpl.getInstanceTest();
			staticTest = Jep334MHHelperImpl.getStaticTest();
			arrayTest = Jep334MHHelperImpl.getArrayTest();
			arrayTest2 = Jep334MHHelperImpl.getArrayTest2();
		} catch (Throwable e) {
			Assert.fail("Test variables could not be initialized for Test_VarHandleDesc.");
		}

		/* setup declaring class */
		Optional<ClassDesc> declaringClassDescOptional = Jep334MHHelperImpl.class.describeConstable();
		if (!declaringClassDescOptional.isPresent()) {
			Assert.fail("Error with tests ClassDesc for declaring class could not be generated for Test_VarHandleDesc.");
		}
		declaringClassDesc = declaringClassDescOptional.get();
	};

	/*
	 * Test Java 12 API VarHandleDesc.ofArray()
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleDescOfArray() {
		ofTestGeneral("testVarHandleDescOfArray (1)", Jep334MHHelperImpl.array_test, arrayTest);
		ofTestGeneral("testVarHandleDescOfArray (2)", Jep334MHHelperImpl.array_test, arrayTest2);
	}

	/*
	 * Test Java 12 API VarHandleDesc.ofField()
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleDescOfField() {
		ofTestGeneral("testVarHandleDescOfField", Jep334MHHelperImpl.instance_test, instanceTest);
	}

	/*
	 * Test Java 12 API VarHandleDesc.ofStaticField()
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleDescOfStaticField() {
		ofTestGeneral("testVarHandleDescOfStaticField", Jep334MHHelperImpl.static_test, staticTest);
	}

	/* generic test for ofStaticField, ofField and ofArray */
	private void ofTestGeneral(String testName, int test_type, VarHandle handle) {
		/* setup */
		Optional<ClassDesc> descOptional = null;
		if (test_type == Jep334MHHelperImpl.array_test) {
			descOptional = handle.varType().arrayType().describeConstable();
		} else {
			descOptional = handle.varType().describeConstable();
		}
		if (!descOptional.isPresent()) {
			Assert.fail(testName + ": error with tests ClassDesc could not be generated.");
			return;
		}
		ClassDesc desc = descOptional.get();

		/* call test method ofArray */
		VarHandleDesc varDesc = null;
		String originalDescriptor = null;
		switch (test_type) {
		case Jep334MHHelperImpl.array_test:
			varDesc = VarHandleDesc.ofArray(desc);
			originalDescriptor = desc.componentType().descriptorString();
			break;
		case Jep334MHHelperImpl.instance_test:
			varDesc = VarHandleDesc.ofField(declaringClassDesc, desc.displayName(), desc);
			originalDescriptor = desc.descriptorString();
			break;
		case Jep334MHHelperImpl.static_test:
			varDesc = VarHandleDesc.ofStaticField(declaringClassDesc, desc.displayName(), desc);
			originalDescriptor = desc.descriptorString();
			break;
		default:
			Assert.fail(testName + ": unsupported test type.");
		}

		/* verify that the array class is properly described */
		String newDescriptor = varDesc.varType().descriptorString();
		logger.debug(testName + ": Descriptor of original class is: " + originalDescriptor
				+ " descriptor of VarHandleDesc is: " + newDescriptor);
		Assert.assertTrue(originalDescriptor.equals(newDescriptor));
	}

	/*
	 * Test Java 12 API VarHandleDesc.toString() for static type
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleDescToStringStatic() {
		toStringTestGeneral("testVarHandleDescToStringStatic", Jep334MHHelperImpl.static_test, staticTest);
	}

	/*
	 * Test Java 12 API VarHandleDesc.toString() for instance type
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleDescToStringInstance() {
		toStringTestGeneral("testVarHandleDescToStringInstance", Jep334MHHelperImpl.instance_test, instanceTest);
	}

	/*
	 * Test Java 12 API VarHandleDesc.toString() for array type
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleDescToStringArray() {
		toStringTestGeneral("testVarHandleDescToStringArray (1)", Jep334MHHelperImpl.array_test, arrayTest);
		toStringTestGeneral("testVarHandleDescToStringArray (2)", Jep334MHHelperImpl.array_test, arrayTest2);
	}

	/* generic test for toString() */
	private void toStringTestGeneral(String testName, int test_type, VarHandle handle) {
		Optional<ClassDesc> descOptional = null;
		if (test_type == Jep334MHHelperImpl.array_test) {
			descOptional = handle.varType().arrayType().describeConstable();
		} else {
			descOptional = handle.varType().describeConstable();
		}
		if (!descOptional.isPresent()) {
			Assert.fail(testName + ": error with tests ClassDesc could not be generated.");
			return;
		}
		ClassDesc desc = descOptional.get();

		VarHandleDesc varDesc = null;
		String resultString = null;
		switch (test_type) {
		case Jep334MHHelperImpl.array_test:
			varDesc = VarHandleDesc.ofArray(desc);
			resultString = "VarHandleDesc[" + desc.displayName() + "[]]";
			break;
		case Jep334MHHelperImpl.instance_test:
			varDesc = VarHandleDesc.ofField(declaringClassDesc, desc.displayName(), desc);
			resultString = "VarHandleDesc[" + declaringClassDesc.displayName() + "." + varDesc.constantName() + ":"
					+ varDesc.varType().displayName() + "]";
			break;
		case Jep334MHHelperImpl.static_test:
			varDesc = VarHandleDesc.ofStaticField(declaringClassDesc, desc.displayName(), desc);
			resultString = "VarHandleDesc[static " + declaringClassDesc.displayName() + "." + varDesc.constantName()
					+ ":" + varDesc.varType().displayName() + "]";
			break;
		default:
			Assert.fail(testName + ": unsupported test type.");
		}

		String outputString = varDesc.toString();
		logger.debug(testName + ": toString output is: " + outputString + " and should be: " + resultString);
		Assert.assertTrue(outputString.equals(resultString));
	}

	/*
	 * Test Java 12 API VarHandleDesc.resolveConstantDesc()
	 */
	@Test(groups = { "level.sanity" })
	public void testVarHandleDescResolveConstantDesc() throws Throwable {
		resolveConstantDescTestGeneric("testVarHandleDescResolveConstantDesc: test array", arrayTest);
		resolveConstantDescTestGeneric("testVarHandleDescResolveConstantDesc: test array 2", arrayTest2); // fails
		resolveConstantDescTestGeneric("testVarHandleDescResolveConstantDesc: test instance", instanceTest); // fails
		resolveConstantDescTestGeneric("testVarHandleDescResolveConstantDesc: test static", staticTest);
	}

	private void resolveConstantDescTestGeneric(String testName, VarHandle handle) throws Throwable {
		VarHandleDesc handleDesc = handle.describeConstable().orElseThrow();
		VarHandle resolvedHandle = handleDesc.resolveConstantDesc(MethodHandles.lookup());

		logger.debug(testName + " is running.");
		Assert.assertTrue(handle.equals(resolvedHandle));
	}
}