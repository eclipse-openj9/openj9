/*******************************************************************************
 * Copyright IBM Corp. and others 2022
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
package org.openj9.test.jep424.valist;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import static org.testng.Assert.fail;

import java.lang.invoke.VarHandle;
import java.util.NoSuchElementException;

import java.lang.foreign.GroupLayout;
import java.lang.foreign.MemoryAddress;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemoryLayout.PathElement;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.MemorySession;
import java.lang.foreign.SegmentAllocator;
import java.lang.foreign.VaList;
import static java.lang.foreign.ValueLayout.*;
import static java.lang.foreign.VaList.Builder;

/**
 * Test cases for JEP 424: Foreign Linker API (Preview) for the vararg list in VaList API specific cases.
 */
@Test(groups = { "level.sanity" })
public class ApiTests {
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static String arch = System.getProperty("os.arch").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinX64 = osName.contains("win") && (arch.equals("amd64") || arch.equals("x86_64"));
	private static boolean isMacOsAarch64 = osName.contains("mac") && arch.contains("aarch64");
	private static boolean isSysVPPC64le = osName.contains("linux") && arch.contains("ppc64");

	@Test
	public void test_emptyVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList emptyVaList = VaList.empty();
			/* As specified in the implemention of OpenJDK, a NULL address is set to
			 * the empty va_list on Windows/x86_64, MacOS/Aarch64, Linux/ppc64le and
			 * AIX/ppc64 while the va_list without any argument is created on a fixed
			 * address on other platforms.
			 */
			if (isWinX64 || isMacOsAarch64 || isSysVPPC64le || isAixOS) {
				Assert.assertEquals(emptyVaList.address(), MemoryAddress.NULL);
			} else {
				Assert.assertNotEquals(emptyVaList.address(), MemoryAddress.NULL);
			}
		}
	}

	@Test
	public void test_vaListSession() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_INT, 100), session);
			MemorySession vaListSession = vaList.session();
			Assert.assertEquals(vaListSession, session);
		}
	}

	@Test
	public void test_checkIntVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_INT, 700)
					.addVarg(JAVA_INT, 800)
					.addVarg(JAVA_INT, 900), session);

			Assert.assertEquals(vaList.nextVarg(JAVA_INT), 700); /* the 1st argument */
			Assert.assertEquals(vaList.nextVarg(JAVA_INT), 800); /* the 2nd argument */
			Assert.assertEquals(vaList.nextVarg(JAVA_INT), 900); /* the 3rd argument */
		}
	}

	@Test
	public void test_checkLongVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_LONG, 700000L)
					.addVarg(JAVA_LONG, 800000L)
					.addVarg(JAVA_LONG, 900000L), session);

			Assert.assertEquals(vaList.nextVarg(JAVA_LONG), 700000L); /* the 1st argument */
			Assert.assertEquals(vaList.nextVarg(JAVA_LONG), 800000L); /* the 2nd argument */
			Assert.assertEquals(vaList.nextVarg(JAVA_LONG), 900000L); /* the 3rd argument */
		}
	}

	@Test
	public void test_checkDoubleVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_DOUBLE, 111150.1001D)
					.addVarg(JAVA_DOUBLE, 111160.2002D)
					.addVarg(JAVA_DOUBLE, 111170.1001D), session);

			Assert.assertEquals(vaList.nextVarg(JAVA_DOUBLE), 111150.1001D, 0.0001D); /* the 1st argument */
			Assert.assertEquals(vaList.nextVarg(JAVA_DOUBLE), 111160.2002D, 0.0001D); /* the 2nd argument */
			Assert.assertEquals(vaList.nextVarg(JAVA_DOUBLE), 111170.1001D, 0.0001D); /* the 3rd argument */
		}
	}

	@Test
	public void test_checkIntPtrVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment intSegmt1 = allocator.allocate(JAVA_INT, 700);
			MemorySegment intSegmt2 = allocator.allocate(JAVA_INT, 800);
			MemorySegment intSegmt3 = allocator.allocate(JAVA_INT, 900);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, intSegmt1.address())
					.addVarg(ADDRESS, intSegmt2.address())
					.addVarg(ADDRESS, intSegmt3.address()), session);

			Assert.assertEquals(vaList.nextVarg(ADDRESS), intSegmt1.address()); /* the 1st argument */
			Assert.assertEquals(vaList.nextVarg(ADDRESS), intSegmt2.address()); /* the 2nd argument */
			Assert.assertEquals(vaList.nextVarg(ADDRESS), intSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_checkLongPtrVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment longSegmt1 = allocator.allocate(JAVA_LONG, 700000L);
			MemorySegment longSegmt2 = allocator.allocate(JAVA_LONG, 800000L);
			MemorySegment longSegmt3 = allocator.allocate(JAVA_LONG, 900000L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, longSegmt1.address())
					.addVarg(ADDRESS, longSegmt2.address())
					.addVarg(ADDRESS, longSegmt3.address()), session);

			Assert.assertEquals(vaList.nextVarg(ADDRESS), longSegmt1.address()); /* the 1st argument */
			Assert.assertEquals(vaList.nextVarg(ADDRESS), longSegmt2.address()); /* the 2nd argument */
			Assert.assertEquals(vaList.nextVarg(ADDRESS), longSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_checkDoublePtrVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment doubleSegmt1 = allocator.allocate(JAVA_DOUBLE, 111150.1001D);
			MemorySegment doubleSegmt2 = allocator.allocate(JAVA_DOUBLE, 111160.2002D);
			MemorySegment doubleSegmt3 = allocator.allocate(JAVA_DOUBLE, 111170.1001D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, doubleSegmt1.address())
					.addVarg(ADDRESS, doubleSegmt2.address())
					.addVarg(ADDRESS, doubleSegmt3.address()), session);

			Assert.assertEquals(vaList.nextVarg(ADDRESS), doubleSegmt1.address()); /* the 1st argument */
			Assert.assertEquals(vaList.nextVarg(ADDRESS), doubleSegmt2.address()); /* the 2nd argument */
			Assert.assertEquals(vaList.nextVarg(ADDRESS), doubleSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_checkIntStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 1122333);
			intHandle2.set(structSegmt1, 4455666);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 2244668);
			intHandle2.set(structSegmt2, 1133557);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);

			MemorySegment argSegmt = vaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(argSegmt), 1122333); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(intHandle2.get(argSegmt), 4455666); /* the 2nd element of the 1st struct argument */
			argSegmt = vaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(argSegmt), 2244668); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(intHandle2.get(argSegmt), 1133557); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_checkLongStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 6677889911L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 2233445566L);
			longHandle2.set(structSegmt2, 7788991122L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);

			MemorySegment argSegmt = vaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(argSegmt), 1122334455L); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(longHandle2.get(argSegmt), 6677889911L); /* the 2nd element of the 1st struct argument */
			argSegmt = vaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(argSegmt), 2233445566L); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(longHandle2.get(argSegmt), 7788991122L); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_checkLongStructVaListFromPrefixAllocator() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 6677889911L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1), session);

			MemorySegment structSegmt2 = MemorySegment.allocateNative(structLayout, session);
			MemorySegment argSegmt = vaList.nextVarg(structLayout, SegmentAllocator.prefixAllocator(structSegmt2));
			Assert.assertEquals(longHandle1.get(argSegmt), 1122334455L); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(longHandle2.get(argSegmt), 6677889911L); /* the 2nd element of the 1st struct argument */
			Assert.assertEquals(longHandle1.get(structSegmt2), 1122334455L); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(longHandle2.get(structSegmt2), 6677889911L); /* the 2nd element of the 1st struct argument */
		}
	}

	@Test
	public void test_checkDoubleStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 111150.1001D);
			doubleHandle2.set(structSegmt1, 111160.2002D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 111170.1001D);
			doubleHandle2.set(structSegmt2, 111180.2002D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);

			MemorySegment argSegmt = vaList.nextVarg(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(argSegmt), 111150.1001D, 0.0001D); /* the 1st element of the 1st struct argument */
			Assert.assertEquals((double)doubleHandle2.get(argSegmt), 111160.2002D, 0.0001D); /* the 2nd element of the 1st struct argument */
			argSegmt = vaList.nextVarg(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(argSegmt), 111170.1001D, 0.0001D); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals((double)doubleHandle2.get(argSegmt), 111180.2002D, 0.0001D); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_copyIntVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_INT, 700)
					.addVarg(JAVA_INT, 800)
					.addVarg(JAVA_INT, 900), session);
			VaList resultVaList = vaList.copy();

			Assert.assertEquals(resultVaList.nextVarg(JAVA_INT), 700); /* the 1st argument */
			Assert.assertEquals(resultVaList.nextVarg(JAVA_INT), 800); /* the 2nd argument */
			Assert.assertEquals(resultVaList.nextVarg(JAVA_INT), 900); /* the 3rd argument */
		}
	}

	@Test
	public void test_copyLongVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_LONG, 700000L)
					.addVarg(JAVA_LONG, 800000L)
					.addVarg(JAVA_LONG, 900000L), session);
			VaList resultVaList = vaList.copy();

			Assert.assertEquals(resultVaList.nextVarg(JAVA_LONG), 700000L); /* the 1st argument */
			Assert.assertEquals(resultVaList.nextVarg(JAVA_LONG), 800000L); /* the 2nd argument */
			Assert.assertEquals(resultVaList.nextVarg(JAVA_LONG), 900000L); /* the 3rd argument */
		}
	}

	@Test
	public void test_copyDoubleVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_DOUBLE, 111150.1001D)
					.addVarg(JAVA_DOUBLE, 111160.2002D)
					.addVarg(JAVA_DOUBLE, 111170.1001D), session);
			VaList resultVaList = vaList.copy();

			Assert.assertEquals(resultVaList.nextVarg(JAVA_DOUBLE), 111150.1001D, 0001D); /* the 1st argument */
			Assert.assertEquals(resultVaList.nextVarg(JAVA_DOUBLE), 111160.2002D, 0001D); /* the 2nd argument */
			Assert.assertEquals(resultVaList.nextVarg(JAVA_DOUBLE), 111170.1001D, 0001D); /* the 3rd argument */
		}
	}

	@Test
	public void test_copyIntPtrVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment intSegmt1 = allocator.allocate(JAVA_INT, 700);
			MemorySegment intSegmt2 = allocator.allocate(JAVA_INT, 800);
			MemorySegment intSegmt3 = allocator.allocate(JAVA_INT, 900);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, intSegmt1.address())
					.addVarg(ADDRESS, intSegmt2.address())
					.addVarg(ADDRESS, intSegmt3.address()), session);
			VaList resultVaList = vaList.copy();

			Assert.assertEquals(resultVaList.nextVarg(ADDRESS), intSegmt1.address()); /* the 1st argument */
			Assert.assertEquals(resultVaList.nextVarg(ADDRESS), intSegmt2.address()); /* the 2nd argument */
			Assert.assertEquals(resultVaList.nextVarg(ADDRESS), intSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_copyLongPtrVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment longSegmt1 = allocator.allocate(JAVA_LONG, 700000L);
			MemorySegment longSegmt2 = allocator.allocate(JAVA_LONG, 800000L);
			MemorySegment longSegmt3 = allocator.allocate(JAVA_LONG, 900000L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, longSegmt1.address())
					.addVarg(ADDRESS, longSegmt2.address())
					.addVarg(ADDRESS, longSegmt3.address()), session);
			VaList resultVaList = vaList.copy();

			Assert.assertEquals(resultVaList.nextVarg(ADDRESS), longSegmt1.address()); /* the 1st argument */
			Assert.assertEquals(resultVaList.nextVarg(ADDRESS), longSegmt2.address()); /* the 2nd argument */
			Assert.assertEquals(resultVaList.nextVarg(ADDRESS), longSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_copyDoublePtrVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment doubleSegmt1 = allocator.allocate(JAVA_DOUBLE, 111150.1001D);
			MemorySegment doubleSegmt2 = allocator.allocate(JAVA_DOUBLE, 111160.2002D);
			MemorySegment doubleSegmt3 = allocator.allocate(JAVA_DOUBLE, 111170.1001D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, doubleSegmt1.address())
					.addVarg(ADDRESS, doubleSegmt2.address())
					.addVarg(ADDRESS, doubleSegmt3.address()), session);
			VaList resultVaList = vaList.copy();

			Assert.assertEquals(resultVaList.nextVarg(ADDRESS), doubleSegmt1.address()); /* the 1st argument */
			Assert.assertEquals(resultVaList.nextVarg(ADDRESS), doubleSegmt2.address()); /* the 2nd argument */
			Assert.assertEquals(resultVaList.nextVarg(ADDRESS), doubleSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_copyIntStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 1122333);
			intHandle2.set(structSegmt1, 4455666);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 2244668);
			intHandle2.set(structSegmt2, 1133557);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);
			VaList resultVaList = vaList.copy();

			MemorySegment resultArgSegmt = resultVaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(resultArgSegmt), 1122333); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(intHandle2.get(resultArgSegmt), 4455666); /* the 2nd element of the 1st struct argument */
			resultArgSegmt = resultVaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(resultArgSegmt), 2244668); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(intHandle2.get(resultArgSegmt), 1133557); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_copyLongStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 6677889911L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 2233445566L);
			longHandle2.set(structSegmt2, 7788991122L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);
			VaList resultVaList = vaList.copy();

			MemorySegment resultArgSegmt = resultVaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(resultArgSegmt), 1122334455L); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(longHandle2.get(resultArgSegmt), 6677889911L); /* the 2nd element of the 1st struct argument */
			resultArgSegmt = resultVaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(resultArgSegmt), 2233445566L); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(longHandle2.get(resultArgSegmt), 7788991122L); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_copyDoubleStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 111150.1001D);
			doubleHandle2.set(structSegmt1, 111160.2002D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 111170.1001D);
			doubleHandle2.set(structSegmt2, 111180.2002D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);
			VaList resultVaList = vaList.copy();

			MemorySegment resultArgSegmt = resultVaList.nextVarg(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(resultArgSegmt), 111150.1001D, 0.0001D); /* the 1st element of the 1st struct argument */
			Assert.assertEquals((double)doubleHandle2.get(resultArgSegmt), 111160.2002D, 0.0001D); /* the 2nd element of the 1st struct argument */
			resultArgSegmt = resultVaList.nextVarg(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(resultArgSegmt), 111170.1001D, 0.0001D); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals((double)doubleHandle2.get(resultArgSegmt), 111180.2002D, 0.0001D); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test(expectedExceptions = NoSuchElementException.class, expectedExceptionsMessageRegExp = "No such element.*")
	public void test_NoMoreNextArg_IntVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_INT, 700)
					.addVarg(JAVA_INT, 800)
					.addVarg(JAVA_INT, 900), session);

			vaList.nextVarg(JAVA_INT); /* the 1st argument */
			vaList.nextVarg(JAVA_INT); /* the 2nd argument */
			vaList.nextVarg(JAVA_INT); /* the 3rd argument */

			/* An exception is thrown as there is no more argument in VaList */
			vaList.nextVarg(JAVA_INT);
			fail("Failed to throw out NoSuchElementException when nextVarg() exceeds the memory region of VaList");
		}
	}

	@Test
	public void test_createIntVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_INT, 700)
					.addVarg(JAVA_INT, 800)
					.addVarg(JAVA_INT, 900), session);
			VaList newVaList = VaList.ofAddress(vaList.address(), session);

			Assert.assertEquals(newVaList.nextVarg(JAVA_INT), 700); /* the 1st argument */
			Assert.assertEquals(newVaList.nextVarg(JAVA_INT), 800); /* the 2nd argument */
			Assert.assertEquals(newVaList.nextVarg(JAVA_INT), 900); /* the 3rd argument */
		}
	}

	@Test
	public void test_createLongVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_LONG, 700000L)
					.addVarg(JAVA_LONG, 800000L)
					.addVarg(JAVA_LONG, 900000L), session);
			VaList newVaList = VaList.ofAddress(vaList.address(), session);

			Assert.assertEquals(newVaList.nextVarg(JAVA_LONG), 700000L); /* the 1st argument */
			Assert.assertEquals(newVaList.nextVarg(JAVA_LONG), 800000L); /* the 2nd argument */
			Assert.assertEquals(newVaList.nextVarg(JAVA_LONG), 900000L); /* the 3rd argument */
		}
	}

	@Test
	public void test_createDoubleVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_DOUBLE, 111150.1001D)
					.addVarg(JAVA_DOUBLE, 111160.2002D)
					.addVarg(JAVA_DOUBLE, 111170.1001D), session);
			VaList newVaList = VaList.ofAddress(vaList.address(), session);

			Assert.assertEquals(newVaList.nextVarg(JAVA_DOUBLE), 111150.1001D, 0.0001D); /* the 1st argument */
			Assert.assertEquals(newVaList.nextVarg(JAVA_DOUBLE), 111160.2002D, 0.0001D); /* the 2nd argument */
			Assert.assertEquals(newVaList.nextVarg(JAVA_DOUBLE), 111170.1001D, 0.0001D); /* the 3rd argument */
		}
	}

	@Test
	public void test_createIntPtrVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment intSegmt1 = allocator.allocate(JAVA_INT, 700);
			MemorySegment intSegmt2 = allocator.allocate(JAVA_INT, 800);
			MemorySegment intSegmt3 = allocator.allocate(JAVA_INT, 900);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, intSegmt1.address())
					.addVarg(ADDRESS, intSegmt2.address())
					.addVarg(ADDRESS, intSegmt3.address()), session);
			VaList newVaList = VaList.ofAddress(vaList.address(), session);

			MemoryAddress resultAddr = newVaList.nextVarg(ADDRESS);
			Assert.assertEquals(resultAddr.get(JAVA_INT, 0), 700); /* the 1st argument */
			resultAddr = newVaList.nextVarg(ADDRESS);
			Assert.assertEquals(resultAddr.get(JAVA_INT, 0), 800); /* the 2nd argument */
			resultAddr = newVaList.nextVarg(ADDRESS);
			Assert.assertEquals(resultAddr.get(JAVA_INT, 0), 900); /* the 3rd argument */
		}
	}

	@Test
	public void test_createLongPtrVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment longSegmt1 = allocator.allocate(JAVA_LONG, 700000L);
			MemorySegment longSegmt2 = allocator.allocate(JAVA_LONG, 800000L);
			MemorySegment longSegmt3 = allocator.allocate(JAVA_LONG, 900000L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, longSegmt1.address())
					.addVarg(ADDRESS, longSegmt2.address())
					.addVarg(ADDRESS, longSegmt3.address()), session);
			VaList newVaList = VaList.ofAddress(vaList.address(), session);

			MemoryAddress resultAddr = newVaList.nextVarg(ADDRESS);
			Assert.assertEquals(resultAddr.get(JAVA_LONG, 0), 700000L); /* the 1st argument */
			resultAddr = newVaList.nextVarg(ADDRESS);
			Assert.assertEquals(resultAddr.get(JAVA_LONG, 0), 800000L); /* the 2nd argument */
			resultAddr = newVaList.nextVarg(ADDRESS);
			Assert.assertEquals(resultAddr.get(JAVA_LONG, 0), 900000L); /* the 3rd argument */
		}
	}

	@Test
	public void test_createDoublePtrVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment doubleSegmt1 = allocator.allocate(JAVA_DOUBLE, 111150.1001D);
			MemorySegment doubleSegmt2 = allocator.allocate(JAVA_DOUBLE, 111160.2002D);
			MemorySegment doubleSegmt3 = allocator.allocate(JAVA_DOUBLE, 111170.1001D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, doubleSegmt1.address())
					.addVarg(ADDRESS, doubleSegmt2.address())
					.addVarg(ADDRESS, doubleSegmt3.address()), session);
			VaList newVaList = VaList.ofAddress(vaList.address(), session);

			MemoryAddress resultAddr = newVaList.nextVarg(ADDRESS);
			Assert.assertEquals(resultAddr.get(JAVA_DOUBLE, 0), 111150.1001D, 0.0001D); /* the 1st argument */
			resultAddr = newVaList.nextVarg(ADDRESS);
			Assert.assertEquals(resultAddr.get(JAVA_DOUBLE, 0), 111160.2002D, 0.0001D); /* the 2nd argument */
			resultAddr = newVaList.nextVarg(ADDRESS);
			Assert.assertEquals(resultAddr.get(JAVA_DOUBLE, 0), 111170.1001D, 0.0001D); /* the 3rd argument */
		}
	}

	@Test(expectedExceptions = NoSuchElementException.class, expectedExceptionsMessageRegExp = "No such element.*")
	public void test_NoMoreNextArg_IntStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 1122333);
			intHandle2.set(structSegmt1, 4455666);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 2244668);
			intHandle2.set(structSegmt2, 1133557);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);

			vaList.nextVarg(structLayout, allocator); /* the 1st struct argument */
			vaList.nextVarg(structLayout, allocator); /* the 2nd struct argument */

			/* An exception is thrown as there is no more argument in VaList */
			vaList.nextVarg(structLayout, allocator);
			fail("Failed to throw out NoSuchElementException when nextVarg() exceeds the memory region of VaList");
		}
	}

	@Test
	public void test_createIntStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 1122333);
			intHandle2.set(structSegmt1, 4455666);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 2244668);
			intHandle2.set(structSegmt2, 1133557);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);
			VaList newVaList = VaList.ofAddress(vaList.address(), session);

			MemorySegment newArgSegmt = newVaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(newArgSegmt), 1122333); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(intHandle2.get(newArgSegmt), 4455666); /* the 2nd element of the 1st struct argument */
			newArgSegmt = newVaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(newArgSegmt), 2244668); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(intHandle2.get(newArgSegmt), 1133557); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_createLongStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 6677889911L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 2233445566L);
			longHandle2.set(structSegmt2, 7788991122L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);
			VaList newVaList = VaList.ofAddress(vaList.address(), session);

			MemorySegment newArgSegmt = newVaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(newArgSegmt), 1122334455L); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(longHandle2.get(newArgSegmt), 6677889911L); /* the 2nd element of the 1st struct argument */
			newArgSegmt = newVaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(newArgSegmt), 2233445566L); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(longHandle2.get(newArgSegmt), 7788991122L); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_createDoubleStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11150.1001D);
			doubleHandle2.set(structSegmt1, 11160.2002D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 11170.1001D);
			doubleHandle2.set(structSegmt2, 11180.2002D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);
			VaList newVaList = VaList.ofAddress(vaList.address(), session);

			MemorySegment newArgSegmt = newVaList.nextVarg(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(newArgSegmt), 11150.1001D, 0.0001D); /* the 1st element of the 1st struct argument */
			Assert.assertEquals((double)doubleHandle2.get(newArgSegmt), 11160.2002D, 0.0001D); /* the 2nd element of the 1st struct argument */
			newArgSegmt = newVaList.nextVarg(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(newArgSegmt), 11170.1001D, 0.0001D); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals((double)doubleHandle2.get(newArgSegmt), 11180.2002D, 0.0001D); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test(expectedExceptions = NoSuchElementException.class, expectedExceptionsMessageRegExp = "No such element.*")
	public void test_NoMoreSkippedArg_IntArgOfVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_INT, 700)
					.addVarg(JAVA_INT, 800)
					.addVarg(JAVA_INT, 900)
					.addVarg(JAVA_INT, 1000), session);
			vaList.skip(JAVA_INT, JAVA_INT, JAVA_INT, JAVA_INT); /* Skip over all arguments in VaList */

			/* An exception is thrown as there is no more argument in VaList */
			vaList.skip(JAVA_INT);
			fail("Failed to throw out NoSuchElementException when skip() exceeds the memory region of VaList");
		}
	}

	@Test
	public void test_skipIntArgOfVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_INT, 700)
					.addVarg(JAVA_INT, 800)
					.addVarg(JAVA_INT, 900)
					.addVarg(JAVA_INT, 1000), session);
			vaList.skip(JAVA_INT);
			Assert.assertEquals(vaList.nextVarg(JAVA_INT), 800); /* the 2nd argument */
			vaList.skip(JAVA_INT);
			Assert.assertEquals(vaList.nextVarg(JAVA_INT), 1000); /* the 4th argument */
		}
	}

	@Test
	public void test_skipLongArgOfVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_LONG, 700000L)
					.addVarg(JAVA_LONG, 800000L)
					.addVarg(JAVA_LONG, 900000L)
					.addVarg(JAVA_LONG, 1000000L), session);
			vaList.skip(JAVA_LONG, JAVA_LONG);
			Assert.assertEquals(vaList.nextVarg(JAVA_LONG), 900000L); /* the 3rd argument */
		}
	}

	@Test
	public void test_skipDoubleArgOfVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(JAVA_DOUBLE, 111150.1001D)
					.addVarg(JAVA_DOUBLE, 111160.2002D)
					.addVarg(JAVA_DOUBLE, 111170.1001D)
					.addVarg(JAVA_DOUBLE, 111180.2002D), session);
			vaList.skip(JAVA_DOUBLE, JAVA_DOUBLE, JAVA_DOUBLE);
			Assert.assertEquals(vaList.nextVarg(JAVA_DOUBLE), 111180.2002D, 0.0001D); /* the 4th argument */
		}
	}

	@Test
	public void test_skipIntPtrArgOfVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment intSegmt1 = allocator.allocate(JAVA_INT, 700);
			MemorySegment intSegmt2 = allocator.allocate(JAVA_INT, 800);
			MemorySegment intSegmt3 = allocator.allocate(JAVA_INT, 900);
			MemorySegment intSegmt4 = allocator.allocate(JAVA_INT, 1000);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, intSegmt1.address())
					.addVarg(ADDRESS, intSegmt2.address())
					.addVarg(ADDRESS, intSegmt3.address())
					.addVarg(ADDRESS, intSegmt4.address()), session);
			vaList.skip(ADDRESS);
			Assert.assertEquals(vaList.nextVarg(ADDRESS), intSegmt2.address()); /* the 2nd argument */
		}
	}

	@Test
	public void test_skipLongPtrArgOfVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment longSegmt1 = allocator.allocate(JAVA_LONG, 700000L);
			MemorySegment longSegmt2 = allocator.allocate(JAVA_LONG, 800000L);
			MemorySegment longSegmt3 = allocator.allocate(JAVA_LONG, 900000L);
			MemorySegment longSegmt4 = allocator.allocate(JAVA_LONG, 1000000L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, longSegmt1.address())
					.addVarg(ADDRESS, longSegmt2.address())
					.addVarg(ADDRESS, longSegmt3.address())
					.addVarg(ADDRESS, longSegmt4.address()), session);
			vaList.skip(ADDRESS, ADDRESS);
			Assert.assertEquals(vaList.nextVarg(ADDRESS), longSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_skipDoublePtrArgOfVaList() throws Throwable {
		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment doubleSegmt1 = allocator.allocate(JAVA_DOUBLE, 111150.1001D);
			MemorySegment doubleSegmt2 = allocator.allocate(JAVA_DOUBLE, 111160.2002D);
			MemorySegment doubleSegmt3 = allocator.allocate(JAVA_DOUBLE, 111170.1001D);
			MemorySegment doubleSegmt4 = allocator.allocate(JAVA_DOUBLE, 111180.1002D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(ADDRESS, doubleSegmt1.address())
					.addVarg(ADDRESS, doubleSegmt2.address())
					.addVarg(ADDRESS, doubleSegmt3.address())
					.addVarg(ADDRESS, doubleSegmt4.address()), session);
			vaList.skip(ADDRESS, ADDRESS, ADDRESS);
			Assert.assertEquals(vaList.nextVarg(ADDRESS), doubleSegmt4.address()); /* the 4th argument */
		}
	}

	@Test(expectedExceptions = NoSuchElementException.class, expectedExceptionsMessageRegExp = "No such element.*")
	public void test_NoMoreSkippedArg_IntStructOfVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 1122333);
			intHandle2.set(structSegmt1, 4455666);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 2244668);
			intHandle2.set(structSegmt2, 1133557);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);
			vaList.skip(structLayout, structLayout); /* Skip over all arguments in VaList */

			/* An exception is thrown as there is no more argument in VaList */
			vaList.skip(structLayout);
			fail("Failed to throw out NoSuchElementException when skip() exceeds the memory region of VaList");
		}
	}

	@Test
	public void test_skipIntStructOfVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"), JAVA_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 1122333);
			intHandle2.set(structSegmt1, 4455666);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 2244668);
			intHandle2.set(structSegmt2, 1133557);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);
			vaList.skip(structLayout);

			MemorySegment argSegmt = vaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(argSegmt), 2244668); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(intHandle2.get(argSegmt), 1133557); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_skipLongStructOfVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 6677889911L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 2233445566L);
			longHandle2.set(structSegmt2, 7788991122L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);
			vaList.skip(structLayout);

			MemorySegment argSegmt = vaList.nextVarg(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(argSegmt), 2233445566L); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(longHandle2.get(argSegmt), 7788991122L); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_skipDoubleStructOfVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		try (MemorySession session = MemorySession.openConfined()) {
			SegmentAllocator allocator = SegmentAllocator.newNativeArena(session);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11150.1001D);
			doubleHandle2.set(structSegmt1, 11160.2002D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 11170.1001D);
			doubleHandle2.set(structSegmt2, 11180.2002D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.addVarg(structLayout, structSegmt1)
					.addVarg(structLayout, structSegmt2), session);
			vaList.skip(structLayout);

			MemorySegment argSegmt = vaList.nextVarg(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(argSegmt), 11170.1001D, 0.0001D); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals((double)doubleHandle2.get(argSegmt), 11180.2002D, 0.0001D); /* the 2nd element of the 2nd struct argument */
		}
	}
}
