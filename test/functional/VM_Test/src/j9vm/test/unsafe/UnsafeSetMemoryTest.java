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
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.ByteOrder;

import sun.misc.Unsafe;

public class UnsafeSetMemoryTest {
	static Unsafe myUnsafe;
	static Method setMemoryMethod;
	static final ByteOrder byteOrder = ByteOrder.nativeOrder();

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

	private byte byteInWord(long value, int bytesInValue, long offset) {
		int shiftAmount = (int)(offset % bytesInValue) * 8;
		if (byteOrder == ByteOrder.BIG_ENDIAN) {
			/* Least significant byte on BIG_ENDIAN is on the left
			 * therefore requiring largest shiftAmount 
			 */
			shiftAmount = ((bytesInValue - 1) * 8) - shiftAmount;
		}
		value >>= shiftAmount;
		return (byte)value;
	}
	
	private byte byteAt(Object array, long offset) {
		if (array instanceof byte[]) {
			long value = ((byte[])array)[(int)offset];
			return byteInWord(value, 1, offset);
		} else if (array instanceof short[]) {
			long value = ((short[])array)[(int)(offset / 2)];
			return byteInWord(value, 2, offset);
		} else if (array instanceof int[]) {
			long value = ((int[])array)[(int)(offset / 4)];
			return byteInWord(value, 4, offset);
		} else if (array instanceof long[]) {
			long value = ((long[])array)[(int)(offset / 8)];
			return byteInWord(value, 8, offset);
		} else {
			throw new IllegalArgumentException();
		}
	}
	
	private static void setMemory(java.lang.Object o, long offset, long bytes, byte value) {
		try {
			setMemoryMethod.invoke(myUnsafe, new Object[] { o, offset, bytes, value });
		} catch (IllegalArgumentException e) {
			throw new Error("Reflect exception.", e);
		} catch (IllegalAccessException e) {
			throw new Error("Reflect exception.", e);
		} catch (InvocationTargetException e) {
			throw new Error("Reflect exception.", e);
		}
	}
	
	public void testLargeArrays() {
		System.out.println("Testing Unsafe.setMemory(Object,long,long,byte) for large arrays.");
		/* we can't test large arrays exhaustively; use prime numbers to try to get a good selection of test cases */
		for (int arrayBytes = 32*1024; arrayBytes <= 1024*1024; arrayBytes = arrayBytes * 11 - 1) {
			for (long length = 1021; length <= arrayBytes; length = length * 13 - 1) {
				for (long start = 859; start < arrayBytes - length; start = start * 17 - 1) {
					/* test with setValue=1, unsetValue=0 */
					testSetMemory(new byte[arrayBytes], (byte)1, (byte)0, start, length);
					testSetMemory(new short[arrayBytes / 2], (byte)1, (byte)0, start, length);
					testSetMemory(new int[arrayBytes / 4], (byte)1, (byte)0, start, length);
					testSetMemory(new long[arrayBytes / 8], (byte)1, (byte)0, start, length);
				}
			}
		}
	}
	
	public void testSmallArrays() {
		System.out.println("Testing Unsafe.setMemory(Object,long,long,byte) for small arrays.");
		int arrayBytes = 32;
		for (long length = 1; length <= arrayBytes; length++) {
			for (long start = 0; start < arrayBytes - length; start++) {
				/* test with setValue=1, unsetValue=0 */
				testSetMemory(new byte[arrayBytes], (byte)1, (byte)0, start, length);
				testSetMemory(new short[arrayBytes / 2], (byte)1, (byte)0, start, length);
				testSetMemory(new int[arrayBytes / 4], (byte)1, (byte)0, start, length);
				testSetMemory(new long[arrayBytes / 8], (byte)1, (byte)0, start, length);
				
				/* test with setValue=-1, unsetValue=0 */
				testSetMemory(new byte[arrayBytes], (byte)-1, (byte)0, start, length);
				testSetMemory(new short[arrayBytes / 2], (byte)-1, (byte)0, start, length);
				testSetMemory(new int[arrayBytes / 4], (byte)-1, (byte)0, start, length);
				testSetMemory(new long[arrayBytes / 8], (byte)-1, (byte)0, start, length);

				/* test with setValue=0, unsetValue=-1 */
				byte[] byteArray = new byte[arrayBytes];
				for (int i = 0; i < byteArray.length; i++) {
					byteArray[i] = -1;
				}
				testSetMemory(byteArray, (byte)0, (byte)-1, start, length);

				short[] shortArray = new short[arrayBytes / 2];
				for (int i = 0; i < shortArray.length; i++) {
					shortArray[i] = -1;
				}
				testSetMemory(shortArray, (byte)0, (byte)-1, start, length);
			
				int[] intArray = new int[arrayBytes / 4];
				for (int i = 0; i < intArray.length; i++) {
					intArray[i] = -1;
				}
				testSetMemory(intArray, (byte)0, (byte)-1, start, length);

				long[] longArray = new long[arrayBytes / 8];
				for (int i = 0; i < longArray.length; i++) {
					longArray[i] = -1L;
				}
				testSetMemory(longArray, (byte)0, (byte)-1, start, length);
			}
		}
	}

	private void testSetMemory(Object array, byte setValue, byte unsetValue, long start, long length) throws Error {
		long baseOffset = myUnsafe.arrayBaseOffset(array.getClass());
		long indexScale = myUnsafe.arrayIndexScale(array.getClass());
		long arrayLength = Array.getLength(array); 
		setMemory(array, baseOffset + start, length, setValue);
		for (long i = 0; i < arrayLength * indexScale; i++) {
			byte value = byteAt(array, i);
			if ( (i < start) || (i >= start + length) ) {
				if (value != unsetValue) throw new Error("Found unexpected value " + value + " at " + i + " in unset range; expected value " + unsetValue);
			} else {
				if (value != setValue) throw new Error("Found unexpected value " + value + " at " + i + " in set range; expected value " + setValue);
			}
		}
	}

	public static void main(String[] args) throws Exception {
		myUnsafe = getUnsafeInstance();

		/* We want to test the Java 7 setMemory helper. Since these tests also run on Java 6, look it up using reflect. */
		try {
			/* signature: public native void setMemory(java.lang.Object o, long offset, long bytes, byte value); */
			setMemoryMethod = myUnsafe.getClass().getDeclaredMethod("setMemory", new Class[] { Object.class, long.class, long.class, byte.class });
		} catch (NoSuchMethodException e) {
			System.out.println("Class library does not include sun.misc.Unsafe.setMemory(java.lang.Object o, long offset, long bytes, byte value) -- skipping test");
			return;
		}

		UnsafeSetMemoryTest tester = new UnsafeSetMemoryTest();
		tester.testSmallArrays();
		tester.testLargeArrays();
	}
	
}
