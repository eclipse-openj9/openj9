/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.commands;

import java.util.List;

public class StringUtils {

	/**
	 * To get the first token end index.  Note tokens are separated by spaces.
	 * <p>
	 * @param line	The string containing all tokens.
	 * <p>
	 * @return	The end index of the first token.
	 */
	public static int getFirstTokenEndIndex(String line) {
		return getNextTokenEndIndex(0, line);
	}
	/**
	 * To get the next token end index.  Note tokens are separated by spaces.
	 * <p>
	 * @param startIndex	The start index from where the first token is to be searched.
	 * @param line		The string containing all tokens.
	 * <p>
	 * @return	The end index of the next token.
	 */
	public static int getNextTokenEndIndex(int startIndex, String line) {
		int index = startIndex;
		
		// move over to the first no space char.
		for(; index < line.length(); index++) {
			if(" \t\n\r\f".indexOf(line.charAt(index)) < 0) {
				break;
			}
		}
		
		// find the first non-space char or the end of the line.
		while(index < line.length()) {
			if(" \t\n\r\f".indexOf(line.charAt(index)) >= 0) {
				return index - 1;
			}
			if(index == line.length() - 1) {
				return index;
			} else {
				index++;
			}
		}
		return -1;
	}

	/**
	 * To check if any line in the list contains a special string.
	 * <p>
	 * @param allLines		All the lines.
	 * @param strToSearch	The string to look for.
	 * @param ignoreCase	Indicating whether the comparison should ignore case.
	 * <p>
	 * @return	<code>true</code> if any of the line contains the string to be searched;
	 * 			<code>false</code> otherwise.
	 */
	public static boolean containInAnyLines(List<String> allLines, String strToSearch, boolean ignoreCase) {

		for(String line : allLines) {
			if(ignoreCase) {
				if(line.toLowerCase().contains(strToSearch.toLowerCase())) {
					return true;
				}
			} else {
				if(line.contains(strToSearch)) {
					return true;
				}	
			}
		}
		return false;
	}
	
	/**
	 * To check if any line in the list contains a special string.
	 * <p>
	 * @param str			The string to be searched on.
	 * @param strToSearch	The string to look for.
	 * @param ignoreCase	Indicating whether the comparison should ignore case.
	 * <p>
	 * @return	<code>true</code> if string contains the string to be searched;
	 * 			<code>false</code> otherwise.
	 */
	public static boolean contain(String str, String strToSearch, boolean ignoreCase) {

		if(ignoreCase) {
			if(str.toLowerCase().contains(strToSearch.toLowerCase())) {
				return true;
			}
		} else {
			if(str.contains(strToSearch)) {
				return true;
			}	
		}
		return false;
	}
	
}
