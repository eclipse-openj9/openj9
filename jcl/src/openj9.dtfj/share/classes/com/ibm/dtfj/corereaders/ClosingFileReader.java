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
package com.ibm.dtfj.corereaders;

import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.net.URI;

import javax.imageio.stream.ImageInputStream;
import javax.imageio.stream.ImageInputStreamImpl;

/**
 * A wrapper around the functionality that we require from RandomAccessFiles but with the added auto-closing
 * functionality that we require.  Since it is not always easy to determine who "owns" a file and we do not want
 * to introduce the concept of state, this class will manage all such file ownership.
 * 
 * We now also extend javax.imageio.stream.ImageInputStreamImpl as a convenient common interface that we can
 * share with the zebedee corefile reader
 * 
 * @author jmdisher
 */
public class ClosingFileReader extends ImageInputStreamImpl implements ResourceReleaser
{
	private static final int PAGE_SIZE = 4096;
	private static final int BUFFER_PAGES = 4;
	private static final int BUFFER_SIZE = BUFFER_PAGES * PAGE_SIZE;
	private IRandomAccessFile _file;
	//used for creating the stream
	private File _fileRef;
	private byte[] _buffer = new byte[BUFFER_SIZE];
	private long _bufferBase = -1;
	private long _bufferEnd = -1;
	private boolean deleteOnClose = false;
	private int _page_size = PAGE_SIZE;
	
	/**
	 * Interface that mimics java.io.RandomAccessFile, so we can have basic plus
	 * an alternative implementation for z/OS native record-based files
	 */
	private static interface IRandomAccessFile 
	{
		public void close() throws IOException;
		public long length() throws IOException;
		public int read() throws IOException;
		public void readFully(byte[] b) throws IOException;
		public void readFully(byte[] b, int off, int len) throws IOException;
		public void seek(long pos) throws IOException;
	}
	
	/**
	 * com.ibm.recordio does not provide file length, so we use an EOF exception
	 */
	private static class EORFException extends EOFException 
	{
		private static final long serialVersionUID = 1L;
		public int bytesRead;
		
		public EORFException(int n) {
			bytesRead = n;
		}
	}
	
	/**
	 * Base implementation of IRandomAccessFile - delegates to java.io.RandomAccessFile
	 */
	private static class BaseRandomAccessFile implements IRandomAccessFile
	{
		private RandomAccessFile _file;
		
		public BaseRandomAccessFile(File file, String mode) throws FileNotFoundException {
			_file = new RandomAccessFile( file, mode);
		}
		public void close() throws IOException {
			_file.close();
		}
		public long length() throws IOException {
			return _file.length();
		}
		public int read() throws IOException {
			return _file.read();
		}
		public void readFully(byte[] b) throws IOException {
			_file.readFully(b);
		}
		public void readFully(byte[] b, int off, int len) throws IOException {
			_file.readFully(b, off, len);
		}
		public void seek(long pos) throws IOException {
			_file.seek(pos);
		}
	}

	/**
	 * Stream implementation of IRandomAccessFile - delegates to ImageInputStream
	 */
	private static class StreamRandomAccessFile implements IRandomAccessFile
	{
		private ImageInputStream _file;
		public StreamRandomAccessFile(ImageInputStream file) {
			_file = file;
		}		
		public void close() throws IOException
		{
			_file.close();
		}
		public long length() throws IOException
		{
			// ImageInputStream does not always provide file length, so we handle using EOF
			return _file.length();
		}
		public int read() throws IOException
		{
			return _file.read();
		}
		public void readFully(byte[] b) throws IOException
		{
			readFully(b, 0, b.length);
		}
		public void readFully(byte[] b, int off, int len) throws IOException
		{
			int sofar = 0;
					
			do {
				int nbytes = _file.read(b, off + sofar, len - sofar);
				//if (nbytes <= 0) throw new EORFException(sofar);
				if (nbytes <= 0) throw new EOFException();
				sofar += nbytes;
			} while (sofar < len);
			
		}
		public void seek(long pos) throws IOException
		{
			_file.seek(pos);
		}
	}

	/*[IF PLATFORM-mz31 | PLATFORM-mz64 | ! ( Sidecar18-SE-OpenJ9 | Sidecar19-SE )]*/
	/**
	 * z/OS native implementation of IRandomAccessFile - wrappers com.ibm.recordio.RandomAccessRecordFile
	 */
	private static class ZosRandomAccessFile implements IRandomAccessFile
	{
		private com.ibm.jzos.ZFile _zFile;
		private byte[] _recordBuffer = null;
		private int _recordLength;
		private long _seekOffset = 0;
		private long _recordNumber = 0;
		private long _bufferValid = 0;
		
		public ZosRandomAccessFile(File file, String mode) throws IOException {
			try {
				// create a com.ibm.recordio.RecordFile first
				_zFile = new com.ibm.jzos.ZFile("//'" + file.getName() + "'", "rb,type=record");
				// then use it to create a com.ibm.recordio.RandomAccessRecordFile
				_recordLength = _zFile.getLrecl();
				_recordBuffer = new byte[_recordLength];
			} catch (IOException e) {
				throw new FileNotFoundException(e.toString());			
			} catch (LinkageError e) {
				throw new FileNotFoundException(e.toString());
			}
		}
		
		public void close() throws IOException {
			_zFile.close();
		}
		
		public long length() throws IOException {
			// com.ibm.recordio does not provide file length, so we handle using EOF
			return -1;
		}
		
		public int read() throws IOException {
			byte b[] = new byte[1];
			try {
				readFully(b);
				return b[0] & 0xff;
			} catch (EOFException e) {
				return -1;
			}
		}
		
		public void readFully(byte[] b) throws IOException {
			readFully(b, 0, b.length);
		}
		
		public void readFully(byte[] b, int off, int len) throws IOException {
			//System.out.println("Read seekOff="+_seekOffset+" off="+off+" len="+len);
			int sofar = 0;

			while (sofar < len) {
				if (_seekOffset != 0 || len - sofar < _recordLength) {
					// copy bytes from partial record
					// If there is data in the buffer then use that, otherwise read a new record
					long nbytes;
					
					nbytes = _bufferValid > 0 ? _bufferValid : _zFile.read(_recordBuffer);
					// Have we been unable to read up to the required start point?
					if (nbytes < _seekOffset) throw new EORFException(sofar);
					// How much useful new data have we got
					long tocopy = nbytes - _seekOffset;
					if (tocopy > len - sofar) {
						// More new data available than required, so just copy what is needed
						System.arraycopy(_recordBuffer, (int)_seekOffset, b, off + sofar, len - sofar);
						sofar = len;
						// Remember where we got to and the spare data in the buffer
						_seekOffset += sofar;
						_bufferValid = nbytes;
					} else {
						// Less or equal new data than required, so copy it all
						System.arraycopy(_recordBuffer, (int)_seekOffset, b, off + sofar, (int)tocopy);
						sofar += tocopy;
						// Next read will be from the start of a new record
						++_recordNumber;
						_seekOffset = 0;
						_bufferValid = 0;
					}
					//System.out.println("Read3 "+off+" "+sofar+" "+len+" "+nbytes+" b="+b.length+" val="+b[0]);
				} else {			
					// Read a full record directly into the output buffer
					int nbytes;
					nbytes = _zFile.read(b, off + sofar, len - sofar);
					//System.out.println("Read2 "+off+" "+sofar+" "+len+" "+nbytes+" b="+b.length+" val="+b[0]);
					if (nbytes < 0) throw new EORFException(sofar);
					++_recordNumber;
					sofar += nbytes;
				}
			}
		}
		
		public void seek(long pos) throws IOException {
			//System.out.println("Seek "+pos);
			long newRecordNumber = pos/_recordLength;
			// Avoid unnecessary seeks
			if (newRecordNumber != _recordNumber) {
				_zFile.seek(newRecordNumber, 0);
				_recordNumber = newRecordNumber;
				_bufferValid = 0;
			}
			_seekOffset = pos%_recordLength;
		}
	}
	/*[ENDIF] PLATFORM-mz31 | PLATFORM-mz64 | ! ( Sidecar18-SE-OpenJ9 | Sidecar19-SE )]*/

	public ClosingFileReader(File file) throws IOException
	{
		_fileRef = file;
		/*[IF PLATFORM-mz31 | PLATFORM-mz64 | ! ( Sidecar18-SE-OpenJ9 | Sidecar19-SE )]*/
		try {
			_file = new BaseRandomAccessFile(_fileRef, "r");
		} catch (FileNotFoundException e1) {
			// try the z/OS com.ibm.jzos
			try {
				ZosRandomAccessFile zosFile = new ZosRandomAccessFile(_fileRef, "r");
				_file = zosFile;
				// Set the buffer to be a multiple of the record length
				_page_size = zosFile._recordLength;
				_buffer = new byte[BUFFER_PAGES * _page_size];
			} catch (FileNotFoundException e2) {
				FileNotFoundException e = new FileNotFoundException("z/OS file not found");
				e.initCause(e2);
				throw e;
			}
		}
		/*[ELSE] PLATFORM-mz31 | PLATFORM-mz64 | ! ( Sidecar18-SE-OpenJ9 | Sidecar19-SE )*/
		_file = new BaseRandomAccessFile(_fileRef, "r");
		/*[ENDIF] PLATFORM-mz31 | PLATFORM-mz64 | ! ( Sidecar18-SE-OpenJ9 | Sidecar19-SE )*/
	}
	
	/**
	 * Create a closing file reader from a stream
	 * @param file An ImageInputStream
	 * @throws FileNotFoundException
	 */
	public ClosingFileReader(final ImageInputStream file) throws IOException
	{
		_file = new StreamRandomAccessFile(file);
	}

	/**
	 * Create a closing file reader which can close/delete the backing file on shutdown
	 * @param file
	 * @param deleteOnCloseOrExit Whether to delete the file when the file is closed or at shutdown
	 * @throws FileNotFoundException
	 */
	public ClosingFileReader(final File file, boolean deleteOnCloseOrExit) throws IOException
	{
		this(file);
		if (deleteOnCloseOrExit) {
			deleteOnClose = true;
			/*  
			 * Ensure this file is closed on shutdown so that it can be deleted if required.
			 */
			Runtime.getRuntime().addShutdownHook(new Thread() {
				public void run() {
					try {
						close();
					} catch (IOException e) {
						// Ignore errors e.g. if file is already closed
					}
				}
			});
		}
	}
	
	public byte[] readBytes(int n) throws IOException
	{
		byte[] buffer = new byte[n];
		readFully(buffer);
		return buffer;
	}
	
	public int readInt() throws IOException
	{
		byte[] buffer = new byte[4];
		readFully(buffer);
		return (int)(((buffer[0] & 0xFFL) << 24)
				| ((buffer[1] & 0xFFL) << 16)
				| ((buffer[2] & 0xFFL) << 8)
				| (buffer[3] & 0xFFL));
	}
	
	public void seek(long position) throws IOException
	{
		streamPos = position;
	}
	
	public long readLong() throws IOException
	{
		byte[] buffer = new byte[8];
		readFully(buffer);
		return (((buffer[0] & 0xFFL) << 56)
				| ((buffer[1] & 0xFFL) << 48)
				| ((buffer[2] & 0xFFL) << 40)
				| ((buffer[3] & 0xFFL) << 32)
				| ((buffer[4] & 0xFFL) << 24)
				| ((buffer[5] & 0xFFL) << 16)
				| ((buffer[6] & 0xFFL) << 8)
				| (buffer[7] & 0xFFL));
	}
	
	public short readShort() throws IOException
	{
		byte[] buffer = new byte[2];
		readFully(buffer);
		return (short)(((buffer[0] & 0xFF) << 8)
				| (buffer[1] & 0xFF));
	}
	
	public byte readByte() throws IOException
	{
		byte[] buffer = new byte[1];
		readFully(buffer);
		return buffer[0];
	}
	
	public void finalize() throws Throwable
	{
		try {
			close();
		} finally {
			super.finalize();
		}
	}
	
	public void readFully(byte[] buffer) throws IOException
	{
		int x  = 0;
		while (x < buffer.length) {
			int chunk = Math.min(_buffer.length, buffer.length - x);
			int didRead = read(buffer, x, chunk);
			
			if (didRead <= 0) {
				//make sure that we don't loop here on EOF
				throw new EOFException("Read "+x+" bytes ");
			}
			x+= didRead;
		}
	}
	
	/**
	 * @param buffer
	 * @param bStart
	 * @param length
	 * @return The number of bytes actually read
	 * @throws IOException
	 */
	public int read(byte[] buffer, int bStart, int length) throws IOException
	{
		int readSize=0;
		try {
			if (-1 == _bufferBase) {
				//cache never used so initialize it
				_recache();
			}
			//the buffer is valid so calculate the overlap
			long bLow = _bufferBase;
			long bHigh = _bufferEnd;
			long fLow = streamPos;
			long fHigh = streamPos + length;
			long overlapStart = Math.max(bLow, fLow);
			long overlapEnd = Math.min(bHigh, fHigh);
			
			if ((overlapEnd <= overlapStart) || (streamPos < _bufferBase)) {
				//disjoint memory regions - recache before read
				_recache();
				bLow = _bufferBase;
				bHigh = _bufferEnd;
				overlapStart = Math.max(bLow, fLow);
				overlapEnd = Math.min(bHigh, fHigh);
			}
			
			// By this point, the region in [start, end) is in the buffer and can be read (be sure to correct for the delta from the _bufferBase)
			//now read
			int offset = (int)(overlapStart - _bufferBase);
			readSize = (int) (overlapEnd - overlapStart);
			try {
				System.arraycopy(_buffer, offset, buffer, bStart, readSize);
			} catch (ArrayIndexOutOfBoundsException e) {
				ArrayIndexOutOfBoundsException e1 = new ArrayIndexOutOfBoundsException("System.arraycopy("+_buffer+" length="+_buffer.length+","+offset+","+buffer+" length="+buffer.length+","+bStart+","+readSize+")");
				e1.initCause(e);
				throw e1;
			}
			//note that length != (end - start) in all cases.  If, for example, we are reading the end of the file, the length will be the full buffer while the delta of end and start will be the part of the file we can read
			streamPos += readSize;
			if (readSize ==0 && length > 0) {
				// Force EndOfFile indication
				readSize = -1;
			}
		} catch (EOFException e) {
			readSize = -1;
		}
		return readSize;
	}
	
	/**
	 * Updates the cache to hold the data following the current _readPointer
	 * @throws IOException 
	 */
	private void _recache() throws IOException
	{
		// -1 means unknown length
		long fileLength = _file.length();
		if (fileLength == -1 || fileLength > streamPos) {
			//round down to a page
			long offset = streamPos % _page_size;
			long aligned = streamPos - offset;

			_bufferBase = aligned;
			_bufferEnd = aligned;
			try {
				_file.seek(aligned);
				_file.readFully(_buffer);
				_bufferEnd = aligned + _buffer.length;
			} catch (EORFException e) {
				// exception tells us how many bytes were left
				_file.seek(aligned);
				_file.readFully(_buffer, 0, e.bytesRead);
				_bufferEnd = aligned + e.bytesRead;				
			} catch (EOFException e) {
				//check how big the file is and make sure that we only read what is there
				if (fileLength != -1) {
					_bufferEnd = fileLength;
					_file.seek(aligned);
					_file.readFully(_buffer, 0, (int) (_bufferEnd - aligned));
				} else {
					// Try byte by byte
					_file.seek(aligned);
					_bufferEnd = aligned;
					for (int i = 0; i < _buffer.length; ++i) {
						int r = _file.read();
						if (r == -1) break;						
						_buffer[i] = (byte)r;
						++_bufferEnd;
					}
					if (_bufferEnd == aligned) throw new EOFException("trying to cache beyond end of file");
				}
			}
		} else {
			throw new EOFException("trying to cache beyond end of file");
		}
	}
	
	/**
	 * Currently this attempts to fill the entire buffer
	 */
	public int read(byte[] buffer) throws IOException
	{
		return read(buffer, 0, buffer.length);
	}
	
	public int read() throws IOException 
	{
		byte b[] = new byte[1];
		int r = read(b);
		if (r < 0) return r;
		return b[0] & 0xff;
	}
	
	public long length()
	{
		try {
			return _file.length();
		} catch (IOException e) {
			return -1L;
		}
	}
	
	public void close() throws IOException
	{
		_file.close();
		if (this.deleteOnClose) {
			_fileRef.delete();
		}
	}
	
	/**
	 * @return A new stream for reading from the underlying file
	 * 
	 * @throws FileNotFoundException If this file has moved since the ClosingFileReader was created (since we would have failed in the constructor if the file was ever there)
	 */
	public InputStream streamFromFile() throws FileNotFoundException
	{
		return new FileInputStream(_fileRef);
	}
	
	public URI getURIOfFile() {
		return _fileRef.toURI();
	}
	
	public String getAbsolutePath()
	{
		if (_fileRef == null) return null;
		return _fileRef.getAbsolutePath();
	}
	
	public boolean isMVSFile()
	{
		/*[IF PLATFORM-mz31 | PLATFORM-mz64 | ! ( Sidecar18-SE-OpenJ9 | Sidecar19-SE )]*/
		return _file instanceof ZosRandomAccessFile;
		/*[ELSE]
		return false;
		/*[ENDIF]*/
	}

	public void releaseResources() throws IOException {
		close();		
	}
}
