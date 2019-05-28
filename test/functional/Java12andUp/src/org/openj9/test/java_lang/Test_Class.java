/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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
package org.openj9.test.java_lang;

import java.lang.constant.ClassDesc;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.util.Optional;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

/**
 * This test Java.lang.Class API added in Java 12 and later version.
 *
 * new methods: 
 * - describeConstable() 
 * - descriptorString() 
 * - arrayType() 
 * - componentType(): wrapper for Class.getComponentType(), not tested
 */
public class Test_Class {
	public static Logger logger = Logger.getLogger(Test_Class.class);

	Object[][] primitiveTest = { 
		{ boolean.class, "Z" }, 
		{ byte.class, "B" }, 
		{ char.class, "C" }, 
		{ short.class, "S" },
		{ int.class, "I" }, 
		{ float.class, "F" }, 
		{ double.class, "D" }, 
		{ long.class, "J" }, 
		{ void.class, "V" } 
	};

	/* descriptor strings */
	String objectResult = "Ljava/lang/Object;";
	String arrayResult = "[D";
	String arrayResult2 = "[[[D";

	/* test classes */
	Class<?> objectTest = new Object().getClass();
	Class<?> arrayTest = new double[0].getClass();
	Class<?> arrayTest2 = new double[0][][].getClass();
	Runnable lambdaExp = () -> System.out.println("test");
	Class<?> lambdaExpTest = lambdaExp.getClass();

	/*
	 * Test Java 12 API Class.descriptorString()
	 */
	@Test(groups = { "level.sanity" })
	public void testClassDescriptorString() {
		/* primitive type test */
		for (Object[] prim : primitiveTest) {
			logger.debug("testClassDescriptorString: Primitive test result is: " + ((Class<?>)prim[0]).descriptorString() + " expected: " + prim[1]);
			Assert.assertTrue(((Class<?>)prim[0]).descriptorString().equals(prim[1]));
		}

		/* ObjectType test */
		logger.debug("testClassDescriptorString: Object test result is: " + objectTest.descriptorString() + " expected: " + objectResult);
		Assert.assertTrue(objectTest.descriptorString().equals(objectResult));

		/* ArrayType test */
		logger.debug("testClassDescriptorString: Array test result is: " + arrayTest.descriptorString() + " expected: " + arrayResult);
		Assert.assertTrue(arrayTest.descriptorString().equals(arrayResult));

		/* ArrayType test 2 */
		logger.debug("testClassDescriptorString: Array test 2 result is: " + arrayTest2.descriptorString() + " expected: " + arrayResult2);
		Assert.assertTrue(arrayTest2.descriptorString().equals(arrayResult2));

		/*
		 * Lambda expression test. DescriptorString value will be different in each run.
		 * Verify that no error is thrown.
		 */
		logger.debug("testClassDescriptorString: Lambda test result is: " + lambdaExpTest.descriptorString());
	}

	/*
	 * Test Java 12 API Class.describeConstable()
	 */
	@Test(groups = { "level.sanity" })
	public void testClassDescribeConstable() throws Throwable {
		for (Object[] prim : primitiveTest) {
			describeConstableTestGeneral("testClassDescribeConstable (primitive " + prim[1] + ")", (Class<?>)prim[0]);
		}
		describeConstableTestGeneral("testClassDescribeConstable (object)", objectTest);
		describeConstableTestGeneral("testClassDescribeConstable (array)", arrayTest);
		describeConstableTestGeneral("testClassDescribeConstable (array 2)", arrayTest2);
		describeConstableTestLambda("testClassDescribeConstable (lambda expression)");
	}

	private void describeConstableTestGeneral(String testName, Class<?> testClass) throws Throwable {
		Optional<ClassDesc> optionalDesc = testClass.describeConstable();
		ClassDesc desc = optionalDesc.orElseThrow();

		/*
		 * verify that descriptor can be resolved. Otherwise exception will be thrown.
		 */
		Class<?> resolvedClass = (Class<?>)desc.resolveConstantDesc(MethodHandles.lookup());

		String originalDescriptor = testClass.descriptorString();
		String newDescriptor = resolvedClass.descriptorString();
		logger.debug(testName + ": Descriptor of original class is: " + originalDescriptor + " descriptor of ClassDesc is: " + newDescriptor);
		Assert.assertTrue(testClass.equals(resolvedClass));
	}

	/*
	 * separate test case for lambda expression because resolveConstantDesc is
	 * expected to fail.
	 */
	private void describeConstableTestLambda(String testName) {
		Optional<ClassDesc> optionalDesc = lambdaExpTest.describeConstable();
		ClassDesc desc = optionalDesc.orElseThrow();

		/*
		 * verify that descriptor can be resolved. Otherwise exception will be thrown.
		 */
		try {
			Class<?> resolvedClass = (Class<?>)desc.resolveConstantDesc(MethodHandles.lookup());
			Assert.fail(testName + " : resolveConstantDesc did not throw an error.");
		} catch (Throwable e) {
			/* resolveConstantDesc is expected to fail */
			logger.debug(testName + ": exception thrown for resolveConstantDesc was " + e.toString());
		}
	}

	/*
	 * Test Java 12 API Class.arrayType()
	 */
	@Test(groups = { "level.sanity" })
	public void testClassArrayType() throws Throwable {
		for (Object[] prim : primitiveTest) {
			if ((Class<?>)prim[0] == void.class) {
				/* test is expected to throw IllegalArgumentException */
				try {
					arrayTypeTestGeneral("testClassArrayType (primitive) this test should throw IllegalArgumentException", (Class<?>)prim[0], (String)prim[1]);
					Assert.fail();
				} catch(IllegalArgumentException e) {
					/* test passed */
				}
			} else {				
				arrayTypeTestGeneral("testClassArrayType (primitive)", (Class<?>)prim[0], (String)prim[1]);
			}
		}
		arrayTypeTestGeneral("testClassArrayType (object)", objectTest, objectResult);
		arrayTypeTestGeneral("testClassArrayType (embedded array)", arrayTest, arrayResult);
	}

	private void arrayTypeTestGeneral(String testName, Class<?> testClass, String result) throws Throwable {
		Class<?> array = testClass.arrayType();
		String arrayString = array.descriptorString();
		logger.debug(testName + ": Descriptor of component is: " + result + " descriptor of array is: " + arrayString);
		Assert.assertTrue(arrayString.equals("[" + result));
	}
}