/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.OutputStream;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;

/**
 * Jdmpview specific print stream which allows access to the underlying
 * Output class which controls an arbitrary number of write destinations
 * 
 * @author adam
 *
 */
public class JdmpviewPrintStream extends PrintStream {
	private OutputStream os = null;
	
	public JdmpviewPrintStream(OutputStream out) {
		super(out);
		os = out;
	}

	public JdmpviewPrintStream(String fileName) throws FileNotFoundException {
		super(fileName);
	}

	public JdmpviewPrintStream(File file) throws FileNotFoundException {
		super(file);
	}

	public JdmpviewPrintStream(OutputStream out, boolean autoFlush) {
		super(out, autoFlush);
		os = out;
	}

	public JdmpviewPrintStream(String fileName, String csn)	throws FileNotFoundException, UnsupportedEncodingException {
		super(fileName, csn);
	}

	public JdmpviewPrintStream(File file, String csn) throws FileNotFoundException, UnsupportedEncodingException {
		super(file, csn);
	}

	public JdmpviewPrintStream(OutputStream out, boolean autoFlush,	String encoding) throws UnsupportedEncodingException {
		super(out, autoFlush, encoding);
		os = out;
	}
	
	/**
	 * Returns the output stream that was used in the constructor.
	 * 
	 * @return the stream or null if one wasn't used by the constructor
	 */
	public OutputStream getOutputStream() {
		return os;
	}

}
