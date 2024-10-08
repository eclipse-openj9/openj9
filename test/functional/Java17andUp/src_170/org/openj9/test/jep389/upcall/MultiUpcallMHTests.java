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
import jdk.incubator.foreign.MemoryAccess;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.SymbolLookup;
import jdk.incubator.foreign.ValueLayout;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) in upcall intended for
 * the situations when the multiple primitive specific upcalls happen within
 * the same resource scope or from different resource scopes.
 */
@Test(groups = { "level.sanity" })
public class MultiUpcallMHTests {
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinOS = osName.contains("win");
	/* long long is 64 bits on AIX/ppc64, which is the same as Windows */
	private static ValueLayout longLayout = (isWinOS || isAixOS) ? C_LONG_LONG : C_LONG;
	private static CLinker clinker = CLinker.getInstance();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addTwoBoolsWithOrByUpcallMH_SameScope() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, boolean.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolsWithOrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr1 = clinker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR), scope);
			boolean result = (boolean)mh.invokeExact(true, false, upcallFuncAddr1);
			Assert.assertEquals(result, true);

			MemoryAddress upcallFuncAddr2 = clinker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR), scope);
			result = (boolean)mh.invokeExact(true, false, upcallFuncAddr2);
			Assert.assertEquals(result, true);

			MemoryAddress upcallFuncAddr3 = clinker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR), scope);
			result = (boolean)mh.invokeExact(true, false, upcallFuncAddr3);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addTwoBoolsWithOrByUpcallMH_DiffScope() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, boolean.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolsWithOrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR), scope);
			boolean result = (boolean)mh.invokeExact(true, false, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR), scope);
			boolean result = (boolean)mh.invokeExact(true, false, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2BoolsWithOr,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR), scope);
			boolean result = (boolean)mh.invokeExact(true, false, upcallFuncAddr);
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPointerByUpcallMH_SameScope() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, MemoryAddress.class, char.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);

			MemoryAddress upcallFuncAddr1 = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT), scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setChar(shortSegmt, 'B');
			char result = (char)mh.invokeExact(shortSegmt.address(), 'D', upcallFuncAddr1);
			Assert.assertEquals(result, 'C');

			MemoryAddress upcallFuncAddr2 = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT), scope);
			result = (char)mh.invokeExact(shortSegmt.address(), 'D', upcallFuncAddr2);
			Assert.assertEquals(result, 'C');

			MemoryAddress upcallFuncAddr3 = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT), scope);
			result = (char)mh.invokeExact(shortSegmt.address(), 'D', upcallFuncAddr3);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_createNewCharFromCharAndCharFromPointerByUpcallMH_DiffScope() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, MemoryAddress.class, char.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFromCharAndCharFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr1 = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setChar(shortSegmt, 'B');
			char result = (char)mh.invokeExact(shortSegmt.address(), 'D', upcallFuncAddr1);
			Assert.assertEquals(result, 'C');
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr1 = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setChar(shortSegmt, 'B');
			char result = (char)mh.invokeExact(shortSegmt.address(), 'D', upcallFuncAddr1);
			Assert.assertEquals(result, 'C');
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr1 = clinker.upcallStub(UpcallMethodHandles.MH_createNewCharFromCharAndCharFromPointer,
					FunctionDescriptor.of(C_SHORT, C_POINTER, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setChar(shortSegmt, 'B');
			char result = (char)mh.invokeExact(shortSegmt.address(), 'D', upcallFuncAddr1);
			Assert.assertEquals(result, 'C');
		}
	}

	@Test
	public void test_addByteAndByteFromNativePtrByUpcallMH_SameScope() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndByteFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr1 = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER), scope);
			byte result = (byte)mh.invokeExact((byte)33, upcallFuncAddr1);
			Assert.assertEquals(result, (byte)88);

			MemoryAddress upcallFuncAddr2 = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER), scope);
			result = (byte)mh.invokeExact((byte)33, upcallFuncAddr2);
			Assert.assertEquals(result, (byte)88);

			MemoryAddress upcallFuncAddr3 = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER), scope);
			result = (byte)mh.invokeExact((byte)33, upcallFuncAddr3);
			Assert.assertEquals(result, (byte)88);
		}
	}

	@Test
	public void test_addByteAndByteFromNativePtrByUpcallMH_DiffScope() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addByteAndByteFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER), scope);
			byte result = (byte)mh.invokeExact((byte)33, upcallFuncAddr);
			Assert.assertEquals(result, (byte)88);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER), scope);
			byte result = (byte)mh.invokeExact((byte)33, upcallFuncAddr);
			Assert.assertEquals(result, (byte)88);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addByteAndByteFromPointer,
					FunctionDescriptor.of(C_CHAR, C_CHAR, C_POINTER), scope);
			byte result = (byte)mh.invokeExact((byte)33, upcallFuncAddr);
			Assert.assertEquals(result, (byte)88);
		}
	}

	@Test
	public void test_addShortAndShortFromPtr_RetPtr_ByUpcallMH_SameScope() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, short.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);

			MemoryAddress upcallFuncAddr1 = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT), scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setShort(shortSegmt, (short)444);
			MemoryAddress resultAddr1 = (MemoryAddress)mh.invokeExact(shortSegmt.address(), (short)555, upcallFuncAddr1);
			short result = MemoryAccess.getShort(resultAddr1.asSegment(C_SHORT.byteSize(), resultAddr1.scope()));
			Assert.assertEquals(result, (short)999);

			MemoryAddress upcallFuncAddr2 = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT), scope);
			MemoryAddress resultAddr2 = (MemoryAddress)mh.invokeExact(shortSegmt.address(), (short)555, upcallFuncAddr2);
			result = MemoryAccess.getShort(resultAddr2.asSegment(C_SHORT.byteSize(), resultAddr2.scope()));
			Assert.assertEquals(result, (short)999);

			MemoryAddress upcallFuncAddr3 = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT), scope);
			MemoryAddress resultAddr3 = (MemoryAddress)mh.invokeExact(shortSegmt.address(), (short)555, upcallFuncAddr3);
			result = MemoryAccess.getShort(resultAddr3.asSegment(C_SHORT.byteSize(), resultAddr3.scope()));
			Assert.assertEquals(result, (short)999);
		}
	}

	@Test
	public void test_addShortAndShortFromPtr_RetPtr_ByUpcallMH_DiffScope() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, short.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAndShortFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setShort(shortSegmt, (short)444);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(shortSegmt.address(), (short)555, upcallFuncAddr);
			short result = MemoryAccess.getShort(resultAddr.asSegment(C_SHORT.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, (short)999);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setShort(shortSegmt, (short)444);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(shortSegmt.address(), (short)555, upcallFuncAddr);
			short result = MemoryAccess.getShort(resultAddr.asSegment(C_SHORT.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, (short)999);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAndShortFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_SHORT), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment shortSegmt = allocator.allocate(C_SHORT);
			MemoryAccess.setShort(shortSegmt, (short)444);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(shortSegmt.address(), (short)555, upcallFuncAddr);
			short result = MemoryAccess.getShort(resultAddr.asSegment(C_SHORT.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, (short)999);
		}
	}

	@Test
	public void test_addTwoIntsByUpcallMH_SameScope() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr1 = clinker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(C_INT, C_INT, C_INT), scope);
			int result = (int)mh.invokeExact(111112, 111123, upcallFuncAddr1);
			Assert.assertEquals(result, 222235);

			MemoryAddress upcallFuncAddr2 = clinker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(C_INT, C_INT, C_INT), scope);
			result = (int)mh.invokeExact(111112, 111123, upcallFuncAddr2);
			Assert.assertEquals(result, 222235);

			MemoryAddress upcallFuncAddr3 = clinker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(C_INT, C_INT, C_INT), scope);
			result = (int)mh.invokeExact(111112, 111123, upcallFuncAddr3);
			Assert.assertEquals(result, 222235);
		}
	}

	@Test
	public void test_addTwoIntsByUpcallMH_DiffScope() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(C_INT, C_INT, C_INT), scope);
			int result = (int)mh.invokeExact(111112, 111123, upcallFuncAddr);
			Assert.assertEquals(result, 222235);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(C_INT, C_INT, C_INT), scope);
			int result = (int)mh.invokeExact(111112, 111123, upcallFuncAddr);
			Assert.assertEquals(result, 222235);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2Ints,
					FunctionDescriptor.of(C_INT, C_INT, C_INT), scope);
			int result = (int)mh.invokeExact(111112, 111123, upcallFuncAddr);
			Assert.assertEquals(result, 222235);
		}
	}

	@Test
	public void test_addTwoIntsReturnVoidByUpcallMH_SameScope() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsReturnVoidByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr1 = clinker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(C_INT, C_INT), scope);
			mh.invokeExact(111454, 111398, upcallFuncAddr1);

			MemoryAddress upcallFuncAddr2 = clinker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(C_INT, C_INT), scope);
			mh.invokeExact(111454, 111398, upcallFuncAddr2);

			MemoryAddress upcallFuncAddr3 = clinker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(C_INT, C_INT), scope);
			mh.invokeExact(111454, 111398, upcallFuncAddr3);
		}
	}

	@Test
	public void test_addTwoIntsReturnVoidByUpcallMH_DiffScope() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsReturnVoidByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(C_INT, C_INT), scope);
			mh.invokeExact(111454, 111398, upcallFuncAddr);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(C_INT, C_INT), scope);
			mh.invokeExact(111454, 111398, upcallFuncAddr);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntsReturnVoid,
					FunctionDescriptor.ofVoid(C_INT, C_INT), scope);
			mh.invokeExact(111454, 111398, upcallFuncAddr);
		}
	}

	@Test
	public void test_addLongAndLongFromPointerByUpcallMH_SameScope() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class, long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_POINTER, longLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);

			MemoryAddress upcallFuncAddr1 = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(longLayout, C_POINTER, longLayout), scope);
			MemorySegment longSegmt = allocator.allocate(longLayout);
			MemoryAccess.setLong(longSegmt, 5742457424L);
			long result = (long)mh.invokeExact(longSegmt.address(), 6666698235L, upcallFuncAddr1);
			Assert.assertEquals(result, 12409155659L);

			MemoryAddress upcallFuncAddr2 = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(longLayout, C_POINTER, longLayout), scope);
			result = (long)mh.invokeExact(longSegmt.address(), 6666698235L, upcallFuncAddr2);
			Assert.assertEquals(result, 12409155659L);

			MemoryAddress upcallFuncAddr3 = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(longLayout, C_POINTER, longLayout), scope);
			result = (long)mh.invokeExact(longSegmt.address(), 6666698235L, upcallFuncAddr3);
			Assert.assertEquals(result, 12409155659L);
		}
	}

	@Test
	public void test_addLongAndLongFromPointerByUpcallMH_DiffScope() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class, long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_POINTER, longLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLongFromPointerByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(longLayout, C_POINTER, longLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment longSegmt = allocator.allocate(longLayout);
			MemoryAccess.setLong(longSegmt, 5742457424L);
			long result = (long)mh.invokeExact(longSegmt.address(), 6666698235L, upcallFuncAddr);
			Assert.assertEquals(result, 12409155659L);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(longLayout, C_POINTER, longLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment longSegmt = allocator.allocate(longLayout);
			MemoryAccess.setLong(longSegmt, 5742457424L);
			long result = (long)mh.invokeExact(longSegmt.address(), 6666698235L, upcallFuncAddr);
			Assert.assertEquals(result, 12409155659L);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLongFromPointer,
					FunctionDescriptor.of(longLayout, C_POINTER, longLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment longSegmt = allocator.allocate(longLayout);
			MemoryAccess.setLong(longSegmt, 5742457424L);
			long result = (long)mh.invokeExact(longSegmt.address(), 6666698235L, upcallFuncAddr);
			Assert.assertEquals(result, 12409155659L);
		}
	}

	@Test
	public void test_addFloatAndFloatFromNativePtrByUpcallMH_SameScope() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr1 = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER), scope);
			float result = (float)mh.invokeExact(5.74F, upcallFuncAddr1);
			Assert.assertEquals(result, 12.53F, 0.01F);

			MemoryAddress upcallFuncAddr2 = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER), scope);
			result = (float)mh.invokeExact(5.74F, upcallFuncAddr2);
			Assert.assertEquals(result, 12.53F, 0.01F);

			MemoryAddress upcallFuncAddr3 = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER), scope);
			result = (float)mh.invokeExact(5.74F, upcallFuncAddr3);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatFromNativePtrByUpcallMH_DiffScope() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatFromNativePtrByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER), scope);
			float result = (float)mh.invokeExact(5.74F, upcallFuncAddr);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER), scope);
			float result = (float)mh.invokeExact(5.74F, upcallFuncAddr);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatFromPointer,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_POINTER), scope);
			float result = (float)mh.invokeExact(5.74F, upcallFuncAddr);
			Assert.assertEquals(result, 12.53F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH_SameScope() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, double.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, C_DOUBLE, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);

			MemoryAddress upcallFuncAddr1 = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_DOUBLE), scope);
			MemorySegment doubleSegmt = allocator.allocate(C_DOUBLE);
			MemoryAccess.setDouble(doubleSegmt, 1159.748d);
			MemoryAddress resultAddr1 = (MemoryAddress)mh.invokeExact(doubleSegmt.address(), 1262.795d, upcallFuncAddr1);
			double result = MemoryAccess.getDouble(resultAddr1.asSegment(C_DOUBLE.byteSize(), resultAddr1.scope()));
			Assert.assertEquals(result, 2422.543d, 0.001d);

			MemoryAddress upcallFuncAddr2 = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_DOUBLE), scope);
			MemoryAddress resultAddr2 = (MemoryAddress)mh.invokeExact(doubleSegmt.address(), 1262.795d, upcallFuncAddr2);
			result = MemoryAccess.getDouble(resultAddr2.asSegment(C_DOUBLE.byteSize(), resultAddr2.scope()));
			Assert.assertEquals(result, 2422.543d, 0.001d);

			MemoryAddress upcallFuncAddr3 = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_DOUBLE), scope);
			MemoryAddress resultAddr3 = (MemoryAddress)mh.invokeExact(doubleSegmt.address(), 1262.795d, upcallFuncAddr3);
			result = MemoryAccess.getDouble(resultAddr3.asSegment(C_DOUBLE.byteSize(), resultAddr3.scope()));
			Assert.assertEquals(result, 2422.543d, 0.001d);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH_DiffScope() throws Throwable {
		MethodType mt = MethodType.methodType(MemoryAddress.class, MemoryAddress.class, double.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_POINTER, C_POINTER, C_DOUBLE, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFromPtr_RetPtr_ByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_DOUBLE), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment doubleSegmt = allocator.allocate(C_DOUBLE);
			MemoryAccess.setDouble(doubleSegmt, 1159.748d);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(doubleSegmt.address(), 1262.795d, upcallFuncAddr);
			double result = MemoryAccess.getDouble(resultAddr.asSegment(C_DOUBLE.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 2422.543d, 0.001d);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_DOUBLE), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment doubleSegmt = allocator.allocate(C_DOUBLE);
			MemoryAccess.setDouble(doubleSegmt, 1159.748d);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(doubleSegmt.address(), 1262.795d, upcallFuncAddr);
			double result = MemoryAccess.getDouble(resultAddr.asSegment(C_DOUBLE.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 2422.543d, 0.001d);
		}

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFromPtr_RetPtr,
					FunctionDescriptor.of(C_POINTER, C_POINTER, C_DOUBLE), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment doubleSegmt = allocator.allocate(C_DOUBLE);
			MemoryAccess.setDouble(doubleSegmt, 1159.748d);
			MemoryAddress resultAddr = (MemoryAddress)mh.invokeExact(doubleSegmt.address(), 1262.795d, upcallFuncAddr);
			double result = MemoryAccess.getDouble(resultAddr.asSegment(C_DOUBLE.byteSize(), resultAddr.scope()));
			Assert.assertEquals(result, 2422.543d, 0.001d);
		}
	}
}
