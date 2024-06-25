package org.openj9.test.com.ibm.jit;

/*
 * Copyright IBM Corp. and others 1998
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

import java.lang.reflect.Field;
import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity" })
public class Test_JITHelpersIntrinsicIndexOf {

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

	/*
	 * Tests for Java 8/11 only
	 */

	/**
	 * @tests com.ibm.jit.JITHelpers#intrinsicIndexOfLatin1(Object, byte, int, int)
	 */
	public static void test_intrinsicIndexOfLatin1() {
		try {
			Field valueField = String.class.getDeclaredField("value");
			valueField.setAccessible(true);

			// Latin1 tests are valid if and only if compact strings are enabled because of the way we extract the
			// String.value field and pass it off to the intrinsicIndexOfLatin1 API. The only way to ensure
			// the extracted array is in Latin1 format is to make sure compact strings are enabled.
			Field enableCompressionField = String.class.getDeclaredField("COMPACT_STRINGS");
			enableCompressionField.setAccessible(true);

			if ((boolean)enableCompressionField.get(null)) {
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get(""), (byte)'a', 0, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("a"), (byte)'a', 0, 1), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("a"), (byte)'a', 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("a"), (byte)'b', 0, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("a"), (byte)'b', 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("a"), (byte)'A', 0, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'b', 0, 2), 1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'b', 1, 2), 1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'B', 0, 2), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'B', 1, 2), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'a', 0, 2), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'A', 1, 2), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab"), (byte)'a', 1, 2), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("aba"), (byte)'b', 1, 3), 1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'o', 0, 15), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'o', 7, 15), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'a', 0, 15), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'x', 0, 15), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'x', 1, 15), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'x', 7, 15), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'d', 0, 15), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'d', 2, 15), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmno"), (byte)'d', 5, 15), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'o', 0, 16), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'o', 7, 16), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'p', 0, 16), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'p', 5, 16), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'a', 0, 16), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'x', 0, 16), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'x', 1, 16), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'x', 7, 16), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'d', 0, 16), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'d', 2, 16), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnop"), (byte)'d', 5, 16), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'o', 0, 17), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'o', 7, 17), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'p', 0, 17), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'p', 5, 17), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'q', 0, 17), 16);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'q', 5, 17), 16);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'a', 0, 17), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'x', 0, 17), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'x', 1, 17), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'x', 7, 17), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'d', 0, 17), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'d', 2, 17), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("abcdefghijklmnopq"), (byte)'d', 5, 17), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'o', 0, 31), 28);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'o', 7, 31), 28);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'p', 0, 31), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'p', 5, 31), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'q', 0, 31), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'q', 5, 31), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'a', 0, 31), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'x', 0, 31), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'x', 1, 31), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'x', 7, 31), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'d', 0, 31), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'1', 0, 31), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'1', 1, 31), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'1', 2, 31), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'1', 3, 31), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'1', 4, 31), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'d', 2, 31), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), (byte)'d', 5, 31), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'o', 0, 32), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'o', 7, 32), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'p', 0, 32), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'p', 5, 32), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'q', 0, 32), 31);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'q', 5, 32), 31);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'a', 0, 32), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'x', 0, 32), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'x', 1, 32), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'x', 7, 32), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'d', 0, 32), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'d', 2, 32), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), (byte)'d', 5, 32), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'o', 0, 34), 31);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'o', 7, 34), 31);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'p', 0, 34), 32);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'p', 5, 34), 32);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'q', 0, 34), 33);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'q', 5, 34), 33);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'a', 0, 34), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'x', 0, 34), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'x', 1, 34), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'x', 7, 34), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'d', 0, 34), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'1', 3, 34), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'1', 4, 34), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'1', 0, 34), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'1', 1, 34), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'1', 2, 34), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'6', 3, 34), 7);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'6', 8, 34), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'d', 2, 34), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), (byte)'d', 5, 34), 20);
			}
		} catch (IllegalAccessException e) {
			throw new RuntimeException(e);
		} catch (NoSuchFieldException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers#intrinsicIndexOfUTF16(Object, char, int, int)
	 */
	public static void test_intrinsicIndexOfUTF16() {
		try {
			Field valueField = String.class.getDeclaredField("value");
			valueField.setAccessible(true);

            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get(""), '\u0190', 0, 0), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190"), '\u0190', 0, 1), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190"), '\u0190', 1, 1), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190"), 'b', 0, 1), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190"), 'b', 1, 1), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190"), 'A', 0, 1), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), 'b', 0, 2), 1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), 'b', 1, 2), 1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), 'B', 0, 2), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), 'B', 1, 2), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), '\u0190', 0, 2), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), 'A', 1, 2), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b"), '\u0190', 1, 2), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b\u0190"), 'b', 1, 3), 1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'o', 0, 15), 14);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'o', 7, 15), 14);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), '\u0190', 0, 15), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'x', 0, 15), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'x', 1, 15), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'x', 7, 15), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'd', 0, 15), 3);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'd', 2, 15), 3);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmno"), 'd', 5, 15), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'o', 0, 16), 14);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'o', 7, 16), 14);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'p', 0, 16), 15);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'p', 5, 16), 15);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), '\u0190', 0, 16), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'x', 0, 16), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'x', 1, 16), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'x', 7, 16), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'd', 0, 16), 3);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'd', 2, 16), 3);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnop"), 'd', 5, 16), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'o', 0, 17), 14);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'o', 7, 17), 14);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'p', 0, 17), 15);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'p', 5, 17), 15);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'q', 0, 17), 16);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'q', 5, 17), 16);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), '\u0190', 0, 17), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'x', 0, 17), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'x', 1, 17), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'x', 7, 17), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'd', 0, 17), 3);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'd', 2, 17), 3);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190bcdefghijklmnopq"), 'd', 5, 17), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'o', 0, 31), 28);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'o', 7, 31), 28);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'p', 0, 31), 29);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'p', 5, 31), 29);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'q', 0, 31), 30);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'q', 5, 31), 30);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), '\u0190', 0, 31), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'x', 0, 31), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'x', 1, 31), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'x', 7, 31), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'd', 0, 31), 17);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), '1', 0, 31), 2);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), '1', 1, 31), 2);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), '1', 2, 31), 2);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), '1', 3, 31), 12);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), '1', 4, 31), 12);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'd', 2, 31), 17);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234cdefghijklmnopq"), 'd', 5, 31), 17);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'o', 0, 32), 29);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'o', 7, 32), 29);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'p', 0, 32), 30);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'p', 5, 32), 30);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'q', 0, 32), 31);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'q', 5, 32), 31);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), '\u0190', 0, 32), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'x', 0, 32), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'x', 1, 32), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'x', 7, 32), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'd', 0, 32), 18);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'd', 2, 32), 18);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b123456789012345cdefghijklmnopq"), 'd', 5, 32), 18);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'o', 0, 34), 31);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'o', 7, 34), 31);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'p', 0, 34), 32);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'p', 5, 34), 32);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'q', 0, 34), 33);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'q', 5, 34), 33);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '\u0190', 0, 34), 0);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'x', 0, 34), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'x', 1, 34), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'x', 7, 34), -1);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'd', 0, 34), 20);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '1', 3, 34), 12);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '1', 4, 34), 12);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '1', 0, 34), 2);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '1', 1, 34), 2);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '1', 2, 34), 2);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '6', 3, 34), 7);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), '6', 8, 34), 17);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'd', 2, 34), 20);
            Assert.assertEquals(helpers.intrinsicIndexOfUTF16(valueField.get("\u0190b12345678901234567cdefghijklmnopq"), 'd', 5, 34), 20);
		} catch (IllegalAccessException e) {
			throw new RuntimeException(e);
		} catch (NoSuchFieldException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers.intrinsicIndexOfStringLatin1(char[], int, char[], int, int)
	 */
	public static void test_intrinsicIndexOfStringLatin1() {
		try {
			Field valueField = String.class.getDeclaredField("value");
			valueField.setAccessible(true);

			// Latin1 tests are valid if and only if compact strings are enabled because of the way we extract the
			// String.value field and pass it off to the intrinsicIndexOfStringLatin1 API. The only way to ensure
			// the extracted array is in Latin1 format is to make sure compact strings are enabled.
			Field enableCompressionField = String.class.getDeclaredField("COMPACT_STRINGS");
			enableCompressionField.setAccessible(true);

			if ((boolean)enableCompressionField.get(null)) {
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("a"), 1, valueField.get("a"), 1, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("a"), 1, valueField.get("a"), 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("a"), 1, valueField.get("b"), 1, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("a"), 1, valueField.get("b"), 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("a"), 1, valueField.get("A"), 1, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab"), 2, valueField.get("a"), 1, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab"), 2, valueField.get("b"), 1, 0), 1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab"), 2, valueField.get("b"), 1, 1), 1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab"), 2, valueField.get("B"), 1, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab"), 2, valueField.get("B"), 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("a"), 1, valueField.get("ab"), 2, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("b"), 1, valueField.get("ba"), 2, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("b"), 1, valueField.get("ba"), 2, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("B"), 1, valueField.get("BA"), 2, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("B"), 1, valueField.get("BA"), 2, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab"), 2, valueField.get("ab"), 2, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab"), 2, valueField.get("ba"), 2, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab"), 2, valueField.get("Ab"), 2, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab"), 2, valueField.get("ab"), 2, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("aba"), 3, valueField.get("ba"), 2, 1), 1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("a"), 1, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("o"), 1, 0), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("o"), 1, 7), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("ab"), 2, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("abc"), 3, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("abcdefghijklmno"), 15, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("x"), 1, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("x"), 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("x"), 1, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("xy"), 2, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("xy"), 2, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("xy"), 2, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("d"), 1, 0), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("de"), 2, 0), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("def"), 3, 0), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("defghijklmn"), 11, 0), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("d"), 1, 2), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("de"), 2, 2), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("def"), 3, 2), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("defghijklmn"), 11, 2), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("d"), 1, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("de"), 2, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("def"), 3, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmno"), 15, valueField.get("defghijklmn"), 11, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("a"), 1, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("o"), 1, 0), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("o"), 1, 7), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("op"), 2, 0), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("op"), 2, 7), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("p"), 1, 0), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("p"), 1, 5), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("ab"), 2, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("abc"), 3, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("abcdefghijklmnop"), 16, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopx"), 16, valueField.get("x"), 1, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("x"), 1, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("x"), 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("x"), 1, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("xy"), 2, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("xy"), 2, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("xy"), 2, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("d"), 1, 0), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("de"), 2, 0), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("def"), 3, 0), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("defghijklmn"), 11, 0), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("d"), 1, 2), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("de"), 2, 2), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("def"), 3, 2), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("defghijklmn"), 11, 2), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("d"), 1, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("de"), 2, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("def"), 3, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnop"), 16, valueField.get("defghijklmn"), 11, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("a"), 1, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("o"), 1, 0), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("o"), 1, 7), 14);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("op1"), 3, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("op1"), 3, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("p"), 1, 0), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("p"), 1, 5), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("pq"), 2, 0), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("pq"), 2, 5), 15);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("q"), 1, 0), 16);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("q"), 1, 5), 16);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("ab"), 2, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("abc"), 3, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("abcdefghijklmnopq"), 17, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("x"), 1, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("x"), 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("x"), 1, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("xy"), 2, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("xy"), 2, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("xy"), 2, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("d"), 1, 0), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("de"), 2, 0), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("def"), 3, 0), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("defghijklmn"), 11, 0), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("d"), 1, 2), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("de"), 2, 2), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("def"), 3, 2), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("defghijklmn"), 11, 2), 3);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("d"), 1, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("de"), 2, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("def"), 3, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghijklmnopq"), 17, valueField.get("defghijklmn"), 11, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("a"), 1, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("o"), 1, 0), 28);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("o"), 1, 7), 28);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("op1"), 3, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("op1"), 3, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("p"), 1, 0), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("p"), 1, 5), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("pq"), 2, 0), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("pq"), 2, 5), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("q"), 1, 0), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("q"), 1, 5), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("ab"), 2, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("abc"), 3, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("abcdefghijklmnopq"), 17, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("x"), 1, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("x"), 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("x"), 1, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("xy"), 2, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("xy"), 2, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("xy"), 2, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("d"), 1, 0), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("de"), 2, 0), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("def"), 3, 0), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("defghijklmn"), 11, 0), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("1234"), 4, 0), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("1234"), 4, 1), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("1234"), 4, 2), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("1234"), 4, 3), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("1234"), 4, 4), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("12345678901234cdef"), 18, 0), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("12345678901234cdef"), 18, 1), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("12345678901234cdef"), 18, 2), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("1234cdefghijklmnop"), 18, 3), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("1234cdefghijklmnop"), 18, 4), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("d"), 1, 2), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("de"), 2, 2), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("def"), 3, 2), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("defghijklmn"), 11, 2), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("d"), 1, 5), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("de"), 2, 5), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("def"), 3, 5), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234cdefghijklmnopq"), 31, valueField.get("defghijklmn"), 11, 5), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("a"), 1, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("o"), 1, 0), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("o"), 1, 7), 29);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("op1"), 3, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("op1"), 3, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("p"), 1, 0), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("p"), 1, 5), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("pq"), 2, 0), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("pq"), 2, 5), 30);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("q"), 1, 0), 31);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("q"), 1, 5), 31);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("ab"), 2, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("abc"), 3, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("abcdefghijklmnopq"), 17, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("x"), 1, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("x"), 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("x"), 1, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("xy"), 2, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("xy"), 2, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("xy"), 2, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("d"), 1, 0), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("de"), 2, 0), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("def"), 3, 0), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("defghijklmn"), 11, 0), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("d"), 1, 2), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("de"), 2, 2), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("def"), 3, 2), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("defghijklmn"), 11, 2), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("d"), 1, 5), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("de"), 2, 5), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("def"), 3, 5), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab123456789012345cdefghijklmnopq"), 32, valueField.get("defghijklmn"), 11, 5), 18);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("a"), 1, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("o"), 1, 0), 31);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("o"), 1, 7), 31);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("op1"), 3, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("op1"), 3, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("p"), 1, 0), 32);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("p"), 1, 5), 32);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("pq"), 2, 0), 32);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("pq"), 2, 5), 32);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("q"), 1, 0), 33);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("q"), 1, 5), 33);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("ab"), 2, 0), 0);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("abc"), 3, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("abcdefghijklmnopq"), 17, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("x"), 1, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("x"), 1, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("x"), 1, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("xy"), 2, 0), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("xy"), 2, 1), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("xy"), 2, 7), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("d"), 1, 0), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("de"), 2, 0), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("def"), 3, 0), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("defghijklmn"), 11, 0), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("1234"), 4, 0), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("1234"), 4, 1), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("1234"), 4, 2), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("1234"), 4, 3), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("1234"), 4, 4), 12);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("12345678901234567c"), 18, 0), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("12345678901234567c"), 18, 1), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("12345678901234567c"), 18, 2), 2);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("67cdefghijklmnopq"), 17, 3), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("67cdefghijklmnopq"), 17, 4), 17);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("d"), 1, 2), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("de"), 2, 2), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("def"), 3, 2), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("defghijklmn"), 11, 2), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("d"), 1, 5), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("de"), 2, 5), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("def"), 3, 5), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("ab12345678901234567cdefghijklmnopq"), 34, valueField.get("defghijklmn"), 11, 5), 20);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghiklmnopqrstwyzabcdefghikabcdefghiklmnopqlmmnopqrstwyzabcdepoisd"), 71, valueField.get("abcdefghiklmnopqlmmnopqrstwyzabcdepoisd"), 39, 0), 32);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghiklmnopqrstwyzabcdefghikabcdefghiklmnopqlmmnopqrstwyzabcdepoisd"), 37, valueField.get("zabcdefghikabcdefg"), 17, 5), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghiklmnopqrstwyzabcdefghikabcdefghiklmnopqlmmnopqrstwyzabcdepoisd"), 37, valueField.get("zabcdefghikabcdefg"), 17, 2), -1);
				Assert.assertEquals(helpers.intrinsicIndexOfStringLatin1(valueField.get("abcdefghiklmnopqrstwyzabcdefghikabcdefghiklmnopqlmmnopqrstwyzabcdepoisd"), 38, valueField.get("zabcdefghikabcdefg"), 17, 5), 21);
			}
		} catch (IllegalAccessException e) {
			throw new RuntimeException(e);
		} catch (NoSuchFieldException e) {
			throw new RuntimeException(e);
		}
	}

	/**
	 * @tests com.ibm.jit.JITHelpers.intrinsicIndexOfStringUTF16(char[], int, char[], int, int)
	 */
	public static void test_intrinsicIndexOfStringUTF16() {
		try {
			Field valueField = String.class.getDeclaredField("value");
			valueField.setAccessible(true);

			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190"), 1, valueField.get("\u0190"), 1, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190"), 1, valueField.get("\u0190"), 1, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190"), 1, valueField.get("\u0191"), 1, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190"), 1, valueField.get("\u0191"), 1, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190"), 1, valueField.get("\u0192"), 1, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191"), 2, valueField.get("\u0190"), 1, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191"), 2, valueField.get("\u0191"), 1, 0), 1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191"), 2, valueField.get("\u0191"), 1, 1), 1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191"), 2, valueField.get("\u0193"), 1, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191"), 2, valueField.get("\u0193"), 1, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190"), 1, valueField.get("\u0190\u0191"), 2, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0191"), 1, valueField.get("\u0191\u0190"), 2, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0191"), 1, valueField.get("\u0191\u0190"), 2, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0193"), 1, valueField.get("\u0193\u0192"), 2, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0193"), 1, valueField.get("\u0193\u0192"), 2, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191"), 2, valueField.get("\u0190\u0191"), 2, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191"), 2, valueField.get("\u0191\u0190"), 2, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191"), 2, valueField.get("\u0192\u0191"), 2, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191"), 2, valueField.get("\u0190\u0191"), 2, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191\u0190"), 3, valueField.get("\u0191\u0190"), 2, 1), 1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0190"), 1, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0194"), 1, 0), 14);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0194"), 1, 7), 14);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0190\u0191"), 2, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0190\u0191c"), 3, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0195"), 1, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0195"), 1, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0195"), 1, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0195y"), 2, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0195y"), 2, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0195y"), 2, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0196"), 1, 0), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0196e"), 2, 0), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0196ef"), 3, 0), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0196efghijklmn"), 11, 0), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0196"), 1, 2), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0196e"), 2, 2), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0196ef"), 3, 2), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0196efghijklmn"), 11, 2), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0196"), 1, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0196e"), 2, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0196ef"), 3, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194"), 15, valueField.get("\u0196efghijklmn"), 11, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0190"), 1, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0194"), 1, 0), 14);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0194"), 1, 7), 14);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0194\u0197"), 2, 0), 14);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0194\u0197"), 2, 7), 14);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0197"), 1, 0), 15);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0197"), 1, 5), 15);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0190\u0191"), 2, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0190\u0191c"), 3, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0195"), 1, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0195"), 1, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0195"), 1, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0195y"), 2, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0195y"), 2, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0195y"), 2, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0196"), 1, 0), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0196e"), 2, 0), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0196ef"), 3, 0), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0196efghijklmn"), 11, 0), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0196"), 1, 2), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0196e"), 2, 2), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0196ef"), 3, 2), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0196efghijklmn"), 11, 2), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0196"), 1, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0196e"), 2, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0196ef"), 3, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197"), 16, valueField.get("\u0196efghijklmn"), 11, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0190"), 1, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0194"), 1, 0), 14);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0194"), 1, 7), 14);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0194\u01971"), 3, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0194\u01971"), 3, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0197"), 1, 0), 15);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0197"), 1, 5), 15);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0197\u0198"), 2, 0), 15);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0197\u0198"), 2, 5), 15);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0198"), 1, 0), 16);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0198"), 1, 5), 16);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0190\u0191"), 2, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0190\u0191c"), 3, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0195"), 1, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0195"), 1, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0195"), 1, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0195y"), 2, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0195y"), 2, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0195y"), 2, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0196"), 1, 0), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0196e"), 2, 0), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0196ef"), 3, 0), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0196efghijklmn"), 11, 0), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0196"), 1, 2), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0196e"), 2, 2), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0196ef"), 3, 2), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0196efghijklmn"), 11, 2), 3);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0196"), 1, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0196e"), 2, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0196ef"), 3, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, valueField.get("\u0196efghijklmn"), 11, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0190"), 1, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0194"), 1, 0), 28);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0194"), 1, 7), 28);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0194\u01971"), 3, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0194\u01971"), 3, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0197"), 1, 0), 29);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0197"), 1, 5), 29);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0197\u0198"), 2, 0), 29);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0197\u0198"), 2, 5), 29);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0198"), 1, 0), 30);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0198"), 1, 5), 30);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0190\u0191"), 2, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0190\u0191c"), 3, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0195"), 1, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0195"), 1, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0195"), 1, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0195y"), 2, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0195y"), 2, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0195y"), 2, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0196"), 1, 0), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0196e"), 2, 0), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0196ef"), 3, 0), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0196efghijklmn"), 11, 0), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("12\u01994"), 4, 0), 2);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("12\u01994"), 4, 1), 2);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("12\u01994"), 4, 2), 2);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("12\u01994"), 4, 3), 12);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("12\u01994"), 4, 4), 12);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("12\u0199456789012\u01994c\u0196ef"), 18, 0), 2);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("12\u0199456789012\u01994c\u0196ef"), 18, 1), 2);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("12\u0199456789012\u01994c\u0196ef"), 18, 2), 2);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("12\u01994c\u0196efghijklmn\u0194\u0197"), 18, 3), 12);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("12\u01994c\u0196efghijklmn\u0194\u0197"), 18, 4), 12);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0196"), 1, 2), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0196e"), 2, 2), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0196ef"), 3, 2), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0196efghijklmn"), 11, 2), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0196"), 1, 5), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0196e"), 2, 5), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0196ef"), 3, 5), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994c\u0196efghijklmn\u0194\u0197\u0198"), 31, valueField.get("\u0196efghijklmn"), 11, 5), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0190"), 1, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0194"), 1, 0), 29);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0194"), 1, 7), 29);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0194\u01971"), 3, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0194\u01971"), 3, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0197"), 1, 0), 30);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0197"), 1, 5), 30);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0197\u0198"), 2, 0), 30);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0197\u0198"), 2, 5), 30);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0198"), 1, 0), 31);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0198"), 1, 5), 31);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0190\u0191"), 2, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0190\u0191c"), 3, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0195"), 1, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0195"), 1, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0195"), 1, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0195y"), 2, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0195y"), 2, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0195y"), 2, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0196"), 1, 0), 18);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0196e"), 2, 0), 18);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0196ef"), 3, 0), 18);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0196efghijklmn"), 11, 0), 18);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0196"), 1, 2), 18);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0196e"), 2, 2), 18);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0196ef"), 3, 2), 18);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0196efghijklmn"), 11, 2), 18);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0196"), 1, 5), 18);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0196e"), 2, 5), 18);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0196ef"), 3, 5), 18);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u019945c\u0196efghijklmn\u0194\u0197\u0198"), 32, valueField.get("\u0196efghijklmn"), 11, 5), 18);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0190"), 1, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0194"), 1, 0), 31);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0194"), 1, 7), 31);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0194\u01971"), 3, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0194\u01971"), 3, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0197"), 1, 0), 32);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0197"), 1, 5), 32);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0197\u0198"), 2, 0), 32);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0197\u0198"), 2, 5), 32);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0198"), 1, 0), 33);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0198"), 1, 5), 33);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0190\u0191"), 2, 0), 0);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0190\u0191c"), 3, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0190\u0191c\u0196efghijklmn\u0194\u0197\u0198"), 17, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0195"), 1, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0195"), 1, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0195"), 1, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0195y"), 2, 0), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0195y"), 2, 1), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0195y"), 2, 7), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0196"), 1, 0), 20);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0196e"), 2, 0), 20);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0196ef"), 3, 0), 20);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0196efghijklmn"), 11, 0), 20);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("12\u01994"), 4, 0), 2);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("12\u01994"), 4, 1), 2);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("12\u01994"), 4, 2), 2);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("12\u01994"), 4, 3), 12);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("12\u01994"), 4, 4), 12);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("12\u0199456789012\u01994567c"), 18, 0), 2);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("12\u0199456789012\u01994567c"), 18, 1), 2);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("12\u0199456789012\u01994567c"), 18, 2), 2);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("67c\u0196efghijklmn\u0194\u0197\u0198"), 17, 3), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("67c\u0196efghijklmn\u0194\u0197\u0198"), 17, 4), 17);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0196"), 1, 2), 20);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0196e"), 2, 2), 20);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0196ef"), 3, 2), 20);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0196efghijklmn"), 11, 2), 20);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0196"), 1, 5), 20);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0196e"), 2, 5), 20);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0196ef"), 3, 5), 20);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 34, valueField.get("\u0196efghijklmn"), 11, 5), 20);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 29, valueField.get("\u0196efghijklmn"), 10, 3), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 29, valueField.get("\u0196efghijklmn"), 10, 5), -1);
			Assert.assertEquals(helpers.intrinsicIndexOfStringUTF16(valueField.get("\u0190\u019112\u0199456789012\u01994567c\u0196efghijklmn\u0194\u0197\u0198"), 30, valueField.get("\u0196efghijklmn"), 10, 5), 20);
		} catch (IllegalAccessException e) {
			throw new RuntimeException(e);
		} catch (NoSuchFieldException e) {
			throw new RuntimeException(e);
		}
	}
}
