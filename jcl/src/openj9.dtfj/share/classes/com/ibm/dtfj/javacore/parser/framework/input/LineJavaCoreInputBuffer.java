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
package com.ibm.dtfj.javacore.parser.framework.input;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.Reader;


/**
 * Input buffer for javacore scanner. Consumes a subsection of characters from the buffer
 *
 */
public class LineJavaCoreInputBuffer extends OffsetBasedJavaCoreInputBuffer {
	
	private BufferedReader fReader;

	public LineJavaCoreInputBuffer(Reader reader) throws IOException {
		super();
		init(reader);
	}
	
	/**
	 * Initialize fields and read the first line.
	 * @param reader
	 * @param lineDelimiter
	 */
	private void init(Reader reader) throws IOException {
		fReader = new BufferedReader(reader);
	}

	/**
	 * @see com.ibm.dtfj.javacore.parser.framework.input.IInputBuffer#close()
	 */
	public void close() throws IOException {
		fReader.close();
	}

	
	/**
	 * 
	 * Consumes elements from the current line.
	 * @see com.ibm.dtfj.javacore.parser.framework.input.IInputBuffer#consume(int)
	 * 
	 * 
	 * @param amount of characters to consume.
	 * @return return the offset value of the consumed portion. Note that this does not return the new offset after consumption
	 * but the offset of the section being consumed.
	 * @throws IOException if error occurs while refilling the buffer.
	 * @throws IndexOutOfBoundsException if length is outside buffer range
	 */
	public int consume(int length) throws IOException{
		if (length < 0 || length > fBuffer.length()) {
			throw new IndexOutOfBoundsException("Incorrect length:"+length+" to consume from: \""+fBuffer+"\"");
		}
		int offset = fOffset;
		fBuffer.delete(0, length);
		updateOffset(length);
		return offset;
	}
	
	
	
	/**
	 * 
	 */
	public boolean nextLine() throws IOException{
		boolean readNext = false;
		if (fBuffer.length() == 0 && !isStreamEnd()) {
			String line = fReader.readLine();
			if (line != null) {
				fBuffer.append(line);
				readNext = true;
			}
			else {
				markStreamEnd(true);
				fReader.close();
			}
		}
		return readNext;
	}
	
	


	



}
