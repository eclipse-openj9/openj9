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
package org.openj9.test.jep389.downcall;

import org.testng.Assert;
import static org.testng.Assert.fail;
import org.testng.annotations.Test;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.lang.invoke.VarHandle;
import java.lang.invoke.WrongMethodTypeException;

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
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemoryLayout.PathElement;
import jdk.incubator.foreign.MemoryLayouts;
import jdk.incubator.foreign.MemorySegment;
import jdk.incubator.foreign.ResourceScope;
import jdk.incubator.foreign.SegmentAllocator;
import jdk.incubator.foreign.SymbolLookup;
import jdk.incubator.foreign.ValueLayout;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) in downcall, which verifies
 * the majority of illegal cases including the mismatch between the type and the
 * corresponding layout, the inconsisitency of the arity, etc.
 */
@Test(groups = { "level.sanity" })
public class InvalidDownCallTests {
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
	private static final SymbolLookup defaultLibLookup = CLinker.systemLookup();

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidBooleanTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Boolean.class, boolean.class, boolean.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolsWithOr").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Boolean return type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidBooleanTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, Boolean.class, boolean.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolsWithOr").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Boolean argument");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidCharacterTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Character.class, char.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFrom2Chars").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Character return type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidCharacterTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, char.class, Character.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFrom2Chars").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Character argument");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidByteTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Byte.class, byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Byte return type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidByteTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, Byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Byte argument");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidShortTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Short.class, short.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Short return type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidShortTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, Short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Short argument");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidIntegerTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Integer.class,int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Integer return type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidIntegerTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(int.class,Integer.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Integer argument");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidLongTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Long.class, long.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, longLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Long return type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidLongTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, long.class, Long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, longLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Long argument");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidFloatTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Float.class, float.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Float return type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidFloatTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, Float.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Float argument");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidDoubleTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Double.class, double.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Double return type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidDoubleTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, double.class, Double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Double argument");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatch between the layout .*")
	public void test_mismatchedVoidReturnLayoutWithIntType() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the void return layout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatch between the layout .*")
	public void test_mismatchedVoidReturnTypeWithIntReturnLayout() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the void return type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedShortReturnLayoutWithIntType() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_INT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the non-void return layout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedShortReturnTypeWithIntReturnLayout() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the non-void return type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = ".* inconsistent with the arity .*")
	public void test_mismatchedArity() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched arity");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedBoolArgWithShortLayout() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, boolean.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_SHORT, C_CHAR);
		Addressable functionSymbol = nativeLibLookup.lookup("add2BoolsWithOr").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched layout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedCharArgWithCharLayout() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, char.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_CHAR, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("createNewCharFrom2Chars").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched layout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedByteArgWithShortLayout() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched layout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedIntArgTypeWithShortLayout() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, int.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched int type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedIntArgLayoutWithShortType() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_INT, C_SHORT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched int layout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedIntArgTypeWithLongLayout() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, int.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, longLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched int type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedIntArgLayoutWithLongType() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, long.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched int layout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatch between the layout .*")
	public void test_mismatchedIntArgTypeWithFloatLayout() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, int.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched int type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatch between the layout .*")
	public void test_mismatchedIntArgLayoutWithFloatType() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_INT);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched int layout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatch between the layout .*")
	public void test_mismatchedLongArgTypeWithDoubleLayout() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, long.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched long type");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatch between the layout .*")
	public void test_mismatchedLongArgLayoutWithDoubleType() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, double.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, longLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched long layout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "ValueLayout is expected.*")
	public void test_invalidMemoryLayoutForIntType() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, MemoryLayout.paddingLayout(32));
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsReturnVoid").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the invalid MemoryLayout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "ValueLayout is expected.*")
	public void test_invalidMemoryLayoutForMemoryAddress() throws Throwable {
		Addressable functionSymbol = defaultLibLookup.lookup("strlen").get();
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, MemoryLayout.paddingLayout(64));
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the invalid MemoryLayout");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedLayoutSizeForIntType() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, MemoryLayouts.BITS_64_LE);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntsReturnVoid").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched layout size");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedLayoutSizeForMemoryAddress() throws Throwable {
		Addressable functionSymbol = defaultLibLookup.lookup("strlen").get();
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, MemoryLayouts.BITS_16_LE);
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched layout size");
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = ".* neither primitive nor .*")
	public void test_unsupportedStringType() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, String.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, MemoryLayouts.BITS_64_LE);
		Addressable functionSymbol = nativeLibLookup.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the unsupported String type");
	}

	@Test(expectedExceptions = NullPointerException.class)
	public void test_nullValueForPtrArgument() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		int result = (int)mh.invoke(19202122, null);
		fail("Failed to throw out NullPointerException in the case of the null value");
	}

	@Test(expectedExceptions = WrongMethodTypeException.class)
	public void test_nullValueForStructArgument() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(allocator, structSegmt1, null);
			fail("Failed to throw out WrongMethodTypeException in the case of the null value");
		}
	}

	public void test_nullSegmentForPtrArgument() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("validateNullAddrArgument").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		int result = (int)mh.invoke(19202122, MemoryAddress.NULL);
		Assert.assertEquals(result, 19202122);
	}

	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "A heap address is not allowed.*")
	public void test_heapSegmentForPtrArgument() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(int.class, int.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("addIntAndIntsFromStructPointer").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		MemorySegment structSegmt = MemorySegment.ofArray(new int[]{11121314, 15161718});
		int result = (int)mh.invoke(19202122, structSegmt.address());
		fail("Failed to throw out IllegalArgumentException in the case of the heap address");
	}

	public void test_heapSegmentForStructArgument() throws Throwable {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStruct").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);

		try (ResourceScope scope = ResourceScope.newConfinedScope()) {
			SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
			MemorySegment structSegmt1 = allocator.allocate(structLayout);
			intHandle1.set(structSegmt1, 11223344);
			intHandle2.set(structSegmt1, 55667788);
			MemorySegment structSegmt2 = MemorySegment.ofArray(new int[]{99001122, 33445566});

			MemorySegment resultSegmt = (MemorySegment)mh.invokeExact(allocator, structSegmt1, structSegmt2);
			Assert.assertEquals(intHandle1.get(resultSegmt), 110224466);
			Assert.assertEquals(intHandle2.get(resultSegmt), 89113354);
		}
	}
}
