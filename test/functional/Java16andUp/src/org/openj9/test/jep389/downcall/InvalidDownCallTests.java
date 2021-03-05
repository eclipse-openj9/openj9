/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package org.openj9.test.jep389.downcall;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import static org.testng.Assert.fail;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import jdk.incubator.foreign.CLinker;
import static jdk.incubator.foreign.CLinker.*;
import jdk.incubator.foreign.FunctionDescriptor;
import jdk.incubator.foreign.ValueLayout;
import jdk.incubator.foreign.MemoryLayout;
import jdk.incubator.foreign.MemoryLayouts;
import jdk.incubator.foreign.MemoryAddress;
import jdk.incubator.foreign.LibraryLookup;
import static jdk.incubator.foreign.LibraryLookup.Symbol;

/**
 * Test cases for JEP 389: Foreign Linker API (Incubator) DownCall for primitive types,
 * which verifies the majority of illegal cases including the mismatch between the type
 * and the corresponding layout, the inconsisitency of the arity, etc.
 */
@Test(groups = { "level.sanity" })
public class InvalidDownCallTests {
	private static boolean isWinOS = System.getProperty("os.name").toLowerCase().contains("win");
	private static ValueLayout longLayout = isWinOS ? C_LONG_LONG : C_LONG;
	LibraryLookup nativeLib = LibraryLookup.ofLibrary("clinkerffitests");
	LibraryLookup defaultLib = LibraryLookup.ofDefault();
	CLinker clinker = CLinker.getInstance();
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidBooleanTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Boolean.class, boolean.class, boolean.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2BoolsWithOr").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Boolean return type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidBooleanTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, Boolean.class, boolean.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2BoolsWithOr").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Boolean argument");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidCharacterTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Character.class, char.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("createNewCharFrom2Chars").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Character return type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidCharacterTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, char.class, Character.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("createNewCharFrom2Chars").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Character argument");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidByteTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Byte.class, byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Symbol functionSymbol = nativeLib.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Byte return type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidByteTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, Byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_CHAR);
		Symbol functionSymbol = nativeLib.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Byte argument");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidShortTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Short.class, short.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Short return type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidShortTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, Short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Short argument");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidIntegerTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Integer.class,int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Integer return type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidIntegerTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(int.class,Integer.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Integer argument");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidLongTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Long.class, long.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, longLayout);
		Symbol functionSymbol = nativeLib.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Long return type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidLongTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, long.class, Long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, longLayout);
		Symbol functionSymbol = nativeLib.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Long argument");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidFloatTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Float.class, float.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT);
		Symbol functionSymbol = nativeLib.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Float return type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidFloatTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, Float.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT);
		Symbol functionSymbol = nativeLib.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Float argument");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The return type must be .*")
	public void test_invalidDoubleTypeOnReturn() throws Throwable {
		MethodType mt = MethodType.methodType(Double.class, double.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE);
		Symbol functionSymbol = nativeLib.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Double return type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "The passed-in argument type at index .*")
	public void test_invalidDoubleTypeArgument() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, double.class, Double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE);
		Symbol functionSymbol = nativeLib.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the Double argument");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatch between the layout .*")
	public void test_mismatchedVoidReturnLayoutWithIntType() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the void return layout");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatch between the layout .*")
	public void test_mismatchedVoidReturnTypeWithIntReturnLayout() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the void return type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedShortReturnLayoutWithIntType() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the non-void return layout");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedShortReturnTypeWithIntReturnLayout() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the non-void return type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = ".* inconsistent with the arity .*")
	public void test_mismatchedArity() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, C_INT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched arity");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedBoolArgWithShortLayout() throws Throwable {
		MethodType mt = MethodType.methodType(boolean.class, boolean.class, boolean.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_SHORT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2BoolsWithOr").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched layout");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedCharArgWithCharLayout() throws Throwable {
		MethodType mt = MethodType.methodType(char.class, char.class, char.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_CHAR, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("createNewCharFrom2Chars").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched layout");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedByteArgWithShortLayout() throws Throwable {
		MethodType mt = MethodType.methodType(byte.class, byte.class, byte.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_CHAR, C_CHAR, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("add2Bytes").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched layout");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedIntArgTypeWithShortLayout() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, int.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_SHORT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched int type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedIntArgLayoutWithShortType() throws Throwable {
		MethodType mt = MethodType.methodType(short.class, short.class, short.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_SHORT, C_INT, C_SHORT);
		Symbol functionSymbol = nativeLib.lookup("add2Shorts").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched int layout");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedIntArgTypeWithLongLayout() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, int.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, longLayout);
		Symbol functionSymbol = nativeLib.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched int type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedIntArgLayoutWithLongType() throws Throwable {
		MethodType mt = MethodType.methodType(long.class, long.class, long.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, longLayout, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Longs").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched int layout");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatch between the layout .*")
	public void test_mismatchedIntArgTypeWithFloatLayout() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, int.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_FLOAT);
		Symbol functionSymbol = nativeLib.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched int type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatch between the layout .*")
	public void test_mismatchedIntArgLayoutWithFloatType() throws Throwable {
		MethodType mt = MethodType.methodType(float.class, float.class, float.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_FLOAT, C_FLOAT, C_INT);
		Symbol functionSymbol = nativeLib.lookup("add2Floats").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched int layout");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatch between the layout .*")
	public void test_mismatchedLongArgTypeWithDoubleLayout() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, long.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, C_DOUBLE);
		Symbol functionSymbol = nativeLib.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched long type");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatch between the layout .*")
	public void test_mismatchedLongArgLayoutWithDoubleType() throws Throwable {
		MethodType mt = MethodType.methodType(double.class, double.class, double.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_DOUBLE, C_DOUBLE, longLayout);
		Symbol functionSymbol = nativeLib.lookup("add2Doubles").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched long layout");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "ValueLayout is expected.*")
	public void test_invalidMemoryLayoutForIntType() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, MemoryLayout.ofPaddingBits(32));
		Symbol functionSymbol = nativeLib.lookup("add2IntsReturnVoid").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the invalid MemoryLayout");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "ValueLayout is expected.*")
	public void test_invalidMemoryLayoutForMemoryAddress() throws Throwable {
		Symbol functionSymbol = defaultLib.lookup("strlen").get();
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, MemoryLayout.ofPaddingBits(64));
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the invalid MemoryLayout");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedLayoutSizeForIntType() throws Throwable {
		MethodType mt = MethodType.methodType(void.class, int.class, int.class);
		FunctionDescriptor fd = FunctionDescriptor.ofVoid(C_INT, MemoryLayouts.BITS_64_LE);
		Symbol functionSymbol = nativeLib.lookup("add2IntsReturnVoid").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched layout size");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = "Mismatched size .*")
	public void test_mismatchedLayoutSizeForMemoryAddress() throws Throwable {
		Symbol functionSymbol = defaultLib.lookup("strlen").get();
		MethodType mt = MethodType.methodType(long.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(longLayout, MemoryLayouts.BITS_16_LE);
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the mismatched layout size");
	}
	
	@Test(expectedExceptions = IllegalArgumentException.class, expectedExceptionsMessageRegExp = ".* neither primitive nor .*")
	public void test_unsupportedStringType() throws Throwable {
		MethodType mt = MethodType.methodType(int.class, int.class, String.class);
		FunctionDescriptor fd = FunctionDescriptor.of(C_INT, C_INT, MemoryLayouts.BITS_64_LE);
		Symbol functionSymbol = nativeLib.lookup("add2Ints").get();
		MethodHandle mh = clinker.downcallHandle(functionSymbol, mt, fd);
		fail("Failed to throw out IllegalArgumentException in the case of the unsupported String type");
	}

}
