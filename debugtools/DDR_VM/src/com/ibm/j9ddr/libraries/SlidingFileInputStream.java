/*******************************************************************************
 * Copyright (c) 1991, 2019 IBM Corp. and others
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

//sliding input stream which presents a portion of a file as the complete stream

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;

/**
 * @author GB0048506
 *
 */
public class SlidingFileInputStream extends InputStream {
	private final long length;		//length of the stream
	private final ImageInputStream stream;
	private final byte[] buffer = new byte[4096];
	private int bytesAvailable = 0;
	private boolean EOF = false;
	private int bufferPos = 0;
	private long bytesRead = 0;

	public SlidingFileInputStream(File file, long start, long length) throws FileNotFoundException, IOException {
		this.length = length;
		stream = new FileImageInputStream(file);
		stream.seek(start);
	}
	
	public SlidingFileInputStream(ImageInputStream iis, long start, long length) throws FileNotFoundException, IOException {
		this.length = length;
		stream = iis;
		stream.seek(start);
	}
	
	@Override
	public int read() throws IOException {
		if(EOF) return -1;			//end of file
		if(bytesRead == length) {
			EOF = true;
			return -1;
		}
		if(bytesAvailable == bufferPos) {
			bytesAvailable = stream.read(buffer);
			if(bytesAvailable == -1) {
				EOF = true;
				return -1;
			}
			bufferPos = 0;
		}
		bytesRead++;
		return 0xFF & buffer[bufferPos++];			//return and increment buffer pointer
	}
	
	
	/**
	 * Actually closes the underlying stream. The close() method does not close the stream so as to allow its reuse.
	 * @throws IOException
	 */
	public void disposeStream() throws IOException {
		stream.close();		
	}
}
