/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package org.openj9.test.invoker;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.reflect.Method;
import java.util.Hashtable;

import org.openj9.test.invoker.util.ClassGenerator;
import org.openj9.test.invoker.util.DummyClassGenerator;
import org.openj9.test.invoker.util.GeneratedClassLoader;
import org.openj9.test.invoker.util.HelperClassGenerator;
import org.openj9.test.invoker.util.InterfaceGenerator;
import org.openj9.test.invoker.util.MethodType;

@Test(groups = { "level.sanity" })
public class SharedCPEntryInvokerTestCases {
	public static Logger logger = Logger.getLogger(SharedCPEntryInvokerTestCases.class);

	private static final String PKG_NAME = "org/openj9/test/invoker";

	String dummyClassName = PKG_NAME + "/" + ClassGenerator.DUMMY_CLASS_NAME;
	String helperClassName = PKG_NAME + "/" + ClassGenerator.HELPER_CLASS_NAME;
	String intfClassName = PKG_NAME + "/" + ClassGenerator.INTERFACE_NAME;

	public void testStaticInterfaceV1() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testStaticInterfaceV1";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_INTERFACE, DummyClassGenerator.INVOKE_STATIC });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = ClassGenerator.DUMMY_CLASS_NAME + "." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}
		logger.debug("Calling invokeStatic(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}
	}

	public void testStaticInterfaceV2() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testStaticInterfaceV2";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.INTF_HAS_DEFAULT_METHOD, new Boolean(true));
		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_INTERFACE, DummyClassGenerator.INVOKE_STATIC });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = ClassGenerator.DUMMY_CLASS_NAME + "." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}
		logger.debug("Calling invokeStatic(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}
	}

	public void testStaticInterfaceV3() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testStaticInterfaceV3";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.INTF_HAS_STATIC_METHOD, new Boolean(true));
		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_INTERFACE, DummyClassGenerator.INVOKE_STATIC });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		logger.debug("Calling invokeInterface(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}

		expected = "Static " + ClassGenerator.INTERFACE_NAME + "." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeStatic(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}
	}

	public void testSpecialInterfaceV1() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testSpecialInterfaceV1";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_INTERFACE, DummyClassGenerator.INVOKE_SPECIAL });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = ClassGenerator.DUMMY_CLASS_NAME + "." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}

		logger.debug("Calling invokeSpecial(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}
	}

	public void testSpecialInterfaceV2() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testSpecialInterfaceV2";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.INTF_HAS_DEFAULT_METHOD, new Boolean(true));
		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_INTERFACE, DummyClassGenerator.INVOKE_SPECIAL });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = ClassGenerator.DUMMY_CLASS_NAME + "." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}

		expected = "Default " + ClassGenerator.INTERFACE_NAME + "." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeSpecial(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}
	}

	public void testStaticSpecialV1() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testStaticSpecialV1";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator helperClassGenerator = new HelperClassGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.HELPER_METHOD_TYPE, MethodType.STATIC);
		helperClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(helperClassGenerator.getClassData());
		Class.forName(helperClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.EXTENDS_HELPER_CLASS, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_STATIC, DummyClassGenerator.INVOKE_SPECIAL });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = "Static " + ClassGenerator.HELPER_CLASS_NAME + "." + ClassGenerator.HELPER_METHOD_NAME;
		logger.debug("Calling invokeStatic(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}

		logger.debug("Calling invokeSpecial(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}
	}

	public void testStaticSpecialV2() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testStaticSpecialV2";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator helperClassGenerator = new HelperClassGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.HELPER_METHOD_TYPE, MethodType.VIRTUAL);
		helperClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(helperClassGenerator.getClassData());
		Class.forName(helperClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.EXTENDS_HELPER_CLASS, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_STATIC, DummyClassGenerator.INVOKE_SPECIAL });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		logger.debug("Calling invokeStatic(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}

		expected = "Virtual " + ClassGenerator.HELPER_CLASS_NAME + "." + ClassGenerator.HELPER_METHOD_NAME;
		logger.debug("Calling invokeSpecial(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}
	}

	public void testStaticVirtualV1() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testStaticVirtualV1";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator helperClassGenerator = new HelperClassGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.HELPER_METHOD_TYPE, MethodType.STATIC);
		helperClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(helperClassGenerator.getClassData());
		Class.forName(helperClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.USE_HELPER_CLASS, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_STATIC, DummyClassGenerator.INVOKE_VIRTUAL });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = "Static " + ClassGenerator.HELPER_CLASS_NAME + "." + ClassGenerator.HELPER_METHOD_NAME;
		logger.debug("Calling invokeStatic(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}

		logger.debug("Calling invokeVirtual(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeVirtual", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}
	}

	public void testStaticVirtualV2() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testStaticVirtualV2";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator helperClassGenerator = new HelperClassGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.HELPER_METHOD_TYPE, MethodType.VIRTUAL);
		helperClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(helperClassGenerator.getClassData());
		Class.forName(helperClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.USE_HELPER_CLASS, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_STATIC, DummyClassGenerator.INVOKE_VIRTUAL });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		logger.debug("Calling invokeStatic(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}

		expected = "Virtual " + ClassGenerator.HELPER_CLASS_NAME + "." + ClassGenerator.HELPER_METHOD_NAME;
		logger.debug("Calling invokeVirtual(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeVirtual", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}
	}

	public void testSpecialVirtual() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testSpecialVirtual";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator helperClassGenerator = new HelperClassGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.HELPER_METHOD_TYPE, MethodType.VIRTUAL);
		helperClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(helperClassGenerator.getClassData());
		Class.forName(helperClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.EXTENDS_HELPER_CLASS, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_SPECIAL, DummyClassGenerator.INVOKE_VIRTUAL });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = "Virtual " + ClassGenerator.HELPER_CLASS_NAME + "." + ClassGenerator.HELPER_METHOD_NAME;
		logger.debug("Calling invokeSpecial(). Expected to Expected to return: " + expected);
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}

		expected = "Virtual " + ClassGenerator.HELPER_CLASS_NAME + "." + ClassGenerator.HELPER_METHOD_NAME;
		logger.debug("Calling invokeVirtual(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeVirtual", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}
	}

	public void testStaticSpecialInterfaceV1() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testStaticSpecialInterfaceV1";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] {
						DummyClassGenerator.INVOKE_INTERFACE,
						DummyClassGenerator.INVOKE_STATIC,
						DummyClassGenerator.INVOKE_SPECIAL });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = ClassGenerator.DUMMY_CLASS_NAME + "." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}

		logger.debug("Calling invokeStatic(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}

		logger.debug("Calling invokeSpecial(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}
	}

	public void testStaticSpecialInterfaceV2() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testStaticSpecialInterfaceV2";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.INTF_HAS_DEFAULT_METHOD, new Boolean("true"));
		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] {
						DummyClassGenerator.INVOKE_INTERFACE,
						DummyClassGenerator.INVOKE_STATIC,
						DummyClassGenerator.INVOKE_SPECIAL });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = ClassGenerator.DUMMY_CLASS_NAME + "." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}

		logger.debug("Calling invokeStatic(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}

		expected = "Default " + ClassGenerator.INTERFACE_NAME + "." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeSpecial(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}
	}

	public void testStaticSpecialInterfaceV3() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testStaticSpecialInterfaceV2";
		Class<?> dummyClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.INTF_HAS_STATIC_METHOD, new Boolean("true"));
		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] {
						DummyClassGenerator.INVOKE_INTERFACE,
						DummyClassGenerator.INVOKE_STATIC,
						DummyClassGenerator.INVOKE_SPECIAL });

		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		logger.debug("Calling invokeInterface(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}

		expected = "Static " + ClassGenerator.INTERFACE_NAME + "." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeStatic(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown", e);
		}

		logger.debug("Calling invokeSpecial(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught.");
		}
	}
}
