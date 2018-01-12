/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

public class WildCard {

	private static final String WILDCARD_CHARACTER = "*";

	public static final int EXACT_MATCH = 0x00;
	public static final int LEADING_WILDCARD = 0x01;
	public static final int TRAILING_WILDCARD = 0x02;
	public static final int BOTH_WILDCARDS = (LEADING_WILDCARD | TRAILING_WILDCARD);

	/*
	 * Parses a wildcard string, storing the result in needle, needleLength and
	 * matchFlag. The result string, needle, is a pointer into the input string.
	 * This function only supports leading or trailing wild card characters. The
	 * only supported wild card character is '*'.
	 * 
	 * Returns matchFlag on success, or -1 if the input string contains wild
	 * card characters in an unsupported location.
	 */
	static int parseWildcard(String pattern, StringBuffer needle) {
		int matchFlag = 0;

		int indexOfInvalidWildCard = pattern.indexOf(WILDCARD_CHARACTER, 1);
		if (indexOfInvalidWildCard != -1 && indexOfInvalidWildCard != pattern.length() - 1) {
			return -1;
		}
		if (pattern.startsWith(WILDCARD_CHARACTER)) {
			matchFlag |= LEADING_WILDCARD;
			needle.append(pattern.substring(1));
		} else {
			needle.append(pattern);
		}
		if (pattern.endsWith(WILDCARD_CHARACTER)) {
			matchFlag |= TRAILING_WILDCARD;
			int length = needle.length() - 1;
			if(length != -1) {
				needle.delete(length, length + 1);
			}
		}
		return matchFlag;
	}

	/*
	 * Determines if needle appears in haystack, using the match rules encoded
	 * in matchFlag. matchFlag, needle and needleLength must have been
	 * initialized by calling parseWildcard.
	 * 
	 * Return TRUE on match, FALSE if no match.
	 */
	public static boolean wildcardMatch(long matchFlags, String needle, String haystack) {
		boolean retval = false;

		if (matchFlags == BOTH_WILDCARDS) {
			retval = haystack.contains(needle);
		} else if (matchFlags == EXACT_MATCH) {
			retval = haystack.equalsIgnoreCase(needle);
		} else if (matchFlags == LEADING_WILDCARD) {
			retval = haystack.endsWith(needle);
		} else if (matchFlags == TRAILING_WILDCARD) {
			retval = haystack.startsWith(needle);
		}
		return retval;
	}
}
