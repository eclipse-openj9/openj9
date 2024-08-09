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
package org.openj9.test.jep389.downcall;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;

import jdk.incubator.foreign.Addressable;
import jdk.incubator.foreign.CLinker;
import static jdk.incubator.foreign.CLinker.C_INT;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAccess;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.SequenceLayout;
import jdk.incubator.foreign.SymbolLookup;

/**
 * Test cases for JEP 389: Foreign Linker API for argument/return struct in downcall.
 *
 * Note:
 * The test suite is mainly intended for duplicate structs in arguments
 * and return type in downcall.
 */
@Test(groups = { "level.sanity" })
public class DuplicateMixedCallTests {
	private static MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
	private static CLinker clinker = CLinker.getInstance();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addIntAndIntsFromNestedStruct_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), C_INT.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt, 0, 21222324);
			MemoryAccess.setIntAtOffset(structSegmt, 4, 25262728);
			MemoryAccess.setIntAtOffset(structSegmt, 8, 29303132);

			int result = (int)mh.invokeExact(33343536, structSegmt);
			Assert.assertEquals(result, 109131720);
		}

		structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromNestedStruct_reverseOrder").get();
		mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt, 0, 21222324);
			MemoryAccess.setIntAtOffset(structSegmt, 4, 25262728);
			MemoryAccess.setIntAtOffset(structSegmt, 8, 29303132);

			int result = (int)mh.invokeExact(33343537, structSegmt);
			Assert.assertEquals(result, 109131721);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedStructArray_dupStruct() throws Throwable {
		GroupLayout intStruct = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, intStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt, 0, 1111111);
			MemoryAccess.setIntAtOffset(structSegmt, 4, 2222222);
			MemoryAccess.setIntAtOffset(structSegmt, 8, 3333333);
			MemoryAccess.setIntAtOffset(structSegmt, 12, 4444444);
			MemoryAccess.setIntAtOffset(structSegmt, 16, 5555555);

			int result = (int)mh.invokeExact(6666666, structSegmt);
			Assert.assertEquals(result, 23333331);
		}

		structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), structArray.withName("struct_array_elem2"));
		fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructWithNestedStructArray_reverseOrder").get();
		mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt, 0, 1111111);
			MemoryAccess.setIntAtOffset(structSegmt, 4, 2222222);
			MemoryAccess.setIntAtOffset(structSegmt, 8, 3333333);
			MemoryAccess.setIntAtOffset(structSegmt, 12, 4444444);
			MemoryAccess.setIntAtOffset(structSegmt, 16, 5555555);

			int result = (int)mh.invokeExact(6666667, structSegmt);
			Assert.assertEquals(result, 23333332);
		}
	}
}
