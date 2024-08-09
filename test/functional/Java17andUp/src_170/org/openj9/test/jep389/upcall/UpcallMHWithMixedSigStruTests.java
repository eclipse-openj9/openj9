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
import java.lang.invoke.VarHandle;

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
import jdk.incubator.foreign.GroupLayout;
import jdk.incubator.foreign.MemoryAccess;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemoryLayout.PathElement;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.SequenceLayout;
import jdk.incubator.foreign.SymbolLookup;
import jdk.incubator.foreign.ValueLayout;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) for the mixed native signatures
 * in argument/return struct in upcall.
 *
 * Note: the padding elements in the struct are only required by RI or VarHandle (accessing the
 * data address) while they are totally ignored in OpenJ9 given the padding/alignment are
 * computed by libffi automatically in native.
 */
@Test(groups = { "level.sanity" })
public class UpcallMHWithMixedSigStruTests {
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
	public void test_addIntAndIntShortFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_SHORT.withName("elem2"), MemoryLayout.paddingLayout(16));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntShortFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntShortFromStruct,
					FunctionDescriptor.of(C_INT, C_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 11223344);
			elemHandle2.set(structSegmt, (short)32766);

			int result = (int)mh.invokeExact(22334455, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 33590565);
		}
	}

	@Test
	public void test_addIntAndShortIntFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"),
				MemoryLayout.paddingLayout(16), C_INT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndShortIntFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndShortIntFromStruct,
					FunctionDescriptor.of(C_INT, C_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, (short)32766);
			elemHandle2.set(structSegmt, 22446688);

			int result = (int)mh.invokeExact(11335577, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 33815031);
		}
	}

	@Test
	public void test_addIntAndIntLongFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				MemoryLayout.paddingLayout(32), longLayout.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(long.class, int.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_INT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntLongFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndIntLongFromStruct,
					FunctionDescriptor.of(longLayout, C_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 11223344);
			elemHandle2.set(structSegmt, 667788990011L);

			long result = (long)mh.invokeExact(22446688, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 667822660043L);
		}
	}

	@Test
	public void test_addIntAndLongIntFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"),
				C_INT.withName("elem2"), MemoryLayout.paddingLayout(32));
		VarHandle elemHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(long.class, int.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, C_INT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndLongIntFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addIntAndLongIntFromStruct,
					FunctionDescriptor.of(longLayout, C_INT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 667788990011L);
			elemHandle2.set(structSegmt, 11223344);

			long result = (long)mh.invokeExact(1234567, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 667801447922L);
		}
	}

	@Test
	public void test_addDoubleAndIntDoubleFromStructByUpcallMH() throws Throwable {
		/* The size of [int, double] on AIX/PPC 64-bit is 12 bytes without padding by default
		 * while the same struct is 16 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_DOUBLE.withName("elem2")) : MemoryLayout.structLayout(C_INT.withName("elem1"),
						MemoryLayout.paddingLayout(32), C_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndIntDoubleFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndIntDoubleFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 111111111);
			elemHandle2.set(structSegmt, 619.777D);

			double result = (double)mh.invokeExact(113.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111111844.344D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleIntFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_INT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleIntFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleIntFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 218.555D);
			elemHandle2.set(structSegmt, 111111111);

			double result = (double)mh.invokeExact(216.666D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111111546.221D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatDoubleFromStructByUpcallMH() throws Throwable {
		/* The size of [float, double] on AIX/PPC 64-bit is 12 bytes without padding by default
		 * while the same struct is 16 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_DOUBLE.withName("elem2")) : MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
						MemoryLayout.paddingLayout(32), C_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndFloatDoubleFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatDoubleFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 18.444F);
			elemHandle2.set(structSegmt, 619.777D);

			double result = (double)mh.invokeExact(113.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 751.788D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFloatFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFloatFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFloatFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 218.555D);
			elemHandle2.set(structSegmt, 19.22F);

			double result = (double)mh.invokeExact(216.666D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 454.441D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFloatPlusPaddingFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"),
				C_FLOAT.withName("elem2"), MemoryLayout.paddingLayout(32));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFloatFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFloatFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 218.555D);
			elemHandle2.set(structSegmt, 19.22F);

			double result = (double)mh.invokeExact(216.666D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 454.441D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAnd2FloatsDoubleFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_DOUBLE.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(double.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAnd2FloatsDoubleFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAnd2FloatsDoubleFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 11.22F);
			elemHandle2.set(structSegmt, 22.33F);
			elemHandle3.set(structSegmt, 333.444D);

			double result = (double)mh.invokeExact(111.111D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 478.105D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDouble2FloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"),
				C_FLOAT.withName("elem2"), C_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDouble2FloatsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDouble2FloatsFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 333.444D);
			elemHandle2.set(structSegmt, 11.22F);
			elemHandle3.set(structSegmt, 22.33F);

			double result = (double)mh.invokeExact(111.111D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 478.105D, 0.001D);
		}
	}

	@Test
	public void test_addFloatAndInt2FloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndInt2FloatsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndInt2FloatsFromStruct,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 111111);
			elemHandle2.set(structSegmt, 11.22F);
			elemHandle3.set(structSegmt, 22.33F);

			float result = (float)mh.invokeExact(55.567F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111200.12F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatIntFloatFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_INT.withName("elem2"), C_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndFloatIntFloatFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatIntFloatFromStruct,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 11.22F);
			elemHandle2.set(structSegmt, 111111);
			elemHandle3.set(structSegmt, 22.33F);

			float result = (float)mh.invokeExact(55.567F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111200.12F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndIntFloatDoubleFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_DOUBLE.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(double.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndIntFloatDoubleFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndIntFloatDoubleFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 111111111);
			elemHandle2.set(structSegmt, 22.33F);
			elemHandle3.set(structSegmt, 333.444D);

			double result = (double)mh.invokeExact(555.55D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111112022.324D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatIntDoubleFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_INT.withName("elem2"), C_DOUBLE.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(double.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndFloatIntDoubleFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatIntDoubleFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 22.33F);
			elemHandle2.set(structSegmt, 111111111);
			elemHandle3.set(structSegmt, 333.444D);

			double result = (double)mh.invokeExact(555.55D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111112022.324D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndLongDoubleFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndLongDoubleFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndLongDoubleFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 22222222222222L);
			elemHandle2.set(structSegmt, 33333.444D);

			double result = (double)mh.invokeExact(55555.111D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 22222222311110.555D, 0.001D);
		}
	}

	@Test
	public void test_addFloatAndInt3FloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_FLOAT.withName("elem3"), C_FLOAT.withName("elem4"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));
		VarHandle elemHandle4 = structLayout.varHandle(float.class, PathElement.groupElement("elem4"));

		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndInt3FloatsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndInt3FloatsFromStruct,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 77777777);
			elemHandle2.set(structSegmt, 11.22F);
			elemHandle3.set(structSegmt, 22.33F);
			elemHandle4.set(structSegmt, 44.55F);

			float result = (float)mh.invokeExact(66.678F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 77777921.778F, 0.001F);
		}
	}

	@Test
	public void test_addLongAndLong2FloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"),
				C_FLOAT.withName("elem2"), C_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndLong2FloatsFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndLong2FloatsFromStruct,
					FunctionDescriptor.of(longLayout, longLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 777777777777L);
			elemHandle2.set(structSegmt, 11.25F);
			elemHandle3.set(structSegmt, 22.75F);

			long result = (long)mh.invokeExact(555555555555L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1333333333365L);
		}
	}

	@Test
	public void test_addFloatAnd3FloatsIntFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_FLOAT.withName("elem3"), C_INT.withName("elem4"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));
		VarHandle elemHandle4 = structLayout.varHandle(int.class, PathElement.groupElement("elem4"));

		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAnd3FloatsIntFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAnd3FloatsIntFromStruct,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 11.22F);
			elemHandle2.set(structSegmt, 22.33F);
			elemHandle3.set(structSegmt, 44.55F);
			elemHandle4.set(structSegmt, 77777777);

			float result = (float)mh.invokeExact(66.456F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 77777921.556F, 0.001F);
		}
	}

	@Test
	public void test_addLongAndFloatLongFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
					MemoryLayout.paddingLayout(32), longLayout.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAndFloatLongFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAndFloatLongFromStruct,
					FunctionDescriptor.of(longLayout, longLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 55.11F);
			elemHandle2.set(structSegmt, 150000000000L);

			long result = (long)mh.invokeExact(5555555555L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 155555555610L);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFloatIntFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"),
				C_FLOAT.withName("elem2"), C_INT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleFloatIntFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFloatIntFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 333.444D);
			elemHandle2.set(structSegmt, 22.33F);
			elemHandle3.set(structSegmt, 111111111);

			double result = (double)mh.invokeExact(555.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111112022.341D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleLongFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), longLayout.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndDoubleLongFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleLongFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 33333.444D);
			elemHandle2.set(structSegmt, 222222222222L);

			double result = (double)mh.invokeExact(55555.111D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 222222311110.555D, 0.001D);
		}
	}

	@Test
	public void test_addLongAnd2FloatsLongFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_FLOAT.withName("elem2"), longLayout.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(long.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(long.class, long.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addLongAnd2FloatsLongFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addLongAnd2FloatsLongFromStruct,
					FunctionDescriptor.of(longLayout, longLayout, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 11.11F);
			elemHandle2.set(structSegmt, 22.11F);
			elemHandle3.set(structSegmt, 4444444444L);

			long result = (long)mh.invokeExact(11111111111L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 15555555588L);
		}
	}

	@Test
	public void test_addShortAnd3ShortsCharFromStructByUpcallMH() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(3, C_SHORT);
		GroupLayout structLayout = MemoryLayout.structLayout(shortArray, C_SHORT);

		MethodType mt = MethodType.methodType(short.class, short.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addShortAnd3ShortsCharFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addShortAnd3ShortsCharFromStruct,
					FunctionDescriptor.of(C_SHORT, C_SHORT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			MemoryAccess.setShortAtOffset(structSegmt, 0, (short)1000);
			MemoryAccess.setShortAtOffset(structSegmt, 2, (short)2000);
			MemoryAccess.setShortAtOffset(structSegmt, 4, (short)3000);
			MemoryAccess.setCharAtOffset(structSegmt, 6, 'A');

			short result = (short)mh.invokeExact((short)4000, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 10065);
		}
	}

	@Test
	public void test_addFloatAndIntFloatIntFloatFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_INT.withName("elem3"), C_FLOAT.withName("elem4"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));
		VarHandle elemHandle4 = structLayout.varHandle(float.class, PathElement.groupElement("elem4"));

		MethodType mt = MethodType.methodType(float.class, float.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addFloatAndIntFloatIntFloatFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addFloatAndIntFloatIntFloatFromStruct,
					FunctionDescriptor.of(C_FLOAT, C_FLOAT, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 555555555);
			elemHandle2.set(structSegmt, 11.222F);
			elemHandle3.set(structSegmt, 666666666);
			elemHandle4.set(structSegmt, 33.444F);

			float result = (float)mh.invokeExact(77.456F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1222222343.122F, 0.001F);
		}
	}

	@Test
	public void test_addDoubleAndIntDoubleFloatFromStructByUpcallMH() throws Throwable {
		/* The size of [int, double, float] on AIX/PPC 64-bit is 16 bytes without padding by default
		 * while the same struct is 20 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_DOUBLE.withName("elem2"), C_FLOAT.withName("elem3"))
				: MemoryLayout.structLayout(C_INT.withName("elem1"), MemoryLayout.paddingLayout(32),
						C_DOUBLE.withName("elem2"), C_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndIntDoubleFloatFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndIntDoubleFloatFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 7777);
			elemHandle2.set(structSegmt, 218.555D);
			elemHandle3.set(structSegmt, 33.444F);

			double result = (double)mh.invoke(555.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 8584.566D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatDoubleIntFromStructByUpcallMH() throws Throwable {
		/* The size of [float, double, int] on AIX/PPC 64-bit is 16 bytes without padding by default
		 * while the same struct is 20 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_DOUBLE.withName("elem2"), C_INT.withName("elem3"))
				: MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
						MemoryLayout.paddingLayout(32), C_DOUBLE.withName("elem2"),
						C_INT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndFloatDoubleIntFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatDoubleIntFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 33.444F);
			elemHandle2.set(structSegmt, 218.555D);
			elemHandle3.set(structSegmt, 7777);

			double result = (double)mh.invoke(555.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 8584.566D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndIntDoubleIntFromStructByUpcallMH() throws Throwable {
		/* The size of [int, double, int] on AIX/PPC 64-bit is 16 bytes without padding by default
		 * while the same struct is 20 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_DOUBLE.withName("elem2"), C_INT.withName("elem3"))
				: MemoryLayout.structLayout(C_INT.withName("elem1"), MemoryLayout.paddingLayout(32),
						C_DOUBLE.withName("elem2"), C_INT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndIntDoubleIntFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndIntDoubleIntFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 6666);
			elemHandle2.set(structSegmt, 218.555D);
			elemHandle3.set(structSegmt, 7777);

			double result = (double)mh.invoke(555.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 15217.122D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatDoubleFloatFromStructByUpcallMH() throws Throwable {
		/* The size of [float, double, float] on AIX/PPC 64-bit is 16 bytes without padding by default
		 * while the same struct is 20 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_DOUBLE.withName("elem2"), C_FLOAT.withName("elem3"))
				: MemoryLayout.structLayout(C_FLOAT.withName("elem1"), MemoryLayout.paddingLayout(32),
						C_DOUBLE.withName("elem2"), C_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndFloatDoubleFloatFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatDoubleFloatFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 11.222F);
			elemHandle2.set(structSegmt, 218.555D);
			elemHandle3.set(structSegmt, 33.444F);

			double result = (double)mh.invoke(555.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 818.788D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndIntDoubleLongFromStructByUpcallMH() throws Throwable {
		/* The padding in the struct [int, double, long] on AIX/PPC 64-bit is different from
		 * other platforms as follows:
		 * 1) there is no padding between int and double.
		 * 2) there is a 4-byte padding between double and long.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(C_INT.withName("elem1"), C_DOUBLE.withName("elem2"),
								MemoryLayout.paddingLayout(32), longLayout.withName("elem3")) : MemoryLayout.structLayout(C_INT.withName("elem1"),
								MemoryLayout.paddingLayout(32), C_DOUBLE.withName("elem2"), longLayout.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(long.class, PathElement.groupElement("elem3"));

		MethodType mt = MethodType.methodType(double.class, double.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addDoubleAndIntDoubleLongFromStructByUpcallMH").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_addDoubleAndIntDoubleLongFromStruct,
					FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, structLayout), scope);
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt = allocator.allocate(structLayout);
			elemHandle1.set(structSegmt, 111111111);
			elemHandle2.set(structSegmt, 619.777D);
			elemHandle3.set(structSegmt, 888888888888L);

			double result = (double)mh.invoke(113.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 889000000732.344D, 0.001D);
		}
	}

	@Test
	public void test_return254BytesFromStructByUpcallMH() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(254, C_CHAR);
		GroupLayout structLayout = MemoryLayout.structLayout(byteArray);

		MethodType mt = MethodType.methodType(MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("return254BytesFromStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_return254BytesFromStruct,
					FunctionDescriptor.of(structLayout), scope);

			MemorySegment byteArrStruSegment = (MemorySegment)mh.invoke(upcallFuncAddr);
			for (int i = 0; i < 254; i++) {
				Assert.assertEquals(MemoryAccess.getByteAtOffset(byteArrStruSegment, i), (byte)i);
			}
		}
	}

	@Test
	public void test_return4KBytesFromStructByUpcallMH() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(4096, C_CHAR);
		GroupLayout structLayout = MemoryLayout.structLayout(byteArray);

		MethodType mt = MethodType.methodType(MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("return4KBytesFromStructByUpcallMH").get();

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
			MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_return4KBytesFromStruct,
					FunctionDescriptor.of(structLayout), scope);

			MemorySegment byteArrStruSegment = (MemorySegment)mh.invoke(upcallFuncAddr);
			for (int i = 0; i < 4096; i++) {
				Assert.assertEquals(MemoryAccess.getByteAtOffset(byteArrStruSegment, i), (byte)i);
			}
		}
	}
}
