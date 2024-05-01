/*
 * Copyright IBM Corp. and others 2022
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

package jit.test.recognizedMethod;
import org.testng.AssertJUnit;
import org.testng.annotations.Test;
import java.io.UnsupportedEncodingException;

public class TestJavaLangStringCodingEncodeASCII {

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public void test_java_lang_StringCoding_encodeASCII() {
	char[] inputCharArray = null;
	byte[] expectedOutput = null;
	String inputString = null;
	String encoding = "ASCII";
	byte[] actualOutput = null;

	try
	{
		// warmup
		for (int i = 0; i < 100000; i++)
		{
			actualOutput = encoding.getBytes(encoding);
		}

		// Test exactly 16 Characters
		// i.e. output in string representation: @ABCDEFGHIJKLMNO
		inputCharArray = new char[]{ '\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F' };
		expectedOutput = new byte[] {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79};
		inputString = new String(inputCharArray);
		actualOutput = inputString.getBytes(encoding);
		AssertJUnit.assertArrayEquals("Incorrect result for 16 characters", expectedOutput, actualOutput);

		// Test less than 16 characters
		// i.e. output in string representation: @ABCDEFGHIJKLMN
		inputCharArray = new char[]{ '\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E' };
		expectedOutput = new byte[] {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78};
		inputString = new String(inputCharArray);
		actualOutput = inputString.getBytes(encoding);
		AssertJUnit.assertArrayEquals("Incorrect result for less than 16 characters", expectedOutput, actualOutput);

		// Test More than 16 characters (i.e. 17 characters)
		// i.e. output in string representation: @ABCDEFGHIJKLMNO
		inputCharArray = new char[]{ '\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F', '\u0050' };
		expectedOutput = new byte[] {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80};
		inputString = new String(inputCharArray);
		actualOutput = inputString.getBytes(encoding);
		AssertJUnit.assertArrayEquals("Incorrect result for more than 16 characters", expectedOutput, actualOutput);

		// Test less than 16 characters, and first character is out of range
		// i.e. output in string representation: ?ABCDEFGHIJKLMN
		inputCharArray = new char[]{ '\u008E', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E' };
		expectedOutput = new byte[] {63, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78};
		inputString = new String(inputCharArray);
		actualOutput = inputString.getBytes(encoding);
		AssertJUnit.assertArrayEquals("Incorrect result for less than 16 characters, and first character is out of range", expectedOutput, actualOutput);

		// Test less than 16 characters, and middle character is out of range
		// i.e. output in string representation: @ABCDEF?HIJKLMN
		inputCharArray = new char[]{ '\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u008E', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E' };
		expectedOutput = new byte[] {64, 65, 66, 67, 68, 69, 70, 63, 72, 73, 74, 75, 76, 77, 78};
		inputString = new String(inputCharArray);
		actualOutput = inputString.getBytes(encoding);
		AssertJUnit.assertArrayEquals("Incorrect result for less than 16 characters, and middle character is out of range", expectedOutput, actualOutput);

		// Test less than 16 characters, and end character is out of range
		// i.e. output in string representation: @ABCDEFGHIJKLM?
		inputCharArray = new char[]{ '\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u008E' };
		expectedOutput = new byte[] {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 63};
		inputString = new String(inputCharArray);
		actualOutput = inputString.getBytes(encoding);
		AssertJUnit.assertArrayEquals("Incorrect result for less than 16 characters, and end character is out of range", expectedOutput, actualOutput);

		// Test less than 16 characters, and 2 chars are out of range
		// i.e. output in string representation: ?ABCDEFGHIJKLM?
		inputCharArray = new char[]{ '\u008F', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u008E' };
		expectedOutput = new byte[] { 63, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 63};
		inputString = new String(inputCharArray);
		actualOutput = inputString.getBytes(encoding);
		AssertJUnit.assertArrayEquals("Incorrect result for less than 16 characters, and 2 chars are out of range", expectedOutput, actualOutput);

		// Test less than 16 characters, and 3 chars are out of range
		// i.e. output in string representation: ?ABCDEF?HIJKLM?
		inputCharArray = new char[]{ '\u008F', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u009A', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u008E' };
		expectedOutput = new byte[] {63, 65, 66, 67, 68, 69, 70, 63, 72, 73, 74, 75, 76, 77, 63};
		inputString = new String(inputCharArray);
		actualOutput = inputString.getBytes(encoding);
		AssertJUnit.assertArrayEquals("Incorrect result for less than 16 characters, and 3 chars are out of range", expectedOutput, actualOutput);

		// Test less than 16 characters, and all characters are out of range
		// i.e. output in string representation: ???????????????
		inputCharArray = new char[]{ '\u008F', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u009A', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u008E' };
		expectedOutput = new byte[]{63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63};
		inputString = new String(inputCharArray);
		actualOutput = inputString.getBytes(encoding);
		AssertJUnit.assertArrayEquals("Incorrect result for less than 16 characters, and all chars are out of range", expectedOutput, actualOutput);

		// Test 1 character, and it's in range
		// i.e. output in string representation: @
		inputCharArray = new char[]{ '\u0040' };
		expectedOutput = new byte[]{64};
		inputString = new String(inputCharArray);
                actualOutput = inputString.getBytes(encoding);
                AssertJUnit.assertArrayEquals("Incorrect result for testing 1 character (in range)", expectedOutput, actualOutput);

		// Test 1 character, and it's out of range
		// i.e. output in string representation: ?
		inputCharArray = new char[]{ '\u009A' };
		expectedOutput = new byte[]{63};
		inputString = new String(inputCharArray);
                actualOutput = inputString.getBytes(encoding);
                AssertJUnit.assertArrayEquals("Incorrect result for testing 1 character (out of range)", expectedOutput, actualOutput);

		// Test empty string
		// i.e. output in string representation:
		inputCharArray = new char[]{ };
		expectedOutput = new byte[]{};
		inputString = new String(inputCharArray);
                actualOutput = inputString.getBytes(encoding);
                AssertJUnit.assertArrayEquals("Incorrect result for testing empty string", expectedOutput, actualOutput);

		// Test 32 Characters, all in range
		// i.e. output in string representation: @ABCDEFGHIJKLMNO@ABCDEFGHIJKLMNO
		inputCharArray = new char[]{
				'\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F',
				'\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F'
				};
		expectedOutput = new byte[] { 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79};
		inputString = new String(inputCharArray);
                actualOutput = inputString.getBytes(encoding);
                AssertJUnit.assertArrayEquals("Incorrect result for testing 32 valid characters", expectedOutput, actualOutput);

		// Test 64 Characters, all in range
		// i.e. output in string representation: @ABCDEFGHIJKLMNO@ABCDEFGHIJKLMNO@ABCDEFGHIJKLMNO@ABCDEFGHIJKLMNO
		inputCharArray = new char[]{
				'\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F',
				'\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F',
				'\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F',
				'\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F'
				};
		expectedOutput = new byte[] {
				64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
				64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
				64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
				64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79
				};
		inputString = new String(inputCharArray);
                actualOutput = inputString.getBytes(encoding);
                AssertJUnit.assertArrayEquals("Incorrect result for tesing 64 valid characters", expectedOutput, actualOutput);

		// Test 24 valid characters
		// i.e. output in string representation: @ABCDEFGHIJKLMNOHIJKLMNO
		inputCharArray =  new char[]{ '\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F', '\u0048', '\u0049',
						'\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F' };
		expectedOutput = new byte[] {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 72, 73, 74, 75, 76, 77, 78, 79};
		inputString = new String(inputCharArray);
                actualOutput = inputString.getBytes(encoding);
                AssertJUnit.assertArrayEquals("Incorrect result for testing 24 valid characters", expectedOutput, actualOutput);

		// Test 40 valid characters
		// i.e. output in string representation: @ABCDEFGHIJKLMNOHIJKLMNO@ABCDEFGHIJKLMNO
		inputCharArray =  new char[]{ '\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F',
					'\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F', '\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047',
					'\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F'};
		expectedOutput = new byte[] {64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 72, 73, 74, 75, 76, 77, 78, 79, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79};
		inputString = new String(inputCharArray);
                actualOutput = inputString.getBytes(encoding);
                AssertJUnit.assertArrayEquals("Incorrect result for testing 40 valid characters", expectedOutput, actualOutput);

		// Test 41 characters, first character out of range
		// i.e. output in string representation: ?@ABCDEFGHIJKLMNOHIJKLMNO@ABCDEFGHIJKLMNO
		inputCharArray = new char[]{ '\u008E', '\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C',
					'\u004D', '\u004E', '\u004F', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F', '\u0040', '\u0041', '\u0042', '\u0043',
					'\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F'};
		expectedOutput = new byte[] {63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 72, 73, 74, 75, 76, 77, 78, 79, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79};
		inputString = new String(inputCharArray);
                actualOutput = inputString.getBytes(encoding);
                AssertJUnit.assertArrayEquals("Incorrect result for testing 41 characters, first character out of range", expectedOutput, actualOutput);

		// Test 42 characters, first and last character out of range
		// i.e. output in string representation: ?@ABCDEFGHIJKLMNOHIJKLMNO@ABCDEFGHIJKLMNO?
		inputCharArray = new char[]{ '\u008E', '\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D',
						'\u004E', '\u004F', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F', '\u0040', '\u0041', '\u0042', '\u0043', '\u0044',
						'\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F', '\u008F'};
		expectedOutput = new byte[] {63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 72, 73, 74, 75, 76, 77, 78, 79, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 63};
		inputString = new String(inputCharArray);
                actualOutput = inputString.getBytes(encoding);
                AssertJUnit.assertArrayEquals("Incorrect result for testing 42 characters, first and last character out of range", expectedOutput, actualOutput);

		// Test 43 characters, first, middle, and last character out of range
		// i.e. output in string representation: ?@ABCDEFGHIJKLMNOHIJK?LMNO@ABCDEFGHIJKLMNO?
		inputCharArray = new char[]{ '\u008E', '\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D',
						'\u004E', '\u004F', '\u0048', '\u0049', '\u004A', '\u004B', '\u008F', '\u004C', '\u004D', '\u004E', '\u004F', '\u0040', '\u0041', '\u0042', '\u0043',
						'\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F', '\u008F'};
		expectedOutput = new byte[] {63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 72, 73, 74, 75, 63, 76, 77, 78, 79, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 63};
		inputString = new String(inputCharArray);
                actualOutput = inputString.getBytes(encoding);
                AssertJUnit.assertArrayEquals("Incorrect result for testing 43 characters, first, middle and last character out of range", expectedOutput, actualOutput);

		// Test 43 characters, all bad
		// i.e. output in string representation: ???????????????????????????????????????????
		inputCharArray = new char[]{ '\u008E', '\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E',
						'\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E',
						'\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E','\u008E'};
		expectedOutput = new byte[] {63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63, 63};
		inputString = new String(inputCharArray);
                actualOutput = inputString.getBytes(encoding);
                AssertJUnit.assertArrayEquals("Incorrect result for testing 43 characters, all out of range", expectedOutput, actualOutput);

	}
	catch (UnsupportedEncodingException e)
	{
		AssertJUnit.fail("Unexpected: " + e);
	}
    }
}
