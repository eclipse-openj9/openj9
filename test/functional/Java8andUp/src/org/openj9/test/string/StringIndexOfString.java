/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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
package org.openj9.test.string;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.reflect.Field;
import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.util.List;

/**
 * Designed to test SIMD accelerated String.indexOf(String) on platforms such as
 * z/Architecture. The vector register size is assumed to be 16 bytes.
 */
@Test(groups = { "level.sanity" })
public class StringIndexOfString {

	public static final Logger logger = Logger.getLogger(StringIndexOfString.class);

	public static String shortString200Char = "Canada ic *a zzthe xxwwthi xx country located in ...zzthe xxwwthi xx northern part of North Americann. Its ten provinces and three territories extend from the Atlantic to the e xxwwthi xx northern part of North America. Its te Pacific and northward into the Arctic Ocean, covering 9.98 million square kilometres (3.85 million square miles), making it the world's second-largest country by total area. \";\r\n";
	private static final int NUM_TEST_CASE = 19;
	private static final int VECTOR_SIZE = 16;

	private static String searchString;
	private static String[] inputStrings;
	private static int[] expectedResults;

	/**
	 * Initialize an array of sub-strings with different offsets and alignments.
	 */
	static {
		searchString = shortString200Char;
		inputStrings = new String[NUM_TEST_CASE];
		expectedResults = new int[NUM_TEST_CASE];

		int subStringByteIndex = 0;
		int subStringByteLen = (VECTOR_SIZE - 4);;

		for (int testCaseNum = 1; testCaseNum <= NUM_TEST_CASE; ++testCaseNum) {
			switch (testCaseNum) {
			case 1:
				// sub-string head is 16B aligned, and its length < VECTOR_SIZE.
				subStringByteIndex = 0;
				subStringByteLen = (VECTOR_SIZE - 4);
				break;
			case 2:
				// sub-string head is not 16B aligned, and its length < VECTOR_SIZE.
				subStringByteIndex = 2;
				subStringByteLen = (VECTOR_SIZE - 6);
				break;
			case 3:
				// sub-string head not 16B aligned but is aligned at the end. s2Len < vector
				// size.
				subStringByteIndex = 4;
				subStringByteLen = (VECTOR_SIZE - 4);
				break;
			case 4:
				// sub-sting head is not 16B aligned, and its length < VECTOR_SIZE, and crosses
				// a 16B boundary.
				subStringByteIndex = 8;
				subStringByteLen = (VECTOR_SIZE - 4);
				break;
			case 5:
				// sub-string head is 16B aligned at the second 16B boundary. Its length <
				// VECTOR_SIZE.
				subStringByteIndex = VECTOR_SIZE;
				subStringByteLen = (VECTOR_SIZE - 2);
				break;
			case 6:
				// sub-string head and tail are not aligned to 16B boundary. Its length <
				// VECTOR_SIZE.
				subStringByteIndex = (VECTOR_SIZE + 2);
				subStringByteLen = (VECTOR_SIZE - 4);
				break;
			case 7:
				// This case has 1st-char-collision (1st char of sub-string appears multiple 
				// times before the sub-string index).
				// sub-string crosses the 4th 16B boundary. sub-string length <= VECTOR_SIZE.
				subStringByteIndex = (VECTOR_SIZE * 3 + 12);
				subStringByteLen = (VECTOR_SIZE);
				break;
			case 8:
				// Similar to case 6. The whole sub-string is between the 5th and 6th 16B
				// boundary.
				subStringByteIndex = (VECTOR_SIZE * 4 + 18);
				subStringByteLen = (VECTOR_SIZE - 4);
				break;
			case 9:
				// sub-string head aligned to byte 0 of source string. Length = VECTOR_SIZE
				subStringByteIndex = 0;
				subStringByteLen = VECTOR_SIZE;
				break;
			case 10:
				// sub-string head aligned to byte 0 of source string. Length = VECTOR_SIZE +
				// 1-char
				subStringByteIndex = 0;
				subStringByteLen = (VECTOR_SIZE + 2);
				break;
			case 11:
				// sub-string head aligned to byte 0 of source string. Length = VECTOR_SIZE x 2
				subStringByteIndex = 0;
				subStringByteLen = VECTOR_SIZE * 2;
				break;
			case 12:
				// sub-string head aligned to byte 0 of source string. Length = VECTOR_SIZE x 2 +
				// 4B
				subStringByteIndex = 0;
				subStringByteLen = VECTOR_SIZE * 2 + 4;
				break;

			case 13:
				// 1st char collision
				// Sub-string is 3*VECTOR_SIZE long, spanning between the 4th and 7th 16B
				// boundary.
				subStringByteIndex = VECTOR_SIZE * 3;
				subStringByteLen = VECTOR_SIZE * 3;
				break;
			case 14:
				// s2 header collision.
				// sub-string length > 3*VECTOR_SIZE long, starting from the 8th boundary and
				// goes slightly pass the 11th boundary.
				subStringByteIndex = (VECTOR_SIZE * 7);
				subStringByteLen = (VECTOR_SIZE * 3 + 6);
				break;
			case 15:
				// s2 header collision.
				// Sub-string length > 6*VECTOR_SIZE long; its head and tail are not aligned to
				// boundaries.
				subStringByteIndex = (VECTOR_SIZE * 7 + 8);
				subStringByteLen = (VECTOR_SIZE * 6 + 6);
				break;
			case 16:
				// s2 header match but body mismatch.
				subStringByteIndex = 350;
				subStringByteLen = 102;
				break;
			case 17:
				// Same as case 16, Sub-string to be modified.
				// Test case for string not found. Mismatch at the beginning.
				subStringByteIndex = 350;
				subStringByteLen = 102;
				break;
			case 18:
				// Same as case 15, sub-string to be modified.
				// Test case for string not found. Mismatch in the middle.
				subStringByteIndex = (VECTOR_SIZE * 7 + 8);
				subStringByteLen = (VECTOR_SIZE * 6 + 6);
				break;
			case 19:
				// Same as case 15, sub-string to be modified.
				// Test case for string not found. mismatch at the end
				subStringByteIndex = (VECTOR_SIZE * 7 + 8);
				subStringByteLen = (VECTOR_SIZE * 6 + 6);
				break;
			default:
				AssertJUnit.assertTrue("Unexpected testcase number in test initialization", false);
			}

			subStringByteIndex /= 2;
			subStringByteLen /= 2;

			String subString = searchString.substring(subStringByteIndex, subStringByteIndex + subStringByteLen);
			int expectedResult = subStringByteIndex;

			if (testCaseNum == 17) {
				subString = "::" + subString;
				expectedResult = -1;
			} else if (testCaseNum == 18) {
				int splicePoint = (int) (subString.length() * 0.6);
				String sub1 = subString.substring(0, splicePoint - 1);
				String sub2 = subString.substring(splicePoint + 2, subString.length() - 1);
				subString = sub1 + "::" + sub2;
				expectedResult = -1;
			} else if (testCaseNum == 19) {
				subString += "::";
				expectedResult = -1;
			}

			expectedResults[testCaseNum - 1] = expectedResult;
			inputStrings[testCaseNum - 1] = subString;
		}
	}

	@Test
	public void testStringIndexOfString() {
		// Warm up loop
		for (int iter = 0; iter < 10; iter++) {
			for (int testcaseNum = 1; testcaseNum <= NUM_TEST_CASE; ++testcaseNum) {
				doTest(testcaseNum, inputStrings[testcaseNum - 1], false);
			}
		}

		for (int testcaseNum = 1; testcaseNum <= NUM_TEST_CASE; ++testcaseNum) {
			doTest(testcaseNum, inputStrings[testcaseNum - 1], true);
		}
	}

	private void doTest(int testcase, String x, boolean checkResult) {

		int index = searchString.indexOf(x);

		if (checkResult) {
			AssertJUnit.assertEquals("compare indexOf() result to expected result", index,
					expectedResults[testcase - 1]);
		}

		return;
	}
}
