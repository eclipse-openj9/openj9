/*
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package org.openj9.test.jep389.downcall;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;

import jdk.incubator.foreign.Addressable;
import jdk.incubator.foreign.CLinker;
import static jdk.incubator.foreign.CLinker.C_CHAR;
import static jdk.incubator.foreign.CLinker.C_DOUBLE;
import static jdk.incubator.foreign.CLinker.C_FLOAT;
import static jdk.incubator.foreign.CLinker.C_INT;
import static jdk.incubator.foreign.CLinker.C_LONG;
import static jdk.incubator.foreign.CLinker.C_LONG_LONG;
import static jdk.incubator.foreign.CLinker.C_POINTER;
import static jdk.incubator.foreign.CLinker.C_SHORT;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.MemoryAccess;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.SymbolLookup;
import jdk.incubator.foreign.ValueLayout;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) for primitive types in downcall.
 *
 * Note: the test suite is intended for the following Clinker API:
 * MethodHandle downcallHandle(MethodType type, FunctionDescriptor function)
 */
@Test(groups = { "level.sanity" })
public class PrimitiveTypeTests3 {
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinOS = osName.contains("win");
	/* long long is 64 bits on AIX/ppc64, which is the same as Windows */
	private static ValueLayout longLayout = (isWinOS || isAixOS) ? C_LONG_LONG : C_LONG;
	private static CLinker clinker = CLinker.getInstance();
	private static ResourceScope resourceScope = ResourceScope.newImplicitScope();
	private static SegmentAllocator allocator = SegmentAllocator.ofScope(resourceScope);

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();
	private static final SymbolLookup defaultLibLookup = CLinker.systemLookup();

	@Test
	public void test_addTwoBoolsWithOr_3() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, boolean.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolsWithOr").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		boolean result = (boolean)mh.invokeExact(functionSymbol, true, false);
		Assert.assertEquals(result, true);
	}

	@Test
	public void test_addBoolAndBoolFromPointerWithOr_3() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolFromPointerWithOr").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);

		MemorySegment boolSegmt = MemorySegment.allocateNative(C_CHAR, resourceScope);
		MemoryAccess.setByte(boolSegmt, (byte)1);
		boolean result = (boolean)mh.invokeExact(functionSymbol, false, boolSegmt.address());
		Assert.assertEquals(result, true);
	}

	@Test
	public void test_addTwoBoolsWithOr_fromMemAddr_3() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, boolean.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolsWithOr").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		boolean result = (boolean)mh.invokeExact(functionSymbol, true, false);
		Assert.assertEquals(result, true);
	}

	@Test
	public void test_generateNewChar_3() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, char.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFrom2Chars").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		char result = (char)mh.invokeExact(functionSymbol, 'B', 'D');
		Assert.assertEquals(result, 'C');
	}

	@Test
	public void test_generateNewCharFromPointer_3() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, MemoryAddress.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);

		MemorySegment shortSegmt = MemorySegment.allocateNative(C_SHORT, resourceScope);
		MemoryAccess.setChar(shortSegmt, 'B');
		char result = (char)mh.invokeExact(functionSymbol, shortSegmt.address(), 'D');
		Assert.assertEquals(result, 'C');
	}

	@Test
	public void test_generateNewChar_fromMemAddr_3() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, char.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFrom2Chars").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		char result = (char)mh.invokeExact(functionSymbol, 'B', 'D');
		Assert.assertEquals(result, 'C');
	}

	@Test
	public void test_addTwoBytes_3() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		byte result = (byte)mh.invokeExact(functionSymbol, (byte)6, (byte)3);
		Assert.assertEquals(result, (byte)9);
	}

	@Test
	public void test_addTwoNegtiveBytes_3() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		byte result = (byte)mh.invokeExact(functionSymbol, (byte)-6, (byte)-3);
		Assert.assertEquals(result, (byte)-9);
	}

	@Test
	public void test_addByteAndByteFromPointer_3() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndByteFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);

		MemorySegment charSegmt = MemorySegment.allocateNative(C_CHAR, resourceScope);
		MemoryAccess.setByte(charSegmt, (byte)3);
		byte result = (byte)mh.invokeExact(functionSymbol, (byte)6, charSegmt.address());
		Assert.assertEquals(result, (byte)9);
	}

	@Test
	public void test_addTwoBytes_fromMemAddr_3() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		byte result = (byte)mh.invokeExact(functionSymbol, (byte)6, (byte)3);
		Assert.assertEquals(result, (byte)9);
	}

	@Test
	public void test_addTwoShorts_3() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		short result = (short)mh.invokeExact(functionSymbol, (short)24, (short)32);
		Assert.assertEquals(result, (short)56);
	}

	@Test
	public void test_addTwoNegtiveShorts_3() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		short result = (short)mh.invokeExact(functionSymbol, (short)-24, (short)-32);
		Assert.assertEquals(result, (short)-56);
	}

	@Test
	public void test_addShortAndShortFromPointer_3() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, MemoryAddress.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);

		MemorySegment shortSegmt = MemorySegment.allocateNative(C_SHORT, resourceScope);
		MemoryAccess.setShort(shortSegmt, (short)24);
		short result = (short)mh.invokeExact(functionSymbol, shortSegmt.address(), (short)32);
		Assert.assertEquals(result, (short)56);
	}

	@Test
	public void test_addTwoShorts_fromMemAddr_3() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		short result = (short)mh.invokeExact(functionSymbol, (short)24, (short)32);
		Assert.assertEquals(result, (short)56);
	}

	@Test
	public void test_addTwoInts_3() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		int result = (int)mh.invokeExact(functionSymbol, 112, 123);
		Assert.assertEquals(result, 235);
	}

	@Test
	public void test_addTwoNegtiveInts_3() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		int result = (int)mh.invokeExact(functionSymbol, -112, -123);
		Assert.assertEquals(result, -235);
	}

	@Test
	public void test_addIntAndIntFromPointer_3() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);

		MemorySegment intSegmt = MemorySegment.allocateNative(C_INT, resourceScope);
		MemoryAccess.setInt(intSegmt, 215);
		int result = (int)mh.invokeExact(functionSymbol, 321, intSegmt.address());
		Assert.assertEquals(result, 536);
	}

	@Test
	public void test_addTwoInts_fromMemAddr_3() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		int result = (int)mh.invokeExact(functionSymbol, 112, 123);
		Assert.assertEquals(result, 235);
	}

	@Test
	public void test_addTwoIntsReturnVoid_3() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsReturnVoid").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		mh.invokeExact(functionSymbol, 454, 398);
	}

	@Test
	public void test_addTwoIntsReturnVoid_fromMemAddr_3() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsReturnVoid").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		mh.invokeExact(functionSymbol, 454, 398);
	}

	@Test
	public void test_addIntAndChar_3() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndChar").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		int result = (int)mh.invokeExact(functionSymbol, 58, 'A');
		Assert.assertEquals(result, 123);
	}

	@Test
	public void test_addIntAndChar_fromMemAddr_3() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndChar").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		int result = (int)mh.invokeExact(functionSymbol, 58, 'A');
		Assert.assertEquals(result, 123);
	}

	@Test
	public void test_addTwoLongs_3() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, long.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, longLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		long result = (long)mh.invokeExact(functionSymbol, 57424L, 698235L);
		Assert.assertEquals(result, 755659L);
	}

	@Test
	public void test_addLongAndLongFromPointer_3() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_POINTER, longLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);

		MemorySegment longSegmt = MemorySegment.allocateNative(longLayout, resourceScope);
		MemoryAccess.setLong(longSegmt, 57424L);
		long result = (long)mh.invokeExact(functionSymbol, longSegmt.address(), 698235L);
		Assert.assertEquals(result, 755659L);
	}

	@Test
	public void test_addTwoLongs_fromMemAddr_3() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, long.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, longLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		long result = (long)mh.invokeExact(functionSymbol, 57424L, 698235L);
		Assert.assertEquals(result, 755659L);
	}

	@Test
	public void test_addTwoFloats_3() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		float result = (float)mh.invokeExact(functionSymbol, 5.74f, 6.79f);
		Assert.assertEquals(result, 12.53f, 0.01f);
	}

	@Test
	public void test_addFloatAndFloatFromPointer_3() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);

		MemorySegment floatSegmt = MemorySegment.allocateNative(C_FLOAT, resourceScope);
		MemoryAccess.setFloat(floatSegmt, 6.79f);
		float result = (float)mh.invokeExact(functionSymbol, 5.74f, floatSegmt.address());
		Assert.assertEquals(result, 12.53f, 0.01f);
	}

	@Test
	public void test_addTwoFloats_fromMemAddr_3() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		float result = (float)mh.invokeExact(functionSymbol, 5.74f, 6.79f);
		Assert.assertEquals(result, 12.53f, 0.01f);
	}

	@Test
	public void test_addTwoDoubles_3() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, double.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		double result = (double)mh.invokeExact(functionSymbol, 159.748d, 262.795d);
		Assert.assertEquals(result, 422.543d, 0.001d);
	}

	@Test
	public void test_addDoubleAndDoubleFromPointer_3() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, MemoryAddress.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_POINTER, C_DOUBLE);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);

		MemorySegment doubleSegmt = MemorySegment.allocateNative(C_DOUBLE, resourceScope);
		MemoryAccess.setDouble(doubleSegmt, 159.748d);
		double result = (double)mh.invokeExact(functionSymbol, doubleSegmt.address(), 262.795d);
		Assert.assertEquals(result, 422.543d, 0.001d);
	}

	@Test
	public void test_addTwoDoubles_fromMemAddr_3() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, double.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		double result = (double)mh.invokeExact(functionSymbol, 159.748d, 262.795d);
		Assert.assertEquals(result, 422.543d, 0.001d);
	}

	@Test
	public void test_strlenFromDefaultLibWithMemAddr_3() throws Throwable {
		Addressable strlenSymbol = defaultLibLookup.lookup("strlen").get();
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_POINTER);
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		MemorySegment funcMemSegment = CLinker.toCString("JEP389 DOWNCALL TEST SUITES", resourceScope);
		long strLength = (long)mh.invokeExact(strlenSymbol, funcMemSegment.address());
		Assert.assertEquals(strLength, 27);
	}

	@Test
	public void test_strlenFromDefaultLibWithMemAddr_fromMemAddr_3() throws Throwable {
		Addressable strlenSymbol = defaultLibLookup.lookup("strlen").get();
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_POINTER);
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		MemorySegment funcMemSegment = CLinker.toCString("JEP389 DOWNCALL TEST SUITES", resourceScope);
		long strLength = (long)mh.invokeExact(strlenSymbol, funcMemSegment.address());
		Assert.assertEquals(strLength, 27);
	}

	@Test
	public void test_memoryAllocFreeFromDefaultLib_3() throws Throwable {
		Addressable allocSymbol = defaultLibLookup.lookup("malloc").get();
		MethodType allocMethodType = MethodType.methodType(MemoryAddress.class, long.class);
		FunctionDescriptor allocFuncDesc = FunctionDescriptor.of(C_POINTER, longLayout);
		MethodHandle allocHandle = clinker.downcallHandle(allocMethodType, allocFuncDesc);
		MemoryAddress allocMemAddr = (MemoryAddress)allocHandle.invokeExact(allocSymbol, 10L);
		MemorySegment memSeg = allocMemAddr.asSegment(10L, resourceScope);
		MemoryAccess.setIntAtOffset(memSeg, 0, 15);
		Assert.assertEquals(MemoryAccess.getIntAtOffset(memSeg, 0), 15);

		Addressable freeSymbol = defaultLibLookup.lookup("free").get();
		MethodType freeMethodType = MethodType.methodType(void.class, MemoryAddress.class);
		FunctionDescriptor freeFuncDesc = FunctionDescriptor.ofVoid(C_POINTER);
		MethodHandle freeHandle = clinker.downcallHandle(freeMethodType, freeFuncDesc);
		freeHandle.invokeExact(freeSymbol, allocMemAddr);
	}

	@Test
	public void test_memoryAllocFreeFromDefaultLib_fromMemAddr_3() throws Throwable {
		Addressable allocSymbol = defaultLibLookup.lookup("malloc").get();
		MemoryAddress allocMemAddrFromSymbol = allocSymbol.address();
		MethodType allocMethodType = MethodType.methodType(MemoryAddress.class, long.class);
		FunctionDescriptor allocFuncDesc = FunctionDescriptor.of(C_POINTER, longLayout);
		MethodHandle allocHandle = clinker.downcallHandle(allocMethodType, allocFuncDesc);
		MemoryAddress allocMemAddr = (MemoryAddress)allocHandle.invokeExact(allocSymbol, 10L);
		MemorySegment memSeg = allocMemAddr.asSegment(10L, resourceScope);
		MemoryAccess.setIntAtOffset(memSeg, 0, 15);
		Assert.assertEquals(MemoryAccess.getIntAtOffset(memSeg, 0), 15);

		Addressable freeSymbol = defaultLibLookup.lookup("free").get();
		MemoryAddress freeMemAddr = freeSymbol.address();
		MethodType freeMethodType = MethodType.methodType(void.class, MemoryAddress.class);
		FunctionDescriptor freeFuncDesc = FunctionDescriptor.ofVoid(C_POINTER);
		MethodHandle freeHandle = clinker.downcallHandle(freeMethodType, freeFuncDesc);
		freeHandle.invokeExact(freeSymbol, allocMemAddr);
	}

	@Test
	public void test_memoryAllocFreeFromCLinkerMethod_3() throws Throwable {
		MemoryAddress allocMemAddr = CLinker.allocateMemory(10L);
		MemorySegment memSeg = allocMemAddr.asSegment(10L, resourceScope);
		MemoryAccess.setIntAtOffset(memSeg, 0, 49);
		Assert.assertEquals(MemoryAccess.getIntAtOffset(memSeg, 0), 49);
		CLinker.freeMemory(allocMemAddr);
	}

	@Test
	public void test_printfFromDefaultLibWithMemAddr_3() throws Throwable {
		Addressable functionSymbol = defaultLibLookup.lookup("printf").get();
		MethodType mt = MethodType.methodType(int.class, MemoryAddress.class, int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_POINTER, C_INT, C_INT, C_INT);
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		MemorySegment formatMemSegment = CLinker.toCString("\n%d + %d = %d\n", resourceScope);
		mh.invoke(functionSymbol, formatMemSegment.address(), 15, 27, 42);
	}

	@Test
	public void test_printfFromDefaultLibWithMemAddr_fromMemAddr_3() throws Throwable {
		Addressable functionSymbol = defaultLibLookup.lookup("printf").get();
		MethodType mt = MethodType.methodType(int.class, MemoryAddress.class, int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_POINTER, C_INT, C_INT, C_INT);
		MethodHandle mh = clinker.downcallHandle(mt, fd);
		MemorySegment formatMemSegment = CLinker.toCString("\n%d + %d = %d\n", resourceScope);
		mh.invoke(functionSymbol, formatMemSegment.address(), 15, 27, 42);
	}
}
