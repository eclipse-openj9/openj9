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
import java.lang.foreign.GroupLayout;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemoryLayout.PathElement;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SegmentAllocator;
import java.lang.foreign.SequenceLayout;
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
import java.lang.invoke.VarHandle;

/**
 * Test cases for JEP 454: Foreign Linker API for argument/return struct in upcall.
 *
 * Note: the padding elements in the struct are only required by RI or VarHandle (accessing the
 * data address) while they are totally ignored in OpenJ9 given the padding/alignment are
 * computed by libffi automatically in native.
 */
@Test(groups = { "level.sanity" })
public class UpcallMHWithStructTests {
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addBoolAndBoolsFromStructWithXorByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithXorByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			boolHandle1.set(structSegmt, 0L, false);
			boolHandle2.set(structSegmt, 0L, true);

			boolean result = (boolean)mh.invoke(false, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAnd20BoolsFromStructWithXorByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN, JAVA_BOOLEAN,
				JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN,
				JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN,
				JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN
				);

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAnd20BoolsFromStructWithXorByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAnd20BoolsFromStructWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, false);
			structSegmt.set(JAVA_BOOLEAN, 1, true);
			structSegmt.set(JAVA_BOOLEAN, 2, false);
			structSegmt.set(JAVA_BOOLEAN, 3, true);
			structSegmt.set(JAVA_BOOLEAN, 4, false);
			structSegmt.set(JAVA_BOOLEAN, 5, true);
			structSegmt.set(JAVA_BOOLEAN, 6, false);
			structSegmt.set(JAVA_BOOLEAN, 7, true);
			structSegmt.set(JAVA_BOOLEAN, 8, false);
			structSegmt.set(JAVA_BOOLEAN, 9, true);
			structSegmt.set(JAVA_BOOLEAN, 10, false);
			structSegmt.set(JAVA_BOOLEAN, 11, true);
			structSegmt.set(JAVA_BOOLEAN, 12, false);
			structSegmt.set(JAVA_BOOLEAN, 13, true);
			structSegmt.set(JAVA_BOOLEAN, 14, false);
			structSegmt.set(JAVA_BOOLEAN, 15, true);
			structSegmt.set(JAVA_BOOLEAN, 16, false);
			structSegmt.set(JAVA_BOOLEAN, 17, true);
			structSegmt.set(JAVA_BOOLEAN, 18, false);
			structSegmt.set(JAVA_BOOLEAN, 19, true);

			boolean result = (boolean)mh.invoke(true, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolFromPointerAndBoolsFromStructWithXorByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolFromPointerAndBoolsFromStructWithXorByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolFromPointerAndBoolsFromStructWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, ADDRESS, structLayout), arena);
			MemorySegment boolSegmt = arena.allocate(JAVA_BOOLEAN);
			boolSegmt.set(JAVA_BOOLEAN, 0, true);
			MemorySegment structSegmt = arena.allocate(structLayout);
			boolHandle1.set(structSegmt, 0L, false);
			boolHandle2.set(structSegmt, 0L, true);

			boolean result = (boolean)mh.invoke(boolSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, false);
		}
	}

	@Test
	public void test_addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN, JAVA_BOOLEAN);
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment boolSegmt = arena.allocate(JAVA_BOOLEAN);
			boolSegmt.set(JAVA_BOOLEAN, 0, false);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, false);
			structSegmt.set(JAVA_BOOLEAN, 1, true);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(boolSegmt, structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_BOOLEAN.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), true);
			Assert.assertEquals(resultSegmt.address(), boolSegmt.address());
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructPointerWithXorByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructPointerWithXorByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructPointerWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			boolHandle1.set(structSegmt, 0L, true);
			boolHandle2.set(structSegmt, 0L, false);

			boolean result = (boolean)mh.invoke(false, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromNestedStructWithXorByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromNestedStructWithXorByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromNestedStructWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, true);
			structSegmt.set(JAVA_BOOLEAN, 1, false);
			structSegmt.set(JAVA_BOOLEAN, 2, true);

			boolean result = (boolean)mh.invoke(true, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromNestedStructWithXor_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromNestedStructWithXor_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromNestedStructWithXor_reverseOrder,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, true);
			structSegmt.set(JAVA_BOOLEAN, 1, false);
			structSegmt.set(JAVA_BOOLEAN, 2, true);

			boolean result = (boolean)mh.invoke(true, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedBoolArrayByUpcallMH() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, JAVA_BOOLEAN);
		GroupLayout structLayout = MemoryLayout.structLayout(boolArray.withName("array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithNestedBoolArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithNestedBoolArray,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, false);
			structSegmt.set(JAVA_BOOLEAN, 1, true);
			structSegmt.set(JAVA_BOOLEAN, 2, false);

			boolean result = (boolean)mh.invoke(false, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, JAVA_BOOLEAN);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), boolArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, false);
			structSegmt.set(JAVA_BOOLEAN, 1, true);
			structSegmt.set(JAVA_BOOLEAN, 2, false);

			boolean result = (boolean)mh.invoke(false, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout boolStruct = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, boolStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, false);
			structSegmt.set(JAVA_BOOLEAN, 1, true);
			structSegmt.set(JAVA_BOOLEAN, 2, false);
			structSegmt.set(JAVA_BOOLEAN, 3, true);
			structSegmt.set(JAVA_BOOLEAN, 4, false);

			boolean result = (boolean)mh.invoke(true, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout boolStruct = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, boolStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, false);
			structSegmt.set(JAVA_BOOLEAN, 1, true);
			structSegmt.set(JAVA_BOOLEAN, 2, false);
			structSegmt.set(JAVA_BOOLEAN, 3, true);
			structSegmt.set(JAVA_BOOLEAN, 4, false);

			boolean result = (boolean)mh.invoke(true, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_add2BoolStructsWithXor_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolStructsWithXor_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2BoolStructsWithXor_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			boolHandle1.set(structSegmt1, 0L, true);
			boolHandle2.set(structSegmt1, 0L, false);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			boolHandle1.set(structSegmt2, 0L, true);
			boolHandle2.set(structSegmt2, 0L, true);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(boolHandle1.get(resultSegmt, 0L), false);
			Assert.assertEquals(boolHandle2.get(resultSegmt, 0L), true);
		}
	}

	@Test
	public void test_add2BoolStructsWithXor_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolStructsWithXor_returnStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2BoolStructsWithXor_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			boolHandle1.set(structSegmt1, 0L, true);
			boolHandle2.set(structSegmt1, 0L, false);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			boolHandle1.set(structSegmt2, 0L, true);
			boolHandle2.set(structSegmt2, 0L, true);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), false);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 1), true);
		}
	}

	@Test
	public void test_add3BoolStructsWithXor_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"),
				JAVA_BOOLEAN.withName("elem2"), JAVA_BOOLEAN.withName("elem3"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle boolHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3BoolStructsWithXor_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3BoolStructsWithXor_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			boolHandle1.set(structSegmt1, 0L, true);
			boolHandle2.set(structSegmt1, 0L, false);
			boolHandle3.set(structSegmt1, 0L, true);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			boolHandle1.set(structSegmt2, 0L, true);
			boolHandle2.set(structSegmt2, 0L, true);
			boolHandle3.set(structSegmt2, 0L, false);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(boolHandle1.get(resultSegmt, 0L), false);
			Assert.assertEquals(boolHandle2.get(resultSegmt, 0L), true);
			Assert.assertEquals(boolHandle3.get(resultSegmt, 0L), true);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStruct,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			byteHandle1.set(structSegmt, 0L, (byte)8);
			byteHandle2.set(structSegmt, 0L, (byte)9);

			byte result = (byte)mh.invoke((byte)6, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 23);
		}
	}

	@Test
	public void test_addByteAnd20BytesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE, JAVA_BYTE,
				JAVA_BYTE, JAVA_BYTE, JAVA_BYTE, JAVA_BYTE, JAVA_BYTE, JAVA_BYTE,
				JAVA_BYTE, JAVA_BYTE, JAVA_BYTE, JAVA_BYTE, JAVA_BYTE, JAVA_BYTE,
				JAVA_BYTE, JAVA_BYTE, JAVA_BYTE, JAVA_BYTE, JAVA_BYTE, JAVA_BYTE
				);

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAnd20BytesFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAnd20BytesFromStruct,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)1);
			structSegmt.set(JAVA_BYTE, 1, (byte)2);
			structSegmt.set(JAVA_BYTE, 2, (byte)3);
			structSegmt.set(JAVA_BYTE, 3, (byte)4);
			structSegmt.set(JAVA_BYTE, 4, (byte)5);
			structSegmt.set(JAVA_BYTE, 5, (byte)6);
			structSegmt.set(JAVA_BYTE, 6, (byte)7);
			structSegmt.set(JAVA_BYTE, 7, (byte)8);
			structSegmt.set(JAVA_BYTE, 8, (byte)9);
			structSegmt.set(JAVA_BYTE, 9, (byte)10);
			structSegmt.set(JAVA_BYTE, 10, (byte)1);
			structSegmt.set(JAVA_BYTE, 11, (byte)2);
			structSegmt.set(JAVA_BYTE, 12, (byte)3);
			structSegmt.set(JAVA_BYTE, 13, (byte)4);
			structSegmt.set(JAVA_BYTE, 14, (byte)5);
			structSegmt.set(JAVA_BYTE, 15, (byte)6);
			structSegmt.set(JAVA_BYTE, 16, (byte)7);
			structSegmt.set(JAVA_BYTE, 17, (byte)8);
			structSegmt.set(JAVA_BYTE, 18, (byte)9);
			structSegmt.set(JAVA_BYTE, 19, (byte)10);

			byte result = (byte)mh.invoke((byte)11, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 121);
		}
	}

	@Test
	public void test_addByteFromPointerAndBytesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteFromPointerAndBytesFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteFromPointerAndBytesFromStruct,
					FunctionDescriptor.of(JAVA_BYTE, ADDRESS, structLayout), arena);
			MemorySegment byteSegmt = arena.allocateFrom(JAVA_BYTE, (byte)12);
			MemorySegment structSegmt = arena.allocate(structLayout);
			byteHandle1.set(structSegmt, 0L, (byte)18);
			byteHandle2.set(structSegmt, 0L, (byte)19);

			byte result = (byte)mh.invoke(byteSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 49);
		}
	}

	@Test
	public void test_addByteFromPointerAndBytesFromStruct_returnBytePointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteFromPointerAndBytesFromStruct_returnBytePointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteFromPointerAndBytesFromStruct_returnBytePointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment byteSegmt = arena.allocateFrom(JAVA_BYTE, (byte)12);
			MemorySegment structSegmt = arena.allocate(structLayout);
			byteHandle1.set(structSegmt, 0L, (byte)14);
			byteHandle2.set(structSegmt, 0L, (byte)16);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(byteSegmt, structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_BYTE.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BYTE, 0), 42);
			Assert.assertEquals(resultSegmt.address(), byteSegmt.address());
		}
	}

	@Test
	public void test_addByteAndBytesFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructPointer,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			byteHandle1.set(structSegmt, 0L, (byte)11);
			byteHandle2.set(structSegmt, 0L, (byte)12);
			byte result = (byte)mh.invoke((byte)13, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 36);
		}
	}

	@Test
	public void test_addByteAndBytesFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromNestedStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromNestedStruct,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)11);
			structSegmt.set(JAVA_BYTE, 1, (byte)22);
			structSegmt.set(JAVA_BYTE, 2, (byte)33);

			byte result = (byte)mh.invoke((byte)46, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 112);
		}
	}

	@Test
	public void test_addByteAndBytesFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)12);
			structSegmt.set(JAVA_BYTE, 1, (byte)24);
			structSegmt.set(JAVA_BYTE, 2, (byte)36);

			byte result = (byte)mh.invoke((byte)48, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 120);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedByteArrayByUpcallMH() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, JAVA_BYTE);
		GroupLayout structLayout =  MemoryLayout.structLayout(byteArray.withName("array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructWithNestedByteArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructWithNestedByteArray,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)11);
			structSegmt.set(JAVA_BYTE, 1, (byte)22);
			structSegmt.set(JAVA_BYTE, 2, (byte)33);

			byte result = (byte)mh.invoke((byte)14, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 80);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedByteArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, JAVA_BYTE);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), byteArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructWithNestedByteArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructWithNestedByteArray_reverseOrder,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)12);
			structSegmt.set(JAVA_BYTE, 1, (byte)14);
			structSegmt.set(JAVA_BYTE, 2, (byte)16);

			byte result = (byte)mh.invoke((byte)18, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 60);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout byteStruct = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, byteStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)11);
			structSegmt.set(JAVA_BYTE, 1, (byte)12);
			structSegmt.set(JAVA_BYTE, 2, (byte)13);
			structSegmt.set(JAVA_BYTE, 3, (byte)14);
			structSegmt.set(JAVA_BYTE, 4, (byte)15);

			byte result = (byte)mh.invoke((byte)16, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 81);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout byteStruct = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, byteStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)12);
			structSegmt.set(JAVA_BYTE, 1, (byte)14);
			structSegmt.set(JAVA_BYTE, 2, (byte)16);
			structSegmt.set(JAVA_BYTE, 3, (byte)18);
			structSegmt.set(JAVA_BYTE, 4, (byte)20);

			byte result = (byte)mh.invoke((byte)22, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 102);
		}
	}

	@Test
	public void test_add1ByteStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add1ByteStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add1ByteStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt1, 0L, (byte)25);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt2, 0L, (byte)24);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt, 0L), (byte)49);
		}
	}

	@Test
	public void test_add2ByteStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ByteStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2ByteStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt1, 0L, (byte)25);
			byteHandle2.set(structSegmt1, 0L, (byte)11);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt2, 0L, (byte)24);
			byteHandle2.set(structSegmt2, 0L, (byte)13);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt, 0L), (byte)49);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt, 0L), (byte)24);
		}
	}

	@Test
	public void test_add2ByteStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ByteStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2ByteStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt1, 0L, (byte)25);
			byteHandle2.set(structSegmt1, 0L, (byte)11);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt2, 0L, (byte)24);
			byteHandle2.set(structSegmt2, 0L, (byte)13);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BYTE, 0), 49);
			Assert.assertEquals(resultSegmt.get(JAVA_BYTE, 1), 24);
		}
	}

	@Test
	public void test_add3ByteStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"),
				JAVA_BYTE.withName("elem2"), JAVA_BYTE.withName("elem3"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle byteHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3ByteStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3ByteStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt1, 0L, (byte)25);
			byteHandle2.set(structSegmt1, 0L, (byte)11);
			byteHandle3.set(structSegmt1, 0L, (byte)12);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt2, 0L, (byte)24);
			byteHandle2.set(structSegmt2, 0L, (byte)13);
			byteHandle3.set(structSegmt2, 0L, (byte)16);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt, 0L), (byte)49);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt, 0L), (byte)24);
			Assert.assertEquals((byte)byteHandle3.get(resultSegmt, 0L), (byte)28);
		}
	}

	@Test
	public void test_addCharAndCharsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStruct,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			charHandle1.set(structSegmt, 0L, 'A');
			charHandle2.set(structSegmt, 0L, 'B');

			char result = (char)mh.invoke('C', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'D');
		}
	}

	@Test
	public void test_addCharAnd10CharsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR, JAVA_CHAR, JAVA_CHAR, JAVA_CHAR,
				JAVA_CHAR, JAVA_CHAR, JAVA_CHAR, JAVA_CHAR, JAVA_CHAR, JAVA_CHAR);

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAnd10CharsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAnd10CharsFromStruct,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'A');
			structSegmt.set(JAVA_CHAR, 2, 'A');
			structSegmt.set(JAVA_CHAR, 4, 'B');
			structSegmt.set(JAVA_CHAR, 6, 'B');
			structSegmt.set(JAVA_CHAR, 8, 'C');
			structSegmt.set(JAVA_CHAR, 10, 'C');
			structSegmt.set(JAVA_CHAR, 12, 'D');
			structSegmt.set(JAVA_CHAR, 14, 'D');
			structSegmt.set(JAVA_CHAR, 16, 'E');
			structSegmt.set(JAVA_CHAR, 18, 'E');

			char result = (char)mh.invoke('A', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'U');
		}
	}

	@Test
	public void test_addCharFromPointerAndCharsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharFromPointerAndCharsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharFromPointerAndCharsFromStruct,
					FunctionDescriptor.of(JAVA_CHAR, ADDRESS, structLayout), arena);
			MemorySegment charSegmt = arena.allocateFrom(JAVA_CHAR, 'D');
			MemorySegment structSegmt = arena.allocate(structLayout);
			charHandle1.set(structSegmt, 0L, 'E');
			charHandle2.set(structSegmt, 0L, 'F');

			char result = (char)mh.invoke(charSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'M');
		}
	}

	@Test
	public void test_addCharFromPointerAndCharsFromStruct_returnCharPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharFromPointerAndCharsFromStruct_returnCharPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharFromPointerAndCharsFromStruct_returnCharPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment charSegmt = arena.allocateFrom(JAVA_CHAR, 'D');
			MemorySegment structSegmt = arena.allocate(structLayout);
			charHandle1.set(structSegmt, 0L, 'E');
			charHandle2.set(structSegmt, 0L, 'F');

			MemorySegment resultAddr = (MemorySegment)mh.invoke(charSegmt, structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_CHAR.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'M');
			Assert.assertEquals(resultSegmt.address(), charSegmt.address());
		}
	}

	@Test
	public void test_addCharAndCharsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructPointer,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, ADDRESS), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			charHandle1.set(structSegmt, 0L, 'H');
			charHandle2.set(structSegmt, 0L, 'I');

			char result = (char)mh.invoke('G', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'V');
		}
	}

	@Test
	public void test_addCharAndCharsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromNestedStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromNestedStruct,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'E');
			structSegmt.set(JAVA_CHAR, 2, 'F');
			structSegmt.set(JAVA_CHAR, 4, 'G');

			char result = (char)mh.invoke('H', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'W');
		}
	}

	@Test
	public void test_addCharAndCharsFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'E');
			structSegmt.set(JAVA_CHAR, 2, 'F');
			structSegmt.set(JAVA_CHAR, 4, 'G');

			char result = (char)mh.invoke('H', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'W');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedCharArrayByUpcallMH() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, JAVA_CHAR);
		GroupLayout structLayout = MemoryLayout.structLayout(charArray.withName("array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructWithNestedCharArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructWithNestedCharArray,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'A');
			structSegmt.set(JAVA_CHAR, 2, 'B');
			structSegmt.set(JAVA_CHAR, 4, 'C');

			char result = (char)mh.invoke('D', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'G');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedCharArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, JAVA_CHAR);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), charArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructWithNestedCharArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructWithNestedCharArray_reverseOrder,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'A');
			structSegmt.set(JAVA_CHAR, 2, 'B');
			structSegmt.set(JAVA_CHAR, 4, 'C');

			char result = (char)mh.invoke('D', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'G');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout charStruct = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, charStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'E');
			structSegmt.set(JAVA_CHAR, 2, 'F');
			structSegmt.set(JAVA_CHAR, 4, 'G');
			structSegmt.set(JAVA_CHAR, 6, 'H');
			structSegmt.set(JAVA_CHAR, 8, 'I');

			char result = (char)mh.invoke('J', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'h');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout charStruct = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, charStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'E');
			structSegmt.set(JAVA_CHAR, 2, 'F');
			structSegmt.set(JAVA_CHAR, 4, 'G');
			structSegmt.set(JAVA_CHAR, 6, 'H');
			structSegmt.set(JAVA_CHAR, 8, 'I');

			char result = (char)mh.invoke('J', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'h');
		}
	}

	@Test
	public void test_add2CharStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2CharStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2CharStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			charHandle1.set(structSegmt1, 0L, 'A');
			charHandle2.set(structSegmt1, 0L, 'B');
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			charHandle1.set(structSegmt2, 0L, 'C');
			charHandle2.set(structSegmt2, 0L, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(charHandle1.get(resultSegmt, 0L), 'C');
			Assert.assertEquals(charHandle2.get(resultSegmt, 0L), 'E');
		}
	}

	@Test
	public void test_add2CharStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2CharStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2CharStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			charHandle1.set(structSegmt1, 0L, 'A');
			charHandle2.set(structSegmt1, 0L, 'B');
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			charHandle1.set(structSegmt2, 0L, 'C');
			charHandle2.set(structSegmt2, 0L, 'D');

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'C');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 2), 'E');
		}
	}

	@Test
	public void test_add3CharStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"),
				JAVA_CHAR.withName("elem2"), JAVA_CHAR.withName("elem3"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle charHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3CharStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3CharStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			charHandle1.set(structSegmt1, 0L, 'A');
			charHandle2.set(structSegmt1, 0L, 'B');
			charHandle3.set(structSegmt1, 0L, 'C');
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			charHandle1.set(structSegmt2, 0L, 'B');
			charHandle2.set(structSegmt2, 0L, 'C');
			charHandle3.set(structSegmt2, 0L, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(charHandle1.get(resultSegmt, 0L), 'B');
			Assert.assertEquals(charHandle2.get(resultSegmt, 0L), 'D');
			Assert.assertEquals(charHandle3.get(resultSegmt, 0L), 'F');
		}
	}

	@Test
	public void test_addShortAndShortsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStruct,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			shortHandle1.set(structSegmt, 0L, (short)888);
			shortHandle2.set(structSegmt, 0L, (short)999);

			short result = (short)mh.invoke((short)777, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 2664);
		}
	}

	@Test
	public void test_addShortAnd10ShortsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT, JAVA_SHORT, JAVA_SHORT,
				JAVA_SHORT, JAVA_SHORT, JAVA_SHORT, JAVA_SHORT, JAVA_SHORT, JAVA_SHORT, JAVA_SHORT);

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAnd10ShortsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAnd10ShortsFromStruct,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)10);
			structSegmt.set(JAVA_SHORT, 2, (short)20);
			structSegmt.set(JAVA_SHORT, 4, (short)30);
			structSegmt.set(JAVA_SHORT, 6, (short)40);
			structSegmt.set(JAVA_SHORT, 8, (short)50);
			structSegmt.set(JAVA_SHORT, 10, (short)60);
			structSegmt.set(JAVA_SHORT, 12, (short)70);
			structSegmt.set(JAVA_SHORT, 14, (short)80);
			structSegmt.set(JAVA_SHORT, 16, (short)90);
			structSegmt.set(JAVA_SHORT, 18, (short)100);

			short result = (short)mh.invoke((short)110, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 660);
		}
	}

	@Test
	public void test_addShortFromPointerAndShortsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortFromPointerAndShortsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortFromPointerAndShortsFromStruct,
					FunctionDescriptor.of(JAVA_SHORT, ADDRESS, structLayout), arena);
			MemorySegment shortSegmt = arena.allocateFrom(JAVA_SHORT, (short)1112);
			MemorySegment structSegmt = arena.allocate(structLayout);
			shortHandle1.set(structSegmt, 0L, (short)1118);
			shortHandle2.set(structSegmt, 0L, (short)1119);

			short result = (short)mh.invoke(shortSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 3349);
		}
	}

	@Test
	public void test_addShortFromPointerAndShortsFromStruct_returnShortPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortFromPointerAndShortsFromStruct_returnShortPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortFromPointerAndShortsFromStruct_returnShortPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment shortSegmt = arena.allocateFrom(JAVA_SHORT, (short)1112);
			MemorySegment structSegmt = arena.allocate(structLayout);
			shortHandle1.set(structSegmt, 0L, (short)1118);
			shortHandle2.set(structSegmt, 0L, (short)1119);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(shortSegmt, structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_SHORT.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 3349);
			Assert.assertEquals(resultSegmt.address(), shortSegmt.address());
		}
	}

	@Test
	public void test_addShortAndShortsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructPointer,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, ADDRESS), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			shortHandle1.set(structSegmt, 0L, (short)2222);
			shortHandle2.set(structSegmt, 0L, (short)4444);

			short result = (short)mh.invoke((short)6666, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 13332);
		}
	}

	@Test
	public void test_addShortAndShortsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromNestedStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromNestedStruct,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)331);
			structSegmt.set(JAVA_SHORT, 2, (short)333);
			structSegmt.set(JAVA_SHORT, 4, (short)335);

			short result = (short)mh.invoke((short)337, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1336);
		}
	}

	@Test
	public void test_addShortAndShortsFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)331);
			structSegmt.set(JAVA_SHORT, 2, (short)333);
			structSegmt.set(JAVA_SHORT, 4, (short)335);

			short result = (short)mh.invoke((short)337, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1336);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedShortArrayByUpcallMH() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, JAVA_SHORT);
		GroupLayout structLayout = MemoryLayout.structLayout(shortArray.withName("array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructWithNestedShortArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructWithNestedShortArray,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)1111);
			structSegmt.set(JAVA_SHORT, 2, (short)2222);
			structSegmt.set(JAVA_SHORT, 4, (short)3333);

			short result = (short)mh.invoke((short)4444, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 11110);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedShortArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, JAVA_SHORT);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), shortArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructWithNestedShortArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructWithNestedShortArray_reverseOrder,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)1111);
			structSegmt.set(JAVA_SHORT, 2, (short)2222);
			structSegmt.set(JAVA_SHORT, 4, (short)3333);

			short result = (short)mh.invoke((short)4444, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 11110);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout shortStruct = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, shortStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struc_array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)1111);
			structSegmt.set(JAVA_SHORT, 2, (short)2222);
			structSegmt.set(JAVA_SHORT, 4, (short)3333);
			structSegmt.set(JAVA_SHORT, 6, (short)4444);
			structSegmt.set(JAVA_SHORT, 8, (short)5555);

			short result = (short)mh.invoke((short)6666, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 23331);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout shortStruct = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, shortStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), structArray.withName("struc_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)1111);
			structSegmt.set(JAVA_SHORT, 2, (short)2222);
			structSegmt.set(JAVA_SHORT, 4, (short)3333);
			structSegmt.set(JAVA_SHORT, 6, (short)4444);
			structSegmt.set(JAVA_SHORT, 8, (short)5555);

			short result = (short)mh.invoke((short)6666, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 23331);
		}
	}

	@Test
	public void test_add2ShortStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ShortStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2ShortStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			shortHandle1.set(structSegmt1, 0L, (short)356);
			shortHandle2.set(structSegmt1, 0L, (short)345);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			shortHandle1.set(structSegmt2, 0L, (short)378);
			shortHandle2.set(structSegmt2, 0L, (short)367);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt, 0L), (short)734);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt, 0L), (short)712);
		}
	}

	@Test
	public void test_add2ShortStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ShortStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2ShortStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			shortHandle1.set(structSegmt1, 0L, (short)356);
			shortHandle2.set(structSegmt1, 0L, (short)345);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			shortHandle1.set(structSegmt2, 0L, (short)378);
			shortHandle2.set(structSegmt2, 0L, (short)367);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 734);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 2), 712);
		}
	}

	@Test
	public void test_add3ShortStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"),
				JAVA_SHORT.withName("elem2"), JAVA_SHORT.withName("elem3"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle shortHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3ShortStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3ShortStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			shortHandle1.set(structSegmt1, 0L, (short)325);
			shortHandle2.set(structSegmt1, 0L, (short)326);
			shortHandle3.set(structSegmt1, 0L, (short)327);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			shortHandle1.set(structSegmt2, 0L, (short)334);
			shortHandle2.set(structSegmt2, 0L, (short)335);
			shortHandle3.set(structSegmt2, 0L, (short)336);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt, 0L), (short)659);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt, 0L), (short)661);
			Assert.assertEquals((short)shortHandle3.get(resultSegmt, 0L), (short)663);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStruct,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			intHandle1.set(structSegmt, 0L, 1122334);
			intHandle2.set(structSegmt, 0L, 1234567);

			int result = (int)mh.invoke(2244668, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 4601569);
		}
	}

	@Test
	public void test_addIntAnd5IntsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"),
				JAVA_INT.withName("elem3"), JAVA_INT.withName("elem4"), JAVA_INT.withName("elem5"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle intHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));
		VarHandle intHandle4 = structLayout.varHandle(PathElement.groupElement("elem4"));
		VarHandle intHandle5 = structLayout.varHandle(PathElement.groupElement("elem5"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAnd5IntsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAnd5IntsFromStruct,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			intHandle1.set(structSegmt, 0L, 1111111);
			intHandle2.set(structSegmt, 0L, 2222222);
			intHandle3.set(structSegmt, 0L, 3333333);
			intHandle4.set(structSegmt, 0L, 2222222);
			intHandle5.set(structSegmt, 0L, 1111111);

			int result = (int)mh.invoke(4444444, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 14444443);
		}
	}

	@Test
	public void test_addIntFromPointerAndIntsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntFromPointerAndIntsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntFromPointerAndIntsFromStruct,
					FunctionDescriptor.of(JAVA_INT, ADDRESS, structLayout), arena);
			MemorySegment intSegmt = arena.allocateFrom(JAVA_INT, 7654321);
			MemorySegment structSegmt = arena.allocate(structLayout);
			intHandle1.set(structSegmt, 0L, 1234567);
			intHandle2.set(structSegmt, 0L, 2468024);

			int result = (int)mh.invoke(intSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 11356912);
		}
	}

	@Test
	public void test_addIntFromPointerAndIntsFromStruct_returnIntPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntFromPointerAndIntsFromStruct_returnIntPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntFromPointerAndIntsFromStruct_returnIntPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment intSegmt = arena.allocateFrom(JAVA_INT, 1122333);
			MemorySegment structSegmt = arena.allocate(structLayout);
			intHandle1.set(structSegmt, 0L, 4455666);
			intHandle2.set(structSegmt, 0L, 7788999);
			MemorySegment resultAddr = (MemorySegment)mh.invoke(intSegmt, structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_INT.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 13366998);
			Assert.assertEquals(resultSegmt.address(), intSegmt.address());
		}
	}

	@Test
	public void test_addIntAndIntsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructPointer,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			intHandle1.set(structSegmt, 0L, 11121314);
			intHandle2.set(structSegmt, 0L, 15161718);

			int result = (int)mh.invoke(19202122, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 45485154);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_INT.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromNestedStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromNestedStruct,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 21222324);
			structSegmt.set(JAVA_INT, 4, 25262728);
			structSegmt.set(JAVA_INT, 8, 29303132);

			int result = (int)mh.invoke(33343536, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 109131720);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 21222324);
			structSegmt.set(JAVA_INT, 4, 25262728);
			structSegmt.set(JAVA_INT, 8, 29303132);

			int result = (int)mh.invoke(33343536, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 109131720);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedIntArrayByUpcallMH() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, JAVA_INT);
		GroupLayout structLayout = MemoryLayout.structLayout(intArray.withName("array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructWithNestedIntArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructWithNestedIntArray,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 1111111);
			structSegmt.set(JAVA_INT, 4, 2222222);
			structSegmt.set(JAVA_INT, 8, 3333333);

			int result = (int)mh.invoke(4444444, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 11111110);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedIntArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, JAVA_INT);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), intArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructWithNestedIntArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructWithNestedIntArray_reverseOrder,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 1111111);
			structSegmt.set(JAVA_INT, 4, 2222222);
			structSegmt.set(JAVA_INT, 8, 3333333);

			int result = (int)mh.invoke(4444444, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 11111110);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout intStruct = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, intStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 1111111);
			structSegmt.set(JAVA_INT, 4, 2222222);
			structSegmt.set(JAVA_INT, 8, 3333333);
			structSegmt.set(JAVA_INT, 12, 4444444);
			structSegmt.set(JAVA_INT, 16, 5555555);

			int result = (int)mh.invoke(6666666, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 23333331);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout intStruct = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, intStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 1111111);
			structSegmt.set(JAVA_INT, 4, 2222222);
			structSegmt.set(JAVA_INT, 8, 3333333);
			structSegmt.set(JAVA_INT, 12, 4444444);
			structSegmt.set(JAVA_INT, 16, 5555555);

			int result = (int)mh.invoke(6666666, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 23333331);
		}
	}

	@Test
	public void test_add2IntStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 0L, 11223344);
			intHandle2.set(structSegmt1, 0L, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 0L, 99001122);
			intHandle2.set(structSegmt2, 0L, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(intHandle1.get(resultSegmt, 0L), 110224466);
			Assert.assertEquals(intHandle2.get(resultSegmt, 0L), 89113354);
		}
	}

	@Test
	public void test_add2IntStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 0L, 11223344);
			intHandle2.set(structSegmt1, 0L, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 0L, 99001122);
			intHandle2.set(structSegmt2, 0L, 33445566);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 110224466);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 4), 89113354);
		}
	}

	@Test
	public void test_add3IntStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"), JAVA_INT.withName("elem3"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle intHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3IntStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3IntStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 0L, 11223344);
			intHandle2.set(structSegmt1, 0L, 55667788);
			intHandle3.set(structSegmt1, 0L, 99001122);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 0L, 33445566);
			intHandle2.set(structSegmt2, 0L, 77889900);
			intHandle3.set(structSegmt2, 0L, 44332211);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(intHandle1.get(resultSegmt, 0L), 44668910);
			Assert.assertEquals(intHandle2.get(resultSegmt, 0L), 133557688);
			Assert.assertEquals(intHandle3.get(resultSegmt, 0L), 143333333);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStruct,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			longHandle1.set(structSegmt, 0L, 1234567890L);
			longHandle2.set(structSegmt, 0L, 9876543210L);

			long result = (long)mh.invoke(2468024680L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 13579135780L);
		}
	}

	@Test
	public void test_addLongFromPointerAndLongsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongFromPointerAndLongsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongFromPointerAndLongsFromStruct,
					FunctionDescriptor.of(JAVA_LONG, ADDRESS, structLayout), arena);
			MemorySegment longSegmt = arena.allocateFrom(JAVA_LONG, 1111111111L);
			MemorySegment structSegmt = arena.allocate(structLayout);
			longHandle1.set(structSegmt, 0L, 3333333333L);
			longHandle2.set(structSegmt, 0L, 5555555555L);

			long result = (long)mh.invoke(longSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 9999999999L);
		}
	}

	@Test
	public void test_addLongFromPointerAndLongsFromStruct_returnLongPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongFromPointerAndLongsFromStruct_returnLongPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongFromPointerAndLongsFromStruct_returnLongPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment longSegmt = arena.allocateFrom(JAVA_LONG, 1122334455L);
			MemorySegment structSegmt = arena.allocate(structLayout);
			longHandle1.set(structSegmt, 0L, 6677889900L);
			longHandle2.set(structSegmt, 0L, 1234567890L);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(longSegmt, structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_LONG.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0), 9034792245L);
			Assert.assertEquals(resultSegmt.address(), longSegmt.address());
		}
	}

	@Test
	public void test_addLongAndLongsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructPointer,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, ADDRESS), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			longHandle1.set(structSegmt, 0L, 224466880022L);
			longHandle2.set(structSegmt, 0L, 446688002244L);

			long result = (long)mh.invoke(668800224466L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1339955106732L);
		}
	}

	@Test
	public void test_addLongAndLongsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromNestedStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromNestedStruct,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 135791357913L);
			structSegmt.set(JAVA_LONG, 8, 246802468024L);
			structSegmt.set(JAVA_LONG, 16,112233445566L);

			long result = (long)mh.invoke(778899001122L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1273726272625L);
		}
	}

	@Test
	public void test_addLongAndLongsFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 135791357913L);
			structSegmt.set(JAVA_LONG, 8, 246802468024L);
			structSegmt.set(JAVA_LONG, 16,112233445566L);

			long result = (long)mh.invoke(778899001122L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1273726272625L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedLongArrayByUpcallMH() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, JAVA_LONG);
		GroupLayout structLayout = MemoryLayout.structLayout(longArray.withName("array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructWithNestedLongArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructWithNestedLongArray,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 11111111111L);
			structSegmt.set(JAVA_LONG, 8, 22222222222L);
			structSegmt.set(JAVA_LONG, 16, 33333333333L);

			long result = (long)mh.invoke(44444444444L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111111111110L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedLongArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, JAVA_LONG);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), longArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructWithNestedLongArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructWithNestedLongArray_reverseOrder,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 11111111111L);
			structSegmt.set(JAVA_LONG, 8, 22222222222L);
			structSegmt.set(JAVA_LONG, 16, 33333333333L);

			long result = (long)mh.invoke(44444444444L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111111111110L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout longStruct = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, longStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 11111111111L);
			structSegmt.set(JAVA_LONG, 8, 22222222222L);
			structSegmt.set(JAVA_LONG, 16, 33333333333L);
			structSegmt.set(JAVA_LONG, 24, 44444444444L);
			structSegmt.set(JAVA_LONG, 32, 55555555555L);

			long result = (long)mh.invoke(66666666666L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 233333333331L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout longStruct = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, longStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 11111111111L);
			structSegmt.set(JAVA_LONG, 8, 22222222222L);
			structSegmt.set(JAVA_LONG, 16, 33333333333L);
			structSegmt.set(JAVA_LONG, 24, 44444444444L);
			structSegmt.set(JAVA_LONG, 32, 55555555555L);

			long result = (long)mh.invoke(66666666666L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 233333333331L);
		}
	}

	@Test
	public void test_add2LongStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2LongStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2LongStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			longHandle1.set(structSegmt1, 0L, 987654321987L);
			longHandle2.set(structSegmt1, 0L, 123456789123L);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			longHandle1.set(structSegmt2, 0L, 224466880022L);
			longHandle2.set(structSegmt2, 0L, 113355779911L);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(longHandle1.get(resultSegmt, 0L), 1212121202009L);
			Assert.assertEquals(longHandle2.get(resultSegmt, 0L), 236812569034L);
		}
	}

	@Test
	public void test_add2LongStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2LongStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2LongStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			longHandle1.set(structSegmt1, 0L, 1122334455L);
			longHandle2.set(structSegmt1, 0L, 5566778899L);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			longHandle1.set(structSegmt2, 0L, 9900112233L);
			longHandle2.set(structSegmt2, 0L, 3344556677L);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0), 11022446688L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 8), 8911335576L);
		}
	}

	@Test
	public void test_add3LongStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"), JAVA_LONG.withName("elem3"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle longHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3LongStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3LongStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			longHandle1.set(structSegmt1, 0L, 987654321987L);
			longHandle2.set(structSegmt1, 0L, 123456789123L);
			longHandle3.set(structSegmt1, 0L, 112233445566L);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			longHandle1.set(structSegmt2, 0L, 224466880022L);
			longHandle2.set(structSegmt2, 0L, 113355779911L);
			longHandle3.set(structSegmt2, 0L, 778899001122L);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(longHandle1.get(resultSegmt, 0L), 1212121202009L);
			Assert.assertEquals(longHandle2.get(resultSegmt, 0L), 236812569034L);
			Assert.assertEquals(longHandle3.get(resultSegmt, 0L), 891132446688L);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStruct,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			floatHandle1.set(structSegmt, 0L, 8.12F);
			floatHandle2.set(structSegmt, 0L, 9.24F);

			float result = (float)mh.invoke(6.56F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 23.92F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAnd5FloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"),
				JAVA_FLOAT.withName("elem3"), JAVA_FLOAT.withName("elem4"), JAVA_FLOAT.withName("elem5"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle floatHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));
		VarHandle floatHandle4 = structLayout.varHandle(PathElement.groupElement("elem4"));
		VarHandle floatHandle5 = structLayout.varHandle(PathElement.groupElement("elem5"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAnd5FloatsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAnd5FloatsFromStruct,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			floatHandle1.set(structSegmt, 0L, 1.01F);
			floatHandle2.set(structSegmt, 0L, 1.02F);
			floatHandle3.set(structSegmt, 0L, 1.03F);
			floatHandle4.set(structSegmt, 0L, 1.04F);
			floatHandle5.set(structSegmt, 0L, 1.05F);

			float result = (float)mh.invoke(1.06F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 6.21F, 0.01F);
		}
	}

	@Test
	public void test_addFloatFromPointerAndFloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatFromPointerAndFloatsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatFromPointerAndFloatsFromStruct,
					FunctionDescriptor.of(JAVA_FLOAT, ADDRESS, structLayout), arena);
			MemorySegment floatSegmt = arena.allocateFrom(JAVA_FLOAT, 12.12F);
			MemorySegment structSegmt = arena.allocate(structLayout);
			floatHandle1.set(structSegmt, 0L, 18.23F);
			floatHandle2.set(structSegmt, 0L, 19.34F);

			float result = (float)mh.invoke(floatSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 49.69F, 0.01F);
		}
	}

	@Test
	public void test_addFloatFromPointerAndFloatsFromStruct_returnFloatPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatFromPointerAndFloatsFromStruct_returnFloatPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatFromPointerAndFloatsFromStruct_returnFloatPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment floatSegmt = arena.allocateFrom(JAVA_FLOAT, 12.12F);
			MemorySegment structSegmt = arena.allocate(structLayout);
			floatHandle1.set(structSegmt, 0L, 18.23F);
			floatHandle2.set(structSegmt, 0L, 19.34F);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(floatSegmt, structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_FLOAT.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 49.69F, 0.01F);
			Assert.assertEquals(resultSegmt.address(), floatSegmt.address());
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructPointer,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			floatHandle1.set(structSegmt, 0L, 35.11F);
			floatHandle2.set(structSegmt, 0L, 46.22F);

			float result = (float)mh.invoke(79.33F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 160.66F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromNestedStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromNestedStruct,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 31.22F);
			structSegmt.set(JAVA_FLOAT, 4, 33.44F);
			structSegmt.set(JAVA_FLOAT, 8, 35.66F);

			float result = (float)mh.invoke(37.88F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 138.2F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 31.22F);
			structSegmt.set(JAVA_FLOAT, 4, 33.44F);
			structSegmt.set(JAVA_FLOAT, 8, 35.66F);

			float result = (float)mh.invoke(37.88F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 138.2F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedFloatArrayByUpcallMH() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, JAVA_FLOAT);
		GroupLayout structLayout = MemoryLayout.structLayout(floatArray.withName("array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructWithNestedFloatArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructWithNestedFloatArray,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt.set(JAVA_FLOAT, 8, 333.33F);

			float result = (float)mh.invoke(444.44F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1111.1F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, JAVA_FLOAT);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), floatArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt.set(JAVA_FLOAT, 8, 333.33F);

			float result = (float)mh.invoke(444.44F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1111.1F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout floatStruct = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, floatStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt.set(JAVA_FLOAT, 8, 333.33F);
			structSegmt.set(JAVA_FLOAT, 12, 444.44F);
			structSegmt.set(JAVA_FLOAT, 16, 555.55F);

			float result = (float)mh.invoke(666.66F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 2333.31F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout floatStruct = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, floatStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt.set(JAVA_FLOAT, 8, 333.33F);
			structSegmt.set(JAVA_FLOAT, 12, 444.44F);
			structSegmt.set(JAVA_FLOAT, 16, 555.55F);

			float result = (float)mh.invoke(666.66F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 2333.31F, 0.01F);
		}
	}

	@Test
	public void test_add3FloatStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"),  JAVA_FLOAT.withName("elem3"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle floatHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3FloatStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3FloatStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			floatHandle1.set(structSegmt1, 0L, 25.12F);
			floatHandle2.set(structSegmt1, 0L, 11.23F);
			floatHandle3.set(structSegmt1, 0L, 45.67F);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			floatHandle1.set(structSegmt2, 0L, 24.34F);
			floatHandle2.set(structSegmt2, 0L, 13.45F);
			floatHandle3.set(structSegmt2, 0L, 69.72F);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt, 0L), 49.46F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt, 0L), 24.68F, 0.01F);
			Assert.assertEquals((float)floatHandle3.get(resultSegmt, 0L), 115.39, 0.01F);
		}
	}

	@Test
	public void test_add2FloatStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2FloatStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2FloatStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			floatHandle1.set(structSegmt1, 0L, 25.12F);
			floatHandle2.set(structSegmt1, 0L, 11.23F);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			floatHandle1.set(structSegmt2, 0L, 24.34F);
			floatHandle2.set(structSegmt2, 0L, 13.45F);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt, 0L), 49.46F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt, 0L), 24.68F, 0.01F);
		}
	}

	@Test
	public void test_add2FloatStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2FloatStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2FloatStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			floatHandle1.set(structSegmt1, 0L, 25.12F);
			floatHandle2.set(structSegmt1, 0L, 11.23F);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			floatHandle1.set(structSegmt2, 0L, 24.34F);
			floatHandle2.set(structSegmt2, 0L, 13.45F);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 49.46F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 4), 24.68F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt, 0L, 2228.111D);
			doubleHandle2.set(structSegmt, 0L, 2229.221D);

			double result = (double)mh.invoke(3336.333D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 7793.665D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleFromPointerAndDoublesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleFromPointerAndDoublesFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleFromPointerAndDoublesFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, structLayout), arena);
			MemorySegment doubleSegmt = arena.allocateFrom(JAVA_DOUBLE, 112.123D);
			MemorySegment structSegmt = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt, 0L, 118.456D);
			doubleHandle2.set(structSegmt, 0L, 119.789D);

			double result = (double)mh.invoke(doubleSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 350.368D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleFromPointerAndDoublesFromStruct_returnDoublePointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleFromPointerAndDoublesFromStruct_returnDoublePointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment doubleSegmt = arena.allocateFrom(JAVA_DOUBLE, 212.123D);
			MemorySegment structSegmt = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt, 0L, 218.456D);
			doubleHandle2.set(structSegmt, 0L, 219.789D);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(doubleSegmt, structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_DOUBLE.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0), 650.368D, 0.001D);
			Assert.assertEquals(resultSegmt.address(), doubleSegmt.address());
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructPointer,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt, 0L, 22.111D);
			doubleHandle2.set(structSegmt, 0L, 44.222D);

			double result = (double)mh.invoke(66.333D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 132.666D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_DOUBLE.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromNestedStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromNestedStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 31.789D);
			structSegmt.set(JAVA_DOUBLE, 8, 33.456D);
			structSegmt.set(JAVA_DOUBLE, 16, 35.123D);

			double result = (double)mh.invoke(37.864D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 138.232D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), nestedStructLayout.withName("struct_elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 31.789D);
			structSegmt.set(JAVA_DOUBLE, 8, 33.456D);
			structSegmt.set(JAVA_DOUBLE, 16, 35.123D);

			double result = (double)mh.invoke(37.864D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 138.232D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedDoubleArrayByUpcallMH() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, JAVA_DOUBLE);
		GroupLayout structLayout = MemoryLayout.structLayout(doubleArray.withName("array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructWithNestedDoubleArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructWithNestedDoubleArray,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			structSegmt.set(JAVA_DOUBLE, 8, 222.222D);
			structSegmt.set(JAVA_DOUBLE, 16, 333.333D);

			double result = (double)mh.invoke(444.444D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1111.11D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, JAVA_DOUBLE);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), doubleArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			structSegmt.set(JAVA_DOUBLE, 8, 222.222D);
			structSegmt.set(JAVA_DOUBLE, 16, 333.333D);

			double result = (double)mh.invoke(444.444D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1111.11D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout doubleStruct = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, doubleStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			structSegmt.set(JAVA_DOUBLE, 8, 222.222D);
			structSegmt.set(JAVA_DOUBLE, 16, 333.333D);
			structSegmt.set(JAVA_DOUBLE, 24, 444.444D);
			structSegmt.set(JAVA_DOUBLE, 32, 555.555D);

			double result = (double)mh.invoke(666.666D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 2333.331D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout doubleStruct = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, doubleStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			structSegmt.set(JAVA_DOUBLE, 8, 222.222D);
			structSegmt.set(JAVA_DOUBLE, 16, 333.333D);
			structSegmt.set(JAVA_DOUBLE, 24, 444.444D);
			structSegmt.set(JAVA_DOUBLE, 32, 555.555D);

			double result = (double)mh.invoke(666.666D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 2333.331D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2DoubleStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2DoubleStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 0L, 11.222D);
			doubleHandle2.set(structSegmt1, 0L, 22.333D);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 0L, 33.444D);
			doubleHandle2.set(structSegmt2, 0L, 44.555D);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2,  upcallFuncAddr);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt, 0L), 44.666D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt, 0L), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2DoubleStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2DoubleStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 0L, 11.222D);
			doubleHandle2.set(structSegmt1, 0L, 22.333D);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 0L, 33.444D);
			doubleHandle2.set(structSegmt2, 0L, 44.555D);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0), 44.666D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 8), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_add3DoubleStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"), JAVA_DOUBLE.withName("elem3"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle doubleHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3DoubleStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3DoubleStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 0L, 11.222D);
			doubleHandle2.set(structSegmt1, 0L, 22.333D);
			doubleHandle3.set(structSegmt1, 0L, 33.123D);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 0L, 33.444D);
			doubleHandle2.set(structSegmt2, 0L, 44.555D);
			doubleHandle3.set(structSegmt2, 0L, 55.456D);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt, 0L), 44.666D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt, 0L), 66.888D, 0.001D);
			Assert.assertEquals((double)doubleHandle3.get(resultSegmt, 0L), 88.579D, 0.001D);
		}
	}

	@Test
	public void test_addNegBytesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addNegBytesFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addNegBytesFromStruct,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, JAVA_BYTE, JAVA_BYTE), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			byteHandle1.set(structSegmt, 0L, (byte)-8);
			byteHandle2.set(structSegmt, 0L, (byte)-9);

			byte result = (byte)mh.invoke((byte)-6, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, (byte)-40);
		}
	}

	@Test
	public void test_addNegShortsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addNegShortsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addNegShortsFromStruct,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, JAVA_SHORT, JAVA_SHORT), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			shortHandle1.set(structSegmt, 0L, (short)-888);
			shortHandle2.set(structSegmt, 0L, (short)-999);

			short result = (short)mh.invoke((short)-777, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, (short)-4551);
		}
	}
}
