/*******************************************************************************
 * Copyright (c) 2004, 2014 IBM Corp. and others
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

package com.ibm.j9ddr.corereaders;


import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Collection;

import javax.imageio.stream.ImageInputStream;

import com.ibm.j9ddr.corereaders.memory.IMemorySource;

/**
 * @author ccristal
 */
public abstract class AbstractCoreReader implements ICore
{

	protected ImageInputStream _fileReader;
	protected Collection<? extends IMemorySource> _memoryRanges = new ArrayList<IMemorySource>();

	private ShutdownHook _fileTracker = new ShutdownHook();
	protected File coreFile;

	@Override
	public boolean equals(Object obj) {
		if((obj == null) || !(obj instanceof AbstractCoreReader)) {
			return false;
		}
		AbstractCoreReader reader = (AbstractCoreReader) obj;
		if(coreFile == null) {
			//this reader is working from a stream
			if(reader.coreFile != null) {
				return false;
			}
			return _fileReader.equals(reader._fileReader);
		} else {
			//this reader is working from a file
			if(reader.coreFile == null) {
				return false;
			}
			return coreFile.equals(reader.coreFile);
		}
	}

	@Override
	public int hashCode() {
		if(coreFile == null) {
			return super.hashCode();
		}
		return coreFile.hashCode();
	}
	
	/**
	 * This sets the reader to use to retrieve the underlying bytes to process.
	 * The reader may perform buffering or other operations before making the
	 * data available.
	 * 
	 * Subclasses should override this method if additional actions need to be
	 * performed when the reader is set, typically this will be things such as
	 * an initial read of the data.
	 * 
	 * @param reader
	 */
	public void setReader(ImageInputStream reader) throws IOException
	{
		_fileReader = reader;
		_fileTracker.addFile(reader);
	}

	public long readLong() throws IOException
	{
		return _fileReader.readLong();
	}

	public int readInt() throws IOException
	{
		return _fileReader.readInt();
	}

	public short readShort() throws IOException
	{
		return _fileReader.readShort();
	}

	public byte readByte() throws IOException
	{
		return _fileReader.readByte();
	}

	public void seek(long pos) throws IOException
	{
		_fileReader.seek(pos);
	}

	// this method has been ported from the ClosingFileReader so that this class
	// can accept a
	// generic ImageInputStream
	public byte[] readBytes(int len) throws IOException
	{
		byte[] buffer = new byte[len];
		_fileReader.readFully(buffer);
		return buffer;
	}

	public void readFully(byte[] b) throws IOException
	{
		_fileReader.readFully(b);
	}

	public void readFully(byte[] buffer, int offset, int length)
			throws IOException
	{
		_fileReader.readFully(buffer, offset, length);
	}

	protected boolean checkOffset(long location) throws IOException
	{
		boolean canRead;
		long currentPos = _fileReader.getStreamPosition();
		try {
			_fileReader.seek(location);
			_fileReader.readByte();
			canRead = true;
		} catch (IOException ioe) {
			canRead = false;
		}
		_fileReader.seek(currentPos);
		return canRead;
	}

	protected String readString() throws IOException
	{
		StringBuffer buffer = new StringBuffer();
		byte b = readByte();
		while (0 != b) {
			buffer.append(new String(new byte[] { b }, "ASCII"));
			b = readByte();
		}
		return buffer.toString();
	}

	public static String format(int i)
	{
		return "0x" + Integer.toHexString(i);
	}

	public static String format(long l)
	{
		return "0x" + Long.toHexString(l);
	}

	protected static long readLong(byte[] data, int start)
	{
		return (0xFF00000000000000L & (((long) data[start + 0]) << 56))
				| (0x00FF000000000000L & (((long) data[start + 1]) << 48))
				| (0x0000FF0000000000L & (((long) data[start + 2]) << 40))
				| (0x000000FF00000000L & (((long) data[start + 3]) << 32))
				| (0x00000000FF000000L & (((long) data[start + 4]) << 24))
				| (0x0000000000FF0000L & (((long) data[start + 5]) << 16))
				| (0x000000000000FF00L & (((long) data[start + 6]) << 8))
				| (0x00000000000000FFL & (data[start + 7]));
	}

	protected static int readInt(byte[] data, int start)
	{
		return (0xFF000000 & ((data[start + 0]) << 24))
				| (0x00FF0000 & ((data[start + 1]) << 16))
				| (0x0000FF00 & ((data[start + 2]) << 8))
				| (0x000000FF & (data[start + 3]));
	}
	
	public void close() throws IOException {
		if(_fileReader != null) {
			_fileReader.close();
		}
	}
}
