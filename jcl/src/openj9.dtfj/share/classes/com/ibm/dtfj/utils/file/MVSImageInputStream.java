/*[INCLUDE-IF PLATFORM-mz31 | PLATFORM-mz64 | ! ( Sidecar18-SE-OpenJ9 | Sidecar19-SE )]*/
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
package com.ibm.dtfj.utils.file;

import java.io.FileNotFoundException;
import java.io.IOException;

import javax.imageio.stream.ImageInputStreamImpl;

import com.ibm.jzos.ZFile;

/**
 * A wrapper class which converts z/OS record I/O to random access image input
 * stream I/O
 * 
 * @author ajohnson
 */
public class MVSImageInputStream extends ImageInputStreamImpl {
	/** The record file identifier */
	private ZFile destFile;
	/** The block size */
	private final int recordLength;
	private final long fileLength;

	/**
	 * Open the file
	 * 
	 * @param filename
	 * @throws FileNotFoundException
	 */
	public MVSImageInputStream(String filename) throws FileNotFoundException {
		try {
			destFile = new ZFile("//'" + filename + "'", "rb,type=record");
			recordLength = destFile.getLrecl();
			fileLength = destFile.getRecordCount() * recordLength;
		} catch (IllegalArgumentException e) {
			// IAE is thrown by JRIO when a DSN name segment is invalid eg. too long or too many dots
			FileNotFoundException e1 = new FileNotFoundException("Could not find: " + filename);
			e1.initCause(e);
			throw e1;
		} catch (IOException e) {
			FileNotFoundException e1 = new FileNotFoundException("Could not find: " + filename);
			e1.initCause(e);
			throw e1;
		}
	}

	/**
	 * Read a set of bytes The record i/o needs to be at the record containing
	 * the data. It needs to skip over the bytes from the beginning of the
	 * record to the file position as known by the caller.
	 */
	public int read(byte buf[], int off, int len) throws IOException {

		long blockNumber = streamPos / recordLength;
		destFile.seek(blockNumber, 0);

		int recordOffset = (int) (streamPos % recordLength); // offset within the record
		int n = 0;
		
		if (recordOffset != 0) {
			// We can only read one record's worth, so limit the length of the temp buffer to that
			if (recordOffset + len > recordLength) {
				len = recordLength - recordOffset;
			}
			byte buf2[] = new byte[recordOffset + len];
			n = destFile.read(buf2) - recordOffset;
			if (n > 0) {
				System.arraycopy(buf2, recordOffset, buf, off, n);
			}
		} else {
			// Optimization for 0 offset. Read directly into the destination.
			n = destFile.read(buf, off, len);
		}

		if (n < 0 || n == 0 && len > 0) {
			seek(streamPos);
			return -1;
		} else {
			seek(streamPos + n);
			return n;
		}
	}

	/**
	 * Simple read one byte method - needed as ImageInputStreamImpl doesn't do it
	 */
	public int read() throws IOException {
		byte b[] = new byte[1];
		int n = read(b);
		return n < 0 ? -1 : b[1] & 0xff;
	}

	public void close() throws IOException {
		if (destFile != null) {
			destFile.close();
		}
		super.close();
	}

	public long size() {
		return fileLength;
	}
}
