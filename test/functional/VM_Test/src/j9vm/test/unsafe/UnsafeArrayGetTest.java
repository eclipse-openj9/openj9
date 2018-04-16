/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
package j9vm.test.unsafe;

import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.nio.ByteOrder;
import java.util.Random;

import sun.misc.Unsafe;

public class UnsafeArrayGetTest {
	static Unsafe myUnsafe;
	static final ByteOrder byteOrder = ByteOrder.nativeOrder();
	
	/* the small arrays are initialized with sequential, known values to make it easier to debug errors here */
	static final byte[] byteArray = new byte[] { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
	static final short[] shortArray = new short[] { 0x0001, 0x0203, 0x0405, 0x0607, 0x0809, 0x0a0b, 0x0c0d, 0x0e0f };
	static final int[] intArray = new int[] { 0x00010203, 0x04050607, 0x080090a0b, 0x0c0d0e0f };
	static final long[] longArray = new long[] { 0x0001020304050607L, 0x08090a0b0c0d0e0fL };
	
	/* the large arrays are initialized with random values to ensure good coverage */
	static final byte[] largeByteArray = new byte[1024 * 1024];
	static final short[] largeShortArray = new short[1024 * 1024 / 2];
	static final int[] largeIntArray = new int[1024 * 1024 / 4];
	static final long[] largeLongArray = new long[1024 * 1024 / 8];

	/* Theses buffers contain the same data as the above arrays, but stored in byte arrays to allow us to simulate misaligned access. */
	/* We could have used ByteBuffer for this, but it might use Unsafe in its implementation, negating the value of the test. */
	static final byte[] byteBuffer = new byte[byteArray.length];
	static final byte[] shortBuffer = new byte[shortArray.length * 2];
	static final byte[] intBuffer = new byte[intArray.length * 4];
	static final byte[] longBuffer = new byte[longArray.length * 8];
	static final byte[] largeByteBuffer = new byte[largeByteArray.length];
	static final byte[] largeShortBuffer = new byte[largeShortArray.length * 2];
	static final byte[] largeIntBuffer = new byte[largeIntArray.length * 4];
	static final byte[] largeLongBuffer = new byte[largeLongArray.length * 8];

	static final Object[] arrays  = { byteArray,  shortArray,  intArray,  longArray,  largeByteArray,  largeShortArray,  largeIntArray,  largeLongArray  };
	static final byte[][] buffers = { byteBuffer, shortBuffer, intBuffer, longBuffer, largeByteBuffer, largeShortBuffer, largeIntBuffer, largeLongBuffer };
	
	static void initializeData() {
		Random random = new Random(0x80968096L);
		for (int i = 0; i < largeByteArray.length; i++) {
			largeByteArray[i] = (byte)(0x7f - random.nextInt(0xff));
		}
		for (int i = 0; i < largeShortArray.length; i++) {
			largeShortArray[i] = (short)(0x7fff - random.nextInt(0xffff));
		}
		for (int i = 0; i < largeIntArray.length; i++) {
			largeIntArray[i] = random.nextInt();
		}
		for (int i = 0; i < largeLongArray.length; i++) {
			largeLongArray[i] = random.nextLong();
		}
		
		for (int i = 0; i < byteArray.length; i++) {
			putByte(byteBuffer, i, byteArray[i]);
		}
		for (int i = 0; i < shortArray.length; i++) {
			putShort(shortBuffer, i*2, shortArray[i]);
		}
		for (int i = 0; i < intArray.length; i++) {
			putInt(intBuffer, i*4, intArray[i]);
		}
		for (int i = 0; i < longArray.length; i++) {
			putLong(longBuffer, i*8, longArray[i]);
		}
		
		for (int i = 0; i < largeByteArray.length; i++) {
			putByte(largeByteBuffer, i, largeByteArray[i]);
		}
		for (int i = 0; i < largeShortArray.length; i++) {
			putShort(largeShortBuffer, i*2, largeShortArray[i]);
		}
		for (int i = 0; i < largeIntArray.length; i++) {
			putInt(largeIntBuffer, i*4, largeIntArray[i]);
		}
		for (int i = 0; i < largeLongArray.length; i++) {
			putLong(largeLongBuffer, i*8, largeLongArray[i]);
		}
	}
	
	private static byte getByte(byte[] buffer, int index) {
		return buffer[index];
	}

	private static void putByte(byte[] buffer, int index, byte value) {
		buffer[index] = value;
	}
	
	private static short getShort(byte[] buffer, int index) {
		int byte0 = buffer[index] & 0xff;
		int byte1 = buffer[index + 1] & 0xff;
		if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
			return (short)((byte1 << 8) | byte0);
		} else {
			return (short)((byte0 << 8) | byte1);
		}
	}

	private static void putShort(byte[] buffer, int index, short value) {
		if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
			buffer[index] = (byte)value;
			buffer[index+1] = (byte)(value >> 8);
		} else {
			buffer[index+1] = (byte)value;
			buffer[index] = (byte)(value >> 8);
		}
	}
	
	private static int getInt(byte[] buffer, int index) {
		int byte0 = buffer[index] & 0xff;
		int byte1 = buffer[index + 1] & 0xff;
		int byte2 = buffer[index + 2] & 0xff;
		int byte3 = buffer[index + 3] & 0xff;
		if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
			return ((byte3 << 24) | (byte2 << 16) | (byte1 << 8) | byte0);
		} else {
			return ((byte0 << 24) | (byte1 << 16) | (byte2 << 8) | byte3);
		}
	}

	private static void putInt(byte[] buffer, int index, int value) {
		if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
			buffer[index] = (byte)value;
			buffer[index+1] = (byte)(value >> 8);
			buffer[index+2] = (byte)(value >> 16);
			buffer[index+3] = (byte)(value >> 24);
		} else {
			buffer[index+3] = (byte)value;
			buffer[index+2] = (byte)(value >> 8);
			buffer[index+1] = (byte)(value >> 16);
			buffer[index] = (byte)(value >> 24);
		}
	}

	private static long getLong(byte[] buffer, int index) {
		long byte0 = buffer[index] & 0xff;
		long byte1 = buffer[index + 1] & 0xff;
		long byte2 = buffer[index + 2] & 0xff;
		long byte3 = buffer[index + 3] & 0xff;
		long byte4 = buffer[index + 4] & 0xff;
		long byte5 = buffer[index + 5] & 0xff;
		long byte6 = buffer[index + 6] & 0xff;
		long byte7 = buffer[index + 7] & 0xff;
		if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
			return ((byte7 << 56) | (byte6 << 48) | (byte5 << 40) | (byte4 << 32) | (byte3 << 24) | (byte2 << 16) | (byte1 << 8) | byte0);
		} else {
			return ((byte0 << 56) | (byte1 << 48) | (byte2 << 40) | (byte3 << 32) | (byte4 << 24) | (byte5 << 16) | (byte6 << 8) | byte7);
		}
	}
	
	private static void putLong(byte[] buffer, int index, long value) {
		if (byteOrder == ByteOrder.LITTLE_ENDIAN) {
			buffer[index] = (byte)value;
			buffer[index+1] = (byte)(value >> 8);
			buffer[index+2] = (byte)(value >> 16);
			buffer[index+3] = (byte)(value >> 24);
			buffer[index+4] = (byte)(value >> 32);
			buffer[index+5] = (byte)(value >> 40);
			buffer[index+6] = (byte)(value >> 48);
			buffer[index+7] = (byte)(value >> 56);
		} else {
			buffer[index+7] = (byte)value;
			buffer[index+6] = (byte)(value >> 8);
			buffer[index+5] = (byte)(value >> 16);
			buffer[index+4] = (byte)(value >> 24);
			buffer[index+3] = (byte)(value >> 32);
			buffer[index+2] = (byte)(value >> 40);
			buffer[index+1] = (byte)(value >> 48);
			buffer[index] = (byte)(value >> 56);
		}
	}

	/* use reflection to bypass the security manager */
	private static Unsafe getUnsafeInstance() throws IllegalAccessException {
		/* the instance is likely stored in a static field of type Unsafe */
		Field[] staticFields = Unsafe.class.getDeclaredFields();
		for (Field field : staticFields) {
			if (field.getType() == Unsafe.class) {
		 		field.setAccessible(true);
		 		return (Unsafe)field.get(Unsafe.class);
			}
		}
		throw new Error("Unable to find an instance of Unsafe");
	}
	
	public void testGetObject() {
		Object[] array = new Object[] { "a", "b", "c", "d" };
		
		long arrayIndexScale = myUnsafe.arrayIndexScale(array.getClass());
		long arrayIndexBase = myUnsafe.arrayBaseOffset(array.getClass());
		
		long lowestOffset = arrayIndexBase;
		long highestOffset = arrayIndexBase + (arrayIndexScale * (array.length - 1)); 
		
		System.out.println("Testing getObject() for " + array.getClass().getComponentType() + "[" + array.length + "]");
		/* don't try to test misaligned offsets since that will result in invalid object pointers */
		int index = 0;
		for (long offset = lowestOffset; offset <= highestOffset; offset += arrayIndexScale, index += 1) {
			Object value = myUnsafe.getObject(array, offset);
			Object volatileValue = myUnsafe.getObjectVolatile(array,offset);
			if (value != volatileValue) {
				throw new Error("getObject() != getObjectVolatile() for offset=" + offset);
			}
			if (value != array[index]) {
				throw new Error("getObject() != array[i] for offset=" + offset);
			}
		}
	}
	
	public void testGetByte() {
		for (int i = 0; i < arrays.length; i++) {
			Object array = arrays[i];
			byte[] buffer = buffers[i];
			
			int arrayLength = Array.getLength(array);
			long arrayIndexScale = myUnsafe.arrayIndexScale(array.getClass());
			long arrayIndexBase = myUnsafe.arrayBaseOffset(array.getClass());
			long elementSize = myUnsafe.arrayIndexScale(byte[].class);

			long lowestOffset = arrayIndexBase;
			long highestOffset = arrayIndexBase + (arrayIndexScale * arrayLength) - elementSize; 
			
			System.out.println("Testing getByte() for " + array.getClass().getComponentType() + "[" + arrayLength + "]");
			int bufferIndex = 0;
			for (long offset = lowestOffset; offset <= highestOffset; offset += 1, bufferIndex += 1) {
				byte value = myUnsafe.getByte(array, offset);
				byte volatileValue = myUnsafe.getByteVolatile(array,offset);
				if (value != volatileValue) {
					throw new Error("getByte() != getByteVolatile() for offset=" + offset);
				}
				byte bufferValue = getByte(buffer, bufferIndex);
				if (value != bufferValue) {
					throw new Error("getByte() != manual read for offset=" + offset + "; expecting " + bufferValue + "; got " + value);
				}
			}
		}
	}
	
	public void testGetShort() {
		for (int i = 0; i < arrays.length; i++) {
			Object array = arrays[i];
			byte[] buffer = buffers[i];

			int arrayLength = Array.getLength(array);
			long arrayIndexScale = myUnsafe.arrayIndexScale(array.getClass());
			long arrayIndexBase = myUnsafe.arrayBaseOffset(array.getClass());
			long elementSize = myUnsafe.arrayIndexScale(short[].class);

			long lowestOffset = arrayIndexBase;
			long highestOffset = arrayIndexBase + (arrayIndexScale * arrayLength) - elementSize; 
			
			System.out.println("Testing getShort() for " + array.getClass().getComponentType() + "[" + arrayLength + "]");
			int bufferIndex = 0;
			for (long offset = lowestOffset; offset <= highestOffset; offset += 1, bufferIndex += 1) {
				short value = myUnsafe.getShort(array, offset);
				short volatileValue = myUnsafe.getShortVolatile(array,offset);
				if (value != volatileValue) {
					throw new Error("getShort() != getShortVolatile() for offset=" + offset);
				}
				short bufferValue = getShort(buffer, bufferIndex);
				if (value != bufferValue) {
					throw new Error("getShort() != manual read for offset=" + offset + "; expecting " + bufferValue + "; got " + value);
				}
			}
		}
	}
	
	public void testGetInt() {
		for (int i = 0; i < arrays.length; i++) {
			Object array = arrays[i];
			byte[] buffer = buffers[i];

			int arrayLength = Array.getLength(array);
			long arrayIndexScale = myUnsafe.arrayIndexScale(array.getClass());
			long arrayIndexBase = myUnsafe.arrayBaseOffset(array.getClass());
			long elementSize = myUnsafe.arrayIndexScale(int[].class);
			
			long lowestOffset = arrayIndexBase;
			long highestOffset = arrayIndexBase + (arrayIndexScale * arrayLength) - elementSize; 
			
			System.out.println("Testing getInt() for " + array.getClass().getComponentType() + "[" + arrayLength + "]");
			int bufferIndex = 0;
			for (long offset = lowestOffset; offset <= highestOffset; offset += 1, bufferIndex += 1) {
				int value = myUnsafe.getInt(array, offset);
				int volatileValue = myUnsafe.getIntVolatile(array,offset);
				if (value != volatileValue) {
					throw new Error("getInt() != getIntVolatile() for offset=" + offset);
				}
				int bufferValue = getInt(buffer, bufferIndex);
				if (value != bufferValue) {
					throw new Error("getInt() != manual read for offset=" + offset + "; expecting " + bufferValue + "; got " + value);
				}
			}
		}
	}
	
	public void testGetLong() {
		for (int i = 0; i < arrays.length; i++) {
			Object array = arrays[i];
			byte[] buffer = buffers[i];

			int arrayLength = Array.getLength(array);
			long arrayIndexScale = myUnsafe.arrayIndexScale(array.getClass());
			long arrayIndexBase = myUnsafe.arrayBaseOffset(array.getClass());
			long elementSize = myUnsafe.arrayIndexScale(long[].class);
			
			long lowestOffset = arrayIndexBase;
			long highestOffset = arrayIndexBase + (arrayIndexScale * arrayLength) - elementSize; 
			
			System.out.println("Testing getLong() for " + array.getClass().getComponentType() + "[" + arrayLength + "]");
			int bufferIndex = 0;
			for (long offset = lowestOffset; offset <= highestOffset; offset += 1, bufferIndex += 1) {
				long value = myUnsafe.getLong(array, offset);
				long volatileValue = myUnsafe.getLongVolatile(array,offset);
				if (value != volatileValue) {
					throw new Error("getLong() != getLongVolatile() for offset=" + offset);
				}
				long bufferValue = getLong(buffer, bufferIndex);
				if (value != bufferValue) {
					throw new Error("getLong() != manual read for offset=" + offset + "; expecting " + bufferValue + "; got " + value);
				}
			}
		}
	}
	
	public static void main(String[] args) throws Exception {
		System.out.println("Initializing data for " + UnsafeArrayGetTest.class);
		myUnsafe = getUnsafeInstance();
		initializeData();
		
		UnsafeArrayGetTest tester = new UnsafeArrayGetTest();
		tester.testGetLong();
		tester.testGetInt();
		tester.testGetShort();
		tester.testGetByte();
		tester.testGetObject();
	}
	
}
