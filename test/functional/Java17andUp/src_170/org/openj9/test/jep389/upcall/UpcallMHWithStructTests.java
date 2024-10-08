/*
 * Copyright IBM Corp. and others 2021
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
package org.openj9.test.jep389.upcall;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.lang.invoke.VarHandle;
import java.nio.ByteOrder;

import jdk.incubator.foreign.Addressable;
import jdk.incubator.foreign.CLinker;
import static jdk.incubator.foreign.CLinker.C_CHAR;
import static jdk.incubator.foreign.CLinker.C_DOUBLE;
import static jdk.incubator.foreign.CLinker.C_FLOAT;
import static jdk.incubator.foreign.CLinker.C_INT;
import static jdk.incubator.foreign.CLinker.C_LONG;
import static jdk.incubator.foreign.CLinker.C_LONG_LONG;
import static jdk.incubator.foreign.CLinker.C_POINTER;
import static jdk.incubator.foreign.CLinker.C_SHORT;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAccess;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryHandles;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemoryLayout.PathElement;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.SequenceLayout;
import jdk.incubator.foreign.SymbolLookup;
import jdk.incubator.foreign.ValueLayout;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) for argument/return struct in upcall.
 *
 * Note: the padding elements in the struct are only required by RI or VarHandle (accessing the
 * data address) while they are totally ignored in OpenJ9 given the padding/alignment are
 * computed by libffi automatically in native.
 */
@Test(groups = { "level.sanity" })
public class UpcallMHWithStructTests {
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static String arch = System.getProperty("os.arch").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinOS = osName.contains("win");
	/* The padding of struct is not required on Linux/s390x and Windows/x64 */
	private static boolean isStructPaddingNotRequired = isWinOS && (arch.equals("amd64") || arch.equals("x86_64"))
														|| osName.contains("linux") && arch.equals("s390x");
	/* long long is 64 bits on AIX/ppc64, which is the same as Windows */
	private static ValueLayout longLayout = (isWinOS || isAixOS) ? C_LONG_LONG : C_LONG;
	private static CLinker clinker = CLinker.getInstance();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test(enabled=false)
	public static boolean byteToBool(byte value) {
		return (value != 0);
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithXorByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromStructWithXorByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithXor,
				FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt, (byte)0);
			boolHandle2.set(structSegmt, (byte)1);

			boolean result = (boolean)mh.invokeExact(false, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAnd20BoolsFromStructWithXorByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR,
				C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR,
				C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR);

		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAnd20BoolsFromStructWithXorByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAnd20BoolsFromStructWithXor,
					FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 3, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 4, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 5, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 6, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 7, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 8, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 9, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 10, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 11, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 12, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 13, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 14, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 15, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 16, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 17, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 18, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 19, (byte)1);

			boolean result = (boolean)mh.invokeExact(true, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolFromPointerAndBoolsFromStructWithXorByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(boolean.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolFromPointerAndBoolsFromStructWithXorByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolFromPointerAndBoolsFromStructWithXor,
				FunctionDescriptor.of(C_CHAR, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment boolSegmt = allocator.allocate(C_CHAR);
			MemoryAccess.setByte(boolSegmt, (byte)1);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt, (byte)0);
			boolHandle2.set(structSegmt, (byte)1);

			boolean result = (boolean)mh.invokeExact(boolSegmt.address(), structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, false);
		}
	}

	@Test
	public void test_addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR, C_CHAR);
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer,
				FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment boolSegmt = allocator.allocate(C_CHAR);
			MemoryAccess.setByte(boolSegmt, (byte)0);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)1);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(boolSegmt.address(), structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(C_CHAR.byteSize(), scope);
			boolean result = byteToBool(MemoryAccess.getByte(resultSegmt));
			Assert.assertEquals(result, true);
			Assert.assertEquals(resultAddr.toRawLongValue(), boolSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructPointerWithXorByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromStructPointerWithXorByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructPointerWithXor,
				FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt, (byte)1);
			boolHandle2.set(structSegmt, (byte)0);

			boolean result = (boolean)mh.invokeExact(false, structSegmt.address(), upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromNestedStructWithXorByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				C_CHAR.withName("elem2")) : MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
						C_CHAR.withName("elem2"), MemoryLayout.paddingLayout(8));
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromNestedStructWithXorByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromNestedStructWithXor,
				FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)1);

			boolean result = (boolean)mh.invokeExact(true, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromNestedStructWithXor_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				nestedStructLayout.withName("struct_elem2")) : MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				nestedStructLayout.withName("struct_elem2"), MemoryLayout.paddingLayout(8));
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromNestedStructWithXor_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromNestedStructWithXor_reverseOrder,
				FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)1);

			boolean result = (boolean)mh.invokeExact(true, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedBoolArrayByUpcallMH() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, C_CHAR);
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(boolArray.withName("array_elem1"),
				C_CHAR.withName("elem2")): MemoryLayout.structLayout(boolArray.withName("array_elem1"),
				C_CHAR.withName("elem2"), MemoryLayout.paddingLayout(8));
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromStructWithNestedBoolArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithNestedBoolArray,
				FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)0);

			boolean result = (boolean)mh.invokeExact(false, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout boolArray = MemoryLayout.sequenceLayout(2, C_CHAR);
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				boolArray.withName("array_elem2")): MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				boolArray.withName("array_elem2"), MemoryLayout.paddingLayout(8));
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder,
				FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)0);

			boolean result = (boolean)mh.invokeExact(false, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout boolStruct = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, boolStruct);
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(structArray.withName("struct_array_elem1"),
				C_CHAR.withName("elem2")) : MemoryLayout.structLayout(structArray.withName("struct_array_elem1"),
				C_CHAR.withName("elem2"), MemoryLayout.paddingLayout(C_CHAR.bitSize() * 3));
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithNestedStructArray,
				FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 3, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 4, (byte)0);

			boolean result = (boolean)mh.invokeExact(true, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout boolStruct = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, boolStruct);
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				structArray.withName("struct_array_elem2")) : MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				structArray.withName("struct_array_elem2"), MemoryLayout.paddingLayout(C_CHAR.bitSize() * 3));
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder,
				FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)0);
			MemoryAccess.setByteAtOffset(structSegmt, 3, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 4, (byte)0);

			boolean result = (boolean)mh.invokeExact(true, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_add2BoolStructsWithXor_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolStructsWithXor_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2BoolStructsWithXor_returnStruct,
				FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt1, (byte)1);
			boolHandle2.set(structSegmt1, (byte)0);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt2, (byte)1);
			boolHandle2.set(structSegmt2, (byte)1);


			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(byteToBool((byte)boolHandle1.get(resultSegmt)), false);
			Assert.assertEquals(byteToBool((byte)boolHandle2.get(resultSegmt)), true);
		}
	}

	@Test
	public void test_add2BoolStructsWithXor_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolStructsWithXor_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2BoolStructsWithXor_returnStructPointer,
				FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt1, (byte)1);
			boolHandle2.set(structSegmt1, (byte)0);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt2, (byte)1);
			boolHandle2.set(structSegmt2, (byte)1);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(structLayout.byteSize(), scope);
			Assert.assertEquals(byteToBool((byte)boolHandle1.get(resultSegmt)), false);
			Assert.assertEquals(byteToBool((byte)boolHandle2.get(resultSegmt)), true);
		}
	}

	@Test
	public void test_add3BoolStructsWithXor_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"),
				C_CHAR.withName("elem3")) : MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"),
				C_CHAR.withName("elem3"), MemoryLayout.paddingLayout(8));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));
		VarHandle boolHandle3 = structLayout.varHandle(byte.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add3BoolStructsWithXor_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3BoolStructsWithXor_returnStruct,
				FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt1, (byte)1);
			boolHandle2.set(structSegmt1, (byte)0);
			boolHandle3.set(structSegmt1, (byte)1);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt2, (byte)1);
			boolHandle2.set(structSegmt2, (byte)1);
			boolHandle3.set(structSegmt2, (byte)0);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(byteToBool((byte)boolHandle1.get(resultSegmt)), false);
			Assert.assertEquals(byteToBool((byte)boolHandle2.get(resultSegmt)), true);
			Assert.assertEquals(byteToBool((byte)boolHandle3.get(resultSegmt)), true);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStruct,
					FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)8);
			byteHandle2.set(structSegmt, (byte)9);

			byte result = (byte)mh.invokeExact((byte)6, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 23);
		}
	}

	@Test
	public void test_addByteAnd20BytesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR,
				C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR,
				C_CHAR, C_CHAR, C_CHAR, C_CHAR, C_CHAR);

		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAnd20BytesFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAnd20BytesFromStruct,
					FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)2);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)3);
			MemoryAccess.setByteAtOffset(structSegmt, 3, (byte)4);
			MemoryAccess.setByteAtOffset(structSegmt, 4, (byte)5);
			MemoryAccess.setByteAtOffset(structSegmt, 5, (byte)6);
			MemoryAccess.setByteAtOffset(structSegmt, 6, (byte)7);
			MemoryAccess.setByteAtOffset(structSegmt, 7, (byte)8);
			MemoryAccess.setByteAtOffset(structSegmt, 8, (byte)9);
			MemoryAccess.setByteAtOffset(structSegmt, 9, (byte)10);
			MemoryAccess.setByteAtOffset(structSegmt, 10, (byte)1);
			MemoryAccess.setByteAtOffset(structSegmt, 11, (byte)2);
			MemoryAccess.setByteAtOffset(structSegmt, 12, (byte)3);
			MemoryAccess.setByteAtOffset(structSegmt, 13, (byte)4);
			MemoryAccess.setByteAtOffset(structSegmt, 14, (byte)5);
			MemoryAccess.setByteAtOffset(structSegmt, 15, (byte)6);
			MemoryAccess.setByteAtOffset(structSegmt, 16, (byte)7);
			MemoryAccess.setByteAtOffset(structSegmt, 17, (byte)8);
			MemoryAccess.setByteAtOffset(structSegmt, 18, (byte)9);
			MemoryAccess.setByteAtOffset(structSegmt, 19, (byte)10);

			byte result = (byte)mh.invokeExact((byte)11, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 121);
		}
	}

	@Test
	public void test_addByteFromPointerAndBytesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(byte.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteFromPointerAndBytesFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteFromPointerAndBytesFromStruct,
					FunctionDescriptor.of(C_CHAR, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment byteSegmt = allocator.allocate(C_CHAR);
			MemoryAccess.setByte(byteSegmt, (byte)12);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)18);
			byteHandle2.set(structSegmt, (byte)19);

			byte result = (byte)mh.invokeExact(byteSegmt.address(), structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 49);
		}
	}

	@Test
	public void test_addByteFromPointerAndBytesFromStruct_returnBytePointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteFromPointerAndBytesFromStruct_returnBytePointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteFromPointerAndBytesFromStruct_returnBytePointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment byteSegmt = allocator.allocate(C_CHAR);
			MemoryAccess.setByte(byteSegmt, (byte)12);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)14);
			byteHandle2.set(structSegmt, (byte)16);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(byteSegmt.address(), structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(C_CHAR.byteSize(), scope);
			VarHandle byteHandle = MemoryHandles.varHandle(byte.class, ByteOrder.nativeOrder());
			byte result = (byte)byteHandle.get(resultSegmt, 0);
			Assert.assertEquals(result, 42);
			Assert.assertEquals(resultAddr.toRawLongValue(), byteSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addByteAndBytesFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(byte.class, byte.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructPointer,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)11);
			byteHandle2.set(structSegmt, (byte)12);

			byte result = (byte)mh.invokeExact((byte)13, structSegmt.address(), upcallFuncAddr);
			Assert.assertEquals(result, 36);
		}
	}

	@Test
	public void test_addByteAndBytesFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				C_CHAR.withName("elem2")) : MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				C_CHAR.withName("elem2"), MemoryLayout.paddingLayout(8));
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromNestedStruct,
					FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)11);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)22);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)33);

			byte result = (byte)mh.invokeExact((byte)46, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 112);
		}
	}

	@Test
	public void test_addByteAndBytesFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				nestedStructLayout.withName("struct_elem2")): MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				nestedStructLayout.withName("struct_elem2"), MemoryLayout.paddingLayout(8));
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)12);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)24);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)36);

			byte result = (byte)mh.invokeExact((byte)48, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 120);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedByteArrayByUpcallMH() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, C_CHAR);
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(byteArray.withName("array_elem1"),
				C_CHAR.withName("elem2")): MemoryLayout.structLayout(byteArray.withName("array_elem1"),
				C_CHAR.withName("elem2"), MemoryLayout.paddingLayout(8));
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromStructWithNestedByteArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructWithNestedByteArray,
					FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)11);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)12);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)13);

			byte result = (byte)mh.invokeExact((byte)14, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 50);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedByteArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(2, C_CHAR);
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				byteArray.withName("array_elem2")): MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				byteArray.withName("array_elem2"), MemoryLayout.paddingLayout(8));
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromStructWithNestedByteArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructWithNestedByteArray_reverseOrder,
					FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)12);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)14);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)16);

			byte result = (byte)mh.invokeExact((byte)18, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 60);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout byteStruct = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, byteStruct);
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(structArray.withName("struct_array_elem1"),
				C_CHAR.withName("elem2")) : MemoryLayout.structLayout(structArray.withName("struct_array_elem1"),
				C_CHAR.withName("elem2"), MemoryLayout.paddingLayout(C_CHAR.bitSize() * 3));
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructWithNestedStructArray,
					FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)11);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)12);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)13);
			MemoryAccess.setByteAtOffset(structSegmt, 3, (byte)14);
			MemoryAccess.setByteAtOffset(structSegmt, 4, (byte)15);

			byte result = (byte)mh.invokeExact((byte)16, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 81);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout byteStruct = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, byteStruct);
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				structArray.withName("struct_array_elem2")) : MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				structArray.withName("struct_array_elem2"), MemoryLayout.paddingLayout(C_CHAR.bitSize() * 3));
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)12);
			MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)14);
			MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)16);
			MemoryAccess.setByteAtOffset(structSegmt, 3, (byte)18);
			MemoryAccess.setByteAtOffset(structSegmt, 4, (byte)20);

			byte result = (byte)mh.invokeExact((byte)22, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 102);
		}
	}

	@Test
	public void test_add1ByteStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add1ByteStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add1ByteStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)25);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)24);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
		}
	}

	@Test
	public void test_add2ByteStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2ByteStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2ByteStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)24);
			byteHandle2.set(structSegmt2, (byte)13);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)24);
		}
	}

	@Test
	public void test_add2ByteStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2ByteStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2ByteStructs_returnStructPointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)24);
			byteHandle2.set(structSegmt2, (byte)13);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(structLayout.byteSize(), scope);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)24);
		}
	}

	@Test
	public void test_add3ByteStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"),
				C_CHAR.withName("elem3")) : MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"),
				C_CHAR.withName("elem3"), MemoryLayout.paddingLayout(8));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));
		VarHandle byteHandle3 = structLayout.varHandle(byte.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add3ByteStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3ByteStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			byteHandle3.set(structSegmt1, (byte)12);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)24);
			byteHandle2.set(structSegmt2, (byte)13);
			byteHandle3.set(structSegmt2, (byte)16);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)24);
			Assert.assertEquals((byte)byteHandle3.get(resultSegmt), (byte)28);
		}
	}

	@Test
	public void test_addCharAndCharsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStruct,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			charHandle1.set(structSegmt, 'A');
			charHandle2.set(structSegmt, 'B');

			char result = (char)mh.invokeExact('C', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'D');
		}
	}

	@Test
	public void test_addCharAnd10CharsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT, C_SHORT, C_SHORT, C_SHORT,
				C_SHORT, C_SHORT, C_SHORT, C_SHORT, C_SHORT, C_SHORT);

		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharAnd10CharsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAnd10CharsFromStruct,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt, 0, 'A');
			MemoryAccess.setCharAtOffset(structSegmt, 2, 'A');
			MemoryAccess.setCharAtOffset(structSegmt, 4, 'B');
			MemoryAccess.setCharAtOffset(structSegmt, 6, 'B');
			MemoryAccess.setCharAtOffset(structSegmt, 8, 'C');
			MemoryAccess.setCharAtOffset(structSegmt, 10, 'C');
			MemoryAccess.setCharAtOffset(structSegmt, 12, 'D');
			MemoryAccess.setCharAtOffset(structSegmt, 14, 'D');
			MemoryAccess.setCharAtOffset(structSegmt, 16, 'E');
			MemoryAccess.setCharAtOffset(structSegmt, 18, 'E');

			char result = (char)mh.invokeExact('A', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'U');
		}
	}

	@Test
	public void test_addCharFromPointerAndCharsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(char.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharFromPointerAndCharsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharFromPointerAndCharsFromStruct,
					FunctionDescriptor.of(C_SHORT, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment charSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setChar(charSegmt, 'D');
			MemorySegment structSegmt = allocator.allocate(structLayout);
			charHandle1.set(structSegmt, 'E');
			charHandle2.set(structSegmt, 'F');

			char result = (char)mh.invokeExact(charSegmt.address(), structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'M');
		}
	}

	@Test
	public void test_addCharFromPointerAndCharsFromStruct_returnCharPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharFromPointerAndCharsFromStruct_returnCharPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharFromPointerAndCharsFromStruct_returnCharPointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment charSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setChar(charSegmt, 'D');
			MemorySegment structSegmt = allocator.allocate(structLayout);
			charHandle1.set(structSegmt, 'E');
			charHandle2.set(structSegmt, 'F');

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(charSegmt.address(), structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(C_SHORT.byteSize(), scope);
			VarHandle charHandle = MemoryHandles.varHandle(char.class, ByteOrder.nativeOrder());
			char result = (char)charHandle.get(resultSegmt, 0);
			Assert.assertEquals(result, 'M');
			Assert.assertEquals(resultSegmt.address().toRawLongValue(), charSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addCharAndCharsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(char.class, char.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructPointer,
					FunctionDescriptor.of(C_SHORT, C_SHORT, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			charHandle1.set(structSegmt, 'H');
			charHandle2.set(structSegmt, 'I');

			char result = (char)mh.invokeExact('G', structSegmt.address(), upcallFuncAddr);
			Assert.assertEquals(result, 'V');
		}
	}

	@Test
	public void test_addCharAndCharsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				C_SHORT.withName("elem2")) : MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				C_SHORT.withName("elem2"), MemoryLayout.paddingLayout(16));

		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromNestedStruct,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt, 0, 'E');
			MemoryAccess.setCharAtOffset(structSegmt, 2, 'F');
			MemoryAccess.setCharAtOffset(structSegmt, 4, 'G');

			char result = (char)mh.invokeExact('H', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'W');
		}
	}

	@Test
	public void test_addCharAndCharsFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_SHORT.withName("elem1"),
				nestedStructLayout.withName("struct_elem2")) : MemoryLayout.structLayout(C_SHORT.withName("elem1"),
						nestedStructLayout.withName("struct_elem2"), MemoryLayout.paddingLayout(16));
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt, 0, 'E');
			MemoryAccess.setCharAtOffset(structSegmt, 2, 'F');
			MemoryAccess.setCharAtOffset(structSegmt, 4, 'G');

			char result = (char)mh.invokeExact('H', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'W');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedCharArrayByUpcallMH() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, C_SHORT);
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(charArray.withName("array_elem1"),
				C_SHORT.withName("elem2")) : MemoryLayout.structLayout(charArray.withName("array_elem1"),
				C_SHORT.withName("elem2"), MemoryLayout.paddingLayout(16));
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromStructWithNestedCharArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructWithNestedCharArray,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt, 0, 'A');
			MemoryAccess.setCharAtOffset(structSegmt, 2, 'B');
			MemoryAccess.setCharAtOffset(structSegmt, 4, 'C');

			char result = (char)mh.invokeExact('D', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'G');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedCharArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout charArray = MemoryLayout.sequenceLayout(2, C_SHORT);
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_SHORT.withName("elem1"),
				charArray.withName("array_elem2")) : MemoryLayout.structLayout(C_SHORT.withName("elem1"),
				charArray.withName("array_elem2"), MemoryLayout.paddingLayout(16));
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromStructWithNestedCharArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructWithNestedCharArray_reverseOrder,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt, 0, 'A');
			MemoryAccess.setCharAtOffset(structSegmt, 2, 'B');
			MemoryAccess.setCharAtOffset(structSegmt, 4, 'C');

			char result = (char)mh.invokeExact('D', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'G');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout charStruct = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, charStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_SHORT.withName("elem2"));
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructWithNestedStructArray,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt, 0, 'E');
			MemoryAccess.setCharAtOffset(structSegmt, 2, 'F');
			MemoryAccess.setCharAtOffset(structSegmt, 4, 'G');
			MemoryAccess.setCharAtOffset(structSegmt, 6, 'H');
			MemoryAccess.setCharAtOffset(structSegmt, 8, 'I');

			char result = (char)mh.invokeExact('J', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'h');
		}
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout charStruct = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, charStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"),
				structArray.withName("struct_array_elem2"), MemoryLayout.paddingLayout(16));
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setCharAtOffset(structSegmt, 0, 'E');
			MemoryAccess.setCharAtOffset(structSegmt, 2, 'F');
			MemoryAccess.setCharAtOffset(structSegmt, 4, 'G');
			MemoryAccess.setCharAtOffset(structSegmt, 6, 'H');
			MemoryAccess.setCharAtOffset(structSegmt, 8, 'I');

			char result = (char)mh.invokeExact('J', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'h');
		}
	}

	@Test
	public void test_add2CharStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2CharStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2CharStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt2, 'C');
			charHandle2.set(structSegmt2, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'C');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'E');
		}
	}

	@Test
	public void test_add2CharStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2CharStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2CharStructs_returnStructPointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt2, 'C');
			charHandle2.set(structSegmt2, 'D');

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(structLayout.byteSize(), scope);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'C');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'E');
		}
	}

	@Test
	public void test_add3CharStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"),
				C_SHORT.withName("elem3")) : MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"),
				C_SHORT.withName("elem3"), MemoryLayout.paddingLayout(16));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));
		VarHandle charHandle3 = structLayout.varHandle(char.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add3CharStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3CharStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			charHandle3.set(structSegmt1, 'C');
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt2, 'B');
			charHandle2.set(structSegmt2, 'C');
			charHandle3.set(structSegmt2, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'B');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'D');
			Assert.assertEquals(charHandle3.get(resultSegmt), 'F');
		}
	}

	@Test
	public void test_addShortAndShortsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStruct,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)888);
			shortHandle2.set(structSegmt, (short)999);

			short result = (short)mh.invokeExact((short)777, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 2664);
		}
	}

	@Test
	public void test_addShortAnd10ShortsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT, C_SHORT, C_SHORT, C_SHORT,
				C_SHORT, C_SHORT, C_SHORT, C_SHORT, C_SHORT, C_SHORT);

		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAnd10ShortsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAnd10ShortsFromStruct,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt, 0, (short)10);
			MemoryAccess.setShortAtOffset(structSegmt, 2, (short)20);
			MemoryAccess.setShortAtOffset(structSegmt, 4, (short)30);
			MemoryAccess.setShortAtOffset(structSegmt, 6, (short)40);
			MemoryAccess.setShortAtOffset(structSegmt, 8, (short)50);
			MemoryAccess.setShortAtOffset(structSegmt, 10, (short)60);
			MemoryAccess.setShortAtOffset(structSegmt, 12, (short)70);
			MemoryAccess.setShortAtOffset(structSegmt, 14, (short)80);
			MemoryAccess.setShortAtOffset(structSegmt, 16, (short)90);
			MemoryAccess.setShortAtOffset(structSegmt, 18, (short)100);

			short result = (short)mh.invokeExact((short)110, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 660);
		}
	}

	@Test
	public void test_addShortFromPointerAndShortsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(short.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortFromPointerAndShortsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortFromPointerAndShortsFromStruct,
					FunctionDescriptor.of(C_SHORT, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setShort(shortSegmt, (short)1112);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)1118);
			shortHandle2.set(structSegmt, (short)1119);

			short result = (short)mh.invokeExact(shortSegmt.address(), structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 3349);
		}
	}

	@Test
	public void test_addShortFromPointerAndShortsFromStruct_returnShortPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortFromPointerAndShortsFromStruct_returnShortPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortFromPointerAndShortsFromStruct_returnShortPointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setShort(shortSegmt, (short)1112);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)1118);
			shortHandle2.set(structSegmt, (short)1119);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(shortSegmt.address(), structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(C_SHORT.byteSize(), scope);
			VarHandle shortHandle = MemoryHandles.varHandle(short.class, ByteOrder.nativeOrder());
			short result = (short)shortHandle.get(resultSegmt, 0);
			Assert.assertEquals(result, 3349);
			Assert.assertEquals(resultSegmt.address().toRawLongValue(), shortSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addShortAndShortsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(short.class, short.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructPointer,
					FunctionDescriptor.of(C_SHORT, C_SHORT, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)2222);
			shortHandle2.set(structSegmt, (short)4444);

			short result = (short)mh.invokeExact((short)6666, structSegmt.address(), upcallFuncAddr);
			Assert.assertEquals(result, 13332);
		}
	}

	@Test
	public void test_addShortAndShortsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				C_SHORT.withName("elem2")) : MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				C_SHORT.withName("elem2"), MemoryLayout.paddingLayout(16));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromNestedStruct,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt, 0, (short)331);
			MemoryAccess.setShortAtOffset(structSegmt, 2, (short)333);
			MemoryAccess.setShortAtOffset(structSegmt, 4, (short)335);

			short result = (short)mh.invokeExact((short)337, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1336);
		}
	}

	@Test
	public void test_addShortAndShortsFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_SHORT.withName("elem1"),
				nestedStructLayout.withName("struct_elem2")) : MemoryLayout.structLayout(C_SHORT.withName("elem1"),
				nestedStructLayout.withName("struct_elem2"), MemoryLayout.paddingLayout(16));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt, 0, (short)331);
			MemoryAccess.setShortAtOffset(structSegmt, 2, (short)333);
			MemoryAccess.setShortAtOffset(structSegmt, 4, (short)335);

			short result = (short)mh.invokeExact((short)337, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1336);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedShortArrayByUpcallMH() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, C_SHORT);
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(shortArray.withName("array_elem1"),
				C_SHORT.withName("elem2")) : MemoryLayout.structLayout(shortArray.withName("array_elem1"),
				C_SHORT.withName("elem2"), MemoryLayout.paddingLayout(16));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromStructWithNestedShortArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructWithNestedShortArray,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt, 0, (short)1111);
			MemoryAccess.setShortAtOffset(structSegmt, 2, (short)2222);
			MemoryAccess.setShortAtOffset(structSegmt, 4, (short)3333);

			short result = (short)mh.invokeExact((short)4444, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 11110);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedShortArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(2, C_SHORT);
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_SHORT.withName("elem1"),
				shortArray.withName("array_elem2")) : MemoryLayout.structLayout(C_SHORT.withName("elem1"),
				shortArray.withName("array_elem2"), MemoryLayout.paddingLayout(16));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromStructWithNestedShortArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructWithNestedShortArray_reverseOrder,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt, 0, (short)1111);
			MemoryAccess.setShortAtOffset(structSegmt, 2, (short)2222);
			MemoryAccess.setShortAtOffset(structSegmt, 4, (short)3333);

			short result = (short)mh.invokeExact((short)4444, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 11110);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout shortStruct = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, shortStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struc_array_elem1"), C_SHORT.withName("elem2"));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructWithNestedStructArray,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt, 0, (short)1111);
			MemoryAccess.setShortAtOffset(structSegmt, 2, (short)2222);
			MemoryAccess.setShortAtOffset(structSegmt, 4, (short)3333);
			MemoryAccess.setShortAtOffset(structSegmt, 6, (short)4444);
			MemoryAccess.setShortAtOffset(structSegmt, 8, (short)5555);

			short result = (short)mh.invokeExact((short)6666, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 23331);
		}
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout shortStruct = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, shortStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), structArray.withName("struc_array_elem2"));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt, 0, (short)1111);
			MemoryAccess.setShortAtOffset(structSegmt, 2, (short)2222);
			MemoryAccess.setShortAtOffset(structSegmt, 4, (short)3333);
			MemoryAccess.setShortAtOffset(structSegmt, 6, (short)4444);
			MemoryAccess.setShortAtOffset(structSegmt, 8, (short)5555);

			short result = (short)mh.invokeExact((short)6666, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 23331);
		}
	}

	@Test
	public void test_add2ShortStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2ShortStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2ShortStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt1, (short)356);
			shortHandle2.set(structSegmt1, (short)345);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt2, (short)378);
			shortHandle2.set(structSegmt2, (short)367);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)734);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)712);
		}
	}

	@Test
	public void test_add2ShortStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2ShortStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2ShortStructs_returnStructPointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt1, (short)356);
			shortHandle2.set(structSegmt1, (short)345);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt2, (short)378);
			shortHandle2.set(structSegmt2, (short)367);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(structLayout.byteSize(), scope);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)734);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)712);
		}
	}

	@Test
	public void test_add3ShortStructs_returnStructByUpcallMH() throws Throwable  {
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"),
				C_SHORT.withName("elem3")) : MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"),
				C_SHORT.withName("elem3"), MemoryLayout.paddingLayout(16));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));
		VarHandle shortHandle3 = structLayout.varHandle(short.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add3ShortStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3ShortStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt1, (short)325);
			shortHandle2.set(structSegmt1, (short)326);
			shortHandle3.set(structSegmt1, (short)327);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt2, (short)334);
			shortHandle2.set(structSegmt2, (short)335);
			shortHandle3.set(structSegmt2, (short)336);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)659);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)661);
			Assert.assertEquals((short)shortHandle3.get(resultSegmt), (short)663);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStruct,
					FunctionDescriptor.of(C_INT, C_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			intHandle1.set(structSegmt, 1122334);
			intHandle2.set(structSegmt, 1234567);

			int result = (int)mh.invokeExact(2244668, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 4601569);
		}
	}

	@Test
	public void test_addIntAnd5IntsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"),
				C_INT.withName("elem3"), C_INT.withName("elem4"), C_INT.withName("elem5"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));
		VarHandle intHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));
		VarHandle intHandle4 = structLayout.varHandle(int.class, PathElement.groupElement("elem4"));
		VarHandle intHandle5 = structLayout.varHandle(int.class, PathElement.groupElement("elem5"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAnd5IntsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAnd5IntsFromStruct,
					FunctionDescriptor.of(C_INT, C_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			intHandle1.set(structSegmt, 1111111);
			intHandle2.set(structSegmt, 2222222);
			intHandle3.set(structSegmt, 3333333);
			intHandle4.set(structSegmt, 2222222);
			intHandle5.set(structSegmt, 1111111);

			int result = (int)mh.invokeExact(4444444, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 14444443);
		}
	}

	@Test
	public void test_addIntFromPointerAndIntsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntFromPointerAndIntsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntFromPointerAndIntsFromStruct,
					FunctionDescriptor.of(C_INT, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment intSegmt = allocator.allocate(C_INT);
			MemoryAccess.setInt(intSegmt, 7654321);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			intHandle1.set(structSegmt, 1234567);
			intHandle2.set(structSegmt, 2468024);

			int result = (int)mh.invokeExact(intSegmt.address(), structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 11356912);
		}
	}

	@Test
	public void test_addIntFromPointerAndIntsFromStruct_returnIntPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntFromPointerAndIntsFromStruct_returnIntPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntFromPointerAndIntsFromStruct_returnIntPointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment intSegmt = allocator.allocate(C_INT);
			MemoryAccess.setInt(intSegmt, 1122333);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			intHandle1.set(structSegmt, 4455666);
			intHandle2.set(structSegmt, 7788999);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(intSegmt.address(), structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(C_INT.byteSize(), scope);
			VarHandle intHandle = MemoryHandles.varHandle(int.class, ByteOrder.nativeOrder());
			int result = (int)intHandle.get(resultSegmt, 0);
			Assert.assertEquals(result, 13366998);
			Assert.assertEquals(resultSegmt.address().toRawLongValue(), intSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addIntAndIntsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructPointer,
					FunctionDescriptor.of(C_INT, C_INT, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			intHandle1.set(structSegmt, 11121314);
			intHandle2.set(structSegmt, 15161718);

			int result = (int)mh.invokeExact(19202122, structSegmt.address(), upcallFuncAddr);
			Assert.assertEquals(result, 45485154);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), C_INT.withName("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromNestedStruct,
					FunctionDescriptor.of(C_INT, C_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt, 0, 21222324);
			MemoryAccess.setIntAtOffset(structSegmt, 4, 25262728);
			MemoryAccess.setIntAtOffset(structSegmt, 8, 29303132);

			int result = (int)mh.invokeExact(33343536, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 109131720);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(C_INT, C_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt, 0, 21222324);
			MemoryAccess.setIntAtOffset(structSegmt, 4, 25262728);
			MemoryAccess.setIntAtOffset(structSegmt, 8, 29303132);

			int result = (int)mh.invokeExact(33343536, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 109131720);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedIntArrayByUpcallMH() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, C_INT);
		GroupLayout structLayout = MemoryLayout.structLayout(intArray.withName("array_elem1"), C_INT.withName("elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructWithNestedIntArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructWithNestedIntArray,
					FunctionDescriptor.of(C_INT, C_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt, 0, 1111111);
			MemoryAccess.setIntAtOffset(structSegmt, 4, 2222222);
			MemoryAccess.setIntAtOffset(structSegmt, 8, 3333333);

			int result = (int)mh.invokeExact(4444444, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 11111110);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedIntArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout intArray = MemoryLayout.sequenceLayout(2, C_INT);
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), intArray.withName("array_elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructWithNestedIntArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructWithNestedIntArray_reverseOrder,
					FunctionDescriptor.of(C_INT, C_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt, 0, 1111111);
			MemoryAccess.setIntAtOffset(structSegmt, 4, 2222222);
			MemoryAccess.setIntAtOffset(structSegmt, 8, 3333333);

			int result = (int)mh.invokeExact(4444444, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 11111110);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout intStruct = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, intStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_INT.withName("elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructWithNestedStructArray,
					FunctionDescriptor.of(C_INT, C_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt, 0, 1111111);
			MemoryAccess.setIntAtOffset(structSegmt, 4, 2222222);
			MemoryAccess.setIntAtOffset(structSegmt, 8, 3333333);
			MemoryAccess.setIntAtOffset(structSegmt, 12, 4444444);
			MemoryAccess.setIntAtOffset(structSegmt, 16, 5555555);

			int result = (int)mh.invokeExact(6666666, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 23333331);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout intStruct = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, intStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), structArray.withName("struct_array_elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(C_INT, C_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt, 0, 1111111);
			MemoryAccess.setIntAtOffset(structSegmt, 4, 2222222);
			MemoryAccess.setIntAtOffset(structSegmt, 8, 3333333);
			MemoryAccess.setIntAtOffset(structSegmt, 12, 4444444);
			MemoryAccess.setIntAtOffset(structSegmt, 16, 5555555);

			int result = (int)mh.invokeExact(6666666, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 23333331);
		}
	}

	@Test
	public void test_add2IntStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(intHandle1.get(resultSegmt), 110224466);
			Assert.assertEquals(intHandle2.get(resultSegmt), 89113354);
		}
	}

	@Test
	public void test_add2IntStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStructPointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(structLayout.byteSize(), scope);
			Assert.assertEquals(intHandle1.get(resultSegmt), 110224466);
			Assert.assertEquals(intHandle2.get(resultSegmt), 89113354);
		}
	}

	@Test
	public void test_add3IntStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"), C_INT.withName("elem3"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));
		VarHandle intHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add3IntStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3IntStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			intHandle3.set(structSegmt1, 99001122);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 33445566);
			intHandle2.set(structSegmt2, 77889900);
			intHandle3.set(structSegmt2, 44332211);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(intHandle1.get(resultSegmt), 44668910);
			Assert.assertEquals(intHandle2.get(resultSegmt), 133557688);
			Assert.assertEquals(intHandle3.get(resultSegmt), 143333333);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStruct,
					FunctionDescriptor.of(longLayout, longLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			longHandle1.set(structSegmt, 1234567890L);
			longHandle2.set(structSegmt, 9876543210L);

			long result = (long)mh.invokeExact(2468024680L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 13579135780L);
		}
	}

	@Test
	public void test_addLongFromPointerAndLongsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongFromPointerAndLongsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongFromPointerAndLongsFromStruct,
					FunctionDescriptor.of(longLayout, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment longSegmt = allocator.allocate(longLayout);
			MemoryAccess.setLong(longSegmt, 1111111111L);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			longHandle1.set(structSegmt, 3333333333L);
			longHandle2.set(structSegmt, 5555555555L);

			long result = (long)mh.invokeExact(longSegmt.address(), structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 9999999999L);
		}
	}

	@Test
	public void test_addLongFromPointerAndLongsFromStruct_returnLongPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongFromPointerAndLongsFromStruct_returnLongPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongFromPointerAndLongsFromStruct_returnLongPointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment longSegmt = allocator.allocate(longLayout);
			MemoryAccess.setLong(longSegmt, 1122334455L);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			longHandle1.set(structSegmt, 6677889900L);
			longHandle2.set(structSegmt, 1234567890L);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(longSegmt.address(), structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(longLayout.byteSize(), scope);
			VarHandle longHandle = MemoryHandles.varHandle(long.class, ByteOrder.nativeOrder());
			long result = (long)longHandle.get(resultSegmt, 0);
			Assert.assertEquals(result, 9034792245L);
			Assert.assertEquals(resultSegmt.address().toRawLongValue(), longSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addLongAndLongsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(long.class, long.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructPointer,
					FunctionDescriptor.of(longLayout, longLayout, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			longHandle1.set(structSegmt, 224466880022L);
			longHandle2.set(structSegmt, 446688002244L);

			long result = (long)mh.invokeExact(668800224466L, structSegmt.address(), upcallFuncAddr);
			Assert.assertEquals(result, 1339955106732L);
		}
	}

	@Test
	public void test_addLongAndLongsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromNestedStruct,
					FunctionDescriptor.of(longLayout, longLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setLongAtOffset(structSegmt, 0, 135791357913L);
			MemoryAccess.setLongAtOffset(structSegmt, 8, 246802468024L);
			MemoryAccess.setLongAtOffset(structSegmt, 16,112233445566L);

			long result = (long)mh.invokeExact(778899001122L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1273726272625L);
		}
	}

	@Test
	public void test_addLongAndLongsFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(longLayout, longLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setLongAtOffset(structSegmt, 0, 135791357913L);
			MemoryAccess.setLongAtOffset(structSegmt, 8, 246802468024L);
			MemoryAccess.setLongAtOffset(structSegmt, 16,112233445566L);

			long result = (long)mh.invokeExact(778899001122L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1273726272625L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedLongArrayByUpcallMH() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, longLayout);
		GroupLayout structLayout = MemoryLayout.structLayout(longArray.withName("array_elem1"), longLayout.withName("elem2"));
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromStructWithNestedLongArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructWithNestedLongArray,
					FunctionDescriptor.of(longLayout, longLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setLongAtOffset(structSegmt, 0, 11111111111L);
			MemoryAccess.setLongAtOffset(structSegmt, 8, 22222222222L);
			MemoryAccess.setLongAtOffset(structSegmt, 16, 33333333333L);

			long result = (long)mh.invokeExact(44444444444L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111111111110L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedLongArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout longArray = MemoryLayout.sequenceLayout(2, longLayout);
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longArray.withName("array_elem2"));
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromStructWithNestedLongArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructWithNestedLongArray_reverseOrder,
					FunctionDescriptor.of(longLayout, longLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setLongAtOffset(structSegmt, 0, 11111111111L);
			MemoryAccess.setLongAtOffset(structSegmt, 8, 22222222222L);
			MemoryAccess.setLongAtOffset(structSegmt, 16, 33333333333L);

			long result = (long)mh.invokeExact(44444444444L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111111111110L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout longStruct = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, longStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), longLayout.withName("elem2"));
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructWithNestedStructArray,
					FunctionDescriptor.of(longLayout, longLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setLongAtOffset(structSegmt, 0, 11111111111L);
			MemoryAccess.setLongAtOffset(structSegmt, 8, 22222222222L);
			MemoryAccess.setLongAtOffset(structSegmt, 16, 33333333333L);
			MemoryAccess.setLongAtOffset(structSegmt, 24, 44444444444L);
			MemoryAccess.setLongAtOffset(structSegmt, 32, 55555555555L);

			long result = (long)mh.invokeExact(66666666666L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 233333333331L);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout longStruct = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, longStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), structArray.withName("struct_array_elem2"));
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(longLayout, longLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setLongAtOffset(structSegmt, 0, 11111111111L);
			MemoryAccess.setLongAtOffset(structSegmt, 8, 22222222222L);
			MemoryAccess.setLongAtOffset(structSegmt, 16, 33333333333L);
			MemoryAccess.setLongAtOffset(structSegmt, 24, 44444444444L);
			MemoryAccess.setLongAtOffset(structSegmt, 32, 55555555555L);

			long result = (long)mh.invokeExact(66666666666L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 233333333331L);
		}
	}

	@Test
	public void test_add2LongStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2LongStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2LongStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 224466880022L);
			longHandle2.set(structSegmt2, 113355779911L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(longHandle1.get(resultSegmt), 1212121202009L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 236812569034L);
		}
	}

	@Test
	public void test_add2LongStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2LongStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2LongStructs_returnStructPointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 5566778899L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 9900112233L);
			longHandle2.set(structSegmt2, 3344556677L);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(structLayout.byteSize(), scope);
			Assert.assertEquals(longHandle1.get(resultSegmt), 11022446688L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 8911335576L);
		}
	}

	@Test
	public void test_add3LongStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"), longLayout.withName("elem3"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));
		VarHandle longHandle3 = structLayout.varHandle(long.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add3LongStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3LongStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			longHandle3.set(structSegmt1, 112233445566L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 224466880022L);
			longHandle2.set(structSegmt2, 113355779911L);
			longHandle3.set(structSegmt2, 778899001122L);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(longHandle1.get(resultSegmt), 1212121202009L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 236812569034L);
			Assert.assertEquals(longHandle3.get(resultSegmt), 891132446688L);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStruct,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt, 8.12F);
			floatHandle2.set(structSegmt, 9.24F);

			float result = (float)mh.invokeExact(6.56F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 23.92F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAnd5FloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"),
				C_FLOAT.withName("elem3"), C_FLOAT.withName("elem4"), C_FLOAT.withName("elem5"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle floatHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));
		VarHandle floatHandle4 = structLayout.varHandle(float.class, PathElement.groupElement("elem4"));
		VarHandle floatHandle5 = structLayout.varHandle(float.class, PathElement.groupElement("elem5"));

		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAnd5FloatsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAnd5FloatsFromStruct,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt, 1.01F);
			floatHandle2.set(structSegmt, 1.02F);
			floatHandle3.set(structSegmt, 1.03F);
			floatHandle4.set(structSegmt, 1.04F);
			floatHandle5.set(structSegmt, 1.05F);

			float result = (float)mh.invokeExact(1.06F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 6.21F, 0.01F);
		}
	}

	@Test
	public void test_addFloatFromPointerAndFloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(float.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatFromPointerAndFloatsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatFromPointerAndFloatsFromStruct,
					FunctionDescriptor.of(C_FLOAT, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment floatSegmt = allocator.allocate(C_FLOAT);
			MemoryAccess.setFloat(floatSegmt, 12.12F);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt, 18.23F);
			floatHandle2.set(structSegmt, 19.34F);

			float result = (float)mh.invokeExact(floatSegmt.address(), structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 49.69F, 0.01F);
		}
	}

	@Test
	public void test_addFloatFromPointerAndFloatsFromStruct_returnFloatPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatFromPointerAndFloatsFromStruct_returnFloatPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatFromPointerAndFloatsFromStruct_returnFloatPointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment floatSegmt = allocator.allocate(C_FLOAT);
			MemoryAccess.setFloat(floatSegmt, 12.12F);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt, 18.23F);
			floatHandle2.set(structSegmt, 19.34F);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(floatSegmt.address(), structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(C_FLOAT.byteSize(), scope);
			VarHandle floatHandle = MemoryHandles.varHandle(float.class, ByteOrder.nativeOrder());
			float result = (float)floatHandle.get(resultSegmt, 0);
			Assert.assertEquals(result, 49.69F, 0.01F);
			Assert.assertEquals(resultSegmt.address().toRawLongValue(), floatSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(float.class, float.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructPointer,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt, 35.11F);
			floatHandle2.set(structSegmt, 46.22F);

			float result = (float)mh.invokeExact(79.33F, structSegmt.address(), upcallFuncAddr);
			Assert.assertEquals(result, 160.66F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), C_FLOAT.withName("elem2"));
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromNestedStruct,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt, 0, 31.22F);
			MemoryAccess.setFloatAtOffset(structSegmt, 4, 33.44F);
			MemoryAccess.setFloatAtOffset(structSegmt, 8, 35.66F);

			float result = (float)mh.invokeExact(37.88F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 138.2F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt, 0, 31.22F);
			MemoryAccess.setFloatAtOffset(structSegmt, 4, 33.44F);
			MemoryAccess.setFloatAtOffset(structSegmt, 8, 35.66F);

			float result = (float)mh.invokeExact(37.88F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 138.2F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedFloatArrayByUpcallMH() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, C_FLOAT);
		GroupLayout structLayout = MemoryLayout.structLayout(floatArray.withName("array_elem1"), C_FLOAT.withName("elem2"));
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromStructWithNestedFloatArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructWithNestedFloatArray,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt, 0, 111.11F);
			MemoryAccess.setFloatAtOffset(structSegmt, 4, 222.22F);
			MemoryAccess.setFloatAtOffset(structSegmt, 8, 333.33F);

			float result = (float)mh.invokeExact(444.44F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1111.1F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.sequenceLayout(2, C_FLOAT);
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), floatArray.withName("array_elem2"));
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt, 0, 111.11F);
			MemoryAccess.setFloatAtOffset(structSegmt, 4, 222.22F);
			MemoryAccess.setFloatAtOffset(structSegmt, 8, 333.33F);

			float result = (float)mh.invokeExact(444.44F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1111.1F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout floatStruct = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, floatStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_FLOAT.withName("elem2"));
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructWithNestedStructArray,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt, 0, 111.11F);
			MemoryAccess.setFloatAtOffset(structSegmt, 4, 222.22F);
			MemoryAccess.setFloatAtOffset(structSegmt, 8, 333.33F);
			MemoryAccess.setFloatAtOffset(structSegmt, 12, 444.44F);
			MemoryAccess.setFloatAtOffset(structSegmt, 16, 555.55F);

			float result = (float)mh.invokeExact(666.66F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 2333.31F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout floatStruct = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, floatStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), structArray.withName("struct_array_elem2"));
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt, 0, 111.11F);
			MemoryAccess.setFloatAtOffset(structSegmt, 4, 222.22F);
			MemoryAccess.setFloatAtOffset(structSegmt, 8, 333.33F);
			MemoryAccess.setFloatAtOffset(structSegmt, 12, 444.44F);
			MemoryAccess.setFloatAtOffset(structSegmt, 16, 555.55F);

			float result = (float)mh.invokeExact(666.66F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 2333.31F, 0.01F);
		}
	}

	@Test
	public void test_add3FloatStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"),  C_FLOAT.withName("elem3"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle floatHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add3FloatStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3FloatStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			floatHandle3.set(structSegmt1, 45.67F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt2, 24.34F);
			floatHandle2.set(structSegmt2, 13.45F);
			floatHandle3.set(structSegmt2, 69.72F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 49.46F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 24.68F, 0.01F);
			Assert.assertEquals((float)floatHandle3.get(resultSegmt), 115.39, 0.01F);
		}
	}

	@Test
	public void test_add2FloatStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2FloatStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2FloatStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt2, 24.34F);
			floatHandle2.set(structSegmt2, 13.45F);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 49.46F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 24.68F, 0.01F);
		}
	}

	@Test
	public void test_add2FloatStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2FloatStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2FloatStructs_returnStructPointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt2, 24.34F);
			floatHandle2.set(structSegmt2, 13.45F);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(structLayout.byteSize(), scope);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 49.46F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 24.68F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt, 2228.111D);
			doubleHandle2.set(structSegmt, 2229.221D);

			double result = (double)mh.invokeExact(3336.333D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 7793.665D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleFromPointerAndDoublesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleFromPointerAndDoublesFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleFromPointerAndDoublesFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment doubleSegmt = allocator.allocate(C_DOUBLE);
			MemoryAccess.setDouble(doubleSegmt, 112.123D);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt, 118.456D);
			doubleHandle2.set(structSegmt, 119.789D);

			double result = (double)mh.invokeExact(doubleSegmt.address(), structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 350.368D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleFromPointerAndDoublesFromStruct_returnDoublePointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleFromPointerAndDoublesFromStruct_returnDoublePointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment doubleSegmt = allocator.allocate(C_DOUBLE);
			MemoryAccess.setDouble(doubleSegmt, 212.123D);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt, 218.456D);
			doubleHandle2.set(structSegmt, 219.789D);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(doubleSegmt.address(), structSegmt, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(C_DOUBLE.byteSize(), scope);
			VarHandle doubleHandle = MemoryHandles.varHandle(double.class, ByteOrder.nativeOrder());
			double result = (double)doubleHandle.get(resultSegmt, 0);
			Assert.assertEquals(result, 650.368D, 0.001D);
			Assert.assertEquals(resultSegmt.address().toRawLongValue(), doubleSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemoryAddress.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_POINTER, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructPointer,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_POINTER), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt, 22.111D);
			doubleHandle2.set(structSegmt, 44.222D);

			double result = (double)mh.invokeExact(66.333D, structSegmt.address(), upcallFuncAddr);
			Assert.assertEquals(result, 132.666D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), C_DOUBLE.withName("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromNestedStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setDoubleAtOffset(structSegmt, 0, 31.789D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 8, 33.456D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 16, 35.123D);

			double result = (double)mh.invokeExact(37.864D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 138.232D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedStruct_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), nestedStructLayout.withName("struct_elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setDoubleAtOffset(structSegmt, 0, 31.789D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 8, 33.456D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 16, 35.123D);

			double result = (double)mh.invokeExact(37.864D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 138.232D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedDoubleArrayByUpcallMH() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, C_DOUBLE);
		GroupLayout structLayout = MemoryLayout.structLayout(doubleArray.withName("array_elem1"), C_DOUBLE.withName("elem2"));
		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromStructWithNestedDoubleArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructWithNestedDoubleArray,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setDoubleAtOffset(structSegmt, 0, 111.111D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 8, 222.222D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 16, 333.333D);

			double result = (double)mh.invokeExact(444.444D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1111.11D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrderByUpcallMH() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.sequenceLayout(2, C_DOUBLE);
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), doubleArray.withName("array_elem2"));
		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setDoubleAtOffset(structSegmt, 0, 111.111D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 8, 222.222D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 16, 333.333D);

			double result = (double)mh.invokeExact(444.444D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1111.11D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedStructArrayByUpcallMH() throws Throwable {
		GroupLayout doubleStruct = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, doubleStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(structArray.withName("struct_array_elem1"), C_DOUBLE.withName("elem2"));
		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructWithNestedStructArray,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setDoubleAtOffset(structSegmt, 0, 111.111D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 8, 222.222D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 16, 333.333D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 24, 444.444D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 32, 555.555D);

			double result = (double)mh.invokeExact(666.666D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 2333.331D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrderByUpcallMH() throws Throwable {
		GroupLayout doubleStruct = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.sequenceLayout(2, doubleStruct);
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), structArray.withName("struct_array_elem2"));
		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setDoubleAtOffset(structSegmt, 0, 111.111D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 8, 222.222D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 16, 333.333D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 24, 444.444D);
			MemoryAccess.setDoubleAtOffset(structSegmt, 32, 555.555D);

			double result = (double)mh.invokeExact(666.666D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 2333.331D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2DoubleStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2DoubleStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 33.444D);
			doubleHandle2.set(structSegmt2, 44.555D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2,  upcallFuncAddr);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 44.666D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2DoubleStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2DoubleStructs_returnStructPointer,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 33.444D);
			doubleHandle2.set(structSegmt2, 44.555D);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.asSegment(structLayout.byteSize(), scope);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 44.666D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_add3DoubleStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"), C_DOUBLE.withName("elem3"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));
		VarHandle doubleHandle3 = structLayout.varHandle(double.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add3DoubleStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3DoubleStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			doubleHandle3.set(structSegmt1, 33.123D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 33.444D);
			doubleHandle2.set(structSegmt2, 44.555D);
			doubleHandle3.set(structSegmt2, 55.456D);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 44.666D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 66.888D, 0.001D);
			Assert.assertEquals((double)doubleHandle3.get(resultSegmt), 88.579D, 0.001D);
		}
	}

	@Test
	public void test_addNegBytesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addNegBytesFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addNegBytesFromStruct,
					FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout, C_CHAR, C_CHAR), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)-8);
			byteHandle2.set(structSegmt, (byte)-9);

			byte result = (byte)mh.invoke((byte)-6, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, (byte)-40);
		}
	}

	@Test
	public void test_addNegShortsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addNegShortsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addNegShortsFromStruct,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_SHORT, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)-888);
			shortHandle2.set(structSegmt, (short)-999);

			short result = (short)mh.invoke((short)-777, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, (short)-4551);
		}
	}
}
