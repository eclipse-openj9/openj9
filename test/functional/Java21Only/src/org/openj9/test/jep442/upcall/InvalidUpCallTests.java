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
import static org.testng.Assert.fail;
import org.testng.annotations.Test;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.GroupLayout;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemoryLayout.PathElement;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SegmentAllocator;
import java.lang.foreign.SymbolLookup;
import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_INT;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.VarHandle;

/**
 * Test cases for JEP 442: Foreign Linker API (Third Preview) in upcall,
 * which verify the illegal cases including the returned segment, etc.
 */
@Test(groups = { "level.sanity" })
public class InvalidUpCallTests {
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_throwExceptionFromUpcallMethod() throws Throwable {
		runTerminatingUpcall("test_throwExceptionFromUpcallMethod", "IllegalArgumentException", false);
	}

	private void test_throwExceptionFromUpcallMethodInChild() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructByUpcallMH_mustNotReturn").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct_throwException,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			fail("Failed to throw out IllegalArgumentException from the the upcall method");
		}
	}

	@Test
	public void test_nestedUpcall_throwExceptionFromUpcallMethod() throws Throwable {
		runTerminatingUpcall("test_nestedUpcall_throwExceptionFromUpcallMethod", "IllegalArgumentException", false);
	}

	private void test_nestedUpcall_throwExceptionFromUpcallMethodInChild() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructByUpcallMH_mustNotReturn").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct_nestedUpcall,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			fail("Failed to throw out IllegalArgumentException from the nested upcall");
		}
	}

	@Test
	public void test_nullValueForReturnPtr() throws Throwable {
		runTerminatingUpcall("test_nullValueForReturnPtr", "NullPointerException", false);
	}

	private void test_nullValueForReturnPtrInChild() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStructPointer_nullValue,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			fail("Failed to throw out NullPointerException in the case of the null value upon return");
		}
	}

	@Test
	public void test_nullValueForReturnStruct() throws Throwable {
		runTerminatingUpcall("test_nullValueForReturnStruct", "NullPointerException", false);
	}

	private void test_nullValueForReturnStructInChild() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct_nullValue,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			fail("Failed to throw out NullPointerException in the case of the null value upon return");
		}
	}

	public void test_nullSegmentForReturnPtr() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("validateReturnNullAddrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStructPointer_nullSegmt,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			MemorySegment resultSegmt = resultAddr.reinterpret(structLayout.byteSize());
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 0), 11223344);
			Assert.assertEquals(resultSegmt.get(JAVA_INT, 4), 55667788);
		}
	}

	@Test
	public void test_nullSegmentForReturnStruct() throws Throwable {
		runTerminatingUpcall("test_nullSegmentForReturnStruct", "NullPointerException", false);
	}

	private void test_nullSegmentForReturnStructInChild() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct_nullSegmt,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			fail("Failed to throw out NullPointerException in the case of the null segment upon return");
		}
	}

	@Test
	public void test_heapSegmentForReturnPtr() throws Throwable {
		runTerminatingUpcall("test_heapSegmentForReturnPtr", "IllegalArgumentException", false);
	}

	private void test_heapSegmentForReturnPtrInChild() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStructPointer_heapSegmt,
					FunctionDescriptor.of(ADDRESS, ADDRESS, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultAddr = (MemorySegment)mh.invoke(structSegmt1, structSegmt2, upcallFuncAddr);
			fail("Failed to throw out IllegalArgumentException in the case of the heap segment upon return");
		}
	}

	public void test_heapSegmentForReturnStruct() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct_heapSegmt,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena);
			MemorySegment structSegmt1 = arena.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = arena.allocate(structLayout);
			intHandle1.set(structSegmt2, 99001122);
			intHandle2.set(structSegmt2, 33445566);

			MemorySegment resultSegmt = (MemorySegment)mh.invoke((SegmentAllocator)arena, structSegmt1, structSegmt2, upcallFuncAddr);
			Assert.assertEquals(intHandle1.get(resultSegmt), 110224466);
			Assert.assertEquals(intHandle2.get(resultSegmt), 89113354);
		}
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Not supported for upcall.*")
	public void test_InvalidLinkerOptions_firstVariadicArg() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena, Linker.Option.firstVariadicArg(0));
			fail("Failed to throw out IllegalArgumentException in the case of the invalid linker option for upcall.");
		}
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Not supported for upcall.*")
	public void test_InvalidLinkerOptions_captureCallState() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena, Linker.Option.captureCallState("errno"));
			fail("Failed to throw out IllegalArgumentException in the case of the invalid linker option for upcall.");
		}
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Not supported for upcall.*")
	public void test_InvalidLinkerOptions_isTrivial_1() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("add2IntStructs_returnStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct,
					FunctionDescriptor.of(structLayout, structLayout, structLayout), arena, Linker.Option.isTrivial());
			fail("Failed to throw out IllegalArgumentException in the case of the invalid linker option for upcall.");
		}
	}

	@Test
	public void test_InvalidLinkerOptions_isTrivial_2() throws Throwable {
		runTerminatingUpcall("test_InvalidLinkerOptions_isTrivial_2", "An FFM upcall is not allowed during a critical downcall", true);
	}

	private void test_InvalidLinkerOptions_isTrivial_2InChild() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("captureTrivialOptionByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.isTrivial());

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_captureTrivialOption,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT), arena);
			int result = (int)mh.invoke(111, upcallFuncAddr);
			fail("Failed to throw out IllegalThreadStateException in the case of the invalid upcall during the trivial downcall.");
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
