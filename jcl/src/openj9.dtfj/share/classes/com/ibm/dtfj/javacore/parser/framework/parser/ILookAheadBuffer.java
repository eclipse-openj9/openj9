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
package com.ibm.dtfj.javacore.parser.framework.parser;

import java.io.IOException;

import com.ibm.dtfj.javacore.parser.framework.scanner.IParserToken;
import com.ibm.dtfj.javacore.parser.framework.scanner.ScannerException;


/**
 * Prediction buffer that holds a certain number of tokens. Tokens are consumed when the are matched, and the buffer
 * is kept filled all the time until end of token stream is reached.
 *
 */
public interface ILookAheadBuffer {
	
	/**
	 * Matches a given type to the token type found at the first element of the buffer.
	 * 
	 * @param type to match against first element in buffer
	 * @return true if match is successful
	 */
	public boolean match(String type);
	
	
	/**
	 * Consumes the first element of the buffer.
	 * 
	 * @throws ScannerException 
	 * @throws IOException
	 */
	public void consume() throws IOException, ScannerException;
	
	
	/**
	 * 
	 * @return true if all tokens from both the input stream and the lookahead buffer have been consumed
	 */
	public boolean allConsumed();
	
	/**
	 * Looks ahead by a depth amount. The lowest depth to look ahead is 1. Note that in some cases,
	 * a lookahead may be attempted on an earlier error condition that lead to an empty buffer (e.g., 
	 * an exception was thrown during a fill process, and although more tokens may remain in the
	 * input source, the lookahead value may itself be null. Consequently, the lookahead buffer refills first
	 * before attempting the lookahead.
	 * 
	 * @param depth to lookahead
	 * @return token at specified lookahead depth or {@link IndexOutOfBoundsException}.
	 * @throws ScannerException
	 * @throws IOException
	 * @throws IndexOutOfBoundsException if depth is outside the depth range of the buffer. Minimum depth is always 1. 
	 */
	public IParserToken lookAhead(int depth) throws IOException, ScannerException, IndexOutOfBoundsException;
	
	
	public int length();
	
	public int maxDepth();
	
	/**
	 * 
	 * @throws ScannerException
	 * @throws IOException
	 */
	public void init() throws IOException, ScannerException;
	
	
	/**
	 * Depth of the lookahead can be changed after creation.
	 * <br>
	 * If the new depth is less that the current depth,
	 * no new tokens will be read until the outstanding tokens (current depth - new depth)
	 * are first consumed.
	 * <br>
	 * If the new depth is greater than the current depth,
	 * the lookahead will read new tokens to fill the delta.
	 * <br>
	 * @param depth to change
	 * @throws ScannerException as increasing the current depth of the buffer requires to fill the buffer to the new length
	 */
	public void setLookAheadDepth(int depth) throws IOException, ScannerException;

}
