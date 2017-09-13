/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package com.ibm.jvm.format;

import java.io.IOException;
import java.math.BigInteger;
import java.util.Vector;

/**
 * Extends Trace Record. Special processing needed for internal trace records.
 * 
 * @author Simon Rowland
 */
public class TraceRecord50 implements Comparable {
	public static final int INTERNAL_WRAP_SPLIT_TP = -1;
	public static final int EXTERNAL_WRAP_SPLIT_TP = -2;
	
	/*
	 * the following fields represent the UtTraceRecord struct's fields that
	 * were written as the header to this file
	 */
	private BigInteger timeStamp;

	private BigInteger wrapTime;

	private BigInteger writePlatform = BigInteger.ZERO;

	private BigInteger writeSystem = BigInteger.ZERO;

	private long threadID = 0;

	private long threadSyn1 = 0;

	private long threadSyn2 = 0;

	private int firstEntry;

	private int nextEntry;

	private String threadName;

	/* end of UtTraceRecord struct - see ute_internal.h */
	private BigInteger upperTimeWord = BigInteger.ZERO;

	private String threadIDString = null;

	private String fromFileName = null;

	private long offsetInFile = -1;

	private int bufferLength = -1;

	private int traceRecordType = -1;

	private Vector tps = new Vector();

	private int dataStart;

	private int dataEnd;

	private int dataLength;

	private byte[] overspillData = null;

	private BigInteger overspillUpperWord = null;

	private boolean primed = false;

	private byte[] traceRecordBytes;

	private TraceFile traceFile;

	private int offset;

	private int tplength;

	private long absolutePositionInFile;

	private byte[] dataAtEndOfBuffer;

	private boolean isMiddleOfTracePoint = false;
	
	private long lostRecordCount = 0;
	
	private BigInteger earliestTimeStamp = null;

	public int processTraceBufferHeader(TraceFile traceFile, long start,
			int bufferLength) throws IOException {
		this.fromFileName = traceFile.toString();
		this.offsetInFile = start;
		this.bufferLength = bufferLength;

		this.traceFile = traceFile;
		/* read header */
		traceFile.seek(start);
		timeStamp = traceFile.readBigInteger(8);
		wrapTime = traceFile.readBigInteger(8);
		writePlatform = traceFile.readBigInteger(8);
		writeSystem = traceFile.readBigInteger(8);
		threadID = traceFile.readL();
		threadSyn1 = traceFile.readL();
		threadSyn2 = traceFile.readL();
		firstEntry = traceFile.readI();
		nextEntry = traceFile.readI();
		threadName = traceFile.readString(firstEntry - 64);

		upperTimeWord = timeStamp.shiftRight(32);
		threadIDString = Util.formatAsHexString(threadID);

		dataStart = firstEntry;
		dataEnd = nextEntry;

		if (dataEnd < 0) {
			isMiddleOfTracePoint = true;
			dataLength = bufferLength - dataStart;
			Util.Debug.println("Found a middle section - dataLength == "
					+ dataLength);
		} else {			
			dataLength = dataEnd - dataStart;
		}
		
		if ( nextEntry >= 0 && (nextEntry < dataStart || nextEntry > bufferLength) ) {
			dataLength = 0;
			TraceFormat.invalidBuffers++;
		}

		Util.Debug.println("Buffer is at offset " + Long.toHexString(start) + " in " + traceFile);
		Util.Debug.println("  First TracePoint in buffer is at offset "
				+ firstEntry);
		Util.Debug.println("  Last TracePoint in buffer is at offset  "
				+ nextEntry);
		Util.Debug.println("  timeStamp:    " + timeStamp);
		Util.Debug.println("  wrapTime:     " + wrapTime);
		Util.Debug.println("  writePlatform " + writePlatform);
		Util.Debug.println("  writeSystem   " + writeSystem);

		// Update the overall timers that are used in the TraceFormat class.
		if (writePlatform.compareTo(TraceFormat.lastWritePlatform) > 0) {
			Util.Debug.println("updating lastWritePlatform" + writePlatform);
			Util.Debug.println("updating lastWriteSystem  " + writeSystem);
			TraceFormat.lastWritePlatform = writePlatform;
			TraceFormat.lastWriteSystem = writeSystem;
		}
		if (wrapTime.compareTo(TraceFormat.first) < 0) {
			TraceFormat.first = wrapTime;
		}
		if (timeStamp.compareTo(TraceFormat.last) > 0) {
			TraceFormat.last = timeStamp;
		}
		
		// If this is the latest record so far, update the file wrap point for this trace file
		if (writeSystem.compareTo(traceFile.lastWriteSystem) > 0) {
			traceFile.lastWriteSystem = writeSystem;
			traceFile.wrapOffset = start;
		}
		return 1;
	}

	public TracePoint getNextTracePoint() {
		TracePoint tp = null;
		/* testing */
		if (!primed) {
			try {
				primeRecord(traceFile);
				primed = true;
			} catch (IOException ioe) {
				ioe.printStackTrace();
			}
		}
		if (tps.isEmpty()){
			Util.Debug.println("Reached end of buffer ");
			tp = null;
		} else {
			tp = (TracePoint) tps.elementAt(0);
			tps.removeElementAt(0);
		}
		return tp;
	}

	public boolean addOverspillData(byte[] overspillFromPreviousBuffer,
			BigInteger upperWord) {
		overspillData = overspillFromPreviousBuffer;
		return true;
	}

	public boolean isMiddleOfTracePoint() {
		return isMiddleOfTracePoint;
	}

	public byte[] getExtraData() {
		if (isMiddleOfTracePoint == true) {
			if (!primed) {
				try {
					primeRecord(traceFile);
					primed = true;
				} catch (IOException ioe) {
					ioe.printStackTrace();
					return null;
				}
			}
			if (traceRecordBytes != null) {
				Util.Debug.println("returning " + traceRecordBytes.length
						+ " bytes of middle");
			} else {
				Util.Debug.println("returning some null stuff for the middle");
			}
			return traceRecordBytes;
		} else {
			return dataAtEndOfBuffer;
		}
	}

	/** 
	 * read the bytes of this UtTraceRecord into a byte array.
	 * 
	 * @param traceFile to read from.
	 * @return an array of bytes that constitute this UtTraceRecord.
	 * @throws IOException if there is a problem reading from the trace file
	 */
	private byte[] readRecord(TraceFile traceFile) throws IOException {
		byte [] recordBytes;
		absolutePositionInFile = offsetInFile + (long) dataStart;
		traceFile.seek(absolutePositionInFile);
		long startOfExtraData = 0;

		/* read the data for this trace record into a buffer to parse */
		recordBytes = new byte[dataLength + 1];
		int dataread = traceFile.read(recordBytes);
		if (dataread != dataLength + 1) {
			System.err.println("*** Incorrect data length read, expecting "
					+ (dataLength + 1) + " found " + dataread
					+ " skipping this TraceBuffer");
			throw new IOException("Can't read record from tracefile");
		}

		/* A tracepoint runs straight through this buffer, 
		 * so save the data but don't format anything yet. This
		 * formatting will be done by the TraceRecord that contains
		 * the end of this tracepoint. */
		if (dataEnd < 0) {
			Util.Debug.println("this is the middle - dataLength == "
					+ dataLength);
			Util.Debug.println("dataStart " + dataStart + " dataEnd " + dataEnd
					+ " bufferLength " + bufferLength);
			/*
			 * Replace the magic number with the dataStart. This will cause
			 * all of the data in this tracepoint to be read in and added to
			 * the overspill data in the code block below.
			 */
			dataEnd = dataStart;
		}

		/* the actual byte at dataEnd is the length of the final tracepoint! */
		int dataLeftAtEndOfBufLength = (bufferLength - dataEnd);
		if (dataLeftAtEndOfBufLength > 0) {
			/* the data at the end of the tracepoint may be the beginning of a tracepoint
			 * that runs into the next TraceRecord. We can't know until we format the next
			 * TraceRecord, so we need to store the extra data for the next TraceRecord to 
			 * retrieve if it needs it. */
			dataAtEndOfBuffer = new byte[dataLeftAtEndOfBufLength];
			startOfExtraData = (long) dataEnd + offsetInFile;
			traceFile.seek(startOfExtraData);
			dataread = traceFile.read(dataAtEndOfBuffer);
			if (dataread != dataLeftAtEndOfBufLength) {
				TraceFormat.outStream
						.println("*** Can't read the last "
								+ dataLeftAtEndOfBufLength
								+ "bytes from a trace record. Record begins at file offset "
								+ offsetInFile
								+ " data to be read should start at " + dataEnd);
				TraceFormat.outStream
						.println("  * will continue processing the remainder of the buffer");
			} else {
				Util.Debug
						.println("Read some excess data from end of buffer at offset "
								+ (dataEnd + offsetInFile) + " in " + traceFile);
			}
		}
		return recordBytes;
	}
	
	/**
	 * primeRecord readies this UtTraceRecord to have its tracepoint data read.
	 * 
	 * Each tracepoint's length field is at the end of the tracepoint, so the traceoints are
	 * formatted backwards out of the record. To enable forward iteration, the whole record 
	 * must be formatted into a queue, which is then iterated over in reverse order.
	 * 
	 * The record should already have been primed with e.g. it's location in the tracefile
	 * through it's constructor.
	 * 
	 * @param traceFile
	 * @return true if it works, false if any errors are encountered. Errors are reported
	 * internally.
	 * @throws IOException if there is an error reading the record's data from the trace
	 * file.
	 */
	public boolean primeRecord(TraceFile traceFile) throws IOException {
		traceRecordBytes = readRecord(traceFile);
		/* start at the end of the data */
		offset = traceRecordBytes.length - 1;
		if (offset < 0){
			return true;
		}
		
		/* keep debug records of where the current tracepoint is and what it looks like */
		long endOfCurrentTP = absolutePositionInFile + offset;
		long startOfCurrentTP = -3;
		boolean currentTPIsFragmented;
		TracePoint sp = null;
		/* allocate once, and allow for max normal size! */
		byte[] tpdata = new byte[bufferLength];

		boolean recordHasWrapped = false;		
		boolean longTPLen = false;

		/* we determine here if this record starts with a lost record tracepoint.
		 * This doesn't actually influence anything until we hit the tracepoint
		 * while working back through the buffer during tracepoint processing.
		 */
		if (traceRecordBytes[0] == 0x0) {
			/* possible lost record, check for signature but make sure we have enough bytes to construct an int */
			if (traceRecordBytes[1] == 0x0 && traceRecordBytes[2] == 0x1 && traceRecordBytes[3] == 0x0 && traceRecordBytes.length >= 8) {
				lostRecordCount = Util.constructUnsignedInt(traceRecordBytes, 4);
				TraceFormat.lostRecordCount+= lostRecordCount; 
				
				/* discard remainder data from previous buffer, it'll be invalid */
				overspillData = null; 
			}
		}
		
		/* parse each tracepoint */
		do {
			if (!longTPLen) {
				tplength = Util.constructUnsignedByte(traceRecordBytes, offset);
			}
			longTPLen = false;
			currentTPIsFragmented = false;
			startOfCurrentTP = endOfCurrentTP - tplength;

			/*
			 * need to check if the previously processed tracepoint (i.e. the
			 * one that was actually in the buffer after this one) was a long
			 * trace record
			 */
			if (sp != null && sp.isLongTracePoint()) {
				int lengthOfLongTracePoint = sp.longTracePointLength();
				Util.Debug.println("Processing long tracepoint - length = "
						+ lengthOfLongTracePoint);
				tplength = lengthOfLongTracePoint;
				longTPLen = true;
			}

			/* we're done with the current tracepoint if we have one */
			sp = null;

			if (tplength > offset) {
				/* 
				 * This tracepoint runs for longer than the available data. Therefore
				 * it is the *end* of a tracepoint that is in more than one buffer.
				 */
				currentTPIsFragmented = true;
				if (recordHasWrapped) {
					/*
					 * this means we are processing an internal trace record,
					 * and that we already did the jiggery-pokery to wrap the
					 * buffer below. So we have the inevitable incomplete oldest
					 * tracepoint that has been partially overwritten with newer data.
					 */
					break;
					/* this record's data is exhausted */
				}
				int numberOfMissingBytes = tplength - offset;
				if (traceRecordType == 0) {
					/* We are in a record that wraps back into itself */
					startOfCurrentTP = INTERNAL_WRAP_SPLIT_TP;
					if (dataAtEndOfBuffer != null) {
						byte restOfBuffer[] = dataAtEndOfBuffer;
						/*
						 * create a temp buffer with the beginning of and the
						 * end of the restOfBuffer
						 */
						byte newBufferToWorkOn[] = new byte[restOfBuffer.length
								+ offset + 1];
						/*
						 * The bytes from 0 -> offset+1 in rawTracePoint contain
						 * the incomplete end of a tracepoint. The bytes that
						 * were ferreted away in the fromEndOfData
						 * incompleteTracePoint contain the beginning of that
						 * tracepoint, and probably several more tracepoints
						 * before that. So I create a temporary buffer,
						 * concatonate those two arrays of bytes, and then
						 * simply replace the rawTraceRecord (which has been
						 * fully processed if we have reached this point) with
						 * the new array and process it as a normal buffer
						 * (which it is!).
						 */
						System.arraycopy(restOfBuffer, 0, newBufferToWorkOn, 0,
								restOfBuffer.length);
						System.arraycopy(traceRecordBytes, 0, newBufferToWorkOn,
								restOfBuffer.length, offset + 1);
						Util.Debug
								.println(" coalesced the buffers successfully! ");
						recordHasWrapped = true;
						traceRecordBytes = newBufferToWorkOn;
						offset = newBufferToWorkOn.length - 1;
						continue;
					} else {
						Util.Debug
								.println(" TraceRecord50 wrapped an internal buffer which doesn't seem to have any extra data at the end");
						break;
					}
				} else {
					/*
					 * external trace buffer - ie missing data should be in
					 * previous buffer
					 */
					startOfCurrentTP = EXTERNAL_WRAP_SPLIT_TP;
					if (overspillData != null) {
						byte restOfBuffer[] = overspillData;
						/*
						 * create a temp buffer with the beginning of and the
						 * end of the restOfBuffer
						 */
						byte newBufferToWorkOn[] = new byte[restOfBuffer.length
								+ offset + 1];
						/*
						 * The bytes from 0 -> offset+1 in rawTracePoint contain
						 * the incomplete end of a tracepoint. The bytes that
						 * were ferreted away in the fromEndOfData
						 * incompleteTracePoint contain the beginning of that
						 * tracepoint, and probably several more tracepoints
						 * before that. So I create a temporary buffer,
						 * concatenate those two arrays of bytes, and then
						 * simply replace the rawTraceRecord (which has been
						 * fully processed if we have reached this point) with
						 * the new array and process it as a normal buffer
						 * (which it is!).
						 */
						System.arraycopy(restOfBuffer, 0, newBufferToWorkOn, 0,
								restOfBuffer.length);
						System.arraycopy(traceRecordBytes, 0, newBufferToWorkOn,
								restOfBuffer.length, offset + 1);
						Util.Debug
								.println(" coalesced the buffers successfully! ");
						recordHasWrapped = true;
						traceRecordBytes = newBufferToWorkOn;
						offset = newBufferToWorkOn.length - 1;
						continue;
					} else {
						/* check if this record started with a lost record tracepoint */
						if (lostRecordCount > 0) {
							/* we recorded lost records so spurious data at the start of the buffer it's entirely surprising */
							sp = new TracePoint(traceRecordBytes, 8, upperTimeWord, threadID, traceFile.toString(), absolutePositionInFile, absolutePositionInFile+8, false);
						}
						Util.Debug
								.println(" TraceRecord50 walked back off the beginning of this buffer, but found no overspill data");
						break;
					}
				}
			}
			offset -= tplength;
			if (sp == null) {
				if (tpdata.length < tplength) {
					tpdata = new byte[tplength + 1];
				}
				System.arraycopy(traceRecordBytes, offset, tpdata, 0, tplength);
				sp = new TracePoint(tpdata, tplength, upperTimeWord, threadID,
						traceFile.toString(), startOfCurrentTP,
						endOfCurrentTP, currentTPIsFragmented);
			}

			if (sp.isTimerUpperWord()) {
				/*
				 * we can simply replace upperTimeWord with the new one, since
				 * the timers work backwards to facilitate formatting!
				 */
				upperTimeWord = BigInteger.valueOf(sp.getNewTimerUpperWord());
			} else if (sp.isLostRecord()) {
				/* we use the timestamp of the fist tracepoint in the buffer if present, or the time of last
				 * modification of the buffer (timeStamp)
				 */
				if (earliestTimeStamp != null) {
					sp.setRawTimeStamp(earliestTimeStamp);
				} else {
					sp.setRawTimeStamp(timeStamp);
				}
				
				if (tps.size() > 0) {
					tps.insertElementAt(sp, tps.size() -1);
				} else {
					tps.add(sp);
				}
			}

			if (sp.isNormalTracepoint()) {
				/* add it to a queue - use a vector pending java 5.0! */
				tps.add(sp);
				earliestTimeStamp = sp.getRawTimeStamp();
			} /* else it is an internal tracepoint so discard it */
			
			/* move pointers to next tracepoint */
			endOfCurrentTP -= tplength;
		} while ((tplength > 0) && (offset >= 0));

		if (offset != 0) {
			Util.Debug.println("TraceRecord ended with some extra data");
		}

		/* this sort is needed in case we processed a wrapped internal buffer */
		Util.Debug.print("About to sort");
		java.util.Collections.sort(tps);
		Util.Debug.println(" ... sorted");
		if (!isMiddleOfTracePoint) {
			traceRecordBytes = null;
		}
		return true;
	}

	public String getThreadName() {
		return threadName;
	}

	public long getThreadIDAsLong() {
		return threadID;
	}

	public String getFileName() {
		return fromFileName;
	}

	public long getOffsetInFile() {
		return offsetInFile;
	}

	public BigInteger getTimeStamp() {
		return timeStamp;
	}

	public BigInteger getLastTimerWrap() {
		return wrapTime;
	}

	public void setTimeStamp(BigInteger newTimeStamp) {
		timeStamp = newTimeStamp;
	}

	public int compareTo(Object other) {
		/* CMVC 177932. Fix to use system write time for sorting trace records rather than the nanosecond tracepoint
		 * timer, because the nanosecond timer is known to hop around across CPUs on some systems. The records for a 
		 * thread are always written FIFO, so sorting is only actually needed in case the trace file was wrapped, i.e.
		 * if trace option -Xtrace:output={file,size} was used.
		 * 
		 * The system write time is only millisecs precision. So adjacent trace records may have identical system times,
		 * particularly in trace snap files. This is mostly OK, as the buffers are written FIFO, and Collections.sort will 
		 * not re-order if the times are identical. We do however need a tie-breaker for cases where we have wrapped from
		 * the end of a trace file to the start.
		 * 
		 * Note that the new trace formatter (com.ibm.jvm.TraceFormat) does not sort records (it does not yet support 
		 * wrapped or generation trace files).
		 * 
		 * Invoked indirectly via call to Collections.sort() in com.ibm.jvm.format.TraceFormat.prime().
		 */
		
		if (writeSystem == BigInteger.ZERO || ((TraceRecord50)other).writeSystem == BigInteger.ZERO) {
			Util.Debug.println("TraceRecord50.compareTo() found trace record with a zero system write time");
			return 0;
		}
		
		int result = writeSystem.compareTo(((TraceRecord50)other).writeSystem);
		if (result == 0) {
			// record write times are the same, calculate tie-breaker for records in wrapped trace files
			if (offsetInFile <= traceFile.wrapOffset && ((TraceRecord50)other).offsetInFile > traceFile.wrapOffset) {
				return 1;
			}
			if (offsetInFile > traceFile.wrapOffset && ((TraceRecord50)other).offsetInFile <= traceFile.wrapOffset) {
				return -1;
			}
			// drop through
		}
		return result;
	}

	public int getTraceType() {
		return traceRecordType;
	}

	public void setTraceType(int traceType) {
		traceRecordType = traceType;
	}

	public BigInteger getLastUpperWord() {
		return upperTimeWord;
	}
	
	public String toString(){
		StringBuffer sb = new StringBuffer();

		sb.append("TraceRecord50:");
		sb.append(timeStamp);
		sb.append(":offset in file ");
		sb.append(absolutePositionInFile);
		sb.append(":len ");
		sb.append(bufferLength);

		return sb.toString();
	}
}
