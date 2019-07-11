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
package org.openj9.test.java.lang;

import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.openj9.test.support.Support_15Help;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity" })
public class Test_StringBuffer {

	StringBuffer testBuffer;

	/**
	 * @tests java.lang.StringBuffer#StringBuffer()
	 */
	@Test
	public void test_Constructor() {
		new StringBuffer();
		AssertJUnit.assertTrue("Invalid buffer created", true);
	}

	/**
	 * @tests java.lang.StringBuffer#StringBuffer(int)
	 */
	@Test
	public void test_Constructor2() {
		StringBuffer sb = new StringBuffer(8);
		AssertJUnit.assertTrue("Newly constructed buffer is of incorrect length", sb.length() == 0);
	}

	/**
	 * @tests java.lang.StringBuffer#StringBuffer(java.lang.String)
	 */
	@Test
	public void test_Constructor3() {
		StringBuffer sb = new StringBuffer("HelloWorld");

		AssertJUnit.assertTrue("Invalid buffer created", sb.length() == 10 && (sb.toString().equals("HelloWorld")));

		boolean pass = false;
		try {
			new StringBuffer(null);
		} catch (NullPointerException e) {
			pass = true;
		}
		AssertJUnit.assertTrue("Should throw NullPointerException", pass);
	}

	/**
	 * @tests java.lang.StringBuffer#StringBuffer(java.lang.CharSequence)
	 */
	@Test
	public void test_Constructor4() {
		StringBuffer sb = new StringBuffer((CharSequence)"test1");
		AssertJUnit.assertTrue("String", sb.toString().equals("test1"));
		StringBuffer sb2 = new StringBuffer((CharSequence)new StringBuffer("test2"));
		AssertJUnit.assertTrue("StringBuffer", sb2.toString().equals("test2"));
		StringBuffer sb3 = new StringBuffer(Support_15Help.createCharSequence("test3"));
		AssertJUnit.assertTrue("CharSequence", sb3.toString().equals("test3"));
	}

	/**
	 * @tests java.lang.StringBuffer#append(char[])
	 */
	@Test
	public void test_append() {
		char buf[] = new char[4];
		"char".getChars(0, 4, buf, 0);
		testBuffer.append(buf);
		AssertJUnit.assertTrue("Append of char[] failed", testBuffer.toString().equals("This is a test bufferchar"));
	}

	/**
	 * @tests java.lang.StringBuffer#append(char[], int, int)
	 */
	@Test
	public void test_append2() {
		StringBuffer sb = new StringBuffer();
		char[] buf1 = { 'H', 'e', 'l', 'l', 'o' };
		char[] buf2 = { 'W', 'o', 'r', 'l', 'd' };
		sb.append(buf1, 0, buf1.length);
		AssertJUnit.assertTrue("Buffer is invalid length after append", sb.length() == 5);
		sb.append(buf2, 0, buf2.length);
		AssertJUnit.assertTrue("Buffer is invalid length after append", sb.length() == 10);
		AssertJUnit.assertTrue("Buffer contains invalid chars", (sb.toString().equals("HelloWorld")));
	}

	/**
	 * @tests java.lang.StringBuffer#append(char)
	 */
	@Test
	public void test_append3() {
		StringBuffer sb = new StringBuffer();
		char buf1 = 'H';
		char buf2 = 'W';
		sb.append(buf1);
		AssertJUnit.assertTrue("Buffer is invalid length after append", sb.length() == 1);
		sb.append(buf2);
		AssertJUnit.assertTrue("Buffer is invalid length after append", sb.length() == 2);
		AssertJUnit.assertTrue("Buffer contains invalid chars", (sb.toString().equals("HW")));
	}

	/**
	 * @tests java.lang.StringBuffer#append(double)
	 */
	@Test
	public void test_append4() {
		StringBuffer sb = new StringBuffer();
		sb.append(Double.MAX_VALUE);
		AssertJUnit.assertTrue("Buffer is invalid length after append", sb.length() == 22);
		AssertJUnit.assertTrue("Buffer contains invalid characters", sb.toString().equals("1.7976931348623157E308"));
	}

	/**
	 * @tests java.lang.StringBuffer#append(float)
	 */
	@Test
	public void test_append5() {
		StringBuffer sb = new StringBuffer();
		final float floatNum = 900.87654F;
		sb.append(floatNum);
		AssertJUnit.assertTrue("Buffer is invalid length after append: " + sb.length(),
				sb.length() == String.valueOf(floatNum).length());
		AssertJUnit.assertTrue("Buffer contains invalid characters", sb.toString().equals(String.valueOf(floatNum)));
	}

	/**
	 * @tests java.lang.StringBuffer#append(int)
	 */
	@Test
	public void test_append6() {
		StringBuffer sb = new StringBuffer();
		sb.append(9000);
		AssertJUnit.assertTrue("Buffer is invalid length after append", sb.length() == 4);
		sb.append(1000);
		AssertJUnit.assertTrue("Buffer is invalid length after append", sb.length() == 8);
		AssertJUnit.assertTrue("Buffer contains invalid characters", sb.toString().equals("90001000"));
	}

	/**
	 * @tests java.lang.StringBuffer#append(long)
	 */
	@Test
	public void test_append7() {
		StringBuffer sb = new StringBuffer();
		long t = 927654321098L;
		sb.append(t);
		AssertJUnit.assertTrue("Buffer is of invlaid length", sb.length() == 12);
		AssertJUnit.assertTrue("Buffer contains invalid characters", sb.toString().equals("927654321098"));
	}

	/**
	 * @tests java.lang.StringBuffer#append(java.lang.Object)
	 */
	@Test
	public void test_append8() {
		StringBuffer sb = new StringBuffer();
		Object obj1 = new Object();
		Object obj2 = new Object();
		sb.append(obj1);
		sb.append(obj2);
		AssertJUnit.assertTrue("Buffer contains invalid characters",
				sb.toString().equals(obj1.toString() + obj2.toString()));
	}

	/**
	 * @tests java.lang.StringBuffer#append(java.lang.String)
	 */
	@Test
	public void test_append9() {
		StringBuffer sb = new StringBuffer();
		String buf1 = "Hello";
		String buf2 = "World";
		sb.append(buf1);
		AssertJUnit.assertTrue("Buffer is invalid length after append", sb.length() == 5);
		sb.append(buf2);
		AssertJUnit.assertTrue("Buffer is invalid length after append", sb.length() == 10);
		AssertJUnit.assertTrue("Buffer contains invalid chars", (sb.toString().equals("HelloWorld")));
	}

	/**
	 * @tests java.lang.StringBuffer#append(boolean)
	 */
	@Test
	public void test_append10() {
		// Test for method java.lang.StringBuffer
		// java.lang.StringBuffer.append(boolean)
		StringBuffer sb = new StringBuffer();
		sb.append(false);
		AssertJUnit.assertTrue("Buffer is invalid length after append", sb.length() == 5);
		sb.append(true);
		AssertJUnit.assertTrue("Buffer is invalid length after append", sb.length() == 9);
		AssertJUnit.assertTrue("Buffer is invalid length after append", (sb.toString().equals("falsetrue")));
	}

	/**
	 * @tests java.lang.StringBuffer#append(java.lang.CharSequence)
	 */
	@Test
	public void test_append11() {
		StringBuffer sb = new StringBuffer();
		sb.append((CharSequence)"test1");
		AssertJUnit.assertTrue("String", sb.toString().equals("test1"));
		StringBuffer sb2 = new StringBuffer();
		sb2.append((CharSequence)new StringBuffer("test2"));
		AssertJUnit.assertTrue("StringBuffer", sb2.toString().equals("test2"));
		StringBuffer sb3 = new StringBuffer();
		sb3.append(Support_15Help.createCharSequence("test3"));
		AssertJUnit.assertTrue("CharSequence", sb3.toString().equals("test3"));

		CharSequence cs1 = new StringBuilder(" there");
		StringBuffer sb4 = new StringBuffer("hi");
		sb4.append(cs1);
		AssertJUnit.assertTrue("StringBuilder", sb4.toString().equals("hi there"));
	}

	/**
	 * @tests java.lang.StringBuffer#append(java.lang.CharSequence, int, int)
	 */
	@Test
	public void test_append12() {
		StringBuffer sb = new StringBuffer();
		sb.append((CharSequence)"test1", 2, 4);
		AssertJUnit.assertTrue("String", sb.toString().equals("st"));
		StringBuffer sb2 = new StringBuffer();
		sb2.append((CharSequence)new StringBuffer("test2"), 1, 3);
		AssertJUnit.assertTrue("StringBuffer", sb2.toString().equals("es"));
		StringBuffer sb3 = new StringBuffer();
		sb3.append(Support_15Help.createCharSequence("test3"), 0, 2);
		AssertJUnit.assertTrue("CharSequence", sb3.toString().equals("te"));

		CharSequence cs1 = new StringBuilder(" there");
		StringBuffer sb4 = new StringBuffer("hi");
		sb4.append(cs1, 0, 5);
		AssertJUnit.assertTrue("StringBuilder", sb4.toString().equals("hi ther"));
	}

	/**
	 * @tests java.lang.StringBuffer#capacity()
	 */
	@Test
	public void test_capacity() {
		StringBuffer sb = new StringBuffer(10);
		AssertJUnit.assertTrue("Returned incorrect capacity", sb.capacity() == 10);
		sb.ensureCapacity(100);
		AssertJUnit.assertTrue("Returned incorrect capacity", sb.capacity() >= 100);
	}

	/**
	 * @tests java.lang.StringBuffer#charAt(int)
	 */
	@Test
	public void test_charAt() {
		// Test for method char java.lang.StringBuffer.charAt(int)
		AssertJUnit.assertTrue("Returned incorrect char", testBuffer.charAt(3) == 's');
	}

	/**
	 * @tests java.lang.StringBuffer#delete(int, int)
	 */
	@Test
	public void test_delete() {
		testBuffer.delete(7, 7);
		AssertJUnit.assertTrue("Deleted chars when start == end",
				testBuffer.toString().equals("This is a test buffer"));
		testBuffer.delete(4, 14);
		AssertJUnit.assertTrue("Deleted incorrect chars", testBuffer.toString().equals("This buffer"));

		testBuffer = new StringBuffer("This is a test buffer");
		String sharedStr = testBuffer.toString();
		testBuffer.delete(0, testBuffer.length());
		AssertJUnit.assertTrue("Didn't clone shared buffer", sharedStr.equals("This is a test buffer"));
		AssertJUnit.assertTrue("Deleted incorrect chars", testBuffer.toString().equals(""));
		testBuffer.append("more stuff");
		AssertJUnit.assertTrue("Didn't clone shared buffer 2", sharedStr.equals("This is a test buffer"));
		AssertJUnit.assertTrue("Wrong contents", testBuffer.toString().equals("more stuff"));
		try {
			testBuffer.delete(-5, 2);
		} catch (IndexOutOfBoundsException e) {
		}
		AssertJUnit.assertTrue("Wrong contents 2", testBuffer.toString().equals("more stuff"));
	}

	/**
	 * @tests java.lang.StringBuffer#deleteCharAt(int)
	 */
	@Test
	public void test_deleteCharAt() {
		testBuffer.deleteCharAt(3);
		AssertJUnit.assertTrue("Deleted incorrect char", testBuffer.toString().equals("Thi is a test buffer"));
	}

	/**
	 * @tests java.lang.StringBuffer#ensureCapacity(int)
	 */
	@Test
	public void test_ensureCapacity() {
		StringBuffer sb = new StringBuffer(10);
		sb.ensureCapacity(100);
		AssertJUnit.assertTrue("Failed to increase capacity", sb.capacity() >= 100);
	}

	/**
	 * @tests java.lang.StringBuffer#getChars(int, int, char[], int)
	 */
	@Test
	public void test_getChars() {
		char[] buf = new char[10];
		testBuffer.getChars(4, 8, buf, 2);
		AssertJUnit.assertTrue("Returned incorrect chars",
				new String(buf, 2, 4).equals(testBuffer.toString().substring(4, 8)));

		boolean exception = false;
		try {
			StringBuffer buf2 = new StringBuffer("");
			buf2.getChars(0, 0, new char[5], 2);
		} catch (IndexOutOfBoundsException e) {
			exception = true;
		}
		AssertJUnit.assertTrue("did not expect IndexOutOfBoundsException", !exception);
	}

	/**
	 * @tests java.lang.StringBuffer#insert(int, char[])
	 */
	@Test
	public void test_insert() {
		char buf[] = new char[4];
		"char".getChars(0, 4, buf, 0);
		testBuffer.insert(15, buf);
		AssertJUnit.assertTrue("Insert test failed", testBuffer.toString().equals("This is a test charbuffer"));

		/*
		 * [PR 101359] insert(-1, (char[])null) should throw
		 * StringIndexOutOfBoundsException
		 */
		boolean exception = false;
		StringBuffer buf1 = new StringBuffer("abcd");
		try {
			buf1.insert(-1, (char[])null);
		} catch (StringIndexOutOfBoundsException e) {
			exception = true;
		} catch (NullPointerException e) {
		}
		AssertJUnit.assertTrue("Should throw StringIndexOutOfBoundsException", exception);
	}

	/**
	 * @tests java.lang.StringBuffer#insert(int, char[], int, int)
	 */
	@Test
	public void test_insert2() {
		char[] c = new char[] { 'n', 'o', 't', ' ' };
		testBuffer.insert(8, c, 0, 4);
		AssertJUnit.assertTrue("Insert failed: " + testBuffer.toString(),
				testBuffer.toString().equals("This is not a test buffer"));

		/*
		 * [PR 101359] insert(-1, (char[])null, x, x) should throw
		 * StringIndexOutOfBoundsException
		 */
		boolean exception = false;
		StringBuffer buf1 = new StringBuffer("abcd");
		try {
			buf1.insert(-1, (char[])null, 0, 0);
		} catch (StringIndexOutOfBoundsException e) {
			exception = true;
		} catch (NullPointerException e) {
		}
		AssertJUnit.assertTrue("Should throw StringIndexOutOfBoundsException", exception);
	}

	/**
	 * @tests java.lang.StringBuffer#insert(int, char)
	 */
	@Test
	public void test_insert3() {
		testBuffer.insert(15, 'T');
		AssertJUnit.assertTrue("Insert test failed", testBuffer.toString().equals("This is a test Tbuffer"));
	}

	/**
	 * @tests java.lang.StringBuffer#insert(int, double)
	 */
	@Test
	public void test_insert4() {
		testBuffer.insert(15, Double.MAX_VALUE);
		AssertJUnit.assertTrue("Insert test failed",
				testBuffer.toString().equals("This is a test " + Double.MAX_VALUE + "buffer"));
	}

	/**
	 * @tests java.lang.StringBuffer#insert(int, float)
	 */
	@Test
	public void test_insert5() {
		testBuffer.insert(15, Float.MAX_VALUE);
		String testBufferString = testBuffer.toString();
		String expectedResult = "This is a test " + String.valueOf(Float.MAX_VALUE) + "buffer";
		AssertJUnit.assertTrue("Insert test failed, got: " + "\'" + testBufferString + "\'" + " but wanted: " + "\'"
				+ expectedResult + "\'", testBufferString.equals(expectedResult));
	}

	/**
	 * @tests java.lang.StringBuffer#insert(int, int)
	 */
	@Test
	public void test_insert6() {
		testBuffer.insert(15, 100);
		AssertJUnit.assertTrue("Insert test failed", testBuffer.toString().equals("This is a test 100buffer"));
	}

	/**
	 * @tests java.lang.StringBuffer#insert(int, long)
	 */
	@Test
	public void test_insert7() {
		testBuffer.insert(15, 88888888888888888L);
		AssertJUnit.assertTrue("Insert test failed",
				testBuffer.toString().equals("This is a test 88888888888888888buffer"));
	}

	/**
	 * @tests java.lang.StringBuffer#insert(int, java.lang.Object)
	 */
	@Test
	public void test_insert8() {
		Object obj1 = new Object();
		testBuffer.insert(15, obj1);
		AssertJUnit.assertTrue("Insert test failed",
				testBuffer.toString().equals("This is a test " + obj1.toString() + "buffer"));
	}

	/**
	 * @tests java.lang.StringBuffer#insert(int, java.lang.String)
	 */
	@Test
	public void test_insert9() {
		testBuffer.insert(15, "STRING ");
		AssertJUnit.assertTrue("Insert test failed", testBuffer.toString().equals("This is a test STRING buffer"));
	}

	/**
	 * @tests java.lang.StringBuffer#insert(int, boolean)
	 */
	@Test
	public void test_insert10() {
		testBuffer.insert(15, true);
		AssertJUnit.assertTrue("Insert test failed", testBuffer.toString().equals("This is a test truebuffer"));
	}

	/**
	 * @tests java.lang.StringBuffer#insert(int, java.lang.CharSequence)
	 */
	@Test
	public void test_insert11() {
		StringBuffer sb = new StringBuffer("abc");
		sb.insert(2, (CharSequence)"t1");
		AssertJUnit.assertTrue("String", sb.toString().equals("abt1c"));
		StringBuffer sb2 = new StringBuffer("abc");
		sb2.insert(3, (CharSequence)new StringBuffer("t2"));
		AssertJUnit.assertTrue("StringBuffer", sb2.toString().equals("abct2"));
		StringBuffer sb3 = new StringBuffer("abc");
		sb3.insert(1, Support_15Help.createCharSequence("t3"));
		AssertJUnit.assertTrue("StringBuffer", sb3.toString().equals("at3bc"));

		CharSequence cs1 = new StringBuilder(" there");
		StringBuffer sb4 = new StringBuffer("hi");
		sb4.insert(0, cs1);
		AssertJUnit.assertTrue("StringBuilder", sb4.toString().equals(" therehi"));
	}

	/**
	 * @tests java.lang.StringBuffer#insert(int, java.lang.CharSequence, int,
	 *        int)
	 */
	@Test
	public void test_insert12() {
		StringBuffer buf = new StringBuffer("test");
		buf.insert(1, (CharSequence)null, 0, 1);
		AssertJUnit.assertTrue("start, end ignored", buf.toString().equals("tnest"));

		StringBuffer sb = new StringBuffer("abc");
		sb.insert(2, (CharSequence)"test1", 1, 4);
		AssertJUnit.assertTrue("String", sb.toString().equals("abestc"));
		StringBuffer sb2 = new StringBuffer("abc");
		sb2.insert(0, (CharSequence)new StringBuffer("test2"), 0, 2);
		AssertJUnit.assertTrue("StringBuffer", sb2.toString().equals("teabc"));
		StringBuffer sb3 = new StringBuffer("abc");
		sb3.insert(1, Support_15Help.createCharSequence("test3"), 2, 5);
		AssertJUnit.assertTrue("StringBuffer", sb3.toString().equals("ast3bc"));

		CharSequence cs1 = new StringBuilder(" there");
		StringBuffer sb4 = new StringBuffer("hi");
		sb4.insert(0, cs1, 0, 5);
		AssertJUnit.assertTrue("StringBuilder", sb4.toString().equals(" therhi"));
	}

	/**
	 * @tests java.lang.StringBuffer#length()
	 */
	@Test
	public void test_length() {
		AssertJUnit.assertTrue("Incorrect length returned", testBuffer.length() == 21);
	}

	/**
	 * @tests java.lang.StringBuffer#replace(int, int, java.lang.String)
	 */
	@Test
	public void test_replace() {
		testBuffer.replace(5, 9, "is a replaced");
		AssertJUnit
				.assertTrue(
						"Replace failed, wanted: " + "\'" + "This is a replaced test buffer" + "\'" + " but got: "
								+ "\'" + testBuffer.toString() + "\'",
						testBuffer.toString().equals("This is a replaced test buffer"));
		AssertJUnit.assertTrue("insert1", new StringBuffer().replace(0, 0, "text").toString().equals("text"));
		AssertJUnit.assertTrue("insert2", new StringBuffer("123").replace(3, 3, "text").toString().equals("123text"));
		AssertJUnit.assertTrue("insert2", new StringBuffer("123").replace(1, 1, "text").toString().equals("1text23"));
	}

	private String writeString(String in) {
		StringBuffer result = new StringBuffer();
		result.append("\"");
		for (int i = 0; i < in.length(); i++) {
			result.append(" 0x" + Integer.toHexString(in.charAt(i)));
		}
		result.append("\"");
		return result.toString();
	}

	private void reverseTest(String id, String org, String rev, String back) {
		// create non-shared StringBuffer
		StringBuffer sb = new StringBuffer(org);
		sb.reverse();
		String reversed = sb.toString();
		AssertJUnit.assertTrue("reversed surrogate " + id + ": " + writeString(reversed), reversed.equals(rev));
		// create non-shared StringBuffer
		sb = new StringBuffer(reversed);
		sb.reverse();
		reversed = sb.toString();
		AssertJUnit.assertTrue("reversed surrogate " + id + "a: " + writeString(reversed), reversed.equals(back));

		// test algorithm when StringBuffer is shared
		sb = new StringBuffer(org);
		String copy = sb.toString();
		AssertJUnit.assertEquals(org, copy);
		sb.reverse();
		reversed = sb.toString();
		AssertJUnit.assertTrue("reversed surrogate " + id + ": " + writeString(reversed), reversed.equals(rev));
		sb = new StringBuffer(reversed);
		copy = sb.toString();
		AssertJUnit.assertEquals(rev, copy);
		sb.reverse();
		reversed = sb.toString();
		AssertJUnit.assertTrue("reversed surrogate " + id + "a: " + writeString(reversed), reversed.equals(back));

	}

	/**
	 * @tests java.lang.StringBuffer#reverse()
	 */
	@Test
	public void test_reverse() {
		String org;
		org = "a";
		reverseTest("0", org, org, org);

		org = "ab";
		reverseTest("1", org, "ba", org);

		org = "abcdef";
		reverseTest("2", org, "fedcba", org);

		org = "abcdefg";
		reverseTest("3", org, "gfedcba", org);

		/*
		 * [PR CMVC 93149] ArrayIndexOutOfBoundsException in
		 * StringBuffer.reverse()
		 */
		org = "\ud800\udc00";
		reverseTest("4a", org, org, org);

		org = "\udc00\ud800";
		reverseTest("4b", org, "\ud800\udc00", "\ud800\udc00");

		org = "a\ud800\udc00";
		reverseTest("4", org, "\ud800\udc00a", org);

		org = "ab\ud800\udc00";
		reverseTest("5", org, "\ud800\udc00ba", org);

		org = "abc\ud800\udc00";
		reverseTest("6", org, "\ud800\udc00cba", org);

		org = "\ud800\udc00\udc01\ud801\ud802\udc02";
		reverseTest("7", org, "\ud802\udc02\ud801\udc01\ud800\udc00", "\ud800\udc00\ud801\udc01\ud802\udc02");

		org = "\ud800\udc00\ud801\udc01\ud802\udc02";
		reverseTest("8", org, "\ud802\udc02\ud801\udc01\ud800\udc00", org);

		org = "\ud800\udc00\udc01\ud801a";
		reverseTest("9", org, "a\ud801\udc01\ud800\udc00", "\ud800\udc00\ud801\udc01a");

		org = "a\ud800\udc00\ud801\udc01";
		reverseTest("10", org, "\ud801\udc01\ud800\udc00a", org);

		org = "\ud800\udc00\udc01\ud801ab";
		reverseTest("11", org, "ba\ud801\udc01\ud800\udc00", "\ud800\udc00\ud801\udc01ab");

		org = "ab\ud800\udc00\ud801\udc01";
		reverseTest("12", org, "\ud801\udc01\ud800\udc00ba", org);

		org = "\ud800\udc00\ud801\udc01";
		reverseTest("13", org, "\ud801\udc01\ud800\udc00", org);

		org = "a\ud800\udc00z\ud801\udc01";
		reverseTest("14", org, "\ud801\udc01z\ud800\udc00a", org);

		org = "a\ud800\udc00bz\ud801\udc01";
		reverseTest("15", org, "\ud801\udc01zb\ud800\udc00a", org);

		org = "abc\ud802\udc02\ud801\udc01\ud800\udc00";
		reverseTest("16", org, "\ud800\udc00\ud801\udc01\ud802\udc02cba", org);

		org = "abcd\ud802\udc02\ud801\udc01\ud800\udc00";
		reverseTest("17", org, "\ud800\udc00\ud801\udc01\ud802\udc02dcba", org);
	}

	/**
	 * @tests java.lang.StringBuffer#setCharAt(int, char)
	 */
	@Test
	public void test_setCharAt() {
		StringBuffer s = new StringBuffer("HelloWorld");
		s.setCharAt(4, 'Z');
		AssertJUnit.assertTrue("Returned incorrect char", s.charAt(4) == 'Z');
	}

	/**
	 * @tests java.lang.StringBuffer#setLength(int)
	 */
	@Test
	public void test_setLength() {
		testBuffer.setLength(1000);
		AssertJUnit.assertTrue("Failed to increase length", testBuffer.length() == 1000);
		AssertJUnit.assertTrue("Increase in length trashed buffer",
				testBuffer.toString().startsWith("This is a test buffer"));
		testBuffer.setLength(2);
		AssertJUnit.assertTrue("Failed to decrease length", testBuffer.length() == 2);
		AssertJUnit.assertTrue("Decrease in length failed", testBuffer.toString().equals("Th"));

		StringBuffer sb1;
		sb1 = new StringBuffer();
		sb1.append("abcdefg");
		sb1.setLength(2);
		sb1.setLength(5);
		for (int i = 2; i < 5; i++)
			AssertJUnit.assertTrue("setLength() char(" + i + ") not zeroed", sb1.charAt(i) == 0);

		sb1 = new StringBuffer();
		sb1.append("abcdefg");
		sb1.delete(2, 4);
		sb1.setLength(7);
		for (int i = 5; i < 7; i++)
			AssertJUnit.assertTrue("delete() char(" + i + ") not zeroed", sb1.charAt(i) == 0);
	}

	/**
	 * @tests java.lang.StringBuffer#setLength(int)
	 * @tests java.lang.StringBuffer#replace(int, int, java.lang.String)
	 */
	@Test
	public void test_setLength_replace() {
		StringBuffer sb1 = new StringBuffer();
		sb1.append("abcdefg");
		sb1.replace(2, 5, "z");
		sb1.setLength(7);
		for (int i = 5; i < 7; i++)
			AssertJUnit.assertTrue("replace() char(" + i + ") not zeroed", sb1.charAt(i) == 0);
	}

	/**
	 * @tests java.lang.StringBuffer#substring(int)
	 */
	@Test
	public void test_substring() {
		AssertJUnit.assertTrue("Returned incorrect substring", testBuffer.substring(5).equals("is a test buffer"));
	}

	/**
	 * @tests java.lang.StringBuffer#substring(int, int)
	 */
	@Test
	public void test_substring2() {
		AssertJUnit.assertTrue("Returned incorrect substring", testBuffer.substring(5, 7).equals("is"));
	}

	/**
	 * @tests java.lang.StringBuffer#toString()
	 */
	@Test
	public void test_toString() {
		AssertJUnit.assertTrue("Incorrect string value returned",
				testBuffer.toString().equals("This is a test buffer"));
	}

	/**
	 * @tests java.lang.StringBuffer#trimToSize()
	 */
	@Test
	public void test_trimToSize() {
		StringBuffer buffer = new StringBuffer(4);
		buffer.append("testing");
		AssertJUnit.assertTrue("unexpected capacity", buffer.capacity() > buffer.length());
		buffer.trimToSize();
		AssertJUnit.assertTrue("incorrect value", buffer.toString().equals("testing"));
		AssertJUnit.assertTrue("capacity not trimmed", buffer.capacity() == buffer.length());
	}

	/**
	 * @tests java.lang.StringBuffer#codePointCount(int, int)
	 */
	@Test
	public void test_codePointCount() {
		StringBuffer sb = new StringBuffer("A\ud800\udc00C");
		AssertJUnit.assertTrue("wrong count 1", sb.codePointCount(0, 4) == 3);
		AssertJUnit.assertTrue("wrong count 2", sb.codePointCount(1, 4) == 2);
	}

	/**
	 * @tests java.lang.StringBuffer#appendCodePoint(int)
	 */
	@Test
	public void test_appendCodePoint() {
		StringBuffer buffer = new StringBuffer();
		for (int i = 0; i < 0x10FFFF; i++) {
			buffer.appendCodePoint(i);
			if (i <= 0xffff) {
				AssertJUnit.assertTrue("invalid length <= 0xffff", buffer.length() == 1);
				AssertJUnit.assertTrue("charAt() returns different value", buffer.charAt(0) == i);
			} else {
				AssertJUnit.assertTrue("invalid length > 0xffff", buffer.length() == 2);
			}
			AssertJUnit.assertTrue("codePointAt() returns different value", buffer.codePointAt(0) == i);
			buffer.setLength(0);
		}
	}

	/**
	 * Sets up the fixture, for example, open a network connection. This method
	 * is called before a test is executed.
	 */
	@BeforeMethod
	protected void setUp() {
		testBuffer = new StringBuffer("This is a test buffer");
	}
}
