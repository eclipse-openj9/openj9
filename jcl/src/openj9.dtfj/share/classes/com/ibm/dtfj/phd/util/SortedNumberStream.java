/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2002, 2017 IBM Corp. and others
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

/**
 * Packs references more efficiently by sorting them in order, so allowing smaller 
 * deltas to be encoded.
 */
public class SortedNumberStream extends NumberStream {

	NumberStream streamBuffer;
	static final int BUFFER_SIZE = 200000;
	LongArray buffer;
	int bufferCount;

	public SortedNumberStream() {
		buffer = new LongArray(BUFFER_SIZE, 0);
		streamBuffer = new NumberStream();
	}

	void flushBuffer() {
		//System.out.println("flush");
		if (bufferCount == 0)
			return;
		buffer.setSize(bufferCount);
		buffer.sort();
		buffer.setSize(BUFFER_SIZE);
		reset();
		readElementCount = elementCount;
		streamBuffer.reset();
		streamBuffer.elementCount = 0;
		streamBuffer.readElementCount = 0;
		streamBuffer.bitStream.clear();
		int n = 0;
		while (n < bufferCount || hasMore()) {
			if (hasMore()) {
				long l = readLong();
				while (n < bufferCount && buffer.get(n) < l) {
					//System.out.println("insert from buffer " + Long.toHexString(buffer.get(n)));
					streamBuffer.writeLong(buffer.get(n++));
				}
				//System.out.println("add " + Long.toHexString(l));
				streamBuffer.writeLong(l);
			} else {
				//System.out.println("add from buffer " + Long.toHexString(buffer.get(n)));
				streamBuffer.writeLong(buffer.get(n++));
			}
		}
		streamBuffer.flush();
		BitStream tmp = bitStream;
		setBitStream(streamBuffer.bitStream);
		streamBuffer.setBitStream(tmp);
		elementCount = streamBuffer.elementCount;
		readElementCount = streamBuffer.readElementCount;
		bufferCount = 0;
	}

	public void writeLong(long n) {
		buffer.put(bufferCount++, n);
		if (bufferCount == BUFFER_SIZE) {
			flushBuffer();
		}
	}
	
	public void rewind() {
		flushBuffer();
		super.rewind();
	}
}
