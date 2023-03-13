/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright IBM Corp. and others 2012
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.jvm.dtfjview.tools.utils;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.OutputStream;

/**
 * This class works with the class OutputStream and class IStringModifier.
 * It first caches the bytes from the OutputStream.  If a new line char is
 * encountered or if the output stream is closed, it will ask the string 
 * modifier to modify it first before it sends the line to the output stream.
 * <p>
 * @author Manqing Li
 */
public class OutputStreamModifier extends OutputStream {

	private final OutputStream _out;
	private final IStringModifier _modifier;
	private ByteArrayOutputStream _buffer;

	public OutputStreamModifier(OutputStream out, IStringModifier modifier) {
		super();
		_out = out;
		_modifier = modifier;
		_buffer = new ByteArrayOutputStream();
	}

	@Override
	public void write(int b) throws IOException {
		_buffer.write(b);
		if ('\n' == b || 0x15 == b) { // EBCDIC systems use NL (New Line 0x15).
			writeBuffer();
		}
	}

	@Override
	public void close() throws IOException {
		writeBuffer();
		super.close();
	}

	private void writeBuffer() throws IOException {
		if (0 != _buffer.size()) {
			String content = _buffer.toString();
			_buffer = new ByteArrayOutputStream();
			_out.write(_modifier.modify(content).getBytes());
		}
	}

}
