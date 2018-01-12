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
package com.ibm.dtfj.javacore.parser.framework.input;

import java.io.IOException;

public interface IInputBuffer {

	/**
	 * 
	 * Consumes a sublength of the buffer, if the sublength is > 0 and sublength <= buffer length.
	 * It then reads from the input source to refill the previously freed spaces. 
	 * 
	 * 
	 * @param amount of characters to consume.
	 * @return return the offset value of the consumed portion. Note that this does not return the new offset after consumption
	 * but the offset of the section being consumed.
	 * @throws IOException if error occurs while refilling the buffer.
	 */
	public int consume(int length) throws IOException;
	
	
	/**
	 * Closes the java.io.Reader or input stream associated with this
	 * buffered reader. 
	 * 
	 * @throws IOException if error encountered while closing.
	 */
	public void close()	throws IOException;
	
	
	/**
	 * Retrieves a character at a given position. No consumption occurs.
	 * Typically used by an external scanner to determine if a sequence of characters in the buffer
	 * matches a desired pattern. If so, the pattern is consumed by a consume method.
	 * 
	 * @param buffer index where character is located
	 * @return character at specified index
	 */
	public char charAt(int i);
	
	
	/**
	 *
	 * 
	 * @return boolean: determines if everything has been consumed from the reader stream.
	 */
	public boolean endReached();
	
	
	/**
	 * Reads the next line if the current buffer is empty. Returns true if a new line read, false otherwise.
	 * 
	 */
	public boolean nextLine() throws IOException;

	
	

	
	/**
	 *	NOTE That the length of the buffer may vary during character consumption, so it cannot be assumed
	 *  to be fixed all the time. 
	 *  
	 * @return the length of the buffer
	 */
	public int length();
	
	
	
	
	

	

	
	
}
