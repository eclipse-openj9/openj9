/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

package com.ibm.j9ddr.libraries;

import java.io.File;
import java.io.FileDescriptor;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

public class MonitoredFileOutputStream extends FileOutputStream {
	private int bytesWritten = 0;

	public MonitoredFileOutputStream(File file, boolean append)	throws FileNotFoundException {
		super(file, append);
	}

	public MonitoredFileOutputStream(File file) throws FileNotFoundException {
		super(file);
	}

	public MonitoredFileOutputStream(FileDescriptor fdObj) {
		super(fdObj);
	}

	public MonitoredFileOutputStream(String name, boolean append) throws FileNotFoundException {
		super(name, append);
	}

	public MonitoredFileOutputStream(String name) throws FileNotFoundException {
		super(name);
	}

	@Override
	public void write(byte[] b, int off, int len) throws IOException {
		bytesWritten += len;
		super.write(b, off, len);
	}

	@Override
	public void write(byte[] b) throws IOException {
		bytesWritten += b.length;
		super.write(b);
	}

	@Override
	public void write(int b) throws IOException {
		bytesWritten++;
		super.write(b);
	}

	public int getBytesWritten() {
		return bytesWritten;
	}
	
	public byte[] getBytesWrittenAsArray() {
		byte[] data = new byte[4];
		for(int i = 0; i < data.length; i++) {
			int shift = (3 - i) * 8;
			data[i] = (byte)((bytesWritten & (0xFF << shift)) >> shift);
		}
		return data;
	}
	
}
