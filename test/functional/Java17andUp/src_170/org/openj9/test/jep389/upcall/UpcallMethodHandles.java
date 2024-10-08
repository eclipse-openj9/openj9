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

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import static java.lang.invoke.MethodType.methodType;
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
 * The helper class that contains all upcall method handles with primitive types or struct as arguments.
 */
public class UpcallMethodHandles {
	private static final Lookup lookup = MethodHandles.lookup();
	private static ResourceScope scope = ResourceScope.newImplicitScope();
	private static String osName = System.getProperty("os.name").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinOS = osName.contains("win");
	/* long long is 64 bits on AIX/ppc64, which is the same as Windows */
	private static ValueLayout longLayout = (isWinOS || isAixOS) ? C_LONG_LONG : C_LONG;

	static final MethodType MT_Bool_Bool_MemSegmt = methodType(boolean.class, boolean.class, MemorySegment.class);
	static final MethodType MT_MemAddr_Bool_MemAddr = methodType(MemoryAddress.class, boolean.class, MemoryAddress.class);
	static final MethodType MT_Char_Char_MemSegmt = methodType(char.class, char.class, MemorySegment.class);
	static final MethodType MT_MemAddr_MemAddr_Char = methodType(MemoryAddress.class, MemoryAddress.class, char.class);
	static final MethodType MT_Byte_Byte_MemSegmt = methodType(byte.class, byte.class, MemorySegment.class);
	static final MethodType MT_MemAddr_Byte_MemAddr = methodType(MemoryAddress.class, byte.class, MemoryAddress.class);
	static final MethodType MT_Short_Short_MemSegmt = methodType(short.class, short.class, MemorySegment.class);
	static final MethodType MT_MemAddr_MemAddr_Short = methodType(MemoryAddress.class, MemoryAddress.class, short.class);
	static final MethodType MT_Int_Int_MemSegmt = methodType(int.class, int.class, MemorySegment.class);
	static final MethodType MT_MemAddr_Int_MemAddr = methodType(MemoryAddress.class, int.class, MemoryAddress.class);
	static final MethodType MT_Long_Long_MemSegmt = methodType(long.class, long.class, MemorySegment.class);
	static final MethodType MT_MemAddr_MemAddr_Long = methodType(MemoryAddress.class, MemoryAddress.class, long.class);
	static final MethodType MT_Long_Int_MemSegmt = methodType(long.class, int.class, MemorySegment.class);
	static final MethodType MT_Float_Float_MemSegmt = methodType(float.class, float.class, MemorySegment.class);
	static final MethodType MT_MemAddr_Float_MemAddr = methodType(MemoryAddress.class, float.class, MemoryAddress.class);
	static final MethodType MT_Double_Double_MemSegmt = methodType(double.class, double.class, MemorySegment.class);
	static final MethodType MT_MemAddr_MemAddr_Double = methodType(MemoryAddress.class, MemoryAddress.class, double.class);
	static final MethodType MT_MemAddr_MemAddr_MemSegmt = methodType(MemoryAddress.class, MemoryAddress.class, MemorySegment.class);
	static final MethodType MT_MemSegmt_MemSegmt_MemSegmt = methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class);
	static final MethodType MT_MemSegmt = methodType(MemorySegment.class);

	public static final MethodHandle MH_add2BoolsWithOr;
	public static final MethodHandle MH_addBoolAndBoolFromPointerWithOr;
	public static final MethodHandle MH_addBoolAndBoolFromPtrWithOr_RetPtr;
	public static final MethodHandle MH_addBoolAndBoolFromPtrWithOr_RetArgPtr;
	public static final MethodHandle MH_createNewCharFrom2Chars;
	public static final MethodHandle MH_createNewCharFromCharAndCharFromPointer;
	public static final MethodHandle MH_createNewCharFromCharAndCharFromPtr_RetPtr;
	public static final MethodHandle MH_createNewCharFromCharAndCharFromPtr_RetArgPtr;
	public static final MethodHandle MH_add2Bytes;
	public static final MethodHandle MH_addByteAndByteFromPointer;
	public static final MethodHandle MH_addByteAndByteFromPtr_RetPtr;
	public static final MethodHandle MH_addByteAndByteFromPtr_RetArgPtr;
	public static final MethodHandle MH_add2Shorts;
	public static final MethodHandle MH_addShortAndShortFromPointer;
	public static final MethodHandle MH_addShortAndShortFromPtr_RetPtr;
	public static final MethodHandle MH_addShortAndShortFromPtr_RetArgPtr;
	public static final MethodHandle MH_add2Ints;
	public static final MethodHandle MH_addIntAndIntFromPointer;
	public static final MethodHandle MH_addIntAndIntFromPtr_RetPtr;
	public static final MethodHandle MH_addIntAndIntFromPtr_RetArgPtr;
	public static final MethodHandle MH_add3Ints;
	public static final MethodHandle MH_addIntAndChar;
	public static final MethodHandle MH_add2IntsReturnVoid;
	public static final MethodHandle MH_add2Longs;
	public static final MethodHandle MH_addLongAndLongFromPointer;
	public static final MethodHandle MH_addLongAndLongFromPtr_RetPtr;
	public static final MethodHandle MH_addLongAndLongFromPtr_RetArgPtr;
	public static final MethodHandle MH_add2Floats;
	public static final MethodHandle MH_addFloatAndFloatFromPointer;
	public static final MethodHandle MH_addFloatAndFloatFromPtr_RetPtr;
	public static final MethodHandle MH_addFloatAndFloatFromPtr_RetArgPtr;
	public static final MethodHandle MH_add2Doubles;
	public static final MethodHandle MH_addDoubleAndDoubleFromPointer;
	public static final MethodHandle MH_addDoubleAndDoubleFromPtr_RetPtr;
	public static final MethodHandle MH_addDoubleAndDoubleFromPtr_RetArgPtr;
	public static final MethodHandle MH_compare;

	public static final MethodHandle MH_addBoolAndBoolsFromStructWithXor;
	public static final MethodHandle MH_addBoolAnd20BoolsFromStructWithXor;
	public static final MethodHandle MH_addBoolFromPointerAndBoolsFromStructWithXor;
	public static final MethodHandle MH_addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer;
	public static final MethodHandle MH_addBoolAndBoolsFromStructPointerWithXor;
	public static final MethodHandle MH_addBoolAndBoolsFromNestedStructWithXor;
	public static final MethodHandle MH_addBoolAndBoolsFromNestedStructWithXor_reverseOrder;
	public static final MethodHandle MH_addBoolAndBoolsFromStructWithNestedBoolArray;
	public static final MethodHandle MH_addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder;
	public static final MethodHandle MH_addBoolAndBoolsFromStructWithNestedStructArray;
	public static final MethodHandle MH_addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder;
	public static final MethodHandle MH_add2BoolStructsWithXor_returnStruct;
	public static final MethodHandle MH_add2BoolStructsWithXor_returnStructPointer;
	public static final MethodHandle MH_add3BoolStructsWithXor_returnStruct;

	public static final MethodHandle MH_addByteAndBytesFromStruct;
	public static final MethodHandle MH_addByteAnd20BytesFromStruct;
	public static final MethodHandle MH_addByteFromPointerAndBytesFromStruct;
	public static final MethodHandle MH_addByteFromPointerAndBytesFromStruct_returnBytePointer;
	public static final MethodHandle MH_addByteAndBytesFromStructPointer;
	public static final MethodHandle MH_addByteAndBytesFromNestedStruct;
	public static final MethodHandle MH_addByteAndBytesFromNestedStruct_reverseOrder;
	public static final MethodHandle MH_addByteAndBytesFromStructWithNestedByteArray;
	public static final MethodHandle MH_addByteAndBytesFromStructWithNestedByteArray_reverseOrder;
	public static final MethodHandle MH_addByteAndBytesFromStructWithNestedStructArray;
	public static final MethodHandle MH_addByteAndBytesFromStructWithNestedStructArray_reverseOrder;
	public static final MethodHandle MH_add1ByteStructs_returnStruct;
	public static final MethodHandle MH_add2ByteStructs_returnStruct;
	public static final MethodHandle MH_add2ByteStructs_returnStructPointer;
	public static final MethodHandle MH_add3ByteStructs_returnStruct;

	public static final MethodHandle MH_addCharAndCharsFromStruct;
	public static final MethodHandle MH_addCharAnd10CharsFromStruct;
	public static final MethodHandle MH_addCharFromPointerAndCharsFromStruct;
	public static final MethodHandle MH_addCharFromPointerAndCharsFromStruct_returnCharPointer;
	public static final MethodHandle MH_addCharAndCharsFromStructPointer;
	public static final MethodHandle MH_addCharAndCharsFromNestedStruct;
	public static final MethodHandle MH_addCharAndCharsFromNestedStruct_reverseOrder;
	public static final MethodHandle MH_addCharAndCharsFromStructWithNestedCharArray;
	public static final MethodHandle MH_addCharAndCharsFromStructWithNestedCharArray_reverseOrder;
	public static final MethodHandle MH_addCharAndCharsFromStructWithNestedStructArray;
	public static final MethodHandle MH_addCharAndCharsFromStructWithNestedStructArray_reverseOrder;
	public static final MethodHandle MH_add2CharStructs_returnStruct;
	public static final MethodHandle MH_add2CharStructs_returnStructPointer;
	public static final MethodHandle MH_add3CharStructs_returnStruct;

	public static final MethodHandle MH_addShortAndShortsFromStruct;
	public static final MethodHandle MH_addShortAnd10ShortsFromStruct;
	public static final MethodHandle MH_addShortFromPointerAndShortsFromStruct;
	public static final MethodHandle MH_addShortFromPointerAndShortsFromStruct_returnShortPointer;
	public static final MethodHandle MH_addShortAndShortsFromStructPointer;
	public static final MethodHandle MH_addShortAndShortsFromNestedStruct;
	public static final MethodHandle MH_addShortAndShortsFromNestedStruct_reverseOrder;
	public static final MethodHandle MH_addShortAndShortsFromStructWithNestedShortArray;
	public static final MethodHandle MH_addShortAndShortsFromStructWithNestedShortArray_reverseOrder;
	public static final MethodHandle MH_addShortAndShortsFromStructWithNestedStructArray;
	public static final MethodHandle MH_addShortAndShortsFromStructWithNestedStructArray_reverseOrder;
	public static final MethodHandle MH_add2ShortStructs_returnStruct;
	public static final MethodHandle MH_add2ShortStructs_returnStructPointer;
	public static final MethodHandle MH_add3ShortStructs_returnStruct;

	public static final MethodHandle MH_addIntAndIntsFromStruct;
	public static final MethodHandle MH_addIntAnd5IntsFromStruct;
	public static final MethodHandle MH_addIntFromPointerAndIntsFromStruct;
	public static final MethodHandle MH_addIntFromPointerAndIntsFromStruct_returnIntPointer;
	public static final MethodHandle MH_addIntAndIntsFromStructPointer;
	public static final MethodHandle MH_addIntAndIntsFromNestedStruct;
	public static final MethodHandle MH_addIntAndIntsFromNestedStruct_reverseOrder;
	public static final MethodHandle MH_addIntAndIntsFromStructWithNestedIntArray;
	public static final MethodHandle MH_addIntAndIntsFromStructWithNestedIntArray_reverseOrder;
	public static final MethodHandle MH_addIntAndIntsFromStructWithNestedStructArray;
	public static final MethodHandle MH_addIntAndIntsFromStructWithNestedStructArray_reverseOrder;
	public static final MethodHandle MH_add2IntStructs_returnStruct;
	public static final MethodHandle MH_add2IntStructs_returnStruct_throwException;
	public static final MethodHandle MH_add2IntStructs_returnStruct_nestedUpcall;
	public static final MethodHandle MH_add2IntStructs_returnStruct_nullValue;
	public static final MethodHandle MH_add2IntStructs_returnStruct_heapSegmt;
	public static final MethodHandle MH_add2IntStructs_returnStructPointer;
	public static final MethodHandle MH_add2IntStructs_returnStructPointer_nullValue;
	public static final MethodHandle MH_add2IntStructs_returnStructPointer_nullAddr;
	public static final MethodHandle MH_add2IntStructs_returnStructPointer_heapSegmt;
	public static final MethodHandle MH_add3IntStructs_returnStruct;

	public static final MethodHandle MH_addLongAndLongsFromStruct;
	public static final MethodHandle MH_addLongFromPointerAndLongsFromStruct;
	public static final MethodHandle MH_addLongFromPointerAndLongsFromStruct_returnLongPointer;
	public static final MethodHandle MH_addLongAndLongsFromStructPointer;
	public static final MethodHandle MH_addLongAndLongsFromNestedStruct;
	public static final MethodHandle MH_addLongAndLongsFromNestedStruct_reverseOrder;
	public static final MethodHandle MH_addLongAndLongsFromStructWithNestedLongArray;
	public static final MethodHandle MH_addLongAndLongsFromStructWithNestedLongArray_reverseOrder;
	public static final MethodHandle MH_addLongAndLongsFromStructWithNestedStructArray;
	public static final MethodHandle MH_addLongAndLongsFromStructWithNestedStructArray_reverseOrder;
	public static final MethodHandle MH_add2LongStructs_returnStruct;
	public static final MethodHandle MH_add2LongStructs_returnStructPointer;
	public static final MethodHandle MH_add3LongStructs_returnStruct;

	public static final MethodHandle MH_addFloatAndFloatsFromStruct;
	public static final MethodHandle MH_addFloatAnd5FloatsFromStruct;
	public static final MethodHandle MH_addFloatFromPointerAndFloatsFromStruct;
	public static final MethodHandle MH_addFloatFromPointerAndFloatsFromStruct_returnFloatPointer;
	public static final MethodHandle MH_addFloatAndFloatsFromStructPointer;
	public static final MethodHandle MH_addFloatAndFloatsFromNestedStruct;
	public static final MethodHandle MH_addFloatAndFloatsFromNestedStruct_reverseOrder;
	public static final MethodHandle MH_addFloatAndFloatsFromStructWithNestedFloatArray;
	public static final MethodHandle MH_addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder;
	public static final MethodHandle MH_addFloatAndFloatsFromStructWithNestedStructArray;
	public static final MethodHandle MH_addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder;
	public static final MethodHandle MH_add2FloatStructs_returnStruct;
	public static final MethodHandle MH_add2FloatStructs_returnStructPointer;
	public static final MethodHandle MH_add3FloatStructs_returnStruct;

	public static final MethodHandle MH_addDoubleAndDoublesFromStruct;
	public static final MethodHandle MH_addDoubleFromPointerAndDoublesFromStruct;
	public static final MethodHandle MH_addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer;
	public static final MethodHandle MH_addDoubleAndDoublesFromStructPointer;
	public static final MethodHandle MH_addDoubleAndDoublesFromNestedStruct;
	public static final MethodHandle MH_addDoubleAndDoublesFromNestedStruct_reverseOrder;
	public static final MethodHandle MH_addDoubleAndDoublesFromStructWithNestedDoubleArray;
	public static final MethodHandle MH_addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder;
	public static final MethodHandle MH_addDoubleAndDoublesFromStructWithNestedStructArray;
	public static final MethodHandle MH_addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder;
	public static final MethodHandle MH_add2DoubleStructs_returnStruct;
	public static final MethodHandle MH_add2DoubleStructs_returnStructPointer;
	public static final MethodHandle MH_add3DoubleStructs_returnStruct;

	public static final MethodHandle MH_addIntAndIntShortFromStruct;
	public static final MethodHandle MH_addIntAndShortIntFromStruct;
	public static final MethodHandle MH_addIntAndIntLongFromStruct;
	public static final MethodHandle MH_addIntAndLongIntFromStruct;
	public static final MethodHandle MH_addDoubleAndIntDoubleFromStruct;
	public static final MethodHandle MH_addDoubleAndDoubleIntFromStruct;
	public static final MethodHandle MH_addDoubleAndFloatDoubleFromStruct;
	public static final MethodHandle MH_addDoubleAndDoubleFloatFromStruct;
	public static final MethodHandle MH_addDoubleAnd2FloatsDoubleFromStruct;
	public static final MethodHandle MH_addDoubleAndDouble2FloatsFromStruct;
	public static final MethodHandle MH_addFloatAndInt2FloatsFromStruct;
	public static final MethodHandle MH_addFloatAndFloatIntFloatFromStruct;
	public static final MethodHandle MH_addDoubleAndIntFloatDoubleFromStruct;
	public static final MethodHandle MH_addDoubleAndFloatIntDoubleFromStruct;
	public static final MethodHandle MH_addDoubleAndLongDoubleFromStruct;
	public static final MethodHandle MH_addFloatAndInt3FloatsFromStruct;
	public static final MethodHandle MH_addLongAndLong2FloatsFromStruct;
	public static final MethodHandle MH_addFloatAnd3FloatsIntFromStruct;
	public static final MethodHandle MH_addLongAndFloatLongFromStruct;
	public static final MethodHandle MH_addDoubleAndDoubleFloatIntFromStruct;
	public static final MethodHandle MH_addDoubleAndDoubleLongFromStruct;
	public static final MethodHandle MH_addLongAnd2FloatsLongFromStruct;
	public static final MethodHandle MH_addShortAnd3ShortsCharFromStruct;
	public static final MethodHandle MH_addFloatAndIntFloatIntFloatFromStruct;
	public static final MethodHandle MH_addDoubleAndIntDoubleFloatFromStruct;
	public static final MethodHandle MH_addDoubleAndFloatDoubleIntFromStruct;
	public static final MethodHandle MH_addDoubleAndIntDoubleIntFromStruct;
	public static final MethodHandle MH_addDoubleAndFloatDoubleFloatFromStruct;
	public static final MethodHandle MH_addDoubleAndIntDoubleLongFromStruct;
	public static final MethodHandle MH_return254BytesFromStruct;
	public static final MethodHandle MH_return4KBytesFromStruct;

	public static final MethodHandle MH_addNegBytesFromStruct;
	public static final MethodHandle MH_addNegShortsFromStruct;

	private static CLinker clinker = CLinker.getInstance();

	static {
		System.loadLibrary("clinkerffitests");

		try {
			MH_add2BoolsWithOr = lookup.findStatic(UpcallMethodHandles.class, "add2BoolsWithOr", methodType(boolean.class, boolean.class, boolean.class)); //$NON-NLS-1$
			MH_addBoolAndBoolFromPointerWithOr = lookup.findStatic(UpcallMethodHandles.class, "addBoolAndBoolFromPointerWithOr", methodType(boolean.class, boolean.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addBoolAndBoolFromPtrWithOr_RetPtr = lookup.findStatic(UpcallMethodHandles.class, "addBoolAndBoolFromPtrWithOr_RetPtr", MT_MemAddr_Bool_MemAddr); //$NON-NLS-1$
			MH_addBoolAndBoolFromPtrWithOr_RetArgPtr = lookup.findStatic(UpcallMethodHandles.class, "addBoolAndBoolFromPtrWithOr_RetArgPtr", MT_MemAddr_Bool_MemAddr); //$NON-NLS-1$

			MH_createNewCharFrom2Chars = lookup.findStatic(UpcallMethodHandles.class, "createNewCharFrom2Chars", methodType(char.class, char.class, char.class)); //$NON-NLS-1$
			MH_createNewCharFromCharAndCharFromPointer = lookup.findStatic(UpcallMethodHandles.class, "createNewCharFromCharAndCharFromPointer", methodType(char.class, MemoryAddress.class, char.class)); //$NON-NLS-1$
			MH_createNewCharFromCharAndCharFromPtr_RetPtr = lookup.findStatic(UpcallMethodHandles.class, "createNewCharFromCharAndCharFromPtr_RetPtr", MT_MemAddr_MemAddr_Char); //$NON-NLS-1$
			MH_createNewCharFromCharAndCharFromPtr_RetArgPtr = lookup.findStatic(UpcallMethodHandles.class, "createNewCharFromCharAndCharFromPtr_RetArgPtr", MT_MemAddr_MemAddr_Char); //$NON-NLS-1$

			MH_add2Bytes = lookup.findStatic(UpcallMethodHandles.class, "add2Bytes", methodType(byte.class, byte.class, byte.class)); //$NON-NLS-1$
			MH_addByteAndByteFromPointer = lookup.findStatic(UpcallMethodHandles.class, "addByteAndByteFromPointer", methodType(byte.class, byte.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addByteAndByteFromPtr_RetPtr = lookup.findStatic(UpcallMethodHandles.class, "addByteAndByteFromPtr_RetPtr", MT_MemAddr_Byte_MemAddr); //$NON-NLS-1$
			MH_addByteAndByteFromPtr_RetArgPtr = lookup.findStatic(UpcallMethodHandles.class, "addByteAndByteFromPtr_RetArgPtr", MT_MemAddr_Byte_MemAddr); //$NON-NLS-1$

			MH_add2Shorts = lookup.findStatic(UpcallMethodHandles.class, "add2Shorts", methodType(short.class, short.class, short.class)); //$NON-NLS-1$
			MH_addShortAndShortFromPointer = lookup.findStatic(UpcallMethodHandles.class, "addShortAndShortFromPointer", methodType(short.class, MemoryAddress.class, short.class)); //$NON-NLS-1$
			MH_addShortAndShortFromPtr_RetPtr = lookup.findStatic(UpcallMethodHandles.class, "addShortAndShortFromPtr_RetPtr", MT_MemAddr_MemAddr_Short); //$NON-NLS-1$
			MH_addShortAndShortFromPtr_RetArgPtr = lookup.findStatic(UpcallMethodHandles.class, "addShortAndShortFromPtr_RetArgPtr", MT_MemAddr_MemAddr_Short); //$NON-NLS-1$

			MH_add2Ints = lookup.findStatic(UpcallMethodHandles.class, "add2Ints", methodType(int.class, int.class, int.class)); //$NON-NLS-1$
			MH_addIntAndIntFromPointer = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntFromPointer", methodType(int.class, int.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addIntAndIntFromPtr_RetPtr = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntFromPtr_RetPtr", MT_MemAddr_Int_MemAddr); //$NON-NLS-1$
			MH_addIntAndIntFromPtr_RetArgPtr = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntFromPtr_RetArgPtr", MT_MemAddr_Int_MemAddr); //$NON-NLS-1$
			MH_add3Ints = lookup.findStatic(UpcallMethodHandles.class, "add3Ints", methodType(int.class, int.class, int.class, int.class)); //$NON-NLS-1$
			MH_addIntAndChar = lookup.findStatic(UpcallMethodHandles.class, "addIntAndChar", methodType(int.class, int.class, char.class)); //$NON-NLS-1$
			MH_add2IntsReturnVoid = lookup.findStatic(UpcallMethodHandles.class, "add2IntsReturnVoid", methodType(void.class, int.class, int.class)); //$NON-NLS-1$

			MH_add2Longs = lookup.findStatic(UpcallMethodHandles.class, "add2Longs", methodType(long.class, long.class, long.class)); //$NON-NLS-1$
			MH_addLongAndLongFromPointer = lookup.findStatic(UpcallMethodHandles.class, "addLongAndLongFromPointer", methodType(long.class, MemoryAddress.class, long.class)); //$NON-NLS-1$
			MH_addLongAndLongFromPtr_RetPtr = lookup.findStatic(UpcallMethodHandles.class, "addLongAndLongFromPtr_RetPtr", MT_MemAddr_MemAddr_Long); //$NON-NLS-1$
			MH_addLongAndLongFromPtr_RetArgPtr = lookup.findStatic(UpcallMethodHandles.class, "addLongAndLongFromPtr_RetArgPtr", MT_MemAddr_MemAddr_Long); //$NON-NLS-1$

			MH_add2Floats = lookup.findStatic(UpcallMethodHandles.class, "add2Floats", methodType(float.class, float.class, float.class)); //$NON-NLS-1$
			MH_addFloatAndFloatFromPointer = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndFloatFromPointer", methodType(float.class, float.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addFloatAndFloatFromPtr_RetPtr = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndFloatFromPtr_RetPtr", MT_MemAddr_Float_MemAddr); //$NON-NLS-1$
			MH_addFloatAndFloatFromPtr_RetArgPtr = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndFloatFromPtr_RetArgPtr", MT_MemAddr_Float_MemAddr); //$NON-NLS-1$

			MH_add2Doubles = lookup.findStatic(UpcallMethodHandles.class, "add2Doubles", methodType(double.class, double.class, double.class)); //$NON-NLS-1$
			MH_addDoubleAndDoubleFromPointer = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoubleFromPointer", methodType(double.class, MemoryAddress.class, double.class)); //$NON-NLS-1$
			MH_addDoubleAndDoubleFromPtr_RetPtr = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoubleFromPtr_RetPtr", MT_MemAddr_MemAddr_Double); //$NON-NLS-1$
			MH_addDoubleAndDoubleFromPtr_RetArgPtr = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoubleFromPtr_RetArgPtr", MT_MemAddr_MemAddr_Double); //$NON-NLS-1$

			MH_compare = lookup.findStatic(UpcallMethodHandles.class, "compare", methodType(int.class, MemoryAddress.class, MemoryAddress.class)); //$NON-NLS-1$

			MH_addBoolAndBoolsFromStructWithXor = lookup.findStatic(UpcallMethodHandles.class, "addBoolAndBoolsFromStructWithXor", MT_Bool_Bool_MemSegmt); //$NON-NLS-1$
			MH_addBoolAnd20BoolsFromStructWithXor = lookup.findStatic(UpcallMethodHandles.class, "addBoolAnd20BoolsFromStructWithXor", MT_Bool_Bool_MemSegmt); //$NON-NLS-1$
			MH_addBoolFromPointerAndBoolsFromStructWithXor = lookup.findStatic(UpcallMethodHandles.class, "addBoolFromPointerAndBoolsFromStructWithXor", methodType(boolean.class, MemoryAddress.class, MemorySegment.class)); //$NON-NLS-1$
			MH_addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer = lookup.findStatic(UpcallMethodHandles.class, "addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_addBoolAndBoolsFromStructPointerWithXor = lookup.findStatic(UpcallMethodHandles.class, "addBoolAndBoolsFromStructPointerWithXor", methodType(boolean.class, boolean.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addBoolAndBoolsFromNestedStructWithXor = lookup.findStatic(UpcallMethodHandles.class, "addBoolAndBoolsFromNestedStructWithXor", MT_Bool_Bool_MemSegmt); //$NON-NLS-1$
			MH_addBoolAndBoolsFromNestedStructWithXor_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addBoolAndBoolsFromNestedStructWithXor_reverseOrder", MT_Bool_Bool_MemSegmt); //$NON-NLS-1$
			MH_addBoolAndBoolsFromStructWithNestedBoolArray = lookup.findStatic(UpcallMethodHandles.class, "addBoolAndBoolsFromStructWithNestedBoolArray", MT_Bool_Bool_MemSegmt); //$NON-NLS-1$
			MH_addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder", MT_Bool_Bool_MemSegmt); //$NON-NLS-1$
			MH_addBoolAndBoolsFromStructWithNestedStructArray = lookup.findStatic(UpcallMethodHandles.class, "addBoolAndBoolsFromStructWithNestedStructArray", MT_Bool_Bool_MemSegmt); //$NON-NLS-1$
			MH_addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder", MT_Bool_Bool_MemSegmt); //$NON-NLS-1$
			MH_add2BoolStructsWithXor_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add2BoolStructsWithXor_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2BoolStructsWithXor_returnStructPointer = lookup.findStatic(UpcallMethodHandles.class, "add2BoolStructsWithXor_returnStructPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_add3BoolStructsWithXor_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add3BoolStructsWithXor_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$

			MH_addByteAndBytesFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addByteAndBytesFromStruct", MT_Byte_Byte_MemSegmt); //$NON-NLS-1$
			MH_addByteAnd20BytesFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addByteAnd20BytesFromStruct", MT_Byte_Byte_MemSegmt); //$NON-NLS-1$
			MH_addByteFromPointerAndBytesFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addByteFromPointerAndBytesFromStruct", methodType(byte.class, MemoryAddress.class, MemorySegment.class)); //$NON-NLS-1$
			MH_addByteFromPointerAndBytesFromStruct_returnBytePointer = lookup.findStatic(UpcallMethodHandles.class, "addByteFromPointerAndBytesFromStruct_returnBytePointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_addByteAndBytesFromStructPointer = lookup.findStatic(UpcallMethodHandles.class, "addByteAndBytesFromStructPointer", methodType(byte.class, byte.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addByteAndBytesFromNestedStruct = lookup.findStatic(UpcallMethodHandles.class, "addByteAndBytesFromNestedStruct", MT_Byte_Byte_MemSegmt); //$NON-NLS-1$
			MH_addByteAndBytesFromNestedStruct_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addByteAndBytesFromNestedStruct_reverseOrder", MT_Byte_Byte_MemSegmt); //$NON-NLS-1$
			MH_addByteAndBytesFromStructWithNestedByteArray = lookup.findStatic(UpcallMethodHandles.class, "addByteAndBytesFromStructWithNestedByteArray", MT_Byte_Byte_MemSegmt); //$NON-NLS-1$
			MH_addByteAndBytesFromStructWithNestedByteArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addByteAndBytesFromStructWithNestedByteArray_reverseOrder", MT_Byte_Byte_MemSegmt); //$NON-NLS-1$
			MH_addByteAndBytesFromStructWithNestedStructArray = lookup.findStatic(UpcallMethodHandles.class, "addByteAndBytesFromStructWithNestedStructArray", MT_Byte_Byte_MemSegmt); //$NON-NLS-1$
			MH_addByteAndBytesFromStructWithNestedStructArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addByteAndBytesFromStructWithNestedStructArray_reverseOrder", MT_Byte_Byte_MemSegmt); //$NON-NLS-1$
			MH_add1ByteStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add1ByteStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2ByteStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add2ByteStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2ByteStructs_returnStructPointer = lookup.findStatic(UpcallMethodHandles.class, "add2ByteStructs_returnStructPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_add3ByteStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add3ByteStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$

			MH_addCharAndCharsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addCharAndCharsFromStruct", MT_Char_Char_MemSegmt); //$NON-NLS-1$
			MH_addCharAnd10CharsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addCharAnd10CharsFromStruct", MT_Char_Char_MemSegmt); //$NON-NLS-1$
			MH_addCharFromPointerAndCharsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addCharFromPointerAndCharsFromStruct", methodType(char.class, MemoryAddress.class, MemorySegment.class)); //$NON-NLS-1$
			MH_addCharFromPointerAndCharsFromStruct_returnCharPointer = lookup.findStatic(UpcallMethodHandles.class, "addCharFromPointerAndCharsFromStruct_returnCharPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_addCharAndCharsFromStructPointer = lookup.findStatic(UpcallMethodHandles.class, "addCharAndCharsFromStructPointer", methodType(char.class, char.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addCharAndCharsFromNestedStruct = lookup.findStatic(UpcallMethodHandles.class, "addCharAndCharsFromNestedStruct", MT_Char_Char_MemSegmt); //$NON-NLS-1$
			MH_addCharAndCharsFromNestedStruct_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addCharAndCharsFromNestedStruct_reverseOrder", MT_Char_Char_MemSegmt); //$NON-NLS-1$
			MH_addCharAndCharsFromStructWithNestedCharArray = lookup.findStatic(UpcallMethodHandles.class, "addCharAndCharsFromStructWithNestedCharArray", MT_Char_Char_MemSegmt); //$NON-NLS-1$
			MH_addCharAndCharsFromStructWithNestedCharArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addCharAndCharsFromStructWithNestedCharArray_reverseOrder", MT_Char_Char_MemSegmt); //$NON-NLS-1$
			MH_addCharAndCharsFromStructWithNestedStructArray = lookup.findStatic(UpcallMethodHandles.class, "addCharAndCharsFromStructWithNestedStructArray", MT_Char_Char_MemSegmt); //$NON-NLS-1$
			MH_addCharAndCharsFromStructWithNestedStructArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addCharAndCharsFromStructWithNestedStructArray_reverseOrder", MT_Char_Char_MemSegmt); //$NON-NLS-1$
			MH_add2CharStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add2CharStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2CharStructs_returnStructPointer = lookup.findStatic(UpcallMethodHandles.class, "add2CharStructs_returnStructPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_add3CharStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add3CharStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$

			MH_addShortAndShortsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addShortAndShortsFromStruct", MT_Short_Short_MemSegmt); //$NON-NLS-1$
			MH_addShortAnd10ShortsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addShortAnd10ShortsFromStruct", MT_Short_Short_MemSegmt); //$NON-NLS-1$
			MH_addShortFromPointerAndShortsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addShortFromPointerAndShortsFromStruct", methodType(short.class, MemoryAddress.class, MemorySegment.class)); //$NON-NLS-1$
			MH_addShortFromPointerAndShortsFromStruct_returnShortPointer = lookup.findStatic(UpcallMethodHandles.class, "addShortFromPointerAndShortsFromStruct_returnShortPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_addShortAndShortsFromStructPointer = lookup.findStatic(UpcallMethodHandles.class, "addShortAndShortsFromStructPointer", methodType(short.class, short.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addShortAndShortsFromNestedStruct = lookup.findStatic(UpcallMethodHandles.class, "addShortAndShortsFromNestedStruct", MT_Short_Short_MemSegmt); //$NON-NLS-1$
			MH_addShortAndShortsFromNestedStruct_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addShortAndShortsFromNestedStruct_reverseOrder", MT_Short_Short_MemSegmt); //$NON-NLS-1$
			MH_addShortAndShortsFromStructWithNestedShortArray = lookup.findStatic(UpcallMethodHandles.class, "addShortAndShortsFromStructWithNestedShortArray", MT_Short_Short_MemSegmt); //$NON-NLS-1$
			MH_addShortAndShortsFromStructWithNestedShortArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addShortAndShortsFromStructWithNestedShortArray_reverseOrder", MT_Short_Short_MemSegmt); //$NON-NLS-1$
			MH_addShortAndShortsFromStructWithNestedStructArray = lookup.findStatic(UpcallMethodHandles.class, "addShortAndShortsFromStructWithNestedStructArray", MT_Short_Short_MemSegmt); //$NON-NLS-1$
			MH_addShortAndShortsFromStructWithNestedStructArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addShortAndShortsFromStructWithNestedStructArray_reverseOrder", MT_Short_Short_MemSegmt); //$NON-NLS-1$
			MH_add2ShortStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add2ShortStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2ShortStructs_returnStructPointer = lookup.findStatic(UpcallMethodHandles.class, "add2ShortStructs_returnStructPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_add3ShortStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add3ShortStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$

			MH_addIntAndIntsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntsFromStruct", MT_Int_Int_MemSegmt); //$NON-NLS-1$
			MH_addIntAnd5IntsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addIntAnd5IntsFromStruct", MT_Int_Int_MemSegmt); //$NON-NLS-1$
			MH_addIntFromPointerAndIntsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addIntFromPointerAndIntsFromStruct", methodType(int.class, MemoryAddress.class, MemorySegment.class)); //$NON-NLS-1$
			MH_addIntFromPointerAndIntsFromStruct_returnIntPointer = lookup.findStatic(UpcallMethodHandles.class, "addIntFromPointerAndIntsFromStruct_returnIntPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_addIntAndIntsFromStructPointer = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntsFromStructPointer", methodType(int.class, int.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addIntAndIntsFromNestedStruct = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntsFromNestedStruct", MT_Int_Int_MemSegmt); //$NON-NLS-1$
			MH_addIntAndIntsFromNestedStruct_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntsFromNestedStruct_reverseOrder", MT_Int_Int_MemSegmt); //$NON-NLS-1$
			MH_addIntAndIntsFromStructWithNestedIntArray = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntsFromStructWithNestedIntArray", MT_Int_Int_MemSegmt); //$NON-NLS-1$
			MH_addIntAndIntsFromStructWithNestedIntArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntsFromStructWithNestedIntArray_reverseOrder", MT_Int_Int_MemSegmt); //$NON-NLS-1$
			MH_addIntAndIntsFromStructWithNestedStructArray = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntsFromStructWithNestedStructArray", MT_Int_Int_MemSegmt); //$NON-NLS-1$
			MH_addIntAndIntsFromStructWithNestedStructArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntsFromStructWithNestedStructArray_reverseOrder", MT_Int_Int_MemSegmt); //$NON-NLS-1$
			MH_add2IntStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add2IntStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2IntStructs_returnStruct_throwException = lookup.findStatic(UpcallMethodHandles.class, "add2IntStructs_returnStruct_throwException", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2IntStructs_returnStruct_nestedUpcall = lookup.findStatic(UpcallMethodHandles.class, "add2IntStructs_returnStruct_nestedUpcall", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2IntStructs_returnStruct_nullValue = lookup.findStatic(UpcallMethodHandles.class, "add2IntStructs_returnStruct_nullValue", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2IntStructs_returnStruct_heapSegmt = lookup.findStatic(UpcallMethodHandles.class, "add2IntStructs_returnStruct_heapSegmt", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2IntStructs_returnStructPointer = lookup.findStatic(UpcallMethodHandles.class, "add2IntStructs_returnStructPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_add2IntStructs_returnStructPointer_nullValue = lookup.findStatic(UpcallMethodHandles.class, "add2IntStructs_returnStructPointer_nullValue", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_add2IntStructs_returnStructPointer_nullAddr = lookup.findStatic(UpcallMethodHandles.class, "add2IntStructs_returnStructPointer_nullAddr", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_add2IntStructs_returnStructPointer_heapSegmt = lookup.findStatic(UpcallMethodHandles.class, "add2IntStructs_returnStructPointer_heapSegmt", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_add3IntStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add3IntStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$

			MH_addLongAndLongsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addLongAndLongsFromStruct", MT_Long_Long_MemSegmt); //$NON-NLS-1$
			MH_addLongFromPointerAndLongsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addLongFromPointerAndLongsFromStruct", methodType(long.class, MemoryAddress.class, MemorySegment.class)); //$NON-NLS-1$
			MH_addLongFromPointerAndLongsFromStruct_returnLongPointer = lookup.findStatic(UpcallMethodHandles.class, "addLongFromPointerAndLongsFromStruct_returnLongPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_addLongAndLongsFromStructPointer = lookup.findStatic(UpcallMethodHandles.class, "addLongAndLongsFromStructPointer", methodType(long.class, long.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addLongAndLongsFromNestedStruct = lookup.findStatic(UpcallMethodHandles.class, "addLongAndLongsFromNestedStruct", MT_Long_Long_MemSegmt); //$NON-NLS-1$
			MH_addLongAndLongsFromNestedStruct_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addLongAndLongsFromNestedStruct_reverseOrder", MT_Long_Long_MemSegmt); //$NON-NLS-1$
			MH_addLongAndLongsFromStructWithNestedLongArray = lookup.findStatic(UpcallMethodHandles.class, "addLongAndLongsFromStructWithNestedLongArray", MT_Long_Long_MemSegmt); //$NON-NLS-1$
			MH_addLongAndLongsFromStructWithNestedLongArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addLongAndLongsFromStructWithNestedLongArray_reverseOrder", MT_Long_Long_MemSegmt); //$NON-NLS-1$
			MH_addLongAndLongsFromStructWithNestedStructArray = lookup.findStatic(UpcallMethodHandles.class, "addLongAndLongsFromStructWithNestedStructArray", MT_Long_Long_MemSegmt); //$NON-NLS-1$
			MH_addLongAndLongsFromStructWithNestedStructArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addLongAndLongsFromStructWithNestedStructArray_reverseOrder", MT_Long_Long_MemSegmt); //$NON-NLS-1$
			MH_add2LongStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add2LongStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2LongStructs_returnStructPointer = lookup.findStatic(UpcallMethodHandles.class, "add2LongStructs_returnStructPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_add3LongStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add3LongStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$

			MH_addFloatAndFloatsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndFloatsFromStruct", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_addFloatAnd5FloatsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addFloatAnd5FloatsFromStruct", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_addFloatFromPointerAndFloatsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addFloatFromPointerAndFloatsFromStruct", methodType(float.class, MemoryAddress.class, MemorySegment.class)); //$NON-NLS-1$
			MH_addFloatFromPointerAndFloatsFromStruct_returnFloatPointer = lookup.findStatic(UpcallMethodHandles.class, "addFloatFromPointerAndFloatsFromStruct_returnFloatPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_addFloatAndFloatsFromStructPointer = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndFloatsFromStructPointer", methodType(float.class, float.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addFloatAndFloatsFromNestedStruct = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndFloatsFromNestedStruct", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_addFloatAndFloatsFromNestedStruct_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndFloatsFromNestedStruct_reverseOrder", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_addFloatAndFloatsFromStructWithNestedFloatArray = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndFloatsFromStructWithNestedFloatArray", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_addFloatAndFloatsFromStructWithNestedStructArray = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndFloatsFromStructWithNestedStructArray", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_add2FloatStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add2FloatStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2FloatStructs_returnStructPointer = lookup.findStatic(UpcallMethodHandles.class, "add2FloatStructs_returnStructPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_add3FloatStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add3FloatStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$

			MH_addDoubleAndDoublesFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoublesFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleFromPointerAndDoublesFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleFromPointerAndDoublesFromStruct", methodType(double.class, MemoryAddress.class, MemorySegment.class)); //$NON-NLS-1$
			MH_addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer = lookup.findStatic(UpcallMethodHandles.class, "addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndDoublesFromStructPointer = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoublesFromStructPointer", methodType(double.class, double.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addDoubleAndDoublesFromNestedStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoublesFromNestedStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndDoublesFromNestedStruct_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoublesFromNestedStruct_reverseOrder", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndDoublesFromStructWithNestedDoubleArray = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoublesFromStructWithNestedDoubleArray", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndDoublesFromStructWithNestedStructArray = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoublesFromStructWithNestedStructArray", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_add2DoubleStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add2DoubleStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$
			MH_add2DoubleStructs_returnStructPointer = lookup.findStatic(UpcallMethodHandles.class, "add2DoubleStructs_returnStructPointer", MT_MemAddr_MemAddr_MemSegmt); //$NON-NLS-1$
			MH_add3DoubleStructs_returnStruct = lookup.findStatic(UpcallMethodHandles.class, "add3DoubleStructs_returnStruct", MT_MemSegmt_MemSegmt_MemSegmt); //$NON-NLS-1$

			MH_addIntAndIntShortFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntShortFromStruct", MT_Int_Int_MemSegmt); //$NON-NLS-1$
			MH_addIntAndShortIntFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addIntAndShortIntFromStruct", MT_Int_Int_MemSegmt); //$NON-NLS-1$
			MH_addIntAndIntLongFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addIntAndIntLongFromStruct", MT_Long_Int_MemSegmt); //$NON-NLS-1$
			MH_addIntAndLongIntFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addIntAndLongIntFromStruct", MT_Long_Int_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndIntDoubleFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndIntDoubleFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndDoubleIntFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoubleIntFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndFloatDoubleFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndFloatDoubleFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndDoubleFloatFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoubleFloatFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAnd2FloatsDoubleFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAnd2FloatsDoubleFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndDouble2FloatsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDouble2FloatsFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addFloatAndInt2FloatsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndInt2FloatsFromStruct", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_addFloatAndFloatIntFloatFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndFloatIntFloatFromStruct", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndIntFloatDoubleFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndIntFloatDoubleFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndFloatIntDoubleFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndFloatIntDoubleFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndLongDoubleFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndLongDoubleFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addFloatAndInt3FloatsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndInt3FloatsFromStruct", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_addLongAndLong2FloatsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addLongAndLong2FloatsFromStruct", MT_Long_Long_MemSegmt); //$NON-NLS-1$
			MH_addFloatAnd3FloatsIntFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addFloatAnd3FloatsIntFromStruct", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_addLongAndFloatLongFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addLongAndFloatLongFromStruct", MT_Long_Long_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndDoubleFloatIntFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoubleFloatIntFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndDoubleLongFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndDoubleLongFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addLongAnd2FloatsLongFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addLongAnd2FloatsLongFromStruct", MT_Long_Long_MemSegmt); //$NON-NLS-1$
			MH_addShortAnd3ShortsCharFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addShortAnd3ShortsCharFromStruct", MT_Short_Short_MemSegmt); //$NON-NLS-1$
			MH_addFloatAndIntFloatIntFloatFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addFloatAndIntFloatIntFloatFromStruct", MT_Float_Float_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndIntDoubleFloatFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndIntDoubleFloatFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndFloatDoubleIntFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndFloatDoubleIntFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndIntDoubleIntFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndIntDoubleIntFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndFloatDoubleFloatFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndFloatDoubleFloatFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_addDoubleAndIntDoubleLongFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addDoubleAndIntDoubleLongFromStruct", MT_Double_Double_MemSegmt); //$NON-NLS-1$
			MH_return254BytesFromStruct = lookup.findStatic(UpcallMethodHandles.class, "return254BytesFromStruct", MT_MemSegmt); //$NON-NLS-1$
			MH_return4KBytesFromStruct = lookup.findStatic(UpcallMethodHandles.class, "return4KBytesFromStruct", MT_MemSegmt); //$NON-NLS-1$

			MH_addNegBytesFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addNegBytesFromStruct", MT_Byte_Byte_MemSegmt.appendParameterTypes(byte.class, byte.class)); //$NON-NLS-1$
			MH_addNegShortsFromStruct = lookup.findStatic(UpcallMethodHandles.class, "addNegShortsFromStruct", MT_Short_Short_MemSegmt.appendParameterTypes(short.class, short.class)); //$NON-NLS-1$

		} catch (IllegalAccessException | NoSuchMethodException e) {
			throw new InternalError(e);
		}
	}
	private static final SymbolLookup nativeLibLookup = SymbolLookup.loaderLookup();

	public static boolean byteToBool(byte value) {
		return (value != 0);
	}

	public static byte boolToByte(boolean value) {
		int intValue = (value == true) ? 1 : 0;
		return (byte)intValue;
	}

	public static boolean add2BoolsWithOr(boolean boolArg1, boolean boolArg2) {
		boolean result = boolArg1 || boolArg2;
		return result;
	}

	public static boolean addBoolAndBoolFromPointerWithOr(boolean boolArg1, MemoryAddress boolArg2Addr) {
		MemorySegment boolArg2Segmt = boolArg2Addr.asSegment(C_CHAR.byteSize(), scope);
		byte boolArg2 = MemoryAccess.getByte(boolArg2Segmt);
		boolean result = boolArg1 || byteToBool(boolArg2);
		return result;
	}

	public static MemoryAddress addBoolAndBoolFromPtrWithOr_RetPtr(boolean boolArg1, MemoryAddress boolArg2Addr) {
		MemorySegment boolArg2Segmt = boolArg2Addr.asSegment(C_CHAR.byteSize(), scope);
		byte boolArg2 = MemoryAccess.getByte(boolArg2Segmt);
		boolean result = boolArg1 || byteToBool(boolArg2);
		MemorySegment resultSegmt = MemorySegment.allocateNative(C_CHAR.byteSize(), scope);
		MemoryAccess.setByte(resultSegmt, boolToByte(result));
		return resultSegmt.address();
	}

	public static MemoryAddress addBoolAndBoolFromPtrWithOr_RetArgPtr(boolean boolArg1, MemoryAddress boolArg2Addr) {
		MemorySegment boolArg2Segmt = boolArg2Addr.asSegment(C_CHAR.byteSize(), scope);
		byte boolArg2 = MemoryAccess.getByte(boolArg2Segmt);
		boolean result = boolArg1 || byteToBool(boolArg2);
		MemoryAccess.setByte(boolArg2Segmt, boolToByte(result));
		return boolArg2Addr;
	}

	public static char createNewCharFrom2Chars(char charArg1, char charArg2) {
		int diff = (charArg2 >= charArg1) ? (charArg2 - charArg1) : (charArg1 - charArg2);
		diff = (diff > 5) ? 5 : diff;
		char result = (char)(diff + 'A');
		return result;
	}

	public static char createNewCharFromCharAndCharFromPointer(MemoryAddress charArg1Addr, char charArg2) {
		MemorySegment charArg1Segmt = charArg1Addr.asSegment(C_SHORT.byteSize(), scope);
		short charArg1 = MemoryAccess.getShort(charArg1Segmt);
		int diff = (charArg2 >= charArg1) ? (charArg2 - charArg1) : (charArg1 - charArg2);
		diff = (diff > 5) ? 5 : diff;
		char result = (char)(diff + 'A');
		return result;
	}

	public static MemoryAddress createNewCharFromCharAndCharFromPtr_RetPtr(MemoryAddress charArg1Addr, char charArg2) {
		MemorySegment charArg1Segmt = charArg1Addr.asSegment(C_SHORT.byteSize(), scope);
		short charArg1 = MemoryAccess.getShort(charArg1Segmt);
		int diff = (charArg2 >= charArg1) ? (charArg2 - charArg1) : (charArg1 - charArg2);
		diff = (diff > 5) ? 5 : diff;
		char result = (char)(diff + 'A');
		MemorySegment resultSegmt = MemorySegment.allocateNative(C_SHORT.byteSize(), scope);
		MemoryAccess.setChar(resultSegmt, result);
		return resultSegmt.address();
	}

	public static MemoryAddress createNewCharFromCharAndCharFromPtr_RetArgPtr(MemoryAddress charArg1Addr, char charArg2) {
		MemorySegment charArg1Segmt = charArg1Addr.asSegment(C_SHORT.byteSize(), scope);
		short charArg1 = MemoryAccess.getShort(charArg1Segmt);
		int diff = (charArg2 >= charArg1) ? (charArg2 - charArg1) : (charArg1 - charArg2);
		diff = (diff > 5) ? 5 : diff;
		char result = (char)(diff + 'A');
		MemoryAccess.setChar(charArg1Segmt, result);
		return charArg1Addr;
	}

	public static byte add2Bytes(byte byteArg1, byte byteArg2) {
		byte byteSum = (byte)(byteArg1 + byteArg2);
		return byteSum;
	}

	public static byte addByteAndByteFromPointer(byte byteArg1, MemoryAddress byteArg2Addr) {
		MemorySegment byteArg2Segmt = byteArg2Addr.asSegment(C_CHAR.byteSize(), scope);
		byte byteArg2 = MemoryAccess.getByte(byteArg2Segmt);
		byte byteSum = (byte)(byteArg1 + byteArg2);
		return byteSum;
	}

	public static MemoryAddress addByteAndByteFromPtr_RetPtr(byte byteArg1, MemoryAddress byteArg2Addr) {
		MemorySegment byteArg2Segmt = byteArg2Addr.asSegment(C_CHAR.byteSize(), scope);
		byte byteArg2 = MemoryAccess.getByte(byteArg2Segmt);
		byte byteSum = (byte)(byteArg1 + byteArg2);
		MemorySegment resultSegmt = MemorySegment.allocateNative(C_CHAR.byteSize(), scope);
		MemoryAccess.setByte(resultSegmt, byteSum);
		return resultSegmt.address();
	}

	public static MemoryAddress addByteAndByteFromPtr_RetArgPtr(byte byteArg1, MemoryAddress byteArg2Addr) {
		MemorySegment byteArg2Segmt = byteArg2Addr.asSegment(C_CHAR.byteSize(), scope);
		byte byteArg2 = MemoryAccess.getByte(byteArg2Segmt);
		byte byteSum = (byte)(byteArg1 + byteArg2);
		MemoryAccess.setByte(byteArg2Segmt, byteSum);
		return byteArg2Addr;
	}

	public static short add2Shorts(short shortArg1, short shortArg2) {
		short shortSum = (short)(shortArg1 + shortArg2);
		return shortSum;
	}

	public static short addShortAndShortFromPointer(MemoryAddress shortArg1Addr, short shortArg2) {
		MemorySegment shortArg1Segmt = shortArg1Addr.asSegment(C_SHORT.byteSize(), scope);
		short shortArg1 = MemoryAccess.getShort(shortArg1Segmt);
		short shortSum = (short)(shortArg1 + shortArg2);
		return shortSum;
	}

	public static MemoryAddress addShortAndShortFromPtr_RetPtr(MemoryAddress shortArg1Addr, short shortArg2) {
		MemorySegment shortArg1Segmt = shortArg1Addr.asSegment(C_SHORT.byteSize(), scope);
		short shortArg1 = MemoryAccess.getShort(shortArg1Segmt);
		short shortSum = (short)(shortArg1 + shortArg2);
		MemorySegment resultSegmt = MemorySegment.allocateNative(C_SHORT.byteSize(), scope);
		MemoryAccess.setShort(resultSegmt, shortSum);
		return resultSegmt.address();
	}

	public static MemoryAddress addShortAndShortFromPtr_RetArgPtr(MemoryAddress shortArg1Addr, short shortArg2) {
		MemorySegment shortArg1Segmt = shortArg1Addr.asSegment(C_SHORT.byteSize(), scope);
		short shortArg1 = MemoryAccess.getShort(shortArg1Segmt);
		short shortSum = (short)(shortArg1 + shortArg2);
		MemoryAccess.setShort(shortArg1Segmt, shortSum);
		return shortArg1Addr;
	}

	public static int add2Ints(int intArg1, int intArg2) {
		int intSum = intArg1 + intArg2;
		return intSum;
	}

	public static int addIntAndIntFromPointer(int intArg1, MemoryAddress intArg2Addr) {
		MemorySegment intArg2Segmt = intArg2Addr.asSegment(C_INT.byteSize(), scope);
		int intArg2 = MemoryAccess.getInt(intArg2Segmt);
		int intSum = intArg1 + intArg2;
		return intSum;
	}

	public static MemoryAddress addIntAndIntFromPtr_RetPtr(int intArg1, MemoryAddress intArg2Addr) {
		MemorySegment intArg2Segmt = intArg2Addr.asSegment(C_INT.byteSize(), scope);
		int intArg2 = MemoryAccess.getInt(intArg2Segmt);
		int intSum = intArg1 + intArg2;
		MemorySegment resultSegmt = MemorySegment.allocateNative(C_INT.byteSize(), scope);
		MemoryAccess.setInt(resultSegmt, intSum);
		return resultSegmt.address();
	}

	public static MemoryAddress addIntAndIntFromPtr_RetArgPtr(int intArg1, MemoryAddress intArg2Addr) {
		MemorySegment intArg2Segmt = intArg2Addr.asSegment(C_INT.byteSize(), scope);
		int intArg2 = MemoryAccess.getInt(intArg2Segmt);
		int intSum = intArg1 + intArg2;
		MemoryAccess.setInt(intArg2Segmt, intSum);
		return intArg2Addr;
	}

	public static int add3Ints(int intArg1, int intArg2, int intArg3) {
		int intSum = intArg1 + intArg2 + intArg3;
		return intSum;
	}

	public static int addIntAndChar(int intArg, char charArg) {
		int sum = intArg + charArg;
		return sum;
	}

	public static void add2IntsReturnVoid(int intArg1, int intArg2) {
		int intSum = intArg1 + intArg2;
		System.out.println("add2IntsReturnVoid: intSum = " + intSum + "\n");
	}

	public static long add2Longs(long longArg1, long longArg2) {
		long longSum = longArg1 + longArg2;
		return longSum;
	}

	public static long addLongAndLongFromPointer(MemoryAddress longArg1Addr, long longArg2) {
		MemorySegment longArg1Segmt = longArg1Addr.asSegment(longLayout.byteSize(), scope);
		long longArg1 = MemoryAccess.getLong(longArg1Segmt);
		long longSum = longArg1 + longArg2;
		return longSum;
	}

	public static MemoryAddress addLongAndLongFromPtr_RetPtr(MemoryAddress longArg1Addr, long longArg2) {
		MemorySegment longArg1Segmt = longArg1Addr.asSegment(longLayout.byteSize(), scope);
		long longArg1 = MemoryAccess.getLong(longArg1Segmt);
		long longSum = longArg1 + longArg2;
		MemorySegment resultSegmt = MemorySegment.allocateNative(longLayout.byteSize(), scope);
		MemoryAccess.setLong(resultSegmt, longSum);
		return resultSegmt.address();
	}

	public static MemoryAddress addLongAndLongFromPtr_RetArgPtr(MemoryAddress longArg1Addr, long longArg2) {
		MemorySegment longArg1Segmt = longArg1Addr.asSegment(longLayout.byteSize(), scope);
		long longArg1 = MemoryAccess.getLong(longArg1Segmt);
		long longSum = longArg1 + longArg2;
		MemoryAccess.setLong(longArg1Segmt, longSum);
		return longArg1Addr;
	}

	public static float add2Floats(float floatArg1, float floatArg2) {
		float floatSum = floatArg1 + floatArg2;
		return floatSum;
	}

	public static float addFloatAndFloatFromPointer(float floatArg1, MemoryAddress floatArg2Addr) {
		MemorySegment floatArg2Segmt = floatArg2Addr.asSegment(C_FLOAT.byteSize(), scope);
		float floatArg2 = MemoryAccess.getFloat(floatArg2Segmt);
		float floatSum = floatArg1 + floatArg2;
		return floatSum;
	}

	public static MemoryAddress addFloatAndFloatFromPtr_RetPtr(float floatArg1, MemoryAddress floatArg2Addr) {
		MemorySegment floatArg2Segmt = floatArg2Addr.asSegment(C_FLOAT.byteSize(), scope);
		float floatArg2 = MemoryAccess.getFloat(floatArg2Segmt);
		float floatSum = floatArg1 + floatArg2;
		MemorySegment resultSegmt = MemorySegment.allocateNative(C_FLOAT.byteSize(), scope);
		MemoryAccess.setFloat(resultSegmt, floatSum);
		return resultSegmt.address();
	}

	public static MemoryAddress addFloatAndFloatFromPtr_RetArgPtr(float floatArg1, MemoryAddress floatArg2Addr) {
		MemorySegment floatArg2Segmt = floatArg2Addr.asSegment(C_FLOAT.byteSize(), scope);
		float floatArg2 = MemoryAccess.getFloat(floatArg2Segmt);
		float floatSum = floatArg1 + floatArg2;
		MemoryAccess.setFloat(floatArg2Segmt, floatSum);
		return floatArg2Addr;
	}

	public static double add2Doubles(double doubleArg1, double doubleArg2) {
		double doubleSum = doubleArg1 + doubleArg2;
		return doubleSum;
	}

	public static double addDoubleAndDoubleFromPointer(MemoryAddress doubleArg1Addr, double doubleArg2) {
		MemorySegment doubleArg1Segmt = doubleArg1Addr.asSegment(C_DOUBLE.byteSize(), scope);
		double doubleArg1 = MemoryAccess.getDouble(doubleArg1Segmt);
		double doubleSum = doubleArg1 + doubleArg2;
		return doubleSum;
	}

	public static MemoryAddress addDoubleAndDoubleFromPtr_RetPtr(MemoryAddress doubleArg1Addr, double doubleArg2) {
		MemorySegment doubleArg1Segmt = doubleArg1Addr.asSegment(C_DOUBLE.byteSize(), scope);
		double doubleArg1 = MemoryAccess.getDouble(doubleArg1Segmt);
		double doubleSum = doubleArg1 + doubleArg2;
		MemorySegment resultSegmt = MemorySegment.allocateNative(C_DOUBLE.byteSize(), scope);
		MemoryAccess.setDouble(resultSegmt, doubleSum);
		return resultSegmt.address();
	}

	public static MemoryAddress addDoubleAndDoubleFromPtr_RetArgPtr(MemoryAddress doubleArg1Addr, double doubleArg2) {
		MemorySegment doubleArg1Segmt = doubleArg1Addr.asSegment(C_DOUBLE.byteSize(), scope);
		double doubleArg1 = MemoryAccess.getDouble(doubleArg1Segmt);
		double doubleSum = doubleArg1 + doubleArg2;
		MemoryAccess.setDouble(doubleArg1Segmt, doubleSum);
		return doubleArg1Addr;
	}

	public static int compare(MemoryAddress argAddr1, MemoryAddress argAddr2) {
		int intArg1 = MemoryAccess.getInt(argAddr1.asSegment(C_INT.byteSize(), scope));
		int intArg2 = MemoryAccess.getInt(argAddr2.asSegment(C_INT.byteSize(), scope));
		return (intArg1 - intArg2);
	}

	public static boolean addBoolAndBoolsFromStructWithXor(boolean arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		boolean boolSum = arg1 ^ byteToBool((byte)boolHandle1.get(arg2)) ^ byteToBool((byte)boolHandle2.get(arg2));
		return boolSum;
	}

	public static boolean addBoolAnd20BoolsFromStructWithXor(boolean arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"),
				C_CHAR.withName("elem3"), C_CHAR.withName("elem4"), C_CHAR.withName("elem5"), C_CHAR.withName("elem6"),
				C_CHAR.withName("elem7"), C_CHAR.withName("elem8"), C_CHAR.withName("elem9"), C_CHAR.withName("elem10"),
				C_CHAR.withName("elem11"), C_CHAR.withName("elem12"), C_CHAR.withName("elem13"), C_CHAR.withName("elem14"),
				C_CHAR.withName("elem15"), C_CHAR.withName("elem16"), C_CHAR.withName("elem17"), C_CHAR.withName("elem18"),
				C_CHAR.withName("elem19"), C_CHAR.withName("elem20"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));
		VarHandle boolHandle3 = structLayout.varHandle(byte.class, PathElement.groupElement("elem3"));
		VarHandle boolHandle4 = structLayout.varHandle(byte.class, PathElement.groupElement("elem4"));
		VarHandle boolHandle5 = structLayout.varHandle(byte.class, PathElement.groupElement("elem5"));
		VarHandle boolHandle6 = structLayout.varHandle(byte.class, PathElement.groupElement("elem6"));
		VarHandle boolHandle7 = structLayout.varHandle(byte.class, PathElement.groupElement("elem7"));
		VarHandle boolHandle8 = structLayout.varHandle(byte.class, PathElement.groupElement("elem8"));
		VarHandle boolHandle9 = structLayout.varHandle(byte.class, PathElement.groupElement("elem9"));
		VarHandle boolHandle10 = structLayout.varHandle(byte.class, PathElement.groupElement("elem10"));
		VarHandle boolHandle11 = structLayout.varHandle(byte.class, PathElement.groupElement("elem11"));
		VarHandle boolHandle12 = structLayout.varHandle(byte.class, PathElement.groupElement("elem12"));
		VarHandle boolHandle13 = structLayout.varHandle(byte.class, PathElement.groupElement("elem13"));
		VarHandle boolHandle14 = structLayout.varHandle(byte.class, PathElement.groupElement("elem14"));
		VarHandle boolHandle15 = structLayout.varHandle(byte.class, PathElement.groupElement("elem15"));
		VarHandle boolHandle16 = structLayout.varHandle(byte.class, PathElement.groupElement("elem16"));
		VarHandle boolHandle17 = structLayout.varHandle(byte.class, PathElement.groupElement("elem17"));
		VarHandle boolHandle18 = structLayout.varHandle(byte.class, PathElement.groupElement("elem18"));
		VarHandle boolHandle19 = structLayout.varHandle(byte.class, PathElement.groupElement("elem19"));
		VarHandle boolHandle20 = structLayout.varHandle(byte.class, PathElement.groupElement("elem20"));

		boolean boolSum = arg1 ^ byteToBool((byte)boolHandle1.get(arg2)) ^ byteToBool((byte)boolHandle2.get(arg2))
				^ byteToBool((byte)boolHandle3.get(arg2)) ^ byteToBool((byte)boolHandle4.get(arg2)) ^ byteToBool((byte)boolHandle5.get(arg2))
				^ byteToBool((byte)boolHandle6.get(arg2)) ^ byteToBool((byte)boolHandle7.get(arg2)) ^ byteToBool((byte)boolHandle8.get(arg2))
				^ byteToBool((byte)boolHandle9.get(arg2)) ^ byteToBool((byte)boolHandle10.get(arg2)) ^ byteToBool((byte)boolHandle11.get(arg2))
				^ byteToBool((byte)boolHandle12.get(arg2)) ^ byteToBool((byte)boolHandle13.get(arg2)) ^ byteToBool((byte)boolHandle14.get(arg2))
				^ byteToBool((byte)boolHandle15.get(arg2)) ^ byteToBool((byte)boolHandle16.get(arg2)) ^ byteToBool((byte)boolHandle17.get(arg2))
				^ byteToBool((byte)boolHandle18.get(arg2)) ^ byteToBool((byte)boolHandle19.get(arg2)) ^ byteToBool((byte)boolHandle20.get(arg2));
		return boolSum;
	}

	public static boolean addBoolFromPointerAndBoolsFromStructWithXor(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_CHAR.byteSize(), arg1Addr.scope());
		byte arg1 = MemoryAccess.getByte(arg1Segmt);
		boolean boolSum = byteToBool(arg1) ^ byteToBool((byte)boolHandle1.get(arg2)) ^ byteToBool((byte)boolHandle2.get(arg2));
		return boolSum;
	}

	public static MemoryAddress addBoolFromPointerAndBoolsFromStructWithXor_returnBoolPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_CHAR.byteSize(), arg1Addr.scope());
		byte arg1 = MemoryAccess.getByte(arg1Segmt);
		boolean boolSum = byteToBool(arg1) ^ byteToBool((byte)boolHandle1.get(arg2)) ^ byteToBool((byte)boolHandle2.get(arg2));
		MemoryAccess.setByte(arg1Segmt, boolToByte(boolSum));
		return arg1Addr;
	}

	public static boolean addBoolAndBoolsFromStructPointerWithXor(boolean arg1, MemoryAddress arg2Addr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MemorySegment arg2 = arg2Addr.asSegment(structLayout.byteSize(), arg2Addr.scope());
		boolean boolSum = arg1 ^ byteToBool((byte)boolHandle1.get(arg2)) ^ byteToBool((byte)boolHandle2.get(arg2));
		return boolSum;
	}

	public static boolean addBoolAndBoolsFromNestedStructWithXor(boolean arg1, MemorySegment arg2) {
		boolean nestedStructElem1 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 0));
		boolean nestedStructElem2 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 1));
		boolean structElem2 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 2));
		boolean boolSum = arg1 ^ nestedStructElem1 ^ nestedStructElem2 ^ structElem2;
		return boolSum;
	}

	public static boolean addBoolAndBoolsFromNestedStructWithXor_reverseOrder(boolean arg1, MemorySegment arg2) {
		boolean structElem1 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 0));
		boolean nestedStructElem1 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 1));
		boolean nestedStructElem2 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 2));
		boolean boolSum = arg1 ^ structElem1 ^ nestedStructElem1 ^ nestedStructElem2;
		return boolSum;
	}

	public static boolean addBoolAndBoolsFromStructWithNestedBoolArray(boolean arg1, MemorySegment arg2) {
		boolean nestedBoolArrayElem1 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 0));
		boolean nestedBoolArrayElem2 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 1));
		boolean structElem2 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 2));

		boolean boolSum = arg1 ^ nestedBoolArrayElem1 ^ nestedBoolArrayElem2 ^ structElem2;
		return boolSum;
	}

	public static boolean addBoolAndBoolsFromStructWithNestedBoolArray_reverseOrder(boolean arg1, MemorySegment arg2) {
		boolean structElem1 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 0));
		boolean nestedBoolArrayElem1 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 1));
		boolean nestedBoolArrayElem2 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 2));

		boolean boolSum = arg1 ^ structElem1 ^ nestedBoolArrayElem1 ^ nestedBoolArrayElem2;
		return boolSum;
	}

	public static boolean addBoolAndBoolsFromStructWithNestedStructArray(boolean arg1, MemorySegment arg2) {
		boolean nestedStructArrayElem1_Elem1 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 0));
		boolean nestedStructArrayElem1_Elem2 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 1));
		boolean nestedStructArrayElem2_Elem1 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 2));
		boolean nestedStructArrayElem2_Elem2 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 3));
		boolean structElem2 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 4));

		boolean boolSum = arg1 ^ structElem2
				^ nestedStructArrayElem1_Elem1 ^ nestedStructArrayElem1_Elem2
				^ nestedStructArrayElem2_Elem1 ^ nestedStructArrayElem2_Elem2;
		return boolSum;
	}

	public static boolean addBoolAndBoolsFromStructWithNestedStructArray_reverseOrder(boolean arg1, MemorySegment arg2) {
		boolean structElem1 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 0));
		boolean nestedStructArrayElem1_Elem1 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 1));
		boolean nestedStructArrayElem1_Elem2 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 2));
		boolean nestedStructArrayElem2_Elem1 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 3));
		boolean nestedStructArrayElem2_Elem2 = byteToBool(MemoryAccess.getByteAtOffset(arg2, 4));

		boolean boolSum = arg1 ^ structElem1
				^ nestedStructArrayElem1_Elem1 ^ nestedStructArrayElem1_Elem2
				^ nestedStructArrayElem2_Elem1 ^ nestedStructArrayElem2_Elem2;
		return boolSum;
	}

	public static MemorySegment add2BoolStructsWithXor_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MemorySegment boolStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		boolean boolStruct_Elem1 = byteToBool((byte)boolHandle1.get(arg1)) ^ byteToBool((byte)boolHandle1.get(arg2));
		boolean boolStruct_Elem2 = byteToBool((byte)boolHandle2.get(arg1)) ^ byteToBool((byte)boolHandle2.get(arg2));
		boolHandle1.set(boolStructSegmt, boolToByte(boolStruct_Elem1));
		boolHandle2.set(boolStructSegmt, boolToByte(boolStruct_Elem2));
		return boolStructSegmt;
	}

	public static MemoryAddress add2BoolStructsWithXor_returnStructPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MemorySegment arg1 = arg1Addr.asSegment(structLayout.byteSize(), arg1Addr.scope());
		boolean boolStruct_Elem1 = byteToBool((byte)boolHandle1.get(arg1)) ^ byteToBool((byte)boolHandle1.get(arg2));
		boolean boolStruct_Elem2 = byteToBool((byte)boolHandle2.get(arg1)) ^ byteToBool((byte)boolHandle2.get(arg2));
		boolHandle1.set(arg1, boolToByte(boolStruct_Elem1));
		boolHandle2.set(arg1, boolToByte(boolStruct_Elem2));
		return arg1Addr;
	}

	public static MemorySegment add3BoolStructsWithXor_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"),
				C_CHAR.withName("elem3"), MemoryLayout.paddingLayout(16));
		VarHandle boolHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle boolHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));
		VarHandle boolHandle3 = structLayout.varHandle(byte.class, PathElement.groupElement("elem3"));

		MemorySegment boolStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		boolean boolStruct_Elem1 = byteToBool((byte)boolHandle1.get(arg1)) ^ byteToBool((byte)boolHandle1.get(arg2));
		boolean boolStruct_Elem2 = byteToBool((byte)boolHandle2.get(arg1)) ^ byteToBool((byte)boolHandle2.get(arg2));
		boolean boolStruct_Elem3 = byteToBool((byte)boolHandle3.get(arg1)) ^ byteToBool((byte)boolHandle3.get(arg2));
		boolHandle1.set(boolStructSegmt, boolToByte(boolStruct_Elem1));
		boolHandle2.set(boolStructSegmt, boolToByte(boolStruct_Elem2));
		boolHandle3.set(boolStructSegmt, boolToByte(boolStruct_Elem3));
		return boolStructSegmt;
	}

	public static byte addByteAndBytesFromStruct(byte arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		byte byteSum = (byte)(arg1 + (byte)byteHandle1.get(arg2) + (byte)byteHandle2.get(arg2));
		return byteSum;
	}

	public static byte addByteAnd20BytesFromStruct(byte arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"),
		C_CHAR.withName("elem3"), C_CHAR.withName("elem4"), C_CHAR.withName("elem5"), C_CHAR.withName("elem6"),
		C_CHAR.withName("elem7"), C_CHAR.withName("elem8"), C_CHAR.withName("elem9"), C_CHAR.withName("elem10"),
		C_CHAR.withName("elem11"), C_CHAR.withName("elem12"), C_CHAR.withName("elem13"), C_CHAR.withName("elem14"),
		C_CHAR.withName("elem15"), C_CHAR.withName("elem16"), C_CHAR.withName("elem17"), C_CHAR.withName("elem18"),
		C_CHAR.withName("elem19"), C_CHAR.withName("elem20"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));
		VarHandle byteHandle3 = structLayout.varHandle(byte.class, PathElement.groupElement("elem3"));
		VarHandle byteHandle4 = structLayout.varHandle(byte.class, PathElement.groupElement("elem4"));
		VarHandle byteHandle5 = structLayout.varHandle(byte.class, PathElement.groupElement("elem5"));
		VarHandle byteHandle6 = structLayout.varHandle(byte.class, PathElement.groupElement("elem6"));
		VarHandle byteHandle7 = structLayout.varHandle(byte.class, PathElement.groupElement("elem7"));
		VarHandle byteHandle8 = structLayout.varHandle(byte.class, PathElement.groupElement("elem8"));
		VarHandle byteHandle9 = structLayout.varHandle(byte.class, PathElement.groupElement("elem9"));
		VarHandle byteHandle10 = structLayout.varHandle(byte.class, PathElement.groupElement("elem10"));
		VarHandle byteHandle11 = structLayout.varHandle(byte.class, PathElement.groupElement("elem11"));
		VarHandle byteHandle12 = structLayout.varHandle(byte.class, PathElement.groupElement("elem12"));
		VarHandle byteHandle13 = structLayout.varHandle(byte.class, PathElement.groupElement("elem13"));
		VarHandle byteHandle14 = structLayout.varHandle(byte.class, PathElement.groupElement("elem14"));
		VarHandle byteHandle15 = structLayout.varHandle(byte.class, PathElement.groupElement("elem15"));
		VarHandle byteHandle16 = structLayout.varHandle(byte.class, PathElement.groupElement("elem16"));
		VarHandle byteHandle17 = structLayout.varHandle(byte.class, PathElement.groupElement("elem17"));
		VarHandle byteHandle18 = structLayout.varHandle(byte.class, PathElement.groupElement("elem18"));
		VarHandle byteHandle19 = structLayout.varHandle(byte.class, PathElement.groupElement("elem19"));
		VarHandle byteHandle20 = structLayout.varHandle(byte.class, PathElement.groupElement("elem20"));

		byte bytelSum = (byte)(arg1 + (byte)byteHandle1.get(arg2) + (byte)byteHandle2.get(arg2)
				+ (byte)byteHandle3.get(arg2) + (byte)byteHandle4.get(arg2) + (byte)byteHandle5.get(arg2)
				+ (byte)byteHandle6.get(arg2) + (byte)byteHandle7.get(arg2) + (byte)byteHandle8.get(arg2)
				+ (byte)byteHandle9.get(arg2) + (byte)byteHandle10.get(arg2) + (byte)byteHandle11.get(arg2)
				+ (byte)byteHandle12.get(arg2) + (byte)byteHandle13.get(arg2) + (byte)byteHandle14.get(arg2)
				+ (byte)byteHandle15.get(arg2) + (byte)byteHandle16.get(arg2) + (byte)byteHandle17.get(arg2)
				+ (byte)byteHandle18.get(arg2) + (byte)byteHandle19.get(arg2) + (byte)byteHandle20.get(arg2));
		return bytelSum;
	}

	public static byte addByteFromPointerAndBytesFromStruct(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_CHAR.byteSize(), arg1Addr.scope());
		byte arg1 = MemoryAccess.getByte(arg1Segmt);
		byte byteSum = (byte)(arg1 + (byte)byteHandle1.get(arg2) + (byte)byteHandle2.get(arg2));
		return byteSum;
	}

	public static MemoryAddress addByteFromPointerAndBytesFromStruct_returnBytePointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_CHAR.byteSize(), arg1Addr.scope());
		byte arg1 = MemoryAccess.getByte(arg1Segmt);
		byte byteSum = (byte)(arg1 + (byte)byteHandle1.get(arg2) + (byte)byteHandle2.get(arg2));
		MemoryAccess.setByte(arg1Segmt, byteSum);
		return arg1Addr;
	}

	public static byte addByteAndBytesFromStructPointer(byte arg1, MemoryAddress arg2Addr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MemorySegment arg2 = arg2Addr.asSegment(structLayout.byteSize(), arg2Addr.scope());
		byte byteSum = (byte)(arg1 + (byte)byteHandle1.get(arg2) + (byte)byteHandle2.get(arg2));
		return byteSum;
	}

	public static byte addByteAndBytesFromNestedStruct(byte arg1, MemorySegment arg2) {
		byte nestedStructElem1 = MemoryAccess.getByteAtOffset(arg2, 0);
		byte nestedStructElem2 = MemoryAccess.getByteAtOffset(arg2, 1);
		byte structElem2 = MemoryAccess.getByteAtOffset(arg2, 2);

		byte byteSum = (byte)(arg1 + nestedStructElem1 + nestedStructElem2 + structElem2);
		return byteSum;
	}

	public static byte addByteAndBytesFromNestedStruct_reverseOrder(byte arg1, MemorySegment arg2) {
		byte structElem1 = MemoryAccess.getByteAtOffset(arg2, 0);
		byte nestedStructElem1 = MemoryAccess.getByteAtOffset(arg2, 1);
		byte nestedStructElem2 = MemoryAccess.getByteAtOffset(arg2, 2);

		byte byteSum = (byte)(arg1 + structElem1 + nestedStructElem1 + nestedStructElem2);
		return byteSum;
	}

	public static byte addByteAndBytesFromStructWithNestedByteArray(byte arg1, MemorySegment arg2) {
		byte nestedByteArrayElem1 = MemoryAccess.getByteAtOffset(arg2, 0);
		byte nestedByteArrayElem2 = MemoryAccess.getByteAtOffset(arg2, 1);
		byte structElem2 = MemoryAccess.getByteAtOffset(arg2, 2);

		byte byteSum = (byte)(arg1 + nestedByteArrayElem1 + nestedByteArrayElem2 + structElem2);
		return byteSum;
	}

	public static byte addByteAndBytesFromStructWithNestedByteArray_reverseOrder(byte arg1, MemorySegment arg2) {
		byte structElem1 = MemoryAccess.getByteAtOffset(arg2, 0);
		byte nestedByteArrayElem1 = MemoryAccess.getByteAtOffset(arg2, 1);
		byte nestedByteArrayElem2 = MemoryAccess.getByteAtOffset(arg2, 2);

		byte byteSum = (byte)(arg1 + structElem1 + nestedByteArrayElem1 + nestedByteArrayElem2);
		return byteSum;
	}

	public static byte addByteAndBytesFromStructWithNestedStructArray(byte arg1, MemorySegment arg2) {
		byte nestedStructArrayElem1_Elem1 = MemoryAccess.getByteAtOffset(arg2, 0);
		byte nestedStructArrayElem1_Elem2 = MemoryAccess.getByteAtOffset(arg2, 1);
		byte nestedStructArrayElem2_Elem1 = MemoryAccess.getByteAtOffset(arg2, 2);
		byte nestedStructArrayElem2_Elem2 = MemoryAccess.getByteAtOffset(arg2, 3);
		byte structElem2 = MemoryAccess.getByteAtOffset(arg2, 4);

		byte byteSum = (byte)(arg1 + structElem2
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2);
		return byteSum;
	}

	public static byte addByteAndBytesFromStructWithNestedStructArray_reverseOrder(byte arg1, MemorySegment arg2) {
		byte structElem1 = MemoryAccess.getByteAtOffset(arg2, 0);
		byte nestedStructArrayElem1_Elem1 = MemoryAccess.getByteAtOffset(arg2, 1);
		byte nestedStructArrayElem1_Elem2 = MemoryAccess.getByteAtOffset(arg2, 2);
		byte nestedStructArrayElem2_Elem1 = MemoryAccess.getByteAtOffset(arg2, 3);
		byte nestedStructArrayElem2_Elem2 = MemoryAccess.getByteAtOffset(arg2, 4);

		byte byteSum = (byte)(arg1 + structElem1
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2);
		return byteSum;
	}

	public static MemorySegment add1ByteStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));

		MemorySegment byteStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		byte byteStruct_Elem1 = (byte)((byte)byteHandle1.get(arg1) + (byte)byteHandle1.get(arg2));
		byteHandle1.set(byteStructSegmt, byteStruct_Elem1);
		return byteStructSegmt;
	}

	public static MemorySegment add2ByteStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MemorySegment byteStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		byte byteStruct_Elem1 = (byte)((byte)byteHandle1.get(arg1) + (byte)byteHandle1.get(arg2));
		byte byteStruct_Elem2 = (byte)((byte)byteHandle2.get(arg1) + (byte)byteHandle2.get(arg2));
		byteHandle1.set(byteStructSegmt, byteStruct_Elem1);
		byteHandle2.set(byteStructSegmt, byteStruct_Elem2);
		return byteStructSegmt;
	}

	public static MemoryAddress add2ByteStructs_returnStructPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		MemorySegment arg1 = arg1Addr.asSegment(structLayout.byteSize(), arg1Addr.scope());
		byte byteStruct_Elem1 = (byte)((byte)byteHandle1.get(arg1) + (byte)byteHandle1.get(arg2));
		byte byteStruct_Elem2 = (byte)((byte)byteHandle2.get(arg1) + (byte)byteHandle2.get(arg2));
		byteHandle1.set(arg1, byteStruct_Elem1);
		byteHandle2.set(arg1, byteStruct_Elem2);
		return arg1Addr;
	}

	public static MemorySegment add3ByteStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"),
				C_CHAR.withName("elem3"), MemoryLayout.paddingLayout(16));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));
		VarHandle byteHandle3 = structLayout.varHandle(byte.class, PathElement.groupElement("elem3"));

		MemorySegment byteStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		byte byteStruct_Elem1 = (byte)((byte)byteHandle1.get(arg1) + (byte)byteHandle1.get(arg2));
		byte byteStruct_Elem2 = (byte)((byte)byteHandle2.get(arg1) + (byte)byteHandle2.get(arg2));
		byte byteStruct_Elem3 = (byte)((byte)byteHandle3.get(arg1) + (byte)byteHandle3.get(arg2));
		byteHandle1.set(byteStructSegmt, byteStruct_Elem1);
		byteHandle2.set(byteStructSegmt, byteStruct_Elem2);
		byteHandle3.set(byteStructSegmt, byteStruct_Elem3);
		return byteStructSegmt;
	}

	public static char addCharAndCharsFromStruct(char arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		char result = (char)(arg1 + (char)charHandle1.get(arg2) + (char)charHandle2.get(arg2) - 2 * 'A');
		return result;
	}

	public static char addCharAnd10CharsFromStruct(char arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"),
				C_SHORT.withName("elem3"), C_SHORT.withName("elem4"), C_SHORT.withName("elem5"),
				C_SHORT.withName("elem6"), C_SHORT.withName("elem7"), C_SHORT.withName("elem8"),
				C_SHORT.withName("elem9"), C_SHORT.withName("elem10"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));
		VarHandle charHandle3 = structLayout.varHandle(char.class, PathElement.groupElement("elem3"));
		VarHandle charHandle4 = structLayout.varHandle(char.class, PathElement.groupElement("elem4"));
		VarHandle charHandle5 = structLayout.varHandle(char.class, PathElement.groupElement("elem5"));
		VarHandle charHandle6 = structLayout.varHandle(char.class, PathElement.groupElement("elem6"));
		VarHandle charHandle7 = structLayout.varHandle(char.class, PathElement.groupElement("elem7"));
		VarHandle charHandle8 = structLayout.varHandle(char.class, PathElement.groupElement("elem8"));
		VarHandle charHandle9 = structLayout.varHandle(char.class, PathElement.groupElement("elem9"));
		VarHandle charHandle10 = structLayout.varHandle(char.class, PathElement.groupElement("elem10"));

		char result = (char)(arg1 + (char)charHandle1.get(arg2) + (char)charHandle2.get(arg2)
		+ (char)charHandle3.get(arg2) + (char)charHandle4.get(arg2) + (char)charHandle5.get(arg2)
		+ (char)charHandle6.get(arg2) + (char)charHandle7.get(arg2) + (char)charHandle8.get(arg2)
		+ (char)charHandle9.get(arg2) + (char)charHandle10.get(arg2) - 10 * 'A');
		return result;
	}

	public static char addCharFromPointerAndCharsFromStruct(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_SHORT.byteSize(), arg1Addr.scope());
		char arg1 = MemoryAccess.getChar(arg1Segmt);
		char result = (char)(arg1 + (char)charHandle1.get(arg2) + (char)charHandle2.get(arg2) - 2 * 'A');
		return result;
	}

	public static MemoryAddress addCharFromPointerAndCharsFromStruct_returnCharPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_SHORT.byteSize(), arg1Addr.scope());
		char arg1 = MemoryAccess.getChar(arg1Segmt);
		char result = (char)(arg1 + (char)charHandle1.get(arg2) + (char)charHandle2.get(arg2) - 2 * 'A');
		MemoryAccess.setChar(arg1Segmt, result);
		return arg1Addr;
	}

	public static char addCharAndCharsFromStructPointer(char arg1, MemoryAddress arg2Addr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MemorySegment arg2 = arg2Addr.asSegment(structLayout.byteSize(), arg2Addr.scope());
		char result = (char)(arg1 + (char)charHandle1.get(arg2) + (char)charHandle2.get(arg2) - 2 * 'A');
		return result;
	}

	public static char addCharAndCharsFromNestedStruct(char arg1, MemorySegment arg2) {
		char nestedStructElem1 = MemoryAccess.getCharAtOffset(arg2, 0);
		char nestedStructElem2 = MemoryAccess.getCharAtOffset(arg2, 2);
		char structElem2 = MemoryAccess.getCharAtOffset(arg2, 4);

		char result = (char)(arg1 + nestedStructElem1 + nestedStructElem2 + structElem2 - 3 * 'A');
		return result;
	}

	public static char addCharAndCharsFromNestedStruct_reverseOrder(char arg1, MemorySegment arg2) {
		char structElem1 = MemoryAccess.getCharAtOffset(arg2, 0);
		char nestedStructElem1 = MemoryAccess.getCharAtOffset(arg2, 2);
		char nestedStructElem2 = MemoryAccess.getCharAtOffset(arg2, 4);

		char result = (char)(arg1 + structElem1 + nestedStructElem1 + nestedStructElem2 - 3 * 'A');
		return result;
	}

	public static char addCharAndCharsFromStructWithNestedCharArray(char arg1, MemorySegment arg2) {
		char nestedCharArrayElem1 = MemoryAccess.getCharAtOffset(arg2, 0);
		char nestedCharArrayElem2 = MemoryAccess.getCharAtOffset(arg2, 2);
		char structElem2 = MemoryAccess.getCharAtOffset(arg2, 4);

		char result = (char)(arg1 + nestedCharArrayElem1 + nestedCharArrayElem2 + structElem2 - 3 * 'A');
		return result;
	}

	public static char addCharAndCharsFromStructWithNestedCharArray_reverseOrder(char arg1, MemorySegment arg2) {
		char structElem1 = MemoryAccess.getCharAtOffset(arg2, 0);
		char nestedCharArrayElem1 = MemoryAccess.getCharAtOffset(arg2, 2);
		char nestedCharArrayElem2 = MemoryAccess.getCharAtOffset(arg2, 4);

		char result = (char)(arg1 + structElem1 + nestedCharArrayElem1 + nestedCharArrayElem2 - 3 * 'A');
		return result;
	}

	public static char addCharAndCharsFromStructWithNestedStructArray(char arg1, MemorySegment arg2) {
		char nestedStructArrayElem1_Elem1 = MemoryAccess.getCharAtOffset(arg2, 0);
		char nestedStructArrayElem1_Elem2 = MemoryAccess.getCharAtOffset(arg2, 2);
		char nestedStructArrayElem2_Elem1 = MemoryAccess.getCharAtOffset(arg2, 4);
		char nestedStructArrayElem2_Elem2 = MemoryAccess.getCharAtOffset(arg2, 6);
		char structElem2 = MemoryAccess.getCharAtOffset(arg2, 8);

		char result = (char)(arg1 + structElem2
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2 - 5 * 'A');
		return result;
	}

	public static char addCharAndCharsFromStructWithNestedStructArray_reverseOrder(char arg1, MemorySegment arg2) {
		char structElem1 = MemoryAccess.getCharAtOffset(arg2, 0);
		char nestedStructArrayElem1_Elem1 = MemoryAccess.getCharAtOffset(arg2, 2);
		char nestedStructArrayElem1_Elem2 = MemoryAccess.getCharAtOffset(arg2, 4);
		char nestedStructArrayElem2_Elem1 = MemoryAccess.getCharAtOffset(arg2, 6);
		char nestedStructArrayElem2_Elem2 = MemoryAccess.getCharAtOffset(arg2, 8);

		char result = (char)(arg1 + structElem1
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2 - 5 * 'A');
		return result;
	}

	public static MemorySegment add2CharStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MemorySegment charStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		char charStruct_Elem1 = (char)((char)charHandle1.get(arg1) + (char)charHandle1.get(arg2) - 'A');
		char charStruct_Elem2 = (char)((char)charHandle2.get(arg1) + (char)charHandle2.get(arg2) - 'A');
		charHandle1.set(charStructSegmt, charStruct_Elem1);
		charHandle2.set(charStructSegmt, charStruct_Elem2);
		return charStructSegmt;
	}

	public static MemoryAddress add2CharStructs_returnStructPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));

		MemorySegment arg1 = arg1Addr.asSegment(structLayout.byteSize(), arg1Addr.scope());
		char charStruct_Elem1 = (char)((char)charHandle1.get(arg1) + (char)charHandle1.get(arg2) - 'A');
		char charStruct_Elem2 = (char)((char)charHandle2.get(arg1) + (char)charHandle2.get(arg2) - 'A');
		charHandle1.set(arg1, charStruct_Elem1);
		charHandle2.set(arg1, charStruct_Elem2);
		return arg1Addr;
	}

	public static MemorySegment add3CharStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"),
				C_SHORT.withName("elem3"), MemoryLayout.paddingLayout(16));
		VarHandle charHandle1 = structLayout.varHandle(char.class, PathElement.groupElement("elem1"));
		VarHandle charHandle2 = structLayout.varHandle(char.class, PathElement.groupElement("elem2"));
		VarHandle charHandle3 = structLayout.varHandle(char.class, PathElement.groupElement("elem3"));

		MemorySegment charStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		char charStruct_Elem1 = (char)((char)charHandle1.get(arg1) + (char)charHandle1.get(arg2) - 'A');
		char charStruct_Elem2 = (char)((char)charHandle2.get(arg1) + (char)charHandle2.get(arg2) - 'A');
		char charStruct_Elem3 = (char)((char)charHandle3.get(arg1) + (char)charHandle3.get(arg2) - 'A');
		charHandle1.set(charStructSegmt, charStruct_Elem1);
		charHandle2.set(charStructSegmt, charStruct_Elem2);
		charHandle3.set(charStructSegmt, charStruct_Elem3);
		return charStructSegmt;
	}

	public static short addShortAndShortsFromStruct(short arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		short shortSum = (short)(arg1 + (short)shortHandle1.get(arg2) + (short)shortHandle2.get(arg2));
		return shortSum;
	}

	public static short addShortAnd10ShortsFromStruct(short arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"),
				C_SHORT.withName("elem3"), C_SHORT.withName("elem4"), C_SHORT.withName("elem5"),
				C_SHORT.withName("elem6"), C_SHORT.withName("elem7"), C_SHORT.withName("elem8"),
				C_SHORT.withName("elem9"), C_SHORT.withName("elem10"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));
		VarHandle shortHandle3 = structLayout.varHandle(short.class, PathElement.groupElement("elem3"));
		VarHandle shortHandle4 = structLayout.varHandle(short.class, PathElement.groupElement("elem4"));
		VarHandle shortHandle5 = structLayout.varHandle(short.class, PathElement.groupElement("elem5"));
		VarHandle shortHandle6 = structLayout.varHandle(short.class, PathElement.groupElement("elem6"));
		VarHandle shortHandle7 = structLayout.varHandle(short.class, PathElement.groupElement("elem7"));
		VarHandle shortHandle8 = structLayout.varHandle(short.class, PathElement.groupElement("elem8"));
		VarHandle shortHandle9 = structLayout.varHandle(short.class, PathElement.groupElement("elem9"));
		VarHandle shortHandle10 = structLayout.varHandle(short.class, PathElement.groupElement("elem10"));

		short shortSum = (short)(arg1 + (short)shortHandle1.get(arg2) + (short)shortHandle2.get(arg2)
		+ (short)shortHandle3.get(arg2) + (short)shortHandle4.get(arg2) + (short)shortHandle5.get(arg2)
		+ (short)shortHandle6.get(arg2) + (short)shortHandle7.get(arg2) + (short)shortHandle8.get(arg2)
		+ (short)shortHandle9.get(arg2) + (short)shortHandle10.get(arg2));
		return shortSum;
	}

	public static short addShortFromPointerAndShortsFromStruct(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_SHORT.byteSize(), arg1Addr.scope());
		short arg1 = MemoryAccess.getShort(arg1Segmt);
		short shortSum = (short)(arg1 + (short)shortHandle1.get(arg2) + (short)shortHandle2.get(arg2));
		return shortSum;
	}

	public static MemoryAddress addShortFromPointerAndShortsFromStruct_returnShortPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_SHORT.byteSize(), arg1Addr.scope());
		short arg1 = MemoryAccess.getShort(arg1Segmt);
		short shortSum = (short)(arg1 + (short)shortHandle1.get(arg2) + (short)shortHandle2.get(arg2));
		MemoryAccess.setShort(arg1Segmt, shortSum);
		return arg1Addr;
	}

	public static short addShortAndShortsFromStructPointer(short arg1, MemoryAddress arg2Addr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MemorySegment arg2 = arg2Addr.asSegment(structLayout.byteSize(), arg2Addr.scope());
		short shortSum = (short)(arg1 + (short)shortHandle1.get(arg2) + (short)shortHandle2.get(arg2));
		return shortSum;
	}

	public static short addShortAndShortsFromNestedStruct(short arg1, MemorySegment arg2) {
		short nestedStructElem1 = MemoryAccess.getShortAtOffset(arg2, 0);
		short nestedStructElem2 = MemoryAccess.getShortAtOffset(arg2, 2);
		short structElem2 = MemoryAccess.getShortAtOffset(arg2, 4);

		short shortSum = (short)(arg1 + nestedStructElem1 + nestedStructElem2 + structElem2);
		return shortSum;
	}

	public static short addShortAndShortsFromNestedStruct_reverseOrder(short arg1, MemorySegment arg2) {
		short structElem1 = MemoryAccess.getShortAtOffset(arg2, 0);
		short nestedStructElem1 = MemoryAccess.getShortAtOffset(arg2, 2);
		short nestedStructElem2 = MemoryAccess.getShortAtOffset(arg2, 4);

		short shortSum = (short)(arg1 + structElem1 + nestedStructElem1 + nestedStructElem2);
		return shortSum;
	}

	public static short addShortAndShortsFromStructWithNestedShortArray(short arg1, MemorySegment arg2) {
		short nestedShortArrayElem1 = MemoryAccess.getShortAtOffset(arg2, 0);
		short nestedShortArrayElem2 = MemoryAccess.getShortAtOffset(arg2, 2);
		short structElem2 = MemoryAccess.getShortAtOffset(arg2, 4);

		short shortSum = (short)(arg1 + nestedShortArrayElem1 + nestedShortArrayElem2 + structElem2);
		return shortSum;
	}

	public static short addShortAndShortsFromStructWithNestedShortArray_reverseOrder(short arg1, MemorySegment arg2) {
		short structElem1 = MemoryAccess.getShortAtOffset(arg2, 0);
		short nestedShortArrayElem1 = MemoryAccess.getShortAtOffset(arg2, 2);
		short nestedShortArrayElem2 = MemoryAccess.getShortAtOffset(arg2, 4);

		short shortSum = (short)(arg1 + structElem1 + nestedShortArrayElem1 + nestedShortArrayElem2);
		return shortSum;
	}

	public static short addShortAndShortsFromStructWithNestedStructArray(short arg1, MemorySegment arg2) {
		short nestedStructArrayElem1_Elem1 = MemoryAccess.getShortAtOffset(arg2, 0);
		short nestedStructArrayElem1_Elem2 = MemoryAccess.getShortAtOffset(arg2, 2);
		short nestedStructArrayElem2_Elem1 = MemoryAccess.getShortAtOffset(arg2, 4);
		short nestedStructArrayElem2_Elem2 = MemoryAccess.getShortAtOffset(arg2, 6);
		short structElem2 = MemoryAccess.getShortAtOffset(arg2, 8);

		short shortSum = (short)(arg1 + structElem2
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2);
		return shortSum;
	}

	public static short addShortAndShortsFromStructWithNestedStructArray_reverseOrder(short arg1, MemorySegment arg2) {
		short structElem1 = MemoryAccess.getShortAtOffset(arg2, 0);
		short nestedStructArrayElem1_Elem1 = MemoryAccess.getShortAtOffset(arg2, 2);
		short nestedStructArrayElem1_Elem2 = MemoryAccess.getShortAtOffset(arg2, 4);
		short nestedStructArrayElem2_Elem1 = MemoryAccess.getShortAtOffset(arg2, 6);
		short nestedStructArrayElem2_Elem2 = MemoryAccess.getShortAtOffset(arg2, 8);

		short shortSum = (short)(arg1 + structElem1
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2);
		return shortSum;
	}

	public static MemorySegment add2ShortStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MemorySegment shortStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		short shortStruct_Elem1 = (short)((short)shortHandle1.get(arg1) + (short)shortHandle1.get(arg2));
		short shortStruct_Elem2 = (short)((short)shortHandle2.get(arg1) + (short)shortHandle2.get(arg2));
		shortHandle1.set(shortStructSegmt, shortStruct_Elem1);
		shortHandle2.set(shortStructSegmt, shortStruct_Elem2);
		return shortStructSegmt;
	}

	public static MemoryAddress add2ShortStructs_returnStructPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		MemorySegment arg1 = arg1Addr.asSegment(structLayout.byteSize(), arg1Addr.scope());
		short shortStruct_Elem1 = (short)((short)shortHandle1.get(arg1) + (short)shortHandle1.get(arg2));
		short shortStruct_Elem2 = (short)((short)shortHandle2.get(arg1) + (short)shortHandle2.get(arg2));
		shortHandle1.set(arg1, shortStruct_Elem1);
		shortHandle2.set(arg1, shortStruct_Elem2);
		return arg1Addr;
	}

	public static MemorySegment add3ShortStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"),
				C_SHORT.withName("elem3"), MemoryLayout.paddingLayout(16));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));
		VarHandle shortHandle3 = structLayout.varHandle(short.class, PathElement.groupElement("elem3"));

		MemorySegment shortStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		short shortStruct_Elem1 = (short)((short)shortHandle1.get(arg1) + (short)shortHandle1.get(arg2));
		short shortStruct_Elem2 = (short)((short)shortHandle2.get(arg1) + (short)shortHandle2.get(arg2));
		short shortStruct_Elem3 = (short)((short)shortHandle3.get(arg1) + (short)shortHandle3.get(arg2));
		shortHandle1.set(shortStructSegmt, shortStruct_Elem1);
		shortHandle2.set(shortStructSegmt, shortStruct_Elem2);
		shortHandle3.set(shortStructSegmt, shortStruct_Elem3);
		return shortStructSegmt;
	}

	public static int addIntAndIntsFromStruct(int arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		int intSum = arg1 + (int)intHandle1.get(arg2) + (int)intHandle2.get(arg2);
		return intSum;
	}

	public static int addIntAnd5IntsFromStruct(int arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"),
				C_INT.withName("elem3"), C_INT.withName("elem4"), C_INT.withName("elem5"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));
		VarHandle intHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));
		VarHandle intHandle4 = structLayout.varHandle(int.class, PathElement.groupElement("elem4"));
		VarHandle intHandle5 = structLayout.varHandle(int.class, PathElement.groupElement("elem5"));

		int intSum = arg1 + (int)intHandle1.get(arg2) + (int)intHandle2.get(arg2)
		+ (int)intHandle3.get(arg2) + (int)intHandle4.get(arg2) + (int)intHandle5.get(arg2);
		return intSum;
	}

	public static int addIntFromPointerAndIntsFromStruct(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_INT.byteSize(), arg1Addr.scope());
		int arg1 = MemoryAccess.getInt(arg1Segmt);
		int intSum = arg1 + (int)intHandle1.get(arg2) + (int)intHandle2.get(arg2);
		return intSum;
	}

	public static MemoryAddress addIntFromPointerAndIntsFromStruct_returnIntPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_INT.byteSize(), arg1Addr.scope());
		int arg1 = MemoryAccess.getInt(arg1Segmt);
		int intSum = arg1 + (int)intHandle1.get(arg2) + (int)intHandle2.get(arg2);
		MemoryAccess.setInt(arg1Segmt, intSum);
		return arg1Addr;
	}

	public static int addIntAndIntsFromStructPointer(int arg1, MemoryAddress arg2Addr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MemorySegment arg2 = arg2Addr.asSegment(structLayout.byteSize(), arg2Addr.scope());
		int intSum = arg1 + (int)intHandle1.get(arg2) + (int)intHandle2.get(arg2);
		return intSum;
	}

	public static int addIntAndIntsFromNestedStruct(int arg1, MemorySegment arg2) {
		int nestedStructElem1 = MemoryAccess.getIntAtOffset(arg2, 0);
		int nestedStructElem2 = MemoryAccess.getIntAtOffset(arg2, 4);
		int structElem2 = MemoryAccess.getIntAtOffset(arg2, 8);

		int intSum = arg1 + nestedStructElem1 + nestedStructElem2 + structElem2;
		return intSum;
	}

	public static int addIntAndIntsFromNestedStruct_reverseOrder(int arg1, MemorySegment arg2) {
		int structElem1 = MemoryAccess.getIntAtOffset(arg2, 0);
		int nestedStructElem1 = MemoryAccess.getIntAtOffset(arg2, 4);
		int nestedStructElem2 = MemoryAccess.getIntAtOffset(arg2, 8);

		int intSum = arg1 + structElem1 + nestedStructElem1 + nestedStructElem2;
		return intSum;
	}

	public static int addIntAndIntsFromStructWithNestedIntArray(int arg1, MemorySegment arg2) {
		int nestedIntArrayElem1 = MemoryAccess.getIntAtOffset(arg2, 0);
		int nestedIntArrayElem2 = MemoryAccess.getIntAtOffset(arg2, 4);
		int structElem2 = MemoryAccess.getIntAtOffset(arg2, 8);

		int intSum = arg1 + nestedIntArrayElem1 + nestedIntArrayElem2 + structElem2;
		return intSum;
	}

	public static int addIntAndIntsFromStructWithNestedIntArray_reverseOrder(int arg1, MemorySegment arg2) {
		int structElem1 = MemoryAccess.getIntAtOffset(arg2, 0);
		int nestedIntArrayElem1 = MemoryAccess.getIntAtOffset(arg2, 4);
		int nestedIntArrayElem2 = MemoryAccess.getIntAtOffset(arg2, 8);

		int intSum = arg1 + structElem1 + nestedIntArrayElem1 + nestedIntArrayElem2;
		return intSum;
	}

	public static int addIntAndIntsFromStructWithNestedStructArray(int arg1, MemorySegment arg2) {
		int nestedStructArrayElem1_Elem1 = MemoryAccess.getIntAtOffset(arg2, 0);
		int nestedStructArrayElem1_Elem2 = MemoryAccess.getIntAtOffset(arg2, 4);
		int nestedStructArrayElem2_Elem1 = MemoryAccess.getIntAtOffset(arg2, 8);
		int nestedStructArrayElem2_Elem2 = MemoryAccess.getIntAtOffset(arg2, 12);
		int structElem2 = MemoryAccess.getIntAtOffset(arg2, 16);

		int intSum = arg1 + structElem2
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2;
		return intSum;
	}

	public static int addIntAndIntsFromStructWithNestedStructArray_reverseOrder(int arg1, MemorySegment arg2) {
		int structElem1 = MemoryAccess.getIntAtOffset(arg2, 0);
		int nestedStructArrayElem1_Elem1 = MemoryAccess.getIntAtOffset(arg2, 4);
		int nestedStructArrayElem1_Elem2 = MemoryAccess.getIntAtOffset(arg2, 8);
		int nestedStructArrayElem2_Elem1 = MemoryAccess.getIntAtOffset(arg2, 12);
		int nestedStructArrayElem2_Elem2 = MemoryAccess.getIntAtOffset(arg2, 16);

		int intSum = arg1 + structElem1
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2;
		return intSum;
	}

	public static MemorySegment add2IntStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MemorySegment intStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		int intStruct_Elem1 = (int)intHandle1.get(arg1) + (int)intHandle1.get(arg2);
		int intStruct_Elem2 = (int)intHandle2.get(arg1) + (int)intHandle2.get(arg2);
		intHandle1.set(intStructSegmt, intStruct_Elem1);
		intHandle2.set(intStructSegmt, intStruct_Elem2);
		return intStructSegmt;
	}

	public static MemorySegment add2IntStructs_returnStruct_throwException(MemorySegment arg1, MemorySegment arg2) {
		throw new IllegalArgumentException("An exception is thrown from the upcall method");
	}

	public static MemorySegment add2IntStructs_returnStruct_nestedUpcall(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		MethodType mt = MethodType.methodType(MemorySegment.class, MemorySegment.class, MemorySegment.class, MemoryAddress.class);
		FunctionDescriptor fd = FunctionDescriptor.of(structLayout, structLayout, structLayout, C_POINTER);
		Addressable functionSymbol = nativeLibLookup.lookup("add2IntStructs_returnStructByUpcallMH").get();
		SegmentAllocator allocator = SegmentAllocator.ofScope(scope);
		MethodHandle mh = clinker.downcallHandle(functionSymbol, allocator, mt, fd);
		MemoryAddress upcallFuncAddr = clinker.upcallStub(UpcallMethodHandles.MH_add2IntStructs_returnStruct_throwException,
				FunctionDescriptor.of(structLayout, structLayout, structLayout), scope);
		MemorySegment resultSegmt = null;
		try {
			resultSegmt = (MemorySegment)mh.invoke(arg1, arg2, upcallFuncAddr);
		} catch (Throwable e) {
			throw (IllegalArgumentException)e;
		}
		return resultSegmt;
	}

	public static MemorySegment add2IntStructs_returnStruct_nullValue(MemorySegment arg1, MemorySegment arg2) {
		return null;
	}

	public static MemorySegment add2IntStructs_returnStruct_heapSegmt(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		int intStruct_Elem1 = (int)intHandle1.get(arg1) + (int)intHandle1.get(arg2);
		int intStruct_Elem2 = (int)intHandle2.get(arg1) + (int)intHandle2.get(arg2);
		return MemorySegment.ofArray(new int[]{intStruct_Elem1, intStruct_Elem2});
	}

	public static MemoryAddress add2IntStructs_returnStructPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MemorySegment arg1 = arg1Addr.asSegment(structLayout.byteSize(), arg1Addr.scope());
		int intSum_Elem1 = (int)intHandle1.get(arg1) + (int)intHandle1.get(arg2);
		int intSum_Elem2 = (int)intHandle2.get(arg1) + (int)intHandle2.get(arg2);
		intHandle1.set(arg1, intSum_Elem1);
		intHandle2.set(arg1, intSum_Elem2);
		return arg1Addr;
	}

	public static MemoryAddress add2IntStructs_returnStructPointer_nullValue(MemoryAddress arg1Addr, MemorySegment arg2) {
		return null;
	}

	public static MemoryAddress add2IntStructs_returnStructPointer_nullAddr(MemoryAddress arg1Addr, MemorySegment arg2) {
		return MemoryAddress.NULL;
	}

	public static MemoryAddress add2IntStructs_returnStructPointer_heapSegmt(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		MemorySegment arg1 = arg1Addr.asSegment(structLayout.byteSize(), arg1Addr.scope());
		int intSum_Elem1 = (int)intHandle1.get(arg1) + (int)intHandle1.get(arg2);
		int intSum_Elem2 = (int)intHandle2.get(arg1) + (int)intHandle2.get(arg2);
		MemorySegment resultSegmt = MemorySegment.ofArray(new int[]{intSum_Elem1, intSum_Elem2});
		return resultSegmt.address();
	}

	public static MemorySegment add3IntStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"), C_INT.withName("elem3"));
		VarHandle intHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle intHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));
		VarHandle intHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));

		MemorySegment intStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		int intStruct_Elem1 = (int)intHandle1.get(arg1) + (int)intHandle1.get(arg2);
		int intStruct_Elem2 = (int)intHandle2.get(arg1) + (int)intHandle2.get(arg2);
		int intStruct_Elem3 = (int)intHandle3.get(arg1) + (int)intHandle3.get(arg2);
		intHandle1.set(intStructSegmt, intStruct_Elem1);
		intHandle2.set(intStructSegmt, intStruct_Elem2);
		intHandle3.set(intStructSegmt, intStruct_Elem3);
		return intStructSegmt;
	}

	public static long addLongAndLongsFromStruct(long arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		long longSum = arg1 + (long)longHandle1.get(arg2) + (long)longHandle2.get(arg2);
		return longSum;
	}

	public static long addLongFromPointerAndLongsFromStruct(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(longLayout.byteSize(), arg1Addr.scope());
		long arg1 = MemoryAccess.getLong(arg1Segmt);
		long longSum = arg1 + (long)longHandle1.get(arg2) + (long)longHandle2.get(arg2);
		return longSum;
	}

	public static MemoryAddress addLongFromPointerAndLongsFromStruct_returnLongPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(longLayout.byteSize(), arg1Addr.scope());
		long arg1 = MemoryAccess.getLong(arg1Segmt);
		long longSum = arg1 + (long)longHandle1.get(arg2) + (long)longHandle2.get(arg2);
		MemoryAccess.setLong(arg1Segmt, longSum);
		return arg1Addr;
	}

	public static long addLongAndLongsFromStructPointer(long arg1, MemoryAddress arg2Addr) {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MemorySegment arg2 = arg2Addr.asSegment(structLayout.byteSize(), arg2Addr.scope());
		long longSum = arg1 + (long)longHandle1.get(arg2) + (long)longHandle2.get(arg2);
		return longSum;
	}

	public static long addLongAndLongsFromNestedStruct(long arg1, MemorySegment arg2) {
		long nestedStructElem1 = MemoryAccess.getLongAtOffset(arg2, 0);
		long nestedStructElem2 = MemoryAccess.getLongAtOffset(arg2, 8);
		long structElem2 = MemoryAccess.getLongAtOffset(arg2, 16);

		long longSum = arg1 + nestedStructElem1 + nestedStructElem2 + structElem2;
		return longSum;
	}

	public static long addLongAndLongsFromNestedStruct_reverseOrder(long arg1, MemorySegment arg2) {
		long structElem1 = MemoryAccess.getLongAtOffset(arg2, 0);
		long nestedStructElem1 = MemoryAccess.getLongAtOffset(arg2, 8);
		long nestedStructElem2 = MemoryAccess.getLongAtOffset(arg2, 16);

		long longSum = arg1 + structElem1 + nestedStructElem1 + nestedStructElem2;
		return longSum;
	}

	public static long addLongAndLongsFromStructWithNestedLongArray(long arg1, MemorySegment arg2) {
		long nestedLongrrayElem1 = MemoryAccess.getLongAtOffset(arg2, 0);
		long nestedLongrrayElem2 = MemoryAccess.getLongAtOffset(arg2, 8);
		long structElem2 = MemoryAccess.getLongAtOffset(arg2, 16);

		long longSum = arg1 + nestedLongrrayElem1 + nestedLongrrayElem2 + structElem2;
		return longSum;
	}

	public static long addLongAndLongsFromStructWithNestedLongArray_reverseOrder(long arg1, MemorySegment arg2) {
		long structElem1 = MemoryAccess.getLongAtOffset(arg2, 0);
		long nestedLongrrayElem1 = MemoryAccess.getLongAtOffset(arg2, 8);
		long nestedLongrrayElem2 = MemoryAccess.getLongAtOffset(arg2, 16);

		long longSum = arg1 + structElem1 + nestedLongrrayElem1 + nestedLongrrayElem2;
		return longSum;
	}

	public static long addLongAndLongsFromStructWithNestedStructArray(long arg1, MemorySegment arg2) {
		long nestedStructArrayElem1_Elem1 = MemoryAccess.getLongAtOffset(arg2, 0);
		long nestedStructArrayElem1_Elem2 = MemoryAccess.getLongAtOffset(arg2, 8);
		long nestedStructArrayElem2_Elem1 = MemoryAccess.getLongAtOffset(arg2, 16);
		long nestedStructArrayElem2_Elem2 = MemoryAccess.getLongAtOffset(arg2, 24);
		long structElem2 = MemoryAccess.getLongAtOffset(arg2, 32);

		long longSum = arg1 + structElem2
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2;
		return longSum;
	}

	public static long addLongAndLongsFromStructWithNestedStructArray_reverseOrder(long arg1, MemorySegment arg2) {
		long structElem1 = MemoryAccess.getLongAtOffset(arg2, 0);
		long nestedStructArrayElem1_Elem1 = MemoryAccess.getLongAtOffset(arg2, 8);
		long nestedStructArrayElem1_Elem2 = MemoryAccess.getLongAtOffset(arg2, 16);
		long nestedStructArrayElem2_Elem1 = MemoryAccess.getLongAtOffset(arg2, 24);
		long nestedStructArrayElem2_Elem2 = MemoryAccess.getLongAtOffset(arg2, 32);

		long longSum = arg1 + structElem1
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2;
		return longSum;
	}

	public static MemorySegment add2LongStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MemorySegment longStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		long longStruct_Elem1 = (long)longHandle1.get(arg1) + (long)longHandle1.get(arg2);
		long longStruct_Elem2 = (long)longHandle2.get(arg1) + (long)longHandle2.get(arg2);
		longHandle1.set(longStructSegmt, longStruct_Elem1);
		longHandle2.set(longStructSegmt, longStruct_Elem2);
		return longStructSegmt;
	}

	public static MemoryAddress add2LongStructs_returnStructPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		MemorySegment arg1 = arg1Addr.asSegment(structLayout.byteSize(), arg1Addr.scope());
		long longSum_Elem1 = (long)longHandle1.get(arg1) + (long)longHandle1.get(arg2);
		long longSum_Elem2 = (long)longHandle2.get(arg1) + (long)longHandle2.get(arg2);
		longHandle1.set(arg1, longSum_Elem1);
		longHandle2.set(arg1, longSum_Elem2);
		return arg1Addr;
	}

	public static MemorySegment add3LongStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"), longLayout.withName("elem3"));
		VarHandle longHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle longHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));
		VarHandle longHandle3 = structLayout.varHandle(long.class, PathElement.groupElement("elem3"));

		MemorySegment longStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		long longStruct_Elem1 = (long)longHandle1.get(arg1) + (long)longHandle1.get(arg2);
		long longStruct_Elem2 = (long)longHandle2.get(arg1) + (long)longHandle2.get(arg2);
		long longStruct_Elem3 = (long)longHandle3.get(arg1) + (long)longHandle3.get(arg2);
		longHandle1.set(longStructSegmt, longStruct_Elem1);
		longHandle2.set(longStructSegmt, longStruct_Elem2);
		longHandle3.set(longStructSegmt, longStruct_Elem3);
		return longStructSegmt;
	}

	public static float addFloatAndFloatsFromStruct(float arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		float floatSum = arg1 + (float)floatHandle1.get(arg2) + (float)floatHandle2.get(arg2);
		return floatSum;
	}

	public static float addFloatAnd5FloatsFromStruct(float arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"),
				C_FLOAT.withName("elem3"), C_FLOAT.withName("elem4"), C_FLOAT.withName("elem5"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle floatHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));
		VarHandle floatHandle4 = structLayout.varHandle(float.class, PathElement.groupElement("elem4"));
		VarHandle floatHandle5 = structLayout.varHandle(float.class, PathElement.groupElement("elem5"));

		float floatSum = arg1 + (float)floatHandle1.get(arg2) + (float)floatHandle2.get(arg2)
		+ (float)floatHandle3.get(arg2) + (float)floatHandle4.get(arg2) + (float)floatHandle5.get(arg2);
		return floatSum;
	}

	public static float addFloatFromPointerAndFloatsFromStruct(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_FLOAT.byteSize(), arg1Addr.scope());
		float arg1 = MemoryAccess.getFloat(arg1Segmt);
		float floatSum = arg1 + (float)floatHandle1.get(arg2) + (float)floatHandle2.get(arg2);
		return floatSum;
	}

	public static MemoryAddress addFloatFromPointerAndFloatsFromStruct_returnFloatPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_FLOAT.byteSize(), arg1Addr.scope());
		float arg1 = MemoryAccess.getFloat(arg1Segmt);
		float floatSum = arg1 + (float)floatHandle1.get(arg2) + (float)floatHandle2.get(arg2);
		MemoryAccess.setFloat(arg1Segmt, floatSum);
		return arg1Addr;
	}

	public static float addFloatAndFloatsFromStructPointer(float arg1, MemoryAddress arg2Addr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MemorySegment arg2 = arg2Addr.asSegment(structLayout.byteSize(), arg2Addr.scope());
		float floatSum = arg1 + (float)floatHandle1.get(arg2) + (float)floatHandle2.get(arg2);
		return floatSum;
	}

	public static float addFloatAndFloatsFromNestedStruct(float arg1, MemorySegment arg2) {
		float nestedStructElem1 = MemoryAccess.getFloatAtOffset(arg2, 0);
		float nestedStructElem2 = MemoryAccess.getFloatAtOffset(arg2, 4);
		float structElem2 = MemoryAccess.getFloatAtOffset(arg2, 8);

		float floatSum = arg1 + nestedStructElem1 + nestedStructElem2 + structElem2;
		return floatSum;
	}

	public static float addFloatAndFloatsFromNestedStruct_reverseOrder(float arg1, MemorySegment arg2) {
		float structElem1 = MemoryAccess.getFloatAtOffset(arg2, 0);
		float nestedStructElem1 = MemoryAccess.getFloatAtOffset(arg2, 4);
		float nestedStructElem2 = MemoryAccess.getFloatAtOffset(arg2, 8);

		float floatSum = arg1 + structElem1 + nestedStructElem1 + nestedStructElem2;
		return floatSum;
	}

	public static float addFloatAndFloatsFromStructWithNestedFloatArray(float arg1, MemorySegment arg2) {
		float nestedFloatArrayElem1 = MemoryAccess.getFloatAtOffset(arg2, 0);
		float nestedFloatArrayElem2 = MemoryAccess.getFloatAtOffset(arg2, 4);
		float structElem2 = MemoryAccess.getFloatAtOffset(arg2, 8);

		float floatSum = arg1 + nestedFloatArrayElem1 + nestedFloatArrayElem2 + structElem2;
		return floatSum;
	}

	public static float addFloatAndFloatsFromStructWithNestedFloatArray_reverseOrder(float arg1, MemorySegment arg2) {
		float structElem1 = MemoryAccess.getFloatAtOffset(arg2, 0);
		float nestedFloatArrayElem1 = MemoryAccess.getFloatAtOffset(arg2, 4);
		float nestedFloatArrayElem2 = MemoryAccess.getFloatAtOffset(arg2, 8);

		float floatSum = arg1 + structElem1 + nestedFloatArrayElem1 + nestedFloatArrayElem2;
		return floatSum;
	}

	public static float addFloatAndFloatsFromStructWithNestedStructArray(float arg1, MemorySegment arg2) {
		float nestedStructArrayElem1_Elem1 = MemoryAccess.getFloatAtOffset(arg2, 0);
		float nestedStructArrayElem1_Elem2 = MemoryAccess.getFloatAtOffset(arg2, 4);
		float nestedStructArrayElem2_Elem1 = MemoryAccess.getFloatAtOffset(arg2, 8);
		float nestedStructArrayElem2_Elem2 = MemoryAccess.getFloatAtOffset(arg2, 12);
		float structElem2 = MemoryAccess.getFloatAtOffset(arg2, 16);

		float floatSum = arg1 + structElem2
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2;
		return floatSum;
	}

	public static float addFloatAndFloatsFromStructWithNestedStructArray_reverseOrder(float arg1, MemorySegment arg2) {
		float structElem1 = MemoryAccess.getFloatAtOffset(arg2, 0);
		float nestedStructArrayElem1_Elem1 = MemoryAccess.getFloatAtOffset(arg2, 4);
		float nestedStructArrayElem1_Elem2 = MemoryAccess.getFloatAtOffset(arg2, 8);
		float nestedStructArrayElem2_Elem1 = MemoryAccess.getFloatAtOffset(arg2, 12);
		float nestedStructArrayElem2_Elem2 = MemoryAccess.getFloatAtOffset(arg2, 16);

		float floatSum = arg1 + structElem1
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2;
		return floatSum;
	}

	public static MemorySegment add2FloatStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MemorySegment floatStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		float floatStruct_Elem1 = (float)floatHandle1.get(arg1) + (float)floatHandle1.get(arg2);
		float floatStruct_Elem2 = (float)floatHandle2.get(arg1) + (float)floatHandle2.get(arg2);
		floatHandle1.set(floatStructSegmt, floatStruct_Elem1);
		floatHandle2.set(floatStructSegmt, floatStruct_Elem2);
		return floatStructSegmt;
	}

	public static MemoryAddress add2FloatStructs_returnStructPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		MemorySegment arg1 = arg1Addr.asSegment(structLayout.byteSize(), arg1Addr.scope());
		float floatSum_Elem1 = (float)floatHandle1.get(arg1) + (float)floatHandle1.get(arg2);
		float floatSum_Elem2 = (float)floatHandle2.get(arg1) + (float)floatHandle2.get(arg2);
		floatHandle1.set(arg1, floatSum_Elem1);
		floatHandle2.set(arg1, floatSum_Elem2);
		return arg1Addr;
	}

	public static MemorySegment add3FloatStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"),  C_FLOAT.withName("elem3"));
		VarHandle floatHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle floatHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle floatHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		MemorySegment floatStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		float floatStruct_Elem1 = (float)floatHandle1.get(arg1) + (float)floatHandle1.get(arg2);
		float floatStruct_Elem2 = (float)floatHandle2.get(arg1) + (float)floatHandle2.get(arg2);
		float floatStruct_Elem3 = (float)floatHandle3.get(arg1) + (float)floatHandle3.get(arg2);
		floatHandle1.set(floatStructSegmt, floatStruct_Elem1);
		floatHandle2.set(floatStructSegmt, floatStruct_Elem2);
		floatHandle3.set(floatStructSegmt, floatStruct_Elem3);
		return floatStructSegmt;
	}

	public static double addDoubleAndDoublesFromStruct(double arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		double doubleSum = arg1 + (double)doubleHandle1.get(arg2) + (double)doubleHandle2.get(arg2);
		return doubleSum;
	}

	public static double addDoubleFromPointerAndDoublesFromStruct(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_DOUBLE.byteSize(), arg1Addr.scope());
		double arg1 = MemoryAccess.getDouble(arg1Segmt);
		double doubleSum = arg1 + (double)doubleHandle1.get(arg2) + (double)doubleHandle2.get(arg2);
		return doubleSum;
	}

	public static MemoryAddress addDoubleFromPointerAndDoublesFromStruct_returnDoublePointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MemorySegment arg1Segmt = arg1Addr.asSegment(C_DOUBLE.byteSize(), arg1Addr.scope());
		double arg1 = MemoryAccess.getDouble(arg1Segmt);
		double doubleSum = arg1 + (double)doubleHandle1.get(arg2) + (double)doubleHandle2.get(arg2);
		MemoryAccess.setDouble(arg1Segmt, doubleSum);
		return arg1Addr;
	}

	public static double addDoubleAndDoublesFromStructPointer(double arg1, MemoryAddress arg2Addr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MemorySegment arg2 = arg2Addr.asSegment(structLayout.byteSize(), arg2Addr.scope());
		double doubleSum = arg1 + (double)doubleHandle1.get(arg2) + (double)doubleHandle2.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAndDoublesFromNestedStruct(double arg1, MemorySegment arg2) {
		double nestedStructElem1 = MemoryAccess.getDoubleAtOffset(arg2, 0);
		double nestedStructElem2 = MemoryAccess.getDoubleAtOffset(arg2, 8);
		double structElem2 = MemoryAccess.getDoubleAtOffset(arg2, 16);

		double doubleSum = arg1 + nestedStructElem1 + nestedStructElem2 + structElem2;
		return doubleSum;
	}

	public static double addDoubleAndDoublesFromNestedStruct_reverseOrder(double arg1, MemorySegment arg2) {
		double structElem1 = MemoryAccess.getDoubleAtOffset(arg2, 0);
		double nestedStructElem1 = MemoryAccess.getDoubleAtOffset(arg2, 8);
		double nestedStructElem2 = MemoryAccess.getDoubleAtOffset(arg2, 16);

		double doubleSum = arg1 + structElem1 + nestedStructElem1 + nestedStructElem2;
		return doubleSum;
	}

	public static double addDoubleAndDoublesFromStructWithNestedDoubleArray(double arg1, MemorySegment arg2) {
		double nestedDoubleArrayElem1 = MemoryAccess.getDoubleAtOffset(arg2, 0);
		double nestedDoubleArrayElem2 = MemoryAccess.getDoubleAtOffset(arg2, 8);
		double structElem2 = MemoryAccess.getDoubleAtOffset(arg2, 16);

		double doubleSum = arg1 + nestedDoubleArrayElem1 + nestedDoubleArrayElem2 + structElem2;
		return doubleSum;
	}

	public static double addDoubleAndDoublesFromStructWithNestedDoubleArray_reverseOrder(double arg1, MemorySegment arg2) {
		double structElem1 = MemoryAccess.getDoubleAtOffset(arg2, 0);
		double nestedDoubleArrayElem1 = MemoryAccess.getDoubleAtOffset(arg2, 8);
		double nestedDoubleArrayElem2 = MemoryAccess.getDoubleAtOffset(arg2, 16);

		double doubleSum = arg1 + structElem1 + nestedDoubleArrayElem1 + nestedDoubleArrayElem2;
		return doubleSum;
	}

	public static double addDoubleAndDoublesFromStructWithNestedStructArray(double arg1, MemorySegment arg2) {
		double nestedStructArrayElem1_Elem1 = MemoryAccess.getDoubleAtOffset(arg2, 0);
		double nestedStructArrayElem1_Elem2 = MemoryAccess.getDoubleAtOffset(arg2, 8);
		double nestedStructArrayElem2_Elem1 = MemoryAccess.getDoubleAtOffset(arg2, 16);
		double nestedStructArrayElem2_Elem2 = MemoryAccess.getDoubleAtOffset(arg2, 24);
		double structElem2 = MemoryAccess.getDoubleAtOffset(arg2, 32);

		double doubleSum = arg1 + structElem2
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2;
		return doubleSum;
	}

	public static double addDoubleAndDoublesFromStructWithNestedStructArray_reverseOrder(double arg1, MemorySegment arg2) {
		double structElem1 = MemoryAccess.getDoubleAtOffset(arg2, 0);
		double nestedStructArrayElem1_Elem1 = MemoryAccess.getDoubleAtOffset(arg2, 8);
		double nestedStructArrayElem1_Elem2 = MemoryAccess.getDoubleAtOffset(arg2, 16);
		double nestedStructArrayElem2_Elem1 = MemoryAccess.getDoubleAtOffset(arg2, 24);
		double nestedStructArrayElem2_Elem2 = MemoryAccess.getDoubleAtOffset(arg2, 32);

		double doubleSum = arg1 + structElem1
				+ nestedStructArrayElem1_Elem1 + nestedStructArrayElem1_Elem2
				+ nestedStructArrayElem2_Elem1 + nestedStructArrayElem2_Elem2;
		return doubleSum;
	}

	public static MemorySegment add2DoubleStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MemorySegment doubleStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		double doubleStruct_Elem1 = (double)doubleHandle1.get(arg1) + (double)doubleHandle1.get(arg2);
		double doubleStruct_Elem2 = (double)doubleHandle2.get(arg1) + (double)doubleHandle2.get(arg2);
		doubleHandle1.set(doubleStructSegmt, doubleStruct_Elem1);
		doubleHandle2.set(doubleStructSegmt, doubleStruct_Elem2);
		return doubleStructSegmt;
	}

	public static MemoryAddress add2DoubleStructs_returnStructPointer(MemoryAddress arg1Addr, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		MemorySegment arg1 = arg1Addr.asSegment(structLayout.byteSize(), arg1Addr.scope());
		double doubleSum_Elem1 = (double)doubleHandle1.get(arg1) + (double)doubleHandle1.get(arg2);
		double doubleSum_Elem2 = (double)doubleHandle2.get(arg1) + (double)doubleHandle2.get(arg2);
		doubleHandle1.set(arg1, doubleSum_Elem1);
		doubleHandle2.set(arg1, doubleSum_Elem2);
		return arg1Addr;
	}

	public static MemorySegment add3DoubleStructs_returnStruct(MemorySegment arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"), C_DOUBLE.withName("elem3"));
		VarHandle doubleHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle doubleHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));
		VarHandle doubleHandle3 = structLayout.varHandle(double.class, PathElement.groupElement("elem3"));

		MemorySegment doubleStructSegmt = MemorySegment.allocateNative(structLayout, scope);
		double doubleStruct_Elem1 = (double)doubleHandle1.get(arg1) + (double)doubleHandle1.get(arg2);
		double doubleStruct_Elem2 = (double)doubleHandle2.get(arg1) + (double)doubleHandle2.get(arg2);
		double doubleStruct_Elem3 = (double)doubleHandle3.get(arg1) + (double)doubleHandle3.get(arg2);
		doubleHandle1.set(doubleStructSegmt, doubleStruct_Elem1);
		doubleHandle2.set(doubleStructSegmt, doubleStruct_Elem2);
		doubleHandle3.set(doubleStructSegmt, doubleStruct_Elem3);
		return doubleStructSegmt;
	}

	public static int addIntAndIntShortFromStruct(int arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_SHORT.withName("elem2"), MemoryLayout.paddingLayout(16));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		int intSum = arg1 + (int)elemHandle1.get(arg2) + (short)elemHandle2.get(arg2);
		return intSum;
	}

	public static int addIntAndShortIntFromStruct(int arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"),
				MemoryLayout.paddingLayout(16), C_INT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		int intSum = arg1 + (short)elemHandle1.get(arg2) + (int)elemHandle2.get(arg2);
		return intSum;
	}

	public static long addIntAndIntLongFromStruct(int arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				MemoryLayout.paddingLayout(32), longLayout.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		long longSum = arg1 + (int)elemHandle1.get(arg2) + (long)elemHandle2.get(arg2);
		return longSum;
	}

	public static long addIntAndLongIntFromStruct(int arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"),
				C_INT.withName("elem2"), MemoryLayout.paddingLayout(32));
		VarHandle elemHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		long longSum = arg1 + (long)elemHandle1.get(arg2) + (int)elemHandle2.get(arg2);
		return longSum;
	}

	public static double addDoubleAndIntDoubleFromStruct(double arg1, MemorySegment arg2) {
		/* The size of [int, double] on AIX/PPC 64-bit is 12 bytes without padding by default
		 * while the same struct is 16 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_DOUBLE.withName("elem2")) : MemoryLayout.structLayout(C_INT.withName("elem1"),
						MemoryLayout.paddingLayout(32), C_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		double doubleSum = arg1 + (int)elemHandle1.get(arg2) + (double)elemHandle2.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAndDoubleIntFromStruct(double arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_INT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		double doubleSum = arg1 + (double)elemHandle1.get(arg2) + (int)elemHandle2.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAndFloatDoubleFromStruct(double arg1, MemorySegment arg2) {
		/* The size of [float, double] on AIX/PPC 64-bit is 12 bytes without padding by default
		 * while the same struct is 16 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_DOUBLE.withName("elem2")) : MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
						MemoryLayout.paddingLayout(32), C_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		double doubleSum = arg1 + (float)elemHandle1.get(arg2) + (double)elemHandle2.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAndDoubleFloatFromStruct(double arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		double doubleSum = arg1 + (double)elemHandle1.get(arg2) + (float)elemHandle2.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAnd2FloatsDoubleFromStruct(double arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_DOUBLE.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(double.class, PathElement.groupElement("elem3"));

		double doubleSum = arg1 + (float)elemHandle1.get(arg2) + (float)elemHandle2.get(arg2) + (double)elemHandle3.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAndDouble2FloatsFromStruct(double arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"),
				C_FLOAT.withName("elem2"), C_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		double doubleSum = arg1 + (double)elemHandle1.get(arg2) + (float)elemHandle2.get(arg2) + (float)elemHandle3.get(arg2);
		return doubleSum;
	}

	public static float addFloatAndInt2FloatsFromStruct(float arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		float floatSum = arg1 + (int)elemHandle1.get(arg2) + (float)elemHandle2.get(arg2) + (float)elemHandle3.get(arg2);
		return floatSum;
	}

	public static float addFloatAndFloatIntFloatFromStruct(float arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_INT.withName("elem2"), C_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		float structElem2 = Integer.valueOf((int)elemHandle2.get(arg2)).floatValue();
		float floatSum = arg1 + (float)elemHandle1.get(arg2) + structElem2 + (float)elemHandle3.get(arg2);
		return floatSum;
	}

	public static double addDoubleAndIntFloatDoubleFromStruct(double arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_DOUBLE.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(double.class, PathElement.groupElement("elem3"));

		double doubleSum = arg1 + (int)elemHandle1.get(arg2) + (float)elemHandle2.get(arg2) + (double)elemHandle3.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAndFloatIntDoubleFromStruct(double arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_INT.withName("elem2"), C_DOUBLE.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(double.class, PathElement.groupElement("elem3"));

		double doubleSum = arg1 + (float)elemHandle1.get(arg2) + (int)elemHandle2.get(arg2) + (double)elemHandle3.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAndLongDoubleFromStruct(double arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		double doubleSum = arg1 + (long)elemHandle1.get(arg2) + (double)elemHandle2.get(arg2);
		return doubleSum;
	}

	public static float addFloatAndInt3FloatsFromStruct(float arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_FLOAT.withName("elem3"), C_FLOAT.withName("elem4"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));
		VarHandle elemHandle4 = structLayout.varHandle(float.class, PathElement.groupElement("elem4"));

		float floatSum = arg1 + (int)elemHandle1.get(arg2) + (float)elemHandle2.get(arg2)
		+ (float)elemHandle3.get(arg2) + (float)elemHandle4.get(arg2);
		return floatSum;
	}

	public static long addLongAndLong2FloatsFromStruct(long arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"),
				C_FLOAT.withName("elem2"), C_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		long structElem1 = (long)elemHandle1.get(arg2);
		long structElem2 = Float.valueOf((float)elemHandle2.get(arg2)).longValue();
		long structElem3 = Float.valueOf((float)elemHandle3.get(arg2)).longValue();
		long longSum = arg1 + structElem1 + structElem2 + structElem3;
		return longSum;
	}

	public static float addFloatAnd3FloatsIntFromStruct(float arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_FLOAT.withName("elem3"), C_INT.withName("elem4"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));
		VarHandle elemHandle4 = structLayout.varHandle(int.class, PathElement.groupElement("elem4"));

		float floatSum = arg1 + (float)elemHandle1.get(arg2) + (float)elemHandle2.get(arg2)
		+ (float)elemHandle3.get(arg2) + (int)elemHandle4.get(arg2);
		return floatSum;
	}

	public static long addLongAndFloatLongFromStruct(long arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
					MemoryLayout.paddingLayout(32), longLayout.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		long structElem1 = Float.valueOf((float)elemHandle1.get(arg2)).longValue();
		long longSum = arg1 + structElem1 + (long)elemHandle2.get(arg2);
		return longSum;
	}

	public static double addDoubleAndDoubleFloatIntFromStruct(double arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"),
				C_FLOAT.withName("elem2"), C_INT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));

		double doubleSum = arg1 + (double)elemHandle1.get(arg2) + (float)elemHandle2.get(arg2) + (int)elemHandle3.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAndDoubleLongFromStruct(double arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), longLayout.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		double doubleSum = arg1 + (double)elemHandle1.get(arg2) + (long)elemHandle2.get(arg2);
		return doubleSum;
	}

	public static long addLongAnd2FloatsLongFromStruct(long arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_FLOAT.withName("elem2"), longLayout.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(long.class, PathElement.groupElement("elem3"));

		long structElem1 = Float.valueOf((float)elemHandle1.get(arg2)).longValue();
		long structElem2 = Float.valueOf((float)elemHandle2.get(arg2)).longValue();
		long structElem3 = (long)elemHandle3.get(arg2);
		long longSum = arg1 + structElem1 + structElem2 + structElem3;
		return longSum;
	}

	public static short addShortAnd3ShortsCharFromStruct(short arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"),
				C_SHORT.withName("elem2"), C_SHORT.withName("elem3"), C_SHORT.withName("elem4"));
		VarHandle elemHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(short.class, PathElement.groupElement("elem3"));
		VarHandle elemHandle4 = structLayout.varHandle(char.class, PathElement.groupElement("elem4"));

		short shortSum = (short)(arg1 + (short)elemHandle1.get(arg2) + (short)elemHandle2.get(arg2)
		+ (short)elemHandle3.get(arg2) + (char)elemHandle4.get(arg2));
		return shortSum;
	}

	public static float addFloatAndIntFloatIntFloatFromStruct(float arg1, MemorySegment arg2) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_INT.withName("elem3"), C_FLOAT.withName("elem4"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));
		VarHandle elemHandle4 = structLayout.varHandle(float.class, PathElement.groupElement("elem4"));

		float floatSum = arg1 + (int)elemHandle1.get(arg2) + (float)elemHandle2.get(arg2)
		+ (int)elemHandle3.get(arg2) + (float)elemHandle4.get(arg2);
		return floatSum;
	}

	public static double addDoubleAndIntDoubleFloatFromStruct(double arg1, MemorySegment arg2) {
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

		double doubleSum = arg1 + (int)elemHandle1.get(arg2) + (double)elemHandle2.get(arg2) + (float)elemHandle3.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAndFloatDoubleIntFromStruct(double arg1, MemorySegment arg2) {
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

		double doubleSum = arg1 + (float)elemHandle1.get(arg2) + (double)elemHandle2.get(arg2) + (int)elemHandle3.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAndIntDoubleIntFromStruct(double arg1, MemorySegment arg2) {
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

		double doubleSum = arg1 + (int)elemHandle1.get(arg2) + (double)elemHandle2.get(arg2) + (int)elemHandle3.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAndFloatDoubleFloatFromStruct(double arg1, MemorySegment arg2) {
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

		double doubleSum = arg1 + (float)elemHandle1.get(arg2) + (double)elemHandle2.get(arg2) + (float)elemHandle3.get(arg2);
		return doubleSum;
	}

	public static double addDoubleAndIntDoubleLongFromStruct(double arg1, MemorySegment arg2) {
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

		double doubleSum = arg1 + (int)elemHandle1.get(arg2) + (double)elemHandle2.get(arg2) + (long)elemHandle3.get(arg2);
		return doubleSum;
	}

	public static MemorySegment return254BytesFromStruct() {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(254, C_CHAR);
		GroupLayout structLayout = MemoryLayout.structLayout(byteArray);
		MemorySegment byteArrStruSegment = MemorySegment.allocateNative(structLayout, scope);

		for (int i = 0; i < 254; i++) {
			MemoryAccess.setByteAtOffset(byteArrStruSegment, i, (byte)i);
		}
		return byteArrStruSegment;
	}

	public static MemorySegment return4KBytesFromStruct() {
		SequenceLayout byteArray = MemoryLayout.sequenceLayout(4096, C_CHAR);
		GroupLayout structLayout = MemoryLayout.structLayout(byteArray);
		MemorySegment byteArrStruSegment = MemorySegment.allocateNative(structLayout, scope);

		for (int i = 0; i < 4096; i++) {
			MemoryAccess.setByteAtOffset(byteArrStruSegment, i, (byte)i);
		}
		return byteArrStruSegment;
	}

	public static byte addNegBytesFromStruct(byte arg1, MemorySegment arg2, byte arg3, byte arg4) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle byteHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle byteHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));
		byte arg2_elem1 = (byte)byteHandle1.get(arg2);
		byte arg2_elem2 = (byte)byteHandle2.get(arg2);

		Assert.assertEquals((byte)-6, ((Byte)arg1).byteValue());
		Assert.assertEquals((byte)-8, ((Byte)arg2_elem1).byteValue());
		Assert.assertEquals((byte)-9, ((Byte)arg2_elem2).byteValue());
		Assert.assertEquals((byte)-8, ((Byte)arg3).byteValue());
		Assert.assertEquals((byte)-9, ((Byte)arg4).byteValue());

		byte byteSum = (byte)(arg1 + arg2_elem1 + arg2_elem2 + arg3 + arg4);
		return byteSum;
	}

	public static short addNegShortsFromStruct(short arg1,  MemorySegment arg2, short arg3, short arg4) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle shortHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle shortHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));
		short arg2_elem1 = (short)shortHandle1.get(arg2);
		short arg2_elem2 = (short)shortHandle2.get(arg2);

		Assert.assertEquals((short)-777, ((Short)arg1).shortValue());
		Assert.assertEquals((short)-888, ((Short)arg2_elem1).shortValue());
		Assert.assertEquals((short)-999, ((Short)arg2_elem2).shortValue());
		Assert.assertEquals((short)-888, ((Short)arg3).shortValue());
		Assert.assertEquals((short)-999, ((Short)arg4).shortValue());

		short shortSum = (short)(arg1 + arg2_elem1 + arg2_elem2 + arg3 + arg4);
		return shortSum;
	}
}
