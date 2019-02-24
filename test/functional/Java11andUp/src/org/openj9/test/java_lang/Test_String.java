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
package org.openj9.test.java_lang;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import java.util.Iterator;
import java.util.stream.Stream;

/**
 * This test Java.lang.String API added in Java 11 and later version.
 *
 */
public class Test_String {
	public static Logger logger = Logger.getLogger(Test_String.class);
	
	/*
	 * Useful variables for testing
	 */
	String empty = "";
	String allWhiteSpace = "   ";
	String latin1 = "abc123";
	String nonLatin1 = "abc\u0153";
	String latin1WithTerm = "one\ntwo\rthree\r\nfour";
	String[] latin1LinesArray = {"one", "two", "three", "four"};
	String nonLatin1WithTerm = "\u0153\n\u0154\r\u0155\r\n\u0156";
	String[] nonLatin1LinesArray = {"\u0153", "\u0154", "\u0155", "\u0156"};
	String emptyWithTerm = "\n";

	/*
	 * Test Java 11 API String.valueOfCodePoint(codePoint)
	 */
	@Test(groups = { "level.sanity" })
	public void testValueOfCodePoint() throws IllegalAccessException, IllegalArgumentException, InvocationTargetException, NoSuchMethodException, SecurityException {
		Method method = String.class.getDeclaredMethod("valueOfCodePoint", int.class);
		method.setAccessible(true);
		// Verify that for each valid Unicode code point i,
		// i equals to String.valueOfCodePoint(i).codePointAt(0).
		for (int i = Character.MIN_CODE_POINT; i <= Character.MAX_CODE_POINT; i++) {
			String str = (String) method.invoke(null, i);
			Assert.assertEquals(i, str.codePointAt(0), "Unicode code point mismatch at " + i);
		}
		// Verify that IllegalArgumentException is thrown for Character.MIN_CODE_POINT - 1.
		try {
			method.invoke(null, Character.MIN_CODE_POINT - 1);
			Assert.fail("Failed to detect invalid Unicode point - Character.MIN_CODE_POINT - 1");
		} catch (IllegalArgumentException | InvocationTargetException e) {
			// Expected exception
		}
		// Verify that IllegalArgumentException is thrown for Character.MAX_CODE_POINT + 1.
		try {
			method.invoke(null, Character.MAX_CODE_POINT + 1);
			Assert.fail("Failed to detect invalid Unicode point - Character.MAX_CODE_POINT + 1");
		} catch (IllegalArgumentException | InvocationTargetException e) {
			// Expected exception
		}
	}

	/*
	 * Test Java 11 method String.repeat(count)
	 */
	@Test(groups = { "level.sanity" })
	public void testRepeat() {
		String str;

		// Verify that correct strings are produced under normal conditions
		Assert.assertTrue(latin1.repeat(3).equals("abc123abc123abc123"), "Repeat failed to produce correct string (LATIN1)");
		Assert.assertTrue(nonLatin1.repeat(3).equals("abc\u0153abc\u0153abc\u0153"), "Repeat failed to produce correct string (non-LATIN1)");

		Assert.assertTrue(empty.repeat(0) == empty, "Failed to return identical empty string - empty base (0)");
		Assert.assertTrue(empty.repeat(2) == empty, "Failed to return identical empty string - empty base (2)");
		Assert.assertTrue(latin1.repeat(0) == empty, "Failed to return identical empty string - nonempty base (0)");

		// Verify that IllegalArgumentException is thrown for count < 0
		try {
			str = empty.repeat(-1);
			Assert.fail("Failed to detect invalid count -1");
		} catch (IllegalArgumentException e) {
			// Expected exception
		}

		try {
			str = latin1.repeat(-3);
			Assert.fail("Failed to detect invalid count -3");
		} catch (IllegalArgumentException e) {
			// Expected exception
		}

		// Verify that a reference to the instance that called the function is returned for count == 1
		Assert.assertTrue(latin1.repeat(1) == latin1, "Should be identical on count 1");
	}
	
	/*
	 * Test Java 11 API String.strip
	 */
	@Test(groups = { "level.sanity" })
	public void testStrip() {
		// empty string parameter should return empty string
		Assert.assertEquals(empty.strip(), empty, 
				"strip: Striped empty string did not return an empty string.");
		
		// pass string with all white space
		Assert.assertEquals(allWhiteSpace.strip(), empty, 
				"strip: Striped all white space string did not return an empty string.");
		
		// pass in string without white space
		Assert.assertEquals(latin1.strip(), latin1, 
				"strip: Striped string with no leading/trailing white space did not return original string.");
		Assert.assertSame(latin1.strip(), latin1,
				"strip: Striped string with no leading/trailing white space returns same object.");

		// pass in string with white space on either side
		Assert.assertEquals((allWhiteSpace + latin1 + allWhiteSpace).strip(), latin1, 
				"strip: Value of striped string with leading/trailing white space was unexpected.");
		
		// pass in string with white space on either side (special characters)
		Assert.assertEquals((allWhiteSpace + nonLatin1 + allWhiteSpace).strip(), nonLatin1, 
				"strip: Value of striped string with leading/trailing white space with non-Latin1 characters was unexpected.");
	}
	
	/*
	 * Test Java 11 API String.stripLeading
	 */
	@Test(groups = { "level.sanity" })
	public void testStripLeading() {
		// empty string parameter should return empty string
		Assert.assertEquals(empty.stripLeading(), empty, 
				"stripLeading: Striped empty string did not return an empty string.");
		
		// pass string with all white space
		Assert.assertEquals(allWhiteSpace.stripLeading(), empty, 
				"stripLeading: Striped all white space string did not return an empty string.");
		
		// pass in string without white space
		Assert.assertEquals(latin1.stripLeading(), latin1, 
				"stripLeading: Striped string with no leading/trailing white space did not return original string.");
		Assert.assertSame(latin1.stripLeading(), latin1,
				"strip: Striped string with no leading/trailing white space returns same object.");

		// pass in string with white space on either side
		Assert.assertEquals((allWhiteSpace + latin1 + allWhiteSpace).stripLeading(), latin1 + allWhiteSpace, 
				"stripLeading: Value of striped string with leading/trailing white space was unexpected.");
		
		// pass in string with white space on either side (special characters)
		Assert.assertEquals((allWhiteSpace + nonLatin1 + allWhiteSpace).stripLeading(), nonLatin1 + allWhiteSpace, 
				"stripLeading: Value of striped string with leading/trailing white space with non-Latin1 characters was unexpected.");
	}
	
	/*
	 * Test Java 11 API String.stripTrailing
	 */
	@Test(groups = { "level.sanity" })
	public void testStripTrailing() {
		// empty string parameter should return empty string
		Assert.assertEquals(empty.stripTrailing(), empty, 
				"stripTrailing: Striped empty string did not return an empty string.");
		
		// pass string with all white space
		Assert.assertEquals(allWhiteSpace.stripTrailing(), empty, 
				"stripTrailing: Striped all white space string did not return an empty string.");
		
		// pass in string without white space
		Assert.assertEquals(latin1.stripTrailing(), latin1, 
				"stripTrailing: Striped string with no leading/trailing white space did not return original string.");
		Assert.assertSame(latin1.stripTrailing(), latin1,
				"strip: Striped string with no leading/trailing white space returns same object.");

		// pass in string with white space on either side
		Assert.assertEquals((allWhiteSpace + latin1 + allWhiteSpace).stripTrailing(), allWhiteSpace + latin1, 
				"stripTrailing: Value of striped string with leading/trailing white space was unexpected.");

		// pass in string with white space on either side (special characters)
		Assert.assertEquals((allWhiteSpace + nonLatin1 + allWhiteSpace).stripTrailing(), allWhiteSpace + nonLatin1, 
				"stripTrailing: Value of striped string with leading/trailing white space with non-Latin1 characters was unexpected.");
	}

	/*
	 * Test Java 11 API String.isBlank
	 */
	@Test(groups = { "level.sanity" })
	public void testIsblank() {
		// pass empty string
		Assert.assertTrue(empty.isBlank(),
				"isBlank: failed to return true on empty string.");

		// pass string with all white space
		Assert.assertTrue(allWhiteSpace.isBlank(),
				"isBlank: failed to return true on all white space string.");

		// pass non-blank strings
		Assert.assertFalse(latin1.isBlank(),
				"isBlank: failed to return false on non-blank latin1 string.");
		Assert.assertFalse(nonLatin1.isBlank(),
				"isBlank: failed to return false on non-blank non-latin1 string.");

		Assert.assertFalse((allWhiteSpace + latin1).isBlank(),
				"isBlank: failed to return false on non-blank latin1 string with leading white space.");
		Assert.assertFalse((allWhiteSpace + nonLatin1).isBlank(),
				"isBlank: failed to return false on non-blank non-latin1 string with leading white space.");
	}
	 
	/*	
	 * Test Java 11 API String.lines
	 */
	@Test(groups = { "level.sanity" })
	public void testLines() {
		Stream<String> ss;
		Iterator<String> iter;
		String s;
		int i;

		// ensure latin1 and nonLatin1 properly breaks apart strings by line terminators
		ss = latin1WithTerm.lines();
		iter = ss.iterator();
		i = 0;
		while (iter.hasNext()) {
			s = iter.next();
			Assert.assertEquals(s, latin1LinesArray[i],
					"lines: Failed to break apart latin1 string by line terminators - failed on " + latin1LinesArray[i]);
			i++;
		}

		ss = nonLatin1WithTerm.lines();
		iter = ss.iterator();
		i = 0;
		while (iter.hasNext()) {
			s = iter.next();
			Assert.assertEquals(s, nonLatin1LinesArray[i],
					"lines: Failed to break apart latin1 string by line terminators - failed on " + nonLatin1LinesArray[i]);
			i++;
		}

		// pass in string with only one line terminator - should return empty string
		ss = emptyWithTerm.lines();
		Assert.assertEquals(ss.findAny().get(), "",
					"lines: Failed to get empty string from string with a single line terminator.");

		// pass in empty string
		try {
			ss = empty.lines();
			s = ss.findAny().get();
			Assert.fail("Failed to throw exception on get non-existent value - empty string.");
		} catch (java.util.NoSuchElementException e) {
			// Expected exception
		}
	}
}
