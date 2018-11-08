package org.openj9.test.java.security;

/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

import java.lang.reflect.Method;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.lang.reflect.InvocationTargetException;

@Test(groups = { "level.sanity" })
public class Test_MHLoaderConstraints {
	private ClassLoader clOne = new ClassLoaderOne("org.openj9.test.java.security.Dummy", 
		"org.openj9.test.java.security.ChildMHLookupField");
	private Class<?> clzChildMHLookupField = clOne.loadClass("org.openj9.test.java.security.ChildMHLookupField");
	private Object instChildMHLookupField = clzChildMHLookupField.newInstance();

	public Test_MHLoaderConstraints() throws Exception {
	}
	
	/**
	 *  java.lang.invoke.MethodHandles.lookup()#findGetter()
	 *  Loading constraints are violated when reference and type classes (instance field getter) 
	 *  are loaded by different class loaders.
	 *  IllegalAccessException with LinkageError cause is expected to be wrapped within InvocationTargetException.
	 */
	@Test
	public void test_MHInstanceFieldGetter1() throws Throwable {
		Method testInstanceFieldGetter = clzChildMHLookupField.getDeclaredMethod("instanceFieldGetter1");
		try {
			testInstanceFieldGetter.invoke(instChildMHLookupField);
			Assert.fail("InvocationTargetException expected");
		} catch (InvocationTargetException e) {
			Throwable e2 = e.getCause();
			Assert.assertNotNull(e2, "Expected IllegalAccessException to be cause of InvocationTargetException");
			Assert.assertTrue(e2 instanceof IllegalAccessException);
			Assert.assertTrue(e2.getCause() instanceof LinkageError);
		}
	}

	/**
	 *  java.lang.invoke.MethodHandles.lookup()#findGetter()
	 *  Loading constraints are violated when reference/type(instance field getter) and access classes 
	 *  are loaded by different class loaders.
	 *  IllegalAccessException with LinkageError cause is expected to be wrapped within InvocationTargetException.
	 */
	@Test
	public void test_MHInstanceFieldGetter2() throws Throwable {
		Method testInstanceFieldGetter = clzChildMHLookupField.getDeclaredMethod("instanceFieldGetter2");
		try {
			testInstanceFieldGetter.invoke(instChildMHLookupField);
			Assert.fail("InvocationTargetException expected");
		} catch (InvocationTargetException e) {
			Throwable e2 = e.getCause();
			Assert.assertNotNull(e2, "Expected IllegalAccessException to be cause of InvocationTargetException");
			Assert.assertTrue(e2 instanceof IllegalAccessException);
			Assert.assertTrue(e2.getCause() instanceof LinkageError);
		}
	}
	
	/**
	 *  java.lang.invoke.MethodHandles.lookup()#findSetter()
	 *  Loading constraints are violated when reference and type classes (instance field setter) 
	 *  are loaded by different class loaders.
	 *  IllegalAccessException with LinkageError cause is expected to be wrapped within InvocationTargetException.
	 */
	@Test
	public void test_MHInstanceFieldSetter1() throws Throwable {
		Method testInstanceFieldSetter = clzChildMHLookupField.getDeclaredMethod("instanceFieldSetter1");
		try {
			testInstanceFieldSetter.invoke(instChildMHLookupField);
			Assert.fail("InvocationTargetException expected");
		} catch (InvocationTargetException e) {
			Throwable e2 = e.getCause();
			Assert.assertNotNull(e2, "Expected IllegalAccessException to be cause of InvocationTargetException");
			Assert.assertTrue(e2 instanceof IllegalAccessException);
			Assert.assertTrue(e2.getCause() instanceof LinkageError);
		}
	}

	/**
	 *  java.lang.invoke.MethodHandles.lookup()#findSetter()
	 *  Loading constraints are violated when reference/type(instance field setter) and access classes 
	 *  are loaded by different class loaders.
	 *  IllegalAccessException with LinkageError cause is expected to be wrapped within InvocationTargetException.
	 */
	@Test
	public void test_MHInstanceFieldSetter2() throws Throwable {
		Method testInstanceFieldSetter = clzChildMHLookupField.getDeclaredMethod("instanceFieldSetter2");
		try {
			testInstanceFieldSetter.invoke(instChildMHLookupField);
			Assert.fail("InvocationTargetException expected");
		} catch (InvocationTargetException e) {
			Throwable e2 = e.getCause();
			Assert.assertNotNull(e2, "Expected IllegalAccessException to be cause of InvocationTargetException");
			Assert.assertTrue(e2 instanceof IllegalAccessException);
			Assert.assertTrue(e2.getCause() instanceof LinkageError);
		}
	}
	
	/**
	 *  java.lang.invoke.MethodHandles.lookup()#findStaticGetter()
	 *  Loading constraints are violated when reference and type classes (static field getter) 
	 *  are loaded by different class loaders.
	 *  IllegalAccessException with LinkageError cause is expected to be wrapped within InvocationTargetException.
	 */
	@Test
	public void test_MHStaticFieldGetter1() throws Throwable {
		Method testStaticFieldGetter = clzChildMHLookupField.getDeclaredMethod("staticFieldGetter1");
		try {
			testStaticFieldGetter.invoke(instChildMHLookupField);
			Assert.fail("InvocationTargetException expected");
		} catch (InvocationTargetException e) {
			Throwable e2 = e.getCause();
			Assert.assertNotNull(e2, "Expected IllegalAccessException to be cause of InvocationTargetException");
			Assert.assertTrue(e2 instanceof IllegalAccessException);
			Assert.assertTrue(e2.getCause() instanceof LinkageError);
		}
	}

	/**
	 *  java.lang.invoke.MethodHandles.lookup()#findStaticGetter()
	 *  Loading constraints are violated when reference/type(static field getter) and access classes 
	 *  are loaded by different class loaders.
	 *  IllegalAccessException with LinkageError cause is expected to be wrapped within InvocationTargetException.
	 */
	@Test
	public void test_MHStaticFieldGetter2() throws Throwable {
		Method testStaticFieldGetter = clzChildMHLookupField.getDeclaredMethod("staticFieldGetter2");
		try {
			testStaticFieldGetter.invoke(instChildMHLookupField);
			Assert.fail("InvocationTargetException expected");
		} catch (InvocationTargetException e) {
			Throwable e2 = e.getCause();
			Assert.assertNotNull(e2, "Expected IllegalAccessException to be cause of InvocationTargetException");
			Assert.assertTrue(e2 instanceof IllegalAccessException);
			Assert.assertTrue(e2.getCause() instanceof LinkageError);
		}
	}
	
	/**
	 *  java.lang.invoke.MethodHandles.lookup()#findStaticSetter()
	 *  Loading constraints are violated when reference and type classes (static field setter) 
	 *  are loaded by different class loaders.
	 *  IllegalAccessException with LinkageError cause is expected to be wrapped within InvocationTargetException.
	 */
	@Test
	public void test_MHStaticFieldSetter1() throws Throwable {
		Method testStaticFieldSetter = clzChildMHLookupField.getDeclaredMethod("staticFieldSetter1");
		try {
			testStaticFieldSetter.invoke(instChildMHLookupField);
			Assert.fail("InvocationTargetException expected");
		} catch (InvocationTargetException e) {
			Throwable e2 = e.getCause();
			Assert.assertNotNull(e2, "Expected IllegalAccessException to be cause of InvocationTargetException");
			Assert.assertTrue(e2 instanceof IllegalAccessException);
			Assert.assertTrue(e2.getCause() instanceof LinkageError);
		}
	}

	/**
	 *  java.lang.invoke.MethodHandles.lookup()#findStaticSetter()
	 *  Loading constraints are violated when reference/type(static field setter) and access classes 
	 *  are loaded by different class loaders.
	 *  IllegalAccessException with LinkageError cause is expected to be wrapped within InvocationTargetException.
	 */
	@Test
	public void test_MHStaticFieldSetter2() throws Throwable {
		Method testStaticFieldSetter = clzChildMHLookupField.getDeclaredMethod("staticFieldSetter2");
		try {
			testStaticFieldSetter.invoke(instChildMHLookupField);
			Assert.fail("InvocationTargetException expected");
		} catch (InvocationTargetException e) {
			Throwable e2 = e.getCause();
			Assert.assertNotNull(e2, "Expected IllegalAccessException to be cause of InvocationTargetException");
			Assert.assertTrue(e2 instanceof IllegalAccessException);
			Assert.assertTrue(e2.getCause() instanceof LinkageError);
		}
	}
}
