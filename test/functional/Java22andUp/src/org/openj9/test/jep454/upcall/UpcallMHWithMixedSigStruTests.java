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
package org.openj9.test.jep454.upcall;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.GroupLayout;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemoryLayout.PathElement;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SegmentAllocator;
import java.lang.foreign.SequenceLayout;
import java.lang.foreign.SymbolLookup;
import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_BYTE;
import static java.lang.foreign.ValueLayout.JAVA_CHAR;
import static java.lang.foreign.ValueLayout.JAVA_DOUBLE;
import static java.lang.foreign.ValueLayout.JAVA_FLOAT;
import static java.lang.foreign.ValueLayout.JAVA_INT;
import static java.lang.foreign.ValueLayout.JAVA_LONG;
import static java.lang.foreign.ValueLayout.JAVA_SHORT;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.VarHandle;

/**
 * Test cases for JEP 454: Foreign Linker API for the mixed native signatures
 * in argument/return struct in upcall, which are not covered in UpcallMHWithStructTests and
 * specially designed to validate the native signature types required in the genenerated thunk.
 *
 * Note: the padding elements in the struct are only required by RI or VarHandle (accessing the
 * data address) while they are totally ignored in OpenJ9 given the padding/alignment are
 * computed by libffi automatically in native.
 */
@Test(groups = { "level.sanity" })
public class UpcallMHWithMixedSigStruTests {
	private static boolean isAixOS = System.getProperty("os.name").toLowerCase().contains("aix");
	private static Linker linker = Linker.nativeLinker();

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addIntAndIntShortFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_SHORT.withName("elem2"), MemoryLayout.paddingLayout(JAVA_SHORT.byteSize()));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntShortFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntShortFromStruct,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 11223344);
			elemHandle2.set(structSegmt, 0L, (short)32766);

			int result = (int)mh.invoke(22334455, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 33590565);
		}
	}

	@Test
	public void test_addIntAndShortIntFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_SHORT.withName("elem1"),
				MemoryLayout.paddingLayout(JAVA_SHORT.byteSize()), JAVA_INT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndShortIntFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndShortIntFromStruct,
					FunctionDescriptor.of(JAVA_INT, JAVA_INT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, (short)32766);
			elemHandle2.set(structSegmt, 0L, 22446688);

			int result = (int)mh.invoke(11335577, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 33815031);
		}
	}

	@Test
	public void test_addIntAndIntLongFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				MemoryLayout.paddingLayout(JAVA_INT.byteSize()), JAVA_LONG.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntLongFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndIntLongFromStruct,
					FunctionDescriptor.of(JAVA_LONG, JAVA_INT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 11223344);
			elemHandle2.set(structSegmt, 0L, 667788990011L);

			long result = (long)mh.invoke(22446688, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 667822660043L);
		}
	}

	@Test
	public void test_addIntAndLongIntFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"),
				JAVA_INT.withName("elem2"), MemoryLayout.paddingLayout(JAVA_INT.byteSize()));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_INT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndLongIntFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addIntAndLongIntFromStruct,
					FunctionDescriptor.of(JAVA_LONG, JAVA_INT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 667788990011L);
			elemHandle2.set(structSegmt, 0L, 11223344);

			long result = (long)mh.invoke(1234567, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 667801447922L);
		}
	}

	@Test
	public void test_addDoubleAndIntDoubleFromStructByUpcallMH() throws Throwable {
		/* The size of [int, double] on AIX/PPC 64-bit is 12 bytes without padding by default
		 * while the same struct is 16 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2")) : MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
						MemoryLayout.paddingLayout(JAVA_INT.byteSize()), JAVA_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndIntDoubleFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndIntDoubleFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 111111111);
			elemHandle2.set(structSegmt, 0L, 619.777D);

			double result = (double)mh.invoke(113.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111111844.344D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleIntFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"),
				JAVA_INT.withName("elem2")) :  MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"),
						JAVA_INT.withName("elem2"), MemoryLayout.paddingLayout(JAVA_INT.byteSize()));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleIntFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleIntFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 218.555D);
			elemHandle2.set(structSegmt, 0L, 111111111);

			double result = (double)mh.invoke(216.666D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111111546.221D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatDoubleFromStructByUpcallMH() throws Throwable {
		/* The size of [float, double] on AIX/PPC 64-bit is 12 bytes without padding by default
		 * while the same struct is 16 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2")) : MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
						MemoryLayout.paddingLayout(JAVA_FLOAT.byteSize()), JAVA_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatDoubleFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatDoubleFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 18.444F);
			elemHandle2.set(structSegmt, 0L, 619.777D);

			double result = (double)mh.invoke(113.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 751.788D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFloatFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"),
				JAVA_FLOAT.withName("elem2")) :  MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"),
						JAVA_FLOAT.withName("elem2"), MemoryLayout.paddingLayout(JAVA_FLOAT.byteSize()));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFloatFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFloatFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 218.555D);
			elemHandle2.set(structSegmt, 0L, 19.22F);

			double result = (double)mh.invoke(216.666D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 454.441D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAnd2FloatsDoubleFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_DOUBLE.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAnd2FloatsDoubleFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAnd2FloatsDoubleFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 11.22F);
			elemHandle2.set(structSegmt, 0L, 22.33F);
			elemHandle3.set(structSegmt, 0L, 333.444D);

			double result = (double)mh.invoke(111.111D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 478.105D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDouble2FloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDouble2FloatsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDouble2FloatsFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 333.444D);
			elemHandle2.set(structSegmt, 0L, 11.22F);
			elemHandle3.set(structSegmt, 0L, 22.33F);

			double result = (double)mh.invoke(111.111D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 478.105D, 0.001D);
		}
	}

	@Test
	public void test_addFloatAndInt2FloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndInt2FloatsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndInt2FloatsFromStruct,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 111111);
			elemHandle2.set(structSegmt, 0L, 11.22F);
			elemHandle3.set(structSegmt, 0L, 22.33F);

			float result = (float)mh.invoke(55.567F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111200.12F, 0.01F);
		}
	}

	@Test
	public void test_addFloatAndFloatIntFloatFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), JAVA_INT.withName("elem2"), JAVA_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatIntFloatFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndFloatIntFloatFromStruct,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 11.22F);
			elemHandle2.set(structSegmt, 0L, 111111);
			elemHandle3.set(structSegmt, 0L, 22.33F);

			float result = (float)mh.invoke(55.567F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111200.12F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleAndIntFloatDoubleFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_DOUBLE.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndIntFloatDoubleFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndIntFloatDoubleFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 111111111);
			elemHandle2.set(structSegmt, 0L, 22.33F);
			elemHandle3.set(structSegmt, 0L, 333.444D);

			double result = (double)mh.invoke(555.55D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111112022.324D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatIntDoubleFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_INT.withName("elem2"), JAVA_DOUBLE.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatIntDoubleFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatIntDoubleFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 22.33F);
			elemHandle2.set(structSegmt, 0L, 111111111);
			elemHandle3.set(structSegmt, 0L, 333.444D);

			double result = (double)mh.invoke(555.55D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111112022.324D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndLongDoubleFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"), JAVA_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndLongDoubleFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndLongDoubleFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 22222222222222L);
			elemHandle2.set(structSegmt, 0L, 33333.444D);

			double result = (double)mh.invoke(55555.111D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 22222222311110.555D, 0.001D);
		}
	}

	@Test
	public void test_addFloatAndInt3FloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_FLOAT.withName("elem3"), JAVA_FLOAT.withName("elem4"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));
		VarHandle elemHandle4 = structLayout.varHandle(PathElement.groupElement("elem4"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndInt3FloatsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndInt3FloatsFromStruct,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 77777777);
			elemHandle2.set(structSegmt, 0L, 11.22F);
			elemHandle3.set(structSegmt, 0L, 22.33F);
			elemHandle4.set(structSegmt, 0L, 44.55F);

			float result = (float)mh.invoke(66.678F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 77777921.778F, 0.001F);
		}
	}

	@Test
	public void test_addLongAndLong2FloatsFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_LONG.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLong2FloatsFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndLong2FloatsFromStruct,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 777777777777L);
			elemHandle2.set(structSegmt, 0L, 11.25F);
			elemHandle3.set(structSegmt, 0L, 22.75F);

			long result = (long)mh.invoke(555555555555L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1333333333365L);
		}
	}

	@Test
	public void test_addFloatAnd3FloatsIntFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_FLOAT.withName("elem3"), JAVA_INT.withName("elem4"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));
		VarHandle elemHandle4 = structLayout.varHandle(PathElement.groupElement("elem4"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAnd3FloatsIntFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAnd3FloatsIntFromStruct,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 11.22F);
			elemHandle2.set(structSegmt, 0L, 22.33F);
			elemHandle3.set(structSegmt, 0L, 44.55F);
			elemHandle4.set(structSegmt, 0L, 77777777);

			float result = (float)mh.invoke(66.456F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 77777921.556F, 0.001F);
		}
	}

	@Test
	public void test_addLongAndFloatLongFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
					MemoryLayout.paddingLayout(JAVA_FLOAT.byteSize()), JAVA_LONG.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndFloatLongFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAndFloatLongFromStruct,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 55.11F);
			elemHandle2.set(structSegmt, 0L, 150000000000L);

			long result = (long)mh.invoke(5555555555L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 155555555610L);
		}
	}

	@Test
	public void test_addDoubleAndDoubleFloatIntFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_INT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleFloatIntFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleFloatIntFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 333.444D);
			elemHandle2.set(structSegmt, 0L, 22.33F);
			elemHandle3.set(structSegmt, 0L, 111111111);

			double result = (double)mh.invoke(555.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 111112022.341D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndDoubleLongFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_DOUBLE.withName("elem1"), JAVA_LONG.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoubleLongFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndDoubleLongFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 33333.444D);
			elemHandle2.set(structSegmt, 0L, 222222222222L);

			double result = (double)mh.invoke(55555.111D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 222222311110.555D, 0.001D);
		}
	}

	@Test
	public void test_addLongAnd2FloatsLongFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_LONG.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAnd2FloatsLongFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addLongAnd2FloatsLongFromStruct,
					FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 11.11F);
			elemHandle2.set(structSegmt, 0L, 22.11F);
			elemHandle3.set(structSegmt, 0L, 4444444444L);

			long result = (long)mh.invoke(11111111111L, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 15555555588L);
		}
	}

	@Test
	public void test_addShortAnd3ShortsCharFromStructByUpcallMH() throws Throwable {
		SequenceLayout shortArray = MemoryLayout.sequenceLayout(3, JAVA_SHORT);
		GroupLayout structLayout = MemoryLayout.structLayout(shortArray, JAVA_CHAR);

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAnd3ShortsCharFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addShortAnd3ShortsCharFromStruct,
					FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			structSegmt.set(JAVA_SHORT, 0, (short)1000);
			structSegmt.set(JAVA_SHORT, 2, (short)2000);
			structSegmt.set(JAVA_SHORT, 4, (short)3000);
			structSegmt.set(JAVA_CHAR, 6, 'A');

			short result = (short)mh.invoke((short)4000, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 10065);
		}
	}

	@Test
	public void test_addFloatAndIntFloatIntFloatFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_FLOAT.withName("elem2"), JAVA_INT.withName("elem3"), JAVA_FLOAT.withName("elem4"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));
		VarHandle elemHandle4 = structLayout.varHandle(PathElement.groupElement("elem4"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndIntFloatIntFloatFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addFloatAndIntFloatIntFloatFromStruct,
					FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 555555555);
			elemHandle2.set(structSegmt, 0L, 11.222F);
			elemHandle3.set(structSegmt, 0L, 666666666);
			elemHandle4.set(structSegmt, 0L, 33.444F);

			float result = (float)mh.invoke(77.456F, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 1222222343.122F, 0.001F);
		}
	}

	@Test
	public void test_addDoubleAndIntDoubleFloatFromStructByUpcallMH() throws Throwable {
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2"), JAVA_FLOAT.withName("elem3"))
				: MemoryLayout.structLayout(JAVA_INT.withName("elem1"), MemoryLayout.paddingLayout(JAVA_INT.byteSize()),
						JAVA_DOUBLE.withName("elem2"), JAVA_FLOAT.withName("elem3"), MemoryLayout.paddingLayout(JAVA_FLOAT.byteSize()));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndIntDoubleFloatFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndIntDoubleFloatFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 7777);
			elemHandle2.set(structSegmt, 0L, 218.555D);
			elemHandle3.set(structSegmt, 0L, 33.444F);

			double result = (double)mh.invoke(555.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 8584.566D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatDoubleIntFromStructByUpcallMH() throws Throwable {
		/* The size of [float, double, int] on AIX/PPC 64-bit is 16 bytes without padding by default
		 * while the same struct is 20 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2"), JAVA_INT.withName("elem3"))
				: MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), MemoryLayout.paddingLayout(JAVA_FLOAT.byteSize()),
						JAVA_DOUBLE.withName("elem2"), JAVA_INT.withName("elem3"), MemoryLayout.paddingLayout(JAVA_FLOAT.byteSize()));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatDoubleIntFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatDoubleIntFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 33.444F);
			elemHandle2.set(structSegmt, 0L, 218.555D);
			elemHandle3.set(structSegmt, 0L, 7777);

			double result = (double)mh.invoke(555.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 8584.566D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndIntDoubleIntFromStructByUpcallMH() throws Throwable {
		/* The size of [int, double, int] on AIX/PPC 64-bit is 16 bytes without padding by default
		 * while the same struct is 20 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2"), JAVA_INT.withName("elem3"))
				: MemoryLayout.structLayout(JAVA_INT.withName("elem1"), MemoryLayout.paddingLayout(JAVA_INT.byteSize()),
						JAVA_DOUBLE.withName("elem2"), JAVA_INT.withName("elem3"), MemoryLayout.paddingLayout(JAVA_INT.byteSize()));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndIntDoubleIntFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndIntDoubleIntFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 6666);
			elemHandle2.set(structSegmt, 0L, 218.555D);
			elemHandle3.set(structSegmt, 0L, 7777);

			double result = (double)mh.invoke(555.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 15217.122D, 0.001D);
		}
	}

	@Test
	public void test_addDoubleAndFloatDoubleFloatFromStructByUpcallMH() throws Throwable {
		/* The size of [float, double, float] on AIX/PPC 64-bit is 16 bytes without padding by default
		 * while the same struct is 20 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2"), JAVA_FLOAT.withName("elem3"))
				: MemoryLayout.structLayout(JAVA_FLOAT.withName("elem1"), MemoryLayout.paddingLayout(JAVA_FLOAT.byteSize()),
						JAVA_DOUBLE.withName("elem2"), JAVA_FLOAT.withName("elem3"), MemoryLayout.paddingLayout(JAVA_FLOAT.byteSize()));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndFloatDoubleFloatFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndFloatDoubleFloatFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 11.222F);
			elemHandle2.set(structSegmt, 0L, 218.555D);
			elemHandle3.set(structSegmt, 0L, 33.444F);

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
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(JAVA_INT.withName("elem1"),
				JAVA_DOUBLE.withName("elem2"), MemoryLayout.paddingLayout(JAVA_INT.byteSize()), JAVA_LONG.withName("elem3"))
				: MemoryLayout.structLayout(JAVA_INT.withName("elem1"), MemoryLayout.paddingLayout(JAVA_INT.byteSize()),
						JAVA_DOUBLE.withName("elem2"), JAVA_LONG.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(PathElement.groupElement("elem3"));

		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndIntDoubleLongFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_addDoubleAndIntDoubleLongFromStruct,
					FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, structLayout), arena);
			MemorySegment structSegmt = arena.allocate(structLayout);
			elemHandle1.set(structSegmt, 0L, 111111111);
			elemHandle2.set(structSegmt, 0L, 619.777D);
			elemHandle3.set(structSegmt, 0L, 888888888888L);

			double result = (double)mh.invoke(113.567D, structSegmt, upcallFuncAddr);
			Assert.assertEquals(result, 889000000732.344D, 0.001D);
		}
	}

	@Test
	public void test_return254BytesFromStructByUpcallMH() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(254, JAVA_BYTE);
		GroupLayout structLayout = MemoryLayout.structLayout(byteArray);

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("return254BytesFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_return254BytesFromStruct,
					FunctionDescriptor.of(structLayout), arena);
			MemorySegment byteArrStruSegment = (MemorySegment)mh.invoke((SegmentAllocator)arena, upcallFuncAddr);
			for (int i = 0; i < 254; i++) {
				Assert.assertEquals(byteArrStruSegment.get(JAVA_BYTE, i), (byte)i);
			}
		}
	}

	@Test
	public void test_return4KBytesFromStructByUpcallMH() throws Throwable {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(4096, JAVA_BYTE);
		GroupLayout structLayout = MemoryLayout.structLayout(byteArray);

		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("return4KBytesFromStructByUpcallMH").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd);

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment upcallFuncAddr = linker.upcallStub(UpcallMethodHandles.MH_return4KBytesFromStruct,
					FunctionDescriptor.of(structLayout), arena);
			MemorySegment byteArrStruSegment = (MemorySegment)mh.invoke((SegmentAllocator)arena, upcallFuncAddr);
			for (int i = 0; i < 4096; i++) {
				Assert.assertEquals(byteArrStruSegment.get(JAVA_BYTE, i), (byte)i);
			}
		}
	}
}
