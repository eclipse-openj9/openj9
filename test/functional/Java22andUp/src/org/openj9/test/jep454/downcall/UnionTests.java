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
package org.openj9.test.jep454.downcall;

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
 * Test cases for JEP 454: Foreign Linker API for argument/return union in downcall.
 *
 * Note:
 * The test suite is mainly intended for the following Linker API:
 * MethodHandle downcallHandle(MemorySegment symbol, FunctionDescriptor function)
 */
@Test(groups = { "level.sanity" })
public class UnionTests {
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addBoolAndBoolsFromUnionWithXor() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionWithXor").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			boolHandle1.set(unionSegmt, 0L, true);

			boolean result = (boolean)mh.invokeExact(true, unionSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromUnionPtrWithXor() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionPtrWithXor").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			boolHandle2.set(unionSegmt, 0L, false);

			boolean result = (boolean)mh.invoke(true, unionSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromNestedUnionWithXor() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromNestedUnionWithXor").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, true);

			boolean result = (boolean)mh.invokeExact(false, unionSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromNestedUnionWithXor_reverseOrder() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromNestedUnionWithXor_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, true);

			boolean result = (boolean)mh.invokeExact(true, unionSegmt);
			Assert.assertEquals(result, false);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromUnionWithNestedBoolArray() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, JAVA_BOOLEAN);
		UnionLayout unionLayout = MemoryLayout.unionLayout(boolArray.withName("array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionWithNestedBoolArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, true);
			unionSegmt.set(JAVA_BOOLEAN, 1, true);

			boolean result = (boolean)mh.invokeExact(false, unionSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromUnionWithNestedBoolArray_reverseOrder() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, JAVA_BOOLEAN);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), boolArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionWithNestedBoolArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, false);
			unionSegmt.set(JAVA_BOOLEAN, 1, true);

			boolean result = (boolean)mh.invokeExact(false, unionSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromUnionWithNestedUnionArray() throws Throwable {
		UnionLayout boolUnion = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, boolUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_BOOLEAN.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionWithNestedUnionArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, true);
			unionSegmt.set(JAVA_BOOLEAN, 1, true);

			boolean result = (boolean)mh.invokeExact(true, unionSegmt);
			Assert.assertEquals(result, false);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromUnionWithNestedUnionArray_reverseOrder() throws Throwable {
		UnionLayout boolUnion = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, boolUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionWithNestedUnionArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, false);
			unionSegmt.set(JAVA_BOOLEAN, 1, true);

			boolean result = (boolean)mh.invokeExact(true, unionSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_add2BoolUnionsWithXor_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolUnionsWithXor_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			boolHandle1.set(unionSegmt1, 0L, true);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			boolHandle2.set(unionSegmt2, 0L, false);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(boolHandle1.get(resultSegmt, 0L), true);
			Assert.assertEquals(boolHandle2.get(resultSegmt, 0L), true);
		}
	}

	@Test
	public void test_add2BoolUnionsWithXor_returnUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolUnionsWithXor_returnUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			boolHandle2.set(unionSegmt1, 0L, true);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			boolHandle1.set(unionSegmt2, 0L, true);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), true);
		}
	}

	@Test
	public void test_add3BoolUnionsWithXor_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"),
				JAVA_BOOLEAN.withName("elem2"), JAVA_BOOLEAN.withName("elem3"));
		VarHandle boolHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle boolHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3BoolUnionsWithXor_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			boolHandle3.set(unionSegmt1, 0L, true);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			boolHandle2.set(unionSegmt2, 0L, false);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(boolHandle1.get(resultSegmt, 0L), true);
			Assert.assertEquals(boolHandle2.get(resultSegmt, 0L), true);
			Assert.assertEquals(boolHandle3.get(resultSegmt, 0L), true);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt, 0L, (byte)8);

			byte result = (byte)mh.invokeExact((byte)6, unionSegmt);
			Assert.assertEquals(result, 22);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			byteHandle2.set(unionSegmt, 0L, (byte)12);

			byte result = (byte)mh.invoke((byte)13, unionSegmt);
			Assert.assertEquals(result, 37);
		}
	}

	@Test
	public void test_addByteAndBytesFromNestedUnion() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromNestedUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)11);

			byte result = (byte)mh.invokeExact((byte)33, unionSegmt);
			Assert.assertEquals(result, 66);
		}
	}

	@Test
	public void test_addByteAndBytesFromNestedUnion_reverseOrder() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromNestedUnion_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)12);

			byte result = (byte)mh.invokeExact((byte)48, unionSegmt);
			Assert.assertEquals(result, 84);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnionWithNestedByteArray() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, JAVA_BYTE);
		UnionLayout unionLayout = MemoryLayout.unionLayout(byteArray.withName("array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnionWithNestedByteArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)11);
			unionSegmt.set(JAVA_BYTE, 1, (byte)12);

			byte result = (byte)mh.invokeExact((byte)14, unionSegmt);
			Assert.assertEquals(result, 48);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnionWithNestedByteArray_reverseOrder() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, JAVA_BYTE);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), byteArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnionWithNestedByteArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)12);
			unionSegmt.set(JAVA_BYTE, 1, (byte)14);

			byte result = (byte)mh.invokeExact((byte)18, unionSegmt);
			Assert.assertEquals(result, 56);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnionWithNestedUnionArray() throws Throwable {
		UnionLayout byteUnion = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, byteUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_BYTE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnionWithNestedUnionArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)11);
			unionSegmt.set(JAVA_BYTE, 1, (byte)12);

			byte result = (byte)mh.invokeExact((byte)16, unionSegmt);
			Assert.assertEquals(result, 73);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnionWithNestedUnionArray_reverseOrder() throws Throwable {
		UnionLayout byteUnion = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, byteUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnionWithNestedUnionArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)12);
			unionSegmt.set(JAVA_BYTE, 1, (byte)14);

			byte result = (byte)mh.invokeExact((byte)22, unionSegmt);
			Assert.assertEquals(result, 86);
		}
	}

	@Test
	public void test_add2ByteUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ByteUnions_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt1, 0L, (byte)25);
			byteHandle2.set(unionSegmt1, 0L, (byte)11);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt2, 0L, (byte)24);
			byteHandle2.set(unionSegmt2, 0L, (byte)13);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt, 0L), (byte)24);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt, 0L), (byte)24);
		}
	}

	@Test
	public void test_add2ByteUnions_returnUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ByteUnions_returnUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt1, 0L, (byte)25);
			byteHandle2.set(unionSegmt1, 0L, (byte)11);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt2, 0L, (byte)24);
			byteHandle2.set(unionSegmt2, 0L, (byte)13);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt, 0L), (byte)37);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt, 0L), (byte)37);
		}
	}

	@Test
	public void test_add3ByteUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"),
				JAVA_BYTE.withName("elem2"), JAVA_BYTE.withName("elem3"));
		VarHandle byteHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle byteHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3ByteUnions_returnUnion").get();
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt1, 0L, (byte)25);
			byteHandle2.set(unionSegmt1, 0L, (byte)11);
			byteHandle3.set(unionSegmt1, 0L, (byte)12);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			byteHandle1.set(unionSegmt2, 0L, (byte)24);
			byteHandle2.set(unionSegmt2, 0L, (byte)13);
			byteHandle3.set(unionSegmt2, 0L, (byte)16);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt, 0L), (byte)28);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt, 0L), (byte)28);
			Assert.assertEquals((byte)byteHandle3.get(resultSegmt, 0L), (byte)28);
		}
	}

	@Test
	public void test_addCharAndCharsFromUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt, 0L, 'A');
			charHandle2.set(unionSegmt, 0L, 'B');

			char result = (char)mh.invokeExact('C', unionSegmt);
			Assert.assertEquals(result, 'E');
		}
	}

	@Test
	public void test_addCharAndCharsFromUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt, 0L, 'H');
			charHandle2.set(unionSegmt, 0L, 'I');

			char result = (char)mh.invoke('G', unionSegmt);
			Assert.assertEquals(result, 'W');
		}
	}

	@Test
	public void test_addCharAndCharsFromNestedUnion() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromNestedUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'E');

			char result = (char)mh.invokeExact('H', unionSegmt);
			Assert.assertEquals(result, 'T');
		}
	}

	@Test
	public void test_addCharAndCharsFromNestedUnion_reverseOrder() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromNestedUnion_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'F');

			char result = (char)mh.invokeExact('H', unionSegmt);
			Assert.assertEquals(result, 'W');
		}
	}

	@Test
	public void test_addCharAndCharsFromUnionWithNestedCharArray() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, JAVA_CHAR);
		UnionLayout unionLayout = MemoryLayout.unionLayout(charArray.withName("array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnionWithNestedCharArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'A');
			unionSegmt.set(JAVA_CHAR, 2, 'B');

			char result = (char)mh.invokeExact('D', unionSegmt);
			Assert.assertEquals(result, 'E');
		}
	}

	@Test
	public void test_addCharAndCharsFromUnionWithNestedCharArray_reverseOrder() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, JAVA_CHAR);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), charArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnionWithNestedCharArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'A');
			unionSegmt.set(JAVA_CHAR, 2, 'B');

			char result = (char)mh.invokeExact('D', unionSegmt);
			Assert.assertEquals(result, 'E');
		}
	}

	@Test
	public void test_addCharAndCharsFromUnionWithNestedUnionArray() throws Throwable {
		UnionLayout charUnion = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, charUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnionWithNestedUnionArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'B');
			unionSegmt.set(JAVA_CHAR, 2, 'C');

			char result = (char)mh.invokeExact('J', unionSegmt);
			Assert.assertEquals(result, 'Q');
		}
	}

	@Test
	public void test_addCharAndCharsFromUnionWithNestedUnionArray_reverseOrder() throws Throwable {
		UnionLayout charUnion = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, charUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnionWithNestedUnionArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'B');
			unionSegmt.set(JAVA_CHAR, 2, 'C');

			char result = (char)mh.invokeExact('J', unionSegmt);
			Assert.assertEquals(result, 'Q');
		}
	}

	@Test
	public void test_add2CharUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2CharUnions_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt1, 0L, 'A');
			charHandle2.set(unionSegmt1, 0L, 'B');
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt2, 0L, 'C');
			charHandle2.set(unionSegmt2, 0L, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(charHandle1.get(resultSegmt, 0L), 'E');
			Assert.assertEquals(charHandle2.get(resultSegmt, 0L), 'E');
		}
	}

	@Test
	public void test_add2CharUnions_returnUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2CharUnions_returnUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt1, 0L, 'A');
			charHandle2.set(unionSegmt1, 0L, 'B');
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt2, 0L, 'B');
			charHandle2.set(unionSegmt2, 0L, 'C');

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals(charHandle1.get(resultSegmt, 0L), 'F');
			Assert.assertEquals(charHandle2.get(resultSegmt, 0L), 'F');
		}
	}

	@Test
	public void test_add3CharUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"),
				JAVA_CHAR.withName("elem2"), JAVA_CHAR.withName("elem3"));
		VarHandle charHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle charHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3CharUnions_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt1, 0L, 'A');
			charHandle2.set(unionSegmt1, 0L, 'B');
			charHandle3.set(unionSegmt1, 0L, 'C');
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			charHandle1.set(unionSegmt2, 0L, 'B');
			charHandle2.set(unionSegmt2, 0L, 'C');
			charHandle3.set(unionSegmt2, 0L, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(charHandle1.get(resultSegmt, 0L), 'F');
			Assert.assertEquals(charHandle2.get(resultSegmt, 0L), 'F');
			Assert.assertEquals(charHandle3.get(resultSegmt, 0L), 'F');
		}
	}

	@Test
	public void test_addShortAndShortsFromUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			shortHandle2.set(unionSegmt, 0L, (short)9);

			short result = (short)mh.invokeExact((short)6, unionSegmt);
			Assert.assertEquals(result, 24);
		}
	}

	@Test
	public void test_addShortAndShortsFromUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt, 0L, (short)22);

			short result = (short)mh.invoke((short)66, unionSegmt);
			Assert.assertEquals(result, 110);
		}
	}

	@Test
	public void test_addShortAndShortsFromNestedUnion() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromNestedUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)31);

			short result = (short)mh.invokeExact((short)37, unionSegmt);
			Assert.assertEquals(result, 130);
		}
	}

	@Test
	public void test_addShortAndShortsFromNestedUnion_reverseOrder() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromNestedUnion_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)31);

			short result = (short)mh.invokeExact((short)37, unionSegmt);
			Assert.assertEquals(result, 130);
		}
	}

	@Test
	public void test_addShortAndShortsFromUnionWithNestedShortArray() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, JAVA_SHORT);
		UnionLayout unionLayout = MemoryLayout.unionLayout(shortArray.withName("array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnionWithNestedShortArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)111);
			unionSegmt.set(JAVA_SHORT, 2, (short)222);

			short result = (short)mh.invokeExact((short)444, unionSegmt);
			Assert.assertEquals(result, 888);
		}
	}

	@Test
	public void test_addShortAndShortsFromUnionWithNestedShortArray_reverseOrder() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, JAVA_SHORT);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), shortArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnionWithNestedShortArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)111);
			unionSegmt.set(JAVA_SHORT, 2, (short)222);

			short result = (short)mh.invokeExact((short)444, unionSegmt);
			Assert.assertEquals(result, 888);
		}
	}

	@Test
	public void test_addShortAndShortsFromUnionWithNestedUnionArray() throws Throwable {
		UnionLayout shortUnion = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, shortUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnionWithNestedUnionArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)111);
			unionSegmt.set(JAVA_SHORT, 2, (short)222);

			short result = (short)mh.invokeExact((short)666, unionSegmt);
			Assert.assertEquals(result, 1443);
		}
	}

	@Test
	public void test_addShortAndShortsFromUnionWithNestedUnionArray_reverseOrder() throws Throwable {
		UnionLayout shortUnion = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, shortUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnionWithNestedUnionArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)111);
			unionSegmt.set(JAVA_SHORT, 2, (short)222);

			short result = (short)mh.invokeExact((short)666, unionSegmt);
			Assert.assertEquals(result, 1443);
		}
	}

	@Test
	public void test_add2ShortUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ShortUnions_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt1, 0L, (short)56);
			shortHandle2.set(unionSegmt1, 0L, (short)45);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt2, 0L, (short)78);
			shortHandle2.set(unionSegmt2, 0L, (short)67);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt, 0L), (short)112);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt, 0L), (short)112);
		}
	}

	@Test
	public void test_add2ShortUnions_returnUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ShortUnions_returnUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt1, 0L, (short)56);
			shortHandle2.set(unionSegmt1, 0L, (short)45);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt2, 0L, (short)78);
			shortHandle2.set(unionSegmt2, 0L, (short)67);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals((short)shortHandle1.get(resultSegmt, 0L), (short)179);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt, 0L), (short)179);
		}
	}

	@Test
	public void test_add3ShortUnions_returnUnion() throws Throwable  {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"),
				JAVA_SHORT.withName("elem2"), JAVA_SHORT.withName("elem3"));
		VarHandle shortHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle shortHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3ShortUnions_returnUnion").get();

		try (Arena arena = Arena.ofConfined()) {
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt1, 0L, (short)25);
			shortHandle2.set(unionSegmt1, 0L, (short)26);
			shortHandle3.set(unionSegmt1, 0L, (short)27);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			shortHandle1.set(unionSegmt2, 0L, (short)34);
			shortHandle2.set(unionSegmt2, 0L, (short)35);
			shortHandle3.set(unionSegmt2, 0L, (short)36);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt, 0L), (short)63);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt, 0L), (short)63);
			Assert.assertEquals((short)shortHandle3.get(resultSegmt, 0L), (short)63);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			intHandle2.set(unionSegmt, 0L, 1234567);

			int result = (int)mh.invokeExact(2244668, unionSegmt);
			Assert.assertEquals(result, 4713802);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt, 0L, 11121314);
			intHandle2.set(unionSegmt, 0L, 15161718);

			int result = (int)mh.invoke(19202122, unionSegmt);
			Assert.assertEquals(result, 49525558);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedUnion() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_INT.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromNestedUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 21222324);

			int result = (int)mh.invokeExact(33343536, unionSegmt);
			Assert.assertEquals(result, 97010508);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedUnion_reverseOrder() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromNestedUnion_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 21222324);

			int result = (int)mh.invokeExact(33343536, unionSegmt);
			Assert.assertEquals(result, 97010508);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionWithNestedIntArray() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, JAVA_INT);
		UnionLayout unionLayout = MemoryLayout.unionLayout(intArray.withName("array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionWithNestedIntArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 1111111);
			unionSegmt.set(JAVA_INT, 4, 2222222);

			int result = (int)mh.invokeExact(4444444, unionSegmt);
			Assert.assertEquals(result, 8888888);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionWithNestedIntArray_reverseOrder() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, JAVA_INT);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), intArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionWithNestedIntArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 1111111);
			unionSegmt.set(JAVA_INT, 4, 2222222);

			int result = (int)mh.invokeExact(4444444, unionSegmt);
			Assert.assertEquals(result, 8888888);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionWithNestedUnionArray() throws Throwable {
		UnionLayout intUnion = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, intUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionWithNestedUnionArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 2222222);
			unionSegmt.set(JAVA_INT, 4, 3333333);

			int result = (int)mh.invokeExact(6666666, unionSegmt);
			Assert.assertEquals(result, 19999998);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionWithNestedUnionArray_reverseOrder() throws Throwable {
		UnionLayout intUnion = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, intUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionWithNestedUnionArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 2222222);
			unionSegmt.set(JAVA_INT, 4, 3333333);

			int result = (int)mh.invokeExact(6666666, unionSegmt);
			Assert.assertEquals(result, 19999998);
		}
	}

	@Test
	public void test_add2IntUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntUnions_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt1, 0L, 11223344);
			intHandle2.set(unionSegmt1, 0L, 55667788);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt2, 0L, 99001122);
			intHandle2.set(unionSegmt2, 0L, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(intHandle1.get(resultSegmt, 0L), 89113354);
			Assert.assertEquals(intHandle2.get(resultSegmt, 0L), 89113354);
		}
	}

	@Test
	public void test_add2IntUnions_returnUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntUnions_returnUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt1, 0L, 11223344);
			intHandle2.set(unionSegmt1, 0L, 55667788);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt2, 0L, 99001122);
			intHandle2.set(unionSegmt2, 0L, 33445566);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals(intHandle1.get(resultSegmt, 0L), 122558920);
			Assert.assertEquals(intHandle2.get(resultSegmt, 0L), 122558920);
		}
	}

	@Test
	public void test_add3IntUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"), JAVA_INT.withName("elem3"));
		VarHandle intHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle intHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3IntUnions_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt1, 0L, 11223344);
			intHandle2.set(unionSegmt1, 0L, 55667788);
			intHandle3.set(unionSegmt1, 0L, 99001122);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			intHandle1.set(unionSegmt2, 0L, 33445566);
			intHandle2.set(unionSegmt2, 0L, 77889900);
			intHandle3.set(unionSegmt2, 0L, 44332211);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(intHandle1.get(resultSegmt, 0L), 143333333);
			Assert.assertEquals(intHandle2.get(resultSegmt, 0L), 143333333);
			Assert.assertEquals(intHandle3.get(resultSegmt, 0L), 143333333);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt, 0L, 1234567890L);
			longHandle2.set(unionSegmt, 0L, 9876543210L);

			long result = (long)mh.invokeExact(2468024680L, unionSegmt);
			Assert.assertEquals(result, 22221111100L);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt, 0L, 224466880022L);
			longHandle2.set(unionSegmt, 0L, 446688002244L);

			long result = (long)mh.invoke(668800224466L, unionSegmt);
			Assert.assertEquals(result, 1562176228954L);
		}
	}

	@Test
	public void test_addLongAndLongsFromNestedUnion() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromNestedUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 135791357913L);

			long result = (long)mh.invokeExact(778899001122L, unionSegmt);
			Assert.assertEquals(result, 1186273074861L);
		}
	}

	@Test
	public void test_addLongAndLongsFromNestedUnion_reverseOrder() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromNestedUnion_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 135791357913L);

			long result = (long)mh.invokeExact(778899001122L, unionSegmt);
			Assert.assertEquals(result, 1186273074861L);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnionWithNestedLongArray() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, JAVA_LONG);
		UnionLayout unionLayout = MemoryLayout.unionLayout(longArray.withName("array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnionWithNestedLongArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 111111111L);
			unionSegmt.set(JAVA_LONG, 8, 222222222L);

			long result = (long)mh.invokeExact(444444444L, unionSegmt);
			Assert.assertEquals(result, 888888888L);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnionWithNestedLongArray_reverseOrder() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, JAVA_LONG);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), longArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnionWithNestedLongArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 111111111L);
			unionSegmt.set(JAVA_LONG, 8, 222222222L);

			long result = (long)mh.invokeExact(444444444L, unionSegmt);
			Assert.assertEquals(result, 888888888L);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnionWithNestedUnionArray() throws Throwable {
		UnionLayout longUnion = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, longUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_LONG.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnionWithNestedUnionArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 111111111L);
			unionSegmt.set(JAVA_LONG, 8, 222222222L);

			long result = (long)mh.invokeExact(666666666L, unionSegmt);
			Assert.assertEquals(result, 1444444443L);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnionWithNestedUnionArray_reverseOrder() throws Throwable {
		UnionLayout longUnion = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, longUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnionWithNestedUnionArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 111111111L);
			unionSegmt.set(JAVA_LONG, 8, 222222222L);

			long result = (long)mh.invokeExact(666666666L, unionSegmt);
			Assert.assertEquals(result, 1444444443L);
		}
	}

	@Test
	public void test_add2LongUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2LongUnions_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt1, 0L, 987654321987L);
			longHandle2.set(unionSegmt1, 0L, 123456789123L);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt2, 0L, 224466880022L);
			longHandle2.set(unionSegmt2, 0L, 113355779911L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(longHandle1.get(resultSegmt, 0L), 236812569034L);
			Assert.assertEquals(longHandle2.get(resultSegmt, 0L), 236812569034L);
		}
	}

	@Test
	public void test_add2LongUnions_returnUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2LongUnions_returnUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt1, 0L, 1122334455L);
			longHandle2.set(unionSegmt1, 0L, 5566778899L);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt2, 0L, 9900112233L);
			longHandle2.set(unionSegmt2, 0L, 3344556677L);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals(longHandle1.get(resultSegmt, 0L), 12255892253L);
			Assert.assertEquals(longHandle2.get(resultSegmt, 0L), 12255892253L);
		}
	}

	@Test
	public void test_add3LongUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"), JAVA_LONG.withName("elem3"));
		VarHandle longHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle longHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3LongUnions_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt1, 0L, 987654321987L);
			longHandle2.set(unionSegmt1, 0L, 123456789123L);
			longHandle3.set(unionSegmt1, 0L, 112233445566L);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			longHandle1.set(unionSegmt2, 0L, 224466880022L);
			longHandle2.set(unionSegmt2, 0L, 113355779911L);
			longHandle3.set(unionSegmt2, 0L, 778899001122L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(longHandle1.get(resultSegmt, 0L), 891132446688L);
			Assert.assertEquals(longHandle2.get(resultSegmt, 0L), 891132446688L);
			Assert.assertEquals(longHandle3.get(resultSegmt, 0L), 891132446688L);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt, 0L, 8.12F);
			floatHandle2.set(unionSegmt, 0L, 9.24F);

			float result = (float)mh.invokeExact(6.56F, unionSegmt);
			Assert.assertEquals(result, 25.04F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt, 0L, 35.11F);
			floatHandle2.set(unionSegmt, 0L, 46.22F);

			float result = (float)mh.invoke(79.32F, unionSegmt);
			Assert.assertEquals(result, 171.76F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromNestedUnion() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromNestedUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 31.22F);

			float result = (float)mh.invokeExact(37.88F, unionSegmt);
			Assert.assertEquals(result, 131.54F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromNestedUnion_reverseOrder() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromNestedUnion_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 31.22F);

			float result = (float)mh.invokeExact(37.88F, unionSegmt);
			Assert.assertEquals(result, 131.54F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnionWithNestedFloatArray() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, JAVA_FLOAT);
		UnionLayout unionLayout = MemoryLayout.unionLayout(floatArray.withName("array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnionWithNestedFloatArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 111.11F);
			unionSegmt.set(JAVA_FLOAT, 4, 222.22F);

			float result = (float)mh.invokeExact(444.44F, unionSegmt);
			Assert.assertEquals(result, 888.88F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnionWithNestedFloatArray_reverseOrder() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, JAVA_FLOAT);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), floatArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnionWithNestedFloatArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 111.11F);
			unionSegmt.set(JAVA_FLOAT, 4, 222.22F);

			float result = (float)mh.invokeExact(444.44F, unionSegmt);
			Assert.assertEquals(result, 888.88F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnionWithNestedUnionArray() throws Throwable {
		UnionLayout floatUnion = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, floatUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnionWithNestedUnionArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 444.44F);
			unionSegmt.set(JAVA_FLOAT, 4, 555.55F);

			float result = (float)mh.invokeExact(666.66F, unionSegmt);
			Assert.assertEquals(result, 3111.08F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnionWithNestedUnionArray_reverseOrder() throws Throwable {
		UnionLayout floatUnion = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, floatUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnionWithNestedUnionArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 444.44F);
			unionSegmt.set(JAVA_FLOAT, 4, 555.55F);

			float result = (float)mh.invokeExact(666.66F, unionSegmt);
			Assert.assertEquals(result, 3111.08F, 0.01F);
		}
	}

	@Test
	public void test_add2FloatUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2FloatUnions_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt1, 0L, 25.12F);
			floatHandle2.set(unionSegmt1, 0L, 11.23F);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt2, 0L, 24.34F);
			floatHandle2.set(unionSegmt2, 0L, 13.45F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt, 0L), 24.68F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt, 0L), 24.68F, 0.01F);
		}
	}

	@Test
	public void test_add2FloatUnions_returnUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2FloatUnions_returnUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt1, 0L, 25.12F);
			floatHandle2.set(unionSegmt1, 0L, 11.23F);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt2, 0L, 24.34F);
			floatHandle2.set(unionSegmt2, 0L, 13.47F);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals((float)floatHandle1.get(resultSegmt, 0L), 38.17F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt, 0L), 38.17F, 0.01F);
		}
	}

	@Test
	public void test_add3FloatUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"),  JAVA_FLOAT.withName("elem3"));
		VarHandle floatHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle floatHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3FloatUnions_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt1, 0L, 25.12F);
			floatHandle2.set(unionSegmt1, 0L, 11.23F);
			floatHandle3.set(unionSegmt1, 0L, 45.67F);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			floatHandle1.set(unionSegmt2, 0L, 24.34F);
			floatHandle2.set(unionSegmt2, 0L, 13.45F);
			floatHandle3.set(unionSegmt2, 0L, 69.72F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt, 0L), 115.39F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt, 0L), 115.39F, 0.01F);
			Assert.assertEquals((float)floatHandle3.get(resultSegmt, 0L), 115.39F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt, 0L, 2228.111D);
			doubleHandle2.set(unionSegmt, 0L, 2229.221D);

			double result = (double)mh.invokeExact(3336.333D, unionSegmt);
			Assert.assertEquals(result, 7794.775D, 0.001D);
		}
	}


	@Test
	public void test_addDoubleAndDoublesFromUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt, 0L, 2228.111D);
			doubleHandle2.set(unionSegmt, 0L, 2229.221D);

			double result = (double)mh.invokeExact(3336.333D, unionSegmt);
			Assert.assertEquals(result, 7794.775D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedUnion() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(nestedUnionLayout.withName("union_elem1"), JAVA_DOUBLE.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromNestedUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 31.789D);

			double result = (double)mh.invokeExact(37.864D, unionSegmt);
			Assert.assertEquals(result, 133.231D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedUnion_reverseOrder() throws Throwable {
		UnionLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), nestedUnionLayout.withName("union_elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromNestedUnion_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 31.789D);

			double result = (double)mh.invokeExact(37.864D, unionSegmt);
			Assert.assertEquals(result, 133.231D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnionWithNestedDoubleArray() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, JAVA_DOUBLE);
		UnionLayout unionLayout = MemoryLayout.unionLayout(doubleArray.withName("array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnionWithNestedDoubleArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			unionSegmt.set(JAVA_DOUBLE, 8, 222.222D);

			double result = (double)mh.invokeExact(444.444D, unionSegmt);
			Assert.assertEquals(result, 888.888D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnionWithNestedDoubleArray_reverseOrder() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, JAVA_DOUBLE);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), doubleArray.withName("array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnionWithNestedDoubleArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			unionSegmt.set(JAVA_DOUBLE, 8, 222.222D);

			double result = (double)mh.invokeExact(444.444D, unionSegmt);
			Assert.assertEquals(result, 888.888D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnionWithNestedUnionArray() throws Throwable {
		UnionLayout doubleUnion = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, doubleUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(unionArray.withName("union_array_elem1"), JAVA_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnionWithNestedUnionArray").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			unionSegmt.set(JAVA_DOUBLE, 8, 222.222D);

			double result = (double)mh.invokeExact(666.665D, unionSegmt);
			Assert.assertEquals(result, 1444.442D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnionWithNestedUnionArray_reverseOrder() throws Throwable {
		UnionLayout doubleUnion = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		SequenceLayout unionArray = MemoryLayout.sequenceLayout(2, doubleUnion);
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), unionArray.withName("union_array_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnionWithNestedUnionArray_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 111.111D);
			unionSegmt.set(JAVA_DOUBLE, 8, 222.222D);

			double result = (double)mh.invokeExact(666.665D, unionSegmt);
			Assert.assertEquals(result, 1444.442D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2DoubleUnions_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt1, 0L, 11.222D);
			doubleHandle2.set(unionSegmt1, 0L, 22.333D);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt2, 0L, 33.444D);
			doubleHandle2.set(unionSegmt2, 0L, 44.555D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt, 0L), 66.888D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt, 0L), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleUnions_returnUnionPtr() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2DoubleUnions_returnUnionPtr").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt1, 0L, 11.222D);
			doubleHandle2.set(unionSegmt1, 0L, 22.333D);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt2, 0L, 33.444D);
			doubleHandle2.set(unionSegmt2, 0L, 44.555D);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(unionSegmt1, unionSegmt2);
			MemorySegment resultSegmt = resultAddr.reinterpret(unionLayout.byteSize());
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt, 0L), 111.443D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt, 0L), 111.443D, 0.001D);
		}
	}

	@Test
	public void test_add3DoubleUnions_returnUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"), JAVA_DOUBLE.withName("elem3"));
		VarHandle doubleHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle doubleHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add3DoubleUnions_returnUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt1, 0L, 11.222D);
			doubleHandle2.set(unionSegmt1, 0L, 22.333D);
			doubleHandle3.set(unionSegmt1, 0L, 33.123D);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			doubleHandle1.set(unionSegmt2, 0L, 33.444D);
			doubleHandle2.set(unionSegmt2, 0L, 44.555D);
			doubleHandle3.set(unionSegmt2, 0L, 55.456D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt, 0L), 88.579D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt, 0L), 88.579D, 0.001D);
			Assert.assertEquals((double)doubleHandle3.get(resultSegmt, 0L), 88.579D, 0.001D);
		}
	}

	@Test
	public void test_addShortAndShortFromUnionWithByteShort() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortFromUnionWithByteShort").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, (short)10001);
			Assert.assertEquals((short)elemHandle2.get(unionSegmt, 0L), 10001);

			short result = (short)mh.invokeExact((short)10020, unionSegmt);
			Assert.assertEquals(result, 20021);
		}
	}

	@Test
	public void test_addShortAndShortFromUnionWithShortByte_reverseOrder() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortFromUnionWithShortByte_reverseOrder").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 0L, (short)10001);
			Assert.assertEquals((short)elemHandle1.get(unionSegmt, 0L), 10001);

			short result = (short)mh.invokeExact((short)10020, unionSegmt);
			Assert.assertEquals(result, 20021);
		}
	}

	@Test
	public void test_addIntAndByteFromUnionWithIntByte() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndByteFromUnionWithIntByte").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, (byte)110);
			Assert.assertEquals((byte)elemHandle2.get(unionSegmt, 0L), 110);

			int result = (int)mh.invokeExact(22334455, unionSegmt);
			Assert.assertEquals(result, 22334565);
		}
	}

	@Test
	public void test_addIntAndIntFromUnionWithByteInt() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntFromUnionWithByteInt").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, 1122334);
			Assert.assertEquals((int)elemHandle2.get(unionSegmt, 0L), 1122334);

			int result = (int)mh.invokeExact(11335577, unionSegmt);
			Assert.assertEquals(result, 12457911);
		}
	}

	@Test
	public void test_addIntAndShortFromUnionWithIntShort() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndShortFromUnionWithIntShort").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, (short)32766);
			Assert.assertEquals((short)elemHandle2.get(unionSegmt, 0L), 32766);

			int result = (int)mh.invokeExact(22334455, unionSegmt);
			Assert.assertEquals(result, 22367221);
		}
	}

	@Test
	public void test_addIntAndIntFromUnionWithShortInt() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntFromUnionWithShortInt").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, 1122334);
			Assert.assertEquals((int)elemHandle2.get(unionSegmt, 0L), 1122334);

			int result = (int)mh.invokeExact(11335577, unionSegmt);
			Assert.assertEquals(result, 12457911);
		}
	}

	@Test
	public void test_addIntAndByteFromUnionWithIntShortByte() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_SHORT.withName("elem2"), JAVA_BYTE.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndByteFromUnionWithIntShortByte").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle3.set(unionSegmt, 0L, (byte)22);
			Assert.assertEquals((byte)elemHandle3.get(unionSegmt, 0L), 22);

			int result = (int)mh.invokeExact(22334455, unionSegmt);
			Assert.assertEquals(result, 22334477);
		}
	}

	@Test
	public void test_addIntAndShortFromUnionWithByteShortInt() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"),
				JAVA_SHORT.withName("elem2"), JAVA_INT.withName("elem3"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndShortFromUnionWithByteShortInt").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, (short)32766);
			Assert.assertEquals((short)elemHandle2.get(unionSegmt, 0L), 32766);

			int result = (int)mh.invokeExact(11335577, unionSegmt);
			Assert.assertEquals(result, 11368343);
		}
	}

	@Test
	public void test_addIntAndLongFromUnionWithIntLong() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndLongFromUnionWithIntLong").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, 667788990011L);
			Assert.assertEquals((long)elemHandle2.get(unionSegmt, 0L), 667788990011L);

			long result = (long)mh.invokeExact(22446688, unionSegmt);
			Assert.assertEquals(result, 667811436699L);
		}
	}

	@Test
	public void test_addIntAndLongFromUnionWithLongInt() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndLongFromUnionWithLongInt").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 0L, 667788990011L);
			Assert.assertEquals((long)elemHandle1.get(unionSegmt, 0L), 667788990011L);

			long result = (long)mh.invokeExact(1234567, unionSegmt);
			Assert.assertEquals(result, 667790224578L);
		}
	}

	@Test
	public void test_addFloatAndFloatFromUnionWithShortFloat() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromUnionWithShortFloat").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, 0.11F);
			Assert.assertEquals((float)elemHandle2.get(unionSegmt, 0L), 0.11F);

			float result = (float)mh.invokeExact(0.22F, unionSegmt);
			Assert.assertEquals(result, 0.33F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromUnionWithFloatShort() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromUnionWithFloatShort").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 0L, 0.11F);
			Assert.assertEquals((float)elemHandle1.get(unionSegmt, 0L), 0.11F, 0.01F);

			float result = (float)mh.invokeExact(0.22F, unionSegmt);
			Assert.assertEquals(result, 0.33F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromUnionWithIntFloat() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromUnionWithIntFloat").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, 0.11F);
			Assert.assertEquals((float)elemHandle2.get(unionSegmt, 0L), 0.11F, 0.01F);

			float result = (float)mh.invokeExact(0.22F, unionSegmt);
			Assert.assertEquals(result, 0.33F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromUnionWithFloatInt() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromUnionWithFloatInt").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 0L, 0.11F);
			Assert.assertEquals((float)elemHandle1.get(unionSegmt, 0L), 0.11F, 0.01F);

			float result = (float)mh.invokeExact(0.22F, unionSegmt);
			Assert.assertEquals(result, 0.33F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndFloatFromUnionWithFloatDouble() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatFromUnionWithFloatDouble").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 0L, 18.444F);
			Assert.assertEquals((float)elemHandle1.get(unionSegmt, 0L), 18.444F, 001F);

			double result = (double)mh.invokeExact(113.567D, unionSegmt);
			Assert.assertEquals(result, 132.011D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatFromUnionWithDoubleFloat() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatFromUnionWithDoubleFloat").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, 19.22F);
			Assert.assertEquals((float)elemHandle2.get(unionSegmt, 0L), 19.22F, 0.01D);

			double result = (double)mh.invokeExact(216.666D, unionSegmt);
			Assert.assertEquals(result, 235.886D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndIntFromUnionWithIntDouble() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndIntFromUnionWithIntDouble").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 0L, 1122334455);
			Assert.assertEquals((int)elemHandle1.get(unionSegmt, 0L), 1122334455);

			double result = (double)mh.invokeExact(113.567D, unionSegmt);
			Assert.assertEquals(result, 1122334568.567D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromUnionWithDoubleInt() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFromUnionWithDoubleInt").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 0L, 218.555D);
			Assert.assertEquals((double)elemHandle1.get(unionSegmt, 0L), 218.555D, 0.001D);

			double result = (double)mh.invokeExact(216.666D, unionSegmt);
			Assert.assertEquals(result, 435.221D, 0.001D);
		}
	}

	@Test
	public void test_addFloatAndFloatFromUnionWithFloatLong() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatFromUnionWithFloatLong").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 0L, 18.4F);
			Assert.assertEquals((float)elemHandle1.get(unionSegmt, 0L), 18.4F, 0.1F);

			float result = (float)mh.invokeExact(25.3F, unionSegmt);
			Assert.assertEquals(result, 43.7F, 0.1F);
		}
	}

	@Test
	public void test_addLongAndLongFromUnionWithLongFloat() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle elemHandle1 = unionLayout.varHandle(PathElement.groupElement("elem1"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongFromUnionWithLongFloat").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle1.set(unionSegmt, 0L, 100000000005L);
			Assert.assertEquals((long)elemHandle1.get(unionSegmt, 0L), 100000000005L);

			long result = (long)mh.invokeExact(1020302010L, unionSegmt);
			Assert.assertEquals(result, 101020302015L);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromUnionWithLongDouble() throws Throwable {
		UnionLayout unionLayout =MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFromUnionWithLongDouble").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, 222.333D);
			Assert.assertEquals((double)elemHandle2.get(unionSegmt, 0L), 222.333D, 0.001D);

			double result = (double)mh.invokeExact(111.222D, unionSegmt);
			Assert.assertEquals(result, 333.555D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndLongFromUnionWithDoubleLong() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndLongFromUnionWithDoubleLong").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, 11223344556677L);
			Assert.assertEquals((long)elemHandle2.get(unionSegmt, 0L), 11223344556677L);

			double result = (double)mh.invokeExact(222.33D, unionSegmt);
			Assert.assertEquals(result, 11223344556899.33D, 0.01D);
		}
	}

	@Test
	public void test_addDoubleAndFloatFromUnionWithIntFloatDouble() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_DOUBLE.withName("elem3"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatFromUnionWithIntFloatDouble").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, 18.444F);
			Assert.assertEquals((float)elemHandle2.get(unionSegmt, 0L), 18.444F, 0.001F);

			double result = (double)mh.invokeExact(113.567D, unionSegmt);
			Assert.assertEquals(result, 132.011D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatFromUnionWithIntDoubleFloat() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2"), JAVA_FLOAT.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatFromUnionWithIntDoubleFloat").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle3.set(unionSegmt, 0L, 18.444F);
			Assert.assertEquals((float)elemHandle3.get(unionSegmt, 0L), 18.444F, 0.001F);

			double result = (double)mh.invokeExact(113.567D, unionSegmt);
			Assert.assertEquals(result, 132.011D, 0.001D);
		}
	}

	@Test
	public void test_addLongAndLongFromUnionWithIntFloatLong() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_LONG.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongFromUnionWithIntFloatLong").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle3.set(unionSegmt, 0L, 100000000005L);
			Assert.assertEquals((long)elemHandle3.get(unionSegmt, 0L), 100000000005L);

			long result = (long)mh.invokeExact(1020302010L, unionSegmt);
			Assert.assertEquals(result, 101020302015L);
		}
	}

	@Test
	public void test_addLongAndLongFromUnionWithIntLongFloat() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_LONG.withName("elem2"), JAVA_FLOAT.withName("elem3"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongFromUnionWithIntLongFloat").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, 100000000005L);
			Assert.assertEquals((long)elemHandle2.get(unionSegmt, 0L), 100000000005L);

			long result = (long)mh.invokeExact(1020302010L, unionSegmt);
			Assert.assertEquals(result, 101020302015L);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromUnionWithFloatLongDouble() throws Throwable {
		UnionLayout unionLayout =MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_LONG.withName("elem2"), JAVA_DOUBLE.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFromUnionWithFloatLongDouble").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle3.set(unionSegmt, 0L, 100.11D);
			Assert.assertEquals((double)elemHandle3.get(unionSegmt, 0L), 100.11D, 01D);

			double result = (double)mh.invokeExact(222.44D, unionSegmt);
			Assert.assertEquals(result, 322.55D, 0.01D);
		}
	}

	@Test
	public void test_addDoubleAndLongFromUnionWithIntDoubleLong() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2"), JAVA_LONG.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndLongFromUnionWithIntDoubleLong").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle3.set(unionSegmt, 0L, 100000000001L);
			Assert.assertEquals((long)elemHandle3.get(unionSegmt, 0L), 100000000001L);

			double result = (double)mh.invokeExact(222.44D, unionSegmt);
			Assert.assertEquals(result, 100000000223.44D, 0.01D);
		}
	}

	@Test
	public void test_addDoubleAndPtrValueFromUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
		JAVA_DOUBLE.withName("elem2"), ADDRESS.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndPtrValueFromUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			MemorySegment longSegmt = arena.allocateFrom(JAVA_LONG, 112233445566778899L);
			elemHandle3.set(unionSegmt, 0L, longSegmt);
			MemorySegment elemSegmt = ((MemorySegment)elemHandle3.get(unionSegmt, 0L)).reinterpret(JAVA_LONG.byteSize());
			Assert.assertEquals(elemSegmt.get(JAVA_LONG, 0), 112233445566778899L);

			double result = (double)mh.invokeExact(222.44D, unionSegmt);
			Assert.assertEquals(result, 112233445566779121.44D, 0.01D);
		}
	}

	@Test
	public void test_addLongAndPtrValueFromUnion() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), ADDRESS.withName("elem3"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndPtrValueFromUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			MemorySegment longSegmt = arena.allocateFrom(JAVA_LONG, 112233445566778899L);
			elemHandle3.set(unionSegmt, 0L, longSegmt);
			MemorySegment elemSegmt = ((MemorySegment)elemHandle3.get(unionSegmt, 0L)).reinterpret(JAVA_LONG.byteSize());
			Assert.assertEquals(elemSegmt.get(JAVA_LONG, 0), 112233445566778899L);

			long result = (long)mh.invokeExact(1020302010L, unionSegmt);
			Assert.assertEquals(result, 112233446587080909L);
		}
	}

	@Test
	public void test_addLongAndIntFromUnionWithByteShortIntLong() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"),
				JAVA_SHORT.withName("elem2"), JAVA_INT.withName("elem3"), JAVA_LONG.withName("elem4"));
		VarHandle elemHandle3 = unionLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndIntFromUnionWithByteShortIntLong").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle3.set(unionSegmt, 0L, 11223344);
			Assert.assertEquals((int)elemHandle3.get(unionSegmt, 0L), 11223344);

			long result = (long)mh.invokeExact(1020302010L, unionSegmt);
			Assert.assertEquals(result, 1031525354L);
		}
	}

	@Test
	public void test_addLongAndIntFromUnionWithLongIntShortByte() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"),
				JAVA_INT.withName("elem2"), JAVA_SHORT.withName("elem3"), JAVA_BYTE.withName("elem4"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndIntFromUnionWithLongIntShortByte").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, 11223344);
			Assert.assertEquals((int)elemHandle2.get(unionSegmt, 0L), 11223344);

			long result = (long)mh.invokeExact(1020302010L, unionSegmt);
			Assert.assertEquals(result, 1031525354L);
		}
	}

	@Test
	public void test_addDoubleAndShortFromUnionWithByteShortFloatDouble() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"),
				JAVA_SHORT.withName("elem2"), JAVA_FLOAT.withName("elem3"), JAVA_DOUBLE.withName("elem4"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndShortFromUnionWithByteShortFloatDouble").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, (short)32766);
			Assert.assertEquals((short)elemHandle2.get(unionSegmt, 0L), 32766);

			double result = (double)mh.invokeExact(33333.57D, unionSegmt);
			Assert.assertEquals(result, 66099.57D, 0.01D);
		}
	}

	@Test
	public void test_addDoubleAndFloatFromUnionWithDoubleFloatShortByte() throws Throwable {
		UnionLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_SHORT.withName("elem3"), JAVA_BYTE.withName("elem4"));
		VarHandle elemHandle2 = unionLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatFromUnionWithDoubleFloatShortByte").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			elemHandle2.set(unionSegmt, 0L, 1.001F);
			Assert.assertEquals((float)elemHandle2.get(unionSegmt, 0L), 1.001F, 0.001F);

			double result = (double)mh.invokeExact(33333.567D, unionSegmt);
			Assert.assertEquals(result, 33334.568D, 0.001D);
		}
	}
}
