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

import org.openj9.test.invoker.util.DummyClassGenerator;
import org.openj9.test.invoker.util.GeneratedClassLoader;
import org.openj9.test.invoker.util.HelperClassGenerator;
import org.openj9.test.invoker.util.InterfaceGenerator;
import org.openj9.test.invoker.util.ClassGenerator;
import org.openj9.test.invoker.util.MethodType;
import com.ibm.jvmti.tests.redefineClasses.rc001;

@Test(groups = { "level.sanity" })
public class InvalidateSplitTableRetransformationTest {
	public static Logger logger = Logger.getLogger(InvalidateSplitTableRetransformationTest.class);

	private static final String PKG_NAME = "com/ibm/j9/invoker/junit";

	String dummyClassName = PKG_NAME + "/" + ClassGenerator.DUMMY_CLASS_NAME;
	String helperClassName = PKG_NAME + "/" + ClassGenerator.HELPER_CLASS_NAME;
	String intfClassName = PKG_NAME + "/" + ClassGenerator.INTERFACE_NAME;

	private boolean redefineInterface(Class<?> intfClass, String testName, Hashtable<String, Object> characteristics)
			throws Exception {
		ClassGenerator classGenerator = new InterfaceGenerator(testName, PKG_NAME, characteristics);
		byte[] classData = classGenerator.getClassData();
		return rc001.redefineClass(intfClass, classData.length, classData);
	}

	private boolean redefineHelperClass(Class<?> helperClass, String testName,
			Hashtable<String, Object> characteristics) throws Exception {
		ClassGenerator classGenerator = new HelperClassGenerator(testName, PKG_NAME, characteristics);
		byte[] classData = classGenerator.getClassData();
		return rc001.redefineClass(helperClass, classData.length, classData);
	}

	private boolean redefineDummyClass(Class<?> dummyClass, String testName, Hashtable<String, Object> characteristics)
			throws Exception {
		ClassGenerator classGenerator = new DummyClassGenerator(testName, PKG_NAME, characteristics);
		byte[] classData = classGenerator.getClassData();
		return rc001.redefineClass(dummyClass, classData.length, classData);
	}

	public void testInvokestaticInvokeinterface() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testInvokestaticInvokeinterface";
		Class<?> dummyClass = null;
		Class<?> intfClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.VERSION, "V1");
		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		intfClass = Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V1");
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_INTERFACE, DummyClassGenerator.INVOKE_STATIC });
		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V1." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}
		logger.debug("Calling invokeStatic(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}

		logger.debug("Redefining Interface to V2");
		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V2");
		characteristics.put(ClassGenerator.INTF_HAS_DEFAULT_METHOD, new Boolean(true));
		if (!redefineInterface(intfClass, testName, characteristics)) {
			Assert.fail("Failed to redefine interface");
		}

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V1." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}
		logger.debug("Calling invokeStatic(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}

		logger.debug("Redefining Interface to V3");
		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V3");
		characteristics.put(ClassGenerator.INTF_HAS_STATIC_METHOD, new Boolean(true));
		if (!redefineInterface(intfClass, testName, characteristics)) {
			Assert.fail("Failed to redefine interface");
		}

		logger.debug("Calling invokeInterface(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}

		expected = "Static " + ClassGenerator.INTERFACE_NAME + "_V3." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeStatic(). Expected to return " + expected);
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}

		logger.debug("Redefining Interface to V1");
		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V1");
		if (!redefineInterface(intfClass, testName, characteristics)) {
			Assert.fail("Failed to redefine interface");
		}

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V1." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}
		logger.debug("Calling invokeStatic(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}
	}

	public void testInvokespecialInvokeinterface() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testInvokespecialInvokeinterface";
		Class<?> dummyClass = null;
		Class<?> intfClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		ClassGenerator helperClassGenerator = new HelperClassGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.VERSION, "V1");
		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		intfClass = Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V1");
		characteristics.put(ClassGenerator.HELPER_METHOD_TYPE, MethodType.VIRTUAL);
		helperClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(helperClassGenerator.getClassData());
		Class.forName(helperClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V1");
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.EXTENDS_HELPER_CLASS, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_INTERFACE, DummyClassGenerator.INVOKE_SPECIAL });
		characteristics.put(ClassGenerator.SHARED_INVOKERS_TARGET, "INTERFACE");
		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V1." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}
		logger.debug("Calling invokeSpecial(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}

		logger.debug("Redefining Interface to V2");
		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V2");
		characteristics.put(ClassGenerator.INTF_HAS_DEFAULT_METHOD, new Boolean(true));
		if (!redefineInterface(intfClass, testName, characteristics)) {
			Assert.fail("Failed to redefine interface");
		}

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V1." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}

		expected = "Default " + ClassGenerator.INTERFACE_NAME + "_V2." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeSpecial(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}

		logger.debug("Redefining Dummy class to V2");
		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V2");
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.EXTENDS_HELPER_CLASS, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_INTERFACE, DummyClassGenerator.INVOKE_SPECIAL });
		characteristics.put(ClassGenerator.SHARED_INVOKERS_TARGET, "SUPER");
		if (!redefineDummyClass(dummyClass, testName, characteristics)) {
			logger.debug("Expected failure in redefining to dummy class V2");
		} else {
			Assert.fail("Did not expect to be able to redefined to dummy class V2");
		}

	}

	public void testInvokestaticInvokespecial() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testInvokestaticInvokespecial";
		Class<?> dummyClass = null;
		Class<?> helperClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator helperClassGenerator = new HelperClassGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.VERSION, "V1");
		characteristics.put(ClassGenerator.HELPER_METHOD_TYPE, MethodType.STATIC);
		helperClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(helperClassGenerator.getClassData());
		helperClass = Class.forName(helperClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V1");
		characteristics.put(ClassGenerator.EXTENDS_HELPER_CLASS, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] { DummyClassGenerator.INVOKE_STATIC, DummyClassGenerator.INVOKE_SPECIAL });
		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = "Static " + ClassGenerator.HELPER_CLASS_NAME + "_V1." + ClassGenerator.HELPER_METHOD_NAME;
		logger.debug("Calling invokeStatic(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}
		logger.debug("Calling invokeSpecial(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}

		logger.debug("Redefining Helper class to V2");
		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V2");
		characteristics.put(ClassGenerator.HELPER_METHOD_TYPE, MethodType.VIRTUAL);
		if (!redefineHelperClass(helperClass, testName, characteristics)) {
			Assert.fail("Failed to redefine dummy class");
		}

		logger.debug("Calling invokeStatic(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}

		expected = "Virtual " + ClassGenerator.HELPER_CLASS_NAME + "_V2." + ClassGenerator.HELPER_METHOD_NAME;
		logger.debug("Calling invokeSpecial(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}

		logger.debug("Redefining Helper class to V1");
		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V1");
		characteristics.put(ClassGenerator.HELPER_METHOD_TYPE, MethodType.STATIC);
		if (!redefineHelperClass(helperClass, testName, characteristics)) {
			Assert.fail("Failed to redefine dummy class");
		}

		expected = "Static " + ClassGenerator.HELPER_CLASS_NAME + "_V1." + ClassGenerator.HELPER_METHOD_NAME;
		logger.debug("Calling invokeStatic(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}
		logger.debug("Calling invokeSpecial(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}
	}

	public void testInvokestaticInvokespecialInvokeinterface() throws Exception {
		String rc = null;
		String expected = null;
		String testName = "testInvokestaticInvokespecialInvokeinterface";
		Class<?> dummyClass = null;
		Class<?> intfClass = null;
		Method method = null;
		Hashtable<String, Object> characteristics = new Hashtable<String, Object>();
		GeneratedClassLoader classLoader = new GeneratedClassLoader();
		ClassGenerator intfGenerator = new InterfaceGenerator(testName, PKG_NAME);
		ClassGenerator helperClassGenerator = new HelperClassGenerator(testName, PKG_NAME);
		ClassGenerator dummyClassGenerator = new DummyClassGenerator(testName, PKG_NAME);

		characteristics.put(ClassGenerator.VERSION, "V1");
		intfGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(intfGenerator.getClassData());
		intfClass = Class.forName(intfClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V1");
		characteristics.put(ClassGenerator.HELPER_METHOD_TYPE, MethodType.VIRTUAL);
		helperClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(helperClassGenerator.getClassData());
		Class.forName(helperClassName.replace('/', '.'), true, classLoader);

		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V1");
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.EXTENDS_HELPER_CLASS, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] {
						DummyClassGenerator.INVOKE_INTERFACE,
						DummyClassGenerator.INVOKE_SPECIAL,
						DummyClassGenerator.INVOKE_STATIC });
		characteristics.put(ClassGenerator.SHARED_INVOKERS_TARGET, "INTERFACE");
		dummyClassGenerator.setCharacteristics(characteristics);
		classLoader.setClassData(dummyClassGenerator.getClassData());
		dummyClass = Class.forName(dummyClassName.replace('/', '.'), true, classLoader);

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V1." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}
		logger.debug("Calling invokeStatic(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}
		logger.debug("Calling invokeSpecial(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}

		logger.debug("Redefining Interface to V2");
		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V2");
		characteristics.put(ClassGenerator.INTF_HAS_DEFAULT_METHOD, new Boolean(true));
		if (!redefineInterface(intfClass, testName, characteristics)) {
			Assert.fail("Failed to redefine interface");
		}

		expected = ClassGenerator.DUMMY_CLASS_NAME + "_V1." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeInterface(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}

		logger.debug("Calling invokeStatic(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}

		expected = "Default " + ClassGenerator.INTERFACE_NAME + "_V2." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeSpecial(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}

		logger.debug("Redefining Interface to V3");
		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V3");
		characteristics.put(ClassGenerator.INTF_HAS_STATIC_METHOD, new Boolean(true));
		if (!redefineInterface(intfClass, testName, characteristics)) {
			Assert.fail("Failed to redefine interface");
		}

		logger.debug("Calling invokeInterface(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeInterface", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}

		expected = "Static " + ClassGenerator.INTERFACE_NAME + "_V3." + ClassGenerator.INTF_METHOD_NAME;
		logger.debug("Calling invokeStatic(). Expected to return: " + expected);
		method = dummyClass.getMethod("invokeStatic", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			AssertJUnit.assertEquals("Did not get expected value: ", expected, rc);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Did not expect exception to be thrown");
		}
		logger.debug("Calling invokeSpecial(). Expected to throw exception.");
		method = dummyClass.getMethod("invokeSpecial", (Class[])null);
		try {
			rc = (String)method.invoke(dummyClass.newInstance(), (Object[])null);
			logger.debug("Returned: " + rc);
			Assert.fail("Expected to throw exception but returned: " + rc);
		} catch (Exception e) {
			/* exception is expected */
			logger.debug("Expected exception caught");
		}

		logger.debug("Redefining Dummy class to V2");
		characteristics.clear();
		characteristics.put(ClassGenerator.VERSION, "V2");
		characteristics.put(ClassGenerator.IMPLEMENTS_INTERFACE, new Boolean(true));
		characteristics.put(ClassGenerator.EXTENDS_HELPER_CLASS, new Boolean(true));
		characteristics.put(ClassGenerator.SHARED_INVOKERS,
				new String[] {
						DummyClassGenerator.INVOKE_INTERFACE,
						DummyClassGenerator.INVOKE_SPECIAL,
						DummyClassGenerator.INVOKE_STATIC });
		characteristics.put(ClassGenerator.SHARED_INVOKERS_TARGET, "SUPER");
		if (!redefineDummyClass(dummyClass, testName, characteristics)) {
			logger.debug("Expected failure in redefining to dummy class V2");
		} else {
			Assert.fail("Did not expect to be able to redefined to dummy class V2");
		}
	}
}
