/*
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
 */
package org.openj9.test.jep389.valist;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.lang.invoke.VarHandle;
import java.nio.ByteOrder;

import static jdk.incubator.foreign.CLinker.C_DOUBLE;
import static jdk.incubator.foreign.CLinker.C_INT;
import static jdk.incubator.foreign.CLinker.C_LONG;
import static jdk.incubator.foreign.CLinker.C_LONG_LONG;
import static jdk.incubator.foreign.CLinker.C_POINTER;
import jdk.incubator.foreign.CLinker.VaList;
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryHandles;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemoryLayout.PathElement;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.ValueLayout;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) for the vararg list in VaList API specific cases.
 */
@Test(groups = { "level.sanity" })
public class ApiTests {
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static String arch = System.getProperty("os.arch").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinX64 = osName.contains("win") && (arch.equals("amd64") || arch.equals("x86_64"));
	private static boolean isMacOsAarch64 = osName.contains("mac") && arch.contains("aarch64");
	private static boolean isSysVPPC64le = osName.contains("linux") && arch.contains("ppc64");
	/* long long is 64 bits on AIX/ppc64, which is the same as Windows */
	private static ValueLayout longLayout = (isWinX64 || isAixOS) ? C_LONG_LONG : C_LONG;

	@Test
	public void test_emptyVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
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
	public void test_vaListScope() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromInt(C_INT, 100), scope);
			ResourceScope vaListScope = vaList.scope();
			Assert.assertEquals(vaListScope, scope);
		}
	}

	@Test
	public void test_checkIntVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromInt(C_INT, 700)
					.vargFromInt(C_INT, 800)
					.vargFromInt(C_INT, 900), scope);

			Assert.assertEquals(vaList.vargAsInt(C_INT), 700); /* the 1st argument */
			Assert.assertEquals(vaList.vargAsInt(C_INT), 800); /* the 2nd argument */
			Assert.assertEquals(vaList.vargAsInt(C_INT), 900); /* the 3rd argument */
		}
	}

	@Test
	public void test_checkLongVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromLong(longLayout, 700000L)
					.vargFromLong(longLayout, 800000L)
					.vargFromLong(longLayout, 900000L), scope);

			Assert.assertEquals(vaList.vargAsLong(longLayout), 700000L); /* the 1st argument */
			Assert.assertEquals(vaList.vargAsLong(longLayout), 800000L); /* the 2nd argument */
			Assert.assertEquals(vaList.vargAsLong(longLayout), 900000L); /* the 3rd argument */
		}
	}

	@Test
	public void test_checkDoubleVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromDouble(C_DOUBLE, 111150.1001D)
					.vargFromDouble(C_DOUBLE, 111160.2002D)
					.vargFromDouble(C_DOUBLE, 111170.1001D), scope);

			Assert.assertEquals(vaList.vargAsDouble(C_DOUBLE), 111150.1001D, 0.0001D); /* the 1st argument */
			Assert.assertEquals(vaList.vargAsDouble(C_DOUBLE), 111160.2002D, 0.0001D); /* the 2nd argument */
			Assert.assertEquals(vaList.vargAsDouble(C_DOUBLE), 111170.1001D, 0.0001D); /* the 3rd argument */
		}
	}

	@Test
	public void test_checkIntPtrVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment intSegmt1 = allocator.allocate(C_INT, 700);
			MemorySegment intSegmt2 = allocator.allocate(C_INT, 800);
			MemorySegment intSegmt3 = allocator.allocate(C_INT, 900);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromAddress(C_POINTER, intSegmt1.address())
					.vargFromAddress(C_POINTER, intSegmt2.address())
					.vargFromAddress(C_POINTER, intSegmt3.address()), scope);

			Assert.assertEquals(vaList.vargAsAddress(C_POINTER), intSegmt1.address()); /* the 1st argument */
			Assert.assertEquals(vaList.vargAsAddress(C_POINTER), intSegmt2.address()); /* the 2nd argument */
			Assert.assertEquals(vaList.vargAsAddress(C_POINTER), intSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_checkLongPtrVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment longSegmt1 = allocator.allocate(longLayout, 700000L);
			MemorySegment longSegmt2 = allocator.allocate(longLayout, 800000L);
			MemorySegment longSegmt3 = allocator.allocate(longLayout, 900000L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromAddress(C_POINTER, longSegmt1.address())
					.vargFromAddress(C_POINTER, longSegmt2.address())
					.vargFromAddress(C_POINTER, longSegmt3.address()), scope);

			Assert.assertEquals(vaList.vargAsAddress(C_POINTER), longSegmt1.address()); /* the 1st argument */
			Assert.assertEquals(vaList.vargAsAddress(C_POINTER), longSegmt2.address()); /* the 2nd argument */
			Assert.assertEquals(vaList.vargAsAddress(C_POINTER), longSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_checkDoublePtrVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment doubleSegmt1 = allocator.allocate(C_DOUBLE, 111150.1001D);
			MemorySegment doubleSegmt2 = allocator.allocate(C_DOUBLE, 111160.2002D);
			MemorySegment doubleSegmt3 = allocator.allocate(C_DOUBLE, 111170.1001D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromAddress(C_POINTER, doubleSegmt1.address())
					.vargFromAddress(C_POINTER, doubleSegmt2.address())
					.vargFromAddress(C_POINTER, doubleSegmt3.address()), scope);

			Assert.assertEquals(vaList.vargAsAddress(C_POINTER), doubleSegmt1.address()); /* the 1st argument */
			Assert.assertEquals(vaList.vargAsAddress(C_POINTER), doubleSegmt2.address()); /* the 2nd argument */
			Assert.assertEquals(vaList.vargAsAddress(C_POINTER), doubleSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_checkIntStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 1122333);
			intHandle2.set(structSegmt1, 4455666);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 2244668);
			intHandle2.set(structSegmt2, 1133557);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromSegment(structLayout, structSegmt1)
					.vargFromSegment(structLayout, structSegmt2), scope);

			MemorySegment argSegmt = vaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(argSegmt), 1122333); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(intHandle2.get(argSegmt), 4455666); /* the 2nd element of the 1st struct argument */
			argSegmt = vaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(argSegmt), 2244668); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(intHandle2.get(argSegmt), 1133557); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_checkLongStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 6677889911L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 2233445566L);
			longHandle2.set(structSegmt2, 7788991122L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromSegment(structLayout, structSegmt1)
					.vargFromSegment(structLayout, structSegmt2), scope);

			MemorySegment argSegmt = vaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(argSegmt), 1122334455L); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(longHandle2.get(argSegmt), 6677889911L); /* the 2nd element of the 1st struct argument */
			argSegmt = vaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(argSegmt), 2233445566L); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(longHandle2.get(argSegmt), 7788991122L); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_checkDoubleStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 111150.1001D);
			doubleHandle2.set(structSegmt1, 111160.2002D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 111170.1001D);
			doubleHandle2.set(structSegmt2, 111180.2002D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromSegment(structLayout, structSegmt1)
					.vargFromSegment(structLayout, structSegmt2), scope);

			MemorySegment argSegmt = vaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(argSegmt), 111150.1001D, 0.0001D); /* the 1st element of the 1st struct argument */
			Assert.assertEquals((double)doubleHandle2.get(argSegmt), 111160.2002D, 0.0001D); /* the 2nd element of the 1st struct argument */
			argSegmt = vaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(argSegmt), 111170.1001D, 0.0001D); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals((double)doubleHandle2.get(argSegmt), 111180.2002D, 0.0001D); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_copyIntVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromInt(C_INT, 700)
					.vargFromInt(C_INT, 800)
					.vargFromInt(C_INT, 900), scope);
			VaList resultVaList = vaList.copy();

			Assert.assertEquals(resultVaList.vargAsInt(C_INT), 700); /* the 1st argument */
			Assert.assertEquals(resultVaList.vargAsInt(C_INT), 800); /* the 2nd argument */
			Assert.assertEquals(resultVaList.vargAsInt(C_INT), 900); /* the 3rd argument */
		}
	}

	@Test
	public void test_copyLongVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromLong(longLayout, 700000L)
					.vargFromLong(longLayout, 800000L)
					.vargFromLong(longLayout, 900000L), scope);
			VaList resultVaList = vaList.copy();

			Assert.assertEquals(resultVaList.vargAsLong(longLayout), 700000L); /* the 1st argument */
			Assert.assertEquals(resultVaList.vargAsLong(longLayout), 800000L); /* the 2nd argument */
			Assert.assertEquals(resultVaList.vargAsLong(longLayout), 900000L); /* the 3rd argument */
		}
	}

	@Test
	public void test_copyDoubleVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromDouble(C_DOUBLE, 111150.1001D)
					.vargFromDouble(C_DOUBLE, 111160.2002D)
					.vargFromDouble(C_DOUBLE, 111170.1001D), scope);
			VaList resultVaList = vaList.copy();

			Assert.assertEquals(resultVaList.vargAsDouble(C_DOUBLE), 111150.1001D, 0001D); /* the 1st argument */
			Assert.assertEquals(resultVaList.vargAsDouble(C_DOUBLE), 111160.2002D, 0001D); /* the 2nd argument */
			Assert.assertEquals(resultVaList.vargAsDouble(C_DOUBLE), 111170.1001D, 0001D); /* the 3rd argument */
		}
	}

	@Test
	public void test_copyIntPtrVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment intSegmt1 = allocator.allocate(C_INT, 700);
			MemorySegment intSegmt2 = allocator.allocate(C_INT, 800);
			MemorySegment intSegmt3 = allocator.allocate(C_INT, 900);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromAddress(C_POINTER, intSegmt1.address())
					.vargFromAddress(C_POINTER, intSegmt2.address())
					.vargFromAddress(C_POINTER, intSegmt3.address()), scope);
			VaList resultVaList = vaList.copy();

			Assert.assertEquals(resultVaList.vargAsAddress(C_POINTER), intSegmt1.address()); /* the 1st argument */
			Assert.assertEquals(resultVaList.vargAsAddress(C_POINTER), intSegmt2.address()); /* the 2nd argument */
			Assert.assertEquals(resultVaList.vargAsAddress(C_POINTER), intSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_copyLongPtrVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment longSegmt1 = allocator.allocate(longLayout, 700000L);
			MemorySegment longSegmt2 = allocator.allocate(longLayout, 800000L);
			MemorySegment longSegmt3 = allocator.allocate(longLayout, 900000L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromAddress(C_POINTER, longSegmt1.address())
					.vargFromAddress(C_POINTER, longSegmt2.address())
					.vargFromAddress(C_POINTER, longSegmt3.address()), scope);
			VaList resultVaList = vaList.copy();

			Assert.assertEquals(resultVaList.vargAsAddress(C_POINTER), longSegmt1.address()); /* the 1st argument */
			Assert.assertEquals(resultVaList.vargAsAddress(C_POINTER), longSegmt2.address()); /* the 2nd argument */
			Assert.assertEquals(resultVaList.vargAsAddress(C_POINTER), longSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_copyDoublePtrVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment doubleSegmt1 = allocator.allocate(C_DOUBLE, 111150.1001D);
			MemorySegment doubleSegmt2 = allocator.allocate(C_DOUBLE, 111160.2002D);
			MemorySegment doubleSegmt3 = allocator.allocate(C_DOUBLE, 111170.1001D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromAddress(C_POINTER, doubleSegmt1.address())
					.vargFromAddress(C_POINTER, doubleSegmt2.address())
					.vargFromAddress(C_POINTER, doubleSegmt3.address()), scope);
			VaList resultVaList = vaList.copy();

			Assert.assertEquals(resultVaList.vargAsAddress(C_POINTER), doubleSegmt1.address()); /* the 1st argument */
			Assert.assertEquals(resultVaList.vargAsAddress(C_POINTER), doubleSegmt2.address()); /* the 2nd argument */
			Assert.assertEquals(resultVaList.vargAsAddress(C_POINTER), doubleSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_copyIntStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 1122333);
			intHandle2.set(structSegmt1, 4455666);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 2244668);
			intHandle2.set(structSegmt2, 1133557);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromSegment(structLayout, structSegmt1)
					.vargFromSegment(structLayout, structSegmt2), scope);
			VaList resultVaList = vaList.copy();

			MemorySegment resultArgSegmt = resultVaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(resultArgSegmt), 1122333); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(intHandle2.get(resultArgSegmt), 4455666); /* the 2nd element of the 1st struct argument */
			resultArgSegmt = resultVaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(resultArgSegmt), 2244668); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(intHandle2.get(resultArgSegmt), 1133557); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_copyLongStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 6677889911L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 2233445566L);
			longHandle2.set(structSegmt2, 7788991122L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromSegment(structLayout, structSegmt1)
					.vargFromSegment(structLayout, structSegmt2), scope);
			VaList resultVaList = vaList.copy();

			MemorySegment resultArgSegmt = resultVaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(resultArgSegmt), 1122334455L); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(longHandle2.get(resultArgSegmt), 6677889911L); /* the 2nd element of the 1st struct argument */
			resultArgSegmt = resultVaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(resultArgSegmt), 2233445566L); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(longHandle2.get(resultArgSegmt), 7788991122L); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_copyDoubleStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 111150.1001D);
			doubleHandle2.set(structSegmt1, 111160.2002D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 111170.1001D);
			doubleHandle2.set(structSegmt2, 111180.2002D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromSegment(structLayout, structSegmt1)
					.vargFromSegment(structLayout, structSegmt2), scope);
			VaList resultVaList = vaList.copy();

			MemorySegment resultArgSegmt = resultVaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(resultArgSegmt), 111150.1001D, 0.0001D); /* the 1st element of the 1st struct argument */
			Assert.assertEquals((double)doubleHandle2.get(resultArgSegmt), 111160.2002D, 0.0001D); /* the 2nd element of the 1st struct argument */
			resultArgSegmt = resultVaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(resultArgSegmt), 111170.1001D, 0.0001D); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals((double)doubleHandle2.get(resultArgSegmt), 111180.2002D, 0.0001D); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_createIntVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromInt(C_INT, 700)
					.vargFromInt(C_INT, 800)
					.vargFromInt(C_INT, 900), scope);
			VaList newVaList = VaList.ofAddress(vaList.address(), scope);

			Assert.assertEquals(newVaList.vargAsInt(C_INT), 700); /* the 1st argument */
			Assert.assertEquals(newVaList.vargAsInt(C_INT), 800); /* the 2nd argument */
			Assert.assertEquals(newVaList.vargAsInt(C_INT), 900); /* the 3rd argument */
		}
	}

	@Test
	public void test_createLongVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromLong(longLayout, 700000L)
					.vargFromLong(longLayout, 800000L)
					.vargFromLong(longLayout, 900000L), scope);
			VaList newVaList = VaList.ofAddress(vaList.address(), scope);

			Assert.assertEquals(newVaList.vargAsLong(longLayout), 700000L); /* the 1st argument */
			Assert.assertEquals(newVaList.vargAsLong(longLayout), 800000L); /* the 2nd argument */
			Assert.assertEquals(newVaList.vargAsLong(longLayout), 900000L); /* the 3rd argument */
		}
	}

	@Test
	public void test_createDoubleVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromDouble(C_DOUBLE, 111150.1001D)
					.vargFromDouble(C_DOUBLE, 111160.2002D)
					.vargFromDouble(C_DOUBLE, 111170.1001D), scope);
			VaList newVaList = VaList.ofAddress(vaList.address(), scope);

			Assert.assertEquals(newVaList.vargAsDouble(C_DOUBLE), 111150.1001D, 0.0001D); /* the 1st argument */
			Assert.assertEquals(newVaList.vargAsDouble(C_DOUBLE), 111160.2002D, 0.0001D); /* the 2nd argument */
			Assert.assertEquals(newVaList.vargAsDouble(C_DOUBLE), 111170.1001D, 0.0001D); /* the 3rd argument */
		}
	}

	@Test
	public void test_createIntPtrVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment intSegmt1 = allocator.allocate(C_INT, 700);
			MemorySegment intSegmt2 = allocator.allocate(C_INT, 800);
			MemorySegment intSegmt3 = allocator.allocate(C_INT, 900);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromAddress(C_POINTER, intSegmt1.address())
					.vargFromAddress(C_POINTER, intSegmt2.address())
					.vargFromAddress(C_POINTER, intSegmt3.address()), scope);
			VaList newVaList = VaList.ofAddress(vaList.address(), scope);

			VarHandle resultHandle = MemoryHandles.varHandle(int.class, ByteOrder.nativeOrder());
			MemorySegment resultSegmt = newVaList.vargAsAddress(C_POINTER).asSegment(C_INT.byteSize(), scope);
			Assert.assertEquals((int)resultHandle.get(resultSegmt, 0), 700); /* the 1st argument */
			resultSegmt = newVaList.vargAsAddress(C_POINTER).asSegment(C_INT.byteSize(), scope);
			Assert.assertEquals((int)resultHandle.get(resultSegmt, 0), 800); /* the 2nd argument */
			resultSegmt = newVaList.vargAsAddress(C_POINTER).asSegment(C_INT.byteSize(), scope);
			Assert.assertEquals((int)resultHandle.get(resultSegmt, 0), 900); /* the 3rd argument */
		}
	}

	@Test
	public void test_createLongPtrVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment longSegmt1 = allocator.allocate(longLayout, 700000L);
			MemorySegment longSegmt2 = allocator.allocate(longLayout, 800000L);
			MemorySegment longSegmt3 = allocator.allocate(longLayout, 900000L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromAddress(C_POINTER, longSegmt1.address())
					.vargFromAddress(C_POINTER, longSegmt2.address())
					.vargFromAddress(C_POINTER, longSegmt3.address()), scope);
			VaList newVaList = VaList.ofAddress(vaList.address(), scope);

			VarHandle resultHandle = MemoryHandles.varHandle(long.class, ByteOrder.nativeOrder());
			MemorySegment resultSegmt = newVaList.vargAsAddress(C_POINTER).asSegment(longLayout.byteSize(), scope);
			Assert.assertEquals((long)resultHandle.get(resultSegmt, 0), 700000L); /* the 1st argument */
			resultSegmt = newVaList.vargAsAddress(C_POINTER).asSegment(longLayout.byteSize(), scope);
			Assert.assertEquals((long)resultHandle.get(resultSegmt, 0), 800000L); /* the 2nd argument */
			resultSegmt = newVaList.vargAsAddress(C_POINTER).asSegment(longLayout.byteSize(), scope);
			Assert.assertEquals((long)resultHandle.get(resultSegmt, 0), 900000L); /* the 3rd argument */
		}
	}

	@Test
	public void test_createDoublePtrVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment doubleSegmt1 = allocator.allocate(C_DOUBLE, 111150.1001D);
			MemorySegment doubleSegmt2 = allocator.allocate(C_DOUBLE, 111160.2002D);
			MemorySegment doubleSegmt3 = allocator.allocate(C_DOUBLE, 111170.1001D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromAddress(C_POINTER, doubleSegmt1.address())
					.vargFromAddress(C_POINTER, doubleSegmt2.address())
					.vargFromAddress(C_POINTER, doubleSegmt3.address()), scope);
			VaList newVaList = VaList.ofAddress(vaList.address(), scope);

			VarHandle resultHandle = MemoryHandles.varHandle(double.class, ByteOrder.nativeOrder());
			MemorySegment resultSegmt = newVaList.vargAsAddress(C_POINTER).asSegment(C_DOUBLE.byteSize(), scope);
			Assert.assertEquals((double)resultHandle.get(resultSegmt, 0), 111150.1001D, 0.0001D); /* the 1st argument */
			resultSegmt = newVaList.vargAsAddress(C_POINTER).asSegment(C_DOUBLE.byteSize(), scope);
			Assert.assertEquals((double)resultHandle.get(resultSegmt, 0), 111160.2002D, 0.0001D); /* the 2nd argument */
			resultSegmt = newVaList.vargAsAddress(C_POINTER).asSegment(C_DOUBLE.byteSize(), scope);
			Assert.assertEquals((double)resultHandle.get(resultSegmt, 0), 111170.1001D, 0.0001D); /* the 3rd argument */
		}
	}

	@Test
	public void test_createIntStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 1122333);
			intHandle2.set(structSegmt1, 4455666);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 2244668);
			intHandle2.set(structSegmt2, 1133557);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromSegment(structLayout, structSegmt1)
					.vargFromSegment(structLayout, structSegmt2), scope);
			VaList newVaList = VaList.ofAddress(vaList.address(), scope);

			MemorySegment newArgSegmt = newVaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(newArgSegmt), 1122333); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(intHandle2.get(newArgSegmt), 4455666); /* the 2nd element of the 1st struct argument */
			newArgSegmt = newVaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(newArgSegmt), 2244668); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(intHandle2.get(newArgSegmt), 1133557); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_createLongStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 6677889911L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 2233445566L);
			longHandle2.set(structSegmt2, 7788991122L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromSegment(structLayout, structSegmt1)
					.vargFromSegment(structLayout, structSegmt2), scope);
			VaList newVaList = VaList.ofAddress(vaList.address(), scope);

			MemorySegment newArgSegmt = newVaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(newArgSegmt), 1122334455L); /* the 1st element of the 1st struct argument */
			Assert.assertEquals(longHandle2.get(newArgSegmt), 6677889911L); /* the 2nd element of the 1st struct argument */
			newArgSegmt = newVaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(newArgSegmt), 2233445566L); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(longHandle2.get(newArgSegmt), 7788991122L); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_createDoubleStructVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11150.1001D);
			doubleHandle2.set(structSegmt1, 11160.2002D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 11170.1001D);
			doubleHandle2.set(structSegmt2, 11180.2002D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromSegment(structLayout, structSegmt1)
					.vargFromSegment(structLayout, structSegmt2), scope);
			VaList newVaList = VaList.ofAddress(vaList.address(), scope);

			MemorySegment newArgSegmt = newVaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(newArgSegmt), 11150.1001D, 0.0001D); /* the 1st element of the 1st struct argument */
			Assert.assertEquals((double)doubleHandle2.get(newArgSegmt), 11160.2002D, 0.0001D); /* the 2nd element of the 1st struct argument */
			newArgSegmt = newVaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(newArgSegmt), 11170.1001D, 0.0001D); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals((double)doubleHandle2.get(newArgSegmt), 11180.2002D, 0.0001D); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_skipIntArgOfVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromInt(C_INT, 700)
					.vargFromInt(C_INT, 800)
					.vargFromInt(C_INT, 900)
					.vargFromInt(C_INT, 1000), scope);
			vaList.skip(C_INT);
			Assert.assertEquals(vaList.vargAsInt(C_INT), 800); /* the 2nd argument */
		}
	}

	@Test
	public void test_skipLongArgOfVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromLong(longLayout, 700000L)
					.vargFromLong(longLayout, 800000L)
					.vargFromLong(longLayout, 900000L)
					.vargFromLong(longLayout, 1000000L), scope);
			vaList.skip(longLayout, longLayout);
			Assert.assertEquals(vaList.vargAsLong(longLayout), 900000L); /* the 3rd argument */
		}
	}

	@Test
	public void test_skipDoubleArgOfVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromDouble(C_DOUBLE, 111150.1001D)
					.vargFromDouble(C_DOUBLE, 111160.2002D)
					.vargFromDouble(C_DOUBLE, 111170.1001D)
					.vargFromDouble(C_DOUBLE, 111180.2002D), scope);
			vaList.skip(C_DOUBLE, C_DOUBLE, C_DOUBLE);
			Assert.assertEquals(vaList.vargAsDouble(C_DOUBLE), 111180.2002D, 0.0001D); /* the 4th argument */
		}
	}

	@Test
	public void test_skipIntPtrArgOfVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment intSegmt1 = allocator.allocate(C_INT, 700);
			MemorySegment intSegmt2 = allocator.allocate(C_INT, 800);
			MemorySegment intSegmt3 = allocator.allocate(C_INT, 900);
			MemorySegment intSegmt4 = allocator.allocate(C_INT, 1000);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromAddress(C_POINTER, intSegmt1.address())
					.vargFromAddress(C_POINTER, intSegmt2.address())
					.vargFromAddress(C_POINTER, intSegmt3.address())
					.vargFromAddress(C_POINTER, intSegmt4.address()), scope);
			vaList.skip(C_POINTER);
			Assert.assertEquals(vaList.vargAsAddress(C_POINTER), intSegmt2.address()); /* the 2nd argument */
		}
	}

	@Test
	public void test_skipLongPtrArgOfVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment longSegmt1 = allocator.allocate(longLayout, 700000L);
			MemorySegment longSegmt2 = allocator.allocate(longLayout, 800000L);
			MemorySegment longSegmt3 = allocator.allocate(longLayout, 900000L);
			MemorySegment longSegmt4 = allocator.allocate(longLayout, 1000000L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromAddress(C_POINTER, longSegmt1.address())
					.vargFromAddress(C_POINTER, longSegmt2.address())
					.vargFromAddress(C_POINTER, longSegmt3.address())
					.vargFromAddress(C_POINTER, longSegmt4.address()), scope);
			vaList.skip(C_POINTER, C_POINTER);
			Assert.assertEquals(vaList.vargAsAddress(C_POINTER), longSegmt3.address()); /* the 3rd argument */
		}
	}

	@Test
	public void test_skipDoublePtrArgOfVaList() throws Throwable {
		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment doubleSegmt1 = allocator.allocate(C_DOUBLE, 111150.1001D);
			MemorySegment doubleSegmt2 = allocator.allocate(C_DOUBLE, 111160.2002D);
			MemorySegment doubleSegmt3 = allocator.allocate(C_DOUBLE, 111170.1001D);
			MemorySegment doubleSegmt4 = allocator.allocate(C_DOUBLE, 111180.1002D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromAddress(C_POINTER, doubleSegmt1.address())
					.vargFromAddress(C_POINTER, doubleSegmt2.address())
					.vargFromAddress(C_POINTER, doubleSegmt3.address())
					.vargFromAddress(C_POINTER, doubleSegmt4.address()), scope);
			vaList.skip(C_POINTER, C_POINTER, C_POINTER);
			Assert.assertEquals(vaList.vargAsAddress(C_POINTER), doubleSegmt4.address()); /* the 4th argument */
		}
	}

	@Test
	public void test_skipIntStructOfVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 1122333);
			intHandle2.set(structSegmt1, 4455666);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt2, 2244668);
			intHandle2.set(structSegmt2, 1133557);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromSegment(structLayout, structSegmt1)
					.vargFromSegment(structLayout, structSegmt2), scope);
			vaList.skip(structLayout);

			MemorySegment argSegmt = vaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(intHandle1.get(argSegmt), 2244668); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(intHandle2.get(argSegmt), 1133557); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_skipLongStructOfVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt1, 1122334455L);
			longHandle2.set(structSegmt1, 6677889911L);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			longHandle1.set(structSegmt2, 2233445566L);
			longHandle2.set(structSegmt2, 7788991122L);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromSegment(structLayout, structSegmt1)
					.vargFromSegment(structLayout, structSegmt2), scope);
			vaList.skip(structLayout);

			MemorySegment argSegmt = vaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals(longHandle1.get(argSegmt), 2233445566L); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals(longHandle2.get(argSegmt), 7788991122L); /* the 2nd element of the 2nd struct argument */
		}
	}

	@Test
	public void test_skipDoubleStructOfVaList() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt1, 11150.1001D);
			doubleHandle2.set(structSegmt1, 11160.2002D);
			MemorySegment structSegmt2 = allocator.allocate(structLayout);
			doubleHandle1.set(structSegmt2, 11170.1001D);
			doubleHandle2.set(structSegmt2, 11180.2002D);

			VaList vaList = VaList.make(vaListBuilder -> vaListBuilder.vargFromSegment(structLayout, structSegmt1)
					.vargFromSegment(structLayout, structSegmt2), scope);
			vaList.skip(structLayout);

			MemorySegment argSegmt = vaList.vargAsSegment(structLayout, allocator);
			Assert.assertEquals((double)doubleHandle1.get(argSegmt), 11170.1001D, 0.0001D); /* the 1st element of the 2nd struct argument */
			Assert.assertEquals((double)doubleHandle2.get(argSegmt), 11180.2002D, 0.0001D); /* the 2nd element of the 2nd struct argument */
		}
	}
}
