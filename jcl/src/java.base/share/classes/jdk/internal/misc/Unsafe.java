/*[INCLUDE-IF Sidecar19-SE]*/
/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
package jdk.internal.misc;

import com.ibm.oti.vm.VM;
import com.ibm.oti.vm.VMLangAccess;

import java.lang.Class;
import java.lang.ClassLoader;
import java.lang.reflect.Field;
import java.security.ProtectionDomain;
import java.util.Objects;
/*[IF Java12]*/
import java.nio.ByteBuffer;
import sun.nio.ch.DirectBuffer;
import jdk.internal.ref.Cleaner;
/*[ENDIF] Java12 */

public final class Unsafe {

	/* unsafe instance */
	private static final Unsafe theUnsafe;

	/**
	 * Represents an invalid field offset value.
	 */
	public static final int INVALID_FIELD_OFFSET;
	
	/**
	 * Starting offset of byte array.
	 */
	public static final int ARRAY_BYTE_BASE_OFFSET;
	
	/**
	 * Starting offset of int array.
	 */
	public static final int ARRAY_INT_BASE_OFFSET;
	
	/**
	 * Starting offset of long array.
	 */
	public static final int ARRAY_LONG_BASE_OFFSET;
	
	/**
	 * Starting offset of float array.
	 */
	public static final int ARRAY_FLOAT_BASE_OFFSET;
	
	/**
	 * Starting offset of double array.
	 */
	public static final int ARRAY_DOUBLE_BASE_OFFSET;
	
	/**
	 * Starting offset of short array.
	 */
	public static final int ARRAY_SHORT_BASE_OFFSET;
	
	/**
	 * Starting offset of char array.
	 */
	public static final int ARRAY_CHAR_BASE_OFFSET;
	
	/**
	 * Starting offset of boolean array.
	 */
	public static final int ARRAY_BOOLEAN_BASE_OFFSET;
	
	/**
	 * Starting offset of Object array.
	 */
	public static final int ARRAY_OBJECT_BASE_OFFSET;
	
	/**
	 * Index size of byte array in bytes.
	 */
	public static final int ARRAY_BYTE_INDEX_SCALE;
	
	/**
	 * Index size of int array in bytes.
	 */
	public static final int ARRAY_INT_INDEX_SCALE;
	
	/**
	 * Index size of long array in bytes.
	 */
	public static final int ARRAY_LONG_INDEX_SCALE;
	
	/**
	 * Index size of float array in bytes.
	 */
	public static final int ARRAY_FLOAT_INDEX_SCALE;
	
	/**
	 * Index size of double array in bytes.
	 */
	public static final int ARRAY_DOUBLE_INDEX_SCALE;
	
	/**
	 * Index size of short array in bytes.
	 */
	public static final int ARRAY_SHORT_INDEX_SCALE;
	
	/**
	 * Index size of char array in bytes.
	 */
	public static final int ARRAY_CHAR_INDEX_SCALE;
	
	/**
	 * Index size of boolean array in bytes.
	 */
	public static final int ARRAY_BOOLEAN_INDEX_SCALE;
	
	/**
	 * Index size of Object array in bytes.
	 */
	public static final int ARRAY_OBJECT_INDEX_SCALE;
	
	/**
	 * Size of address on machine in use.
	 */
	public static final int ADDRESS_SIZE;

	/* 
	 * internal helpers 
	 */
	/* true if machine is big endian, false otherwise. */
	private static final boolean IS_BIG_ENDIAN;
	/* true if addresses are aligned in memory, false otherwise. */
	private static final boolean UNALIGNED_ACCESS;

	/* Size of int in bytes. */
	private static final int BYTES_IN_INT = 4;
	/* Number of bits in a short. */
	private static final int BITS_IN_SHORT = 16;
	/* Number of bits in a byte. */
	private static final int BITS_IN_BYTE = 8;

	/* 8 bit mask (long) */
	private static final long BYTE_MASK = 0xFFL;
	/* 32 bit mask (long) */
	private static final long INT_MASK = 0xFFFFFFFFL;
	/* 16 bit mask (long) */
	private static final long SHORT_MASK = 0xFFFFL;
	/* 8 bit mask (int) */
	private static final int BYTE_MASK_INT = (int) BYTE_MASK;
	/* 16 bit mask (int) */
	private static final int SHORT_MASK_INT = (int) SHORT_MASK;

	/* Mask aligns offset to be int addressable (all but bottom two bits). */
	private static final int INT_OFFSET_ALIGN_MASK = 0xFFFFFFFC;

	/* 
	 * For int shift instructions (ishr, iushr, ishl), only the low 5 bits of the
	 * shift amount are considered.
	 */
	private static final int BC_SHIFT_INT_MASK = 0b11111;
	
	/* 
	 * For long shift instructions (lshr, lushr, lshl) only the low 6 bits of the
	 * shift amount are considered.
	 */
	private static final int BC_SHIFT_LONG_MASK = 0b111111;

	/* Mask byte offset of an int. */
	private static final long BYTE_OFFSET_MASK = 0b11L;
	
	static {
		registerNatives();

		theUnsafe = new Unsafe();

		INVALID_FIELD_OFFSET = -1;
		
		/* All OpenJ9 array types have the same array base offset. To limit 
		 * JNI calls arrayBaseOffset is only called once in this block and the
		 * value used for each array base offset variable.
		 */
		ARRAY_BYTE_BASE_OFFSET = theUnsafe.arrayBaseOffset(byte[].class);
		ARRAY_INT_BASE_OFFSET = ARRAY_BYTE_BASE_OFFSET;
		ARRAY_LONG_BASE_OFFSET = ARRAY_BYTE_BASE_OFFSET;
		ARRAY_FLOAT_BASE_OFFSET = ARRAY_BYTE_BASE_OFFSET;
		ARRAY_DOUBLE_BASE_OFFSET = ARRAY_BYTE_BASE_OFFSET;
		ARRAY_SHORT_BASE_OFFSET = ARRAY_BYTE_BASE_OFFSET;
		ARRAY_CHAR_BASE_OFFSET = ARRAY_BYTE_BASE_OFFSET;
		ARRAY_BOOLEAN_BASE_OFFSET = ARRAY_BYTE_BASE_OFFSET;
		ARRAY_OBJECT_BASE_OFFSET = ARRAY_BYTE_BASE_OFFSET;
		
		ARRAY_BYTE_INDEX_SCALE = theUnsafe.arrayIndexScale(byte[].class);
		ARRAY_INT_INDEX_SCALE = theUnsafe.arrayIndexScale(int[].class);
		ARRAY_LONG_INDEX_SCALE = theUnsafe.arrayIndexScale(long[].class);
		ARRAY_FLOAT_INDEX_SCALE = theUnsafe.arrayIndexScale(float[].class);
		ARRAY_DOUBLE_INDEX_SCALE = theUnsafe.arrayIndexScale(double[].class);
		ARRAY_SHORT_INDEX_SCALE = theUnsafe.arrayIndexScale(short[].class);
		ARRAY_CHAR_INDEX_SCALE = theUnsafe.arrayIndexScale(char[].class);
		ARRAY_BOOLEAN_INDEX_SCALE = theUnsafe.arrayIndexScale(boolean[].class);
		ARRAY_OBJECT_INDEX_SCALE = theUnsafe.arrayIndexScale(Object[].class);

		ADDRESS_SIZE = VM.ADDRESS_SIZE;
		IS_BIG_ENDIAN = VM.IS_BIG_ENDIAN;
		
		/* Unaligned access is currently supported on all platforms */
		UNALIGNED_ACCESS = true; 
	}

	/* Attach jdk.internal.misc.Unsafe natives. */
	private static native void registerNatives();
	
	/**
	 * Gets the value of the byte in the obj parameter referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return byte value stored in obj
	 */
	public native byte getByte(Object obj, long offset);

	/**
	 * Sets the value of the byte in the obj parameter at memory offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value byte to store in obj
	 */
	public native void putByte(Object obj, long offset, byte value);
	
	/**
	 * Gets the value of the int in the obj parameter referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return int value stored in obj
	 */
	public native int getInt(Object obj, long offset);

	/**
	 * Sets the value of the int in the obj parameter at memory offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value int to store in obj
	 */
	public native void putInt(Object obj, long offset, int value);
	
	/**
	 * Gets the value of the long in the obj parameter referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return long value stored in obj
	 */
	public native long getLong(Object obj, long offset);

	/**
	 * Sets the value of the long in the obj parameter at memory offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value long to store in obj
	 */
	public native void putLong(Object obj, long offset, long value);

	/**
	 * Gets the value of the float in the obj parameter referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return float value stored in obj
	 */
	public native float getFloat(Object obj, long offset);

	/**
	 * Sets the value of the float in the obj parameter at memory offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value float to store in obj
	 */
	public native void putFloat(Object obj, long offset, float value);

	/**
	 * Gets the value of the double in the obj parameter referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return double value stored in obj
	 */
	public native double getDouble(Object obj, long offset);

	/**
	 * Sets the value of the double in the obj parameter at memory offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value double to store in obj
	 */
	public native void putDouble(Object obj, long offset, double value);

	/**
	 * Gets the value of the short in the obj parameter referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return short value stored in obj
	 */
	public native short getShort(Object obj, long offset);

	/**
	 * Sets the value of the short in the obj parameter at memory offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value short to store in obj
	 */
	public native void putShort(Object obj, long offset, short value);
	
	/**
	 * Gets the value of the char in the obj parameter referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return char value stored in obj
	 */
	public native char getChar(Object obj, long offset);

	/**
	 * Sets the value of the char in the obj parameter at memory offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value char to store in obj
	 */
	public native void putChar(Object obj, long offset, char value);
	
	/**
	 * Gets the value of the boolean in the obj parameter referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return boolean value stored in obj
	 */
	public native boolean getBoolean(Object obj, long offset);

	/**
	 * Sets the value of the boolean in the obj parameter at memory offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value boolean to store in obj
	 */
	public native void putBoolean(Object obj, long offset, boolean value);

	/**
	 * Gets the value of the Object in the obj parameter referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return Object value stored in obj
	 */
	public native Object getObject(Object obj, long offset);

	/**
	 * Sets the value of the Object in the obj parameter at memory offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value Object to store in obj
	 */
	public native void putObject(Object obj, long offset, Object value);

/*[IF Java12]*/
	/**
	 * Gets the value of the Object in the obj parameter referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return Object value stored in obj
	 */
	public native Object getReference(Object obj, long offset);

	/**
	 * Sets the value of the Object in the obj parameter at memory offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value Object to store in obj
	 */
	public native void putReference(Object obj, long offset, Object value);
/*[ENDIF] Java12 */
	
	/**
	 * Gets the value of the Object in memory referenced by address.
	 * This is a non-volatile operation.
	 * 
	 * NOTE: native implementation is a stub.
	 *
	 * @param address position of the object in memory
	 * @return Object value stored in memory
	 */
	public native Object getUncompressedObject(long address);
	
	/**
	 * Allocates a block of memory
	 *
	 * @param size number of bytes to allocate
	 * @return address of allocated buffer
	 * 
	 * @throws IllegalArgumentException if size is negative
	 */
	public native long allocateDBBMemory(long size);
	
	/**
	 * Reallocates a block of memory.
	 * 
	 * @param address of old buffer
	 * @param size new size of buffer to allocate
	 * @return address of new buffer
	 * 
	 * @throws IllegalArgumentException if size is negative
	 */
	public native long reallocateDBBMemory(long address, long size);
	
	/**
	 * Removes the block from the list.  Note that the pointers are located 
	 * immediately before the address value.
	 * 
	 * @param address of buffer
	 */
	public native void freeDBBMemory(long address);

	/**
	 * Returns the size of a page in memory in bytes.
	 * 
	 * @return size of memory page
	 */
	public native int pageSize();

	/**
	 * Creates a class out of a given array of bytes with a ProtectionDomain.
	 * 
	 * @param name binary name of the class, null if the name is not known
	 * @param b a byte array of the class data. The bytes should have the format of a valid 
	 * class file as defined by The JVM Spec
	 * @param offset offset of the start of the class data in b
	 * @param bLength length of the class data
	 * @param cl used to load the class being built. If null, the default 
	 * system ClassLoader will be used
	 * @param pd ProtectionDomain for new class
	 * @return the Class created from the byte array and ProtectionDomain parameters
	 * 
	 * @throws IndexOutOfBoundsException if offset or bLength is negative, or if offset + bLength 
	 * is greater than the length of b
	 */
	public native Class<?> defineClass0(String name, byte[] b, int offset, int bLength, ClassLoader cl,
			ProtectionDomain pd);

	/**
	 * Allocate instance of class parameter.
	 * 
	 * @param c class to allocate
	 * @return instance of class c
	 * 
	 * @throws InstantiationException if class c cannot be instantiated
	 */
	public native Object allocateInstance(Class<?> c) throws InstantiationException;

	/**
	 * Throw Throwable parameter.
	 * 
	 * @param t Throwable to execute
	 * 
	 * @throws NullPointerException if Throwable is null
	 */
	public native void throwException(Throwable t);

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final native boolean compareAndSetInt(Object obj, long offset, int compareValue, int setValue);

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final native int compareAndExchangeInt(Object obj, long offset, int compareValue, int exchangeValue);

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final native boolean compareAndSetLong(Object obj, long offset, long compareValue, long setValue);

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final native long compareAndExchangeLong(Object obj, long offset, long compareValue, long exchangeVale);

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final native boolean compareAndSetObject(Object obj, long offset, Object compareValue, Object setValue);

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final native Object compareAndExchangeObject(Object obj, long offset, Object compareValue,
			Object exchangeValue);

/*[IF Java12]*/
	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final native boolean compareAndSetReference(Object obj, long offset, Object compareValue, Object setValue);

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final native Object compareAndExchangeReference(Object obj, long offset, Object compareValue,
			Object exchangeValue);
/*[ENDIF] Java12 */

	/**
	 * Atomically gets the value of the byte in the obj parameter referenced by offset.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return byte value stored in obj
	 */
	public native byte getByteVolatile(Object obj, long offset);

	/**
	 * Atomically sets the value of the byte in the obj parameter at memory offset.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value byte to store in obj
	 */
	public native void putByteVolatile(Object obj, long offset, byte value);
	
	/**
	 * Atomically gets the value of the int in the obj parameter referenced by offset.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return int value stored in obj
	 */
	public native int getIntVolatile(Object obj, long offset);

	/**
	 * Atomically sets the value of the int in the obj parameter at memory offset.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value int to store in obj
	 */
	public native void putIntVolatile(Object obj, long offset, int value);

	/**
	 * Atomically gets the value of the long in the obj parameter referenced by offset.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return long value stored in obj
	 */
	public native long getLongVolatile(Object obj, long offset);

	/**
	 * Atomically sets the value of the long in the obj parameter at memory offset.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value long to store in obj
	 */
	public native void putLongVolatile(Object obj, long offset, long value);

	/**
	 * Atomically gets the value of the float in the obj parameter referenced by offset.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return float value stored in obj
	 */
	public native float getFloatVolatile(Object obj, long offset);

	/**
	 * Atomically sets the value of the float in the obj parameter at memory offset.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value float to store in obj
	 */
	public native void putFloatVolatile(Object obj, long offset, float value);

	/**
	 * Atomically gets the value of the double in the obj parameter referenced by offset.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return double value stored in obj
	 */
	public native double getDoubleVolatile(Object obj, long offset);

	/**
	 * Atomically sets the value of the double in the obj parameter at memory offset.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value double to store in obj
	 */
	public native void putDoubleVolatile(Object obj, long offset, double value);
	
	/**
	 * Atomically gets the value of the short in the obj parameter referenced by offset.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return short value stored in obj
	 */
	public native short getShortVolatile(Object obj, long offset);

	/**
	 * Atomically sets the value of the short in the obj parameter at memory offset.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value short to store in obj
	 */
	public native void putShortVolatile(Object obj, long offset, short value);

	/**
	 * Atomically gets the value of the char in the obj parameter referenced by offset.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return char value stored in obj
	 */
	public native char getCharVolatile(Object obj, long offset);

	/**
	 * Atomically sets the value of the char in the obj parameter at memory offset.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value char to store in obj
	 */
	public native void putCharVolatile(Object obj, long offset, char value);

	/**
	 * Atomically gets the value of the boolean in the obj parameter referenced by offset.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return boolean value stored in obj
	 */
	public native boolean getBooleanVolatile(Object obj, long offset);

	/**
	 * Atomically sets the value of the boolean in the obj parameter at memory offset.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value boolean to store in obj
	 */
	public native void putBooleanVolatile(Object obj, long offset, boolean value);
	
	/**
	 * Atomically gets the value of the Object in the obj parameter referenced by offset.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return Object value stored in obj
	 */
	public native Object getObjectVolatile(Object obj, long offset);

	/**
	 * Atomically sets the value of the Object in the obj parameter at memory offset.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value Object to store in obj
	 */
	public native void putObjectVolatile(Object obj, long offset, Object value);

/*[IF Java12]*/
	/**
	 * Atomically gets the value of the Object in the obj parameter referenced by offset.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return Object value stored in obj
	 */
	public native Object getReferenceVolatile(Object obj, long offset);

	/**
	 * Atomically sets the value of the Object in the obj parameter at memory offset.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value Object to store in obj
	 */
	public native void putReferenceVolatile(Object obj, long offset, Object value);
/*[ENDIF] Java12 */

	/**
	 * Makes permit available for thread parameter.
	 * 
	 * @param thread the thread to unpark. If null, method has no effect
	 */
	public native void unpark(Object thread);

	/**
	 * Disables current thread unless permit is available. Thread will not be 
	 * scheduled until unpark provides permit.
	 * 
	 * @param isAbsolute if true park timeout should be absolute
	 * @param time parks for time in ns. 
	 *	 ms is expected if isAbsolute is set to true.
	 */
	public native void park(boolean isAbsolute, long time);

	/**
	 * Inserts an acquire memory fence, ensuring that no loads before this fence
	 * are reordered with any loads/stores after the fence.
	 */
	public native void loadFence();

	/**
	 * Inserts a release memory fence, ensuring that no stores before this fence
	 *  are reordered with any loads/stores after the fence.
	 */
	public native void storeFence();

	/**
	 * Inserts a complete memory fence, ensuring that no loads/stores before this
	 *  fence are reordered with any loads/stores after the fence.
	 */
	public native void fullFence();
	
	/* 
	 * jdk.internal.misc.Unsafe natives 
	 */
	/* 
	 * Allocates a block of memory.
	 * If size passed is 0, no memory will be allocated.
	 * 
	 * @param size requested size of memory in bytes
	 * @return starting address of memory
	 * 
	 * @throws IllegalArgumentException if size is not valid
	 */
	private native long allocateMemory0(long size);

	/* 
	 * Reallocates a block of memory.
	 * If size passed is 0, no memory will be allocated.
	 * 
	 * @param size requested size of memory in bytes
	 * @return starting address of memory
	 * 
	 * @throws IllegalArgumentException if size is not valid
	 */
	private native long reallocateMemory0(long address, long size);

	/* 
	 * Removes the block from the list. Note that the pointers are located 
	 * immediately before the startIndex value.
	 * 
	 * @param startIndex memory address
	 * 
	 * @throws IllegalArgumentException if startIndex is not valid
	 */
	private native void freeMemory0(long startIndex);
	
	/* 
	 * Set object at offset in obj parameter.
	 * 
	 * @param obj object into which to store the value
	 * @param startIndex position of the value in obj
	 * @param size the number of bytes to be set to the value
	 * @param replace overwrite memory with this value
	 *            
	 * @throws IllegalArgumentException if startIndex is illegal in obj, or if size is invalid
	 */
	private native void setMemory0(Object obj, long startIndex, long size, byte replace);

	/* 
	 * Copy bytes from source object to destination.
	 * 
	 * @param srcObj object to copy from 
	 * @param srcOffset location in srcObj to start copy 
	 * @param destObj object to copy into
	 * @param destOffset location in destObj to start copy
	 * @param size the number of bytes to be copied
	 * 
	 * @throws IllegalArgumentException if srcOffset is illegal in srcObj, 
	 * if destOffset is illegal in destObj, or if size is invalid
	 */
	private native void copyMemory0(Object srcObj, long srcOffset, Object destObj, long destOffset, long size);

	/*
	 * Copy bytes from source object to destination in reverse order.
	 * Memory is reversed in elementSize chunks.
	 * 
	 * @param srcObj object to copy from 
	 * @param srcOffset location in srcObj to start copy
	 * @param destObj object to copy into
	 * @param destOffset location in destObj to start copy
	 * @param copySize the number of bytes to be copied, a multiple of elementSize
	 * @param elementSize the size in bytes of elements that will be reversed
	 * 
	 * @throws IllegalArgumentException if srcOffset is illegal in srcObj, 
	 * if destOffset is illegal in destObj, if copySize is invalid or copySize is not
	 * a multiple of elementSize
	 */
	private native void copySwapMemory0(Object srcObj, long srcOffset, Object destObj, long destOffset, long copySize,
			long elementSize);

	/* 
	 * Returns byte offset to field.
	 * 
	 * @param field which contains desired class or interface
	 * @return offset to start of class or interface
	 * 
	 * @throws IllegalArgumentException if field is static
	 */
	private native long objectFieldOffset0(Field field);
	
/*[IF Java10]*/
	/*
	 * Returns byte offset to field.
	 * 
	 * @param class with desired field
	 * @param string name of desired field
	 * @return offset to start of class or interface
	 * 
	 * @throws IllegalArgumentException if field is static
	 */
	private native long objectFieldOffset1(Class<?> c, String fieldName);
/*[ENDIF]*/

	/* 
	 * Returns byte offset to start of static class or interface.
	 * 
	 * @param field which contains desired class or interface
	 * @return offset to start of class or interface
	 * 
	 * @throws NullPointerException if field parameter is null
	 * @throws IllegalArgumentException if field is not static
	 */
	private native long staticFieldOffset0(Field field);

	/* 
	 * Returns class or interface described by static Field.
	 * 
	 * @param field contains desired class or interface
	 * @return class or interface in static field
	 * 
	 * @throws NullPointerException if field parameter is null
	 * @throws IllegalArgumentException if field is not static
	 */
	private native Object staticFieldBase0(Field field);

	/* 
	 * Determines whether class has been initialized.
	 * 
	 * @param class to verify
	 * @return true if method has not been initialized, false otherwise
	 */
	private native boolean shouldBeInitialized0(Class<?> c);

	/* 
	 * Initializes class parameter if it has not been already.
	 * 
	 * @param c class to initialize if not already
	 * 
	 * @throws NullPointerException if class is null
	 */
	private native void ensureClassInitialized0(Class<?> c);

	/* 
	 * Return offset in array object at which the array data storage begins.
	 * 
	 * @param c class array
	 * @return offset at base of array
	 * 
	 * @throws NullPointerException if the class parameter is null
	 * @throws IllegalArgumentException if class is not an array
	 */
	private native int arrayBaseOffset0(Class<?> c);

	/* 
	 * Return index size of array in bytes.
	 * 
	 * @param c class array
	 * @return returns index size of array in bytes
	 * 
	 * @throws NullPointerException if class is null
	 * @throws IllegalArgumentException if class is not an array
	 */
	private native int arrayIndexScale0(Class<?> c);

	/* @return size of address on machine in use */
	private native int addressSize0();

	/* 
	 * Define a class without making it known to the class loader.
	 * 
	 * @param hostingClass the context for class loader + linkage, and 
	 * access control + protection domain
	 * @param bytecodes class file bytes
	 * @param constPatches entries that are not "null" are replacements 
	 * for the corresponding const pool entries in the 'bytecodes' data
	 * @return class created from bytecodes and constPatches
	 */
	private native Class<?> defineAnonymousClass0(Class<?> hostingClass, byte[] bytecodes, Object[] constPatches);

	/* 
	 * Get the load average in the system.
	 * 
	 * NOTE: native implementation is a stub.
	 * 
	 * @param loadavg array of elements
	 * @param numberOfElements number of samples
	 * @return load average
	 */
	private native int getLoadAverage0(double[] loadavg, int numberOfElements);

	/* @return true if addresses are aligned in memory, false otherwise */
	private native boolean unalignedAccess0();

	/* @return true if machine is big endian, false otherwise */
	private native boolean isBigEndian0();
	
	/**
	 * Getter for unsafe instance.
	 * 
	 * @return unsafe instance
	 */
	public static Unsafe getUnsafe() {
		return theUnsafe;
	}
	
	/**
	 * Gets the address in the obj parameter referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the address
	 * @param address location to retrieve the address in obj
	 * @return address stored in obj
	 */
	public long getAddress(Object obj, long address) {
		long result;

		if (BYTES_IN_INT == ADDRESS_SIZE) {
			result = Integer.toUnsignedLong(getInt(obj, address));
		} else {
			result = getLong(obj, address);
		}

		return result;
	}

	/**
	 * Sets an address in the obj parameter at memory offset.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the address
	 * @param address position to store value in obj
	 * @param value address to store in obj
	 */
	public void putAddress(Object obj, long address, long value) {
		if (BYTES_IN_INT == ADDRESS_SIZE) {
			putInt(obj, address, (int) value);
		} else {
			putLong(obj, address, value);
		}
	}

	/**
	 * Gets the value of the byte in memory referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param locations where to retrieve value in memory
	 * @return byte value stored in memory
	 */
	public byte getByte(long offset) {
		return getByte(null, offset);
	}

	/**
	 * Sets the value of the byte at memory offset.
	 * This is a non-volatile operation.
	 *  
	 * @param offset location to retrieve value in memory
	 * @param value byte to store in memory
	 */
	public void putByte(long offset, byte value) {
		putByte(null, offset, value);
	}
	
	/**
	 * Gets the value of the int in memory referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param offset location to retrieve value in memory
	 * @return int value stored in memory
	 */
	public int getInt(long offset) {
		return getInt(null, offset);
	}

	/**
	 * Sets the value of the int at memory offset.
	 * This is a non-volatile operation.
	 *  
	 * @param offset location to retrieve value in memory
	 * @param value int to store in memory
	 */
	public void putInt(long offset, int value) {
		putInt(null, offset, value);
	}
	
	/**
	 * Gets the value of the long in memory referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param offset location to retrieve value in memory
	 * @return long value stored in memory
	 */
	public long getLong(long offset) {
		return getLong(null, offset);
	}

	/**
	 * Sets the value of the long at memory offset.
	 * This is a non-volatile operation.
	 *  
	 * @param offset location to retrieve value in memory
	 * @param value long to store in memory
	 */
	public void putLong(long offset, long value) {
		putLong(null, offset, value);
	}

	/**
	 * Gets the value of the float in memory referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param offset location to retrieve value in memory
	 * @return float value stored in memory
	 */
	public float getFloat(long offset) {
		return getFloat(null, offset);
	}

	/**
	 * Sets the value of the float at memory offset.
	 * This is a non-volatile operation.
	 *  
	 * @param offset location to retrieve value in memory
	 * @param value float to store in memory
	 */
	public void putFloat(long offset, float value) {
		putFloat(null, offset, value);
	}

	/**
	 * Gets the value of the double in memory referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param offset location to retrieve value in memory
	 * @return double value stored in memory
	 */
	public double getDouble(long offset) {
		return getDouble(null, offset);
	}

	/**
	 * Sets the value of the double at memory offset.
	 * This is a non-volatile operation.
	 *  
	 * @param offset location to retrieve value in memory
	 * @param value double to store in memory
	 */
	public void putDouble(long offset, double value) {
		putDouble(null, offset, value);
	}

	/**
	 * Gets the value of the short in memory referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param offset location to retrieve value in memory
	 * @return short value stored in memory
	 */
	public short getShort(long offset) {
		return getShort(null, offset);
	}

	/**
	 * Sets the value of the short at memory offset.
	 * This is a non-volatile operation.
	 *  
	 * @param offset location to retrieve value in memory
	 * @param value short to store in memory
	 */
	public void putShort(long offset, short value) {
		putShort(null, offset, value);
	}

	/**
	 * Gets the value of the char in memory referenced by offset.
	 * This is a non-volatile operation.
	 * 
	 * @param offset location to retrieve value in memory
	 * @return char value stored in memory
	 */
	public char getChar(long offset) {
		return getChar(null, offset);
	}

	/**
	 * Sets the value of the char at memory offset.
	 * This is a non-volatile operation.
	 *  
	 * @param offset location to retrieve value in memory
	 * @param value char to store in memory
	 */
	public void putChar(long offset, char value) {
		putChar(null, offset, value);
	}

	/**
	 * Gets the address value in memory at location of address parameter.
	 * This is a non-volatile operation.
	 * 
	 * @param address location to retrieve the address in memory
	 * @return address stored in obj
	 */
	public long getAddress(long address) {
		return getAddress(null, address);

	}

	/**
	 * Sets an address value at the location by the address parameter.
	 * This is a non-volatile operation.
	 * 
	 * @param address location to store value in memory
	 * @param value address to store at address parameter
	 */
	public void putAddress(long address, long value) {
		putAddress(null, address, value);
	}

	/**
	 * Allocates a block of memory.
	 * If size passed is 0, no memory will be allocated.
	 * 
	 * @param size requested size of memory in bytes
	 * @return starting address of memory, or 0 if size is 0
	 * 
	 * @throws OutOfMemoryError if no memory can be allocated
	 * @throws IllegalArgumentException if size is not valid
	 */
	public long allocateMemory(long size) {
		allocateMemoryChecks(size);

		if (0L == size) {
			return 0L;
		}

		long address = allocateMemory0(size);

		if (0L == address) {
			throw new OutOfMemoryError();
		}

		return address;
	}

	/**
	 * Reallocates a block of memory.
	 * If size passed is 0, no memory will be allocated.
	 * 
	 * @param size requested size of memory in bytes
	 * @return starting address of memory, or 0 if size is 0
	 * 
	 * @throws OutOfMemoryError if no memory can be allocated
	 * @throws IllegalArgumentException if size is not valid
	 */
	public long reallocateMemory(long address, long size) {
		reallocateMemoryChecks(address, size);

		if (0L == size) {
			freeMemory(address);
			return 0L;
		}

		long addressNew;
		if (0L == address) {
			addressNew = allocateMemory0(size);
		} else {
			addressNew = reallocateMemory0(address, size);
		}

		if (0L == addressNew) {
			throw new OutOfMemoryError();
		}

		return addressNew;
	}

	/**
	 * Set the byte at the index and size in obj parameter.
	 * 
	 * @param obj object into which to store the value
	 * @param startIndex position of the value in obj
	 * @param size the number of bytes to be set to the value
	 * @param replace overwrite memory with this value
	 *            
	 * @throws IllegalArgumentException if startIndex is illegal in obj, or if size is invalid
	 */
	public void setMemory(Object obj, long startIndex, long size, byte replace) {
		setMemoryChecks(obj, startIndex, size, replace);

		if (0 != size) {
			setMemory0(obj, startIndex, size, replace);
		}
	}

	/**
	 * Set the byte at the index and size.
	 * 
	 * @param startAddress location to store value in memory
	 * @param size the number of bytes to be set to the value
	 * @param replace overwrite memory with this value
	 *            
	 * @throws IllegalArgumentException if startIndex is illegal, or if size is invalid
	 */
	public void setMemory(long startAddress, long size, byte replace) {
		setMemory(null, startAddress, size, replace);
	}

	/**
	 * Copy bytes from source object to destination.
	 * 
	 * @param srcObj object to copy from 
	 * @param srcOffset location in srcObj to start copy 
	 * @param destObj object to copy into
	 * @param destOffset location in destObj to start copy
	 * @param size the number of bytes to be copied
	 * 
	 * @throws IllegalArgumentException if srcOffset is illegal in srcObj, 
	 * if destOffset is illegal in destObj, or if size is invalid
	 */
	public void copyMemory(Object srcObj, long srcOffset, Object destObj, long destOffset, long size) {
		copyMemoryChecks(srcObj, srcOffset, destObj, destOffset, size);

		if (0 != size) {
			copyMemory0(srcObj, srcOffset, destObj, destOffset, size);
		}
	}

	/**
	 * Copy bytes from source address to destination.
	 * 
	 * @param srcAddress address to start copy
	 * @param destAddress address to start copy
	 * @param size the number of bytes to be copied
	 * 
	 * @throws IllegalArgumentException if srcAddress or destAddress
	 *  is illegal, or if size is invalid
	 */
	public void copyMemory(long srcAddress, long destAddress, long size) {
		copyMemory(null, srcAddress, null, destAddress, size);
	}

	/**
	 * Copy bytes from source object to destination in reverse order.
	 * Memory is reversed in elementSize chunks.
	 * 
	 * @param srcObj object to copy from 
	 * @param srcOffset location in srcObj to start copy
	 * @param destObj object to copy into
	 * @param destOffset location in destObj to start copy
	 * @param copySize the number of bytes to be copied, a multiple of elementSize
	 * @param elementSize the size in bytes of elements that will be reversed
	 * 
	 * @throws IllegalArgumentException if srcOffset is illegal in srcObj, 
	 * if destOffset is illegal in destObj, if copySize is invalid or copySize is not
	 * a multiple of elementSize
	 */
	public void copySwapMemory(Object srcObj, long srcOffset, Object destObj, long destOffset, long copySize,
			long elementSize) {
		copySwapMemoryChecks(srcObj, srcOffset, destObj, destOffset, copySize, elementSize);

		if (0 != copySize) {
			copySwapMemory0(srcObj, srcOffset, destObj, destOffset, copySize, elementSize);
		}
	}

	/**
	 * Copy bytes from source address to destination in reverse order.
	 * Memory is reversed in elementSize chunks.
	 * 
	 * @param srcAddress location to start copy
	 * @param destAddress location to start copy
	 * @param copySize the number of bytes to be copied, a multiple of elementSize
	 * @param elementSize the size in bytes of elements that will be reversed
	 * 
	 * @throws IllegalArgumentException if srcAddress or destAddress is illegal, 
	 * if copySize is invalid or copySize is not a multiple of elementSize
	 */
	public void copySwapMemory(long srcAddress, long destAddress, long copySize, long elementSize) {
		copySwapMemory(null, srcAddress, null, destAddress, copySize, elementSize);
	}

	/**
	 * Removes the block from the list. Note that the pointers are located 
	 * immediately before the startIndex value.
	 * 
	 * @param startIndex memory address
	 * 
	 * @throws IllegalArgumentException if startIndex is not valid
	 */
	public void freeMemory(long startIndex) {
		freeMemoryChecks(startIndex);

		if (0 != startIndex) {
			freeMemory0(startIndex);
		}
	}

	/**
	 * Returns byte offset to field.
	 * 
	 * @param field which contains desired class or interface
	 * @return offset to start of class or interface
	 * 
	 * @throws NullPointerException if field parameter is null
	 * @throws IllegalArgumentException if field is static
	 */
	public long objectFieldOffset(Field field) {
		Objects.requireNonNull(field);
		return objectFieldOffset0(field);
	}
	
/*[IF Java10]*/
	/**
	 * Returns byte offset to field.
	 * 
	 * @param class with desired field
	 * @param string name of desired field
	 * @return offset to start of class or interface
	 * 
	 * @throws NullPointerException if field parameter is null
	 * @throws IllegalArgumentException if field is static
	 */
	public long objectFieldOffset(Class<?> c, String fieldName) {
		Objects.requireNonNull(c);
		Objects.requireNonNull(fieldName);
		return objectFieldOffset1(c, fieldName);
	}
/*[ENDIF]*/

	/**
	 * Returns byte offset to start of static class or interface.
	 * 
	 * @param field which contains desired class or interface
	 * @return offset to start of class or interface
	 * 
	 * @throws NullPointerException if field parameter is null
	 * @throws IllegalArgumentException if field is not static
	 */
	public long staticFieldOffset(Field field) {
		Objects.requireNonNull(field);
		return staticFieldOffset0(field);
	}

	/**
	 * Returns class or interface described by static Field.
	 * 
	 * @param field contains desired class or interface
	 * @return class or interface in static field
	 * 
	 * @throws NullPointerException if field parameter is null
	 * @throws IllegalArgumentException if field is not static
	 */
	public Object staticFieldBase(Field field) {
		Objects.requireNonNull(field);
		return staticFieldBase0(field);
	}

	/**
	 * Determines whether class has been initialized.
	 * 
	 * @param class to verify
	 * @return true if method has not been initialized, false otherwise
	 * 
	 * @throws NullPointerException if class is null
	 */
	public boolean shouldBeInitialized(Class<?> c) {
		Objects.requireNonNull(c);
		return shouldBeInitialized0(c);
	}

	/** 
	 * Initializes class parameter if it has not been already.
	 * 
	 * @param c class to initialize if not already
	 * 
	 * @throws NullPointerException if class is null
	 */
	public void ensureClassInitialized(Class<?> c) {
		Objects.requireNonNull(c);
		ensureClassInitialized0(c);
	}

	/**
	 * Return offset in array object at which the array data storage begins.
	 * 
	 * @param c class array
	 * @return offset at base of array
	 * 
	 * @throws NullPointerException if the class parameter is null
	 * @throws IllegalArgumentException if class is not an array
	 */
	public int arrayBaseOffset(Class<?> c) {
		Objects.requireNonNull(c);
		return arrayBaseOffset0(c);
	}

	/**
	 * Return index size of array in bytes.
	 * 
	 * @param c class array
	 * @return returns index size of array in bytes
	 * 
	 * @throws NullPointerException if class is null
	 * @throws IllegalArgumentException if class is not an array
	 */
	public int arrayIndexScale(Class<?> c) {
		Objects.requireNonNull(c);
		return arrayIndexScale0(c);
	}

	/**
	 * @return size of address on machine in use
	 */
	public int addressSize() {
		return ADDRESS_SIZE;
	}

	/**
	 * Creates a class out of a given array of bytes with a ProtectionDomain.
	 * 
	 * @param name binary name of the class, null if the name is not known
	 * @param b a byte array of the class data. The bytes should have the format of a 
	 * 			valid class file as defined by The JVM Spec
	 * @param offset offset of the start of the class data in b
	 * @param bLength length of the class data
	 * @param cl ClassLoader used to load the class being built. If null, the default 
	 * system ClassLoader will be used
	 * @param pd ProtectionDomain for new class
	 * @return class created from the byte array and ProtectionDomain parameters
	 * 
	 * @throws NullPointerException if b array of data is null
	 * @throws ArrayIndexOutOfBoundsException if bLength is negative
	 * @throws IndexOutOfBoundsException if offset + bLength is greater than the 
	 * length of b
	 */
	public Class<?> defineClass(String name, byte[] b, int offset, int bLength, ClassLoader cl, ProtectionDomain pd) {
		Objects.requireNonNull(b);

		if (bLength < 0) {
			throw new ArrayIndexOutOfBoundsException();
		}

		Class<?> result = defineClass0(name, b, offset, bLength, cl, pd);
		VMLangAccess access = VM.getVMLangAccess();
		access.addPackageToList(result, cl);
		return result;
	}

	/**
	 * Define a class without making it known to the class loader.
	 * 
	 * @param hostingClass the context for class loader + linkage, and 
	 * access control + protection domain
	 * @param bytecodes class file bytes
	 * @param constPatches entries that are not "null" are replacements 
	 * for the corresponding const pool entries in the 'bytecodes' data
	 * @return class created from bytecodes and constPatches
	 * 
	 * @throws NullPointerException if hostingClass or bytecodes is null
	 * @throws IllegalArgumentException if hostingClass is an array or primitive
	 */
	public Class<?> defineAnonymousClass(Class<?> hostingClass, byte[] bytecodes, Object[] constPatches) {
		Objects.requireNonNull(hostingClass);
		Objects.requireNonNull(bytecodes);
		
		if (hostingClass.isArray() || hostingClass.isPrimitive()) {
			throw invalidInput();
		}

		return defineAnonymousClass0(hostingClass, bytecodes, constPatches);
	}

	/**
	 * Allocate new array of same type as class parameter and 
	 * length of int parameter.
	 * 
	 * @param c class of same type as desired array
	 * @param length desired length of array
	 * @return allocated array of desired length and type
	 * 
	 * @throws IllegalArgumentException if class is null, if class 
	 * is not primitive, or if length is negative
	 */
	public Object allocateUninitializedArray(Class<?> c, int length) {
		if (null == c) {
			/*[MSG "K0701", "Component type is null"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0701")); //$NON-NLS-1$
		}

		if (!c.isPrimitive()) {
			/*[MSG "K0702", "Component type is not primitive"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0702")); //$NON-NLS-1$
		}

		if (length < 0) {
			/*[MSG "K0703", "Negative length"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0703")); //$NON-NLS-1$
		}

		return allocateUninitializedArray0(c, length);
	}


	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final byte compareAndExchangeByte(Object obj, long offset, byte compareValue, byte exchangeValue) {
		return compareAndExchange8bits(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean compareAndSetByte(Object obj, long offset, byte compareValue, byte setValue) {
		byte exchangedValue = compareAndExchangeByte(obj, offset, compareValue, setValue);
		return (compareValue == exchangedValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetByte(Object obj, long offset, byte compareValue, byte setValue) {
		return compareAndSetByte(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetByteAcquire(Object obj, long offset, byte compareValue, byte setValue) {
		return weakCompareAndSetByte(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetByteRelease(Object obj, long offset, byte compareValue, byte setValue) {
		return weakCompareAndSetByte(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetBytePlain(Object obj, long offset, byte compareValue, byte setValue) {
		return weakCompareAndSetByte(obj, offset, compareValue, setValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final byte compareAndExchangeByteAcquire(Object obj, long offset, byte compareValue, byte exchangeValue) {
		return compareAndExchangeByte(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final byte compareAndExchangeByteRelease(Object obj, long offset, byte compareValue, byte exchangeValue) {
		return compareAndExchangeByte(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final int compareAndExchangeIntAcquire(Object obj, long offset, int compareValue, int exchangeValue) {
		return compareAndExchangeInt(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final int compareAndExchangeIntRelease(Object obj, long offset, int compareValue, int exchangeValue) {
		return compareAndExchangeInt(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetIntPlain(Object obj, long offset, int compareValue, int setValue) {
		return compareAndSetInt(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetIntAcquire(Object obj, long offset, int compareValue, int setValue) {
		return compareAndSetInt(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetIntRelease(Object obj, long offset, int compareValue, int setValue) {
		return compareAndSetInt(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetInt(Object obj, long offset, int compareValue, int setValue) {
		return compareAndSetInt(obj, offset, compareValue, setValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final long compareAndExchangeLongAcquire(Object obj, long offset, long compareValue, long exchangeValue) {
		return compareAndExchangeLong(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final long compareAndExchangeLongRelease(Object obj, long offset, long compareValue, long exchangeValue) {
		return compareAndExchangeLong(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetLongPlain(Object obj, long offset, long compareValue, long setValue) {
		return compareAndSetLong(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetLongAcquire(Object obj, long offset, long compareValue, long setValue) {
		return compareAndSetLong(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetLongRelease(Object obj, long offset, long compareValue, long setValue) {
		return compareAndSetLong(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetLong(Object obj, long offset, long compareValue, long setValue) {
		return compareAndSetLong(obj, offset, compareValue, setValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean compareAndSetFloat(Object obj, long offset, float compareValue, float setValue) {
		return compareAndSetInt(obj, offset, Float.floatToRawIntBits(compareValue), Float.floatToRawIntBits(setValue));
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final float compareAndExchangeFloat(Object obj, long offset, float compareValue, float exchangeValue) {
		int result = compareAndExchangeInt(obj, offset, Float.floatToRawIntBits(compareValue),
				Float.floatToRawIntBits(exchangeValue));
		return Float.intBitsToFloat(result);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final float compareAndExchangeFloatAcquire(Object obj, long offset, float compareValue,
			float exchangeValue) {
		int result = compareAndExchangeIntAcquire(obj, offset, Float.floatToRawIntBits(compareValue),
				Float.floatToRawIntBits(exchangeValue));
		return Float.intBitsToFloat(result);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final float compareAndExchangeFloatRelease(Object obj, long offset, float compareValue,
			float exchangeValue) {
		int result = compareAndExchangeIntRelease(obj, offset, Float.floatToRawIntBits(compareValue),
				Float.floatToRawIntBits(exchangeValue));
		return Float.intBitsToFloat(result);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetFloatPlain(Object obj, long offset, float compareValue, float setValue) {
		return weakCompareAndSetIntPlain(obj, offset, Float.floatToRawIntBits(compareValue),
				Float.floatToRawIntBits(setValue));
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetFloatAcquire(Object obj, long offset, float compareValue, float setValue) {
		return weakCompareAndSetIntAcquire(obj, offset, Float.floatToRawIntBits(compareValue),
				Float.floatToRawIntBits(setValue));
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetFloatRelease(Object obj, long offset, float compareValue, float setValue) {
		return weakCompareAndSetIntRelease(obj, offset, Float.floatToRawIntBits(compareValue),
				Float.floatToRawIntBits(setValue));
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetFloat(Object obj, long offset, float compareValue, float setValue) {
		return weakCompareAndSetInt(obj, offset, Float.floatToRawIntBits(compareValue),
				Float.floatToRawIntBits(setValue));
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean compareAndSetDouble(Object obj, long offset, double compareValue, double setValue) {
		return compareAndSetLong(obj, offset, Double.doubleToRawLongBits(compareValue),
				Double.doubleToRawLongBits(setValue));
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final double compareAndExchangeDouble(Object obj, long offset, double compareValue, double exchangeValue) {
		long result = compareAndExchangeLong(obj, offset, Double.doubleToRawLongBits(compareValue),
				Double.doubleToRawLongBits(exchangeValue));
		return Double.longBitsToDouble(result);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final double compareAndExchangeDoubleAcquire(Object obj, long offset, double compareValue,
			double exchangeValue) {
		long result = compareAndExchangeLongAcquire(obj, offset, Double.doubleToRawLongBits(compareValue),
				Double.doubleToRawLongBits(exchangeValue));
		return Double.longBitsToDouble(result);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final double compareAndExchangeDoubleRelease(Object obj, long offset, double compareValue,
			double exchangeValue) {
		long result = compareAndExchangeLongRelease(obj, offset, Double.doubleToRawLongBits(compareValue),
				Double.doubleToRawLongBits(exchangeValue));
		return Double.longBitsToDouble(result);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetDoublePlain(Object obj, long offset, double compareValue, double swapValue) {
		return weakCompareAndSetLongPlain(obj, offset, Double.doubleToRawLongBits(compareValue),
				Double.doubleToRawLongBits(swapValue));
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetDoubleAcquire(Object obj, long offset, double compareValue,
			double swapValue) {
		return weakCompareAndSetLongAcquire(obj, offset, Double.doubleToRawLongBits(compareValue),
				Double.doubleToRawLongBits(swapValue));
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetDoubleRelease(Object obj, long offset, double compareValue,
			double swapValue) {
		return weakCompareAndSetLongRelease(obj, offset, Double.doubleToRawLongBits(compareValue),
				Double.doubleToRawLongBits(swapValue));
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetDouble(Object obj, long offset, double compareValue, double swapValue) {
		return weakCompareAndSetLong(obj, offset, Double.doubleToRawLongBits(compareValue),
				Double.doubleToRawLongBits(swapValue));
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final short compareAndExchangeShort(Object obj, long offset, short compareValue, short exchangeValue) {
		compareAndExchange16BitsOffsetChecks(offset);
		return compareAndExchange16bits(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final boolean compareAndSetShort(Object obj, long offset, short compareValue, short setValue) {
		short exchangedValue = compareAndExchangeShort(obj, offset, compareValue, setValue);
		return (compareValue == exchangedValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final boolean weakCompareAndSetShort(Object obj, long offset, short compareValue, short setValue) {
		return compareAndSetShort(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final boolean weakCompareAndSetShortAcquire(Object obj, long offset, short compareValue, short setValue) {
		return weakCompareAndSetShort(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final boolean weakCompareAndSetShortRelease(Object obj, long offset, short compareValue, short setValue) {
		return weakCompareAndSetShort(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final boolean weakCompareAndSetShortPlain(Object obj, long offset, short compareValue, short setValue) {
		return weakCompareAndSetShort(obj, offset, compareValue, setValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final short compareAndExchangeShortAcquire(Object obj, long offset, short compareValue,
			short exchangeValue) {
		return compareAndExchangeShort(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final short compareAndExchangeShortRelease(Object obj, long offset, short compareValue,
			short exchangeValue) {
		return compareAndExchangeShort(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final boolean compareAndSetChar(Object obj, long offset, char compareValue, char setValue) {
		char exchangedValue = compareAndExchangeChar(obj, offset, compareValue, setValue);
		return (compareValue == exchangedValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final char compareAndExchangeChar(Object obj, long offset, char compareValue, char exchangeValue) {
		compareAndExchange16BitsOffsetChecks(offset);
		short result = compareAndExchange16bits(obj, offset, compareValue, exchangeValue);
		return s2c(result);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final char compareAndExchangeCharAcquire(Object obj, long offset, char compareValue, char exchangeValue) {
		return compareAndExchangeChar(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final char compareAndExchangeCharRelease(Object obj, long offset, char compareValue, char exchangeValue) {
		return compareAndExchangeChar(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final boolean weakCompareAndSetChar(Object obj, long offset, char compareValue, char setValue) {
		return compareAndSetChar(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final boolean weakCompareAndSetCharAcquire(Object obj, long offset, char compareValue, char setValue) {
		return weakCompareAndSetChar(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final boolean weakCompareAndSetCharRelease(Object obj, long offset, char compareValue, char setValue) {
		return weakCompareAndSetChar(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 * 
	 * @throws IllegalArgumentException if value at offset spans over multiple aligned words (4 bytes) in memory
	 */
	public final boolean weakCompareAndSetCharPlain(Object obj, long offset, char compareValue, char setValue) {
		return weakCompareAndSetChar(obj, offset, compareValue, setValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean compareAndSetBoolean(Object obj, long offset, boolean compareValue, boolean setValue) {
		return compareAndSetByte(obj, offset, bool2byte(compareValue), bool2byte(setValue));
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final boolean compareAndExchangeBoolean(Object obj, long offset, boolean compareValue,
			boolean exchangeValue) {
		byte result = compareAndExchange8bits(obj, offset, bool2byte(compareValue), bool2byte(exchangeValue));
		return byte2bool(result);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final boolean compareAndExchangeBooleanAcquire(Object obj, long offset, boolean compareValue,
			boolean exchangeValue) {
		byte result = compareAndExchangeByteAcquire(obj, offset, bool2byte(compareValue), bool2byte(exchangeValue));
		return byte2bool(result);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final boolean compareAndExchangeBooleanRelease(Object obj, long offset, boolean compareValue,
			boolean exchangeValue) {
		byte result = compareAndExchangeByteRelease(obj, offset, bool2byte(compareValue), bool2byte(exchangeValue));
		return byte2bool(result);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetBoolean(Object obj, long offset, boolean compareValue, boolean setValue) {
		return weakCompareAndSetByte(obj, offset, bool2byte(compareValue), bool2byte(setValue));
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetBooleanAcquire(Object obj, long offset, boolean compareValue,
			boolean setValue) {
		return weakCompareAndSetByteAcquire(obj, offset, bool2byte(compareValue), bool2byte(setValue));
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetBooleanRelease(Object obj, long offset, boolean compareValue,
			boolean setValue) {
		return weakCompareAndSetByteRelease(obj, offset, bool2byte(compareValue), bool2byte(setValue));
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetBooleanPlain(Object obj, long offset, boolean compareValue,
			boolean setValue) {
		return weakCompareAndSetBytePlain(obj, offset, bool2byte(compareValue), bool2byte(setValue));
	}
	
	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final Object compareAndExchangeObjectAcquire(Object obj, long offset, Object compareValue,
			Object exchangeValue) {
		return compareAndExchangeObject(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final Object compareAndExchangeObjectRelease(Object obj, long offset, Object compareValue,
			Object exchangeValue) {
		return compareAndExchangeObject(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetObjectPlain(Object obj, long offset, Object compareValue, Object setValue) {
		return compareAndSetObject(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetObjectAcquire(Object obj, long offset, Object compareValue, Object setValue) {
		return compareAndSetObject(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetObjectRelease(Object obj, long offset, Object compareValue, Object setValue) {
		return compareAndSetObject(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetObject(Object obj, long offset, Object compareValue, Object setValue) {
		return compareAndSetObject(obj, offset, compareValue, setValue);
	}

/*[IF Java12]*/
	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final Object compareAndExchangeReferenceAcquire(Object obj, long offset, Object compareValue,
			Object exchangeValue) {
		return compareAndExchangeReference(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Atomically sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param exchangeValue value that will be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue if the exchange was successful
	 */
	public final Object compareAndExchangeReferenceRelease(Object obj, long offset, Object compareValue,
			Object exchangeValue) {
		return compareAndExchangeReference(obj, offset, compareValue, exchangeValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetReferencePlain(Object obj, long offset, Object compareValue, Object setValue) {
		return compareAndSetReference(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetReferenceAcquire(Object obj, long offset, Object compareValue, Object setValue) {
		return compareAndSetReference(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetReferenceRelease(Object obj, long offset, Object compareValue, Object setValue) {
		return compareAndSetReference(obj, offset, compareValue, setValue);
	}

	/**
	 * Sets the parameter value at offset in obj if the compare value 
	 * matches the existing value in the object.
	 * The get operation has memory semantics of get.
	 * The set operation has the memory semantics of set.
	 *
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value that is expected to be in obj at offset
	 * @param setValue value that will be set in obj at offset if compare is successful
	 * @return boolean value indicating whether the field was updated
	 */
	public final boolean weakCompareAndSetReference(Object obj, long offset, Object compareValue, Object setValue) {
		return compareAndSetReference(obj, offset, compareValue, setValue);
	}
/*[ENDIF] Java12 */

	/**
	 * Gets the value of the byte in the obj parameter referenced by offset using acquire semantics.
	 * Preceding loads will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return byte value stored in obj
	 */
	public final byte getByteAcquire(Object obj, long offset) {
		return getByteVolatile(obj, offset);
	}

	/**
	 * Gets the value of the int in the obj parameter referenced by offset using acquire semantics.
	 * Preceding loads will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return int value stored in obj
	 */
	public final int getIntAcquire(Object obj, long offset) {
		return getIntVolatile(obj, offset);
	}

	/**
	 * Gets the value of the long in the obj parameter referenced by offset using acquire semantics.
	 * Preceding loads will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return long value stored in obj
	 */
	public final long getLongAcquire(Object obj, long offset) {
		return getLongVolatile(obj, offset);
	}
	
	/**
	 * Gets the value of the float in the obj parameter referenced by offset using acquire semantics.
	 * Preceding loads will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return float value stored in obj
	 */
	public final float getFloatAcquire(Object obj, long offset) {
		return getFloatVolatile(obj, offset);
	}

	/**
	 * Gets the value of the double in the obj parameter referenced by offset using acquire semantics.
	 * Preceding loads will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return double value stored in obj
	 */
	public final double getDoubleAcquire(Object obj, long offset) {
		return getDoubleVolatile(obj, offset);
	}
	
	/**
	 * Gets the value of the short in the obj parameter referenced by offset using acquire semantics.
	 * Preceding loads will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return short value stored in obj
	 */
	public final short getShortAcquire(Object obj, long offset) {
		return getShortVolatile(obj, offset);
	}

	/**
	 * Gets the value of the char in the obj parameter referenced by offset using acquire semantics.
	 * Preceding loads will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return char value stored in obj
	 */
	public final char getCharAcquire(Object obj, long offset) {
		return getCharVolatile(obj, offset);
	}
	
	/**
	 * Gets the value of the boolean in the obj parameter referenced by offset using acquire semantics.
	 * Preceding loads will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return boolean value stored in obj
	 */
	public final boolean getBooleanAcquire(Object obj, long offset) {
		return getBooleanVolatile(obj, offset);
	}

	/**
	 * Gets the value of the Object in the obj parameter referenced by offset using acquire semantics.
	 * Preceding loads will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return Object value stored in obj
	 */
	public final Object getObjectAcquire(Object obj, long offset) {
		return getObjectVolatile(obj, offset);
	}

	/*[IF Java12]*/
	/**
	 * Gets the value of the Object in the obj parameter referenced by offset using acquire semantics.
	 * Preceding loads will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return Object value stored in obj
	 */
	public final Object getReferenceAcquire(Object obj, long offset) {
		return getReferenceVolatile(obj, offset);
	}
	/*[ENDIF] Java12 */

	/**
	 * Sets the value of the byte in the obj parameter at memory offset using acquire semantics.
	 * Preceding stores will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value byte to store in obj
	 */
	public final void putByteRelease(Object obj, long offset, byte value) {
		putByteVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the int in the obj parameter at memory offset using acquire semantics.
	 * Preceding stores will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value int to store in obj
	 */
	public final void putIntRelease(Object obj, long offset, int value) {
		putIntVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the long in the obj parameter at memory offset using acquire semantics.
	 * Preceding stores will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value long to store in obj
	 */
	public final void putLongRelease(Object obj, long offset, long value) {
		putLongVolatile(obj, offset, value);
	}
	
	/**
	 * Sets the value of the float in the obj parameter at memory offset using acquire semantics.
	 * Preceding stores will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value float to store in obj
	 */
	public final void putFloatRelease(Object obj, long offset, float value) {
		putFloatVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the double in the obj parameter at memory offset using acquire semantics.
	 * Preceding stores will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value double to store in obj
	 */
	public final void putDoubleRelease(Object obj, long offset, double value) {
		putDoubleVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the short in the obj parameter at memory offset using acquire semantics.
	 * Preceding stores will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value short to store in obj
	 */
	public final void putShortRelease(Object obj, long offset, short value) {
		putShortVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the char in the obj parameter at memory offset using acquire semantics.
	 * Preceding stores will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value char to store in obj
	 */
	public final void putCharRelease(Object obj, long offset, char value) {
		putCharVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the boolean in the obj parameter at memory offset using acquire semantics.
	 * Preceding stores will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value boolean to store in obj
	 */
	public final void putBooleanRelease(Object obj, long offset, boolean value) {
		putBooleanVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the Object in the obj parameter at memory offset using acquire semantics.
	 * Preceding stores will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value Object to store in obj
	 */
	public final void putObjectRelease(Object obj, long offset, Object value) {
		putObjectVolatile(obj, offset, value);
	}

/*[IF Java12]*/
	/**
	 * Sets the value of the Object in the obj parameter at memory offset using acquire semantics.
	 * Preceding stores will not be reordered with subsequent loads/stores.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value Object to store in obj
	 */
	public final void putReferenceRelease(Object obj, long offset, Object value) {
		putReferenceVolatile(obj, offset, value);
	}
/*[ENDIF] Java12 */
	
	/**
	 * Gets the value of the byte in the obj parameter referenced by offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return byte value stored in obj
	 */
	public final byte getByteOpaque(Object obj, long offset) {
		return getByteVolatile(obj, offset);
	}

	/**
	 * Gets the value of the int in the obj parameter referenced by offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return int value stored in obj
	 */
	public final int getIntOpaque(Object obj, long offset) {
		return getIntVolatile(obj, offset);
	}

	/**
	 * Gets the value of the long in the obj parameter referenced by offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return long value stored in obj
	 */
	public final long getLongOpaque(Object obj, long offset) {
		return getLongVolatile(obj, offset);
	}
	
	/**
	 * Gets the value of the float in the obj parameter referenced by offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return float value stored in obj
	 */
	public final float getFloatOpaque(Object obj, long offset) {
		return getFloatVolatile(obj, offset);
	}

	/**
	 * Gets the value of the double in the obj parameter referenced by offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return double value stored in obj
	 */
	public final double getDoubleOpaque(Object obj, long offset) {
		return getDoubleVolatile(obj, offset);
	}

	/**
	 * Gets the value of the short in the obj parameter referenced by offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return short value stored in obj
	 */
	public final short getShortOpaque(Object obj, long offset) {
		return getShortVolatile(obj, offset);
	}

	/**
	 * Gets the value of the char in the obj parameter referenced by offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return char value stored in obj
	 */
	public final char getCharOpaque(Object obj, long offset) {
		return getCharVolatile(obj, offset);
	}

	/**
	 * Gets the value of the boolean in the obj parameter referenced by offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return boolean value stored in obj
	 */
	public final boolean getBooleanOpaque(Object obj, long offset) {
		return getBooleanVolatile(obj, offset);
	}
	
	/**
	 * Gets the value of the Object in the obj parameter referenced by offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return Object value stored in obj
	 */
	public final Object getObjectOpaque(Object obj, long offset) {
		return getObjectVolatile(obj, offset);
	}

/*[IF Java12]*/
	/**
	 * Gets the value of the Object in the obj parameter referenced by offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return Object value stored in obj
	 */
	public final Object getReferenceOpaque(Object obj, long offset) {
		return getReferenceVolatile(obj, offset);
	}
/*[ENDIF] Java12 */

	/**
	 * Sets the value of the byte in the obj parameter at memory offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value byte to store in obj
	 */
	public final void putByteOpaque(Object obj, long offset, byte value) {
		putByteVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the int in the obj parameter at memory offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value int to store in obj
	 */
	public final void putIntOpaque(Object obj, long offset, int value) {
		putIntVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the long in the obj parameter at memory offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value long to store in obj
	 */
	public final void putLongOpaque(Object obj, long offset, long value) {
		putLongVolatile(obj, offset, value);
	}
	
	/**
	 * Sets the value of the float in the obj parameter at memory offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value float to store in obj
	 */
	public final void putFloatOpaque(Object obj, long offset, float value) {
		putFloatVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the double in the obj parameter at memory offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value double to store in obj
	 */
	public final void putDoubleOpaque(Object obj, long offset, double value) {
		putDoubleVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the short in the obj parameter at memory offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value short to store in obj
	 */
	public final void putShortOpaque(Object obj, long offset, short value) {
		putShortVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the char in the obj parameter at memory offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value char to store in obj
	 */
	public final void putCharOpaque(Object obj, long offset, char value) {
		putCharVolatile(obj, offset, value);
	}

	/**
	 * Sets the value of the boolean in the obj parameter at memory offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value boolean to store in obj
	 */
	public final void putBooleanOpaque(Object obj, long offset, boolean value) {
		putBooleanVolatile(obj, offset, value);
	}
	
	/**
	 * Sets the value of the Object in the obj parameter at memory offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value Object to store in obj
	 */
	public final void putObjectOpaque(Object obj, long offset, Object value) {
		putObjectVolatile(obj, offset, value);
	}

/*[IF Java12]*/
	/**
	 * Sets the value of the Object in the obj parameter at memory offset.
	 * The operation is in program order, but does enforce ordering with respect to other threads.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value Object to store in obj
	 */
	public final void putReferenceOpaque(Object obj, long offset, Object value) {
		putReferenceVolatile(obj, offset, value);
	}
/*[ENDIF] Java12 */

	/**
	 * Get the load average in the system.
	 * 
	 * NOTE: native implementation is a stub.
	 * 
	 * @param loadavg array of elements
	 * @param numberOfElements number of samples
	 * @return load average
	 * 
	 * @throws ArrayIndexOutOfBoundsException if numberOfElements is negative, more than three, or 
	 * the length of the loadavg array is greater than the numberOfElements
	 */
	public int getLoadAverage(double[] loadavg, int numberOfElements) {
		if ((numberOfElements < 0) || (numberOfElements > 3) || (loadavg.length > numberOfElements)) {
			throw new ArrayIndexOutOfBoundsException();
		}

		return getLoadAverage0(loadavg, numberOfElements);
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndAddByte(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByteVolatile(obj, offset);
			if (weakCompareAndSetByte(obj, offset, byteAtOffset, (byte) (byteAtOffset + value))) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndAddByteRelease(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByte(obj, offset);
			if (weakCompareAndSetByteRelease(obj, offset, byteAtOffset, (byte) (byteAtOffset + value))) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndAddByteAcquire(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByteAcquire(obj, offset);
			if (weakCompareAndSetByteAcquire(obj, offset, byteAtOffset, (byte) (byteAtOffset + value))) {
				return byteAtOffset;
			}
		}
	}
	
	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final int getAndAddInt(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getIntVolatile(obj, offset);
			if (weakCompareAndSetInt(obj, offset, intAtOffset, intAtOffset + value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final int getAndAddIntRelease(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getInt(obj, offset);
			if (weakCompareAndSetIntRelease(obj, offset, intAtOffset, intAtOffset + value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final int getAndAddIntAcquire(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getIntAcquire(obj, offset);
			if (weakCompareAndSetIntAcquire(obj, offset, intAtOffset, intAtOffset + value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final long getAndAddLong(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLongVolatile(obj, offset);
			if (weakCompareAndSetLong(obj, offset, longAtOffset, longAtOffset + value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final long getAndAddLongRelease(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLong(obj, offset);
			if (weakCompareAndSetLongRelease(obj, offset, longAtOffset, longAtOffset + value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final long getAndAddLongAcquire(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLongAcquire(obj, offset);
			if (weakCompareAndSetLongAcquire(obj, offset, longAtOffset, longAtOffset + value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final float getAndAddFloat(Object obj, long offset, float value) {
		for (;;) {
			int intAtOffset = getIntVolatile(obj, offset);
			float floatAtOffset = Float.intBitsToFloat(intAtOffset);
			if (weakCompareAndSetInt(obj, offset, intAtOffset,
					Float.floatToRawIntBits(floatAtOffset + value))) {
				return floatAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final float getAndAddFloatRelease(Object obj, long offset, float value) {
		for (;;) {
			int intAtOffset = getInt(obj, offset);
			float floatAtOffset = Float.intBitsToFloat(intAtOffset);
			if (weakCompareAndSetIntRelease(obj, offset, intAtOffset,
					Float.floatToRawIntBits(floatAtOffset + value))) {
				return floatAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final float getAndAddFloatAcquire(Object obj, long offset, float value) {
		for (;;) {
			int intAtOffset = getIntAcquire(obj, offset);
			float floatAtOffset = Float.intBitsToFloat(intAtOffset);
			if (weakCompareAndSetIntAcquire(obj, offset, intAtOffset,
					Float.floatToRawIntBits(floatAtOffset + value))) {
				return floatAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final double getAndAddDouble(Object obj, long offset, double value) {
		for (;;) {
			long longAtOffset = getLongVolatile(obj, offset);
			double doubleAtOffset = Double.longBitsToDouble(longAtOffset);
			if (weakCompareAndSetLong(obj, offset, longAtOffset,
					Double.doubleToRawLongBits(doubleAtOffset + value))) {
				return doubleAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final double getAndAddDoubleRelease(Object obj, long offset, double value) {
		for (;;) {
			long longAtOffset = getLong(obj, offset);
			double doubleAtOffset = Double.longBitsToDouble(longAtOffset);
			if (weakCompareAndSetLongRelease(obj, offset, longAtOffset,
					Double.doubleToRawLongBits(doubleAtOffset + value))) {
				return doubleAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final double getAndAddDoubleAcquire(Object obj, long offset, double value) {
		for (;;) {
			long longAtOffset = getLongAcquire(obj, offset);
			double doubleAtOffset = Double.longBitsToDouble(longAtOffset);
			if (weakCompareAndSetLongAcquire(obj, offset, longAtOffset,
					Double.doubleToRawLongBits(doubleAtOffset + value))) {
				return doubleAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final short getAndAddShort(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShortVolatile(obj, offset);
			if (weakCompareAndSetShort(obj, offset, shortAtOffset, (short) (shortAtOffset + value))) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final short getAndAddShortRelease(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShort(obj, offset);
			if (weakCompareAndSetShortRelease(obj, offset, shortAtOffset, (short) (shortAtOffset + value))) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final short getAndAddShortAcquire(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShortAcquire(obj, offset);
			if (weakCompareAndSetShortAcquire(obj, offset, shortAtOffset, (short) (shortAtOffset + value))) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final char getAndAddChar(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getCharVolatile(obj, offset);
			if (weakCompareAndSetChar(obj, offset, charAtOffset, (char) (charAtOffset + value))) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final char getAndAddCharRelease(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getChar(obj, offset);
			if (weakCompareAndSetCharRelease(obj, offset, charAtOffset, (char) (charAtOffset + value))) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically adds to the current value of the field at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to add the value
	 * @param offset location to add value in obj
	 * @param value to add to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final char getAndAddCharAcquire(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getCharAcquire(obj, offset);
			if (weakCompareAndSetCharAcquire(obj, offset, charAtOffset, (char) (charAtOffset + value))) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */	
	public final byte getAndSetByte(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByteVolatile(obj, offset);
			if (compareAndSetByte(obj, offset, byteAtOffset, value)) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndSetByteRelease(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByte(obj, offset);
			if (weakCompareAndSetByteRelease(obj, offset, byteAtOffset, value)) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndSetByteAcquire(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByteAcquire(obj, offset);
			if (weakCompareAndSetByteAcquire(obj, offset, byteAtOffset, value)) {
				return byteAtOffset;
			}
		}
	}
	
	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final int getAndSetInt(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getIntVolatile(obj, offset);
			if (compareAndSetInt(obj, offset, intAtOffset, value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final int getAndSetIntRelease(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getInt(obj, offset);
			if (weakCompareAndSetIntRelease(obj, offset, intAtOffset, value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final int getAndSetIntAcquire(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getIntAcquire(obj, offset);
			if (weakCompareAndSetIntAcquire(obj, offset, intAtOffset, value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final long getAndSetLong(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLongVolatile(obj, offset);
			if (compareAndSetLong(obj, offset, longAtOffset, value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final long getAndSetLongRelease(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLong(obj, offset);
			if (weakCompareAndSetLongRelease(obj, offset, longAtOffset, value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final long getAndSetLongAcquire(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLongAcquire(obj, offset);
			if (weakCompareAndSetLongAcquire(obj, offset, longAtOffset, value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final float getAndSetFloat(Object obj, long offset, float value) {
		int result = getAndSetInt(obj, offset, Float.floatToRawIntBits(value));
		return Float.intBitsToFloat(result);
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final float getAndSetFloatRelease(Object obj, long offset, float value) {
		int result = getAndSetIntRelease(obj, offset, Float.floatToRawIntBits(value));
		return Float.intBitsToFloat(result);
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final float getAndSetFloatAcquire(Object obj, long offset, float value) {
		int result = getAndSetIntAcquire(obj, offset, Float.floatToRawIntBits(value));
		return Float.intBitsToFloat(result);
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final double getAndSetDouble(Object obj, long offset, double value) {
		long result = getAndSetLong(obj, offset, Double.doubleToRawLongBits(value));
		return Double.longBitsToDouble(result);
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final double getAndSetDoubleRelease(Object obj, long offset, double value) {
		long result = getAndSetLongRelease(obj, offset, Double.doubleToRawLongBits(value));
		return Double.longBitsToDouble(result);
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final double getAndSetDoubleAcquire(Object obj, long offset, double value) {
		long result = getAndSetLongAcquire(obj, offset, Double.doubleToRawLongBits(value));
		return Double.longBitsToDouble(result);
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final short getAndSetShort(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShortVolatile(obj, offset);
			if (compareAndSetShort(obj, offset, shortAtOffset, value)) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final short getAndSetShortRelease(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShort(obj, offset);
			if (weakCompareAndSetShortRelease(obj, offset, shortAtOffset, value)) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final short getAndSetShortAcquire(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShortAcquire(obj, offset);
			if (weakCompareAndSetShortAcquire(obj, offset, shortAtOffset, value)) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final char getAndSetChar(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getCharVolatile(obj, offset);
			if (compareAndSetChar(obj, offset, charAtOffset, value)) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final char getAndSetCharRelease(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getChar(obj, offset);
			if (weakCompareAndSetCharRelease(obj, offset, charAtOffset, value)) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final char getAndSetCharAcquire(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getCharAcquire(obj, offset);
			if (weakCompareAndSetCharAcquire(obj, offset, charAtOffset, value)) {
				return charAtOffset;
			}
		}
	}
	
	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final boolean getAndSetBoolean(Object obj, long offset, boolean value) {
		byte result = getAndSetByte(obj, offset, bool2byte(value));
		return byte2bool(result);
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final boolean getAndSetBooleanRelease(Object obj, long offset, boolean value) {
		byte result = getAndSetByteRelease(obj, offset, bool2byte(value));
		return byte2bool(result);
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final boolean getAndSetBooleanAcquire(Object obj, long offset, boolean value) {
		byte result = getAndSetByteAcquire(obj, offset, bool2byte(value));
		return byte2bool(result);
	}
	
	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final Object getAndSetObject(Object obj, long offset, Object value) {
		for (;;) {
			Object objectAtOffset = getObjectVolatile(obj, offset);
			if (compareAndSetObject(obj, offset, objectAtOffset, value)) {
				return objectAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final Object getAndSetObjectRelease(Object obj, long offset, Object value) {
		for (;;) {
			Object objectAtOffset = getObject(obj, offset);
			if (weakCompareAndSetObjectRelease(obj, offset, objectAtOffset, value)) {
				return objectAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final Object getAndSetObjectAcquire(Object obj, long offset, Object value) {
		for (;;) {
			Object objectAtOffset = getObjectAcquire(obj, offset);
			if (weakCompareAndSetObjectAcquire(obj, offset, objectAtOffset, value)) {
				return objectAtOffset;
			}
		}
	}

/*[IF Java12]*/
	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final Object getAndSetReference(Object obj, long offset, Object value) {
		for (;;) {
			Object objectAtOffset = getReferenceVolatile(obj, offset);
			if (compareAndSetReference(obj, offset, objectAtOffset, value)) {
				return objectAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final Object getAndSetReferenceRelease(Object obj, long offset, Object value) {
		for (;;) {
			Object objectAtOffset = getReference(obj, offset);
			if (weakCompareAndSetReferenceRelease(obj, offset, objectAtOffset, value)) {
				return objectAtOffset;
			}
		}
	}

	/**
	 * Atomically sets value at offset in obj
	 * and returns the value of the field prior to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to set the value
	 * @param offset location to set value in obj
	 * @param value to set in obj memory
	 * @return value of field in obj at offset before update
	 */
	public final Object getAndSetReferenceAcquire(Object obj, long offset, Object value) {
		for (;;) {
			Object objectAtOffset = getReferenceAcquire(obj, offset);
			if (weakCompareAndSetReferenceAcquire(obj, offset, objectAtOffset, value)) {
				return objectAtOffset;
			}
		}
	}
/*[ENDIF] Java12 */

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndBitwiseOrByte(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByteVolatile(obj, offset);
			if (weakCompareAndSetByte(obj, offset, byteAtOffset, (byte) (byteAtOffset | value))) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndBitwiseOrByteRelease(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByte(obj, offset);
			if (weakCompareAndSetByteRelease(obj, offset, byteAtOffset, (byte) (byteAtOffset | value))) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndBitwiseOrByteAcquire(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByte(obj, offset);
			if (weakCompareAndSetByteAcquire(obj, offset, byteAtOffset, (byte) (byteAtOffset | value))) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndBitwiseAndByte(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByteVolatile(obj, offset);
			if (weakCompareAndSetByte(obj, offset, byteAtOffset, (byte) (byteAtOffset & value))) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndBitwiseAndByteRelease(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByte(obj, offset);
			if (weakCompareAndSetByteRelease(obj, offset, byteAtOffset, (byte) (byteAtOffset & value))) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndBitwiseAndByteAcquire(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByte(obj, offset);
			if (weakCompareAndSetByteAcquire(obj, offset, byteAtOffset, (byte) (byteAtOffset & value))) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndBitwiseXorByte(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByteVolatile(obj, offset);
			if (weakCompareAndSetByte(obj, offset, byteAtOffset, (byte) (byteAtOffset ^ value))) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndBitwiseXorByteRelease(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByte(obj, offset);
			if (weakCompareAndSetByteRelease(obj, offset, byteAtOffset, (byte) (byteAtOffset ^ value))) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final byte getAndBitwiseXorByteAcquire(Object obj, long offset, byte value) {
		for (;;) {
			byte byteAtOffset = getByte(obj, offset);
			if (weakCompareAndSetByteAcquire(obj, offset, byteAtOffset, (byte) (byteAtOffset ^ value))) {
				return byteAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final int getAndBitwiseOrInt(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getIntVolatile(obj, offset);
			if (weakCompareAndSetInt(obj, offset, intAtOffset, intAtOffset | value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final int getAndBitwiseOrIntRelease(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getInt(obj, offset);
			if (weakCompareAndSetIntRelease(obj, offset, intAtOffset, intAtOffset | value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final int getAndBitwiseOrIntAcquire(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getInt(obj, offset);
			if (weakCompareAndSetIntAcquire(obj, offset, intAtOffset, intAtOffset | value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final int getAndBitwiseAndInt(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getIntVolatile(obj, offset);
			if (weakCompareAndSetInt(obj, offset, intAtOffset, intAtOffset & value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final int getAndBitwiseAndIntRelease(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getInt(obj, offset);
			if (weakCompareAndSetIntRelease(obj, offset, intAtOffset, intAtOffset & value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final int getAndBitwiseAndIntAcquire(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getInt(obj, offset);
			if (weakCompareAndSetIntAcquire(obj, offset, intAtOffset, intAtOffset & value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final int getAndBitwiseXorInt(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getIntVolatile(obj, offset);
			if (weakCompareAndSetInt(obj, offset, intAtOffset, intAtOffset ^ value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final int getAndBitwiseXorIntRelease(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getInt(obj, offset);
			if (weakCompareAndSetIntRelease(obj, offset, intAtOffset, intAtOffset ^ value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final int getAndBitwiseXorIntAcquire(Object obj, long offset, int value) {
		for (;;) {
			int intAtOffset = getInt(obj, offset);
			if (weakCompareAndSetIntAcquire(obj, offset, intAtOffset, intAtOffset ^ value)) {
				return intAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final long getAndBitwiseOrLong(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLongVolatile(obj, offset);
			if (weakCompareAndSetLong(obj, offset, longAtOffset, longAtOffset | value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final long getAndBitwiseOrLongRelease(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLong(obj, offset);
			if (weakCompareAndSetLongRelease(obj, offset, longAtOffset, longAtOffset | value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final long getAndBitwiseOrLongAcquire(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLong(obj, offset);
			if (weakCompareAndSetLongAcquire(obj, offset, longAtOffset, longAtOffset | value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final long getAndBitwiseAndLong(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLongVolatile(obj, offset);
			if (weakCompareAndSetLong(obj, offset, longAtOffset, longAtOffset & value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final long getAndBitwiseAndLongRelease(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLong(obj, offset);
			if (weakCompareAndSetLongRelease(obj, offset, longAtOffset, longAtOffset & value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final long getAndBitwiseAndLongAcquire(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLong(obj, offset);
			if (weakCompareAndSetLongAcquire(obj, offset, longAtOffset, longAtOffset & value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final long getAndBitwiseXorLong(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLongVolatile(obj, offset);
			if (weakCompareAndSetLong(obj, offset, longAtOffset, longAtOffset ^ value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final long getAndBitwiseXorLongRelease(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLong(obj, offset);
			if (weakCompareAndSetLongRelease(obj, offset, longAtOffset, longAtOffset ^ value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final long getAndBitwiseXorLongAcquire(Object obj, long offset, long value) {
		for (;;) {
			long longAtOffset = getLong(obj, offset);
			if (weakCompareAndSetLongAcquire(obj, offset, longAtOffset, longAtOffset ^ value)) {
				return longAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final short getAndBitwiseOrShort(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShortVolatile(obj, offset);
			if (weakCompareAndSetShort(obj, offset, shortAtOffset, (short) (shortAtOffset | value))) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final short getAndBitwiseOrShortRelease(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShort(obj, offset);
			if (weakCompareAndSetShortRelease(obj, offset, shortAtOffset, (short) (shortAtOffset | value))) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final short getAndBitwiseOrShortAcquire(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShort(obj, offset);
			if (weakCompareAndSetShortAcquire(obj, offset, shortAtOffset, (short) (shortAtOffset | value))) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final short getAndBitwiseAndShort(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShortVolatile(obj, offset);
			if (weakCompareAndSetShort(obj, offset, shortAtOffset, (short) (shortAtOffset & value))) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final short getAndBitwiseAndShortRelease(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShort(obj, offset);
			if (weakCompareAndSetShortRelease(obj, offset, shortAtOffset, (short) (shortAtOffset & value))) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final short getAndBitwiseAndShortAcquire(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShort(obj, offset);
			if (weakCompareAndSetShortAcquire(obj, offset, shortAtOffset, (short) (shortAtOffset & value))) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final short getAndBitwiseXorShort(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShortVolatile(obj, offset);
			if (weakCompareAndSetShort(obj, offset, shortAtOffset, (short) (shortAtOffset ^ value))) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final short getAndBitwiseXorShortRelease(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShort(obj, offset);
			if (weakCompareAndSetShortRelease(obj, offset, shortAtOffset, (short) (shortAtOffset ^ value))) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final short getAndBitwiseXorShortAcquire(Object obj, long offset, short value) {
		for (;;) {
			short shortAtOffset = getShort(obj, offset);
			if (weakCompareAndSetShortAcquire(obj, offset, shortAtOffset, (short) (shortAtOffset ^ value))) {
				return shortAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final char getAndBitwiseOrChar(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getCharVolatile(obj, offset);
			if (weakCompareAndSetChar(obj, offset, charAtOffset, (char) (charAtOffset | value))) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final char getAndBitwiseOrCharRelease(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getChar(obj, offset);
			if (weakCompareAndSetCharRelease(obj, offset, charAtOffset, (char) (charAtOffset | value))) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final char getAndBitwiseOrCharAcquire(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getChar(obj, offset);
			if (weakCompareAndSetCharAcquire(obj, offset, charAtOffset, (char) (charAtOffset | value))) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final char getAndBitwiseAndChar(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getCharVolatile(obj, offset);
			if (weakCompareAndSetChar(obj, offset, charAtOffset, (char) (charAtOffset & value))) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final char getAndBitwiseAndCharRelease(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getChar(obj, offset);
			if (weakCompareAndSetCharRelease(obj, offset, charAtOffset, (char) (charAtOffset & value))) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final char getAndBitwiseAndCharAcquire(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getChar(obj, offset);
			if (weakCompareAndSetCharAcquire(obj, offset, charAtOffset, (char) (charAtOffset & value))) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final char getAndBitwiseXorChar(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getCharVolatile(obj, offset);
			if (weakCompareAndSetChar(obj, offset, charAtOffset, (char) (charAtOffset ^ value))) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final char getAndBitwiseXorCharRelease(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getChar(obj, offset);
			if (weakCompareAndSetCharRelease(obj, offset, charAtOffset, (char) (charAtOffset ^ value))) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final char getAndBitwiseXorCharAcquire(Object obj, long offset, char value) {
		for (;;) {
			char charAtOffset = getChar(obj, offset);
			if (weakCompareAndSetCharAcquire(obj, offset, charAtOffset, (char) (charAtOffset ^ value))) {
				return charAtOffset;
			}
		}
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final boolean getAndBitwiseOrBoolean(Object obj, long offset, boolean value) {
		byte result = getAndBitwiseOrByte(obj, offset, bool2byte(value));
		return byte2bool(result);
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final boolean getAndBitwiseOrBooleanRelease(Object obj, long offset, boolean value) {
		byte result = getAndBitwiseOrByteRelease(obj, offset, bool2byte(value));
		return byte2bool(result);
	}

	/**
	 * Atomically OR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to OR the value
	 * @param offset location to OR value in obj
	 * @param value to OR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final boolean getAndBitwiseOrBooleanAcquire(Object obj, long offset, boolean value) {
		byte result = getAndBitwiseOrByteAcquire(obj, offset, bool2byte(value));
		return byte2bool(result);
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final boolean getAndBitwiseAndBoolean(Object obj, long offset, boolean value) {
		byte result = getAndBitwiseAndByte(obj, offset, bool2byte(value));
		return byte2bool(result);
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final boolean getAndBitwiseAndBooleanRelease(Object obj, long offset, boolean value) {
		byte result = getAndBitwiseAndByteRelease(obj, offset, bool2byte(value));
		return byte2bool(result);
	}

	/**
	 * Atomically AND's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to AND the value
	 * @param offset location to AND value in obj
	 * @param value to AND to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final boolean getAndBitwiseAndBooleanAcquire(Object obj, long offset, boolean value) {
		byte result = getAndBitwiseAndByteAcquire(obj, offset, bool2byte(value));
		return byte2bool(result);
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getVolatile.
	 * The set operation has the memory semantics of setVolatile.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final boolean getAndBitwiseXorBoolean(Object obj, long offset, boolean value) {
		byte result = getAndBitwiseXorByte(obj, offset, bool2byte(value));
		return byte2bool(result);
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of get.
	 * The set operation has the memory semantics of setRelease.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final boolean getAndBitwiseXorBooleanRelease(Object obj, long offset, boolean value) {
		byte result = getAndBitwiseXorByteRelease(obj, offset, bool2byte(value));
		return byte2bool(result);
	}

	/**
	 * Atomically XOR's the given value to the current value of the 
	 * field at offset in obj and returns the value of the field prior 
	 * to the update.
	 * The get operation has the memory semantics of getAcquire.
	 * The set operation has the memory semantics of set.
	 * 
	 * @param obj object into which to XOR the value
	 * @param offset location to XOR value in obj
	 * @param value to XOR to existing value in field
	 * @return value of field in obj at offset before update
	 */
	public final boolean getAndBitwiseXorBooleanAcquire(Object obj, long offset, boolean value) {
		byte result = getAndBitwiseXorByteAcquire(obj, offset, bool2byte(value));
		return byte2bool(result);
	}

	/**
	 * Inserts an acquire memory fence, ensuring that no loads before this fence
	 * are reordered with any loads/stores after the fence.
	 */
	public final void loadLoadFence() {
		loadFence();
	}

	/**
	 * Inserts a release memory fence, ensuring that no stores before this fence
	 *  are reordered with any loads/stores after the fence.
	 */
	public final void storeStoreFence() {
		storeFence();
	}

	/**
	 * @return true if machine is big endian, false otherwise
	 */
	public final boolean isBigEndian() {
		return IS_BIG_ENDIAN;
	}

	/**
	 * @return true if addresses are aligned in memory, false otherwise
	 */
	public final boolean unalignedAccess() {
		return UNALIGNED_ACCESS;
	}

	/**
	 * Gets the value of the int in the obj parameter referenced by offset
	 *  that may be unaligned in memory.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return int value stored in obj
	 */
	public final int getIntUnaligned(Object obj, long offset) {
		int result = 0;

		if (isOffsetIntAligned(offset)) {
			result = getInt(obj, offset);
		} else if (isOffsetShortAligned(offset)) {
			short first = getShort(obj, offset);
			short second = getShort(obj, 2L + offset);
			result = makeInt(first, second);
		} else {
			byte first = getByte(obj, offset);
			byte second = getByte(obj, 1L + offset);
			byte third = getByte(obj, 2L + offset);
			byte fourth = getByte(obj, 3L + offset);
			result = makeInt(first, second, third, fourth);
		}

		return result;
	}

	/**
	 * Gets the value of the int in the obj parameter referenced by offset
	 *  that may be unaligned in memory. Value may be reversed according to 
	 *  the endianness parameter.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @param bigEndian in what endianness value should be returned
	 * @return int value stored in obj
	 */
	public final int getIntUnaligned(Object obj, long offset, boolean bigEndian) {
		int result = getIntUnaligned(obj, offset);
		return convEndian(bigEndian, result);
	}
	
	/**
	 * Gets the value of the long in the obj parameter referenced by offset
	 * that may be unaligned in memory.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return long value stored in obj
	 */
	public final long getLongUnaligned(Object obj, long offset) {
		long result = 0;

		if (isOffsetLongAligned(offset)) {
			result = getLong(obj, offset);
		} else if (isOffsetIntAligned(offset)) {
			int first = getInt(obj, offset);
			int second = getInt(obj, 4L + offset);
			result = makeLong(first, second);
		} else if (isOffsetShortAligned(offset)) {
			short first = getShort(obj, offset);
			short second = getShort(obj, 2L + offset);
			short third = getShort(obj, 4L + offset);
			short fourth = getShort(obj, 6L + offset);
			result = makeLong(first, second, third, fourth);
		} else {
			byte first = getByte(obj, offset);
			byte second = getByte(obj, 1L + offset);
			byte third = getByte(obj, 2L + offset);
			byte fourth = getByte(obj, 3L + offset);
			byte fifth = getByte(obj, 4L + offset);
			byte sixth = getByte(obj, 5L + offset);
			byte seventh = getByte(obj, 6L + offset);
			byte eighth = getByte(obj, 7L + offset);
			result = makeLong(first, second, third, fourth, fifth, sixth, seventh, eighth);
		}

		return result;
	}

	/**
	 * Gets the value of the long in the obj parameter referenced by offset
	 * that may be unaligned in memory. Value may be reversed according to 
	 *  the endianness parameter.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @param bigEndian in what endianness value should be returned
	 * @return long value stored in obj
	 */
	public final long getLongUnaligned(Object obj, long offset, boolean bigEndian) {
		long result = getLongUnaligned(obj, offset);
		return convEndian(bigEndian, result);
	}

	/**
	 * Gets the value of the short in the obj parameter referenced by offset
	 *  that may be unaligned in memory.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return short value stored in obj
	 */
	public final short getShortUnaligned(Object obj, long offset) {
		short result = 0;

		if (isOffsetShortAligned(offset)) {
			result = getShort(obj, offset);
		} else {
			byte first = getByte(obj, offset);
			byte second = getByte(obj, 1L + offset);
			result = makeShort(first, second);
		}

		return result;
	}

	/**
	 * Gets the value of the short in the obj parameter referenced by offset
	 *  that may be unaligned in memory. Value may be reversed according to 
	 *  the endianness parameter.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @param bigEndian in what endianness value should be returned
	 * @return short value stored in obj
	 */
	public final short getShortUnaligned(Object obj, long offset, boolean bigEndian) {
		short result = getShortUnaligned(obj, offset);
		return convEndian(bigEndian, result);
	}

	/**
	 * Gets the value of the char in the obj parameter referenced by offset
	 *  that may be unaligned in memory.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object from which to retrieve the value
	 * @param offset position of the value in obj
	 * @return char value stored in obj
	 */
	public final char getCharUnaligned(Object obj, long offset) {
		char result = 0;

		if (isOffsetCharAligned(offset)) {
			result = getChar(obj, offset);
		} else {
			byte first = getByte(obj, offset);
			byte second = getByte(obj, 1L + offset);
			result = s2c(makeShort(first, second));
		}

		return result;
	}

	/**
	 * Gets the value of the char in the obj parameter referenced by offset
	 *  that may be unaligned in memory. Value may be reversed according to 
	 *  the endianness parameter.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to retrieve the value
	 * @param offset position of the value in obj
	 * @param bigEndian in what endianness value should be returned
	 * @return char value stored in obj
	 */
	public final char getCharUnaligned(Object obj, long offset, boolean bigEndian) {
		char result = getCharUnaligned(obj, offset);
		return convEndian(bigEndian, result);
	}

	/**
	 * Sets the value of the int in the obj parameter at memory offset
	 *  that may be unaligned in memory.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value int to store in obj
	 */
	public final void putIntUnaligned(Object obj, long offset, int value) {
		if (isOffsetIntAligned(offset)) {
			putInt(obj, offset, value);
		} else if (isOffsetShortAligned(offset)) {
			putIntParts(obj, offset, (short) (value >> 0), (short) (value >>> 16));
		} else {
			putIntParts(obj, offset, (byte) (value >>> 0), (byte) (value >>> 8), (byte) (value >>> 16),
					(byte) (value >>> 24));
		}
	}

	/**
	 * Sets the value of the int in the obj parameter at memory offset
	 *  that may be unaligned in memory. Value may be reversed according to
	 *  the endianness parameter.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value int to store in obj
	 * @param bigEndian in what endianness value should be set
	 */
	public final void putIntUnaligned(Object obj, long offset, int value, boolean bigEndian) {
		int endianValue = convEndian(bigEndian, value);
		putIntUnaligned(obj, offset, endianValue);
	}
	
	/**
	 * Sets the value of the long in the obj parameter at memory offset
	 *  that may be unaligned in memory.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value long to store in obj
	 */
	public final void putLongUnaligned(Object obj, long offset, long value) {
		if (isOffsetLongAligned(offset)) {
			putLong(obj, offset, value);
		} else if (isOffsetIntAligned(offset)) {
			putLongParts(obj, offset, (int) (value >> 0), (int) (value >>> 32));
		} else if (isOffsetShortAligned(offset)) {
			putLongParts(obj, offset, (short) (value >>> 0), (short) (value >>> 16), (short) (value >>> 32),
					(short) (value >>> 48));
		} else {
			putLongParts(obj, offset, (byte) (value >>> 0), (byte) (value >>> 8), (byte) (value >>> 16),
					(byte) (value >>> 24), (byte) (value >>> 32), (byte) (value >>> 40), (byte) (value >>> 48),
					(byte) (value >>> 56));
		}
	}

	/**
	 * Sets the value of the long in the obj parameter at memory offset
	 *  that may be unaligned in memory. Value may be reversed according to
	 *  the endianness parameter.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value long to store in obj
	 * @param bigEndian in what endianness value should be set
	 */
	public final void putLongUnaligned(Object obj, long offset, long value, boolean bigEndian) {
		long endianValue = convEndian(bigEndian, value);
		putLongUnaligned(obj, offset, endianValue);
	}
	
	/**
	 * Sets the value of the short in the obj parameter at memory offset
	 *  that may be unaligned in memory.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value short to store in obj
	 */
	public final void putShortUnaligned(Object obj, long offset, short value) {
		if (isOffsetShortAligned(offset)) {
			putShort(obj, offset, value);
		} else {
			putShortParts(obj, offset, (byte) (value >>> 0), (byte) (value >>> 8));
		}
	}

	/**
	 * Sets the value of the short in the obj parameter at memory offset
	 *  that may be unaligned in memory. Value may be reversed according to
	 *  the endianness parameter.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value short to store in obj
	 * @param bigEndian in what endianness value should be set
	 */
	public final void putShortUnaligned(Object obj, long offset, short value, boolean bigEndian) {
		short endianValue = convEndian(bigEndian, value);
		putShortUnaligned(obj, offset, endianValue);
	}

	/**
	 * Sets the value of the char in the obj parameter at memory offset
	 *  that may be unaligned in memory.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value char to store in obj
	 */
	public final void putCharUnaligned(Object obj, long offset, char value) {
		putShortUnaligned(obj, offset, c2s(value));
	}

	/**
	 * Sets the value of the char in the obj parameter at memory offset
	 *  that may be unaligned in memory. Value may be reversed according to
	 *  the endianness parameter.
	 * This is a non-volatile operation.
	 * 
	 * @param obj object into which to store the value
	 * @param offset position of the value in obj
	 * @param value char to store in obj
	 * @param bigEndian in what endianness value should be set
	 */
	public final void putCharUnaligned(Object obj, long offset, char value, boolean bigEndian) {
		char endianValue = convEndian(bigEndian, value);
		putCharUnaligned(obj, offset, endianValue);
	}

/*[IF Java12]*/
	/**
	 * If incoming ByteBuffer is an instance of sun.nio.ch.DirectBuffer,
	 * and it is direct, and not a slice or duplicate,
	 * if it has a cleaner, it is invoked,
	 * otherwise an IllegalArgumentException is thrown
	 * 
	 * @param bbo a ByteBuffer object
	 * @throws IllegalArgumentException as per description above
	 */
	public void invokeCleaner(ByteBuffer bbo) {
		if (bbo instanceof DirectBuffer) {
			if (bbo.isDirect()) {
				DirectBuffer db = (DirectBuffer)bbo;
				if (db.attachment() == null) {
					Cleaner cleaner = db.cleaner();
					if (cleaner != null) {
						cleaner.clean();
					}
				} else {
					/*[MSG "K0706", "This DirectBuffer object is a slice or duplicate"]*/
					throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0706")); //$NON-NLS-1$
				}
			} else {
				/*[MSG "K0705", "This DirectBuffer object is not direct"]*/
				throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0705")); //$NON-NLS-1$
			}
		} else {
			/*[MSG "K0704", "A sun.nio.ch.DirectBuffer object is expected"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0704")); //$NON-NLS-1$
		}
	}
/*[ENDIF] Java12 */

	/* 
	 * Private methods 
	 */
	/* @return true if offset accesses an int that is aligned in memory, else false */
	private boolean isOffsetIntAligned(long offset) {
		/* Masks bits that must be 0 to be int aligned. */
		final long OFFSET_ALIGNED_INT = 0b11L;
		return (0L == (OFFSET_ALIGNED_INT & offset));
	}
	
	/* @return true if offset accesses a long that is aligned in memory, else false */
	private boolean isOffsetLongAligned(long offset) {
		/* Masks bits that must be 0 to be long aligned. */
		final long OFFSET_ALIGNED_LONG = 0b111L;
		return (0L == (OFFSET_ALIGNED_LONG & offset));
	}

	/* @return true if offset accesses a short that is aligned in memory, else false */
	private boolean isOffsetShortAligned(long offset) {
		/* Masks bits that must be 0 to be short aligned. */
		final long OFFSET_ALIGNED_SHORT = 0b1L;
		return (0L == (OFFSET_ALIGNED_SHORT & offset));
	}

	/* @return true if offset accesses a char that is aligned in memory, else false */
	private boolean isOffsetCharAligned(long offset) {
		return isOffsetShortAligned(offset);
	}

	/* @return new instance of IllegalArgumentException. */
	private RuntimeException invalidInput() {
		return new IllegalArgumentException();
	}

	/* 
	 * Generic compareAndExchange for 8 bit primitives (byte and boolean).
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value extended to the size of an int that is 
	 * expected to be in obj at offset
	 * @param exchangeValue value extended to the size of an int that will 
	 * be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue 
	 * if the exchange was successful
	 */
	private final byte compareAndExchange8bits(Object obj, long offset, int compareValue, int exchangeValue) {
		byte result;

		if ((obj == null) || obj.getClass().isArray()) {
			/* mask extended 8 bit type to remove any sign extension for negative values */
			compareValue = BYTE_MASK_INT & compareValue;
			exchangeValue = BYTE_MASK_INT & exchangeValue;

			int byteOffset = pickPos(3, (int) (BYTE_OFFSET_MASK & offset));
			int bitOffset = BITS_IN_BYTE * byteOffset;
			int primitiveMask = BYTE_MASK_INT << (BC_SHIFT_INT_MASK & bitOffset);

			result = (byte) compareAndExchangeForSmallTypesArraysHelper(obj, offset, compareValue, exchangeValue,
					bitOffset, primitiveMask);
		} else {
			/* object is non-null primitive type */
			result = (byte) compareAndExchangeForSmallTypesPrimitivesHelper(obj, offset, compareValue, exchangeValue);
		}

		return result;
	}

	/*
	 * Verify that parameters are valid.
	 * 
	 * @throws IllegalArgumentException if 16 bit primitive would span multiple
	 * memory blocks
	 */
	private void compareAndExchange16BitsOffsetChecks(long offset) {
		if ((IS_BIG_ENDIAN) && (BYTE_OFFSET_MASK == (BYTE_OFFSET_MASK & offset))) {
			/*[MSG "K0700", "Update spans the word, not supported"]*/
			throw new IllegalArgumentException(com.ibm.oti.util.Msg.getString("K0700")); //$NON-NLS-1$
		}
	}

	/* 
	 * Generic compareAndExchange for 16 bit primitives (short and char).
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value extended to the size of an int that is 
	 * expected to be in obj at offset
	 * @param exchangeValue value extended to the size of an int that will 
	 * be set in obj at offset if compare is successful
	 * @return value in obj at offset before this operation. This will be compareValue 
	 * if the exchange was successful
	 * 
	 * @throws IllegalArgumentException if 16 bit type is unaligned in memory
	 */
	private final short compareAndExchange16bits(Object obj, long offset, int compareValue, int exchangeValue) {
		short result;

		compareAndExchange16BitsOffsetChecks(offset);

		if ((obj == null) || obj.getClass().isArray()) {
			/* mask extended 16 bit type to remove any sign extension for negative values */
			compareValue = SHORT_MASK_INT & compareValue;
			exchangeValue = SHORT_MASK_INT & exchangeValue;
			
			int byteOffset = pickPos(2, (int) (BYTE_OFFSET_MASK & offset));
			int bitOffset = BITS_IN_BYTE * byteOffset;
			int primitiveMask = SHORT_MASK_INT << (BC_SHIFT_INT_MASK & bitOffset);

			result = (short) compareAndExchangeForSmallTypesArraysHelper(obj, offset, compareValue, exchangeValue,
					bitOffset, primitiveMask);
		} else {
			/* object is non-null primitive type */
			result = (short) compareAndExchangeForSmallTypesPrimitivesHelper(obj, offset, compareValue, exchangeValue);
		}

		return result;
	}

	/* 
	 * Compare and exchange for a primitive and not an array. Method is for
	 * primitives smaller than an int such as byte, boolean, char, short.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value extended to the size of an int that is 
	 * expected to be in obj at offset
	 * @param exchangeValue value extended to the size of an int that will 
	 * be set in obj at offset if compare is successful
	 * @return value of the field before this operation extended to the size of 
	 * an int. This will be compareValue if the exchange was successful
	 */
	private final int compareAndExchangeForSmallTypesPrimitivesHelper(Object obj, long offset, int compareValue,
			int exchangeValue) {
		return compareAndExchangeInt(obj, offset, compareValue, exchangeValue);
	}

	/* 
	 * Compare and exchange for an array of primitives. Method is for primitives
	 * smaller than an int such as byte, boolean, char, short.
	 * 
	 * @param obj object into which to store the value
	 * @param offset location to compare and store value in obj
	 * @param compareValue value extended to the size of an int that is 
	 * expected to be in obj at offset
	 * @param exchangeValue value extended to the size of an int that will 
	 * be set in obj at offset if compare is successful
	 * @param bitOffset offset within aligned int to exchanged primitive
	 * @param primitiveMask masks bits of exchange value in int
	 * @return value of the field before this operation extended to the size of an int. 
	 * This will be compareValue if the exchange was successful
	 */
	private final int compareAndExchangeForSmallTypesArraysHelper(Object obj, long offset, int compareValue,
			int exchangeValue, int bitOffset, int primitiveMask) {
		for (;;) {
			int compareCopy = getIntVolatile(obj, offset & INT_OFFSET_ALIGN_MASK);

			/* If object's compare value does not match compareValue argument, fail. */
			if ((compareValue << (BC_SHIFT_INT_MASK & bitOffset)) != (primitiveMask & compareCopy)) {
				return (primitiveMask & compareCopy) >> bitOffset; /* failure */
			}

			/*
			 * Insert exchange value in appropriate position to perform int compare and
			 * exchange.
			 */
			int exchangeCopy = (~primitiveMask) & compareCopy;
			exchangeCopy |= exchangeValue << (BC_SHIFT_INT_MASK & bitOffset);

			int result = compareAndExchangeInt(obj, offset & INT_OFFSET_ALIGN_MASK, compareCopy, exchangeCopy);
			
			if (result == compareCopy) {
				return compareValue; /* success */
			} else {
				/*
				 * An array value has changed so this operation is no longer atomic. If the
				 * index that changed is the one we are trying to exchange, fail. Otherwise try
				 * again.
				 */
				result &= primitiveMask;
				if ((primitiveMask & compareCopy) != result) {
					return result >>> (BC_SHIFT_INT_MASK & bitOffset); /* failure */
				}
			}
		}
	}

	/* 
	 * Verify that that no bits are set in long past the least
	 * significant 32.
	 * 
	 * @return true if no bits past 32 are set, false otherwise
	 */
	private boolean is32BitClean(long value) {
		long shiftedValue = value >>> 32;
		return (0L == shiftedValue);
	}

	/* 
	 * Verify that parameter is a valid size.
	 * 
	 * @throws IllegalArgumentException if parameter is not valid
	 */
	private void checkSize(long value) {
		if (BYTES_IN_INT == ADDRESS_SIZE) {
			if (!is32BitClean(value)) {
				throw invalidInput();
			}
		} else {
			if (value < 0) {
				throw invalidInput();
			}
		}
	}

	/* 
	 * Verify that the address parameter does not exceed the 
	 * maximum possible value of a native address. Address may 
	 * be negative to support sign extended pointers.
	 * 
	 * @throws IllegalArgumentException if address is invalid
	 */
	private void checkNativeAddress(long address) {
		if (BYTES_IN_INT == ADDRESS_SIZE) {
			long shiftedValue = address >> 32;
			
			/* shiftedValue at this point will be either -1 if address
			 * is at most 32 bits and negative, or 0 if the address is 
			 * 32 bits and positive. The following calculation will only 
			 * equate to zero and pass the check in these two cases.
			 */
			shiftedValue = (shiftedValue + 1) & -2L;
			
			if (0 != shiftedValue) {
				throw invalidInput();
			}
		}
	}

	/* 
	 * Verify that parameter is a valid offset in obj.
	 * 
	 * @throws IllegalArgumentException if offset is invalid
	 */
	private void checkOffset(Object obj, long offset) {
		if (BYTES_IN_INT == ADDRESS_SIZE) {
			boolean isClean = is32BitClean(offset);
			if (!isClean) {
				throw invalidInput();
			}
		} else {
			if (offset < 0) {
				throw invalidInput();
			}
		}
	}

	/* 
	 * Verify that parameter is a valid offset in obj.
	 * 
	 * @throws IllegalArgumentException if offset is invalid
	 */
	private void checkPointer(Object obj, long offset) {
		if (null == obj) {
			checkNativeAddress(offset);
		} else {
			checkOffset(obj, offset);
		}
	}

	/* 
	 * Verify that parameter is a valid array.
	 * 
	 * @throws IllegalArgumentException if verification fails
	 */
	private void checkPrimitiveArray(Class<?> c) {
		Class<?> cType = c.getComponentType();

		if (null == cType || !cType.isPrimitive()) {
			throw invalidInput();
		}
	}

	/* 
	 * Verify that parameter is a valid offset in obj,
	 * and obj is a valid array.
	 * 
	 * @throws IllegalArgumentException if verification fails
	 */
	private void checkPrimitivePointer(Object obj, long offset) {
		checkPointer(obj, offset);

		if (null != obj) {
			checkPrimitiveArray(obj.getClass());
		}
	}

	/* 
	 * Verify that parameters is a valid size.
	 * 
	 * @throws IllegalArgumentException if parameter is not valid
	 */
	private void allocateMemoryChecks(long size) {
		checkSize(size);
	}

	/* 
	 * Verify that parameters are valid.
	 * 
	 * @throws IllegalArgumentException if parameters are not valid
	 */
	private void reallocateMemoryChecks(long address, long size) {
		checkPointer(null, address);
		checkSize(size);
	}

	/* 
	 * Verify that parameters are valid.
	 * 
	 * @throws IllegalArgumentException if startIndex is illegal in obj, or 
	 * if size is invalid
	 */
	private void setMemoryChecks(Object obj, long startIndex, long size, byte replace) {
		checkPrimitivePointer(obj, startIndex);
		checkSize(size);
	}

	/* 
	 * Verify that parameters are valid.
	 * 
	 * @throws IllegalArgumentException if srcOffset is illegal in srcObj, 
	 * if destOffset is illegal in destObj, or if size is invalid
	 */
	private void copyMemoryChecks(Object srcObj, long srcOffset, Object destObj, long destOffset, long size) {
		checkSize(size);
		checkPrimitivePointer(srcObj, srcOffset);
		checkPrimitivePointer(destObj, destOffset);
	}

	/* 
	 * Verify that parameters are valid.
	 * 
	 * @throws IllegalArgumentException if srcOffset is illegal in srcObj, 
	 * if destOffset is illegal in destObj, if copySize is invalid or copySize is not
	 * a multiple of elementSize
	 */
	private void copySwapMemoryChecks(Object srcObj, long srcOffset, Object destObj, long destOffset, long copySize,
			long elementSize) {
		checkSize(copySize);
		if ((2 == elementSize) || (4 == elementSize) || (8 == elementSize)) {
			if (0 == (copySize % elementSize)) {
				checkPrimitivePointer(srcObj, srcOffset);
				checkPrimitivePointer(destObj, destOffset);
			} else {
				throw invalidInput();
			}
		} else {
			throw invalidInput();
		}
	}

	/*
	 * Verify that parameter is valid.
	 * 
	 * @throws IllegalArgumentException if parameter is not valid
	 */
	private void freeMemoryChecks(long startIndex) {
		checkPointer(null, startIndex);
	}

	/* 
	 * Allocate new array of same type as class parameter and 
	 * length of int parameter.
	 * 
	 * @param c class of same type as desired array
	 * @param length desired length of array
	 * @return allocated array of desired length and type, or null
	 * if class type is not a primitive wrapper
	 */
	private Object allocateUninitializedArray0(Class<?> c, int length) {
		Object result = null;

		if (c == Byte.TYPE) {
			result = new byte[length];
		} else if (c == Boolean.TYPE) {
			result = new boolean[length];
		} else if (c == Short.TYPE) {
			result = new short[length];
		} else if (c == Character.TYPE) {
			result = new char[length];
		} else if (c == Integer.TYPE) {
			result = new int[length];
		} else if (c == Float.TYPE) {
			result = new float[length];
		} else if (c == Long.TYPE) {
			result = new long[length];
		} else if (c == Double.TYPE) {
			result = new double[length];
		}

		return result;
	}

	/* Convert short primitive to char. */
	private char s2c(short value) {
		return (char) value;
	}

	/*  Convert char primitive to short. */
	private short c2s(char value) {
		return (short) value;
	}

	/* Convert byte primitive to boolean. */
	private boolean byte2bool(byte value) {
		return (value != 0);
	}

	/* Convert boolean primitive to byte. */
	private byte bool2byte(boolean value) {
		return (value) ? (byte) 1 : (byte) 0;
	}

	/* Throws new instance of IllegalAccessError. */
	private static void throwIllegalAccessError() {
		throw new IllegalAccessError();
	}

	/* 
	 * Picks position based on endianness.
	 * 
	 * @param value total length of object
	 * @param position little endian position of object
	 * @return position based on endianness of machine
	 */
	private static int pickPos(int value, int position) {
		return (IS_BIG_ENDIAN) ? (value - position) : (position);
	}

	/* 
	 * Creates an int by concatenating two shorts where the first parameter is the 
	 * least significant. Ordering is based on endianness of the machine.
	 */
	private static int makeInt(short arg0, short arg1) {
		int result;
		result = toUnsignedInt(arg0) << (BC_SHIFT_INT_MASK & pickPos(16, 0));
		result |= toUnsignedInt(arg1) << (BC_SHIFT_INT_MASK & pickPos(16, 16));
		return result;
	}

	/* 
	 * Creates an int by concatenating four bytes where the first parameter is the 
	 * least significant. Ordering is based on endianness of the machine.
	 */
	private static int makeInt(byte arg0, byte arg1, byte arg2, byte arg3) {
		int result;
		result = toUnsignedInt(arg0) << (BC_SHIFT_INT_MASK & pickPos(24, 0));
		result |= toUnsignedInt(arg1) << (BC_SHIFT_INT_MASK & pickPos(24, 8));
		result |= toUnsignedInt(arg2) << (BC_SHIFT_INT_MASK & pickPos(24, 16));
		result |= toUnsignedInt(arg3) << (BC_SHIFT_INT_MASK & pickPos(24, 24));
		return result;
	}
	
	/* 
	 * Creates a long by concatenating eight bytes where the first parameter is the least significant.
	 * Ordering is based on endianness of the machine.
	 */
	private static long makeLong(byte arg0, byte arg1, byte arg2, byte arg3, byte arg4, byte arg5, byte arg6,
			byte arg7) {
		long result;
		result = toUnsignedLong(arg0) << (BC_SHIFT_LONG_MASK & pickPos(56, 0));
		result |= toUnsignedLong(arg1) << (BC_SHIFT_LONG_MASK & pickPos(56, 8));
		result |= toUnsignedLong(arg2) << (BC_SHIFT_LONG_MASK & pickPos(56, 16));
		result |= toUnsignedLong(arg3) << (BC_SHIFT_LONG_MASK & pickPos(56, 24));
		result |= toUnsignedLong(arg4) << (BC_SHIFT_LONG_MASK & pickPos(56, 32));
		result |= toUnsignedLong(arg5) << (BC_SHIFT_LONG_MASK & pickPos(56, 40));
		result |= toUnsignedLong(arg6) << (BC_SHIFT_LONG_MASK & pickPos(56, 48));
		result |= toUnsignedLong(arg7) << (BC_SHIFT_LONG_MASK & pickPos(56, 56));
		return result;
	}

	/* 
	 * Creates a long by concatenating four shorts where the first parameter is the least
	 * significant. Ordering is based on endianness of the machine.
	 */
	private static long makeLong(short arg0, short arg1, short arg2, short arg3) {
		long result;
		result = toUnsignedLong(arg0) << (BC_SHIFT_LONG_MASK & pickPos(48, 0));
		result |= toUnsignedLong(arg1) << (BC_SHIFT_LONG_MASK & pickPos(48, 16));
		result |= toUnsignedLong(arg2) << (BC_SHIFT_LONG_MASK & pickPos(48, 32));
		result |= toUnsignedLong(arg3) << (BC_SHIFT_LONG_MASK & pickPos(48, 48));
		return result;
	}

	/* 
	 * Creates a long by concatenating two integers where the first parameter is the
	 * least significant. Ordering is based on endianness of the machine.
	 */
	private static long makeLong(int arg0, int arg1) {
		long result;
		result = toUnsignedLong(arg0) << (BC_SHIFT_LONG_MASK & pickPos(32, 0));
		result |= toUnsignedLong(arg1) << (BC_SHIFT_LONG_MASK & pickPos(32, 32));
		return result;
	}

	/* 
	 * Creates a short by concatenating two bytes where the first parameter is the 
	 * least significant. Ordering is based on endianness of the machine.
	 */
	private static short makeShort(byte arg0, byte arg1) {
		int result;
		result = toUnsignedInt(arg0) << (BC_SHIFT_INT_MASK & pickPos(8, 0));
		result |= toUnsignedInt(arg1) << (BC_SHIFT_INT_MASK & pickPos(8, 8));
		return (short) result;
	}

	/* 
	 * Pick first parameter if machine is little endian, 
	 * otherwise pick second.
	 */
	private static byte pick(byte arg0, byte arg1) {
		return (IS_BIG_ENDIAN) ? arg1 : arg0;
	}

	/* 
	 * Pick first parameter if machine is little endian, 
	 * otherwise pick second.
	 */
	private static short pick(short arg0, short arg1) {
		return (IS_BIG_ENDIAN) ? arg1 : arg0;
	}

	/* 
	 * Pick first parameter if machine is little endian, 
	 * otherwise pick second.
	 */
	private static int pick(int arg0, int arg1) {
		return (IS_BIG_ENDIAN) ? arg1 : arg0;
	}

	/* 
	 * Insert int value inserted per short into obj at offset. The first short parameter is the 
	 * least significant and will be inserted according to machine endianness.
	 */
	private void putIntParts(Object obj, long offset, short part1, short part2) {
		putShort(obj, 0L + offset, pick(part1, part2));
		putShort(obj, 2L + offset, pick(part2, part1));
	}

	/* 
	 * Insert int value inserted per byte into obj at offset. The first byte parameter is the 
	 * least significant and will be inserted according to machine endianness.
	 */
	private void putIntParts(Object obj, long offset, byte part1, byte part2, byte part3, byte part4) {
		putByte(obj, 0L + offset, pick(part1, part4));
		putByte(obj, 1L + offset, pick(part2, part3));
		putByte(obj, 2L + offset, pick(part3, part2));
		putByte(obj, 3L + offset, pick(part4, part1));
	}

	/* 
	 * Insert long value inserted per byte into obj at offset. The first byte parameter is the 
	 * least significant and will be inserted according to machine endianness.
	 */
	private void putLongParts(Object obj, long offset, byte part1, byte part2, byte part3, byte part4, byte part5,
			byte part6, byte part7, byte part8) {
		putByte(obj, 0L + offset, pick(part1, part8));
		putByte(obj, 1L + offset, pick(part2, part7));
		putByte(obj, 2L + offset, pick(part3, part6));
		putByte(obj, 3L + offset, pick(part4, part5));
		putByte(obj, 4L + offset, pick(part5, part4));
		putByte(obj, 5L + offset, pick(part6, part3));
		putByte(obj, 6L + offset, pick(part7, part2));
		putByte(obj, 7L + offset, pick(part8, part1));
	}
	
	/* 
	 * Insert long value inserted per short into obj at offset. The first short parameter is the 
	 * least significant and will be inserted according to machine endianness.
	 */
	private void putLongParts(Object obj, long offset, short part1, short part2, short part3, short part4) {
		putShort(obj, 0L + offset, pick(part1, part4));
		putShort(obj, 2L + offset, pick(part2, part3));
		putShort(obj, 4L + offset, pick(part3, part2));
		putShort(obj, 6L + offset, pick(part4, part1));
	}

	/* 
	 * Insert long value inserted per int into obj at offset. The first int parameter is the 
	 * least significant and will be inserted according to machine endianness.
	 */
	private void putLongParts(Object obj, long offset, int part1, int part2) {
		putInt(obj, 0L + offset, pick(part1, part2));
		putInt(obj, 4L + offset, pick(part2, part1));
	}

	/* 
	 * Insert short value inserted per byte into obj at offset. The first byte parameter is the 
	 * least significant and will be inserted according to machine endianness.
	 */
	private void putShortParts(Object obj, long offset, byte part1, byte part2) {
		putByte(obj, 0L + offset, pick(part1, part2));
		putByte(obj, 1L + offset, pick(part2, part1));
	}

	/* Converts byte to unsigned int. */
	private static int toUnsignedInt(byte value) {
		int ivalue = value;
		return BYTE_MASK_INT & ivalue;
	}

	/* Converts short to unsigned int. */
	private static int toUnsignedInt(short value) {
		int ivalue = value;
		return SHORT_MASK_INT & ivalue;
	}

	/* Converts byte to unsigned long. */
	private static long toUnsignedLong(byte value) {
		long lvalue = value;
		return BYTE_MASK & lvalue;
	}

	/* Converts short to unsigned long. */
	private static long toUnsignedLong(short value) {
		long lvalue = value;
		return SHORT_MASK & lvalue;
	}

	/* Converts int to unsigned long. */
	private static long toUnsignedLong(int value) {
		long lvalue = value;
		return INT_MASK & lvalue;
	}
	
	/* Convert int value according to users endianness preference. Bytes may be reversed. */
	private static int convEndian(boolean isBigEndian, int value) {
		return (IS_BIG_ENDIAN == isBigEndian) ? value : Integer.reverseBytes(value);
	}

	/* Convert long value according to users endianness preference. Bytes may be reversed. */
	private static long convEndian(boolean isBigEndian, long value) {
		return (IS_BIG_ENDIAN == isBigEndian) ? value : Long.reverseBytes(value);
	}

	/* Convert short value according to users endianness preference. Bytes may be reversed. */
	private static short convEndian(boolean isBigEndian, short value) {
		return (IS_BIG_ENDIAN == isBigEndian) ? value : Short.reverseBytes(value);
	}

	/* Converts char value according to users endianness preference. Bytes may be reversed. */
	private static char convEndian(boolean isBigEndian, char value) {
		return (IS_BIG_ENDIAN == isBigEndian) ? value : Character.reverseBytes(value);
	}
}
