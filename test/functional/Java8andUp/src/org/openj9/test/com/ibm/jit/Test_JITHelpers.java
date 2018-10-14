package org.openj9.test.com.ibm.jit;

/*******************************************************************************
 * Copyright (c) 1998, 2018 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import org.testng.Assert;
import java.io.FileInputStream;
import java.io.InputStream;
import com.ibm.jit.JITHelpers;
import org.openj9.test.com.ibm.jit.Test_JITHelpersImpl;
import java.util.Arrays;
import java.lang.reflect.Field;


@Test(groups = { "level.sanity" })
public class Test_JITHelpers {

	/**
	 * @tests com.ibm.jit.JITHelpers#getSuperclass(java.lang.Class)
	 */

	public static void test_getSuperclass() {
		final Class[] classes = {FileInputStream.class, JITHelpers.class, int[].class, int.class, Runnable.class, Object.class, void.class};
		final Class[] expectedSuperclasses = {InputStream.class, Object.class, Object.class, null, null, null, null};

		for (int i = 0 ; i < classes.length ; i++) {

			Class superclass = Test_JITHelpersImpl.test_getSuperclassImpl(classes[i]);

			AssertJUnit.assertTrue( "The superclass returned by JITHelpers.getSuperclass() is not equal to the expected one.\n"
					+ "\tExpected superclass: " + expectedSuperclasses[i]
					+ "\n\tReturned superclass: " + superclass,
					(superclass == expectedSuperclasses[i]));
		}

	}

	private static final com.ibm.jit.JITHelpers helpers = getJITHelpers();
	private static com.ibm.jit.JITHelpers getJITHelpers() {

		try {
			Field f = com.ibm.jit.JITHelpers.class.getDeclaredField("helpers");
			f.setAccessible(true);
			return (com.ibm.jit.JITHelpers) f.get(null);
		} catch (IllegalAccessException e) {
			throw new RuntimeException(e);
		} catch (NoSuchFieldException e) {
			throw new RuntimeException(e);
		}
	}

	private static int[] edgecaseLengths = new int[]{0, 1, 4, 7, 8, 9, 15, 16, 17, 31, 32, 33};

	private static char[] lowercaseLatin1Char = {0x6162, 0x6364, 0x6566, 0x6768, 0x696a, 0x6b6c, 0x6d6e, 0x6f70, 0x7172, 0x7374, 0x7576, 0x7778, 0x797a,
		(char)0xe0e1, (char)0xe2e3, (char)0xe4e5, (char)0xe600};
	private static char[] uppercaseLatin1Char = {0x4142, 0x4344, 0x4546, 0x4748, 0x494a, 0x4b4c, 0x4d4e, 0x4f50, 0x5152, 0x5354, 0x5556, 0x5758, 0x595a,
		(char)0xc0c1, (char)0xc2c3, (char)0xc4c5, (char)0xc600};
	private static char[] lowercaseUTF16Char = {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71,
		0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5, 0xe6};
	private static char[] uppercaseUTF16Char = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51,
		0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6};

	private static byte[] lowercaseLatin1Byte = {0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71,
		0x72, 0x73, 0x74, 0x75, 0x76,0x77, 0x78, 0x79, 0x7a, (byte)0xe0, (byte)0xe1, (byte)0xe2, (byte)0xe3, (byte)0xe4, (byte)0xe5, (byte)0xe6};
	private static byte[] uppercaseLatin1Byte = {0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51,
		0x52, 0x53, 0x54, 0x55, 0x56,0x57, 0x58, 0x59, 0x5a, (byte)0xc0, (byte)0xc1, (byte)0xc2, (byte)0xc3, (byte)0xc4, (byte)0xc5,(byte)0xc6};
	private static byte[] lowercaseUTF16BigEndianByte = {0x00, 0x61, 0x0, 0x62, 0x0, 0x63, 0x0, 0x64, 0x0, 0x65, 0x0, 0x66, 0x0, 0x67, 0x0, 0x68, 0x0,
		0x69, 0x0, 0x6a, 0x0, 0x6b, 0x0, 0x6c, 0x0, 0x6d, 0x0, 0x6e, 0x0, 0x6f, 0x0, 0x70, 0x0, 0x71, 0x0, 0x72, 0x0, 0x73, 0x0, 0x74, 0x0,
		0x75, 0x0, 0x76, 0x0, 0x77, 0x0, 0x78, 0x0, 0x79, 0x0, 0x7a, 0x0, (byte)0xe0, 0x0, (byte)0xe1, 0x0, (byte)0xe2, 0x0, (byte)0xe3,
		0x0, (byte)0xe4, 0x0, (byte)0xe5, 0x0, (byte)0xe6};
	private static byte[] uppercaseUTF16BigEndianByte = {0x0, 0x41, 0x0, 0x42, 0x0, 0x43, 0x0, 0x44, 0x0, 0x45, 0x0, 0x46, 0x0, 0x47, 0x0, 0x48, 0x0,
		0x49, 0x0, 0x4a, 0x0, 0x4b, 0x0,0x4c, 0x0, 0x4d, 0x0, 0x4e, 0x0, 0x4f, 0x0, 0x50, 0x0, 0x51, 0x0, 0x52, 0x0, 0x53, 0x0, 0x54, 0x0, 0x55, 0x0,
		0x56, 0x0, 0x57, 0x0, 0x58, 0x0, 0x59, 0x0, 0x5a, 0x0, (byte)0xc0, 0x0, (byte)0xc1, 0x0, (byte)0xc2, 0x0, (byte)0xc3, 0x0, (byte)0xc4, 0x0,
		(byte)0xc5, 0x0, (byte)0xc6};
	private static byte[] lowercaseUTF16LittleEndianByte = {0x61, 0x0, 0x62, 0x0, 0x63, 0x0, 0x64, 0x0, 0x65, 0x0, 0x66, 0x0, 0x67, 0x0, 0x68, 0x0, 0x69,
		0x0, 0x6a, 0x0, 0x6b, 0x0, 0x6c, 0x0, 0x6d, 0x0, 0x6e, 0x0, 0x6f, 0x0, 0x70, 0x0, 0x71, 0x0, 0x72, 0x0, 0x73, 0x0, 0x74, 0x0, 0x75, 0x0,
		0x76, 0x0, 0x77, 0x0, 0x78, 0x0, 0x79, 0x0, 0x7a, 0x0, (byte)0xe0, 0x0, (byte)0xe1, 0x0, (byte)0xe2, 0x0, (byte)0xe3, 0x0, (byte)0xe4,
		0x0, (byte)0xe5, 0x0, (byte)0xe6, 0x0};
	private static byte[] uppercaseUTF16LittleEndianByte = {0x41, 0x0, 0x42, 0x0, 0x43, 0x0, 0x44, 0x0, 0x45, 0x0, 0x46, 0x0, 0x47, 0x0, 0x48, 0x0, 0x49, 0x0,
		0x4a, 0x0, 0x4b, 0x0, 0x4c, 0x0, 0x4d, 0x0, 0x4e, 0x0, 0x4f, 0x0, 0x50, 0x0, 0x51, 0x0, 0x52, 0x0, 0x53, 0x0, 0x54, 0x0, 0x55, 0x0, 0x56, 0x0,
		0x57, 0x0, 0x58, 0x0, 0x59, 0x0, 0x5a, 0x0, (byte)0xc0, 0x0, (byte)0xc1, 0x0, (byte)0xc2, 0x0, (byte)0xc3, 0x0, (byte)0xc4, 0x0, (byte)0xc5,
		0x0, (byte)0xc6, 0x0};

	private static int[] indexes = new int[]{0, 1, 4, 7, 8, 9, 15, 16, 17, 31, 32};

	/**
	 * @tests com.ibm.jit.JITHelpers#(char[], char[], int)
	 */
	public static void test_toUpperIntrinsicUTF16() {
		char[] buffer;

		for (int j : edgecaseLengths){
			buffer = new char[j];
			if (helpers.toUpperIntrinsicUTF16(Arrays.copyOfRange(lowercaseUTF16Char, 0, j), buffer, j * 2)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(uppercaseUTF16Char, 0, j)),
					"UTF16 JITHelper upper case conversion with char arrays of " + j + " letters failed");
			}
			buffer.getClass();
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#toUpperIntrinsicUTF16(byte[], byte[], int)
	 */
	public static void test_toUpperIntrinsicUTF16two() {
		byte[] buffer;

		for (int j : edgecaseLengths) {
			buffer = new byte[j * 2];
			if (helpers.toUpperIntrinsicUTF16(Arrays.copyOfRange(lowercaseUTF16BigEndianByte, 0, j * 2), buffer, j * 2)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(uppercaseUTF16BigEndianByte, 0, j * 2)),
					"UTF16 JITHelper upper case conversion with big endian byte arrays of " + j + " letters failed");
			}
			buffer.getClass();

			if (helpers.toUpperIntrinsicUTF16(Arrays.copyOfRange(lowercaseUTF16LittleEndianByte, 0, j * 2), buffer, j * 2)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(uppercaseUTF16LittleEndianByte, 0, j * 2)),
					"UTF16 JITHelper upper case conversion with little endian byte arrays of " + j + " letters failed");
			}
			buffer.getClass();
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#toLowerIntrinsicUTF16(char[], char[], int)
	 */
	public static void test_toLowerIntrinsicUTF16() {
		char[] buffer;

		for (int j : edgecaseLengths) {
			buffer = new char[j];
			if (helpers.toLowerIntrinsicUTF16(Arrays.copyOfRange(uppercaseUTF16Char, 0, j), buffer, j * 2)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(lowercaseUTF16Char, 0, j)),
					"UTF16 JITHelper lower case conversion with byte arrays of " + j + " letters failed");
			}
			buffer.getClass();
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#toLowerIntrinsicUTF16(byte[], byte[], int)
	 */
	public static void test_toLowerIntrinsicUTF16two() {
		byte[] buffer;

		for (int j : edgecaseLengths) {
			buffer = new byte[j * 2];
			if (helpers.toLowerIntrinsicUTF16(Arrays.copyOfRange(uppercaseUTF16BigEndianByte, 0, j * 2), buffer, j * 2)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(lowercaseUTF16BigEndianByte, 0, j * 2)),
					"UTF16 JITHelper lower case conversion with big endian byte arrays of " + j + " letters failed");
			}
			buffer.getClass();

			if (helpers.toLowerIntrinsicUTF16(Arrays.copyOfRange(uppercaseUTF16LittleEndianByte, 0, j * 2), buffer, j * 2)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(lowercaseUTF16LittleEndianByte, 0, j * 2)),
					"UTF16 JITHelper lower case conversion with little endian byte arrays of " + j + " letters failed");
			}
			buffer.getClass();
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#toLowerIntrinsicLatin1(char[], char[], int)
	 */
	public static void test_toUpperIntrinsicLatin1() {
		char[] buffer;
		char[] source, converted;

		for (int j : edgecaseLengths) {
			source = Arrays.copyOfRange(lowercaseLatin1Char, 0, (j + 1) / 2);
			converted = Arrays.copyOfRange(uppercaseLatin1Char, 0, (j + 1) / 2);

			if (j % 2 == 1) {
				source[source.length - 1] &= 0xff00;
				converted[source.length - 1] &= 0xff00;
			}

			buffer = new char[(j + 1) / 2];
			if (helpers.toUpperIntrinsicLatin1(source, buffer, j)) {
				Assert.assertTrue(Arrays.equals(buffer, converted),
					"Latin 1 JITHelper upper case conversion with char arrays of " + j + " letters failed");
			}
			buffer.getClass();
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#toUpperIntrinsicLatin1(byte[], byte[], int)
	 */
	public static void test_toUpperIntrinsicLatin1two() {
		byte[] buffer;

		for (int j : edgecaseLengths) {
			buffer = new byte[j];
			if (helpers.toUpperIntrinsicLatin1(Arrays.copyOfRange(lowercaseLatin1Byte, 0, j), buffer, j)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(uppercaseLatin1Byte, 0, j)),
					"Latin 1 JITHelper upper case conversion with byte arrays of " + j + " letters failed");
			}
			buffer.getClass();
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#toLowerIntrinsicLatin1(char[], char[], int)
	 */
	public static void test_toLowerIntrinsicLatin1() {
		char[] buffer;
		char[] source, converted;

		for (int j : edgecaseLengths) {
			source = Arrays.copyOfRange(uppercaseLatin1Char, 0, (j + 1) / 2);
			converted = Arrays.copyOfRange(lowercaseLatin1Char, 0, (j + 1) / 2);

			if (j % 2 == 1) {
				source[source.length - 1] &= 0xff00;
				converted[source.length - 1] &= 0xff00;
			}

			buffer = new char[(j + 1) / 2];
			if (helpers.toLowerIntrinsicLatin1(source, buffer, j)) {
				Assert.assertTrue(Arrays.equals(buffer, converted),
					"Latin 1 JITHelper lower case conversion with char arrays of " + j + " letters failed");
			}
			buffer.getClass();
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#toLowerIntrinsicLatin1(byte[], byte[], int)
	 */
	public static void test_toLowerIntrinsicLatin1two() {
		byte[] buffer;

		for (int j : edgecaseLengths) {
			buffer = new byte[j];
			if (helpers.toLowerIntrinsicLatin1(Arrays.copyOfRange(uppercaseLatin1Byte, 0, j), buffer, j)) {
				Assert.assertTrue(Arrays.equals(buffer, Arrays.copyOfRange(lowercaseLatin1Byte, 0, j)),
					"Latin 1 JITHelper lower case conversion with byte arrays of " + j + " letters failed");
			}
			buffer.getClass();
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#intrinsicIndexOfLatin1(Object, byte, int, int)
	 */
	public static void test_intrinsicIndexOfLatin1() {
		byte lowerLetter;
		byte upperLetter;
		int lowerResult;
		int upperResult;

		for (int i : indexes) {
			for (int j : indexes) {
				lowerLetter = lowercaseLatin1Byte[j];
				lowerResult = helpers.intrinsicIndexOfLatin1(lowercaseLatin1Byte, lowerLetter, i, lowercaseLatin1Byte.length);

				if (j >= i) {
					Assert.assertEquals(lowerResult, j, "intrinsicIndexOfLatin1 returned incorrect result on a char array of lowercase letters");
				} else {
					Assert.assertEquals(lowerResult, -1, "intrinsicIndexOfLatin1 returned incorrect result on a char array of lowercase letters");
				}

				upperLetter = uppercaseLatin1Byte[j];
				upperResult = helpers.intrinsicIndexOfLatin1(uppercaseLatin1Byte, upperLetter, i, uppercaseLatin1Byte.length);

				if (j >= i) {
					Assert.assertEquals(upperResult, j, "intrinsicIndexOfLatin1 returned incorrect result on a char array of uppercase letters");
				} else {
					Assert.assertEquals(upperResult, -1, "intrinsicIndexOfLatin1 returned incorrect result on a char array of uppercase letters");
				}
			}
		}

		Assert.assertEquals(helpers.intrinsicIndexOfLatin1(lowercaseLatin1Byte, (byte)0x00, 0, lowercaseLatin1Byte.length), -1,
		    "intrinsicIndexOfLatin1 return incorrect result when passed a null character");
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#intrinsicIndexOfUTF16(Object, char, int, int)
	 */
	public static void test_intrinsicIndexOfUTF16() {
		char lowerLetter;
		char upperLetter;
		int lowerResult;
		int upperResult;

		for (int i : indexes) {
			for (int j : indexes) {
				lowerLetter = lowercaseUTF16Char[j];
				lowerResult = helpers.intrinsicIndexOfUTF16(lowercaseUTF16Char, lowerLetter, i, lowercaseUTF16Char.length);

				if (j >= i) {
					Assert.assertEquals(lowerResult, j, "intrinsicIndexOfUTF16 returned incorrect result on a char array of lowercase letters");
				} else {
					Assert.assertEquals(lowerResult, -1, "intrinsicIndexOfUTF16 returned incorrect result on a char array of lowercase letters");
				}

				upperLetter = uppercaseUTF16Char[j];
				upperResult = helpers.intrinsicIndexOfUTF16(uppercaseUTF16Char, upperLetter, i, uppercaseUTF16Char.length);

				if (j >= i) {
					Assert.assertEquals(upperResult, j, "intrinsicIndexOfUTF16 returned incorrect result on a char array of uppercase letters");
				} else {
					Assert.assertEquals(upperResult, -1, "intrinsicIndexOfUTF16 returned incorrect result on a char array of uppercase letters");
				}
			}
		}

		Assert.assertEquals(helpers.intrinsicIndexOfUTF16(lowercaseUTF16Char, (char)0x0000, 0, lowercaseUTF16Char.length), -1,
		    "intrinsicIndexOfUTF16 return incorrect result when passed a null character");
	}
}
