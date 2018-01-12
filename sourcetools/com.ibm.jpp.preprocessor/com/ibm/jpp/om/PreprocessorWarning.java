/*******************************************************************************
 * Copyright (c) 1999, 2017 IBM Corp. and others
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
package com.ibm.jpp.om;

public class PreprocessorWarning {

	private final String message;
	private final int line;
	private final int charstart;
	private final int charend;
	private boolean shouldFail = true;

	public PreprocessorWarning(String message, int line, int charstart, int charend) {
		this.message = message;
		this.line = line;
		this.charstart = charstart;
		this.charend = charend;
	}

	public PreprocessorWarning(String message, int line, int charstart, int charend, boolean shouldFail) {
		this(message, line, charstart, charend);
		this.shouldFail = shouldFail;
	}

	/**
	 * Gets the charend.
	 * 
	 * @return Returns a int
	 */
	public int getCharend() {
		return charend;
	}

	/**
	 * Gets the charstart.
	 * 
	 * @return Returns a int
	 */
	public int getCharstart() {
		return charstart;
	}

	/**
	 * Gets the message.
	 * 
	 * @return Returns a String
	 */
	public String getMessage() {
		return message;
	}

	/**
	 * Gets the line.
	 * 
	 * @return Returns a int
	 */
	public int getLine() {
		return line;
	}

	public boolean shouldFail() {
		return this.shouldFail;
	}

}
