/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2020 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.tools.utils;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;

/**
 * This is a kind of OutputStream which caches the incoming bytes (instead if printing them out) 
 * and releases them as a string whenever it is asked to.
 * <p>
 * @author Manqing Li, IBM.
 */
public class StringReceiver extends OutputStream {

	private ByteArrayOutputStream _buffer;
	private final String _charsetName;

	public StringReceiver(String charsetName) {
		_buffer = new ByteArrayOutputStream();
		_charsetName = charsetName;
	}

	@Override
	public void write(int b) throws IOException {
		_buffer.write(b);
	}

	@Override
	public void close() throws IOException {
		_buffer = new ByteArrayOutputStream();
		super.close();
	}

	public String release() throws UnsupportedEncodingException {
		String content = (_charsetName == null) ? _buffer.toString() : _buffer.toString(_charsetName);
		_buffer = new ByteArrayOutputStream();
		return content;
	}

}
