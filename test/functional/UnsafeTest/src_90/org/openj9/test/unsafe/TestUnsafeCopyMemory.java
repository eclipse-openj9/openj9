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
package org.openj9.test.unsafe;

import java.lang.reflect.Array;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.ByteOrder;

import org.testng.AssertJUnit;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import jdk.internal.misc.Unsafe;

@Test(groups = { "level.sanity" })
public class TestUnsafeCopyMemory extends UnsafeTestBase {
	static Unsafe myUnsafe;
	static Method copyMemoryMethod;
	static final ByteOrder byteOrder = ByteOrder.nativeOrder();
	private static Logger logger = Logger.getLogger(TestUnsafeCopyMemory.class);
	
	public TestUnsafeCopyMemory(String scenario) {
		super(scenario);
	}
	
	/* get logger to use, for child classes to report with their class name instead of UnsafeTestBase*/
	@Override
	protected Logger getLogger() {
		return logger;
	}

	private byte[] modelByte2 = new byte[] { 1, 2, 3, 4, Byte.MAX_VALUE,
			Byte.MIN_VALUE, 0, -1, -1, -2, -3, -4 };
	private char[] modelChar2 = new char[] { Character.MAX_HIGH_SURROGATE,
			Character.MIN_LOW_SURROGATE, 1, 2, Character.MAX_VALUE,
			Character.MAX_VALUE, (char) -1 };
	private short[] modelShort2 = new short[] { Short.MAX_VALUE,
			Short.MAX_VALUE, 0, -1, -1, Short.MIN_VALUE, 1, 2 };
	private int[] modelInt2 = new int[] { Integer.MAX_VALUE, Integer.MIN_VALUE,
			0, -1, -1, 1, 2 };
	private long[] modelLong2 = new long[] { Long.MAX_VALUE, Long.MIN_VALUE, 0,
			-1, -1, -1 };
	private float[] modelFloat2 = new float[] { 2.0f, Float.MAX_VALUE,
			Float.MIN_VALUE, 0.0f, -1f, -1f, 1.0f };
	private double[] modelDouble2 = new double[] { Double.MAX_VALUE,
			Double.MIN_VALUE, 0.0, -1, -1, 1.0 };
	private boolean[] modelBoolean2 = new boolean[] { true, true, true, false, false };

	public void testCopyRawMemoryIntoSmallByteArray() {
		testCopyRawMemoryIntoSmallArray(byte[].class);
	}

	public void testCopyRawMemoryIntoLargeByteArray() {
		testCopyRawMemoryIntoLargeArray(byte[].class);
	}

	public void testCopyRawMemoryIntoSmallShortArray() {
		testCopyRawMemoryIntoSmallArray(short[].class);
	}

	public void testCopyRawMemoryIntoLargeShortArray() {
		testCopyRawMemoryIntoLargeArray(short[].class);
	}

	public void testCopyRawMemoryIntoSmallIntArray() {
		testCopyRawMemoryIntoSmallArray(int[].class);
	}

	public void testCopyRawMemoryIntoLargeIntArray() {
		testCopyRawMemoryIntoLargeArray(int[].class);
	}

	public void testCopyRawMemoryIntoSmallLongArray() {
		testCopyRawMemoryIntoSmallArray(long[].class);
	}

	public void testCopyRawMemoryIntoLargeLongArray() {
		testCopyRawMemoryIntoLargeArray(long[].class);
	}

	public void testCopySmallByteArrayIntoRawMemory() {
		testCopySmallArrayIntoRawMemory(byte[].class);
	}

	public void testCopyLargeByteArrayIntoRawMemory() {
		testCopyLargeArrayIntoRawMemory(byte[].class);
	}

	public void testCopySmallShortArrayIntoRawMemory() {
		testCopySmallArrayIntoRawMemory(short[].class);
	}

	public void testCopyLargeShortArrayIntoRawMemory() {
		testCopyLargeArrayIntoRawMemory(short[].class);
	}

	public void testCopySmallIntArrayIntoRawMemory() {
		testCopySmallArrayIntoRawMemory(int[].class);
	}

	public void testCopyLargeIntArrayIntoRawMemory() {
		testCopyLargeArrayIntoRawMemory(int[].class);
	}

	public void testCopySmallLongArrayIntoRawMemory() {
		testCopySmallArrayIntoRawMemory(long[].class);
	}

	public void testCopyLargeLongArrayIntoRawMemory() {
		testCopyLargeArrayIntoRawMemory(long[].class);
	}

	public void testCopyNativeByteArray() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelByte.length; i++) {
			myUnsafe.putByte(address + i, modelByte[i]);
		}
		testCopyArrayMemoryNative(modelByte, address, true);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeByteArray2() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelByte.length; i++) {
			myUnsafe.putByte(address + i, modelByte[i]);
		}
		testCopyArrayMemoryNative(modelByte, address, false);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeIntArray() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelInt.length; i++) {
			myUnsafe.putInt(address + (i * 4), modelInt[i]);
		}
		testCopyArrayMemoryNative(modelInt, address, true);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeIntArray2() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelInt.length; i++) {
			myUnsafe.putInt(address + (i * 4), modelInt[i]);
		}
		testCopyArrayMemoryNative(modelInt, address, false);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeLongArray() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelLong.length; i++) {
			myUnsafe.putLong(address + (i * 8), modelLong[i]);
		}
		testCopyArrayMemoryNative(modelLong, address, true);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeLongArray2() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelLong.length; i++) {
			myUnsafe.putLong(address + (i * 8), modelLong[i]);
		}
		testCopyArrayMemoryNative(modelLong, address, false);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeCharArray() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelChar.length; i++) {
			myUnsafe.putChar(address + (i * 2), modelChar[i]);
		}
		testCopyArrayMemoryNative(modelChar, address, true);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeCharArray2() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelChar.length; i++) {
			myUnsafe.putChar(address + (i * 2), modelChar[i]);
		}
		testCopyArrayMemoryNative(modelChar, address, false);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeDoubleArray() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelDouble.length; i++) {
			myUnsafe.putDouble(address + (i * 8), modelDouble[i]);
		}
		testCopyArrayMemoryNative(modelDouble, address, true);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeDoubleArray2() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelDouble.length; i++) {
			myUnsafe.putDouble(address + (i * 8), modelDouble[i]);
		}
		testCopyArrayMemoryNative(modelDouble, address, false);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeBooleanArray() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelBoolean.length; i++) {
			myUnsafe.putBoolean(null, address + i, modelBoolean[i]);
		}
		testCopyArrayMemoryNative(modelBoolean, address, true);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeBooleanArray2() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelBoolean.length; i++) {
			myUnsafe.putBoolean(null, address + i, modelBoolean[i]);
		}
		testCopyArrayMemoryNative(modelBoolean, address, false);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeFloatArray() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelFloat.length; i++) {
			myUnsafe.putFloat(address + (i * 4), modelFloat[i]);
		}
		testCopyArrayMemoryNative(modelFloat, address, true);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeFloatArray2() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelFloat.length; i++) {
			myUnsafe.putFloat(address + (i * 4), modelFloat[i]);
		}
		testCopyArrayMemoryNative(modelFloat, address, false);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeShortArray() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelShort.length; i++) {
			myUnsafe.putShort(address + (i * 2), modelShort[i]);
		}
		testCopyArrayMemoryNative(modelShort, address, true);
		myUnsafe.freeMemory(address);
	}

	public void testCopyNativeShortArray2() {
		long address = myUnsafe.allocateMemory(100);
		for (int i = 0; i < modelShort.length; i++) {
			myUnsafe.putShort(address + (i * 2), modelShort[i]);
		}
		testCopyArrayMemoryNative(modelShort, address, false);
		myUnsafe.freeMemory(address);
	}

	public void testOverlappingCase() {
		long indexScale = myUnsafe.arrayIndexScale(modelByte.getClass());
		final int maxNumElements = Array.getLength(modelByte);
		final long maxNumBytesInSrcArray = maxNumElements * indexScale;
		for (int j = 0; j < maxNumBytesInSrcArray; j++) {
			long address = myUnsafe.allocateMemory(100);
			for (int i = 0; i < modelByte.length; i++) {
				myUnsafe.putByte(address + i, modelByte[i]);
			}
			testOverlapping(modelByte, address, address + j,
					maxNumBytesInSrcArray, true);
			myUnsafe.freeMemory(address);
		}
	}

	public void testOverlappingCase2() {
		long indexScale = myUnsafe.arrayIndexScale(modelByte.getClass());
		final int maxNumElements = Array.getLength(modelByte);
		final long maxNumBytesInSrcArray = maxNumElements * indexScale;
		for (int j = 0; j < maxNumBytesInSrcArray; j++) {
			long address = myUnsafe.allocateMemory(100);
			for (int i = 0; i < modelByte.length; i++) {
				myUnsafe.putByte(address + i, modelByte[i]);
			}
			testOverlapping(modelByte, address, address + j,
					maxNumBytesInSrcArray, false);
			myUnsafe.freeMemory(address);
		}
	}

	public void testOverlappingCase3() {
		long indexScale = myUnsafe.arrayIndexScale(modelByte.getClass());
		final int maxNumElements = Array.getLength(modelByte);
		final long maxNumBytesInSrcArray = maxNumElements * indexScale;
		for (int j = 0; j < maxNumBytesInSrcArray; j++) {
			long address = myUnsafe.allocateMemory(100);
			long startingAddress = address + 100 - maxNumElements;
			for (int i = 0; i < modelByte.length; i++) {
				myUnsafe.putByte(startingAddress + i, modelByte[i]);
			}
			testOverlapping(modelByte, startingAddress, startingAddress - j,
					maxNumBytesInSrcArray, true);
			myUnsafe.freeMemory(address);
		}
	}

	public void testOverlappingCase4() {
		long indexScale = myUnsafe.arrayIndexScale(modelByte.getClass());
		final int maxNumElements = Array.getLength(modelByte);
		final long maxNumBytesInSrcArray = maxNumElements * indexScale;
		for (int j = 0; j < maxNumBytesInSrcArray; j++) {
			long address = myUnsafe.allocateMemory(100);
			long startingAddress = address + 100 - maxNumElements;
			for (int i = 0; i < modelByte.length; i++) {
				myUnsafe.putByte(startingAddress + i, modelByte[i]);
			}
			testOverlapping(modelByte, startingAddress, startingAddress - j,
					maxNumBytesInSrcArray, false);
			myUnsafe.freeMemory(address);
		}
	}

	@Override
	@BeforeMethod
	protected void setUp() throws Exception {
		myUnsafe = getUnsafeInstance();
		/*
		 * We want to test the Unsafe.copyMemory(Object,long,Object,long,long)
		 * helper. Since some class libraries do not contain this helper, look
		 * it up using reflect.
		 */
		try {
			/*
			 * signature: public native void copyMemory(java.lang.Object
			 * srcBase, long srcOffset, java.lang.Object destBase, long
			 * destOffset, long bytes);
			 */
			copyMemoryMethod = myUnsafe.getClass().getDeclaredMethod(
					"copyMemory",
					new Class[] { Object.class, long.class, Object.class,
							long.class, long.class });
		} catch (NoSuchMethodException e) {
			logger.error("Class library does not include sun.misc.Unsafe.copyMemory(java.lang.Object srcBase, long srcOffset, java.lang.Object destBase, long destOffset, long bytes) -- skipping test", e);
			return;
		}
	}

	private static void copyMemory(java.lang.Object srcBase, long srcOffset,
			java.lang.Object destBase, long destOffset, long bytes) {
		try {
			copyMemoryMethod.invoke(myUnsafe, new Object[] { srcBase,
					srcOffset, destBase, destOffset, bytes });
		} catch (IllegalArgumentException e) {
			throw new Error("Reflect exception.", e);
		} catch (IllegalAccessException e) {
			throw new Error("Reflect exception.", e);
		} catch (InvocationTargetException e) {
			throw new Error("Reflect exception.", e);
		}
	}

	private byte byteInWord(long value, int bytesInValue, long offset) {
		int shiftAmount = (int) (offset % bytesInValue) * 8;
		if (byteOrder == ByteOrder.BIG_ENDIAN) {
			/*
			 * Least significant byte on BIG_ENDIAN is on the left therefore
			 * requiring largest shiftAmount
			 */
			shiftAmount = ((bytesInValue - 1) * 8) - shiftAmount;
		}
		value >>= shiftAmount;
		return (byte) value;
	}

	private void testUnalignedBeginningAndEnd(Object originalArray,
			Object copyArray, long offset, long numBytes,
			boolean beginningAligned, boolean endAligned) {
		long baseOffset = myUnsafe.arrayBaseOffset(originalArray.getClass());
		long indexScale = myUnsafe.arrayIndexScale(originalArray.getClass());
		long numBytesBetweenBaseAndOffset = offset - baseOffset;

		if (!beginningAligned) {
			int index = (int) (numBytesBetweenBaseAndOffset / indexScale);
			long copiedValue = Array.getLong(copyArray, index);
			long originalValue = Array.getLong(originalArray, index);

			int expectedNumUnsetBytesInCopiedValue = (int) (numBytesBetweenBaseAndOffset % indexScale);
			for (int i = 0; i < expectedNumUnsetBytesInCopiedValue; i++) {
				byte copiedByte = byteInWord(copiedValue, (int) indexScale, i);
				if ((byte) 0 != copiedByte) {
					throw new Error(
							"Unset byte was overwritten in destination array, value: "
									+ copiedByte + ", i=" + i);
				}
			}

			int offsetOfLastSetByteInFirstWord = Math.min((int) indexScale,
					(int) (expectedNumUnsetBytesInCopiedValue + numBytes));

			for (int i = expectedNumUnsetBytesInCopiedValue; i < offsetOfLastSetByteInFirstWord; i++) {
				byte originalByte = byteInWord(originalValue, (int) indexScale,
						i);
				byte copiedByte = byteInWord(copiedValue, (int) indexScale, i);
				if (originalByte != copiedByte) {
					throw new Error(
							"Copied value not equivalent to original, expected: "
									+ originalByte + " got: " + copiedByte
									+ ", i=" + i
									+ " expectedNumUnsetBytesInCopiedValue: "
									+ expectedNumUnsetBytesInCopiedValue
									+ " offset: " + offset + " numBytes: "
									+ numBytes);
				}
			}
		}

		if (!endAligned) {
			int index = (int) ((numBytesBetweenBaseAndOffset + numBytes) / indexScale);

			long copiedValue = Array.getLong(copyArray, index);
			long originalValue = Array.getLong(originalArray, index);
			int expectedNumTrailingUnsetBytes = (int) indexScale
					- (int) ((numBytesBetweenBaseAndOffset + numBytes) % indexScale);

			int offsetOfFirstSetByteInLastWord = 0;
			if (numBytes < indexScale) {
				offsetOfFirstSetByteInLastWord = (int) (offset % indexScale);
			}

			for (int offsetInWord = offsetOfFirstSetByteInLastWord; offsetInWord < (indexScale - expectedNumTrailingUnsetBytes); offsetInWord++) {
				byte originalByte = byteInWord(originalValue, (int) indexScale,
						offsetInWord);
				byte copiedByte = byteInWord(copiedValue, (int) indexScale,
						offsetInWord);
				if (originalByte != copiedByte) {
					throw new Error(
							"Copied value not equivalent to original, expected: "
									+ originalByte + " got: " + copiedByte
									+ ", offsetInWord=" + offsetInWord
									+ " expectedNumTrailingUnsetBytes: "
									+ expectedNumTrailingUnsetBytes
									+ " offset: " + offset + " numBytes: "
									+ numBytes + " index: " + index);
				}
			}

			for (int offsetInWord = (int) (indexScale - expectedNumTrailingUnsetBytes); offsetInWord < indexScale; offsetInWord++) {
				byte copiedByte = byteInWord(copiedValue, (int) indexScale,
						offsetInWord);
				if ((byte) 0 != copiedByte) {
					throw new Error(
							"Unset byte was overwritten in destination array, value: "
									+ copiedByte + ", offsetInWord="
									+ offsetInWord + " indexScale: "
									+ indexScale
									+ " expectedNumTrailingUnsetBytes: "
									+ expectedNumTrailingUnsetBytes
									+ " offset: " + offset + " numBytes: "
									+ numBytes + " index: " + index
									+ " numBytesBetweenBaseAndOffset: "
									+ numBytesBetweenBaseAndOffset);
				}
			}
		}
	}

	private void testEqualArray(Object originalArray, Object copyArray,
			long offset, long numBytes) {
		long baseOffset = myUnsafe.arrayBaseOffset(originalArray.getClass());
		long indexScale = myUnsafe.arrayIndexScale(originalArray.getClass());
		long numBytesBetweenBaseAndOffset = offset - baseOffset;
		int firstFullyCopiedElementIndex = (int) (numBytesBetweenBaseAndOffset / indexScale);
		int lastFullyCopiedElementIndex = (int) ((numBytesBetweenBaseAndOffset + numBytes) / indexScale) - 1;
		boolean beginningAligned = true;
		boolean endAligned = true;

		if (1 < indexScale) {
			/*
			 * if the indexScale is greater than 1 then the copied memory may
			 * not have been aligned with the number of bytes per element in the
			 * array
			 */
			beginningAligned = (0 == (numBytesBetweenBaseAndOffset % indexScale));
			endAligned = (0 == ((numBytesBetweenBaseAndOffset + numBytes) % indexScale));

			if (!beginningAligned) {
				firstFullyCopiedElementIndex += 1;
			}

			if (!beginningAligned || !endAligned) {
				testUnalignedBeginningAndEnd(originalArray, copyArray, offset,
						numBytes, beginningAligned, endAligned);
			}
		}

		if (originalArray instanceof boolean[]) {
			for (int i = 0; i < firstFullyCopiedElementIndex; i++) {
				if (false != ((boolean[]) copyArray)[i]) {
					throw new Error(
							"Uninitialized array element has been overwritten, got: "
									+ ((boolean[]) copyArray)[i] + ", i=" + i);
				}
			}
			for (int i = firstFullyCopiedElementIndex; i <= lastFullyCopiedElementIndex; i++) {
				if (((boolean[]) originalArray)[i] != ((boolean[]) copyArray)[i]) {
					throw new Error(
							"Copy array element is not equivalent to the original, expected: "
									+ ((boolean[]) originalArray)[i] + " got: "
									+ ((boolean[]) copyArray)[i] + ", i=" + i);
				}
			}
			for (int i = lastFullyCopiedElementIndex + 1; i < Array
					.getLength(copyArray); i++) {
				if (false != ((boolean[]) copyArray)[i]) {
					throw new Error(
							"Uninitialized array element has been overwritten, got: "
									+ ((boolean[]) copyArray)[i] + ", i=" + i);
				}
			}
		} else {
			int lastLeadingUnsetElementIndex = beginningAligned ? firstFullyCopiedElementIndex
					: firstFullyCopiedElementIndex - 1;
			for (int i = 0; i < lastLeadingUnsetElementIndex; i++) {
				long unsetValue = Array.getLong(copyArray, i);
				if ((long) 0 != unsetValue) {
					throw new Error(
							"Uninitialized array element has been overwritten, got: "
									+ unsetValue + ", i=" + i);
				}
			}

			for (int i = firstFullyCopiedElementIndex; i <= lastFullyCopiedElementIndex; i++) {
				long originalValue = Array.getLong(originalArray, i);
				long copiedValue = Array.getLong(copyArray, i);
				if (originalValue != copiedValue) {
					throw new Error(
							"Copy array element is not equivalent to the original, expected: "
									+ originalValue + " got: " + copiedValue
									+ ", i=" + i);
				}
			}
			if (0 <= lastFullyCopiedElementIndex) {
				int firstTrailingUnsetElementIndex = (endAligned ? lastFullyCopiedElementIndex + 1
						: lastFullyCopiedElementIndex + 2);
				for (int i = firstTrailingUnsetElementIndex; i < Array
						.getLength(copyArray); i++) {
					long unsetValue = Array.getLong(copyArray, i);
					if ((long) 0 != unsetValue) {
						throw new Error(
								"Uninitialized array element has been overwritten, got: "
										+ unsetValue + ", i=" + i
										+ " endAligned: " + endAligned
										+ " lastFullyCopiedElementIndex: "
										+ lastFullyCopiedElementIndex
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

		for (long offset = baseOffset; offset < (baseOffset + maxNumBytesInSrcArray); offset++) {
			final long maxNumBytesToCopy = maxNumBytesInSrcArray
					- (offset - baseOffset);
			for (long numBytes = 1; numBytes <= (maxNumBytesToCopy); numBytes++) {
				Object destArray = Array.newInstance(srcArray.getClass()
						.getComponentType(), maxNumElements);
				copyMemory(srcArray, offset, destArray, offset, numBytes);
				testEqualArray(srcArray, destArray, offset, numBytes);
			}
		}
	}

	private void testCopyArrayMemoryNative(Object srcArray, long address,
			boolean testCopyMemoryObjNull) {
		long indexScale = myUnsafe.arrayIndexScale(srcArray.getClass());
		final int maxNumElements = Array.getLength(srcArray);
		final long maxNumBytesInSrcArray = maxNumElements * indexScale;
		long count = 0;

		for (long offset = address; offset < (address + maxNumBytesInSrcArray); offset = offset
				+ indexScale) {
			final long maxNumBytesToCopy = maxNumBytesInSrcArray
					- (offset - address);
			for (long numBytes = 0; numBytes <= (maxNumBytesToCopy); numBytes = numBytes
					+ indexScale) {
				long newAddress = createArray(srcArray);
				if (testCopyMemoryObjNull) {
					logger.debug("call myUnsafe.copyMemory(null, "+ offset + ", null, "+ newAddress + count +", " + numBytes + ")");
					myUnsafe.copyMemory(null, offset, null, newAddress + count,	numBytes);
				} else {
					logger.debug("call myUnsafe.copyMemory(" + offset + ", "+ newAddress + count +", " + numBytes + ")");
					myUnsafe.copyMemory(offset, newAddress + count, numBytes);
				}
				if (srcArray instanceof byte[]) {
					for (int i = 0; i < modelByte2.length; i++) {
						byte value = myUnsafe.getByte(newAddress
								+ (i * indexScale));
						
						if (i < count / indexScale
								|| i >= count / indexScale + numBytes
										/ indexScale) {
							logger.debug("index: " + i + " Expected: " +modelByte2[(int) i]+ " unchanged value: " + value);
							AssertJUnit.assertEquals(modelByte2[(int) i], value);
						} else {
							logger.debug("index: " + i + " Expected: " +modelByte[(int) i]+ " copied value: " + value);
							AssertJUnit.assertEquals(modelByte[(int) i], value);
						}
					}
				} else if (srcArray instanceof int[]) {
					for (int i = 0; i < modelInt2.length; i++) {
						int value = myUnsafe.getInt(newAddress
								+ (i * indexScale));
						if (i < count / indexScale
								|| i >= count / indexScale + numBytes
										/ indexScale) {
							logger.debug("index: " + i + " Expected: " +modelInt2[(int) i]+ " unchanged value: " + value);
							AssertJUnit.assertEquals(modelInt2[(int) i], value);
						} else {
							logger.debug("index: " + i + " Expected: " +modelInt[(int) i]+ " copied value: " + value);
							AssertJUnit.assertEquals(modelInt[(int) i], value);
						}
					}
				} else if (srcArray instanceof long[]) {
					for (int i = 0; i < modelLong2.length; i++) {
						long value = myUnsafe.getLong(newAddress
								+ (i * indexScale));
						logger.debug("index of int array: " + i + "value: " + value);
						if (i < count / indexScale
								|| i >= count / indexScale + numBytes
										/ indexScale) {
							logger.debug("index: " + i + " Expected: " +modelLong2[(int) i]+ " unchanged value: " + value);
							AssertJUnit.assertEquals(modelLong2[(int) i], value);
						} else {
							logger.debug("index: " + i + " Expected: " +modelLong[(int) i]+ " copied value: " + value);
							AssertJUnit.assertEquals(modelLong[(int) i], value);
						}
					}
				} else if (srcArray instanceof char[]) {
					for (int i = 0; i < modelChar2.length; i++) {
						long value = myUnsafe.getChar(newAddress
								+ (i * indexScale));
						if (i < count / indexScale
								|| i >= count / indexScale + numBytes
										/ indexScale) {
							logger.debug("index: " + i + " Expected: " +modelChar2[(int) i]+ " unchanged value: " + value);
							AssertJUnit.assertEquals(modelChar2[(int) i], value);
						} else {
							logger.debug("index: " + i + " Expected: " +modelChar[(int) i]+ " copied value: " + value);
							AssertJUnit.assertEquals(modelChar[(int) i], value);
						}
					}
				} else if (srcArray instanceof double[]) {
					for (int i = 0; i < modelDouble2.length; i++) {
						double value = myUnsafe.getDouble(newAddress
								+ (i * indexScale));
						if (i < count / indexScale
								|| i >= count / indexScale + numBytes
										/ indexScale) {
							logger.debug("index: " + i + " Expected: " +modelDouble2[(int) i]+ " unchanged value: " + value);
							AssertJUnit.assertEquals(modelDouble2[(int) i], value);
						} else {
							logger.debug("index: " + i + " Expected: " +modelDouble[(int) i]+ " copied value: " + value);
							AssertJUnit.assertEquals(modelDouble[(int) i], value);
						}
					}
				} else if (srcArray instanceof boolean[]) {
					for (int i = 0; i < modelBoolean2.length; i++) {
						boolean value = myUnsafe.getBoolean(null, newAddress
								+ (i * indexScale));
						if (i < count / indexScale
								|| i >= count / indexScale + numBytes
										/ indexScale) {
							logger.debug("index: " + i + " Expected: " +modelBoolean2[(int) i]+ " unchanged value: " + value);
							AssertJUnit.assertEquals(modelBoolean2[(int) i], value);
						} else {
							logger.debug("index: " + i + " Expected: " +modelBoolean[(int) i]+ " copied value: " + value);
							AssertJUnit.assertEquals(modelBoolean[(int) i], value);
						}
					}
				} else if (srcArray instanceof float[]) {
					for (int i = 0; i < modelFloat2.length; i++) {
						float value = myUnsafe.getFloat(newAddress
								+ (i * indexScale));
						if (i < count / indexScale
								|| i >= count / indexScale + numBytes
										/ indexScale) {
							logger.debug("index: " + i + " Expected: " +modelFloat2[(int) i]+ " unchanged value: " + value);
							AssertJUnit.assertEquals(modelFloat2[(int) i], value);
						} else {
							logger.debug("index: " + i + " Expected: " +modelFloat[(int) i]+ " copied value: " + value);
							AssertJUnit.assertEquals(modelFloat[(int) i], value);
						}
					}
				} else if (srcArray instanceof short[]) {
					for (int i = 0; i < modelShort2.length; i++) {
						long value = myUnsafe.getShort(newAddress
								+ (i * indexScale));
						if (i < count / indexScale
								|| i >= count / indexScale + numBytes
										/ indexScale) {
							logger.debug("index: " + i + " Expected: " +modelShort2[(int) i]+ " unchanged value: " + value);
							AssertJUnit.assertEquals(modelShort2[(int) i], value);
						} else {
							logger.debug("index: " + i + " Expected: " +modelShort[(int) i]+ " copied value: " + value);
							AssertJUnit.assertEquals(modelShort[(int) i], value);
						}
					}
				}
				myUnsafe.freeMemory(newAddress);
			}
			count = count + indexScale;
		}
	}

	private long createArray(Object obj) {
		long address = myUnsafe.allocateMemory(100);
		if (obj instanceof byte[]) {
			for (int i = 0; i < modelByte2.length; i++) {
				myUnsafe.putByte(address + i, modelByte2[i]);
			}
		} else if (obj instanceof int[]) {
			for (int i = 0; i < modelInt2.length; i++) {
				myUnsafe.putInt(address + (i * 4), modelInt2[i]);
			}
		} else if (obj instanceof long[]) {
			for (int i = 0; i < modelLong2.length; i++) {
				myUnsafe.putLong(address + (i * 8), modelLong2[i]);
			}
		} else if (obj instanceof char[]) {
			for (int i = 0; i < modelChar2.length; i++) {
				myUnsafe.putChar(address + (i * 2), modelChar2[i]);
			}
		} else if (obj instanceof double[]) {
			for (int i = 0; i < modelDouble2.length; i++) {
				myUnsafe.putDouble(address + (i * 8), modelDouble2[i]);
			}
		} else if (obj instanceof boolean[]) {
			for (int i = 0; i < modelBoolean2.length; i++) {
				myUnsafe.putBoolean(null, address + i, modelBoolean2[i]);
			}
		} else if (obj instanceof float[]) {
			for (int i = 0; i < modelFloat2.length; i++) {
				myUnsafe.putFloat(address + (i * 4), modelFloat2[i]);
			}
		} else if (obj instanceof short[]) {
			for (int i = 0; i < modelShort2.length; i++) {
				myUnsafe.putShort(address + (i * 2), modelShort2[i]);
			}
		}
		return address;
	}

	private void testOverlapping(Object srcArray, long address,
			long newAddress, long maxNumBytesInSrcArray,
			boolean testCopyMemoryObjNull) {
		if (testCopyMemoryObjNull) {
			logger.debug("call myUnsafe.copyMemory(null, "+ address + ", null, "+ newAddress +", " + maxNumBytesInSrcArray + ")");
			myUnsafe.copyMemory(null, address, null, newAddress,
					maxNumBytesInSrcArray);
		} else {
			logger.debug("call myUnsafe.copyMemory("+ address + ", "+ newAddress +", " + maxNumBytesInSrcArray + ")");
			myUnsafe.copyMemory(address, newAddress, maxNumBytesInSrcArray);
		}
		for (int j = 0; j < maxNumBytesInSrcArray; j++) {
			byte value = myUnsafe.getByte(newAddress + j);
			logger.debug("index: " + j + " Expected: " + modelByte[j] + " value: "
					+ value);
			AssertJUnit.assertEquals(modelByte[j], value);
		}
	}

	public void testCopyBooleanArray() {
		boolean[] srcBooleanArray = { true, true, false, false, true, false,
				true, false };
		testCopyArrayMemoryImpl(srcBooleanArray);
	}

	public void testCopyByteArray() {
		byte[] srcByteArray = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
		testCopyArrayMemoryImpl(srcByteArray);
	}

	public void testCopyShortArray() {
		short[] srcShortArray = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13 };
		testCopyArrayMemoryImpl(srcShortArray);
	}

	public void testCopyIntArray() {
		int[] srcIntArray = { -128, -333, -44, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8,
				9, 10, 11, 12, 13 };
		testCopyArrayMemoryImpl(srcIntArray);
	}

	public void testCopyLongArray() {
		long[] srcLongArray = { -65000, -128, -333, -44, -1, 0, 1, 2, 3, 4, 5,
				6, 7, 8, 9, 10, 11, 12, 13, 65000, 1234567 };
		testCopyArrayMemoryImpl(srcLongArray);
	}

	private void testEqualRawMemoryAndArray(long memoryPointer, long numBytes,
			Object array, long arrayOffset) {
		long indexScale = myUnsafe.arrayIndexScale(array.getClass());
		long baseArrayOffset = myUnsafe.arrayBaseOffset(array.getClass());
		long currentArrayOffset = arrayOffset - baseArrayOffset;

		for (int currentOffset = 0; currentOffset < numBytes; currentOffset++) {
			byte rawByte = myUnsafe.getByte(memoryPointer + currentOffset);
			int arrayIndex = (int) (currentArrayOffset / indexScale);
			long arrayValue = Array.getLong(array, arrayIndex);
			byte arrayByte = byteInWord(arrayValue, (int) indexScale,
					currentArrayOffset);
			if (arrayByte != rawByte) {
				throw new Error(
						"Raw memory and corresponding array element are not equivalent, raw memory: "
								+ rawByte + " arrayByte: " + arrayByte
								+ " currentOffset: " + currentOffset
								+ " currentArrayOffset: " + currentArrayOffset
								+ " arrayIndex:" + arrayIndex);
			}

			currentArrayOffset += 1;
		}
	}

	private void testCopyRawMemoryIntoSmallArray(Class arrayClass) {
		final long maxNumBytes = 64;
		long memoryAddress = myUnsafe.allocateMemory(maxNumBytes);

		if (0 == memoryAddress) {
			throw new Error("Unable to allocate memory for test");
		}

		try {
			for (int i = 0; i < maxNumBytes; i++) {
				myUnsafe.putByte(memoryAddress + i, (byte) (i % Byte.SIZE));
			}

			for (long memoryOffset = memoryAddress; memoryOffset < (memoryAddress + maxNumBytes); memoryOffset++) {
				long maxNumBytesLeft = ((memoryAddress + maxNumBytes) - memoryOffset);
				for (long numBytesToCopy = 0; numBytesToCopy < maxNumBytesLeft; numBytesToCopy++) {
					long indexScale = myUnsafe.arrayIndexScale(arrayClass);
					int arraySize = (int) (maxNumBytes / indexScale) + 2;
					Object destinationArray = Array.newInstance(
							arrayClass.getComponentType(), arraySize);

					long baseArrayOffset = myUnsafe.arrayBaseOffset(arrayClass);
					long arrayOffset = baseArrayOffset
							+ (memoryOffset - memoryAddress);

					copyMemory(null, memoryOffset, destinationArray,
							arrayOffset, numBytesToCopy);
					testEqualRawMemoryAndArray(memoryOffset, numBytesToCopy,
							destinationArray, arrayOffset);
				}
			}
		} finally {
			myUnsafe.freeMemory(memoryAddress);
		}

	}

	private void testCopyRawMemoryIntoLargeArray(Class arrayClass) {
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
				for (long numBytesToCopy = 1; numBytesToCopy < maxNumBytesLeft; numBytesToCopy = (numBytesToCopy + 1)
						* numBytesToCopy) {
					long indexScale = myUnsafe.arrayIndexScale(arrayClass);
					int arraySize = (int) (maxNumBytes / indexScale) + 2;
					Object destinationArray = Array.newInstance(
							arrayClass.getComponentType(), arraySize);

					long baseArrayOffset = myUnsafe.arrayBaseOffset(arrayClass);
					long arrayOffset = baseArrayOffset
							+ (memoryOffset - memoryAddress);

					copyMemory(null, memoryOffset, destinationArray,
							arrayOffset, numBytesToCopy);
					testEqualRawMemoryAndArray(memoryOffset, numBytesToCopy,
							destinationArray, arrayOffset);
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
		final int maxNumElements = (int) (maxNumBytes / indexScale);

		Object array = Array.newInstance(arrayClass.getComponentType(),
				maxNumElements);

		for (int i = 0; i < maxNumElements; i++) {
			Array.setByte(array, i, (byte) (i % Byte.SIZE));
		}

		for (long arrayOffset = baseOffset; arrayOffset < (baseOffset + maxNumBytes); arrayOffset++) {
			long maxNumBytesLeft = ((baseOffset + maxNumBytes) - arrayOffset);
			for (long numBytesToCopy = 0; numBytesToCopy < maxNumBytesLeft; numBytesToCopy++) {
				long memoryPointer = myUnsafe.allocateMemory(maxNumBytes);
				if (0 == memoryPointer) {
					throw new Error("Unable to allocate memory for test");
				}
				try {
					long memoryOffset = memoryPointer
							+ (arrayOffset - baseOffset);
					copyMemory(array, arrayOffset, null, memoryOffset,
							numBytesToCopy);
					testEqualRawMemoryAndArray(memoryOffset, numBytesToCopy,
							array, arrayOffset);
				} finally {
					myUnsafe.freeMemory(memoryPointer);
				}
			}
		}
	}

	private void testCopyLargeArrayIntoRawMemory(Class arrayClass) {
		final long maxNumBytes = 2 * 1024 * 1024;
		final long indexScale = myUnsafe.arrayIndexScale(arrayClass);
		final long baseOffset = myUnsafe.arrayBaseOffset(arrayClass);
		final int maxNumElements = (int) (maxNumBytes / indexScale);

		Object array = Array.newInstance(arrayClass.getComponentType(),
				maxNumElements);

		for (int i = 0; i < maxNumElements; i += 7) {
			Array.setByte(array, i, (byte) (i % Byte.SIZE));
		}

		/*
			For off-heap eanbled case initial arrayOffset would be 0 (baseOffset=0),
			cause the next arrayOffset in loop become to negative (arrayOffset*11-1),
			update logic for the next arrayOffset to avoid negative offset test case.
		*/
		for (long arrayOffset = baseOffset; arrayOffset < (baseOffset + maxNumBytes); arrayOffset = ((arrayOffset==0) ? 16 : arrayOffset) * 11 - 1 ) {
			long maxNumBytesLeft = ((baseOffset + maxNumBytes) - arrayOffset);
			for (long numBytesToCopy = 1; numBytesToCopy < maxNumBytesLeft; numBytesToCopy = (numBytesToCopy + 1)
					* numBytesToCopy) {
				long memoryPointer = myUnsafe.allocateMemory(maxNumBytes);
				if (0 == memoryPointer) {
					throw new Error("Unable to allocate memory for test");
				}
				try {
					long memoryOffset = memoryPointer
							+ (arrayOffset - baseOffset);
					copyMemory(array, arrayOffset, null, memoryOffset,
							numBytesToCopy);
					testEqualRawMemoryAndArray(memoryOffset, numBytesToCopy,
							array, arrayOffset);
				} finally {
					myUnsafe.freeMemory(memoryPointer);
				}
			}
		}
	}
}
