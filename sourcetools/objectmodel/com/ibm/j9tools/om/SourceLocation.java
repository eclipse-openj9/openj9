/*******************************************************************************
 * Copyright (c) 2007, 2011 IBM Corp. and others
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

import java.io.File;

import org.xml.sax.Locator;

/**
 * SourceLocation class provides data origin information. It allows the parsers to
 * specify where a given bit of data was parsed from. The users are able to query 
 * the originating file name as well as line and column numbers. 
 * 
 * @author mac
 */
public class SourceLocation {
	private final File file;
	private final int line;
	private final int column;

	public SourceLocation(File file, int line, int column) {
		this.file = file;
		this.line = line;
		this.column = column;
	}

	public SourceLocation(File file, Locator locator) {
		this(file, locator.getLineNumber(), locator.getColumnNumber());
	}

	public String getFileName() {
		if (null == file) {
			/* We might be parsing a Stream that is not sourced from a file */
			return Messages.getString("SourceLocation.0"); //$NON-NLS-1$
		}

		return file.getAbsolutePath();
	}

	public int getLine() {
		return line;
	}

	public int getColumn() {
		return column;
	}

}
