/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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

import java.io.IOException;
import java.io.OutputStream;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;

/**
 * This is a kind of OutputStream which caches the incoming bytes (instead if printing them out) 
 * and releases them as a string whenever it is asked to.
 * <p>
 * @author Manqing Li, IBM.
 *
 */
public class StringReceiver extends OutputStream {
	public StringReceiver(String charsetName) {
		_buffer = new ArrayList<Byte>();
		_charsetName = charsetName;
	}
	public void write(int b) throws IOException 
	{
		_buffer.add(Integer.valueOf(b).byteValue());		
	}

	public void close() throws IOException {
		_buffer = new ArrayList<Byte>();
		super.close();
	}
	
	public String release() throws UnsupportedEncodingException {
		byte [] byteArray = new byte[_buffer.size()];
		for (int i = 0; i < byteArray.length; i++) {
			byteArray[i] = _buffer.get(i);
		}
		_buffer = new ArrayList<Byte>();
		if(_charsetName == null) {
			return new String(byteArray);
		}
		return new String(byteArray, _charsetName);
	}
	
	private ArrayList<Byte> _buffer = null;
	private String _charsetName = null;
}
