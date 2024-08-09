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
package org.openj9.test.jep454.upcall;

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
 * Test cases for JEP 454: Foreign Linker API for primitive types in upcall.
 */
@Test(groups = { "level.sanity" })
public class UpcallMHWithPrimTests {
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();
	private static final SymbolLookup defaultLibLookup = linker.defaultLookup();

	@Test
	public void test_addTwoBoolsWithOrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolsWithOrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN), arena);
			boolean result = (boolean)mh.invoke(true, false, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolFromPointerWithOrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolFromPointerWithOrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolFromPointerWithOr,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS), arena);
			MemorySegment boolSegmt = arena.allocate(JAVA_BOOLEAN);
			boolSegmt.set(JAVA_BOOLEAN, 0, true);
			boolean result = (boolean)mh.invoke(false, boolSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolFromNativePtrWithOrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolFromNativePtrWithOrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolFromPointerWithOr,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS), arena);
			boolean result = (boolean)mh.invoke(false, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolFromPtrWithOr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_BOOLEAN, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolFromPtrWithOr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolFromPtrWithOr_RetPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_BOOLEAN, ADDRESS), arena);
			MemorySegment boolSegmt = arena.allocate(JAVA_BOOLEAN);
			boolSegmt.set(JAVA_BOOLEAN, 0, true);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(false, boolSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_BOOLEAN.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), true);
		}
	}

	@Test
	public void test_addBoolAndBoolFromPtrWithOr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_BOOLEAN, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolFromPtrWithOr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolFromPtrWithOr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_BOOLEAN, ADDRESS), arena);
			MemorySegment boolSegmt = arena.allocate(JAVA_BOOLEAN);
			boolSegmt.set(JAVA_BOOLEAN, 0, true);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(false, boolSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_BYTE.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), true);
		}
	}

	@Test
	public void test_addTwoBytesByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, JAVA_BYTE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BytesByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2Bytes,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, JAVA_BYTE), arena);
			byte result = (byte)mh.invoke((byte)6, (byte)3, upcallFuncAddr);
			Assert.assertEquals(result, 9);
		}
	}

	@Test
	public void test_addByteAndByteFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndByteFromPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), arena);
			MemorySegment byteSegmt = arena.allocateFrom(JAVA_BYTE, (byte)7);
			byte result = (byte)mh.invoke((byte)8, byteSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 15);
		}
	}

	@Test
	public void test_addByteAndByteFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndByteFromNativePtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), arena);
			byte result = (byte)mh.invoke((byte)33, upcallFuncAddr);
			Assert.assertEquals(result, 88);
		}
	}

	@Test
	public void test_addByteAndByteFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_BYTE, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndByteFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_BYTE, ADDRESS), arena);
			MemorySegment byteSegmt = arena.allocateFrom(JAVA_BYTE, (byte)35);
			MemorySegment resultAddr = (MemorySegment)mh.invoke((byte)47, byteSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_BYTE.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BYTE, 0), 82);
		}
	}

	@Test
	public void test_addByteAndByteFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_BYTE, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndByteFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_BYTE, ADDRESS), arena);
			MemorySegment byteSegmt = arena.allocateFrom(JAVA_BYTE, (byte)35);
			MemorySegment resultAddr = (MemorySegment)mh.invoke((byte)47, byteSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_BYTE.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BYTE, 0), 82);
		}
	}

	@Test
	public void test_createNewCharFrom2CharsByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, JAVA_CHAR, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("createNewCharFrom2CharsByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_createNewCharFrom2Chars,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, JAVA_CHAR), arena);
			char result = (char)mh.invoke('B', 'D', upcallFuncAddr);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("createNewCharFromCharAndCharFromPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR), arena);
			MemorySegment charSegmt = arena.allocateFrom(JAVA_CHAR, 'B');
			char result = (char)mh.invoke(charSegmt, 'D', upcallFuncAddr);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("createNewCharFromCharAndCharFromNativePtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR), arena);
			char result = (char)mh.invoke('D', upcallFuncAddr);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_CHAR, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("createNewCharFromCharAndCharFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_CHAR), arena);
			MemorySegment charSegmt = arena.allocateFrom(JAVA_CHAR, 'B');
			MemorySegment resultAddr = (MemorySegment)mh.invoke(charSegmt, 'D', upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_CHAR.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_CHAR, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("createNewCharFromCharAndCharFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_CHAR), arena);
			MemorySegment charSegmt = arena.allocateFrom(JAVA_CHAR, 'B');
			MemorySegment resultAddr = (MemorySegment)mh.invoke(charSegmt, 'D', upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_CHAR.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'C');
		}
	}

	@Test
	public void test_addTwoShortsByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, JAVA_SHORT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ShortsByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2Shorts,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, JAVA_SHORT), arena);
			short result = (short)mh.invoke((short)1111, (short)2222, upcallFuncAddr);
			Assert.assertEquals(result, 3333);
		}
	}

	@Test
	public void test_addShortAndShortFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, ADDRESS, JAVA_SHORT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortFromPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPointer,
					FunctionDescriptor.of(JAVA_SHORT, ADDRESS, JAVA_SHORT), arena);
			MemorySegment shortSegmt = arena.allocateFrom(JAVA_SHORT, (short)2222);
			short result = (short)mh.invoke(shortSegmt, (short)3333, upcallFuncAddr);
			Assert.assertEquals(result, 5555);
		}
	}

	@Test
	public void test_addShortAndShortFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortFromNativePtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPointer,
					FunctionDescriptor.of(JAVA_SHORT, ADDRESS, JAVA_SHORT), arena);
			short result = (short)mh.invoke((short)789, upcallFuncAddr);
			Assert.assertEquals(result, 1245);
		}
	}

	@Test
	public void test_addShortAndShortFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT), arena);
			MemorySegment shortSegmt = arena.allocateFrom(JAVA_SHORT, (short)444);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(shortSegmt, (short)555, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_SHORT.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 999);
		}
	}

	@Test
	public void test_addShortAndShortFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT), arena);
			MemorySegment shortSegmt = arena.allocateFrom(JAVA_SHORT, (short)444);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(shortSegmt, (short)555, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_SHORT.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 999);
		}
	}

	@Test
	public void test_addTwoIntsByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntsByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT), arena);
			int result = (int)mh.invoke(111112, 111123, upcallFuncAddr);
			Assert.assertEquals(result, 222235);
		}
	}

	@Test
	public void test_addIntAndIntFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntFromPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromPointer,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS), arena);
			MemorySegment intSegmt = arena.allocateFrom(JAVA_INT, 222215);
			int result = (int)mh.invoke(333321, intSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 555536);
		}
	}

	@Test
	public void test_addIntAndIntFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntFromNativePtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromPointer,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS), arena);
			int result = (int)mh.invoke(222222, upcallFuncAddr);
			Assert.assertEquals(result, 666666);
		}
	}

	@Test
	public void test_addIntAndIntFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_INT, ADDRESS), arena);
			MemorySegment intSegmt = arena.allocateFrom(JAVA_INT, 222215);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(333321, intSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_INT.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 555536);
		}
	}

	@Test
	public void test_addIntAndIntFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_INT, ADDRESS), arena);
			MemorySegment intSegmt = arena.allocateFrom(JAVA_INT, 222215);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(333321, intSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_INT.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 555536);
		}
	}

	@Test
	public void test_add3IntsByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, JAVA_INT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3IntsByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3Ints,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, JAVA_INT), arena);
			int result = (int)mh.invoke(111112, 111123, 111124, upcallFuncAddr);
			Assert.assertEquals(result, 333359);
		}
	}

	@Test
	public void test_addIntAndCharByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_CHAR, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndCharByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndChar,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_CHAR), arena);
			int result = (int)mh.invoke(555558, 'A', upcallFuncAddr);
			Assert.assertEquals(result, 555623);
		}
	}

	@Test
	public void test_addTwoIntsReturnVoidByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntsReturnVoidByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT), arena);
			mh.invoke(44454, 333398, upcallFuncAddr);
		}
	}

	@Test
	public void test_addTwoLongsByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, JAVA_LONG, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2LongsByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2Longs,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, JAVA_LONG), arena);
			long result = (long)mh.invoke(333333222222L, 111111555555L, upcallFuncAddr);
			Assert.assertEquals(result, 444444777777L);
		}
	}

	@Test
	public void test_addLongAndLongFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongFromPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG), arena);
			MemorySegment longSegmt = arena.allocateFrom(JAVA_LONG, 5742457424L);
			long result = (long)mh.invoke(longSegmt, 6666698235L, upcallFuncAddr);
			Assert.assertEquals(result, 12409155659L);
		}
	}

	@Test
	public void test_addLongAndLongFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongFromNativePtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG), arena);
			long result = (long)mh.invoke(5555555555L, upcallFuncAddr);
			Assert.assertEquals(result, 8888888888L);
		}
	}

	@Test
	public void test_addLongAndLongFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG), arena);
			MemorySegment longSegmt = arena.allocateFrom(JAVA_LONG, 5742457424L);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(longSegmt, 6666698235L, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_LONG.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0), 12409155659L);
		}
	}

	@Test
	public void test_addLongAndLongFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_LONG), arena);
			MemorySegment longSegmt = arena.allocateFrom(JAVA_LONG, 5742457424L);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(longSegmt, 6666698235L, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_LONG.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0), 12409155659L);
		}
	}

	@Test
	public void test_addTwoFloatsByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, JAVA_FLOAT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2FloatsByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2Floats,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, JAVA_FLOAT), arena);
			float result = (float)mh.invoke(15.74F, 16.79F, upcallFuncAddr);
			Assert.assertEquals(result, 32.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), arena);
			MemorySegment floatSegmt = arena.allocateFrom(JAVA_FLOAT, 6.79F);
			float result = (float)mh.invoke(5.74F, floatSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromNativePtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), arena);
			float result = (float)mh.invoke(5.74F, upcallFuncAddr);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_FLOAT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_FLOAT, ADDRESS), arena);
			MemorySegment floatSegmt = arena.allocateFrom(JAVA_FLOAT, 6.79F);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(5.74F, floatSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_FLOAT.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, JAVA_FLOAT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, JAVA_FLOAT, ADDRESS), arena);
			MemorySegment floatSegmt = arena.allocateFrom(JAVA_FLOAT, 6.79F);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(5.74F, floatSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_FLOAT.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 12.53F, 0.01F);
		}
	}

	@Test
	public void test_add2DoublesByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2DoublesByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2Doubles,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, JAVA_DOUBLE), arena);
			double result = (double)mh.invoke(159.748D, 262.795D, upcallFuncAddr);
			Assert.assertEquals(result, 422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPointerByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, JAVA_DOUBLE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFromPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPointer,
					FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, JAVA_DOUBLE), arena);
			MemorySegment doubleSegmt = arena.allocateFrom(JAVA_DOUBLE, 1159.748D);
			double result = (double)mh.invoke(doubleSegmt, 1262.795D, upcallFuncAddr);
			Assert.assertEquals(result, 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromNativePtrByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFromNativePtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPointer,
					FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, JAVA_DOUBLE), arena);
			double result = (double)mh.invoke(1262.795D, upcallFuncAddr);
			Assert.assertEquals(result, 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE), arena);
			MemorySegment doubleSegmt = arena.allocateFrom(JAVA_DOUBLE, 1159.748D);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(doubleSegmt, 1262.795D, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_DOUBLE.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0), 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPtr_RetArgPtr_ByUpcallMH() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetArgPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE), arena);
			MemorySegment doubleSegmt = arena.allocateFrom(JAVA_DOUBLE, 1159.748D);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(doubleSegmt, 1262.795D, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_DOUBLE.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0), 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_qsortByUpcallMH() throws Throwable {
		int expectedArray[] = {11, 12, 13, 14, 15, 16, 17};
		int expectedArrayLength = expectedArray.length;

		FunctionDescriptor fd = FunctionDescriptor.ofVoid(ADDRESS, JAVA_INT, JAVA_INT, ADDRESS);
		MemorySegment functionSymbol = defaultLibLookup.find("qsort").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_compare,
					FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS), arena);
			MemorySegment arraySegmt =  arena.allocateFrom(JAVA_INT, new int[]{17, 14, 13, 16, 15, 12, 11});
			mh.invoke(arraySegmt, 7, 4, upcallFuncAddr);
			int[] sortedArray = arraySegmt.toArray(JAVA_INT);
			for (int index = 0; index < expectedArrayLength; index++) {
				Assert.assertEquals(sortedArray[index], expectedArray[index]);
			}
		}
	}
}
