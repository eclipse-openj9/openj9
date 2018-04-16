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
//package java.lang.invoke;
package org.openj9.test.jsr335;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandleInfo;
import java.lang.invoke.MethodHandleProxies;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;

@Test(groups = { "level.sanity" })
public class TestMethodHandleInfo {
	
	/**
	 * Helper method used by the tests to determine if the specified
	 * MethodHandleInfo object contains the expected values
	 */
	private void checkMethodHandleInfo(MethodHandleInfo info, 
			Class<?> expectedDeclaringClass, 
			MethodType expectedMethodType, 
			int expectedModifiers, 
			String expectedMethodName, 
			int expectedReferenceKind) 
	{
		AssertJUnit.assertTrue("info.getDeclaringClass() did not return the correct result" +
				"\n\tExpected: " + expectedDeclaringClass
				+ "\n\tActual: " + info.getDeclaringClass(), 
				expectedDeclaringClass.equals(info.getDeclaringClass()));
		
		AssertJUnit.assertTrue("info.getMethodType() did not return the correct result" +
				"\n\tExpected: " + expectedMethodType
				+ "\n\tActual: " + info.getMethodType(), 
				expectedMethodType.equals(info.getMethodType()));
		
		AssertJUnit.assertTrue("info.getModifiers() did not return the correct result" +
				"\n\tExpected: " + Modifier.toString(expectedModifiers) + " => 0x" + Integer.toHexString(expectedModifiers)
				+ "\n\tActual: " + Modifier.toString(info.getModifiers()) + " => 0x" + Integer.toHexString(info.getModifiers()), 
				expectedModifiers == info.getModifiers());
		
		AssertJUnit.assertTrue("info.getName() did not return the correct result" +
				"\n\tExpected: " + expectedMethodName
				+ "\n\tActual: " + info.getName(), 
				expectedMethodName.equals(info.getName()));
		
		AssertJUnit.assertTrue("info.getReferenceKind() did not return the correct result" +
				"\n\tExpected: " + expectedReferenceKind
				+ "\n\tActual: " + info.getReferenceKind(), 
				expectedReferenceKind == info.getReferenceKind());
	}
	
	
	/**
	 * Test a simple constructor
	 */
	@Test
	public void testConstructor() {
		try {
			MethodType methodType = MethodType.methodType(void.class);
			MethodHandle methodHandle = MethodHandles.lookup().findConstructor(Object.class, methodType);
			
			MethodHandleInfo info = MethodHandles.lookup().revealDirect(methodHandle);
			
			checkMethodHandleInfo(info, 
					Object.class, 
					MethodType.methodType(void.class), 
					Modifier.PUBLIC, 
					"<init>", 
					MethodHandleInfo.REF_newInvokeSpecial);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
	}

	/**
	 * Test a public static method reference
	 */
	@Test
	public void testPublicStaticMethod() {
		try {
			Class declaringClass = System.class;
			MethodType methodType = MethodType.methodType(String.class, String.class);
			String methodName = "getenv"; 
			MethodHandle methodHandle = MethodHandles.lookup().findStatic(declaringClass, methodName, methodType);
			
			MethodHandleInfo info = MethodHandles.lookup().revealDirect(methodHandle);
			
			checkMethodHandleInfo(info, 
					declaringClass, 
					methodType, 
					Modifier.PUBLIC | Modifier.STATIC, 
					methodName, 
					MethodHandleInfo.REF_invokeStatic);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
	}
	
	/**
	 * Test a public instance method reference
	 */
	@Test
	public void testPublicInstanceMethod() {
		try {
			Class declaringClass = String.class;
			String methodName = "length";
			MethodType methodType = MethodType.methodType(int.class);
			MethodHandle methodHandle = MethodHandles.lookup().findVirtual(declaringClass, methodName, methodType);
			
			MethodHandleInfo info = MethodHandles.lookup().revealDirect(methodHandle);
			
			checkMethodHandleInfo(info, 
					declaringClass, 
					MethodType.methodType(int.class), 
					Modifier.PUBLIC, 
					methodName, 
					MethodHandleInfo.REF_invokeVirtual);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
	}
	
	public volatile int fieldGetterTestInt = 1;
	/**
	 * Test a field getter handle
	 */
	@Test
	public void testFieldGetter() {
		try {
			Class<?> declaringClass = this.getClass();
			String fieldName = "fieldGetterTestInt";
			MethodHandle methodHandle = MethodHandles.lookup().findGetter(declaringClass, fieldName, int.class);
			
			MethodHandleInfo info = MethodHandles.lookup().revealDirect(methodHandle);
			
			checkMethodHandleInfo(info, 
					declaringClass, 
					MethodType.methodType(int.class), 
					Modifier.PUBLIC | Modifier.VOLATILE, 
					fieldName, 
					MethodHandleInfo.REF_getField);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
	}
	
	public String fieldSetterTestString = "test";
	/**
	 * Test a field setter handle
	 */
	@Test
	public void testFieldSetter() {
		try {
			Class declaringClass = this.getClass();
			String fieldName = "fieldSetterTestString";
			MethodHandle methodHandle = MethodHandles.lookup().findSetter(declaringClass, fieldName, String.class);
			
			MethodHandleInfo info = MethodHandles.lookup().revealDirect(methodHandle);
			
			checkMethodHandleInfo(info, 
					declaringClass, 
					MethodType.methodType(void.class, String.class), 
					Modifier.PUBLIC, 
					fieldName, 
					MethodHandleInfo.REF_putField);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
	}
	
	public static volatile int staticFieldGetterTestInt = 1;
	/**
	 * Test a static field getter handle
	 */
	@Test
	public void testStaticFieldGetter() {
		try {
			Class declaringClass = this.getClass();
			String fieldName = "staticFieldGetterTestInt";
			MethodHandle methodHandle = MethodHandles.lookup().findStaticGetter(declaringClass, fieldName, int.class);
			
			MethodHandleInfo info = MethodHandles.lookup().revealDirect(methodHandle);
			
			checkMethodHandleInfo(info, 
					declaringClass, 
					MethodType.methodType(int.class), 
					Modifier.PUBLIC | Modifier.VOLATILE | Modifier.STATIC, 
					fieldName, 
					MethodHandleInfo.REF_getStatic);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
	}
	
	public static String staticFieldSetterTestString = "test";
	/**
	 * Test a static field setter handle
	 */
	@Test
	public void testStaticFieldSetter() {
		try {
			Class<?> declaringClass = this.getClass();
			String fieldName = "staticFieldSetterTestString";
			MethodHandle methodHandle = MethodHandles.lookup().findStaticSetter(declaringClass, fieldName, String.class);
			
			MethodHandleInfo info = MethodHandles.lookup().revealDirect(methodHandle);
			
			checkMethodHandleInfo(info, 
					declaringClass, 
					MethodType.methodType(void.class, String.class), 
					Modifier.PUBLIC | Modifier.STATIC, 
					fieldName, 
					MethodHandleInfo.REF_putStatic);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
	}
	
	/**
	 * Test an interface handle
	 */
	@Test
	public void testInterface() {
		/*
		 * This class is a subclass of junit.framework.TestCase which 
		 * implements the junit.framework.Test interface. The latter
		 * declares the 'public abstract int countTestCases()' method 
		 */
		try {
//			Class declaringClass = org.testng.junit.JUnit3TestClass.class;
//			String methodName = "getInstanceCount";
			Class declaringClass = junit.framework.Test.class;
			String methodName = "countTestCases";
			MethodHandle methodHandle = MethodHandles.lookup().findVirtual(declaringClass, methodName, MethodType.methodType(int.class));
			
			MethodHandleInfo info = MethodHandles.lookup().revealDirect(methodHandle);
			
			checkMethodHandleInfo(info, 
					declaringClass, 
					MethodType.methodType(int.class), 
					Modifier.PUBLIC | Modifier.ABSTRACT, 
					methodName, 
					MethodHandleInfo.REF_invokeInterface);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
	}
	
	/**
	 * Test a special handle
	 */
	@Test
	public void testSpecial() {
		try {
			Class<?> declaringClass = TestMethodHandleInfo.class;
			String methodName = "testSpecial";
			MethodType methodType = MethodType.methodType(void.class);
			MethodHandle methodHandle = MethodHandles.lookup().findSpecial(declaringClass, methodName, methodType, TestMethodHandleInfo.class);
			
			MethodHandleInfo info = MethodHandles.lookup().revealDirect(methodHandle);
			
			checkMethodHandleInfo(info, 
					declaringClass, 
					methodType, 
					Modifier.PUBLIC, 
					methodName, 
					MethodHandleInfo.REF_invokeSpecial);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
	}
	
	/**
	 * Test a varargs handle
	 */
	@Test
	public void testVarargs() {
		try {
			Class<?> declaringClass = String.class;
			/* public static java.lang.String.format(String format, Object... args) */
			String methodName = "format";
			MethodType methodType = MethodType.methodType(String.class, String.class, Object[].class);
			MethodHandle methodHandle = MethodHandles.lookup().findStatic(declaringClass, methodName, methodType);
			
			AssertJUnit.assertTrue("methodHandle is not an instance of VarargsCollectorHandle", methodHandle.isVarargsCollector());
			
			MethodHandleInfo info = MethodHandles.lookup().revealDirect(methodHandle);
			
			checkMethodHandleInfo(info, 
					declaringClass, 
					methodType, 
					Modifier.PUBLIC | Modifier.STATIC | Modifier.TRANSIENT, /* Note: MethodHandles.lookup().VARARGS == Modifiers.TRANSIENT, the same bit is reused */  
					methodName, 
					MethodHandleInfo.REF_invokeStatic);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail(e.getLocalizedMessage());
		}
	}
	
	/**
	 * Test a non-public JSR 292 handle. 
	 * These should be filtered out by MethodHandles.lookup().revealDirect()
	 * and we should not get a MethodHandleInfo for them.
	 */
	@Test
	public void testNonPublicJSR292Method() throws Throwable{
		try {	
			Method method = MethodHandleProxies.class.getDeclaredMethod("isObjectMethod", Method.class);
			method.setAccessible(true);
			
			MethodHandle methodHandle = MethodHandles.lookup().unreflect(method);
			
			MethodHandleInfo info = MethodHandles.lookup().revealDirect(methodHandle);
			Assert.fail("MethodHandles.lookup().revealDirect() should have returned null because MethodHandleProxies.isObjectMethod() is a non-public method in the java.lang.invoke package.");
		} catch (IllegalArgumentException e) {
			//Success!
		}
	}
}
