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

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

/**
 * This tests Java.lang.String API added in Java 12 and later versions.
 *
 */
public class Test_StringIndent {
	public static Logger logger = Logger.getLogger(Test_String.class);

	private String empty = "";
	private String allWhiteSpace = "   ";
	private String latin1 = "abc123";
	private String emptyWithTerm = "\n";

	/*
	 * Test Java 12 API String.indent
	 */
	@Test(groups = { "level.sanity" })
	public void testIndent() {
		// Empty ""
		logger.debug("testIndent: test with string: " + empty);
		verifyIndentSingleLine(empty, empty, 0);
		verifyIndentSingleLine(empty, empty, 1);
		verifyIndentSingleLine(empty, empty, -1);

		// Newline Only "\n"
		logger.debug("testIndent: test with string: " + emptyWithTerm);
		verifyIndentSingleLine(emptyWithTerm, emptyWithTerm, 0);
		verifyIndentSingleLine(emptyWithTerm, " " + emptyWithTerm, 1);
		verifyIndentSingleLine(emptyWithTerm, emptyWithTerm, -1);

		// // Whitespace " "
		logger.debug("testIndent: test with string: " + allWhiteSpace);
		verifyIndentSingleLine(allWhiteSpace, allWhiteSpace, 0);
		verifyIndentSingleLine(allWhiteSpace, " " + allWhiteSpace, 1);
		verifyIndentSingleLine(allWhiteSpace, allWhiteSpace.substring(1), -1);
		verifyIndentSingleLine(allWhiteSpace, "", -3);
		verifyIndentSingleLine(allWhiteSpace, "", -4);

		// Whitespace with tab "\t "
		String tabAllWhiteSpace = "\t" + allWhiteSpace;
		logger.debug("testIndent: test with string: " + tabAllWhiteSpace);
		verifyIndentSingleLine(tabAllWhiteSpace, tabAllWhiteSpace, 0);
		verifyIndentSingleLine(tabAllWhiteSpace, " " + tabAllWhiteSpace, 1);
		verifyIndentSingleLine(tabAllWhiteSpace, allWhiteSpace, -1);

		// Single-line "abc123"
		logger.debug("testIndent: test with string: " + latin1);
		verifyIndentSingleLine(latin1, latin1, 0);
		verifyIndentSingleLine(latin1, " " + latin1, 1);
		verifyIndentSingleLine(latin1, latin1, -1);

		// Single-line with leading whitespace " abc123"
		String whiteSpaceLatin1 = allWhiteSpace + latin1;
		logger.debug("testIndent: test with string: " + whiteSpaceLatin1);
		verifyIndentSingleLine(whiteSpaceLatin1, whiteSpaceLatin1, 0);
		verifyIndentSingleLine(whiteSpaceLatin1, "  " + whiteSpaceLatin1, 2);
		verifyIndentSingleLine(whiteSpaceLatin1, whiteSpaceLatin1.substring(1), -1);
		verifyIndentSingleLine(whiteSpaceLatin1, whiteSpaceLatin1.substring(3), -3);
		verifyIndentSingleLine(whiteSpaceLatin1, whiteSpaceLatin1.substring(3), -4);

		// Single-line with leading whitespace and terminator " abc123\n"
		String whiteSpaceLatin1Term = allWhiteSpace + latin1 + emptyWithTerm;
		logger.debug("testIndent: test with string: " + whiteSpaceLatin1Term);
		verifyIndentSingleLine(whiteSpaceLatin1Term, whiteSpaceLatin1, 0);
		verifyIndentSingleLine(whiteSpaceLatin1Term, "  " + whiteSpaceLatin1, 2);
		verifyIndentSingleLine(whiteSpaceLatin1Term, whiteSpaceLatin1.substring(1), -1);
		verifyIndentSingleLine(whiteSpaceLatin1Term, whiteSpaceLatin1.substring(3), -3);
		verifyIndentSingleLine(whiteSpaceLatin1Term, whiteSpaceLatin1.substring(3), -4);

		// Multi-line with leading whitespace
		String multiLine = " \tabc\r\n    def\t\n";
		logger.debug("testIndent: test with string: " + multiLine);
		String firstLine = " \tabc";
		String secondLine = "    def\t";
		String[] multiLineArray;

		multiLineArray = new String[] { firstLine, secondLine };
		verifyIndentMultiLine(multiLine, multiLineArray, 0);

		multiLineArray = new String[] { " " + firstLine, " " + secondLine };
		verifyIndentMultiLine(multiLine, multiLineArray, 1);

		multiLineArray = new String[] { firstLine.substring(1), secondLine.substring(1) };
		verifyIndentMultiLine(multiLine, multiLineArray, -1);

		multiLineArray = new String[] { firstLine.substring(2), secondLine.substring(2) };
		verifyIndentMultiLine(multiLine, multiLineArray, -2);

		multiLineArray = new String[] { firstLine.stripLeading(), secondLine.stripLeading() };
		verifyIndentMultiLine(multiLine, multiLineArray, -5);
	}

	/*
	 * Test Java 12 API String.indent Helper Function
	 * 
	 * Verifies indent for a multi-line string.
	 */
	public void verifyIndentMultiLine(String input, String[] expected, int n) {
		String expectedResult = String.join("\n", expected);
		verifyIndentSingleLine(input, expectedResult, n);
	}

	/*
	 * Test Java 12 API String.indent Helper Function
	 * 
	 * Verifies indent for a single-line string.
	 */
	public void verifyIndentSingleLine(String input, String expected, int n) {
		String indentResult = input.indent(n);
		String expectedResult;

		if (input.equals(empty) || input.equals(emptyWithTerm)) {
			expectedResult = expected;
		} else {
			expectedResult = expected + emptyWithTerm;
		}

		Assert.assertEquals(indentResult, expectedResult,
				"indent: failed to indent " + n + " spaces - failed to produce " + expectedResult);
	}
}
