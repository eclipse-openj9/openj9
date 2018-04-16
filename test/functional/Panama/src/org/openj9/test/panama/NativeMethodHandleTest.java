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
package org.openj9.test.panama;


import org.testng.annotations.*;
import org.testng.Assert;

import java.lang.invoke.*;
import java.nicl.*;

/**
 * Test native method handles
 *
 * @author Amy Yu
 *
 */
@Test(groups = { "level.sanity" })
public class NativeMethodHandleTest {
	Library lib;

	@Parameters({ "path" })
	public void setUp(String path) throws Exception {
		lib = NativeLibrary.loadLibraryFile(path + "/libpanamatest.so");
	}

	@Test
	public void testNoLib() throws Throwable {
		/* C Standard Library */
		MethodHandle mh = MethodHandles.lookup().findNative("abs", MethodType.methodType(int.class, int.class));
		int result = (int)mh.invoke(-4);
		Assert.assertEquals(4, result);
	}

	@Test
	public void testByte() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findNative(lib, "addTwoByte", MethodType.methodType(byte.class, byte.class, byte.class));
		byte result = (byte)mh.invoke((byte)2,(byte)3);
		Assert.assertEquals(5, result);
	}

	@Test
	public void testChar() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findNative(lib, "addTwoChar", MethodType.methodType(char.class, char.class, char.class));
		char result = (char)mh.invoke('a','b');
		Assert.assertEquals('c', result);
	}

	@Test
	public void testDouble() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findNative(lib, "addTwoDouble", MethodType.methodType(double.class, double.class, double.class));
		double result = (double)mh.invoke(2.1,3.1);
		Assert.assertEquals(5.2, result);
	}

	@Test
	public void testFloat() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findNative(lib, "addTwoFloat", MethodType.methodType(float.class, float.class, float.class));
		float result = (float)mh.invoke(2.1f,3.1f);
		Assert.assertEquals(5.2f, result);
	}

	@Test
	public void testInt() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findNative(lib, "addTwoInt", MethodType.methodType(int.class, int.class, int.class));
		int result = (int)mh.invoke(2,3);
		Assert.assertEquals(5, result);
	}

	@Test
	public void testLong() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findNative(lib, "addTwoLong", MethodType.methodType(long.class, long.class, long.class));
		long result = (long)mh.invoke(200000000000000L, 300000000000000L);
		Assert.assertEquals(500000000000000L, result);
	}

	@Test
	public void testShort() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findNative(lib, "addTwoShort", MethodType.methodType(short.class, short.class, short.class));
		short result = (short)mh.invoke((short)2,(short)3);
		Assert.assertEquals(5, result);
	}

	@Test
	public void testBoolean() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findNative(lib, "testBoolean", MethodType.methodType(boolean.class, boolean.class));
		boolean result = (boolean)mh.invoke(false);
		Assert.assertEquals(true, result);
	}

	@Test
	public void testNoArgs() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findNative(lib, "returnFour", MethodType.methodType(int.class));
		int result = (int)mh.invoke();
		Assert.assertEquals(4, result);
	}

	@Test
	public void testVoidWithArgs() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findNative(lib, "voidWithArgs", MethodType.methodType(void.class, int.class));
		mh.invoke(2);
	}

	@Test
	public void testVoidNoArgs() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findNative(lib, "voidNoArgs", MethodType.methodType(void.class));
		mh.invoke();
	}

	@Test
	public void testManyArgs() throws Throwable {
		MethodHandle mh = MethodHandles.lookup().findNative(lib, "manyArgs", MethodType.methodType(double.class, int.class, float.class, long.class, double.class));
		double result = (double)mh.invoke(1, 2.5f, 3L, 4.5);
		Assert.assertEquals(13.2545, result);
	}
}