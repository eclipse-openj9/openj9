/*******************************************************************************
 * Copyright (c) 1999, 2019 IBM Corp. and others
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

import java.io.IOException;
import java.io.InputStream;

/**
 * Attempts to seamlessly merge two input streams so that they behave as one.
 */
public class InputStreamMerger extends InputStream {
	private final InputStream prepend;
	private final InputStream append;

	private boolean prependFinished = false;

	/**
	 * @param 		prepend
	 * @param 		append
	 * 
	 * @throws		IOException
	 */
	public InputStreamMerger(InputStream prepend, InputStream append) throws IOException {
		this.prepend = prepend;
		this.append = append;

		if (prepend == null || append == null) {
			throw new NullPointerException();
		}
		if (prepend.available() < 1) {
			prependFinished = true;
		}
	}

	/**
	 * @see java.io.InputStream#read()
	 */
	public int read() throws IOException {
		if (!prependFinished) {
			// do not call read() to determine EOF as this
			// calls the OS and is therefore slow, especially
			// on network drives
			int i = prepend.read();
			if (i != -1) {
				return i;
			} else {
				prependFinished = true;
			}
		}
		return append.read();
	}

	/**
	 * @see java.io.InputStream#available()
	 */
	public int available() throws IOException {
		return (prependFinished) ? append.available() : prepend.available();
	}

	/**
	 * @see java.io.InputStream#close()
	 */
	public void close() throws IOException {
		prepend.close();
		append.close();
	}

	/**
	 * @see java.io.InputStream#mark(int)
	 */
	public synchronized void mark(int readlimit) {
		super.mark(readlimit); //does nothing.
	}

	/**
	 * @see java.io.InputStream#markSupported()
	 */
	public boolean markSupported() {
		return false;
	}

	/**
	 * @see java.io.InputStream#read(byte[], int, int)
	 */
	public int read(byte[] b, int offset, int length) throws IOException {
		if (b == null) {
			throw new NullPointerException();
		}
		if (offset < 0 || length < 0 || offset + length > b.length) {
			throw new IndexOutOfBoundsException();
		}

		int bytesRead = 0;
		if (!prependFinished) {
			bytesRead = prepend.read(b, offset, length);
			if (bytesRead == -1) {
				prependFinished = true;
				bytesRead = 0;
			}
		}

		if (bytesRead < length) {
			bytesRead += append.read(b, offset + bytesRead, length - bytesRead);
		}

		return bytesRead;
	}

	/**
	 * @see java.io.InputStream#read(byte[])
	 */
	public int read(byte[] b) throws IOException {
		return read(b, 0, b.length);
	}

	/**
	 * @see java.io.InputStream#reset()
	 */
	public synchronized void reset() throws IOException {
		if (!markSupported()) {
			throw new IOException();
		}
	}

	/**
	 * @see java.io.InputStream#skip(long)
	 */
	public long skip(long n) throws IOException {
		return super.skip(n);
	}

}
