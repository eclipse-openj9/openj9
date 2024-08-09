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
package org.openj9.test.jep442.upcall;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemoryLayout.PathElement;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SegmentAllocator;
import java.lang.foreign.SequenceLayout;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.UnionLayout;
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
 * Test cases for JEP 442: Foreign Linker API (Third Preview) for argument/return union in upcall.
 */
@Test(groups = { "level.sanity" })
public class UpcallMHWithUnionTests {
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addBoolAndBoolsFromUnionWithXorByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionWithXorByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromUnionWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			boolHandle1.set(unionSegmt, true);

			boolean result = (boolean)mh.invoke(true, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromUnionPtrWithXorByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionPtrWithXorByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromUnionPtrWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			boolHandle2.set(unionSegmt, false);

			boolean result = (boolean)mh.invoke(true, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromNestedUnionWithXorByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromNestedUnionWithXorByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromNestedUnionWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, true);

			boolean result = (boolean)mh.invoke(false, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromNestedUnionWithXor_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromNestedUnionWithXor_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromNestedUnionWithXor_reverseOrder,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, true);

			boolean result = (boolean)mh.invoke(true, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, false);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromUnionWithNestedBoolArrayByUpcallMH() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, JAVA_BOOLEAN);
		UnionLayout unionLayout = MemoryLayout.unionLayout(boolArray.withName("array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionWithNestedBoolArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromUnionWithNestedBoolArray,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, true);
			unionSegmt.set(JAVA_BOOLEAN, 1, true);

			boolean result = (boolean)mh.invoke(false, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromUnionWithNestedBoolArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, JAVA_BOOLEAN);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), boolArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionWithNestedBoolArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromUnionWithNestedBoolArray_reverseOrder,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, false);
			unionSegmt.set(JAVA_BOOLEAN, 1, true);

			boolean result = (boolean)mh.invoke(false, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromUnionWithNestedUnionArrayByUpcallMH() throws Throwable {
		UnionLayout boolUnion = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, boolUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionWithNestedUnionArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromUnionWithNestedUnionArray,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, true);
			unionSegmt.set(JAVA_BOOLEAN, 1, true);

			boolean result = (boolean)mh.invoke(true, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, false);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout boolUnion = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, boolUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromUnionWithNestedUnionArray_reverseOrder,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, false);
			unionSegmt.set(JAVA_BOOLEAN, 1, true);

			boolean result = (boolean)mh.invoke(true, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_add2BoolUnionsWithXor_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolUnionsWithXor_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2BoolUnionsWithXor_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			boolHandle1.set(unionSegmt1, true);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			boolHandle2.set(unionSegmt2, false);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals(boolHandle1.get(resultSegmt), true);
			Assert.assertEquals(boolHandle2.get(resultSegmt), true);
		}
	}

	@Test
	public void test_add2BoolUnionsWithXor_returnUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolUnionsWithXor_returnUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2BoolUnionsWithXor_returnUnionPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			boolHandle1.set(unionSegmt1, true);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			boolHandle2.set(unionSegmt2, true);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), true);
		}
	}

	@Test
	public void test_add3BoolUnionsWithXor_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"),
				JAVA_BOOLEAN.withName("elem2"), JAVA_BOOLEAN.withName("elem3"));
		VarHandle boolHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle boolHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3BoolUnionsWithXor_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3BoolUnionsWithXor_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			boolHandle1.set(unionSegmt1, true);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			boolHandle2.set(unionSegmt2, false);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals(boolHandle1.get(resultSegmt), true);
			Assert.assertEquals(boolHandle2.get(resultSegmt), true);
			Assert.assertEquals(boolHandle3.get(resultSegmt), true);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromUnion,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt, (byte)8);

			byte result = (byte)mh.invokeExact((byte)6, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 22);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);


		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromUnionPtr,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			byteHandle2.set(unionSegmt, (byte)12);

			byte result = (byte)mh.invoke((byte)13, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 37);
		}
	}

	@Test
	public void test_addByteAndBytesFromNestedUnionByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromNestedUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromNestedUnion,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)11);

			byte result = (byte)mh.invoke((byte)33, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 66);
		}
	}

	@Test
	public void test_addByteAndBytesFromNestedUnion_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromNestedUnion_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromNestedUnion_reverseOrder,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)12);

			byte result = (byte)mh.invoke((byte)48, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 84);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnionWithNestedByteArrayByUpcallMH() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, JAVA_BYTE);
		UnionLayout unionLayout = MemoryLayout.unionLayout(byteArray.withName("array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnionWithNestedByteArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromUnionWithNestedByteArray,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)11);
			unionSegmt.set(JAVA_BYTE, 1, (byte)12);

			byte result = (byte)mh.invoke((byte)14, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 48);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnionWithNestedByteArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, JAVA_BYTE);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), byteArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnionWithNestedByteArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromUnionWithNestedByteArray_reverseOrder,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)12);
			unionSegmt.set(JAVA_BYTE, 1, (byte)14);

			byte result = (byte)mh.invoke((byte)18, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 56);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnionWithNestedUnionArrayByUpcallMH() throws Throwable {
		UnionLayout byteUnion = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, byteUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnionWithNestedUnionArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromUnionWithNestedUnionArray,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)11);
			unionSegmt.set(JAVA_BYTE, 1, (byte)12);

			byte result = (byte)mh.invoke((byte)16, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 73);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnionWithNestedUnionArray_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout byteUnion = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, byteUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnionWithNestedUnionArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromUnionWithNestedUnionArray_reverseOrder,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)12);
			unionSegmt.set(JAVA_BYTE, 1, (byte)14);

			byte result = (byte)mh.invoke((byte)22, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 86);
		}
	}

	@Test
	public void test_add2ByteUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ByteUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2ByteUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt1, (byte)25);
			byteHandle2.set(unionSegmt1, (byte)11);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt2, (byte)24);
			byteHandle2.set(unionSegmt2, (byte)13);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)24);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)24);
		}
	}

	@Test
	public void test_add2ByteUnions_returnUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ByteUnions_returnUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2ByteUnions_returnUnionPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt1, (byte)25);
			byteHandle2.set(unionSegmt1, (byte)11);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt2, (byte)24);
			byteHandle2.set(unionSegmt2, (byte)13);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)37);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)37);
		}
	}

	@Test
	public void test_add3ByteUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"),
				JAVA_BYTE.withName("elem2"), JAVA_BYTE.withName("elem3"));
		VarHandle byteHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle byteHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3ByteUnions_returnUnionByUpcallMH").get();
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3ByteUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt1, (byte)25);
			byteHandle2.set(unionSegmt1, (byte)11);
			byteHandle3.set(unionSegmt1, (byte)12);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt2, (byte)24);
			byteHandle2.set(unionSegmt2, (byte)13);
			byteHandle3.set(unionSegmt2, (byte)16);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)28);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)28);
			Assert.assertEquals((byte)byteHandle3.get(resultSegmt), (byte)28);
		}
	}

	@Test
	public void test_addCharAndCharsFromUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromUnion,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt, 'A');
			charHandle2.set(unionSegmt, 'B');

			char result = (char)mh.invokeExact('C', unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'E');
		}
	}

	@Test
	public void test_addCharAndCharsFromUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromUnionPtr,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, ADDRESS), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt, 'H');
			charHandle2.set(unionSegmt, 'I');

			char result = (char)mh.invoke('G', unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'W');
		}
	}

	@Test
	public void test_addCharAndCharsFromNestedUnionByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromNestedUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromNestedUnion,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'E');

			char result = (char)mh.invokeExact('H', unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'T');
		}
	}

	@Test
	public void test_addCharAndCharsFromNestedUnion_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromNestedUnion_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromNestedUnion_reverseOrder,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'F');

			char result = (char)mh.invokeExact('H', unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'W');
		}
	}

	@Test
	public void test_addCharAndCharsFromUnionWithNestedCharArrayByUpcallMH() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, JAVA_CHAR);
		UnionLayout unionLayout = MemoryLayout.unionLayout(charArray.withName("array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnionWithNestedCharArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromUnionWithNestedCharArray,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'A');
			unionSegmt.set(JAVA_CHAR, 2, 'B');

			char result = (char)mh.invokeExact('D', unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'E');
		}
	}

	@Test
	public void test_addCharAndCharsFromUnionWithNestedCharArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, JAVA_CHAR);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), charArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnionWithNestedCharArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromUnionWithNestedCharArray_reverseOrder,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'A');
			unionSegmt.set(JAVA_CHAR, 2, 'B');

			char result = (char)mh.invokeExact('D', unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'E');
		}
	}

	@Test
	public void test_addCharAndCharsFromUnionWithNestedUnionArrayByUpcallMH() throws Throwable {
		UnionLayout charUnion = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, charUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnionWithNestedUnionArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromUnionWithNestedUnionArray,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'B');
			unionSegmt.set(JAVA_CHAR, 2, 'C');

			char result = (char)mh.invokeExact('J', unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'Q');
		}
	}

	@Test
	public void test_addCharAndCharsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout charUnion = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, charUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromUnionWithNestedUnionArray_reverseOrder,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'B');
			unionSegmt.set(JAVA_CHAR, 2, 'C');

			char result = (char)mh.invokeExact('J', unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'Q');
		}
	}

	@Test
	public void test_add2CharUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2CharUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2CharUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt1, 'A');
			charHandle2.set(unionSegmt1, 'B');
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt2, 'C');
			charHandle2.set(unionSegmt2, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'E');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'E');
		}
	}

	@Test
	public void test_add2CharUnions_returnUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2CharUnions_returnUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2CharUnions_returnUnionPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt1, 'A');
			charHandle2.set(unionSegmt1, 'B');
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt2, 'B');
			charHandle2.set(unionSegmt2, 'C');

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals(charHandle1.get(resultSegmt), 'F');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'F');
		}
	}

	@Test
	public void test_add3CharUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"),
				JAVA_CHAR.withName("elem2"), JAVA_CHAR.withName("elem3"));
		VarHandle charHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle charHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3CharUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3CharUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt1, 'A');
			charHandle2.set(unionSegmt1, 'B');
			charHandle3.set(unionSegmt1, 'C');
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt2, 'B');
			charHandle2.set(unionSegmt2, 'C');
			charHandle3.set(unionSegmt2, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'F');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'F');
			Assert.assertEquals(charHandle3.get(resultSegmt), 'F');
		}
	}

	@Test
	public void test_addShortAndShortsFromUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromUnion,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			shortHandle2.set(unionSegmt, (short)9);

			short result = (short)mh.invokeExact((short)6, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 24);
		}
	}

	@Test
	public void test_addShortAndShortsFromUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromUnionPtr,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, ADDRESS), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt, (short)22);

			short result = (short)mh.invoke((short)66, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 110);
		}
	}

	@Test
	public void test_addShortAndShortsFromNestedUnionByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromNestedUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromNestedUnion,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)31);

			short result = (short)mh.invokeExact((short)37, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 130);
		}
	}

	@Test
	public void test_addShortAndShortsFromNestedUnion_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromNestedUnion_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromNestedUnion_reverseOrder,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)31);

			short result = (short)mh.invokeExact((short)37, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 130);
		}
	}

	@Test
	public void test_addShortAndShortsFromUnionWithNestedShortArrayByUpcallMH() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, JAVA_SHORT);
		UnionLayout unionLayout = MemoryLayout.unionLayout(shortArray.withName("array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnionWithNestedShortArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromUnionWithNestedShortArray,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)111);
			unionSegmt.set(JAVA_SHORT, 2, (short)222);

			short result = (short)mh.invokeExact((short)444, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 888);
		}
	}

	@Test
	public void test_addShortAndShortsFromUnionWithNestedShortArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, JAVA_SHORT);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), shortArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnionWithNestedShortArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromUnionWithNestedShortArray_reverseOrder,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)111);
			unionSegmt.set(JAVA_SHORT, 2, (short)222);

			short result = (short)mh.invokeExact((short)444, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 888);
		}
	}

	@Test
	public void test_addShortAndShortsFromUnionWithNestedUnionArrayByUpcallMH() throws Throwable {
		UnionLayout shortUnion = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, shortUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnionWithNestedUnionArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromUnionWithNestedUnionArray,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)111);
			unionSegmt.set(JAVA_SHORT, 2, (short)222);

			short result = (short)mh.invokeExact((short)666, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1443);
		}
	}

	@Test
	public void test_addShortAndShortsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout shortUnion = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, shortUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromUnionWithNestedUnionArray_reverseOrder,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)111);
			unionSegmt.set(JAVA_SHORT, 2, (short)222);

			short result = (short)mh.invokeExact((short)666, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1443);
		}
	}

	@Test
	public void test_add2ShortUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ShortUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2ShortUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt1, (short)56);
			shortHandle2.set(unionSegmt1, (short)45);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt2, (short)78);
			shortHandle2.set(unionSegmt2, (short)67);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)112);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)112);
		}
	}

	@Test
	public void test_add2ShortUnions_returnUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ShortUnions_returnUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2ShortUnions_returnUnionPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt1, (short)56);
			shortHandle2.set(unionSegmt1, (short)45);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt2, (short)78);
			shortHandle2.set(unionSegmt2, (short)67);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)179);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)179);
		}
	}

	@Test
	public void test_add3ShortUnions_returnUnionByUpcallMH() throws Throwable  {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"),
				JAVA_SHORT.withName("elem2"), JAVA_SHORT.withName("elem3"));
		VarHandle shortHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle shortHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3ShortUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3ShortUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt1, (short)25);
			shortHandle2.set(unionSegmt1, (short)26);
			shortHandle3.set(unionSegmt1, (short)27);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt2, (short)34);
			shortHandle2.set(unionSegmt2, (short)35);
			shortHandle3.set(unionSegmt2, (short)36);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)63);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)63);
			Assert.assertEquals((short)shortHandle3.get(resultSegmt), (short)63);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromUnion,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			intHandle2.set(unionSegmt, 1234567);

			int result = (int)mh.invokeExact(2244668, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 4713802);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromUnionPtr,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt, 11121314);
			intHandle2.set(unionSegmt, 15161718);

			int result = (int)mh.invoke(19202122, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 49525558);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedUnionByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_INT.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromNestedUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromNestedUnion,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 21222324);

			int result = (int)mh.invokeExact(33343536, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 97010508);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedUnion_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromNestedUnion_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromNestedUnion_reverseOrder,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 21222324);

			int result = (int)mh.invokeExact(33343536, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 97010508);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionWithNestedIntArrayByUpcallMH() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, JAVA_INT);
		UnionLayout unionLayout = MemoryLayout.unionLayout(intArray.withName("array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionWithNestedIntArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromUnionWithNestedIntArray,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 1111111);
			unionSegmt.set(JAVA_INT, 4, 2222222);

			int result = (int)mh.invokeExact(4444444, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 8888888);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionWithNestedIntArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, JAVA_INT);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), intArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionWithNestedIntArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromUnionWithNestedIntArray_reverseOrder,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 1111111);
			unionSegmt.set(JAVA_INT, 4, 2222222);

			int result = (int)mh.invokeExact(4444444, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 8888888);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionWithNestedUnionArrayByUpcallMH() throws Throwable {
		UnionLayout intUnion = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, intUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionWithNestedUnionArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromUnionWithNestedUnionArray,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 2222222);
			unionSegmt.set(JAVA_INT, 4, 3333333);

			int result = (int)mh.invokeExact(6666666, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 19999998);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout intUnion = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, intUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromUnionWithNestedUnionArray_reverseOrder,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 2222222);
			unionSegmt.set(JAVA_INT, 4, 3333333);

			int result = (int)mh.invokeExact(6666666, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 19999998);
		}
	}

	@Test
	public void test_add2IntUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt1, 11223344);
			intHandle2.set(unionSegmt1, 55667788);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt2, 99001122);
			intHandle2.set(unionSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals(intHandle1.get(resultSegmt), 89113354);
			Assert.assertEquals(intHandle2.get(resultSegmt), 89113354);
		}
	}

	@Test
	public void test_add2IntUnions_returnUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntUnions_returnUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntUnions_returnUnionPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt1, 11223344);
			intHandle2.set(unionSegmt1, 55667788);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt2, 99001122);
			intHandle2.set(unionSegmt2, 33445566);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals(intHandle1.get(resultSegmt), 122558920);
			Assert.assertEquals(intHandle2.get(resultSegmt), 122558920);
		}
	}

	@Test
	public void test_add3IntUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_INT.withName("elem2"), JAVA_INT.withName("elem3"));
		VarHandle intHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle intHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3IntUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3IntUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt1, 11223344);
			intHandle2.set(unionSegmt1, 55667788);
			intHandle3.set(unionSegmt1, 99001122);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt2, 33445566);
			intHandle2.set(unionSegmt2, 77889900);
			intHandle3.set(unionSegmt2, 44332211);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals(intHandle1.get(resultSegmt), 143333333);
			Assert.assertEquals(intHandle2.get(resultSegmt), 143333333);
			Assert.assertEquals(intHandle3.get(resultSegmt), 143333333);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromUnion,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt, 1234567890L);
			longHandle2.set(unionSegmt, 9876543210L);

			long result = (long)mh.invokeExact(2468024680L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 22221111100L);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromUnionPtr,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, ADDRESS), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt, 224466880022L);
			longHandle2.set(unionSegmt, 446688002244L);

			long result = (long)mh.invoke(668800224466L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1562176228954L);
		}
	}

	@Test
	public void test_addLongAndLongsFromNestedUnionByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromNestedUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromNestedUnion,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 135791357913L);

			long result = (long)mh.invokeExact(778899001122L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1186273074861L);
		}
	}

	@Test
	public void test_addLongAndLongsFromNestedUnion_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromNestedUnion_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromNestedUnion_reverseOrder,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 135791357913L);

			long result = (long)mh.invokeExact(778899001122L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1186273074861L);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnionWithNestedLongArrayByUpcallMH() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, JAVA_LONG);
		UnionLayout unionLayout = MemoryLayout.unionLayout(longArray.withName("array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnionWithNestedLongArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromUnionWithNestedLongArray,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 111111111L);
			unionSegmt.set(JAVA_LONG, 8, 222222222L);

			long result = (long)mh.invokeExact(444444444L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 888888888L);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnionWithNestedLongArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, JAVA_LONG);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), longArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnionWithNestedLongArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromUnionWithNestedLongArray_reverseOrder,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 111111111L);
			unionSegmt.set(JAVA_LONG, 8, 222222222L);

			long result = (long)mh.invokeExact(444444444L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 888888888L);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnionWithNestedUnionArrayByUpcallMH() throws Throwable {
		UnionLayout longUnion = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, longUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnionWithNestedUnionArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromUnionWithNestedUnionArray,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 111111111L);
			unionSegmt.set(JAVA_LONG, 8, 222222222L);

			long result = (long)mh.invokeExact(666666666L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1444444443L);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout longUnion = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, longUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromUnionWithNestedUnionArray_reverseOrder,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 111111111L);
			unionSegmt.set(JAVA_LONG, 8, 222222222L);

			long result = (long)mh.invokeExact(666666666L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1444444443L);
		}
	}

	@Test
	public void test_add2LongUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2LongUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2LongUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt1, 987654321987L);
			longHandle2.set(unionSegmt1, 123456789123L);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt2, 224466880022L);
			longHandle2.set(unionSegmt2, 113355779911L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals(longHandle1.get(resultSegmt), 236812569034L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 236812569034L);
		}
	}

	@Test
	public void test_add2LongUnions_returnUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2LongUnions_returnUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2LongUnions_returnUnionPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt1, 1122334455L);
			longHandle2.set(unionSegmt1, 5566778899L);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt2, 9900112233L);
			longHandle2.set(unionSegmt2, 3344556677L);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals(longHandle1.get(resultSegmt), 12255892253L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 12255892253L);
		}
	}

	@Test
	public void test_add3LongUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"), JAVA_LONG.withName("elem3"));
		VarHandle longHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle longHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3LongUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3LongUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt1, 987654321987L);
			longHandle2.set(unionSegmt1, 123456789123L);
			longHandle3.set(unionSegmt1, 112233445566L);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt2, 224466880022L);
			longHandle2.set(unionSegmt2, 113355779911L);
			longHandle3.set(unionSegmt2, 778899001122L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals(longHandle1.get(resultSegmt), 891132446688L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 891132446688L);
			Assert.assertEquals(longHandle3.get(resultSegmt), 891132446688L);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromUnion,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt, 8.12F);
			floatHandle2.set(unionSegmt, 9.24F);

			float result = (float)mh.invokeExact(6.56F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 25.04F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromUnionPtr,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt, 35.11F);
			floatHandle2.set(unionSegmt, 46.22F);

			float result = (float)mh.invoke(79.32F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 171.76F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromNestedUnionByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromNestedUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromNestedUnion,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 31.22F);

			float result = (float)mh.invokeExact(37.88F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 131.54F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromNestedUnion_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromNestedUnion_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromNestedUnion_reverseOrder,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 31.22F);

			float result = (float)mh.invokeExact(37.88F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 131.54F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnionWithNestedFloatArrayByUpcallMH() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, JAVA_FLOAT);
		UnionLayout unionLayout = MemoryLayout.unionLayout(floatArray.withName("array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnionWithNestedFloatArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromUnionWithNestedFloatArray,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 111.11F);
			unionSegmt.set(JAVA_FLOAT, 4, 222.22F);

			float result = (float)mh.invokeExact(444.44F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 888.88F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnionWithNestedFloatArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, JAVA_FLOAT);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), floatArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnionWithNestedFloatArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromUnionWithNestedFloatArray_reverseOrder,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 111.11F);
			unionSegmt.set(JAVA_FLOAT, 4, 222.22F);

			float result = (float)mh.invokeExact(444.44F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 888.88F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnionWithNestedUnionArrayByUpcallMH() throws Throwable {
		UnionLayout floatUnion = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, floatUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnionWithNestedUnionArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromUnionWithNestedUnionArray,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 444.44F);
			unionSegmt.set(JAVA_FLOAT, 4, 555.55F);

			float result = (float)mh.invokeExact(666.66F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 3111.08F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout floatUnion = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, floatUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnionWithNestedUnionArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromUnionWithNestedUnionArray_reverseOrder,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 444.44F);
			unionSegmt.set(JAVA_FLOAT, 4, 555.55F);

			float result = (float)mh.invokeExact(666.66F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 3111.08F, 0.01F);
		}
	}

	@Test
	public void test_add2FloatUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2FloatUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2FloatUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt1, 25.12F);
			floatHandle2.set(unionSegmt1, 11.23F);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt2, 24.34F);
			floatHandle2.set(unionSegmt2, 13.45F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 24.68F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 24.68F, 0.01F);
		}
	}

	@Test
	public void test_add2FloatUnions_returnUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2FloatUnions_returnUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2FloatUnions_returnUnionPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt1, 25.12F);
			floatHandle2.set(unionSegmt1, 11.23F);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt2, 24.34F);
			floatHandle2.set(unionSegmt2, 13.47F);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 38.17F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 38.17F, 0.01F);
		}
	}

	@Test
	public void test_add3FloatUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"),  JAVA_FLOAT.withName("elem3"));
		VarHandle floatHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle floatHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3FloatUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3FloatUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt1, 25.12F);
			floatHandle2.set(unionSegmt1, 11.23F);
			floatHandle3.set(unionSegmt1, 45.67F);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt2, 24.34F);
			floatHandle2.set(unionSegmt2, 13.45F);
			floatHandle3.set(unionSegmt2, 69.72F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 115.39F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 115.39F, 0.01F);
			Assert.assertEquals((float)floatHandle3.get(resultSegmt), 115.39F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromUnion,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt, 2228.111D);
			doubleHandle2.set(unionSegmt, 2229.221D);

			double result = (double)mh.invokeExact(3336.333D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 7794.775D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromUnionPtr,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt, 2228.111D);
			doubleHandle2.set(unionSegmt, 2229.221D);

			double result = (double)mh.invokeExact(3336.333D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 7794.775D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedUnionByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_DOUBLE.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromNestedUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromNestedUnion,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 31.789D);

			double result = (double)mh.invokeExact(37.864D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 133.231D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedUnion_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), nestedUnionLayout.withName("union_elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromNestedUnion_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromNestedUnion_reverseOrder,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 31.789D);

			double result = (double)mh.invokeExact(37.864D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 133.231D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnionWithNestedDoubleArrayByUpcallMH() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, JAVA_DOUBLE);
		UnionLayout unionLayout = MemoryLayout.unionLayout(doubleArray.withName("array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnionWithNestedDoubleArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromUnionWithNestedDoubleArray,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			unionSegmt.set(JAVA_DOUBLE, 8, 222.222D);

			double result = (double)mh.invokeExact(444.444D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 888.888D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnionWithNestedDoubleArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, JAVA_DOUBLE);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), doubleArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnionWithNestedDoubleArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromUnionWithNestedDoubleArray_reverseOrder,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			unionSegmt.set(JAVA_DOUBLE, 8, 222.222D);

			double result = (double)mh.invokeExact(444.444D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 888.888D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnionWithNestedUnionArrayByUpcallMH() throws Throwable {
		UnionLayout doubleUnion = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, doubleUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnionWithNestedUnionArrayByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromUnionWithNestedUnionArray,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			unionSegmt.set(JAVA_DOUBLE, 8, 222.222D);

			double result = (double)mh.invokeExact(666.665D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1444.442D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnionWithNestedUnionArray_reverseOrderByUpcallMH() throws Throwable {
		UnionLayout doubleUnion = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, doubleUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnionWithNestedUnionArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromUnionWithNestedUnionArray_reverseOrder,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			unionSegmt.set(JAVA_DOUBLE, 8, 222.222D);

			double result = (double)mh.invokeExact(666.665D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1444.442D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2DoubleUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2DoubleUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt1, 11.222D);
			doubleHandle2.set(unionSegmt1, 22.333D);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt2, 33.444D);
			doubleHandle2.set(unionSegmt2, 44.555D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 66.888D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleUnions_returnUnionPtrByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2DoubleUnions_returnUnionPtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2DoubleUnions_returnUnionPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt1, 11.222D);
			doubleHandle2.set(unionSegmt1, 22.333D);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt2, 33.444D);
			doubleHandle2.set(unionSegmt2, 44.555D);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 111.443D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 111.443D, 0.001D);
		}
	}

	@Test
	public void test_add3DoubleUnions_returnUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"), JAVA_DOUBLE.withName("elem3"));
		VarHandle doubleHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle doubleHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add3DoubleUnions_returnUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add3DoubleUnions_returnUnion,
					FunctionDescriptor.of(unionLayout, unionLayout, unionLayout), arena);
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt1, 11.222D);
			doubleHandle2.set(unionSegmt1, 22.333D);
			doubleHandle3.set(unionSegmt1, 33.123D);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt2, 33.444D);
			doubleHandle2.set(unionSegmt2, 44.555D);
			doubleHandle3.set(unionSegmt2, 55.456D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2, upcallFuncAddr);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 88.579D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 88.579D, 0.001D);
			Assert.assertEquals((double)doubleHandle3.get(resultSegmt), 88.579D, 0.001D);
		}
	}

	@Test
	public void test_addShortAndShortFromUnionWithByteShortByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortFromUnionWithByteShortByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromUnionWithByteShort,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, (short)10001);
			Assert.assertEquals((short)elemHandle2.get(unionSegmt), 10001);

			short result = (short)mh.invokeExact((short)10020, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 20021);
		}
	}

	@Test
	public void test_addIntAndByteFromUnionWithIntByteByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndByteFromUnionWithIntByteByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndByteFromUnionWithIntByte,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, (byte)110);
			Assert.assertEquals((byte)elemHandle2.get(unionSegmt), 110);

			int result = (int)mh.invokeExact(22334455, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 22334565);
		}
	}

	@Test
	public void test_addIntAndIntFromUnionWithByteIntByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntFromUnionWithByteIntByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromUnionWithByteInt,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 1122334);
			Assert.assertEquals((int)elemHandle2.get(unionSegmt), 1122334);

			int result = (int)mh.invokeExact(11335577, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 12457911);
		}
	}

	@Test
	public void test_addIntAndShortFromUnionWithIntShortByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndShortFromUnionWithIntShortByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndShortFromUnionWithIntShort,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, (short)32766);
			Assert.assertEquals((short)elemHandle2.get(unionSegmt), 32766);

			int result = (int)mh.invokeExact(22334455, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 22367221);
		}
	}

	@Test
	public void test_addIntAndIntFromUnionWithShortIntByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntFromUnionWithShortIntByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntFromUnionWithShortInt,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 1122334);
			Assert.assertEquals((int)elemHandle2.get(unionSegmt), 1122334);

			int result = (int)mh.invokeExact(11335577, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 12457911);
		}
	}

	@Test
	public void test_addIntAndByteFromUnionWithIntShortByteByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_SHORT.withName("elem2"), JAVA_BYTE.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndByteFromUnionWithIntShortByteByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndByteFromUnionWithIntShortByte,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle3.set(unionSegmt, (byte)22);
			Assert.assertEquals((byte)elemHandle3.get(unionSegmt), 22);

			int result = (int)mh.invokeExact(22334455, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 22334477);
		}
	}

	@Test
	public void test_addIntAndShortFromUnionWithByteShortIntByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"),
				JAVA_SHORT.withName("elem2"), JAVA_INT.withName("elem3"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndShortFromUnionWithByteShortIntByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndShortFromUnionWithByteShortInt,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, (short)32766);
			Assert.assertEquals((short)elemHandle2.get(unionSegmt), 32766);

			int result = (int)mh.invokeExact(11335577, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 11368343);
		}
	}

	@Test
	public void test_addIntAndLongFromUnionWithIntLongByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndLongFromUnionWithIntLongByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndLongFromUnionWithIntLong,
					FunctionDescriptor.of(JAVA_LONG, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 667788990011L);
			Assert.assertEquals((long)elemHandle2.get(unionSegmt), 667788990011L);

			long result = (long)mh.invokeExact(22446688, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 667811436699L);
		}
	}

	@Test
	public void test_addIntAndLongFromUnionWithLongIntByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndLongFromUnionWithLongIntByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndLongFromUnionWithLongInt,
					FunctionDescriptor.of(JAVA_LONG, JAVA_INT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 667788990011L);
			Assert.assertEquals((long)elemHandle1.get(unionSegmt), 667788990011L);

			long result = (long)mh.invokeExact(1234567, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 667790224578L);
		}
	}

	@Test
	public void test_addLongAndPtrValueFromUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), ADDRESS.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndPtrValueFromUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndPtrValueFromUnion,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			MemorySegment longSegmt = arena.allocate(JAVA_LONG, 112233445566778899L);
			elemHandle3.set(unionSegmt, longSegmt);
			MemorySegment elemSegmt = ((MemorySegment)elemHandle3.get(unionSegmt)).reinterpret(JAVA_LONG.byteSize());
			Assert.assertEquals(elemSegmt.get(JAVA_LONG, 0), 112233445566778899L);

			long result = (long)mh.invokeExact(1020302010L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 112233446587080909L);
		}
	}

	@Test
	public void test_addLongAndIntFromUnionWithByteShortIntLongByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"),
				JAVA_SHORT.withName("elem2"), JAVA_INT.withName("elem3"), JAVA_LONG.withName("elem4"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndIntFromUnionWithByteShortIntLongByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndIntFromUnionWithByteShortIntLong,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle3.set(unionSegmt, 11223344);
			Assert.assertEquals((int)elemHandle3.get(unionSegmt), 11223344);

			long result = (long)mh.invokeExact(1020302010L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1031525354L);
		}
	}

	@Test
	public void test_addLongAndIntFromUnionWithLongIntShortByteByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"),
				JAVA_INT.withName("elem2"), JAVA_SHORT.withName("elem3"), JAVA_BYTE.withName("elem4"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndIntFromUnionWithLongIntShortByteByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndIntFromUnionWithLongIntShortByte,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 11223344);
			Assert.assertEquals((int)elemHandle2.get(unionSegmt), 11223344);

			long result = (long)mh.invokeExact(1020302010L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1031525354L);
		}
	}

	@Test
	public void test_addLongAndLongFromUnionWithLongFloatByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongFromUnionWithLongFloatByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromUnionWithLongFloat,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 100000000005L);
			Assert.assertEquals((long)elemHandle1.get(unionSegmt), 100000000005L);

			long result = (long)mh.invokeExact(1020302010L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 101020302015L);
		}
	}

	@Test
	public void test_addLongAndLongFromUnionWithIntFloatLongByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_LONG.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongFromUnionWithIntFloatLongByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromUnionWithIntFloatLong,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle3.set(unionSegmt, 100000000005L);
			Assert.assertEquals((long)elemHandle3.get(unionSegmt), 100000000005L);

			long result = (long)mh.invokeExact(1020302010L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 101020302015L);
		}
	}

	@Test
	public void test_addLongAndLongFromUnionWithIntLongFloatByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_LONG.withName("elem2"), JAVA_FLOAT.withName("elem3"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongFromUnionWithIntLongFloatByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromUnionWithIntLongFloat,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 100000000005L);
			Assert.assertEquals((long)elemHandle2.get(unionSegmt), 100000000005L);

			long result = (long)mh.invokeExact(1020302010L, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 101020302015L);
		}
	}

	@Test
	public void test_addFloatAndFloatFromUnionWithShortFloatByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromUnionWithShortFloatByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromUnionWithShortFloat,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0.11F);
			Assert.assertEquals((float)elemHandle2.get(unionSegmt), 0.11F);

			float result = (float)mh.invokeExact(0.22F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 0.33F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromUnionWithFloatShortByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromUnionWithFloatShortByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromUnionWithFloatShort,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 0.11F);
			Assert.assertEquals((float)elemHandle1.get(unionSegmt), 0.11F, 0.01F);

			float result = (float)mh.invokeExact(0.22F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 0.33F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromUnionWithIntFloatByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromUnionWithIntFloatByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromUnionWithIntFloat,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0.11F);
			Assert.assertEquals((float)elemHandle2.get(unionSegmt), 0.11F, 0.01F);

			float result = (float)mh.invokeExact(0.22F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 0.33F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromUnionWithFloatIntByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromUnionWithFloatIntByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromUnionWithFloatInt,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 0.11F);
			Assert.assertEquals((float)elemHandle1.get(unionSegmt), 0.11F, 0.01F);

			float result = (float)mh.invokeExact(0.22F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 0.33F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromUnionWithFloatLongByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromUnionWithFloatLongByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromUnionWithFloatLong,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 0.11F);
			Assert.assertEquals((float)elemHandle1.get(unionSegmt), 0.11F, 0.1F);

			float result = (float)mh.invokeExact(0.22F, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 0.33F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndPtrValueFromUnionByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2"), ADDRESS.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndPtrValueFromUnionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndPtrValueFromUnion,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			MemorySegment longSegmt = arena.allocate(JAVA_LONG, 112233445566778899L);
			elemHandle3.set(unionSegmt, longSegmt);
			MemorySegment elemSegmt = ((MemorySegment)elemHandle3.get(unionSegmt)).reinterpret(JAVA_LONG.byteSize());
			Assert.assertEquals(elemSegmt.get(JAVA_LONG, 0), 112233445566778899L);

			double result = (double)mh.invokeExact(222.44D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 112233445566779121.44D, 0.01D);
		}
	}

	@Test
	public void test_addDoubleAndIntFromUnionWithIntDoubleByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndIntFromUnionWithIntDoubleByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndIntFromUnionWithIntDouble,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 1122334455);
			Assert.assertEquals((int)elemHandle1.get(unionSegmt), 1122334455);

			double result = (double)mh.invokeExact(113.567D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1122334568.567D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndLongFromUnionWithDoubleLongByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndLongFromUnionWithDoubleLongByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndLongFromUnionWithDoubleLong,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 11223344556677L);
			Assert.assertEquals((long)elemHandle2.get(unionSegmt), 11223344556677L);

			double result = (double)mh.invokeExact(222.33D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 11223344556899.33D, 0.01D);
		}
	}

	@Test
	public void test_addDoubleAndLongFromUnionWithIntDoubleLongByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2"), JAVA_LONG.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndLongFromUnionWithIntDoubleLongByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndLongFromUnionWithIntDoubleLong,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle3.set(unionSegmt, 100000000001L);
			Assert.assertEquals((long)elemHandle3.get(unionSegmt), 100000000001L);

			double result = (double)mh.invokeExact(222.44D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 100000000223.44D, 0.01D);
		}
	}

	@Test
	public void test_addDoubleAndFloatFromUnionWithFloatDoubleByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatFromUnionWithFloatDoubleByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatFromUnionWithFloatDouble,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 18.444F);
			Assert.assertEquals((float)elemHandle1.get(unionSegmt), 18.444F, 001F);

			double result = (double)mh.invokeExact(113.567D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 132.011D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatFromUnionWithDoubleFloatByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatFromUnionWithDoubleFloatByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatFromUnionWithDoubleFloat,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 19.22F);
			Assert.assertEquals((float)elemHandle2.get(unionSegmt), 19.22F, 0.01D);

			double result = (double)mh.invokeExact(216.666D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 235.886D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatFromUnionWithIntFloatDoubleByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_DOUBLE.withName("elem3"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatFromUnionWithIntFloatDoubleByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatFromUnionWithIntFloatDouble,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 18.444F);
			Assert.assertEquals((float)elemHandle2.get(unionSegmt), 18.444F, 0.001F);

			double result = (double)mh.invokeExact(113.567D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 132.011D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatFromUnionWithIntDoubleFloatByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2"), JAVA_FLOAT.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatFromUnionWithIntDoubleFloatByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatFromUnionWithIntDoubleFloat,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle3.set(unionSegmt, 18.444F);
			Assert.assertEquals((float)elemHandle3.get(unionSegmt), 18.444F, 0.001F);

			double result = (double)mh.invokeExact(113.567D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 132.011D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromUnionWithDoubleIntByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFromUnionWithDoubleIntByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromUnionWithDoubleInt,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 218.555D);
			Assert.assertEquals((double)elemHandle1.get(unionSegmt), 218.555D, 0.001D);

			double result = (double)mh.invokeExact(216.666D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 435.221D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromUnionWithLongDoubleByUpcallMH() throws Throwable {
		UnionLayout unionLayout =MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFromUnionWithLongDoubleByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromUnionWithLongDouble,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 222.333D);
			Assert.assertEquals((double)elemHandle2.get(unionSegmt), 222.333D, 0.001D);

			double result = (double)mh.invokeExact(111.222D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 333.555D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromUnionWithFloatLongDoubleByUpcallMH() throws Throwable {
		UnionLayout unionLayout =MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_LONG.withName("elem2"), JAVA_DOUBLE.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFromUnionWithFloatLongDoubleByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromUnionWithFloatLongDouble,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle3.set(unionSegmt, 100.11D);
			Assert.assertEquals((double)elemHandle3.get(unionSegmt), 100.11D, 01D);

			double result = (double)mh.invokeExact(222.44D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 322.55D, 0.01D);
		}
	}

	@Test
	public void test_addDoubleAndShortFromUnionWithByteShortFloatDoubleByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"),
				JAVA_SHORT.withName("elem2"), JAVA_FLOAT.withName("elem3"), JAVA_DOUBLE.withName("elem4"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndShortFromUnionWithByteShortFloatDoubleByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndShortFromUnionWithByteShortFloatDouble,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, (short)32766);
			Assert.assertEquals((short)elemHandle2.get(unionSegmt), 32766);

			double result = (double)mh.invokeExact(33333.57D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 66099.57D, 0.01D);
		}
	}

	@Test
	public void test_addDoubleAndFloatFromUnionWithDoubleFloatShortByteByUpcallMH() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_SHORT.withName("elem3"), JAVA_BYTE.withName("elem4"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatFromUnionWithDoubleFloatShortByteByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatFromUnionWithDoubleFloatShortByte,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout), arena);
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 1.001F);
			Assert.assertEquals((float)elemHandle2.get(unionSegmt), 1.001F, 0.001F);

			double result = (double)mh.invokeExact(33333.567D, unionSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 33334.568D, 0.001D);
		}
	}
}
