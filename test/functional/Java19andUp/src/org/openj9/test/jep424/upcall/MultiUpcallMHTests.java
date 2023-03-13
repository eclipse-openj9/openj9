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
package org.openj9.test.jep424.upcall;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;

import java.lang.invoke.MethodHandle;
import java.lang.foreign.Addressable;
import java.lang.foreign.Linker;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.MemoryAddress;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.MemorySession;
import java.lang.foreign.SegmentAllocator;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.ValueLayout;
import static java.lang.foreign.ValueLayout.*;

/**
 * Test cases for JEP 424: Foreign Linker API (Preview) intended for
 * the situations when the multiple primitive specific upcalls happen within
 * the same memory session or from different memory sessions.
 */
@Test(groups = { "level.sanity" })
public class MultiUpcallMHTests {
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addTwoBoolsWithOrByUpcallMH_SameSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolsWithOrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr1 = linker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN), session);
			boolean result = (boolean)mh.invoke(true, false, upcallFuncAddr1);
			Assert.assertEquals(result, true);

			MemorySegment upcallFuncAddr2 = linker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN), session);
			result = (boolean)mh.invoke(true, false, upcallFuncAddr2);
			Assert.assertEquals(result, true);

			MemorySegment upcallFuncAddr3 = linker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN), session);
			result = (boolean)mh.invoke(true, false, upcallFuncAddr3);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addTwoBoolsWithOrByUpcallMH_DiffSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolsWithOrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN), session);
			boolean result = (boolean)mh.invoke(true, false, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN), session);
			boolean result = (boolean)mh.invoke(true, false, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, JAVA_BOOLEAN), session);
			boolean result = (boolean)mh.invoke(true, false, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPointerByUpcallMH_SameSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);

			MemorySegment upcallFuncAddr1 = linker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR), session);
			MemorySegment charSegmt1 = allocator.allocate(JAVA_CHAR, 'B');
			char result = (char)mh.invoke(charSegmt1, 'D', upcallFuncAddr1);
			Assert.assertEquals(result, 'C');

			MemorySegment upcallFuncAddr2 = linker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR), session);
			MemorySegment charSegmt2 = allocator.allocate(JAVA_CHAR, 'B');
			result = (char)mh.invoke(charSegmt2, 'D', upcallFuncAddr2);
			Assert.assertEquals(result, 'C');

			MemorySegment upcallFuncAddr3 = linker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR), session);
			MemorySegment charSegmt3 = allocator.allocate(JAVA_CHAR, 'B');
			result = (char)mh.invoke(charSegmt3, 'D', upcallFuncAddr3);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPointerByUpcallMH_DiffSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr1 = linker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR), session);
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment charSegmt = allocator.allocate(JAVA_CHAR, 'B');
			char result = (char)mh.invoke(charSegmt, 'D', upcallFuncAddr1);
			Assert.assertEquals(result, 'C');
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr1 = linker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR), session);
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment charSegmt = allocator.allocate(JAVA_CHAR, 'B');
			char result = (char)mh.invoke(charSegmt, 'D', upcallFuncAddr1);
			Assert.assertEquals(result, 'C');
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr1 = linker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(JAVA_CHAR, ADDRESS, JAVA_CHAR), session);
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment charSegmt = allocator.allocate(JAVA_CHAR, 'B');
			char result = (char)mh.invoke(charSegmt, 'D', upcallFuncAddr1);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_addByteAndByteFromNativePtrByUpcallMH_SameSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndByteFromNativePtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr1 = linker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), session);
			byte result = (byte)mh.invoke((byte)33, upcallFuncAddr1);
			Assert.assertEquals(result, (byte)88);

			MemorySegment upcallFuncAddr2 = linker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), session);
			result = (byte)mh.invoke((byte)33, upcallFuncAddr2);
			Assert.assertEquals(result, (byte)88);

			MemorySegment upcallFuncAddr3 = linker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), session);
			result = (byte)mh.invoke((byte)33, upcallFuncAddr3);
			Assert.assertEquals(result, (byte)88);
		}
	}

	@Test
	public void test_addByteAndByteFromNativePtrByUpcallMH_DiffSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndByteFromNativePtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), session);
			byte result = (byte)mh.invoke((byte)33, upcallFuncAddr);
			Assert.assertEquals(result, (byte)88);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), session);
			byte result = (byte)mh.invoke((byte)33, upcallFuncAddr);
			Assert.assertEquals(result, (byte)88);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS), session);
			byte result = (byte)mh.invoke((byte)33, upcallFuncAddr);
			Assert.assertEquals(result, (byte)88);
		}
	}

	@Test
	public void test_addShortAndShortFromPtr_RetPtr_ByUpcallMH_SameSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);

			MemorySegment upcallFuncAddr1 = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT), session);
			MemorySegment shortSegmt1 = allocator.allocate(JAVA_SHORT, (short)444);
			MemoryAddress resultAddr1 = (MemoryAddress)mh.invoke(shortSegmt1, (short)555, upcallFuncAddr1);
			Assert.assertEquals(resultAddr1.get(JAVA_SHORT, 0), (short)999);

			MemorySegment upcallFuncAddr2 = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT), session);
			MemorySegment shortSegmt2 = allocator.allocate(JAVA_SHORT, (short)444);
			MemoryAddress resultAddr2 = (MemoryAddress)mh.invoke(shortSegmt2, (short)555, upcallFuncAddr2);
			Assert.assertEquals(resultAddr2.get(JAVA_SHORT, 0), (short)999);

			MemorySegment upcallFuncAddr3 = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT), session);
			MemorySegment shortSegmt3 = allocator.allocate(JAVA_SHORT, (short)444);
			MemoryAddress resultAddr3 = (MemoryAddress)mh.invoke(shortSegmt3, (short)555, upcallFuncAddr3);
			Assert.assertEquals(resultAddr3.get(JAVA_SHORT, 0), (short)999);
		}
	}

	@Test
	public void test_addShortAndShortFromPtr_RetPtr_ByUpcallMH_DiffSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT), session);
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment shortSegmt = allocator.allocate(JAVA_SHORT, (short)444);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(shortSegmt, (short)555, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_SHORT, 0), (short)999);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT), session);
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment shortSegmt = allocator.allocate(JAVA_SHORT, (short)444);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(shortSegmt, (short)555, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_SHORT, 0), (short)999);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_SHORT), session);
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment shortSegmt = allocator.allocate(JAVA_SHORT, (short)444);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(shortSegmt, (short)555, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_SHORT, 0), (short)999);
		}
	}

	@Test
	public void test_addTwoIntsByUpcallMH_SameSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr1 = linker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT), session);
			int result = (int)mh.invoke(111112, 111123, upcallFuncAddr1);
			Assert.assertEquals(result, 222235);

			MemorySegment upcallFuncAddr2 = linker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT), session);
			result = (int)mh.invoke(111112, 111123, upcallFuncAddr2);
			Assert.assertEquals(result, 222235);

			MemorySegment upcallFuncAddr3 = linker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT), session);
			result = (int)mh.invoke(111112, 111123, upcallFuncAddr3);
			Assert.assertEquals(result, 222235);
		}
	}

	@Test
	public void test_addTwoIntsByUpcallMH_DiffSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT), session);
			int result = (int)mh.invoke(111112, 111123, upcallFuncAddr);
			Assert.assertEquals(result, 222235);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT), session);
			int result = (int)mh.invoke(111112, 111123, upcallFuncAddr);
			Assert.assertEquals(result, 222235);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, JAVA_INT), session);
			int result = (int)mh.invoke(111112, 111123, upcallFuncAddr);
			Assert.assertEquals(result, 222235);
		}
	}

	@Test
	public void test_addTwoIntsReturnVoidByUpcallMH_SameSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsReturnVoidByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr1 = linker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT), session);
			mh.invoke(111454, 111398, upcallFuncAddr1);

			MemorySegment upcallFuncAddr2 = linker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT), session);
			mh.invoke(111454, 111398, upcallFuncAddr2);

			MemorySegment upcallFuncAddr3 = linker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT), session);
			mh.invoke(111454, 111398, upcallFuncAddr3);
		}
	}

	@Test
	public void test_addTwoIntsReturnVoidByUpcallMH_DiffSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsReturnVoidByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT), session);
			mh.invoke(111454, 111398, upcallFuncAddr);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT), session);
			mh.invoke(111454, 111398, upcallFuncAddr);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(JAVA_INT, JAVA_INT), session);
			mh.invoke(111454, 111398, upcallFuncAddr);
		}
	}

	@Test
	public void test_addLongAndLongFromPointerByUpcallMH_SameSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongFromPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);

			MemorySegment upcallFuncAddr1 = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG), session);
			MemorySegment longSegmt1 = allocator.allocate(JAVA_LONG, 5742457424L);
			long result = (long)mh.invoke(longSegmt1, 6666698235L, upcallFuncAddr1);
			Assert.assertEquals(result, 12409155659L);

			MemorySegment upcallFuncAddr2 = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG), session);
			MemorySegment longSegmt2 = allocator.allocate(JAVA_LONG, 5742457424L);
			result = (long)mh.invoke(longSegmt2, 6666698235L, upcallFuncAddr2);
			Assert.assertEquals(result, 12409155659L);

			MemorySegment upcallFuncAddr3 = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG), session);
			MemorySegment longSegmt3 = allocator.allocate(JAVA_LONG, 5742457424L);
			result = (long)mh.invoke(longSegmt3, 6666698235L, upcallFuncAddr3);
			Assert.assertEquals(result, 12409155659L);
		}
	}

	@Test
	public void test_addLongAndLongFromPointerByUpcallMH_DiffSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongFromPointerByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG), session);
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment longSegmt = allocator.allocate(JAVA_LONG, 5742457424L);
			long result = (long)mh.invoke(longSegmt, 6666698235L, upcallFuncAddr);
			Assert.assertEquals(result, 12409155659L);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG), session);
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment longSegmt = allocator.allocate(JAVA_LONG, 5742457424L);
			long result = (long)mh.invoke(longSegmt, 6666698235L, upcallFuncAddr);
			Assert.assertEquals(result, 12409155659L);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(JAVA_LONG, ADDRESS, JAVA_LONG), session);
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment longSegmt = allocator.allocate(JAVA_LONG, 5742457424L);
			long result = (long)mh.invoke(longSegmt, 6666698235L, upcallFuncAddr);
			Assert.assertEquals(result, 12409155659L);
		}
	}

	@Test
	public void test_addFloatAndFloatFromNativePtrByUpcallMH_SameSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromNativePtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr1 = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), session);
			float result = (float)mh.invoke(5.74F, upcallFuncAddr1);
			Assert.assertEquals(result, 12.53F, 0.01F);

			MemorySegment upcallFuncAddr2 = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), session);
			result = (float)mh.invoke(5.74F, upcallFuncAddr2);
			Assert.assertEquals(result, 12.53F, 0.01F);

			MemorySegment upcallFuncAddr3 = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), session);
			result = (float)mh.invoke(5.74F, upcallFuncAddr3);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromNativePtrByUpcallMH_DiffSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromNativePtrByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), session);
			float result = (float)mh.invoke(5.74F, upcallFuncAddr);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), session);
			float result = (float)mh.invoke(5.74F, upcallFuncAddr);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS), session);
			float result = (float)mh.invoke(5.74F, upcallFuncAddr);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH_SameSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);

			MemorySegment upcallFuncAddr1 = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE), session);
			MemorySegment doubleSegmt1 = allocator.allocate(JAVA_DOUBLE, 1159.748D);
			MemoryAddress resultAddr1 = (MemoryAddress)mh.invoke(doubleSegmt1, 1262.795D, upcallFuncAddr1);
			Assert.assertEquals(resultAddr1.get(JAVA_DOUBLE, 0), 2422.543D, 0.001D);

			MemorySegment upcallFuncAddr2 = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE), session);
			MemorySegment doubleSegmt2 = allocator.allocate(JAVA_DOUBLE, 1159.748D);
			MemoryAddress resultAddr2 = (MemoryAddress)mh.invoke(doubleSegmt2, 1262.795D, upcallFuncAddr2);
			Assert.assertEquals(resultAddr2.get(JAVA_DOUBLE, 0), 2422.543D, 0.001D);

			MemorySegment upcallFuncAddr3 = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE), session);
			MemorySegment doubleSegmt3 = allocator.allocate(JAVA_DOUBLE, 1159.748D);
			MemoryAddress resultAddr3 = (MemoryAddress)mh.invoke(doubleSegmt3, 1262.795D, upcallFuncAddr3);
			Assert.assertEquals(resultAddr3.get(JAVA_DOUBLE, 0), 2422.543D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH_DiffSession() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE, ADDRESS);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE), session);
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment doubleSegmt = allocator.allocate(JAVA_DOUBLE, 1159.748D);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(doubleSegmt, 1262.795D, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_DOUBLE, 0), 2422.543D, 0.001D);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE), session);
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment doubleSegmt = allocator.allocate(JAVA_DOUBLE, 1159.748D);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(doubleSegmt, 1262.795D, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_DOUBLE, 0), 2422.543D, 0.001D);
		}

		try (MemorySession session = MemorySession.openConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(ADDRESS, ADDRESS, JAVA_DOUBLE), session);
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment doubleSegmt = allocator.allocate(JAVA_DOUBLE, 1159.748D);
			MemoryAddress resultAddr = (MemoryAddress)mh.invoke(doubleSegmt, 1262.795D, upcallFuncAddr);
			Assert.assertEquals(resultAddr.get(JAVA_DOUBLE, 0), 2422.543D, 0.001D);
		}
	}
}
