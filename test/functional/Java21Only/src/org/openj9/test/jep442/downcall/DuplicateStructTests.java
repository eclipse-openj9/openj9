/*
 * Copyright IBM Corp. and others 2024
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
 * Test cases for JEP 442: Foreign Linker API for argument/return struct in downcall.
 *
 * Note:
 * The test suite is mainly intended for duplicate structs/nested struct in arguments
 * and return type in downcall.
 */
@Test(groups = { "level.sanity" })
public class DuplicateStructTests {
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addBoolStructsWithXor_dupStruct() throws Throwable {
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
	public void test_addNestedBoolStructsWithXor_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedBoolStructsWithXor_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_BOOLEAN, 0, true);
			structSegmt1.set(JAVA_BOOLEAN, 1, false);
			structSegmt1.set(JAVA_BOOLEAN, 2, true);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_BOOLEAN, 0, false);
			structSegmt2.set(JAVA_BOOLEAN, 1, true);
			structSegmt2.set(JAVA_BOOLEAN, 2, true);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), true);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 1), true);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 2), false);
		}
	}

	@Test
	public void test_addBoolStruct1AndNestedBoolStruct2WithXor_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolStruct1AndNestedBoolStruct2WithXor_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			boolHandle1.set(structSegmt1, true);
			boolHandle2.set(structSegmt1, false);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_BOOLEAN, 0, true);
			structSegmt2.set(JAVA_BOOLEAN, 1, false);
			structSegmt2.set(JAVA_BOOLEAN, 2, true);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(boolHandle1.get(resultSegmt), false);
			Assert.assertEquals(boolHandle2.get(resultSegmt), true);
		}
	}

	@Test
	public void test_addBoolStruct1AndNestedBoolStruct2WithXor_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolStruct1AndNestedBoolStruct2WithXor_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			boolHandle1.set(structSegmt1, true);
			boolHandle2.set(structSegmt1, false);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_BOOLEAN, 0, true);
			structSegmt2.set(JAVA_BOOLEAN, 1, false);
			structSegmt2.set(JAVA_BOOLEAN, 2, true);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), false);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 1), false);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 2), true);
		}
	}

	@Test
	public void test_addNestedBoolArrayStructsWithXor_dupStruct() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, JAVA_BOOLEAN);
		GroupLayout structLayout = MemoryLayout.structLayout(boolArray.withName("array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedBoolArrayStructsWithXor_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_BOOLEAN, 0, true);
			structSegmt1.set(JAVA_BOOLEAN, 1, false);
			structSegmt1.set(JAVA_BOOLEAN, 2, true);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_BOOLEAN, 0, false);
			structSegmt2.set(JAVA_BOOLEAN, 1, true);
			structSegmt2.set(JAVA_BOOLEAN, 2, true);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), true);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 1), true);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 2), false);
		}
	}

	@Test
	public void test_addBoolStruct1AndNestedBoolArrayStruct2WithXor_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, JAVA_BOOLEAN);
		GroupLayout structLayout2 = MemoryLayout.structLayout(boolArray.withName("array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolStruct1AndNestedBoolArrayStruct2WithXor_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			boolHandle1.set(structSegmt1, true);
			boolHandle2.set(structSegmt1, false);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_BOOLEAN, 0, false);
			structSegmt2.set(JAVA_BOOLEAN, 1, true);
			structSegmt2.set(JAVA_BOOLEAN, 2, false);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(boolHandle1.get(resultSegmt), true);
			Assert.assertEquals(boolHandle2.get(resultSegmt), true);
		}
	}

	@Test
	public void test_addBoolStruct1AndNestedBoolArrayStruct2WithXor_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, JAVA_BOOLEAN);
		GroupLayout structLayout2 = MemoryLayout.structLayout(boolArray.withName("array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolStruct1AndNestedBoolArrayStruct2WithXor_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			boolHandle1.set(structSegmt1, true);
			boolHandle2.set(structSegmt1, false);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_BOOLEAN, 0, false);
			structSegmt2.set(JAVA_BOOLEAN, 1, true);
			structSegmt2.set(JAVA_BOOLEAN, 2, false);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), true);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 1), true);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 2), false);
		}
	}

	@Test
	public void test_addNestedBoolStructArrayStructsWithXor_dupStruct() throws Throwable {
		GroupLayout boolStruct = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, boolStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedBoolStructArrayStructsWithXor_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_BOOLEAN, 0, false);
			structSegmt1.set(JAVA_BOOLEAN, 1, true);
			structSegmt1.set(JAVA_BOOLEAN, 2, false);
			structSegmt1.set(JAVA_BOOLEAN, 3, true);
			structSegmt1.set(JAVA_BOOLEAN, 4, false);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_BOOLEAN, 0, true);
			structSegmt2.set(JAVA_BOOLEAN, 1, false);
			structSegmt2.set(JAVA_BOOLEAN, 2, true);
			structSegmt2.set(JAVA_BOOLEAN, 3, false);
			structSegmt2.set(JAVA_BOOLEAN, 4, false);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), true);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 1), true);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 2), true);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 3), true);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 4), false);
		}
	}

	@Test
	public void test_addBoolStruct1AndNestedBoolStructArrayStruct2WithXor_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolStruct1AndNestedBoolStructArrayStruct2WithXor_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			boolHandle1.set(structSegmt1, true);
			boolHandle2.set(structSegmt1, false);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_BOOLEAN, 0, true);
			structSegmt2.set(JAVA_BOOLEAN, 1, false);
			structSegmt2.set(JAVA_BOOLEAN, 2, true);
			structSegmt2.set(JAVA_BOOLEAN, 3, false);
			structSegmt2.set(JAVA_BOOLEAN, 4, true);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(boolHandle1.get(resultSegmt), true);
			Assert.assertEquals(boolHandle2.get(resultSegmt), true);
		}
	}

	@Test
	public void test_addBoolStruct1AndNestedBoolStructArrayStruct2WithXor_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolStruct1AndNestedBoolStructArrayStruct2WithXor_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			boolHandle1.set(structSegmt1, true);
			boolHandle2.set(structSegmt1, false);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_BOOLEAN, 0, true);
			structSegmt2.set(JAVA_BOOLEAN, 1, false);
			structSegmt2.set(JAVA_BOOLEAN, 2, false);
			structSegmt2.set(JAVA_BOOLEAN, 3, false);
			structSegmt2.set(JAVA_BOOLEAN, 4, true);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), false);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 1), false);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 2), true);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 3), false);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 4), true);
		}
	}

	@Test
	public void test_addByteStructs_dupStruct() throws Throwable {
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
	public void test_addNestedByteStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedByteStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_BYTE, 0, (byte)11);
			structSegmt1.set(JAVA_BYTE, 1, (byte)22);
			structSegmt1.set(JAVA_BYTE, 2, (byte)33);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_BYTE, 0, (byte)12);
			structSegmt2.set(JAVA_BYTE, 1, (byte)23);
			structSegmt2.set(JAVA_BYTE, 2, (byte)34);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 0), (byte)23);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 1), (byte)45);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 2), (byte)67);
		}
	}

	@Test
	public void test_addByteStruct1AndNestedByteStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteStruct1AndNestedByteStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_BYTE, 0, (byte)12);
			structSegmt2.set(JAVA_BYTE, 1, (byte)23);
			structSegmt2.set(JAVA_BYTE, 2, (byte)34);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)37);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)68);
		}
	}

	@Test
	public void test_addByteStruct1AndNestedByteStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteStruct1AndNestedByteStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_BYTE, 0, (byte)12);
			structSegmt2.set(JAVA_BYTE, 1, (byte)23);
			structSegmt2.set(JAVA_BYTE, 2, (byte)34);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 0), (byte)37);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 1), (byte)34);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 2), (byte)45);
		}
	}

	@Test
	public void test_addNestedByteArrayStructs_dupStruct() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, JAVA_BYTE);
		GroupLayout structLayout = MemoryLayout.structLayout(byteArray.withName("array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedByteArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_BYTE, 0, (byte)11);
			structSegmt1.set(JAVA_BYTE, 1, (byte)22);
			structSegmt1.set(JAVA_BYTE, 2, (byte)33);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_BYTE, 0, (byte)12);
			structSegmt2.set(JAVA_BYTE, 1, (byte)23);
			structSegmt2.set(JAVA_BYTE, 2, (byte)34);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 0), (byte)23);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 1), (byte)45);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 2), (byte)67);
		}
	}

	@Test
	public void test_addByteStruct1AndNestedByteArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, JAVA_BYTE);
		GroupLayout structLayout2 = MemoryLayout.structLayout(byteArray.withName("array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteStruct1AndNestedByteArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_BYTE, 0, (byte)11);
			structSegmt2.set(JAVA_BYTE, 1, (byte)12);
			structSegmt2.set(JAVA_BYTE, 2, (byte)14);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)36);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)37);
		}
	}

	@Test
	public void test_addByteStruct1AndNestedByteArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, JAVA_BYTE);
		GroupLayout structLayout2 = MemoryLayout.structLayout(byteArray.withName("array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteStruct1AndNestedByteArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_BYTE, 0, (byte)11);
			structSegmt2.set(JAVA_BYTE, 1, (byte)12);
			structSegmt2.set(JAVA_BYTE, 2, (byte)14);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 0), (byte)36);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 1), (byte)23);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 2), (byte)25);
		}
	}

	@Test
	public void test_addNestedByteStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout byteStruct = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, byteStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedByteStructArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_BYTE, 0, (byte)11);
			structSegmt1.set(JAVA_BYTE, 1, (byte)12);
			structSegmt1.set(JAVA_BYTE, 2, (byte)13);
			structSegmt1.set(JAVA_BYTE, 3, (byte)14);
			structSegmt1.set(JAVA_BYTE, 4, (byte)15);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_BYTE, 0, (byte)21);
			structSegmt2.set(JAVA_BYTE, 1, (byte)23);
			structSegmt2.set(JAVA_BYTE, 2, (byte)25);
			structSegmt2.set(JAVA_BYTE, 3, (byte)27);
			structSegmt2.set(JAVA_BYTE, 4, (byte)29);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 0), (byte)32);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 1), (byte)35);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 2), (byte)38);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 3), (byte)41);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 4), (byte)44);
		}
	}

	@Test
	public void test_addByteStruct1AndNestedByteStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteStruct1AndNestedByteStructArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_BYTE, 0, (byte)21);
			structSegmt2.set(JAVA_BYTE, 1, (byte)23);
			structSegmt2.set(JAVA_BYTE, 2, (byte)25);
			structSegmt2.set(JAVA_BYTE, 3, (byte)27);
			structSegmt2.set(JAVA_BYTE, 4, (byte)29);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)71);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)90);
		}
	}

	@Test
	public void test_addByteStruct1AndNestedByteStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteStruct1AndNestedByteStructArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_BYTE, 0, (byte)21);
			structSegmt2.set(JAVA_BYTE, 1, (byte)23);
			structSegmt2.set(JAVA_BYTE, 2, (byte)25);
			structSegmt2.set(JAVA_BYTE, 3, (byte)27);
			structSegmt2.set(JAVA_BYTE, 4, (byte)29);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 0), (byte)46);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 1), (byte)34);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 2), (byte)50);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 3), (byte)38);
			Assert.assertEquals((byte)resultSegmt.get(JAVA_BYTE, 4), (byte)40);
		}
	}

	@Test
	public void test_addCharStructs_dupStruct() throws Throwable {
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
	public void test_addNestedCharStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedCharStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_CHAR, 0, 'A');
			structSegmt1.set(JAVA_CHAR, 2, 'B');
			structSegmt1.set(JAVA_CHAR, 4, 'C');
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_CHAR, 0, 'E');
			structSegmt2.set(JAVA_CHAR, 2, 'F');
			structSegmt2.set(JAVA_CHAR, 4, 'G');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'E');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 2), 'G');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 4), 'I');
		}
	}

	@Test
	public void test_addCharStruct1AndNestedCharStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharStruct1AndNestedCharStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_CHAR, 0, 'E');
			structSegmt2.set(JAVA_CHAR, 2, 'F');
			structSegmt2.set(JAVA_CHAR, 4, 'G');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'E');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'M');
		}
	}

	@Test
	public void test_addCharStruct1AndNestedCharStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharStruct1AndNestedCharStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_CHAR, 0, 'E');
			structSegmt2.set(JAVA_CHAR, 2, 'F');
			structSegmt2.set(JAVA_CHAR, 4, 'G');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'E');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 2), 'G');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 4), 'H');
		}
	}

	@Test
	public void test_addNestedCharArrayStructs_dupStruct() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, JAVA_CHAR);
		GroupLayout structLayout = MemoryLayout.structLayout(charArray.withName("array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedCharArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_CHAR, 0, 'A');
			structSegmt1.set(JAVA_CHAR, 2, 'B');
			structSegmt1.set(JAVA_CHAR, 4, 'C');
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_CHAR, 0, 'E');
			structSegmt2.set(JAVA_CHAR, 2, 'F');
			structSegmt2.set(JAVA_CHAR, 4, 'G');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'E');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 2), 'G');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 4), 'I');
		}
	}

	@Test
	public void test_addCharStruct1AndNestedCharArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, JAVA_CHAR);
		GroupLayout structLayout2 = MemoryLayout.structLayout(charArray.withName("array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharStruct1AndNestedCharArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_CHAR, 0, 'E');
			structSegmt2.set(JAVA_CHAR, 2, 'F');
			structSegmt2.set(JAVA_CHAR, 4, 'G');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'E');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'M');
		}
	}

	@Test
	public void test_addCharStruct1AndNestedCharArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, JAVA_CHAR);
		GroupLayout structLayout2 = MemoryLayout.structLayout(charArray.withName("array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharStruct1AndNestedCharArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_CHAR, 0, 'E');
			structSegmt2.set(JAVA_CHAR, 2, 'F');
			structSegmt2.set(JAVA_CHAR, 4, 'G');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'E');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 2), 'G');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 4), 'H');
		}
	}

	@Test
	public void test_addNestedCharStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout charStruct = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, charStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedCharStructArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_CHAR, 0, 'A');
			structSegmt1.set(JAVA_CHAR, 2, 'B');
			structSegmt1.set(JAVA_CHAR, 4, 'C');
			structSegmt1.set(JAVA_CHAR, 6, 'D');
			structSegmt1.set(JAVA_CHAR, 8, 'E');
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_CHAR, 0, 'F');
			structSegmt2.set(JAVA_CHAR, 2, 'G');
			structSegmt2.set(JAVA_CHAR, 4, 'H');
			structSegmt2.set(JAVA_CHAR, 6, 'I');
			structSegmt2.set(JAVA_CHAR, 8, 'J');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'F');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 2), 'H');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 4), 'J');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 6), 'L');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 8), 'N');

		}
	}

	@Test
	public void test_addCharStruct1AndNestedCharStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharStruct1AndNestedCharStructArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_CHAR, 0, 'F');
			structSegmt2.set(JAVA_CHAR, 2, 'G');
			structSegmt2.set(JAVA_CHAR, 4, 'H');
			structSegmt2.set(JAVA_CHAR, 6, 'I');
			structSegmt2.set(JAVA_CHAR, 8, 'J');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'M');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'Y');
		}
	}

	@Test
	public void test_addCharStruct1AndNestedCharStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharStruct1AndNestedCharStructArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_CHAR, 0, 'F');
			structSegmt2.set(JAVA_CHAR, 2, 'G');
			structSegmt2.set(JAVA_CHAR, 4, 'H');
			structSegmt2.set(JAVA_CHAR, 6, 'I');
			structSegmt2.set(JAVA_CHAR, 8, 'J');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'F');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 2), 'H');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 4), 'H');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 6), 'J');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 8), 'K');
		}
	}

	@Test
	public void test_addShortStructs_dupStruct() throws Throwable {
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
	public void test_addNestedShortStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedShortStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_SHORT, 0, (short)31);
			structSegmt1.set(JAVA_SHORT, 2, (short)33);
			structSegmt1.set(JAVA_SHORT, 4, (short)35);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_SHORT, 0, (short)10);
			structSegmt2.set(JAVA_SHORT, 2, (short)12);
			structSegmt2.set(JAVA_SHORT, 4, (short)14);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 41);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 2), 45);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 4), 49);
		}
	}

	@Test
	public void test_addShortStruct1AndNestedShortStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortStruct1AndNestedShortStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			shortHandle1.set(structSegmt1, (short)56);
			shortHandle2.set(structSegmt1, (short)45);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_SHORT, 0, (short)31);
			structSegmt2.set(JAVA_SHORT, 2, (short)33);
			structSegmt2.set(JAVA_SHORT, 4, (short)35);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)87);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)113);
		}
	}

	@Test
	public void test_addShortStruct1AndNestedShortStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortStruct1AndNestedShortStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			shortHandle1.set(structSegmt1, (short)56);
			shortHandle2.set(structSegmt1, (short)45);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_SHORT, 0, (short)31);
			structSegmt2.set(JAVA_SHORT, 2, (short)33);
			structSegmt2.set(JAVA_SHORT, 4, (short)35);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 87);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 2), 78);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 4), 80);
		}
	}

	@Test
	public void test_addNestedShortArrayStructs_dupStruct() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, JAVA_SHORT);
		GroupLayout structLayout = MemoryLayout.structLayout(shortArray.withName("array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedShortArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_SHORT, 0, (short)111);
			structSegmt1.set(JAVA_SHORT, 2, (short)222);
			structSegmt1.set(JAVA_SHORT, 4, (short)333);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_SHORT, 0, (short)100);
			structSegmt2.set(JAVA_SHORT, 2, (short)200);
			structSegmt2.set(JAVA_SHORT, 4, (short)300);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 211);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 2), 422);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 4), 633);
		}
	}

	@Test
	public void test_addShortStruct1AndNestedShortArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, JAVA_SHORT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(shortArray.withName("array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortStruct1AndNestedShortArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			shortHandle1.set(structSegmt1, (short)156);
			shortHandle2.set(structSegmt1, (short)145);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_SHORT, 0, (short)111);
			structSegmt2.set(JAVA_SHORT, 2, (short)222);
			structSegmt2.set(JAVA_SHORT, 4, (short)333);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)267);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)700);
		}
	}

	@Test
	public void test_addShortStruct1AndNestedShortArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, JAVA_SHORT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(shortArray.withName("array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortStruct1AndNestedShortArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			shortHandle1.set(structSegmt1, (short)156);
			shortHandle2.set(structSegmt1, (short)145);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_SHORT, 0, (short)111);
			structSegmt2.set(JAVA_SHORT, 2, (short)222);
			structSegmt2.set(JAVA_SHORT, 4, (short)333);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 267);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 2), 367);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 4), 478);
		}
	}

	@Test
	public void test_addNestedShortStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout shortStruct = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, shortStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struc_array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedShortStructArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_SHORT, 0, (short)123);
			structSegmt1.set(JAVA_SHORT, 2, (short)156);
			structSegmt1.set(JAVA_SHORT, 4, (short)112);
			structSegmt1.set(JAVA_SHORT, 6, (short)234);
			structSegmt1.set(JAVA_SHORT, 8, (short)131);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_SHORT, 0, (short)111);
			structSegmt2.set(JAVA_SHORT, 2, (short)222);
			structSegmt2.set(JAVA_SHORT, 4, (short)333);
			structSegmt2.set(JAVA_SHORT, 6, (short)444);
			structSegmt2.set(JAVA_SHORT, 8, (short)555);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 234);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 2), 378);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 4), 445);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 6), 678);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 8), 686);
		}
	}

	@Test
	public void test_addShortStruct1AndNestedShortStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struc_array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortStruct1AndNestedShortStructArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			shortHandle1.set(structSegmt1, (short)156);
			shortHandle2.set(structSegmt1, (short)145);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_SHORT, 0, (short)111);
			structSegmt2.set(JAVA_SHORT, 2, (short)222);
			structSegmt2.set(JAVA_SHORT, 4, (short)333);
			structSegmt2.set(JAVA_SHORT, 6, (short)444);
			structSegmt2.set(JAVA_SHORT, 8, (short)555);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)600);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)1366);
		}
	}

	@Test
	public void test_addShortStruct1AndNestedShortStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struc_array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortStruct1AndNestedShortStructArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			shortHandle1.set(structSegmt1, (short)156);
			shortHandle2.set(structSegmt1, (short)145);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_SHORT, 0, (short)111);
			structSegmt2.set(JAVA_SHORT, 2, (short)222);
			structSegmt2.set(JAVA_SHORT, 4, (short)333);
			structSegmt2.set(JAVA_SHORT, 6, (short)444);
			structSegmt2.set(JAVA_SHORT, 8, (short)555);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 267);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 2), 367);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 4), 489);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 6), 589);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 8), 700);
		}
	}

	@Test
	public void test_addIntStructs_dupStruct() throws Throwable {
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
	public void test_addNestedIntStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedIntStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_INT, 0, 2122232);
			structSegmt1.set(JAVA_INT, 4, 2526272);
			structSegmt1.set(JAVA_INT, 8, 2930313);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_INT, 0, 1111111);
			structSegmt2.set(JAVA_INT, 4, 2222222);
			structSegmt2.set(JAVA_INT, 8, 3333333);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 3233343);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 4), 4748494);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 8), 6263646);
		}
	}

	@Test
	public void test_addIntStruct1AndNestedIntStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntStruct1AndNestedIntStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_INT, 0, 21222324);
			structSegmt2.set(JAVA_INT, 4, 25262728);
			structSegmt2.set(JAVA_INT, 8, 29303132);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(intHandle1.get(resultSegmt), 32445668);
			Assert.assertEquals(intHandle2.get(resultSegmt), 110233648);
		}
	}

	@Test
	public void test_addIntStruct1AndNestedIntStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntStruct1AndNestedIntStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_INT, 0, 21222324);
			structSegmt2.set(JAVA_INT, 4, 25262728);
			structSegmt2.set(JAVA_INT, 8, 29303132);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 32445668);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 4), 80930516);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 8), 84970920);
		}
	}

	@Test
	public void test_addNestedIntArrayStructs_dupStruct() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, JAVA_INT);
		GroupLayout structLayout = MemoryLayout.structLayout(intArray.withName("array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedIntArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_INT, 0, 1111111);
			structSegmt1.set(JAVA_INT, 4, 2222222);
			structSegmt1.set(JAVA_INT, 8, 3333333);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_INT, 0, 2122232);
			structSegmt2.set(JAVA_INT, 4, 2526272);
			structSegmt2.set(JAVA_INT, 8, 2930313);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 3233343);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 4), 4748494);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 8), 6263646);
		}
	}

	@Test
	public void test_addIntStruct1AndNestedIntArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, JAVA_INT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(intArray.withName("array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntStruct1AndNestedIntArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_INT, 0, 21222324);
			structSegmt2.set(JAVA_INT, 4, 25262728);
			structSegmt2.set(JAVA_INT, 8, 29303132);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(intHandle1.get(resultSegmt), 32445668);
			Assert.assertEquals(intHandle2.get(resultSegmt), 110233648);
		}
	}

	@Test
	public void test_addIntStruct1AndNestedIntArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, JAVA_INT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(intArray.withName("array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntStruct1AndNestedIntArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_INT, 0, 21222324);
			structSegmt2.set(JAVA_INT, 4, 25262728);
			structSegmt2.set(JAVA_INT, 8, 29303132);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 32445668);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 4), 80930516);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 8), 84970920);
		}
	}

	@Test
	public void test_addNestedIntStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout intStruct = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, intStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedIntStructArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_INT, 0,  1111111);
			structSegmt1.set(JAVA_INT, 4,  2222222);
			structSegmt1.set(JAVA_INT, 8,  3333333);
			structSegmt1.set(JAVA_INT, 12, 4444444);
			structSegmt1.set(JAVA_INT, 16, 5555555);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_INT, 0,  1222324);
			structSegmt2.set(JAVA_INT, 4,  3262728);
			structSegmt2.set(JAVA_INT, 8,  1303132);
			structSegmt2.set(JAVA_INT, 12, 2323233);
			structSegmt2.set(JAVA_INT, 16, 1545454);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0),  2333435);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 4),  5484950);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 8),  4636465);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 12), 6767677);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 16), 7101009);
		}
	}

	@Test
	public void test_addIntStruct1AndNestedIntStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntStruct1AndNestedIntStructArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			intHandle1.set(structSegmt1, 1111111);
			intHandle2.set(structSegmt1, 2222222);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_INT, 0,  1222324);
			structSegmt2.set(JAVA_INT, 4,  3262728);
			structSegmt2.set(JAVA_INT, 8,  1303132);
			structSegmt2.set(JAVA_INT, 12, 2323233);
			structSegmt2.set(JAVA_INT, 16, 1545454);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(intHandle1.get(resultSegmt), 3636567);
			Assert.assertEquals(intHandle2.get(resultSegmt), 9353637);
		}
	}

	@Test
	public void test_addIntStruct1AndNestedIntStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntStruct1AndNestedIntStructArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			intHandle1.set(structSegmt1, 1111111);
			intHandle2.set(structSegmt1, 2222222);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_INT, 0,  1222324);
			structSegmt2.set(JAVA_INT, 4,  3262728);
			structSegmt2.set(JAVA_INT, 8,  1303132);
			structSegmt2.set(JAVA_INT, 12, 2323233);
			structSegmt2.set(JAVA_INT, 16, 1545454);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0),  2333435);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 4),  5484950);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 8),  2414243);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 12), 4545455);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 16), 3767676);
		}
	}

	@Test
	public void test_addLongStructs_dupStruct() throws Throwable {
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
	public void test_addNestedLongStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedLongStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_LONG, 0,  1357913579L);
			structSegmt1.set(JAVA_LONG, 8,  2468024680L);
			structSegmt1.set(JAVA_LONG, 16, 1122334455L);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_LONG, 0,  1111111111L);
			structSegmt2.set(JAVA_LONG, 8,  2222222222L);
			structSegmt2.set(JAVA_LONG, 16, 3333333333L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0),  2469024690L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 8),  4690246902L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 16), 4455667788L);
		}
	}

	@Test
	public void test_addLongStruct1AndNestedLongStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongStruct1AndNestedLongStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_LONG, 0,  1111111111L);
			structSegmt2.set(JAVA_LONG, 8,  2222222222L);
			structSegmt2.set(JAVA_LONG, 16, 3333333333L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(longHandle1.get(resultSegmt), 988765433098L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 129012344678L);
		}
	}

	@Test
	public void test_addLongStruct1AndNestedLongStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongStruct1AndNestedLongStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_LONG, 0,  1111111111L);
			structSegmt2.set(JAVA_LONG, 8,  2222222222L);
			structSegmt2.set(JAVA_LONG, 16, 3333333333L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0),  988765433098L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 8),  125679011345L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 16), 126790122456L);
		}
	}

	@Test
	public void test_addNestedLongArrayStructs_dupStruct() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, JAVA_LONG);
		GroupLayout structLayout = MemoryLayout.structLayout(longArray.withName("array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedLongArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_LONG, 0,  1357913579L);
			structSegmt1.set(JAVA_LONG, 8,  2468024680L);
			structSegmt1.set(JAVA_LONG, 16, 1122334455L);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_LONG, 0,  1111111111L);
			structSegmt2.set(JAVA_LONG, 8,  2222222222L);
			structSegmt2.set(JAVA_LONG, 16, 3333333333L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0),  2469024690L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 8),  4690246902L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 16), 4455667788L);
		}
	}

	@Test
	public void test_addLongStruct1AndNestedLongArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, JAVA_LONG);
		GroupLayout structLayout2 = MemoryLayout.structLayout(longArray.withName("array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongStruct1AndNestedLongArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_LONG, 0,  1111111111L);
			structSegmt2.set(JAVA_LONG, 8,  2222222222L);
			structSegmt2.set(JAVA_LONG, 16, 3333333333L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(longHandle1.get(resultSegmt), 988765433098L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 129012344678L);
		}
	}

	@Test
	public void test_addLongStruct1AndNestedLongArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, JAVA_LONG);
		GroupLayout structLayout2 = MemoryLayout.structLayout(longArray.withName("array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongStruct1AndNestedLongArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_LONG, 0,  1111111111L);
			structSegmt2.set(JAVA_LONG, 8,  2222222222L);
			structSegmt2.set(JAVA_LONG, 16, 3333333333L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0),  988765433098L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 8),  125679011345L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 16), 126790122456L);
		}
	}

	@Test
	public void test_addNestedLongStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout longStruct = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, longStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedLongStructArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_LONG, 0, 111111111L);
			structSegmt1.set(JAVA_LONG, 8, 222222222L);
			structSegmt1.set(JAVA_LONG, 16, 333333333L);
			structSegmt1.set(JAVA_LONG, 24, 244444444L);
			structSegmt1.set(JAVA_LONG, 32, 155555555L);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_LONG, 0, 111111111L);
			structSegmt2.set(JAVA_LONG, 8, 222222222L);
			structSegmt2.set(JAVA_LONG, 16, 333333333L);
			structSegmt2.set(JAVA_LONG, 24, 144444444L);
			structSegmt2.set(JAVA_LONG, 32, 255555555L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0),  222222222L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 8),  444444444L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 16), 666666666L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 24), 388888888L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 32), 411111110L);
		}
	}

	@Test
	public void test_addLongStruct1AndNestedLongStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongStruct1AndNestedLongStructArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_LONG, 0, 111111111L);
			structSegmt2.set(JAVA_LONG, 8, 222222222L);
			structSegmt2.set(JAVA_LONG, 16, 333333333L);
			structSegmt2.set(JAVA_LONG, 24, 444444444L);
			structSegmt2.set(JAVA_LONG, 32, 155555555L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(longHandle1.get(resultSegmt), 988098766431L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 124279011344L);
		}
	}

	@Test
	public void test_addLongStruct1AndNestedLongStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongStruct1AndNestedLongStructArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_LONG, 0, 111111111L);
			structSegmt2.set(JAVA_LONG, 8, 222222222L);
			structSegmt2.set(JAVA_LONG, 16, 333333333L);
			structSegmt2.set(JAVA_LONG, 24, 444444444L);
			structSegmt2.set(JAVA_LONG, 32, 155555555L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0),  987765433098L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 8),  123679011345L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 16), 987987655320L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 24), 123901233567L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 32), 123612344678L);
		}
	}

	@Test
	public void test_addFloatStructs_dupStruct() throws Throwable {
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
	public void test_addNestedFloatStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedFloatStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_FLOAT, 0, 31.22F);
			structSegmt1.set(JAVA_FLOAT, 4, 33.44F);
			structSegmt1.set(JAVA_FLOAT, 8, 35.66F);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_FLOAT, 0, 22.33F);
			structSegmt2.set(JAVA_FLOAT, 4, 44.55F);
			structSegmt2.set(JAVA_FLOAT, 8, 66.78F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 53.55F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 4), 77.99F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 8), 102.44F, 0.01F);
		}
	}

	@Test
	public void test_addFloatStruct1AndNestedFloatStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatStruct1AndNestedFloatStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_FLOAT, 0, 31.22F);
			structSegmt2.set(JAVA_FLOAT, 4, 33.44F);
			structSegmt2.set(JAVA_FLOAT, 8, 35.66F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 56.34F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 80.33F, 0.01F);
		}
	}

	@Test
	public void test_addFloatStruct1AndNestedFloatStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatStruct1AndNestedFloatStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_FLOAT, 0, 31.22F);
			structSegmt2.set(JAVA_FLOAT, 4, 33.44F);
			structSegmt2.set(JAVA_FLOAT, 8, 35.66F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 56.34F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 4), 44.67F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 8), 46.89F, 0.01F);
		}
	}

	@Test
	public void test_addNestedFloatArrayStructs_dupStruct() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, JAVA_FLOAT);
		GroupLayout structLayout = MemoryLayout.structLayout(floatArray.withName("array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedFloatArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt1.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt1.set(JAVA_FLOAT, 8, 333.33F);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_FLOAT, 0, 123.45F);
			structSegmt2.set(JAVA_FLOAT, 4, 234.13F);
			structSegmt2.set(JAVA_FLOAT, 8, 245.79F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 234.56F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 4), 456.35F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 8), 579.12F, 0.01F);
		}
	}

	@Test
	public void test_addFloatStruct1AndNestedFloatArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, JAVA_FLOAT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(floatArray.withName("array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatStruct1AndNestedFloatArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt2.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt2.set(JAVA_FLOAT, 8, 333.33F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 136.23F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 566.78F, 0.01F);
		}
	}

	@Test
	public void test_addFloatStruct1AndNestedFloatArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, JAVA_FLOAT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(floatArray.withName("array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatStruct1AndNestedFloatArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt2.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt2.set(JAVA_FLOAT, 8, 333.33F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 136.23F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 4), 233.45F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 8), 344.56F, 0.01F);
		}
	}

	@Test
	public void test_addNestedFloatStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout floatStruct = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, floatStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedFloatStructArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt1.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt1.set(JAVA_FLOAT, 8, 333.33F);
			structSegmt1.set(JAVA_FLOAT, 12, 444.44F);
			structSegmt1.set(JAVA_FLOAT, 16, 555.55F);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_FLOAT, 0, 311.18F);
			structSegmt2.set(JAVA_FLOAT, 4, 122.27F);
			structSegmt2.set(JAVA_FLOAT, 8, 233.36F);
			structSegmt2.set(JAVA_FLOAT, 12, 144.43F);
			structSegmt2.set(JAVA_FLOAT, 16, 255.51F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 422.29F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 4), 344.49F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 8), 566.69F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 12), 588.87F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 16), 811.06F, 0.01F);
		}
	}

	@Test
	public void test_addFloatStruct1AndNestedFloatStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatStruct1AndNestedFloatStructArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt2.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt2.set(JAVA_FLOAT, 8, 333.38F);
			structSegmt2.set(JAVA_FLOAT, 12, 444.44F);
			structSegmt2.set(JAVA_FLOAT, 16, 155.55F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 469.61F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 833.44F, 0.01F);
		}
	}

	@Test
	public void test_addFloatStruct1AndNestedFloatStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatStruct1AndNestedFloatStructArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_FLOAT, 0, 111.11F);
			structSegmt2.set(JAVA_FLOAT, 4, 222.22F);
			structSegmt2.set(JAVA_FLOAT, 8, 333.33F);
			structSegmt2.set(JAVA_FLOAT, 12, 444.44F);
			structSegmt2.set(JAVA_FLOAT, 16, 155.55F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 136.23F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 4), 233.45F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 8), 358.45F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 12), 455.67F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 16), 166.78F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleStructs_dupStruct() throws Throwable {
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
	public void test_addNestedDoubleStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedDoubleStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_DOUBLE, 0,  31.789D);
			structSegmt1.set(JAVA_DOUBLE, 8,  33.456D);
			structSegmt1.set(JAVA_DOUBLE, 16, 35.123D);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_DOUBLE, 0,  11.222D);
			structSegmt2.set(JAVA_DOUBLE, 8,  22.333D);
			structSegmt2.set(JAVA_DOUBLE, 16, 33.445D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0),  43.011D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 8),  55.789D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 16), 68.568D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleStruct1AndNestedDoubleStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleStruct1AndNestedDoubleStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_DOUBLE, 0, 31.789D);
			structSegmt2.set(JAVA_DOUBLE, 8, 33.456D);
			structSegmt2.set(JAVA_DOUBLE, 16, 35.123D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 43.011D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 90.912D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleStruct1AndNestedDoubleStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleStruct1AndNestedDoubleStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_DOUBLE, 0, 31.789D);
			structSegmt2.set(JAVA_DOUBLE, 8, 33.456D);
			structSegmt2.set(JAVA_DOUBLE, 16, 35.124D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0),  43.011D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 8),  55.789D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 16), 57.457D, 0.001D);
		}
	}

	@Test
	public void test_addNestedDoubleArrayStructs_dupStruct() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, JAVA_DOUBLE);
		GroupLayout structLayout = MemoryLayout.structLayout(doubleArray.withName("array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedDoubleArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_DOUBLE, 0,  31.789D);
			structSegmt1.set(JAVA_DOUBLE, 8,  33.456D);
			structSegmt1.set(JAVA_DOUBLE, 16, 35.123D);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_DOUBLE, 0,  11.222D);
			structSegmt2.set(JAVA_DOUBLE, 8,  22.333D);
			structSegmt2.set(JAVA_DOUBLE, 16, 33.445D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0),  43.011D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 8),  55.789D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 16), 68.568D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleStruct1AndNestedDoubleArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, JAVA_DOUBLE);
		GroupLayout structLayout2 = MemoryLayout.structLayout(doubleArray.withName("array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleStruct1AndNestedDoubleArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_DOUBLE, 0, 31.789D);
			structSegmt2.set(JAVA_DOUBLE, 8, 33.456D);
			structSegmt2.set(JAVA_DOUBLE, 16, 35.123D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 43.011D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 90.912D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleStruct1AndNestedDoubleArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, JAVA_DOUBLE);
		GroupLayout structLayout2 = MemoryLayout.structLayout(doubleArray.withName("array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleStruct1AndNestedDoubleArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_DOUBLE, 0, 31.789D);
			structSegmt2.set(JAVA_DOUBLE, 8, 33.456D);
			structSegmt2.set(JAVA_DOUBLE, 16, 35.124D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0),  43.011D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 8),  55.789D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 16), 57.457D, 0.001D);
		}
	}

	@Test
	public void test_addNestedDoubleStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout doubleStruct = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, doubleStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addNestedDoubleStructArrayStructs_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_DOUBLE, 0, 111.111D);
			structSegmt1.set(JAVA_DOUBLE, 8, 222.222D);
			structSegmt1.set(JAVA_DOUBLE, 16, 333.333D);
			structSegmt1.set(JAVA_DOUBLE, 24, 444.444D);
			structSegmt1.set(JAVA_DOUBLE, 32, 555.555D);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_DOUBLE, 0, 519.117D);
			structSegmt2.set(JAVA_DOUBLE, 8, 428.226D);
			structSegmt2.set(JAVA_DOUBLE, 16, 336.335D);
			structSegmt2.set(JAVA_DOUBLE, 24, 247.444D);
			structSegmt2.set(JAVA_DOUBLE, 32, 155.553D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0),  630.228D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 8),  650.448D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 16), 669.668D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 24), 691.888D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 32), 711.108D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleStruct1AndNestedDoubleStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleStruct1AndNestedDoubleStructArrayStruct2_returnStruct1_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_DOUBLE, 0, 519.117D);
			structSegmt2.set(JAVA_DOUBLE, 8, 428.226D);
			structSegmt2.set(JAVA_DOUBLE, 16, 336.331D);
			structSegmt2.set(JAVA_DOUBLE, 24, 247.444D);
			structSegmt2.set(JAVA_DOUBLE, 32, 155.552D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 866.67D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 853.555D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleStruct1AndNestedDoubleStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout1.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout1.varHandle(PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleStruct1AndNestedDoubleStructArrayStruct2_returnStruct2_dupStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout1);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = arena.allocate(structLayout2);
			structSegmt2.set(JAVA_DOUBLE, 0, 519.116D);
			structSegmt2.set(JAVA_DOUBLE, 8, 428.225D);
			structSegmt2.set(JAVA_DOUBLE, 16, 336.331D);
			structSegmt2.set(JAVA_DOUBLE, 24, 247.444D);
			structSegmt2.set(JAVA_DOUBLE, 32, 155.552D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0),  530.338D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 8),  450.558D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 16), 347.553D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 24), 269.777D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 32), 177.885D, 0.001D);
		}
	}
}
