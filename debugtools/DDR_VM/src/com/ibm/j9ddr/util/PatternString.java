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
package com.ibm.j9ddr.util;

import java.io.Serializable;
import java.util.LinkedList;
import java.util.List;
import java.util.StringTokenizer;

/**
 * A string containing wildcards and other characters that enable it
 * to represent many strings at once.
 * @author sfoley
 */
public class PatternString implements Serializable {
	/** Pattern string's <code>serialVersionUID</code> */
	private static final long serialVersionUID = 3257289149408162611L;
	
	/*
	 * the following flags indicate special cases, the pattern
	 * with no wildcards and the pattern "*" which matches everything 
	 */
	private boolean isRegularString;
	private boolean alwaysMatches;
	
	final String pattern;
	final String tokens[];
	
	public static final char wildCard = '*';
	public static final char singleCharWildCard = '?';
	private static int nullArray[] = new int[0];
	public static final PatternString ALWAYS_MATCHES = new PatternString(String.valueOf(wildCard));
	
	 /**
     * Constructor for creating a PatternString class which can contain wildcard expressions and 
     * other characters to represent many strings at once.
     *
     * @param pattern	a String which can contain wildcards
     */
	public PatternString(String pattern) {
		this.pattern = pattern;
		if(pattern.equals(String.valueOf(wildCard))) {
			alwaysMatches = true;
			tokens = null;
			return;
		}
		
		StringTokenizer tokenizer = new StringTokenizer(pattern, 
					String.valueOf(wildCard) + singleCharWildCard, true);
		List tokenList = new LinkedList();
		isRegularString = true;
		while(tokenizer.hasMoreTokens()) {
			String token = tokenizer.nextToken();
			if(isRegularString) {
				switch(token.charAt(0)) {
					case wildCard:
					case singleCharWildCard:
						isRegularString = false;
				}
			}
			tokenList.add(token);
		}
		tokens = (String[]) tokenList.toArray(new String[tokenList.size()]);
	}
	
	 /**
     * Indicates the location in the PatternString of the first instance of a wildcard
     *
     * @return int	returns the index where the wildcard can be found, if no wildcards present returns -1, if pattern is '*' returns 0 
     */
	int indexOfWildcard() {
		if(isRegularString) {
			return -1;
		}
		if(alwaysMatches) {
			return 0;
		}
		int index = 0;
		for(int i=0; i<tokens.length; i++) {
			String token = tokens[i];
			switch(token.charAt(0)) {
				case wildCard:
				case singleCharWildCard:
					return index;
				default:
					index += token.length();
			}
		}
		//should never reach here
		return -1;
	}

	/**
	 * A class to represent the PatternString in two separate patterns
	 * 
	 */
	public static class PatternStringPair{
		public final PatternString first;
		public final PatternString second;
		
		/**
		 * Constructor for PatternStringPair, which represents a PatternString in two separate patterns
		 * 
		 * @param first		a PatternString which is the first pattern of the two that represent the PatternString
		 * @param second	a PatternString which is the second pattern of the two that represent the PatternString
		 */
		PatternStringPair(PatternString first, PatternString second) {
			this.first = first;
			this.second = second;
		}
		
		/**
		 * Constructor for PatternStringPair, which represents a PatternString in two separate patterns
		 * 
		 * @param first		a String which is first pattern of the two that represent the PatternString
		 * @param second	a String which is second pattern of the two that represent the PatternString
		 */
		PatternStringPair(String first, String second) {
			this(new PatternString(first), new PatternString(second));
		}
	}
	
	/**
	 * Splits the patterns into two separate patterns, the splitting being done by the
	 * character at the indicated index, so this character is not included in either string.  
	 * If such a character is a variable length wildcard then the 
	 * new patterns will begin and end with wildcards as needed.   
	 * 
	 * @param index	the index at which to split the PatternString in two parts
	 */
	public PatternStringPair split(int index) {
		int c = pattern.charAt(index);
		if(c == wildCard) {
			return new PatternStringPair(pattern.substring(0, index + 1),
				pattern.substring(index));
		}
		return new PatternStringPair(pattern.substring(0, index), pattern.substring(index + 1));
	}
	

	/**
	 * Finds the indices of the possible first locations of the indicated character.
	 * The results are returned in ascending order.
	 * 
	 * @param c 		index of the pattern within this PatternString
	 * @return int[]	an array indicating the possible first locations of the character indicated at the index provided.
	 */
	public int[] indexOf(int c) {
		int results[] = new int[pattern.length()];
		int totalLocations;
		int index = pattern.indexOf(c);
		if(index == -1) {
			totalLocations = findAdditionalLocations(results, pattern, c, true);
		}
		else {
			totalLocations = findAdditionalLocations(results, pattern.substring(0, index), c, true);
			results[totalLocations++] = index;
		}
		if(totalLocations == 0) {
			return nullArray;
		}
		if(totalLocations == pattern.length()) {
			return results;
		}
		int newResults[] = new int[totalLocations];
		System.arraycopy(results, 0, newResults, 0, totalLocations);
		return newResults;
	}
	
	
	private int findAdditionalLocations(int results[], String sub, int c, boolean forward) {
		int resultIndex = 0;
		int len = sub.length();
		for(int i=0; i<len; i++) {
			int j = (forward ? i : (len - i - 1)); 
			char ch = sub.charAt(j);
			if(ch == wildCard || ch == singleCharWildCard) {
				results[resultIndex++] = j;
			}
		}
		return resultIndex;
	}
	
	/**
	 * Finds the indices of the possible last locations of the indicated character.
	 * The results are returned in descending order.
	 * 
	 * @param c 		index of the pattern within this PatternString
	 * @return int[]	an array indicating the possible last locations of the character indicated at the index provided.
	 */
	public int[] lastIndexOf(int c) {
		int maxResults = pattern.length();
		int results[] = new int[maxResults];
		int totalLocations;
		int index = pattern.lastIndexOf(c);
		if(index == -1) {
			totalLocations = findAdditionalLocations(results, pattern, c, false);
		}
		else {
			int offset = index + 1;
			totalLocations = findAdditionalLocations(results, pattern.substring(offset), c, false);
			for(int i=0; i<totalLocations; i++) {
				results[i] += offset;
			}
			results[totalLocations++] = index;
		}
		if(totalLocations == 0) {
			return nullArray;
		}
		if(totalLocations == maxResults) {
			return results;
		}
		int newResults[] = new int[totalLocations];
		System.arraycopy(results, 0, newResults, 0, totalLocations);
		return newResults;
	}
	
	
	/**
	 * Checks if this PatternString contains no wildcards.
	 * 
	 * @return boolean 	true if this PatternString contains no wildcards, false otherwise
	 */
	public boolean isRegularString() {
		return isRegularString;
	}
	
	/**
	 * Checks if the provided character is a wildcard
	 * 
	 * @param c		the character to check whether it is a wildcard or not
	 * @return boolean 	true if the character given is a wildcard
	 */
	public static boolean isWildcard(char c) {
		return c == wildCard || c == singleCharWildCard;
	}
	
	/**
	 * Determine if there is a match to the pattern that ends with the given string
	 * 
	 * @param string	the String to match
	 * @return	boolean 	returns true if there is a match, false if not
	 */
	public boolean endsWith(String string) {
		String reversePattern = new StringBuffer(pattern).reverse().toString();
		String reverseString = new StringBuffer(string).reverse().toString();
		return new PatternString(reversePattern).startsWith(reverseString);
	}
	
	/** 
	 * Determines if the pattern matches with a given string.
	 * 
	 * @param string	the String to match
	 * @return	boolean 	returns true if there is a match, false if not
	 */
	public boolean isMatch(String string) {
		return alwaysMatches || (isRegularString ? pattern.equals(string) : isMatch(string, false));
	}
	
	/**
	 * Determine if there is a match to the pattern that starts with the given string
	 * 
	 * @param string	the String to match
	 * @return boolean 	returns true if there is a match, false if not
	 */
	public boolean startsWith(String string) {
		return alwaysMatches || (isRegularString ? pattern.startsWith(string) : isMatch(string, true));
	}
	
	
	/**
	 * A class to manage the state of a match between two strings.
	 *
	 */
	class MatchState implements Cloneable {
		boolean onWildCard; 
		boolean onSingleCharWildCard;
		int numChars;
		
		void reset() {
			onWildCard = onSingleCharWildCard = false;
			numChars = 0;
		}
		
		boolean matchesRemainder(String string, int stringStartIndex) {
			return ((string.length() == stringStartIndex + numChars) 
					|| (onWildCard && (string.length() >= stringStartIndex + numChars)));
		}
		
		void matchWildcard(char c) {
			switch(c) {
				case wildCard:
					onWildCard = true;
					return;
				case singleCharWildCard:
					onSingleCharWildCard = true;
					numChars++;
					return;
				default:
			}
		}
		
		boolean startsWithRemainder(String string, int stringStartIndex, String token) {
			String remainderToMatch = string.substring(stringStartIndex + numChars);
			return token.startsWith(remainderToMatch);
		}
		
		int matchToken(String string, int stringStartIndex, int searchIndex, String token) {
			if(onSingleCharWildCard) {
				//we add this here because the onDotExcludingSingleCharWildCard must consume at least one character
				searchIndex = Math.max(stringStartIndex + numChars, searchIndex);
			}
			int index = string.indexOf(token, searchIndex);
			if(index < 0) {
				return -1;
			}
			switch(index - stringStartIndex) {
				default: //index > 1
					if(!onWildCard) {
						if(!onSingleCharWildCard || stringStartIndex + numChars != index) {
							return -1;
						}
					}
					//fall through
				case 0:
					break;
			}
			return index;
		}
		
		public Object clone() {
			try {
				return super.clone();
			} catch(CloneNotSupportedException e) {}
			return null;
		}
	}
	
	/* in a multi-threaded environment this field would need to go */
	private MatchState savedState = new MatchState();
	
	private boolean isMatch(String string, boolean startsWith) {
		savedState.reset();
		return isMatch(string, 0, startsWith, 0, savedState);
	}
	
	private boolean isMatch(String string, int stringStartIndex, boolean startsWith, int startTokenIndex, MatchState state) {
		for(int i=startTokenIndex; i<tokens.length; i++) {
			if(startsWith && state.matchesRemainder(string, stringStartIndex)) {
				return true;
			}
			String currentToken = tokens[i];
			char c = currentToken.charAt(0);
			if(isWildcard(c)) {
				state.matchWildcard(currentToken.charAt(0));
			}
			else {
				int stringTokenIndex = state.matchToken(string, stringStartIndex, stringStartIndex, currentToken);
				if(stringTokenIndex < 0) {
					if(startsWith) {
						return state.startsWithRemainder(string, stringStartIndex, currentToken);
						//return currentToken.startsWith(string.substring(stringStartIndex));
					}
					return false;
				}
				else {
					//we have found the token in the string
					//now we check if we can find it again further along the string
					int searchIndex = stringTokenIndex + 1;
					while(true) {
						int nextIndex = state.matchToken(string, stringStartIndex, searchIndex, currentToken);
						if(nextIndex < 0) {
							break;
						}
						MatchState newState = (MatchState) state.clone();
						newState.reset();
						if(isMatch(string, nextIndex + currentToken.length(), startsWith, i+1, newState)) {
							return true;
						}
						searchIndex = nextIndex + 1;
					}
					state.reset();
					stringStartIndex = stringTokenIndex + currentToken.length();
				}
			}
		}
		return state.matchesRemainder(string, stringStartIndex);
	}

	/**
	 * Gives the pattern used to create this PatternString class
	 * 
	 * @return String 	returns the pattern used to construct this PatternString
	 */
	public String toString() {
		return pattern;
	}
	
	/*
	static int failures = 0;
	public static void main(String args[]) {
		checkMatch("", "ind", "abc", false);
		checkMatch("", "x", "abc", false);
		checkMatch("", "", "abc", true);
		checkMatch("", "ind", "indind", false);
		checkMatch("", "indind", "indind", false);
		checkMatch("", "ind", "ind", false);
		checkMatch("", "x", "ind", false);
		checkMatch("", "", "ind", true);
		
		checkMatch("" + wildCard, "ind", "abc", true);
		checkMatch("" + wildCard, "x", "abc", true);
		checkMatch("" + wildCard, "", "abc", true);
		checkMatch("" + wildCard, "ind", "indind", true);
		checkMatch("" + wildCard, "indind", "indind", true);
		checkMatch("" + wildCard, "ind", "ind", true);
		checkMatch("" + wildCard, "x", "ind", true);
		checkMatch("" + wildCard, "", "ind", true);
		
		
		checkMatch("" + singleCharWildCard, "ind", "abc", false);
		checkMatch("" + singleCharWildCard, "x", "abc", true);
		checkMatch("" + singleCharWildCard, "x", "x", true);
		checkMatch("" + singleCharWildCard, "", "abc", false);
		
		checkMatch("" + singleCharWildCard + singleCharWildCard, "ind", "abc", false);
		checkMatch("" + singleCharWildCard + singleCharWildCard, "x", "abc", false);
		checkMatch("" + singleCharWildCard + singleCharWildCard, "x", "x", false);
		checkMatch("" + singleCharWildCard + singleCharWildCard, "xx", "abc", true);
		checkMatch("" + singleCharWildCard + singleCharWildCard, "xx", "x", true);
		checkMatch("" + singleCharWildCard + singleCharWildCard, "", "abc", false);
		
		
		checkMatch("" + singleCharWildCard + wildCard, "ind", "abc", true);
		checkMatch("" + singleCharWildCard + wildCard, "x", "abc", true);
		checkMatch("" + singleCharWildCard + wildCard, "", "abc", false);
		checkMatch("" + singleCharWildCard + wildCard, "ind", "indind", true);
		checkMatch("" + singleCharWildCard + wildCard, "indind", "indind", true);
		checkMatch("" + singleCharWildCard + wildCard, "ind", "ind", true);
		checkMatch("" + singleCharWildCard + wildCard, "x", "ind", true);
		checkMatch("" + singleCharWildCard + wildCard, "", "ind", false);
		
		checkMatch("" + wildCard + singleCharWildCard, "ind", "abc", true);
		checkMatch("" + wildCard + singleCharWildCard, "x", "abc", true);
		checkMatch("" + wildCard + singleCharWildCard, "", "abc", false);
		checkMatch("" + wildCard + singleCharWildCard, "ind", "indind", true);
		checkMatch("" + wildCard + singleCharWildCard, "indind", "indind", true);
		checkMatch("" + wildCard + singleCharWildCard, "ind", "ind", true);
		checkMatch("" + wildCard + singleCharWildCard, "x", "ind", true);
		checkMatch("" + wildCard + singleCharWildCard, "", "ind", false);
		
		
		System.out.println("failures: " + failures);
	}
	
	static void checkMatch(String wildcard, String match, String pattern, boolean wildcardMatchesMatch) {
		checkMatch(wildcard + pattern, match + pattern, wildcardMatchesMatch);
		checkMatch(pattern + wildcard, pattern + match, wildcardMatchesMatch);
		checkMatch(pattern + wildcard + pattern, pattern + match + pattern, wildcardMatchesMatch);
	}
	
	static void checkMatch(String pattern, String string, boolean expectedResult) {
		CopyOfPatternString p = new CopyOfPatternString(pattern);
		boolean result = p.isMatch(string);
		if(result != expectedResult) {
			System.out.print("FAIL");
			failures++;
		}
		else {
			System.out.print("PASS");
		}
		System.out.println(": " + pattern + ", " + string + ", matches: " + result);
		
		
		if(expectedResult) {
			String newPattern = pattern + "end";
			p = new CopyOfPatternString(newPattern);
			//String newString = string + "end";
			result = p.startsWith(string);
			if(result) {
				System.out.print("PASS: ");
			}
			else {
				System.out.print("FAIL: ");
				failures++;
			}
			System.out.println(newPattern + ", " + string + ", starts with: " + result);
		}
		
	}
	*/
		
}
