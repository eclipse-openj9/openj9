/*******************************************************************************
 * Copyright (c) 2021, 2021 IBM Corp. and others
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

package jit.test.recognizedMethod;
import org.testng.AssertJUnit;
import org.testng.annotations.Test;

public class TestJavaLangStringLatin1 {

    @Test(groups = {"level.sanity"}, invocationCount=2)
    public void test_java_lang_StringLatin1_inflate() {
        char[] expected = new char[0];
        char[] actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 0 array", expected, actual);

        expected = new char[] { '\u0054' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 1 array", expected, actual);

        expected = new char[] { '\u003E', '\u003F' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 2 array", expected, actual);

        expected = new char[] { '\u007E', '\u007F', '\u0080' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 3 array", expected, actual);

        expected = new char[] { '\u007A', '\u007B', '\u007C', '\u007D' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 4 array", expected, actual);

        expected = new char[] { '\u005B', '\u005D', '\u005D', '\u005E', '\u005F' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 5 array", expected, actual);

        expected = new char[] { '\u0067', '\u0068', '\u0069', '\u006A', '\u006B', '\u006C' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 6 array", expected, actual);

        expected = new char[] { '\u008C', '\u008D', '\u008E', '\u008F', '\u0090', '\u0091', '\u0092' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 7 array", expected, actual);

        expected = new char[] { '\u0074', '\u0075', '\u0076', '\u0077', '\u0078', '\u0079', '\u007A', '\u007B' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 8 array", expected, actual);

        expected = new char[] { '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 9 array", expected, actual);

        expected = new char[] { '\u0063', '\u0064', '\u0065', '\u0066', '\u0067', '\u0068', '\u0069', '\u006A', '\u006B', '\u006C' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 10 array", expected, actual);

        expected = new char[] { '\u005D', '\u005E', '\u005F', '\u0060', '\u0061', '\u0062', '\u0063', '\u0064', '\u0065', '\u0066', '\u0067' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 11 array", expected, actual);

        expected = new char[] { '\u0060', '\u0061', '\u0062', '\u0063', '\u0064', '\u0065', '\u0066', '\u0067', '\u0068', '\u0069', '\u006A', '\u006B' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 12 array", expected, actual);

        expected = new char[] { '\u0072', '\u0073', '\u0074', '\u0075', '\u0076', '\u0077', '\u0078', '\u0079', '\u007A', '\u007B', '\u007C', '\u007D', '\u007E' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 13 array", expected, actual);

        expected = new char[] { '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 14 array", expected, actual);

        expected = new char[] { '\u0083', '\u0084', '\u0085', '\u0086', '\u0087', '\u0088', '\u0089', '\u008A', '\u008B', '\u008C', '\u008D', '\u008E', '\u008F', '\u0090', '\u0091' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 15 array", expected, actual);

        expected = new char[] { '\u006B', '\u006C', '\u006D', '\u006E', '\u006F', '\u0070', '\u0071', '\u0072', '\u0073', '\u0074', '\u0075', '\u0076', '\u0077', '\u0078', '\u0079', '\u007A' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 16 array", expected, actual);

        expected = new char[] { '\u0094', '\u0095', '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 17 array", expected, actual);

        expected = new char[] { '\u0061', '\u0062', '\u0063', '\u0064', '\u0065', '\u0066', '\u0067', '\u0068', '\u0069', '\u006A', '\u006B', '\u006C', '\u006D', '\u006E', '\u006F', '\u0070', '\u0071', '\u0072' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 18 array", expected, actual);

        expected = new char[] { '\u005B', '\u005D', '\u005D', '\u005E', '\u005F', '\u0060', '\u0061', '\u0062', '\u0063', '\u0064', '\u0065', '\u0066', '\u0067', '\u0068', '\u0069', '\u006A', '\u006B', '\u006C', '\u006D', '\u006E', '\u006F', '\u0070', '\u0071' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 23 array", expected, actual);

        expected = new char[] { '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 24 array", expected, actual);

        expected = new char[] { '\u0093', '\u0094', '\u0095', '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 25 array", expected, actual);

        expected = new char[] { '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F', '\u0050', '\u0051', '\u0052', '\u0053', '\u0054', '\u0055', '\u0056', '\u0057', '\u0058', '\u0059', '\u005A', '\u005B', '\u005D', '\u005D', '\u005E', '\u005F', '\u0060', '\u0061' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 26 array", expected, actual);

        expected = new char[] { '\u005D', '\u005D', '\u005E', '\u005F', '\u0060', '\u0061', '\u0062', '\u0063', '\u0064', '\u0065', '\u0066', '\u0067', '\u0068', '\u0069', '\u006A', '\u006B', '\u006C', '\u006D', '\u006E', '\u006F', '\u0070', '\u0071', '\u0072', '\u0073', '\u0074', '\u0075', '\u0076', '\u0077', '\u0078', '\u0079', '\u007A' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 31 array", expected, actual);

        expected = new char[] { '\u0057', '\u0058', '\u0059', '\u005A', '\u005B', '\u005D', '\u005D', '\u005E', '\u005F', '\u0060', '\u0061', '\u0062', '\u0063', '\u0064', '\u0065', '\u0066', '\u0067', '\u0068', '\u0069', '\u006A', '\u006B', '\u006C', '\u006D', '\u006E', '\u006F', '\u0070', '\u0071', '\u0072', '\u0073', '\u0074', '\u0075', '\u0076' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 32 array", expected, actual);

        expected = new char[] { '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4', '\u00B5', '\u00B6', '\u00B7', '\u00B8', '\u00B9', '\u00BA', '\u00BB' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 33 array", expected, actual);

        expected = new char[] { '\u008F', '\u0090', '\u0091', '\u0092', '\u0093', '\u0094', '\u0095', '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 34 array", expected, actual);

        expected = new char[] { '\u003C', '\u003D', '\u003E', '\u003F', '\u0040', '\u0041', '\u0042', '\u0043', '\u0044', '\u0045', '\u0046', '\u0047', '\u0048', '\u0049', '\u004A', '\u004B', '\u004C', '\u004D', '\u004E', '\u004F', '\u0050', '\u0051', '\u0052', '\u0053', '\u0054', '\u0055', '\u0056', '\u0057', '\u0058', '\u0059', '\u005A', '\u005B', '\u005D', '\u005D', '\u005E', '\u005F', '\u0060', '\u0061', '\u0062', '\u0063', '\u0064', '\u0065', '\u0066', '\u0067', '\u0068' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 45 array", expected, actual);

        expected = new char[] { '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4', '\u00B5', '\u00B6', '\u00B7', '\u00B8', '\u00B9', '\u00BA', '\u00BB', '\u00BC', '\u00BD', '\u00BE', '\u00BF', '\u00C0', '\u00C1', '\u00C2', '\u00C3' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 46 array", expected, actual);

        expected = new char[] { '\u004E', '\u004F', '\u0050', '\u0051', '\u0052', '\u0053', '\u0054', '\u0055', '\u0056', '\u0057', '\u0058', '\u0059', '\u005A', '\u005B', '\u005D', '\u005D', '\u005E', '\u005F', '\u0060', '\u0061', '\u0062', '\u0063', '\u0064', '\u0065', '\u0066', '\u0067', '\u0068', '\u0069', '\u006A', '\u006B', '\u006C', '\u006D', '\u006E', '\u006F', '\u0070', '\u0071', '\u0072', '\u0073', '\u0074', '\u0075', '\u0076', '\u0077', '\u0078', '\u0079', '\u007A', '\u007B', '\u007C' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 47 array", expected, actual);

        expected = new char[] { '\u0089', '\u008A', '\u008B', '\u008C', '\u008D', '\u008E', '\u008F', '\u0090', '\u0091', '\u0092', '\u0093', '\u0094', '\u0095', '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4', '\u00B5', '\u00B6', '\u00B7', '\u00B8' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 48 array", expected, actual);

        expected = new char[] { '\u0055', '\u0056', '\u0057', '\u0058', '\u0059', '\u005A', '\u005B', '\u005D', '\u005D', '\u005E', '\u005F', '\u0060', '\u0061', '\u0062', '\u0063', '\u0064', '\u0065', '\u0066', '\u0067', '\u0068', '\u0069', '\u006A', '\u006B', '\u006C', '\u006D', '\u006E', '\u006F', '\u0070', '\u0071', '\u0072', '\u0073', '\u0074', '\u0075', '\u0076', '\u0077', '\u0078', '\u0079', '\u007A', '\u007B', '\u007C', '\u007D', '\u007E', '\u007F', '\u0080', '\u0081', '\u0082', '\u0083', '\u0084', '\u0085' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 49 array", expected, actual);

        expected = new char[] { '\u008E', '\u008F', '\u0090', '\u0091', '\u0092', '\u0093', '\u0094', '\u0095', '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4', '\u00B5', '\u00B6', '\u00B7', '\u00B8', '\u00B9', '\u00BA', '\u00BB', '\u00BC', '\u00BD', '\u00BE', '\u00BF' };
        actual = new String(expected).toCharArray();
        AssertJUnit.assertArrayEquals("Incorrect result for length 50 array", expected, actual);

        // For tests where the expected and actual arrays are different from the initial input array.
        char[] input = new char[] { '\u008E', '\u008F', '\u0090', '\u0091', '\u0092', '\u0093', '\u0094', '\u0095', '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4', '\u00B5', '\u00B6', '\u00B7', '\u00B8', '\u00B9', '\u00BA', '\u00BB', '\u00BC', '\u00BD', '\u00BE', '\u00BF' };
        String inputString = new String(input);

        // Test if copying last 49 elements works correctly from input[]
        expected = new char[] { '\u008F', '\u0090', '\u0091', '\u0092', '\u0093', '\u0094', '\u0095', '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4', '\u00B5', '\u00B6', '\u00B7', '\u00B8', '\u00B9', '\u00BA', '\u00BB', '\u00BC', '\u00BD', '\u00BE', '\u00BF' };
        actual = new char[49];
        inputString.getChars(1, 50, actual, 0);
        AssertJUnit.assertArrayEquals("Incorrect result when trying to copy last 49 elements from 50 element array", expected, actual);

        // Test if copying first 49 works as expected from input[]
        expected = new char[] { '\u008E', '\u008F', '\u0090', '\u0091', '\u0092', '\u0093', '\u0094', '\u0095', '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4', '\u00B5', '\u00B6', '\u00B7', '\u00B8', '\u00B9', '\u00BA', '\u00BB', '\u00BC', '\u00BD', '\u00BE' };
        actual = new char[49];
        inputString.getChars(0, 49, actual, 0);
        AssertJUnit.assertArrayEquals("Incorrect result when trying to copy first 49 elements from 50 length array", expected, actual);

        // Test if copying elements 1-48 works as expected from input[]
        expected = new char[] { '\u008F', '\u0090', '\u0091', '\u0092', '\u0093', '\u0094', '\u0095', '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4', '\u00B5', '\u00B6', '\u00B7', '\u00B8', '\u00B9', '\u00BA', '\u00BB', '\u00BC', '\u00BD', '\u00BE' };
        actual = new char[48];
        inputString.getChars(1, 49, actual, 0);
        AssertJUnit.assertArrayEquals("Incorrect result when trying to copy elements 1-48 from 50 element array", expected, actual);

        // Test if elements 10-49 can be copied correctly from input[]
        expected = new char[] { '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4', '\u00B5', '\u00B6', '\u00B7', '\u00B8', '\u00B9', '\u00BA', '\u00BB', '\u00BC', '\u00BD', '\u00BE', '\u00BF' };
        actual = new char[40];
        inputString.getChars(10, 50, actual, 0);
        AssertJUnit.assertArrayEquals("Incorrect result when trying to copy elements 10-49 from 50 element array", expected, actual);

        // Test if elements 0-39 can be copied correctly from input[]
        expected = new char[] { '\u008E', '\u008F', '\u0090', '\u0091', '\u0092', '\u0093', '\u0094', '\u0095', '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4', '\u00B5' };
        actual = new char[40];
        inputString.getChars(0, 40, actual, 0);
        AssertJUnit.assertArrayEquals("Incorrect result when trying to copy elements 0-39 from 50 element array", expected, actual);

        // Copy elements 29-38 (inclusive) from input and place them at index 10-19 in resulting arrays.
        expected = new char[] { '\u008E', '\u008F', '\u0090', '\u0091', '\u0092', '\u0093', '\u0094', '\u0095', '\u0096', '\u0097', /* start*/'\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4' /*end*/, '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4', '\u00B5', '\u00B6', '\u00B7', '\u00B8', '\u00B9', '\u00BA', '\u00BB', '\u00BC', '\u00BD', '\u00BE', '\u00BF' };
        actual = new char[] { '\u008E', '\u008F', '\u0090', '\u0091', '\u0092', '\u0093', '\u0094', '\u0095', '\u0096', '\u0097', '\u0098', '\u0099', '\u009A', '\u009B', '\u009C', '\u009D', '\u009E', '\u009F', '\u00A0', '\u00A1', '\u00A2', '\u00A3', '\u00A4', '\u00A5', '\u00A6', '\u00A7', '\u00A8', '\u00A9', '\u00AA', '\u00AB', '\u00AC', '\u00AD', '\u00AE', '\u00AF', '\u00B0', '\u00B1', '\u00B2', '\u00B3', '\u00B4', '\u00B5', '\u00B6', '\u00B7', '\u00B8', '\u00B9', '\u00BA', '\u00BB', '\u00BC', '\u00BD', '\u00BE', '\u00BF' };
        inputString.getChars(29, 39, actual, 10);
        AssertJUnit.assertArrayEquals("Incorrect result when trying to copy elements 29-38 from 50 element array and overwrite elements 10-19 in resulting arrays", expected, actual);
    }
}
