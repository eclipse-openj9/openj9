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
package org.openj9.test.jep454.downcall;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.GroupLayout;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SegmentAllocator;
import java.lang.foreign.SequenceLayout;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.UnionLayout;
import static java.lang.foreign.ValueLayout.JAVA_INT;
import java.lang.invoke.MethodHandle;

/**
 * Test cases for JEP 454: Foreign Linker API for argument/return struct/union in downcall.
 *
 * Note:
 * The test suite is mainly intended for duplicate structs/union in arguments
 * and return type in downcall.
 */
@Test(groups = { "level.sanity" })
public class DuplicateMixedCallTests {
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addIntAndIntsFromNestedStruct_dupStruct() throws Throwable {
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

		structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		functionSymbol = nativeLibLookup.find("addIntAndIntsFromNestedStruct_reverseOrder").get();
		mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 21222324);
			structSegmt.set(JAVA_INT, 4, 25262728);
			structSegmt.set(JAVA_INT, 8, 29303132);

			int result = (int)mh.invokeExact(33343537, structSegmt);
			Assert.assertEquals(result, 109131721);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedStructArray_dupStruct() throws Throwable {
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

		structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), structArray.withName("struct_array_elem2"));
		fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout);
		functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructWithNestedStructArray_reverseOrder").get();
		mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_INT, 0, 1111111);
			structSegmt.set(JAVA_INT, 4, 2222222);
			structSegmt.set(JAVA_INT, 8, 3333333);
			structSegmt.set(JAVA_INT, 12, 4444444);
			structSegmt.set(JAVA_INT, 16, 5555555);

			int result = (int)mh.invokeExact(6666667, structSegmt);
			Assert.assertEquals(result, 23333332);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedUnion_dupUnion() throws Throwable {
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

		unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), nestedUnionLayout.withName("union_elem2"));
		fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		functionSymbol = nativeLibLookup.find("addIntAndIntsFromNestedUnion_reverseOrder").get();
		mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 21222324);

			int result = (int)mh.invokeExact(33343537, unionSegmt);
			Assert.assertEquals(result, 97010509);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionWithNestedIntArray_dupUnion() throws Throwable {
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

		unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), intArray.withName("array_elem2"));
		fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionWithNestedIntArray_reverseOrder").get();
		mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 1111111);
			unionSegmt.set(JAVA_INT, 4, 2222222);

			int result = (int)mh.invokeExact(4444445, unionSegmt);
			Assert.assertEquals(result, 8888889);
		}
	}

	@Test
	public void test_addIntAndIntsFromUnionWithNestedUnionArray_dupUnion() throws Throwable {
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

		unionLayout = MemoryLayout.unionLayout(JAVA_INT.withName("elem1"), unionArray.withName("union_array_elem2"));
		fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, unionLayout);
		functionSymbol = nativeLibLookup.find("addIntAndIntsFromUnionWithNestedUnionArray_reverseOrder").get();
		mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment unionSegmt = arena.allocate(unionLayout);
			unionSegmt.set(JAVA_INT, 0, 2222222);
			unionSegmt.set(JAVA_INT, 4, 3333333);

			int result = (int)mh.invokeExact(6666667, unionSegmt);
			Assert.assertEquals(result, 19999999);
		}
	}

	@Test
	public void test_addIntsFromStructs_NestedUnion_dupStruct() throws Throwable {
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

		fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		functionSymbol = nativeLibLookup.find("add2IntStructs_returnStruct_Nested2IntUnion").get();
		mh = linker.downcallHandle(functionSymbol, fd);

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
	public void test_addIntsFromUnions_NestedStruct_dupUnion() throws Throwable {
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

		fd = FunctionDescriptor.of(unionLayout, unionLayout, unionLayout);
		functionSymbol = nativeLibLookup.find("add2IntUnions_returnUnion_Nested2IntStruct").get();
		mh = linker.downcallHandle(functionSymbol, fd);

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
}
