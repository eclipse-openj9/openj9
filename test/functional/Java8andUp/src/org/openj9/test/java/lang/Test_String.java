/*******************************************************************************
 * Copyright (c) 1998, 2020 IBM Corp. and others
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
package org.openj9.test.java.lang;

import org.openj9.test.util.VersionCheck;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.io.UnsupportedEncodingException;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Locale;

@Test(groups = { "level.sanity" })
public class Test_String {
	private static final Logger logger = Logger.getLogger(Test_String.class);

	String hw1 = "HelloWorld";
	String hw2 = "HelloWorld";
	String hwlc = "helloworld";
	String hwuc = "HELLOWORLD";
	String hello1 = "Hello";
	String world1 = "World";
	String comp11 = "Test String";
	String split1 = "boo:and:foo";
	Object obj = new Object();
	char[] buf = { 'W', 'o', 'r', 'l', 'd' };
	char[] rbuf = new char[5];

	/**
	 * @tests java.lang.String#String()
	 */
	@Test
	public void test_Constructor() {
		AssertJUnit.assertTrue("Created incorrect string", new String().equals(""));
	}

	/**
	 * @tests java.lang.String#String(byte[])
	 */
	@Test
	public void test_Constructor2() {
		AssertJUnit.assertTrue("Failed to create string", new String(hw1.getBytes()).equals(hw1));
	}

	/**
	 * @tests java.lang.String#String(byte[], int)
	 */
	@Test
	public void test_Constructor3() {
		String s = new String(new byte[] { 65, 66, 67, 68, 69 }, 0);
		AssertJUnit.assertTrue("Incorrect string returned: " + s, s.equals("ABCDE"));
		s = new String(new byte[] { 65, 66, 67, 68, 69 }, 1);
		AssertJUnit.assertTrue("Did not use nonzero hibyte", !s.equals("ABCDE"));
	}

	/**
	 * @tests java.lang.String#String(byte[], int, int)
	 */
	@Test
	public void test_Constructor4() {
		AssertJUnit.assertTrue("Failed to create string",
				new String(hw1.getBytes(), 0, hw1.getBytes().length).equals(hw1));

		boolean exception = false;
		try {
			new String(new byte[0], 0, Integer.MAX_VALUE);
		} catch (IndexOutOfBoundsException e) {
			exception = true;
		}
		AssertJUnit.assertTrue("Did not throw exception", exception);
	}

	/**
	 * @tests java.lang.String#String(byte[], int, int, int)
	 */
	@Test
	public void test_Constructor5() {
		String s = new String(new byte[] { 65, 66, 67, 68, 69 }, 0, 1, 3);
		AssertJUnit.assertTrue("Incorrect string returned: " + s, s.equals("BCD"));
		s = new String(new byte[] { 65, 66, 67, 68, 69 }, 1, 0, 5);
		AssertJUnit.assertTrue("Did not use nonzero hibyte", !s.equals("ABCDE"));
	}

	/**
	 * @tests java.lang.String#String(byte[], int, int, java.lang.String)
	 */
	@Test
	public void test_Constructor6() {
		String s = null;
		try {
			s = new String(new byte[] { 65, 66, 67, 68, 69 }, 0, 5, "8859_1");
		} catch (Exception e) {
			AssertJUnit.assertTrue("Threw exception: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Incorrect string returned: " + s, s.equals("ABCDE"));
	}

	/**
	 * @tests java.lang.String#String(byte[], java.lang.String)
	 */
	@Test
	public void test_Constructor7() {
		String s = null;
		try {
			s = new String(new byte[] { 65, 66, 67, 68, 69 }, "8859_1");
		} catch (Exception e) {
			AssertJUnit.assertTrue("Threw exception: " + e.toString(), false);
		}
		AssertJUnit.assertTrue("Incorrect string returned: " + s, s.equals("ABCDE"));
	}

	/**
	 * @tests java.lang.String#String(byte[], java.lang.String)
	 */
	@Test
	public void test_Constructor8() {
		/* [PR 122454] Count invalid GB18030 sequences correctly */
		try {
			new String(new byte[] { (byte)0x90, (byte)0x30, 0, 0 }, "GB18030");
		} catch (UnsupportedEncodingException e) {
			// passed
		} catch (Exception e) {
			Assert.fail("Unexpected: " + e);
		}
	}

	/**
	 * @tests java.lang.String#String(char[])
	 */
	@Test
	public void test_Constructor9() {
		AssertJUnit.assertTrue("Failed Constructor test", new String(buf).equals("World"));
	}

	/**
	 * @tests java.lang.String#String(char[], int, int)
	 */
	@Test
	public void test_Constructor10() {
		char[] buf1 = { 'H', 'e', 'l', 'l', 'o', 'W', 'o', 'r', 'l', 'd' };
		String s1 = new String(buf1, 0, buf1.length);
		AssertJUnit.assertTrue("Incorrect string created", hw1.equals(s1));

		boolean exception = false;
		try {
			new String(new char[0], 0, Integer.MAX_VALUE);
		} catch (IndexOutOfBoundsException e) {
			exception = true;
		}
		AssertJUnit.assertTrue("Did not throw exception", exception);

		char[] buf2 = { '\u201c', 'X', '\u201d', '\n', '\u201c', 'X', '\u201d' };
		String s2 = new String(buf2, 4, 3);
		AssertJUnit.assertTrue("Incorrect string created", s2.equals("\u201cX\u201d"));
	}

	/**
	 * @tests java.lang.String#String(java.lang.String)
	 */
	@Test
	public void test_Constructor11() {
		String s = new String("Hello World");
		AssertJUnit.assertTrue("Failed to construct correct string", s.equals("Hello World"));
	}

	/**
	 * @tests java.lang.String#String(java.lang.String)
	 */
	@Test
	public void test_Constructor12() {
		// substring
		String s = new String("Hello World".substring(0));
		AssertJUnit.assertTrue("Failed to construct correct string", s.equals("Hello World"));
	}

	/**
	 * @tests java.lang.String#String(java.lang.String)
	 */
	@Test
	public void test_Constructor13() {
		// substring length_0
		String s = new String("Hello World".substring(4, 4));
		AssertJUnit.assertTrue("Failed to construct correct string when the argument is a substring of length 0",
				s.equals(""));
	}

	/**
	 * @tests java.lang.String#String(java.lang.String)
	 */
	@Test
	public void test_Constructor14() {
		// substring_length_1_range
		char[] chars = new char[256];
		for (int i = 0; i < chars.length; i++) {
			chars[i] = (char)i;
		}
		String string = new String(chars);
		for (int i = 0; i < chars.length; i++) {
			String s = new String(string.substring(i, i + 1));
			AssertJUnit.assertTrue(
					"Failed to construct correct string when i is " + i + ", expected: " + chars[i] + "; but got: " + s,
					s.equals("" + chars[i]));
		}
	}

	/**
	 * @tests java.lang.String#String(java.lang.String)
	 */
	@Test
	public void test_Constructor15() {
		// substring_length_1
		String s = new String(("Hello World" + "\u0090").substring(11, 12));
		AssertJUnit.assertTrue(
				"Failed to construct correct string when the argument is a susbtring of legnth 1 and is out the range of ascii",
				s.equals("\u0090"));
	}

	/**
	 * @tests java.lang.String#String(java.lang.String)
	 */
	@Test
	public void test_Constructor16() {
		// substring_length_normal
		String s = new String("Hello World".substring(2));
		AssertJUnit.assertTrue("Failed to construct correct string when the argument is a substring",
				s.equals("llo World"));
	}

	/**
	 * @tests java.lang.String#String(java.lang.StringBuffer)
	 */
	@Test
	public void test_Constructor17() {
		StringBuffer sb = new StringBuffer();
		sb.append("HelloWorld");
		AssertJUnit.assertTrue("Created incorrect string", new String(sb).equals("HelloWorld"));
	}

	/**
	 * @tests java.lang.String#String(int[], int, int)
	 */
	@Test
	public void test_Constructor18() {
		String s1 = new String(new int[] { 65, 66, 67, 68 }, 0, 4);
		AssertJUnit.assertTrue("Invalid start=0", s1.equals("ABCD"));
		s1 = new String(new int[] { 65, 66, 67, 68 }, 1, 3);
		AssertJUnit.assertTrue("Invalid start=1", s1.equals("BCD"));
		s1 = new String(new int[] { 65, 66, 67, 68 }, 1, 2);
		AssertJUnit.assertTrue("Invalid start=1,length=2", s1.equals("BC"));
		s1 = new String(new int[] { 65, 66, 0x10000, 68, 69 }, 1, 3);
		AssertJUnit.assertTrue("Invalid codePoint 0x10000", s1.equals("B\ud800\udc00D"));
		s1 = new String(new int[] { 65, 66, 0x10400, 68 }, 1, 3);
		AssertJUnit.assertTrue("Invalid codePoint 0x10400", s1.equals("B\ud801\udc00D"));
		s1 = new String(new int[] { 65, 66, 0x10ffff, 68 }, 1, 3);
		AssertJUnit.assertTrue("Invalid codePoint 0x10ffff", s1.equals("B\udbff\udfffD"));
	}

	/**
	 * @tests java.lang.String#charAt(int)
	 */
	@Test
	public void test_charAt() {
		AssertJUnit.assertTrue("Incorrect character returned", hw1.charAt(5) == 'W' && (hw1.charAt(1) != 'Z'));
	}

	/**
	 * @tests java.lang.String#compareTo(java.lang.String)
	 */
	@Test
	public void test_compareTo() {
		AssertJUnit.assertTrue("Returned incorrect value for first < second", "aaaaab".compareTo("aaaaac") < 0);
		AssertJUnit.assertTrue("Returned incorrect value for first = second", "aaaaac".compareTo("aaaaac") == 0);
		AssertJUnit.assertTrue("Returned incorrect value for first > second", "aaaaac".compareTo("aaaaab") > 0);
		AssertJUnit.assertTrue("Considered case to not be of importance", !("A".compareTo("a") == 0));
	}

	/**
	 * @tests java.lang.String#compareToIgnoreCase(java.lang.String)
	 */
	@Test
	public void test_compareToIgnoreCase() {
		AssertJUnit.assertTrue("Returned incorrect value for first < second",
				"aaaaab".compareToIgnoreCase("aaaaac") < 0);
		AssertJUnit.assertTrue("Returned incorrect value for first = second",
				"aaaaac".compareToIgnoreCase("aaaaac") == 0);
		AssertJUnit.assertTrue("Returned incorrect value for first > second",
				"aaaaac".compareToIgnoreCase("aaaaab") > 0);
		AssertJUnit.assertTrue("Considered case to not be of importance", "A".compareToIgnoreCase("a") == 0);

		/* [PR 101422] compareToIgnoreCase() should be locale independent */
		AssertJUnit.assertTrue("0xbf should not compare = to 'ss'", "\u00df".compareToIgnoreCase("ss") != 0);
		AssertJUnit.assertTrue("0x130 should compare = to 'i'", "\u0130".compareToIgnoreCase("i") == 0);
		AssertJUnit.assertTrue("0x131 should compare = to 'i'", "\u0131".compareToIgnoreCase("i") == 0);
		if (VersionCheck.major() >= 16) {
			AssertJUnit.assertTrue("DESERET CAPITAL LETTER LONG I should compare == to DESERET SMALL LETTER LONG I",
				"\ud801\udc00".compareToIgnoreCase("\ud801\udc28") == 0);
			AssertJUnit.assertTrue("low surrogate only of a Deseret capital/small letter pair at end of string should compare unequal",
				"A\udc00".compareToIgnoreCase("A\udc28") != 0);
		}

		Locale defLocale = Locale.getDefault();
		try {
			Locale.setDefault(new Locale("tr", ""));
			AssertJUnit.assertTrue("Locale tr: 0x130 should compare = to 'i'", "\u0130".compareToIgnoreCase("i") == 0);
			AssertJUnit.assertTrue("Locale tr: 0x131 should compare = to 'i'", "\u0131".compareToIgnoreCase("i") == 0);
		} finally {
			Locale.setDefault(defLocale);
		}
	}

	/**
	 * @tests java.lang.String#concat(java.lang.String)
	 */
	@Test
	public void test_concat() {
		AssertJUnit.assertTrue("Concatenation failed to produce correct string", hello1.concat(world1).equals(hw1));
		boolean exception = false;
		try {
			String a = new String("test");
			String b = null;
			a.concat(b);
		} catch (NullPointerException e) {
			exception = true;
		}
		AssertJUnit.assertTrue("Concatenation failed to throw NP exception (1)", exception);
		exception = false;
		try {
			String a = new String("");
			String b = null;
			a.concat(b);
		} catch (NullPointerException e) {
			exception = true;
		}
		AssertJUnit.assertTrue("Concatenation failed to throw NP exception (2)", exception);

		/* [PR CMVC 96260] return copy even when receiver is empty */
		String s1 = "";
		String s2 = "s2";
		String s3 = s1.concat(s2);
		AssertJUnit.assertTrue("should not be identical", s2 != s3);
	}


	@Test
	public void test_split() {
		String[] result = split1.split(":", 2);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 2, result.length);
		AssertJUnit.assertEquals("split returned the wrong string: 0", "boo", result[0]);
		AssertJUnit.assertEquals("split returned the wrong string: 1", "and:foo", result[1]);
	}

	@Test
	public void test_split2() {
		String[] result = split1.split(":", 5);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 3, result.length);
		AssertJUnit.assertEquals("split returned the wrong string: 0", "boo", result[0]);
		AssertJUnit.assertEquals("split returned the wrong string: 1", "and", result[1]);
		AssertJUnit.assertEquals("split returned the wrong string: 2", "foo", result[2]);
	}

	@Test
	public void test_split3() {
		String[] result = split1.split(":", -2);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 3, result.length);
		AssertJUnit.assertEquals("split returned the wrong string: 0", "boo", result[0]);
		AssertJUnit.assertEquals("split returned the wrong string: 1", "and", result[1]);
		AssertJUnit.assertEquals("split returned the wrong string: 2", "foo", result[2]);
	}

	@Test
	public void test_split4() {
		String[] result = split1.split("o", 5);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 5, result.length);
		AssertJUnit.assertEquals("split returned the wrong string: 0", "b", result[0]);
		AssertJUnit.assertEquals("split returned the wrong string: 1", "", result[1]);
		AssertJUnit.assertEquals("split returned the wrong string: 2", ":and:f", result[2]);
		AssertJUnit.assertEquals("split returned the wrong string: 3", "", result[3]);
		AssertJUnit.assertEquals("split returned the wrong string: 4", "", result[4]);
	}

	@Test
	public void test_split5() {
		String[] result = split1.split("o", -2);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 5, result.length);
		AssertJUnit.assertEquals("split returned the wrong string: 0", "b", result[0]);
		AssertJUnit.assertEquals("split returned the wrong string: 1", "", result[1]);
		AssertJUnit.assertEquals("split returned the wrong string: 2", ":and:f", result[2]);
		AssertJUnit.assertEquals("split returned the wrong string: 3", "", result[3]);
		AssertJUnit.assertEquals("split returned the wrong string: 4", "", result[4]);
	}

	@Test
	public void test_split6() {
		String[] result = split1.split("o", 0);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 3, result.length);
		AssertJUnit.assertEquals("split returned the wrong string: 0", "b", result[0]);
		AssertJUnit.assertEquals("split returned the wrong string: 1", "", result[1]);
		AssertJUnit.assertEquals("split returned the wrong string: 2", ":and:f", result[2]);
	}

	@Test
	public void test_split7() {
		String[] result = split1.split("o", 1);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 1, result.length);
		AssertJUnit.assertSame("split did not return an identity string", split1, result[0]);
	}

	@Test
	public void test_split8() {
		String[] result = split1.split("o");
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 3, result.length);
		AssertJUnit.assertEquals("split returned the wrong string: 0", "b", result[0]);
		AssertJUnit.assertEquals("split returned the wrong string: 1", "", result[1]);
		AssertJUnit.assertEquals("split returned the wrong string: 2", ":and:f", result[2]);
	}

	@Test
	public void test_split9() {
		String[] result = split1.split("z", -1);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 1, result.length);
		AssertJUnit.assertSame("split did not return an identity string", split1, result[0]);
	}

	@Test
	public void test_split10() {
		String[] result = split1.split("oo", -1);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 3, result.length);
		AssertJUnit.assertEquals("split returned the wrong string: 0", "b", result[0]);
		AssertJUnit.assertEquals("split returned the wrong string: 1", ":and:f", result[1]);
		AssertJUnit.assertEquals("split returned the wrong string: 2", "", result[2]);
	}

	@Test
	public void test_split11() {
		String[] result = split1.split("oo", 0);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 2, result.length);
		AssertJUnit.assertEquals("split returned the wrong string: 0", "b", result[0]);
		AssertJUnit.assertEquals("split returned the wrong string: 1", ":and:f", result[1]);
	}

	@Test
	public void test_split12() {
		String[] result = "oooo".split("o", 0);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 0, result.length);
	}

	@Test
	public void test_split13() {
		String test = new String("");
		String[] result = test.split("o");
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 1, result.length);
		AssertJUnit.assertSame("split did not return an identity string", test, result[0]);
	}

	@Test
	public void test_split14() {
		String[] result = split1.split("o+", 0);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 2, result.length);
		AssertJUnit.assertEquals("split returned the wrong string: 0", "b", result[0]);
		AssertJUnit.assertEquals("split returned the wrong string: 1", ":and:f", result[1]);
	}

	@Test
	public void test_split15() {
		boolean exception = false;
		boolean correctException = false;
		try {
			split1.split("+", -1);
		} catch (java.util.regex.PatternSyntaxException e) {
			exception = true;
			correctException = true;
		} catch (Exception e) {
			exception = true;
		}
		AssertJUnit.assertTrue("split should have thrown an exception", exception);
		AssertJUnit.assertTrue("split threw the wrong exception", correctException);
	}

	@Test
	public void test_split16() {
		String test = new String("foo");
		String[] result = test.split("", -1);
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 4, result.length);
		AssertJUnit.assertEquals("split returned the wrong string: 0", "f", result[0]);
		AssertJUnit.assertEquals("split returned the wrong string: 1", "o", result[1]);
		AssertJUnit.assertEquals("split returned the wrong string: 2", "o", result[2]);
		AssertJUnit.assertEquals("split returned the wrong string: 3", "", result[3]);
	}

	@Test
	public void test_split17() {
		String test = new String("foo");
		String[] result = test.split("\u0131");
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 1, result.length);
		AssertJUnit.assertSame("split did not return an identity string", test, result[0]);
	}

	@Test
	public void test_split18() {
		String[] result = "abc\u0131def".split("\u0131");
		AssertJUnit.assertNotNull("split returned null", result);
		AssertJUnit.assertEquals("split returned the wrong length", 2, result.length);
		AssertJUnit.assertEquals("split returned the wrong string: 0", "abc", result[0]);
		AssertJUnit.assertEquals("split returned the wrong string: 1", "def", result[1]);
	}

	/**
	 * @tests java.lang.String#copyValueOf(char[])
	 */
	@Test
	public void test_copyValueOf() {
		char[] t = { 'H', 'e', 'l', 'l', 'o', 'W', 'o', 'r', 'l', 'd' };
		AssertJUnit.assertTrue("copyValueOf returned incorrect String", String.copyValueOf(t).equals("HelloWorld"));
	}

	/**
	 * @tests java.lang.String#copyValueOf(char[], int, int)
	 */
	@Test
	public void test_copyValueOf2() {
		char[] t = { 'H', 'e', 'l', 'l', 'o', 'W', 'o', 'r', 'l', 'd' };
		AssertJUnit.assertTrue("copyValueOf returned incorrect String", String.copyValueOf(t, 5, 5).equals("World"));
	}

	/**
	 * @tests java.lang.String#endsWith(java.lang.String)
	 */
	@Test
	public void test_endsWith() {
		AssertJUnit.assertTrue("Failed to fine ending string", hw1.endsWith("ld"));
	}

	/**
	 * @tests java.lang.String#equals(java.lang.Object)
	 */
	@Test
	public void test_equals() {
		AssertJUnit.assertTrue("String not equal", hw1.equals(hw2) && !(hw1.equals(comp11)));
	}

	/**
	 * @tests java.lang.String#equalsIgnoreCase(java.lang.String)
	 */
	@Test
	public void test_equalsIgnoreCase() {
		AssertJUnit.assertTrue("lc version returned unequal to uc", hwlc.equalsIgnoreCase(hwuc));
		if (VersionCheck.major() >= 16) {
			AssertJUnit.assertTrue("DESERET CAPITAL LETTER LONG I returned unequal to DESERET SMALL LETTER LONG I",
				"\ud801\udc00".equalsIgnoreCase("\ud801\udc28"));
			AssertJUnit.assertFalse("low surrogate only of a Deseret capital/small letter pair at end of string should return unequal",
				"A\udc00".equalsIgnoreCase("A\udc28"));
		}
	}

	/**
	 * @tests java.lang.String#contentEquals(java.lang.StringBuffer)
	 */
	@Test
	public void test_contentEquals() {
		AssertJUnit.assertTrue("contentEquals: failed to return true with matching strings", hw1.contentEquals(new StringBuffer(hw2)));
		AssertJUnit.assertFalse("contentEquals: failed to return false with non matching strings", hw1.contentEquals(new StringBuffer(hello1)));
	}

	/**
	 * @tests java.lang.String#getBytes()
	 */
	@Test
	public void test_getBytes() {
		byte[] sbytes = hw1.getBytes();
		/* [PR JAZZ 9610] Test_String failures on zOS because of EBCIDIC */
		byte[] sbytes2 = null;
		try {
			sbytes2 = hw1.getBytes(System.getProperty("file.encoding"));
		} catch (UnsupportedEncodingException e) {
			Assert.fail("Unexpected: " + e);
		}
		AssertJUnit.assertTrue("not same length", sbytes.length == sbytes2.length);
		for (int i = 0; i < hw1.length(); i++) {
			AssertJUnit.assertTrue("Returned incorrect bytes", sbytes[i] == sbytes2[i]);
		}

		byte[] bytes = new byte[1];
		for (int i = 0; i < 256; i++) {
			bytes[0] = (byte)i;
			String result = null;
			try {
				result = new String(bytes, "8859_1");
				AssertJUnit.assertTrue("Wrong char length", result.length() == 1);
				AssertJUnit.assertTrue("Wrong char value", result.charAt(0) == (char)i);
			} catch (java.io.UnsupportedEncodingException e) {
			}
		}

		/*
		 * [PR CMVC 114302] String.getBytes(String) does not throw
		 * NullPointerException
		 */
		try {
			"abc".getBytes((String)null);
			Assert.fail("should throw NullPointerException");
		} catch (UnsupportedEncodingException e) {
			Assert.fail("Unexpected: " + e);
		} catch (NullPointerException e) {
			// expected
		}
	}

	/**
	 * @tests java.lang.String#getBytes(int, int, byte[], int)
	 */
	@Test
	public void test_getBytes2() {
		byte[] buf = new byte[5];
		"Hello World".getBytes(6, 11, buf, 0);
		/* [PR JAZZ 9610] Test_String failures on zOS because of EBCIDIC */
		try {
			AssertJUnit.assertTrue("Returned incorrect bytes", new String(buf, "ISO8859_1").equals("World"));
		} catch (UnsupportedEncodingException e) {
			Assert.fail("Unexpected: " + e);
		}

		/*
		 * [PR 116414] String.getBytes(int, int, byte[], int) throws wrong
		 * exception
		 */
		Exception exception = null;
		try {
			"Hello World".getBytes(-1, 1, null, 0);
			Assert.fail("Expected StringIndexOutOfBoundsException");
		} catch (StringIndexOutOfBoundsException e) {
		} catch (NullPointerException e) {
			Assert.fail("Threw wrong exception");
		}
	}

	/**
	 * @tests java.lang.String#getBytes(java.lang.String)
	 */
	@Test
	public void test_getBytes3() {
		byte[] buf = "Hello World".getBytes();
		AssertJUnit.assertTrue("Returned incorrect bytes", new String(buf).equals("Hello World"));

		boolean exception = false;
		try {
			"string".getBytes("8849_1");
		} catch (java.io.UnsupportedEncodingException e) {
			exception = true;
		}
		AssertJUnit.assertTrue("Did not throw exception", exception);

		/* [PR 111781] CharacterConverter does not maintain state */
		try {
			byte[] bytes = "\u3048".getBytes("ISO2022JP");
			String converted = new String(bytes, "ISO8859_1");
			AssertJUnit.assertTrue("invalid conversion: " + converted, converted.equals("\u001b$B$(\u001b(B"));
		} catch (UnsupportedEncodingException e) {
			// Can't test missing converter
			e.printStackTrace();
			Assert.fail("Unexpected exception" + e.toString());
		}
	}

	/**
	 * @tests java.lang.String#getBytes(java.lang.String)
	 */
	//JAZZ103 73901 - SVT:JIT CharsetEncoder.encode works inconsistently on z10
	@Test(groups = { "defect.73901.arch.390" })
	public void test_getBytes4() {
		char[] chars = new char[1];
		for (int i = 0; i < 65536; i++) {
			// skip surrogates
			if (i == 0xd800)
				i = 0xe000;
			byte[] result = null;
			chars[0] = (char)i;
			String string = new String(chars);
			try {
				result = string.getBytes("8859_1");
				AssertJUnit.assertTrue("Wrong byte conversion 8859_1: " + i,
						(i < 256 && result[0] == (byte)i) || (i > 255 && result[0] == '?'));
			} catch (java.io.UnsupportedEncodingException e) {
			}
			try {
				result = string.getBytes("UTF8");
				int length = i < 0x80 ? 1 : (i < 0x800 ? 2 : 3);
				AssertJUnit.assertTrue("Wrong length UTF8: " + Integer.toHexString(i), result.length == length);
				AssertJUnit.assertTrue("Wrong bytes UTF8: " + Integer.toHexString(i),
						(i < 0x80 && result[0] == i)
								|| (i >= 0x80 && i < 0x800 && result[0] == (byte)(0xc0 | ((i & 0x7c0) >> 6))
										&& result[1] == (byte)(0x80 | (i & 0x3f)))
								|| (i >= 0x800 && result[0] == (byte)(0xe0 | (i >> 12))
										&& result[1] == (byte)(0x80 | ((i & 0xfc0) >> 6))
										&& result[2] == (byte)(0x80 | (i & 0x3f))));
			} catch (java.io.UnsupportedEncodingException e) {
			}

			String bytes = null;
			try {
				bytes = new String(result, "UTF8");
				if (bytes.length() != 1) {
					for (int j = 0; j < result.length; j++) {
						logger.debug(Integer.toHexString(result[j]) + " ");
					}
				}
				AssertJUnit.assertTrue("Wrong UTF8 byte length: " + bytes.length() + "(" + i + ")",
						bytes.length() == 1);
				AssertJUnit.assertTrue("Wrong char UTF8: " + Integer.toHexString(bytes.charAt(0)) + " (" + i + ")",
						bytes.charAt(0) == i);
			} catch (java.io.UnsupportedEncodingException e) {
			}
		}
	}

	/**
	 * @tests java.lang.String#getBytes(byte[], int, byte)
	 */
	@Test
	public void test_getBytes6() {
		Method method;
		try {
			method = String.class.getDeclaredMethod("getBytes", new Class<?>[] { byte[].class, int.class, byte.class });
			method.setAccessible(true);

			byte[] bytesResult = new byte[10];
			byte[] bytesExpected = new byte[] { 0, 0, 97, 0, 98, 0, 49, 0, 50, 0 };
			if (ByteOrder.nativeOrder() == ByteOrder.BIG_ENDIAN) {
				for (int i = 0; i < bytesExpected.length - 1; i += 2) {
					byte temp = bytesExpected[i];
					bytesExpected[i] = bytesExpected[i + 1];
					bytesExpected[i + 1] = temp;
				}
			}
			byte coder = 1;
			String strInput = "ab12";
			method.invoke(strInput, bytesResult, 1, coder);
			if (!Arrays.equals(bytesResult, bytesExpected)) {
				Assert.fail(
						"Expected: " + Arrays.toString(bytesExpected) + ", but got: " + Arrays.toString(bytesResult));
			}
		} catch (NoSuchMethodException e) {
			// ignore NoSuchMethodException
		} catch (Exception e) {
			Assert.fail(e.getMessage());
		}
	}

	/**
	 * @tests java.lang.String#getChars(int, int, char[], int)
	 */
	@Test
	public void test_getChars() {
		hw1.getChars(5, hw1.length(), rbuf, 0);

		for (int i = 0; i < rbuf.length; i++)
			AssertJUnit.assertTrue("getChars returned incorrect char(s)", rbuf[i] == buf[i]);
	}

	/**
	 * @tests java.lang.String#hashCode()
	 */
	@Test
	public void test_hashCode() {
		// Test for method int java.lang.String.hashCode()
		int hwHashCode = 0;
		final int hwLength = hw1.length();
		int powerOfThirtyOne = 1;
		for (int counter = hwLength - 1; counter >= 0; counter--) {
			hwHashCode += ((int)hw1.charAt(counter)) * powerOfThirtyOne;
			powerOfThirtyOne *= 31;
		}
		AssertJUnit.assertTrue("String did not hash to correct value--got: " + String.valueOf(hw1.hashCode())
				+ " but wanted: " + String.valueOf(hwHashCode), hw1.hashCode() == hwHashCode);
		AssertJUnit.assertTrue("The empty string \"\" did not hash to zero", 0 == "".hashCode());
	}

	/*[PR CMVC 201361] String.equals fails to return consistent output for volatile String */
	// There was a time hole between first read of s.hashCode and second read
	//	if another thread does hashcode computing for incoming string object
	private static final String source = "abcdef";
	private static volatile String target = new String(source.getBytes());
	private static volatile boolean equalsPassed = true;
	private static long defaultDuration = 15000000000L; // ns

	private static class Equals extends Thread {
		public void run() {
			long start = System.nanoTime();
			do {
				for (int i = 0; i < 100000; i++) {
					if (!source.equals(target)) {
						equalsPassed = false;
						break;
					}
				}
				long now = System.nanoTime();
				if ((now - start) > defaultDuration || !equalsPassed) {
					break;
				}
			} while (true);
		}
	}

	private static class HashCode extends Thread {
		public void run() {
			long start = System.nanoTime();
			do {
				for (int i = 0; i < 100000; i++) {
					target.hashCode();
					target = new String(source.getBytes());
				}
				long now = System.nanoTime();
				if ((now - start) > defaultDuration || !equalsPassed) {
					break;
				}
			} while (true);
		}
	}

	/**
	 * @tests java.lang.String#hashCode()
	 */
	@Test
	public void test_hashCode_multithread() {
		HashCode hc = new HashCode();
		Equals e = new Equals();
		source.hashCode();
		hc.start();
		e.start();
		try {
			hc.join();
			e.join();
		} catch (InterruptedException e1) {
			logger.error("InterruptedException thrown: " + e1);
		}
		AssertJUnit.assertTrue("equals should return TRUE!!", equalsPassed);
	}

	/**
	 * @tests java.lang.String#indexOf(int)
	 */
	@Test
	public void test_indexOf() {
		AssertJUnit.assertTrue("Invalid index returned", 1 == hw1.indexOf('e'));

		AssertJUnit.assertTrue("Could not find first surrogate", "a\ud800\udc00".indexOf(0x10000) == 1);
		AssertJUnit.assertTrue("Could not find last surrogate", "a\udbff\udfff".indexOf(0x10ffff) == 1);
		AssertJUnit.assertTrue("Found surrogate", "a\ud800\udc00".indexOf(0x10001) == -1);
		AssertJUnit.assertTrue("Could not find surrogate part1", "a\ud800\udc00".indexOf(0xd800) == 1);
		AssertJUnit.assertTrue("Could not find surrogate part2", "a\ud800\udc00".indexOf(0xdc00) == 2);
	}

	/**
	 * @tests java.lang.String#indexOf(int, int)
	 */
	@Test
	public void test_indexOf2() {
		AssertJUnit.assertTrue("Invalid character index returned", 5 == hw1.indexOf('W', 2));
		String sub = "eeeetesteeee".substring(4, 8);
		AssertJUnit.assertTrue("search should start at zero1", sub.indexOf('e', -3) == 1);
		AssertJUnit.assertTrue("search should start at zero2", sub.indexOf('e', Integer.MIN_VALUE) == 1);
		AssertJUnit.assertTrue("search should end at length1", sub.indexOf('e', 4) == -1);
		AssertJUnit.assertTrue("search should end at length2", sub.indexOf('e', Integer.MAX_VALUE) == -1);

		AssertJUnit.assertTrue("Could not find surrogate1", "ab\ud800\udc00".indexOf(0x10000, 1) == 2);
		AssertJUnit.assertTrue("Could not find surrogate2", "ab\ud800\udc00".indexOf(0x10000, 2) == 2);
		AssertJUnit.assertTrue("Found surrogate1", "ab\ud800\udc00".indexOf(0x10001, 1) == -1);
		AssertJUnit.assertTrue("Found surrogate2", "ab\ud800\udc00".indexOf(0x10000, 3) == -1);
		AssertJUnit.assertTrue("Could not find surrogate part1", "ab\ud800\udc00".indexOf(0xd800, 1) == 2);
		AssertJUnit.assertTrue("Could not find surrogate part2", "ab\ud800\udc00".indexOf(0xdc00, 1) == 3);
	}


	/**
	 * @tests java.lang.String#indexOf(java.lang.String)
	 */
	@Test
	public void test_indexOf3() {
		AssertJUnit.assertTrue("Failed to find string", hw1.indexOf("World") > 0);
		AssertJUnit.assertTrue("Failed to find string", !(hw1.indexOf("ZZ") > 0));
	}

	/**
	 * @tests java.lang.String#indexOf(java.lang.String, int)
	 */
	@Test
	public void test_indexOf4() {
		AssertJUnit.assertTrue("Failed to find string", hw1.indexOf("World", 0) > 0);
		AssertJUnit.assertTrue("Found string outside index", !(hw1.indexOf("Hello", 6) > 0));
		AssertJUnit.assertTrue("Did not accept valid negative starting position", hello1.indexOf("", -5) == 0);
		/* [PR 107673] "abc".indexOf("", 3) should return 3, not -1 */
		AssertJUnit.assertTrue("Reported wrong error code", hello1.indexOf("", 5) == 5);
		AssertJUnit.assertTrue("Wrong for empty in empty", "".indexOf("", 0) == 0);
		String sub = "eeeetesteeee".substring(4, 8);
		AssertJUnit.assertTrue("search should start at zero1", sub.indexOf("e", -3) == 1);
		AssertJUnit.assertTrue("search should start at zero2", sub.indexOf("e", Integer.MIN_VALUE) == 1);
		AssertJUnit.assertTrue("search should end at length1", sub.indexOf("e", 4) == -1);
		AssertJUnit.assertTrue("search should end at length2", sub.indexOf("e", Integer.MAX_VALUE) == -1);
	}

	/**
	 * @tests java.lang.String#intern()
	 */
	@Test
	public void test_intern() {
		AssertJUnit.assertTrue("Intern returned incorrect result", hw1.intern() == hw2.intern());
	}

	/**
	 * @tests java.lang.String#lastIndexOf(int)
	 */
	@Test
	public void test_lastIndexOf() {
		AssertJUnit.assertTrue("Failed to return correct index", hw1.lastIndexOf('W') == 5);
		AssertJUnit.assertTrue("Returned index for non-existent char", hw1.lastIndexOf('Z') == -1);

		AssertJUnit.assertTrue("Could not find first surrogate", "a\ud800\udc00z".lastIndexOf(0x10000) == 1);
		AssertJUnit.assertTrue("Could not find last surrogate", "a\udbff\udfffz".lastIndexOf(0x10ffff) == 1);
		AssertJUnit.assertTrue("Found surrogate", "a\ud800\udc00z".lastIndexOf(0x10001) == -1);
		AssertJUnit.assertTrue("Could not find surrogate part1", "a\ud800\udc00z".lastIndexOf(0xd800) == 1);
		AssertJUnit.assertTrue("Could not find surrogate part2", "a\ud800\udc00z".lastIndexOf(0xdc00) == 2);
	}

	/**
	 * @tests java.lang.String#lastIndexOf(int, int)
	 */
	@Test
	public void test_lastIndexOf2() {
		AssertJUnit.assertTrue("Failed to return correct index", hw1.lastIndexOf('W', 6) == 5);
		AssertJUnit.assertTrue("Returned index for char out of specified range", hw1.lastIndexOf('W', 4) == -1);
		AssertJUnit.assertTrue("Returned index for non-existent char", hw1.lastIndexOf('Z', 9) == -1);
		String sub = "eeeetesteeee".substring(4, 8);
		AssertJUnit.assertTrue("search should start at length1", sub.lastIndexOf('e', 6) == 1);
		AssertJUnit.assertTrue("search should start at length2", sub.lastIndexOf('e', Integer.MAX_VALUE) == 1);
		AssertJUnit.assertTrue("search should end at zero1", sub.lastIndexOf('e', 0) == -1);
		AssertJUnit.assertTrue("search should end at zero2", sub.lastIndexOf('e', Integer.MIN_VALUE) == -1);

		AssertJUnit.assertTrue("Could not find surrogate1", "a\ud800\udc00yz".lastIndexOf(0x10000, 3) == 1);
		AssertJUnit.assertTrue("Could not find surrogate2", "a\ud800\udc00yz".lastIndexOf(0x10000, 1) == 1);
		AssertJUnit.assertTrue("Found surrogate1", "a\ud800\udc00yz".lastIndexOf(0x10001, 3) == -1);
		AssertJUnit.assertTrue("Found surrogate2", "a\ud800\udc00z".lastIndexOf(0x10000, 0) == -1);
		AssertJUnit.assertTrue("Could not find surrogate part1", "a\ud800\udc00yz".lastIndexOf(0xd800, 3) == 1);
		AssertJUnit.assertTrue("Could not find surrogate part2", "a\ud800\udc00yz".lastIndexOf(0xdc00, 3) == 2);
	}


	/**
	 * @tests java.lang.String#lastIndexOf(java.lang.String)
	 */
	@Test
	public void test_lastIndexOf3() {
		AssertJUnit.assertTrue("Returned incorrect index", hw1.lastIndexOf("World") == 5);
		AssertJUnit.assertTrue("Found String outside of index", hw1.lastIndexOf("HeKKKKKKKK") == -1);
	}

	/**
	 * @tests java.lang.String#lastIndexOf(java.lang.String, int)
	 */
	@Test
	public void test_lastIndexOf4() {
		AssertJUnit.assertTrue("Returned incorrect index", hw1.lastIndexOf("World", 9) == 5);
		int result = hw1.lastIndexOf("Hello", 2);
		AssertJUnit.assertTrue("Found String outside of index: " + result, result == 0);
		AssertJUnit.assertTrue("Reported wrong error code", hello1.lastIndexOf("", -5) == -1);
		AssertJUnit.assertTrue("Did not accept valid large starting position", hello1.lastIndexOf("", 5) == 5);
		String sub = "eeeetesteeee".substring(4, 8);
		AssertJUnit.assertTrue("search should start at length1", sub.lastIndexOf("e", 6) == 1);
		AssertJUnit.assertTrue("search should start at length2", sub.lastIndexOf("e", Integer.MAX_VALUE) == 1);
		AssertJUnit.assertTrue("search should end at zero1", sub.lastIndexOf("e", 0) == -1);
		AssertJUnit.assertTrue("search should end at zero2", sub.lastIndexOf("e", Integer.MIN_VALUE) == -1);
	}

	/**
	 * @tests java.lang.String#length()
	 */
	@Test
	public void test_length() {
		AssertJUnit.assertTrue("Invalid length returned", comp11.length() == 11);
	}

	/**
	 * @tests java.lang.String#regionMatches(int, java.lang.String, int, int)
	 */
	@Test
	public void test_regionMatches() {
		String bogusString = "xxcedkedkleiorem lvvwr e''' 3r3r 23r";

		AssertJUnit.assertTrue("identical regions failed comparison", hw1.regionMatches(2, hw2, 2, 5));
		AssertJUnit.assertTrue("Different regions returned true", !hw1.regionMatches(2, bogusString, 2, 5));
	}

	/**
	 * @tests java.lang.String#regionMatches(boolean, int, java.lang.String,
	 *        int, int)
	 */
	@Test
	public void test_regionMatches2() {
		String bogusString = "xxcedkedkleiorem lvvwr e''' 3r3r 23r";

		AssertJUnit.assertTrue("identical regions failed comparison", hw1.regionMatches(false, 2, hw2, 2, 5));
		AssertJUnit.assertTrue("identical regions failed comparison with different cases",
				hw1.regionMatches(true, 2, hw2, 2, 5));
		AssertJUnit.assertTrue("Different regions returned true", !hw1.regionMatches(true, 2, bogusString, 2, 5));
		AssertJUnit.assertTrue("identical regions failed comparison with different cases",
				hw1.regionMatches(false, 2, hw2, 2, 5));
		if (VersionCheck.major() >= 16) {
			AssertJUnit.assertTrue("DESERET CAPITAL LETTER LONG I and DESERET SMALL LETTER LONG I should match when case insensitive",
				"\ud801\udc00".regionMatches(true, 0, "\ud801\udc28", 0 ,2));
			AssertJUnit.assertFalse("low surrogate only of a Deseret capital/small letter pair at end of region should not match when case insensitive",
				"A\udc00B".regionMatches(true, 0, "A\udc28B", 0 ,2));
		}
	}

	/**
	 * @tests java.lang.String#replace(char, char)
	 */
	@Test
	public void test_replace() {
		AssertJUnit.assertTrue("Failed replace", hw1.replace('l', 'z').equals("HezzoWorzd"));
	}

	/**
	 * @tests java.lang.String#replace(java.lang.CharSequence,
	 *        java.lang.CharSequence)
	 */
	@Test
	public void test_replace2() {
		/* [PR CMVC 98959] string replace incorrect */
		AssertJUnit.assertTrue("Failed replace 1",
				"build1 build2 buff boil".replace("build", "create").equals("create1 create2 buff boil"));
		/* [PR CMVC 99521] string replace incorrect */
		AssertJUnit.assertTrue("Failed replace 2", "A testing string".replace("A test", "").equals("ing string"));
	}

	/**
	 * @tests java.lang.String#startsWith(java.lang.String)
	 */
	@Test
	public void test_startsWith() {
		AssertJUnit.assertTrue("Failed to find string", hw1.startsWith("Hello"));
		AssertJUnit.assertTrue("Found incorrect string", !hw1.startsWith("T"));
	}

	/**
	 * @tests java.lang.String#startsWith(java.lang.String, int)
	 */
	@Test
	public void test_startsWith2() {
		AssertJUnit.assertTrue("Failed to find string", hw1.startsWith("World", 5));
		AssertJUnit.assertTrue("Found incorrect string", !hw1.startsWith("Hello", 5));
	}

	/**
	 * @tests java.lang.String#substring(int)
	 */
	@Test
	public void test_substring() {
		AssertJUnit.assertTrue("Incorrect substring returned", hw1.substring(5).equals("World"));
		/* [PR 115691] Unmodified substring() should return identical String */
		AssertJUnit.assertTrue("not identical", hw1.substring(0) == hw1);
	}

	/**
	 * @tests java.lang.String#substring(int, int)
	 */
	@Test
	public void test_substring2() {
		AssertJUnit.assertTrue("Incorrect substring returned",
				hw1.substring(0, 5).equals("Hello") && (hw1.substring(5, 10).equals("World")));
		/* [PR 115691] Unmodified substring() should return identical String */
		AssertJUnit.assertTrue("not identical", hw1.substring(0, hw1.length()) == hw1);
	}

	/**
	 * @tests java.lang.String#toCharArray()
	 */
	@Test
	public void test_toCharArray() {
		String s = new String(buf, 0, buf.length);
		char[] schars = s.toCharArray();
		// Spec seems to indicate that this should be true, but the JDK does not
		// return
		// a buffer of the correct length either.
		// assertTrue("Returned array is of different size", buf.length !=
		// schars.length);
		for (int i = 0; i < s.length(); i++)
			AssertJUnit.assertTrue("Returned incorrect char array", buf[i] == schars[i]);
	}

	/**
	 * @tests java.lang.String#toLowerCase()
	 */
	@Test
	public void test_toLowerCase() {
		AssertJUnit.assertTrue("toLowerCase case conversion did not succeed", hwuc.toLowerCase().equals(hwlc));

		// Long strings and strings of lengths near multiples of 16 bytes to test vector HW acceleration edge cases.
		Assert.assertEquals("THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG".toLowerCase(), "the quick brown fox jumps over the lazy dog",
			"toLowerCase case conversion failed");
		Assert.assertEquals("".toLowerCase(), "",
			"toLowerCase case conversion failed");
		Assert.assertEquals("A".toLowerCase(), "a",
			"toLowerCase case conversion failed");
		Assert.assertEquals("ABCD".toLowerCase(), "abcd",
			"toLowerCase case conversion failed");
		Assert.assertEquals("ABCDEFG".toLowerCase(), "abcdefg",
			"toLowerCase case conversion failed");
		Assert.assertEquals("ABCDEFGH".toLowerCase(), "abcdefgh",
			"toLowerCase case conversion failed");
		Assert.assertEquals("ABCDEFGHI".toLowerCase(), "abcdefghi",
			"toLowerCase case conversion failed");
		Assert.assertEquals("ABCDEFGHIJKLMNO".toLowerCase(), "abcdefghijklmno",
			"toLowerCase case conversion failed");
		Assert.assertEquals("ABCDEFGHIJKLMNOP".toLowerCase(), "abcdefghijklmnop",
			"toLowerCase case conversion failed");
		Assert.assertEquals("ABCDEFGHIJKLMNOPQ".toLowerCase(), "abcdefghijklmnopq",
			"toLowerCase case conversion failed");
		Assert.assertEquals("ABCDEFGHIJKLMNOPQRSTUVWXYZ\u00C0\u00C1\u00c2\u00C3\u00C4".toLowerCase(),
			"abcdefghijklmnopqrstuvwxyz\u00E0\u00E1\u00E2\u00E3\u00E4",
			"toLowerCase case conversion failed");
		Assert.assertEquals("ABCDEFGHIJKLMNOPQRSTUVWXYZ\u00C0\u00C1\u00c2\u00C3\u00C4\u00C5".toLowerCase(),
			"abcdefghijklmnopqrstuvwxyz\u00E0\u00E1\u00E2\u00E3\u00E4\u00E5",
			"toLowerCase case conversion failed");
		Assert.assertEquals("ABCDEFGHIJKLMNOPQRSTUVWXYZ\u00C0\u00C1\u00c2\u00C3\u00C4\u00C5\u00C6".toLowerCase(),
			"abcdefghijklmnopqrstuvwxyz\u00E0\u00E1\u00E2\u00E3\u00E4\u00E5\u00E6",
			"toLowerCase case conversion failed");

		/* [PR 111901] toLowerCase(), toUpperCase() should use codePoints */
		for (int i = 0x10000; i < 0x10FFFF; i++) {
			StringBuilder builder = new StringBuilder();
			builder.appendCodePoint(i);
			String lower = builder.toString().toLowerCase();
			AssertJUnit.assertTrue("invalid toLower " + Integer.toHexString(i),
					lower.codePointAt(0) == Character.toLowerCase(i));
		}

		class TestHelp {
			private boolean isWordPart(int codePoint) {
				return codePoint == 0x345 || isWordStart(codePoint);
			}

			private boolean isWordStart(int codePoint) {
				int type = Character.getType(codePoint);
				return (type >= Character.UPPERCASE_LETTER && type <= Character.TITLECASE_LETTER)
						|| (codePoint >= 0x2b0 && codePoint <= 0x2b8) || (codePoint >= 0x2c0 && codePoint <= 0x2c1)
						|| (codePoint >= 0x2e0 && codePoint <= 0x2e4) || codePoint == 0x37a
						|| (codePoint >= 0x2160 && codePoint <= 0x217f) || (codePoint >= 0x1d2c && codePoint <= 0x1d61);
			}
		}
		TestHelp helper = new TestHelp();

		/*
		 * [PR CMVC 85206] Sigma has different lower case value at end of String
		 * with Unicode 4.0
		 */
		AssertJUnit.assertTrue("a) Sigma has different lower case value at end of word with Unicode 4.0",
				"\u03a3".toLowerCase().equals("\u03c3"));
		String test1 = "\u03a3 a\u03a3c de\u03a3 f\u03a3\u03a3 \u03a3- g\u03a3- h\u03a3. i\u03a31 j\u03a3";
		String expt1 = "\u03c3 a\u03c3c de\u03c2 f\u03c3\u03c2 \u03c3- g\u03c2- h\u03c2. i\u03c21 j\u03c2";
		String result1 = test1.toLowerCase();
		AssertJUnit.assertTrue("b) Sigma has different lower case value at end of word with Unicode 4.0",
				result1.equals(expt1));
		boolean passed = true;
		for (int i = 0; i < 0x10FFFF; i++) {
			try {
				char ch = (char)i;
				StringBuffer buf = new StringBuffer("a\u03a3");
				buf.appendCodePoint(i);
				String t1 = buf.toString();
				String r1 = t1.toLowerCase();
				boolean result;
				if (helper.isWordPart(i)) {
					result = r1.charAt(1) == '\u03c3';
				} else {
					result = r1.charAt(1) == '\u03c2';
				}
				if (!result) {
					passed = false;
					logger.error("1 Unexpected sigma conversion " + Integer.toHexString(r1.charAt(1)) + " at: "
							+ Integer.toHexString(i) + " " + Character.getType(i));
				}
			} catch (NullPointerException e) {
				// occurs on JDK 1.5 on Character.toLowerCase(0xffff);
				e.printStackTrace();
				Assert.fail("Unexpected exception" + e.toString() + "\n" + "Integer.toHexString(i) is : " + Integer.toHexString(i));
			}
		}
		for (int i = 0; i < 0x10FFFF; i++) {
			char ch = (char)i;
			StringBuffer buf = new StringBuffer();
			buf.appendCodePoint(i);
			buf.append("\u03a3 ");
			String t1 = buf.toString();
			String r1 = t1.toLowerCase();
			boolean result;
			int offset = r1.length() - 2;
			if (helper.isWordStart(i)) {
				result = r1.charAt(offset) == '\u03c2';
			} else {
				result = r1.charAt(offset) == '\u03c3';
			}
			if (!result) {
				passed = false;
				logger.error("2 Unexpected sigma conversion " + Integer.toHexString(r1.charAt(offset)) + " at: "
						+ Integer.toHexString(i) + " " + Character.getType(i));
			}
		}
		AssertJUnit.assertTrue("Invalid sigma conversion (see test output)", passed);
	}

	/**
	 * @tests java.lang.String#toLowerCase(java.util.Locale)
	 */
	@Test
	public void test_toLowerCase2() {
		AssertJUnit.assertTrue("toLowerCase case conversion did not succeed",
				hwuc.toLowerCase(java.util.Locale.getDefault()).equals(hwlc));
		AssertJUnit.assertTrue("Invalid \\u0049 for English", "\u0049".toLowerCase(Locale.ENGLISH).equals("\u0069"));
		AssertJUnit.assertTrue("Invalid \\u0049 for Turkish",
				"\u0049".toLowerCase(new Locale("tr", "")).equals("\u0131"));
		/* [PR 111909] Unicode 4.0 special case mappings for tr, az */
		AssertJUnit.assertTrue("Invalid \\u0049 for Azeri", "\u0049".toLowerCase(new Locale("az")).equals("\u0131"));
		AssertJUnit.assertTrue("Invalid \\u0049 \\u0307 for Turkish",
				"\u0049\u0307".toLowerCase(new Locale("tr")).equals("\u0069"));
		AssertJUnit.assertTrue("Invalid \\u0049 \\u0307 for Azeri",
				"\u0049\u0307".toLowerCase(new Locale("az")).equals("\u0069"));
	}

	/**
	 * @tests java.lang.String#toLowerCase(java.util.Locale)
	 */
	@Test
	public void test_toLowerCase4() {
		AssertJUnit.assertTrue("Wrong conversion 0x0130 with Locale tr",
				"\u0130".toLowerCase(new Locale("tr", "TR")).equals("i"));
		AssertJUnit.assertTrue("Wrong conversion 0x0130 with Locale az",
				"\u0130".toLowerCase(new Locale("az")).equals("i"));
		AssertJUnit.assertTrue("Wrong conversion 0x0130 with Locale lt",
				"\u0130".toLowerCase(new Locale("lt")).equals("\u0069\u0307"));
		AssertJUnit.assertTrue("Wrong conversion 0x0130 with Locale.US",
				"\u0130".toLowerCase(Locale.US).equals("\u0069\u0307"));
		AssertJUnit.assertTrue("Wrong conversion 0x0130 with Locale.JAPAN",
				"\u0130".toLowerCase(Locale.JAPAN).equals("\u0069\u0307"));
		AssertJUnit.assertTrue("Wrong conversion 0x0130 with Locale.ROOT",
				"\u0130".toLowerCase(Locale.ROOT).equals("\u0069\u0307"));
	}

	private static String writeString(String in) {
		StringBuffer result = new StringBuffer();
		result.append("\"");
		for (int i = 0; i < in.length(); i++) {
			result.append(" 0x" + Integer.toHexString(in.charAt(i)));
		}
		result.append("\"");
		return result.toString();
	}

	/**
	 * @tests java.lang.String#toString()
	 */
	@Test
	public void test_toString() {
		AssertJUnit.assertTrue("Incorrect string returned", hw1.toString().equals(hw1));
	}

	/**
	 * @tests java.lang.String#toUpperCase()
	 */
	@Test
	public void test_toUpperCase() {
		AssertJUnit.assertTrue("Returned string is not UpperCase", hwlc.toUpperCase().equals(hwuc));

		AssertJUnit.assertTrue("Wrong conversion", "\u00df".toUpperCase().equals("SS"));

		// Long strings and strings of lengths near multiples of 16 bytes to test vector HW acceleration edge cases.
		Assert.assertEquals("the quick brown fox jumps over the lazy dog".toUpperCase(), "THE QUICK BROWN FOX JUMPS OVER THE LAZY DOG",
			"toUpperCase case conversion failed");
		Assert.assertEquals("".toUpperCase(), "",
			"toUpperCase case conversion failed");
		Assert.assertEquals("a".toUpperCase(), "A",
			"toUpperCase case conversion failed");
		Assert.assertEquals("abcd".toUpperCase(), "ABCD",
			"toUpperCase case conversion failed");
		Assert.assertEquals("abcdefg".toUpperCase(), "ABCDEFG",
			"toUpperCase case conversion failed");
		Assert.assertEquals("abcdefgh".toUpperCase(), "ABCDEFGH",
			"toUpperCase case conversion failed");
		Assert.assertEquals("abcdefghi".toUpperCase(), "ABCDEFGHI",
			"toUpperCase case conversion failed");
		Assert.assertEquals("abcdefghijklmno".toUpperCase(), "ABCDEFGHIJKLMNO",
			"toUpperCase case conversion failed");
		Assert.assertEquals("abcdefghijklmnop".toUpperCase(), "ABCDEFGHIJKLMNOP",
			"toUpperCase case conversion failed");
		Assert.assertEquals("abcdefghijklmnopq".toUpperCase(), "ABCDEFGHIJKLMNOPQ",
			"toUpperCase case conversion failed");
		Assert.assertEquals("abcdefghijklmnopqrstuvwxyz\u00E0\u00E1\u00E2\u00E3\u00E4".toUpperCase(),
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ\u00C0\u00C1\u00c2\u00C3\u00C4",
			"toUpperCase case conversion failed");
		Assert.assertEquals("abcdefghijklmnopqrstuvwxyz\u00E0\u00E1\u00E2\u00E3\u00E4\u00E5".toUpperCase(),
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ\u00C0\u00C1\u00c2\u00C3\u00C4\u00C5",
			"toUpperCase case conversion failed");
		Assert.assertEquals("abcdefghijklmnopqrstuvwxyz\u00E0\u00E1\u00E2\u00E3\u00E4\u00E5\u00E6".toUpperCase(),
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ\u00C0\u00C1\u00c2\u00C3\u00C4\u00C5\u00C6",
			"toUpperCase case conversion failed");

		/*
		 * [PR CMVC 79544 "a\u00df\u1f56".toUpperCase() throws
		 * IndexOutOfBoundsException
		 */
		String s = "a\u00df\u1f56";
		AssertJUnit.assertTrue("Invalid conversion", !s.toUpperCase().equals(s));

		/* [PR 111901] toLowerCase(), toUpperCase() should use codePoints */
		for (int i = 0x10000; i < 0x10FFFF; i++) {
			StringBuilder builder = new StringBuilder();
			builder.appendCodePoint(i);
			String upper = builder.toString().toUpperCase();
			AssertJUnit.assertTrue("invalid toUpper " + Integer.toHexString(i),
					upper.codePointAt(0) == Character.toUpperCase(i));
		}
	}

	/**
	 * @tests java.lang.String#toUpperCase(java.util.Locale)
	 */
	@Test
	public void test_toUpperCase2() {
		AssertJUnit.assertTrue("Returned string is not UpperCase", hwlc.toUpperCase().equals(hwuc));
		AssertJUnit.assertTrue("Invalid \\u0069 for English", "\u0069".toUpperCase(Locale.ENGLISH).equals("\u0049"));
		AssertJUnit.assertTrue("Invalid \\u0069 for Turkish",
				"\u0069".toUpperCase(new Locale("tr", "")).equals("\u0130"));
		/* [PR 111909] Unicode 4.0 special case mappings for tr, az */
		AssertJUnit.assertTrue("Invalid \\u0069 for Azeri", "\u0069".toUpperCase(new Locale("az")).equals("\u0130"));
	}

	/**
	 * @tests java.util.Locale#getLanguage()
	 */
	@Test
	public void test_localeLanguageReferenceEquality() {
		AssertJUnit.assertTrue("String constructor failed to create new string for Local langauge testing",
				new String("en") != "en");
		AssertJUnit.assertTrue("Non-interned Locale language string from Locale.US", Locale.US.getLanguage() == "en");
		AssertJUnit.assertTrue("Non-interned Locale language string from Locale.<init>(String)",
				(new Locale(new String("en"))).getLanguage() == "en");
		AssertJUnit.assertTrue("Non-interned Locale language string from Locale.<init>(String,String)",
				(new Locale(new String("en"), "GB")).getLanguage() == "en");
		AssertJUnit.assertTrue("Non-interned Locale language string from Locale.<init>(String,String,String)",
				(new Locale(new String("en"), "GB", "emodeng")).getLanguage() == "en");
		AssertJUnit.assertTrue("Non-normalized Locale language string from Local.<init>(String)",
				(new Locale("EN")).getLanguage() == "en");
		AssertJUnit.assertTrue("Non-normalized Locale language string from Local.<init>(String,String)",
				(new Locale("EN", "GB")).getLanguage() == "en");
		AssertJUnit.assertTrue("Non-normalized Locale language string from Local.<init>(String,String,String)",
				(new Locale("EN", "GB", "emodeng")).getLanguage() == "en");

		Locale.Builder builder = new Locale.Builder();
		builder.setLanguage(new String("en"));

		AssertJUnit.assertTrue("Non-interned Locale language string from Local.Builder.setLanguage(String)",
				builder.build().getLanguage() == "en");

		builder = new Locale.Builder();
		builder.setLanguageTag(new String("en"));
		AssertJUnit.assertTrue("Non-interned Locale language string from Local.Builder.setLanguageTag(String)",
				builder.build().getLanguage() == "en");

		AssertJUnit.assertTrue("Non-interned Locale language string from Local.forLanguageTag(String) with \"en\"",
				Locale.forLanguageTag(new String("en")).getLanguage() == "en");
		AssertJUnit.assertTrue("Non-interned Locale language string from Local.forLanguageTag(String) with \"en-US\"",
				Locale.forLanguageTag("en-US").getLanguage() == "en");

	}

	/**
	 * @tests java.lang.String#trim()
	 */
	@Test
	public void test_trim() {
		AssertJUnit.assertTrue("Incorrect string returned", " HelloWorld ".trim().equals(hw1));
	}

	/**
	 * @tests java.lang.String#valueOf(char[])
	 */
	@Test
	public void test_valueOf() {
		AssertJUnit.assertTrue("Returned incorrect String", String.valueOf(buf).equals("World"));
	}

	/**
	 * @tests java.lang.String#valueOf(char[], int, int)
	 */
	@Test
	public void test_valueOf2() {
		char[] t = { 'H', 'e', 'l', 'l', 'o', 'W', 'o', 'r', 'l', 'd' };
		AssertJUnit.assertTrue("copyValueOf returned incorrect String", String.valueOf(t, 5, 5).equals("World"));
	}

	/**
	 * @tests java.lang.String#valueOf(char)
	 */
	@Test
	public void test_valueOf3() {
		for (int i = 0; i < 65536; i++)
			AssertJUnit.assertTrue("Incorrect valueOf(char) returned: " + i,
					String.valueOf((char)i).charAt(0) == (char)i);
	}

	/**
	 * @tests java.lang.String#valueOf(double)
	 */
	@Test
	public void test_valueOf4() {
		AssertJUnit.assertTrue("Incorrect double string returned",
				String.valueOf(Double.MAX_VALUE).equals("1.7976931348623157E308"));
	}

	/**
	 * @tests java.lang.String#valueOf(float)
	 */
	@Test
	public void test_valueOf5() {
		AssertJUnit.assertTrue("incorrect float string returned--got: " + String.valueOf(1.0F) + " wanted: 1.0",
				String.valueOf(1.0F).equals("1.0"));
		AssertJUnit.assertTrue("incorrect float string returned--got: " + String.valueOf(0.9F) + " wanted: 0.9",
				String.valueOf(0.9F).equals("0.9"));
		AssertJUnit.assertTrue("incorrect float string returned--got: " + String.valueOf(109.567F) + " wanted: 109.567",
				String.valueOf(109.567F).equals("109.567"));
	}

	/**
	 * @tests java.lang.String#valueOf(int)
	 */
	@Test
	public void test_valueOf6() {
		AssertJUnit.assertTrue("returned invalid int string", String.valueOf(1).equals("1"));
	}

	/**
	 * @tests java.lang.String#valueOf(long)
	 */
	@Test
	public void test_valueOf7() {
		AssertJUnit.assertTrue("returned incorrect long string", String.valueOf(927654321098L).equals("927654321098"));
	}

	/**
	 * @tests java.lang.String#valueOf(java.lang.Object)
	 */
	@Test
	public void test_valueOf8() {
		AssertJUnit.assertTrue("Incorrect Object string returned", obj.toString().equals(String.valueOf(obj)));
	}

	/**
	 * @tests java.lang.String#valueOf(boolean)
	 */
	@Test
	public void test_valueOf9() {
		AssertJUnit.assertTrue("Incorrect boolean string returned",
				String.valueOf(false).equals("false") && (String.valueOf(true).equals("true")));
	}

	/**
	 * @tests java.lang.String#codePointAt(int)
	 */
	@Test
	public void test_codePointAt() {
		String s1 = "A\ud800\udc00C";
		AssertJUnit.assertTrue("not A", s1.codePointAt(0) == 'A');
		AssertJUnit.assertTrue("not 0x10000", s1.codePointAt(1) == 0x10000);
		AssertJUnit.assertTrue("not 0xdc00", s1.codePointAt(2) == 0xdc00);
		AssertJUnit.assertTrue("not C", s1.codePointAt(3) == 'C');
	}

	/**
	 * @tests java.lang.String#codePointBefore(int)
	 */
	@Test
	public void test_codePointBefore() {
		String s1 = "A\ud800\udc00C";
		AssertJUnit.assertTrue("not A", s1.codePointBefore(1) == 'A');
		AssertJUnit.assertTrue("not 0xd800", s1.codePointBefore(2) == 0xd800);
		AssertJUnit.assertTrue("not 0x10000", s1.codePointBefore(3) == 0x10000);
		AssertJUnit.assertTrue("not C", s1.codePointBefore(4) == 'C');
	}

	/**
	 * @tests java.lang.String#codePointCount(int, int)
	 */
	@Test
	public void test_codePointCount() {
		String s1 = "A\ud800\udc00C";
		AssertJUnit.assertTrue("wrong count 1", s1.codePointCount(0, 4) == 3);
		AssertJUnit.assertTrue("wrong count 2", s1.codePointCount(1, 4) == 2);
	}

	/**
	 * @tests java.lang.String#offsetByCodePoints(int, int)
	 */
	@Test
	public void test_offsetByCodePoints() {
		String s1 = "A\ud800\udc00C";
		AssertJUnit.assertTrue("wrong offset 0, 0", s1.offsetByCodePoints(0, 0) == 0);
		AssertJUnit.assertTrue("wrong offset 0, 1", s1.offsetByCodePoints(0, 1) == 1);
		AssertJUnit.assertTrue("wrong offset 0, 2", s1.offsetByCodePoints(0, 2) == 3);
		AssertJUnit.assertTrue("wrong offset 0, 3", s1.offsetByCodePoints(0, 3) == 4);
		AssertJUnit.assertTrue("wrong offset 1, 0", s1.offsetByCodePoints(1, 0) == 1);
		AssertJUnit.assertTrue("wrong offset 1, 1", s1.offsetByCodePoints(1, 1) == 3);
		AssertJUnit.assertTrue("wrong offset 1, 2", s1.offsetByCodePoints(1, 2) == 4);
		AssertJUnit.assertTrue("wrong offset 2, 0", s1.offsetByCodePoints(2, 0) == 2);
		AssertJUnit.assertTrue("wrong offset 2, 1", s1.offsetByCodePoints(2, 1) == 3);
		AssertJUnit.assertTrue("wrong offset 2, 2", s1.offsetByCodePoints(2, 2) == 4);
		AssertJUnit.assertTrue("wrong offset 3, 0", s1.offsetByCodePoints(3, 0) == 3);
		AssertJUnit.assertTrue("wrong offset 3, 1", s1.offsetByCodePoints(3, 1) == 4);
		AssertJUnit.assertTrue("wrong offset 4, 0", s1.offsetByCodePoints(4, 0) == 4);

		AssertJUnit.assertTrue("wrong offset 4, -1", s1.offsetByCodePoints(4, -1) == 3);
		AssertJUnit.assertTrue("wrong offset 4, -2", s1.offsetByCodePoints(4, -2) == 1);
		AssertJUnit.assertTrue("wrong offset 4, -3", s1.offsetByCodePoints(4, -3) == 0);
		AssertJUnit.assertTrue("wrong offset 3, -1", s1.offsetByCodePoints(3, -1) == 1);
		AssertJUnit.assertTrue("wrong offset 3, -2", s1.offsetByCodePoints(3, -2) == 0);
		AssertJUnit.assertTrue("wrong offset 2, -1", s1.offsetByCodePoints(2, -1) == 1);
		AssertJUnit.assertTrue("wrong offset 2, -2", s1.offsetByCodePoints(2, -2) == 0);
		AssertJUnit.assertTrue("wrong offset 1, -1", s1.offsetByCodePoints(1, -1) == 0);
	}

	/**
	 * @tests java.lang.String
	 */
	@Test
	public void test_stringAppend() {
		/*
		 * [PR CMVC 100597] Optimize heap allocations for String(String, int)
		 * jit optimization
		 */
		AssertJUnit.assertEquals("hello" + 0, "hello0");
		AssertJUnit.assertEquals("hello" + 1, "hello1");
		AssertJUnit.assertEquals("hello" + -1, "hello-1");
		AssertJUnit.assertEquals("hello" + 5, "hello5");
		AssertJUnit.assertEquals("hello" + -5, "hello-5");
		AssertJUnit.assertEquals("hello" + 10, "hello10");
		AssertJUnit.assertEquals("hello" + -10, "hello-10");
		AssertJUnit.assertEquals("hello" + 854, "hello854");
		AssertJUnit.assertEquals("hello" + -854, "hello-854");
		AssertJUnit.assertEquals("hello" + Integer.MAX_VALUE, "hello2147483647");
		AssertJUnit.assertEquals("hello" + Integer.MIN_VALUE, "hello-2147483648");
	}

	/**
	 * @tests java.lang.String#isEmpty()
	 */
	@Test
	public void test_isEmpty() {
		AssertJUnit.assertTrue("1", "".isEmpty());
		AssertJUnit.assertTrue("2", "abc".substring(0, 0).isEmpty());
		AssertJUnit.assertTrue("2a", "abc".substring(1, 1).isEmpty());
		AssertJUnit.assertTrue("3", new StringBuffer().toString().isEmpty());
		AssertJUnit.assertTrue("3a", new StringBuffer("abc").substring(0, 0).isEmpty());
		AssertJUnit.assertTrue("3b", new StringBuffer("abc").substring(1, 1).isEmpty());
		AssertJUnit.assertTrue("4", new StringBuilder().toString().isEmpty());
		AssertJUnit.assertTrue("4a", new StringBuilder("abc").substring(0, 0).isEmpty());
		AssertJUnit.assertTrue("4a", new StringBuilder("abc").substring(1, 1).isEmpty());
	}

	/**
	 * @tests java.lang.String#concat(java.lang.String)
	 */
	@Test
	public void testConcat() {
		String s1 = "";
		String s2 = "abc";
		String s3 = s2.concat(s1);
		AssertJUnit.assertEquals(s2, s3);
	}

	/**
	 * @tests java.lang.String#concat(java.lang.String)
	 */
	@Test
	public void testConcat2() {
		String s1 = "";
		String s2 = "abc";
		String s3 = s1.concat(s2);
		AssertJUnit.assertFalse(s2 == s3);
	}


	/**
	 * @tests java.lang.String#concat(java.lang.String)
	 */
	@Test
	public void testConcat3() {
		String s1 = "";
		String s2 = "abc";
		String s3 = s1.concat(s1);
		AssertJUnit.assertEquals(s1, s3);
	}

	/**
	 * @tests java.lang.String#join(java.lang.CharSequence, java.lang.CharSequence...)
	 */
	@Test
	public void test_join1() {
		boolean exception;
		CharSequence del;
		CharSequence str1, str2;
		CharSequence[] charSeqArray;
		String result;

		/* CASE 1 */
		del = null;
		str1 = "str1";
		try {
			result = String.join(del, str1);
			exception = false;
		} catch (NullPointerException e) {
			//Expected..
			exception = true;
		}
		AssertJUnit.assertTrue("Failed to throw NPE (case1)", exception);

		/* CASE 2 */
		del = null;
		str1 = "str1";
		str2 = "str2";
		try {
			result = String.join(del, str1, str2);
			exception = false;
		} catch (NullPointerException e) {
			//Expected..
			exception = true;
		}
		AssertJUnit.assertTrue("Failed to throw NPE (case2)", exception);

		/* CASE 3 */
		charSeqArray = null;
		try {
			result = String.join(del, charSeqArray);
			exception = false;
		} catch (NullPointerException e) {
			//Expected..
			exception = true;
		}
		AssertJUnit.assertTrue("Failed to throw NPE (case3)", exception);

		/* CASE 4 */
		del = "del";
		str1 = null;
		try {
			result = String.join(del, str1);
			AssertJUnit.assertTrue(
					"Returned joined String is wrong. Expected : \"null\"; returned : \"" + result + "\"",
					result.equals("null"));
		} catch (NullPointerException e) {
			Assert.fail("NPE is thrown (case4)");
		}

		/* CASE 5 */
		del = "_";
		str1 = null;
		str2 = null;
		try {
			result = String.join(del, str1, str2);
			AssertJUnit.assertTrue(
					"Returned joined String is wrong. Expected : \"null_null\"; returned : \"" + result + "\"",
					result.equals("null_null"));
		} catch (NullPointerException e) {
			Assert.fail("NPE is thrown (case5)");
		}

		/* CASE 6 */
		del = "_";
		str1 = null;
		str2 = "null";
		try {
			result = String.join(del, str1, str2);
			AssertJUnit.assertTrue(
					"Returned joined String is wrong. Expected : \"null_null\"; returned : \"" + result + "\"",
					result.equals("null_null"));
		} catch (NullPointerException e) {
			Assert.fail("NPE is thrown (case6)");
		}

		/* CASE 7 */
		del = "looks ";
		str1 = "This test ";
		str2 = "good";
		try {
			result = String.join(del, str1, str2);
			AssertJUnit.assertTrue("Returned joined String is wrong. Expected : \"This test looks good\"; returned : \""
					+ result + "\"", result.equals("This test looks good"));
		} catch (NullPointerException e) {
			Assert.fail("NPE is thrown (case7)");
		}
	}

	/**
	 * @tests java.lang.String#join(java.lang.CharSequence, java.lang.Iterable)
	 */
	@Test
	public void test_join2() {
		boolean exception;
		ArrayList<String> iterable;
		String result;
		String del;

		/* CASE 1 */
		del = null;
		iterable = new ArrayList<String>();
		iterable.add("test");

		try {
			result = String.join(del, iterable);
			exception = false;
		} catch (NullPointerException e) {
			//Expected..
			exception = true;
		}
		AssertJUnit.assertTrue("Failed to throw NPE (case1)", exception);

		/* CASE 2 */
		del = "del";
		iterable = null;

		try {
			result = String.join(del, iterable);
			exception = false;
		} catch (NullPointerException e) {
			//Expected..
			exception = true;
		}
		AssertJUnit.assertTrue("Failed to throw NPE (case2)", exception);

		/* CASE 3 */
		del = "del";
		iterable = new ArrayList<String>();
		iterable.add("OnlyThis");

		try {
			result = String.join(del, iterable);
			AssertJUnit.assertTrue(
					"Returned joined String is wrong. Expected : \"OnlyThis\"; returned : \"" + result + "\"",
					result.equals("OnlyThis"));
		} catch (NullPointerException e) {
			Assert.fail("NPE is thrown (case3)");
		}

		/* CASE 4 */
		del = "With ";
		iterable = new ArrayList<String>();
		iterable.add("Hockey ");
		iterable.add("Hearth");

		try {
			result = String.join(del, iterable);
			AssertJUnit.assertTrue(
					"Returned joined String is wrong. Expected : \"Hockey With Hearth\"; returned : \"" + result + "\"",
					result.equals("Hockey With Hearth"));
		} catch (NullPointerException e) {
			Assert.fail("NPE is thrown (case4)");
		}
	}

	/**
	 * @tests java.lang.String#replaceAll(String, String)
	 */
	@Test
	public void test_replaceAll_last_char_dollarsign() {
		try {
			"1a2b3c".replaceAll("[0-9]", "$");
			Assert.fail("IAE should be thrown if `$` is the last character in replacement string!");
		} catch (IllegalArgumentException iie) {
			// expected
		}
	}
	/**
	 * @tests java.lang.String#replaceAll(String, String)
	 */
	@Test
	public void test_replaceAll_last_char_backslash() {
		try {
			"1a2b3c".replaceAll("[0-9]", "\\");
			Assert.fail("IAE should be thrown if `\\` is the last character in replacement string!");
		} catch (IllegalArgumentException iie) {
			// expected
		}
	}

}
