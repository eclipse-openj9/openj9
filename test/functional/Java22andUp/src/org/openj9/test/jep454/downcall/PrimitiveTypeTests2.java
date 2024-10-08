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
package org.openj9.test.jep454.downcall;

import org.testng.Assert;
import org.testng.annotations.Test;

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

/**
 * Test cases for JEP 454: Foreign Linker API for primitive types in downcall.
 *
 * Note: the test suite is intended for the following Clinker API:
 * MethodHandle downcallHandle(FunctionDescriptor function)
 */
@Test(groups = { "level.sanity" })
public class PrimitiveTypeTests2 {
	private static Linker linker = Linker.nativeLinker();
	private static Arena arena = Arena.ofAuto();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();
	private static final SymbolLookup defaultLibLookup = linker.defaultLookup();

	@Test
	public void test_addTwoBoolsWithOr_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolsWithOr").get();
		MethodHandle mh = linker.downcallHandle(fd);
		boolean result = (boolean)mh.invokeExact(functionSymbol, true, false);
		Assert.assertEquals(result, true);
	}

	@Test
	public void test_addBoolAndBoolFromPointerWithOr_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolFromPointerWithOr").get();
		MethodHandle mh = linker.downcallHandle(fd);

		MemorySegment boolSegmt = arena.allocate(JAVA_BOOLEAN);
		boolSegmt.set(JAVA_BOOLEAN, 0, true);
		boolean result = (boolean)mh.invoke(functionSymbol, false, boolSegmt);
		Assert.assertEquals(result, true);
	}

	@Test
	public void test_generateNewChar_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, JAVA_CHAR);
		MemorySegment functionSymbol = nativeLibLookup.find("createNewCharFrom2Chars").get();
		MethodHandle mh = linker.downcallHandle(fd);
		char result = (char)mh.invokeExact(functionSymbol, 'B', 'D');
		Assert.assertEquals(result, 'C');
	}

	@Test
	public void test_generateNewCharFromPointer_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR);
		MemorySegment functionSymbol = nativeLibLookup.find("createNewCharFromCharAndCharFromPointer").get();
		MethodHandle mh = linker.downcallHandle(fd);

		MemorySegment charSegmt = arena.allocateFrom(JAVA_CHAR, 'B');
		char result = (char)mh.invoke(functionSymbol, charSegmt, 'D');
		Assert.assertEquals(result, 'C');
	}

	@Test
	public void test_addTwoBytes_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, JAVA_BYTE);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Bytes").get();
		MethodHandle mh = linker.downcallHandle(fd);
		byte result = (byte)mh.invokeExact(functionSymbol, (byte)6, (byte)3);
		Assert.assertEquals(result, (byte)9);
	}

	@Test
	public void test_addTwoNegtiveBytes_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, JAVA_BYTE);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Bytes").get();
		MethodHandle mh = linker.downcallHandle(fd);
		byte result = (byte)mh.invokeExact(functionSymbol, (byte)-6, (byte)-3);
		Assert.assertEquals(result, (byte)-9);
	}

	@Test
	public void test_addByteAndByteFromPointer_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndByteFromPointer").get();
		MethodHandle mh = linker.downcallHandle(fd);

		MemorySegment byteSegmt = arena.allocateFrom(JAVA_BYTE, (byte)3);
		byte result = (byte)mh.invoke(functionSymbol, (byte)6, byteSegmt);
		Assert.assertEquals(result, (byte)9);
	}

	@Test
	public void test_addTwoShorts_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, JAVA_SHORT);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Shorts").get();
		MethodHandle mh = linker.downcallHandle(fd);
		short result = (short)mh.invokeExact(functionSymbol, (short)24, (short)32);
		Assert.assertEquals(result, (short)56);
	}

	@Test
	public void test_addTwoNegtiveShorts_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, JAVA_SHORT);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Shorts").get();
		MethodHandle mh = linker.downcallHandle(fd);
		short result = (short)mh.invokeExact(functionSymbol, (short)-24, (short)-32);
		Assert.assertEquals(result, (short)-56);
	}

	@Test
	public void test_addShortAndShortFromPointer_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, ADDRESS, JAVA_SHORT);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortFromPointer").get();
		MethodHandle mh = linker.downcallHandle(fd);

		MemorySegment shortSegmt = arena.allocateFrom(JAVA_SHORT, (short)24);
		short result = (short)mh.invoke(functionSymbol, shortSegmt, (short)32);
		Assert.assertEquals(result, (short)56);
	}

	@Test
	public void test_addTwoInts_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Ints").get();
		MethodHandle mh = linker.downcallHandle(fd);
		int result = (int)mh.invokeExact(functionSymbol, 112, 123);
		Assert.assertEquals(result, 235);
	}

	@Test
	public void test_addTwoNegtiveInts_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Ints").get();
		MethodHandle mh = linker.downcallHandle(fd);
		int result = (int)mh.invokeExact(functionSymbol, -112, -123);
		Assert.assertEquals(result, -235);
	}

	@Test
	public void test_addIntAndIntFromPointer_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntFromPointer").get();
		MethodHandle mh = linker.downcallHandle(fd);

		MemorySegment intSegmt = arena.allocateFrom(JAVA_INT, 215);
		int result = (int)mh.invoke(functionSymbol, 321, intSegmt);
		Assert.assertEquals(result, 536);
	}

	@Test
	public void test_addTwoIntsReturnVoid_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntsReturnVoid").get();
		MethodHandle mh = linker.downcallHandle(fd);
		mh.invokeExact(functionSymbol, 454, 398);
	}

	@Test
	public void test_addIntAndChar_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_CHAR);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndChar").get();
		MethodHandle mh = linker.downcallHandle(fd);
		int result = (int)mh.invokeExact(functionSymbol, 58, 'A');
		Assert.assertEquals(result, 123);
	}

	@Test
	public void test_addTwoLongs_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, JAVA_LONG);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Longs").get();
		MethodHandle mh = linker.downcallHandle(fd);
		long result = (long)mh.invokeExact(functionSymbol, 57424L, 698235L);
		Assert.assertEquals(result, 755659L);
	}

	@Test
	public void test_addLongAndLongFromPointer_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongFromPointer").get();
		MethodHandle mh = linker.downcallHandle(fd);

		MemorySegment longSegmt = arena.allocateFrom(JAVA_LONG, 57424L);
		long result = (long)mh.invoke(functionSymbol, longSegmt, 698235L);
		Assert.assertEquals(result, 755659L);
	}

	@Test
	public void test_addTwoFloats_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, JAVA_FLOAT);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Floats").get();
		MethodHandle mh = linker.downcallHandle(fd);
		float result = (float)mh.invokeExact(functionSymbol, 5.74f, 6.79f);
		Assert.assertEquals(result, 12.53f, 0.01f);
	}

	@Test
	public void test_addFloatAndFloatFromPointer_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromPointer").get();
		MethodHandle mh = linker.downcallHandle(fd);

		MemorySegment floatSegmt = arena.allocateFrom(JAVA_FLOAT, 6.79f);
		float result = (float)mh.invoke(functionSymbol, 5.74f, floatSegmt);
		Assert.assertEquals(result, 12.53f, 0.01f);
	}

	@Test
	public void test_addTwoDoubles_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, JAVA_DOUBLE);
		MemorySegment functionSymbol = nativeLibLookup.find("add2Doubles").get();
		MethodHandle mh = linker.downcallHandle(fd);
		double result = (double)mh.invokeExact(functionSymbol, 159.748d, 262.795d);
		Assert.assertEquals(result, 422.543d, 0.001d);
	}

	@Test
	public void test_addDoubleAndDoubleFromPointer_2() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, JAVA_DOUBLE);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFromPointer").get();
		MethodHandle mh = linker.downcallHandle(fd);

		MemorySegment doubleSegmt = arena.allocateFrom(JAVA_DOUBLE, 159.748d);
		double result = (double)mh.invoke(functionSymbol, doubleSegmt, 262.795d);
		Assert.assertEquals(result, 422.543d, 0.001d);
	}

	@Test
	public void test_strlenFromDefaultLibWithMemAddr_2() throws Throwable {
		MemorySegment strlenSymbol = defaultLibLookup.find("strlen").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, ADDRESS);
		MethodHandle mh = linker.downcallHandle(fd);
		MemorySegment funcSegmt = PrimitiveTypeTests1.allocateString("JEP454 DOWNCALL TEST SUITES");
		long strLength = (long)mh.invoke(strlenSymbol, funcSegmt);
		Assert.assertEquals(strLength, 27);
	}

	@Test
	public void test_memoryAllocFreeFromDefaultLib_2() throws Throwable {
		MemorySegment allocSymbol = defaultLibLookup.find("malloc").get();
		FunctionDescriptor allocFuncDesc = FunctionDescriptor.of(ADDRESS, JAVA_LONG);
		MethodHandle allocHandle = linker.downcallHandle(allocFuncDesc);
		MemorySegment allocMemAddr = (MemorySegment)allocHandle.invokeExact(allocSymbol, 10L);
		MemorySegment allocMemSegmt = allocMemAddr.reinterpret(10L);
		allocMemSegmt.set(JAVA_INT, 0, 15);
		Assert.assertEquals(allocMemSegmt.get(JAVA_INT, 0), 15);

		MemorySegment freeSymbol = defaultLibLookup.find("free").get();
		FunctionDescriptor freeFuncDesc = FunctionDescriptor.ofVoid(ADDRESS);
		MethodHandle freeHandle = linker.downcallHandle(freeFuncDesc);
		freeHandle.invoke(freeSymbol, allocMemAddr);
	}

	@Test
	public void test_printfFromDefaultLibWithMemAddr_2() throws Throwable {
		MemorySegment functionSymbol = defaultLibLookup.find("printf").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_INT, JAVA_INT, JAVA_INT);
		MethodHandle mh = linker.downcallHandle(fd);
		MemorySegment formatSegmt = PrimitiveTypeTests1.allocateString("\n%d + %d = %d\n");
		mh.invoke(functionSymbol, formatSegmt, 15, 27, 42);
	}

	@Test
	public void test_printfFromDefaultLibWithMemAddr_LinkerOption_2() throws Throwable {
		MemorySegment functionSymbol = defaultLibLookup.find("printf").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, ADDRESS, JAVA_INT, JAVA_INT, JAVA_INT);
		MethodHandle mh = linker.downcallHandle(fd, Linker.Option.firstVariadicArg(1));
		MemorySegment formatSegmt = PrimitiveTypeTests1.allocateString("\n%d + %d = %d\n");
		mh.invoke(functionSymbol, formatSegmt, 15, 27, 42);
	}
}
