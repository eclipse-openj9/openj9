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
import java.nicl.types.Pointer;
import java.nicl.types.Transformer;

import panama.test.panamatest;
import panama.test.panamatest.*;

/**
 * Test native method handles
 *
 * @author Amy Yu
 *
 */
@Test(groups = { "level.sanity" })
public class CustomFFINativeMethodHandleTest {
	panamatest panama;
	Scope scope;

	@Parameters({ "path" })
	public void setUp(String path) throws Exception {
		Library lib = NativeLibrary.loadLibraryFile(path + "/libpanamatest.so");
		panama = NativeLibrary.bindRaw(panamatest.class, lib);
		scope = new NativeScope();
	}

	@Test
	public void testIntPtr() throws Throwable {
		Pointer<Integer> arr = panama.getIntPtr();
		Assert.assertEquals(1, panama.checkIntPtr(arr));
		panama.freeIntPtr(arr);
	}

	@Test
	public void testStructArray() throws Throwable {
		Pointer<Point> sa = panama.getStructArray();
		Assert.assertEquals(1, panama.checkStructArray(sa));
		panama.freeStructArray(sa);
	}

	@Test
	public void testPoint() throws Throwable {
		Point p1 = panama.getPoint(2, 3);
		Point p2 = panama.getPoint(4, 6);
		Assert.assertEquals(1, panama.checkPoint(p1, 2, 3));
	}

	@Test
	public void testPointPtr() throws Throwable {
		Pointer<Point> pptr = panama.getPointPtr(7, 8);
		Assert.assertEquals(1, panama.checkPointPtr(pptr, 7, 8));
		panama.freePointPtr(pptr);
	}

	@Test
	public void testLine() throws Throwable {
		Point p1 = panama.getPoint(2, 3);
		Point p2 = panama.getPoint(4, 6);
		Line ln = panama.getLine(p1, p2);
		Assert.assertEquals(1, panama.checkLine(ln, p1, p2));
	}

	@Test
	public void testSmallStruct() throws Throwable {
		SmallStruct small = panama.getSmallStruct((byte)4);
		Assert.assertEquals(1, panama.checkSmallStruct(small, (byte)4));
	}

	@Test
	public void testLargeStruct() throws Throwable {
		Point p1 = panama.getPoint(2, 3);
		Point p2 = panama.getPoint(4, 6);
		Line ln = panama.getLine(p1, p2);
		Pointer<Byte> str = Transformer.toCString("abcdfghijk", scope);
		LargeStruct large = panama.getLargeStruct(str, 4, ln.ptr(), p1, 1, 2, 3, 2.5);
		Assert.assertEquals(1, panama.checkLargeStruct(large, str, 4, ln, p1, 1, 2, 3, 2.5));
	}

	@Test
	public void testPadding() throws Throwable {
		Padding pad = panama.getPadding(99999999999999999L, 8, 99999999999999999L);
		Assert.assertEquals(1, panama.checkPadding(pad, 99999999999999999L, 8, 99999999999999999L));
	}

	@Test
	public void testComplex() throws Throwable {
		Complex comp = panama.getComplex(99999999999999999L, 4.6);
		Assert.assertEquals(1, panama.checkComplex(comp, 99999999999999999L, 4.6));
	}

	@Test
	public void testScalarVector() throws Throwable {
		Point p1 = panama.getPoint(2, 3);
		ScalarVector sv = panama.getScalarVector(2, p1);
		Assert.assertEquals(1, panama.checkScalarVector(sv, 2, p1));
	}

	@Test
	public void testPointerVector() throws Throwable {
		Point p1 = panama.getPoint(2, 3);
		Pointer<Byte> str1 = Transformer.toCString("qwerty", scope);
		PointerVector pv = panama.getPointerVector(str1, p1);
		Assert.assertEquals(1, panama.checkPointerVector(pv, str1, p1));
	}

	@Test
	public void testPointerPointer() throws Throwable {
		Pointer<Byte> str1 = Transformer.toCString("qwerty", scope);
		Pointer<Byte> str2 = Transformer.toCString("yuiop", scope);
		PointerPointer pp = panama.getPointerPointer(str1, str2);
		Assert.assertEquals(1, panama.checkPointerPointer(pp, str1, str2));
	}

	@Test
	public void testArrayStruct() throws Throwable {
		Point p1 = panama.getPoint(2, 3);
		Point p2 = panama.getPoint(4, 6);
		Point p3 = panama.getPoint(8, 0);
		Line ln1 = panama.getLine(p1, p2);
		Line ln2 = panama.getLine(p1, p3);
		Line ln3 = panama.getLine(p3, p2);
		ArrayStruct arr = panama.getArrayStruct(ln1, ln2, ln3, 1, 2, 3, 4);
		Assert.assertEquals(1, panama.checkArrayStruct(arr, ln1, ln2, ln3, 1, 2, 3, 4));
	}
}