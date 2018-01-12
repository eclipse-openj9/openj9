/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2010, 2017 IBM Corp. and others
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
package com.ibm.dtfj.phd.util;

import java.util.NoSuchElementException;

/**
 * Improved performance NumberStream for small number of references
 * Keeps the refs in a LongArray - only outputs to the compressed bitstream if it overflows
 */
public class BufferedNumberStream extends NumberStream {
	
	static final int BUFFER_SIZE = LongArray.CHUNK_SIZE;
	LongArray buffer;
	int bufferCount;
	int size;
	int readBufferCount;
	
	public BufferedNumberStream() {
		this(BUFFER_SIZE);
	}
	
	public BufferedNumberStream(int size) {
		buffer = new LongArray(size);
		this.size = size;
	}
	
	public void writeLong(long n) {
		buffer.put(bufferCount++, n);
		if (bufferCount == BUFFER_SIZE) {
			flushBuffer();
		}
		return;
	}

	void flushBuffer() {
		for (int i = 0; i < bufferCount; ++i) {
			long l = buffer.get(i);
			super.writeLong(l);
		}
		bufferCount = 0;
	}

	public long readLong() {
		if (super.hasMore()) {
			return super.readLong();
		} else {
			if (readBufferCount > 0) {
				int j = bufferCount - readBufferCount--;
				return buffer.get(j);
			} else
				throw new NoSuchElementException();
		}
	}
	
	public boolean hasMore() {
		return super.hasMore() || readBufferCount > 0;
	}
	
	public int elementCount() {
		return super.elementCount() + bufferCount;
	}
	
	public void rewind() {
		//flushBuffer();
		super.rewind();
		readBufferCount = bufferCount;
	}
	
	public void clear() {
		super.clear();
		bufferCount = 0;
	}
}
