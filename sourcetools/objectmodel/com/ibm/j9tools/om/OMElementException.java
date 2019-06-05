/*******************************************************************************
 * Copyright (c) 2007, 2019 IBM Corp. and others
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
package com.ibm.j9tools.om;

/**
 * Object Model exception used to describe errors in an element of a {@link OMObject}.
 * 
 * @author	Gabriel Castro
 * @since	v1.5.0
 */
public abstract class OMElementException extends OMException {
	private static final long serialVersionUID = 1L;	/** Identifier for serialized instances. */

	protected int lineNumber = -1;
	protected int column = -1;
	protected String fileName;
	
	/**
	 * Creates an {@link OMElementException} for the given object with its associated
	 * error message.
	 * 
	 * @param 	message		the error message
	 * @param 	object		the source of the error
	 */
	public OMElementException(String message, OMObject object) {
		super(message);
		
		if (object != null && object.getLocation() != null) {
			this.lineNumber = object.getLocation().getLine();
			this.fileName = object.getLocation().getFileName();
			this.column = object.getLocation().getColumn();
		}
		
		this.object = object;
	}

	/**
	 * Overwrites the error line number for this exception.
	 * 
	 * @param 	lineNumber	the line number in which the error occured
	 */
	public void setLineNumber(int lineNumber) {
		this.lineNumber = lineNumber;
	}

	/**
	 * Retrieves the line number for this exception.
	 * 
	 * @return	the line number
	 */
	public int getLineNumber() {
		return lineNumber;
	}

	/**
	 * Overwrites the error column number for this exception.
	 * 
	 * @param 	column		the column in which the error occured
	 */
	public void setColumn(int column) {
		this.column = column;
	}
	
	/**
	 * Retrieves the column number for this exception.
	 * 
	 * @return	the column number
	 */
	public int getColumn() {
		return column;
	}

	/**
	 * Overwrites the file name associated with this exception's object.
	 * 
	 * @param 	fileName	the new file name
	 */
	public void setFileName(String fileName) {
		this.fileName = fileName;
	}

	/**
	 * Retrieves the file name associated with this exception's object.
	 * 
	 * @return	the name of the object's file
	 */
	public String getFileName() {
		return fileName;
	}
}
