/*
 * Copyright IBM Corp. and others 2024
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
package jit.test.tr.arrayTranslate;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity","component.jit" })
public class ArrayTranslateTest {

	private static final int arrSize = 128;
	private char[] chars;
	private byte[] bytes;

	private static final char maxValueTRTO = 127;
	private static final char maxValueTRTO255 = 255;

	/* JIT-compile this method */
	private int arrayTranslateTRTO(char[] sa, int sp, byte[] da, int dp, int len)
	{
		int i = 0;
		for (; i < len; i++) {
			char c = sa[sp++];
			if (c > maxValueTRTO) break;
			da[dp++] = (byte)c;
		}
		return i;
	}

	@Test
	public void testArrayTranslateTRTO()
	{
		chars = new char[arrSize];
		bytes = new byte[arrSize];

		// Fill chars[] with safe characters
		for (int i = 0; i < arrSize; i++) {
			chars[i] = (char)('0' + (i >> 1));
		}

		for (int offset = 0; offset <= 16; offset++) {
			for (int len = 0; len < arrSize-offset; len++) {
				int n = arrayTranslateTRTO(chars, offset, bytes, 0, len);
				AssertJUnit.assertEquals("TRTO: Wrong result: length=" + len, len, n);
				for (int i = 0; i < len; i++) {
					AssertJUnit.assertEquals("TRTO: Wrong value at bytes[" + i + "]", chars[offset+i], (char)bytes[i]);
				}
			}
		}

		for (int len = 1; len <= 64; len++) {
			for (int i = 0; i < len; i++) {
				char c = chars[i];
				chars[i] = maxValueTRTO + 1;
				int n = arrayTranslateTRTO(chars, 0, bytes, 0, len);
				AssertJUnit.assertEquals("TRTO: Wrong result for failure case: length=" + len, i, n);
				chars[i] = c;
			}
		}
	}

	/* JIT-compile this method */
	private int arrayTranslateTRTO255(char[] sa, int sp, byte[] da, int dp, int len)
	{
		int i = 0;
		for (; i < len; i++) {
			char c = sa[sp++];
			if (c > maxValueTRTO255) break;
			da[dp++] = (byte)c;
		}
		return i;
	}

	@Test
	public void testArrayTranslateTRTO255()
	{
		chars = new char[arrSize];
		bytes = new byte[arrSize];

		// Fill chars[] with safe characters
		for (int i = 0; i < arrSize; i++) {
			chars[i] = (char)('0' + (i >> 1));
		}

		for (int offset = 0; offset <= 16; offset++) {
			for (int len = 0; len < arrSize-offset; len++) {
				int n = arrayTranslateTRTO255(chars, offset, bytes, 0, len);
				AssertJUnit.assertEquals("TRTO255: Wrong result: length=" + len, len, n);
				for (int i = 0; i < len; i++) {
					AssertJUnit.assertEquals("TRTO255: Wrong value at bytes[" + i + "]", chars[offset+i], (char)bytes[i]);
				}
			}
		}

		for (int len = 1; len <= 64; len++) {
			for (int i = 0; i < len; i++) {
				char c = chars[i];
				chars[i] = maxValueTRTO255 + 1;
				int n = arrayTranslateTRTO255(chars, 0, bytes, 0, len);
				AssertJUnit.assertEquals("TRTO255: Wrong result for failure case: length=" + len, i, n);
				chars[i] = c;
			}
		}
	}

}
