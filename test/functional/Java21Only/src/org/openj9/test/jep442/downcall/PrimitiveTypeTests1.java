/*
 * Copyright IBM Corp. and others 2023
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.jep442.downcall;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.io.UnsupportedEncodingException;
import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SymbolLookup;
import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_BOOLEAN;
import static java.lang.foreign.ValueLayout.JAVA_BYTE;
import static java.lang.foreign.ValueLayout.JAVA_CHAR;
import static java.lang.foreign.ValueLayout.JAVA_DOUBLE;
import static java.lang.foreign.ValueLayout.JAVA_FLOAT;
import static java.lang.foreign.ValueLayout.JAVA_INT;
import static java.lang.foreign.ValueLayout.JAVA_LONG;
import static java.lang.foreign.ValueLayout.JAVA_SHORT;
import java.lang.invoke.MethodHandle;
import java.util.Arrays;

/**
 * Test cases for JEP 442: Foreign Linker API (Third Preview) for primitive types in downcall.
 *
 * Note: the test suite is intended for the following Clinker API:
 * MethodHandle downcallHandle(MemorySegment symbol, FunctionDescriptor function)
 */
@Test(groups = { "level.sanity" })
public class PrimitiveTypeTests1 {
	private static final boolean isZos = "z/OS".equalsIgnoreCase(System.getProperty("os.name"));
	private static Linker linker = Linker.nativeLinker();
	private static Arena arena = Arena.ofAuto();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();
	private static final SymbolLookup defaultLibLookup = linker.defaultLookup();

	static MemorySegment allocateString(String string) throws UnsupportedEncodingException {
		MemorySegment segment;

		if (isZos) {
			byte[] bytes = string.getBytes("IBM1047");
			byte[] withNul = Arrays.copyOf(bytes, bytes.length + 1);

			segment = arena.allocateArray(JAVA_BYTE, withNul);
		} else {
			segment = arena.allocateUtf8String(string);
		}

		return segment;
	}

	@Test
	public void test_addTwoBoolsWithOr_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolsWithOr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		boolean result = (boolean)mh.invokeExact(true, false);
		Assert.assertEquals(result, true);
	}

	@Test
	public void test_addBoolAndBoolFromPointerWithOr_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolFromPointerWithOr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		MemorySegment boolSegmt = arena.allocate(JAVA_BOOLEAN);
		boolSegmt.set(JAVA_BOOLEAN, 0, true);
		boolean result = (boolean)mh.invoke(false, boolSegmt);
		Assert.assertEquals(result, true);
	}

	@Test
	public void test_generateNewChar_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, JAVA_CHAR);
		MemorySegment functionSymbol = nativeLibLookup.find("createNewCharFrom2Chars").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		char result = (char)mh.invokeExact('B', 'D');
		Assert.assertEquals(result, 'C');
	}

	@Test
	public void test_generateNewCharFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR);
		MemorySegment functionSymbol = nativeLibLookup.find("createNewCharFromCharAndCharFromPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		MemorySegment charSegmt = arena.allocate(JAVA_CHAR, 'B');
		char result = (char)mh.invoke(charSegmt, 'D');
		Assert.assertEquals(result, 'C');
	}

	@Test
	public void test_addTwoBytes_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, JAVA_BYTE);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Bytes").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		byte result = (byte)mh.invokeExact((byte)6, (byte)3);
		Assert.assertEquals(result, (byte)9);
	}

	@Test
	public void test_addTwoNegtiveBytes_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, JAVA_BYTE);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Bytes").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		byte result = (byte)mh.invokeExact((byte)-6, (byte)-3);
		Assert.assertEquals(result, (byte)-9);
	}

	@Test
	public void test_addByteAndByteFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndByteFromPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		MemorySegment byteSegmt = arena.allocate(JAVA_BYTE, (byte)3);
		byte result = (byte)mh.invoke((byte)6, byteSegmt);
		Assert.assertEquals(result, (byte)9);
	}

	@Test
	public void test_addTwoShorts_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, JAVA_SHORT);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Shorts").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		short result = (short)mh.invokeExact((short)24, (short)32);
		Assert.assertEquals(result, (short)56);
	}

	@Test
	public void test_addTwoNegtiveShorts_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, JAVA_SHORT);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Shorts").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		short result = (short)mh.invokeExact((short)-24, (short)-32);
		Assert.assertEquals(result, (short)-56);
	}

	@Test
	public void test_addShortAndShortFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, ADDRESS, JAVA_SHORT);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortFromPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		MemorySegment shortSegmt = arena.allocate(JAVA_SHORT, (short)24);
		short result = (short)mh.invoke(shortSegmt, (short)32);
		Assert.assertEquals(result, (short)56);
	}

	@Test
	public void test_addTwoInts_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Ints").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		int result = (int)mh.invokeExact(112, 123);
		Assert.assertEquals(result, 235);
	}

	@Test
	public void test_addTwoNegtiveInts_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Ints").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		int result = (int)mh.invokeExact(-112, -123);
		Assert.assertEquals(result, -235);
	}

	@Test
	public void test_addIntAndIntFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntFromPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		MemorySegment intSegmt = arena.allocate(JAVA_INT, 215);
		int result = (int)mh.invoke(321, intSegmt);
		Assert.assertEquals(result, 536);
	}

	@Test
	public void test_addTwoIntsReturnVoid_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntsReturnVoid").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		mh.invokeExact(454, 398);
	}

	@Test
	public void test_addIntAndChar_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_CHAR);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndChar").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		int result = (int)mh.invokeExact(58, 'A');
		Assert.assertEquals(result, 123);
	}

	@Test
	public void test_addTwoLongs_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, JAVA_LONG);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Longs").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		long result = (long)mh.invokeExact(57424L, 698235L);
		Assert.assertEquals(result, 755659L);
	}

	@Test
	public void test_addLongAndLongFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongFromPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		MemorySegment longSegmt = arena.allocate(JAVA_LONG, 57424L);
		long result = (long)mh.invoke(longSegmt, 698235L);
		Assert.assertEquals(result, 755659L);
	}

	@Test
	public void test_addTwoFloats_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, JAVA_FLOAT);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Floats").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		float result = (float)mh.invokeExact(5.74f, 6.79f);
		Assert.assertEquals(result, 12.53f, 0.01f);
	}

	@Test
	public void test_addFloatAndFloatFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		MemorySegment floatSegmt = arena.allocate(JAVA_FLOAT, 6.79f);
		float result = (float)mh.invoke(5.74f, floatSegmt);
		Assert.assertEquals(result, 12.53f, 0.01f);
	}

	@Test
	public void test_addTwoDoubles_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, JAVA_DOUBLE);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Doubles").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		double result = (double)mh.invokeExact(159.748d, 262.795d);
		Assert.assertEquals(result, 422.543d, 0.001d);
	}

	@Test
	public void test_addDoubleAndDoubleFromPointer_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, JAVA_DOUBLE);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFromPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		MemorySegment doubleSegmt = arena.allocate(JAVA_DOUBLE, 159.748d);
		double result = (double)mh.invoke(doubleSegmt, 262.795d);
		Assert.assertEquals(result, 422.543d, 0.001d);
	}

	@Test
	public void test_strlenFromDefaultLibWithMemAddr_1() throws Throwable {
		MemorySegment strlenSymbol = defaultLibLookup.find("strlen").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, ADDRESS);
		MethodHandle mh = linker.downcallHandle(strlenSymbol, fd);
		MemorySegment funcSegmt = allocateString("JEP442 DOWNCALL TEST SUITES");
		long strLength = (long)mh.invoke(funcSegmt);
		Assert.assertEquals(strLength, 27);
	}

	@Test
	public void test_memoryAllocFreeFromDefaultLib_1() throws Throwable {
		MemorySegment allocSymbol = defaultLibLookup.find("malloc").get();
		FunctionDescriptor allocFuncDesc = FunctionDescriptor.of(ADDRESS, JAVA_LONG);
		MethodHandle allocHandle = linker.downcallHandle(allocSymbol, allocFuncDesc);
		MemorySegment allocMemAddr = (MemorySegment)allocHandle.invokeExact(10L);
		MemorySegment allocMemSegmt = allocMemAddr.reinterpret(10L);
		allocMemSegmt.set(JAVA_INT, 0, 15);
		Assert.assertEquals(allocMemSegmt.get(JAVA_INT, 0), 15);

		MemorySegment freeSymbol = defaultLibLookup.find("free").get();
		FunctionDescriptor freeFuncDesc = FunctionDescriptor.ofVoid(ADDRESS);
		MethodHandle freeHandle = linker.downcallHandle(freeSymbol, freeFuncDesc);
		freeHandle.invoke(allocMemAddr);
	}

	@Test
	public void test_printfFromDefaultLibWithMemAddr_1() throws Throwable {
		MemorySegment functionSymbol = defaultLibLookup.find("printf").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_INT, JAVA_INT, JAVA_INT);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);
		MemorySegment formatSegmt = allocateString("\n%d + %d = %d\n");
		mh.invoke(formatSegmt, 15, 27, 42);
	}

	@Test
	public void test_printfFromDefaultLibWithMemAddr_LinkerOption_1() throws Throwable {
		MemorySegment functionSymbol = defaultLibLookup.find("printf").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_INT, JAVA_INT, JAVA_INT);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.firstVariadicArg(1));
		MemorySegment formatSegmt = allocateString("\n%d + %d = %d\n");
		mh.invoke(formatSegmt, 15, 27, 42);
	}

	@Test
	public void test_validateTrivialOption_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT);
		MemorySegment functionSymbol = nativeLibLookup.find("validateTrivialOption").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.isTrivial());
		int result = (int)mh.invokeExact(111);
		Assert.assertEquals(result, 111);
	}
}
