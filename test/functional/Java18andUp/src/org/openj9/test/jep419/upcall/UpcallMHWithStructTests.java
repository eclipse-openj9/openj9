/*******************************************************************************
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package org.openj9.test.jep419.upcall;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.VarHandle;

import jdk.incubator.foreign.CLinker;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemoryLayout.PathElement;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.NativeSymbol;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.SequenceLayout;
import jdk.incubator.foreign.SymbolLookup;
import jdk.incubator.foreign.ValueLayout;
import static jdk.incubator.foreign.ValueLayout.*;

/**
 * Test cases for JEP 419: Foreign Linker API (Second Incubator) for argument/return struct in upcall.
 *
 * Note: the padding elements in the struct are only required by RI or VarHandle (accessing the
 * data address) while they are totally ignored in OpenJ9 given the padding/alignment are
 * computed by libffi automatically in native.
 */
@Test(groups = { "level.sanity" })
public class UpcallMHWithStructTests {
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static String arch = System.getProperty("os.arch").toLowerCase();
	/* The padding of struct is not required on Linux/s390x and Windows/x64 */
	private static boolean isStructPaddingNotRequired = osName.contains("win") && (arch.equals("amd64") || arch.equals("x86_64"))
														|| osName.contains("linux") && arch.equals("s390x");
	private static CLinker clinker = CLinker.systemCLinker();

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromStructWithXorByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt, false);
			boolHandle2.set(structSegmt, true);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAnd20BoolsFromStructWithXorByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAnd20BoolsFromStructWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolFromPointerAndBoolsFromStructWithXorByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolFromPointerAndBoolsFromStructWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, ADDRESS, structLayout), scope);
			MemorySegment boolSegmt = MemorySegment.allocateNative(JAVA_BOOLEAN, scope);
			boolSegmt.set(JAVA_BOOLEAN, 0, true);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt, false);
			boolHandle2.set(structSegmt, true);

			boolean result = (boolean)mh.invoke(boolSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, false);
		}
	}

	@Test
	public void test_addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN, JAVA_BOOLEAN);
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			MemorySegment boolSegmt = MemorySegment.allocateNative(JAVA_BOOLEAN, scope);
			boolSegmt.set(JAVA_BOOLEAN, 0, false);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			structSegmt.set(JAVA_BOOLEAN, 0, false);
			structSegmt.set(JAVA_BOOLEAN, 1, true);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(boolSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_BOOLEAN, 0), true);
			Assert.assertEquals(resultAddr.toRawLongValue(), boolSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addBoolAndBoolsFromStructPointerWithXorByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromStructPointerWithXorByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructPointerWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt, true);
			boolHandle2.set(structSegmt, false);

			boolean result = (boolean)mh.invoke(false, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addBoolAndBoolsFromNestedStructWithXorByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				JAVA_BOOLEAN.withName("elem2")) : MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				JAVA_BOOLEAN.withName("elem2"), MemoryLayout.paddingLayout(8));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromNestedStructWithXorByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromNestedStructWithXor,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"),
				nestedStructLayout.withName("struct_elem2")) : MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"),
				nestedStructLayout.withName("struct_elem2"), MemoryLayout.paddingLayout(8));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromNestedStructWithXor_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromNestedStructWithXor_reverseOrder,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = MemoryLayout.structLayout(boolArray.withName("array_elem1"),
				JAVA_BOOLEAN.withName("elem2"), MemoryLayout.paddingLayout(8));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromStructWithNestedBoolArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithNestedBoolArray,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"),
				boolArray.withName("array_elem2"), MemoryLayout.paddingLayout(8));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(structArray.withName("struct_array_elem1"),
				JAVA_BOOLEAN.withName("elem2")) : MemoryLayout.structLayout(structArray.withName("struct_array_elem1"),
				JAVA_BOOLEAN.withName("elem2"), MemoryLayout.paddingLayout(JAVA_BOOLEAN.bitSize() * 3));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"),
				structArray.withName("struct_array_elem2")) : MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"),
				structArray.withName("struct_array_elem2"), MemoryLayout.paddingLayout(JAVA_BOOLEAN.bitSize() * 3));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addBoolAndBoolsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2BoolStructsWithXor_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2BoolStructsWithXor_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt1, true);
			boolHandle2.set(structSegmt1, false);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt2, true);
			boolHandle2.set(structSegmt2, true);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(boolHandle1.get(resultSegmt), false);
			Assert.assertEquals(boolHandle2.get(resultSegmt), true);
		}
	}

	@Test
	public void test_add2BoolStructsWithXor_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2BoolStructsWithXor_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2BoolStructsWithXor_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt1, true);
			boolHandle2.set(structSegmt1, false);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt2, true);
			boolHandle2.set(structSegmt2, true);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_BOOLEAN, 0), false);
			Assert.assertEquals(resultAddr.get(JAVA_BOOLEAN, 1), true);
		}
	}

	@Test
	public void test_add3BoolStructsWithXor_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"),
				JAVA_BOOLEAN.withName("elem3")) : MemoryLayout.structLayout(JAVA_BOOLEAN.withName("elem1"), JAVA_BOOLEAN.withName("elem2"),
				JAVA_BOOLEAN.withName("elem3"), MemoryLayout.paddingLayout(8));
		VarHandle boolHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle boolHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add3BoolStructsWithXor_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3BoolStructsWithXor_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt1, true);
			boolHandle2.set(structSegmt1, false);
			boolHandle3.set(structSegmt1, true);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			boolHandle1.set(structSegmt2, true);
			boolHandle2.set(structSegmt2, true);
			boolHandle3.set(structSegmt2, false);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(boolHandle1.get(resultSegmt), false);
			Assert.assertEquals(boolHandle2.get(resultSegmt), true);
			Assert.assertEquals(boolHandle3.get(resultSegmt), true);
		}
	}

	@Test
	public void test_addByteAndBytesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStruct,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)8);
			byteHandle2.set(structSegmt, (byte)9);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAnd20BytesFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAnd20BytesFromStruct,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteFromPointerAndBytesFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteFromPointerAndBytesFromStruct,
					FunctionDescriptor.of(JAVA_BYTE, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment byteSegmt = allocator.allocate(JAVA_BYTE, (byte)12);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)18);
			byteHandle2.set(structSegmt, (byte)19);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteFromPointerAndBytesFromStruct_returnBytePointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteFromPointerAndBytesFromStruct_returnBytePointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment byteSegmt = allocator.allocate(JAVA_BYTE, (byte)12);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)14);
			byteHandle2.set(structSegmt, (byte)16);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(byteSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_BYTE, 0), 42);
			Assert.assertEquals(resultAddr.toRawLongValue(), byteSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addByteAndBytesFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructPointer,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt, (byte)11);
			byteHandle2.set(structSegmt, (byte)12);
			byte result = (byte)mh.invoke((byte)13, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 36);
		}
	}

	@Test
	public void test_addByteAndBytesFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				JAVA_BYTE.withName("elem2")) : MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				JAVA_BYTE.withName("elem2"), MemoryLayout.paddingLayout(8));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromNestedStruct,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"),
				nestedStructLayout.withName("struct_elem2")) : MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"),
				nestedStructLayout.withName("struct_elem2"), MemoryLayout.paddingLayout(8));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(byteArray.withName("array_elem1"),
				JAVA_BYTE.withName("elem2")) : MemoryLayout.structLayout(byteArray.withName("array_elem1"),
				JAVA_BYTE.withName("elem2"), MemoryLayout.paddingLayout(8));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromStructWithNestedByteArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructWithNestedByteArray,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"),
				byteArray.withName("array_elem2")) : MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"),
				byteArray.withName("array_elem2"), MemoryLayout.paddingLayout(8));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromStructWithNestedByteArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructWithNestedByteArray_reverseOrder,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(structArray.withName("struct_array_elem1"),
				JAVA_BYTE.withName("elem2")) : MemoryLayout.structLayout(structArray.withName("struct_array_elem1"),
				JAVA_BYTE.withName("elem2"), MemoryLayout.paddingLayout(JAVA_BYTE.bitSize() * 3));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"),
				structArray.withName("struct_array_elem2")) : MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"),
				structArray.withName("struct_array_elem2"), MemoryLayout.paddingLayout(JAVA_BYTE.bitSize() * 3));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addByteAndBytesFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndBytesFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add1ByteStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add1ByteStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)25);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)24);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
		}
	}

	@Test
	public void test_add2ByteStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2ByteStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2ByteStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)24);
			byteHandle2.set(structSegmt2, (byte)13);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)24);
		}
	}

	@Test
	public void test_add2ByteStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2ByteStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2ByteStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)24);
			byteHandle2.set(structSegmt2, (byte)13);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_BYTE, 0), 49);
			Assert.assertEquals(resultAddr.get(JAVA_BYTE, 1), 24);
		}
	}

	@Test
	public void test_add3ByteStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"),
				JAVA_BYTE.withName("elem3")) : MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"),
				JAVA_BYTE.withName("elem3"), MemoryLayout.paddingLayout(8));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle byteHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add3ByteStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3ByteStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)25);
			byteHandle2.set(structSegmt1, (byte)11);
			byteHandle3.set(structSegmt1, (byte)12);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)24);
			byteHandle2.set(structSegmt2, (byte)13);
			byteHandle3.set(structSegmt2, (byte)16);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
			Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)24);
			Assert.assertEquals((byte)byteHandle3.get(resultSegmt), (byte)28);
		}
	}

	@Test
	public void test_addCharAndCharsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStruct,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			charHandle1.set(structSegmt, 'A');
			charHandle2.set(structSegmt, 'B');

			char result = (char)mh.invoke('C', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'D');
		}
	}

	@Test
	public void test_addCharAnd10CharsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR, JAVA_CHAR, JAVA_CHAR, JAVA_CHAR,
				JAVA_CHAR, JAVA_CHAR, JAVA_CHAR, JAVA_CHAR, JAVA_CHAR, JAVA_CHAR);

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addCharAnd10CharsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAnd10CharsFromStruct,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addCharFromPointerAndCharsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharFromPointerAndCharsFromStruct,
					FunctionDescriptor.of(JAVA_CHAR, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment charSegmt = allocator.allocate(JAVA_CHAR, 'D');
			MemorySegment structSegmt = allocator.allocate(structLayout);
			charHandle1.set(structSegmt, 'E');
			charHandle2.set(structSegmt, 'F');

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addCharFromPointerAndCharsFromStruct_returnCharPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharFromPointerAndCharsFromStruct_returnCharPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment charSegmt = allocator.allocate(JAVA_CHAR, 'D');
			MemorySegment structSegmt = allocator.allocate(structLayout);
			charHandle1.set(structSegmt, 'E');
			charHandle2.set(structSegmt, 'F');

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(charSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_CHAR, 0), 'M');
			Assert.assertEquals(resultAddr.toRawLongValue(), charSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addCharAndCharsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructPointer,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			charHandle1.set(structSegmt, 'H');
			charHandle2.set(structSegmt, 'I');

			char result = (char)mh.invoke('G', structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 'V');
		}
	}

	@Test
	public void test_addCharAndCharsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				JAVA_CHAR.withName("elem2")) : MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				JAVA_CHAR.withName("elem2"), MemoryLayout.paddingLayout(16));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromNestedStruct,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"),
				nestedStructLayout.withName("struct_elem2")) : MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"),
						nestedStructLayout.withName("struct_elem2"), MemoryLayout.paddingLayout(16));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(charArray.withName("array_elem1"),
				JAVA_CHAR.withName("elem2")) : MemoryLayout.structLayout(charArray.withName("array_elem1"),
				JAVA_CHAR.withName("elem2"), MemoryLayout.paddingLayout(16));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromStructWithNestedCharArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructWithNestedCharArray,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"),
				charArray.withName("array_elem2")) : MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"),
				charArray.withName("array_elem2"), MemoryLayout.paddingLayout(16));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromStructWithNestedCharArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructWithNestedCharArray_reverseOrder,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"),
				structArray.withName("struct_array_elem2"), MemoryLayout.paddingLayout(16));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addCharAndCharsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addCharAndCharsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2CharStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2CharStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt2, 'C');
			charHandle2.set(structSegmt2, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'C');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'E');
		}
	}

	@Test
	public void test_add2CharStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2CharStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2CharStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt2, 'C');
			charHandle2.set(structSegmt2, 'D');

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_CHAR, 0), 'C');
			Assert.assertEquals(resultAddr.get(JAVA_CHAR, 2), 'E');
		}
	}

	@Test
	public void test_add3CharStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"),
				JAVA_CHAR.withName("elem3")) : MemoryLayout.structLayout(JAVA_CHAR.withName("elem1"), JAVA_CHAR.withName("elem2"),
				JAVA_CHAR.withName("elem3"), MemoryLayout.paddingLayout(16));
		VarHandle charHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle charHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add3CharStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3CharStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt1, 'A');
			charHandle2.set(structSegmt1, 'B');
			charHandle3.set(structSegmt1, 'C');
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			charHandle1.set(structSegmt2, 'B');
			charHandle2.set(structSegmt2, 'C');
			charHandle3.set(structSegmt2, 'D');

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(charHandle1.get(resultSegmt), 'B');
			Assert.assertEquals(charHandle2.get(resultSegmt), 'D');
			Assert.assertEquals(charHandle3.get(resultSegmt), 'F');
		}
	}

	@Test
	public void test_addShortAndShortsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStruct,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)888);
			shortHandle2.set(structSegmt, (short)999);

			short result = (short)mh.invoke((short)777, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 2664);
		}
	}

	@Test
	public void test_addShortAnd10ShortsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT, JAVA_SHORT, JAVA_SHORT,
				JAVA_SHORT, JAVA_SHORT, JAVA_SHORT, JAVA_SHORT, JAVA_SHORT, JAVA_SHORT, JAVA_SHORT);

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAnd10ShortsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAnd10ShortsFromStruct,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortFromPointerAndShortsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortFromPointerAndShortsFromStruct,
					FunctionDescriptor.of(JAVA_SHORT, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment shortSegmt = allocator.allocate(JAVA_SHORT, (short)1112);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)1118);
			shortHandle2.set(structSegmt, (short)1119);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortFromPointerAndShortsFromStruct_returnShortPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortFromPointerAndShortsFromStruct_returnShortPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment shortSegmt = allocator.allocate(JAVA_SHORT, (short)1112);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)1118);
			shortHandle2.set(structSegmt, (short)1119);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(shortSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_SHORT, 0), 3349);
			Assert.assertEquals(resultAddr.toRawLongValue(), shortSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addShortAndShortsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructPointer,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt, (short)2222);
			shortHandle2.set(structSegmt, (short)4444);

			short result = (short)mh.invoke((short)6666, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 13332);
		}
	}

	@Test
	public void test_addShortAndShortsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				JAVA_SHORT.withName("elem2")) : MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"),
				JAVA_SHORT.withName("elem2"), MemoryLayout.paddingLayout(16));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromNestedStruct,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"),
				nestedStructLayout.withName("struct_elem2")) : MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"),
				nestedStructLayout.withName("struct_elem2"), MemoryLayout.paddingLayout(16));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(shortArray.withName("array_elem1"),
				JAVA_SHORT.withName("elem2")) : MemoryLayout.structLayout(shortArray.withName("array_elem1"),
				JAVA_SHORT.withName("elem2"), MemoryLayout.paddingLayout(16));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromStructWithNestedShortArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructWithNestedShortArray,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"),
				shortArray.withName("array_elem2")) : MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"),
				shortArray.withName("array_elem2"), MemoryLayout.paddingLayout(16));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromStructWithNestedShortArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructWithNestedShortArray_reverseOrder,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addShortAndShortsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2ShortStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2ShortStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt1, (short)356);
			shortHandle2.set(structSegmt1, (short)345);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt2, (short)378);
			shortHandle2.set(structSegmt2, (short)367);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)734);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)712);
		}
	}

	@Test
	public void test_add2ShortStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2ShortStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2ShortStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt1, (short)356);
			shortHandle2.set(structSegmt1, (short)345);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt2, (short)378);
			shortHandle2.set(structSegmt2, (short)367);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_SHORT, 0), 734);
			Assert.assertEquals(resultAddr.get(JAVA_SHORT, 2), 712);
		}
	}

	@Test
	public void test_add3ShortStructs_returnStructByUpcallMH() throws Throwable  {
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"),
				JAVA_SHORT.withName("elem3")) : MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"),
				JAVA_SHORT.withName("elem3"), MemoryLayout.paddingLayout(16));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle shortHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add3ShortStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3ShortStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt1, (short)325);
			shortHandle2.set(structSegmt1, (short)326);
			shortHandle3.set(structSegmt1, (short)327);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt2, (short)334);
			shortHandle2.set(structSegmt2, (short)335);
			shortHandle3.set(structSegmt2, (short)336);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)659);
			Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)661);
			Assert.assertEquals((short)shortHandle3.get(resultSegmt), (short)663);
		}
	}

	@Test
	public void test_addIntAndIntsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStruct,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			intHandle1.set(structSegmt, 1122334);
			intHandle2.set(structSegmt, 1234567);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAnd5IntsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAnd5IntsFromStruct,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			intHandle1.set(structSegmt, 1111111);
			intHandle2.set(structSegmt, 2222222);
			intHandle3.set(structSegmt, 3333333);
			intHandle4.set(structSegmt, 2222222);
			intHandle5.set(structSegmt, 1111111);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntFromPointerAndIntsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntFromPointerAndIntsFromStruct,
					FunctionDescriptor.of(JAVA_INT, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment intSegmt = allocator.allocate(JAVA_INT, 7654321);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			intHandle1.set(structSegmt, 1234567);
			intHandle2.set(structSegmt, 2468024);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntFromPointerAndIntsFromStruct_returnIntPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntFromPointerAndIntsFromStruct_returnIntPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment intSegmt = allocator.allocate(JAVA_INT, 1122333);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			intHandle1.set(structSegmt, 4455666);
			intHandle2.set(structSegmt, 7788999);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(intSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_INT, 0), 13366998);
			Assert.assertEquals(resultAddr.toRawLongValue(), intSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addIntAndIntsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructPointer,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			intHandle1.set(structSegmt, 11121314);
			intHandle2.set(structSegmt, 15161718);

			int result = (int)mh.invoke(19202122, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 45485154);
		}
	}

	@Test
	public void test_addIntAndIntsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_INT.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromNestedStruct,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructWithNestedIntArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructWithNestedIntArray,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructWithNestedIntArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructWithNestedIntArray_reverseOrder,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(intHandle1.get(resultSegmt), 110224466);
			Assert.assertEquals(intHandle2.get(resultSegmt), 89113354);
		}
	}

	@Test
	public void test_add2IntStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_INT, 0), 110224466);
			Assert.assertEquals(resultAddr.get(JAVA_INT, 4), 89113354);
		}
	}

	@Test
	public void test_add3IntStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"), JAVA_INT.withName("elem3"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle intHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add3IntStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3IntStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			intHandle3.set(structSegmt1, 99001122);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 33445566);
			intHandle2.set(structSegmt2, 77889900);
			intHandle3.set(structSegmt2, 44332211);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(intHandle1.get(resultSegmt), 44668910);
			Assert.assertEquals(intHandle2.get(resultSegmt), 133557688);
			Assert.assertEquals(intHandle3.get(resultSegmt), 143333333);
		}
	}

	@Test
	public void test_addLongAndLongsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStruct,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			longHandle1.set(structSegmt, 1234567890L);
			longHandle2.set(structSegmt, 9876543210L);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongFromPointerAndLongsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongFromPointerAndLongsFromStruct,
					FunctionDescriptor.of(JAVA_LONG, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment longSegmt = allocator.allocate(JAVA_LONG, 1111111111L);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			longHandle1.set(structSegmt, 3333333333L);
			longHandle2.set(structSegmt, 5555555555L);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongFromPointerAndLongsFromStruct_returnLongPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongFromPointerAndLongsFromStruct_returnLongPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment longSegmt = allocator.allocate(JAVA_LONG, 1122334455L);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			longHandle1.set(structSegmt, 6677889900L);
			longHandle2.set(structSegmt, 1234567890L);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(longSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_LONG, 0), 9034792245L);
			Assert.assertEquals(resultAddr.toRawLongValue(), longSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addLongAndLongsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructPointer,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			longHandle1.set(structSegmt, 224466880022L);
			longHandle2.set(structSegmt, 446688002244L);

			long result = (long)mh.invoke(668800224466L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1339955106732L);
		}
	}

	@Test
	public void test_addLongAndLongsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromNestedStruct,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromStructWithNestedLongArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructWithNestedLongArray,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromStructWithNestedLongArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructWithNestedLongArray_reverseOrder,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addLongAndLongsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2LongStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2LongStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 224466880022L);
			longHandle2.set(structSegmt2, 113355779911L);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(longHandle1.get(resultSegmt), 1212121202009L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 236812569034L);
		}
	}

	@Test
	public void test_add2LongStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2LongStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2LongStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 5566778899L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 9900112233L);
			longHandle2.set(structSegmt2, 3344556677L);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_LONG, 0), 11022446688L);
			Assert.assertEquals(resultAddr.get(JAVA_LONG, 8), 8911335576L);
		}
	}

	@Test
	public void test_add3LongStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"), JAVA_LONG.withName("elem3"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle longHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add3LongStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3LongStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 987654321987L);
			longHandle2.set(structSegmt1, 123456789123L);
			longHandle3.set(structSegmt1, 112233445566L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 224466880022L);
			longHandle2.set(structSegmt2, 113355779911L);
			longHandle3.set(structSegmt2, 778899001122L);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(longHandle1.get(resultSegmt), 1212121202009L);
			Assert.assertEquals(longHandle2.get(resultSegmt), 236812569034L);
			Assert.assertEquals(longHandle3.get(resultSegmt), 891132446688L);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStruct,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt, 8.12F);
			floatHandle2.set(structSegmt, 9.24F);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAnd5FloatsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAnd5FloatsFromStruct,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt, 1.01F);
			floatHandle2.set(structSegmt, 1.02F);
			floatHandle3.set(structSegmt, 1.03F);
			floatHandle4.set(structSegmt, 1.04F);
			floatHandle5.set(structSegmt, 1.05F);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatFromPointerAndFloatsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatFromPointerAndFloatsFromStruct,
					FunctionDescriptor.of(JAVA_FLOAT, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment floatSegmt = allocator.allocate(JAVA_FLOAT, 12.12F);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt, 18.23F);
			floatHandle2.set(structSegmt, 19.34F);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatFromPointerAndFloatsFromStruct_returnFloatPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatFromPointerAndFloatsFromStruct_returnFloatPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment floatSegmt = allocator.allocate(JAVA_FLOAT, 12.12F);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt, 18.23F);
			floatHandle2.set(structSegmt, 19.34F);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(floatSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_FLOAT, 0), 49.69F, 0.01F);
			Assert.assertEquals(resultAddr.toRawLongValue(), floatSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addFloatAndFloatsFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructPointer,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt, 35.11F);
			floatHandle2.set(structSegmt, 46.22F);

			float result = (float)mh.invoke(79.33F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 160.66F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatsFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_FLOAT.withName("elem2"));
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromNestedStruct,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromStructWithNestedFloatArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructWithNestedFloatArray,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addFloatAndFloatsFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add3FloatStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3FloatStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			floatHandle3.set(structSegmt1, 45.67F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt2, 24.34F);
			floatHandle2.set(structSegmt2, 13.45F);
			floatHandle3.set(structSegmt2, 69.72F);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 49.46F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 24.68F, 0.01F);
			Assert.assertEquals((float)floatHandle3.get(resultSegmt), 115.39, 0.01F);
		}
	}

	@Test
	public void test_add2FloatStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2FloatStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2FloatStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt2, 24.34F);
			floatHandle2.set(structSegmt2, 13.45F);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((float)floatHandle1.get(resultSegmt), 49.46F, 0.01F);
			Assert.assertEquals((float)floatHandle2.get(resultSegmt), 24.68F, 0.01F);
		}
	}

	@Test
	public void test_add2FloatStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2FloatStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2FloatStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt1, 25.12F);
			floatHandle2.set(structSegmt1, 11.23F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt2, 24.34F);
			floatHandle2.set(structSegmt2, 13.45F);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_FLOAT, 0), 49.46F, 0.01F);
			Assert.assertEquals(resultAddr.get(JAVA_FLOAT, 4), 24.68F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt, 2228.111D);
			doubleHandle2.set(structSegmt, 2229.221D);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleFromPointerAndDoublesFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleFromPointerAndDoublesFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment doubleSegmt = allocator.allocate(JAVA_DOUBLE, 112.123D);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt, 118.456D);
			doubleHandle2.set(structSegmt, 119.789D);

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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleFromPointerAndDoublesFromStruct_returnDoublePointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment doubleSegmt = allocator.allocate(JAVA_DOUBLE, 212.123D);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt, 218.456D);
			doubleHandle2.set(structSegmt, 219.789D);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(doubleSegmt, structSegmt, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_DOUBLE, 0), 650.368D, 0.001D);
			Assert.assertEquals(resultAddr.toRawLongValue(), doubleSegmt.address().toRawLongValue());
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructPointer,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt, 22.111D);
			doubleHandle2.set(structSegmt, 44.222D);

			double result = (double)mh.invoke(66.333D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 132.666D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedStructByUpcallMH() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.structLayout(nestedStructLayout.withName("struct_elem1"), JAVA_DOUBLE.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromNestedStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromNestedStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromNestedStruct_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromNestedStruct_reverseOrder,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromStructWithNestedDoubleArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructWithNestedDoubleArray,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromStructWithNestedStructArrayByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructWithNestedStructArray,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrderByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
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
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2DoubleStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2DoubleStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 33.444D);
			doubleHandle2.set(structSegmt2, 44.555D);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2,  upcallFuncAddr);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 44.666D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_add2DoubleStructs_returnStructPointerByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add2DoubleStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2DoubleStructs_returnStructPointer,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 33.444D);
			doubleHandle2.set(structSegmt2, 44.555D);

			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_DOUBLE, 0), 44.666D, 0.001D);
			Assert.assertEquals(resultAddr.get(JAVA_DOUBLE, 8), 66.888D, 0.001D);
		}
	}

	@Test
	public void test_add3DoubleStructs_returnStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"), JAVA_DOUBLE.withName("elem3"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle doubleHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		NativeSymbol functionSymbol = nativeLibLookup.lookup("add3DoubleStructs_returnStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			NativeSymbol upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add3DoubleStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11.222D);
			doubleHandle2.set(structSegmt1, 22.333D);
			doubleHandle3.set(structSegmt1, 33.123D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 33.444D);
			doubleHandle2.set(structSegmt2, 44.555D);
			doubleHandle3.set(structSegmt2, 55.456D);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke(allocator, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 44.666D, 0.001D);
			Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 66.888D, 0.001D);
			Assert.assertEquals((double)doubleHandle3.get(resultSegmt), 88.579D, 0.001D);
		}
	}
}
