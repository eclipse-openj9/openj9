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
package org.openj9.test.jep454.downcall;

import org.testng.Assert;
import org.testng.annotations.Test;

import java.lang.foreign.Arena;
import java.lang.foreign.FunctionDescriptor;
import java.lang.foreign.Linker;
import java.lang.foreign.MemoryLayout;
import java.lang.foreign.MemorySegment;
import java.lang.foreign.SequenceLayout;
import java.lang.foreign.SymbolLookup;
import static java.lang.foreign.ValueLayout.ADDRESS;
import static java.lang.foreign.ValueLayout.JAVA_BOOLEAN;
import static java.lang.foreign.ValueLayout.JAVA_BYTE;
import static java.lang.foreign.ValueLayout.JAVA_CHAR;
import static java.lang.foreign.ValueLayout.JAVA_DOUBLE;
import static java.lang.foreign.ValueLayout.JAVA_FLOAT;
import static java.lang.foreign.ValueLayout.JAVA_INT;
import static java.lang.foreign.ValueLayout.JAVA_LONG;
import static java.lang.foreign.ValueLayout.JAVA_SHORT;
import java.lang.invoke.MethodHandle;

/**
 * Test cases for JEP 454: Foreign Linker API for argument/return struct in the
 * case of contiguous/discontigous heap array in downcall.
 *
 * Note:
 * The test suite is mainly intended for the following Clinker API:
 * MethodHandle downcallHandle(MemorySegment symbol, FunctionDescriptor function)
 */
@Test(groups = { "level.sanity" })
public class HeapArrayTests1 {
	private static Linker linker = Linker.nativeLinker();
	private static int arrayLength = 1024 * 1024;

	static {
		System.loadLibrary("clinkerffitests");
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	@Test
	public void test_addBoolFromStructPtrsWithOffset_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BOOLEAN, JAVA_BOOLEAN, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addBoolAndBoolsFromStructPointerWithXor").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = MemorySegment.ofArray(new byte[]{(byte)0, (byte)1, (byte)1});
			boolean result = (boolean)mh.invoke(true, structSegmt.asSlice(JAVA_BOOLEAN.byteSize()));
			Assert.assertEquals(result, true);
		}
	}

	@Test
	public void test_addByteFromStructPtrsWithOffset_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_BYTE, JAVA_BYTE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteAndBytesFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = MemorySegment.ofArray(new byte[]{(byte)13, (byte)25, (byte)37});
			byte result = (byte)mh.invoke((byte)45, structSegmt.asSlice(JAVA_BYTE.byteSize()));
			Assert.assertEquals(result, 107);
		}
	}

	@Test
	public void test_addCharFromStructPtrsWithOffset_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_CHAR, JAVA_CHAR, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharAndCharsFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = MemorySegment.ofArray(new char[]{'A', 'B', 'C'});
			char result = (char)mh.invoke('D', structSegmt.asSlice(JAVA_CHAR.byteSize()));
			Assert.assertEquals(result, 'G');
		}
	}

	@Test
	public void test_addShortFromStructPtrsWithOffset_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_SHORT, JAVA_SHORT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortAndShortsFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = MemorySegment.ofArray(new short[]{(short)113, (short)235, (short)378});
			short result = (short)mh.invoke((short)459, structSegmt.asSlice(JAVA_SHORT.byteSize()));
			Assert.assertEquals(result, 1072);
		}
	}

	@Test
	public void test_addIntFromStructPtrsWithOffset_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_INT, JAVA_INT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntAndIntsFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = MemorySegment.ofArray(new int[]{13151, 25281, 46371});
			int result = (int)mh.invoke(87462, structSegmt.asSlice(JAVA_INT.byteSize()));
			Assert.assertEquals(result, 159114);
		}
	}

	@Test
	public void test_addLongFromStructPtrsWithOffset_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_LONG, JAVA_LONG, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongAndLongsFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = MemorySegment.ofArray(new long[]{112233445566L, 204060801030L, 103050709010L});
			long result = (long)mh.invoke(224466880022L, structSegmt.asSlice(JAVA_LONG.byteSize()));
			Assert.assertEquals(result, 531578390062L);
		}
	}

	@Test
	public void test_addFloatFromStructPtrsWithOffset_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_FLOAT, JAVA_FLOAT, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatAndFloatsFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = MemorySegment.ofArray(new float[]{11.22F, 22.33F, 13.24F});
			float result = (float)mh.invoke(35.68F, structSegmt.asSlice(JAVA_FLOAT.byteSize()));
			Assert.assertEquals(result, 71.25F, 0.01F);
		}
	}

	@Test
	public void test_addDoubleFromStructPtrsWithOffset_1() throws Throwable {
		FunctionDescriptor fd = FunctionDescriptor.of(JAVA_DOUBLE, JAVA_DOUBLE, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleAndDoublesFromStructPointer").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment structSegmt = MemorySegment.ofArray(new double[]{11.222D, 22.333D, 13.248D});
			double result = (double)mh.invoke(66.776D, structSegmt.asSlice(JAVA_DOUBLE.byteSize()));
			Assert.assertEquals(result, 102.357D, 0.001D);
		}
	}

	@Test
	public void test_setBoolFromArrayPtrWithXor_discontiguousArrays_1() throws Throwable {
		SequenceLayout arrayLayout = MemoryLayout.sequenceLayout(arrayLength, JAVA_BOOLEAN);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("setBoolFromArrayPtrWithXor").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment heapSegmt = MemorySegment.ofArray(new byte[arrayLength]);
			MemorySegment nativeSegmt = arena.allocate(arrayLayout);
			for (int index = 0; index < arrayLength; index++) {
				nativeSegmt.setAtIndex(JAVA_BOOLEAN, index, true);
				heapSegmt.setAtIndex(JAVA_BOOLEAN, index, false);
			}

			mh.invoke(arrayLength, heapSegmt, nativeSegmt);
			for (int index = 0; index < arrayLength; index++) {
				Assert.assertEquals(heapSegmt.getAtIndex(JAVA_BOOLEAN, index), nativeSegmt.getAtIndex(JAVA_BOOLEAN, index));
			}
		}
	}

	@Test
	public void test_addByteFromArrayPtrByOne_discontiguousArrays_1() throws Throwable {
		SequenceLayout arrayLayout = MemoryLayout.sequenceLayout(arrayLength, JAVA_BYTE);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addByteFromArrayPtrByOne").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment heapSegmt = MemorySegment.ofArray(new byte[arrayLength]);
			MemorySegment nativeSegmt = arena.allocate(arrayLayout);
			for (int index = 0; index < arrayLength; index++) {
				nativeSegmt.setAtIndex(JAVA_BYTE, index, (byte)10);
				heapSegmt.setAtIndex(JAVA_BYTE, index, nativeSegmt.getAtIndex(JAVA_BYTE, index));
			}

			mh.invoke(arrayLength, heapSegmt, nativeSegmt);
			for (int index = 0; index < arrayLength; index++) {
				Assert.assertEquals(heapSegmt.getAtIndex(JAVA_BYTE, index), nativeSegmt.getAtIndex(JAVA_BYTE, index) + 1);
			}
		}
	}

	@Test
	public void test_addCharFromArrayPtrByOne_discontiguousArrays_1() throws Throwable {
		SequenceLayout arrayLayout = MemoryLayout.sequenceLayout(arrayLength, JAVA_CHAR);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addCharFromArrayPtrByOne").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment heapSegmt = MemorySegment.ofArray(new char[arrayLength]);
			MemorySegment nativeSegmt = arena.allocate(arrayLayout);
			for (int index = 0; index < arrayLength; index++) {
				nativeSegmt.setAtIndex(JAVA_CHAR, index, 'A');
				heapSegmt.setAtIndex(JAVA_CHAR, index, nativeSegmt.getAtIndex(JAVA_CHAR, index));
			}

			mh.invoke(arrayLength, heapSegmt, nativeSegmt);
			for (int index = 0; index < arrayLength; index++) {
				Assert.assertEquals(heapSegmt.getAtIndex(JAVA_CHAR, index), nativeSegmt.getAtIndex(JAVA_CHAR, index) + 1);
			}
		}
	}

	@Test
	public void test_addShortFromArrayPtrByOne_discontiguousArrays_1() throws Throwable {
		SequenceLayout arrayLayout = MemoryLayout.sequenceLayout(arrayLength, JAVA_SHORT);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addShortFromArrayPtrByOne").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment heapSegmt = MemorySegment.ofArray(new short[arrayLength]);
			MemorySegment nativeSegmt = arena.allocate(arrayLayout);
			for (int index = 0; index < arrayLength; index++) {
				nativeSegmt.setAtIndex(JAVA_SHORT, index, (short)1000);
				heapSegmt.setAtIndex(JAVA_SHORT, index, nativeSegmt.getAtIndex(JAVA_SHORT, index));
			}

			mh.invoke(arrayLength, heapSegmt, nativeSegmt);
			for (int index = 0; index < arrayLength; index++) {
				Assert.assertEquals(heapSegmt.getAtIndex(JAVA_SHORT, index), nativeSegmt.getAtIndex(JAVA_SHORT, index) + 1);
			}
		}
	}

	@Test
	public void test_addIntFromArrayPtrByOne_discontiguousArrays_1() throws Throwable {
		SequenceLayout arrayLayout = MemoryLayout.sequenceLayout(arrayLength, JAVA_INT);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addIntFromArrayPtrByOne").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment heapSegmt = MemorySegment.ofArray(new int[arrayLength]);
			MemorySegment nativeSegmt = arena.allocate(arrayLayout);
			for (int index = 0; index < arrayLength; index++) {
				nativeSegmt.setAtIndex(JAVA_INT, index, index);
				heapSegmt.setAtIndex(JAVA_INT, index, nativeSegmt.getAtIndex(JAVA_INT, index));
			}

			mh.invoke(arrayLength, heapSegmt, nativeSegmt);
			for (int index = 0; index < arrayLength; index++) {
				Assert.assertEquals(heapSegmt.getAtIndex(JAVA_INT, index), nativeSegmt.getAtIndex(JAVA_INT, index) + 1);
			}
		}
	}

	@Test
	public void test_addLongFromArrayPtrByOne_discontiguousArrays_1() throws Throwable {
		SequenceLayout arrayLayout = MemoryLayout.sequenceLayout(arrayLength, JAVA_LONG);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addLongFromArrayPtrByOne").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment heapSegmt = MemorySegment.ofArray(new long[arrayLength]);
			MemorySegment nativeSegmt = arena.allocate(arrayLayout);
			for (int index = 0; index < arrayLength; index++) {
				nativeSegmt.setAtIndex(JAVA_LONG, index, 10000000000L);
				heapSegmt.setAtIndex(JAVA_LONG, index, nativeSegmt.getAtIndex(JAVA_LONG, index));
			}

			mh.invoke(arrayLength, heapSegmt, nativeSegmt);
			for (int index = 0; index < arrayLength; index++) {
				Assert.assertEquals(heapSegmt.getAtIndex(JAVA_LONG, index), nativeSegmt.getAtIndex(JAVA_LONG, index) + 1);
			}
		}
	}

	@Test
	public void test_addFloatFromArrayPtrByOne_discontiguousArrays_1() throws Throwable {
		SequenceLayout arrayLayout = MemoryLayout.sequenceLayout(arrayLength, JAVA_FLOAT);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addFloatFromArrayPtrByOne").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment heapSegmt = MemorySegment.ofArray(new float[arrayLength]);
			MemorySegment nativeSegmt = arena.allocate(arrayLayout);
			for (int index = 0; index < arrayLength; index++) {
				nativeSegmt.setAtIndex(JAVA_FLOAT, index, 123.456F);
				heapSegmt.setAtIndex(JAVA_FLOAT, index, nativeSegmt.getAtIndex(JAVA_FLOAT, index));
			}

			mh.invoke(arrayLength, heapSegmt, nativeSegmt);
			for (int index = 0; index < arrayLength; index++) {
				Assert.assertEquals(heapSegmt.getAtIndex(JAVA_FLOAT, index), nativeSegmt.getAtIndex(JAVA_FLOAT, index) + 1, 0.001F);
			}
		}
	}

	@Test
	public void test_addDoubleFromArrayPtrByOne_discontiguousArrays_1() throws Throwable {
		SequenceLayout arrayLayout = MemoryLayout.sequenceLayout(arrayLength, JAVA_DOUBLE);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(JAVA_INT, ADDRESS, ADDRESS);
		MemorySegment functionSymbol = nativeLibLookup.find("addDoubleFromArrayPtrByOne").get();
		MethodHandle mh = linker.downcallHandle(functionSymbol, fd, Linker.Option.critical(true));

		try (Arena arena = Arena.ofConfined()) {
			MemorySegment heapSegmt = MemorySegment.ofArray(new double[arrayLength]);
			MemorySegment nativeSegmt = arena.allocate(arrayLayout);
			for (int index = 0; index < arrayLength; index++) {
				nativeSegmt.setAtIndex(JAVA_DOUBLE, index, 1234567890.345D);
				heapSegmt.setAtIndex(JAVA_DOUBLE, index, nativeSegmt.getAtIndex(JAVA_DOUBLE, index));
			}

			mh.invoke(arrayLength, heapSegmt, nativeSegmt);
			for (int index = 0; index < arrayLength; index++) {
				Assert.assertEquals(heapSegmt.getAtIndex(JAVA_DOUBLE, index), nativeSegmt.getAtIndex(JAVA_DOUBLE, index) + 1, 0.001D);
			}
		}
	}
}
