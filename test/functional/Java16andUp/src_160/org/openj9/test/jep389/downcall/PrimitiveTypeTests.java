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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
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

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) DownCall for primitive types,
 * which covers generic tests, tests with the void type, the MemoryAddress type, and the vararg list.
 */
@Test(groups = { "level.sanity" })
public class PrimitiveTypeTests {
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinOS = osName.contains("win");
	/* long long is 64 bits on AIX/ppc64, which is the same as Windows */
	private static ValueLayout longLayout = (isWinOS || isAixOS) ? C_LONG_LONG : C_LONG;

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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_INT.withName("int"), C_INT.withName("int"), C_INT.withName("int"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (boolean)mh.invokeExact(false, false);
		Assert.assertEquals(result, false);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_INT.withName("int"), C_INT.withName("int"), C_POINTER.withName("pointer"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		intSegmt = MemorySegment.allocateNative(C_INT);
		MemoryAccess.setInt(intSegmt, 0);
		result = (boolean)mh.invokeExact(true, intSegmt.address());
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_INT.withName("int"), C_INT.withName("int"), C_INT.withName("int"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (boolean)mh.invokeExact(false, true);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_SHORT.withName("short"), C_SHORT.withName("short"), C_SHORT.withName("short"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (char)mh.invokeExact('B', 'D');
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_SHORT.withName("short"), C_POINTER.withName("pointer"), C_SHORT.withName("short"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		shortSegmt = MemorySegment.allocateNative(C_SHORT);
		MemoryAccess.setChar(shortSegmt, 'B');
		result = (char)mh.invokeExact(shortSegmt.address(), 'D');
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_SHORT.withName("short"), C_SHORT.withName("short"), C_SHORT.withName("short"));
		mh = clinker.downcallHandle(memAddr, mt, fd2);
		result = (char)mh.invokeExact('B', 'D');
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_CHAR.withName("char"), C_CHAR.withName("char"), C_CHAR.withName("char"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (byte)mh.invokeExact((byte)8, (byte)9);
		Assert.assertEquals(result, (byte)17);
	}

	@Test
	public void test_addTwoNegtiveBytes() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Symbol functionSymbol = nativeLib.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		byte result = (byte)mh.invokeExact((byte)-6, (byte)-3);
		Assert.assertEquals(result, (byte)-9);

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_CHAR.withName("char"), C_CHAR.withName("char"), C_CHAR.withName("char"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (byte)mh.invokeExact((byte)-7, (byte)-8);
		Assert.assertEquals(result, (byte)-15);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_CHAR.withName("char"), C_CHAR.withName("char"), C_POINTER.withName("pointer"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		charSegmt = MemorySegment.allocateNative(C_CHAR);
		MemoryAccess.setByte(charSegmt, (byte)7);
		result = (byte)mh.invokeExact((byte)8, charSegmt.address());
		charSegmt.close();
		Assert.assertEquals(result, (byte)15);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_CHAR.withName("char"), C_CHAR.withName("char"), C_CHAR.withName("char"));
		mh = clinker.downcallHandle(memAddr, mt, fd2);
		result = (byte)mh.invokeExact((byte)6, (byte)7);
		Assert.assertEquals(result, (byte)13);
	}

	@Test
	public void test_addTwoShorts() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		short result = (short)mh.invokeExact((short)24, (short)32);
		Assert.assertEquals(result, (short)56);

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_SHORT.withName("short"), C_SHORT.withName("short"), C_SHORT.withName("short"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (short)mh.invokeExact((short)11, (short)22);
		Assert.assertEquals(result, (short)33);
	}

	@Test
	public void test_addTwoNegtiveShorts() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		short result = (short)mh.invokeExact((short)-24, (short)-32);
		Assert.assertEquals(result, (short)-56);

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_SHORT.withName("short"), C_SHORT.withName("short"), C_SHORT.withName("short"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (short)mh.invokeExact((short)-11, (short)-22);
		Assert.assertEquals(result, (short)-33);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_SHORT.withName("short"), C_POINTER.withName("pointer"), C_SHORT.withName("short"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		shortSegmt = MemorySegment.allocateNative(C_SHORT);
		MemoryAccess.setShort(shortSegmt, (short)22);
		result = (short)mh.invokeExact(shortSegmt.address(), (short)33);
		Assert.assertEquals(result, (short)55);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_SHORT.withName("short"), C_SHORT.withName("short"), C_SHORT.withName("short"));
		mh = clinker.downcallHandle(memAddr, mt, fd2);
		result = (short)mh.invokeExact((short)12, (short)34);
		Assert.assertEquals(result, (short)46);
	}

	@Test
	public void test_addTwoInts() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		int result = (int)mh.invokeExact(112, 123);
		Assert.assertEquals(result, 235);

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_INT.withName("short"), C_INT.withName("short"), C_INT.withName("short"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (int)mh.invokeExact(111, 222);
		Assert.assertEquals(result, 333);
	}

	@Test
	public void test_addTwoNegtiveInts() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		int result = (int)mh.invokeExact(-112, -123);
		Assert.assertEquals(result, -235);

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_INT.withName("int"), C_INT.withName("int"), C_INT.withName("int"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (int)mh.invokeExact(-222, -444);
		Assert.assertEquals(result, -666);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_INT.withName("int"), C_INT.withName("int"), C_POINTER.withName("pointer"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		intSegmt = MemorySegment.allocateNative(C_INT);
		MemoryAccess.setInt(intSegmt, 333);
		result = (int)mh.invokeExact(444, intSegmt.address());
		intSegmt.close();
		Assert.assertEquals(result, 777);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_INT.withName("int"), C_INT.withName("int"), C_INT.withName("int"));
		mh = clinker.downcallHandle(memAddr, mt, fd2);
		result = (int)mh.invokeExact(234, 567);
		Assert.assertEquals(result, 801);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_INT.withName("int"), C_INT.withName("int"), C_SHORT.withName("short"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (int)mh.invokeExact(60, 'B');
		Assert.assertEquals(result, 126);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_INT.withName("int"), C_INT.withName("int"), C_SHORT.withName("short"));
		mh = clinker.downcallHandle(memAddr, mt, fd2);
		result = (int)mh.invokeExact(60, 'B');
		Assert.assertEquals(result, 126);
	}

	@Test
	public void test_addTwoLongs() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, long.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, longLayout);
		Symbol functionSymbol = nativeLib.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		long result = (long)mh.invokeExact(57424L, 698235L);
		Assert.assertEquals(result, 755659L);

		FunctionDescriptor fd2 = FunctionDescriptor.of(longLayout.withName("long"), longLayout.withName("long"), longLayout.withName("long"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (long)mh.invokeExact(333222L, 111555L);
		Assert.assertEquals(result, 444777L);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(longLayout.withName("long"), C_POINTER.withName("int"), longLayout.withName("long"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		longSegmt = MemorySegment.allocateNative(longLayout.withName("long"));
		MemoryAccess.setLong(longSegmt, 11111L);
		result = (long)mh.invokeExact(longSegmt.address(), 22222L);
		longSegmt.close();
		Assert.assertEquals(result, 33333L);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(longLayout.withName("long"), longLayout.withName("long"), longLayout.withName("long"));
		mh = clinker.downcallHandle(memAddr, mt, fd2);
		result = (long)mh.invokeExact(111222L, 333444L);
		Assert.assertEquals(result, 444666L);
	}

	@Test
	public void test_addTwoFloats() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT);
		Symbol functionSymbol = nativeLib.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		float result = (float)mh.invokeExact(5.74f, 6.79f);
		Assert.assertEquals(result, 12.53f, 0.01f);

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_FLOAT.withName("float"), C_FLOAT.withName("float"), C_FLOAT.withName("float"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (float)mh.invokeExact(15.74f, 16.79f);
		Assert.assertEquals(result, 32.53f, 0.01f);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_FLOAT.withName("float"), C_FLOAT.withName("float"), C_POINTER.withName("pointer"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		floatSegmt = MemorySegment.allocateNative(C_FLOAT.withName("float"));
		MemoryAccess.setFloat(floatSegmt, 16.79f);
		result = (float)mh.invokeExact(15.74f, floatSegmt.address());
		floatSegmt.close();
		Assert.assertEquals(result, 32.53f, 0.01f);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_FLOAT.withName("float"), C_FLOAT.withName("float"), C_FLOAT.withName("float"));
		mh = clinker.downcallHandle(memAddr, mt, fd2);
		result = (float)mh.invokeExact(15.74f, 16.79f);
		Assert.assertEquals(result, 32.53f, 0.01f);
	}

	@Test
	public void test_addTwoDoubles() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, double.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE);
		Symbol functionSymbol = nativeLib.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		double result = (double)mh.invokeExact(159.748d, 262.795d);
		Assert.assertEquals(result, 422.543d, 0.001d);

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_DOUBLE.withName("double"), C_DOUBLE.withName("double"), C_DOUBLE.withName("double"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		result = (double)mh.invokeExact(1159.748d, 1262.795d);
		Assert.assertEquals(result, 2422.543d, 0.001d);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_DOUBLE.withName("double"), C_POINTER.withName("pointer"), C_DOUBLE.withName("double"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		doubleSegmt = MemorySegment.allocateNative(C_DOUBLE.withName("double"));
		MemoryAccess.setDouble(doubleSegmt, 1159.748d);
		result = (double)mh.invokeExact(doubleSegmt.address(), 1262.795d);
		doubleSegmt.close();
		Assert.assertEquals(result, 2422.543d, 0.001d);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_DOUBLE.withName("double"), C_DOUBLE.withName("double"), C_DOUBLE.withName("double"));
		mh = clinker.downcallHandle(memAddr, mt, fd2);
		result = (double)mh.invokeExact(1159.748d, 1262.795d);
		Assert.assertEquals(result, 2422.543d, 0.001d);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(longLayout.withName("long"), C_POINTER.withName("pointer"));
		mh = clinker.downcallHandle(strlenSymbol, mt, fd2);
		strLength = (long)mh.invokeExact(funcMemSegment.address());
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(longLayout.withName("long"), C_POINTER.withName("pointer"));
		mh = clinker.downcallHandle(memAddr, mt, fd2);
		strLength = (long)mh.invokeExact(funcMemSegment.address());
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_INT.withName("int"), C_POINTER.withName("pointer"),
				C_INT.withName("int"), C_INT.withName("int"), C_INT.withName("int"));
		mh = clinker.downcallHandle(functionSymbol, mt, fd2);
		mh.invoke(formatMemSegment.address(), 115, 127, 242);
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

		FunctionDescriptor fd2 = FunctionDescriptor.of(C_INT.withName("int"), C_POINTER.withName("pointer"),
				C_INT.withName("int"), C_INT.withName("int"), C_INT.withName("int"));
		mh = clinker.downcallHandle(memAddr, mt, fd2);
		mh.invoke(formatMemSegment.address(), 115, 127, 242);
	}
}
