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

import org.openj9.test.java_lang_invoke.helpers.Jep334MHHelper;
import org.openj9.test.java_lang_invoke.helpers.Jep334MHHelperImpl;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import java.lang.constant.MethodHandleDesc;
import java.lang.invoke.MethodType;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandleInfo;
import java.lang.invoke.MethodHandles;
import java.util.Optional;

/**
 * This test Java.lang.invoke.MethodHandle API added in Java 12 and later version.
 * - describeConstable: one test for each constant in DirectMethodHandleDesc.Kind to make sure each type is supported
 *
 */
public class Test_MethodHandle {
    public static Logger logger = Logger.getLogger(Test_MethodHandle.class);

	private static MethodType methodType = MethodType.methodType(void.class);
	private static MethodHandle constructorHandle = null;
	private static MethodHandle constructorWithArgsHandle = null;
	private static MethodHandle getterHandle = null;
	private static MethodHandle interfaceSpecialHandle = null;
	private static MethodHandle interfaceStaticHandle = null;
	private static MethodHandle interfaceVirtualHandle = null;
	private static MethodHandle setterHandle = null;
	private static MethodHandle specialHandle = null;
	private static MethodHandle staticHandle = null;
	private static MethodHandle staticGetterHandle = null;
	private static MethodHandle staticSetterHandle = null;
	private static MethodHandle virtualHandle = null;
	private static MethodHandles.Lookup lookup = null;
	private static MethodHandles.Lookup specialLookup = null;

	/* helper for special test */
	private static MethodHandle superHandle = null;

	static {
		lookup = MethodHandles.lookup();
		specialLookup = Jep334MHHelperImpl.lookup();
		try {
			/* constructor handle */
			constructorHandle = lookup.findConstructor(Jep334MHHelperImpl.class, methodType);
			constructorWithArgsHandle = lookup.findConstructor(Jep334MHHelperImpl.class, MethodType.methodType(void.class, String.class, int.class));
			
			/* field handles */
			getterHandle = lookup.findGetter(Jep334MHHelperImpl.class, "getterTest", int.class);
			setterHandle = lookup.findSetter(Jep334MHHelperImpl.class, "setterTest", int.class);
			staticGetterHandle = lookup.findStaticGetter(Jep334MHHelperImpl.class, "staticGetterTest", int.class);
			staticSetterHandle = lookup.findStaticSetter(Jep334MHHelperImpl.class, "staticSetterTest", int.class);
			
			/* direct handles */
			staticHandle = lookup.findStatic(Jep334MHHelperImpl.class, "staticTest", methodType);
			interfaceStaticHandle = lookup.findStatic(Jep334MHHelperImpl.class, "staticTest", methodType);
			specialHandle = specialLookup.findSpecial(Jep334MHHelperImpl.class, "specialTest", methodType, Jep334MHHelperImpl.class);
			interfaceSpecialHandle = specialLookup.findSpecial(Jep334MHHelper.class, "specialTest", methodType, Jep334MHHelperImpl.class);

			/* indirect handles */
			interfaceVirtualHandle = lookup.findVirtual(Jep334MHHelper.class, "virtualTest", methodType);
			virtualHandle = lookup.findVirtual(Jep334MHHelperImpl.class, "virtualTest", methodType);
		} catch(Throwable e) {
			Assert.fail("testMethodHandleDescribeConstable: " + e.toString());
		}
	}

	/*
	 * Test Java 12 API MethodHandle.describeConstable() for constructor types
	 */
	@Test(groups = { "level.sanity" })
	public void testMethodHandleDescribeConstableConstructor() throws Throwable {
		testMethodDescribeConstableGeneral("testMethodHandleDescribeConstableConstructor for constructor", constructorHandle);
		testMethodDescribeConstableGeneral("testMethodHandleDescribeConstableConstructor for constructor with arguments", constructorWithArgsHandle);
	}

	/*
	 * Test Java 12 API MethodHandle.describeConstable() for field types
	 */
	@Test(groups = { "level.sanity" })
	public void testMethodHandleDescribeConstableField() throws Throwable {
		testMethodDescribeConstableGeneral("testMethodHandleDescribeConstableField for getter", getterHandle);
		testMethodDescribeConstableGeneral("testMethodHandleDescribeConstableField for setter", setterHandle);
		testMethodDescribeConstableGeneral("testMethodHandleDescribeConstableField for static_getter", staticGetterHandle);
		testMethodDescribeConstableGeneral("testMethodHandleDescribeConstableField for static_setter", staticSetterHandle);
	}

	/*
	 * Test Java 12 API MethodHandle.describeConstable() for indirect handle types
	 */
	@Test(groups = { "level.sanity" })
	public void testMethodHandleDescribeConstableIndirect() throws Throwable {
		testMethodDescribeConstableGeneral("testMethodHandleDescribeConstableIndirect for virtual", virtualHandle);
		testMethodDescribeConstableGeneral("testMethodHandleDescribeConstableIndirect for interface_virtual", interfaceVirtualHandle);
	}

	/*
	 * Test Java 12 API MethodHandle.describeConstable() for direct handle types
	 */
	@Test(groups = { "level.sanity" })
	public void testMethodHandleDescribeConstableDirect() throws Throwable {
		testMethodDescribeConstableGeneral("testMethodHandleDescribeConstableDirect for static", staticHandle);
		testMethodDescribeConstableGeneral("testMethodHandleDescribeConstableDirect for interface_static", interfaceStaticHandle);
		testMethodDescribeConstableSpecial("testMethodHandleDescribeConstableDirect for special", specialHandle);
		testMethodDescribeConstableSpecial("testMethodHandleDescribeConstableDirect for interface_special", interfaceSpecialHandle);
	}
	
	private void testMethodDescribeConstableGeneral(String testName, MethodHandle handle) throws Throwable {
		testMethodDescribeConstableGeneralSub(testName, handle, lookup);
	}

	private void testMethodDescribeConstableSpecial(String testName, MethodHandle handle) throws Throwable {
		testMethodDescribeConstableGeneralSub(testName, handle, specialLookup);
	}

	private void testMethodDescribeConstableGeneralSub(String testName, MethodHandle handle, MethodHandles.Lookup resolveLookup) throws Throwable {
		MethodHandleDesc handleDesc = handle.describeConstable().orElseThrow();

		/* verify that descriptor can be resolved. If not ReflectiveOperationException will be thrown. */
		MethodHandle resolvedHandle = (MethodHandle)handleDesc.resolveConstantDesc(resolveLookup);

		/* verify that resolved MethodType is the same as original */
		MethodHandleInfo oInfo = MethodHandles.lookup().revealDirect(handle);
		MethodHandleInfo rInfo = MethodHandles.lookup().revealDirect(resolvedHandle);

		logger.debug(testName + ": Descriptor of original MethodHandle " + oInfo.getMethodType().descriptorString() 
			+ " descriptor of MethodHandleDesc is: " + rInfo.getMethodType().descriptorString());

		Assert.assertTrue(oInfo.getMethodType().equals(rInfo.getMethodType()));
		Assert.assertTrue(oInfo.getDeclaringClass().equals(rInfo.getDeclaringClass()));
		Assert.assertTrue(oInfo.getName().equals(rInfo.getName()));
		Assert.assertEquals(oInfo.getModifiers(), rInfo.getModifiers());
		Assert.assertEquals(oInfo.getReferenceKind(), rInfo.getReferenceKind());
	}

}