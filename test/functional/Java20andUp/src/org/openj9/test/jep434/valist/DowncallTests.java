/*******************************************************************************
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package org.openj9.test.jep434.valist;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.VarHandle;

import java.lang.foreign.Arena;
import java.lang.foreign.Linker;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.GroupLayout;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemoryLayout.PathElement;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SegmentAllocator;
import java.lang.foreign.SymbolLookup;
import java.lang.foreign.VaList;
import static java.lang.foreign.ValueLayout.*;
import static java.lang.foreign.VaList.Builder;

/**
 * Test cases for JEP 434: Foreign Linker API (Second Preview) for the vararg list in downcall.
 */
@Test(groups = { "level.sanity" })
public class DowncallTests {
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static String arch = System.getProperty("os.arch").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinX64 = osName.contains("win") && (arch.equals("amd64") || arch.equals("x86_64"));
	private static boolean isLinuxX64 = osName.contains("linux") && (arch.equals("amd64") || arch.equals("x86_64"));
	private static boolean isLinuxAarch64 = osName.contains("linux") && arch.equals("aarch64");
	/* The padding of struct is not required on Power in terms of VaList */
	private static boolean isStructPaddingNotRequired = arch.startsWith("ppc64");
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();
	private static final SymbolLookup defaultLibLookup = linker.defaultLookup();

	@Test
	public void test_addIntsFromVaList() throws Throwable {
		MemorySegment functionSymbol = nativeLibLookup.find("addIntsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_INT, 700)
					.addVarg(JAVA_INT, 800)
					.addVarg(JAVA_INT, 900)
					.addVarg(JAVA_INT, 1000), arena.scope());
			int result = (int)mh.invoke(4, vaList.segment());
			Assert.assertEquals(result, 3400);
		}
	}

	@Test
	public void test_addLongsFromVaList() throws Throwable {
		MemorySegment functionSymbol = nativeLibLookup.find("addLongsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_LONG, 700000L)
					.addVarg(JAVA_LONG, 800000L)
					.addVarg(JAVA_LONG, 900000L)
					.addVarg(JAVA_LONG, 1000000L), arena.scope());
			long result = (long)mh.invoke(4, vaList.segment());
			Assert.assertEquals(result, 3400000L);
		}
	}

	@Test
	public void test_addDoublesFromVaList() throws Throwable {
		MemorySegment functionSymbol = nativeLibLookup.find("addDoublesFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_DOUBLE, 150.1001D)
					.addVarg(JAVA_DOUBLE, 160.2002D)
					.addVarg(JAVA_DOUBLE, 170.1001D)
					.addVarg(JAVA_DOUBLE, 180.2002D), arena.scope());
			double result = (double)mh.invoke(4, vaList.segment());
			Assert.assertEquals(result, 660.6006D, 0.0001D);
		}
	}

	@Test
	public void test_addMixedArgsFromVaList() throws Throwable {
		/* VaList on Windows/x86_64 in OpenJDK has problem in supporting this struct with
		 * the mixed elements (confirmed by OpenJDK/Hotspot). Thus, the test is disabled
		 * on Windows/x86_64 for now till the issue is fixed in OpenJDK and verified on
		 * OpenJDK/Hotspot in the future.
		 */
		if (!isWinX64) {
			MemorySegment functionSymbol = nativeLibLookup.find("addMixedArgsFromVaList").get();
			FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS);
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			try (Arena arena = Arena.openConfined()) {
				VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_INT, 700)
						.addVarg(JAVA_LONG, 800000L)
						.addVarg(JAVA_DOUBLE, 160.2002D), arena.scope());
				double result = (double)mh.invoke(vaList.segment());
				Assert.assertEquals(result, 800860.2002D, 0.0001D);
			}
		}
	}

	@Test
	public void test_addMoreMixedArgsFromVaList() throws Throwable {
		/* VaList on Linux/x86_64 and Windows/x86_64 in OpenJDK is unable to handle
		 * the va_list with over 8 arguments (confirmed by OpenJDK/Hotspot). So the
		 * test is disabled for now till the issue is fixed by OpenJDK.
		 */
		if (!isLinuxX64 && !isWinX64) {
			MemorySegment functionSymbol = nativeLibLookup.find("addMoreMixedArgsFromVaList").get();
			FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, ADDRESS);
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			try (Arena arena = Arena.openConfined()) {
				VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_INT, 100)
						.addVarg(JAVA_LONG, 200000L)
						.addVarg(JAVA_INT, 300)
						.addVarg(JAVA_LONG, 400000L)
						.addVarg(JAVA_INT, 500)
						.addVarg(JAVA_LONG, 600000L)
						.addVarg(JAVA_INT, 700)
						.addVarg(JAVA_DOUBLE, 161.2001D)
						.addVarg(JAVA_INT, 800)
						.addVarg(JAVA_DOUBLE, 162.2002D)
						.addVarg(JAVA_INT, 900)
						.addVarg(JAVA_DOUBLE, 163.2003D)
						.addVarg(JAVA_INT, 1000)
						.addVarg(JAVA_DOUBLE, 164.2004D)
						.addVarg(JAVA_INT, 1100)
						.addVarg(JAVA_DOUBLE, 165.2005D), arena.scope());
				double result = (double)mh.invoke(vaList.segment());
				Assert.assertEquals(result, 1206216.0015D, 0.0001D);
			}
		}
	}

	@Test
	public void test_addIntsByPtrFromVaList() throws Throwable {
		MemorySegment functionSymbol = nativeLibLookup.find("addIntsByPtrFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment intSegmt1 = allocator.allocate(JAVA_INT, 700);
			MemorySegment intSegmt2 = allocator.allocate(JAVA_INT, 800);
			MemorySegment intSegmt3 = allocator.allocate(JAVA_INT, 900);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, intSegmt1)
					.addVarg(ADDRESS, intSegmt2)
					.addVarg(ADDRESS, intSegmt3), arena.scope());
			int result = (int)mh.invoke(3, vaList.segment());
			Assert.assertEquals(result, 2400);
		}
	}

	@Test
	public void test_addLongsByPtrFromVaList() throws Throwable {
		MemorySegment functionSymbol = nativeLibLookup.find("addLongsByPtrFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment longSegmt1 = allocator.allocate(JAVA_LONG, 700000L);
			MemorySegment longSegmt2 = allocator.allocate(JAVA_LONG, 800000L);
			MemorySegment longSegmt3 = allocator.allocate(JAVA_LONG, 900000L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, longSegmt1)
					.addVarg(ADDRESS, longSegmt2)
					.addVarg(ADDRESS, longSegmt3), arena.scope());
			long result = (long)mh.invoke(3, vaList.segment());
			Assert.assertEquals(result, 2400000L);
		}
	}

	@Test
	public void test_addDoublesByPtrFromVaList() throws Throwable {
		MemorySegment functionSymbol = nativeLibLookup.find("addDoublesByPtrFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment doubleSegmt1 = allocator.allocate(JAVA_DOUBLE, 111150.1001D);
			MemorySegment doubleSegmt2 = allocator.allocate(JAVA_DOUBLE, 111160.2002D);
			MemorySegment doubleSegmt3 = allocator.allocate(JAVA_DOUBLE, 111170.1001D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, doubleSegmt1)
					.addVarg(ADDRESS, doubleSegmt2)
					.addVarg(ADDRESS, doubleSegmt3), arena.scope());
			double result = (double)mh.invoke(3, vaList.segment());
			Assert.assertEquals(result, 333480.4004D, 0.0001D);
		}
	}

	@Test
	public void test_add1ByteOfStructsFromVaList() throws Throwable {
		/* There are a few issues with the test on some platforms as follows:
		 * 1) VaList on Linux/x86_64 in OpenJDK is unable to handle the va_list with
		 *    over 8 arguments (confirmed by OpenJDK/Hotspot).
		 * 2) VaList on Linux/Aarch64 and Windows/x86_64 in OpenJDK has problem in supporting
		 *    the struct with only one integral element (confirmed by OpenJDK/Hotspot).
		 * Thus, the test is disabled on both these platforms for now till these issues
		 * are fixed in OpenJDK and verified on OpenJDK/Hotspot in the future.
		 */
		if (!isLinuxX64 && !isLinuxAarch64 && !isWinX64) {
			GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"));
			VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));

			MemorySegment functionSymbol = nativeLibLookup.find("add1ByteOfStructsFromVaList").get();
			FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_INT, ADDRESS);
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			try (Arena arena = Arena.openConfined()) {
				SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
				MemorySegment structSegmt1 = allocator.allocate(structLayout);
				byteHandle1.set(structSegmt1, (byte)1);
				MemorySegment structSegmt2 = allocator.allocate(structLayout);
				byteHandle1.set(structSegmt2, (byte)2);
				MemorySegment structSegmt3 = allocator.allocate(structLayout);
				byteHandle1.set(structSegmt3, (byte)3);
				MemorySegment structSegmt4 = allocator.allocate(structLayout);
				byteHandle1.set(structSegmt4, (byte)4);
				MemorySegment structSegmt5 = allocator.allocate(structLayout);
				byteHandle1.set(structSegmt5, (byte)5);
				MemorySegment structSegmt6 = allocator.allocate(structLayout);
				byteHandle1.set(structSegmt6, (byte)6);
				MemorySegment structSegmt7 = allocator.allocate(structLayout);
				byteHandle1.set(structSegmt7, (byte)7);
				MemorySegment structSegmt8 = allocator.allocate(structLayout);
				byteHandle1.set(structSegmt8, (byte)8);
				MemorySegment structSegmt9 = allocator.allocate(structLayout);
				byteHandle1.set(structSegmt9, (byte)9);
				MemorySegment structSegmt10 = allocator.allocate(structLayout);
				byteHandle1.set(structSegmt10, (byte)10);

				VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
						.addVarg(structLayout, structSegmt2)
						.addVarg(structLayout, structSegmt3)
						.addVarg(structLayout, structSegmt4)
						.addVarg(structLayout, structSegmt5)
						.addVarg(structLayout, structSegmt6)
						.addVarg(structLayout, structSegmt7)
						.addVarg(structLayout, structSegmt8)
						.addVarg(structLayout, structSegmt9)
						.addVarg(structLayout, structSegmt10), arena.scope());
				byte result = (byte)mh.invoke(10, vaList.segment());
				Assert.assertEquals(result, 55);
			}
		}
	}

	@Test
	public void test_add2BytesOfStructsFromVaList() throws Throwable {
		/* VaList on Windows/x86_64 in OpenJDK has problem in supporting the struct with
		 * two byte elements (confirmed by OpenJDK/Hotspot). Thus, the test is disabled
		 * on Windows/x86_64 for now till the issue is fixed in OpenJDK and verified on
		 * OpenJDK/Hotspot in the future.
		 */
		if (!isWinX64) {
			GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"), JAVA_BYTE.withName("elem2"));
			VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
			VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

			MemorySegment functionSymbol = nativeLibLookup.find("add2BytesOfStructsFromVaList").get();
			FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_INT, ADDRESS);
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			try (Arena arena = Arena.openConfined()) {
				SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
				MemorySegment structSegmt1 = allocator.allocate(structLayout);
				byteHandle1.set(structSegmt1, (byte)1);
				byteHandle2.set(structSegmt1, (byte)2);
				MemorySegment structSegmt2 = allocator.allocate(structLayout);
				byteHandle1.set(structSegmt2, (byte)3);
				byteHandle2.set(structSegmt2, (byte)4);

				VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
						.addVarg(structLayout, structSegmt2), arena.scope());
				byte result = (byte)mh.invoke(2, vaList.segment());
				Assert.assertEquals(result, 10);
			}
		}
	}

	@Test
	public void test_add3BytesOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"),
									JAVA_BYTE.withName("elem2"), JAVA_BYTE.withName("elem3"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle byteHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		MemorySegment functionSymbol = nativeLibLookup.find("add3BytesOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)1);
			byteHandle2.set(structSegmt1, (byte)2);
			byteHandle3.set(structSegmt1, (byte)3);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)4);
			byteHandle2.set(structSegmt2, (byte)5);
			byteHandle3.set(structSegmt2, (byte)6);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), arena.scope());
			byte result = (byte)mh.invoke(2, vaList.segment());
			Assert.assertEquals(result, 21);
		}
	}

	@Test
	public void test_add5BytesOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"),
				JAVA_BYTE.withName("elem2"), JAVA_BYTE.withName("elem3"), JAVA_BYTE.withName("elem4"), JAVA_BYTE.withName("elem5"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle byteHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));
		VarHandle byteHandle4 = structLayout.varHandle(PathElement.groupElement("elem4"));
		VarHandle byteHandle5 = structLayout.varHandle(PathElement.groupElement("elem5"));

		MemorySegment functionSymbol = nativeLibLookup.find("add5BytesOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)1);
			byteHandle2.set(structSegmt1, (byte)2);
			byteHandle3.set(structSegmt1, (byte)3);
			byteHandle4.set(structSegmt1, (byte)4);
			byteHandle5.set(structSegmt1, (byte)5);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)6);
			byteHandle2.set(structSegmt2, (byte)7);
			byteHandle3.set(structSegmt2, (byte)8);
			byteHandle4.set(structSegmt2, (byte)9);
			byteHandle5.set(structSegmt2, (byte)10);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), arena.scope());
			byte result = (byte)mh.invoke(2, vaList.segment());
			Assert.assertEquals(result, 55);
		}
	}

	@Test
	public void test_add7BytesOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_BYTE.withName("elem1"),
				JAVA_BYTE.withName("elem2"), JAVA_BYTE.withName("elem3"), JAVA_BYTE.withName("elem4"),
				JAVA_BYTE.withName("elem5"), JAVA_BYTE.withName("elem6"), JAVA_BYTE.withName("elem7"));
		VarHandle byteHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle byteHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));
		VarHandle byteHandle4 = structLayout.varHandle(PathElement.groupElement("elem4"));
		VarHandle byteHandle5 = structLayout.varHandle(PathElement.groupElement("elem5"));
		VarHandle byteHandle6 = structLayout.varHandle(PathElement.groupElement("elem6"));
		VarHandle byteHandle7 = structLayout.varHandle(PathElement.groupElement("elem7"));

		MemorySegment functionSymbol = nativeLibLookup.find("add7BytesOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt1, (byte)1);
			byteHandle2.set(structSegmt1, (byte)2);
			byteHandle3.set(structSegmt1, (byte)3);
			byteHandle4.set(structSegmt1, (byte)4);
			byteHandle5.set(structSegmt1, (byte)5);
			byteHandle6.set(structSegmt1, (byte)6);
			byteHandle7.set(structSegmt1, (byte)7);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			byteHandle1.set(structSegmt2, (byte)8);
			byteHandle2.set(structSegmt2, (byte)9);
			byteHandle3.set(structSegmt2, (byte)10);
			byteHandle4.set(structSegmt2, (byte)11);
			byteHandle5.set(structSegmt2, (byte)12);
			byteHandle6.set(structSegmt2, (byte)13);
			byteHandle7.set(structSegmt2, (byte)14);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), arena.scope());
			byte result = (byte)mh.invoke(2, vaList.segment());
			Assert.assertEquals(result, 105);
		}
	}

	@Test
	public void test_add1ShortOfStructsFromVaList() throws Throwable {
		/* There are a few issues with the test on some platforms as follows:
		 * 1) VaList on Linux/x86_64 in OpenJDK is unable to handle the va_list with
		 *    over 8 arguments (confirmed by OpenJDK/Hotspot).
		 * 2) VaList on Linux/Aarch64 and Windows/x86_64 in OpenJDK has problem in supporting
		 *    the struct with only one integral element (confirmed by OpenJDK/Hotspot).
		 * Thus, the test is disabled on both these platforms for now till these issues
		 * are fixed in OpenJDK and verified on OpenJDK/Hotspot in the future.
		 */
		if (!isLinuxX64 && !isLinuxAarch64 && !isWinX64) {
			GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"));
			VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));

			MemorySegment functionSymbol = nativeLibLookup.find("add1ShortOfStructsFromVaList").get();
			FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_INT, ADDRESS);
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			try (Arena arena = Arena.openConfined()) {
				SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
				MemorySegment structSegmt1 = allocator.allocate(structLayout);
				shortHandle1.set(structSegmt1, (short)111);
				MemorySegment structSegmt2 = allocator.allocate(structLayout);
				shortHandle1.set(structSegmt2, (short)222);
				MemorySegment structSegmt3 = allocator.allocate(structLayout);
				shortHandle1.set(structSegmt3, (short)333);
				MemorySegment structSegmt4 = allocator.allocate(structLayout);
				shortHandle1.set(structSegmt4, (short)444);
				MemorySegment structSegmt5 = allocator.allocate(structLayout);
				shortHandle1.set(structSegmt5, (short)555);
				MemorySegment structSegmt6 = allocator.allocate(structLayout);
				shortHandle1.set(structSegmt6, (short)666);
				MemorySegment structSegmt7 = allocator.allocate(structLayout);
				shortHandle1.set(structSegmt7, (short)777);
				MemorySegment structSegmt8 = allocator.allocate(structLayout);
				shortHandle1.set(structSegmt8, (short)888);
				MemorySegment structSegmt9 = allocator.allocate(structLayout);
				shortHandle1.set(structSegmt9, (short)999);
				MemorySegment structSegmt10 = allocator.allocate(structLayout);
				shortHandle1.set(structSegmt10, (short)123);

				VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
						.addVarg(structLayout, structSegmt2)
						.addVarg(structLayout, structSegmt3)
						.addVarg(structLayout, structSegmt4)
						.addVarg(structLayout, structSegmt5)
						.addVarg(structLayout, structSegmt6)
						.addVarg(structLayout, structSegmt7)
						.addVarg(structLayout, structSegmt8)
						.addVarg(structLayout, structSegmt9)
						.addVarg(structLayout, structSegmt10), arena.scope());
				short result = (short)mh.invoke(10, vaList.segment());
				Assert.assertEquals(result, 5118);
			}
		}
	}

	@Test
	public void test_add2ShortsOfStructsFromVaList() throws Throwable {
		/* VaList on Windows/x86_64 in OpenJDK has problem in supporting the struct with
		 * two short elements (confirmed by OpenJDK/Hotspot). Thus, the test is disabled
		 * on Windows/x86_64 for now till the issue is fixed in OpenJDK and verified on
		 * OpenJDK/Hotspot in the future.
		 */
		if (!isWinX64) {
			GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"), JAVA_SHORT.withName("elem2"));
			VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
			VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

			MemorySegment functionSymbol = nativeLibLookup.find("add2ShortsOfStructsFromVaList").get();
			FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_INT, ADDRESS);
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			try (Arena arena = Arena.openConfined()) {
				SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
				MemorySegment structSegmt1 = allocator.allocate(structLayout);
				shortHandle1.set(structSegmt1, (short)111);
				shortHandle2.set(structSegmt1, (short)222);
				MemorySegment structSegmt2 = allocator.allocate(structLayout);
				shortHandle1.set(structSegmt2, (short)333);
				shortHandle2.set(structSegmt2, (short)444);

				VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
						.addVarg(structLayout, structSegmt2), arena.scope());
				short result = (short)mh.invoke(2, vaList.segment());
				Assert.assertEquals(result, 1110);
			}
		}
	}

	@Test
	public void test_add3ShortsOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"),
				JAVA_SHORT.withName("elem2"), JAVA_SHORT.withName("elem3"));
		VarHandle shortHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle shortHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		MemorySegment functionSymbol = nativeLibLookup.find("add3ShortsOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt1, (short)111);
			shortHandle2.set(structSegmt1, (short)222);
			shortHandle3.set(structSegmt1, (short)333);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			shortHandle1.set(structSegmt2, (short)444);
			shortHandle2.set(structSegmt2, (short)555);
			shortHandle3.set(structSegmt2, (short)666);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), arena.scope());
			short result = (short)mh.invoke(2, vaList.segment());
			Assert.assertEquals(result, 2331);
		}
	}

	@Test
	public void test_add1IntOfStructsFromVaList() throws Throwable {
		/* There are a few issues with the test on some platforms as follows:
		 * 1) VaList on Linux/x86_64 in OpenJDK is unable to handle the va_list with
		 *    over 8 arguments (confirmed by OpenJDK/Hotspot).
		 * 2) VaList on Linux/Aarch64 and Windows/x86_64 in OpenJDK has problem in supporting
		 *    the struct with only one integral element (confirmed by OpenJDK/Hotspot).
		 * Thus, the test is disabled on both these platforms for now till these issues
		 * are fixed in OpenJDK and verified on OpenJDK/Hotspot in the future.
		 */
		if (!isLinuxX64 && !isLinuxAarch64 && !isWinX64) {
			GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"));
			VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));

			MemorySegment functionSymbol = nativeLibLookup.find("add1IntOfStructsFromVaList").get();
			FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			try (Arena arena = Arena.openConfined()) {
				SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
				MemorySegment structSegmt1 = allocator.allocate(structLayout);
				intHandle1.set(structSegmt1, 1111111);
				MemorySegment structSegmt2 = allocator.allocate(structLayout);
				intHandle1.set(structSegmt2, 2222222);
				MemorySegment structSegmt3 = allocator.allocate(structLayout);
				intHandle1.set(structSegmt3, 3333333);
				MemorySegment structSegmt4 = allocator.allocate(structLayout);
				intHandle1.set(structSegmt4, 4444444);
				MemorySegment structSegmt5 = allocator.allocate(structLayout);
				intHandle1.set(structSegmt5, 5555555);
				MemorySegment structSegmt6 = allocator.allocate(structLayout);
				intHandle1.set(structSegmt6, 6666666);
				MemorySegment structSegmt7 = allocator.allocate(structLayout);
				intHandle1.set(structSegmt7, 7777777);
				MemorySegment structSegmt8 = allocator.allocate(structLayout);
				intHandle1.set(structSegmt8, 8888888);
				MemorySegment structSegmt9 = allocator.allocate(structLayout);
				intHandle1.set(structSegmt9, 9999999);
				MemorySegment structSegmt10 = allocator.allocate(structLayout);
				intHandle1.set(structSegmt10, 1234567);

				VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
						.addVarg(structLayout, structSegmt2)
						.addVarg(structLayout, structSegmt3)
						.addVarg(structLayout, structSegmt4)
						.addVarg(structLayout, structSegmt5)
						.addVarg(structLayout, structSegmt6)
						.addVarg(structLayout, structSegmt7)
						.addVarg(structLayout, structSegmt8)
						.addVarg(structLayout, structSegmt9)
						.addVarg(structLayout, structSegmt10), arena.scope());
				int result = (int)mh.invoke(10, vaList.segment());
				Assert.assertEquals(result, 51234562);
			}
		}
	}

	@Test
	public void test_add2IntsOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		MemorySegment functionSymbol = nativeLibLookup.find("add2IntsOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 1122333);
			intHandle2.set(structSegmt1, 4455666);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 2244668);
			intHandle2.set(structSegmt2, 1133557);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), arena.scope());
			int result = (int)mh.invoke(2, vaList.segment());
			Assert.assertEquals(result, 8956224);
		}
	}

	@Test
	public void test_add3IntsOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_INT.withName("elem2"), JAVA_INT.withName("elem3"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle intHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		MemorySegment functionSymbol = nativeLibLookup.find("add3IntsOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 1122333);
			intHandle2.set(structSegmt1, 4455666);
			intHandle3.set(structSegmt1, 7788999);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 1133555);
			intHandle2.set(structSegmt2, 2244666);
			intHandle3.set(structSegmt2, 3322111);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), arena.scope());
			int result = (int)mh.invoke(2, vaList.segment());
			Assert.assertEquals(result, 20067330);
		}
	}

	@Test
	public void test_add2LongsOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		MemorySegment functionSymbol = nativeLibLookup.find("add2LongsOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 6677889911L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 2233445566L);
			longHandle2.set(structSegmt2, 7788991122L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), arena.scope());
			long result = (long)mh.invoke(2, vaList.segment());
			Assert.assertEquals(result, 17822661054L);
		}
	}

	@Test
	public void test_add1FloatOfStructsFromVaList() throws Throwable {
		/* VaList on Windows/x86_64 in OpenJDK has problem in supporting the struct with only
		 * one float element (confirmed by OpenJDK/Hotspot). Thus, the test is disabled on
		 * on Windows/x86_64 for now till the issue is fixed in OpenJDK and verified on
		 * OpenJDK/Hotspot in the future.
		 */
		if (!isWinX64) {
			GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"));
			VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));

			MemorySegment functionSymbol = nativeLibLookup.find("add1FloatOfStructsFromVaList").get();
			FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_INT, ADDRESS);
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			try (Arena arena = Arena.openConfined()) {
				SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
				MemorySegment structSegmt1 = allocator.allocate(structLayout);
				floatHandle1.set(structSegmt1, 1.11F);
				MemorySegment structSegmt2 = allocator.allocate(structLayout);
				floatHandle1.set(structSegmt2, 2.22F);
				MemorySegment structSegmt3 = allocator.allocate(structLayout);
				floatHandle1.set(structSegmt3, 3.33F);
				MemorySegment structSegmt4 = allocator.allocate(structLayout);
				floatHandle1.set(structSegmt4, 4.44F);
				MemorySegment structSegmt5 = allocator.allocate(structLayout);
				floatHandle1.set(structSegmt5, 5.56F);

				VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
						.addVarg(structLayout, structSegmt2)
						.addVarg(structLayout, structSegmt3)
						.addVarg(structLayout, structSegmt4)
						.addVarg(structLayout, structSegmt5), arena.scope());
				float result = (float)mh.invoke(5, vaList.segment());
				Assert.assertEquals(result, 16.66F, 0.01F);
			}
		}
	}

	@Test
	public void test_add2FloatsOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		MemorySegment functionSymbol = nativeLibLookup.find("add2FloatsOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt1, 1.11F);
			floatHandle2.set(structSegmt1, 2.22F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt2, 3.33F);
			floatHandle2.set(structSegmt2, 4.44F);
			MemorySegment structSegmt3 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt3, 5.55F);
			floatHandle2.set(structSegmt3, 6.66F);
			MemorySegment structSegmt4 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt4, 7.77F);
			floatHandle2.set(structSegmt4, 8.88F);
			MemorySegment structSegmt5 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt5, 9.99F);
			floatHandle2.set(structSegmt5, 1.23F);
			MemorySegment structSegmt6 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt6, 4.56F);
			floatHandle2.set(structSegmt6, 7.89F);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2)
					.addVarg(structLayout, structSegmt3)
					.addVarg(structLayout, structSegmt4)
					.addVarg(structLayout, structSegmt5)
					.addVarg(structLayout, structSegmt6), arena.scope());
			float result = (float)mh.invoke(6, vaList.segment());
			Assert.assertEquals(result, 63.63F, 0.01F);
		}
	}

	@Test
	public void test_add3FloatsOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = isStructPaddingNotRequired ? MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_FLOAT.withName("elem3")) : MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
						JAVA_FLOAT.withName("elem2"), JAVA_FLOAT.withName("elem3"), MemoryLayout.paddingLayout(32));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle floatHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		MemorySegment functionSymbol = nativeLibLookup.find("add3FloatsOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt1, 1.11F);
			floatHandle2.set(structSegmt1, 2.22F);
			floatHandle3.set(structSegmt1, 3.33F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt2, 4.44F);
			floatHandle2.set(structSegmt2, 5.55F);
			floatHandle3.set(structSegmt2, 6.66F);
			MemorySegment structSegmt3 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt3, 7.77F);
			floatHandle2.set(structSegmt3, 8.88F);
			floatHandle3.set(structSegmt3, 9.99F);
			MemorySegment structSegmt4 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt4, 1.23F);
			floatHandle2.set(structSegmt4, 4.56F);
			floatHandle3.set(structSegmt4, 7.89F);
			MemorySegment structSegmt5 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt5, 9.87F);
			floatHandle2.set(structSegmt5, 6.54F);
			floatHandle3.set(structSegmt5, 3.21F);
			MemorySegment structSegmt6 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt6, 2.46F);
			floatHandle2.set(structSegmt6, 8.13F);
			floatHandle3.set(structSegmt6, 5.79F);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2)
					.addVarg(structLayout, structSegmt3)
					.addVarg(structLayout, structSegmt4)
					.addVarg(structLayout, structSegmt5)
					.addVarg(structLayout, structSegmt6), arena.scope());
			float result = (float)mh.invoke(6, vaList.segment());
			Assert.assertEquals(result, 99.63F, 0.01F);
		}
	}

	@Test
	public void test_add1DoubleOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"));
		VarHandle floatHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));

		MemorySegment functionSymbol = nativeLibLookup.find("add1DoubleOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt1, 11111.1001D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt2, 11111.1002D);
			MemorySegment structSegmt3 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt3, 11111.1003D);
			MemorySegment structSegmt4 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt4, 11111.1004D);
			MemorySegment structSegmt5 = allocator.allocate(structLayout);
			floatHandle1.set(structSegmt5, 11111.1005D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2)
					.addVarg(structLayout, structSegmt3)
					.addVarg(structLayout, structSegmt4)
					.addVarg(structLayout, structSegmt5), arena.scope());
			double result = (double)mh.invoke(5, vaList.segment());
			Assert.assertEquals(result, 55555.5015D, 0.0001D);
		}
	}

	@Test
	public void test_add2DoublesOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		MemorySegment functionSymbol = nativeLibLookup.find("add2DoublesOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11150.1001D);
			doubleHandle2.set(structSegmt1, 11160.2002D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 11170.1001D);
			doubleHandle2.set(structSegmt2, 11180.2002D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), arena.scope());
			double result = (double)mh.invoke(2, vaList.segment());
			Assert.assertEquals(result, 44660.6006D, 0.0001D);
		}
	}

	@Test
	public void test_vprintfFromDefaultLibFromVaList() throws Throwable {
		/* 1) Disable the test on Windows given a misaligned access exception coming from
		 * java.base/java.lang.invoke.MemoryAccessVarHandleBase triggered by CLinker.toCString()
		 * is also captured on OpenJDK/Hotspot.
		 * 2) Disable the test on AIX as Valist is not yet implemented in OpenJDK.
		 */
		if (!isWinX64 && !isAixOS) {
			MemorySegment functionSymbol = defaultLibLookup.find("vprintf").get();
			FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, ADDRESS, ADDRESS);
			MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

			try (Arena arena = Arena.openConfined()) {
				SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
				MemorySegment formatSegmt = allocator.allocateUtf8String("%d * %d = %d\n");
				VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_INT, 7)
						.addVarg(JAVA_INT, 8)
						.addVarg(JAVA_INT, 56), arena.scope());
				mh.invoke(formatSegmt, vaList.segment());
			}
		}
	}

	@Test
	public void test_addIntShortOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_SHORT.withName("elem2"), MemoryLayout.paddingLayout(16));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		MemorySegment functionSymbol = nativeLibLookup.find("addIntShortOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt1, 1111111);
			elemHandle2.set(structSegmt1, (short)123);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt2, 2222222);
			elemHandle2.set(structSegmt2, (short)456);
			MemorySegment structSegmt3 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt3, 3333333);
			elemHandle2.set(structSegmt3, (short)789);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2)
					.addVarg(structLayout, structSegmt3), arena.scope());
			int result = (int)mh.invoke(3, vaList.segment());
			Assert.assertEquals(result, 6668034);
		}
	}

	@Test
	public void test_addShortIntOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"),
				MemoryLayout.paddingLayout(16), JAVA_INT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		MemorySegment functionSymbol = nativeLibLookup.find("addShortIntOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt1, (short)123);
			elemHandle2.set(structSegmt1, 1111111);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt2, (short)456);
			elemHandle2.set(structSegmt2, 2222222);
			MemorySegment structSegmt3 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt3, (short)789);
			elemHandle2.set(structSegmt3, 3333333);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2)
					.addVarg(structLayout, structSegmt3), arena.scope());
			int result = (int)mh.invoke(3, vaList.segment());
			Assert.assertEquals(result, 6668034);
		}
	}

	@Test
	public void test_addIntLongOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				MemoryLayout.paddingLayout(32), JAVA_LONG.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		MemorySegment functionSymbol = nativeLibLookup.find("addIntLongOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt1, 1111111);
			elemHandle2.set(structSegmt1, 101010101010L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt2, 2222222);
			elemHandle2.set(structSegmt2, 202020202020L);
			MemorySegment structSegmt3 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt3, 3333333);
			elemHandle2.set(structSegmt3, 303030303030L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2)
					.addVarg(structLayout, structSegmt3), arena.scope());
			long result = (long)mh.invoke(3, vaList.segment());
			Assert.assertEquals(result, 606067272726L);
		}
	}

	@Test
	public void test_addLongIntOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"),
				JAVA_INT.withName("elem2"), MemoryLayout.paddingLayout(32));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		MemorySegment functionSymbol = nativeLibLookup.find("addLongIntOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt1, 101010101010L);
			elemHandle2.set(structSegmt1, 1111111);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt2, 202020202020L);
			elemHandle2.set(structSegmt2, 2222222);
			MemorySegment structSegmt3 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt3, 303030303030L);
			elemHandle2.set(structSegmt3, 3333333);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2)
					.addVarg(structLayout, structSegmt3), arena.scope());
			long result = (long)mh.invoke(3, vaList.segment());
			Assert.assertEquals(result, 606067272726L);
		}
	}

	@Test
	public void test_addFloatDoubleOfStructsFromVaList() throws Throwable {
		/* The size of [float, double] on AIX/PPC 64-bit is 12 bytes without padding by default
		 * while the same struct is 16 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2").withBitAlignment(32)) : MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
				MemoryLayout.paddingLayout(32), JAVA_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		MemorySegment functionSymbol = nativeLibLookup.find("addFloatDoubleOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt1, 1.11F);
			elemHandle2.set(structSegmt1, 222.222D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt2, 2.22F);
			elemHandle2.set(structSegmt2, 333.333D);
			MemorySegment structSegmt3 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt3, 3.33F);
			elemHandle2.set(structSegmt3, 444.444D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2)
					.addVarg(structLayout, structSegmt3), arena.scope());
			double result = (double)mh.invoke(3, vaList.segment());
			Assert.assertEquals(result, 1006.659D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleFloatOfStructsFromVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"),
				JAVA_FLOAT.withName("elem2") , MemoryLayout.paddingLayout(32));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleFloatOfStructsFromVaList").get();
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_INT, ADDRESS);
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.nativeAllocator(arena.scope());
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt1, 222.222D);
			elemHandle2.set(structSegmt1, 1.11F);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt2, 333.333D);
			elemHandle2.set(structSegmt2, 2.22F);
			MemorySegment structSegmt3 = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt3, 444.444D);
			elemHandle2.set(structSegmt3, 3.33F);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2)
					.addVarg(structLayout, structSegmt3), arena.scope());
			double result = (double)mh.invoke(3, vaList.segment());
			Assert.assertEquals(result, 1006.659D, 0.001D);
		}
	}
}
