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
package com.ibm.dtfj.javacore.parser.framework.scanner;

import java.io.IOException;
import java.util.ArrayList;

import com.ibm.dtfj.javacore.parser.framework.input.IInputBuffer;
import com.ibm.dtfj.javacore.parser.j9.J9TagManager;

public class JavaCoreScanner implements IScanner {
	
	private IInputBuffer fInputBuffer;
	private StringBuffer fLocalCache;
	private int fLineNumber;
	private String fUnknownType;
	private String fUnparsedType;
	private J9TagManager fTagManager;
	private int fMaxLineLength;


	
	/*
	 * Manipulate as a FIFO queue.
	 */
	private ArrayList fTokenCache;
	
	/*
	 * Whitespace options are designated by setting individual bits.
	 */

	private static final short SEPARATE_ON_WHITESPACE = 0x01;
	private static final short GRAB_ALL = 0x02;



	/**
	 * 
	 * @param inputBuffer
	 * @throws NullPointerException if inputbuffer is null.
	 */
	public JavaCoreScanner(IInputBuffer inputBuffer, J9TagManager tagManager) {
		fTagManager = tagManager;
		fUnknownType = IGeneralTokenTypes.UNKNOWN_TAG;
		fUnparsedType = IGeneralTokenTypes.UNPARSED_STRING;
		fInputBuffer = inputBuffer;
		fLocalCache = new StringBuffer();
		fLineNumber = 0;
		fTokenCache = new ArrayList();
		fMaxLineLength = 32768;
	}


	
	/**
	 * @throws ScannerException 
	 * 
	 */
	public IParserToken next() throws  IOException, ScannerException {
		IParserToken token = null;
		if (fTokenCache.isEmpty()) {
			fillTokenCache();
		}
		if (fTokenCache.size() > 0) {
			token = (IParserToken) fTokenCache.remove(0);
		}
		return token;
	}
	

	
	
	/**
	 * 
	 * Scanned tokens are added to a cache first before being returned to the caller.
	 * @throws ScannerException 
	 * 
	 *
	 */
	private void fillTokenCache() throws  IOException, ScannerException {
		readNextLine();
		boolean done = false;
		while(!done && !fInputBuffer.endReached()) {
			clear(fLocalCache);
			nextCharacterSequence(SEPARATE_ON_WHITESPACE, fLocalCache);
			/*
			 * A token was identified, thus process.
			 */
			if (fLocalCache.length() > 0) {
				if (fTagManager.isComment(fLocalCache)) {
					consume(fLocalCache);
					/*
					 * If it is a comment, consume the rest of the buffer until endline without generating
					 * a token.
					 */
					if (moreLeftInLine()) {
						nextCharacterSequence(GRAB_ALL, fLocalCache);
						consume(fLocalCache);
					}
					readNextLine();
				}
				else  {
					processTag();
					done = true;
				}
			}
			/*
			 * either whitespace or some unidentifiable data was encountered
			 */
			else {
				if (moreLeftInLine()) consume(1);
				readNextLine();
			}
		}
		
	}
	
	
	
	/**
	 * 
	 *
	 */
	private boolean readNextLine() throws IOException {
		boolean result = false;
		if (fInputBuffer.length() == 0) {
			if (result = fInputBuffer.nextLine()) {
				fLineNumber++;
			}
		}
		return result;
	}

	
	
	
	/**
	 * Consumes a length equivalent to the argument buffer length from the input buffer, and clears
	 * the argument buffer.
	 * 
	 * @param buffer
	 * 
	 * @throws IOException
	 */
	private int consume(StringBuffer buffer) throws IOException {
		int offset = consume(buffer.length());
		clear(buffer);
		return offset;
	}
	
	
	/**
	 * 
	 * @param length
	 * 
	 * @throws IOException
	 */
	private int consume(int length) throws IOException {
		return fInputBuffer.consume(length);
	}
	
	

	
	
	
	
	/**
	 * 
	 * 
	 */
	private boolean moreLeftInLine() {
		return fInputBuffer.length() > 0;
	}

	
	
	/**
	 * 
	 * If a token has been identified as a valid tag, the rest of the characters on the same
	 * line are assumed to be the (unparsed) attributes for that tag. Whoever calls this scanner
	 * should send the unparsed attributes to a separate attribute parser. 
	 * 
	 * @param currentLineNumber
	 * @param tokenType
	 * @throws IOException
	 * @return if a token is generated return true
	 * @throws ScannerException 
	 */
	private void processTag() throws IOException, ScannerException {
		String value = fLocalCache.toString();
		String tokenType = fTagManager.hasTag(value) ? value : fUnknownType;
		int lengthConsumed = value.length();
		int offset = consume(fLocalCache);
		fTokenCache.add(TokenManager.getToken(lengthConsumed, offset, fLineNumber, tokenType, value));

		/*
		 * If more characters exist on the same line, get all of them as potential attributes of the
		 * tag (i.e., consume the rest of the line).
		 */
		if (moreLeftInLine()) {
			tokenType = fUnparsedType;
			nextCharacterSequence(GRAB_ALL, fLocalCache);
			value = fLocalCache.toString();
			lengthConsumed = fLocalCache.length();
			consume(fLocalCache);
			fTokenCache.add(TokenManager.getToken(lengthConsumed, offset, fLineNumber, tokenType, value));
		}
	}
	
	

	
	
	
	
	/**
	 * This method works on the precondition that the input buffer contains an entire line, excluding
	 * the line feed ('\r' or '\n' or both).
	 * 
	 * This method only returns a subsequence of characters based on a specified
	 * separating condition (usually around a whitespaces). It also allows
	 * the caller to specify if the whitespace should be included in the subsequence
	 * or not. Note that the pattern is NOT consumed from the buffer. It is only
	 * returned so that another method can do the proper analysis as to whether it should
	 * be consumed or not.
	 * 
	 * <br>
	 * <br>
	 * An empty target buffer (i.e., nothing is written into the target buffer) results when:
	 * - the input buffer is empty OR
	 * - The scanner is set to separate on the first whitespace encountered, that whitespace
	 * should not be written to the buffer, and the first character encountered happens to be a
	 * whitespace
	 * 
	 * 
	 * @param whiteSpaceOption indicates what to do if a white space is encountered 
	 * @param targetBuffer target buffer where subsequence of characters should be read
	 * @throws ScannerException 
	 * 
	 * @see #SEPARATE_ON_WHITESPACE
	 * @see #GRAB_ALL
	 * 
	 * @throws NullPointerException if the target buffer is null.
	 */
	private void nextCharacterSequence(int whiteSpaceOption, StringBuffer targetBuffer) throws ScannerException {
		int copiedWhiteSpaceOption = whiteSpaceOption;

		/*
		 * Clear "separate on whitespace" and end at new line bits until either one is encountered
		 */
		copiedWhiteSpaceOption &= ~(SEPARATE_ON_WHITESPACE);
		
		/*
		 * No consumption is occurring here, so the buffer length can be assumed to be fixed for
		 * this computation.
		 */
		int length = fInputBuffer.length();
		char nextCharacter;
		for(int i = 0; i < length && ((copiedWhiteSpaceOption & SEPARATE_ON_WHITESPACE) == 0); i++) {
			nextCharacter = fInputBuffer.charAt(i);
			if (Character.isWhitespace(nextCharacter) 
					&& ((whiteSpaceOption & SEPARATE_ON_WHITESPACE) == SEPARATE_ON_WHITESPACE)) {
				copiedWhiteSpaceOption |= SEPARATE_ON_WHITESPACE;
			}
			else {
				targetBuffer.append(nextCharacter);
			}
	
			/* Check for upper bound on line length. Likely bad input file. */
			if (i > fMaxLineLength) {
				throw new ScannerException("Maximum line length ("+fMaxLineLength+") exceeded. Input file corrupt or not a javacore.");
			}
		}
	}
	
	
	
	
	/**
	 * 
	 * @param buffer
	 */
	private void clear(StringBuffer buffer) {
		buffer.delete(0, buffer.length());
	}
	
	
	
	/**
	 * 
	 */
	public boolean allTokensGenerated() {
		return fInputBuffer.endReached() && fTokenCache.isEmpty();
	}


	/**
	 * @param maxLength
	 */
	public void setMaximumLineLength(int maxLength) {
		fMaxLineLength = maxLength;
	}
	
	/**
	 * @return
	 */
	public int getMaximumLineLength() {
		return fMaxLineLength;
	}
	

}
