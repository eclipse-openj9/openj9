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

import panama.test.panamatest;

/**
 * Test native method handles
 *
 * @author Amy Yu
 *
 */
@Test(groups = { "level.sanity" })
public class HigherLevelNativeMethodHandleTest {
	panamatest panama;

	@Parameters({ "path" })
	public void setUp(String path) throws Exception {
		Library lib = NativeLibrary.loadLibraryFile(path + "/libpanamatest.so");
		panama = NativeLibrary.bindRaw(panamatest.class, lib);
	}

	@Test
	public void testByte() throws Throwable {
		byte resultByte = panama.addTwoByte((byte)2,(byte)3);
		Assert.assertEquals(5, resultByte);
	}

	@Test
	public void testDouble() throws Throwable {
		double resultDouble = panama.addTwoDouble(2.1,3.1);
		Assert.assertEquals(5.2, resultDouble);
	}

	@Test
	public void testFloat() throws Throwable {
		float resultFloat = panama.addTwoFloat(2.1f,3.1f);
		Assert.assertEquals(5.2f, resultFloat);
	}

	@Test
	public void testInt() throws Throwable {
		int resultInt = panama.addTwoInt(2,3);
		Assert.assertEquals(5, resultInt);
	}

	@Test
	public void testLong() throws Throwable {
		long resultLong = panama.addTwoLong(200000000000000L, 300000000000000L);
		Assert.assertEquals(500000000000000L, resultLong);
	}

	@Test
	public void testShort() throws Throwable {
		short resultShort = panama.addTwoShort((short)2,(short)3);
		Assert.assertEquals(5, resultShort);
	}

	@Test
	public void testNoArgs() throws Throwable {
		int resultFour = panama.returnFour();
		Assert.assertEquals(4, resultFour);
	}

	@Test
	public void testVoidWithArgs() throws Throwable {
		panama.voidWithArgs(2);
	}

	@Test
	public void testVoidNoArgs() throws Throwable {
		panama.voidNoArgs();
	}

	@Test
	public void testManyArgs() throws Throwable {
		double resultMany = panama.manyArgs(1, 2.5f, 3L, 4.5);
		Assert.assertEquals(13.2545, resultMany);
	}
}