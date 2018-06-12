/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2018 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.framework.tag;

import java.util.HashMap;
import java.util.regex.Matcher;

import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;
import com.ibm.dtfj.javacore.parser.framework.scanner.TokenManager;
import com.ibm.dtfj.javacore.parser.j9.AttributeValueMapFactory;
import com.ibm.dtfj.javacore.parser.j9.IAttributeValueMap;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

/**
 * This is the basic line parser for a javacore tag. A set of frequently-encountered pattern matching
 * behaviours are implemented as helper methods to facilitate pattern matching. However, the concrete subtype
 * must implement the actual pattern matching and token generation logic for a particular tag.
 * Each tag is ideally assigned it's own linerule, unless multiple tags share the exact same pattern.
 * <br><br>
 * This linerule parses a line based on the parsing logic implemented by the concrete type, and adds
 * the parsed data as tokens to an internal token map. The concrete classes only need to specify
 * HOW the line is parsed, and HOW the tokens are generated, but it need not worry about the actual
 * line parsing call. The parsing framework calls the line parser internally.
 * <br><br>
 * <b>GENERAL BEHAVIOUR</b>:
 * See the javadoc of the helper methods, but generally, as tokens are generated, or alternately, sections
 * of the source string (which is stored internally in a buffer) matched and discarded, the processed subsections are removed from the source string
 * until nothing remains. Some helper methods may just find a pattern but not do anything, others will find the first
 * occurrence of a pattern, and discard all characters from the start of the source to the first encountered pattern.
 * <br><br>
 * <b>HANDLING TOKEN TYPES</b>:
 * Based on how javacores are structured, it is assumed that
 * each tag line contains at most one instance of a token type.
 * If two or more of the same token types exist, this is indicative
 * that the line should be split into two or more separate javacore
 * tag lines. Consequently, only at most one instance of a token
 * type is added to the token map. If the token type already exists
 * in the map, it will be overwritten.
 * <br><br>
 * The parser framework will execute the line parsing logic implemented by the concrete subclass and
 * return a list of tokens (generally a map, where the key is the token type, and the value is the token value)
 * to the section parser that uses these line rules. A series of line rules
 * can be registered into a Tag Parser, which in turn gets used by a Section Parser.
 * 
 * <br><br>
 * Note that methods are provided to explicitly generate and add a token, and the concrete-class must
 * call these methods explicitly to add a token. 
 * <br><br>

 * 
 * 
 * @see com.ibm.dtfj.javacore.parser.framework.tag.ITagParser
 * @see com.ibm.dtfj.javacore.parser.framework.tag.ITagAttributeResult
 */
public abstract class LineRule implements ILineRule {
	
	protected final StringBuffer fSource;
	protected final StringBuffer fCharSubSet;
	protected int fLineNumber;
	
	// Support for offset calculation of a token, but 
	// not currently used.
	protected int fOffSet;
	private HashMap fTokenList;


	public LineRule() {
		fSource = new StringBuffer();
		fCharSubSet = new StringBuffer();
		fLineNumber = 0;
		fOffSet = 0;
	}
	
	/**
	 * Initializes buffer, token list, and other fields prior to parsing and tokenising a line.
	 * The user-implemented method that parses the line is called here. This method is called
	 * internally by the framework, so the user never needs to explicitly call this method.
	 * 
	 * @param source string containing source to parse
	 * @param lineNumber line number on file of source
	 * @param startingOffset offset of the first character in the source line. offsets are determined externally from the start of the file.
	 * @return attribute value map containing parsed tokens.
	 */
	public IAttributeValueMap parseLine(String source, int lineNumber, int startingOffset) {
		fSource.delete(0, fSource.length());
		fSource.append(source);
		fLineNumber = lineNumber;
		fTokenList = new HashMap();
		// User-implemented
		processLine(source, startingOffset);
		return AttributeValueMapFactory.createAttributeValueMap(fTokenList);
	}
	
	/**
	 * @return line number on disk of the source line being parsed.
	 */
	protected int getLineNumber() {
		return fLineNumber;
	}
	
	/**
	 * Must be implemented by the subtype. It gets called internally
	 * by the parser framework, so the user need not worry about
	 * calling this explicitly. 
	 * 
	 * @param startingOffset
	 * @param source to be parsed.
	 */
	protected abstract void processLine(String source, int startingOffset);

	/**
	 * Adds a token of the type specified via argument to the internal token map, and assigns it 
	 * the value matched by the pattern matcher. The generated token is
	 * returned, or null if no pattern was matched.
	 * <br><br>
	 * If the token type already exists in the token map, it overwrites the value with the new token.
	 * 
	 * @param type of the token to be generated
	 * @param matcher containing pattern to match. the value matched is assigned as the value of the token
	 * @return generated token if match is successful, or null otherwise
	 */
	protected IParserToken addToken(String type, Matcher matcher) {
		return internalAddToken(type, matcher);
	}
	
	/**
	 * Adds a non-null token to the token map. If the token type already exists, it
	 * overwrites the value.
	 * 
	 * @param token
	 */
	protected void addToken(IParserToken token) {
		if (token != null) {
			fTokenList.put(token.getType(), token);
		}
	}
	
	/**
	 * Adds a token of the type specified to the internal token list and assigns it the value specified by the argument.
	 * Returns the generated token.
	 * 
	 * <br><br>
	 * If the token type already exists in the token map, it overwrites the value with the new token.
	 * 
	 * @param type of the token
	 * @param value of the token
	 * @return generated token.
	 */
	protected IParserToken addToken(String type, String value) {
		IParserToken token = TokenManager.getToken(value.length(), fOffSet, fLineNumber, type, value);
		addToken(token);
		return token;
	}
	
	
	/**
	 * 
	 * Grabs all characters up to the first match specified by the matcher, and adds
	 * the latter as a token of a argument-specified type to the internal token list. All the aforementioned characters
	 * as well as the first pattern matched are removed from the source buffer. The matched pattern
	 * is not included in the token.
	 * 
	 * <br><br>
	 * If the token type already exists in the token map, it overwrites the value with the new token.
	 * @param type
	 * @param matcher
	 * @return generated token if match found, or null otherwise
	 */
	protected IParserToken addAllCharactersAsTokenAndConsumeFirstMatch(String type, Matcher matcher) {
		matcher.reset(fSource);
		IParserToken token = null;
		if (matcher.find()) {
			int startIndex = matcher.start();
			int matchLength = matcher.end() - startIndex;
			token = addAllCharactersAsTokenUntilIndex(type, startIndex, false);
			fSource.delete(0, matchLength);
		}
		return token;
	}
	
	/**
	 * Grabs all characters up to but excluding the first match specified by the matcher,
	 * and adds the latter as a token of an argument-specified type.
	 * 
	 * <br><br>
	 * If the token type already exists in the token map, it overwrites the value with the new token.
	 * @param type
	 * @param matcher
	 * @return generated token if match found, or null otherwise
	 */
	protected IParserToken addAllCharactersAsTokenUntilFirstMatch(String type, Matcher matcher) {
		matcher.reset(fSource);
		IParserToken token = null;
		if (matcher.find()) {
			token = addAllCharactersAsTokenUntilIndex(type, matcher.start(), false);
		}
		return token;
	}
	
	/**
	 * Copies all characters from start to endIndex - 1. Also gives the option to strip trailing whitespace.
	 * <br><br>
	 * If the token type already exists in the token map, it overwrites the value with the new token.
	 * @param type token type
	 * @param endIndex
	 * @param stripTrailingWhitespace if whitespace starting from endIndex - 1 to n, where n >=0 && n < endIndex.
	 * @throws IndexOutOfBoundsException if endIndex > source buffer length || endIndex <= 0
	 * @return generated token
	 */
	protected IParserToken addAllCharactersAsTokenUntilIndex(String type, int endIndex, boolean stripTrailingWhitespace) {
		if (endIndex > fSource.length() || endIndex < 0) {
			throw new IndexOutOfBoundsException(endIndex+":"+fSource.length());
		}

		int adjustedEndIndex = endIndex - 1;

		if (stripTrailingWhitespace) {
			int i = adjustedEndIndex;
			for (; i >= 0 && Character.isWhitespace(fSource.charAt(i));i--);
			adjustedEndIndex = i;
		}
		fCharSubSet.delete(0, fCharSubSet.length());

		for (int i = 0; i <= adjustedEndIndex; i++) {
			fCharSubSet.append(fSource.charAt(i));
		}
		fSource.delete(0, endIndex);
		IParserToken token = TokenManager.getToken(fCharSubSet.length(), fOffSet, fLineNumber, type, fCharSubSet.toString());
		addToken(token);
		return token;
	}
	
	/**
	 * Matches the first hexadecimal match encountered that is not prefixed by "0x" as a token. The latter
	 * is then prefixed to the value of the token before generating the token.
	 * 
	 * <br><br>
	 * If the token type already exists in the token map, it overwrites the value with the new token.
	 * 
	 * @param type of token to be generated
	 * @return generated token if hexadecimal match found, or null otherwise.
	 */
	protected IParserToken addNonPrefixedHexToken(String type) {
		Matcher matcher = CommonPatternMatchers.hex;

		matcher.reset(fSource);
		IParserToken token = null;
		if (matcher.find()) {
			fCharSubSet.delete(0, fCharSubSet.length());
			int startIndex = matcher.start();
			int endIndex = matcher.end();
			fCharSubSet.append('0');
			fCharSubSet.append('x');
			for (int i = startIndex; i < endIndex; i++) {
				fCharSubSet.append(fSource.charAt(i));
			}
			String value = fCharSubSet.toString();
			fSource.delete(0, endIndex);
			token = addToken(type, value);
		}
		return token;
	}
	
	/**
	 * Matches the first prefixed hexadecimal pattern encountered, and adds it as a token.
	 * Pattern, and all characters prior to the pattern are consumed from the buffer.
	 * <br><br>
	 * If the token type already exists in the token map, it overwrites the value with the new token.
	 * @param type of token to be generated
	 * @return generated token, if match found, or null otherwise
	 */
	protected IParserToken addPrefixedHexToken(String type) {
		return addToken(type, CommonPatternMatchers.hex_0x);
	}

	/**
	 * Used internally for adding a token to the token list.
	 * 
	 * <br><br>
	 * If the token type already exists in the token map, it overwrites the value with the new token.
	 * 
	 * @param type
	 * @param matcher
	 * @param offset
	 * @return matched token or null otherwise
	 */
	private IParserToken internalAddToken(String type, Matcher matcher) {
		String value = null;
		IParserToken token = null;
		int offset = fOffSet;
		if ((value = matchAndConsumeValue(matcher)) != null) {
			token = TokenManager.getToken(value.length(), offset, fLineNumber, type, value);
			fTokenList.put(token.getType(), token);
		}
		return token;
	}
	


	/**
	 * Finds the first occurrence of the pattern in the source.
	 * <br>
	 * Returns the character sequence that matched that pattern.
	 * <br>
	 * Deletes any character sequences from the start of the source to the end
	 * of the matched character sequence, and retains the remainder characters for
	 * further analysis.
	 * <br>
	 * @param matcher containing pattern to match

	 * @return string value or null
	 */
	protected String matchAndConsumeValue(Matcher matcher) {
		matcher.reset(fSource);
		String value = null;
		if (matcher.find()) {
			value = matcher.group();
			fSource.delete(0, matcher.start() + value.length());
		}
		return value;
	}
	
	
	/**
	 * Consumes a section of characters from the buffer.
	 * 
	 * @param startIndex inclusive
	 * @param endIndex exclusive
	 * @throws IndexOutOfBoundsException if start and end are outside the buffer range.
	 * @return String value of consumed characters
	 */
	protected String consumeCharacters(int startIndex, int endIndex) {
		String value = fSource.substring(startIndex, endIndex);
		fSource.delete(startIndex, endIndex);
		return value;
	}
	
	/**
	 * Matches the first occurrence of the pattern and consumes all characters
	 * from the start of the buffer until the first occurrence of the pattern.
	 * The pattern itself is also consumed. No tokens are generated.
	 * <br>
	 * @param matcher
	 * @return true if matched and consumed, false otherwise
	 */
	protected boolean consumeUntilFirstMatch(Matcher matcher) {
		matcher.reset(fSource);
		boolean matched = false;
		if (matched = matcher.find()) {
			fSource.delete(0, matcher.end());
		}
		return matched;
	}
	
	/**
	 * Finds the first match. No consumption or token generation occurs. 
	 * The buffer state is left intact.
	 * <br>
	 * @param matcher
	 * @return true if match found, false otherwise
	 */
	protected boolean findFirst(Matcher matcher) {
		matcher.reset(fSource);
		return matcher.find();
	}
	
	/**
	 * Returns the starting index of the last matched pattern, but doesn't consume anything or generate
	 * tokens.
	 * @param matcher
	 * @return starting index of last match, or -1 if nothing found
	 */
	protected int indexOfLast(Matcher matcher) {
		int startingIndex = -1;
		matcher.reset(fSource);
		while(matcher.find()) {
			startingIndex = matcher.start();
		}
		return startingIndex;
	}
}
