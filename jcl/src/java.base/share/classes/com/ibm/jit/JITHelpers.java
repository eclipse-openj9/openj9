/*[INCLUDE-IF Sidecar17]*/

package com.ibm.jit;

/*******************************************************************************
 * Copyright (c) 1998, 2021 IBM Corp. and others
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

import com.ibm.oti.vm.J9UnmodifiableClass;
import java.lang.reflect.Field;
import java.lang.reflect.Array;
import com.ibm.oti.vm.VM;
/*[IF Sidecar19-SE]
import jdk.internal.misc.Unsafe;
import jdk.internal.reflect.Reflection;
import jdk.internal.reflect.CallerSensitive;
/*[ELSE]*/
import sun.misc.Unsafe;
import sun.reflect.Reflection;
import sun.reflect.CallerSensitive;
/*[ENDIF]*/

/**
 * The <code>JITHelpers</code> class contains methods used by the JIT to optimize certain primitive operations.
 */
@SuppressWarnings("javadoc")
@J9UnmodifiableClass
public final class JITHelpers {

	private static final JITHelpers helpers;
	private static final Unsafe unsafe;

	private JITHelpers() {
	}

	@CallerSensitive
	public static JITHelpers getHelpers() {
		if (Reflection.getCallerClass().getClassLoader() != null) {
			throw new SecurityException("JITHelpers"); //$NON-NLS-1$
		}
		return helpers;
	}

	/*
	 * Private method to provide access to the JITHelpers instance from jitted code without the stackwalk.
	 */
	private static JITHelpers jitHelpers() {
		return helpers;
	}

	public native int transformedEncodeUTF16Big(long src, long dest, int num);

	public native int transformedEncodeUTF16Little(long src, long dest, int num);

	/*
	 * Constants for getSuperclass.
	 */
	private static final int CLASS_IS_INTERFACE_OR_PRIMITIVE = classIsInterfaceFlag() | VM.J9_ACC_CLASS_INTERNAL_PRIMITIVE_TYPE;
	private static final long JLCLASS_J9CLASS_OFFSET = javaLangClassJ9ClassOffset();
	private static final int POINTER_SIZE = VM.ADDRESS_SIZE;
	private static final boolean IS_32_BIT = (POINTER_SIZE == 4);

	public static final boolean IS_BIG_ENDIAN = isBigEndian();

	/*
	 * The following natives are used by the static initializer. They do not require special treatment by the JIT.
	 */
	private static native int javaLangClassJ9ClassOffset();

	private static int classIsInterfaceFlag() {
		/* JVMS Table 4.8 ACC_INTERFACE=0x200 */
		return 0x200;
	}

	static {
		helpers = new JITHelpers();
		unsafe = Unsafe.getUnsafe();
	}

	/**
	 * Java implementation of the native getSuperclass
	 * 
	 * @param clazz
	 *           The class to introspect (must not be null).
	 * @return The superclass, or null for primitive types, interfaces and java.lang.Object.
	 */
	public Class<?> getSuperclass(Class<?> clazz) {
		if (IS_32_BIT) {
			int j9Class = unsafe.getInt(clazz, JLCLASS_J9CLASS_OFFSET);
			int romClass = unsafe.getInt(j9Class + VM.J9CLASS_ROMCLASS_OFFSET);
			int modifiers = unsafe.getInt(romClass + VM.J9ROMCLASS_MODIFIERS_OFFSET);
			if (0 == (modifiers & CLASS_IS_INTERFACE_OR_PRIMITIVE)) {
				int superclasses = unsafe.getInt(j9Class + VM.J9CLASS_SUPERCLASSES_OFFSET);
				int depthAndFlags = unsafe.getInt(j9Class + VM.J9CLASS_CLASS_DEPTH_AND_FLAGS_OFFSET);
				int depth = (depthAndFlags & VM.J9_JAVA_CLASS_DEPTH_MASK);
				if (depth > 0) {
					int superclass = unsafe.getInt(superclasses + (POINTER_SIZE * (depth - 1)));
					return getClassFromJ9Class32(superclass);
				}
			}
		} else {
			long j9Class = unsafe.getLong(clazz, JLCLASS_J9CLASS_OFFSET);
			long romClass = unsafe.getLong(j9Class + VM.J9CLASS_ROMCLASS_OFFSET);
			int modifiers = unsafe.getInt(romClass + VM.J9ROMCLASS_MODIFIERS_OFFSET);
			if (0 == (modifiers & CLASS_IS_INTERFACE_OR_PRIMITIVE)) {
				long superclasses = unsafe.getLong(j9Class + VM.J9CLASS_SUPERCLASSES_OFFSET);
				long depthAndFlags = unsafe.getLong(j9Class + VM.J9CLASS_CLASS_DEPTH_AND_FLAGS_OFFSET);
				long depth = (depthAndFlags & VM.J9_JAVA_CLASS_DEPTH_MASK);
				if (depth > 0) {
					long superclass = unsafe.getLong(superclasses + (POINTER_SIZE * (depth - 1)));
					return getClassFromJ9Class64(superclass);
				}
			}
		}
		return null;
	}

	/*
	 * To be recognized by the JIT and turned into trees that is foldable when obj is known at compile time.
	 */
	public boolean isArray(Object obj) {
		if (is32Bit()) {
			int j9Class = getJ9ClassFromObject32(obj);
			int flags = getClassDepthAndFlagsFromJ9Class32(j9Class);
			return (flags & VM.J9_ACC_CLASS_ARRAY) != 0;
		}
		else {
			long j9Class = getJ9ClassFromObject64(obj);
			// Cast the flag to int because only the lower 32 bits are valid and VM.J9_ACC_CLASS_ARRAY is int type
			int flags = (int)getClassDepthAndFlagsFromJ9Class64(j9Class);
			return (flags & VM.J9_ACC_CLASS_ARRAY) != 0;
		}
	}

	/*
	 * To be recognized by the JIT and turned into a load with vft symbol.
	 */
	public long getJ9ClassFromObject64(Object obj) {
		Class<?> clazz = obj.getClass();
		return getJ9ClassFromClass64(clazz);
	}

	public int getJ9ClassFromObject32(Object obj) {
		Class<?> clazz = obj.getClass();
		return getJ9ClassFromClass32(clazz);
	}


	/*
	 * To be recognized by the JIT and returns true if the hardware supports SIMD case conversion.
	 */
	public boolean supportsIntrinsicCaseConversion() {
		return false;
	}

	/**
	 * To be used by the JIT when performing SIMD upper case conversion with Latin 1 strings.
	 * @param value the underlying array for the source string.
	 * @param output a new array which will be used for the converted string.
	 * @param length the number of bytes used to represent the string.
	 * @return True if intrinsic conversion was successful, false if fallback conversion must be used.
	 */
	public boolean toUpperIntrinsicLatin1(byte[] value, byte[] output, int length) {
		return false;
	}

	/**
	 * To be used by the JIT when performing SIMD lower case conversion with Latin 1 strings.
	 * @param value the underlying array for the source string.
	 * @param output a new array which will be used for the converted string.
	 * @param length the number of bytes used to represent the string.
	 * @return True if intrinsic conversion was successful, false if fallback conversion must be used.
	 */
	public boolean toLowerIntrinsicLatin1(byte[] value, byte[] output, int length) {
		return false;
	}

	/**
	 * To be used by the JIT when performing SIMD upper case conversion with UTF16 strings.
	 * @param value the underlying array for the source string.
	 * @param output a new array which will be used for the converted string.
	 * @param length the number of bytes used to represent the string.
	 * @return True if intrinsic conversion was successful, false if fallback conversion must be used.
	 */
	public boolean toUpperIntrinsicUTF16(byte[] value, byte[] output, int length) {
		return false;
	}

	/**
	 * To be used by the JIT when performing SIMD lower case conversion with UTF16 strings.
	 * @param value the underlying array for the source string.
	 * @param output a new array which will be used for the converted string.
	 * @param length the number of bytes used to represent the string.
	 * @return True if intrinsic conversion was successful, false if fallback conversion must be used.
	 */
	public boolean toLowerIntrinsicUTF16(byte[] value, byte[] output, int length) {
		return false;
	}
	/**
	 * To be used by the JIT when performing SIMD upper case conversion with Latin 1 strings.
	 * @param value the underlying array for the source string.
	 * @param output a new array which will be used for the converted string.
	 * @param length the number of bytes used to represent the string.
	 * @return True if intrinsic conversion was successful, false if fallback conversion must be used.
	 */
	public boolean toUpperIntrinsicLatin1(char[] value, char[] output, int length) {
		return false;
	}

	/**
	 * To be used by the JIT when performing SIMD lower case conversion with Latin 1 strings.
	 * @param value the underlying array for the source string.
	 * @param output a new array which will be used for the converted string.
	 * @param length the number of bytes used to represent the string.
	 * @return True if intrinsic conversion was successful, false if fallback conversion must be used.
	 */
	public boolean toLowerIntrinsicLatin1(char[] value, char[] output, int length) {
		return false;
	}

	/**
	 * To be used by the JIT when performing SIMD upper case conversion with UTF16 strings.
	 * @param value the underlying array for the source string.
	 * @param output a new array which will be used for the converted string.
	 * @param length the number of bytes used to represent the string.
	 * @return True if intrinsic conversion was successful, false if fallback conversion must be used.
	 */
	public boolean toUpperIntrinsicUTF16(char[] value, char[] output, int length) {
		return false;
	}

	/**
	 * To be used by the JIT when performing SIMD lower case conversion with UTF16 strings.
	 * @param value the underlying array for the source string.
	 * @param output a new array which will be used for the converted string.
	 * @param length the number of bytes used to represent the string.
	 * @return True if intrinsic conversion was successful, false if fallback conversion must be used.
	 */
	public boolean toLowerIntrinsicUTF16(char[] value, char[] output, int length) {
		return false;
	}

	/*
	 * sun.misc.Unsafe.get* and put* have to generate internal control flow for correctness due to different object shapes. The JIT emitted sequences
	 * checks for and handles: 1) standard fields in normal objects 2) static fields from classes 3) entries from arrays This sequence is branchy and
	 * hard for us to optimize. The better solution in cases where we know the type of object is to use case specific accessors which are the methods
	 * below. Be careful that you know the type of the object you are getting from before using these.
	 * 
	 * NOTE: JIT assumes that obj is non-null and offset is positive - breaking this assumption is just asking for trouble.
	 */
	public int getIntFromObject(Object obj, long offset) {
		return unsafe.getInt(obj, offset);
	}

	public int getIntFromObjectVolatile(Object obj, long offset) {
		return unsafe.getIntVolatile(obj, offset);
	}

	public long getLongFromObject(Object obj, long offset) {
		return unsafe.getLong(obj, offset);
	}

	public long getLongFromObjectVolatile(Object obj, long offset) {
		return unsafe.getLongVolatile(obj, offset);
	}

	public Object getObjectFromObject(Object obj, long offset) {
		return unsafe.getObject(obj, offset);
	}

	public Object getObjectFromObjectVolatile(Object obj, long offset) {
		return unsafe.getObjectVolatile(obj, offset);
	}

	public void putIntInObject(Object obj, long offset, int value) {
		unsafe.putInt(obj, offset, value);
	}

	public void putIntInObjectVolatile(Object obj, long offset, int value) {
		unsafe.putIntVolatile(obj, offset, value);
	}

	public void putLongInObject(Object obj, long offset, long value) {
		unsafe.putLong(obj, offset, value);
	}

	public void putLongInObjectVolatile(Object obj, long offset, long value) {
		unsafe.putLongVolatile(obj, offset, value);
	}

	public void putObjectInObject(Object obj, long offset, Object value) {
		unsafe.putObject(obj, offset, value);
	}

	public void putObjectInObjectVolatile(Object obj, long offset, Object value) {
		unsafe.putObjectVolatile(obj, offset, value);
	}

	public boolean compareAndSwapIntInObject(Object obj, long offset, int expected, int value) {
/*[IF Sidecar19-SE-OpenJ9]*/				
		return unsafe.compareAndSetInt(obj, offset, expected, value);
/*[ELSE]
		return unsafe.compareAndSwapInt(obj, offset, expected, value);
/*[ENDIF]*/		
	}

	public boolean compareAndSwapLongInObject(Object obj, long offset, long expected, long value) {
/*[IF Sidecar19-SE-OpenJ9]*/				
		return unsafe.compareAndSetLong(obj, offset, expected, value);
/*[ELSE]
		return unsafe.compareAndSwapLong(obj, offset, expected, value);
/*[ENDIF]*/		
	}

	public boolean compareAndSwapObjectInObject(Object obj, long offset, Object expected, Object value) {
/*[IF Sidecar19-SE-OpenJ9]*/				
		return unsafe.compareAndSetObject(obj, offset, expected, value);
/*[ELSE]
		return unsafe.compareAndSwapObject(obj, offset, expected, value);
/*[ENDIF]*/		
	}

	public byte getByteFromArray(Object obj, long offset) {
		return unsafe.getByte(obj, offset);
	}

	public byte getByteFromArrayVolatile(Object obj, long offset) {
		return unsafe.getByteVolatile(obj, offset);
	}

	public char getCharFromArray(Object obj, long offset) {
		return unsafe.getChar(obj, offset);
	}

	public char getCharFromArrayVolatile(Object obj, long offset) {
		return unsafe.getCharVolatile(obj, offset);
	}

	public int getIntFromArray(Object obj, long offset) {
		return unsafe.getInt(obj, offset);
	}

	public int getIntFromArrayVolatile(Object obj, long offset) {
		return unsafe.getIntVolatile(obj, offset);
	}

	public long getLongFromArray(Object obj, long offset) {
		return unsafe.getLong(obj, offset);
	}

	public long getLongFromArrayVolatile(Object obj, long offset) {
		return unsafe.getLongVolatile(obj, offset);
	}

	public Object getObjectFromArray(Object obj, long offset) {
		return unsafe.getObject(obj, offset);
	}

	public Object getObjectFromArrayVolatile(Object obj, long offset) {
		return unsafe.getObjectVolatile(obj, offset);
	}

	public void putByteInArray(Object obj, long offset, byte value) {
		unsafe.putByte(obj, offset, value);
	}

	public void putByteInArrayVolatile(Object obj, long offset, byte value) {
		unsafe.putByteVolatile(obj, offset, value);
	}

	public void putCharInArray(Object obj, long offset, char value) {
		unsafe.putChar(obj, offset, value);
	}

	public void putCharInArrayVolatile(Object obj, long offset, char value) {
		unsafe.putCharVolatile(obj, offset, value);
	}

	public void putIntInArray(Object obj, long offset, int value) {
		unsafe.putInt(obj, offset, value);
	}

	public void putIntInArrayVolatile(Object obj, long offset, int value) {
		unsafe.putIntVolatile(obj, offset, value);
	}

	public void putLongInArray(Object obj, long offset, long value) {
		unsafe.putLong(obj, offset, value);
	}

	public void putLongInArrayVolatile(Object obj, long offset, long value) {
		unsafe.putLongVolatile(obj, offset, value);
	}

	public void putObjectInArray(Object obj, long offset, Object value) {
		unsafe.putObject(obj, offset, value);
	}

	public void putObjectInArrayVolatile(Object obj, long offset, Object value) {
		unsafe.putObjectVolatile(obj, offset, value);
	}

	public boolean compareAndSwapIntInArray(Object obj, long offset, int expected, int value) {
/*[IF Sidecar19-SE-OpenJ9]*/				
		return unsafe.compareAndSetInt(obj, offset, expected, value);
/*[ELSE]
		return unsafe.compareAndSwapInt(obj, offset, expected, value);
/*[ENDIF]*/		
	}

	public boolean compareAndSwapLongInArray(Object obj, long offset, long expected, long value) {
/*[IF Sidecar19-SE-OpenJ9]*/				
		return unsafe.compareAndSetLong(obj, offset, expected, value);
/*[ELSE]
		return unsafe.compareAndSwapLong(obj, offset, expected, value);
/*[ENDIF]*/		
	}

	public boolean compareAndSwapObjectInArray(Object obj, long offset, Object expected, Object value) {
/*[IF Sidecar19-SE-OpenJ9]*/				
		return unsafe.compareAndSetObject(obj, offset, expected, value);
/*[ELSE]
		return unsafe.compareAndSwapObject(obj, offset, expected, value);
/*[ENDIF]*/		
	}

	public char byteToCharUnsigned(byte b) {
		return (char) ((char) b & (char) 0x00ff);
	}

	/**
	 * Determine whether {@code lhs} is at a lower address than {@code rhs}.
	 *
	 * Because objects can be moved by the garbage collector, the ordering of
	 * the addresses of distinct objects is not stable over time. As such, the
	 * result of this comparison should be used for heuristic purposes only.
	 *
	 * A null reference is considered less than any non-null reference.
	 *
	 * @param lhs The left hand side of the comparison
	 * @param rhs The right hand side of the comparison
	 * @return true if {@code lhs} is at a lower address, false otherwise
	 */
	public native boolean acmplt(Object lhs, Object rhs);

	private static long storeBits(long dest, int width, long value, int vwidth, int offset) {
		int offsetToModify = IS_BIG_ENDIAN ? ((width - 1) - ((offset * vwidth) % width)) : ((offset * vwidth) % width);

		long vmask = (-1 >>> ((8 - vwidth) * 8));
		long dmask = ~(vmask << (8 * offsetToModify));

		return (dest & dmask) | ((value & vmask) << (8 * offsetToModify));
	}

	private static long readBits(long src, int width, int vwidth, int offset) {
		int offsetToRead = IS_BIG_ENDIAN ? ((width - 1) - ((offset * vwidth) % width)) : ((offset * vwidth) % width);

		long vmask = (-1 >>> ((8 - vwidth) * 8));
		long smask = vmask << (8 * offsetToRead);

		return (src & smask) >>> (8 * offsetToRead);
	}

	public void putByteInArrayByIndex(Object obj, int index, byte value) {
		Class<?> clazz = obj.getClass();

		if (clazz == byte[].class) {
			((byte[]) obj)[index] = value;
		} else if (clazz == char[].class) {
			char[] array = (char[]) obj;
			int cidx = index >> 1;
			char c = array[cidx];
			array[cidx] = (char) storeBits(c, 2, value, 1, index);
		} else if (clazz == int[].class) {
			int[] array = (int[]) obj;
			int iidx = index >> 2;
			int i = array[iidx];
			array[iidx] = (int) storeBits(i, 4, value, 1, index);
		} else if (clazz == long[].class) {
			long[] array = (long[]) obj;
			int lidx = index >> 3;
			long l = array[lidx];
			array[lidx] = (long) storeBits(l, 8, value, 1, index);
		}
	}

	public byte getByteFromArrayByIndex(Object obj, int index) {
		Class<?> clazz = obj.getClass();
		
		if (clazz == byte[].class) {
			return ((byte[]) obj)[index];
		} else if (clazz == char[].class) {
			return (byte) readBits(((char[]) obj)[index >> 1], 2, 1, index);
		} else if (clazz == int[].class) {
			return (byte) readBits(((int[]) obj)[index >> 2], 4, 1, index);
		} else if (clazz == long[].class) {
			return (byte) readBits(((long[]) obj)[index >> 3], 8, 1, index);
		}
		throw new RuntimeException("Unknown array type for bit manipulation");
	}

	public void putCharInArrayByIndex(Object obj, int index, char value) {
		Class<?> clazz = obj.getClass();

		if (clazz == byte[].class) {
			index = index << 1;
			byte[] array = (byte[]) obj;
			if (!IS_BIG_ENDIAN) {
				array[index] = (byte) value;
				array[index + 1] = (byte) (value >>> 8);
			} else {
				array[index] = (byte) (value >>> 8);
				array[index + 1] = (byte) value;
			}
		} else if (clazz == char[].class) {
			((char[]) obj)[index] = value;
		} else if (clazz == int[].class) {
			int[] array = (int[]) obj;
			int iidx = index >> 1;
			int i = array[iidx];
			array[iidx] = (int) storeBits(i, 4, value, 2, index);
		} else if (clazz == long[].class) {
			long[] array = (long[]) obj;
			int lidx = index >> 2;
			long l = array[lidx];
			array[lidx] = (long) storeBits(l, 8, value, 2, index);
		}
	}

	public char getCharFromArrayByIndex(Object obj, int index) {
		Class<?> clazz = obj.getClass();

		if (clazz == byte[].class) {
			index = index << 1;
			byte[] array = (byte[]) obj;
			if (!IS_BIG_ENDIAN) {
				return (char) ((byteToCharUnsigned(array[index + 1]) << 8) | byteToCharUnsigned(array[index]));
			} else {
				return (char) (byteToCharUnsigned(array[index + 1]) | (byteToCharUnsigned(array[index]) << 8));
			}
		} else if (clazz == char[].class) {
			return ((char[]) obj)[index];
		} else if (clazz == int[].class) {
			return (char) readBits(((int[]) obj)[index >> 1], 4, 2, index);
		} else if (clazz == long[].class) {
			return (char) readBits(((long[]) obj)[index >> 2], 8, 2, index);
		} else {
			throw new RuntimeException("Unknown array type for bit manipulation");
		}
	}

	/**
	 * Returns the first index of the target character array within the source character array starting from the specified
	 * offset.
	 *
	 * <p>This API implicitly assumes the following:
	 * <blockquote><pre>
	 *     - s1Value != null
	 *     - s2Value != null
	 *     - 0 <= s1len <= s1Value.length * 2
	 *     - 1 <= s2len <= s2Value.length * 2
	 *     - 0 <= start < s1len
	 * <blockquote><pre>
	 * 
	 * @param s1Value the source character array to search in.
	 * @param s1len   the length (in number of characters) of the source array.
	 * @param s2Value the target character array to search for.
	 * @param s2len   the length (in number of characters) of the target array.
	 * @param start   the starting offset (in number of characters) to search from.
	 * @return        the index (in number of characters) of the target array within the source array, or -1 if the
	 *                target array is not found within the source array.
	 */
	public int intrinsicIndexOfStringLatin1(Object s1Value, int s1len, Object s2Value, int s2len, int start) {
		char firstChar = byteToCharUnsigned(getByteFromArrayByIndex(s2Value, 0));

		while (true) {
			int i = intrinsicIndexOfLatin1(s1Value, (byte)firstChar, start, s1len);
						
			// Handles subCount > count || start >= count
			if (i == -1 || s2len + i > s1len) {
				return -1;
			}

			int o1 = i;
			int o2 = 0;

			while (++o2 < s2len && getByteFromArrayByIndex(s1Value, ++o1) == getByteFromArrayByIndex(s2Value, o2))
				;

			if (o2 == s2len) {
				return i;
			}

			start = i + 1;
		}
	}
	
	/**
	 * Returns the first index of the target character array within the source character array starting from the specified
	 * offset.
	 *
	 * <p>This API implicitly assumes the following:
	 * <blockquote><pre>
	 *     - s1Value != null
	 *     - s2Value != null
	 *     - 0 <= s1len <= s1Value.length
	 *     - 1 <= s2len <= s2Value.length
	 *     - 0 <= start < s1len
	 * <blockquote><pre>
	 * 
	 * @param s1Value the source character array to search in.
	 * @param s1len   the length (in number of characters) of the source array.
	 * @param s2Value the target character array to search for.
	 * @param s2len   the length (in number of characters) of the target array.
	 * @param start   the starting offset (in number of characters) to search from.
	 * @return        the index (in number of characters) of the target array within the source array, or -1 if the
	 *                target array is not found within the source array.
	 */
	public int intrinsicIndexOfStringUTF16(Object s1Value, int s1len, Object s2Value, int s2len, int start) {
		char firstChar = getCharFromArrayByIndex(s2Value, 0);

		while (true) {
			int i = intrinsicIndexOfUTF16(s1Value, firstChar, start, s1len);
			
			// Handles subCount > count || start >= count
			if (i == -1 || s2len + i > s1len) {
				return -1;
			}

			int o1 = i;
			int o2 = 0;

			while (++o2 < s2len && getCharFromArrayByIndex(s1Value, ++o1) == getCharFromArrayByIndex(s2Value, o2))
				;

			if (o2 == s2len) {
				return i;
			}

			start = i + 1;
		}
	}

	/**
	 * Returns the first index of the character within the source character array starting from the specified offset.
	 *
	 * <p>This API implicitly assumes the following:
	 * <blockquote><pre>
	 *     - array != null
	 *     - 0 <= offset < length <= array.length * 1 (if array instanceof byte[])
	 *     - 0 <= offset < length <= array.length * 2 (if array instanceof char[])
	 * <blockquote><pre>
	 * 
	 * @param array  the source character array to search in.
	 * @param ch     the character to search for.
	 * @param offset the starting offset (in number of characters) to search from.
	 * @param length the length (in number of characters) of the source array.
	 * @return       the index (in number of characters) of \p ch within the source array, or -1 if \p ch is not found
	 *               within the source array.
	 */
	public int intrinsicIndexOfLatin1(Object array, byte ch, int offset, int length) {
		for (int i = offset; i < length; i++) {
			if (getByteFromArrayByIndex(array, i) == ch) {
				return i;
			}
		}
		return -1;
	}

	/**
	 * Returns the first index of the character within the source character array starting from the specified offset.
	 *
	 * <p>This API implicitly assumes the following:
	 * <blockquote><pre>
	 *     - array != null
	 *     - 0 <= offset < length <= array.length * 1 (if array instanceof byte[])
	 *     - 0 <= offset < length <= array.length * 2 (if array instanceof char[])
	 * <blockquote><pre>
	 * 
	 * @param array  the source character array to search in.
	 * @param ch     the character to search for.
	 * @param offset the starting offset (in number of characters) to search from.
	 * @param length the length (in number of characters) of the source array.
	 * @return       the index (in number of characters) of \p ch within the source array, or -1 if \p ch is not found
	 *               within the source array.
	 */
	public int intrinsicIndexOfUTF16(Object array, char ch, int offset, int length) {
		for (int i = offset; i < length; i++) {
			if (getCharFromArrayByIndex(array, i) == ch) {
				return i;
			}
		}
		return -1;
	}

	/*
	 * Constants for optimizedClone
	 */
	private static final int DESCRIPTION_WORD_SIZE = VM.ADDRESS_SIZE;
	private static final int DESCRIPTION_WORD_BIT_SIZE = DESCRIPTION_WORD_SIZE * 8;
	// Depends on the definition of slot and whether it's 32 or 64 bit
	private static final int SLOT_SIZE = VM.FJ9OBJECT_SIZE;

	private static boolean isDescriptorPointerTagged(int descriptorPtr) {
		return (descriptorPtr & 1) == 1;
	}

	private static boolean isDescriptorPointerTagged(long descriptorPtr) {
		return (descriptorPtr & 1) == 1;
	}

	/**
	 * Called by the JIT on 32bit platforms to copy a source object's contents to a destination object as part of a clone. This is a shallow copy only.
	 * 
	 * @param srcObj
	 *           The object whose fields are to be copied
	 * @param destObj
	 *           The object whose fields are to be set (may not be initialized)
	 * @param j9clazz
	 *           The j9class pointer for the type of the object being copied (usually from a loadaddr)
	 */
	public final void unsafeObjectShallowCopy32(Object srcObj, Object destObj, int j9clazz) {
		int instanceSize = unsafe.getInt(j9clazz + VM.J9CLASS_INSTANCESIZE_OFFSET);
		int descriptorPtr = unsafe.getInt(j9clazz + VM.J9CLASS_INSTANCE_DESCRIPTION_OFFSET);
		int numSlotsInObject = instanceSize / SLOT_SIZE;

		int descriptorWord = descriptorPtr;
		int bitIndex = DESCRIPTION_WORD_BIT_SIZE - 1;
		if ((descriptorWord & 1) == 1) {
			descriptorWord = descriptorWord >> 1;
		} else {
			descriptorWord = unsafe.getInt(descriptorPtr);
			descriptorPtr = descriptorPtr + DESCRIPTION_WORD_SIZE;
		}

		for (int slot = 0; slot < numSlotsInObject; ++slot) {
			if (((descriptorWord & 1) == 1)) {
				Object fieldValue = getObjectFromObject(srcObj, VM.OBJECT_HEADER_SIZE + (slot * SLOT_SIZE));
				putObjectInObject(destObj, VM.OBJECT_HEADER_SIZE + (slot * SLOT_SIZE), fieldValue);
			} else {
				int fieldValue = getIntFromObject(srcObj, VM.OBJECT_HEADER_SIZE + (slot * SLOT_SIZE));
				putIntInObject(destObj, VM.OBJECT_HEADER_SIZE + (slot * SLOT_SIZE), fieldValue);
			}

			descriptorWord = descriptorWord >> 1;
			if (bitIndex-- == 0) {
				descriptorWord = unsafe.getInt(descriptorPtr);
				descriptorPtr = descriptorPtr + DESCRIPTION_WORD_SIZE;
				bitIndex = DESCRIPTION_WORD_BIT_SIZE - 1;
			}
		}

		/*
		 * If the object has a lockword, it needs to be set to the correct initial value since this method
		 * is effectively the cloned object's initialization.
		 */
		int lockOffset = unsafe.getInt(j9clazz + VM.J9CLASS_LOCK_OFFSET_OFFSET);
		if (lockOffset != 0) {
			int lwValue = getInitialLockword32(j9clazz);
			putIntInObject(destObj, lockOffset, lwValue);
		}
		unsafe.storeFence();
	}

	/**
	 * Called by the JIT on 64bit platforms to copy a source object's contents to a destination object as part of a clone. This is a shallow copy only.
	 * 
	 * @param srcObj
	 *           The object whose fields are to be copied
	 * @param destObj
	 *           The object whose fields are to be set (may not be initialized)
	 * @param j9clazz
	 *           The j9class pointer for the type of the object being copied (usually from a loadaddr)
	 */
	public final void unsafeObjectShallowCopy64(Object srcObj, Object destObj, long j9clazz) {
		long instanceSize = unsafe.getLong(j9clazz + VM.J9CLASS_INSTANCESIZE_OFFSET);
		long descriptorPtr = unsafe.getLong(j9clazz + VM.J9CLASS_INSTANCE_DESCRIPTION_OFFSET);
		int header = VM.OBJECT_HEADER_SIZE;
		int numSlotsInObject = (int) (instanceSize / SLOT_SIZE);

		long descriptorWord = descriptorPtr;
		int bitIndex = DESCRIPTION_WORD_BIT_SIZE - 1;
		if ((descriptorWord & 1) == 1) {
			descriptorWord = descriptorWord >> 1;
		} else {
			descriptorWord = unsafe.getLong(descriptorPtr);
			descriptorPtr = descriptorPtr + DESCRIPTION_WORD_SIZE;
		}

		for (int slot = 0; slot < numSlotsInObject; ++slot) {
			if (((descriptorWord & 1) == 1)) {
				Object fieldValue = getObjectFromObject(srcObj, (header + (slot * SLOT_SIZE)));
				putObjectInObject(destObj, header + (slot * SLOT_SIZE), fieldValue);
			} else {
				if (SLOT_SIZE == 4) {
					int fieldValue = getIntFromObject(srcObj, header + (slot * SLOT_SIZE));
					putIntInObject(destObj, header + (slot * SLOT_SIZE), fieldValue);
				} else {
					long fieldValue = getLongFromObject(srcObj, header + (slot * SLOT_SIZE));
					putLongInObject(destObj, header + (slot * SLOT_SIZE), fieldValue);
				}
			}

			descriptorWord = descriptorWord >> 1;
			if (bitIndex-- == 0) {
				descriptorWord = unsafe.getLong(descriptorPtr);
				descriptorPtr = descriptorPtr + DESCRIPTION_WORD_SIZE;
				bitIndex = DESCRIPTION_WORD_BIT_SIZE - 1;
			}
		}

		/*
		 * If the object has a lockword, it needs to be set to the correct initial value since this method
		 * is effectively the cloned object's initialization.
		 */
		long lockOffset = unsafe.getLong(j9clazz + VM.J9CLASS_LOCK_OFFSET_OFFSET);
		if (lockOffset != 0) {
			int lwValue = getInitialLockword64(j9clazz);
			if (SLOT_SIZE == 4) {
				// for compressed reference, the LockWord is 4 bytes
				putIntInObject(destObj, lockOffset, lwValue);
			} else {
				putLongInObject(destObj, lockOffset, lwValue);
			}
		}
		unsafe.storeFence();
	}

	/**
	 * Java implementation of optimized clone.
	 * 
	 * @param srcObj
	 *           The source object of the clone.
	 * @return The cloned object.
	 */
	public Object optimizedClone(Object srcObj) throws CloneNotSupportedException {
		Class<?> clnClass = srcObj.getClass();

		if (clnClass.isArray()) {
			Class<?> eleClass = clnClass.getComponentType();
			int len = Array.getLength(srcObj);
			Object clnObj = Array.newInstance(eleClass, len);
			System.arraycopy(srcObj, 0, clnObj, 0, len);
			return clnObj;
		}

		if (!(srcObj instanceof Cloneable)) {
			throw new CloneNotSupportedException();
		}

		Object clnObj = null;
		try {
			clnObj = (Object) unsafe.allocateInstance(clnClass);
		} catch (InstantiationException e) {
			// This can never actually happen as clnClass is clearly instantiable
		}

		int bitIndex = 0;
		if (IS_32_BIT) {
			int j9clazz = unsafe.getInt(clnClass, JLCLASS_J9CLASS_OFFSET);
			// Get the instance size out of the J9Class
			int instanceSize = unsafe.getInt(j9clazz + VM.J9CLASS_INSTANCESIZE_OFFSET);
			// Get the pointer to instance descriptor out of the J9Class. Tells which fields are refs and prims
			int descriptorPtr = unsafe.getInt(j9clazz + VM.J9CLASS_INSTANCE_DESCRIPTION_OFFSET);
			int numSlotsInObject = instanceSize / SLOT_SIZE;
			int descriptorWord = 0;
			if (isDescriptorPointerTagged(descriptorPtr)) {
				bitIndex++;
				descriptorWord = descriptorPtr >>> 1;
			} else {
				descriptorWord = unsafe.getInt(descriptorPtr);
			}

			int countSlots = 0;
			for (int index = 0; numSlotsInObject != 0; index++) {
				if (isDescriptorPointerTagged(descriptorWord)) {
					Object fieldValue = unsafe.getObject(srcObj, VM.OBJECT_HEADER_SIZE + (countSlots * SLOT_SIZE));
					unsafe.putObject(clnObj, VM.OBJECT_HEADER_SIZE + (index * SLOT_SIZE), fieldValue);
				} else {
					int fieldValue = unsafe.getInt(srcObj, VM.OBJECT_HEADER_SIZE + (countSlots * SLOT_SIZE));
					unsafe.putInt(clnObj, VM.OBJECT_HEADER_SIZE + (index * SLOT_SIZE), fieldValue);
				}
				countSlots++;
				if (countSlots >= numSlotsInObject) {
					break;
				}
				if (bitIndex == (DESCRIPTION_WORD_BIT_SIZE - 1)) {
					descriptorPtr = descriptorPtr + DESCRIPTION_WORD_SIZE;
					bitIndex = 0;
					descriptorWord = unsafe.getInt(descriptorPtr);
				} else {
					descriptorWord = descriptorWord >>> 1;
					bitIndex++;
				}
			}
			/* If the object has a lockword, it needs to be set to the correct initial value. */
			long lockOffset = unsafe.getInt(j9clazz + VM.J9CLASS_LOCK_OFFSET_OFFSET);
			if (lockOffset != 0) {
				int lwValue = getInitialLockword32(j9clazz);
				unsafe.putInt(clnObj, lockOffset, lwValue);
			}
		} else {
			long j9clazz = unsafe.getLong(clnClass, JLCLASS_J9CLASS_OFFSET);
			// Get the instance size out of the J9Class
			long instanceSize = unsafe.getLong(j9clazz + VM.J9CLASS_INSTANCESIZE_OFFSET);
			// Get the pointer to instance descriptor out of the J9Class. Tells which fields are refs and prims
			long descriptorPtr = unsafe.getLong(j9clazz + VM.J9CLASS_INSTANCE_DESCRIPTION_OFFSET);
			int numSlotsInObject = (int) (instanceSize / SLOT_SIZE);
			long descriptorWord = 0;
			if (isDescriptorPointerTagged(descriptorPtr)) {
				bitIndex++;
				descriptorWord = descriptorPtr >>> 1;
			} else {
				descriptorWord = unsafe.getLong(descriptorPtr);
			}
			int countSlots = 0;
			for (int index = 0; numSlotsInObject != 0; index++) {
				if (isDescriptorPointerTagged(descriptorWord)) {
					Object fieldValue = unsafe.getObject(srcObj, VM.OBJECT_HEADER_SIZE + (countSlots * SLOT_SIZE));
					unsafe.putObject(clnObj, VM.OBJECT_HEADER_SIZE + (index * SLOT_SIZE), fieldValue);
				} else {
					long fieldValue = unsafe.getLong(srcObj, VM.OBJECT_HEADER_SIZE + (countSlots * SLOT_SIZE));
					unsafe.putLong(clnObj, VM.OBJECT_HEADER_SIZE + (index * SLOT_SIZE), fieldValue);
				}
				countSlots++;
				if (countSlots >= numSlotsInObject) {
					break;
				}
				if (bitIndex == (DESCRIPTION_WORD_BIT_SIZE - 1)) {
					descriptorPtr = descriptorPtr + DESCRIPTION_WORD_SIZE;
					bitIndex = 0;
					descriptorWord = unsafe.getLong(descriptorPtr);
				} else {
					descriptorWord = descriptorWord >>> 1;
					bitIndex++;
				}
			}
			/* If the object has a lockword, it needs to be set to the correct initial value. */
			long lockOffset = unsafe.getLong(j9clazz + VM.J9CLASS_LOCK_OFFSET_OFFSET);
			if (lockOffset != 0) {
				int lwValue = getInitialLockword64(j9clazz);
				if (SLOT_SIZE == 4) {
					// for compressed reference, the LockWord is 4 bytes
					unsafe.putInt(clnObj, lockOffset, lwValue);
				} else {
					unsafe.putLong(clnObj, lockOffset, lwValue);
				}
			}
		}

		unsafe.storeFence();
		return clnObj;
	}

	/**
	 * Get class initialize status flag. Calls to this method will be recognized and optimized by the JIT.
	 * @parm defc
	 *          The class whose initialize status is desired.
	 * @return
	 *          initializeStatus from J9Class.
	 */
	public final int getClassInitializeStatus(Class<?> defc) {
		long defcClass = 0;
		if (is32Bit()) {
			defcClass = getJ9ClassFromClass32(defc);
		} else {
			defcClass = getJ9ClassFromClass64(defc);
		}
		int initStatus = 0;
		if (4 == VM.ADDRESS_SIZE) {
			initStatus = unsafe.getInt(defcClass + VM.J9CLASS_INITIALIZE_STATUS_OFFSET);
		} else {
			initStatus = (int)unsafe.getLong(defcClass + VM.J9CLASS_INITIALIZE_STATUS_OFFSET);
		}
		return initStatus;
	}

	/**
	 * Determine initial lockword value, 32-bit version. Calls to this method will return what the lockword should start at.
	 *
	 * @parm j9clazz
	 *          The J9Class of the object whose lockword is being initialized.
	 *
	 * @return
	 *          The initial lockword to use.
	 */
	public int getInitialLockword32(int j9clazz) {
		int flags = getClassFlagsFromJ9Class32(j9clazz);

		/*
		 * Unsafe calls are used to get the reservedCounter and cancelCounter out of the J9Class.
		 * reservedCounter and cancelCounter are unsigned 16 bit values so a mask is used to force a zero extend.
		 */
		int reservedCounter = ((int)(unsafe.getShort(j9clazz + VM.J9CLASS_LOCK_RESERVATION_HISTORY_RESERVED_COUNTER_OFFSET))) & 0xFFFF;
		int cancelCounter = ((int)(unsafe.getShort(j9clazz + VM.J9CLASS_LOCK_RESERVATION_HISTORY_CANCEL_COUNTER_OFFSET))) & 0xFFFF;

		return getInitialLockword(flags, reservedCounter, cancelCounter);
	}

	/**
	 * Determine initial lockword value, 64-bit version. Calls to this method will return what the lockword should start at.
	 *
	 * @parm j9clazz
	 *          The J9Class of the object whose lockword is being initialized.
	 *
	 * @return
	 *          The initial lockword to use.
	 */
	public int getInitialLockword64(long j9clazz) {
		int flags = getClassFlagsFromJ9Class64(j9clazz);

		/*
		 * Unsafe calls are used to get the reservedCounter and cancelCounter out of the J9Class.
		 * reservedCounter and cancelCounter are unsigned 16 bit values so a mask is used to force a zero extend.
		 */
		int reservedCounter = ((int)(unsafe.getShort(j9clazz + VM.J9CLASS_LOCK_RESERVATION_HISTORY_RESERVED_COUNTER_OFFSET))) & 0xFFFF;
		int cancelCounter = ((int)(unsafe.getShort(j9clazz + VM.J9CLASS_LOCK_RESERVATION_HISTORY_CANCEL_COUNTER_OFFSET))) & 0xFFFF;

		return getInitialLockword(flags, reservedCounter, cancelCounter);
	}

	/**
	 * Determine initial lockword value. Calls to this method will return what the lockword should start at.
	 *
	 * @parm flags
	 *          Class flags from the J9Class.
	 *
	 * @parm reservedCounter
	 *          reservedCounter from the J9Class.
	 *
	 * @parm cancelCounter
	 *          cancelCounter from the J9Class.
	 *
	 * @return
	 *          The initial lockword to use.
	 */
	public int getInitialLockword(int flags, int reservedCounter, int cancelCounter) {
		int lwValue = 0;

		if (1 == VM.GLR_ENABLE_GLOBAL_LOCK_RESERVATION) {
			/* This path is taken when Global Lock Reservation is enabled. */
			int reservedAbsoluteThreshold = VM.GLR_RESERVED_ABSOLUTE_THRESHOLD;
			int minimumReservedRatio = VM.GLR_MINIMUM_RESERVED_RATIO;

			int cancelAbsoluteThreshold = VM.GLR_CANCEL_ABSOLUTE_THRESHOLD;
			int minimumLearningRatio = VM.GLR_MINIMUM_LEARNING_RATIO;

			/*
			 * Check reservedCounter and cancelCounter against different thresholds to determine what to initialize the lockword to.
			 * First check if the ratio of resevation to cancellations is high enough to set the lockword to the New-AutoReserve state.
			 * If so, the initial lockword value is OBJECT_HEADER_LOCK_RESERVED.
			 * Second check if the ratio is high enough to set the lockword to the New-PreLearning state.
			 * If so, the initial lockword value is OBJECT_HEADER_LOCK_LEARNING.
			 * If the ratio is too low, the lockword starts in the Flat-Unlocked state and 0 is returned for the lockword value.
			 *
			 * Start as New-AutoReserve -> Initial lockword: OBJECT_HEADER_LOCK_RESERVED
			 * Start as New-PreLearning -> Initial lockword: OBJECT_HEADER_LOCK_LEARNING
			 * Start as Flat-Unlocked   -> Initial lockword: 0
			 *
			 * reservedCounter, cancelCounter, minimumReservedRatio and minimumLearningRatio all have a maximum value of 0xFFFF so casting to long
			 * is required to guarantee signed overflow does not occur after multiplication.
			 */
			if ((reservedCounter >= reservedAbsoluteThreshold) && ((long)reservedCounter > ((long)cancelCounter * (long)minimumReservedRatio))) {
				lwValue = VM.OBJECT_HEADER_LOCK_RESERVED;
			} else if ((cancelCounter < cancelAbsoluteThreshold) || ((long)reservedCounter > ((long)cancelCounter * (long)minimumLearningRatio))) {
				lwValue = VM.OBJECT_HEADER_LOCK_LEARNING;
			}
		} else if ((flags & VM.J9CLASS_RESERVABLE_LOCK_WORD_INIT) != 0) {
			/* Initialize lockword to New-ReserveOnce. This path is x86 only. */
			lwValue = VM.OBJECT_HEADER_LOCK_RESERVED;
		}

		return lwValue;
	}

	/**		
	 * Determines whether the underlying platform's memory model is big-endian.		
	 * 		
	 * @return True if the underlying platform's memory model is big-endian, false otherwise.		
	 */		
	private native static final boolean isBigEndian();

	/* Placeholder for JIT GPU optimizations - this method never actually gets run */
	public final native void GPUHelper();

	/*
	 * The following natives are recognized and handled specially by the JIT.
	 */

	public native boolean is32Bit();

	public native int getNumBitsInReferenceField();

	public native int getNumBytesInReferenceField();

	public native int getNumBitsInDescriptionWord();

	public native int getNumBytesInDescriptionWord();

	public native int getNumBytesInJ9ObjectHeader();

	public native int getJ9ClassFromClass32(Class c);

	public native Class getClassFromJ9Class32(int j9clazz);

	public native int getTotalInstanceSizeFromJ9Class32(int j9clazz);

	public native int getInstanceDescriptionFromJ9Class32(int j9clazz);

	public native int getDescriptionWordFromPtr32(int descriptorPtr);

	public native long getJ9ClassFromClass64(Class c);

	public native Class getClassFromJ9Class64(long j9clazz);

	public native long getTotalInstanceSizeFromJ9Class64(long j9clazz);

	public native long getInstanceDescriptionFromJ9Class64(long j9clazz);

	public native long getDescriptionWordFromPtr64(long descriptorPtr);

	public native int getNumSlotsInObject(Class currentClass);

	public native int getSlotIndex(Field field);

	public native boolean isDescriptorPointerTagged(int descriptorPtr, long descriptorPtr64);

	public native int getRomClassFromJ9Class32(int j9clazz);

	public native int getSuperClassesFromJ9Class32(int j9clazz);

	public native int getClassDepthAndFlagsFromJ9Class32(int j9clazz);

	public native int getBackfillOffsetFromJ9Class32(int j9clazz);

	public native int getArrayShapeFromRomClass32(int j9romclazz);

	public native int getModifiersFromRomClass32(int j9romclazz);

	public native long getRomClassFromJ9Class64(long j9clazz);

	public native long getSuperClassesFromJ9Class64(long j9clazz);

	public native long getClassDepthAndFlagsFromJ9Class64(long j9clazz);

	public native long getBackfillOffsetFromJ9Class64(long j9clazz);

	public native int getArrayShapeFromRomClass64(long j9romclazz);

	public native int getModifiersFromRomClass64(long j9romclazz);

	public native int getClassFlagsFromJ9Class32(int j9clazz);

	public native int getClassFlagsFromJ9Class64(long j9clazz);

	public static native void dispatchComputedStaticCall();
}
