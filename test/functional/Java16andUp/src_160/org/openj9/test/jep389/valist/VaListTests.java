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
package org.openj9.test.jep389.valist;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import jdk.incubator.foreign.CLinker;
import static jdk.incubator.foreign.CLinker.*;
import static jdk.incubator.foreign.CLinker.VaList.Builder;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.ValueLayout;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemoryAccess;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.LibraryLookup;
import static jdk.incubator.foreign.LibraryLookup.Symbol;
import jdk.incubator.foreign.NativeScope;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) DownCall for the vararg list.
 */
@Test(groups = { "level.sanity" })
public class VaListTests {
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinOS = osName.contains("win");
	/* long long is 64 bits on AIX/ppc64, which is the same as Windows */
	private static ValueLayout longLayout = (isWinOS || isAixOS) ? C_LONG_LONG : C_LONG;

	private static LibraryLookup nativeLib = LibraryLookup.ofLibrary("clinkerffitests");
	private static LibraryLookup defaultLib = LibraryLookup.ofDefault();
	private static CLinker clinker = CLinker.getInstance();

	@Test
	public void test_addIntsWithVaList() throws Throwable {
		Symbol functionSymbol = nativeLib.lookup("addIntsFromVaList").get();
		MethodType mt = MethodType.methodType(int.class, int.class, VaList.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_VA_LIST);
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (NativeScope scope = NativeScope.unboundedScope()) {
			VaList vaList = CLinker.VaList.make(vaListBuilder -> vaListBuilder.vargFromInt(C_INT, 700)
					.vargFromInt(C_INT, 800)
					.vargFromInt(C_INT, 900)
					.vargFromInt(C_INT, 1000), scope);
			int result = (int)mh.invoke(4, vaList);
			Assert.assertEquals(result, 3400);
		}
	}

	@Test
	public void test_addLongsWithVaList() throws Throwable {
		Symbol functionSymbol = nativeLib.lookup("addLongsFromVaList").get();
		MethodType mt = MethodType.methodType(long.class, int.class, VaList.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_INT, C_VA_LIST);
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (NativeScope scope = NativeScope.unboundedScope()) {
			VaList vaList = CLinker.VaList.make(vaListBuilder -> vaListBuilder.vargFromLong(longLayout, 700000L)
					.vargFromLong(longLayout, 800000L)
					.vargFromLong(longLayout, 900000L)
					.vargFromLong(longLayout, 1000000L), scope);
			long result = (long)mh.invoke(4, vaList);
			Assert.assertEquals(result, 3400000L);
		}
	}

	@Test
	public void test_addDoublesWithVaList() throws Throwable {
		Symbol functionSymbol = nativeLib.lookup("addDoublesFromVaList").get();
		MethodType mt = MethodType.methodType(double.class, int.class, VaList.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_INT, C_VA_LIST);
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (NativeScope scope = NativeScope.unboundedScope()) {
			VaList vaList = CLinker.VaList.make(vaListBuilder -> vaListBuilder.vargFromDouble(C_DOUBLE, 150.1001D)
					.vargFromDouble(C_DOUBLE, 160.2002D)
					.vargFromDouble(C_DOUBLE, 170.1001D)
					.vargFromDouble(C_DOUBLE, 180.2002D), scope);
			double result = (double)mh.invoke(4, vaList);
			Assert.assertEquals(result, 660.6006D);
		}
	}

	@Test
	public void test_vprintfFromDefaultLibWithVaList() throws Throwable {
		/* 1) Disable the test on Windows given a misaligned access exception coming from
		 * java.base/java.lang.invoke.MemoryAccessVarHandleBase triggered by CLinker.toCString()
		 * is also captured on OpenJDK/Hotspot.
		 * 2) Disable the test on AIX as the default library loading is not yet implemented in OpenJDK.
		 */
		if (!isWinOS && !isAixOS) {
			Symbol functionSymbol = defaultLib.lookup("vprintf").get();
			MethodType mt = MethodType.methodType(int.class, MemoryAddress.class, VaList.class);
			FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_POINTER, C_VA_LIST);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

			try (NativeScope scope = NativeScope.unboundedScope()) {
				MemorySegment formatMemSegment = CLinker.toCString("%d * %d = %d\n", scope);
				VaList vaList = CLinker.VaList.make(vaListBuilder -> vaListBuilder.vargFromInt(C_INT, 7)
						.vargFromInt(C_INT, 8)
						.vargFromInt(C_INT, 56), scope);
				mh.invoke(formatMemSegment.address(), vaList);
			}
		}
	}

	@Test
	public void test_vprintfFromDefaultLibWithVaList_fromMemAddr() throws Throwable {
		/* 1) Disable the test on Windows given a misaligned access exception coming from
		 * java.base/java.lang.invoke.MemoryAccessVarHandleBase triggered by CLinker.toCString()
		 * is also captured on OpenJDK/Hotspot.
		 * 2) Disable the test on AIX as the default library loading is not yet implemented in OpenJDK.
		 */
		if (!isWinOS && !isAixOS) {
			Symbol functionSymbol = defaultLib.lookup("vprintf").get();
			MemoryAddress memAddr = functionSymbol.address();
			MethodType mt = MethodType.methodType(int.class, MemoryAddress.class, VaList.class);
			FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_POINTER, C_VA_LIST);
			MethodHandle mh = clinker.downcallHandle(memAddr, mt, fd);

			try (NativeScope scope = NativeScope.unboundedScope()) {
				MemorySegment formatMemSegment = CLinker.toCString("%d * %d = %d\n", scope);
				VaList vaList = CLinker.VaList.make(vaListBuilder -> vaListBuilder.vargFromInt(C_INT, 7)
						.vargFromInt(C_INT, 8)
						.vargFromInt(C_INT, 56), scope);
				mh.invoke(formatMemSegment.address(), vaList);
			}
		}
	}
}
