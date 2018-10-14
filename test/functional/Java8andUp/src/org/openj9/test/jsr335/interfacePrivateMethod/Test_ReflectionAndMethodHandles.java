/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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
package org.openj9.test.jsr335.interfacePrivateMethod;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Set;
import java.util.HashSet;
import java.lang.invoke.MethodHandles.Lookup;

import org.openj9.test.util.VersionCheck;

/**
 * Test Reflection and MethodHandle lookup for private methods in interfaces.
 */
@Test(groups = { "level.sanity" })
public class Test_ReflectionAndMethodHandles {
	final static MethodType type = MethodType.methodType(String.class);
	
	final static LoadInterfaceContainingPrivateMethod loadClass = new LoadInterfaceContainingPrivateMethod();
	
	/* Object instance is an instance of the class ImplClassToTestReflectionAndMethodHandles which implements 
	 * the interface ITestChild 
	 */
	static Object instance; 
	static {
		try {
			instance = Class.forName("ImplClassToTestReflectionAndMethodHandles", false, loadClass).newInstance();
		} catch (InstantiationException | IllegalAccessException | ClassNotFoundException e) {
			throw new RuntimeException(e);
		}
	}
	
	private static Lookup getLookupObject(Class<?> c) throws Exception {
		Method method = c.getDeclaredMethod("getLookup");
		method.setAccessible(true);
		return (Lookup)method.invoke(null);
	}
	
	/* Expected outputs for various method invocations being tested later during the test */
	private final static String expected_child_public_non_static = "In public non static method of child interface";
	private final static String expected_child_public_static = "In public static method of child interface";
	private final static String expected_child_private_non_static = "In private non static method of child interface";
	private final static String expected_child_private_static = "In private static method of child interface";
	private final static String expected_ITest_private_static = "private_static_method";
	private final static String expected_ITest_private_non_static = "private_non_static_method";


	private static Set<String> expectedGetMethodsSet() {
		Set<String> expectedGetMethodsSet = new HashSet<String>();
		expectedGetMethodsSet.add("public static java.lang.invoke.MethodHandles$Lookup ITestChild.getLookup()");
		expectedGetMethodsSet.add("public default java.lang.String ITestChild.child_public_non_static_method()");
		expectedGetMethodsSet.add("public static java.lang.String ITestChild.child_public_static_method()");
		expectedGetMethodsSet.add("public default java.lang.String ITest.RunTest_private_non_static_method_with_InvokeStatic()");
		expectedGetMethodsSet.add("public default java.lang.String ITest.RunTest_private_non_static_method_with_InvokeVirtual(ITest)");
		expectedGetMethodsSet.add("public default java.lang.String ITest.RunTest_private_non_static_method_with_InvokeSpecial(ITest)");
		return expectedGetMethodsSet;
	}
	
	/**
	 * testFindMethodsUsingGetMethods verifies the proper operation of getMethods() by comparing the
	 * methods returned by calling it on interface ITestChild with the expected good values already stored in 
	 * the expectedGetMethodsSet. These values were found by calling getMethods() with Oracle JDK and are 
	 * used as a base for verification.
	 */
	@Test
	public void testFindMethodsUsingGetMethods() throws Exception {
		Set<String> expectedGetMethodsSet = expectedGetMethodsSet();	
		Class<?> c = Class.forName("ITestChild", true, loadClass);  
		
		Method[] myMethods = c.getMethods();
		AssertJUnit.assertTrue("Failing as Class.getMethods found '" + myMethods.length 
				+ "' methods, but the expected number of methods was '" + expectedGetMethodsSet.size() + "'", 
				myMethods.length == expectedGetMethodsSet.size());
		
		for (Method method : myMethods) {
			String foundMethodName = method.toGenericString();
			boolean foundMethod = expectedGetMethodsSet.remove(foundMethodName);
			if (!foundMethod) {
				Assert.fail("Failing as getMethods() returned unexpected method: " + foundMethodName);
			}
		}
		
		/* If the set isn't empty, we found the right number of methods but with different attributes.
		 * Print the results of getMethods and the left over values in expectedGetMethodsSet
		 */
		if (!expectedGetMethodsSet.isEmpty()){
			printFailResults("getMethods", expectedGetMethodsSet, myMethods);
			Assert.fail("Failing as getMethods() did not return all expected methods");
		}
	}
	
	static void printFailResults(String apiMethodName, Set<String> expectedResults, Method[] actualResults) {
		if (!expectedResults.isEmpty()){
			System.out.println("==========  Expected results for " + apiMethodName +" still contains: ==========");
			for (String s : expectedResults) {
				System.out.println(s);
			}
			System.out.println("\n==========  Full actual results for " + apiMethodName + ": ==========");
			for (Method method : actualResults) {
				System.out.println(method.toGenericString());
			}
		}
	}
	
	static Set<String> expectedGetDeclaredMethodsSet() {
		Set<String> expectedGetDeclaredMethodsSet = new HashSet<String>();
		expectedGetDeclaredMethodsSet.add("public static java.lang.invoke.MethodHandles$Lookup ITestChild.getLookup()");
		expectedGetDeclaredMethodsSet.add("private java.lang.String ITestChild.child_private_non_static_method()");
		expectedGetDeclaredMethodsSet.add("public default java.lang.String ITestChild.child_public_non_static_method()");
		expectedGetDeclaredMethodsSet.add("private static java.lang.String ITestChild.child_private_static_method()");
		expectedGetDeclaredMethodsSet.add("public static java.lang.String ITestChild.child_public_static_method()");
		return expectedGetDeclaredMethodsSet;
	}
	
	/**
	 * testFindMethodsUsingGetDeclaredMethods verifies the proper operation of getDeclaredMethods() by comparing the
	 * methods returned by calling it on interface ITestChild with the expected good values already stored in 
	 * the expectedGetDeclaredMethodsSet. These values were found by calling getDeclaredMethods() with Oracle JDK and are 
	 * used as a base for verification.
	 */
	@Test
	public void testFindMethodsUsingGetDeclaredMethods() throws Exception {
		Set<String> expectedGetDeclaredMethodsSet = expectedGetDeclaredMethodsSet();
		
		Class<?> c = Class.forName("ITestChild", true, loadClass); 
		Method[] myDeclaredMethods = c.getDeclaredMethods(); 
		AssertJUnit.assertTrue("Failing as Class.getDeclaredMethods found '" + myDeclaredMethods.length + "'" 
				+ " methods, but the expected number of methods was '" + expectedGetDeclaredMethodsSet.size() + "'",
				myDeclaredMethods.length == expectedGetDeclaredMethodsSet.size());
		
		for (Method method : myDeclaredMethods) {
			String foundMethodName = method.toGenericString();
			boolean foundMethod = expectedGetDeclaredMethodsSet.remove(foundMethodName);
			if (!foundMethod) {
				Assert.fail("Failing as getDeclaredMethods() returned unexpected method: " + foundMethodName);
			}
		}
		
		if (!expectedGetDeclaredMethodsSet.isEmpty()) {
			printFailResults("getDeclaredMethods", expectedGetDeclaredMethodsSet, myDeclaredMethods);
			Assert.fail("Failing as getDeclaredMethods() did not return all expected methods");
		}
	}
	
	/* --------------- Tests to invoke methods contained in child interface ITestChild ---------------------- */
	
	/**
	 * This method is used to test if a public non static method in our ITestChild interface can be invoked
	 */
	@Test
	public static void test_child_public_non_static_method() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Method method = c.getDeclaredMethod("child_public_non_static_method");
		method.setAccessible(true);
		Object returnStatement = method.invoke(instance);
		AssertJUnit.assertEquals("Failing as public non static method of ITestChild interface didnt return expected output", expected_child_public_non_static, returnStatement);
	}

	/**
	 * This method is used to test if a private non static method in our ITestChild interface can be invoked
	 */
	@Test
	public static void test_child_private_non_static_method() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Method method = c.getDeclaredMethod("child_private_non_static_method");
		method.setAccessible(true);
		Object returnStatement = method.invoke(instance);
		AssertJUnit.assertEquals("Failing as private non static method of ITestChild interface didnt return expected output", expected_child_private_non_static, returnStatement);
	}

	/**
	 * This method is used to test if a private static method in our ITestChild interface can be invoked
	 */
	@Test
	public static void test_child_private_static_method() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Method method = c.getDeclaredMethod("child_private_static_method");
		method.setAccessible(true);
		Object returnStatement = method.invoke(instance);
		AssertJUnit.assertEquals("Failing as private static method of ITestChild interface didnt return expected output", expected_child_private_static, returnStatement);
	}

	/**
	 * This method is used to test if a public static method in our ITestChild interface can be invoked
	 */
	@Test
	public static void test_child_public_static_method() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Method method = c.getDeclaredMethod("child_public_static_method");
		method.setAccessible(true);
		Object returnStatement = method.invoke(instance);
		AssertJUnit.assertEquals("Failing as private static method of ITestChild interface didnt return expected output", expected_child_public_static, returnStatement);
	}

	/* --------------- Tests  to invoke methods contained in parent interface ITest ---------------------- */

	/**
	 * This method is used to test if a private static method in our parent interface ITest can be invoked
	 */
	@Test
	public static void test_ITest_private_static_method() throws Exception {
		Class<?> c = Class.forName("ITest", true, loadClass);
		Method method = c.getDeclaredMethod("private_static_method");
		method.setAccessible(true);
		Object returnStatement = method.invoke(instance);
		AssertJUnit.assertEquals("Failing as private static method of parent interface ITest didnt return expected output", expected_ITest_private_static, returnStatement);
	}

	/**
	 * This method is used to test if a private non static method in our parent interface ITest can be invoked
	 */
	@Test
	public static void test_ITest_private_non_static_method() throws Exception {
		Class<?> c = Class.forName("ITest", true, loadClass);
		Method method = c.getDeclaredMethod("private_non_static_method");
		method.setAccessible(true);
		Object returnStatement = method.invoke(instance);
		AssertJUnit.assertEquals("Failing as private non static method of parent interface ITest didnt return expected output", expected_ITest_private_non_static, returnStatement);
	}

	/**
	 * This method is used to test if a public static method in our parent interface ITest can be invoked
	 */
	@Test
	public static void test_ITest_public_static_method() throws Exception {
		Class<?> c = Class.forName("ITest", true, loadClass);
		Method method = c.getDeclaredMethod("RunTest_private_static_method_with_InvokeStatic");
		method.setAccessible(true);
		Object returnStatement = method.invoke(instance);
		AssertJUnit.assertEquals("Failing as public static method of parent interface ITest didnt return expected output", expected_ITest_private_static, returnStatement);
	}

	/**
	 * This method is used to test if a public non static method in our parent interface ITest can be invoked
	 */
	@Test
	public static void test_ITest_public_non_static_method() throws Exception {
		Class<?> c = Class.forName("ITest", true, loadClass);
		Method method = c.getDeclaredMethod("RunTest_private_non_static_method_with_InvokeSpecial", c);
		method.setAccessible(true);
		Object returnStatement = method.invoke(instance, instance);
		AssertJUnit.assertEquals("Failing as public non static method of parent interface ITest didnt return expected output", expected_ITest_private_non_static, returnStatement);
	}
		
	/* --------------- Tests to test findStatic ---------------------- */
	
	/**
	 * This method is used to verify if the method handle for a private static method 
	 * in our child interface can be successfully looked up using findStatic, and then 
	 * successfully invoke the method to verify that correct MethodHandle was found 
	 */
	@Test
	public static void test_find_static_child_private_static() throws Throwable {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		MethodHandle mh = lookup.findStatic(c, "child_private_static_method", type);
		String returnStatement = (String)mh.invoke();
		AssertJUnit.assertEquals("Failing as private static method of child interface didnt return expected output when invoked", expected_child_private_static, returnStatement);
	}
	
	/**
	 * This method is used to verify if the method handle of a public static method
	 * in our child interface can be successfully looked up using findStatic, and then
	 * successfully invoke the method to verify that the correct MethodHandle was found
	 */
	@Test
	public static void test_find_static_child_public_static() throws Throwable {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		MethodHandle mh = lookup.findStatic(c, "child_public_static_method", type);
		String returnStatement = (String)mh.invoke();
		AssertJUnit.assertEquals("Failing as public static method of child interface didnt return expected output when invoked", expected_child_public_static, returnStatement);
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the public non static
	 * method cannot be successfully looked up using findStatic. An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_static_child_public_non_static() throws Throwable {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findStatic(c, "child_public_non_static_method", type);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the private non static
	 * method cannot be successfully looked up using findStatic. An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_static_child_private_non_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findStatic(c, "child_private_non_static_method", type);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the private static
	 * method from the parent interface ITest cannot be successfully looked up using findStatic. 
	 * A NoSuchMethodException is expected to be thrown 
	 * @throws NoSuchMethodException
	 */
	@Test
	public static void test_find_static_ancestor_private_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findStatic(c, "private_static_method", type);
			Assert.fail("Should have thrown a NoSuchMethodException");
		} catch (NoSuchMethodException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the public static
	 * method from the parent interface ITest cannot be successfully looked up using findStatic. 
	 * A NoSuchMethodException is expected to be thrown 
	 * @throws NoSuchMethodException
	 */
	@Test
	public static void test_find_static_ancestor_public_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findStatic(c, "RunTest_private_static_method_with_InvokeStatic", type);
			Assert.fail("Should have thrown a NoSuchMethodException");
		} catch (NoSuchMethodException e) {
			/* Pass */
		}
	}
	
	/* --------------- Tests to test findVirtual ---------------------- */
	
	/**
	 * This method is used to verify if the method handle for a public non static method 
	 * in our child interface can be successfully looked up using findVirtual, and then 
	 * successfully invoke the method to verify that the correct MethodHandle was found
	 */
	@Test
	public static void test_find_virtual_child_public_non_static() throws Throwable {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		MethodHandle mh = lookup.findVirtual(c, "child_public_non_static_method", type);
		String returnStatement = (String) mh.invoke(instance);
		AssertJUnit.assertEquals("Failing as public non static method of child interface didnt return expected output when invoked", expected_child_public_non_static, returnStatement);
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the public static
	 * method from the child interface cannot be successfully looked up using findVirtual. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_virtual_child_public_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findVirtual(c, "child_public_static_method", type);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the private static
	 * method from the child interface cannot be successfully looked up using findVirtual. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_virtual_child_private_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findVirtual(c, "child_private_static_method", type);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the method handle for a private static method 
	 * in the interface ITest cannot be successfully looked up using findVirtual
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_findVirtual_interface_ITest_private_static() throws Exception {
		Class<?> d = Class.forName("ITest", true, loadClass);
		Lookup lookup = getLookupObject(d);
		MethodType type = MethodType.methodType(String.class);
		try {
			MethodHandle mh = lookup.findVirtual(d, "private_static_method", type);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the method handle for a private non static method 
	 * in the interface ITest cannot be successfully looked up using findVirtual
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_virtual_interface_ITest_private_non_static() throws Exception {
		Class<?> d = Class.forName("ITest", true, loadClass);
		Lookup lookup = getLookupObject(d);
		MethodType type = MethodType.methodType(String.class);
		try {
			MethodHandle mh = lookup.findVirtual(d, "private_non_static_method", type);
			if (VersionCheck.major() < 11) {
				Assert.fail("Should have thrown an IllegalAccessException");
			}
		} catch (IllegalAccessException e) {
			if (VersionCheck.major() >= 11) {
				Assert.fail("private access should be allowed");				
			}
			/* Pass */
		}
	}
	
	/**
	 * This method is a test to verify that the methodHandle for the public non static
	 * method from the parent interface ITest can be successfully looked up using findVirtual, and then a 
	 * negative test to verify that the method cannot be invoked. Here an IncompatibleClassChangeError is 
	 * expected to be thrown   
	 * @throws IncompatibleClassChangeError
	 */
	@Test
	public static void test_find_virtual_ancestor_public_non_static() throws Throwable {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		MethodHandle mh = lookup.findVirtual(c, "RunTest_private_non_static_method_with_InvokeStatic", type);
		try {	
			String returnStatement = (String) mh.invoke(instance);
			Assert.fail("Failing as expected IncompatibleClassChangeError to be thrown when invoking method "
					+ "RunTest_private_non_static_method_with_InvokeStatic");
		} catch (IncompatibleClassChangeError e) {
			/* Pass */
		}
	}
	
	/* --------------- Tests to test findSpecial ---------------------- */
	
	/**
	 * This method is a negative test to verify that the methodHandle for the private static
	 * method from the child interface cannot be successfully looked up using findSpecial. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_special_child_private_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findSpecial(c, "child_private_static_method", type, Test_ReflectionAndMethodHandles.class);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the private non static
	 * method from the child interface cannot be successfully looked up using findSpecial. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_special_child_private_non_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findSpecial(c, "child_private_non_static_method", type, Test_ReflectionAndMethodHandles.class);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the public static
	 * method from the child interface cannot be successfully looked up using findSpecial. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_special_child_public_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findSpecial(c, "child_public_static_method", type, Test_ReflectionAndMethodHandles.class);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the public non static
	 * method from the child interface cannot be successfully looked up using findSpecial. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_special_child_public_non_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findSpecial(c, "child_public_non_static_method", type, Test_ReflectionAndMethodHandles.class);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the private static
	 * method from the parent interface cannot be successfully looked up using findSpecial. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_special_ancestor_private_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findSpecial(c, "private_static_method", type, Test_ReflectionAndMethodHandles.class);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (Exception e) {
			AssertJUnit.assertTrue("Failing test as expected IllegalAccessException to get thrown, instead of " + e, e instanceof IllegalAccessException);
		}
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the private non static
	 * method from the parent interface cannot be successfully looked up using findSpecial. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_special_ancestor_private_non_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findSpecial(c, "private_non_static_method", type, Test_ReflectionAndMethodHandles.class);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the public static
	 * method from the parent interface cannot be successfully looked up using findSpecial. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_special_ancestor_public_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findSpecial(c, "RunTest_private_static_method_with_InvokeStatic", type, Test_ReflectionAndMethodHandles.class);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the public static
	 * method from the parent interface cannot be successfully looked up using findSpecial. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_find_special_ancestor_public_non_static() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findSpecial(c, "RunTest_private_non_static_method_with_InvokeStatic", type, Test_ReflectionAndMethodHandles.class);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the public clinit
	 * from the child interface cannot be successfully looked up using findSpecial. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	/* This behavior for clinit cannot be verified with Oracle as we crash with a EXCEPTION_ACCESS_VIOLATION 
	 * when running the test having clinit in the interface with Oracle 
	 */
	@Test
	public static void test_find_special_child_clinit() throws Exception {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.findSpecial(c, "<clinit>", type, Test_ReflectionAndMethodHandles.class);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/* --------------- Tests to test unreflect ---------------------- */
	
	/**
	 * This method is used to verify if the direct method handle to invoke the private static method 
	 * in our child interface can be successfully looked up using unreflect and then if it can be successfully invoked
	 */
	@Test
	public static void test_unreflect_child_private_static_method() throws Throwable {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Method method = c.getDeclaredMethod("child_private_static_method");
		method.setAccessible(true);
		Lookup lookup = getLookupObject(c);
		MethodHandle mh = lookup.unreflect(method);
		String returnStatement = (String)mh.invoke();
		AssertJUnit.assertEquals("Failing as private static method of ITestChild interface didnt return expected output", expected_child_private_static, returnStatement);
	}
	
	/**
	 * This method is used to verify if the direct method handle to invoke the public static method 
	 * in our child interface can be successfully looked up using unreflect, and then if it can be successfully invoked
	 */
	@Test
	public static void test_unreflect_child_public_static_method() throws Throwable {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Method method = c.getDeclaredMethod("child_public_static_method");
		method.setAccessible(true);
		Lookup lookup = getLookupObject(c);
		MethodHandle mh = lookup.unreflect(method);
		String returnStatement = (String)mh.invoke();
		AssertJUnit.assertEquals("Failing as public static method of ITestChild interface didnt return expected output", expected_child_public_static, returnStatement);
	}
	
	/**
	 * This method is used to verify if the direct method handle to invoke the public non static method 
	 * in our child interface can be successfully looked up using unreflect, and then if it can be successfully invoked 
	 */
	@Test
	public static void test_unreflect_child_public_non_static_method() throws Throwable {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Method method = c.getDeclaredMethod("child_public_non_static_method");
		method.setAccessible(true);
		Lookup lookup = getLookupObject(c);
		MethodHandle mh = lookup.unreflect(method);
		String returnStatement = (String)mh.invoke(instance);
		AssertJUnit.assertEquals("Failing as public non static method of ITestChild interface didnt return expected output", expected_child_public_non_static, returnStatement);
	}

	/**
	 * This method is used to verify if the direct method handle to invoke the private static method 
	 * in our interface ITest can be successfully looked up using unreflect, and then if it can be successfully invoked 
	 */
	@Test
	public static void test_unreflect_interface_ITest_private_static_method() throws Throwable {
		Class<?> d = Class.forName("ITest", true, loadClass);
		Method method = d.getDeclaredMethod("private_static_method");
		method.setAccessible(true);
		Lookup lookup = getLookupObject(d);
		MethodHandle mh = lookup.unreflect(method);
		String returnStatement = (String)mh.invoke();
		AssertJUnit.assertEquals("Failing as private static method of interface ITest didnt return expected output", expected_ITest_private_static, returnStatement);
	}
	
	@Test
	public static void test_unreflect_interface_ITest_private_non_static_method() throws Throwable {
		Class<?> d = Class.forName("ITest", true, loadClass);
		Method method = d.getDeclaredMethod("private_non_static_method");
		method.setAccessible(true);
		Lookup lookup = getLookupObject(d);
		MethodHandle mh = lookup.unreflect(method);
		try {
			String returnStatement = (String) mh.invoke(instance);
			if (VersionCheck.major() < 9) {
				Assert.fail("Should have thrown AbstractMethodError!!");
			}
		} catch (AbstractMethodError e) {
			if (VersionCheck.major() >= 9) {
				Assert.fail("private access should be allowed");				
			}
			/* Pass */
		} 
	}
	
	/* --------------- Tests to test unreflectSpecial ---------------------- */

	/**
	 * This method is a negative test to verify that the methodHandle for the private static method
	 * from the child interface cannot be successfully produced using unreflectSpecial. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_unreflectSpecial_child_private_static_method() throws Throwable {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Method method = c.getDeclaredMethod("child_private_static_method");
		method.setAccessible(true);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.unreflectSpecial(method, c);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is used to verify if the method handle for a private non static method 
	 * in our child interface can be successfully produced using unreflectSpecial, and then 
	 * successfully invoke the method to verify that the correct MethodHandle was found
	 */
	@Test
	public static void test_unreflectSpecial_child_private_non_static_method() throws Throwable {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Method method = c.getDeclaredMethod("child_private_non_static_method");
		method.setAccessible(true);
		Lookup lookup = getLookupObject(c);
		MethodHandle mh = lookup.unreflectSpecial(method, c);
		String returnStatement = (String)mh.invoke(instance);
		AssertJUnit.assertEquals("Failing as private non static method of ITestChild interface didnt return expected output", expected_child_private_non_static, returnStatement);
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the public static method
	 * from the child interface cannot be successfully produced using unreflectSpecial. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_unreflectSpecial_child_public_static_method() throws Throwable {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Method method = c.getDeclaredMethod("child_public_static_method");
		method.setAccessible(true);
		Lookup lookup = getLookupObject(c);
		try {
			MethodHandle mh = lookup.unreflectSpecial(method, c);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
	
	/**
	 * This method is used to verify if the method handle for a public non static method 
	 * in our child interface can be successfully produced using unreflectSpecial, and then 
	 * successfully invoke the method to verify that the correct MethodHandle was found
	 */
	@Test
	public static void test_unreflectSpecial_child_public_non_static_method() throws Throwable {
		Class<?> c = Class.forName("ITestChild", true, loadClass);
		Method method = c.getDeclaredMethod("child_public_non_static_method");
		method.setAccessible(true);
		Lookup lookup = getLookupObject(c);
		MethodHandle mh = lookup.unreflectSpecial(method, c);
		String returnStatement = (String)mh.invoke(instance);
		AssertJUnit.assertEquals("Failing as public non static method of ITestChild interface didnt return expected output", expected_child_public_non_static, returnStatement);
	}

	/**
	 * This method is a negative test to verify that the methodHandle for the private non static method
	 * from the interface ITest can be successfully produced using unreflectSpecial and then 
	 * successfully invoke the method to verify that the correct MethodHandle was found
	 */
	@Test
	public static void test_unreflectSpecial_interface_ITest_private_non_static_method() throws Throwable {
		Class<?> d = Class.forName("ITest", true, loadClass);
		Method method = d.getDeclaredMethod("private_non_static_method");
		method.setAccessible(true);
		Lookup lookup = getLookupObject(d);
		MethodHandle mh = lookup.unreflectSpecial(method, d);
		String returnStatement = (String)mh.invoke(instance);
		AssertJUnit.assertEquals("Failing as public non static method of ITestChild interface didnt return expected output", expected_ITest_private_non_static, returnStatement);
	}
	
	/**
	 * This method is a negative test to verify that the methodHandle for the private static method
	 * from the parent interface cannot be successfully produced using unreflectSpecial. 
	 * An IllegalAccessException is expected to be thrown 
	 * @throws IllegalAccessException
	 */
	@Test
	public static void test_unreflectSpecial_interface_ITest_private_static_method() throws Throwable {
		Class<?> d = Class.forName("ITest", true, loadClass);
		Method method = d.getDeclaredMethod("private_static_method");
		method.setAccessible(true);
		Lookup lookup = getLookupObject(d);
		try {
			MethodHandle mh = lookup.unreflectSpecial(method, d);
			Assert.fail("Should have thrown an IllegalAccessException");
		} catch (IllegalAccessException e) {
			/* Pass */
		}
	}
}
