/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
package org.openj9.test.jep389.downcall;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import jdk.incubator.foreign.CLinker;
import static jdk.incubator.foreign.CLinker.*;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.ValueLayout;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemoryAccess;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.LibraryLookup;
import jdk.incubator.foreign.NativeScope;
import static jdk.incubator.foreign.LibraryLookup.Symbol;
import static jdk.incubator.foreign.CLinker.VaList.Builder;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) DownCall for primitive types,
 * which covers generic tests, tests with the void type, the MemoryAddress type, and the vararg list.
 */
@Test(groups = { "level.sanity" })
public class PrimitiveTypeTests {
	private static boolean isWinOS = System.getProperty("os.name").toLowerCase().contains("win");
	private static ValueLayout longLayout = isWinOS ? C_LONG_LONG : C_LONG;
	private static LibraryLookup nativeLib = LibraryLookup.ofLibrary("clinkerffitests");
	private static LibraryLookup defaultLib = LibraryLookup.ofDefault();
	private static CLinker clinker = CLinker.getInstance();
	
	@Test
	public void test_addTwoBoolsWithOr() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, boolean.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2BoolsWithOr").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		boolean result = (boolean)mh.invokeExact(true, false);
		Assert.assertEquals(result, true);
	}
	
	@Test
	public void test_addBoolAndBoolFromPointerWithOr() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER);
		Symbol functionSymbol = nativeLib.lookup("addBoolAndBoolFromPointerWithOr").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		
		MemorySegment intSegmt = MemorySegment.allocateNative(C_INT);
		MemoryAccess.setInt(intSegmt, 1);
		boolean result = (boolean)mh.invokeExact(false, intSegmt.address());
		intSegmt.close();
		Assert.assertEquals(result, true);
	}
	
	@Test
	public void test_addTwoBoolsWithOr_fromMemAddr() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, boolean.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2BoolsWithOr").get();
		MemoryAddress memAddr = functionSymbol.address();
		MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
		boolean result = (boolean)mh.invokeExact(true, false);
		Assert.assertEquals(result, true);
	}
	
	@Test
	public void test_generateNewChar() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, char.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("createNewCharFrom2Chars").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		char result = (char)mh.invokeExact('B', 'D');
		Assert.assertEquals(result, 'C');
	}
	
	@Test
	public void test_generateNewCharFromPointer() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, MemoryAddress.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("createNewCharFromCharAndCharFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		
		MemorySegment shortSegmt = MemorySegment.allocateNative(C_SHORT);
		MemoryAccess.setChar(shortSegmt, 'B');
		char result = (char)mh.invokeExact(shortSegmt.address(), 'D');
		shortSegmt.close();
		Assert.assertEquals(result, 'C');
	}
	
	@Test
	public void test_generateNewChar_fromMemAddr() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, char.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("createNewCharFrom2Chars").get();
		MemoryAddress memAddr = functionSymbol.address();
		MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
		char result = (char)mh.invokeExact('B', 'D');
		Assert.assertEquals(result, 'C');
	}
	
	@Test
	public void test_addTwoBytes() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Symbol functionSymbol = nativeLib.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		byte result = (byte)mh.invokeExact((byte)6, (byte)3);
		Assert.assertEquals(result, (byte)9);
	}
	
	@Test
	public void test_addTwoNegtiveBytes() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Symbol functionSymbol = nativeLib.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		byte result = (byte)mh.invokeExact((byte)-6, (byte)-3);
		Assert.assertEquals(result, (byte)-9);
	}
	
	@Test
	public void test_addByteAndByteFromPointer() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER);
		Symbol functionSymbol = nativeLib.lookup("addByteAndByteFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		
		MemorySegment charSegmt = MemorySegment.allocateNative(C_CHAR);
		MemoryAccess.setByte(charSegmt, (byte)3);
		byte result = (byte)mh.invokeExact((byte)6, charSegmt.address());
		charSegmt.close();
		Assert.assertEquals(result, (byte)9);
	}
	
	@Test
	public void test_addTwoBytes_fromMemAddr() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Symbol functionSymbol = nativeLib.lookup("add2Bytes").get();
		MemoryAddress memAddr = functionSymbol.address();
		MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
		byte result = (byte)mh.invokeExact((byte)6, (byte)3);
		Assert.assertEquals(result, (byte)9);
	}
	
	@Test
	public void test_addTwoShorts() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		short result = (short)mh.invokeExact((short)24, (short)32);
		Assert.assertEquals(result, (short)56);
	}
	
	@Test
	public void test_addTwoNegtiveShorts() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		short result = (short)mh.invokeExact((short)-24, (short)-32);
		Assert.assertEquals(result, (short)-56);
	}
	
	@Test
	public void test_addShortAndShortFromPointer() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, MemoryAddress.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("addShortAndShortFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		
		MemorySegment shortSegmt = MemorySegment.allocateNative(C_SHORT);
		MemoryAccess.setShort(shortSegmt, (short)24);
		short result = (short)mh.invokeExact(shortSegmt.address(), (short)32);
		Assert.assertEquals(result, (short)56);
	}
	
	@Test
	public void test_addTwoShorts_fromMemAddr() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("add2Shorts").get();
		MemoryAddress memAddr = functionSymbol.address();
		MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
		short result = (short)mh.invokeExact((short)24, (short)32);
		Assert.assertEquals(result, (short)56);
	}
	
	@Test
	public void test_addTwoInts() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		int result = (int)mh.invokeExact(112, 123);
		Assert.assertEquals(result, 235);
	}
	
	@Test
	public void test_addTwoNegtiveInts() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		int result = (int)mh.invokeExact(-112, -123);
		Assert.assertEquals(result, -235);
	}
	
	@Test
	public void test_addIntAndIntFromPointer() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		
		MemorySegment intSegmt = MemorySegment.allocateNative(C_INT);
		MemoryAccess.setInt(intSegmt, 215);
		int result = (int)mh.invokeExact(321, intSegmt.address());
		intSegmt.close();
		Assert.assertEquals(result, 536);
	}
	
	@Test
	public void test_addTwoInts_fromMemAddr() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MemoryAddress memAddr = functionSymbol.address();
		MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
		int result = (int)mh.invokeExact(112, 123);
		Assert.assertEquals(result, 235);
	}
	
	@Test
	public void test_addIntsWithVaList() throws Throwable {
		Symbol functionSymbol = nativeLib.lookup("addIntsFromVaList").get();
		MethodType mt = MethodType.methodType(int.class, int.class, VaList.class); 
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_VA_LIST);
		NativeScope nativeScope = NativeScope.unboundedScope();
		VaList vaList = CLinker.VaList.make(vaListBuilder -> vaListBuilder.vargFromInt(C_INT, 700)
				.vargFromInt(C_INT, 800)
				.vargFromInt(C_INT, 900)
				.vargFromInt(C_INT, 1000), nativeScope);
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		int result = (int)mh.invoke(4, vaList);
		Assert.assertEquals(result, 3400);
	}
	
	@Test
	public void test_addTwoIntsReturnVoid() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2IntsReturnVoid").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		mh.invokeExact(454, 398);
	}
	
	@Test
	public void test_addTwoIntsReturnVoid_fromMemAddr() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2IntsReturnVoid").get();
		MemoryAddress memAddr = functionSymbol.address();
		MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
		mh.invokeExact(454, 398);
	}
	
	@Test
	public void test_addIntAndChar() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("addIntAndChar").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		int result = (int)mh.invokeExact(58, 'A');
		Assert.assertEquals(result, 123);
	}
	
	@Test
	public void test_addIntAndChar_fromMemAddr() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("addIntAndChar").get();
		MemoryAddress memAddr = functionSymbol.address();
		MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
		int result = (int)mh.invokeExact(58, 'A');
		Assert.assertEquals(result, 123);
	}
	
	@Test
	public void test_addTwoLongs() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, long.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, longLayout);
		Symbol functionSymbol = nativeLib.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		long result = (long)mh.invokeExact(57424L, 698235L);
		Assert.assertEquals(result, 755659L);
	}
	
	@Test
	public void test_addLongAndLongFromPointer() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_POINTER, longLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongAndLongFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		
		MemorySegment longSegmt = MemorySegment.allocateNative(longLayout);
		MemoryAccess.setLong(longSegmt, 57424L);
		long result = (long)mh.invokeExact(longSegmt.address(), 698235L);
		longSegmt.close();
		Assert.assertEquals(result, 755659L);
	}
	
	@Test
	public void test_addTwoLongs_fromMemAddr() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, long.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, longLayout);
		Symbol functionSymbol = nativeLib.lookup("add2Longs").get();
		MemoryAddress memAddr = functionSymbol.address();
		MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
		long result = (long)mh.invokeExact(57424L, 698235L);
		Assert.assertEquals(result, 755659L);
	}
	
	@Test
	public void test_addLongsWithVaList() throws Throwable {
		Symbol functionSymbol = nativeLib.lookup("addLongsFromVaList").get();
		MethodType mt = MethodType.methodType(long.class, int.class, VaList.class); 
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_INT, C_VA_LIST);
		NativeScope nativeScope = NativeScope.unboundedScope();
		VaList vaList = CLinker.VaList.make(vaListBuilder -> vaListBuilder.vargFromLong(longLayout, 700000L)
				.vargFromLong(longLayout, 800000L)
				.vargFromLong(longLayout, 900000L)
				.vargFromLong(longLayout, 1000000L), nativeScope);
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		long result = (long)mh.invoke(4, vaList);
		Assert.assertEquals(result, 3400000L);
	}
	
	@Test
	public void test_addTwoFloats() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT);
		Symbol functionSymbol = nativeLib.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		float result = (float)mh.invokeExact(5.74f, 6.79f);
		Assert.assertEquals(result, 12.53f, 0.01f);
	}
	
	@Test
	public void test_addFloatAndFloatFromPointer() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER);
		Symbol functionSymbol = nativeLib.lookup("addFloatAndFloatFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		
		MemorySegment floatSegmt = MemorySegment.allocateNative(C_FLOAT);
		MemoryAccess.setFloat(floatSegmt, 6.79f);
		float result = (float)mh.invokeExact(5.74f, floatSegmt.address());
		floatSegmt.close();
		Assert.assertEquals(result, 12.53f, 0.01f);
	}
	
	@Test
	public void test_addTwoFloats_fromMemAddr() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT);
		Symbol functionSymbol = nativeLib.lookup("add2Floats").get();
		MemoryAddress memAddr = functionSymbol.address();
		MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
		float result = (float)mh.invokeExact(5.74f, 6.79f);
		Assert.assertEquals(result, 12.53f, 0.01f);
	}
	
	@Test
	public void test_addTwoDoubles() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, double.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE);
		Symbol functionSymbol = nativeLib.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		double result = (double)mh.invokeExact(159.748d, 262.795d);
		Assert.assertEquals(result, 422.543d, 0.001d);
	}
	
	@Test
	public void test_addDoubleAndDoubleFromPointer() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, MemoryAddress.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_POINTER, C_DOUBLE);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoubleFromPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		
		MemorySegment doubleSegmt = MemorySegment.allocateNative(C_DOUBLE);
		MemoryAccess.setDouble(doubleSegmt, 159.748d);
		double result = (double)mh.invokeExact(doubleSegmt.address(), 262.795d);
		doubleSegmt.close();
		Assert.assertEquals(result, 422.543d, 0.001d);
	}
	
	@Test
	public void test_addTwoDoubles_fromMemAddr() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, double.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE);
		Symbol functionSymbol = nativeLib.lookup("add2Doubles").get();
		MemoryAddress memAddr = functionSymbol.address();
		MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
		double result = (double)mh.invokeExact(159.748d, 262.795d);
		Assert.assertEquals(result, 422.543d, 0.001d);
	}
	
	@Test
	public void test_addDoublesWithVaList() throws Throwable {
		Symbol functionSymbol = nativeLib.lookup("addDoublesFromVaList").get();
		MethodType mt = MethodType.methodType(double.class, int.class, VaList.class); 
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_INT, C_VA_LIST);
		NativeScope nativeScope = NativeScope.unboundedScope();
		VaList vaList = CLinker.VaList.make(vaListBuilder -> vaListBuilder.vargFromDouble(C_DOUBLE, 150.1001D)
				.vargFromDouble(C_DOUBLE, 160.2002D)
				.vargFromDouble(C_DOUBLE, 170.1001D)
				.vargFromDouble(C_DOUBLE, 180.2002D), nativeScope);
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		double result = (double)mh.invoke(4, vaList);
		Assert.assertEquals(result, 660.6006D);
	}
	
	@Test
	public void test_strlenFromDefaultLibWithMemAddr() throws Throwable {
		Symbol strlenSymbol = defaultLib.lookup("strlen").get();
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_POINTER);
		MethodHandle mh = clinker.downcallHandle(strlenSymbol, mt, fd);
		MemorySegment funcMemSegment = CLinker.toCString("JEP389 DOWNCALL TEST SUITES");
		long strLength = (long)mh.invokeExact(funcMemSegment.address());
		Assert.assertEquals(strLength, 27);
	}
	
	@Test
	public void test_strlenFromDefaultLibWithMemAddr_fromMemAddr() throws Throwable {
		Symbol strlenSymbol = defaultLib.lookup("strlen").get();
		MemoryAddress memAddr = strlenSymbol.address();
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_POINTER);
		MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
		MemorySegment funcMemSegment = CLinker.toCString("JEP389 DOWNCALL TEST SUITES");
		long strLength = (long)mh.invokeExact(funcMemSegment.address());
		Assert.assertEquals(strLength, 27);
	}
	
	@Test
	public void test_memoryAllocFreeFromDefaultLib() throws Throwable {
		Symbol allocSymbol = defaultLib.lookup("malloc").get();
		MethodType allocMethodType = MethodType.methodType(MemoryAddress.class, long.class);
		FunctionDescriptor allocFuncDesc = FunctionDescriptor.of(C_POINTER, longLayout);
		MethodHandle allocHandle = clinker.downcallHandle(allocSymbol, allocMethodType, allocFuncDesc);
		MemoryAddress allocMemAddr = (MemoryAddress)allocHandle.invokeExact(10L);
		long allocMemAddrValue = allocMemAddr.toRawLongValue();
		
		MemorySegment memSeg = MemorySegment.ofNativeRestricted();
		MemoryAccess.setIntAtOffset(memSeg, allocMemAddrValue, 15);
		Assert.assertEquals(MemoryAccess.getIntAtOffset(memSeg, allocMemAddrValue), 15);
		
		Symbol freeSymbol = defaultLib.lookup("free").get();
		MethodType freeMethodType = MethodType.methodType(void.class, MemoryAddress.class);
		FunctionDescriptor freeFuncDesc = FunctionDescriptor.ofVoid(C_POINTER);
		MethodHandle freeHandle = clinker.downcallHandle(freeSymbol, freeMethodType, freeFuncDesc);
		freeHandle.invokeExact(allocMemAddr);
	}
	
	@Test
	public void test_memoryAllocFreeFromDefaultLib_fromMemAddr() throws Throwable {
		Symbol allocSymbol = defaultLib.lookup("malloc").get();
		MemoryAddress allocMemAddrFromSymbol = allocSymbol.address();
		MethodType allocMethodType = MethodType.methodType(MemoryAddress.class, long.class);
		FunctionDescriptor allocFuncDesc = FunctionDescriptor.of(C_POINTER, longLayout);
		MethodHandle allocHandle = clinker.downcallHandle(allocMemAddrFromSymbol, allocMethodType, allocFuncDesc);
		MemoryAddress allocMemAddr = (MemoryAddress)allocHandle.invokeExact(10L);
		long allocMemAddrValue = allocMemAddr.toRawLongValue();
		
		MemorySegment memSeg = MemorySegment.ofNativeRestricted();
		MemoryAccess.setIntAtOffset(memSeg, allocMemAddrValue, 15);
		Assert.assertEquals(MemoryAccess.getIntAtOffset(memSeg, allocMemAddrValue), 15);
		
		Symbol freeSymbol = defaultLib.lookup("free").get();
		MemoryAddress freeMemAddr = freeSymbol.address();
		MethodType freeMethodType = MethodType.methodType(void.class, MemoryAddress.class);
		FunctionDescriptor freeFuncDesc = FunctionDescriptor.ofVoid(C_POINTER);
		MethodHandle freeHandle = clinker.downcallHandle(freeMemAddr, freeMethodType, freeFuncDesc);
		freeHandle.invokeExact(allocMemAddr);
	}
	
	@Test
	public void test_memoryAllocFreeFromCLinkerMethod() throws Throwable {
		MemoryAddress allocMemAddr = CLinker.allocateMemoryRestricted(10L);
		long allocMemAddrValue = allocMemAddr.toRawLongValue();
		
		MemorySegment memSeg = MemorySegment.ofNativeRestricted();
		MemoryAccess.setIntAtOffset(memSeg, allocMemAddrValue, 49);
		Assert.assertEquals(MemoryAccess.getIntAtOffset(memSeg, allocMemAddrValue), 49);
		
		CLinker.freeMemoryRestricted(allocMemAddr);
	}
	
	@Test
	public void test_printfFromDefaultLibWithMemAddr() throws Throwable {
		Symbol functionSymbol = defaultLib.lookup("printf").get();
		MethodType mt = MethodType.methodType(int.class, MemoryAddress.class, int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_POINTER, C_INT, C_INT, C_INT);
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		MemorySegment formatMemSegment = CLinker.toCString("\n%d + %d = %d\n");
		mh.invoke(formatMemSegment.address(), 15, 27, 42);
	}
	
	@Test
	public void test_printfFromDefaultLibWithMemAddr_fromMemAddr() throws Throwable {
		Symbol functionSymbol = defaultLib.lookup("printf").get();
		MemoryAddress memAddr = functionSymbol.address();
		MethodType mt = MethodType.methodType(int.class, MemoryAddress.class, int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_POINTER, C_INT, C_INT, C_INT);
		MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
		MemorySegment formatMemSegment = CLinker.toCString("\n%d + %d = %d\n");
		mh.invoke(formatMemSegment.address(), 15, 27, 42);
	}
	
	@Test
	public void test_vprintfFromDefaultLibWithVaList() throws Throwable {
		/* Disable the test on Windows given a misaligned access exception coming from
		 * java.base/java.lang.invoke.MemoryAccessVarHandleBase triggered by CLinker.toCString()
		 * is also captured on OpenJDK/Hotspot.
		 */
		if (!isWinOS) {
			Symbol functionSymbol = defaultLib.lookup("vprintf").get();
			MethodType mt = MethodType.methodType(int.class, MemoryAddress.class, VaList.class); 
			FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_POINTER, C_VA_LIST);
			NativeScope nativeScope = NativeScope.unboundedScope();
			MemorySegment formatMemSegment = CLinker.toCString("%d * %d = %d\n", nativeScope);
			VaList vaList = CLinker.VaList.make(vaListBuilder -> vaListBuilder.vargFromInt(C_INT, 7)
					.vargFromInt(C_INT, 8)
					.vargFromInt(C_INT, 56), nativeScope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
			mh.invoke(formatMemSegment.address(), vaList);
		}
	}
	
	@Test
	public void test_vprintfFromDefaultLibWithVaList_fromMemAddr() throws Throwable {
		/* Disable the test on Windows given a misaligned access exception coming from
		 * java.base/java.lang.invoke.MemoryAccessVarHandleBase triggered by CLinker.toCString()
		 * is also captured on OpenJDK/Hotspot.
		 */
		if (!isWinOS) {
			Symbol functionSymbol = defaultLib.lookup("vprintf").get();
			MemoryAddress memAddr = functionSymbol.address();
			MethodType mt = MethodType.methodType(int.class, MemoryAddress.class, VaList.class); 
			FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_POINTER, C_VA_LIST);
			NativeScope nativeScope = NativeScope.unboundedScope();
			MemorySegment formatMemSegment = CLinker.toCString("%d * %d = %d\n", nativeScope);
			VaList vaList = CLinker.VaList.make(vaListBuilder -> vaListBuilder.vargFromInt(C_INT, 7)
					.vargFromInt(C_INT, 8)
					.vargFromInt(C_INT, 56), nativeScope);
			MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);
			mh.invoke(formatMemSegment.address(), vaList);
		}
	}
}
