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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
package org.openj9.test.jep389.downcall;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;

import java.nio.ByteOrder;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.lang.invoke.VarHandle;

import jdk.incubator.foreign.CLinker;
import static jdk.incubator.foreign.CLinker.*;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.LibraryLookup;
import static jdk.incubator.foreign.LibraryLookup.Symbol;

import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemoryLayout.PathElement;
import jdk.incubator.foreign.ValueLayout;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.SequenceLayout;
import jdk.incubator.foreign.MemoryAccess;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.MemoryHandles;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) DownCall for argument/return struct.
 *
 * Note: the padding elements in the struct are only required by RI or VarHandle (accessing the
 * data address) while they are totally ignored in OpenJ9 given the padding/alignment are
 * computed by libffi automatically in native.
 */
@Test(groups = { "level.sanity" })
public class StructTests {
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinOS = osName.contains("win");
	private static ValueLayout longLayout = (isWinOS || isAixOS) ? C_LONG_LONG : C_LONG;
	private static LibraryLookup nativeLib = LibraryLookup.ofLibrary("clinkerffitests");
	private static CLinker clinker = CLinker.getInstance();

	@Test
	public void test_addBoolAndBoolsFromStructWithXor() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addBoolAndBoolsFromStructWithXor").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		boolHandle1.set(structSegmt, 0);
		boolHandle2.set(structSegmt, 1);

		boolean result = (boolean)mh.invokeExact(false, structSegmt);
		Assert.assertEquals(result, true);
		structSegmt.close();
	}

	@Test
	public void test_addBoolFromPointerAndBoolsFromStructWithXor() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(boolean.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addBoolFromPointerAndBoolsFromStructWithXor").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment booleanSegmt = MemorySegment.allocateNative(C_INT);
		MemoryAccess.setInt(booleanSegmt, 1);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		boolHandle1.set(structSegmt, 0);
		boolHandle2.set(structSegmt, 1);

		boolean result = (boolean)mh.invokeExact(booleanSegmt.address(), structSegmt);
		Assert.assertEquals(result, false);
		booleanSegmt.close();
		structSegmt.close();
	}

	@Test
	public void test_addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT, C_INT);
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment booleanSegmt = MemorySegment.allocateNative(C_INT);
		MemoryAccess.setInt(booleanSegmt, 0);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 1);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(booleanSegmt.address(), structSegmt);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(C_INT.byteSize());
		VarHandle intHandle = MemoryHandles.varHandle(int.class, ByteOrder.nativeOrder());
		int result = (int)intHandle.get(resultSegmt, 0);
		Assert.assertEquals(result, 1);
		Assert.assertEquals(resultSegmt.address().toRawLongValue(), booleanSegmt.address().toRawLongValue());
		booleanSegmt.close();
		structSegmt.close();
		resultSegmt.close();
	}

	@Test
	public void test_addBoolAndBoolsFromStructPointerWithXor() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER);
		Symbol functionSymbol = nativeLib.lookup("addBoolAndBoolsFromStructPointerWithXor").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		boolHandle1.set(structSegmt, 1);
		boolHandle2.set(structSegmt, 0);

		boolean result = (boolean)mh.invokeExact(false, structSegmt.address());
		Assert.assertEquals(result, true);
		structSegmt.close();
	}

	@Test
	public void test_addBoolAndBoolsFromNestedStructWithXor() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout.withName("struct_elem1"), C_INT.withName("elem2"));
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addBoolAndBoolsFromNestedStructWithXor").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 1);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 1);

		boolean result = (boolean)mh.invokeExact(true, structSegmt);
		Assert.assertEquals(result, true);
		structSegmt.close();
	}

	@Test
	public void test_addBoolAndBoolsFromNestedStructWithXor_reverseOrder() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addBoolAndBoolsFromNestedStructWithXor_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 1);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 1);

		boolean result = (boolean)mh.invokeExact(true, structSegmt);
		Assert.assertEquals(result, true);
		structSegmt.close();
	}

	@Test
	public void test_addBoolAndBoolsFromNestedStructWithXor_withoutLayoutName() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_INT, C_INT);
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout, C_INT);
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addBoolAndBoolsFromNestedStructWithXor").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 1);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 1);

		boolean result = (boolean)mh.invokeExact(true, structSegmt);
		Assert.assertEquals(result, true);
		structSegmt.close();
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedBoolArray() throws Throwable {
		SequenceLayout intArray = MemoryLayout.ofSequence(2, C_INT);
		GroupLayout structLayout = MemoryLayout.ofStruct(intArray.withName("array_elem1"), C_INT.withName("elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addBoolAndBoolsFromStructWithNestedBoolArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 1);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 0);

		int result = (int)mh.invokeExact(0, structSegmt);
		Assert.assertEquals(result, 1);
		structSegmt.close();
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder() throws Throwable {
		SequenceLayout intArray = MemoryLayout.ofSequence(2, C_INT);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), intArray.withName("array_elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 1);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 0);

		int result = (int)mh.invokeExact(0, structSegmt);
		Assert.assertEquals(result, 1);
		structSegmt.close();
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedBoolArray_withoutLayoutName() throws Throwable {
		SequenceLayout intArray = MemoryLayout.ofSequence(2, C_INT);
		GroupLayout structLayout = MemoryLayout.ofStruct(intArray, C_INT);
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addBoolAndBoolsFromStructWithNestedBoolArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 1);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 1);

		int result = (int)mh.invokeExact(0, structSegmt);
		Assert.assertEquals(result, 0);
		structSegmt.close();
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedStructArray() throws Throwable {
		GroupLayout intStruct = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, intStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray.withName("struct_array_elem1"), C_INT.withName("elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addBoolAndBoolsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 1);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 12, 1);
		MemoryAccess.setIntAtOffset(structSegmt, 16, 0);

		int result = (int)mh.invokeExact(1, structSegmt);
		Assert.assertEquals(result, 1);
		structSegmt.close();
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder() throws Throwable {
		GroupLayout intStruct = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, intStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), structArray.withName("struct_array_elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 1);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 12, 1);
		MemoryAccess.setIntAtOffset(structSegmt, 16, 0);

		int result = (int)mh.invokeExact(1, structSegmt);
		Assert.assertEquals(result, 1);
		structSegmt.close();
	}

	@Test
	public void test_addBoolAndBoolsFromStructWithNestedStructArray_withoutLayoutName() throws Throwable {
		GroupLayout intStruct = MemoryLayout.ofStruct(C_INT, C_INT);
		SequenceLayout structArray = MemoryLayout.ofSequence(2, intStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray, C_INT);
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addBoolAndBoolsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 1);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 1);
		MemoryAccess.setIntAtOffset(structSegmt, 12, 0);
		MemoryAccess.setIntAtOffset(structSegmt, 16, 1);

		int result = (int)mh.invokeExact(0, structSegmt);
		Assert.assertEquals(result, 1);
		structSegmt.close();
	}

	@Test
	public void test_add2BoolStructsWithXor_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2BoolStructsWithXor_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		boolHandle1.set(structSegmt1, 1);
		boolHandle2.set(structSegmt1, 0);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		boolHandle1.set(structSegmt2, 1);
		boolHandle2.set(structSegmt2, 1);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals(boolHandle1.get(resultSegmt), 0);
		Assert.assertEquals(boolHandle2.get(resultSegmt), 1);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add2BoolStructsWithXor_returnStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2BoolStructsWithXor_returnStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		boolHandle1.set(structSegmt1, 1);
		boolHandle2.set(structSegmt1, 0);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		boolHandle1.set(structSegmt2, 1);
		boolHandle2.set(structSegmt2, 1);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(structLayout.byteSize());
		Assert.assertEquals(boolHandle1.get(resultSegmt), 0);
		Assert.assertEquals(boolHandle2.get(resultSegmt), 1);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add3BoolStructsWithXor_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"), C_INT.withName("elem3"));
		VarHandle boolHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));
		VarHandle boolHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add3BoolStructsWithXor_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		boolHandle1.set(structSegmt1, 1);
		boolHandle2.set(structSegmt1, 0);
		boolHandle3.set(structSegmt1, 1);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		boolHandle1.set(structSegmt2, 1);
		boolHandle2.set(structSegmt2, 1);
		boolHandle3.set(structSegmt2, 0);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals(boolHandle1.get(resultSegmt), 0);
		Assert.assertEquals(boolHandle2.get(resultSegmt), 1);
		Assert.assertEquals(boolHandle3.get(resultSegmt), 1);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_addByteAndBytesFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addByteAndBytesFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		byteHandle1.set(structSegmt, (byte)8);
		byteHandle2.set(structSegmt, (byte)9);

		byte result = (byte)mh.invokeExact((byte)6, structSegmt);
		Assert.assertEquals(result, 23);
		structSegmt.close();
	}

	@Test
	public void test_addByteFromPointerAndBytesFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(byte.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addByteFromPointerAndBytesFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment byteSegmt = MemorySegment.allocateNative(C_CHAR);
		MemoryAccess.setByte(byteSegmt, (byte)12);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		byteHandle1.set(structSegmt, (byte)14);
		byteHandle2.set(structSegmt, (byte)16);

		byte result = (byte)mh.invokeExact(byteSegmt.address(), structSegmt);
		Assert.assertEquals(result, 42);
		byteSegmt.close();
		structSegmt.close();
	}

	@Test
	public void test_addByteFromPointerAndBytesFromStruct_returnBytePointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addByteFromPointerAndBytesFromStruct_returnBytePointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment byteSegmt = MemorySegment.allocateNative(C_CHAR);
		MemoryAccess.setByte(byteSegmt, (byte)12);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		byteHandle1.set(structSegmt, (byte)18);
		byteHandle2.set(structSegmt, (byte)19);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(byteSegmt.address(), structSegmt);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(C_CHAR.byteSize());
		VarHandle byteHandle = MemoryHandles.varHandle(byte.class, ByteOrder.nativeOrder());
		byte result = (byte)byteHandle.get(resultSegmt, 0);
		Assert.assertEquals(result, 49);
		Assert.assertEquals(resultSegmt.address().toRawLongValue(), byteSegmt.address().toRawLongValue());
		byteSegmt.close();
		structSegmt.close();
		resultSegmt.close();
	}

	@Test
	public void test_addByteAndBytesFromStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(byte.class, byte.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER);
		Symbol functionSymbol = nativeLib.lookup("addByteAndBytesFromStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		byteHandle1.set(structSegmt, (byte)11);
		byteHandle2.set(structSegmt, (byte)12);

		byte result = (byte)mh.invokeExact((byte)13, structSegmt.address());
		Assert.assertEquals(result, 36);
		structSegmt.close();
	}

	@Test
	public void test_addByteAndBytesFromNestedStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout.withName("struct_elem1"), C_CHAR.withName("elem2"));
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addByteAndBytesFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)11);
		MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)22);
		MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)33);

		byte result = (byte)mh.invokeExact((byte)46, structSegmt);
		Assert.assertEquals(result, 112);
		structSegmt.close();
	}

	@Test
	public void test_addByteAndBytesFromNestedStruct_reverseOrder() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(C_CHAR.withName("elem1"),
				nestedStructLayout.withName("struct_elem2"), MemoryLayout.ofPaddingBits(C_CHAR.bitSize()));
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addByteAndBytesFromNestedStruct_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)12);
		MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)24);
		MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)36);

		byte result = (byte)mh.invokeExact((byte)48, structSegmt);
		Assert.assertEquals(result, 120);
		structSegmt.close();
	}

	@Test
	public void test_addByteAndBytesFromNestedStruct_withoutLayoutName() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_CHAR, C_CHAR);
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout, C_CHAR);
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addByteAndBytesFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)11);
		MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)22);
		MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)33);

		byte result = (byte)mh.invokeExact((byte)46, structSegmt);
		Assert.assertEquals(result, 112);
		structSegmt.close();
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedByteArray() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.ofSequence(2, C_CHAR);
		GroupLayout structLayout = MemoryLayout.ofStruct(byteArray.withName("array_elem1"),
				C_CHAR.withName("elem2"), MemoryLayout.ofPaddingBits(C_CHAR.bitSize()));
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addByteAndBytesFromStructWithNestedByteArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)11);
		MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)12);
		MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)13);

		byte result = (byte)mh.invokeExact((byte)14, structSegmt);
		Assert.assertEquals(result, 50);
		structSegmt.close();
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedByteArray_reverseOrder() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.ofSequence(2, C_CHAR);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_CHAR.withName("elem1"),
				byteArray.withName("array_elem2"), MemoryLayout.ofPaddingBits(C_CHAR.bitSize()));
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addByteAndBytesFromStructWithNestedByteArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)12);
		MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)14);
		MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)16);

		byte result = (byte)mh.invokeExact((byte)18, structSegmt);
		Assert.assertEquals(result, 60);
		structSegmt.close();
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedByteArray_withoutLayoutName() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.ofSequence(2, C_CHAR);
		GroupLayout structLayout = MemoryLayout.ofStruct(byteArray, C_CHAR);
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addByteAndBytesFromStructWithNestedByteArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)11);
		MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)12);
		MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)13);

		byte result = (byte)mh.invokeExact((byte)14, structSegmt);
		Assert.assertEquals(result, 50);
		structSegmt.close();
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedStructArray() throws Throwable {
		GroupLayout byteStruct = MemoryLayout.ofStruct(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, byteStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray.withName("struct_array_elem1"),
				C_CHAR.withName("elem2"), MemoryLayout.ofPaddingBits(C_CHAR.bitSize() * 3));
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addByteAndBytesFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)11);
		MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)12);
		MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)13);
		MemoryAccess.setByteAtOffset(structSegmt, 3, (byte)14);
		MemoryAccess.setByteAtOffset(structSegmt, 4, (byte)15);

		byte result = (byte)mh.invokeExact((byte)16, structSegmt);
		Assert.assertEquals(result, 81);
		structSegmt.close();
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedStructArray_reverseOrder() throws Throwable {
		GroupLayout byteStruct = MemoryLayout.ofStruct(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, byteStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_CHAR.withName("elem1"),
				structArray.withName("struct_array_elem2"), MemoryLayout.ofPaddingBits(C_CHAR.bitSize() * 3));
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addByteAndBytesFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)12);
		MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)14);
		MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)16);
		MemoryAccess.setByteAtOffset(structSegmt, 3, (byte)18);
		MemoryAccess.setByteAtOffset(structSegmt, 4, (byte)20);

		byte result = (byte)mh.invokeExact((byte)22, structSegmt);
		Assert.assertEquals(result, 102);
		structSegmt.close();
	}

	@Test
	public void test_addByteAndBytesFromStructWithNestedStructArray_withoutLayoutName() throws Throwable {
		GroupLayout byteStruct = MemoryLayout.ofStruct(C_CHAR, C_CHAR);
		SequenceLayout structArray = MemoryLayout.ofSequence(2, byteStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray, C_CHAR);
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addByteAndBytesFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setByteAtOffset(structSegmt, 0, (byte)11);
		MemoryAccess.setByteAtOffset(structSegmt, 1, (byte)12);
		MemoryAccess.setByteAtOffset(structSegmt, 2, (byte)13);
		MemoryAccess.setByteAtOffset(structSegmt, 3, (byte)14);
		MemoryAccess.setByteAtOffset(structSegmt, 4, (byte)15);

		byte result = (byte)mh.invokeExact((byte)16, structSegmt);
		Assert.assertEquals(result, 81);
		structSegmt.close();
	}

	@Test
	public void test_add2ByteStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2ByteStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		byteHandle1.set(structSegmt1, (byte)25);
		byteHandle2.set(structSegmt1, (byte)11);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		byteHandle1.set(structSegmt2, (byte)24);
		byteHandle2.set(structSegmt2, (byte)13);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
		Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)24);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add2ByteStructs_returnStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2ByteStructs_returnStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		byteHandle1.set(structSegmt1, (byte)25);
		byteHandle2.set(structSegmt1, (byte)11);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		byteHandle1.set(structSegmt2, (byte)24);
		byteHandle2.set(structSegmt2, (byte)13);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(structLayout.byteSize());
		Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
		Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)24);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add3ByteStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"),
				C_CHAR.withName("elem3"), MemoryLayout.ofPaddingBits(C_CHAR.bitSize()));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));
		VarHandle byteHandle3 = structLayout.varHandle(byte.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add3ByteStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		byteHandle1.set(structSegmt1, (byte)25);
		byteHandle2.set(structSegmt1, (byte)11);
		byteHandle3.set(structSegmt1, (byte)12);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		byteHandle1.set(structSegmt2, (byte)24);
		byteHandle2.set(structSegmt2, (byte)13);
		byteHandle3.set(structSegmt2, (byte)16);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals((byte)byteHandle1.get(resultSegmt), (byte)49);
		Assert.assertEquals((byte)byteHandle2.get(resultSegmt), (byte)24);
		Assert.assertEquals((byte)byteHandle3.get(resultSegmt), (byte)28);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_addCharAndCharsFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addCharAndCharsFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		charHandle1.set(structSegmt, 'A');
		charHandle2.set(structSegmt, 'B');

		char result = (char)mh.invokeExact('C', structSegmt);
		Assert.assertEquals(result, 'D');
		structSegmt.close();
	}

	@Test
	public void test_addCharFromPointerAndCharsFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(char.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addCharFromPointerAndCharsFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment charSegmt = MemorySegment.allocateNative(C_SHORT);
		MemoryAccess.setChar(charSegmt, 'D');
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		charHandle1.set(structSegmt, 'E');
		charHandle2.set(structSegmt, 'F');

		char result = (char)mh.invokeExact(charSegmt.address(), structSegmt);
		Assert.assertEquals(result, 'M');
		charSegmt.close();
		structSegmt.close();
	}

	@Test
	public void test_addCharFromPointerAndCharsFromStruct_returnCharPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addCharFromPointerAndCharsFromStruct_returnCharPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment charSegmt = MemorySegment.allocateNative(C_SHORT);
		MemoryAccess.setChar(charSegmt, 'D');
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		charHandle1.set(structSegmt, 'E');
		charHandle2.set(structSegmt, 'F');

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(charSegmt.address(), structSegmt);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(C_SHORT.byteSize());
		VarHandle charHandle = MemoryHandles.varHandle(char.class, ByteOrder.nativeOrder());
		char result = (char)charHandle.get(resultSegmt, 0);
		Assert.assertEquals(result, 'M');
		Assert.assertEquals(resultSegmt.address().toRawLongValue(), charSegmt.address().toRawLongValue());
		charSegmt.close();
		structSegmt.close();
		resultSegmt.close();
	}

	@Test
	public void test_addCharAndCharsFromStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(char.class, char.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_POINTER);
		Symbol functionSymbol = nativeLib.lookup("addCharAndCharsFromStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		charHandle1.set(structSegmt, 'H');
		charHandle2.set(structSegmt, 'I');

		char result = (char)mh.invokeExact('G', structSegmt.address());
		Assert.assertEquals(result, 'V');
		structSegmt.close();
	}

	@Test
	public void test_addCharAndCharsFromNestedStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout.withName("struct_elem1"), C_SHORT.withName("elem2"));
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addCharAndCharsFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setCharAtOffset(structSegmt, 0, 'E');
		MemoryAccess.setCharAtOffset(structSegmt, 2, 'F');
		MemoryAccess.setCharAtOffset(structSegmt, 4, 'G');

		char result = (char)mh.invokeExact('H', structSegmt);
		Assert.assertEquals(result, 'W');
		structSegmt.close();
	}

	@Test
	public void test_addCharAndCharsFromNestedStruct_reverseOrder() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"),
				nestedStructLayout.withName("struct_elem2"), MemoryLayout.ofPaddingBits(C_SHORT.bitSize()));
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addCharAndCharsFromNestedStruct_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setCharAtOffset(structSegmt, 0, 'E');
		MemoryAccess.setCharAtOffset(structSegmt, 2, 'F');
		MemoryAccess.setCharAtOffset(structSegmt, 4, 'G');

		char result = (char)mh.invokeExact('H', structSegmt);
		Assert.assertEquals(result, 'W');
		structSegmt.close();
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedCharArray() throws Throwable {
		SequenceLayout charArray = MemoryLayout.ofSequence(2, C_SHORT);
		GroupLayout structLayout = MemoryLayout.ofStruct(charArray.withName("array_elem1"),
				C_SHORT.withName("elem2"), MemoryLayout.ofPaddingBits(C_SHORT.bitSize()));
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addCharAndCharsFromStructWithNestedCharArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setCharAtOffset(structSegmt, 0, 'A');
		MemoryAccess.setCharAtOffset(structSegmt, 2, 'B');
		MemoryAccess.setCharAtOffset(structSegmt, 4, 'C');

		char result = (char)mh.invokeExact('D', structSegmt);
		Assert.assertEquals(result, 'G');
		structSegmt.close();
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedCharArray_reverseOrder() throws Throwable {
		SequenceLayout charArray = MemoryLayout.ofSequence(2, C_SHORT);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"),
				charArray.withName("array_elem2"), MemoryLayout.ofPaddingBits(C_SHORT.bitSize()));
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addCharAndCharsFromStructWithNestedCharArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setCharAtOffset(structSegmt, 0, 'A');
		MemoryAccess.setCharAtOffset(structSegmt, 2, 'B');
		MemoryAccess.setCharAtOffset(structSegmt, 4, 'C');

		char result = (char)mh.invokeExact('D', structSegmt);
		Assert.assertEquals(result, 'G');
		structSegmt.close();
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedCharArray_withoutLayoutName() throws Throwable {
		SequenceLayout charArray = MemoryLayout.ofSequence(2, C_SHORT);
		GroupLayout structLayout = MemoryLayout.ofStruct(charArray, C_SHORT, MemoryLayout.ofPaddingBits(C_SHORT.bitSize()));
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addCharAndCharsFromStructWithNestedCharArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setCharAtOffset(structSegmt, 0, 'A');
		MemoryAccess.setCharAtOffset(structSegmt, 2, 'B');
		MemoryAccess.setCharAtOffset(structSegmt, 4, 'C');

		char result = (char)mh.invokeExact('D', structSegmt);
		Assert.assertEquals(result, 'G');
		structSegmt.close();
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedStructArray() throws Throwable {
		GroupLayout charStruct = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, charStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray.withName("struct_array_elem1"), C_SHORT.withName("elem2"));
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addCharAndCharsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setCharAtOffset(structSegmt, 0, 'E');
		MemoryAccess.setCharAtOffset(structSegmt, 2, 'F');
		MemoryAccess.setCharAtOffset(structSegmt, 4, 'G');
		MemoryAccess.setCharAtOffset(structSegmt, 6, 'H');
		MemoryAccess.setCharAtOffset(structSegmt, 8, 'I');

		char result = (char)mh.invokeExact('J', structSegmt);
		Assert.assertEquals(result, 'h');
		structSegmt.close();
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedStructArray_reverseOrder() throws Throwable {
		GroupLayout charStruct = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, charStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"),
				structArray.withName("struct_array_elem2"), MemoryLayout.ofPaddingBits(C_SHORT.bitSize()));
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addCharAndCharsFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setCharAtOffset(structSegmt, 0, 'E');
		MemoryAccess.setCharAtOffset(structSegmt, 2, 'F');
		MemoryAccess.setCharAtOffset(structSegmt, 4, 'G');
		MemoryAccess.setCharAtOffset(structSegmt, 6, 'H');
		MemoryAccess.setCharAtOffset(structSegmt, 8, 'I');

		char result = (char)mh.invokeExact('J', structSegmt);
		Assert.assertEquals(result, 'h');
		structSegmt.close();
	}

	@Test
	public void test_addCharAndCharsFromStructWithNestedStructArray_withoutLayoutName() throws Throwable {
		GroupLayout charStruct = MemoryLayout.ofStruct(C_SHORT, C_SHORT);
		SequenceLayout structArray = MemoryLayout.ofSequence(2, charStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray, C_SHORT);
		MethodType mt = MethodType.methodType(char.class, char.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addCharAndCharsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setCharAtOffset(structSegmt, 0, 'E');
		MemoryAccess.setCharAtOffset(structSegmt, 2, 'F');
		MemoryAccess.setCharAtOffset(structSegmt, 4, 'G');
		MemoryAccess.setCharAtOffset(structSegmt, 6, 'H');
		MemoryAccess.setCharAtOffset(structSegmt, 8, 'I');

		char result = (char)mh.invokeExact('J', structSegmt);
		Assert.assertEquals(result, 'h');
		structSegmt.close();
	}

	@Test
	public void test_add2CharStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2CharStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		charHandle1.set(structSegmt1, 'A');
		charHandle2.set(structSegmt1, 'B');
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		charHandle1.set(structSegmt2, 'C');
		charHandle2.set(structSegmt2, 'D');

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals(charHandle1.get(resultSegmt), 'C');
		Assert.assertEquals(charHandle2.get(resultSegmt), 'E');
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add2CharStructs_returnStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2CharStructs_returnStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		charHandle1.set(structSegmt1, 'A');
		charHandle2.set(structSegmt1, 'B');
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		charHandle1.set(structSegmt2, 'C');
		charHandle2.set(structSegmt2, 'D');

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(structLayout.byteSize());
		Assert.assertEquals(charHandle1.get(resultSegmt), 'C');
		Assert.assertEquals(charHandle2.get(resultSegmt), 'E');
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add3CharStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"), C_SHORT.withName("elem3"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));
		VarHandle charHandle3 = structLayout.varHandle(char.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add3CharStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		charHandle1.set(structSegmt1, 'A');
		charHandle2.set(structSegmt1, 'B');
		charHandle3.set(structSegmt1, 'C');
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		charHandle1.set(structSegmt2, 'B');
		charHandle2.set(structSegmt2, 'C');
		charHandle3.set(structSegmt2, 'D');

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals(charHandle1.get(resultSegmt), 'B');
		Assert.assertEquals(charHandle2.get(resultSegmt), 'D');
		Assert.assertEquals(charHandle3.get(resultSegmt), 'F');
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_addShortAndShortsFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addShortAndShortsFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		shortHandle1.set(structSegmt, (short)8);
		shortHandle2.set(structSegmt, (short)9);
		short result = (short)mh.invokeExact((short)6, structSegmt);
		Assert.assertEquals(result, 23);
		structSegmt.close();
	}

	@Test
	public void test_addShortFromPointerAndShortsFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(short.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addShortFromPointerAndShortsFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment shortSegmt = MemorySegment.allocateNative(C_SHORT);
		MemoryAccess.setShort(shortSegmt, (short)12);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		shortHandle1.set(structSegmt, (short)18);
		shortHandle2.set(structSegmt, (short)19);

		short result = (short)mh.invokeExact(shortSegmt.address(), structSegmt);
		Assert.assertEquals(result, 49);
		shortSegmt.close();
		structSegmt.close();
	}

	@Test
	public void test_addShortFromPointerAndShortsFromStruct_returnShortPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addShortFromPointerAndShortsFromStruct_returnShortPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment shortSegmt = MemorySegment.allocateNative(C_SHORT);
		MemoryAccess.setShort(shortSegmt, (short)12);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		shortHandle1.set(structSegmt, (short)18);
		shortHandle2.set(structSegmt, (short)19);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(shortSegmt.address(), structSegmt);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(C_SHORT.byteSize());
		VarHandle shortHandle = MemoryHandles.varHandle(short.class, ByteOrder.nativeOrder());
		short result = (short)shortHandle.get(resultSegmt, 0);
		Assert.assertEquals(result, 49);
		Assert.assertEquals(resultSegmt.address().toRawLongValue(), shortSegmt.address().toRawLongValue());
		shortSegmt.close();
		structSegmt.close();
		resultSegmt.close();
	}

	@Test
	public void test_addShortAndShortsFromStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(short.class, short.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_POINTER);
		Symbol functionSymbol = nativeLib.lookup("addShortAndShortsFromStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		shortHandle1.set(structSegmt, (short)22);
		shortHandle2.set(structSegmt, (short)44);

		short result = (short)mh.invokeExact((short)66, structSegmt.address());
		Assert.assertEquals(result, 132);
		structSegmt.close();
	}

	@Test
	public void test_addShortAndShortsFromNestedStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout.withName("struct_elem1"),
				C_SHORT.withName("elem2"), MemoryLayout.ofPaddingBits(C_SHORT.bitSize()));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addShortAndShortsFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setShortAtOffset(structSegmt, 0, (short)31);
		MemoryAccess.setShortAtOffset(structSegmt, 2, (short)33);
		MemoryAccess.setShortAtOffset(structSegmt, 4, (short)35);

		short result = (short)mh.invokeExact((short)37, structSegmt);
		Assert.assertEquals(result, 136);
		structSegmt.close();
	}

	@Test
	public void test_addShortAndShortsFromNestedStruct_reverseOrder() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"),
				nestedStructLayout.withName("struct_elem2"), MemoryLayout.ofPaddingBits(C_SHORT.bitSize()));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addShortAndShortsFromNestedStruct_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setShortAtOffset(structSegmt, 0, (short)31);
		MemoryAccess.setShortAtOffset(structSegmt, 2, (short)33);
		MemoryAccess.setShortAtOffset(structSegmt, 4, (short)35);

		short result = (short)mh.invokeExact((short)37, structSegmt);
		Assert.assertEquals(result, 136);
		structSegmt.close();
	}

	@Test
	public void test_addShortAndShortsFromNestedStruct_withoutLayoutName() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_SHORT, C_SHORT);
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout, C_SHORT);
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addShortAndShortsFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setShortAtOffset(structSegmt, 0, (short)31);
		MemoryAccess.setShortAtOffset(structSegmt, 2, (short)33);
		MemoryAccess.setShortAtOffset(structSegmt, 4, (short)35);

		short result = (short)mh.invokeExact((short)37, structSegmt);
		Assert.assertEquals(result, 136);
		structSegmt.close();
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedShortArray() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.ofSequence(2, C_SHORT);
		GroupLayout structLayout = MemoryLayout.ofStruct(shortArray.withName("array_elem1"),
				C_SHORT.withName("elem2"), MemoryLayout.ofPaddingBits(C_SHORT.bitSize()));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addShortAndShortsFromStructWithNestedShortArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setShortAtOffset(structSegmt, 0, (short)111);
		MemoryAccess.setShortAtOffset(structSegmt, 2, (short)222);
		MemoryAccess.setShortAtOffset(structSegmt, 4, (short)333);

		short result = (short)mh.invokeExact((short)444, structSegmt);
		Assert.assertEquals(result, 1110);
		structSegmt.close();
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedShortArray_reverseOrder() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.ofSequence(2, C_SHORT);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"),
				shortArray.withName("array_elem2"), MemoryLayout.ofPaddingBits(C_SHORT.bitSize()));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addShortAndShortsFromStructWithNestedShortArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setShortAtOffset(structSegmt, 0, (short)111);
		MemoryAccess.setShortAtOffset(structSegmt, 2, (short)222);
		MemoryAccess.setShortAtOffset(structSegmt, 4, (short)333);

		short result = (short)mh.invokeExact((short)444, structSegmt);
		Assert.assertEquals(result, 1110);
		structSegmt.close();
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedShortArray_withoutLayoutName() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.ofSequence(2, C_SHORT);
		GroupLayout structLayout = MemoryLayout.ofStruct(shortArray, C_SHORT, MemoryLayout.ofPaddingBits(C_SHORT.bitSize()));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addShortAndShortsFromStructWithNestedShortArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setShortAtOffset(structSegmt, 0, (short)111);
		MemoryAccess.setShortAtOffset(structSegmt, 2, (short)222);
		MemoryAccess.setShortAtOffset(structSegmt, 4, (short)333);

		short result = (short)mh.invokeExact((short)444, structSegmt);
		Assert.assertEquals(result, 1110);
		structSegmt.close();
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedStructArray() throws Throwable {
		GroupLayout shortStruct = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, shortStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray.withName("struc_array_elem1"), C_SHORT.withName("elem2"));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addShortAndShortsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setShortAtOffset(structSegmt, 0, (short)111);
		MemoryAccess.setShortAtOffset(structSegmt, 2, (short)222);
		MemoryAccess.setShortAtOffset(structSegmt, 4, (short)333);
		MemoryAccess.setShortAtOffset(structSegmt, 6, (short)444);
		MemoryAccess.setShortAtOffset(structSegmt, 8, (short)555);

		short result = (short)mh.invokeExact((short)666, structSegmt);
		Assert.assertEquals(result, 2331);
		structSegmt.close();
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedStructArray_reverseOrder() throws Throwable {
		GroupLayout shortStruct = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, shortStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), structArray.withName("struc_array_elem2"));
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addShortAndShortsFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setShortAtOffset(structSegmt, 0, (short)111);
		MemoryAccess.setShortAtOffset(structSegmt, 2, (short)222);
		MemoryAccess.setShortAtOffset(structSegmt, 4, (short)333);
		MemoryAccess.setShortAtOffset(structSegmt, 6, (short)444);
		MemoryAccess.setShortAtOffset(structSegmt, 8, (short)555);

		short result = (short)mh.invokeExact((short)666, structSegmt);
		Assert.assertEquals(result, 2331);
		structSegmt.close();
	}

	@Test
	public void test_addShortAndShortsFromStructWithNestedStructArray_withoutLayoutName() throws Throwable {
		GroupLayout shortStruct = MemoryLayout.ofStruct(C_SHORT, C_SHORT);
		SequenceLayout structArray = MemoryLayout.ofSequence(2, shortStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray, C_SHORT);
		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addShortAndShortsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setShortAtOffset(structSegmt, 0, (short)111);
		MemoryAccess.setShortAtOffset(structSegmt, 2, (short)222);
		MemoryAccess.setShortAtOffset(structSegmt, 4, (short)333);
		MemoryAccess.setShortAtOffset(structSegmt, 6, (short)444);
		MemoryAccess.setShortAtOffset(structSegmt, 8, (short)555);

		short result = (short)mh.invokeExact((short)666, structSegmt);
		Assert.assertEquals(result, 2331);
		structSegmt.close();
	}

	@Test
	public void test_add2ShortStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2ShortStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		shortHandle1.set(structSegmt1, (short)56);
		shortHandle2.set(structSegmt1, (short)45);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		shortHandle1.set(structSegmt2, (short)78);
		shortHandle2.set(structSegmt2, (short)67);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)134);
		Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)112);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add2ShortStructs_returnStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2ShortStructs_returnStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		shortHandle1.set(structSegmt1, (short)56);
		shortHandle2.set(structSegmt1, (short)45);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		shortHandle1.set(structSegmt2, (short)78);
		shortHandle2.set(structSegmt2, (short)67);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(structLayout.byteSize());
		Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)134);
		Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)112);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add3ShortStructs_returnStruct() throws Throwable  {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"),
				C_SHORT.withName("elem3"), MemoryLayout.ofPaddingBits(C_SHORT.bitSize()));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));
		VarHandle shortHandle3 = structLayout.varHandle(short.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add3ShortStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		shortHandle1.set(structSegmt1, (short)25);
		shortHandle2.set(structSegmt1, (short)26);
		shortHandle3.set(structSegmt1, (short)27);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		shortHandle1.set(structSegmt2, (short)34);
		shortHandle2.set(structSegmt2, (short)35);
		shortHandle3.set(structSegmt2, (short)36);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals((short)shortHandle1.get(resultSegmt), (short)59);
		Assert.assertEquals((short)shortHandle2.get(resultSegmt), (short)61);
		Assert.assertEquals((short)shortHandle3.get(resultSegmt), (short)63);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_addIntAndIntsFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntsFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		intHandle1.set(structSegmt, 1122334);
		intHandle2.set(structSegmt, 1234567);

		int result = (int)mh.invokeExact(2244668, structSegmt);
		Assert.assertEquals(result, 4601569);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndIntShortFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"),
				C_SHORT.withName("elem2"), MemoryLayout.ofPaddingBits(C_SHORT.bitSize()));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntShortFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		elemHandle1.set(structSegmt, 11223344);
		elemHandle2.set(structSegmt, (short)32766);

		int result = (int)mh.invokeExact(22334455, structSegmt);
		Assert.assertEquals(result, 33590565);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndShortIntFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_SHORT.withName("elem1"),
				MemoryLayout.ofPaddingBits(C_SHORT.bitSize()), C_INT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndShortIntFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		elemHandle1.set(structSegmt, (short)32766);
		elemHandle2.set(structSegmt, 22446688);

		int result = (int)mh.invokeExact(11335577, structSegmt);
		Assert.assertEquals(result, 33815031);
		structSegmt.close();
	}

	@Test
	public void test_addIntFromPointerAndIntsFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntFromPointerAndIntsFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment intSegmt = MemorySegment.allocateNative(C_INT);
		MemoryAccess.setInt(intSegmt, 7654321);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		intHandle1.set(structSegmt, 1234567);
		intHandle2.set(structSegmt, 2468024);

		int result = (int)mh.invokeExact(intSegmt.address(), structSegmt);
		Assert.assertEquals(result, 11356912);
		structSegmt.close();
		intSegmt.close();
	}

	@Test
	public void test_addIntFromPointerAndIntsFromStruct_returnIntPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntFromPointerAndIntsFromStruct_returnIntPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment intSegmt = MemorySegment.allocateNative(C_INT);
		MemoryAccess.setInt(intSegmt, 1122333);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		intHandle1.set(structSegmt, 4455666);
		intHandle2.set(structSegmt, 7788999);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(intSegmt.address(), structSegmt);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(C_INT.byteSize());
		VarHandle intHandle = MemoryHandles.varHandle(int.class, ByteOrder.nativeOrder());
		int result = (int)intHandle.get(resultSegmt, 0);
		Assert.assertEquals(result, 13366998);
		Assert.assertEquals(resultSegmt.address().toRawLongValue(), intSegmt.address().toRawLongValue());
		intSegmt.close();
		structSegmt.close();
		resultSegmt.close();
	}

	@Test
	public void test_addIntAndIntsFromStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntsFromStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		intHandle1.set(structSegmt, 11121314);
		intHandle2.set(structSegmt, 15161718);

		int result = (int)mh.invokeExact(19202122, structSegmt.address());
		Assert.assertEquals(result, 45485154);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndIntsFromNestedStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout.withName("struct_elem1"), C_INT.withName("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntsFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 21222324);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 25262728);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 29303132);

		int result = (int)mh.invokeExact(33343536, structSegmt);
		Assert.assertEquals(result, 109131720);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndIntsFromNestedStruct_reverseOrder() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntsFromNestedStruct_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 21222324);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 25262728);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 29303132);

		int result = (int)mh.invokeExact(33343536, structSegmt);
		Assert.assertEquals(result, 109131720);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndIntsFromNestedStruct_withoutLayoutName() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_INT, C_INT);
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout, C_INT);

		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntsFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 21222324);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 25262728);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 29303132);

		int result = (int)mh.invokeExact(33343536, structSegmt);
		Assert.assertEquals(result, 109131720);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedIntArray() throws Throwable {
		SequenceLayout intArray = MemoryLayout.ofSequence(2, C_INT);
		GroupLayout structLayout = MemoryLayout.ofStruct(intArray.withName("array_elem1"), C_INT.withName("elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntsFromStructWithNestedIntArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 1111111);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 2222222);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 3333333);

		int result = (int)mh.invokeExact(4444444, structSegmt);
		Assert.assertEquals(result, 11111110);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedIntArray_reverseOrder() throws Throwable {
		SequenceLayout intArray = MemoryLayout.ofSequence(2, C_INT);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), intArray.withName("array_elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntsFromStructWithNestedIntArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 1111111);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 2222222);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 3333333);

		int result = (int)mh.invokeExact(4444444, structSegmt);
		Assert.assertEquals(result, 11111110);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedIntArray_withoutLayoutName() throws Throwable {
		SequenceLayout intArray = MemoryLayout.ofSequence(2, C_INT);
		GroupLayout structLayout = MemoryLayout.ofStruct(intArray, C_INT);
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntsFromStructWithNestedIntArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 1111111);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 2222222);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 3333333);

		int result = (int)mh.invokeExact(4444444, structSegmt);
		Assert.assertEquals(result, 11111110);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedStructArray() throws Throwable {
		GroupLayout intStruct = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, intStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray.withName("struct_array_elem1"), C_INT.withName("elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 1111111);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 2222222);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 3333333);
		MemoryAccess.setIntAtOffset(structSegmt, 12, 4444444);
		MemoryAccess.setIntAtOffset(structSegmt, 16, 5555555);

		int result = (int)mh.invokeExact(6666666, structSegmt);
		Assert.assertEquals(result, 23333331);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedStructArray_reverseOrder() throws Throwable {
		GroupLayout intStruct = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, intStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), structArray.withName("struct_array_elem2"));
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntsFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 1111111);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 2222222);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 3333333);
		MemoryAccess.setIntAtOffset(structSegmt, 12, 4444444);
		MemoryAccess.setIntAtOffset(structSegmt, 16, 5555555);

		int result = (int)mh.invokeExact(6666666, structSegmt);
		Assert.assertEquals(result, 23333331);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndIntsFromStructWithNestedStructArray_withoutLayoutName() throws Throwable {
		GroupLayout intStruct = MemoryLayout.ofStruct(C_INT, C_INT);
		SequenceLayout structArray = MemoryLayout.ofSequence(2, intStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray, C_INT);
		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setIntAtOffset(structSegmt, 0, 1111111);
		MemoryAccess.setIntAtOffset(structSegmt, 4, 2222222);
		MemoryAccess.setIntAtOffset(structSegmt, 8, 3333333);
		MemoryAccess.setIntAtOffset(structSegmt, 12, 4444444);
		MemoryAccess.setIntAtOffset(structSegmt, 16, 5555555);

		int result = (int)mh.invokeExact(6666666, structSegmt);
		Assert.assertEquals(result, 23333331);
		structSegmt.close();
	}

	@Test
	public void test_add2IntStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2IntStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		intHandle1.set(structSegmt1, 11223344);
		intHandle2.set(structSegmt1, 55667788);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		intHandle1.set(structSegmt2, 99001122);
		intHandle2.set(structSegmt2, 33445566);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals(intHandle1.get(resultSegmt), 110224466);
		Assert.assertEquals(intHandle2.get(resultSegmt), 89113354);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add2IntStructs_returnStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2IntStructs_returnStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		intHandle1.set(structSegmt1, 11223344);
		intHandle2.set(structSegmt1, 55667788);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		intHandle1.set(structSegmt2, 99001122);
		intHandle2.set(structSegmt2, 33445566);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(structLayout.byteSize());
		Assert.assertEquals(intHandle1.get(resultSegmt), 110224466);
		Assert.assertEquals(intHandle2.get(resultSegmt), 89113354);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add3IntStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_INT.withName("elem2"), C_INT.withName("elem3"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));
		VarHandle intHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add3IntStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		intHandle1.set(structSegmt1, 11223344);
		intHandle2.set(structSegmt1, 55667788);
		intHandle3.set(structSegmt1, 99001122);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		intHandle1.set(structSegmt2, 33445566);
		intHandle2.set(structSegmt2, 77889900);
		intHandle3.set(structSegmt2, 44332211);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals(intHandle1.get(resultSegmt), 44668910);
		Assert.assertEquals(intHandle2.get(resultSegmt), 133557688);
		Assert.assertEquals(intHandle3.get(resultSegmt), 143333333);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_addLongAndLongsFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongAndLongsFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		longHandle1.set(structSegmt, 1234567890L);
		longHandle2.set(structSegmt, 9876543210L);
		long result = (long)mh.invokeExact(2468024680L, structSegmt);
		Assert.assertEquals(result, 13579135780L);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndIntLongFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"),
				MemoryLayout.ofPaddingBits(C_INT.bitSize()), longLayout.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(long.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndIntLongFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		elemHandle1.set(structSegmt, 11223344);
		elemHandle2.set(structSegmt, 667788990011L);

		long result = (long)mh.invokeExact(22446688, structSegmt);
		Assert.assertEquals(result, 667822660043L);
		structSegmt.close();
	}

	@Test
	public void test_addIntAndLongIntFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"),
				C_INT.withName("elem2"), MemoryLayout.ofPaddingBits(C_INT.bitSize()));
		VarHandle elemHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(long.class, int.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_INT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addIntAndLongIntFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		elemHandle1.set(structSegmt, 667788990011L);
		elemHandle2.set(structSegmt, 11223344);

		long result = (long)mh.invokeExact(1234567, structSegmt);
		Assert.assertEquals(result, 667801447922L);
		structSegmt.close();
	}

	@Test
	public void test_addLongFromPointerAndLongsFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongFromPointerAndLongsFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment longSegmt = MemorySegment.allocateNative(longLayout);
		MemoryAccess.setLong(longSegmt, 1111111111L);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		longHandle1.set(structSegmt, 3333333333L);
		longHandle2.set(structSegmt, 5555555555L);

		long result = (long)mh.invokeExact(longSegmt.address(), structSegmt);
		Assert.assertEquals(result, 9999999999L);
		longSegmt.close();
		structSegmt.close();
	}

	@Test
	public void test_addLongFromPointerAndLongsFromStruct_returnLongPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongFromPointerAndLongsFromStruct_returnLongPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment longSegmt = MemorySegment.allocateNative(longLayout);
		MemoryAccess.setLong(longSegmt, 1122334455L);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		longHandle1.set(structSegmt, 6677889900L);
		longHandle2.set(structSegmt, 1234567890L);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(longSegmt.address(), structSegmt);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(longLayout.byteSize());
		VarHandle longHandle = MemoryHandles.varHandle(long.class, ByteOrder.nativeOrder());
		long result = (long)longHandle.get(resultSegmt, 0);
		Assert.assertEquals(result, 9034792245L);
		Assert.assertEquals(resultSegmt.address().toRawLongValue(), longSegmt.address().toRawLongValue());
		longSegmt.close();
		structSegmt.close();
		resultSegmt.close();
	}

	@Test
	public void test_addLongAndLongsFromStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(long.class, long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, C_POINTER);
		Symbol functionSymbol = nativeLib.lookup("addLongAndLongsFromStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		longHandle1.set(structSegmt, 224466880022L);
		longHandle2.set(structSegmt, 446688002244L);

		long result = (long)mh.invokeExact(668800224466L, structSegmt.address());
		Assert.assertEquals(result, 1339955106732L);
		structSegmt.close();
	}

	@Test
	public void test_addLongAndLongsFromNestedStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), longLayout.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongAndLongsFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setLongAtOffset(structSegmt, 0, 135791357913L);
		MemoryAccess.setLongAtOffset(structSegmt, 8, 246802468024L);
		MemoryAccess.setLongAtOffset(structSegmt, 16,112233445566L);

		long result = (long)mh.invokeExact(778899001122L, structSegmt);
		Assert.assertEquals(result, 1273726272625L);
		structSegmt.close();
	}

	@Test
	public void test_addLongAndLongsFromNestedStruct_reverseOrder() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), longLayout.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongAndLongsFromNestedStruct_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setLongAtOffset(structSegmt, 0, 135791357913L);
		MemoryAccess.setLongAtOffset(structSegmt, 8, 246802468024L);
		MemoryAccess.setLongAtOffset(structSegmt, 16,112233445566L);

		long result = (long)mh.invokeExact(778899001122L, structSegmt);
		Assert.assertEquals(result, 1273726272625L);
		structSegmt.close();
	}

	@Test
	public void test_addLongAndLongsFromNestedStruct_withoutLayoutName() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(longLayout, longLayout);
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout, nestedStructLayout);
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongAndLongsFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setLongAtOffset(structSegmt, 0, 135791357913L);
		MemoryAccess.setLongAtOffset(structSegmt, 8, 246802468024L);
		MemoryAccess.setLongAtOffset(structSegmt, 16,112233445566L);

		long result = (long)mh.invokeExact(778899001122L, structSegmt);
		Assert.assertEquals(result, 1273726272625L);
		structSegmt.close();
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedLongArray() throws Throwable {
		SequenceLayout longArray = MemoryLayout.ofSequence(2, longLayout);
		GroupLayout structLayout = MemoryLayout.ofStruct(longArray.withName("array_elem1"), longLayout.withName("elem2"));
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongAndLongsFromStructWithNestedLongArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setLongAtOffset(structSegmt, 0, 111111111L);
		MemoryAccess.setLongAtOffset(structSegmt, 8, 222222222L);
		MemoryAccess.setLongAtOffset(structSegmt, 16, 333333333L);

		long result = (long)mh.invokeExact(444444444L, structSegmt);
		Assert.assertEquals(result, 1111111110L);
		structSegmt.close();
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedLongArray_reverseOrder() throws Throwable {
		SequenceLayout longArray = MemoryLayout.ofSequence(2, longLayout);
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), longArray.withName("array_elem2"));
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongAndLongsFromStructWithNestedLongArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setLongAtOffset(structSegmt, 0, 111111111L);
		MemoryAccess.setLongAtOffset(structSegmt, 8, 222222222L);
		MemoryAccess.setLongAtOffset(structSegmt, 16, 333333333L);

		long result = (long)mh.invokeExact(444444444L, structSegmt);
		Assert.assertEquals(result, 1111111110L);
		structSegmt.close();
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedLongArray_withoutLayoutName() throws Throwable {
		SequenceLayout longArray = MemoryLayout.ofSequence(2, longLayout);
		GroupLayout structLayout = MemoryLayout.ofStruct(longArray, longLayout);
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongAndLongsFromStructWithNestedLongArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setLongAtOffset(structSegmt, 0, 111111111L);
		MemoryAccess.setLongAtOffset(structSegmt, 8, 222222222L);
		MemoryAccess.setLongAtOffset(structSegmt, 16, 333333333L);

		long result = (long)mh.invokeExact(444444444L, structSegmt);
		Assert.assertEquals(result, 1111111110L);
		structSegmt.close();
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedStructArray() throws Throwable {
		GroupLayout longStruct = MemoryLayout.ofStruct(longLayout.withName("elem1"), longLayout.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, longStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray.withName("struct_array_elem1"), longLayout.withName("elem2"));
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongAndLongsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setLongAtOffset(structSegmt, 0, 111111111L);
		MemoryAccess.setLongAtOffset(structSegmt, 8, 222222222L);
		MemoryAccess.setLongAtOffset(structSegmt, 16, 333333333L);
		MemoryAccess.setLongAtOffset(structSegmt, 24, 444444444L);
		MemoryAccess.setLongAtOffset(structSegmt, 32, 555555555L);

		long result = (long)mh.invokeExact(666666666L, structSegmt);
		Assert.assertEquals(result, 2333333331L);
		structSegmt.close();
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedStructArray_reverseOrder() throws Throwable {
		GroupLayout longStruct = MemoryLayout.ofStruct(longLayout.withName("elem1"), longLayout.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, longStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), structArray.withName("struct_array_elem2"));
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongAndLongsFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setLongAtOffset(structSegmt, 0, 111111111L);
		MemoryAccess.setLongAtOffset(structSegmt, 8, 222222222L);
		MemoryAccess.setLongAtOffset(structSegmt, 16, 333333333L);
		MemoryAccess.setLongAtOffset(structSegmt, 24, 444444444L);
		MemoryAccess.setLongAtOffset(structSegmt, 32, 555555555L);

		long result = (long)mh.invokeExact(666666666L, structSegmt);
		Assert.assertEquals(result, 2333333331L);
		structSegmt.close();
	}

	@Test
	public void test_addLongAndLongsFromStructWithNestedStructArray_withoutLayoutName() throws Throwable {
		GroupLayout longStruct = MemoryLayout.ofStruct(longLayout, longLayout);
		SequenceLayout structArray = MemoryLayout.ofSequence(2, longStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray, longLayout);
		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addLongAndLongsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setLongAtOffset(structSegmt, 0, 111111111L);
		MemoryAccess.setLongAtOffset(structSegmt, 8, 222222222L);
		MemoryAccess.setLongAtOffset(structSegmt, 16, 333333333L);
		MemoryAccess.setLongAtOffset(structSegmt, 24, 444444444L);
		MemoryAccess.setLongAtOffset(structSegmt, 32, 555555555L);

		long result = (long)mh.invokeExact(666666666L, structSegmt);
		Assert.assertEquals(result, 2333333331L);
		structSegmt.close();
	}

	@Test
	public void test_add2LongStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2LongStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		longHandle1.set(structSegmt1, 987654321987L);
		longHandle2.set(structSegmt1, 123456789123L);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		longHandle1.set(structSegmt2, 224466880022L);
		longHandle2.set(structSegmt2, 113355779911L);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals(longHandle1.get(resultSegmt), 1212121202009L);
		Assert.assertEquals(longHandle2.get(resultSegmt), 236812569034L);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add2LongStructs_returnStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2LongStructs_returnStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		longHandle1.set(structSegmt1, 1122334455L);
		longHandle2.set(structSegmt1, 5566778899L);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		longHandle1.set(structSegmt2, 9900112233L);
		longHandle2.set(structSegmt2, 3344556677L);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(structLayout.byteSize());
		Assert.assertEquals(longHandle1.get(resultSegmt), 11022446688L);
		Assert.assertEquals(longHandle2.get(resultSegmt), 8911335576L);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add3LongStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(longLayout.withName("elem1"), longLayout.withName("elem2"), longLayout.withName("elem3"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));
		VarHandle longHandle3 = structLayout.varHandle(long.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add3LongStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		longHandle1.set(structSegmt1, 987654321987L);
		longHandle2.set(structSegmt1, 123456789123L);
		longHandle3.set(structSegmt1, 112233445566L);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		longHandle1.set(structSegmt2, 224466880022L);
		longHandle2.set(structSegmt2, 113355779911L);
		longHandle3.set(structSegmt2, 778899001122L);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals(longHandle1.get(resultSegmt), 1212121202009L);
		Assert.assertEquals(longHandle2.get(resultSegmt), 236812569034L);
		Assert.assertEquals(longHandle3.get(resultSegmt), 891132446688L);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_addFloatAndFloatsFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addFloatAndFloatsFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		floatHandle1.set(structSegmt, 8.12F);
		floatHandle2.set(structSegmt, 9.24F);
		float result = (float)mh.invokeExact(6.56F, structSegmt);
		Assert.assertEquals(result, 23.92F, 0.01F);
		structSegmt.close();
	}

	@Test
	public void test_addFloatFromPointerAndFloatsFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(float.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addFloatFromPointerAndFloatsFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment floatSegmt = MemorySegment.allocateNative(C_FLOAT);
		MemoryAccess.setFloat(floatSegmt, 12.12F);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		floatHandle1.set(structSegmt, 18.23F);
		floatHandle2.set(structSegmt, 19.34F);

		float result = (float)mh.invokeExact(floatSegmt.address(), structSegmt);
		Assert.assertEquals(result, 49.69F, 0.01F);
		structSegmt.close();
		floatSegmt.close();
	}

	@Test
	public void test_addFloatFromPointerAndFloatsFromStruct_returnFloatPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addFloatFromPointerAndFloatsFromStruct_returnFloatPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment floatSegmt = MemorySegment.allocateNative(C_FLOAT);
		MemoryAccess.setFloat(floatSegmt, 12.12F);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		floatHandle1.set(structSegmt, 18.23F);
		floatHandle2.set(structSegmt, 19.34F);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(floatSegmt.address(), structSegmt);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(C_FLOAT.byteSize());
		VarHandle floatHandle = MemoryHandles.varHandle(float.class, ByteOrder.nativeOrder());
		float result = (float)floatHandle.get(resultSegmt, 0);
		Assert.assertEquals(result, 49.69F, 0.01F);
		Assert.assertEquals(resultSegmt.address().toRawLongValue(), floatSegmt.address().toRawLongValue());
		floatSegmt.close();
		structSegmt.close();
		resultSegmt.close();
	}

	@Test
	public void test_addFloatAndFloatsFromStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(float.class, float.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER);
		Symbol functionSymbol = nativeLib.lookup("addFloatAndFloatsFromStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		floatHandle1.set(structSegmt, 35.11F);
		floatHandle2.set(structSegmt, 46.22F);

		float result = (float)mh.invokeExact(79.33F, structSegmt.address());
		Assert.assertEquals(result, 160.66F, 0.01F);
		structSegmt.close();
	}

	@Test
	public void test_addFloatAndFloatsFromNestedStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout.withName("struct_elem1"), C_FLOAT.withName("elem2"));
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addFloatAndFloatsFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setFloatAtOffset(structSegmt, 0, 31.22F);
		MemoryAccess.setFloatAtOffset(structSegmt, 4, 33.44F);
		MemoryAccess.setFloatAtOffset(structSegmt, 8, 35.66F);

		float result = (float)mh.invokeExact(37.88F, structSegmt);
		Assert.assertEquals(result, 138.2F, 0.01F);
		structSegmt.close();
	}

	@Test
	public void test_addFloatAndFloatsFromNestedStruct_reverseOrder() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), nestedStructLayout.withName("struct_elem2"));
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addFloatAndFloatsFromNestedStruct_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setFloatAtOffset(structSegmt, 0, 31.22F);
		MemoryAccess.setFloatAtOffset(structSegmt, 4, 33.44F);
		MemoryAccess.setFloatAtOffset(structSegmt, 8, 35.66F);

		float result = (float)mh.invokeExact(37.88F, structSegmt);
		Assert.assertEquals(result, 138.2F, 0.01F);
		structSegmt.close();
	}

	@Test
	public void test_addFloatAndFloatsFromNestedStruct_withoutLayoutName() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_FLOAT, C_FLOAT);
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout, C_FLOAT);

		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addFloatAndFloatsFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setFloatAtOffset(structSegmt, 0, 31.22F);
		MemoryAccess.setFloatAtOffset(structSegmt, 4, 33.44F);
		MemoryAccess.setFloatAtOffset(structSegmt, 8, 35.66F);

		float result = (float)mh.invokeExact(37.88F, structSegmt);
		Assert.assertEquals(result, 138.2F, 0.01F);
		structSegmt.close();
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedFloatArray() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.ofSequence(2, C_FLOAT);
		GroupLayout structLayout = MemoryLayout.ofStruct(floatArray.withName("array_elem1"), C_FLOAT.withName("elem2"));
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addFloatAndFloatsFromStructWithNestedFloatArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setFloatAtOffset(structSegmt, 0, 111.11F);
		MemoryAccess.setFloatAtOffset(structSegmt, 4, 222.22F);
		MemoryAccess.setFloatAtOffset(structSegmt, 8, 333.33F);

		float result = (float)mh.invokeExact(444.44F, structSegmt);
		Assert.assertEquals(result, 1111.1F, 0.01F);
		structSegmt.close();
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.ofSequence(2, C_FLOAT);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), floatArray.withName("array_elem2"));
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setFloatAtOffset(structSegmt, 0, 111.11F);
		MemoryAccess.setFloatAtOffset(structSegmt, 4, 222.22F);
		MemoryAccess.setFloatAtOffset(structSegmt, 8, 333.33F);

		float result = (float)mh.invokeExact(444.44F, structSegmt);
		Assert.assertEquals(result, 1111.1F, 0.01F);
		structSegmt.close();
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedFloatArray_withoutLayoutName() throws Throwable {
		SequenceLayout floatArray = MemoryLayout.ofSequence(2, C_FLOAT);
		GroupLayout structLayout = MemoryLayout.ofStruct(floatArray, C_FLOAT);
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addFloatAndFloatsFromStructWithNestedFloatArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setFloatAtOffset(structSegmt, 0, 111.11F);
		MemoryAccess.setFloatAtOffset(structSegmt, 4, 222.22F);
		MemoryAccess.setFloatAtOffset(structSegmt, 8, 333.33F);

		float result = (float)mh.invokeExact(444.44F, structSegmt);
		Assert.assertEquals(result, 1111.1F, 0.01F);
		structSegmt.close();
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedStructArray() throws Throwable {
		GroupLayout floatStruct = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, floatStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray.withName("struct_array_elem1"), C_FLOAT.withName("elem2"));
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addFloatAndFloatsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setFloatAtOffset(structSegmt, 0, 111.11F);
		MemoryAccess.setFloatAtOffset(structSegmt, 4, 222.22F);
		MemoryAccess.setFloatAtOffset(structSegmt, 8, 333.33F);
		MemoryAccess.setFloatAtOffset(structSegmt, 12, 444.44F);
		MemoryAccess.setFloatAtOffset(structSegmt, 16, 555.55F);

		float result = (float)mh.invokeExact(666.66F, structSegmt);
		Assert.assertEquals(result, 2333.31F, 0.01F);
		structSegmt.close();
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder() throws Throwable {
		GroupLayout floatStruct = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, floatStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), structArray.withName("struct_array_elem2"));
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setFloatAtOffset(structSegmt, 0, 111.11F);
		MemoryAccess.setFloatAtOffset(structSegmt, 4, 222.22F);
		MemoryAccess.setFloatAtOffset(structSegmt, 8, 333.33F);
		MemoryAccess.setFloatAtOffset(structSegmt, 12, 444.44F);
		MemoryAccess.setFloatAtOffset(structSegmt, 16, 555.55F);

		float result = (float)mh.invokeExact(666.66F, structSegmt);
		Assert.assertEquals(result, 2333.31F, 0.01F);
		structSegmt.close();
	}

	@Test
	public void test_addFloatAndFloatsFromStructWithNestedStructArray_withoutLayoutName() throws Throwable {
		GroupLayout floatStruct = MemoryLayout.ofStruct(C_FLOAT, C_FLOAT);
		SequenceLayout structArray = MemoryLayout.ofSequence(2, floatStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray, C_FLOAT);
		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addFloatAndFloatsFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setFloatAtOffset(structSegmt, 0, 111.11F);
		MemoryAccess.setFloatAtOffset(structSegmt, 4, 222.22F);
		MemoryAccess.setFloatAtOffset(structSegmt, 8, 333.33F);
		MemoryAccess.setFloatAtOffset(structSegmt, 12, 444.44F);
		MemoryAccess.setFloatAtOffset(structSegmt, 16, 555.55F);

		float result = (float)mh.invokeExact(666.66F, structSegmt);
		Assert.assertEquals(result, 2333.31F, 0.01F);
		structSegmt.close();
	}

	@Test
	public void test_add2FloatStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2FloatStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		floatHandle1.set(structSegmt1, 25.12F);
		floatHandle2.set(structSegmt1, 11.23F);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		floatHandle1.set(structSegmt2, 24.34F);
		floatHandle2.set(structSegmt2, 13.45F);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals((float)floatHandle1.get(resultSegmt), 49.46F, 0.01F);
		Assert.assertEquals((float)floatHandle2.get(resultSegmt), 24.68F, 0.01F);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add2FloatStructs_returnStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2FloatStructs_returnStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		floatHandle1.set(structSegmt1, 25.12F);
		floatHandle2.set(structSegmt1, 11.23F);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		floatHandle1.set(structSegmt2, 24.34F);
		floatHandle2.set(structSegmt2, 13.45F);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(structLayout.byteSize());
		Assert.assertEquals((float)floatHandle1.get(resultSegmt), 49.46F, 0.01F);
		Assert.assertEquals((float)floatHandle2.get(resultSegmt), 24.68F, 0.01F);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add3FloatStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"),  C_FLOAT.withName("elem3"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle floatHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add3FloatStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		floatHandle1.set(structSegmt1, 25.12F);
		floatHandle2.set(structSegmt1, 11.23F);
		floatHandle3.set(structSegmt1, 45.67F);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		floatHandle1.set(structSegmt2, 24.34F);
		floatHandle2.set(structSegmt2, 13.45F);
		floatHandle3.set(structSegmt2, 69.72F);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals((float)floatHandle1.get(resultSegmt), 49.46F, 0.01F);
		Assert.assertEquals((float)floatHandle2.get(resultSegmt), 24.68F, 0.01F);
		Assert.assertEquals((float)floatHandle3.get(resultSegmt), 115.39, 0.01F);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoublesFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoublesFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		doubleHandle1.set(structSegmt, 2228.111D);
		doubleHandle2.set(structSegmt, 2229.221D);
		double result = (double)mh.invokeExact(3336.333D, structSegmt);
		Assert.assertEquals(result, 7793.665D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndFloatDoubleFromStruct() throws Throwable {
		GroupLayout structLayout = null;
		MemorySegment structSegmt = null;

		/* The size of [float, double] on AIX/PPC 64-bit is 12 bytes without padding by default
		 * while the same struct is 16 bytes with padding on other platforms.
		 */
		if (isAixOS) {
			structLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"), C_DOUBLE.withName("elem2"));
			structSegmt = MemorySegment.allocateNative(structLayout);
			MemoryAccess.setFloatAtOffset(structSegmt, 0, 18.444F);
			MemoryAccess.setDoubleAtOffset(structSegmt, 4, 619.777D);
		} else {
			structLayout = MemoryLayout.ofStruct(C_FLOAT.withName("elem1"),
					MemoryLayout.ofPaddingBits(C_FLOAT.bitSize()), C_DOUBLE.withName("elem2"));
			VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
			VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));
			structSegmt = MemorySegment.allocateNative(structLayout);
			elemHandle1.set(structSegmt, 18.444F);
			elemHandle2.set(structSegmt, 619.777D);
		}

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndFloatDoubleFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		double result = (double)mh.invokeExact(113.567D, structSegmt);
		Assert.assertEquals(result, 751.788D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndIntDoubleFromStruct() throws Throwable {
		GroupLayout structLayout = null;
		MemorySegment structSegmt = null;

		/* The size of [int, double] on AIX/PPC 64-bit is 12 bytes without padding by default
		 * while the same struct is 16 bytes with padding on other platforms.
		 */
		if (isAixOS) {
			structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"), C_DOUBLE.withName("elem2"));
			structSegmt = MemorySegment.allocateNative(structLayout);
			MemoryAccess.setIntAtOffset(structSegmt, 0, 18);
			MemoryAccess.setDoubleAtOffset(structSegmt, 4, 619.777D);
		} else {
			structLayout = MemoryLayout.ofStruct(C_INT.withName("elem1"),
					MemoryLayout.ofPaddingBits(C_INT.bitSize()), C_DOUBLE.withName("elem2"));
			VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
			VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));
			structSegmt = MemorySegment.allocateNative(structLayout);
			elemHandle1.set(structSegmt, 18);
			elemHandle2.set(structSegmt, 619.777D);
		}

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndIntDoubleFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		double result = (double)mh.invokeExact(113.567D, structSegmt);
		Assert.assertEquals(result, 751.344D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoubleFloatFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoubleFloatFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		elemHandle1.set(structSegmt, 218.555D);
		elemHandle2.set(structSegmt, 19.22F);

		double result = (double)mh.invokeExact(216.666D, structSegmt);
		Assert.assertEquals(result, 454.441D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoubleIntFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_INT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoubleIntFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		elemHandle1.set(structSegmt, 218.555D);
		elemHandle2.set(structSegmt, 19);

		double result = (double)mh.invokeExact(216.666D, structSegmt);
		Assert.assertEquals(result, 454.221D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleFromPointerAndDoublesFromStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleFromPointerAndDoublesFromStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment doubleSegmt = MemorySegment.allocateNative(C_DOUBLE);
		MemoryAccess.setDouble(doubleSegmt, 112.123D);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		doubleHandle1.set(structSegmt, 118.456D);
		doubleHandle2.set(structSegmt, 119.789D);

		double result = (double)mh.invokeExact(doubleSegmt.address(), structSegmt);
		Assert.assertEquals(result, 350.368D, 0.001D);
		doubleSegmt.close();
		structSegmt.close();
	}

	@Test
	public void test_addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment doubleSegmt = MemorySegment.allocateNative(C_DOUBLE);
		MemoryAccess.setDouble(doubleSegmt, 212.123D);
		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		doubleHandle1.set(structSegmt, 218.456D);
		doubleHandle2.set(structSegmt, 219.789D);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(doubleSegmt.address(), structSegmt);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(C_DOUBLE.byteSize());
		VarHandle doubleHandle = MemoryHandles.varHandle(double.class, ByteOrder.nativeOrder());
		double result = (double)doubleHandle.get(resultSegmt, 0);
		Assert.assertEquals(result, 650.368D, 0.001D);
		Assert.assertEquals(resultSegmt.address().toRawLongValue(), doubleSegmt.address().toRawLongValue());
		doubleSegmt.close();
		structSegmt.close();
		resultSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoublesFromStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_POINTER);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoublesFromStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		doubleHandle1.set(structSegmt, 22.111D);
		doubleHandle2.set(structSegmt, 44.222D);

		double result = (double)mh.invokeExact(66.333D, structSegmt.address());
		Assert.assertEquals(result, 132.666D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedStruct() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout.withName("struct_elem1"), C_DOUBLE.withName("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoublesFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setDoubleAtOffset(structSegmt, 0, 31.789D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 8, 33.456D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 16, 35.123D);

		double result = (double)mh.invokeExact(37.864D, structSegmt);
		Assert.assertEquals(result, 138.232D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedStruct_reverseOrder() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		GroupLayout structLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), nestedStructLayout.withName("struct_elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoublesFromNestedStruct_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setDoubleAtOffset(structSegmt, 0, 31.789D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 8, 33.456D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 16, 35.123D);

		double result = (double)mh.invokeExact(37.864D, structSegmt);
		Assert.assertEquals(result, 138.232D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoublesFromNestedStruct_withoutLayoutName() throws Throwable {
		GroupLayout nestedStructLayout = MemoryLayout.ofStruct(C_DOUBLE, C_DOUBLE);
		GroupLayout structLayout = MemoryLayout.ofStruct(nestedStructLayout, C_DOUBLE);

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoublesFromNestedStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setDoubleAtOffset(structSegmt, 0, 31.789D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 8, 33.456D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 16, 35.123D);

		double result = (double)mh.invokeExact(37.864D, structSegmt);
		Assert.assertEquals(result, 138.232D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedDoubleArray() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.ofSequence(2, C_DOUBLE);
		GroupLayout structLayout = MemoryLayout.ofStruct(doubleArray.withName("array_elem1"), C_DOUBLE.withName("elem2"));
		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoublesFromStructWithNestedDoubleArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setDoubleAtOffset(structSegmt, 0, 111.111D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 8, 222.222D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 16, 333.333D);

		double result = (double)mh.invokeExact(444.444D, structSegmt);
		Assert.assertEquals(result, 1111.11D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.ofSequence(2, C_DOUBLE);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), doubleArray.withName("array_elem2"));
		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setDoubleAtOffset(structSegmt, 0, 111.111D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 8, 222.222D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 16, 333.333D);

		double result = (double)mh.invokeExact(444.444D, structSegmt);
		Assert.assertEquals(result, 1111.11D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedDoubleArray_withoutLayoutName() throws Throwable {
		SequenceLayout doubleArray = MemoryLayout.ofSequence(2, C_DOUBLE);
		GroupLayout structLayout = MemoryLayout.ofStruct(doubleArray, C_DOUBLE);
		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoublesFromStructWithNestedDoubleArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setDoubleAtOffset(structSegmt, 0, 111.111D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 8, 222.222D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 16, 333.333D);

		double result = (double)mh.invokeExact(444.444D, structSegmt);
		Assert.assertEquals(result, 1111.11D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedStructArray() throws Throwable {
		GroupLayout doubleStruct = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, doubleStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray.withName("struct_array_elem1"), C_DOUBLE.withName("elem2"));
		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoublesFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setDoubleAtOffset(structSegmt, 0, 111.111D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 8, 222.222D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 16, 333.333D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 24, 444.444D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 32, 555.555D);

		double result = (double)mh.invokeExact(666.666D, structSegmt);
		Assert.assertEquals(result, 2333.331D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder() throws Throwable {
		GroupLayout doubleStruct = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		SequenceLayout structArray = MemoryLayout.ofSequence(2, doubleStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), structArray.withName("struct_array_elem2"));
		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setDoubleAtOffset(structSegmt, 0, 111.111D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 8, 222.222D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 16, 333.333D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 24, 444.444D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 32, 555.555D);

		double result = (double)mh.invokeExact(666.666D, structSegmt);
		Assert.assertEquals(result, 2333.331D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_addDoubleAndDoublesFromStructWithNestedStructArray_withoutLayoutName() throws Throwable {
		GroupLayout doubleStruct = MemoryLayout.ofStruct(C_DOUBLE, C_DOUBLE);
		SequenceLayout structArray = MemoryLayout.ofSequence(2, doubleStruct);
		GroupLayout structLayout = MemoryLayout.ofStruct(structArray, C_DOUBLE);
		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout);
		Symbol functionSymbol = nativeLib.lookup("addDoubleAndDoublesFromStructWithNestedStructArray").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.allocateNative(structLayout);
		MemoryAccess.setDoubleAtOffset(structSegmt, 0, 111.111D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 8, 222.222D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 16, 333.333D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 24, 444.444D);
		MemoryAccess.setDoubleAtOffset(structSegmt, 32, 555.555D);

		double result = (double)mh.invokeExact(666.666D, structSegmt);
		Assert.assertEquals(result, 2333.331D, 0.001D);
		structSegmt.close();
	}

	@Test
	public void test_add2DoubleStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2DoubleStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		doubleHandle1.set(structSegmt1, 11.222D);
		doubleHandle2.set(structSegmt1, 22.333D);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		doubleHandle1.set(structSegmt2, 33.444D);
		doubleHandle2.set(structSegmt2, 44.555D);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 44.666D, 0.001D);
		Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 66.888D, 0.001D);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add2DoubleStructs_returnStructPointer() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add2DoubleStructs_returnStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		doubleHandle1.set(structSegmt1, 11.222D);
		doubleHandle2.set(structSegmt1, 22.333D);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		doubleHandle1.set(structSegmt2, 33.444D);
		doubleHandle2.set(structSegmt2, 44.555D);

		MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2);
		MemorySegment resultSegmt = resultAddr.asSegmentRestricted(structLayout.byteSize());
		Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 44.666D, 0.001D);
		Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 66.888D, 0.001D);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}

	@Test
	public void test_add3DoubleStructs_returnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.ofStruct(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"), C_DOUBLE.withName("elem3"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));
		VarHandle doubleHandle3 = structLayout.varHandle(double.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Symbol functionSymbol = nativeLib.lookup("add3DoubleStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt1 = MemorySegment.allocateNative(structLayout);
		doubleHandle1.set(structSegmt1, 11.222D);
		doubleHandle2.set(structSegmt1, 22.333D);
		doubleHandle3.set(structSegmt1, 33.123D);
		MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout);
		doubleHandle1.set(structSegmt2, 33.444D);
		doubleHandle2.set(structSegmt2, 44.555D);
		doubleHandle3.set(structSegmt2, 55.456D);

		MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2);
		Assert.assertEquals((double)doubleHandle1.get(resultSegmt), 44.666D, 0.001D);
		Assert.assertEquals((double)doubleHandle2.get(resultSegmt), 66.888D, 0.001D);
		Assert.assertEquals((double)doubleHandle3.get(resultSegmt), 88.579D, 0.001D);
		structSegmt1.close();
		structSegmt2.close();
		resultSegmt.close();
	}
}
