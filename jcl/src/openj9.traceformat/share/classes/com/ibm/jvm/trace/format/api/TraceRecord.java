/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2009, 2016 IBM Corp. and others
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
package com.ibm.jvm.trace.format.api;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.math.BigInteger;
import java.nio.ByteOrder;
import java.util.List;
import java.util.Vector;

public class TraceRecord implements Comparable<TraceRecord> {
	private TraceContext context;
	private String textSummary;

	/*
	 * the following fields represent the UtTraceRecord struct's fields that
	 * were written as the header to this file
	 */	
	private BigInteger endTime = BigInteger.ZERO;
	BigInteger wrapTime = BigInteger.ZERO;
	BigInteger writePlatform = BigInteger.ZERO;
	private byte endTimeBytes[] = new byte[8];
	private byte wrapTimeBytes[] = new byte[8];
	BigInteger writeSystem = BigInteger.ZERO;
	
	long threadID;
	long threadSyn1;
	long threadSyn2;
	int firstEntry;
	int nextEntry;
	String threadName = "";	
	/* end of UtTraceRecord struct - see ute_internal.h */

	private byte[] data;
	/* does the record start with a lostRecord. Valid once appendToStream has run */
	boolean lostRecord = false;
	
	/* if a user discards records we can rely on lost record trace points so we record it here.
	 * True means that data immediately prior to this record may have been discarded so we won't
	 * try to merge with any partial data on the end of the stream
	 */
	boolean userDiscardedData = false;
	
	private static final byte userDiscardTracePoint[] = new byte[]{8, 0, 1, 0, 0, 0, 0, 0};

	/* This is the size of the static portion of the tracerecord header, ie excluding the thread name */
	public static final int TRACERECORD_HEADER_SIZE = 64;
	
	private static final int GUESSED_MAX_THREAD_NAME = 128;
	
	/* These fields are only used if this is a file backed trace record */
	RandomAccessFile file;
	long offset;
	
	/* a record of the offsets that we've preprocessed to aid in debugging */
	List<Integer> debugOffsets = null;

	/**
	 * This will create a TraceRecord from a byte array. The byte array must be of the correct length
	 * for a trace record in this context.
	 * 
	 * @param data
	 * @param context
	 * @throws IllegalArgumentException
	 */
	public TraceRecord(TraceContext context, byte[] data) throws IllegalArgumentException {
		this.context = context;
		this.data = data;
		
		if (data.length != context.getRecordSize()) {
			throw new IllegalArgumentException();
		}

		if (context.debugLevel > 0) {
			debugOffsets = new Vector<Integer>();
		}
		
		parseHeader(data);
	}
	
	public TraceRecord(TraceContext context, RandomAccessFile file, long offset) throws IOException, IllegalArgumentException {
		this.context = context;
		this.file = file;
		this.offset = offset;

		int required = TRACERECORD_HEADER_SIZE + GUESSED_MAX_THREAD_NAME;

		if (context.debugLevel > 0) {
			debugOffsets = new Vector<Integer>();
		}
		
		while (required != 0) {
			/* should only run twice at most, and only more than once with thread
			 * names of more than GUESSED_MAX_THREAD_NAME chars.
			 */
			byte data[] = new byte[required];
			
			file.seek(offset);
			if (file.read(data) != data.length) {
				throw new IllegalArgumentException();
			}

			required = parseHeader(data);
		}

		if (context.debugStream != null) {
			context.debug(this, 3, summary());
		}
	}
	
	private int parseHeader(byte[] data) throws IllegalArgumentException {
		ByteStream stream = context.createByteStream(data);

		stream.peek(endTimeBytes);
		endTime = stream.getBigInteger(8);
		stream.peek(wrapTimeBytes);
		wrapTime = stream.getBigInteger(8);
		writePlatform = stream.getBigInteger(8);
		writeSystem = stream.getBigInteger(8);
		
		threadID = stream.getLong();
		threadSyn1 = stream.getLong();
		threadSyn2 = stream.getLong();
		firstEntry = stream.getInt();
		nextEntry = stream.getInt();

		
		/* do some sanity checks */
		String error = null;
		if (error == null && firstEntry < TRACERECORD_HEADER_SIZE) {
			error = "data passed to TraceRecord gives firstEntry["+firstEntry+"] < header size["+TRACERECORD_HEADER_SIZE+"]";
		}

		if (error == null && nextEntry > context.getRecordSize()) {
			error = "data passed to TraceRecord gives nextEntry["+nextEntry+"] > record size["+context.getRecordSize()+"]";
		}

		if (error == null && firstEntry > nextEntry && nextEntry != -1) {
			error = "data passed to TraceRecord gives firstEntry["+firstEntry+"] > nextEntry["+nextEntry+"]";
		}

		if (error != null) {
			context.error(this, error);
			throw new IllegalArgumentException(error);
		}

		/* check we've got enough data for reading the name, if not pass back how many actually needed
		 * to be read in.
		 */
		if (data.length < firstEntry) {
			return firstEntry;
		}

		threadName = stream.getASCIIString(firstEntry - TRACERECORD_HEADER_SIZE).intern();

		return 0;
	}
	
	/**
	 * This method ensures that if the record is backed by data in a file that the data is present in memory.
	 * If it's not backed by a file it will return the current size of the records data array.
	 * @return - the number of bytes loaded for the record
	 */
	private int load() {
		/* if we've got a file and offset then make sure we've got all the data */
		if (file != null && (data == null || data.length != context.getRecordSize())) {
			data = new byte[context.getRecordSize()];
			if (context.debugStream != null) {
				context.debug(this, 3, "Reading in full "+data.length+ "byte record @"+offset);
			}

			int bytesRead = 0;
			
			try {
				file.seek(offset);
				bytesRead = file.read(data);
				if (bytesRead != data.length) {
					context.error(this, "couldn't read an entire record from the file");
					
					if (bytesRead <= nextEntry) {
						return 0;
					}

					/* if we've got to at least nextEntry we'll keep going as we can at
					 * least format the stuff entirely in this buffer, but move it into
					 * an array that's shrunk to the data so we don't see extraneous
					 * bytes at the end.
					 */
					byte[] shrunk = new byte[bytesRead];
					System.arraycopy(data, 0, shrunk, 0, bytesRead);
					data = shrunk;
				}
				
				return bytesRead;
			} catch (IOException e) {
				context.error(this, "IOException while reading record at offset "+offset);
				context.error(this, e.getMessage());
				
				return 0;
			}
		}
		
		return data.length;
	}
	
	/**
	 * Appends the body of the data from this trace record to the stream IN
	 * CHRONOLOGICAL ORDER. This means that if the buffer wrapped at all the
	 * data will be reordered so that reading from the bytestream will return
	 * data correctly ordered for reading as a continuous temporal stream. 
	 * @param stream - the stream onto which the record should be appended
	 * @param newThread - indicates whether this is the first record on a thread
	 * @return the number of bytes appended
	 */
	public int appendToStream(ByteStream stream, boolean newThread) {
		/* make sure the records fully in memory */
		if (load() == 0) {
			return 0;
		}

		context.totalRecords++;
		
		/* does the lostRecord tracepoint get written into the record that wrapped or
		 * the one after?
		 * If it's the latter can we put it at the start of the one that wrapped and
		 * then spin in the remainder of the buffer?
		 */
		
		/* if we're spanning an entire record */
		if (nextEntry == -1) {
			if (context.getTraceType() == TraceContext.EXTERNAL) {
				/* we can't yet fix up the length */
				stream.setGuardBytes(stream.getGuardBytes() + data.length - firstEntry);
				stream.add(data, firstEntry);
				
				return data.length - firstEntry;
			} else {
				/* we can't deal with this for internal trace, but it could happen */
				context.warning(this, "Found \"middle of tracepoint record\" for internal trace record.");
				return 0;
			}
		}
		
		/* if we've a perfect fit in the buffer then nextEntry will be == buffersize. We want it to
		 * point to the last byte in the buffer
		 */
		if (nextEntry == context.getRecordSize()) {
			nextEntry--;
		}
		

		/* if this is an internal record that could have wrapped then glue it back together before we start fixing up trace points */
		if (context.getTraceType() == TraceContext.INTERNAL) {
			byte tmp[] = new byte[data.length];
			int pivotIndex = nextEntry + 1;

			System.arraycopy(data, pivotIndex, tmp, firstEntry, data.length - pivotIndex);
			System.arraycopy(data, firstEntry, tmp, data.length - pivotIndex + firstEntry, pivotIndex - firstEntry);
			
			nextEntry = data.length -1;

			data = tmp;
		}
		
		/* fix up the record, moving lengths to the front of the tracepoints */
		int indexSource = nextEntry;
		int indexTarget = nextEntry;
		byte entryLengthSource = data[indexSource];
		
		/* defect workaround for 147869 - out-by-one nextEntry value when a sequence wrap trace point is
		 * written by the non-fastpath section of the trace writing code.
		 */
		if (entryLengthSource == 0 && (nextEntry - firstEntry) > 7) {
			if (data[nextEntry - 1] == 8) {
				/* the length looks right, so check the id bytes */
				if (data[nextEntry - 8] == 0 && data[nextEntry - 7] == 0 && data[nextEntry - 6] == 0) {
					/* this is as good as we're going to get for detecting this defect */
					indexSource--;
					indexTarget--;
					nextEntry--;
					entryLengthSource = data[indexSource];
					
					context.warning(this, "Fixed up misaligned sequence wrap trace point from defect 147869");
				}
			}
		}

		data[indexSource] = '\0';

		/* the leadin is 1 first tracepoints written into a brand new buffer rather than any data from the
		 * preceding tracepoint spilling over, otherwise the data at data[firstEntry] is actually important.
		 */
		int leadin = 0;

		/* number of bytes reserved from earlier buffers */		
		int guardBytes = stream.getGuardBytes();

		/* controls whether we're keeping or discarding the last bytes of the previous buffer and the first
		 * incomplete trace point (if present) in this one. We default to true because that's slightly more
		 * fail safe. The only time we want to keep the fragmented data is when we successfully fix it up!
		 */
		boolean discard = true;

		/* set up a sequence wrap tracepoint that holds the records sequence wrap time. sequence wrap is:
		 * 1 byte of length (8)
		 * 3 bytes of id (0,0,0)
		 * 4 bytes of timestamp
		 *  */
		byte startTimestamp[] = new byte[8]; 
		startTimestamp[0] = 8;
		startTimestamp[1] = 0;
		startTimestamp[2] = 0;
		startTimestamp[3] = 0;

		if (context.metadata.byteOrder == ByteOrder.LITTLE_ENDIAN) {
			System.arraycopy(endTimeBytes, 4, startTimestamp, 4, 4);
		} else {
			System.arraycopy(endTimeBytes, 0, startTimestamp, 4, 4);			
		}
		
		
		/* if indexTarget == firstEntry then the length byte for the previous record is the first byte
		 * in this and we need to do the fixup.
		 */
		while (indexTarget >= firstEntry && entryLengthSource != 0) {
			/* turn the length byte into an int. We & with 0xff to ensure we only get the low bits */
			int len = (int)(entryLengthSource & 0xff);
			indexTarget = indexSource - len;

//			if (context.debugStream != null) {
//				context.debug(this, "tracepoint from 0x"+Long.toString(indexTarget + offset, 16)+"->0x"+Long.toString(indexSource + offset, 16) + ", length: "+len+", offset: "+(indexTarget+1));
//			}
//
//			if (context.debugStream != null) {
//				context.debugStream.print("tracepoint data, displayed back-to-front [");
//				for (int i = indexSource; i > indexTarget && i >= firstEntry; i--) {
//					context.debugStream.print(data[i]+",");
//				}
//				context.debugStream.println("]");
//			}

			if (len == 4) {
				/* we take it on faith that anything with a length of 4 is a long trace point indicator
				 * TracePoint can fix things up if it's wrong
				 */

				/* this is a special tracepoint, indicating that the tracepoint preceding this in the
				 * buffer has a length that can't be stored in a single byte. Instead the length is
				 * a short that is constructed from two non-contiguous bytes.
				 * 
				 * The length for this special tracepoint specifies that it's 4 bytes long, however it
				 * is actually only 3bytes. Byte [0] is the length byte for the regular tracepoint that
				 * it corresponds to and contains the low order bits, [1] and [2] are null, [3] contains
				 * the high order bits. Combining [0] and [3] gives a short with the actual length of
				 * the long tracepoint that preceded this special one.
				 */

				byte longTPSize[] = new byte[4];
				
				/* is the entire special trace point in this record? */
				if (indexTarget < firstEntry) {
					/* nope, so retrieve what's missing */
					byte missing[] = new byte[firstEntry - indexTarget];
					try {
						int found = stream.truncate(missing);
						guardBytes = stream.getGuardBytes();
						if (found != missing.length) {
							if (context.debugStream != null) {
								context.debug(this, 4, "discarding spanned long trace point special because we failed retriving "+missing.length+"bytes, got "+found);
							}
							discard = true;
							break;
						}
					} catch (IndexOutOfBoundsException e) {
						if (context.debugStream != null) {
							context.debug(this, 4, "discarding spanned long trace point special because we underflowed trying to get "+missing.length+"bytes");
						}
						discard = true;
						break;
					}
					
					System.arraycopy(missing, 0, longTPSize, 0, missing.length);
					System.arraycopy(data, firstEntry, longTPSize, missing.length, 4 - missing.length);
				} else {
					longTPSize[0] = data[indexSource - 4];
					longTPSize[1] = data[indexSource - 3];
					longTPSize[2] = data[indexSource - 2];
					longTPSize[3] = data[indexSource - 1];
				}

				/* what's the target index of the actual tracepoint? */
				byte highBits = longTPSize[3];
				byte lowBits = longTPSize[0];
				
				len = (((highBits & 0xff)<< 8) | (lowBits & 0xff));
				
				
				/* sanity check the mid bytes */
				if (longTPSize[1] != '\0' || longTPSize[2] != '\0') {
					if (indexTarget < firstEntry) {
						/* this special spans buffers and it we're likely missing the
						 * earlier one
						 */
						context.warning(this, "center 2 bytes for spanned long tracepoint special are not null, discarding remaining data");
					} else {
						context.error(this, "center 2 bytes for long tracepoint are not null, discarding remaining data");
					}
					
					/* we don't want to process any more no matter where we are because we'll just put
					 * garbage into the stream
					 */
					discard = true;
					break;
				} else {
					if (context.debugStream != null) {
						context.debug(this, 4, "length for long tracepoint is "+len);
					}
				}

				/* doesn't matter how much of the longtp special is in this record, it's still 4 bytes long
				 * and we need to subtract that
				 */
				indexTarget = indexSource - 4 - len;

				
				/* if it fits in the buffer then we operate in place. We check against firstEntry -1 because if we're only one
				 * over then it's just the length byte and we discard the trailing null from the previous buffer we when fall though
				 * into the regular tracepoint processing
				 */
				if (indexTarget >= firstEntry - 1) {
					/* move the actual tracepoint over the 4 byte long tracepoint special we've just read */
					System.arraycopy(data, indexTarget + 1, data, indexTarget + 5, len - 1);
					System.arraycopy(longTPSize, 0, data, indexTarget + 1, longTPSize.length);
				} else {
					/* can be negative if the longtp special spans records */
					int localLen = indexSource - 4 - firstEntry;

					/* insert the long trace point special into an earlier record */
					try {
						if (localLen > 0) {
							stream.put(longTPSize, localLen - len + 1);
						} else {
							stream.put(longTPSize, 0 - len + 1);
						}
						guardBytes = stream.getGuardBytes();
					} catch (IndexOutOfBoundsException e) {
						if (context.debugStream != null) {
							context.warning(this, "Unable to insert long trace point special at index "+(localLen - len + 1)+" in the byte stream");
						}
						discard = true;
						break;
					}
					
					if (localLen > 0) {
						/* move the data in this record up over the 4 bytes of the special */
						System.arraycopy(data, firstEntry, data, firstEntry + 4, localLen);
						leadin+= 4;
					} else {
						/* need to skip the bytes of the longtp special in this record (we've already
						 * sucked up the bytes in the previous record when we assembled the special
						 * above
						 */
						leadin+= 4 + localLen;
					}
				}
			} else if (len == 8) {
				/* sequence wrap or lost record */
				if (data[indexTarget+1] == 0x0 && data[indexTarget+3] == 0x0) {
					if (data[indexTarget+2] == 0x0) {
						/* sequence wrap */
						if (indexTarget > firstEntry) {
							byte timestamp[] = new byte[8];
							timestamp[0] = 8;
							/* copy this new sequence wrap into temp array, excluding the length byte */
							System.arraycopy(data, indexTarget + 1, timestamp, 1, 7);

							/* overwrite the sequence wrap with the one constructed from the records wrapTime */
							System.arraycopy(startTimestamp, 1, data, indexTarget + 1, 7);

							startTimestamp = timestamp;
							
							/* DEBUG accounting */
							if (context.debugLevel > 0) {
								debugOffsets.add(Integer.valueOf(indexTarget));
							}
						} else {
							/*
							 * there is no point in processing a sequence wrap trace point that
							 * spans records or is aligned with the start as we've already got the
							 * same data from the record header
							 */
							int fragmentSize = stream.getGuardBytes();
							if (fragmentSize != 0 && fragmentSize != firstEntry - indexTarget) {
								if (context.debugStream != null) {
									context.debug(this, 4, "Sequence wrap trace point spans buffers but doesn't match fragment size from preceeding buffer, discarding both");
								}
							}
							discard = true;
							break;
						}
					} else if (indexTarget == firstEntry && data[indexTarget] == 0x0 && data[indexTarget+2] == 0x1) {
						/* lost record - these are always aligned at the beginning of the record so we don't need to care about spanning */
						discard = true;
						lostRecord = true;

						/* DEBUG accounting */
						if (context.debugLevel > 0) {
							debugOffsets.add(Integer.valueOf(indexTarget));
						}
					} else {
						/* oh crap */
						context.error(this, "Special tracepoint (length is 8) and neither sequence wrap or lost record");
					}
				}
			}

			/* regular tracepoint */
			if (indexTarget > firstEntry) {
				/* it's all within the current record */
				if (context.debugLevel >= 5) {
					if (len > 12) {
						context.debug(this, 5, "fixing up tracepoint: "+new String(data, indexTarget + 12, Math.min(len - 12,8))+", data["+indexTarget+"] = "+(entryLengthSource & 0xff));
					} else {
						context.debug(this, 5, "fixing up special tracepoint, length "+len);
					}
				}
				
				
				byte entryLengthTarget = data[indexTarget];
				data[indexTarget] = entryLengthSource;
				entryLengthSource = entryLengthTarget;
				
				indexSource = indexTarget;
				
				/* DEBUG accounting */
				if (context.debugLevel > 0) {
					debugOffsets.add(Integer.valueOf(indexTarget));
				}
			} else if (indexTarget == firstEntry && data[firstEntry] == '\0') {
				/* this means the first tracepoint in the buffer is exactly aligned with the start of the
				 * data. In the case we've flushed the record before hand there may be dangling data from the
				 * preceding record that we want to discard.
				 */
				
				/* this discards dangling data and also the trailing length byte which we no longer need */
				if (guardBytes > 0) {
					discard = true;
				}
				
				/* we use that empty space at firstEntry to hold the length */
				data[indexTarget] = entryLengthSource;
				/* so that we add the first tracepoint */
				indexSource = indexTarget;

				/* DEBUG accounting */
				if (context.debugLevel > 0) {
					debugOffsets.add(Integer.valueOf(indexTarget));
				}

				break;
			} else if (indexTarget == firstEntry && data[firstEntry] != '\0') {
				/* if indexTarget == firstEntry then data[firstEntry] contains the length for a tracepoint,
				 * spilled from the preceding buffer so we fix up the current tracepoint, then test this again
				 * when indexTarget will be negative.
				 */
				
				/* we do exactly the same here as we would for a tracepoint that fits entirely */
				byte entryLengthTarget = data[indexTarget];
				data[indexTarget] = entryLengthSource;
				entryLengthSource = entryLengthTarget;
				
				indexSource = indexTarget;
			} else {
				/* indexTarget < firstEntry */
				if (context.getTraceType() == TraceContext.EXTERNAL) {
				
					int negativeOffset = indexTarget - firstEntry - leadin;
					if (context.debugStream != null) {
						context.debug(this, 5, "Negative offset for fixup is "+negativeOffset);
					}
					
					if (negativeOffset == -1) {
						/* the tracepoint is entirely in this record, it's just the length we're trying to promote */
						
						if (guardBytes == 1) {
							/* get rid of the 0'd length byte at the end of the stream that we're expecting */
							stream.truncate(1);
							discard = false;
						} else {
							/* we're discarding more data than anticipated */
							discard = true;
						}
						
						/* put the length into firstEntry -1 as we're done with the header data */
						data[indexTarget] = entryLengthSource;
						indexSource = indexTarget;
						
						/* DEBUG accounting */
						if (context.debugLevel > 0) {
							debugOffsets.add(Integer.valueOf(indexTarget));
						}
					} else {						
						if (guardBytes + negativeOffset != 0 || userDiscardedData) {
							discard = true;
						} else {
							if (context.debugStream != null) {
								context.debug(this, 5, "Trying to put byte into stream at offset "+ negativeOffset +" into "+stream.getGuardBytes()+" guard bytes");
							}
							try {
								byte b = stream.put(entryLengthSource, negativeOffset);
								
								if (b != '\0') {
									discard = true;
								} else {
									if (context.debugStream != null) {
										context.debug(this, 5, "successfully fixed up earlier buffer");
									}
									discard = false;
									
									/* DEBUG accounting */
									if (context.debugLevel > 0) {
										debugOffsets.add(Integer.valueOf(indexTarget));
									}
								}
							} catch (IndexOutOfBoundsException e) {
								/* we don't have enough previous data to complete this tracepoint so discard it */
								context.error(this, "discarding end of spanned tracepoint for "+ getIdentifier() +", missing "+Math.abs(negativeOffset)+" bytes for start, discarding "+(indexSource - firstEntry - leadin)+" bytes");
								discard = true;
							}
						}

						if (!discard) {
							/* if there was any of this tracepoint in the current buffer then add it to the stream now */
							if (indexSource > firstEntry) {
								stream.add(data, firstEntry + leadin, indexSource - firstEntry - leadin);
							}
						}
						
						/* we're done with this record */
						break;
					}
				} else {
					/* we've wrapped in this buffer, so we need to reorder the pieces */
					if (context.getTraceType() == TraceContext.INTERNAL) {
						/* we assumed that any data after nextEntry was trace data we were overwriting, but it could
						 * have been garbage or null so discard anything remaining
						 */
					} else {
						context.error(this, "Unknown trace type: "+context.getTraceType());
					}

					discard = true;
				}
			}
			
			context.totalTracePoints++;
		}

		if (discard) {
			/* The amount of data we expect in the previous buffer and the amount actually there don't match.
			 * This implies:
			 *   a. missing data inbetween the end of the stream and the start of this buffer
			 *   b. corrputed trace data
			 *   c. incorrect parsing
			 *   
			 * We throw away the mismatched parts as they're inconsistent.
			 */
			long expected = Math.abs(indexTarget - firstEntry - leadin);

			stream.truncate(guardBytes);
			leadin = indexSource - firstEntry;

			if (userDiscardedData && !lostRecord) {
				/* inject a lost record trace point if there was user discarded data. This allows us to report it
				 * when this section of the stream is consumed.
				 */
				stream.add(userDiscardTracePoint);
			} else if (expected != guardBytes) {
				if (lostRecord) {
					/* do nothing, spurious data in previous record is expected */
					context.debug(this, 4, "Discarding data depending on contents of lost records");
				} else if (expected == 0) {
					/* the previous record was probably flushed */
					context.debug(this, 4, "Previous record was likely flushed, discarding trailing data");
				} else if (!newThread) {
					context.warning(this, "Expected "+expected+" of data from previous buffer for thread "+ getIdentifier() +", got "+guardBytes+" so discarded both inconsistent pieces");
				}
			}
		}
		
		/* figure out how many bytes at the end of the record could be the start of a spanning
		 * tracepoint and guard them
		 */
		if (context.debugStream != null) {
			context.debug(this, 4, "guarding "+(data.length - nextEntry)+"bytes");
		}
		stream.setGuardBytes(data.length - nextEntry);

		/* add the start time to the stream */
		stream.add(startTimestamp);
		/* DEBUG accounting */
		if (context.debugLevel > 0) {
			debugOffsets.add(Integer.valueOf(Integer.MIN_VALUE));
		}

		stream.add(data, indexSource);

		return data.length - (firstEntry + leadin);
	}

	public String toString() {
		return getIdentifier();
	}
	
	private String getIdentifier() {
		return (threadID + threadName).intern();
	}
	
	public String summary() {
		if (textSummary == null) {
			StringBuilder s = new StringBuilder("TraceRecord:"+System.getProperty("line.separator"));

			if (file != null) {
				s.append("file offset:    "+offset).append(System.getProperty("line.separator"));
			} else {
				s.append("non file data").append(System.getProperty("line.separator"));
			}
			
			s.append("internal id:    ").append(toString()).append(System.getProperty("line.separator"));
			
			s.append("endTime:        ").append(endTime).append(System.getProperty("line.separator"));
			s.append("wrapTime:       ").append(wrapTime).append(System.getProperty("line.separator"));
			s.append("writePlatform:  ").append(writePlatform).append(System.getProperty("line.separator"));
			s.append("writeSystem:    ").append(writeSystem).append(System.getProperty("line.separator"));
			s.append("threadID:       ").append("0x"+Long.toHexString(threadID)).append(System.getProperty("line.separator"));
			s.append("threadSyn1:     ").append(threadSyn1).append(System.getProperty("line.separator"));
			s.append("threadSyn2:     ").append(threadSyn2).append(System.getProperty("line.separator"));
			s.append("firstEntry:     ").append(firstEntry).append(System.getProperty("line.separator"));
			s.append("nextEntry:      ").append(nextEntry).append(System.getProperty("line.separator"));

			textSummary = s.toString();
		}

		return textSummary;
	}

	public int compareTo(TraceRecord other) {
		return wrapTime.compareTo(other.wrapTime);
	}
}
