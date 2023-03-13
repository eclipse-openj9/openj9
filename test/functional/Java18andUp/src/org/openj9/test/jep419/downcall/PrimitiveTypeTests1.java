/*******************************************************************************
 * Copyright IBM Corp. and others 2021
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package org.openj9.test.jep419.downcall;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;

import java.lang.invoke.MethodHandle;
import jdk.incubator.foreign.CLinker;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.NativeSymbol;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.SymbolLookup;
import static jdk.incubator.foreign.ValueLayout.*;

/**
 * Test cases for JEP 419: Foreign Linker API (Second Incubator) for primitive types in downcall.
 *
 * Note: the test suite is intended for the following Clinker API:
 * MethodHandle downcallHandle(NativeSymbol symbol, FunctionDescriptor function)
 */
@Test(groups = { "level.sanity" })
public class PrimitiveTypeTests1 {
	private static CLinker clinker = CLinker.systemCLinker();
	private static ResourceScope resourceScope = ResourceScope.newImplicitScope();
	private static SegmentAllocator nativeAllocator = SegmentAllocator.nativeAllocator(resourceScope);

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addTwoBoolsWithOr_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2BoolsWithOr").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		boolean result = (boolean)mh.invokeExact(true, false);
		Assert.assertEquals(result, true);
	}

	@Test
	public void test_addBoolAndBoolFromPointerWithOr_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolFromPointerWithOr").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		MemorySegment boolSegmt = MemorySegment.allocateNative(JAVA_BOOLEAN, resourceScope);
		boolSegmt.set(JAVA_BOOLEAN, 0, true);
		boolean result = (boolean)mh.invoke(false, boolSegmt);
		Assert.assertEquals(result, true);
	}

	@Test
	public void test_generateNewChar_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, JAVA_CHAR);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("createNewCharFrom2Chars").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		char result = (char)mh.invokeExact('B', 'D');
		Assert.assertEquals(result, 'C');
	}

	@Test
	public void test_generateNewCharFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		MemorySegment charSegmt = nativeAllocator.allocate(JAVA_CHAR, 'B');
		char result = (char)mh.invoke(charSegmt, 'D');
		Assert.assertEquals(result, 'C');
	}

	@Test
	public void test_addTwoBytes_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, JAVA_BYTE);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		byte result = (byte)mh.invokeExact((byte)6, (byte)3);
		Assert.assertEquals(result, (byte)9);
	}

	@Test
	public void test_addTwoNegtiveBytes_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, JAVA_BYTE);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		byte result = (byte)mh.invokeExact((byte)-6, (byte)-3);
		Assert.assertEquals(result, (byte)-9);
	}

	@Test
	public void test_addByteAndByteFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndByteFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		MemorySegment byteSegmt = nativeAllocator.allocate(JAVA_BYTE, (byte)3);
		byte result = (byte)mh.invoke((byte)6, byteSegmt);
		Assert.assertEquals(result, (byte)9);
	}

	@Test
	public void test_addTwoShorts_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, JAVA_SHORT);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		short result = (short)mh.invokeExact((short)24, (short)32);
		Assert.assertEquals(result, (short)56);
	}

	@Test
	public void test_addTwoNegtiveShorts_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, JAVA_SHORT);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		short result = (short)mh.invokeExact((short)-24, (short)-32);
		Assert.assertEquals(result, (short)-56);
	}

	@Test
	public void test_addShortAndShortFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, ADDRESS, JAVA_SHORT);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		MemorySegment shortSegmt = nativeAllocator.allocate(JAVA_SHORT, (short)24);
		short result = (short)mh.invoke(shortSegmt, (short)32);
		Assert.assertEquals(result, (short)56);
	}

	@Test
	public void test_addTwoInts_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		int result = (int)mh.invokeExact(112, 123);
		Assert.assertEquals(result, 235);
	}

	@Test
	public void test_addTwoNegtiveInts_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		int result = (int)mh.invokeExact(-112, -123);
		Assert.assertEquals(result, -235);
	}

	@Test
	public void test_addIntAndIntFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		MemorySegment intSegmt = nativeAllocator.allocate(JAVA_INT, 215);
		int result = (int)mh.invoke(321, intSegmt);
		Assert.assertEquals(result, 536);
	}

	@Test
	public void test_addTwoIntsReturnVoid_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2IntsReturnVoid").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		mh.invokeExact(454, 398);
	}

	@Test
	public void test_addIntAndChar_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_CHAR);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndChar").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		int result = (int)mh.invokeExact(58, 'A');
		Assert.assertEquals(result, 123);
	}

	@Test
	public void test_addTwoLongs_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, JAVA_LONG);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		long result = (long)mh.invokeExact(57424L, 698235L);
		Assert.assertEquals(result, 755659L);
	}

	@Test
	public void test_addLongAndLongFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		MemorySegment longSegmt = nativeAllocator.allocate(JAVA_LONG, 57424L);
		long result = (long)mh.invoke(longSegmt, 698235L);
		Assert.assertEquals(result, 755659L);
	}

	@Test
	public void test_addTwoFloats_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, JAVA_FLOAT);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		float result = (float)mh.invokeExact(5.74f, 6.79f);
		Assert.assertEquals(result, 12.53f, 0.01f);
	}

	@Test
	public void test_addFloatAndFloatFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		MemorySegment floatSegmt = nativeAllocator.allocate(JAVA_FLOAT, 6.79f);
		float result = (float)mh.invoke(5.74f, floatSegmt);
		Assert.assertEquals(result, 12.53f, 0.01f);
	}

	@Test
	public void test_addTwoDoubles_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, JAVA_DOUBLE);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		double result = (double)mh.invokeExact(159.748d, 262.795d);
		Assert.assertEquals(result, 422.543d, 0.001d);
	}

	@Test
	public void test_addDoubleAndDoubleFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, JAVA_DOUBLE);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		MemorySegment doubleSegmt = nativeAllocator.allocate(JAVA_DOUBLE, 159.748d);
		double result = (double)mh.invoke(doubleSegmt, 262.795d);
		Assert.assertEquals(result, 422.543d, 0.001d);
	}

	@Test
	public void test_strlenFromDefaultLibWithMemAddr_1() throws Throwable {
		NativeSymbol strlenSymbol = clinker.lookup("strlen").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, ADDRESS);
		MethodHandle mh = clinker.downcallHandle(strlenSymbol, fd);
		MemorySegment funcSegmt = nativeAllocator.allocateUtf8String("JEP419 DOWNCALL TEST SUITES");
		long strLength = (long)mh.invoke(funcSegmt);
		Assert.assertEquals(strLength, 27);
	}

	@Test
	public void test_memoryAllocFreeFromDefaultLib_1() throws Throwable {
		NativeSymbol allocSymbol = clinker.lookup("malloc").get();
		FunctionDescriptor allocFuncDesc = FunctionDescriptor.of(ADDRESS, JAVA_LONG);
		MethodHandle allocHandle = clinker.downcallHandle(allocSymbol, allocFuncDesc);
		MemoryAddress allocMemAddr = (MemoryAddress)allocHandle.invokeExact(10L);
		allocMemAddr.set(JAVA_INT, 0, 15);
		Assert.assertEquals(allocMemAddr.get(JAVA_INT, 0), 15);

		NativeSymbol freeSymbol = clinker.lookup("free").get();
		FunctionDescriptor freeFuncDesc = FunctionDescriptor.ofVoid(ADDRESS);
		MethodHandle freeHandle = clinker.downcallHandle(freeSymbol, freeFuncDesc);
		freeHandle.invoke(allocMemAddr);
	}

	@Test
	public void test_printfFromDefaultLibWithMemAddr_1() throws Throwable {
		NativeSymbol functionSymbol = clinker.lookup("printf").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_INT, JAVA_INT, JAVA_INT);
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);
		MemorySegment formatSegmt = nativeAllocator.allocateUtf8String("\n%d + %d = %d\n");
		mh.invoke(formatSegmt, 15, 27, 42);
	}
}
