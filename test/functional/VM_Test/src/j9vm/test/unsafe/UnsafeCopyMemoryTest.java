/*
 * Copyright IBM Corp. and others 2001
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
package j9vm.test.unsafe;

import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.ByteOrder;

import sun.misc.Unsafe;

public class UnsafeCopyMemoryTest {
	static Unsafe myUnsafe;
	static Method copyMemoryMethod;
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
	
	private static void copyMemory(java.lang.Object srcBase, long srcOffset, java.lang.Object destBase, long destOffset, long bytes) {
		try {
			copyMemoryMethod.invoke(myUnsafe, new Object[] { srcBase, srcOffset, destBase, destOffset, bytes});
		} catch (IllegalArgumentException e) {
			throw new Error("Reflect exception.", e);
		} catch (IllegalAccessException e) {
			throw new Error("Reflect exception.", e);
		} catch (InvocationTargetException e) {
			throw new Error("Reflect exception.", e);
		}
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
	
	private void testUnalignedBeginningAndEnd(Object originalArray, Object copyArray, long offset, long numBytes, boolean beginningAligned, boolean endAligned) {
		long baseOffset = myUnsafe.arrayBaseOffset(originalArray.getClass());
		long indexScale = myUnsafe.arrayIndexScale(originalArray.getClass());
		long numBytesBetweenBaseAndOffset = offset - baseOffset;
		
		if (!beginningAligned) {
			int index = (int)(numBytesBetweenBaseAndOffset / indexScale);
			long copiedValue = Array.getLong(copyArray, index); 
			long originalValue = Array.getLong(originalArray, index);
			
			int expectedNumUnsetBytesInCopiedValue = (int)(numBytesBetweenBaseAndOffset % indexScale);
			for (int i = 0 ; i < expectedNumUnsetBytesInCopiedValue ; i++) {
				byte copiedByte = byteInWord(copiedValue, (int)indexScale, i) ;
				if ( (byte)0 != copiedByte) {
					throw new Error("Unset byte was overwritten in destination array, value: " + copiedByte + ", i=" + i);		
				}
			}
			
			int offsetOfLastSetByteInFirstWord = Math.min((int)indexScale, (int)(expectedNumUnsetBytesInCopiedValue + numBytes));
						
			for (int i = expectedNumUnsetBytesInCopiedValue ; i < offsetOfLastSetByteInFirstWord ; i++) {
				byte originalByte = byteInWord(originalValue, (int)indexScale, i);
				byte copiedByte = byteInWord(copiedValue, (int)indexScale, i) ;
				if ( originalByte != copiedByte) {
					throw new Error("Copied value not equivalent to original, expected: " 
							+ originalByte + " got: " + copiedByte + ", i=" + i
							+ " expectedNumUnsetBytesInCopiedValue: " + expectedNumUnsetBytesInCopiedValue 
							+ " offset: " + offset + " numBytes: " + numBytes);		
				}
			}
		}
		
		if (!endAligned) {
			int index = (int)((numBytesBetweenBaseAndOffset + numBytes) / indexScale);

			long copiedValue = Array.getLong(copyArray, index); 
			long originalValue = Array.getLong(originalArray, index);
			int expectedNumTrailingUnsetBytes = (int)indexScale - (int)((numBytesBetweenBaseAndOffset + numBytes) % indexScale);
			
			int offsetOfFirstSetByteInLastWord = 0;
			if (numBytes < indexScale) {
				offsetOfFirstSetByteInLastWord = (int)(offset % indexScale);
			}
			
			for (int offsetInWord = offsetOfFirstSetByteInLastWord ; offsetInWord < (indexScale - expectedNumTrailingUnsetBytes) ; offsetInWord++) {
				byte originalByte = byteInWord(originalValue, (int)indexScale, offsetInWord);
				byte copiedByte = byteInWord(copiedValue, (int)indexScale, offsetInWord) ;
				if ( originalByte != copiedByte) {
					throw new Error("Copied value not equivalent to original, expected: " 
							+ originalByte + " got: " + copiedByte + ", offsetInWord=" + offsetInWord
							+ " expectedNumTrailingUnsetBytes: " + expectedNumTrailingUnsetBytes 
							+ " offset: " + offset 
							+ " numBytes: " + numBytes
							+ " index: " + index);		
				}
			}
			
			for (int offsetInWord = (int)(indexScale - expectedNumTrailingUnsetBytes) ; offsetInWord < indexScale ; offsetInWord++) {
				byte copiedByte = byteInWord(copiedValue, (int)indexScale, offsetInWord) ;
				if ((byte)0 != copiedByte) {
					throw new Error("Unset byte was overwritten in destination array, value: " + copiedByte 
							+ ", offsetInWord=" + offsetInWord
							+ " indexScale: " + indexScale
							+ " expectedNumTrailingUnsetBytes: " + expectedNumTrailingUnsetBytes
							+ " offset: " + offset
							+ " numBytes: " + numBytes
							+ " index: " + index
							+ " numBytesBetweenBaseAndOffset: " + numBytesBetweenBaseAndOffset);		
				}
			}
		}
	}
	
	private void testEqualArray(Object originalArray, Object copyArray, long offset, long numBytes) {
		long baseOffset = myUnsafe.arrayBaseOffset(originalArray.getClass());
		long indexScale = myUnsafe.arrayIndexScale(originalArray.getClass());
		long numBytesBetweenBaseAndOffset = offset - baseOffset; 
		int firstFullyCopiedElementIndex = (int)(numBytesBetweenBaseAndOffset / indexScale); 
		int lastFullyCopiedElementIndex = (int)((numBytesBetweenBaseAndOffset + numBytes) / indexScale) - 1;
		boolean beginningAligned = true;
		boolean endAligned = true;
		
		if (1 < indexScale) {
			/* if the indexScale is greater than 1 then the copied 
			 * memory may not have been aligned with the number of 
			 * bytes per element in the array
			 */
			beginningAligned = (0 == (numBytesBetweenBaseAndOffset % indexScale));
			endAligned = ( 0 == ((numBytesBetweenBaseAndOffset + numBytes) % indexScale) );
			
			if (!beginningAligned) {
				firstFullyCopiedElementIndex += 1;
			}
			
			if (!beginningAligned || !endAligned) {
				testUnalignedBeginningAndEnd(originalArray, copyArray, offset, numBytes, beginningAligned, endAligned);
			}
		}
		
		if (originalArray instanceof boolean[]) {
			for (int i = 0 ; i < firstFullyCopiedElementIndex ; i++) {
				if ( false != ((boolean[])copyArray)[i]) {
					throw new Error("Uninitialized array element has been overwritten, got: " + ((boolean[])copyArray)[i] + ", i=" + i);
				}
			}
			for (int i = firstFullyCopiedElementIndex ; i <= lastFullyCopiedElementIndex ; i++) {
				if ( ((boolean[])originalArray)[i] != ((boolean[])copyArray)[i]) {
					throw new Error("Copy array element is not equivalent to the original, expected: " + ((boolean[])originalArray)[i] + " got: " + ((boolean[])copyArray)[i] + ", i=" + i);
				}
			}
			for (int i = lastFullyCopiedElementIndex + 1 ; i < Array.getLength(copyArray) ; i++) {
				if ( false != ((boolean[])copyArray)[i]) {
					throw new Error("Uninitialized array element has been overwritten, got: " + ((boolean[])copyArray)[i] + ", i=" + i);
				}
			}
		} else {
			int lastLeadingUnsetElementIndex = beginningAligned ? firstFullyCopiedElementIndex : firstFullyCopiedElementIndex - 1;
			for (int i = 0 ; i < lastLeadingUnsetElementIndex ; i++) {
				long unsetValue = Array.getLong(copyArray, i);
				if ((long)0 != unsetValue) {
					throw new Error("Uninitialized array element has been overwritten, got: " + unsetValue + ", i=" + i);		
				}
			}
			
			for (int i = firstFullyCopiedElementIndex ; i <= lastFullyCopiedElementIndex ; i++) {
				long originalValue = Array.getLong(originalArray, i);
				long copiedValue = Array.getLong(copyArray, i);
				if (originalValue != copiedValue) {
					throw new Error("Copy array element is not equivalent to the original, expected: " + originalValue + " got: " + copiedValue + ", i=" + i);
				}
			}
			if (0 <= lastFullyCopiedElementIndex) {
				int firstTrailingUnsetElementIndex = (endAligned ? lastFullyCopiedElementIndex + 1 : lastFullyCopiedElementIndex + 2);
				for (int i = firstTrailingUnsetElementIndex ; i < Array.getLength(copyArray) ; i++) {
					long unsetValue = Array.getLong(copyArray, i);
					if ((long)0 != unsetValue) {
						throw new Error("Uninitialized array element has been overwritten, got: " + unsetValue 
								+ ", i=" + i
								+ " endAligned: " + endAligned
								+ " lastFullyCopiedElementIndex: " + lastFullyCopiedElementIndex
								+ " numBytes: " + numBytes
								+ " offset: " + offset);		
					}
				}
			}
		}
	}
	
	private void testCopyArrayMemoryImpl(Object srcArray) {
		long baseOffset = myUnsafe.arrayBaseOffset(srcArray.getClass());
		long indexScale = myUnsafe.arrayIndexScale(srcArray.getClass());
		final int maxNumElements = Array.getLength(srcArray);
		final long maxNumBytesInSrcArray = maxNumElements * indexScale;
		
		for (long offset = baseOffset ; offset < (baseOffset + maxNumBytesInSrcArray) ; offset ++) {
			final long maxNumBytesToCopy = maxNumBytesInSrcArray - (offset - baseOffset);
			for (long numBytes = 1 ; numBytes <= (maxNumBytesToCopy) ; numBytes++) {
				Object destArray = Array.newInstance(srcArray.getClass().getComponentType(), maxNumElements);
				copyMemory(srcArray, offset, destArray, offset, numBytes);
				testEqualArray(srcArray, destArray, offset, numBytes);
			}
		}
	}
	
	public void testCopyBooleanArray() {
		boolean[] srcBooleanArray = {true, true, false, false, true, false, true, false};
		testCopyArrayMemoryImpl(srcBooleanArray);
	}
	
	public void testCopyByteArray() {
		byte[] srcByteArray = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
		testCopyArrayMemoryImpl(srcByteArray);
	}
	
	public void testCopyShortArray() {
		short[] srcShortArray = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
		testCopyArrayMemoryImpl(srcShortArray);
	}
	

	public void testCopyIntArray() {
		int[] srcIntArray = {-128, -333, -44, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
		testCopyArrayMemoryImpl(srcIntArray);
	}
	
	public void testCopyLongArray() {
		long[] srcLongArray = {-65000, -128, -333, -44, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 65000, 1234567};
		testCopyArrayMemoryImpl(srcLongArray);
	}
	
	private void testEqualRawMemoryAndArray(long memoryPointer, long numBytes, Object array, long arrayOffset) {
		long indexScale = myUnsafe.arrayIndexScale(array.getClass());
		long baseArrayOffset = myUnsafe.arrayBaseOffset(array.getClass());
		long currentArrayOffset = arrayOffset - baseArrayOffset;
		
		for (int currentOffset = 0 ; currentOffset < numBytes ; currentOffset++) {
			byte rawByte = myUnsafe.getByte(memoryPointer + currentOffset);
			int arrayIndex = (int)(currentArrayOffset / indexScale);
			long arrayValue = Array.getLong(array, arrayIndex);
			byte arrayByte = byteInWord(arrayValue, (int)indexScale, currentArrayOffset);
			if ( arrayByte != rawByte ) {
				throw new Error("Raw memory and corresponding array element are not equivalent, raw memory: " + rawByte 
						+ " arrayByte: " + arrayByte 
						+ " currentOffset: " + currentOffset
						+ " currentArrayOffset: " + currentArrayOffset
						+ " arrayIndex:" + arrayIndex);
			}
			
			currentArrayOffset += 1;
		}	
	}
	
	public void testCopyRawMemoryIntoSmallArray(Class arrayClass) {
		final long maxNumBytes = 64;
		long memoryAddress = myUnsafe.allocateMemory(maxNumBytes);
		
		if (0 == memoryAddress) {
			throw new Error("Unable to allocate memory for test");
		}

		try {
			for (int i = 0 ; i < maxNumBytes ; i++) {
				myUnsafe.putByte(memoryAddress + i, (byte)(i % Byte.SIZE));	
			}

			for (long memoryOffset = memoryAddress ; memoryOffset < (memoryAddress + maxNumBytes) ; memoryOffset++) {
				long maxNumBytesLeft = ((memoryAddress + maxNumBytes) - memoryOffset) ;
				for (long numBytesToCopy = 0 ; numBytesToCopy < maxNumBytesLeft ; numBytesToCopy++) {
					long indexScale = myUnsafe.arrayIndexScale(arrayClass);
					int arraySize = (int)(maxNumBytes / indexScale) + 2;
					Object destinationArray = Array.newInstance(arrayClass.getComponentType(), arraySize);

					long baseArrayOffset = myUnsafe.arrayBaseOffset(arrayClass);
					long arrayOffset = baseArrayOffset + (memoryOffset - memoryAddress);

					copyMemory(null, memoryOffset, destinationArray, arrayOffset, numBytesToCopy);
					testEqualRawMemoryAndArray(memoryOffset, numBytesToCopy, destinationArray, arrayOffset);
				}
			} 
		} finally {
			myUnsafe.freeMemory(memoryAddress);
		}
		
	}
	
	public void testCopyRawMemoryIntoLargeArray(Class arrayClass) {
		final long maxNumBytes = 2*1024*1024;
		long memoryAddress = myUnsafe.allocateMemory(maxNumBytes);
		
		if (0 == memoryAddress) {
			throw new Error("Unable to allocate memory for test");
		}

		try {
			for (int i = 0 ; i < (maxNumBytes / 8) ; i++) {
				myUnsafe.putLong(memoryAddress + i, i);
			}

			for (long memoryOffset = memoryAddress + (32 * 1024) ; memoryOffset < (memoryAddress + maxNumBytes) ; memoryOffset = memoryOffset * 11 - 1) {
				long maxNumBytesLeft = ((memoryAddress + maxNumBytes) - memoryOffset) ;
				for (long numBytesToCopy = 1 ; numBytesToCopy < maxNumBytesLeft ; numBytesToCopy = (numBytesToCopy + 1) * numBytesToCopy) {
					long indexScale = myUnsafe.arrayIndexScale(arrayClass);
					int arraySize = (int)(maxNumBytes / indexScale) + 2;
					Object destinationArray = Array.newInstance(arrayClass.getComponentType(), arraySize);

					long baseArrayOffset = myUnsafe.arrayBaseOffset(arrayClass);
					long arrayOffset = baseArrayOffset + (memoryOffset - memoryAddress);
					
					copyMemory(null, memoryOffset, destinationArray, arrayOffset, numBytesToCopy);
					testEqualRawMemoryAndArray(memoryOffset, numBytesToCopy, destinationArray, arrayOffset);
				}
			}
		} finally {
			myUnsafe.freeMemory(memoryAddress);
		}
	}
	
	private void testCopySmallArrayIntoRawMemory(Class arrayClass) {
		final long maxNumBytes = 64;
		final long indexScale = myUnsafe.arrayIndexScale(arrayClass);
		final long baseOffset = myUnsafe.arrayBaseOffset(arrayClass);
		final int maxNumElements = (int)(maxNumBytes / indexScale);

		Object array = Array.newInstance(arrayClass.getComponentType(), maxNumElements);

		for (int i = 0 ; i < maxNumElements ; i++) {
			Array.setByte(array, i, (byte)(i % Byte.SIZE));	
		}

		for (long arrayOffset = baseOffset ; arrayOffset < (baseOffset + maxNumBytes) ; arrayOffset++) {
			long maxNumBytesLeft = ((baseOffset + maxNumBytes) - arrayOffset) ;
			for (long numBytesToCopy = 0 ; numBytesToCopy < maxNumBytesLeft ; numBytesToCopy++) {
				long memoryPointer = myUnsafe.allocateMemory(maxNumBytes);
				if (0 == memoryPointer) {
					throw new Error("Unable to allocate memory for test");
				}
				try {
					long memoryOffset = memoryPointer + (arrayOffset - baseOffset);
					copyMemory(array, arrayOffset, null, memoryOffset, numBytesToCopy);
					testEqualRawMemoryAndArray(memoryOffset, numBytesToCopy, array, arrayOffset);
				} finally {
					myUnsafe.freeMemory(memoryPointer);
				}
			}
		} 
	}
	
	public void testCopyLargeArrayIntoRawMemory(Class arrayClass) {
		final long maxNumBytes = 2 * 1024 * 1024;
		final long indexScale = myUnsafe.arrayIndexScale(arrayClass);
		final long baseOffset = myUnsafe.arrayBaseOffset(arrayClass);
		final int maxNumElements = (int)(maxNumBytes / indexScale);

		Object array = Array.newInstance(arrayClass.getComponentType(), maxNumElements);

		for (int i = 0 ; i < maxNumElements ; i += 7) {
			Array.setByte(array, i, (byte)(i % Byte.SIZE));	
		}

		/*
			For off-heap eanbled case initial arrayOffset would be 0 (baseOffset=0),
			cause the next arrayOffset in loop become to negative (arrayOffset*11-1),
			update logic for the next arrayOffset to avoid negative offset test case.
		*/
		for (long arrayOffset = baseOffset; arrayOffset < (baseOffset + maxNumBytes); arrayOffset = ((arrayOffset==0) ? 16 : arrayOffset) * 11 - 1 ) {
			long maxNumBytesLeft = ((baseOffset + maxNumBytes) - arrayOffset) ;
			for (long numBytesToCopy = 1 ; numBytesToCopy < maxNumBytesLeft ; numBytesToCopy = (numBytesToCopy + 1) * numBytesToCopy) {
				long memoryPointer = myUnsafe.allocateMemory(maxNumBytes);
				if (0 == memoryPointer) {
					throw new Error("Unable to allocate memory for test");
				}
				try {
					long memoryOffset = memoryPointer + (arrayOffset - baseOffset);
					copyMemory(array, arrayOffset, null, memoryOffset, numBytesToCopy);
					testEqualRawMemoryAndArray(memoryOffset, numBytesToCopy, array, arrayOffset);
				} finally {
					myUnsafe.freeMemory(memoryPointer);
				}
			}
		} 
	}
	
	public static void main(String[] args) throws Exception {
		myUnsafe = getUnsafeInstance();
		
		/* We want to test the Unsafe.copyMemory(Object,long,Object,long,long) helper. Since some class libraries do not contain this helper, look it up using reflect. */
		try {
			/* signature: public native void copyMemory(java.lang.Object srcBase, long srcOffset, java.lang.Object destBase, long destOffset, long bytes); */
			copyMemoryMethod = myUnsafe.getClass().getDeclaredMethod("copyMemory", new Class[] { Object.class, long.class, Object.class, long.class, long.class });
		} catch (NoSuchMethodException e) {
			System.out.println("Class library does not include sun.misc.Unsafe.copyMemory(java.lang.Object srcBase, long srcOffset, java.lang.Object destBase, long destOffset, long bytes) -- skipping test");
			return;
		}
		
		UnsafeCopyMemoryTest tester = new UnsafeCopyMemoryTest();
		System.out.println("testCopyBooleanArray();");
		tester.testCopyBooleanArray();
		System.out.println("testCopyByteArray();");
		tester.testCopyByteArray();
		System.out.println("testCopyShortArray();");
		tester.testCopyShortArray();
		System.out.println("testCopyIntArray();");
		tester.testCopyIntArray();
		System.out.println("testCopyLongArray();");
		tester.testCopyLongArray();
		
		System.out.println("testCopyRawMemoryIntoSmallArray(byte[].class)");
		tester.testCopyRawMemoryIntoSmallArray(byte[].class);
		System.out.println("testCopyRawMemoryIntoLargeArray(byte[].class)");
		tester.testCopyRawMemoryIntoLargeArray(byte[].class);
		System.out.println("testCopyRawMemoryIntoSmallArray(short[].class)");
		tester.testCopyRawMemoryIntoSmallArray(short[].class);
		System.out.println("testCopyRawMemoryIntoLargeArray(short[].class)");
		tester.testCopyRawMemoryIntoLargeArray(short[].class);
		System.out.println("testCopyRawMemoryIntoSmallArray(int[].class)");
		tester.testCopyRawMemoryIntoSmallArray(int[].class);
		System.out.println("testCopyRawMemoryIntoLargeArray(int[].class)");
		tester.testCopyRawMemoryIntoLargeArray(int[].class);
		System.out.println("testCopyRawMemoryIntoSmallArray(long[].class)");
		tester.testCopyRawMemoryIntoSmallArray(long[].class);
		System.out.println("testCopyRawMemoryIntoLargeArray(long[].class)");
		tester.testCopyRawMemoryIntoLargeArray(long[].class);
		
		System.out.println("testCopySmallArrayIntoRawMemory(byte[].class)");
		tester.testCopySmallArrayIntoRawMemory(byte[].class);
		System.out.println("testCopyLargeArrayIntoRawMemory(byte[].class)");
		tester.testCopyLargeArrayIntoRawMemory(byte[].class);
		System.out.println("testCopySmallArrayIntoRawMemory(short[].class)");
		tester.testCopySmallArrayIntoRawMemory(short[].class);
		System.out.println("testCopyLargeArrayIntoRawMemory(short[].class)");
		tester.testCopyLargeArrayIntoRawMemory(short[].class);
		System.out.println("testCopySmallArrayIntoRawMemory(int[].class)");
		tester.testCopySmallArrayIntoRawMemory(int[].class);
		System.out.println("testCopyLargeArrayIntoRawMemory(int[].class)");
		tester.testCopyLargeArrayIntoRawMemory(int[].class);
		System.out.println("testCopySmallArrayIntoRawMemory(long[].class)");
		tester.testCopySmallArrayIntoRawMemory(long[].class);
		System.out.println("testCopyLargeArrayIntoRawMemory(long[].class)");
		tester.testCopyLargeArrayIntoRawMemory(long[].class);
	}
	
}
