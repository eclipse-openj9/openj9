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
public class Test_StringBuilder {

	StringBuilder testBuffer;

	private String writeString(String in) {
		StringBuilder result = new StringBuilder();
		result.append("\"");
		for (int i = 0; i < in.length(); i++) {
			result.append(" 0x" + Integer.toHexString(in.charAt(i)));
		}
		result.append("\"");
		return result.toString();
	}

	private void reverseTest(String id, String org, String rev, String back) {
		// create non-shared StringBuilder
		StringBuilder sb = new StringBuilder(org);
		sb.reverse();
		String reversed = sb.toString();
		AssertJUnit.assertTrue("reversed surrogate " + id + ": " + writeString(reversed), reversed.equals(rev));
		// create non-shared StringBuilder
		sb = new StringBuilder(reversed);
		sb.reverse();
		reversed = sb.toString();
		AssertJUnit.assertTrue("reversed surrogate " + id + "a: " + writeString(reversed), reversed.equals(back));

		// test algorithm when StringBuilder is shared
		sb = new StringBuilder(org);
		String copy = sb.toString();
		AssertJUnit.assertEquals(org, copy);
		sb.reverse();
		reversed = sb.toString();
		AssertJUnit.assertTrue("reversed surrogate " + id + ": " + writeString(reversed), reversed.equals(rev));
		sb = new StringBuilder(reversed);
		copy = sb.toString();
		AssertJUnit.assertEquals(rev, copy);
		sb.reverse();
		reversed = sb.toString();
		AssertJUnit.assertTrue("reversed surrogate " + id + "a: " + writeString(reversed), reversed.equals(back));

	}

	/**
	 * @tests java.lang.StringBuilder#reverse()
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
		 * [PR 117344, CMVC 93149] ArrayIndexOutOfBoundsException in
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
	 * @tests java.lang.StringBuilder#StringBuilder(java.lang.CharSequence)
	 */
	@Test
	public void test_Constructor() {
		StringBuilder sb = new StringBuilder((CharSequence)"test1");
		AssertJUnit.assertTrue("String", sb.toString().equals("test1"));
		StringBuilder sb2 = new StringBuilder((CharSequence)new StringBuffer("test2"));
		AssertJUnit.assertTrue("StringBuffer", sb2.toString().equals("test2"));
		StringBuilder sb3 = new StringBuilder(Support_15Help.createCharSequence("test3"));
		AssertJUnit.assertTrue("CharSequence", sb3.toString().equals("test3"));
	}

	/**
	 * @tests java.lang.StringBuilder#append(java.lang.CharSequence)
	 */
	@Test
	public void test_append() {
		StringBuilder sb = new StringBuilder();
		sb.append((CharSequence)"test1");
		AssertJUnit.assertTrue("String", sb.toString().equals("test1"));
		StringBuilder sb2 = new StringBuilder();
		sb2.append((CharSequence)new StringBuffer("test2"));
		AssertJUnit.assertTrue("StringBuffer", sb2.toString().equals("test2"));
		StringBuilder sb3 = new StringBuilder();
		sb3.append(Support_15Help.createCharSequence("test3"));
		AssertJUnit.assertTrue("CharSequence", sb3.toString().equals("test3"));

		CharSequence cs1 = new StringBuilder(" there");
		StringBuilder sb4 = new StringBuilder("hi");
		sb4.append(cs1);
		AssertJUnit.assertTrue("StringBuilder", sb4.toString().equals("hi there"));
	}

	/**
	 * @tests java.lang.StringBuilder#append(java.lang.CharSequence, int, int)
	 */
	@Test
	public void test_append2() {
		StringBuilder sb = new StringBuilder();
		sb.append((CharSequence)"test1", 2, 4);
		AssertJUnit.assertTrue("String", sb.toString().equals("st"));
		StringBuilder sb2 = new StringBuilder();
		sb2.append((CharSequence)new StringBuffer("test2"), 1, 3);
		AssertJUnit.assertTrue("StringBuffer", sb2.toString().equals("es"));
		StringBuilder sb3 = new StringBuilder();
		sb3.append(Support_15Help.createCharSequence("test3"), 0, 2);
		AssertJUnit.assertTrue("CharSequence", sb3.toString().equals("te"));

		CharSequence cs1 = new StringBuilder(" there");
		StringBuilder sb4 = new StringBuilder("hi");
		sb4.append(cs1, 0, 5);
		AssertJUnit.assertTrue("StringBuilder", sb4.toString().equals("hi ther"));
	}

	/**
	 * @tests java.lang.StringBuilder#insert(int, java.lang.CharSequence)
	 */
	@Test
	public void test_insert() {
		StringBuilder sb = new StringBuilder("abc");
		sb.insert(2, (CharSequence)"t1");
		AssertJUnit.assertTrue("String", sb.toString().equals("abt1c"));
		StringBuilder sb2 = new StringBuilder("abc");
		sb2.insert(3, (CharSequence)new StringBuffer("t2"));
		AssertJUnit.assertTrue("StringBuffer", sb2.toString().equals("abct2"));
		StringBuilder sb3 = new StringBuilder("abc");
		sb3.insert(1, Support_15Help.createCharSequence("t3"));
		AssertJUnit.assertTrue("CharSequence", sb3.toString().equals("at3bc"));

		CharSequence cs1 = new StringBuilder(" there");
		StringBuilder sb4 = new StringBuilder("hi");
		sb4.insert(0, cs1);
		AssertJUnit.assertTrue("StringBuilder", sb4.toString().equals(" therehi"));
	}

	/**
	 * @tests java.lang.StringBuilder#insert(int, java.lang.CharSequence, int,
	 *        int)
	 */
	@Test
	public void test_insert3() {
		StringBuilder buf = new StringBuilder("test");
		buf.insert(1, (CharSequence)null, 0, 1);
		AssertJUnit.assertTrue("start, end ignored", buf.toString().equals("tnest"));

		StringBuilder sb = new StringBuilder("abc");
		sb.insert(2, (CharSequence)"test1", 1, 4);
		AssertJUnit.assertTrue("String", sb.toString().equals("abestc"));
		StringBuilder sb2 = new StringBuilder("abc");
		sb2.insert(0, (CharSequence)new StringBuffer("test2"), 0, 2);
		AssertJUnit.assertTrue("StringBuffer", sb2.toString().equals("teabc"));
		StringBuilder sb3 = new StringBuilder("abc");
		sb3.insert(1, Support_15Help.createCharSequence("test3"), 2, 5);
		AssertJUnit.assertTrue("CharSequence", sb3.toString().equals("ast3bc"));

		CharSequence cs1 = new StringBuilder(" there");
		StringBuilder sb4 = new StringBuilder("hi");
		sb4.insert(0, cs1, 0, 5);
		AssertJUnit.assertTrue("StringBuilder", sb4.toString().equals(" therhi"));
	}

	/**
	 * @tests java.lang.StringBuilder#trimToSize()
	 */
	@Test
	public void test_trimToSize() {
		StringBuilder builder = new StringBuilder(4);
		builder.append("testing");
		AssertJUnit.assertTrue("unexpected capacity", builder.capacity() > builder.length());
		builder.trimToSize();
		AssertJUnit.assertTrue("incorrect value", builder.toString().equals("testing"));
		AssertJUnit.assertTrue("capacity not trimmed", builder.capacity() == builder.length());
	}

	/**
	 * @tests java.lang.StringBuilder#codePointCount(int, int)
	 */
	@Test
	public void test_codePointCount() {
		StringBuilder sb = new StringBuilder("A\ud800\udc00C");
		AssertJUnit.assertTrue("wrong count 1", sb.codePointCount(0, 4) == 3);
		AssertJUnit.assertTrue("wrong count 2", sb.codePointCount(1, 4) == 2);
	}

	/**
	 * @tests java.lang.StringBuilder#appendCodePoint(int)
	 */
	@Test
	public void test_appendCodePoint() {
		StringBuilder builder = new StringBuilder();
		for (int i = 0; i < 0x10FFFF; i++) {
			builder.appendCodePoint(i);
			if (i <= 0xffff) {
				AssertJUnit.assertTrue("invalid length <= 0xffff", builder.length() == 1);
				AssertJUnit.assertTrue("charAt() returns different value", builder.charAt(0) == i);
			} else {
				AssertJUnit.assertTrue("invalid length > 0xffff", builder.length() == 2);
			}
			AssertJUnit.assertTrue("codePointAt() returns different value", builder.codePointAt(0) == i);
			builder.setLength(0);
		}
	}

	/**
	 * @tests java.lang.StringBuilder#delete(int, int)
	 */
	@Test
	public void test_delete() {
		testBuffer.delete(7, 7);
		AssertJUnit.assertTrue("Deleted chars when start == end",
				testBuffer.toString().equals("This is a test buffer"));
		testBuffer.delete(4, 14);
		AssertJUnit.assertTrue("Deleted incorrect chars", testBuffer.toString().equals("This buffer"));

		testBuffer = new StringBuilder("This is a test buffer");
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
	 * @tests java.lang.StringBuilder#setLength(int)
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

		StringBuilder sb1;
		sb1 = new StringBuilder();
		sb1.append("abcdefg");
		sb1.setLength(2);
		sb1.setLength(5);
		for (int i = 2; i < 5; i++)
			AssertJUnit.assertTrue("setLength() char(" + i + ") not zeroed", sb1.charAt(i) == 0);

		sb1 = new StringBuilder();
		sb1.append("abcdefg");
		sb1.delete(2, 4);
		sb1.setLength(7);
		for (int i = 5; i < 7; i++)
			AssertJUnit.assertTrue("delete() char(" + i + ") not zeroed", sb1.charAt(i) == 0);
	}

	/**
	 * @tests java.lang.StringBuilder#setLength(int)
	 * @tests java.lang.StringBuilder#replace(int, int, java.lang.String)
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

	@BeforeMethod
	protected void setUp() {
		testBuffer = new StringBuilder("This is a test buffer");
	}
}
