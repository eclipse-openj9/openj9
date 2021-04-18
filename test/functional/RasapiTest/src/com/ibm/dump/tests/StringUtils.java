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
package com.ibm.dump.tests;

import java.util.ArrayList;
import java.util.NoSuchElementException;
import java.util.StringTokenizer;


public class StringUtils {

	/**
	 * Check that the line contains a given value at a given position. 
	 * Whether it does or not, write a line to System.err to trace progress. 
	 * 
	 * @param line the line to check
	 * @param whichWord which word in the line to check
	 * @param requiredValue the number to look for
	 * @return true or false depending on whether the line does indeed contain the given number at the given word 
	 */
	static boolean linePassesCheckForStringAtWord(String line, int whichWord, String requiredValue) {
		StringTokenizer st = new StringTokenizer(line);
		String word = "";
		try {
			for (int i = 0;i < whichWord; i++) {
				word = st.nextToken();
			}
		} catch (NoSuchElementException e) {
			System.err.printf("***Failed to find the value %s at word %d in line \"%s\", no such element\n", requiredValue, whichWord, line, word) ;		
			return false;			
		}
		if (word.equals(requiredValue)) {
			System.err.printf("Successfully found the value %s at word %d in line \"%s\"\n", requiredValue, whichWord, line) ;		
			return true; 
		} else {
			System.err.printf("***Failed to find the value %s at word %d in line \"%s\", found %s instead\n", requiredValue, whichWord, line, word) ;		
			return false;
		}

	}

	/**
	 * Check that the line contains one of a number of given values at a given position. 
	 * Whether it does or not, write a line to System.err to trace progress. 
	 * 
	 * @param line the line to check
	 * @param whichWord which word in the line to check
	 * @param requiredValue the number to look for
	 * @return true or false depending on whether the line does indeed contain the given number at the given word 
	 */
	static boolean linePassesCheckForStringsAtWord(String line, int whichWord, String[] requiredValues) {
		StringTokenizer st = new StringTokenizer(line);
		String word = "";
		try {
			for (int i = 0;i < whichWord; i++) {
				word = st.nextToken();
			}
		} catch (NoSuchElementException e) {
			System.err.printf("***Failed to find any value at word %d in line \"%s\", no such element\n", whichWord, line, word) ;		
			return false;			
		}
		for (String requiredValue : requiredValues) {
			if (word.equals(requiredValue)) {
				System.err.printf("Successfully found the value %s at word %d in line \"%s\"\n", requiredValue, whichWord, line) ;		
				return true; 
			} 
		}
		System.err.printf("***Failed to find any of the required values at word %d in line \"%s\", found %s instead. ", whichWord, line, word) ;
		System.err.printf("The required values were:") ;
		for (String requiredValue : requiredValues) {
			System.err.printf(requiredValue + " ");			
		}
		System.err.printf("\n");			
		return false;
	}


	/**
	 * Given a line break it into an array of tokens (words)
	 * @param line
	 * @return arraylist of tokens (words)
	 */
	public static ArrayList<String> extractTokensFromLine(String line) {
		ArrayList<String> tokens = new ArrayList<String>();
		StringTokenizer st = new StringTokenizer(line);
		while (st.hasMoreTokens()) {
			String word = st.nextToken();
			tokens.add(word);
		}
		return tokens;
	}


}
