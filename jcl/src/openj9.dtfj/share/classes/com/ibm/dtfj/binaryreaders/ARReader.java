/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.dtfj.binaryreaders;

import java.io.IOException;

import com.ibm.dtfj.corereaders.ClosingFileReader;


/**
 * @author jmdisher
 * Reads the AR files used to store a flat list of shared libraries on AIX
 */
public class ARReader
{
	private ClosingFileReader _backing;
	private long _firstModuleHeader;
	
		
	public ARReader(ClosingFileReader file)
	{
		_backing = file;
		try {
			_backing.seek(0);
			byte[] magic = new byte[8];
			_backing.readFully(magic);
			if (!"<bigaf>\n".equals(new String(magic))) {
				throw new IllegalArgumentException();
			}
			_backing.seek(68);
			byte[] firstOffset = new byte[20];
			_backing.readFully(firstOffset);
			_firstModuleHeader = _longFromBuffer(firstOffset);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}
	
	public long offsetOfModule(String name)
	{
		long next = _firstModuleHeader;
		
		while (0 != next) {
			try {
				_backing.seek(next);
				byte[] buffer = new byte[20];
				_backing.readFully(buffer);
				long length = _longFromBuffer(buffer);
				_backing.readFully(buffer);
				long nextOffset = _longFromBuffer(buffer);
				_backing.readFully(buffer);
				long prevOffset = _longFromBuffer(buffer);
				_backing.seek(next+ 108);
				buffer = new byte[4];
				_backing.readFully(buffer);
				int nameLen = (int)_longFromBuffer(buffer);
				buffer = new byte[nameLen];
				_backing.readFully(buffer);
				String moduleName = null;
				if ((0 < nameLen) && ('`' == buffer[0]) && ('\n' == buffer[1])) {
					moduleName = "";
				} else {
					moduleName = new String(buffer, 0, nameLen);
				}
				if (moduleName.equals(name)) {
					long offset = next + 112 + nameLen;
					if (1 == (offset % 2)) {
						offset += 3;
					} else {
						offset += 2;
					}
					return offset;
				}
				next = nextOffset;
			} catch (IOException e) {
				return -1;
			}
		}
		return -1;
	}
	
	public long sizeOfModule(String name)
	{
		long next = _firstModuleHeader;
		
		while (0 != next) {
			try {
				_backing.seek(next);
				byte[] buffer = new byte[20];
				_backing.readFully(buffer);
				long length = _longFromBuffer(buffer);
				_backing.readFully(buffer);
				long nextOffset = _longFromBuffer(buffer);
				_backing.readFully(buffer);
				long prevOffset = _longFromBuffer(buffer);
				_backing.seek(next+ 108);
				buffer = new byte[4];
				_backing.readFully(buffer);
				int nameLen = (int)_longFromBuffer(buffer);
				buffer = new byte[nameLen];
				_backing.readFully(buffer);
				String moduleName = null;
				if ((0 < nameLen) && ('`' == buffer[0]) && ('\n' == buffer[1])) {
					moduleName = "";
				} else {
					moduleName = new String(buffer, 0, nameLen);
				}
				if (moduleName.equals(name)) {
					return length;
				}
				next = nextOffset;
			} catch (IOException e) {
				return -1;
			}
		}
		return -1;
	}

	
	/**
	 * @param buffer
	 * @return
	 */
	private long _longFromBuffer(byte[] buffer)
	{
		int total = buffer.length;
		while (' ' == buffer[total-1]) {
			total--;
		}
		long length = Long.parseLong(new String(buffer, 0, total));
		return length;
	}
}
