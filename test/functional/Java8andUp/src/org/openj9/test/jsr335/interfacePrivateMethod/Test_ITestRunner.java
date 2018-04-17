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
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

@Test(groups = { "level.sanity"})
public class Test_ITestRunner {
	
	private final static String expectedPrivateStaticMethodOutput = "private_static_method";
	private final static String expectedPrivateNonStaticMethodOutput = "private_non_static_method";

	final static LoadInterfaceContainingPrivateMethod loadClass = new LoadInterfaceContainingPrivateMethod();

	/* Object instance is an instance of the class ImplClass which implements the interface ITest */
	static Object instance; 
	static {
		try {
			instance = Class.forName("ImplClass", false, loadClass).newInstance();
		} catch (InstantiationException | IllegalAccessException | ClassNotFoundException e) {
			throw new RuntimeException(e);
		}
	}
		
	/**
	 * This method is used to test if a private static method in our interface can be called with the INVOKESTATIC opcode
	 */
	
	@Test
	public  void testRunTest_private_static_method_with_InvokeStatic() throws Exception, LinkageError {
		Class<?> c = Class.forName("ITest", true, loadClass);
		Method method = c.getDeclaredMethod("RunTest_private_static_method_with_InvokeStatic");
		method.setAccessible(true);
		Object ret = method.invoke(instance);
		AssertJUnit.assertEquals("private static method with INVOKESTATIC failed!!", expectedPrivateStaticMethodOutput, ret);
	}
	
	/**
	 * This method is used to test is a private non static method in our interface can be called with the INVOKESTATIC opcode using
	 * reflection. This is expected to throw an IncompatibleClassChangeError
	 */
	
	@Test
	public void testRunTest_private_non_static_method_with_InvokeStatic() throws Exception, LinkageError {
		try {
			Class<?> c = Class.forName("ITest", true, loadClass);
			Method method = c.getDeclaredMethod("RunTest_private_non_static_method_with_InvokeStatic");
			method.setAccessible(true);
			method.invoke(instance);
			Assert.fail("Should have thrown an IncompatibleClassChangeError");
		} catch(InvocationTargetException e) {
			Throwable cause = e.getCause();
			AssertJUnit.assertTrue("private non static method with INVOKESTATIC failed!!", cause instanceof IncompatibleClassChangeError);
		}
		
	}

	/**
	 * This method is used to test is a private static method in our interface can be called with the INVOKEVIRTUAL opcode using
	 * reflection. This is expected to throw an IncompatibleClassChangeError
	 */
	
	@Test
	public static void testRunTest_private_static_method_with_InvokeVirtual() throws Exception, LinkageError {
		try {
			Class<?> c = Class.forName("ITest", true, loadClass);
			Method method = c.getDeclaredMethod("RunTest_private_static_method_with_InvokeVirtual", c);
			method.setAccessible(true);
			method.invoke(instance, instance);
			Assert.fail("Should have thrown an IncompatibleClassChangeError");
		} catch(InvocationTargetException e) {
				Throwable cause = e.getCause();
				AssertJUnit.assertTrue("private static method with INVOKEVIRTUAL failed!!", cause instanceof IncompatibleClassChangeError);
		}
	}	
	
	/**
	 * This method is used to test is a private non static method in our interface can be called with the INVOKEVIRTUAL opcode using
	 * reflection. This is expected to throw an IncompatibleClassChangeError
	 */
	
	@Test
	public static void testRunTest_private_non_static_method_with_InvokeVirtual() throws Exception, LinkageError {
		try {
			Class<?> c = Class.forName("ITest", true, loadClass);
			Method method = c.getDeclaredMethod("RunTest_private_non_static_method_with_InvokeVirtual", c);
			method.setAccessible(true);
			method.invoke(instance, instance);
			Assert.fail("Should have thrown an IncompatibleClassChangeError");
		} catch(InvocationTargetException e) {
			Throwable cause = e.getCause();
			AssertJUnit.assertTrue("private static method with INVOKEVIRTUAL failed!!", cause instanceof IncompatibleClassChangeError);
		}
	}	
	
	/**
	 * This method is used to test is a private static method in our interface can be called with the INVOKESPECIAL opcode using
	 * reflection. This is expected to throw an IncompatibleClassChangeError
	 */
	
	@Test
	public static void testRunTest_private_static_method_with_InvokeSpecial() throws Exception, LinkageError {
		try {
			Class<?> c = Class.forName("ITest", true, loadClass);
			Method method = c.getDeclaredMethod("RunTest_private_static_method_with_InvokeSpecial", c);
			method.setAccessible(true);
			method.invoke(instance, instance);
			Assert.fail("Should have thrown an IncompatibleClassChangeError");
		}	catch(InvocationTargetException e) {
			Throwable cause = e.getCause();
			AssertJUnit.assertTrue("private static method with INVOKESPECIAL failed!!", cause instanceof IncompatibleClassChangeError);
		}
	}
	
	/**
	 * This method is used to test if a private static method in our interface can be called with the INVOKESTATIC opcode
	 */
	
	@Test
	public void testRunTest_private_non_static_method_with_InvokeSpecial() throws Exception, LinkageError {
		Class<?> c = Class.forName("ITest", true, loadClass);
		Method method = c.getDeclaredMethod("RunTest_private_non_static_method_with_InvokeSpecial",c);
		method.setAccessible(true);
		Object ret = method.invoke(instance, instance);
		AssertJUnit.assertEquals("private non static method with INVOKESPECIAL failed!!", expectedPrivateNonStaticMethodOutput, ret);
	}
	
	/**
	 * This method is used to test if a private static method can be called using the INVOKEINTERFACE opcode. It is expected to throw 
	 * a VerifyError during the initialization of the instance of the class which implements the interface   
	 */
	
	@Test
	public static void testRunTest_private_static_method_with_InvokeInterface() throws Exception, LinkageError {
		try { 
			Class.forName("ImplClassForStaticInvokeInterface", true, loadClass);
			Assert.fail("Should have thrown a VerifyError");
		} catch (VerifyError e) {
		} 
	}	

	/**
	 * This method is used to test if a private non static method can be called using the INVOKEINTERFACE opcode. It is expected to throw 
	 * a VerifyError during the initialization of the instance of the class which implements the interface   
	 */
	
	@Test
	public static void testRunTest_private_non_static_method_with_InvokeInterface() throws Exception, LinkageError {
		try {
			Class.forName("ImplClassForNonStaticInvokeInterface", true, loadClass);
			Assert.fail("Should have thrown a VerifyError");
		} catch (VerifyError e) {
		}
	} 
		
}
