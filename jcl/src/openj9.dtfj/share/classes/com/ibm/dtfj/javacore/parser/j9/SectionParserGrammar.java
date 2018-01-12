/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9;

import java.io.IOException;
import java.util.Iterator;
import java.util.Vector;

import com.ibm.dtfj.javacore.parser.framework.parser.ILookAheadBuffer;
import com.ibm.dtfj.javacore.parser.framework.parser.ISectionParser;
import com.ibm.dtfj.javacore.parser.framework.parser.ParserException;
import com.ibm.dtfj.javacore.parser.framework.scanner.IGeneralTokenTypes;
import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;
import com.ibm.dtfj.javacore.parser.framework.scanner.ScannerException;
import com.ibm.dtfj.javacore.parser.framework.tag.ILineRule;
import com.ibm.dtfj.javacore.parser.framework.tag.ITagParser;

/**
 * Parent section parser that implements most of the grammar based functionality used
 * by the concrete section parsers.
 *
 */
public abstract class SectionParserGrammar implements ISectionParser{

	protected static final int FORCE_THROW = 1;
	protected ILookAheadBuffer fLookAheadBuffer;
	protected String fSectionName;
	protected J9TagManager fTagManager;
	protected ITagParser fTagParser;
	protected static final int DEFAULT_DEPTH = 3;
	protected boolean anyMatched = false;

	private final Vector fErrors;
	
	public SectionParserGrammar(String sectionName) {
		fSectionName = sectionName;
		fErrors = new Vector();
	}
	
	/**
	 * 
	 * @param lookAheadBuffer
	 * @param depth
	 * @throws ParserException
	 */
	protected void setLookAheadBuffer(ILookAheadBuffer lookAheadBuffer, int depth) throws ParserException{
		fLookAheadBuffer = lookAheadBuffer;
		try {
			fLookAheadBuffer.setLookAheadDepth(depth);
		} catch (IOException e) {
			/*
			 * IOException may indicate a non-recoverable error,
			 * so exist parser via error throw.
			 */
			handleError(e, FORCE_THROW);
		} catch (ScannerException e) {
			handleError(e, FORCE_THROW);
		}	
	}

	
	/**
	 * 
	 * @param lookAheadBuffer
	 * @throws ParserException
	 */
	protected void setLookAheadBuffer(ILookAheadBuffer lookAheadBuffer) throws ParserException {
		setLookAheadBuffer(lookAheadBuffer, DEFAULT_DEPTH);
	}
	
	/**
	 * 
	 * 
	 */
	protected ILookAheadBuffer getLookAheadBuffer() {
		return fLookAheadBuffer;
	}
	
	/**
	 * 
	 * @param tagManager
	 * @throws NullPointerException if tag manager is null
	 */
	protected void setTagManager(J9TagManager tagManager) {
		fTagManager = tagManager;
		fTagParser = fTagManager.getTagParser(fSectionName);
	}

	/**
	 * 
	 * @param scanner
	 * @throws ParserException
	 */
	protected void handleRequiredMismatch(String expectedType, String actualType, String actualValue) throws ParserException {
		handleErrorAtLineNumber(getCurrentFileLineNumber(), "Expected Type: " + expectedType + " / Actual Type: " + actualType + ": " + actualValue, null);
	}
	
	protected void handleUnknownMismatch(String actualType, String actualValue) throws ParserException {
		handleErrorAtLineNumber(getCurrentFileLineNumber(), "Unknown data -> " + actualType + ": " + actualValue, null);
	}
	
	/**
	 * 
	 * @param depth
	 * 
	 * @throws ParserException
	 */
	protected IParserToken lookAhead(int depth) throws ParserException{
		IParserToken token = null;
		try {
			token = fLookAheadBuffer.lookAhead(depth);
		} catch (IOException e) {
			handleError(e, FORCE_THROW);
		} catch (ScannerException e) {
			handleError(e, FORCE_THROW);
		} catch (IndexOutOfBoundsException e) {
			handleError(e, FORCE_THROW);
		}
		return token;
	}
	
	/**
	 * 
	 */
	public String getSectionName() {
		return fSectionName;
	}
	
	/**
	 * 
	 * @param lookAheadBuffer
	 * @throws ParserException
	 */
	protected void consume() throws ParserException {
		try {
			fLookAheadBuffer.consume();
		} catch (IOException e) {
			handleError(e, FORCE_THROW);
		} catch (ScannerException e) {
			handleError(e, FORCE_THROW);
		}
	}
	
	/**
	 * The match performs one additional function:
	 * <br>
	 * if the mismatch is due to an unrecognised javacore tag, that erroneous tag can be
	 * interpreted as garbage for now, so the latter is consumed without further processing. 
	 * <br>
	 * However, <b>beware</b> of considering
	 * all mismatches to be garbage. It is possible that the mismatch is due to a VALID
	 * javacore tag that pertains to another language rule. In this case, an argument
	 * to the method decides whether to handle it as an error (i.e., match was required) or let it fall through to the next
	 * grammar rule (match was optional).
	 * 
	 * @return true if matched, false otherwise.
	 * @param type to match
	 * @param required will generate an error, otherwise false ignores the mismatch.
	 */
	protected boolean match(String type, boolean required) throws ParserException {
		boolean matched = false;
		boolean searchMore = true;
		IParserToken token = null;
		String tokenType = null;
		String tokenValue = null;
		while (searchMore && !fLookAheadBuffer.allConsumed() && !(matched = fLookAheadBuffer.match(type))) {	
			if ((token = lookAhead(1)) != null) {
				tokenType = token.getType();
				tokenValue = token.getValue();
			}
			// If it is a valid javacore tag, do not consume
			// as it may be part of another section that is yet
			// to be parsed.
			if (isValidJavaCoreTag(token)) {
				searchMore = false;
				if (required) {
					handleRequiredMismatch(type, tokenType, tokenValue);
				}
			}
			else {
				//TODO: Uncomment this line for output on unhandled data.
//				handleUnknownMismatch(tokenType, tokenValue);
				consume();
			}
		}
		anyMatched |= matched;
		return matched;
	}
	
	/**
	 * 
	 * @param type
	 * 
	 * @throws ParserException
	 */
	protected IAttributeValueMap getLineRuleResults(IParserToken token) throws ParserException {
		IAttributeValueMap results = null;
		if (token != null) {
			ILineRule lineRule = fTagParser.getLineRule(token.getType());
			token = lookAhead(1);
			if (token != null && token.getType().equals(IGeneralTokenTypes.UNPARSED_STRING)) {
				consume();
				if (lineRule != null) {
					results = lineRule.parseLine(token.getValue(), token.getLineNumber(), token.getOffset());
				}
			}
		}
		else {
			handleError("Cannot get line rule for a null token.");
		}
		return results;
	}
	
	/*
	 * Some common grammar patterns used in javacore parsing.
	 * This is not an exhaustive list of all possible patterns, only
	 * some that commonly occur, and can be used in subclasses that
	 * implement grammar rules. It is not necessary to use any of
	 * these methods implement a functional parser. 
	 */
	
	/**
	 * Match a type, and generate an error if a mismatch occurs. This is usually
	 * used for grammar matches that are required.
	 * 
	 * @param type
	 * 
	 * @throws ParserException
	 */
	protected boolean matchRequired(String type) throws ParserException {
		return match(type, true);
	}
	
	/**
	 * Match a type, but do not generate an error if mismatch occurs.
	 * This is usually used when a terminal in a grammar rule is optional.
	 * 
	 * @param type
	 * 
	 * @throws ParserException
	 */
	protected boolean matchOptional(String type) throws ParserException {
		return match(type, false);
	}
	
	/**
	 * 
	 * @param tagName
	 * @return attributevaluemap if tag line found and parsed, or null if not found.
	 * @throws ParserException
	 */
	protected IAttributeValueMap processTagLineRequired(String tagName) throws ParserException {
		return processTagLine(tagName, true);
	}
	
	/**
	 * 
	 * @param tagName
	 * @return attributevaluemap if tag line found and parsed, or null if not found.
	 * @throws ParserException
	 */
	protected IAttributeValueMap processTagLineOptional(String tagName) throws ParserException {
		return processTagLine(tagName, false);
	}

	/**
	 * 
	 * @param tagName
	 * @param required
	 * @return attribute value map if tag line successfully parsed or null otherwise.
	 * @throws ParserException if parsing error threshold reached 
	 */
	private IAttributeValueMap processTagLine(String tagName, boolean required) throws ParserException {
		IAttributeValueMap list = null;
		if (required ? matchRequired(tagName) : matchOptional(tagName)) {
			IParserToken token = lookAhead(1);
			consume();
			list = getLineRuleResults(token);
		}
		return list;
	}
	
	/*
	 * 
	 * HELPER METHODS
	 * 
	 * 
	 */
	
	/**
	 * 
	 * @param tag
	 * 
	 */
	protected boolean isValidSectionTag(String tag) {
		return fTagManager.isTagInSection(tag, fSectionName);
	}
	
	/**
	 * 
	 * @param token
	 * 
	 */
	protected boolean isValidJavaCoreTag(IParserToken token) {
		return (token != null) ? fTagManager.isTagInSection(token.getType(), J9TagManager.CHECK_ALL) : false;
	}
	
	/**
	 * Returns the current line number being parsed, or -1 if no parsing is occurring (parsing is finished, etc..).
	 * 
	 * @return current line number of line being parsed, or -1 if there is nothing left to parse.
	 * @throws ParserException
	 */
	public int getCurrentFileLineNumber() throws ParserException {
		int fileLineNumber = -1;
		if (!fLookAheadBuffer.allConsumed()) {
			IParserToken token = lookAhead(1); 
			if (token != null) {
				fileLineNumber = token.getLineNumber();
			}
		}
		return fileLineNumber;
	}
	
	/**
	 * @return non null Iterator to a list of errors encountered during parsing.
	 */
	public Iterator getErrors() {
		return fErrors.iterator();
	}
	
	/*
	 * 
	 * ERROR HANDLING. Add error handlers as needed.
	 * 
	 */
	
	/**
	 * 
	 * @param message
	 * @throws ParserException
	 */
	protected void handleError(String message, Exception e) throws ParserException {
		handleError(message + e.getMessage());
	}
	
	protected void handleError(String message) throws ParserException {
		if (message != null) {
			fErrors.add(message);
		}
	}
	
	/**
	 * 
	 * @param fileLineNumber
	 * @param message
	 * @param e
	 * @throws ParserException
	 */
	protected void handleErrorAtLineNumber(int fileLineNumber, String message, Exception e) throws ParserException {
		String msg = "In Section " + getSectionName() + " at line " + fileLineNumber + " :: ";
		if (message != null) {
			msg += message;
		}
		if (e != null) {
			handleError(msg, e);
		}
		else {
			handleError(msg);
		}
	}
	
	/**
	 * 
	 * @param message
	 * @param offset
	 * @param length
	 * @throws ParserException
	 */
	protected void handleError(String message, int offset, int length) throws ParserException {
		handleError(message);
	}
	
	/**
	 * 
	 * @param e
	 * @throws ParserException
	 */
	protected void handleError(Exception e) throws ParserException {
		handleError(e.getMessage());
	}
	
	/**
	 * 
	 * @param e
	 * @param behaviour
	 * @throws ParserException
	 */
	protected void handleError(Exception e, int behaviour) throws ParserException {
		if (behaviour == FORCE_THROW) {
			throw new ParserException(e);
		}
	}
	
	public boolean anyMatched() {
		return anyMatched;
	}
}
