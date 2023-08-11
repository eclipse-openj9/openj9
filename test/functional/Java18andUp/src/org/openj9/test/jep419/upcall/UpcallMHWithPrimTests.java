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
package org.openj9.test.jep419.upcall;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;

import java.lang.invoke.MethodHandle;
import jdk.incubator.foreign.NativeSymbol;
import jdk.incubator.foreign.CLinker;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.NativeSymbol;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.SymbolLookup;
import jdk.incubator.foreign.ValueLayout;
import static jdk.incubator.foreign.ValueLayout.*;

/**
 * Test cases for JEP 419: Foreign Linker API (Second Incubator) for primitive types in upcall.
 */
@Test(groups = { "level.sanity" })
public class UpcallMHWithPrimTests {
	private static CLinker clinker = CLinker.systemCLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addTwoBoolsWithOrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2BoolsWithOrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN), scope);
			boolean result = (boolean)mh.invoke(true, false, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolFromPointerWithOrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolFromPointerWithOrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolFromPointerWithOr,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS), scope);
			MemorySegment boolSegmt = MemorySegment.allocateNative(JAVA_BOOLEAN, scope);
			boolSegmt.set(JAVA_BOOLEAN, 0, true);
			boolean result = (boolean)mh.invoke(false, boolSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolFromNativePtrWithOrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolFromNativePtrWithOrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolFromPointerWithOr,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS), scope);
			boolean result = (boolean)mh.invoke(false, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolFromPtrWithOr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_BOOLEAN, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolFromPtrWithOr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolFromPtrWithOr_RetPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_BOOLEAN, ADDRESS), scope);
			MemorySegment boolSegmt = MemorySegment.allocateNative(JAVA_BOOLEAN, scope);
			boolSegmt.set(JAVA_BOOLEAN, 0, true);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(false, boolSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_BOOLEAN, 0), true);
		}
	}

	@Test
	public void test_addBoolAndBoolFromPtrWithOr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_BOOLEAN, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolFromPtrWithOr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolFromPtrWithOr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_BOOLEAN, ADDRESS), scope);
			MemorySegment boolSegmt = MemorySegment.allocateNative(JAVA_BOOLEAN, scope);
			boolSegmt.set(JAVA_BOOLEAN, 0, true);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(false, boolSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_BOOLEAN, 0), true);
		}
	}

	@Test
	public void test_addTwoBytesByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, JAVA_BYTE, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2BytesByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Bytes,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, JAVA_BYTE), scope);
			byte result = (byte)mh.invoke((byte)6, (byte)3, upcallFuncAddr);
			Assert.assertEquals(result, 9);
		}
	}

	@Test
	public void test_addByteAndByteFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndByteFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment byteSegmt = allocator.allocate(JAVA_BYTE, (byte)7);
			byte result = (byte)mh.invoke((byte)8, byteSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 15);
		}
	}

	@Test
	public void test_addByteAndByteFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndByteFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), scope);
			byte result = (byte)mh.invoke((byte)33, upcallFuncAddr);
			Assert.assertEquals(result, 88);
		}
	}

	@Test
	public void test_addByteAndByteFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_BYTE, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndByteFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_BYTE, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment byteSegmt = allocator.allocate(JAVA_BYTE, (byte)35);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke((byte)47, byteSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_BYTE, 0), 82);
		}
	}

	@Test
	public void test_addByteAndByteFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_BYTE, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndByteFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_BYTE, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment byteSegmt = allocator.allocate(JAVA_BYTE, (byte)35);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke((byte)47, byteSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_BYTE, 0), 82);
		}
	}

	@Test
	public void test_createNewCharFrom2CharsByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, JAVA_CHAR, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("createNewCharFrom2CharsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFrom2Chars,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, JAVA_CHAR), scope);
			char result = (char)mh.invoke('B', 'D', upcallFuncAddr);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment charSegmt = allocator.allocate(JAVA_CHAR, 'B');
			char result = (char)mh.invoke(charSegmt, 'D', upcallFuncAddr);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR), scope);
			char result = (char)mh.invoke('D', upcallFuncAddr);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_CHAR, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_CHAR), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment charSegmt = allocator.allocate(JAVA_CHAR, 'B');
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(charSegmt, 'D', upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_CHAR, 0), 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_CHAR, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_CHAR), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment charSegmt = allocator.allocate(JAVA_CHAR, 'B');
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(charSegmt, 'D', upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_CHAR, 0), 'C');
		}
	}

	@Test
	public void test_addTwoShortsByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, JAVA_SHORT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2ShortsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Shorts,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, JAVA_SHORT), scope);
			short result = (short)mh.invoke((short)1111, (short)2222, upcallFuncAddr);
			Assert.assertEquals(result, 3333);
		}
	}

	@Test
	public void test_addShortAndShortFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, ADDRESS, JAVA_SHORT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPointer,
					FunctionDescriptor.of(JAVA_SHORT, ADDRESS, JAVA_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment shortSegmt = allocator.allocate(JAVA_SHORT, (short)2222);
			short result = (short)mh.invoke(shortSegmt, (short)3333, upcallFuncAddr);
			Assert.assertEquals(result, 5555);
		}
	}

	@Test
	public void test_addShortAndShortFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPointer,
					FunctionDescriptor.of(JAVA_SHORT, ADDRESS, JAVA_SHORT), scope);
			short result = (short)mh.invoke((short)789, upcallFuncAddr);
			Assert.assertEquals(result, 1245);
		}
	}

	@Test
	public void test_addShortAndShortFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment shortSegmt = allocator.allocate(JAVA_SHORT, (short)444);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(shortSegmt, (short)555, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_SHORT, 0), 999);
		}
	}

	@Test
	public void test_addShortAndShortFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment shortSegmt = allocator.allocate(JAVA_SHORT, (short)444);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(shortSegmt, (short)555, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_SHORT, 0), 999);
		}
	}

	@Test
	public void test_addTwoIntsByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2IntsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT), scope);
			int result = (int)mh.invoke(111112, 111123, upcallFuncAddr);
			Assert.assertEquals(result, 222235);
		}
	}

	@Test
	public void test_addIntAndIntFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromPointer,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment intSegmt = allocator.allocate(JAVA_INT, 222215);
			int result = (int)mh.invoke(333321, intSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 555536);
		}
	}

	@Test
	public void test_addIntAndIntFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromPointer,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS), scope);
			int result = (int)mh.invoke(222222, upcallFuncAddr);
			Assert.assertEquals(result, 666666);
		}
	}

	@Test
	public void test_addIntAndIntFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_INT, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_INT, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment intSegmt = allocator.allocate(JAVA_INT, 222215);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(333321, intSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_INT, 0), 555536);
		}
	}

	@Test
	public void test_addIntAndIntFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_INT, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_INT, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment intSegmt = allocator.allocate(JAVA_INT, 222215);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(333321, intSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_INT, 0), 555536);
		}
	}

	@Test
	public void test_add3IntsByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, JAVA_INT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add3IntsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3Ints,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, JAVA_INT), scope);
			int result = (int)mh.invoke(111112, 111123, 111124, upcallFuncAddr);
			Assert.assertEquals(result, 333359);
		}
	}

	@Test
	public void test_addIntAndCharByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_CHAR, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndCharByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndChar,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_CHAR), scope);
			int result = (int)mh.invoke(555558, 'A', upcallFuncAddr);
			Assert.assertEquals(result, 555623);
		}
	}

	@Test
	public void test_addTwoIntsReturnVoidByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2IntsReturnVoidByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT), scope);
			mh.invoke(44454, 333398, upcallFuncAddr);
		}
	}

	@Test
	public void test_addTwoLongsByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, JAVA_LONG, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2LongsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Longs,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, JAVA_LONG), scope);
			long result = (long)mh.invoke(333333222222L, 111111555555L, upcallFuncAddr);
			Assert.assertEquals(result, 444444777777L);
		}
	}

	@Test
	public void test_addLongAndLongFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment longSegmt = allocator.allocate(JAVA_LONG, 5742457424L);
			long result = (long)mh.invoke(longSegmt, 6666698235L, upcallFuncAddr);
			Assert.assertEquals(result, 12409155659L);
		}
	}

	@Test
	public void test_addLongAndLongFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG), scope);
			long result = (long)mh.invoke(5555555555L, upcallFuncAddr);
			Assert.assertEquals(result, 8888888888L);
		}
	}

	@Test
	public void test_addLongAndLongFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment longSegmt = allocator.allocate(JAVA_LONG, 5742457424L);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(longSegmt, 6666698235L, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_LONG, 0), 12409155659L);
		}
	}

	@Test
	public void test_addLongAndLongFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment longSegmt = allocator.allocate(JAVA_LONG, 5742457424L);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(longSegmt, 6666698235L, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_LONG, 0), 12409155659L);
		}
	}

	@Test
	public void test_addTwoFloatsByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, JAVA_FLOAT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2FloatsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Floats,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, JAVA_FLOAT), scope);
			float result = (float)mh.invoke(15.74F, 16.79F, upcallFuncAddr);
			Assert.assertEquals(result, 32.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment floatSegmt = allocator.allocate(JAVA_FLOAT, 6.79F);
			float result = (float)mh.invoke(5.74F, floatSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), scope);
			float result = (float)mh.invoke(5.74F, upcallFuncAddr);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_FLOAT, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_FLOAT, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment floatSegmt = allocator.allocate(JAVA_FLOAT, 6.79F);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(5.74F, floatSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_FLOAT, 0), 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_FLOAT, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_FLOAT, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment floatSegmt = allocator.allocate(JAVA_FLOAT, 6.79F);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(5.74F, floatSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_FLOAT, 0), 12.53F, 0.01F);
		}
	}

	@Test
	public void test_add2DoublesByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2DoublesByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Doubles,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, JAVA_DOUBLE), scope);
			double result = (double)mh.invoke(159.748D, 262.795D, upcallFuncAddr);
			Assert.assertEquals(result, 422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, JAVA_DOUBLE, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPointer,
					FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, JAVA_DOUBLE), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment doubleSegmt = allocator.allocate(JAVA_DOUBLE, 1159.748D);
			double result = (double)mh.invoke(doubleSegmt, 1262.795D, upcallFuncAddr);
			Assert.assertEquals(result, 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPointer,
					FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, JAVA_DOUBLE), scope);
			double result = (double)mh.invoke(1262.795D, upcallFuncAddr);
			Assert.assertEquals(result, 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment doubleSegmt = allocator.allocate(JAVA_DOUBLE, 1159.748D);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(doubleSegmt, 1262.795D, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_DOUBLE, 0), 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment doubleSegmt = allocator.allocate(JAVA_DOUBLE, 1159.748D);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(doubleSegmt, 1262.795D, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_DOUBLE, 0), 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_qsortByUpcallMH() throws Throwable {
		int expectedArray[] = {11, 12, 13, 14, 15, 16, 17};
		int expectedArrayLength = expectedArray.length;

		FunctionDescriptor fd = FunctionDescriptor.ofVoid(ADDRESS, JAVA_INT, JAVA_INT, ADDRESS);
		NativeSymbol functionSymbol = clinker.lookup("qsort").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_compare,
					FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment arraySegmt =  allocator.allocateArray(JAVA_INT, new int[]{17, 14, 13, 16, 15, 12, 11});
			mh.invoke(arraySegmt, 7, 4, upcallFuncAddr);
			int[] sortedArray = arraySegmt.toArray(JAVA_INT);
			for (int index = 0; index < expectedArrayLength; index++) {
				Assert.assertEquals(sortedArray[index], expectedArray[index]);
			}
		}
	}
}
