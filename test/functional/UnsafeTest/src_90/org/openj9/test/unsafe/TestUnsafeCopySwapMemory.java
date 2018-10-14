/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package org.openj9.test.unsafe;

import java.lang.reflect.Array;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import org.testng.AssertJUnit;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import jdk.internal.misc.Unsafe;

@Test(groups = { "level.sanity" })
public class TestUnsafeCopySwapMemory extends UnsafeTestBase {
	static Unsafe myUnsafe;
	static Method copySwapMemoryMethod;
	private static Logger logger = Logger.getLogger(TestUnsafeCopySwapMemory.class);
	
	public TestUnsafeCopySwapMemory(String scenario) {
		super(scenario);
	}
	
	/* get logger to use, for child classes to report with their class name instead of UnsafeTestBase */
	@Override
	protected Logger getLogger() {
		return logger;
	}

	private short[] modelShort2 = new short[] { Short.MAX_VALUE,
			Short.MAX_VALUE, 0, -1, -1, Short.MIN_VALUE, 1, 2 };
	private int[] modelInt2 = new int[] { Integer.MAX_VALUE, Integer.MIN_VALUE,
			0, -1, -1, 1, 2 };
	private long[] modelLong2 = new long[] { Long.MAX_VALUE, Long.MIN_VALUE, 0,
			-1, -1, -1 };

	public void testCopySwapRawMemoryIntoSmallShortArray() {
		testCopySwapRawMemoryIntoSmallArray(short[].class);
	}

	public void testCopySwapRawMemoryIntoLargeShortArray() {
		testCopySwapRawMemoryIntoLargeArray(short[].class);
	}

	public void testCopySwapRawMemoryIntoSmallIntArray() {
		testCopySwapRawMemoryIntoSmallArray(int[].class);
	}

	public void testCopySwapRawMemoryIntoLargeIntArray() {
		testCopySwapRawMemoryIntoLargeArray(int[].class);
	}

	public void testCopySwapRawMemoryIntoSmallLongArray() {
		testCopySwapRawMemoryIntoSmallArray(long[].class);
	}

	public void testCopySwapRawMemoryIntoLargeLongArray() {
		testCopySwapRawMemoryIntoLargeArray(long[].class);
	}

	public void testCopySwapSmallShortArrayIntoRawMemory() {
		testCopySwapSmallArrayIntoRawMemory(short[].class);
	}

	public void testCopySwapLargeShortArrayIntoRawMemory() {
		testCopySwapLargeArrayIntoRawMemory(short[].class);
	}

	public void testCopySwapSmallIntArrayIntoRawMemory() {
		testCopySwapSmallArrayIntoRawMemory(int[].class);
	}

	public void testCopySwapLargeIntArrayIntoRawMemory() {
		testCopySwapLargeArrayIntoRawMemory(int[].class);
	}

	public void testCopySwapSmallLongArrayIntoRawMemory() {
		testCopySwapSmallArrayIntoRawMemory(long[].class);
	}

	public void testCopySwapLargeLongArrayIntoRawMemory() {
		testCopySwapLargeArrayIntoRawMemory(long[].class);
	}

	public void testCopySwapNativeIntArray() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelInt.length; i++) {
			myUnsafe.putInt(address + (i * 4), modelInt[i]);
		}
		testCopySwapArrayMemoryNative(modelInt, address, true);
		myUnsafe.freeMemory(address);
	}

	public void testCopySwapNativeIntArray2() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelInt.length; i++) {
			myUnsafe.putInt(address + (i * 4), modelInt[i]);
		}
		testCopySwapArrayMemoryNative(modelInt, address, false);
		myUnsafe.freeMemory(address);
	}

	public void testCopySwapNativeLongArray() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelLong.length; i++) {
			myUnsafe.putLong(address + (i * 8), modelLong[i]);
		}
		testCopySwapArrayMemoryNative(modelLong, address, true);
		myUnsafe.freeMemory(address);
	}

	public void testCopySwapNativeLongArray2() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelLong.length; i++) {
			myUnsafe.putLong(address + (i * 8), modelLong[i]);
		}
		testCopySwapArrayMemoryNative(modelLong, address, false);
		myUnsafe.freeMemory(address);
	}

	public void testCopySwapNativeShortArray() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelShort.length; i++) {
			myUnsafe.putShort(address + (i * 2), modelShort[i]);
		}
		testCopySwapArrayMemoryNative(modelShort, address, true);
		myUnsafe.freeMemory(address);
	}

	public void testCopySwapNativeShortArray2() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelShort.length; i++) {
			myUnsafe.putShort(address + (i * 2), modelShort[i]);
		}
		testCopySwapArrayMemoryNative(modelShort, address, false);
		myUnsafe.freeMemory(address);
	}

	@Override
	@BeforeMethod
	protected void setUp() throws Exception {
		myUnsafe = getUnsafeInstance();
		/*
		 * We want to test the Unsafe.copySwapMemory(Object, long, Object, long, long, long)
		 * helper. Since some class libraries do not contain this helper, look
		 * it up using reflect.
		 */
		try {
			/*
			 * signature: public native void copySwapMemory(java.lang.Object
			 * srcBase, long srcOffset, java.lang.Object destBase, long
			 * destOffset, long copySize, long elementSize);
			 */
			copySwapMemoryMethod = myUnsafe.getClass().getDeclaredMethod(
					"copySwapMemory",
					new Class[] { Object.class, long.class, Object.class,
							long.class, long.class, long.class });
		} catch (NoSuchMethodException e) {
			logger.error("Class library does not include sun.misc.Unsafe.copySwapMemory(java.lang.Object srcBase, long srcOffset, java.lang.Object destBase, long destOffset, long copySize, long elementSize) -- skipping test", e);
			return;
		}
	}

	private static void copySwapMemory(java.lang.Object srcBase, long srcOffset,
			java.lang.Object destBase, long destOffset, long copySize, long elementSize) {
		try {
			copySwapMemoryMethod.invoke(myUnsafe, new Object[] { srcBase,
					srcOffset, destBase, destOffset, copySize, elementSize });
		} catch (IllegalArgumentException e) {
			throw new Error("Reflect exception.", e);
		} catch (IllegalAccessException e) {
			throw new Error("Reflect exception.", e);
		} catch (InvocationTargetException e) {
			throw new Error("Reflect exception.", e);
		}
	}

	private void testCopySwapArrayMemoryNative(Object srcArray, long address, boolean testCopyMemoryObjNull) {
		long indexScale = myUnsafe.arrayIndexScale(srcArray.getClass());
		final int maxNumElements = Array.getLength(srcArray);
		final long maxNumBytesInSrcArray = maxNumElements * indexScale; 

		for (long count = 0; count < maxNumBytesInSrcArray; count += indexScale) {
			long offset = address + count;
			final long maxNumBytesToCopy = maxNumBytesInSrcArray - count;
			
			for (long numBytes = 0; numBytes <= (maxNumBytesToCopy); numBytes += indexScale) {
				long newAddress = createArray(srcArray);
				
				if (testCopyMemoryObjNull) {
					logger.debug("call myUnsafe.copySwapMemory(null, "+ offset + ", null, "+ newAddress + count +", " + numBytes + ", " + indexScale + ")");
					myUnsafe.copySwapMemory(null, offset, null, newAddress + count,	numBytes, indexScale);
				} else {
					logger.debug("call myUnsafe.copySwapMemory(" + offset + ", "+ newAddress + count +", " + numBytes + ", " + indexScale + ")");
					myUnsafe.copySwapMemory(offset, newAddress + count, numBytes, indexScale);
				}
				
				/* verify swap was successful */
				if (srcArray instanceof int[]) {
					for (int i = 0; i < modelInt.length; i++) {
						int realValue = myUnsafe.getInt(newAddress + (i * indexScale));
						if (i < (count / indexScale) || i >= ((count / indexScale) + (numBytes / indexScale))) {
							logger.debug("index: " + i 
									+ " Expected: " + modelInt2[i]
									+ " unchanged value: " + realValue);
							AssertJUnit.assertEquals(modelInt2[i], realValue);
						} else {
							int reversedValue = Integer.reverseBytes(modelInt[i]);
							logger.debug("index: " + i 
									+ " Expected: " + modelInt[i]
									+ " Expected reversed: " + reversedValue
									+ " Copied value: " + realValue);
							AssertJUnit.assertEquals(reversedValue, realValue);
						}
					}
				} else if (srcArray instanceof long[]) {
					for (int i = 0; i < modelLong2.length; i++) {
						long realValue = myUnsafe.getLong(newAddress + (i * indexScale));
						if (i < (count / indexScale) || i >= ((count / indexScale) + (numBytes / indexScale))) {
							logger.debug("index: " + i + " Expected: " +modelLong2[(int) i]+ " unchanged value: " + realValue);
							AssertJUnit.assertEquals(modelLong2[(int) i], realValue);
						} else {
							long reversedValue = Long.reverseBytes(modelLong[i]);
							logger.debug("index: " + i 
									+ " Expected: " + modelLong[i]
									+ " Expected reversed: " + reversedValue
									+ " Copied value: " + realValue);
							AssertJUnit.assertEquals(reversedValue, realValue);
						}
					}
				} else if (srcArray instanceof short[]) {
					for (int i = 0; i < modelShort2.length; i++) {
						short realValue = myUnsafe.getShort(newAddress + (i * indexScale));
						if (i < (count / indexScale) || i >= ((count / indexScale) + (numBytes / indexScale))) {
							logger.debug("index: " + i + " Expected: " + modelShort2[(int) i]+ " unchanged value: " + realValue);
							AssertJUnit.assertEquals(modelShort2[(int) i], realValue);
						} else {
							short reversedValue = Short.reverseBytes(modelShort[i]);
							logger.debug("index: " + i 
									+ " Expected: " + modelShort[i]
									+ " Expected reversed: " + reversedValue
									+ " Copied value: " + realValue);
							AssertJUnit.assertEquals(reversedValue, realValue);
						}
					}
				}
				myUnsafe.freeMemory(newAddress);
			}
		}
	}

	private long createArray(Object obj) {
		long address = myUnsafe.allocateMemory(100);
		if (obj instanceof int[]) {
			for (int i = 0; i < modelInt2.length; i++) {
				myUnsafe.putInt(address + (i * 4), modelInt2[i]);
			}
		} else if (obj instanceof long[]) {
			for (int i = 0; i < modelLong2.length; i++) {
				myUnsafe.putLong(address + (i * 8), modelLong2[i]);
			}
		} else if (obj instanceof short[]) {
			for (int i = 0; i < modelShort2.length; i++) {
				myUnsafe.putShort(address + (i * 2), modelShort2[i]);
			}
		}
		return address;
	}

	private void testSwapRawMemoryAndArray(long memoryPointer, long numBytes,
			Object array, long arrayOffset) {
		long indexScale = myUnsafe.arrayIndexScale(array.getClass());
		long baseArrayOffset = myUnsafe.arrayBaseOffset(array.getClass());
		long currentArrayOffset = arrayOffset - baseArrayOffset;

		for (int currentOffset = 0; currentOffset < numBytes; currentOffset += indexScale) {
			int arrayIndex = (((int)currentArrayOffset + currentOffset) / (int)indexScale);
			if (2 == indexScale) {
				short rawShort = myUnsafe.getShortUnaligned(null, memoryPointer  + currentOffset);
				short rawShortReversed = Short.reverseBytes(rawShort);
				short arrayShort = Array.getShort(array, arrayIndex);
				
				if (rawShortReversed != arrayShort) {
					logger.debug(
							"Raw memory and corresponding array element are not swapped correctly, raw memory: "
									+ rawShort + " raw memory swapped: " + rawShortReversed + " arrayShort: " + arrayShort
									+ " currentOffset: " + currentOffset
									+ " currentArrayOffset: " + currentArrayOffset
									+ " arrayIndex:" + arrayIndex);
					throw new Error(
							"Raw memory and corresponding array element are not swapped correctly, raw memory: "
									+ rawShort + " raw memory swapped: " + rawShortReversed + " arrayShort: " + arrayShort
									+ " currentOffset: " + currentOffset
									+ " currentArrayOffset: " + currentArrayOffset
									+ " arrayIndex:" + arrayIndex);
				}
			} else if (4 == indexScale) {
				int rawInt = myUnsafe.getIntUnaligned(null, memoryPointer  + currentOffset);
				int rawIntReversed = Integer.reverseBytes(rawInt);
				int arrayInt = Array.getInt(array, arrayIndex);
				
				if (rawIntReversed != arrayInt) {
					logger.debug(
							"Raw memory and corresponding array element are not swapped correctly, raw memory: "
									+ rawInt + " raw memory swapped: " + rawIntReversed + " arrayInt: " + arrayInt
									+ " currentOffset: " + currentOffset
									+ " currentArrayOffset: " + currentArrayOffset
									+ " arrayIndex:" + arrayIndex);
					throw new Error(
							"Raw memory and corresponding array element are not swapped correctly, raw memory: "
									+ rawInt + " raw memory swapped: " + rawIntReversed + " arrayInt: " + arrayInt
									+ " currentOffset: " + currentOffset
									+ " currentArrayOffset: " + currentArrayOffset
									+ " arrayIndex:" + arrayIndex);
				}
			} else { /* indexScale is 8 */
				long rawLong = myUnsafe.getLongUnaligned(null, memoryPointer  + currentOffset);
				long rawLongReversed = Long.reverseBytes(rawLong);
				long arrayLong = Array.getLong(array, arrayIndex);
				
				if (rawLongReversed != arrayLong) {
					logger.debug(
							"Raw memory and corresponding array element are not swapped correctly, raw memory: "
									+ rawLong + " raw memory swapped: " + rawLongReversed + " arrayLong: " + arrayLong
									+ " currentOffset: " + currentOffset
									+ " currentArrayOffset: " + currentArrayOffset
									+ " arrayIndex:" + arrayIndex);
					throw new Error(
							"Raw memory and corresponding array element are not swapped correctly, raw memory: "
									+ rawLong + " raw memory swapped: " + rawLongReversed + " arrayLong: " + arrayLong
									+ " currentOffset: " + currentOffset
									+ " currentArrayOffset: " + currentArrayOffset
									+ " arrayIndex:" + arrayIndex);
				}
			}
		}
	}

	private void testCopySwapRawMemoryIntoSmallArray(Class arrayClass) {
		final long maxNumBytes = 64;
		long memoryAddress = myUnsafe.allocateMemory(maxNumBytes);
		long indexScale = myUnsafe.arrayIndexScale(arrayClass);

		if (0 == memoryAddress) {
			throw new Error("Unable to allocate memory for test");
		}

		try {
			for (int i = 0; i < maxNumBytes; i++) {
				myUnsafe.putByte(memoryAddress + i, (byte) (i % Byte.SIZE));
			}

			for (long memoryOffset = memoryAddress; memoryOffset < (memoryAddress + maxNumBytes); memoryOffset += indexScale) {
				long maxNumBytesLeft = ((memoryAddress + maxNumBytes) - memoryOffset);
				for (long numBytesToCopy = 0; numBytesToCopy < maxNumBytesLeft; numBytesToCopy += indexScale) {
					int arraySize = (int) (maxNumBytes / indexScale) + 2;
					Object destinationArray = Array.newInstance(
							arrayClass.getComponentType(), arraySize);

					long baseArrayOffset = myUnsafe.arrayBaseOffset(arrayClass);
					long arrayOffset = baseArrayOffset
							+ (memoryOffset - memoryAddress);
					copySwapMemory(null, memoryOffset, destinationArray,
							arrayOffset, numBytesToCopy, indexScale);
					testSwapRawMemoryAndArray(memoryOffset, numBytesToCopy,
							destinationArray, arrayOffset);
				}
			}
		} finally {
			myUnsafe.freeMemory(memoryAddress);
		}

	}

	private void testCopySwapRawMemoryIntoLargeArray(Class arrayClass) {
		final long maxNumBytes = 2 * 1024 * 1024;
		long memoryAddress = myUnsafe.allocateMemory(maxNumBytes);

		if (0 == memoryAddress) {
			throw new Error("Unable to allocate memory for test");
		}

		try {
			for (int i = 0; i < (maxNumBytes / 8); i++) {
				myUnsafe.putLong(memoryAddress + i, i);
			}

			for (long memoryOffset = memoryAddress + (32 * 1024); memoryOffset < (memoryAddress + maxNumBytes); memoryOffset = memoryOffset * 11 - 1) {
				long maxNumBytesLeft = ((memoryAddress + maxNumBytes) - memoryOffset);
				long indexScale = myUnsafe.arrayIndexScale(arrayClass);
				for (long numBytesToCopy = indexScale; numBytesToCopy < maxNumBytesLeft; numBytesToCopy = (numBytesToCopy + indexScale)
						* numBytesToCopy) {
					int arraySize = (int) (maxNumBytes / indexScale) + 2;
					Object destinationArray = Array.newInstance(
							arrayClass.getComponentType(), arraySize);

					long baseArrayOffset = myUnsafe.arrayBaseOffset(arrayClass);
					long arrayOffset = baseArrayOffset
							+ (memoryOffset - memoryAddress);

					copySwapMemory(null, memoryOffset, destinationArray,
							arrayOffset, numBytesToCopy, indexScale);
					testSwapRawMemoryAndArray(memoryOffset, numBytesToCopy,
							destinationArray, arrayOffset);
				}
			}
		} finally {
			myUnsafe.freeMemory(memoryAddress);
		}
	}

	private void testCopySwapSmallArrayIntoRawMemory(Class arrayClass) {
		final long maxNumBytes = 64;
		final long indexScale = myUnsafe.arrayIndexScale(arrayClass);
		final long baseOffset = myUnsafe.arrayBaseOffset(arrayClass);
		final int maxNumElements = (int) (maxNumBytes / indexScale);

		Object array = Array.newInstance(arrayClass.getComponentType(),
				maxNumElements);

		for (int i = 0; i < maxNumElements; i++) {
			Array.setByte(array, i, (byte) (i % Byte.SIZE));
		}

		for (long arrayOffset = baseOffset; arrayOffset < (baseOffset + maxNumBytes); arrayOffset += indexScale) {
			long maxNumBytesLeft = ((baseOffset + maxNumBytes) - arrayOffset);
			for (long numBytesToCopy = 0; numBytesToCopy < maxNumBytesLeft; numBytesToCopy += indexScale) {
				long memoryPointer = myUnsafe.allocateMemory(maxNumBytes);
				if (0 == memoryPointer) {
					throw new Error("Unable to allocate memory for test");
				}
				try {
					long memoryOffset = memoryPointer
							+ (arrayOffset - baseOffset);
					copySwapMemory(array, arrayOffset, null, memoryOffset,
							numBytesToCopy, indexScale);
					testSwapRawMemoryAndArray(memoryOffset, numBytesToCopy,
							array, arrayOffset);
				} finally {
					myUnsafe.freeMemory(memoryPointer);
				}
			}
		}
	}

	private void testCopySwapLargeArrayIntoRawMemory(Class arrayClass) {
		final long maxNumBytes = 2 * 1024 * 1024;
		final long indexScale = myUnsafe.arrayIndexScale(arrayClass);
		final long baseOffset = myUnsafe.arrayBaseOffset(arrayClass);
		final int maxNumElements = (int) (maxNumBytes / indexScale);

		Object array = Array.newInstance(arrayClass.getComponentType(),
				maxNumElements);

		for (int i = 0; i < maxNumElements; i++) {
			Array.setByte(array, i, (byte) (i % Byte.SIZE));
		}

		for (long arrayOffset = baseOffset; arrayOffset < (baseOffset + maxNumBytes); arrayOffset = arrayOffset * 11) {
			long maxNumBytesLeft = ((baseOffset + maxNumBytes) - arrayOffset);
			for (long numBytesToCopy = indexScale; numBytesToCopy < maxNumBytesLeft; numBytesToCopy = (numBytesToCopy + indexScale) * numBytesToCopy) {
				long memoryPointer = myUnsafe.allocateMemory(maxNumBytes);
				if (0 == memoryPointer) {
					throw new Error("Unable to allocate memory for test");
				}
				try {
					long memoryOffset = memoryPointer
							+ (arrayOffset - baseOffset);
					/* print debugging statement */
					logger.debug("call myUnsafe.copySwapMemory( " + array.toString() + ", " 
							+ arrayOffset + ", null, " + memoryOffset + ", " 
							+ numBytesToCopy + ", "
							+ indexScale + ")");
					copySwapMemory(array, arrayOffset, null, memoryOffset,
							numBytesToCopy, indexScale);
					testSwapRawMemoryAndArray(memoryOffset, numBytesToCopy,
							array, arrayOffset);
				} finally {
					myUnsafe.freeMemory(memoryPointer);
				}
			}
		}
	}
}
