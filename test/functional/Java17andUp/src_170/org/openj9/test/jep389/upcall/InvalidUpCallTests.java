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
package org.openj9.test.jep389.upcall;

import org.testng.Assert;
import static org.testng.Assert.fail;
import org.testng.annotations.Test;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.lang.invoke.VarHandle;

import jdk.incubator.foreign.Addressable;
import jdk.incubator.foreign.CLinker;
import static jdk.incubator.foreign.CLinker.C_INT;
import static jdk.incubator.foreign.CLinker.C_POINTER;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemoryLayout.PathElement;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.SymbolLookup;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) for argument/return struct in upcall
 * which verify the illegal cases specific to the return value.
 */
@Test(groups = { "level.sanity" })
public class InvalidUpCallTests {
	private static CLinker clinker = CLinker.getInstance();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_throwExceptionFromUpcallMethod() throws Throwable {
		runTerminatingUpcall("test_throwExceptionFromUpcallMethod", "IllegalArgumentException", false);
	}

	private void test_throwExceptionFromUpcallMethodInChild() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStructByUpcallMH_mustNotReturn").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct_throwException,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			fail("Failed to throw out IllegalArgumentException from the the upcall method");
		}
	}

	@Test
	public void test_nestedUpcall_throwExceptionFromUpcallMethod() throws Throwable {
		runTerminatingUpcall("test_nestedUpcall_throwExceptionFromUpcallMethod", "IllegalArgumentException", false);
	}

	private void test_nestedUpcall_throwExceptionFromUpcallMethodInChild() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStructByUpcallMH_mustNotReturn").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct_nestedUpcall,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);

			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			fail("Failed to throw out IllegalArgumentException from the nested upcall");
		}
	}

	@Test
	public void test_nullValueForReturnPtr() throws Throwable {
		runTerminatingUpcall("test_nullValueForReturnPtr", "NullPointerException", false);
	}

	private void test_nullValueForReturnPtrInChild() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStructPointer_nullValue,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2, upcallFuncAddr);
			fail("Failed to throw out NullPointerException in the case of the null value upon return");
		}
	}

	@Test
	public void test_nullValueForReturnStruct() throws Throwable {
		runTerminatingUpcall("test_nullValueForReturnStruct", "NullPointerException", false);
	}

	private void test_nullValueForReturnStructInChild() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct_nullValue,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(structSegmt1, structSegmt2, upcallFuncAddr);
			fail("Failed to throw out NullPointerException in the case of the null value upon return");
		}
	}

	public void test_nullAddrForReturnPtr() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("validateReturnNullAddrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStructPointer_nullAddr,
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
			Assert.assertEquals(intHandle1.get(resultSegmt), 11223344);
			Assert.assertEquals(intHandle2.get(resultSegmt), 55667788);
		}
	}

	@Test
	public void test_heapSegmentForReturnPtr() throws Throwable {
		runTerminatingUpcall("test_heapSegmentForReturnPtr", "IllegalArgumentException", false);
	}

	private void test_heapSegmentForReturnPtrInChild() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStructPointer_heapSegmt,
					FunctionDescriptor.of(C_POINTER, C_POINTER, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(structSegmt1.address(), structSegmt2, upcallFuncAddr);
			fail("Failed to throw out IllegalArgumentException in the case of the heap address upon return");
		}
	}

	public void test_heapSegmentForReturnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct_heapSegmt,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
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

	@Test(enabled = false)
	public static void main(String[] args) throws Throwable {
		System.out.println("UPCALL_CHILD_STARTED");
		try {
			java.lang.reflect.Method method = InvalidUpCallTests.class.getDeclaredMethod(args[0] + "InChild");
			method.invoke(new InvalidUpCallTests());
		} catch (Throwable t) {
			System.out.println("UPCALL_EXCEPTION_ESCAPED_TO_JAVA");
			t.printStackTrace();
		}
		System.out.println("UPCALL_JAVA_CONTINUED");
	}

	private static void runTerminatingUpcall(String test, String diagnostic, boolean vmFailure) throws Exception {
		java.util.List<String> command = new java.util.ArrayList<>();
		command.add(java.nio.file.Path.of(System.getProperty("java.home"), "bin", "java").toString());
		command.addAll(java.lang.management.ManagementFactory.getRuntimeMXBean().getInputArguments());
		command.add("-Djava.library.path=" + System.getProperty("java.library.path"));
		command.add("-cp");
		command.add(System.getProperty("java.class.path"));
		command.add(InvalidUpCallTests.class.getName());
		command.add(test);
		java.nio.file.Path output = java.nio.file.Files.createTempFile("upcall-termination-", ".log");
		Process child = null;
		try {
			child = new ProcessBuilder(command).redirectErrorStream(true).redirectOutput(output.toFile()).start();
			if (!child.waitFor(30, java.util.concurrent.TimeUnit.SECONDS)) {
				child.destroyForcibly();
				fail("Upcall child timed out: " + test);
			}
			String text = java.nio.file.Files.readString(output);
			org.testng.Assert.assertTrue(text.contains("UPCALL_CHILD_STARTED"), text);
			org.testng.Assert.assertTrue(text.contains(diagnostic), text);
			if (vmFailure) {
				org.testng.Assert.assertNotEquals(child.exitValue(), 0, text);
			} else {
				org.testng.Assert.assertEquals(child.exitValue(), 1, text);
			}
			org.testng.Assert.assertFalse(text.contains("UPCALL_NATIVE_CONTINUED"), text);
			org.testng.Assert.assertFalse(text.contains("UPCALL_JAVA_CONTINUED"), text);
			org.testng.Assert.assertFalse(text.contains("UPCALL_EXCEPTION_ESCAPED_TO_JAVA"), text);
		} finally {
			if (child != null && child.isAlive()) {
				child.destroyForcibly();
				child.waitFor();
			}
			java.nio.file.Files.deleteIfExists(output);
		}
	}
}
