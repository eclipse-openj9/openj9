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
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SegmentAllocator;
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

/**
 * Test cases for JEP 442: Foreign Linker API (Third Preview) for argument/return struct/union
 * nested with struct/union in downcall.
 *
 * Note:
 * The test suite is mainly intended for the following Linker API:
 * MethodHandle downcallHandle(MemorySegment symbol, FunctionDescriptor function)
 */
@Test(groups = { "level.sanity", "disabled.os.zos" })
public class UnionStructTests {
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addBoolAndBoolsFromStructWithXor_Nested2BoolUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructWithXor_Nested2BoolUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, true);
			structSegmt.set(JAVA_BOOLEAN, 1, false);

			boolean result = (boolean)mh.invokeExact(false, structSegmt);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromUnionWithXor_Nested2BoolStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionWithXor_Nested2BoolStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, false);
			unionSegmt.set(JAVA_BOOLEAN, 1, true);

			boolean result = (boolean)mh.invokeExact(true, unionSegmt);
			Assert.assertEquals(result, false);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromUnionWithXor_Nested4BoolStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"),
		JAVA_BOOLEAN.withName("elem2"), JAVA_BOOLEAN.withName("elem3"), JAVA_BOOLEAN.withName("elem4"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromUnionWithXor_Nested4BoolStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BOOLEAN, 0, false);
			unionSegmt.set(JAVA_BOOLEAN, 1, true);
			unionSegmt.set(JAVA_BOOLEAN, 2, true);
			unionSegmt.set(JAVA_BOOLEAN, 3, true);

			boolean result = (boolean)mh.invokeExact(true, unionSegmt);
			Assert.assertEquals(result, false);
		}
	}

	@Test
	public void test_add2BoolStructsWithXor_returnStruct_Nested2BoolUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolStructsWithXor_returnStruct_Nested2BoolUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_BOOLEAN, 0, true);
			structSegmt1.set(JAVA_BOOLEAN, 1, false);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_BOOLEAN, 0, true);
			structSegmt2.set(JAVA_BOOLEAN, 1, true);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), false);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 1), true);
		}
	}

	@Test
	public void test_add2BoolUnionsWithXor_returnUnion_Nested2BoolStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_BOOLEAN.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2BoolUnionsWithXor_returnUnion_Nested2BoolStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			unionSegmt1.set(JAVA_BOOLEAN, 0, false);
			unionSegmt1.set(JAVA_BOOLEAN, 1, true);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			unionSegmt2.set(JAVA_BOOLEAN, 0, true);
			unionSegmt2.set(JAVA_BOOLEAN, 1, false);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 0), true);
			Assert.assertEquals(resultSegmt.get(JAVA_BOOLEAN, 1), true);
		}
	}

	@Test
	public void test_addByteAndBytesFromStruct_Nested2ByteUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStruct_Nested2ByteUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_BYTE, 0, (byte)8);
			structSegmt.set(JAVA_BYTE, 1, (byte)9);

			byte result = (byte)mh.invokeExact((byte)6, structSegmt);
			Assert.assertEquals(result, 32);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnion_Nested2ByteStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnion_Nested2ByteStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)8);
			unionSegmt.set(JAVA_BYTE, 1, (byte)9);

			byte result = (byte)mh.invokeExact((byte)6, unionSegmt);
			Assert.assertEquals(result, 31);
		}
	}

	@Test
	public void test_addByteAndBytesFromUnion_Nested4ByteStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"),
				JAVA_BYTE.withName("elem2"), JAVA_BYTE.withName("elem3"), JAVA_BYTE.withName("elem4"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromUnion_Nested4ByteStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)8);
			unionSegmt.set(JAVA_BYTE, 1, (byte)9);
			unionSegmt.set(JAVA_BYTE, 2, (byte)10);
			unionSegmt.set(JAVA_BYTE, 3, (byte)11);

			byte result = (byte)mh.invokeExact((byte)6, unionSegmt);
			Assert.assertEquals(result, 52);
		}
	}

	@Test
	public void test_add2ByteStructs_returnStruct_Nested2ByteUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ByteStructs_returnStruct_Nested2ByteUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_BYTE, 0, (byte)25);
			structSegmt1.set(JAVA_BYTE, 1, (byte)11);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_BYTE, 0, (byte)24);
			structSegmt2.set(JAVA_BYTE, 1, (byte)13);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_BYTE, 0), (byte)49);
			Assert.assertEquals(resultSegmt.get(JAVA_BYTE, 1), (byte)24);
		}
	}

	@Test
	public void test_add2ByteUnions_returnUnion_Nested2ByteStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_BYTE.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ByteUnions_returnUnion_Nested2ByteStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			unionSegmt1.set(JAVA_BYTE, 0, (byte)25);
			unionSegmt1.set(JAVA_BYTE, 1, (byte)11);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			unionSegmt2.set(JAVA_BYTE, 0, (byte)24);
			unionSegmt2.set(JAVA_BYTE, 1, (byte)13);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_BYTE, 0), (byte)49);
			Assert.assertEquals(resultSegmt.get(JAVA_BYTE, 1), (byte)24);
		}
	}

	@Test
	public void test_addCharAndCharsFromStruct_Nested2CharUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStruct_Nested2CharUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_CHAR, 0, 'A');
			structSegmt.set(JAVA_CHAR, 2, 'B');

			char result = (char)mh.invokeExact('C', structSegmt);
			Assert.assertEquals(result, 'E');
		}
	}

	@Test
	public void test_addCharAndCharsFromUnion_Nested2CharStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnion_Nested2CharStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'A');
			unionSegmt.set(JAVA_CHAR, 2, 'B');

			char result = (char)mh.invokeExact('C', unionSegmt);
			Assert.assertEquals(result, 'D');
		}
	}

	@Test
	public void test_addCharAndCharsFromUnion_Nested4CharStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"),
		JAVA_CHAR.withName("elem2"), JAVA_CHAR.withName("elem3"), JAVA_CHAR.withName("elem4"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromUnion_Nested4CharStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_CHAR, 0, 'A');
			unionSegmt.set(JAVA_CHAR, 2, 'B');
			unionSegmt.set(JAVA_CHAR, 4, 'C');
			unionSegmt.set(JAVA_CHAR, 6, 'D');

			char result = (char)mh.invokeExact('E', unionSegmt);
			Assert.assertEquals(result, 'K');
		}
	}

	@Test
	public void test_add2CharStructs_returnStruct_Nested2CharUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2CharStructs_returnStruct_Nested2CharUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_CHAR, 0, 'A');
			structSegmt1.set(JAVA_CHAR, 2, 'B');
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_CHAR, 0, 'C');
			structSegmt2.set(JAVA_CHAR, 2, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'C');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 2), 'E');
		}
	}

	@Test
	public void test_add2CharUnions_returnUnion_Nested2CharStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_CHAR.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2CharUnions_returnUnion_Nested2CharStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			unionSegmt1.set(JAVA_CHAR, 0, 'A');
			unionSegmt1.set(JAVA_CHAR, 2, 'B');
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			unionSegmt2.set(JAVA_CHAR, 0, 'C');
			unionSegmt2.set(JAVA_CHAR, 2, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 0), 'C');
			Assert.assertEquals(resultSegmt.get(JAVA_CHAR, 2), 'E');
		}
	}

	public void test_addShortAndShortsFromStruct_Nested2ShortUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStruct_Nested2ShortUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)8);
			structSegmt.set(JAVA_SHORT, 2, (short)9);

			short result = (short)mh.invokeExact((short)6, structSegmt);
			Assert.assertEquals(result, 32);
		}
	}

	@Test
	public void test_addShortAndShortsFromUnion_Nested2ShortStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnion_Nested2ShortStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)8);
			unionSegmt.set(JAVA_SHORT, 2, (short)9);

			short result = (short)mh.invokeExact((short)6, unionSegmt);
			Assert.assertEquals(result, 31);
		}
	}

	@Test
	public void test_addShortAndShortsFromUnion_Nested4ShortStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"),
		JAVA_SHORT.withName("elem2"), JAVA_SHORT.withName("elem3"), JAVA_SHORT.withName("elem4"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromUnion_Nested4ShortStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)1111);
			unionSegmt.set(JAVA_SHORT, 2, (short)2222);
			unionSegmt.set(JAVA_SHORT, 4, (short)3333);
			unionSegmt.set(JAVA_SHORT, 6, (short)4444);

			short result = (short)mh.invokeExact((short)5555, unionSegmt);
			Assert.assertEquals(result, 17776);
		}
	}

	@Test
	public void test_add2ShortStructs_returnStruct_Nested2ShortUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ShortStructs_returnStruct_Nested2ShortUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_SHORT, 0, (short)55);
			structSegmt1.set(JAVA_SHORT, 2, (short)45);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_SHORT, 0, (short)78);
			structSegmt2.set(JAVA_SHORT, 2, (short)67);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 133);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 2), 112);
		}
	}

	@Test
	public void test_add2ShortUnions_returnUnion_Nested2ShortStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2ShortUnions_returnUnion_Nested2ShortStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			unionSegmt1.set(JAVA_SHORT, 0, (short)55);
			unionSegmt1.set(JAVA_SHORT, 2, (short)45);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			unionSegmt2.set(JAVA_SHORT, 0, (short)78);
			unionSegmt2.set(JAVA_SHORT, 2, (short)67);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 0), 133);
			Assert.assertEquals(resultSegmt.get(JAVA_SHORT, 2), 112);
		}
	}

	@Test
	public void test_addIntAndIntsFromStruct_Nested2IntUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStruct_Nested2IntUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 1122334);
			structSegmt.set(JAVA_INT, 4, 1234567);

			int result = (int)mh.invokeExact(2244668, structSegmt);
			Assert.assertEquals(result, 5836136);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnion_Nested2IntStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnion_Nested2IntStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 112233445);
			unionSegmt.set(JAVA_INT, 4, 1234567890);

			int result = (int)mh.invokeExact(2244668, unionSegmt);
			Assert.assertEquals(result, 1461279448);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnion_Nested4IntStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
		JAVA_INT.withName("elem2"), JAVA_INT.withName("elem3"), JAVA_INT.withName("elem4"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnion_Nested4IntStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 11223344);
			unionSegmt.set(JAVA_INT, 4, 55667788);
			unionSegmt.set(JAVA_INT, 8, 99001122);
			unionSegmt.set(JAVA_INT, 12, 33445566);

			int result = (int)mh.invokeExact(22446688, unionSegmt);
			Assert.assertEquals(result, 233007852);
		}
	}

	@Test
	public void test_add2IntStructs_returnStruct_Nested2IntUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStruct_Nested2IntUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_INT, 0, 11223344);
			structSegmt1.set(JAVA_INT, 4, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_INT, 0, 99001122);
			structSegmt2.set(JAVA_INT, 4, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 110224466);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 4), 89113354);
		}
	}

	@Test
	public void test_add2IntUnions_returnUnion_Nested2IntStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntUnions_returnUnion_Nested2IntStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			unionSegmt1.set(JAVA_INT, 0, 11223344);
			unionSegmt1.set(JAVA_INT, 4, 55667788);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			unionSegmt2.set(JAVA_INT, 0, 99001122);
			unionSegmt2.set(JAVA_INT, 4, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 110224466);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 4), 89113354);
		}
	}

	@Test
	public void test_addLongAndLongsFromStruct_Nested2LongUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStruct_Nested2LongUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_LONG, 0, 1234567890L);
			structSegmt.set(JAVA_LONG, 8, 9876543210L);

			long result = (long)mh.invokeExact(2468024680L, structSegmt);
			Assert.assertEquals(result, 23455678990L);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnion_Nested2LongStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnion_Nested2LongStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 1111111L);
			unionSegmt.set(JAVA_LONG, 8, 2222222L);

			long result = (long)mh.invokeExact(3333333L, unionSegmt);
			Assert.assertEquals(result, 7777777L);
		}
	}

	@Test
	public void test_addLongAndLongsFromUnion_Nested4LongStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"),
		JAVA_LONG.withName("elem2"), JAVA_LONG.withName("elem3"), JAVA_LONG.withName("elem4"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromUnion_Nested4LongStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_LONG, 0, 1111111L);
			unionSegmt.set(JAVA_LONG, 8, 2222222L);
			unionSegmt.set(JAVA_LONG, 16, 3333333L);
			unionSegmt.set(JAVA_LONG, 24, 4444444L);

			long result = (long)mh.invokeExact(5555555L, unionSegmt);
			Assert.assertEquals(result, 17777776L);
		}
	}

	@Test
	public void test_add2LongStructs_returnStruct_Nested2LongUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2LongStructs_returnStruct_Nested2LongUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_LONG, 0, 987654321987L);
			structSegmt1.set(JAVA_LONG, 8, 123456789123L);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_LONG, 0, 224466880022L);
			structSegmt2.set(JAVA_LONG, 8, 113355779911L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0), 1212121202009L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 8), 236812569034L);
		}
	}

	@Test
	public void test_add2LongUnions_returnUnion_Nested2LongStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_LONG.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2LongUnions_returnUnion_Nested2LongStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			unionSegmt1.set(JAVA_LONG, 0, 987654321987L);
			unionSegmt1.set(JAVA_LONG, 8, 123456789123L);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			unionSegmt2.set(JAVA_LONG, 0, 224466880022L);
			unionSegmt2.set(JAVA_LONG, 8, 113355779911L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 0), 1212121202009L);
			Assert.assertEquals(resultSegmt.get(JAVA_LONG, 8), 236812569034L);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStruct_Nested2FloatUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStruct_Nested2FloatUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_FLOAT, 0, 8.12F);
			structSegmt.set(JAVA_FLOAT, 4, 9.24F);

			float result = (float)mh.invokeExact(6.56F, structSegmt);
			Assert.assertEquals(result, 33.16F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnion_Nested2FloatStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnion_Nested2FloatStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 8.12F);
			unionSegmt.set(JAVA_FLOAT, 4, 9.24F);

			float result = (float)mh.invokeExact(6.56F, unionSegmt);
			Assert.assertEquals(result, 32.04F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromUnion_Nested4FloatStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_FLOAT.withName("elem3"), JAVA_FLOAT.withName("elem4"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromUnion_Nested4FloatStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_FLOAT, 0, 8.12F);
			unionSegmt.set(JAVA_FLOAT, 4, 9.24F);
			unionSegmt.set(JAVA_FLOAT, 8, 10.48F);
			unionSegmt.set(JAVA_FLOAT, 12, 11.12F);

			float result = (float)mh.invokeExact(6.56F, unionSegmt);
			Assert.assertEquals(result, 53.64F, 0.01F);
		}
	}

	@Test
	public void test_add2FloatStructs_returnStruct_Nested2FloatUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2FloatStructs_returnStruct_Nested2FloatUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_FLOAT, 0, 25.12F);
			structSegmt1.set(JAVA_FLOAT, 4, 11.23F);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_FLOAT, 0, 24.34F);
			structSegmt2.set(JAVA_FLOAT, 4, 11.25F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 49.46F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 4), 22.48F, 0.01F);
		}
	}

	@Test
	public void test_add2FloatUnions_returnUnion_Nested2FloatStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_FLOAT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2FloatUnions_returnUnion_Nested2FloatStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			unionSegmt1.set(JAVA_FLOAT, 0, 25.12F);
			unionSegmt1.set(JAVA_FLOAT, 4, 11.23F);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			unionSegmt2.set(JAVA_FLOAT, 0, 24.34F);
			unionSegmt2.set(JAVA_FLOAT, 4, 13.45F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 0), 49.46F, 0.01F);
			Assert.assertEquals(resultSegmt.get(JAVA_FLOAT, 4), 24.68F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStruct_Nested2DoubleUnion() throws Throwable {
		GroupLayout nestedUnionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStruct_Nested2DoubleUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_DOUBLE, 0, 2228.111D);
			structSegmt.set(JAVA_DOUBLE, 8, 2229.221D);

			double result = (double)mh.invokeExact(3336.333D, structSegmt);
			Assert.assertEquals(result, 10022.886D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnion_Nested2DoubleStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnion_Nested2DoubleStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 2228.111D);
			unionSegmt.set(JAVA_DOUBLE, 8, 2229.221D);

			double result = (double)mh.invokeExact(3336.333D, unionSegmt);
			Assert.assertEquals(result, 10021.776D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromUnion_Nested4DoubleStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"),
		JAVA_DOUBLE.withName("elem2"), JAVA_DOUBLE.withName("elem3"), JAVA_DOUBLE.withName("elem4"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromUnion_Nested4DoubleStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_DOUBLE, 0, 2228.111D);
			unionSegmt.set(JAVA_DOUBLE, 8, 2229.221D);
			unionSegmt.set(JAVA_DOUBLE, 16, 2230.221D);
			unionSegmt.set(JAVA_DOUBLE, 24, 2231.221D);

			double result = (double)mh.invokeExact(3336.333D, unionSegmt);
			Assert.assertEquals(result, 14483.218D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleStructs_returnStruct_Nested2DoubleUnion() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2DoubleStructs_returnStruct_Nested2DoubleUnion").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			structSegmt1.set(JAVA_DOUBLE, 0, 11.222D);
			structSegmt1.set(JAVA_DOUBLE, 8, 22.333D);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			structSegmt2.set(JAVA_DOUBLE, 0, 33.444D);
			structSegmt2.set(JAVA_DOUBLE, 8, 44.555D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, structSegmt1, structSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0), 44.666D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 8), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleUnions_returnUnion_Nested2DoubleStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_DOUBLE.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("add2DoubleUnions_returnUnion_Nested2DoubleStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt1 = arena.allocate(unionLayout);
			unionSegmt1.set(JAVA_DOUBLE, 0, 11.222D);
			unionSegmt1.set(JAVA_DOUBLE, 8, 22.333D);
			MemorySegment unionSegmt2 = arena.allocate(unionLayout);
			unionSegmt2.set(JAVA_DOUBLE, 0, 33.444D);
			unionSegmt2.set(JAVA_DOUBLE, 8, 44.555D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact((SegmentAllocator)arena, unionSegmt1, unionSegmt2);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 0), 44.666D, 0.001D);
			Assert.assertEquals(resultSegmt.get(JAVA_DOUBLE, 8), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_addShortAndShortByteFromUnion_Nested2ByteStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortByteFromUnion_Nested2ByteStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)12345);

			short result = (short)mh.invokeExact((short)11111, unionSegmt);
			Assert.assertEquals(result, 23561);
		}
	}

	@Test
	public void test_addShortAndBytesFromUnion_Nested4ByteStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"),
		JAVA_BYTE.withName("elem3"), JAVA_BYTE.withName("elem4"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_SHORT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndBytesFromUnion_Nested4ByteStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_BYTE, 0, (byte)11);
			unionSegmt.set(JAVA_BYTE, 1, (byte)22);
			unionSegmt.set(JAVA_BYTE, 2, (byte)33);
			unionSegmt.set(JAVA_BYTE, 3, (byte)44);

			short result = (short)mh.invokeExact((short)11111, unionSegmt);
			Assert.assertEquals(result, 11221);
		}
	}

	@Test
	public void test_addIntAndIntShortFromUnion_Nested2ShortStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntShortFromUnion_Nested2ShortStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 1234567890);

			int result = (int)mh.invokeExact(22446688, unionSegmt);
			Assert.assertEquals(result, 1257034138);
		}
	}

	@Test
	public void test_addIntAndShortsFromUnion_Nested4ShortStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"),
		JAVA_SHORT.withName("elem3"), JAVA_SHORT.withName("elem4"));
		GroupLayout unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndShortsFromUnion_Nested4ShortStruct").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_SHORT, 0, (short)11111);
			unionSegmt.set(JAVA_SHORT, 2, (short)11112);
			unionSegmt.set(JAVA_SHORT, 4, (short)11113);
			unionSegmt.set(JAVA_SHORT, 6, (short)11114);

			int result = (int)mh.invokeExact(22446688, unionSegmt);
			Assert.assertEquals(result, 22491138);
		}
	}
}
