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
package org.openj9.test.jsr335.defineAnonClass;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.io.PrintStream;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

@Test(groups = { "level.sanity" })
public class TestUnsafeDefineAnonClass {
	private static final int CLASS_UNLOADING_ITERATIONS = 100000;
	private static final int CLASS_REFLECTION_ITERATIONS = 10000;
	private static final int CLASSESS_KEPT_ALIVE_AT_ANY_TIME = 100;
	public static final int CORRECT_ANSWER = 100;
	
	/**
	 * Create an anonClass repeatedly keeping a certain amount of classes alive at a single time.
	 * This forces GC to unload the classes.
	 */
	@Test(groups = { "level.sanity" })
	public void testAnonClassUnloading() {
		byte[] classBytes = DefineAnonClass.getClassBytesFromResource(BasicClass.class);
		CircularBuffer<Class<?>> buf = new CircularBuffer<Class<?>>(CLASSESS_KEPT_ALIVE_AT_ANY_TIME);
		for (int i = 0; i < CLASS_UNLOADING_ITERATIONS; i++) {
			Class<?> anonClass = DefineAnonClass.callDefineAnonClass(TestUnsafeDefineAnonClass.class, classBytes, null);
			
			/* keep class alive for some time */
			buf.addElement(anonClass);
		}
	}
	
	/**
	 * Create an anonClass repeatedly keeping a certain amount of classes alive at a single time.
	 * Also, call functions on the created class. This forces GC to unload the class and forces the JIT
	 * to address multiple code paths.
	 */
	@Test(groups = { "level.sanity" })
	public void testAnonClassCodePaths() {
		byte[] classBytes = DefineAnonClass.getClassBytesFromResource(BasicClass.class);
		CircularBuffer<Class<?>> buf = new CircularBuffer<Class<?>>(CLASSESS_KEPT_ALIVE_AT_ANY_TIME);
		for (int i = 0; i < CLASS_REFLECTION_ITERATIONS; i++) {
			Class<?> anonClass = DefineAnonClass.callDefineAnonClass(TestUnsafeDefineAnonClass.class, classBytes, null);
			runBasicClassTests(anonClass);
			
			/* keep class alive for some time */
			buf.addElement(anonClass);
		}
	}
	
	@Test(groups = { "level.sanity" })
	public void testAnonClassWithNullClassBytes() {
		try {
			DefineAnonClass.callDefineAnonClass(TestUnsafeDefineAnonClass.class, null, null);
			Assert.fail("NullPointerException expected!");
		} catch (NullPointerException e) {
			/* passed */
		}
	}
	
	@Test(groups = { "level.sanity" })
	public void testAnonClassWithNullClassBytesAndNullHostClass() {
		try {
			DefineAnonClass.callDefineAnonClass(null, null, null);
			Assert.fail("NullPointerException expected!");
		} catch (NullPointerException e) {
			/* passed */
		}
	}
	
	@Test(groups = { "level.sanity" })
	public void testAnonClassWithNullHostClass() {
		try {
			byte[] classBytes = DefineAnonClass.getClassBytesFromResource(BasicClass.class);
			DefineAnonClass.callDefineAnonClass(null, classBytes, null);
			Assert.fail("NullPointerException or IllegalArgumentException expected!");
		} catch (IllegalArgumentException | NullPointerException e) {
			/* IllegalArgumentException expected in Java 8, NullPointerException expected in Java 9 and later */
			if (System.getProperty("java.specification.version").equals("1.8")) {
				Assert.assertTrue(e instanceof IllegalArgumentException);
			} else {
				Assert.assertTrue(e instanceof NullPointerException);
			}
			/* passed */
		}
	}
	
	@Test(groups = { "level.sanity" })
	public void testAnonClassGetOwnClassName() {
		try {
			byte[] classBytes = DefineAnonClass.getClassBytesFromResource(AnonClass.class);
			Class<?> clazz = DefineAnonClass.callDefineAnonClass(TestUnsafeDefineAnonClass.class, classBytes, null);
			Method getClassName = clazz.getDeclaredMethod("getClassName", null);
			String className = (String) getClassName.invoke(clazz.newInstance(), null);
			Assert.assertEquals("AnonClass", className);
		} catch (NoSuchMethodException | IllegalAccessException | InstantiationException
				| InvocationTargetException e) {
			e.printStackTrace();
			Assert.fail("Exception caught!");
		}
	}

	/**
	 * Finds the declared field, then sets it to a different value. After, it verifies that
	 * it was appropriately set.
	 * 
	 * @param fieldName, name of the field
	 * @param anonInstance, instance of anonClass
	 * @param anonClass, 
	 * @param illegalAccess, set when IllegalAccessExcpetion is expected
	 * 
	 */
	private void testIntField(String fieldName, Object anonInstance, Class<?> anonClass, boolean illegalAccess) {
		try {
			Field f = anonClass.getDeclaredField(fieldName);
			f.set(anonInstance, new Integer(CORRECT_ANSWER));
			AssertJUnit.assertFalse("Expected IllegalAccessExcpetion, but exception did not occur!", illegalAccess);
			AssertJUnit.assertEquals("Field is not set to the correct value", CORRECT_ANSWER, f.getInt(anonInstance));
		} catch (NoSuchFieldException | SecurityException | IllegalArgumentException e) {
			Assert.fail("UnexpectedException Occured: " + e);
		} catch (IllegalAccessException e) {
			AssertJUnit.assertTrue("IllegalAccessException occured when not expected!", illegalAccess);
		}
	}
	
	/**
	 * Finds the declared method, then invokes it. After, it verifies that
	 * that correct value was returned.
	 * 
	 * @param methodName, the name of the method
	 * @param anonInstance, instance of the anonClass
	 * @param anonClass
	 * @param illegalAccess, set when IllegalAccessExcpetion is expected
	 * @param staticMethod, set when method is invoke on Class Object
	 */
	private void testIntMethod(String methodName, Object anonInstance, Class<?> anonClass, boolean illegalAccess, boolean staticMethod) {
		try {
			Method m = anonClass.getDeclaredMethod(methodName);
			Object intanceOrClass;
			if (staticMethod) {
				intanceOrClass = anonClass;
			} else {
				intanceOrClass = anonInstance;
			}
			Integer result = (Integer) m.invoke(intanceOrClass, (Object[]) null);
			AssertJUnit.assertFalse("Expected IllegalAccessExcpetion, but exception did not occur!", illegalAccess);
			AssertJUnit.assertEquals("Method did not return the correct value", CORRECT_ANSWER, result.intValue());
		} catch (InvocationTargetException | NoSuchMethodException | SecurityException | IllegalArgumentException e) {
			Assert.fail("UnexpectedException Occured: " + e);
		} catch (IllegalAccessException e) {
			AssertJUnit.assertTrue("IllegalAccessException occured when not expected!", illegalAccess);
		}
	}
	
	/**
	 * Runs basic field and method testing on the provided anonClass
	 * 
	 * @param anonClass
	 */
	private void runBasicClassTests(Class<?> anonClass) {
		Object anonInstance = null;
		try {
			anonInstance = anonClass.newInstance();
		} catch (IllegalAccessException | InstantiationException e1) {
			Assert.fail("could not instansiate anonClass!");
		}
		
		testIntField("staticField", anonInstance, anonClass, false);
		testIntField("defaultField", anonInstance, anonClass, false);
		testIntField("privateStaticField", anonInstance, anonClass, true);
		testIntField("privateField", anonInstance, anonClass, true);
		testIntField("protectedField", anonInstance, anonClass, false);
		testIntField("finalField", anonInstance, anonClass, true);
		testIntField("publicField", anonInstance, anonClass, false);
		
		testIntMethod("callStaticFunction", anonInstance, anonClass, false, true);
		testIntMethod("callFunction", anonInstance, anonClass, false, false);
		testIntMethod("callPrivateFunction", anonInstance, anonClass, true, false);
		testIntMethod("callPrivateStaticFunction", anonInstance, anonClass, true, true);
		testIntMethod("callProtectedStaticFunction", anonInstance, anonClass, false, true);
		testIntMethod("callProtectedFunction", anonInstance, anonClass, false, false);
	}
}
