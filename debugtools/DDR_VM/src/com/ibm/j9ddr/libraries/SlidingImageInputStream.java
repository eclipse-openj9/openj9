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

//sliding image input stream to present a portion of a file as a complete file in itself

import java.io.File;
import java.io.IOException;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;
import javax.imageio.stream.ImageInputStreamImpl;

public class SlidingImageInputStream extends ImageInputStreamImpl {
	private final ImageInputStream stream;
	private long length;
	private long start;
	private final static int BUFFER_SIZE = 4096;
	private final byte[] buffer = new byte[BUFFER_SIZE];
	private int bytesAvailable = 0;
	private boolean EOF = false;
	private int bufferPos = 0;
	private long bytesRead = 0;
	private boolean hasBufferRefreshed = false;
	private long markBytesRead = 0;
	private long markFilePos = 0;
	private long bufferStartPos = 0;
	
	public SlidingImageInputStream(File raf, long start, long length) throws IOException {
		stream = new FileImageInputStream(raf);
		init(start, length);
	}
	
	public SlidingImageInputStream(ImageInputStream iis, long start, long length) throws IOException {
		stream = iis;
		init(start, length);
	}
	
	private void init(long start, long length) throws IOException {
		stream.seek(start);
		checkBuffer();
		this.length = length;
		this.start = start;
		markFilePos = stream.getStreamPosition();
	}
	
	@Override
	public int read() throws IOException {
		if(EOF) return -1;			//end of file
		if(bytesRead == length) {
			EOF = true;
			return -1;
		}
		checkBuffer();
		bytesRead++;
		return 0xFF & buffer[bufferPos++];			//return and increment buffer pointer
	}

	@Override
	public int read(byte[] b, int off, int len) throws IOException {
		if(EOF) return -1;												//end of file already set
		if(bytesRead == length) {										//sliding window over the file is exhausted so treat as EOF
			EOF = true;
			return -1;
		}
		if((len + off) > b.length) {									//check that destination array is big enough
			String msg = String.format("The array is too small to copy %d bytes starting at offset %d", len, off);
			throw new IndexOutOfBoundsException(msg);
		}
		if(len < (bytesAvailable - bufferPos)) {						//this request can be satisfied entirely from the buffer
			int tocopy = (int) Math.min(len, length - bytesRead);		//see which is smaller, amount requested or amount left in window 
			System.arraycopy(buffer, bufferPos, b, off, tocopy);
			bufferPos += tocopy;
			bytesRead += tocopy;
			return tocopy;
		}
		//this request needs to be satisfied from one or more buffer fills
		int copiedBytes = 0;
		while(!EOF && (copiedBytes < len) && (bytesRead < length)){
			int tocopy = (int) Math.min(bytesAvailable - bufferPos, length - bytesRead);	//see which is smaller, amount left in buffer or amount left in window
			tocopy = Math.min(tocopy, len - copiedBytes);									//then see which is smaller the proposed copy amount or the amount left to fulfill the request
			System.arraycopy(buffer, bufferPos, b, copiedBytes, tocopy);
			bufferPos += tocopy;
			bytesRead += tocopy;
			copiedBytes += tocopy;
			checkBuffer();
		}
		return copiedBytes;
	}

	private void checkBuffer() throws IOException {
		if(bytesAvailable == bufferPos) {							//see if need to refill the buffer
			bufferStartPos = bytesRead;								//the start of this buffer is the number of bytes read
			hasBufferRefreshed = true;								//indicate that the buffer has been refreshed
			bytesAvailable = stream.read(buffer);
			if(bytesAvailable == -1) {
				EOF = true;
			}
			bufferPos = 0;
		}
	}

	@Override
	public long length() {
		return length;
	}

	@Override
	public void mark() {
		hasBufferRefreshed = false;
		markBytesRead = bytesRead;
		super.mark();
	}

	@Override
	public void reset() throws IOException {
		super.reset();
		long resetCount = bytesRead - markBytesRead;	//how many bytes to rewind by
		bytesRead -= resetCount;						//update the bytes read count with the rewind
		if(hasBufferRefreshed) {						//cannot reset back within the buffer so we will need to throw away the current contents
			bufferPos = 0;
			bytesAvailable = 0;
			stream.seek(markFilePos - resetCount);		//reset underlying stream position
		} else {
			bufferPos -= resetCount;					//reset within the buffer
		}
	}

	@Override
	public void seek(long pos) throws IOException {
		if((pos < bufferStartPos) || (pos >= (bufferStartPos + BUFFER_SIZE))) {		//move outside of the current buffer
			stream.seek(start + pos);						//move the underlying stream
			bufferPos = 0;									//throw away the current buffer contents
			bytesAvailable = 0;
			bytesRead = pos;
			checkBuffer();						//flag the invalidated buffer
			super.seek(pos);
		} else {												//seek within current buffer
			int diff = (int)(pos - bufferStartPos);				//how far in ?
			bytesRead = bufferStartPos + diff;
			bufferPos = diff;
		}
	}

	@Override
	public long getStreamPosition() throws IOException {
		return bytesRead;
	}
	
	
	
}
