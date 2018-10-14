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
package org.openj9.test.varhandle;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeClass;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.VarHandle;
import java.lang.invoke.VarHandle.AccessMode;
import java.lang.invoke.WrongMethodTypeException;

@Test(groups = {"level.extended"})
public class VarHandleInvokerTest {
	int i;
	VarHandleInvokerTest vhit;
	VarHandle varHandle;

	@BeforeClass
	public void setUp() throws Exception {
		vhit = new VarHandleInvokerTest();
		varHandle = MethodHandles.lookup().findVarHandle(VarHandleInvokerTest.class, "i", int.class);
	}

	@Test
	public void testExactBasic() throws Throwable {
		MethodHandle getHandle = MethodHandles.varHandleExactInvoker(AccessMode.GET, MethodType.methodType(int.class, VarHandleInvokerTest.class));
		MethodHandle setHandle = MethodHandles.varHandleExactInvoker(AccessMode.SET, MethodType.methodType(void.class, VarHandleInvokerTest.class, int.class));
		MethodHandle casHandle = MethodHandles.varHandleExactInvoker(AccessMode.COMPARE_AND_SET, MethodType.methodType(boolean.class, VarHandleInvokerTest.class, int.class, int.class));

		setHandle.invoke(varHandle, vhit, 12345);
		Assert.assertEquals(12345, getHandle.invoke(varHandle, vhit));
		Assert.assertEquals(false, casHandle.invoke(varHandle, vhit, 54321, 111));
		Assert.assertEquals(12345, getHandle.invoke(varHandle, vhit));
		Assert.assertEquals(true, casHandle.invoke(varHandle, vhit, 12345, 54321));
		Assert.assertEquals(54321, getHandle.invoke(varHandle, vhit));

		setHandle.invokeExact(varHandle, vhit, 12345);
		Assert.assertEquals(12345, (int)getHandle.invokeExact(varHandle, vhit));
		Assert.assertEquals(false, (boolean)casHandle.invokeExact(varHandle, vhit, 54321, 111));
		Assert.assertEquals(12345, (int)getHandle.invokeExact(varHandle, vhit));
		Assert.assertEquals(true, (boolean)casHandle.invokeExact(varHandle, vhit, 12345, 54321));
		Assert.assertEquals(54321, (int)getHandle.invokeExact(varHandle, vhit));
	}

	@Test
	public void testGenericBasic() throws Throwable {
		MethodHandle getHandle = MethodHandles.varHandleInvoker(AccessMode.GET, MethodType.methodType(Integer.class, VarHandleInvokerTest.class));
		MethodHandle setHandle = MethodHandles.varHandleInvoker(AccessMode.SET, MethodType.methodType(void.class, VarHandleInvokerTest.class, Integer.class));
		MethodHandle casHandle = MethodHandles.varHandleInvoker(AccessMode.COMPARE_AND_SET, MethodType.methodType(Boolean.class, VarHandleInvokerTest.class, Integer.class, Integer.class));

		setHandle.invoke(varHandle, vhit, new Integer(12345));
		Assert.assertEquals(12345, getHandle.invoke(varHandle, vhit));
		Assert.assertEquals(false, casHandle.invoke(varHandle, vhit, new Integer(54321), new Integer(111)));
		Assert.assertEquals(12345, getHandle.invoke(varHandle, vhit));
		Assert.assertEquals(true, casHandle.invoke(varHandle, vhit, new Integer(12345), new Integer(54321)));
		Assert.assertEquals(54321, getHandle.invoke(varHandle, vhit));

		setHandle.invokeExact(varHandle, vhit, new Integer(12345));
		Assert.assertEquals(new Integer(12345), (Integer)getHandle.invokeExact(varHandle, vhit));
		Assert.assertEquals(new Boolean(false), (Boolean)casHandle.invokeExact(varHandle, vhit, new Integer(54321), new Integer(111)));
		Assert.assertEquals(new Integer(12345), (Integer)getHandle.invokeExact(varHandle, vhit));
		Assert.assertEquals(new Boolean(true), (Boolean)casHandle.invokeExact(varHandle, vhit, new Integer(12345), new Integer(54321)));
		Assert.assertEquals(new Integer(54321), (Integer)getHandle.invokeExact(varHandle, vhit));

		/* Using exact types */
		getHandle = MethodHandles.varHandleInvoker(AccessMode.GET, MethodType.methodType(int.class, VarHandleInvokerTest.class));
		setHandle = MethodHandles.varHandleInvoker(AccessMode.SET, MethodType.methodType(void.class, VarHandleInvokerTest.class, int.class));

		setHandle.invoke(varHandle, vhit, 12345);
		Assert.assertEquals(12345, getHandle.invoke(varHandle, vhit));

		setHandle.invokeExact(varHandle, vhit, 54321);
		Assert.assertEquals(54321, (int)getHandle.invokeExact(varHandle, vhit));
	}

	@Test
	public void testTypeNotVerifiedAtMHCreation() {
		MethodHandle getHandle = MethodHandles.varHandleInvoker(AccessMode.GET, MethodType.methodType(void.class));
		Assert.assertNotNull(getHandle);
		MethodHandle setHandle = MethodHandles.varHandleInvoker(AccessMode.SET, MethodType.methodType(void.class));
		Assert.assertNotNull(setHandle);
		getHandle = MethodHandles.varHandleExactInvoker(AccessMode.GET, MethodType.methodType(void.class));
		Assert.assertNotNull(getHandle);
		setHandle = MethodHandles.varHandleExactInvoker(AccessMode.SET, MethodType.methodType(void.class));
		Assert.assertNotNull(setHandle);
	}

	@Test(expectedExceptions = WrongMethodTypeException.class)
	public void testExactWrongType() throws Throwable {
		MethodHandle setHandle = MethodHandles.varHandleExactInvoker(AccessMode.SET, MethodType.methodType(void.class, VarHandleInvokerTest.class, Integer.class));
		setHandle.invoke(varHandle, vhit, new Integer(12345));
	}

	@Test(expectedExceptions = WrongMethodTypeException.class)
	public void testExactInvokeExactWrongType() throws Throwable {
		MethodHandle setHandle = MethodHandles.varHandleExactInvoker(AccessMode.SET, MethodType.methodType(void.class, VarHandleInvokerTest.class, Integer.class));
		setHandle.invokeExact(varHandle, vhit, new Integer(12345));
	}

	@Test(expectedExceptions = WrongMethodTypeException.class)
	public void testGenericWrongType() throws Throwable {
		MethodHandle setHandle = MethodHandles.varHandleInvoker(AccessMode.SET, MethodType.methodType(void.class, VarHandleInvokerTest.class, String.class));
		setHandle.invoke(varHandle, vhit, "12345");
	}

	@Test(expectedExceptions = WrongMethodTypeException.class)
	public void testGenericInvokeExactWrongType() throws Throwable {
		MethodHandle setHandle = MethodHandles.varHandleInvoker(AccessMode.SET, MethodType.methodType(void.class, VarHandleInvokerTest.class, String.class));
		setHandle.invokeExact(varHandle, vhit, "12345");
	}

	@Test(expectedExceptions = NullPointerException.class)
	public void testExactNPE1() {
		MethodHandle handle = MethodHandles.varHandleExactInvoker(null, MethodType.methodType(void.class, VarHandleInvokerTest.class, Integer.class));
	}

	@Test(expectedExceptions = NullPointerException.class)
	public void testExactNPE2() {
		MethodHandle handle = MethodHandles.varHandleExactInvoker(AccessMode.SET, null);
	}

	@Test(expectedExceptions = NullPointerException.class)
	public void testExactNPE3() throws Throwable {
		MethodHandle setHandle = MethodHandles.varHandleExactInvoker(AccessMode.SET, MethodType.methodType(void.class, VarHandleInvokerTest.class, int.class));
		setHandle.invoke(null, vhit, 12345);
	}

	@Test(expectedExceptions = NullPointerException.class)
	public void testExactInvokeExactNPE3() throws Throwable {
		MethodHandle setHandle = MethodHandles.varHandleExactInvoker(AccessMode.SET, MethodType.methodType(void.class, VarHandleInvokerTest.class, int.class));
		setHandle.invokeExact((VarHandle)null, vhit, 12345);
	}

	@Test(expectedExceptions = NullPointerException.class)
	public void testGenericNPE1() {
		MethodHandle handle = MethodHandles.varHandleInvoker(null, MethodType.methodType(void.class, VarHandleInvokerTest.class, Integer.class));
	}

	@Test(expectedExceptions = NullPointerException.class)
	public void testGenericNPE2() {
		MethodHandle handle = MethodHandles.varHandleInvoker(AccessMode.SET, null);
	}

	@Test(expectedExceptions = NullPointerException.class)
	public void testGenericNPE3() throws Throwable {
		MethodHandle setHandle = MethodHandles.varHandleInvoker(AccessMode.SET, MethodType.methodType(void.class, VarHandleInvokerTest.class, Integer.class));
		setHandle.invoke(null, vhit, new Integer(12345));
	}

	@Test(expectedExceptions = NullPointerException.class)
	public void testGenericInvokeExactNPE3() throws Throwable {
		MethodHandle setHandle = MethodHandles.varHandleInvoker(AccessMode.SET, MethodType.methodType(void.class, VarHandleInvokerTest.class, Integer.class));
		setHandle.invoke((VarHandle)null, vhit, new Integer(12345));
	}
}
