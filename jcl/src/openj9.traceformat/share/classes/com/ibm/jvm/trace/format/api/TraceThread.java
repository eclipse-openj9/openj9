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
package com.ibm.jvm.trace.format.api;

import java.math.BigInteger;
import java.nio.BufferUnderflowException;
import java.util.Collections;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.SortedSet;
import java.util.TreeSet;
import java.util.Vector;

public class TraceThread implements Comparable {
	TraceContext context;

	BigInteger timerUpperWord = BigInteger.ZERO;
	BigInteger newestWrapTime = BigInteger.ZERO;

	long threadLostRecordCount = 0;
	long threadRecordCount = 0;

	protected long threadID = 0;
	protected long nativeThreadID = 0;
	protected String threadName = "";
	private Long threadIdentifier = null;

	protected ByteStream stream = null;
	TracePointImpl next = null;
	TracePointImpl live = null;

	ThreadIterator iterator;
	
	Vector records = new Vector(); 

	/* if a user discards records we can rely on lost record trace points so we record it here.
	 * True means that data immediately after the current contents of the threads stream has been
	 * discarded and that there are no records in the store in which to record the fact.
	 */
	boolean userDiscardedData = false;
	
	/* To aid in debugging, this holds the record number and offset of the preprocessed length from
	 * appendToStream for the current record.
	 */
	List debugOffsets = new Vector();
	
	class ThreadIterator implements Iterator {
		MissingDataException lostBytes;
		TraceThread thread;

		private ThreadIterator(TraceThread thread) {
			this.thread = thread;
		}

		public boolean hasNext() {
			refresh();
			return next != null;
		}

		/*
		 * This does NOT throw the NoSuchElementException in the case
		 * where there's no more tracepoints available as it's possible
		 * more will become available on this iterator in the future.
		 * Instead null is returned.
		 * 
		 * MissingDataException (that extends NoSuchElementException) is
		 * thrown if there was data missing where the next tracepoint
		 * was expected. The iterator remains valid and will continue to
		 * return tracepoints after the missing data.
		 * 
		 * @see java.util.Iterator#next()
		 */
		public Object next() throws MissingDataException {
			MissingDataException saved = lostBytes;
			if (lostBytes != null) {
				/*
				 * the last refresh of next left us with an
				 * exception to throw
				 */
				lostBytes = null;
				throw saved;
			}

			live = next;
			next = null;

			/* repopulate next */
			refresh();

			/*
			 * check to see if live is dg.262 and if so that's
			 * the end of this thread.
			 */
			if (live != null && live.tracepointID == 262 && live.componentName.equals("dg")) {
				/*
				 * this is the last tracepoint for this thread,
				 * best we do at the moment is null the stream
				 * so that the last record can be collected
				 */
				context.debug(this, 2, "End of data for thread " + threadIdentifier);
				// We have called refresh so next should be primed.
				if( next == null ) {
					stream = null;
				}
				context.threadTerminated(thread, next != null);
			}
			return live;
		}

		public void remove() {
			throw new UnsupportedOperationException("Tracepoints are removed as they are returned by the call to next()");
		}
	}

	TraceThread(TraceContext context, long threadID, long osThreadID, String threadName) {
		this.context = context;
		this.threadName = threadName;
		this.threadID = threadID;
		this.nativeThreadID = osThreadID;

		this.threadIdentifier = Long.valueOf(threadID);
		this.stream = context.createByteStream();
	}

	public Iterator getIterator() {
		if (iterator == null) {
			iterator = new ThreadIterator(this);
		}

		return iterator;
	}

	synchronized private TracePointImpl getNextRegularTracepoint() throws MissingDataException {
		long upperWord = 0;
		TracePointImpl tracepoint = null;

		try {
			/*
			 * we shouldn't be calling this if we've nulled the
			 * stream on thread termination, but check
			 */
			if (stream != null) {
				tracepoint = new TracePointImpl(context, stream, this);
				if (context.debugLevel > 0) {
					if (debugOffsets.size() > 0) {
						tracepoint.debugInfo = (TracePointDebugInfo)debugOffsets.remove(0);
					} else {
						tracepoint.debugInfo = new TracePointDebugInfo(-1,-1);
					}
				}
			}

			if (tracepoint == null) {
				return null;
			}
		} catch (BufferUnderflowException e) {
			TraceRecord record = null;

			/* check to see if there's a record waiting to be appended */
			if (records.isEmpty()) {
				return null;
			}

			record = (TraceRecord)records.firstElement();
			records.remove(record);

			record.appendToStream(stream, threadRecordCount == 0);
			threadRecordCount++;

			/* Populate the debug data */
			if (context.debugLevel > 0) {
				/* the record's debug offsets were populated in reverse order */
				Collections.reverse(record.debugOffsets);
				Iterator itr = record.debugOffsets.iterator();
				while (itr.hasNext()) {
					Integer offset = (Integer)itr.next();
					debugOffsets.add(new TracePointDebugInfo((int)context.totalRecords - 1, offset.intValue()));
				}
			}

			/* record the time of the most recent record that's been appended. While in the records store
			 * the order of addition doesn't matter, but once appended the ordering is fixed for records
			 * earlier than this time stamp. 
			 */
			newestWrapTime = record.wrapTime;

			return getNextRegularTracepoint();
		}

		/*
		 * if this is null, or a tracepoint that a user should see then
		 * return it
		 */
		if (tracepoint.isInvalid()) {
			context.warning(this, "suppressing invalid tracepoint for thread " + threadIdentifier);
		} else if (tracepoint.isNormalTracepoint()) {
			tracepoint.time_merged = timerUpperWord.or(BigInteger.valueOf(tracepoint.time_lowerWord));
			// tracepoint.time_merged =
			// BigInteger.valueOf(timerUpperWord |
			// (tracepoint.time_lowerWord >> 1)).shiftLeft(1);

			return tracepoint;
		}

		/*
		 * this is a special tracepoint so update whichever thread state
		 * it's telling us about
		 */
		long lostCount = tracepoint.getLostRecordCount();
		if (lostCount != 0) {
			long bytes = 0;

			if (lostCount > 0) {
				threadLostRecordCount += lostCount;
				bytes = lostCount * context.getRecordSize();
				context.debug(this, 2, "Tracepoint says we lost " + lostCount + " records (total for thread: " + threadLostRecordCount + ")");
			} else if (lostCount == -1) {
				context.debug(this, 2, "Tracepoint probably injected for user discarded data of unknown size");
			} else {
				if (lostCount < 0) {
					context.error(this, "lost bytes count is negative - lost " + lostCount + " records of " + context.getRecordSize() + " bytes each");
					bytes = Long.MAX_VALUE;
				}
			}
			throw new MissingDataException(this, bytes);
		}

		upperWord = tracepoint.getNewTimerUpperWord();

		/*
		 * update the upper word of the timestamp for this thread if
		 * there's a new one
		 */
		if (upperWord != 0) {
			BigInteger oldUpper = timerUpperWord;
			timerUpperWord = BigInteger.valueOf(upperWord).shiftLeft(32);
			// timerUpperWord = upperWord << 31;
//			if (timerUpperWord.compareTo(oldUpper) < 0) {
//				context.error(this, "new upper word for timer is older than current, reverting");
//				timerUpperWord = oldUpper;
//			}
		}

		return getNextRegularTracepoint();
	}

	/**
	 * Adds a record to the set of records associated with this thread. If we have records stored ahead of time
	 * when we run out of data in the current record we append another from the store rather than reporting
	 * an underflow. This allows the adding of all records from trace files before processing starts.
	 * This method maintains ordering in the list
	 * 
	 * @param record
	 */
	synchronized protected void addRecord(TraceRecord record) throws IllegalArgumentException {
		/*
		 * have to check that this record is newer than any previous
		 * ones we've had on this thread
		 */
		int i = 1;
		
		/* if it's been noted that there was user discarded data then propagate that to the record */
		if (userDiscardedData) {
			record.userDiscardedData = true;
			userDiscardedData = false;
		}

		records.add(record);
	}
	
	/**
	 * This records the fact that we've been told that the user discarded data at this point in the series of records.
	 * This fact will be tagged onto the next record to be added to the thread and will cause a lost record trace point
	 * to be injected 
	 */
	synchronized void userDiscardedData() {
		userDiscardedData = true;
	}
	
	public int compareTo(Object obj) {
		TraceThread thread = (TraceThread)obj;

		if (next != null && thread.next != null) {
			/* we actually have a tracepoint for both so can compare */
			return next.time_merged.compareTo(thread.next.time_merged);
		} else if (next == null && thread.next == null) {
			return 0;
		} else if (next == null) {
			return 1;
		} else {
			return -1;
		}
	}

	/**
	 * Allows things to ask us to refresh the cursor without altering other
	 * state
	 */
	void refresh() {
		if (next == null) {
			/* TODO: wrap the other sorted references in sync blocks */
			context.sorted = false;
			/*
			 * TODO: timing issue here with sorted being set... need
			 * to check this
			 */

			/*
			 * need to have the next data point lined up here so we
			 * can sort, so we cycle.
			 */
			long bytes = 0;
			MissingDataException e = null;
			while (true) {
				try {
					next = getNextRegularTracepoint();
					break;
				} catch (MissingDataException e2) {
					bytes += e2.getMissingBytes();
					e = e2;
				}
			}

			/*
			 * we've got the latest actual tracepoint on the stream
			 * if available. Now we need to save the exception to be
			 * thrown next time the iterator's called (remember, we
			 * got the original exception from refreshing next, so
			 * correct time to throw this on to the user is their
			 * next call to iterator.next())
			 */
			if (e != null) {
				e.missingByteCount = bytes;
				((ThreadIterator) getIterator()).lostBytes = e;
			}
		}
	}

	public boolean equals(Long id) {
		return this.threadIdentifier.equals(id);
	}

	public String toString() {
		return ("[" + Long.toHexString(threadIdentifier) + "] " + threadName).intern();
	}

	public long getThreadID() {
		return threadID;
	}

	public long getNativeThreadID() {
		return nativeThreadID;
	}

	public String getThreadName() {
		return threadName;
	}
}
