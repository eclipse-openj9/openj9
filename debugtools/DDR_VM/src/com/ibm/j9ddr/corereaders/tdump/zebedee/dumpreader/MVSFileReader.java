/*******************************************************************************
 * Copyright (c) 2009, 2015 IBM Corp. and others
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

package com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.logging.Logger;

import javax.imageio.stream.ImageInputStreamImpl;

import com.ibm.jzos.ZFile;

/**
 * This is a wrapper class which provides an ImageInputStream implementation for MVS dump datasets on z/OS. The underlying I/O
 * calls to read the dump are made using the JZOS API. JZOS replaced the earlier JRIO API which was deprecated in Java 6.1.
 * 
 * MVS dump datasets are record-based, containing 4160-byte records. Random access is supported, but the rules are:
 *		1) You have to read a complete record at one time, from the start position of a 4160-byte record 
 *		2) Using seek() to access a record using a byte position or byte offset in the file is very expensive because of
 *		the way the records are organized in 'control intervals' with gaps. Essentially it walks the entire file each time.
 *
 * We read a complete record only when required, handing back just the requested bytes to the caller. To avoid using seek()
 * we read all the records in the dataset sequentially first, and save the file position pointers for each record. Then we
 * use the file position for subsequent random access to the records. 
 * 
 * The file position is returned as a byte array. The underlying C structure shows it is actually an array of 8 longs:
 *		struct __fpos_t {
 *			long int __fpos_elem[8];
 *		};
 */
public class MVSFileReader extends ImageInputStreamImpl {

	private ZFile dataset; // dump dataset
	private int recordLength; // dataset record/block size
	byte[] cacheRecord = null; 	// cache for current record in the dataset
	private long cacheRecordNumber = -1; // record number of the current cached record
	private boolean firstTimeThrough = true; // first time scan, save file position for each record
	private List fposList = new ArrayList(10000); // array of file positions for each record, initial size 10,000

	private static Logger log = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);	
	
	/**
	 * Open the MVS dataset using the JZOS ZFile API
	 */
	public MVSFileReader(String filename) throws FileNotFoundException {
		
		log.fine("Opening dump as an MVS dataset");
		try {
			dataset = new ZFile("//'" + filename + "'", "rb,type=record");
			recordLength = dataset.getLrecl();
			cacheRecord = new byte[recordLength];
			log.fine("MVS dataset opened OK, record length = " + recordLength);
		} catch (IOException e) {
			FileNotFoundException e1 =  new FileNotFoundException("Could not find: " + filename);
			e1.initCause(e);
			throw e1;
		}
	}

	/**
	 * Read a set of bytes from the MVS dataset
	 * 
	 * This method allows the caller to read a set of bytes from the current byte position in the input
	 * stream (streamPos). For MVS datasets we have to read whole records then return the bytes requested
	 * from within the record.
	 * 
	 * Using seek() with a byte address on an MVS dump dataset is very slow, so we save the file position
	 * for each record the first time the dataset is read, and use those for subsequent file access. This
	 * optimisation is dependent on the initial sequential scan of the dump which the z/OS dump reader
	 * performs to create a mapping of memory addresses to file offsets for each address space in the dump. 
	 */
	public int read(byte buffer[], int offset, int length) throws IOException {

		// Convert the current stream position to a record number and offset within the record
		long recordNumber = streamPos / recordLength;
		int recordOffset = (int)(streamPos % recordLength);
		
		// If the stream position is not within the current cached record we will need to read the dataset
		if (recordNumber != cacheRecordNumber) {
			// need to read the dataset
			if (recordNumber == cacheRecordNumber + 1) {
				// file position already at the right place, no need to seek/re-position
			} else {
				dataset.setPos((byte[])fposList.get((int)recordNumber));
			}
			
			if (firstTimeThrough) {
				fposList.add(dataset.getPos()); // store the current file position
			}
			
			int bytesRead = dataset.read(cacheRecord);
			if (bytesRead < 0) { // end of file
				firstTimeThrough = false;
				return -1;  
			}
			cacheRecordNumber = recordNumber; // local cache updated
		}
		
		// We can only read one record's worth at a time from the dataset, so limit the length returned
		if (recordOffset + length > recordLength) {
			length = recordLength - recordOffset;
		}
		System.arraycopy(cacheRecord, recordOffset, buffer, offset, length);
		
		seek(streamPos + length); // re-position the stream
		return length;
	}
	
	/**
	 * Simple method to read one byte - needed as ImageInputStreamImpl doesn't do it
	 */
	public int read() throws IOException {
		byte b[] = new byte[1];
		int n = read(b);
		return n < 0 ? -1 : b[1] & 0xff;
	}
	
	/**
	 * Close the MVS dataset (via the JZOS ZFile API) and close the stream
	 */
	public void close() throws IOException {
		if( dataset != null ) {
			dataset.close();
		}
		super.close();
	}
}
