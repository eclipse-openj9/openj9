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
package org.openj9.test.jep389.upcall;

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
 * Test cases for JEP 389: Foreign Linker API (Incubator) for primitive types in upcall.
 */
@Test(groups = { "level.sanity" })
public class UpcallMHWithPrimTests {
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinOS = osName.contains("win");
	/* long long is 64 bits on AIX/ppc64, which is the same as Windows */
	private static ValueLayout longLayout = (isWinOS || isAixOS) ? C_LONG_LONG : C_LONG;
	private static CLinker clinker = CLinker.getInstance();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();
	private static final SymbolLookup defaultLibLookup = CLinker.systemLookup();

	@Test(enabled=false)
	public static boolean byteToBool(byte value) {
		return (value != 0);
	}

	@Test
	public void test_addTwoBoolsWithOrByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, boolean.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolsWithOrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR), scope);
			boolean result = (boolean)mh.invokeExact(true, false, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolFromPointerWithOrByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolFromPointerWithOrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolFromPointerWithOr,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment boolSegmt = allocator.allocate(C_CHAR);
			MemoryAccess.setByte(boolSegmt, (byte)1);
			boolean result = (boolean)mh.invokeExact(false, boolSegmt.address(), upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolFromNativePtrWithOrByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolFromNativePtrWithOrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolFromPointerWithOr,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER), scope);
			boolean result = (boolean)mh.invokeExact(false, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolFromPtrWithOr_RetPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, boolean.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_CHAR, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolFromPtrWithOr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolFromPtrWithOr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_CHAR, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment boolSegmt = allocator.allocate(C_CHAR);
			MemoryAccess.setByte(boolSegmt, (byte)1);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(false, boolSegmt.address(), upcallFuncAddr);
			byte result = MemoryAccess.getByte(resultAddr.asSegment(C_CHAR.byteSize(), resultAddr.scope()));
			Assert.assertEquals(byteToBool(result), true);
		}
	}

	@Test
	public void test_addBoolAndBoolFromPtrWithOr_RetArgPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, boolean.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_CHAR, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolFromPtrWithOr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolFromPtrWithOr_RetArgPtr,
					FunctionDescriptor.of(C_POINTER, C_CHAR, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment boolSegmt = allocator.allocate(C_CHAR);
			MemoryAccess.setByte(boolSegmt, (byte)1);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(false, boolSegmt.address(), upcallFuncAddr);
			byte result = MemoryAccess.getByte(resultAddr.asSegment(C_CHAR.byteSize(), resultAddr.scope()));
			Assert.assertEquals(byteToBool(result), true);
		}
	}

	@Test
	public void test_addTwoBytesByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, byte.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BytesByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Bytes,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR), scope);
			byte result = (byte)mh.invokeExact((byte)6, (byte)3, upcallFuncAddr);
			Assert.assertEquals(result, (byte)9);
		}
	}

	@Test
	public void test_addByteAndByteFromPointerByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndByteFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment charSegmt = allocator.allocate(C_CHAR);
			MemoryAccess.setByte(charSegmt, (byte)7);
			byte result = (byte)mh.invokeExact((byte)8, charSegmt.address(), upcallFuncAddr);
			Assert.assertEquals(result, (byte)15);
		}
	}

	@Test
	public void test_addByteAndByteFromNativePtrByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndByteFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER), scope);
			byte result = (byte)mh.invokeExact((byte)33, upcallFuncAddr);
			Assert.assertEquals(result, (byte)88);
		}
	}

	@Test
	public void test_addByteAndByteFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, byte.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_CHAR, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndByteFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_CHAR, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment charSegmt = allocator.allocate(C_CHAR);
			MemoryAccess.setByte(charSegmt, (byte)35);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact((byte)47, charSegmt.address(), upcallFuncAddr);
			byte result = MemoryAccess.getByte(resultAddr.asSegment(C_CHAR.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, (byte)82);
		}
	}

	@Test
	public void test_addByteAndByteFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, byte.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_CHAR, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndByteFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPtr_RetArgPtr,
					FunctionDescriptor.of(C_POINTER, C_CHAR, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment charSegmt = allocator.allocate(C_CHAR);
			MemoryAccess.setByte(charSegmt, (byte)35);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact((byte)47, charSegmt.address(), upcallFuncAddr);
			byte result = MemoryAccess.getByte(resultAddr.asSegment(C_CHAR.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, (byte)82);
		}
	}

	@Test
	public void test_createNewCharFrom2CharsByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, char.class, char.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFrom2CharsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFrom2Chars,
					FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT), scope);
			char result = (char)mh.invokeExact('B', 'D', upcallFuncAddr);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPointerByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, MemoryAddress.class, char.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setChar(shortSegmt, 'B');
			char result = (char)mh.invokeExact(shortSegmt.address(), 'D', upcallFuncAddr);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromNativePtrByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, char.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT), scope);
			char result = (char)mh.invokeExact('D', upcallFuncAddr);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, char.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setChar(shortSegmt, 'B');
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(shortSegmt.address(), 'D', upcallFuncAddr);
			char result = MemoryAccess.getChar(resultAddr.asSegment(C_SHORT.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, char.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPtr_RetArgPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setChar(shortSegmt, 'B');
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(shortSegmt.address(), 'D', upcallFuncAddr);
			char result = MemoryAccess.getChar(resultAddr.asSegment(C_SHORT.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_addTwoShortsByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, short.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2ShortsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Shorts,
					FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT), scope);
			short result = (short)mh.invokeExact((short)1111, (short)2222, upcallFuncAddr);
			Assert.assertEquals(result, (short)3333);
		}
	}

	@Test
	public void test_addShortAndShortFromPointerByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, MemoryAddress.class, short.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPointer,
					FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setShort(shortSegmt, (short)2222);
			short result = (short)mh.invokeExact(shortSegmt.address(), (short)3333, upcallFuncAddr);
			Assert.assertEquals(result, (short)5555);
		}
	}

	@Test
	public void test_addShortAndShortFromNativePtrByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPointer,
					FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT), scope);
			short result = (short)mh.invokeExact((short)789, upcallFuncAddr);
			Assert.assertEquals(result, (short)1245);
		}
	}

	@Test
	public void test_addShortAndShortFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, short.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setShort(shortSegmt, (short)444);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(shortSegmt.address(), (short)555, upcallFuncAddr);
			short result = MemoryAccess.getShort(resultAddr.asSegment(C_SHORT.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, (short)999);
		}
	}

	@Test
	public void test_addShortAndShortFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, short.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetArgPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setShort(shortSegmt, (short)444);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(shortSegmt.address(), (short)555, upcallFuncAddr);
			short result = MemoryAccess.getShort(resultAddr.asSegment(C_SHORT.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, (short)999);
		}
	}

	@Test
	public void test_addTwoIntsByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(C_INT, C_INT, C_INT), scope);
			int result = (int)mh.invokeExact(111112, 111123, upcallFuncAddr);
			Assert.assertEquals(result, 222235);
		}
	}

	@Test
	public void test_addIntAndIntFromPointerByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromPointer,
					FunctionDescriptor.of(C_INT, C_INT, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment intSegmt = allocator.allocate(C_INT);
			MemoryAccess.setInt(intSegmt, 222215);
			int result = (int)mh.invokeExact(333321, intSegmt.address(), upcallFuncAddr);
			Assert.assertEquals(result, 555536);
		}
	}

	@Test
	public void test_addIntAndIntFromNativePtrByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromPointer,
					FunctionDescriptor.of(C_INT, C_INT, C_POINTER), scope);
			int result = (int)mh.invokeExact(222222, upcallFuncAddr);
			Assert.assertEquals(result, 666666);
		}
	}

	@Test
	public void test_addIntAndIntFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, int.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_INT, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_INT, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment intSegmt = allocator.allocate(C_INT);
			MemoryAccess.setInt(intSegmt, 222215);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(333321, intSegmt.address(), upcallFuncAddr);
			int result = MemoryAccess.getInt(resultAddr.asSegment(C_INT.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 555536);
		}
	}

	@Test
	public void test_addIntAndIntFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, int.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_INT, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromPtr_RetArgPtr,
					FunctionDescriptor.of(C_POINTER, C_INT, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment intSegmt = allocator.allocate(C_INT);
			MemoryAccess.setInt(intSegmt, 222215);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(333321, intSegmt.address(), upcallFuncAddr);
			int result = MemoryAccess.getInt(resultAddr.asSegment(C_INT.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 555536);
		}
	}

	@Test
	public void test_add3IntsByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add3IntsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3Ints,
					FunctionDescriptor.of(C_INT, C_INT, C_INT, C_INT), scope);
			int result = (int)mh.invokeExact(111112, 111123, 111124, upcallFuncAddr);
			Assert.assertEquals(result, 333359);
		}
	}

	@Test
	public void test_addIntAndCharByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, char.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndCharByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndChar,
					FunctionDescriptor.of(C_INT, C_INT, C_SHORT), scope);
			int result = (int)mh.invokeExact(555558, 'A', upcallFuncAddr);
			Assert.assertEquals(result, 555623);
		}
	}

	@Test
	public void test_addTwoIntsReturnVoidByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsReturnVoidByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(C_INT, C_INT), scope);
			mh.invokeExact(44454, 333398, upcallFuncAddr);
		}
	}

	@Test
	public void test_addTwoLongsByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, long.class, long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, longLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2LongsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Longs,
					FunctionDescriptor.of(longLayout, longLayout, longLayout), scope);
			long result = (long)mh.invokeExact(333333222222L, 111111555555L, upcallFuncAddr);
			Assert.assertEquals(result, 444444777777L);
		}
	}

	@Test
	public void test_addLongAndLongFromPointerByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class, long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_POINTER, longLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(longLayout, C_POINTER, longLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment longSegmt = allocator.allocate(longLayout);
			MemoryAccess.setLong(longSegmt, 5742457424L);
			long result = (long)mh.invokeExact(longSegmt.address(), 6666698235L, upcallFuncAddr);
			Assert.assertEquals(result, 12409155659L);
		}
	}

	@Test
	public void test_addLongAndLongFromNativePtrByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(longLayout, C_POINTER, longLayout), scope);
			long result = (long)mh.invokeExact(5555555555L, upcallFuncAddr);
			Assert.assertEquals(result, 8888888888L);
		}
	}

	@Test
	public void test_addLongAndLongFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, longLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, longLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment longSegmt = allocator.allocate(longLayout);
			MemoryAccess.setLong(longSegmt, 5742457424L);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(longSegmt.address(), 6666698235L, upcallFuncAddr);
			long result = MemoryAccess.getLong(resultAddr.asSegment(longLayout.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 12409155659L);
		}
	}

	@Test
	public void test_addLongAndLongFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, longLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPtr_RetArgPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, longLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment longSegmt = allocator.allocate(longLayout);
			MemoryAccess.setLong(longSegmt, 5742457424L);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(longSegmt.address(), 6666698235L, upcallFuncAddr);
			long result = MemoryAccess.getLong(resultAddr.asSegment(longLayout.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 12409155659L);
		}
	}

	@Test
	public void test_addTwoFloatsByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, float.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2FloatsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Floats,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT), scope);
			float result = (float)mh.invokeExact(15.74F, 16.79F, upcallFuncAddr);
			Assert.assertEquals(result, 32.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromPointerByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment floatSegmt = allocator.allocate(C_FLOAT);
			MemoryAccess.setFloat(floatSegmt, 6.79F);
			float result = (float)mh.invokeExact(5.74F, floatSegmt.address(), upcallFuncAddr);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromNativePtrByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER), scope);
			float result = (float)mh.invokeExact(5.74F, upcallFuncAddr);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, float.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_FLOAT, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_FLOAT, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment floatSegmt = allocator.allocate(C_FLOAT);
			MemoryAccess.setFloat(floatSegmt, 6.79F);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(5.74F, floatSegmt.address(), upcallFuncAddr);
			float result = MemoryAccess.getFloat(resultAddr.asSegment(C_FLOAT.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, float.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_FLOAT, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPtr_RetArgPtr,
					FunctionDescriptor.of(C_POINTER, C_FLOAT, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment floatSegmt = allocator.allocate(C_FLOAT);
			MemoryAccess.setFloat(floatSegmt, 6.79F);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(5.74F, floatSegmt.address(), upcallFuncAddr);
			float result = MemoryAccess.getFloat(resultAddr.asSegment(C_FLOAT.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 12.53F, 0.01F);
		}
	}

	@Test
	public void test_add2DoublesByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, double.class, double.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2DoublesByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Doubles,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE), scope);
			double result = (double)mh.invokeExact(159.748D, 262.795D, upcallFuncAddr);
			Assert.assertEquals(result, 422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPointerByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, MemoryAddress.class, double.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_POINTER, C_DOUBLE, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPointer,
					FunctionDescriptor.of(C_DOUBLE, C_POINTER, C_DOUBLE), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment doubleSegmt = allocator.allocate(C_DOUBLE);
			MemoryAccess.setDouble(doubleSegmt, 1159.748D);
			double result = (double)mh.invokeExact(doubleSegmt.address(), 1262.795D, upcallFuncAddr);
			Assert.assertEquals(result, 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromNativePtrByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, double.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPointer,
					FunctionDescriptor.of(C_DOUBLE, C_POINTER, C_DOUBLE), scope);
			double result = (double)mh.invokeExact(1262.795D, upcallFuncAddr);
			Assert.assertEquals(result, 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, double.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, C_DOUBLE, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_DOUBLE), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment doubleSegmt = allocator.allocate(C_DOUBLE);
			MemoryAccess.setDouble(doubleSegmt, 1159.748D);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(doubleSegmt.address(), 1262.795D, upcallFuncAddr);
			double result = MemoryAccess.getDouble(resultAddr.asSegment(C_DOUBLE.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, double.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, C_DOUBLE, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetArgPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_DOUBLE), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment doubleSegmt = allocator.allocate(C_DOUBLE);
			MemoryAccess.setDouble(doubleSegmt, 1159.748D);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(doubleSegmt.address(), 1262.795D, upcallFuncAddr);
			double result = MemoryAccess.getDouble(resultAddr.asSegment(C_DOUBLE.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_qsortByUpcallMH() throws Throwable {
		int expectedArray[] = {11, 12, 13, 14, 15, 16, 17};
		int expectedArrayLength = expectedArray.length;

		MethodType mt = MethodType.methodType(void.class, MemoryAddress.class, int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_POINTER, C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = defaultLibLookup.lookup("qsort").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_compare,
					FunctionDescriptor.of(C_INT, C_POINTER, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment arraySegmt =  allocator.allocateArray(C_INT, new int[]{17, 14, 13, 16, 15, 12, 11});
			mh.invokeExact(arraySegmt.address(), 7, 4, upcallFuncAddr);
			int[] sortedArray = arraySegmt.toIntArray();
			for (int index = 0; index < expectedArrayLength; index++) {
				Assert.assertEquals(sortedArray[index], expectedArray[index]);
			}
		}
	}
}
