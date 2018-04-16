/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package com.oti.j9.exclude;

// Reference: http://www.w3.org/TR/REC-xml
import java.util.Hashtable;

public class XMLSpec {

	static final Hashtable sEscapedChars;

	static {
		sEscapedChars = new Hashtable();
		sEscapedChars.put("lt",  "<");
		sEscapedChars.put("gt",  ">");
		sEscapedChars.put("amp", "&");
		sEscapedChars.put("apos","'");
		sEscapedChars.put("quot","\"");
	}

	static boolean isLetter(char c)
	{
		return (c >= 'A' && c <= 'z');
	}

	static boolean isDigit(char c)
	{
		return Character.isDigit(c);
	}

	static boolean isNameChar(char c)
	{
		return isLetter(c) || isDigit(c) || isAnyOf(".-_:", c);
	}

	static boolean isNameStartChar(char c)
	{
		return isLetter(c) || isAnyOf("_:", c);
	}

	static boolean isLineDelimiter(char c)
	{
		return c == '\n' || c == '\r';
	}

	static boolean isWhitespace(char c)
	{
		return Character.isWhitespace(c);
	}

	static boolean isSpace(char c)
	{
		return c == ' ';
	}

	static String namedEscapeToString(String escapeSequence)
	{
		return (String) sEscapedChars.get(escapeSequence);
	}

	static boolean isAnyOf(String chars, char c)
	{
		return chars.indexOf(String.valueOf(c), 0) >= 0;
	}

}
