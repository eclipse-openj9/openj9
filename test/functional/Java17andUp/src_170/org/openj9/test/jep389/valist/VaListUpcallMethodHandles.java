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
package org.openj9.test.jep389.valist;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import static java.lang.invoke.MethodType.methodType;
import java.lang.invoke.VarHandle;
import java.nio.ByteOrder;

import static jdk.incubator.foreign.CLinker.C_CHAR;
import static jdk.incubator.foreign.CLinker.C_DOUBLE;
import static jdk.incubator.foreign.CLinker.C_FLOAT;
import static jdk.incubator.foreign.CLinker.C_INT;
import static jdk.incubator.foreign.CLinker.C_LONG;
import static jdk.incubator.foreign.CLinker.C_LONG_LONG;
import static jdk.incubator.foreign.CLinker.C_POINTER;
import static jdk.incubator.foreign.CLinker.C_SHORT;
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
 * The helper class that contains all upcall method handles with VaList as arguments
 *
 * Note: VaList is simply treated as a pointer (specified in OpenJDK) in java
 * when va_list is passed as argument in native.
 */
public class VaListUpcallMethodHandles {
	private static final Lookup lookup = MethodHandles.lookup();
	private static ResourceScope scope = ResourceScope.newImplicitScope();
	private static SegmentAllocator allocator = SegmentAllocator.arenaAllocator(scope);

	private static String osName = System.getProperty("os.name").toLowerCase();
	private static boolean isAixOS = osName.contains("aix");
	private static boolean isWinOS = osName.contains("win");
	/* long long is 64 bits on AIX/ppc64, which is the same as Windows */
	private static ValueLayout longLayout = (isWinOS || isAixOS) ? C_LONG_LONG : C_LONG;

	static final MethodType MT_Byte_Int_MemAddr = methodType(byte.class, int.class, MemoryAddress.class);
	static final MethodType MT_Short_Int_MemAddr = methodType(short.class, int.class, MemoryAddress.class);
	static final MethodType MT_Int_Int_MemAddr = methodType(int.class, int.class, MemoryAddress.class);
	static final MethodType MT_Long_Int_MemAddr = methodType(long.class, int.class, MemoryAddress.class);
	static final MethodType MT_Float_Int_MemAddr = methodType(float.class, int.class, MemoryAddress.class);
	static final MethodType MT_Double_Int_MemAddr = methodType(double.class, int.class, MemoryAddress.class);

	public static final MethodHandle MH_addIntsFromVaList;
	public static final MethodHandle MH_addLongsFromVaList;
	public static final MethodHandle MH_addDoublesFromVaList;
	public static final MethodHandle MH_addMixedArgsFromVaList;
	public static final MethodHandle MH_addMoreMixedArgsFromVaList;
	public static final MethodHandle MH_addIntsByPtrFromVaList;
	public static final MethodHandle MH_addLongsByPtrFromVaList;
	public static final MethodHandle MH_addDoublesByPtrFromVaList;
	public static final MethodHandle MH_add1ByteOfStructsFromVaList;
	public static final MethodHandle MH_add2BytesOfStructsFromVaList;
	public static final MethodHandle MH_add3BytesOfStructsFromVaList;
	public static final MethodHandle MH_add5BytesOfStructsFromVaList;
	public static final MethodHandle MH_add7BytesOfStructsFromVaList;
	public static final MethodHandle MH_add1ShortOfStructsFromVaList;
	public static final MethodHandle MH_add2ShortsOfStructsFromVaList;
	public static final MethodHandle MH_add3ShortsOfStructsFromVaList;
	public static final MethodHandle MH_add1IntOfStructsFromVaList;
	public static final MethodHandle MH_add2IntsOfStructsFromVaList;
	public static final MethodHandle MH_add3IntsOfStructsFromVaList;
	public static final MethodHandle MH_add2LongsOfStructsFromVaList;
	public static final MethodHandle MH_add1FloatOfStructsFromVaList;
	public static final MethodHandle MH_add2FloatsOfStructsFromVaList;
	public static final MethodHandle MH_add3FloatsOfStructsFromVaList;
	public static final MethodHandle MH_add1DoubleOfStructsFromVaList;
	public static final MethodHandle MH_add2DoublesOfStructsFromVaList;
	public static final MethodHandle MH_addIntShortOfStructsFromVaList;
	public static final MethodHandle MH_addShortIntOfStructsFromVaList;
	public static final MethodHandle MH_addIntLongOfStructsFromVaList;
	public static final MethodHandle MH_addLongIntOfStructsFromVaList;
	public static final MethodHandle MH_addFloatDoubleOfStructsFromVaList;
	public static final MethodHandle MH_addDoubleFloatOfStructsFromVaList;

	static {
		try {
			MH_addIntsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addIntsFromVaList", MT_Int_Int_MemAddr); //$NON-NLS-1$
			MH_addLongsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addLongsFromVaList", MT_Long_Int_MemAddr); //$NON-NLS-1$
			MH_addDoublesFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addDoublesFromVaList", MT_Double_Int_MemAddr); //$NON-NLS-1$
			MH_addMixedArgsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addMixedArgsFromVaList", methodType(double.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addMoreMixedArgsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addMoreMixedArgsFromVaList", methodType(double.class, MemoryAddress.class)); //$NON-NLS-1$
			MH_addIntsByPtrFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addIntsByPtrFromVaList", MT_Int_Int_MemAddr); //$NON-NLS-1$
			MH_addLongsByPtrFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addLongsByPtrFromVaList", MT_Long_Int_MemAddr); //$NON-NLS-1$
			MH_addDoublesByPtrFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addDoublesByPtrFromVaList", MT_Double_Int_MemAddr); //$NON-NLS-1$
			MH_add1ByteOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add1ByteOfStructsFromVaList", MT_Byte_Int_MemAddr); //$NON-NLS-1$
			MH_add2BytesOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add2BytesOfStructsFromVaList", MT_Byte_Int_MemAddr); //$NON-NLS-1$
			MH_add3BytesOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add3BytesOfStructsFromVaList", MT_Byte_Int_MemAddr); //$NON-NLS-1$
			MH_add5BytesOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add5BytesOfStructsFromVaList", MT_Byte_Int_MemAddr); //$NON-NLS-1$
			MH_add7BytesOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add7BytesOfStructsFromVaList", MT_Byte_Int_MemAddr); //$NON-NLS-1$
			MH_add1ShortOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add1ShortOfStructsFromVaList", MT_Short_Int_MemAddr); //$NON-NLS-1$
			MH_add2ShortsOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add2ShortsOfStructsFromVaList", MT_Short_Int_MemAddr); //$NON-NLS-1$
			MH_add3ShortsOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add3ShortsOfStructsFromVaList", MT_Short_Int_MemAddr); //$NON-NLS-1$
			MH_add1IntOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add1IntOfStructsFromVaList", MT_Int_Int_MemAddr); //$NON-NLS-1$
			MH_add2IntsOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add2IntsOfStructsFromVaList", MT_Int_Int_MemAddr); //$NON-NLS-1$
			MH_add3IntsOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add3IntsOfStructsFromVaList", MT_Int_Int_MemAddr); //$NON-NLS-1$
			MH_add2LongsOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add2LongsOfStructsFromVaList", MT_Long_Int_MemAddr); //$NON-NLS-1$
			MH_add1FloatOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add1FloatOfStructsFromVaList", MT_Float_Int_MemAddr); //$NON-NLS-1$
			MH_add2FloatsOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add2FloatsOfStructsFromVaList", MT_Float_Int_MemAddr); //$NON-NLS-1$
			MH_add3FloatsOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add3FloatsOfStructsFromVaList", MT_Float_Int_MemAddr); //$NON-NLS-1$
			MH_add1DoubleOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add1DoubleOfStructsFromVaList", MT_Double_Int_MemAddr); //$NON-NLS-1$
			MH_add2DoublesOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "add2DoublesOfStructsFromVaList", MT_Double_Int_MemAddr); //$NON-NLS-1$
			MH_addIntShortOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addIntShortOfStructsFromVaList", MT_Int_Int_MemAddr); //$NON-NLS-1$
			MH_addShortIntOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addShortIntOfStructsFromVaList", MT_Int_Int_MemAddr); //$NON-NLS-1$
			MH_addIntLongOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addIntLongOfStructsFromVaList", MT_Long_Int_MemAddr); //$NON-NLS-1$
			MH_addLongIntOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addLongIntOfStructsFromVaList", MT_Long_Int_MemAddr); //$NON-NLS-1$
			MH_addFloatDoubleOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addFloatDoubleOfStructsFromVaList", MT_Double_Int_MemAddr); //$NON-NLS-1$
			MH_addDoubleFloatOfStructsFromVaList = lookup.findStatic(VaListUpcallMethodHandles.class, "addDoubleFloatOfStructsFromVaList", MT_Double_Int_MemAddr); //$NON-NLS-1$

		} catch (IllegalAccessException | NoSuchMethodException e) {
			throw new InternalError(e);
		}
	}

	public static int addIntsFromVaList(int argCount, MemoryAddress intVaListAddr) {
		VaList intVaList = VaList.ofAddress(intVaListAddr, scope);
		int intSum = 0;
		while (argCount > 0) {
			intSum += intVaList.vargAsInt(C_INT);
			argCount--;
		}
		return intSum;
	}

	public static long addLongsFromVaList(int argCount, MemoryAddress longVaListAddr) {
		VaList longVaList = VaList.ofAddress(longVaListAddr, scope);
		long longSum = 0;
		while (argCount > 0) {
			longSum += longVaList.vargAsLong(longLayout);
			argCount--;
		}
		return longSum;
	}

	public static double addDoublesFromVaList(int argCount, MemoryAddress doubleVaListAddr) {
		VaList doubleVaList = VaList.ofAddress(doubleVaListAddr, scope);
		double doubleSum = 0;
		while (argCount > 0) {
			doubleSum += doubleVaList.vargAsDouble(C_DOUBLE);
			argCount--;
		}
		return doubleSum;
	}

	public static double addMixedArgsFromVaList(MemoryAddress argVaListAddr) {
		VaList argVaList = VaList.ofAddress(argVaListAddr, scope);
		double doubleSum = argVaList.vargAsInt(C_INT)
				+ argVaList.vargAsLong(longLayout) + argVaList.vargAsDouble(C_DOUBLE);
		return doubleSum;
	}

	public static double addMoreMixedArgsFromVaList(MemoryAddress argVaListAddr) {
		VaList argVaList = VaList.ofAddress(argVaListAddr, scope);
		double doubleSum = argVaList.vargAsInt(C_INT) + argVaList.vargAsLong(longLayout)
							+ argVaList.vargAsInt(C_INT) + argVaList.vargAsLong(longLayout)
							+ argVaList.vargAsInt(C_INT) + argVaList.vargAsLong(longLayout)
							+ argVaList.vargAsInt(C_INT) + argVaList.vargAsDouble(C_DOUBLE)
							+ argVaList.vargAsInt(C_INT) + argVaList.vargAsDouble(C_DOUBLE)
							+ argVaList.vargAsInt(C_INT) + argVaList.vargAsDouble(C_DOUBLE)
							+ argVaList.vargAsInt(C_INT) + argVaList.vargAsDouble(C_DOUBLE)
							+ argVaList.vargAsInt(C_INT) + argVaList.vargAsDouble(C_DOUBLE);
		return doubleSum;
	}

	public static int addIntsByPtrFromVaList(int argCount, MemoryAddress ptrVaListAddr) {
		VaList ptrVaList = VaList.ofAddress(ptrVaListAddr, scope);
		int intSum = 0;
		while (argCount > 0) {
			MemorySegment nextValueSegmt = ptrVaList.vargAsAddress(C_POINTER).asSegment(C_INT.byteSize(), scope);
			VarHandle nextValueHandle = MemoryHandles.varHandle(int.class, ByteOrder.nativeOrder());
			intSum += (int)nextValueHandle.get(nextValueSegmt, 0);
			argCount--;
		}
		return intSum;
	}

	public static long addLongsByPtrFromVaList(int argCount, MemoryAddress ptrVaListAddr) {
		VaList ptrVaList = VaList.ofAddress(ptrVaListAddr, scope);
		long longSum = 0;
		while (argCount > 0) {
			MemorySegment nextValueSegmt = ptrVaList.vargAsAddress(C_POINTER).asSegment(longLayout.byteSize(), scope);
			VarHandle nextValueHandle = MemoryHandles.varHandle(long.class, ByteOrder.nativeOrder());
			longSum += (long)nextValueHandle.get(nextValueSegmt, 0);
			argCount--;
		}
		return longSum;
	}

	public static double addDoublesByPtrFromVaList(int argCount, MemoryAddress ptrVaListAddr) {
		VaList ptrVaList = VaList.ofAddress(ptrVaListAddr, scope);
		double doubleSum = 0;
		while (argCount > 0) {
			MemorySegment nextValueSegmt = ptrVaList.vargAsAddress(C_POINTER).asSegment(C_DOUBLE.byteSize(), scope);
			VarHandle nextValueHandle = MemoryHandles.varHandle(double.class, ByteOrder.nativeOrder());
			doubleSum += (double)nextValueHandle.get(nextValueSegmt, 0);
			argCount--;
		}
		return doubleSum;
	}

	public static byte add1ByteOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"));
		VarHandle elemHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));

		byte byteSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			byteSum += (byte)elemHandle1.get(argSegmt);
			argCount--;
		}
		return byteSum;
	}

	public static byte add2BytesOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"), C_CHAR.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));

		byte byteSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			byteSum += (byte)elemHandle1.get(argSegmt) + (byte)elemHandle2.get(argSegmt);
			argCount--;
		}
		return byteSum;
	}

	public static byte add3BytesOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				C_CHAR.withName("elem2"), C_CHAR.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(byte.class, PathElement.groupElement("elem3"));

		byte byteSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			byteSum += (byte)elemHandle1.get(argSegmt) + (byte)elemHandle2.get(argSegmt) + (byte)elemHandle3.get(argSegmt);
			argCount--;
		}
		return byteSum;
	}

	public static byte add5BytesOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				C_CHAR.withName("elem2"), C_CHAR.withName("elem3"),
				C_CHAR.withName("elem4"), C_CHAR.withName("elem5"));
		VarHandle elemHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(byte.class, PathElement.groupElement("elem3"));
		VarHandle elemHandle4 = structLayout.varHandle(byte.class, PathElement.groupElement("elem4"));
		VarHandle elemHandle5 = structLayout.varHandle(byte.class, PathElement.groupElement("elem5"));

		byte byteSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			byteSum += (byte)elemHandle1.get(argSegmt) + (byte)elemHandle2.get(argSegmt)
						+ (byte)elemHandle3.get(argSegmt) + (byte)elemHandle4.get(argSegmt)
						+ (byte)elemHandle5.get(argSegmt);
			argCount--;
		}
		return byteSum;
	}

	public static byte add7BytesOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_CHAR.withName("elem1"),
				C_CHAR.withName("elem2"), C_CHAR.withName("elem3"),
				C_CHAR.withName("elem4"), C_CHAR.withName("elem5"),
				C_CHAR.withName("elem6"), C_CHAR.withName("elem7"));
		VarHandle elemHandle1 = structLayout.varHandle(byte.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(byte.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(byte.class, PathElement.groupElement("elem3"));
		VarHandle elemHandle4 = structLayout.varHandle(byte.class, PathElement.groupElement("elem4"));
		VarHandle elemHandle5 = structLayout.varHandle(byte.class, PathElement.groupElement("elem5"));
		VarHandle elemHandle6 = structLayout.varHandle(byte.class, PathElement.groupElement("elem6"));
		VarHandle elemHandle7 = structLayout.varHandle(byte.class, PathElement.groupElement("elem7"));

		byte byteSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			byteSum += (byte)elemHandle1.get(argSegmt) + (byte)elemHandle2.get(argSegmt)
						+ (byte)elemHandle3.get(argSegmt) + (byte)elemHandle4.get(argSegmt)
						+ (byte)elemHandle5.get(argSegmt) + (byte)elemHandle6.get(argSegmt)
						+ (byte)elemHandle7.get(argSegmt);
			argCount--;
		}
		return byteSum;
	}

	public static short add1ShortOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"));
		VarHandle elemHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));

		short shortSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			shortSum += (short)elemHandle1.get(argSegmt);
			argCount--;
		}
		return shortSum;
	}

	public static short add2ShortsOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		short shortSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			shortSum += (short)elemHandle1.get(argSegmt) + (short)elemHandle2.get(argSegmt);
			argCount--;
		}
		return shortSum;
	}

	public static short add3ShortsOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"),
				C_SHORT.withName("elem2"), C_SHORT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(short.class, PathElement.groupElement("elem3"));

		short shortSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			shortSum += (short)elemHandle1.get(argSegmt) + (short)elemHandle2.get(argSegmt) + (short)elemHandle3.get(argSegmt);
			argCount--;
		}
		return shortSum;
	}

	public static int add1IntOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));

		int intSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			intSum += (int)elemHandle1.get(argSegmt);
			argCount--;
		}
		return intSum;
	}

	public static int add2IntsOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_INT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		int intSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			intSum += (int)elemHandle1.get(argSegmt) + (int)elemHandle2.get(argSegmt);
			argCount--;
		}
		return intSum;
	}

	public static int add3IntsOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
				C_INT.withName("elem2"), C_INT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(int.class, PathElement.groupElement("elem3"));

		int intSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			intSum += (int)elemHandle1.get(argSegmt) + (int)elemHandle2.get(argSegmt) + (int)elemHandle3.get(argSegmt);
			argCount--;
		}
		return intSum;
	}

	public static long add2LongsOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"), longLayout.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		long longSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			longSum += (long)elemHandle1.get(argSegmt) + (long)elemHandle2.get(argSegmt);
			argCount--;
		}
		return longSum;
	}

	public static float add1FloatOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));

		float floatSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			floatSum += (float)elemHandle1.get(argSegmt);
			argCount--;
		}
		return floatSum;
	}

	public static float add2FloatsOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"), C_FLOAT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		float floatSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			floatSum += (float)elemHandle1.get(argSegmt) + (float)elemHandle2.get(argSegmt);
			argCount--;
		}
		return floatSum;
	}

	public static float add3FloatsOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_FLOAT.withName("elem2"), C_FLOAT.withName("elem3"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));
		VarHandle elemHandle3 = structLayout.varHandle(float.class, PathElement.groupElement("elem3"));

		float floatSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			floatSum += (float)elemHandle1.get(argSegmt) + (float)elemHandle2.get(argSegmt) + (float)elemHandle3.get(argSegmt);
			argCount--;
		}
		return floatSum;
	}

	public static double add1DoubleOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));

		double doubleSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			doubleSum += (double)elemHandle1.get(argSegmt);
			argCount--;
		}
		return doubleSum;
	}

	public static double add2DoublesOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"), C_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		double doubleSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			doubleSum += (double)elemHandle1.get(argSegmt) + (double)elemHandle2.get(argSegmt);
			argCount--;
		}
		return doubleSum;
	}

	public static int addIntShortOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"), C_SHORT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(short.class, PathElement.groupElement("elem2"));

		int intSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			intSum += (int)elemHandle1.get(argSegmt) + (short)elemHandle2.get(argSegmt);
			argCount--;
		}
		return intSum;
	}

	public static int addShortIntOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_SHORT.withName("elem1"),
		MemoryLayout.paddingLayout(16), C_INT.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(short.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		int intSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			intSum += (short)elemHandle1.get(argSegmt) + (int)elemHandle2.get(argSegmt);
			argCount--;
		}
		return intSum;
	}

	public static long addLongIntOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(longLayout.withName("elem1"),
				C_INT.withName("elem2"), MemoryLayout.paddingLayout(32));
		VarHandle elemHandle1 = structLayout.varHandle(long.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(int.class, PathElement.groupElement("elem2"));

		long longSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			longSum += (long)elemHandle1.get(argSegmt) + (int)elemHandle2.get(argSegmt);
			argCount--;
		}
		return longSum;
	}

	public static long addIntLongOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_INT.withName("elem1"),
		MemoryLayout.paddingLayout(32), longLayout.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(int.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(long.class, PathElement.groupElement("elem2"));

		long longSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			longSum += (int)elemHandle1.get(argSegmt) + (long)elemHandle2.get(argSegmt);
			argCount--;
		}
		return longSum;
	}

	public static double addFloatDoubleOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		/* The size of [float, double] on AIX/PPC 64-bit is 12 bytes without padding by default
		 * while the same struct is 16 bytes with padding on other platforms.
		 */
		GroupLayout structLayout = isAixOS ? MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				C_DOUBLE.withName("elem2")) : MemoryLayout.structLayout(C_FLOAT.withName("elem1"),
				MemoryLayout.paddingLayout(32), C_DOUBLE.withName("elem2"));
		VarHandle elemHandle1 = structLayout.varHandle(float.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(double.class, PathElement.groupElement("elem2"));

		double doubleSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			doubleSum += (float)elemHandle1.get(argSegmt) + (double)elemHandle2.get(argSegmt);
			argCount--;
		}
		return doubleSum;
	}

	public static double addDoubleFloatOfStructsFromVaList(int argCount, MemoryAddress struVaListAddr) {
		GroupLayout structLayout = MemoryLayout.structLayout(C_DOUBLE.withName("elem1"),
				C_FLOAT.withName("elem2"), MemoryLayout.paddingLayout(32));
		VarHandle elemHandle1 = structLayout.varHandle(double.class, PathElement.groupElement("elem1"));
		VarHandle elemHandle2 = structLayout.varHandle(float.class, PathElement.groupElement("elem2"));

		double doubleSum = 0;
		VaList struVaList = VaList.ofAddress(struVaListAddr, scope);
		while (argCount > 0) {
			MemorySegment argSegmt = struVaList.vargAsSegment(structLayout, allocator);
			doubleSum += (double)elemHandle1.get(argSegmt) + (float)elemHandle2.get(argSegmt);
			argCount--;
		}
		return doubleSum;
	}
}
