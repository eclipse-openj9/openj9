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
 * Test cases for JEP 442: Foreign Linker API (Third Preview) for argument/return struct in downcall.
 *
 * Note:
 * [1] the padding elements in the struct are only required by RI or VarHandle (accessing the
 * data address) while they are totally ignored in OpenJ9 given the padding/alignment are
 * computed by libffi automatically in native.
 *
 * [2] the test suite is mainly intended for the following Clinker API:
 * MethodHandle downcallHandle(MemorySegment symbol, FunctionDescriptor function)
 */
@Test(groups = { "level.sanity" })
public class StructTests1 {
	private static boolean isAixOS = System.getProperty("os.name").toLowerCase().contains("aix");
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addBoolAndBoolsFromStructWithXor_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithXor").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			boolHandle1.set(structSegmt, false);
			boolHandle2.set(structSegmt, true);

			boolean result = (boolean)mh.invokeExact(false, structSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolFromPointerAndBoolsFromStructWithXor_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolFromPointerAndBoolsFromStructWithXor").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment boolSegmt = arena.allocate(JAVA_BOOLEAN);
			boolSegmt.set(JAVA_BOOLEAN, 0, true);
			MemorySegment structSegmt = arena.allocate(structLayout);
			boolHandle1.set(structSegmt, false);
			boolHandle2.set(structSegmt, true);

			boolean result = (boolean)mh.invoke(boolSegmt, structSegmt);
			Assert.assertEquals(result, false);
		}
	}

	@Test
	public void test_addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN, JAVA_BOOLEAN);
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment boolSegmt = arena.allocate(JAVA_BOOLEAN);
			boolSegmt.set(JAVA_BOOLEAN, 0, false);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, false);
			structSegmt.set(JAVA_BOOLEAN, 1, true);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(boolSegmt, structSegmt);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_BOOLEAN.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), true);
			Assert.assertEquals(resultSegmt.address(), boolSegmt.address());
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructPointerWithXor_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructPointerWithXor").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			boolHandle1.set(structSegmt, true);
			boolHandle2.set(structSegmt, false);

			boolean result = (boolean)mh.invoke(false, structSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromNestedStructWithXor_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromNestedStructWithXor").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, true);
			structSegmt.set(JAVA_BOOLEAN, 1, false);
			structSegmt.set(JAVA_BOOLEAN, 2, true);

			boolean result = (boolean)mh.invokeExact(true, structSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromNestedStructWithXor_reverseOrder_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromNestedStructWithXor_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, true);
			structSegmt.set(JAVA_BOOLEAN, 1, false);
			structSegmt.set(JAVA_BOOLEAN, 2, true);

			boolean result = (boolean)mh.invokeExact(true, structSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromNestedStructWithXor_withoutLayoutName_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BOOLEAN, JAVA_BOOLEAN);
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout, JAVA_BOOLEAN);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromNestedStructWithXor").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, true);
			structSegmt.set(JAVA_BOOLEAN, 1, false);
			structSegmt.set(JAVA_BOOLEAN, 2, true);

			boolean result = (boolean)mh.invokeExact(true, structSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedBoolArray_1() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, JAVA_BOOLEAN);
		GroupLayout structLayout = MemoryLayout.structLayout(boolArray.withName("array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithNestedBoolArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, false);
			structSegmt.set(JAVA_BOOLEAN, 1, true);
			structSegmt.set(JAVA_BOOLEAN, 2, false);

			boolean result = (boolean)mh.invokeExact(false, structSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder_1() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, JAVA_BOOLEAN);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), boolArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, false);
			structSegmt.set(JAVA_BOOLEAN, 1, true);
			structSegmt.set(JAVA_BOOLEAN, 2, false);

			boolean result = (boolean)mh.invokeExact(false, structSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedBoolArray_withoutLayoutName_1() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, JAVA_BOOLEAN);
		GroupLayout structLayout = MemoryLayout.structLayout(boolArray, JAVA_BOOLEAN);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithNestedBoolArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, true);
			structSegmt.set(JAVA_BOOLEAN, 1, false);
			structSegmt.set(JAVA_BOOLEAN, 2, true);

			boolean result = (boolean)mh.invokeExact(false, structSegmt);
			Assert.assertEquals(result, false);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedStructArray_1() throws Throwable {
		GroupLayout boolStruct = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, boolStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, false);
			structSegmt.set(JAVA_BOOLEAN, 1, true);
			structSegmt.set(JAVA_BOOLEAN, 2, false);
			structSegmt.set(JAVA_BOOLEAN, 3, true);
			structSegmt.set(JAVA_BOOLEAN, 4, false);

			boolean result = (boolean)mh.invokeExact(true, structSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder_1() throws Throwable {
		GroupLayout boolStruct = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, boolStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, false);
			structSegmt.set(JAVA_BOOLEAN, 1, true);
			structSegmt.set(JAVA_BOOLEAN, 2, false);
			structSegmt.set(JAVA_BOOLEAN, 3, true);
			structSegmt.set(JAVA_BOOLEAN, 4, false);

			boolean result = (boolean)mh.invokeExact(true, structSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedStructArray_withoutLayoutName_1() throws Throwable {
		GroupLayout boolStruct = MemoryLayout.structLayout(JAVA_BOOLEAN, JAVA_BOOLEAN);
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, boolStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray, JAVA_BOOLEAN);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, true);
			structSegmt.set(JAVA_BOOLEAN, 1, false);
			structSegmt.set(JAVA_BOOLEAN, 2, true);
			structSegmt.set(JAVA_BOOLEAN, 3, false);
			structSegmt.set(JAVA_BOOLEAN, 4, true);

			boolean result = (boolean)mh.invokeExact(false, structSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_add2BoolStructsWithXor_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolStructsWithXor_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			boolHandle1.set(structSegmt1, true);
			boolHandle2.set(structSegmt1, false);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			boolHandle1.set(structSegmt2, true);
			boolHandle2.set(structSegmt2, true);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(boolHandle1.get(resultSegmt), false);
			Assert.assertEquals(boolHandle2.get(resultSegmt), true);
		}
	}

	@Test
	public void test_add2BoolStructsWithXor_returnStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolStructsWithXor_returnStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			boolHandle1.set(structSegmt1, true);
			boolHandle2.set(structSegmt1, false);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			boolHandle1.set(structSegmt2, true);
			boolHandle2.set(structSegmt2, true);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), false);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 1), true);
		}
	}

	@Test
	public void test_add3BoolStructsWithXor_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"),
				JAVA_BOOLEAN.withName("elem2"), JAVA_BOOLEAN.withName("elem3"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle boolHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3BoolStructsWithXor_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			boolHandle1.set(structSegmt1, true);
			boolHandle2.set(structSegmt1, false);
			boolHandle3.set(structSegmt1, true);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			boolHandle1.set(structSegmt2, true);
			boolHandle2.set(structSegmt2, true);
			boolHandle3.set(structSegmt2, false);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(boolHandle1.get(resultSegmt), false);
			Assert.assertEquals(boolHandle2.get(resultSegmt), true);
			Assert.assertEquals(boolHandle3.get(resultSegmt), true);
		}
	}

	@Test
	public void test_addByteAndBytesFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)8);
			byteHandle2.set(structSegmt, (byte)9);

			byte result = (byte)mh.invokeExact((byte)6, structSegmt);
			Assert.assertEquals(result, 23);
		}
	}

	@Test
	public void test_addByteFromPointerAndBytesFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteFromPointerAndBytesFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment byteSegmt = arena.allocate(JAVA_BYTE, (byte)12);
			MemorySegment structSegmt = arena.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)14);
			byteHandle2.set(structSegmt, (byte)16);

			byte result = (byte)mh.invoke(byteSegmt, structSegmt);
			Assert.assertEquals(result, 42);
		}
	}

	@Test
	public void test_addByteFromPointerAndBytesFromStruct_returnBytePointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteFromPointerAndBytesFromStruct_returnBytePointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment byteSegmt = arena.allocate(JAVA_BYTE, (byte)12);
			MemorySegment structSegmt = arena.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)18);
			byteHandle2.set(structSegmt, (byte)19);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(byteSegmt, structSegmt);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_BYTE.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BYTE, 0), 49);
			Assert.assertEquals(resultSegmt.address(), byteSegmt.address());
		}
	}

	@Test
	public void test_addByteAndBytesFromStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)11);
			byteHandle2.set(structSegmt, (byte)12);

			byte result = (byte)mh.invoke((byte)13, structSegmt);
			Assert.assertEquals(result, 36);
		}
	}

	@Test
	public void test_addByteAndBytesFromNestedStruct_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)11);
			structSegmt.set(JAVA_BYTE, 1, (byte)22);
			structSegmt.set(JAVA_BYTE, 2, (byte)33);

			byte result = (byte)mh.invokeExact((byte)46, structSegmt);
			Assert.assertEquals(result, 112);
		}
	}

	@Test
	public void test_addByteAndBytesFromNestedStruct_reverseOrder_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromNestedStruct_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)12);
			structSegmt.set(JAVA_BYTE, 1, (byte)24);
			structSegmt.set(JAVA_BYTE, 2, (byte)36);

			byte result = (byte)mh.invokeExact((byte)48, structSegmt);
			Assert.assertEquals(result, 120);
		}
	}

	@Test
	public void test_addByteAndBytesFromNestedStruct_withoutLayoutName_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BYTE, JAVA_BYTE);
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout, JAVA_BYTE);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)11);
			structSegmt.set(JAVA_BYTE, 1, (byte)22);
			structSegmt.set(JAVA_BYTE, 2, (byte)33);

			byte result = (byte)mh.invokeExact((byte)46, structSegmt);
			Assert.assertEquals(result, 112);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedByteArray_1() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, JAVA_BYTE);
		GroupLayout structLayout = MemoryLayout.structLayout(byteArray.withName("array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructWithNestedByteArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)11);
			structSegmt.set(JAVA_BYTE, 1, (byte)12);
			structSegmt.set(JAVA_BYTE, 2, (byte)13);

			byte result = (byte)mh.invokeExact((byte)14, structSegmt);
			Assert.assertEquals(result, 50);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedByteArray_reverseOrder_1() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, JAVA_BYTE);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), byteArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructWithNestedByteArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)12);
			structSegmt.set(JAVA_BYTE, 1, (byte)14);
			structSegmt.set(JAVA_BYTE, 2, (byte)16);

			byte result = (byte)mh.invokeExact((byte)18, structSegmt);
			Assert.assertEquals(result, 60);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedByteArray_withoutLayoutName_1() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, JAVA_BYTE);
		GroupLayout structLayout = MemoryLayout.structLayout(byteArray, JAVA_BYTE);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructWithNestedByteArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)11);
			structSegmt.set(JAVA_BYTE, 1, (byte)12);
			structSegmt.set(JAVA_BYTE, 2, (byte)13);

			byte result = (byte)mh.invokeExact((byte)14, structSegmt);
			Assert.assertEquals(result, 50);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedStructArray_1() throws Throwable {
		GroupLayout byteStruct = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, byteStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)11);
			structSegmt.set(JAVA_BYTE, 1, (byte)12);
			structSegmt.set(JAVA_BYTE, 2, (byte)13);
			structSegmt.set(JAVA_BYTE, 3, (byte)14);
			structSegmt.set(JAVA_BYTE, 4, (byte)15);

			byte result = (byte)mh.invokeExact((byte)16, structSegmt);
			Assert.assertEquals(result, 81);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedStructArray_reverseOrder_1() throws Throwable {
		GroupLayout byteStruct = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, byteStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)12);
			structSegmt.set(JAVA_BYTE, 1, (byte)14);
			structSegmt.set(JAVA_BYTE, 2, (byte)16);
			structSegmt.set(JAVA_BYTE, 3, (byte)18);
			structSegmt.set(JAVA_BYTE, 4, (byte)20);

			byte result = (byte)mh.invokeExact((byte)22, structSegmt);
			Assert.assertEquals(result, 102);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedStructArray_withoutLayoutName_1() throws Throwable {
		GroupLayout byteStruct = MemoryLayout.structLayout(JAVA_BYTE, JAVA_BYTE);
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, byteStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray, JAVA_BYTE);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)11);
			structSegmt.set(JAVA_BYTE, 1, (byte)12);
			structSegmt.set(JAVA_BYTE, 2, (byte)13);
			structSegmt.set(JAVA_BYTE, 3, (byte)14);
			structSegmt.set(JAVA_BYTE, 4, (byte)15);

			byte result = (byte)mh.invokeExact((byte)16, structSegmt);
			Assert.assertEquals(result, 81);
		}
	}

	@Test
	public void test_add2ByteStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ByteStructs_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)24);
			byteHandle2.set(structSegmt2, (byte)13);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)24);
		}
	}

	@Test
	public void test_add2ByteStructs_returnStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ByteStructs_returnStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)24);
			byteHandle2.set(structSegmt2, (byte)13);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BYTE, 0), 49);
			Assert.assertEquals(resultSegmt.get(JAVA_BYTE, 1), 24);
		}
	}

	@Test
	public void test_add3ByteStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"),
				JAVA_BYTE.withName("elem2"), JAVA_BYTE.withName("elem3"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle byteHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3ByteStructs_returnStruct").get();
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			byteHandle3.set(structSegmt1, (byte)12);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)24);
			byteHandle2.set(structSegmt2, (byte)13);
			byteHandle3.set(structSegmt2, (byte)16);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)24);
			Assert.assertEquals((byte)byteHandle3.get(resultSegmt), (byte)28);
		}
	}

	@Test
	public void test_addCharAndCharsFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			charHandle1.set(structSegmt, 'A');
			charHandle2.set(structSegmt, 'B');

			char result = (char)mh.invokeExact('C', structSegmt);
			Assert.assertEquals(result, 'D');
		}
	}

	@Test
	public void test_addCharFromPointerAndCharsFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharFromPointerAndCharsFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment charSegmt = arena.allocate(JAVA_CHAR, 'D');
			MemorySegment structSegmt = arena.allocate(structLayout);
			charHandle1.set(structSegmt, 'E');
			charHandle2.set(structSegmt, 'F');

			char result = (char)mh.invoke(charSegmt, structSegmt);
			Assert.assertEquals(result, 'M');
		}
	}

	@Test
	public void test_addCharFromPointerAndCharsFromStruct_returnCharPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharFromPointerAndCharsFromStruct_returnCharPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment charSegmt = arena.allocate(JAVA_CHAR, 'D');
			MemorySegment structSegmt = arena.allocate(structLayout);
			charHandle1.set(structSegmt, 'E');
			charHandle2.set(structSegmt, 'F');

			MemorySegment resultAddr = (MemorySegment)mh.invoke(charSegmt, structSegmt);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_CHAR.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'M');
			Assert.assertEquals(resultSegmt.address(), charSegmt.address());
		}
	}

	@Test
	public void test_addCharAndCharsFromStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			charHandle1.set(structSegmt, 'H');
			charHandle2.set(structSegmt, 'I');

			char result = (char)mh.invoke('G', structSegmt);
			Assert.assertEquals(result, 'V');
		}
	}

	@Test
	public void test_addCharAndCharsFromNestedStruct_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'E');
			structSegmt.set(JAVA_CHAR, 2, 'F');
			structSegmt.set(JAVA_CHAR, 4, 'G');

			char result = (char)mh.invokeExact('H', structSegmt);
			Assert.assertEquals(result, 'W');
		}
	}

	@Test
	public void test_addCharAndCharsFromNestedStruct_reverseOrder_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromNestedStruct_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'E');
			structSegmt.set(JAVA_CHAR, 2, 'F');
			structSegmt.set(JAVA_CHAR, 4, 'G');

			char result = (char)mh.invokeExact('H', structSegmt);
			Assert.assertEquals(result, 'W');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedCharArray_1() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, JAVA_CHAR);
		GroupLayout structLayout = MemoryLayout.structLayout(charArray.withName("array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructWithNestedCharArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'A');
			structSegmt.set(JAVA_CHAR, 2, 'B');
			structSegmt.set(JAVA_CHAR, 4, 'C');

			char result = (char)mh.invokeExact('D', structSegmt);
			Assert.assertEquals(result, 'G');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedCharArray_reverseOrder_1() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, JAVA_CHAR);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), charArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructWithNestedCharArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'A');
			structSegmt.set(JAVA_CHAR, 2, 'B');
			structSegmt.set(JAVA_CHAR, 4, 'C');

			char result = (char)mh.invokeExact('D', structSegmt);
			Assert.assertEquals(result, 'G');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedCharArray_withoutLayoutName_1() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, JAVA_CHAR);
		GroupLayout structLayout = MemoryLayout.structLayout(charArray, JAVA_CHAR);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructWithNestedCharArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'A');
			structSegmt.set(JAVA_CHAR, 2, 'B');
			structSegmt.set(JAVA_CHAR, 4, 'C');

			char result = (char)mh.invokeExact('D', structSegmt);
			Assert.assertEquals(result, 'G');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedStructArray_1() throws Throwable {
		GroupLayout charStruct = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, charStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'E');
			structSegmt.set(JAVA_CHAR, 2, 'F');
			structSegmt.set(JAVA_CHAR, 4, 'G');
			structSegmt.set(JAVA_CHAR, 6, 'H');
			structSegmt.set(JAVA_CHAR, 8, 'I');

			char result = (char)mh.invokeExact('J', structSegmt);
			Assert.assertEquals(result, 'h');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedStructArray_reverseOrder_1() throws Throwable {
		GroupLayout charStruct = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, charStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'E');
			structSegmt.set(JAVA_CHAR, 2, 'F');
			structSegmt.set(JAVA_CHAR, 4, 'G');
			structSegmt.set(JAVA_CHAR, 6, 'H');
			structSegmt.set(JAVA_CHAR, 8, 'I');

			char result = (char)mh.invokeExact('J', structSegmt);
			Assert.assertEquals(result, 'h');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedStructArray_withoutLayoutName_1() throws Throwable {
		GroupLayout charStruct = MemoryLayout.structLayout(JAVA_CHAR, JAVA_CHAR);
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, charStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray, JAVA_CHAR);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'E');
			structSegmt.set(JAVA_CHAR, 2, 'F');
			structSegmt.set(JAVA_CHAR, 4, 'G');
			structSegmt.set(JAVA_CHAR, 6, 'H');
			structSegmt.set(JAVA_CHAR, 8, 'I');

			char result = (char)mh.invokeExact('J', structSegmt);
			Assert.assertEquals(result, 'h');
		}
	}

	@Test
	public void test_add2CharStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2CharStructs_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			charHandle1.set(structSegmt2, 'C');
			charHandle2.set(structSegmt2, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'C');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'E');
		}
	}

	@Test
	public void test_add2CharStructs_returnStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2CharStructs_returnStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			charHandle1.set(structSegmt2, 'C');
			charHandle2.set(structSegmt2, 'D');

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'C');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 2), 'E');
		}
	}

	@Test
	public void test_add3CharStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"),
				JAVA_CHAR.withName("elem2"), JAVA_CHAR.withName("elem3"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle charHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3CharStructs_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			charHandle3.set(structSegmt1, 'C');
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			charHandle1.set(structSegmt2, 'B');
			charHandle2.set(structSegmt2, 'C');
			charHandle3.set(structSegmt2, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'B');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'D');
			Assert.assertEquals(charHandle3.get(resultSegmt), 'F');
		}
	}

	@Test
	public void test_addShortAndShortsFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)8);
			shortHandle2.set(structSegmt, (short)9);

			short result = (short)mh.invokeExact((short)6, structSegmt);
			Assert.assertEquals(result, 23);
		}
	}

	@Test
	public void test_addShortFromPointerAndShortsFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortFromPointerAndShortsFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment shortSegmt = arena.allocate(JAVA_SHORT, (short)12);
			MemorySegment structSegmt = arena.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)18);
			shortHandle2.set(structSegmt, (short)19);

			short result = (short)mh.invoke(shortSegmt, structSegmt);
			Assert.assertEquals(result, 49);
		}
	}

	@Test
	public void test_addShortFromPointerAndShortsFromStruct_returnShortPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortFromPointerAndShortsFromStruct_returnShortPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment shortSegmt = arena.allocate(JAVA_SHORT, (short)12);
			MemorySegment structSegmt = arena.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)18);
			shortHandle2.set(structSegmt, (short)19);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(shortSegmt, structSegmt);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_SHORT.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 49);
			Assert.assertEquals(resultSegmt.address(), shortSegmt.address());
		}
	}

	@Test
	public void test_addShortAndShortsFromStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)22);
			shortHandle2.set(structSegmt, (short)44);

			short result = (short)mh.invoke((short)66, structSegmt);
			Assert.assertEquals(result, 132);
		}
	}

	@Test
	public void test_addShortAndShortsFromNestedStruct_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)31);
			structSegmt.set(JAVA_SHORT, 2, (short)33);
			structSegmt.set(JAVA_SHORT, 4, (short)35);

			short result = (short)mh.invokeExact((short)37, structSegmt);
			Assert.assertEquals(result, 136);
		}
	}

	@Test
	public void test_addShortAndShortsFromNestedStruct_reverseOrder_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromNestedStruct_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)31);
			structSegmt.set(JAVA_SHORT, 2, (short)33);
			structSegmt.set(JAVA_SHORT, 4, (short)35);

			short result = (short)mh.invokeExact((short)37, structSegmt);
			Assert.assertEquals(result, 136);
		}
	}

	@Test
	public void test_addShortAndShortsFromNestedStruct_withoutLayoutName_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_SHORT, JAVA_SHORT);
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout, JAVA_SHORT);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)31);
			structSegmt.set(JAVA_SHORT, 2, (short)33);
			structSegmt.set(JAVA_SHORT, 4, (short)35);

			short result = (short)mh.invokeExact((short)37, structSegmt);
			Assert.assertEquals(result, 136);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedShortArray_1() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, JAVA_SHORT);
		GroupLayout structLayout = MemoryLayout.structLayout(shortArray.withName("array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructWithNestedShortArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)111);
			structSegmt.set(JAVA_SHORT, 2, (short)222);
			structSegmt.set(JAVA_SHORT, 4, (short)333);

			short result = (short)mh.invokeExact((short)444, structSegmt);
			Assert.assertEquals(result, 1110);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedShortArray_reverseOrder_1() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, JAVA_SHORT);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), shortArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructWithNestedShortArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)111);
			structSegmt.set(JAVA_SHORT, 2, (short)222);
			structSegmt.set(JAVA_SHORT, 4, (short)333);

			short result = (short)mh.invokeExact((short)444, structSegmt);
			Assert.assertEquals(result, 1110);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedShortArray_withoutLayoutName_1() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, JAVA_SHORT);
		GroupLayout structLayout = MemoryLayout.structLayout(shortArray, JAVA_SHORT);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructWithNestedShortArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)111);
			structSegmt.set(JAVA_SHORT, 2, (short)222);
			structSegmt.set(JAVA_SHORT, 4, (short)333);

			short result = (short)mh.invokeExact((short)444, structSegmt);
			Assert.assertEquals(result, 1110);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedStructArray_1() throws Throwable {
		GroupLayout shortStruct = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, shortStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struc_array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)111);
			structSegmt.set(JAVA_SHORT, 2, (short)222);
			structSegmt.set(JAVA_SHORT, 4, (short)333);
			structSegmt.set(JAVA_SHORT, 6, (short)444);
			structSegmt.set(JAVA_SHORT, 8, (short)555);

			short result = (short)mh.invokeExact((short)666, structSegmt);
			Assert.assertEquals(result, 2331);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedStructArray_reverseOrder_1() throws Throwable {
		GroupLayout shortStruct = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, shortStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), structArray.withName("struc_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)111);
			structSegmt.set(JAVA_SHORT, 2, (short)222);
			structSegmt.set(JAVA_SHORT, 4, (short)333);
			structSegmt.set(JAVA_SHORT, 6, (short)444);
			structSegmt.set(JAVA_SHORT, 8, (short)555);

			short result = (short)mh.invokeExact((short)666, structSegmt);
			Assert.assertEquals(result, 2331);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedStructArray_withoutLayoutName_1() throws Throwable {
		GroupLayout shortStruct = MemoryLayout.structLayout(JAVA_SHORT, JAVA_SHORT);
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, shortStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray, JAVA_SHORT);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)111);
			structSegmt.set(JAVA_SHORT, 2, (short)222);
			structSegmt.set(JAVA_SHORT, 4, (short)333);
			structSegmt.set(JAVA_SHORT, 6, (short)444);
			structSegmt.set(JAVA_SHORT, 8, (short)555);

			short result = (short)mh.invokeExact((short)666, structSegmt);
			Assert.assertEquals(result, 2331);
		}
	}

	@Test
	public void test_add2ShortStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ShortStructs_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			shortHandle1.set(structSegmt1, (short)56);
			shortHandle2.set(structSegmt1, (short)45);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			shortHandle1.set(structSegmt2, (short)78);
			shortHandle2.set(structSegmt2, (short)67);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)134);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)112);
		}
	}

	@Test
	public void test_add2ShortStructs_returnStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ShortStructs_returnStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			shortHandle1.set(structSegmt1, (short)56);
			shortHandle2.set(structSegmt1, (short)45);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			shortHandle1.set(structSegmt2, (short)78);
			shortHandle2.set(structSegmt2, (short)67);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 134);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 2), 112);
		}
	}

	@Test
	public void test_add3ShortStructs_returnStruct_1() throws Throwable  {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"),
				JAVA_SHORT.withName("elem2"), JAVA_SHORT.withName("elem3"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle shortHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3ShortStructs_returnStruct").get();

		try (Arena arena = Arena.ofConfined()) {
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			MemorySegment structSegmt1 = arena.allocate(structLayout);
			shortHandle1.set(structSegmt1, (short)25);
			shortHandle2.set(structSegmt1, (short)26);
			shortHandle3.set(structSegmt1, (short)27);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			shortHandle1.set(structSegmt2, (short)34);
			shortHandle2.set(structSegmt2, (short)35);
			shortHandle3.set(structSegmt2, (short)36);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)59);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)61);
			Assert.assertEquals((short)shortHandle3.get(resultSegmt), (short)63);
		}
	}

	@Test
	public void test_addIntAndIntsFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			intHandle1.set(structSegmt, 1122334);
			intHandle2.set(structSegmt, 1234567);

			int result = (int)mh.invokeExact(2244668, structSegmt);
			Assert.assertEquals(result, 4601569);
		}
	}

	@Test
	public void test_addIntAndIntShortFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_SHORT.withName("elem2"), MemoryLayout.paddingLayout(JAVA_SHORT.byteSize()));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntShortFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 11223344);
			elemHandle2.set(structSegmt, (short)32766);

			int result = (int)mh.invokeExact(22334455, structSegmt);
			Assert.assertEquals(result, 33590565);
		}
	}

	@Test
	public void test_addIntAndShortIntFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"),
				MemoryLayout.paddingLayout(JAVA_SHORT.byteSize()), JAVA_INT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndShortIntFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, (short)32766);
			elemHandle2.set(structSegmt, 22446688);

			int result = (int)mh.invokeExact(11335577, structSegmt);
			Assert.assertEquals(result, 33815031);
		}
	}

	@Test
	public void test_addIntFromPointerAndIntsFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntFromPointerAndIntsFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment intSegmt = arena.allocate(JAVA_INT, 7654321);
			MemorySegment structSegmt = arena.allocate(structLayout);
			intHandle1.set(structSegmt, 1234567);
			intHandle2.set(structSegmt, 2468024);

			int result = (int)mh.invoke(intSegmt, structSegmt);
			Assert.assertEquals(result, 11356912);
		}
	}

	@Test
	public void test_addIntFromPointerAndIntsFromStruct_returnIntPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntFromPointerAndIntsFromStruct_returnIntPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment intSegmt = arena.allocate(JAVA_INT, 1122333);
			MemorySegment structSegmt = arena.allocate(structLayout);
			intHandle1.set(structSegmt, 4455666);
			intHandle2.set(structSegmt, 7788999);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(intSegmt, structSegmt);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_INT.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 13366998);
			Assert.assertEquals(resultSegmt.address(), intSegmt.address());
		}
	}

	@Test
	public void test_addIntAndIntsFromStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			intHandle1.set(structSegmt, 11121314);
			intHandle2.set(structSegmt, 15161718);

			int result = (int)mh.invoke(19202122, structSegmt);
			Assert.assertEquals(result, 45485154);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedStruct_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_INT.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 21222324);
			structSegmt.set(JAVA_INT, 4, 25262728);
			structSegmt.set(JAVA_INT, 8, 29303132);

			int result = (int)mh.invokeExact(33343536, structSegmt);
			Assert.assertEquals(result, 109131720);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedStruct_reverseOrder_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromNestedStruct_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 21222324);
			structSegmt.set(JAVA_INT, 4, 25262728);
			structSegmt.set(JAVA_INT, 8, 29303132);

			int result = (int)mh.invokeExact(33343536, structSegmt);
			Assert.assertEquals(result, 109131720);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedStruct_withoutLayoutName_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_INT, JAVA_INT);
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout, JAVA_INT);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 21222324);
			structSegmt.set(JAVA_INT, 4, 25262728);
			structSegmt.set(JAVA_INT, 8, 29303132);

			int result = (int)mh.invokeExact(33343536, structSegmt);
			Assert.assertEquals(result, 109131720);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedIntArray_1() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, JAVA_INT);
		GroupLayout structLayout = MemoryLayout.structLayout(intArray.withName("array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructWithNestedIntArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 1111111);
			structSegmt.set(JAVA_INT, 4, 2222222);
			structSegmt.set(JAVA_INT, 8, 3333333);

			int result = (int)mh.invokeExact(4444444, structSegmt);
			Assert.assertEquals(result, 11111110);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedIntArray_reverseOrder_1() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, JAVA_INT);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), intArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructWithNestedIntArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 1111111);
			structSegmt.set(JAVA_INT, 4, 2222222);
			structSegmt.set(JAVA_INT, 8, 3333333);

			int result = (int)mh.invokeExact(4444444, structSegmt);
			Assert.assertEquals(result, 11111110);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedIntArray_withoutLayoutName_1() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, JAVA_INT);
		GroupLayout structLayout = MemoryLayout.structLayout(intArray, JAVA_INT);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructWithNestedIntArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 1111111);
			structSegmt.set(JAVA_INT, 4, 2222222);
			structSegmt.set(JAVA_INT, 8, 3333333);

			int result = (int)mh.invokeExact(4444444, structSegmt);
			Assert.assertEquals(result, 11111110);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedStructArray_1() throws Throwable {
		GroupLayout intStruct = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, intStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 1111111);
			structSegmt.set(JAVA_INT, 4, 2222222);
			structSegmt.set(JAVA_INT, 8, 3333333);
			structSegmt.set(JAVA_INT, 12, 4444444);
			structSegmt.set(JAVA_INT, 16, 5555555);

			int result = (int)mh.invokeExact(6666666, structSegmt);
			Assert.assertEquals(result, 23333331);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedStructArray_reverseOrder_1() throws Throwable {
		GroupLayout intStruct = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, intStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 1111111);
			structSegmt.set(JAVA_INT, 4, 2222222);
			structSegmt.set(JAVA_INT, 8, 3333333);
			structSegmt.set(JAVA_INT, 12, 4444444);
			structSegmt.set(JAVA_INT, 16, 5555555);

			int result = (int)mh.invokeExact(6666666, structSegmt);
			Assert.assertEquals(result, 23333331);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedStructArray_withoutLayoutName_1() throws Throwable {
		GroupLayout intStruct = MemoryLayout.structLayout(JAVA_INT, JAVA_INT);
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, intStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray, JAVA_INT);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 1111111);
			structSegmt.set(JAVA_INT, 4, 2222222);
			structSegmt.set(JAVA_INT, 8, 3333333);
			structSegmt.set(JAVA_INT, 12, 4444444);
			structSegmt.set(JAVA_INT, 16, 5555555);

			int result = (int)mh.invokeExact(6666666, structSegmt);
			Assert.assertEquals(result, 23333331);
		}
	}

	@Test
	public void test_add2IntStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(intHandle1.get(resultSegmt), 110224466);
			Assert.assertEquals(intHandle2.get(resultSegmt), 89113354);
		}
	}

	@Test
	public void test_add2IntStructs_returnStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 110224466);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 4), 89113354);
		}
	}

	@Test
	public void test_add3IntStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"), JAVA_INT.withName("elem3"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle intHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3IntStructs_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			intHandle3.set(structSegmt1, 99001122);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 33445566);
			intHandle2.set(structSegmt2, 77889900);
			intHandle3.set(structSegmt2, 44332211);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(intHandle1.get(resultSegmt), 44668910);
			Assert.assertEquals(intHandle2.get(resultSegmt), 133557688);
			Assert.assertEquals(intHandle3.get(resultSegmt), 143333333);
		}
	}

	@Test
	public void test_addLongAndLongsFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			longHandle1.set(structSegmt, 1234567890L);
			longHandle2.set(structSegmt, 9876543210L);

			long result = (long)mh.invokeExact(2468024680L, structSegmt);
			Assert.assertEquals(result, 13579135780L);
		}
	}

	@Test
	public void test_addIntAndIntLongFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				MemoryLayout.paddingLayout(JAVA_INT.byteSize()), JAVA_LONG.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntLongFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 11223344);
			elemHandle2.set(structSegmt, 667788990011L);

			long result = (long)mh.invokeExact(22446688, structSegmt);
			Assert.assertEquals(result, 667822660043L);
		}
	}

	@Test
	public void test_addIntAndLongIntFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"),
				JAVA_INT.withName("elem2"), MemoryLayout.paddingLayout(JAVA_INT.byteSize()));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndLongIntFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 667788990011L);
			elemHandle2.set(structSegmt, 11223344);

			long result = (long)mh.invokeExact(1234567, structSegmt);
			Assert.assertEquals(result, 667801447922L);
		}
	}

	@Test
	public void test_addLongFromPointerAndLongsFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongFromPointerAndLongsFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment longSegmt = arena.allocate(JAVA_LONG, 1111111111L);
			MemorySegment structSegmt = arena.allocate(structLayout);
			longHandle1.set(structSegmt, 3333333333L);
			longHandle2.set(structSegmt, 5555555555L);

			long result = (long)mh.invoke(longSegmt, structSegmt);
			Assert.assertEquals(result, 9999999999L);
		}
	}

	@Test
	public void test_addLongFromPointerAndLongsFromStruct_returnLongPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongFromPointerAndLongsFromStruct_returnLongPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment longSegmt = arena.allocate(JAVA_LONG, 1122334455L);
			MemorySegment structSegmt = arena.allocate(structLayout);
			longHandle1.set(structSegmt, 6677889900L);
			longHandle2.set(structSegmt, 1234567890L);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(longSegmt, structSegmt);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_LONG.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0), 9034792245L);
			Assert.assertEquals(resultSegmt.address(), longSegmt.address());
		}
	}

	@Test
	public void test_addLongAndLongsFromStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			longHandle1.set(structSegmt, 224466880022L);
			longHandle2.set(structSegmt, 446688002244L);

			long result = (long)mh.invoke(668800224466L, structSegmt);
			Assert.assertEquals(result, 1339955106732L);
		}
	}

	@Test
	public void test_addLongAndLongsFromNestedStruct_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 135791357913L);
			structSegmt.set(JAVA_LONG, 8, 246802468024L);
			structSegmt.set(JAVA_LONG, 16,112233445566L);

			long result = (long)mh.invokeExact(778899001122L, structSegmt);
			Assert.assertEquals(result, 1273726272625L);
		}
	}

	@Test
	public void test_addLongAndLongsFromNestedStruct_reverseOrder_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromNestedStruct_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 135791357913L);
			structSegmt.set(JAVA_LONG, 8, 246802468024L);
			structSegmt.set(JAVA_LONG, 16,112233445566L);

			long result = (long)mh.invokeExact(778899001122L, structSegmt);
			Assert.assertEquals(result, 1273726272625L);
		}
	}

	@Test
	public void test_addLongAndLongsFromNestedStruct_withoutLayoutName_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_LONG, JAVA_LONG);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG, nestedStructLayout);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 135791357913L);
			structSegmt.set(JAVA_LONG, 8, 246802468024L);
			structSegmt.set(JAVA_LONG, 16,112233445566L);

			long result = (long)mh.invokeExact(778899001122L, structSegmt);
			Assert.assertEquals(result, 1273726272625L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedLongArray_1() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, JAVA_LONG);
		GroupLayout structLayout = MemoryLayout.structLayout(longArray.withName("array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructWithNestedLongArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 111111111L);
			structSegmt.set(JAVA_LONG, 8, 222222222L);
			structSegmt.set(JAVA_LONG, 16, 333333333L);

			long result = (long)mh.invokeExact(444444444L, structSegmt);
			Assert.assertEquals(result, 1111111110L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedLongArray_reverseOrder_1() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, JAVA_LONG);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), longArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructWithNestedLongArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 111111111L);
			structSegmt.set(JAVA_LONG, 8, 222222222L);
			structSegmt.set(JAVA_LONG, 16, 333333333L);

			long result = (long)mh.invokeExact(444444444L, structSegmt);
			Assert.assertEquals(result, 1111111110L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedLongArray_withoutLayoutName_1() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, JAVA_LONG);
		GroupLayout structLayout = MemoryLayout.structLayout(longArray, JAVA_LONG);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructWithNestedLongArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 111111111L);
			structSegmt.set(JAVA_LONG, 8, 222222222L);
			structSegmt.set(JAVA_LONG, 16, 333333333L);

			long result = (long)mh.invokeExact(444444444L, structSegmt);
			Assert.assertEquals(result, 1111111110L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedStructArray_1() throws Throwable {
		GroupLayout longStruct = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, longStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 111111111L);
			structSegmt.set(JAVA_LONG, 8, 222222222L);
			structSegmt.set(JAVA_LONG, 16, 333333333L);
			structSegmt.set(JAVA_LONG, 24, 444444444L);
			structSegmt.set(JAVA_LONG, 32, 555555555L);

			long result = (long)mh.invokeExact(666666666L, structSegmt);
			Assert.assertEquals(result, 2333333331L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedStructArray_reverseOrder_1() throws Throwable {
		GroupLayout longStruct = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, longStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 111111111L);
			structSegmt.set(JAVA_LONG, 8, 222222222L);
			structSegmt.set(JAVA_LONG, 16, 333333333L);
			structSegmt.set(JAVA_LONG, 24, 444444444L);
			structSegmt.set(JAVA_LONG, 32, 555555555L);

			long result = (long)mh.invokeExact(666666666L, structSegmt);
			Assert.assertEquals(result, 2333333331L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedStructArray_withoutLayoutName_1() throws Throwable {
		GroupLayout longStruct = MemoryLayout.structLayout(JAVA_LONG, JAVA_LONG);
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, longStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray, JAVA_LONG);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 111111111L);
			structSegmt.set(JAVA_LONG, 8, 222222222L);
			structSegmt.set(JAVA_LONG, 16, 333333333L);
			structSegmt.set(JAVA_LONG, 24, 444444444L);
			structSegmt.set(JAVA_LONG, 32, 555555555L);

			long result = (long)mh.invokeExact(666666666L, structSegmt);
			Assert.assertEquals(result, 2333333331L);
		}
	}

	@Test
	public void test_add2LongStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2LongStructs_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			longHandle1.set(structSegmt2, 224466880022L);
			longHandle2.set(structSegmt2, 113355779911L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(longHandle1.get(resultSegmt), 1212121202009L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 236812569034L);
		}
	}

	@Test
	public void test_add2LongStructs_returnStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2LongStructs_returnStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 5566778899L);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			longHandle1.set(structSegmt2, 9900112233L);
			longHandle2.set(structSegmt2, 3344556677L);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0), 11022446688L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 8), 8911335576L);
		}
	}

	@Test
	public void test_add3LongStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"), JAVA_LONG.withName("elem3"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle longHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3LongStructs_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			longHandle3.set(structSegmt1, 112233445566L);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			longHandle1.set(structSegmt2, 224466880022L);
			longHandle2.set(structSegmt2, 113355779911L);
			longHandle3.set(structSegmt2, 778899001122L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(longHandle1.get(resultSegmt), 1212121202009L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 236812569034L);
			Assert.assertEquals(longHandle3.get(resultSegmt), 891132446688L);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			floatHandle1.set(structSegmt, 8.12F);
			floatHandle2.set(structSegmt, 9.24F);

			float result = (float)mh.invokeExact(6.56F, structSegmt);
			Assert.assertEquals(result, 23.92F, 0.01F);
		}
	}

	@Test
	public void test_addFloatFromPointerAndFloatsFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatFromPointerAndFloatsFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment floatSegmt = arena.allocate(JAVA_FLOAT, 12.12F);
			MemorySegment structSegmt = arena.allocate(structLayout);
			floatHandle1.set(structSegmt, 18.23F);
			floatHandle2.set(structSegmt, 19.34F);

			float result = (float)mh.invoke(floatSegmt, structSegmt);
			Assert.assertEquals(result, 49.69F, 0.01F);
		}
	}

	@Test
	public void test_addFloatFromPointerAndFloatsFromStruct_returnFloatPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatFromPointerAndFloatsFromStruct_returnFloatPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment floatSegmt = arena.allocate(JAVA_FLOAT, 12.12F);
			MemorySegment structSegmt = arena.allocate(structLayout);
			floatHandle1.set(structSegmt, 18.23F);
			floatHandle2.set(structSegmt, 19.34F);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(floatSegmt, structSegmt);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_FLOAT.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 49.69F, 0.01F);
			Assert.assertEquals(resultSegmt.address(), floatSegmt.address());
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			floatHandle1.set(structSegmt, 35.11F);
			floatHandle2.set(structSegmt, 46.22F);

			float result = (float)mh.invoke(79.33F, structSegmt);
			Assert.assertEquals(result, 160.66F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromNestedStruct_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 31.22F);
			structSegmt.set(JAVA_FLOAT, 4, 33.44F);
			structSegmt.set(JAVA_FLOAT, 8, 35.66F);

			float result = (float)mh.invokeExact(37.88F, structSegmt);
			Assert.assertEquals(result, 138.2F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromNestedStruct_reverseOrder_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromNestedStruct_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 31.22F);
			structSegmt.set(JAVA_FLOAT, 4, 33.44F);
			structSegmt.set(JAVA_FLOAT, 8, 35.66F);

			float result = (float)mh.invokeExact(37.88F, structSegmt);
			Assert.assertEquals(result, 138.2F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromNestedStruct_withoutLayoutName_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_FLOAT, JAVA_FLOAT);
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout, JAVA_FLOAT);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 31.22F);
			structSegmt.set(JAVA_FLOAT, 4, 33.44F);
			structSegmt.set(JAVA_FLOAT, 8, 35.66F);

			float result = (float)mh.invokeExact(37.88F, structSegmt);
			Assert.assertEquals(result, 138.2F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedFloatArray_1() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, JAVA_FLOAT);
		GroupLayout structLayout = MemoryLayout.structLayout(floatArray.withName("array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructWithNestedFloatArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt.set(JAVA_FLOAT, 8, 333.33F);

			float result = (float)mh.invokeExact(444.44F, structSegmt);
			Assert.assertEquals(result, 1111.1F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder_1() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, JAVA_FLOAT);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), floatArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt.set(JAVA_FLOAT, 8, 333.33F);

			float result = (float)mh.invokeExact(444.44F, structSegmt);
			Assert.assertEquals(result, 1111.1F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedFloatArray_withoutLayoutName_1() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, JAVA_FLOAT);
		GroupLayout structLayout = MemoryLayout.structLayout(floatArray, JAVA_FLOAT);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructWithNestedFloatArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt.set(JAVA_FLOAT, 8, 333.33F);

			float result = (float)mh.invokeExact(444.44F, structSegmt);
			Assert.assertEquals(result, 1111.1F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedStructArray_1() throws Throwable {
		GroupLayout floatStruct = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, floatStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt.set(JAVA_FLOAT, 8, 333.33F);
			structSegmt.set(JAVA_FLOAT, 12, 444.44F);
			structSegmt.set(JAVA_FLOAT, 16, 555.55F);

			float result = (float)mh.invokeExact(666.66F, structSegmt);
			Assert.assertEquals(result, 2333.31F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder_1() throws Throwable {
		GroupLayout floatStruct = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, floatStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt.set(JAVA_FLOAT, 8, 333.33F);
			structSegmt.set(JAVA_FLOAT, 12, 444.44F);
			structSegmt.set(JAVA_FLOAT, 16, 555.55F);

			float result = (float)mh.invokeExact(666.66F, structSegmt);
			Assert.assertEquals(result, 2333.31F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedStructArray_withoutLayoutName_1() throws Throwable {
		GroupLayout floatStruct = MemoryLayout.structLayout(JAVA_FLOAT, JAVA_FLOAT);
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, floatStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray, JAVA_FLOAT);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt.set(JAVA_FLOAT, 8, 333.33F);
			structSegmt.set(JAVA_FLOAT, 12, 444.44F);
			structSegmt.set(JAVA_FLOAT, 16, 555.55F);

			float result = (float)mh.invokeExact(666.66F, structSegmt);
			Assert.assertEquals(result, 2333.31F, 0.01F);
		}
	}

	@Test
	public void test_add2FloatStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2FloatStructs_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			floatHandle1.set(structSegmt2, 24.34F);
			floatHandle2.set(structSegmt2, 13.45F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 49.46F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 24.68F, 0.01F);
		}
	}

	@Test
	public void test_add2FloatStructs_returnStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2FloatStructs_returnStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			floatHandle1.set(structSegmt2, 24.34F);
			floatHandle2.set(structSegmt2, 13.45F);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 49.46F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 4), 24.68F, 0.01F);
		}
	}

	@Test
	public void test_add3FloatStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"),  JAVA_FLOAT.withName("elem3"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle floatHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3FloatStructs_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			floatHandle3.set(structSegmt1, 45.67F);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			floatHandle1.set(structSegmt2, 24.34F);
			floatHandle2.set(structSegmt2, 13.45F);
			floatHandle3.set(structSegmt2, 69.72F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 49.46F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 24.68F, 0.01F);
			Assert.assertEquals((float)floatHandle3.get(resultSegmt), 115.39, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt, 2228.111D);
			doubleHandle2.set(structSegmt, 2229.221D);

			double result = (double)mh.invokeExact(3336.333D, structSegmt);
			Assert.assertEquals(result, 7793.665D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatDoubleFromStruct_1() throws Throwable {
		/* The size of [float, double] on AIX/PPC 64-bit is 12 bytes without padding by default
		 * while the same struct is 16 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2")) : MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
						MemoryLayout.paddingLayout(JAVA_FLOAT.byteSize()), JAVA_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 18.444F);
			elemHandle2.set(structSegmt, 619.777D);

			FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
			MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatDoubleFromStruct").get();
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			double result = (double)mh.invokeExact(113.567D, structSegmt);
			Assert.assertEquals(result, 751.788D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndIntDoubleFromStruct_1() throws Throwable {
		/* The size of [int, double] on AIX/PPC 64-bit is 12 bytes without padding by default
		 * while the same struct is 16 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2")) : MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
						MemoryLayout.paddingLayout(JAVA_INT.byteSize()), JAVA_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 18);
			elemHandle2.set(structSegmt, 619.777D);

			FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
			MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndIntDoubleFromStruct").get();
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			double result = (double)mh.invokeExact(113.567D, structSegmt);
			Assert.assertEquals(result, 751.344D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFloatFromStruct_1() throws Throwable {
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"),
				JAVA_FLOAT.withName("elem2")) :  MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"),
						JAVA_FLOAT.withName("elem2"), MemoryLayout.paddingLayout(JAVA_FLOAT.byteSize()));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFloatFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 218.555D);
			elemHandle2.set(structSegmt, 19.22F);

			double result = (double)mh.invokeExact(216.666D, structSegmt);
			Assert.assertEquals(result, 454.441D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleIntFromStruct_1() throws Throwable {
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"),
				JAVA_INT.withName("elem2")) :  MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"),
						JAVA_INT.withName("elem2"), MemoryLayout.paddingLayout(JAVA_INT.byteSize()));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleIntFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 218.555D);
			elemHandle2.set(structSegmt, 19);

			double result = (double)mh.invokeExact(216.666D, structSegmt);
			Assert.assertEquals(result, 454.221D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleFromPointerAndDoublesFromStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleFromPointerAndDoublesFromStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment doubleSegmt = arena.allocate(JAVA_DOUBLE, 112.123D);
			MemorySegment structSegmt = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt, 118.456D);
			doubleHandle2.set(structSegmt, 119.789D);

			double result = (double)mh.invoke(doubleSegmt, structSegmt);
			Assert.assertEquals(result, 350.368D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment doubleSegmt = arena.allocate(JAVA_DOUBLE, 212.123D);
			MemorySegment structSegmt = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt, 218.456D);
			doubleHandle2.set(structSegmt, 219.789D);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(doubleSegmt, structSegmt);
			MemorySegment resultSegmt = resultAddr.reinterpret(JAVA_DOUBLE.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0), 650.368D, 0.001D);
			Assert.assertEquals(resultSegmt.address(), doubleSegmt.address());
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt, 22.111D);
			doubleHandle2.set(structSegmt, 44.222D);

			double result = (double)mh.invoke(66.333D, structSegmt);
			Assert.assertEquals(result, 132.666D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedStruct_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_DOUBLE.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 31.789D);
			structSegmt.set(JAVA_DOUBLE, 8, 33.456D);
			structSegmt.set(JAVA_DOUBLE, 16, 35.123D);

			double result = (double)mh.invokeExact(37.864D, structSegmt);
			Assert.assertEquals(result, 138.232D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedStruct_reverseOrder_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), nestedStructLayout.withName("struct_elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromNestedStruct_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 31.789D);
			structSegmt.set(JAVA_DOUBLE, 8, 33.456D);
			structSegmt.set(JAVA_DOUBLE, 16, 35.123D);

			double result = (double)mh.invokeExact(37.864D, structSegmt);
			Assert.assertEquals(result, 138.232D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedStruct_withoutLayoutName_1() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_DOUBLE, JAVA_DOUBLE);
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout, JAVA_DOUBLE);

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromNestedStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 31.789D);
			structSegmt.set(JAVA_DOUBLE, 8, 33.456D);
			structSegmt.set(JAVA_DOUBLE, 16, 35.123D);

			double result = (double)mh.invokeExact(37.864D, structSegmt);
			Assert.assertEquals(result, 138.232D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedDoubleArray_1() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, JAVA_DOUBLE);
		GroupLayout structLayout = MemoryLayout.structLayout(doubleArray.withName("array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructWithNestedDoubleArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			structSegmt.set(JAVA_DOUBLE, 8, 222.222D);
			structSegmt.set(JAVA_DOUBLE, 16, 333.333D);

			double result = (double)mh.invokeExact(444.444D, structSegmt);
			Assert.assertEquals(result, 1111.11D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder_1() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, JAVA_DOUBLE);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), doubleArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			structSegmt.set(JAVA_DOUBLE, 8, 222.222D);
			structSegmt.set(JAVA_DOUBLE, 16, 333.333D);

			double result = (double)mh.invokeExact(444.444D, structSegmt);
			Assert.assertEquals(result, 1111.11D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedDoubleArray_withoutLayoutName_1() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, JAVA_DOUBLE);
		GroupLayout structLayout = MemoryLayout.structLayout(doubleArray, JAVA_DOUBLE);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructWithNestedDoubleArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			structSegmt.set(JAVA_DOUBLE, 8, 222.222D);
			structSegmt.set(JAVA_DOUBLE, 16, 333.333D);

			double result = (double)mh.invokeExact(444.444D, structSegmt);
			Assert.assertEquals(result, 1111.11D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedStructArray_1() throws Throwable {
		GroupLayout doubleStruct = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, doubleStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			structSegmt.set(JAVA_DOUBLE, 8, 222.222D);
			structSegmt.set(JAVA_DOUBLE, 16, 333.333D);
			structSegmt.set(JAVA_DOUBLE, 24, 444.444D);
			structSegmt.set(JAVA_DOUBLE, 32, 555.555D);

			double result = (double)mh.invokeExact(666.666D, structSegmt);
			Assert.assertEquals(result, 2333.331D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder_1() throws Throwable {
		GroupLayout doubleStruct = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, doubleStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), structArray.withName("struct_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			structSegmt.set(JAVA_DOUBLE, 8, 222.222D);
			structSegmt.set(JAVA_DOUBLE, 16, 333.333D);
			structSegmt.set(JAVA_DOUBLE, 24, 444.444D);
			structSegmt.set(JAVA_DOUBLE, 32, 555.555D);

			double result = (double)mh.invokeExact(666.666D, structSegmt);
			Assert.assertEquals(result, 2333.331D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedStructArray_withoutLayoutName_1() throws Throwable {
		GroupLayout doubleStruct = MemoryLayout.structLayout(JAVA_DOUBLE, JAVA_DOUBLE);
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, doubleStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray, JAVA_DOUBLE);
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructWithNestedStructArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			structSegmt.set(JAVA_DOUBLE, 8, 222.222D);
			structSegmt.set(JAVA_DOUBLE, 16, 333.333D);
			structSegmt.set(JAVA_DOUBLE, 24, 444.444D);
			structSegmt.set(JAVA_DOUBLE, 32, 555.555D);

			double result = (double)mh.invokeExact(666.666D, structSegmt);
			Assert.assertEquals(result, 2333.331D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2DoubleStructs_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 33.444D);
			doubleHandle2.set(structSegmt2, 44.555D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 44.666D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleStructs_returnStructPointer_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2DoubleStructs_returnStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 33.444D);
			doubleHandle2.set(structSegmt2, 44.555D);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0), 44.666D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 8), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_add3DoubleStructs_returnStruct_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"), JAVA_DOUBLE.withName("elem3"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle doubleHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3DoubleStructs_returnStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			doubleHandle3.set(structSegmt1, 33.123D);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 33.444D);
			doubleHandle2.set(structSegmt2, 44.555D);
			doubleHandle3.set(structSegmt2, 55.456D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 44.666D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 66.888D, 0.001D);
			Assert.assertEquals((double)doubleHandle3.get(resultSegmt), 88.579D, 0.001D);
		}
	}
}
