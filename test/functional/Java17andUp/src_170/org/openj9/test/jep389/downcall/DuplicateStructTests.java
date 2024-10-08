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
import java.lang.invoke.VarHandle;

import jdk.incubator.foreign.Addressable;
import jdk.incubator.foreign.CLinker;
import static jdk.incubator.foreign.CLinker.C_CHAR;
import static jdk.incubator.foreign.CLinker.C_DOUBLE;
import static jdk.incubator.foreign.CLinker.C_FLOAT;
import static jdk.incubator.foreign.CLinker.C_INT;
import static jdk.incubator.foreign.CLinker.C_LONG;
import static jdk.incubator.foreign.CLinker.C_LONG_LONG;
import static jdk.incubator.foreign.CLinker.C_SHORT;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAccess;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemoryLayout.PathElement;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.SequenceLayout;
import jdk.incubator.foreign.SymbolLookup;
import jdk.incubator.foreign.ValueLayout;

/**
 * Test cases for JEP 389: Foreign Linker API for argument/return struct in downcall.
 *
 * Note:
 * The test suite is mainly intended for duplicate structs/nested struct in arguments
 * and return type in downcall.
 */
@Test(groups = { "level.sanity" })
public class DuplicateStructTests {
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinOS = osName.contains("win");
	/* long long is 64 bits on AIX/ppc64, which is the same as Windows. */
	private static ValueLayout longLayout = (isWinOS || isAixOS) ? C_LONG_LONG : C_LONG;
	private static MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
	private static CLinker clinker = CLinker.getInstance();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addBoolStructsWithXor_dupStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolStructsWithXor_returnStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt1, (byte)1);
			boolHandle2.set(structSegmt1, (byte)0);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt2, (byte)1);
			boolHandle2.set(structSegmt2, (byte)1);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)boolHandle1.get(resultSegmt), (byte)0);
			Assert.assertEquals((byte)boolHandle2.get(resultSegmt), (byte)1);
		}
	}

	@Test
	public void test_addNestedBoolStructsWithXor_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedBoolStructsWithXor_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt1, 0, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt1, 1, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt1, 2, (byte)1);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)1);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 0), (byte)1);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 1), (byte)1);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 2), (byte)0);
		}
	}

	@Test
	public void test_addBoolStruct1AndNestedBoolStruct2WithXor_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout1.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout1.varHandle(byte.class, PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolStruct1AndNestedBoolStruct2WithXor_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			boolHandle1.set(structSegmt1, (byte)1);
			boolHandle2.set(structSegmt1, (byte)0);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)1);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)boolHandle1.get(resultSegmt), (byte)0);
			Assert.assertEquals((byte)boolHandle2.get(resultSegmt), (byte)1);
		}
	}

	@Test
	public void test_addBoolStruct1AndNestedBoolStruct2WithXor_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout1.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout1.varHandle(byte.class, PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolStruct1AndNestedBoolStruct2WithXor_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			boolHandle1.set(structSegmt1, (byte)1);
			boolHandle2.set(structSegmt1, (byte)0);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)1);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 0), (byte)0);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 1), (byte)0);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 2), (byte)1);
		}
	}

	@Test
	public void test_addNestedBoolArrayStructsWithXor_dupStruct() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, C_CHAR);
		GroupLayout structLayout = MemoryLayout.structLayout(boolArray.withName("array_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedBoolArrayStructsWithXor_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt1, 0, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt1, 1, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt1, 2, (byte)1);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)1);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 0), (byte)1);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 1), (byte)1);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 2), (byte)0);
		}
	}

	@Test
	public void test_addBoolStruct1AndNestedBoolArrayStruct2WithXor_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout1.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout1.varHandle(byte.class, PathElement.groupElement("elem2"));

		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, C_CHAR);
		GroupLayout structLayout2 = MemoryLayout.structLayout(boolArray.withName("array_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolStruct1AndNestedBoolArrayStruct2WithXor_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			boolHandle1.set(structSegmt1, (byte)1);
			boolHandle2.set(structSegmt1, (byte)0);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)0);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)boolHandle1.get(resultSegmt), (byte)1);
			Assert.assertEquals((byte)boolHandle2.get(resultSegmt), (byte)1);
		}
	}

	@Test
	public void test_addBoolStruct1AndNestedBoolArrayStruct2WithXor_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout1.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout1.varHandle(byte.class, PathElement.groupElement("elem2"));

		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, C_CHAR);
		GroupLayout structLayout2 = MemoryLayout.structLayout(boolArray.withName("array_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolStruct1AndNestedBoolArrayStruct2WithXor_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			boolHandle1.set(structSegmt1, (byte)1);
			boolHandle2.set(structSegmt1, (byte)0);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)0);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 0), (byte)1);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 1), (byte)1);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 2), (byte)0);
		}
	}

	@Test
	public void test_addNestedBoolStructArrayStructsWithXor_dupStruct() throws Throwable {
		GroupLayout boolStruct = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, boolStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedBoolStructArrayStructsWithXor_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt1, 0, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt1, 1, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt1, 2, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt1, 3, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt1, 4, (byte)0);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt2, 3, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 4, (byte)0);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 0), (byte)1);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 1), (byte)1);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 2), (byte)1);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 3), (byte)1);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 4), (byte)0);
		}
	}

	@Test
	public void test_addBoolStruct1AndNestedBoolStructArrayStruct2WithXor_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout1.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout1.varHandle(byte.class, PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolStruct1AndNestedBoolStructArrayStruct2WithXor_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			boolHandle1.set(structSegmt1, (byte)1);
			boolHandle2.set(structSegmt1, (byte)0);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt2, 3, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 4, (byte)1);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)boolHandle1.get(resultSegmt), (byte)1);
			Assert.assertEquals((byte)boolHandle2.get(resultSegmt), (byte)1);
		}
	}

	@Test
	public void test_addBoolStruct1AndNestedBoolStructArrayStruct2WithXor_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout1.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout1.varHandle(byte.class, PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolStruct1AndNestedBoolStructArrayStruct2WithXor_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			boolHandle1.set(structSegmt1, (byte)1);
			boolHandle2.set(structSegmt1, (byte)0);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 3, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt2, 4, (byte)1);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 0), (byte)0);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 1), (byte)0);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 2), (byte)1);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 3), (byte)0);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 4), (byte)1);
		}
	}

	@Test
	public void test_addByteStructs_dupStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2ByteStructs_returnStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)24);
			byteHandle2.set(structSegmt2, (byte)13);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)24);
		}
	}

	@Test
	public void test_addNestedByteStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedByteStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt1, 0, (byte)11);
			MemoryAccess.setByteAtOffset(structSegmt1, 1, (byte)22);
			MemoryAccess.setByteAtOffset(structSegmt1, 2, (byte)33);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)12);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)23);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)34);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 0), (byte)23);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 1), (byte)45);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 2), (byte)67);
		}
	}

	@Test
	public void test_addByteStruct1AndNestedByteStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout1.varHandle(byte.class ,PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout1.varHandle(byte.class ,PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteStruct1AndNestedByteStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)12);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)23);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)34);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)37);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)68);
		}
	}

	@Test
	public void test_addByteStruct1AndNestedByteStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout1.varHandle(byte.class ,PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout1.varHandle(byte.class ,PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteStruct1AndNestedByteStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)12);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)23);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)34);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 0), (byte)37);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 1), (byte)34);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 2), (byte)45);
		}
	}

	@Test
	public void test_addNestedByteArrayStructs_dupStruct() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, C_CHAR);
		GroupLayout structLayout = MemoryLayout.structLayout(byteArray.withName("array_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedByteArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt1, 0, (byte)11);
			MemoryAccess.setByteAtOffset(structSegmt1, 1, (byte)22);
			MemoryAccess.setByteAtOffset(structSegmt1, 2, (byte)33);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)12);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)23);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)34);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 0), (byte)23);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 1), (byte)45);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 2), (byte)67);
		}
	}

	@Test
	public void test_addByteStruct1AndNestedByteArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout1.varHandle(byte.class ,PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout1.varHandle(byte.class ,PathElement.groupElement("elem2"));

		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, C_CHAR);
		GroupLayout structLayout2 = MemoryLayout.structLayout(byteArray.withName("array_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteStruct1AndNestedByteArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)11);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)12);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)14);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)36);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)37);
		}
	}

	@Test
	public void test_addByteStruct1AndNestedByteArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout1.varHandle(byte.class ,PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout1.varHandle(byte.class ,PathElement.groupElement("elem2"));

		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, C_CHAR);
		GroupLayout structLayout2 = MemoryLayout.structLayout(byteArray.withName("array_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteStruct1AndNestedByteArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)11);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)12);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)14);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 0), (byte)36);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 1), (byte)23);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 2), (byte)25);
		}
	}

	@Test
	public void test_addNestedByteStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout byteStruct = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, byteStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedByteStructArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt1, 0, (byte)11);
			MemoryAccess.setByteAtOffset(structSegmt1, 1, (byte)12);
			MemoryAccess.setByteAtOffset(structSegmt1, 2, (byte)13);
			MemoryAccess.setByteAtOffset(structSegmt1, 3, (byte)14);
			MemoryAccess.setByteAtOffset(structSegmt1, 4, (byte)15);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)21);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)23);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)25);
			MemoryAccess.setByteAtOffset(structSegmt2, 3, (byte)27);
			MemoryAccess.setByteAtOffset(structSegmt2, 4, (byte)29);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 0), (byte)32);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 1), (byte)35);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 2), (byte)38);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 3), (byte)41);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 4), (byte)44);
		}
	}

	@Test
	public void test_addByteStruct1AndNestedByteStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout1.varHandle(byte.class ,PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout1.varHandle(byte.class ,PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteStruct1AndNestedByteStructArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)21);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)23);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)25);
			MemoryAccess.setByteAtOffset(structSegmt2, 3, (byte)27);
			MemoryAccess.setByteAtOffset(structSegmt2, 4, (byte)29);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)71);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)90);
		}
	}

	@Test
	public void test_addByteStruct1AndNestedByteStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout1.varHandle(byte.class ,PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout1.varHandle(byte.class ,PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_CHAR.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteStruct1AndNestedByteStructArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setByteAtOffset(structSegmt2, 0, (byte)21);
			MemoryAccess.setByteAtOffset(structSegmt2, 1, (byte)23);
			MemoryAccess.setByteAtOffset(structSegmt2, 2, (byte)25);
			MemoryAccess.setByteAtOffset(structSegmt2, 3, (byte)27);
			MemoryAccess.setByteAtOffset(structSegmt2, 4, (byte)29);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 0), (byte)46);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 1), (byte)34);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 2), (byte)50);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 3), (byte)38);
			Assert.assertEquals((byte)MemoryAccess.getByteAtOffset(resultSegmt, 4), (byte)40);
		}
	}

	@Test
	public void test_addCharStructs_dupStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2CharStructs_returnStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt2, 'C');
			charHandle2.set(structSegmt2, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'C');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'E');
		}
	}

	@Test
	public void test_addNestedCharStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedCharStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt1, 0, 'A');
			MemoryAccess.setCharAtOffset(structSegmt1, 2, 'B');
			MemoryAccess.setCharAtOffset(structSegmt1, 4, 'C');
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt2, 0, 'E');
			MemoryAccess.setCharAtOffset(structSegmt2, 2, 'F');
			MemoryAccess.setCharAtOffset(structSegmt2, 4, 'G');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 0), 'E');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 2), 'G');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 4), 'I');
		}
	}

	@Test
	public void test_addCharStruct1AndNestedCharStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout1.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout1.varHandle(char.class, PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharStruct1AndNestedCharStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setCharAtOffset(structSegmt2, 0, 'E');
			MemoryAccess.setCharAtOffset(structSegmt2, 2, 'F');
			MemoryAccess.setCharAtOffset(structSegmt2, 4, 'G');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'E');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'M');
		}
	}

	@Test
	public void test_addCharStruct1AndNestedCharStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout1.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout1.varHandle(char.class, PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharStruct1AndNestedCharStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setCharAtOffset(structSegmt2, 0, 'E');
			MemoryAccess.setCharAtOffset(structSegmt2, 2, 'F');
			MemoryAccess.setCharAtOffset(structSegmt2, 4, 'G');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 0), 'E');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 2), 'G');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 4), 'H');
		}
	}

	@Test
	public void test_addNestedCharArrayStructs_dupStruct() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, C_SHORT);
		GroupLayout structLayout = MemoryLayout.structLayout(charArray.withName("array_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedCharArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt1, 0, 'A');
			MemoryAccess.setCharAtOffset(structSegmt1, 2, 'B');
			MemoryAccess.setCharAtOffset(structSegmt1, 4, 'C');
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt2, 0, 'E');
			MemoryAccess.setCharAtOffset(structSegmt2, 2, 'F');
			MemoryAccess.setCharAtOffset(structSegmt2, 4, 'G');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 0), 'E');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 2), 'G');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 4), 'I');
		}
	}

	@Test
	public void test_addCharStruct1AndNestedCharArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout1.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout1.varHandle(char.class, PathElement.groupElement("elem2"));

		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, C_SHORT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(charArray.withName("array_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharStruct1AndNestedCharArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setCharAtOffset(structSegmt2, 0, 'E');
			MemoryAccess.setCharAtOffset(structSegmt2, 2, 'F');
			MemoryAccess.setCharAtOffset(structSegmt2, 4, 'G');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'E');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'M');
		}
	}

	@Test
	public void test_addCharStruct1AndNestedCharArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout1.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout1.varHandle(char.class, PathElement.groupElement("elem2"));

		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, C_SHORT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(charArray.withName("array_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharStruct1AndNestedCharArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setCharAtOffset(structSegmt2, 0, 'E');
			MemoryAccess.setCharAtOffset(structSegmt2, 2, 'F');
			MemoryAccess.setCharAtOffset(structSegmt2, 4, 'G');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 0), 'E');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 2), 'G');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 4), 'H');
		}
	}

	@Test
	public void test_addNestedCharStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout charStruct = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, charStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedCharStructArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt1, 0, 'A');
			MemoryAccess.setCharAtOffset(structSegmt1, 2, 'B');
			MemoryAccess.setCharAtOffset(structSegmt1, 4, 'C');
			MemoryAccess.setCharAtOffset(structSegmt1, 6, 'D');
			MemoryAccess.setCharAtOffset(structSegmt1, 8, 'E');
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt2, 0, 'F');
			MemoryAccess.setCharAtOffset(structSegmt2, 2, 'G');
			MemoryAccess.setCharAtOffset(structSegmt2, 4, 'H');
			MemoryAccess.setCharAtOffset(structSegmt2, 6, 'I');
			MemoryAccess.setCharAtOffset(structSegmt2, 8, 'J');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 0), 'F');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 2), 'H');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 4), 'J');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 6), 'L');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 8), 'N');

		}
	}

	@Test
	public void test_addCharStruct1AndNestedCharStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout1.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout1.varHandle(char.class, PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharStruct1AndNestedCharStructArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setCharAtOffset(structSegmt2, 0, 'F');
			MemoryAccess.setCharAtOffset(structSegmt2, 2, 'G');
			MemoryAccess.setCharAtOffset(structSegmt2, 4, 'H');
			MemoryAccess.setCharAtOffset(structSegmt2, 6, 'I');
			MemoryAccess.setCharAtOffset(structSegmt2, 8, 'J');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'M');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'Y');
		}
	}

	@Test
	public void test_addCharStruct1AndNestedCharStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout1.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout1.varHandle(char.class, PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharStruct1AndNestedCharStructArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setCharAtOffset(structSegmt2, 0, 'F');
			MemoryAccess.setCharAtOffset(structSegmt2, 2, 'G');
			MemoryAccess.setCharAtOffset(structSegmt2, 4, 'H');
			MemoryAccess.setCharAtOffset(structSegmt2, 6, 'I');
			MemoryAccess.setCharAtOffset(structSegmt2, 8, 'J');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 0), 'F');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 2), 'H');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 4), 'H');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 6), 'J');
			Assert.assertEquals(MemoryAccess.getCharAtOffset(resultSegmt, 8), 'K');
		}
	}

	@Test
	public void test_addShortStructs_dupStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2ShortStructs_returnStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt1, (short)56);
			shortHandle2.set(structSegmt1, (short)45);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt2, (short)78);
			shortHandle2.set(structSegmt2, (short)67);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)134);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)112);
		}
	}

	@Test
	public void test_addNestedShortStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedShortStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt1, 0, (short)31);
			MemoryAccess.setShortAtOffset(structSegmt1, 2, (short)33);
			MemoryAccess.setShortAtOffset(structSegmt1, 4, (short)35);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt2, 0, (short)10);
			MemoryAccess.setShortAtOffset(structSegmt2, 2, (short)12);
			MemoryAccess.setShortAtOffset(structSegmt2, 4, (short)14);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 0), 41);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 2), 45);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 4), 49);
		}
	}

	@Test
	public void test_addShortStruct1AndNestedShortStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout1.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout1.varHandle(short.class, PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortStruct1AndNestedShortStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			shortHandle1.set(structSegmt1, (short)56);
			shortHandle2.set(structSegmt1, (short)45);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setShortAtOffset(structSegmt2, 0, (short)31);
			MemoryAccess.setShortAtOffset(structSegmt2, 2, (short)33);
			MemoryAccess.setShortAtOffset(structSegmt2, 4, (short)35);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)87);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)113);
		}
	}

	@Test
	public void test_addShortStruct1AndNestedShortStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout1.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout1.varHandle(short.class, PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortStruct1AndNestedShortStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			shortHandle1.set(structSegmt1, (short)56);
			shortHandle2.set(structSegmt1, (short)45);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setShortAtOffset(structSegmt2, 0, (short)31);
			MemoryAccess.setShortAtOffset(structSegmt2, 2, (short)33);
			MemoryAccess.setShortAtOffset(structSegmt2, 4, (short)35);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 0), 87);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 2), 78);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 4), 80);
		}
	}

	@Test
	public void test_addNestedShortArrayStructs_dupStruct() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, C_SHORT);
		GroupLayout structLayout = MemoryLayout.structLayout(shortArray.withName("array_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedShortArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt1, 0, (short)111);
			MemoryAccess.setShortAtOffset(structSegmt1, 2, (short)222);
			MemoryAccess.setShortAtOffset(structSegmt1, 4, (short)333);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt2, 0, (short)100);
			MemoryAccess.setShortAtOffset(structSegmt2, 2, (short)200);
			MemoryAccess.setShortAtOffset(structSegmt2, 4, (short)300);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 0), 211);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 2), 422);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 4), 633);
		}
	}

	@Test
	public void test_addShortStruct1AndNestedShortArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout1.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout1.varHandle(short.class, PathElement.groupElement("elem2"));

		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, C_SHORT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(shortArray.withName("array_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortStruct1AndNestedShortArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			shortHandle1.set(structSegmt1, (short)156);
			shortHandle2.set(structSegmt1, (short)145);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setShortAtOffset(structSegmt2, 0, (short)111);
			MemoryAccess.setShortAtOffset(structSegmt2, 2, (short)222);
			MemoryAccess.setShortAtOffset(structSegmt2, 4, (short)333);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)267);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)700);
		}
	}

	@Test
	public void test_addShortStruct1AndNestedShortArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout1.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout1.varHandle(short.class, PathElement.groupElement("elem2"));

		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, C_SHORT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(shortArray.withName("array_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortStruct1AndNestedShortArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			shortHandle1.set(structSegmt1, (short)156);
			shortHandle2.set(structSegmt1, (short)145);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setShortAtOffset(structSegmt2, 0, (short)111);
			MemoryAccess.setShortAtOffset(structSegmt2, 2, (short)222);
			MemoryAccess.setShortAtOffset(structSegmt2, 4, (short)333);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 0), 267);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 2), 367);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 4), 478);
		}
	}

	@Test
	public void test_addNestedShortStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout shortStruct = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, shortStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struc_array_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedShortStructArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt1, 0, (short)123);
			MemoryAccess.setShortAtOffset(structSegmt1, 2, (short)156);
			MemoryAccess.setShortAtOffset(structSegmt1, 4, (short)112);
			MemoryAccess.setShortAtOffset(structSegmt1, 6, (short)234);
			MemoryAccess.setShortAtOffset(structSegmt1, 8, (short)131);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt2, 0, (short)111);
			MemoryAccess.setShortAtOffset(structSegmt2, 2, (short)222);
			MemoryAccess.setShortAtOffset(structSegmt2, 4, (short)333);
			MemoryAccess.setShortAtOffset(structSegmt2, 6, (short)444);
			MemoryAccess.setShortAtOffset(structSegmt2, 8, (short)555);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 0), 234);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 2), 378);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 4), 445);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 6), 678);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 8), 686);
		}
	}

	@Test
	public void test_addShortStruct1AndNestedShortStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout1.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout1.varHandle(short.class, PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struc_array_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortStruct1AndNestedShortStructArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			shortHandle1.set(structSegmt1, (short)156);
			shortHandle2.set(structSegmt1, (short)145);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setShortAtOffset(structSegmt2, 0, (short)111);
			MemoryAccess.setShortAtOffset(structSegmt2, 2, (short)222);
			MemoryAccess.setShortAtOffset(structSegmt2, 4, (short)333);
			MemoryAccess.setShortAtOffset(structSegmt2, 6, (short)444);
			MemoryAccess.setShortAtOffset(structSegmt2, 8, (short)555);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)600);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)1366);
		}
	}

	@Test
	public void test_addShortStruct1AndNestedShortStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout1.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout1.varHandle(short.class, PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struc_array_elem1"), C_SHORT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortStruct1AndNestedShortStructArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			shortHandle1.set(structSegmt1, (short)156);
			shortHandle2.set(structSegmt1, (short)145);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setShortAtOffset(structSegmt2, 0, (short)111);
			MemoryAccess.setShortAtOffset(structSegmt2, 2, (short)222);
			MemoryAccess.setShortAtOffset(structSegmt2, 4, (short)333);
			MemoryAccess.setShortAtOffset(structSegmt2, 6, (short)444);
			MemoryAccess.setShortAtOffset(structSegmt2, 8, (short)555);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 0), 267);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 2), 367);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 4), 489);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 6), 589);
			Assert.assertEquals(MemoryAccess.getShortAtOffset(resultSegmt, 8), 700);
		}
	}

	@Test
	public void test_addIntStructs_dupStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(intHandle1.get(resultSegmt), 110224466);
			Assert.assertEquals(intHandle2.get(resultSegmt), 89113354);
		}
	}

	@Test
	public void test_addNestedIntStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), C_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedIntStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt1, 0, 2122232);
			MemoryAccess.setIntAtOffset(structSegmt1, 4, 2526272);
			MemoryAccess.setIntAtOffset(structSegmt1, 8, 2930313);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt2, 0, 1111111);
			MemoryAccess.setIntAtOffset(structSegmt2, 4, 2222222);
			MemoryAccess.setIntAtOffset(structSegmt2, 8, 3333333);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 0), 3233343);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 4), 4748494);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 8), 6263646);
		}
	}

	@Test
	public void test_addIntStruct1AndNestedIntStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout1.varHandle(int.class ,PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout1.varHandle(int.class ,PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntStruct1AndNestedIntStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setIntAtOffset(structSegmt2, 0, 21222324);
			MemoryAccess.setIntAtOffset(structSegmt2, 4, 25262728);
			MemoryAccess.setIntAtOffset(structSegmt2, 8, 29303132);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(intHandle1.get(resultSegmt), 32445668);
			Assert.assertEquals(intHandle2.get(resultSegmt), 110233648);
		}
	}

	@Test
	public void test_addIntStruct1AndNestedIntStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout1.varHandle(int.class ,PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout1.varHandle(int.class ,PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntStruct1AndNestedIntStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setIntAtOffset(structSegmt2, 0, 21222324);
			MemoryAccess.setIntAtOffset(structSegmt2, 4, 25262728);
			MemoryAccess.setIntAtOffset(structSegmt2, 8, 29303132);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 0), 32445668);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 4), 80930516);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 8), 84970920);
		}
	}

	@Test
	public void test_addNestedIntArrayStructs_dupStruct() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, C_INT);
		GroupLayout structLayout = MemoryLayout.structLayout(intArray.withName("array_elem1"), C_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedIntArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt1, 0, 1111111);
			MemoryAccess.setIntAtOffset(structSegmt1, 4, 2222222);
			MemoryAccess.setIntAtOffset(structSegmt1, 8, 3333333);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt2, 0, 2122232);
			MemoryAccess.setIntAtOffset(structSegmt2, 4, 2526272);
			MemoryAccess.setIntAtOffset(structSegmt2, 8, 2930313);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 0), 3233343);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 4), 4748494);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 8), 6263646);
		}
	}

	@Test
	public void test_addIntStruct1AndNestedIntArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout1.varHandle(int.class ,PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout1.varHandle(int.class ,PathElement.groupElement("elem2"));

		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, C_INT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(intArray.withName("array_elem1"), C_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntStruct1AndNestedIntArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setIntAtOffset(structSegmt2, 0, 21222324);
			MemoryAccess.setIntAtOffset(structSegmt2, 4, 25262728);
			MemoryAccess.setIntAtOffset(structSegmt2, 8, 29303132);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(intHandle1.get(resultSegmt), 32445668);
			Assert.assertEquals(intHandle2.get(resultSegmt), 110233648);
		}
	}

	@Test
	public void test_addIntStruct1AndNestedIntArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout1.varHandle(int.class ,PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout1.varHandle(int.class ,PathElement.groupElement("elem2"));

		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, C_INT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(intArray.withName("array_elem1"), C_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntStruct1AndNestedIntArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setIntAtOffset(structSegmt2, 0, 21222324);
			MemoryAccess.setIntAtOffset(structSegmt2, 4, 25262728);
			MemoryAccess.setIntAtOffset(structSegmt2, 8, 29303132);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 0), 32445668);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 4), 80930516);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 8), 84970920);
		}
	}

	@Test
	public void test_addNestedIntStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout intStruct = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, intStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedIntStructArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt1, 0,  1111111);
			MemoryAccess.setIntAtOffset(structSegmt1, 4,  2222222);
			MemoryAccess.setIntAtOffset(structSegmt1, 8,  3333333);
			MemoryAccess.setIntAtOffset(structSegmt1, 12, 4444444);
			MemoryAccess.setIntAtOffset(structSegmt1, 16, 5555555);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt2, 0,  1222324);
			MemoryAccess.setIntAtOffset(structSegmt2, 4,  3262728);
			MemoryAccess.setIntAtOffset(structSegmt2, 8,  1303132);
			MemoryAccess.setIntAtOffset(structSegmt2, 12, 2323233);
			MemoryAccess.setIntAtOffset(structSegmt2, 16, 1545454);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 0),  2333435);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 4),  5484950);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 8),  4636465);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 12), 6767677);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 16), 7101009);
		}
	}

	@Test
	public void test_addIntStruct1AndNestedIntStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout1.varHandle(int.class ,PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout1.varHandle(int.class ,PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntStruct1AndNestedIntStructArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			intHandle1.set(structSegmt1, 1111111);
			intHandle2.set(structSegmt1, 2222222);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setIntAtOffset(structSegmt2, 0,  1222324);
			MemoryAccess.setIntAtOffset(structSegmt2, 4,  3262728);
			MemoryAccess.setIntAtOffset(structSegmt2, 8,  1303132);
			MemoryAccess.setIntAtOffset(structSegmt2, 12, 2323233);
			MemoryAccess.setIntAtOffset(structSegmt2, 16, 1545454);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(intHandle1.get(resultSegmt), 3636567);
			Assert.assertEquals(intHandle2.get(resultSegmt), 9353637);
		}
	}

	@Test
	public void test_addIntStruct1AndNestedIntStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout1.varHandle(int.class ,PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout1.varHandle(int.class ,PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_INT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntStruct1AndNestedIntStructArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			intHandle1.set(structSegmt1, 1111111);
			intHandle2.set(structSegmt1, 2222222);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setIntAtOffset(structSegmt2, 0,  1222324);
			MemoryAccess.setIntAtOffset(structSegmt2, 4,  3262728);
			MemoryAccess.setIntAtOffset(structSegmt2, 8,  1303132);
			MemoryAccess.setIntAtOffset(structSegmt2, 12, 2323233);
			MemoryAccess.setIntAtOffset(structSegmt2, 16, 1545454);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 0),  2333435);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 4),  5484950);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 8),  2414243);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 12), 4545455);
			Assert.assertEquals(MemoryAccess.getIntAtOffset(resultSegmt, 16), 3767676);
		}
	}

	@Test
	public void test_addLongStructs_dupStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2LongStructs_returnStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 224466880022L);
			longHandle2.set(structSegmt2, 113355779911L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(longHandle1.get(resultSegmt), 1212121202009L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 236812569034L);
		}
	}

	@Test
	public void test_addNestedLongStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), longLayout.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedLongStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setLongAtOffset(structSegmt1, 0,  1357913579L);
			MemoryAccess.setLongAtOffset(structSegmt1, 8,  2468024680L);
			MemoryAccess.setLongAtOffset(structSegmt1, 16, 1122334455L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setLongAtOffset(structSegmt2, 0,  1111111111L);
			MemoryAccess.setLongAtOffset(structSegmt2, 8,  2222222222L);
			MemoryAccess.setLongAtOffset(structSegmt2, 16, 3333333333L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 0),  2469024690L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 8),  4690246902L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 16), 4455667788L);
		}
	}

	@Test
	public void test_addLongStruct1AndNestedLongStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout1.varHandle(long.class ,PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout1.varHandle(long.class ,PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), longLayout.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongStruct1AndNestedLongStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setLongAtOffset(structSegmt2, 0,  1111111111L);
			MemoryAccess.setLongAtOffset(structSegmt2, 8,  2222222222L);
			MemoryAccess.setLongAtOffset(structSegmt2, 16, 3333333333L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(longHandle1.get(resultSegmt), 988765433098L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 129012344678L);
		}
	}

	@Test
	public void test_addLongStruct1AndNestedLongStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout1.varHandle(long.class ,PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout1.varHandle(long.class ,PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), longLayout.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongStruct1AndNestedLongStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setLongAtOffset(structSegmt2, 0,  1111111111L);
			MemoryAccess.setLongAtOffset(structSegmt2, 8,  2222222222L);
			MemoryAccess.setLongAtOffset(structSegmt2, 16, 3333333333L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 0),  988765433098L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 8),  125679011345L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 16), 126790122456L);
		}
	}

	@Test
	public void test_addNestedLongArrayStructs_dupStruct() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, longLayout);
		GroupLayout structLayout = MemoryLayout.structLayout(longArray.withName("array_elem1"), longLayout.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedLongArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setLongAtOffset(structSegmt1, 0,  1357913579L);
			MemoryAccess.setLongAtOffset(structSegmt1, 8,  2468024680L);
			MemoryAccess.setLongAtOffset(structSegmt1, 16, 1122334455L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setLongAtOffset(structSegmt2, 0,  1111111111L);
			MemoryAccess.setLongAtOffset(structSegmt2, 8,  2222222222L);
			MemoryAccess.setLongAtOffset(structSegmt2, 16, 3333333333L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 0),  2469024690L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 8),  4690246902L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 16), 4455667788L);
		}
	}

	@Test
	public void test_addLongStruct1AndNestedLongArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout1.varHandle(long.class ,PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout1.varHandle(long.class ,PathElement.groupElement("elem2"));

		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, longLayout);
		GroupLayout structLayout2 = MemoryLayout.structLayout(longArray.withName("array_elem1"), longLayout.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongStruct1AndNestedLongArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setLongAtOffset(structSegmt2, 0,  1111111111L);
			MemoryAccess.setLongAtOffset(structSegmt2, 8,  2222222222L);
			MemoryAccess.setLongAtOffset(structSegmt2, 16, 3333333333L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(longHandle1.get(resultSegmt), 988765433098L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 129012344678L);
		}
	}

	@Test
	public void test_addLongStruct1AndNestedLongArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout1.varHandle(long.class ,PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout1.varHandle(long.class ,PathElement.groupElement("elem2"));

		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, longLayout);
		GroupLayout structLayout2 = MemoryLayout.structLayout(longArray.withName("array_elem1"), longLayout.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongStruct1AndNestedLongArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setLongAtOffset(structSegmt2, 0,  1111111111L);
			MemoryAccess.setLongAtOffset(structSegmt2, 8,  2222222222L);
			MemoryAccess.setLongAtOffset(structSegmt2, 16, 3333333333L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 0),  988765433098L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 8),  125679011345L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 16), 126790122456L);
		}
	}

	@Test
	public void test_addNestedLongStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout longStruct = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, longStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), longLayout.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedLongStructArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setLongAtOffset(structSegmt1, 0, 111111111L);
			MemoryAccess.setLongAtOffset(structSegmt1, 8, 222222222L);
			MemoryAccess.setLongAtOffset(structSegmt1, 16, 333333333L);
			MemoryAccess.setLongAtOffset(structSegmt1, 24, 244444444L);
			MemoryAccess.setLongAtOffset(structSegmt1, 32, 155555555L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setLongAtOffset(structSegmt2, 0, 111111111L);
			MemoryAccess.setLongAtOffset(structSegmt2, 8, 222222222L);
			MemoryAccess.setLongAtOffset(structSegmt2, 16, 333333333L);
			MemoryAccess.setLongAtOffset(structSegmt2, 24, 144444444L);
			MemoryAccess.setLongAtOffset(structSegmt2, 32, 255555555L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 0),  222222222L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 8),  444444444L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 16), 666666666L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 24), 388888888L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 32), 411111110L);
		}
	}

	@Test
	public void test_addLongStruct1AndNestedLongStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout1.varHandle(long.class ,PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout1.varHandle(long.class ,PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), longLayout.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongStruct1AndNestedLongStructArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setLongAtOffset(structSegmt2, 0, 111111111L);
			MemoryAccess.setLongAtOffset(structSegmt2, 8, 222222222L);
			MemoryAccess.setLongAtOffset(structSegmt2, 16, 333333333L);
			MemoryAccess.setLongAtOffset(structSegmt2, 24, 444444444L);
			MemoryAccess.setLongAtOffset(structSegmt2, 32, 155555555L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(longHandle1.get(resultSegmt), 988098766431L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 124279011344L);
		}
	}

	@Test
	public void test_addLongStruct1AndNestedLongStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout1.varHandle(long.class ,PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout1.varHandle(long.class ,PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), longLayout.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongStruct1AndNestedLongStructArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setLongAtOffset(structSegmt2, 0, 111111111L);
			MemoryAccess.setLongAtOffset(structSegmt2, 8, 222222222L);
			MemoryAccess.setLongAtOffset(structSegmt2, 16, 333333333L);
			MemoryAccess.setLongAtOffset(structSegmt2, 24, 444444444L);
			MemoryAccess.setLongAtOffset(structSegmt2, 32, 155555555L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 0),  987765433098L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 8),  123679011345L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 16), 987987655320L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 24), 123901233567L);
			Assert.assertEquals(MemoryAccess.getLongAtOffset(resultSegmt, 32), 123612344678L);
		}
	}

	@Test
	public void test_addFloatStructs_dupStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2FloatStructs_returnStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt2, 24.34F);
			floatHandle2.set(structSegmt2, 13.45F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 49.46F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 24.68F, 0.01F);
		}
	}

	@Test
	public void test_addNestedFloatStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), C_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedFloatStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt1, 0, 31.22F);
			MemoryAccess.setFloatAtOffset(structSegmt1, 4, 33.44F);
			MemoryAccess.setFloatAtOffset(structSegmt1, 8, 35.66F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt2, 0, 22.33F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 4, 44.55F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 8, 66.78F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 0), 53.55F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 4), 77.99F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 8), 102.44F, 0.01F);
		}
	}

	@Test
	public void test_addFloatStruct1AndNestedFloatStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout1.varHandle(float.class ,PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout1.varHandle(float.class ,PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatStruct1AndNestedFloatStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setFloatAtOffset(structSegmt2, 0, 31.22F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 4, 33.44F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 8, 35.66F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 56.34F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 80.33F, 0.01F);
		}
	}

	@Test
	public void test_addFloatStruct1AndNestedFloatStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout1.varHandle(float.class ,PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout1.varHandle(float.class ,PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatStruct1AndNestedFloatStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setFloatAtOffset(structSegmt2, 0, 31.22F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 4, 33.44F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 8, 35.66F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 0), 56.34F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 4), 44.67F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 8), 46.89F, 0.01F);
		}
	}

	@Test
	public void test_addNestedFloatArrayStructs_dupStruct() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, C_FLOAT);
		GroupLayout structLayout = MemoryLayout.structLayout(floatArray.withName("array_elem1"), C_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedFloatArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt1, 0, 111.11F);
			MemoryAccess.setFloatAtOffset(structSegmt1, 4, 222.22F);
			MemoryAccess.setFloatAtOffset(structSegmt1, 8, 333.33F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt2, 0, 123.45F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 4, 234.13F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 8, 245.79F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 0), 234.56F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 4), 456.35F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 8), 579.12F, 0.01F);
		}
	}

	@Test
	public void test_addFloatStruct1AndNestedFloatArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout1.varHandle(float.class ,PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout1.varHandle(float.class ,PathElement.groupElement("elem2"));

		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, C_FLOAT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(floatArray.withName("array_elem1"), C_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatStruct1AndNestedFloatArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setFloatAtOffset(structSegmt2, 0, 111.11F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 4, 222.22F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 8, 333.33F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 136.23F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 566.78F, 0.01F);
		}
	}

	@Test
	public void test_addFloatStruct1AndNestedFloatArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout1.varHandle(float.class ,PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout1.varHandle(float.class ,PathElement.groupElement("elem2"));

		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, C_FLOAT);
		GroupLayout structLayout2 = MemoryLayout.structLayout(floatArray.withName("array_elem1"), C_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatStruct1AndNestedFloatArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setFloatAtOffset(structSegmt2, 0, 111.11F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 4, 222.22F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 8, 333.33F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 0), 136.23F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 4), 233.45F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 8), 344.56F, 0.01F);
		}
	}

	@Test
	public void test_addNestedFloatStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout floatStruct = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, floatStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedFloatStructArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt1, 0, 111.11F);
			MemoryAccess.setFloatAtOffset(structSegmt1, 4, 222.22F);
			MemoryAccess.setFloatAtOffset(structSegmt1, 8, 333.33F);
			MemoryAccess.setFloatAtOffset(structSegmt1, 12, 444.44F);
			MemoryAccess.setFloatAtOffset(structSegmt1, 16, 555.55F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt2, 0, 311.18F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 4, 122.27F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 8, 233.36F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 12, 144.43F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 16, 255.51F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 0), 422.29F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 4), 344.49F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 8), 566.69F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 12), 588.87F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 16), 811.06F, 0.01F);
		}
	}

	@Test
	public void test_addFloatStruct1AndNestedFloatStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout1.varHandle(float.class ,PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout1.varHandle(float.class ,PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatStruct1AndNestedFloatStructArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setFloatAtOffset(structSegmt2, 0, 111.11F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 4, 222.22F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 8, 333.38F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 12, 444.44F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 16, 155.55F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 469.61F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 833.44F, 0.01F);
		}
	}

	@Test
	public void test_addFloatStruct1AndNestedFloatStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout1.varHandle(float.class ,PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout1.varHandle(float.class ,PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatStruct1AndNestedFloatStructArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setFloatAtOffset(structSegmt2, 0, 111.11F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 4, 222.22F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 8, 333.33F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 12, 444.44F);
			MemoryAccess.setFloatAtOffset(structSegmt2, 16, 155.55F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 0), 136.23F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 4), 233.45F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 8), 358.45F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 12), 455.67F, 0.01F);
			Assert.assertEquals(MemoryAccess.getFloatAtOffset(resultSegmt, 16), 166.78F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleStructs_dupStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2DoubleStructs_returnStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 33.444D);
			doubleHandle2.set(structSegmt2, 44.555D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 44.666D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_addNestedDoubleStructs_dupStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), C_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedDoubleStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setDoubleAtOffset(structSegmt1, 0,  31.789D);
			MemoryAccess.setDoubleAtOffset(structSegmt1, 8,  33.456D);
			MemoryAccess.setDoubleAtOffset(structSegmt1, 16, 35.123D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 0,  11.222D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 8,  22.333D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 16, 33.445D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 0),  43.011D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 8),  55.789D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 16), 68.568D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleStruct1AndNestedDoubleStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout1.varHandle(double.class ,PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout1.varHandle(double.class ,PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleStruct1AndNestedDoubleStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 0, 31.789D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 8, 33.456D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 16, 35.123D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 43.011D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 90.912D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleStruct1AndNestedDoubleStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout1.varHandle(double.class ,PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout1.varHandle(double.class ,PathElement.groupElement("elem2"));

		GroupLayout structLayout2 = MemoryLayout.structLayout(structLayout1.withName("struct_elem1"), C_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleStruct1AndNestedDoubleStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 0, 31.789D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 8, 33.456D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 16, 35.124D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 0),  43.011D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 8),  55.789D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 16), 57.457D, 0.001D);
		}
	}

	@Test
	public void test_addNestedDoubleArrayStructs_dupStruct() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, C_DOUBLE);
		GroupLayout structLayout = MemoryLayout.structLayout(doubleArray.withName("array_elem1"), C_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedDoubleArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setDoubleAtOffset(structSegmt1, 0,  31.789D);
			MemoryAccess.setDoubleAtOffset(structSegmt1, 8,  33.456D);
			MemoryAccess.setDoubleAtOffset(structSegmt1, 16, 35.123D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 0,  11.222D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 8,  22.333D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 16, 33.445D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 0),  43.011D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 8),  55.789D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 16), 68.568D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleStruct1AndNestedDoubleArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout1.varHandle(double.class ,PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout1.varHandle(double.class ,PathElement.groupElement("elem2"));

		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, C_DOUBLE);
		GroupLayout structLayout2 = MemoryLayout.structLayout(doubleArray.withName("array_elem1"), C_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleStruct1AndNestedDoubleArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 0, 31.789D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 8, 33.456D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 16, 35.123D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 43.011D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 90.912D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleStruct1AndNestedDoubleArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout1.varHandle(double.class ,PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout1.varHandle(double.class ,PathElement.groupElement("elem2"));

		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, C_DOUBLE);
		GroupLayout structLayout2 = MemoryLayout.structLayout(doubleArray.withName("array_elem1"), C_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleStruct1AndNestedDoubleArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 0, 31.789D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 8, 33.456D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 16, 35.124D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 0),  43.011D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 8),  55.789D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 16), 57.457D, 0.001D);
		}
	}

	@Test
	public void test_addNestedDoubleStructArrayStructs_dupStruct() throws Throwable {
		GroupLayout doubleStruct = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, doubleStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("addNestedDoubleStructArrayStructs_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			MemoryAccess.setDoubleAtOffset(structSegmt1, 0, 111.111D);
			MemoryAccess.setDoubleAtOffset(structSegmt1, 8, 222.222D);
			MemoryAccess.setDoubleAtOffset(structSegmt1, 16, 333.333D);
			MemoryAccess.setDoubleAtOffset(structSegmt1, 24, 444.444D);
			MemoryAccess.setDoubleAtOffset(structSegmt1, 32, 555.555D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 0, 519.117D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 8, 428.226D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 16, 336.335D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 24, 247.444D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 32, 155.553D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 0),  630.228D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 8),  650.448D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 16), 669.668D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 24), 691.888D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 32), 711.108D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleStruct1AndNestedDoubleStructArrayStruct2_returnStruct1_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout1.varHandle(double.class ,PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout1.varHandle(double.class ,PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout1, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleStruct1AndNestedDoubleStructArrayStruct2_returnStruct1_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 0, 519.117D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 8, 428.226D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 16, 336.331D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 24, 247.444D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 32, 155.552D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 866.67D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 853.555D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleStruct1AndNestedDoubleStructArrayStruct2_returnStruct2_dupStruct() throws Throwable {
		GroupLayout structLayout1 = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout1.varHandle(double.class ,PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout1.varHandle(double.class ,PathElement.groupElement("elem2"));

		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, structLayout1);
		GroupLayout structLayout2 = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_DOUBLE.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout2, structLayout1, structLayout2);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleStruct1AndNestedDoubleStructArrayStruct2_returnStruct2_dupStruct").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);

			MemorySegment structSegmt1 = allocator.allocate(structLayout1);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout2);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 0, 519.116D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 8, 428.225D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 16, 336.331D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 24, 247.444D);
			MemoryAccess.setDoubleAtOffset(structSegmt2, 32, 155.552D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 0),  530.338D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 8),  450.558D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 16), 347.553D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 24), 269.777D, 0.001D);
			Assert.assertEquals(MemoryAccess.getDoubleAtOffset(resultSegmt, 32), 177.885D, 0.001D);
		}
	}
}
