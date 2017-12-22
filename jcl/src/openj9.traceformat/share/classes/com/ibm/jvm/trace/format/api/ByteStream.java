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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.jvm.trace.format.api;

import java.io.UnsupportedEncodingException;
import java.math.BigInteger;
import java.nio.BufferUnderflowException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.Vector;

public class ByteStream {
	/* The buffer we're currently wrapping */
	ByteBuffer buffer;

	/* The order of the data we're processing */
	ByteOrder byteOrder = ByteOrder.BIG_ENDIAN;

	/* This holds triples describing a slice of an array */
	class Slice {
		byte[] data;
		int offset;
		int length;

		Slice(byte[] data, int offset, int length) {
			this.data = data;
			this.offset = offset;
			this.length = length;
		}
	}

	/* our queue of raw data */
	Vector rawData = new Vector();

	/* do we block on requests that require data we don't yet have? */
	boolean blocking = false;

	/*
	 * this is similar to the ByteBuffer limit, it's indented so we can fix
	 * up the stream without having to worry that a value may already have
	 * been read. Value is simply added to the commit value for any given
	 * get call.
	 */
	private int guardBytes;

	public ByteStream(byte[] data) {
		add(data);
	}

	public ByteStream(byte[] data, int offset) {
		add(data, offset);
	}

	public ByteStream(byte[] data, int offset, int length) {
		add(data, offset, length);
	}

	public ByteStream() {
	}

	/**
	 * This method ensures that there is sufficient data to satisfy a
	 * request
	 * 
	 * @param bytes
	 * @throws BufferUnderflowException
	 */
	synchronized private void commit(int bytes) throws BufferUnderflowException {
		/* add in our limit */
		int requiredBytes = bytes + guardBytes;
		boolean recurse = true;

		/*
		 * if we've got enough data in the current buffer to service
		 * this request just return
		 */
		if (buffer != null && buffer.remaining() >= requiredBytes) {
			return;
		}

		/* can we swap data in from our queue? */
		if (rawData.isEmpty()) {
			if (!blocking) {
				throw new BufferUnderflowException();
			} else {
				try {
					rawData.wait();
				} catch (InterruptedException e) {
					/*
					 * suppress this, we'll translate to
					 * buffer underflow unless there's new
					 * data
					 */
				}

				if (rawData.isEmpty()) {
					throw new BufferUnderflowException();
				}
			}
		}

		/*
		 * we don't have enough data in the buffer but may be able to
		 * recover
		 */
		Slice slice = (Slice)rawData.get(0);
		
		/* if we can guard out of raw data and there's enough in the buffer */
		if (buffer != null && buffer.remaining() >= bytes && slice.length >= guardBytes) {
			return;
		}

		if (buffer == null || buffer.remaining() == 0) {
			rawData.remove(0);
			buffer = ByteBuffer.wrap(slice.data, slice.offset, slice.length);
		} else {

			/*
			 * we've still got some data left in the buffer so merge
			 * it with the next set of raw data and create a new
			 * buffer with the composite
			 */
			int remaining = buffer.remaining();
			byte mergeData[];
			
			/* snip what's needed off the front of the next slice if the slice is big enough
			 * for us to care about copying it */
			if (slice.length > requiredBytes) {
				mergeData = new byte[bytes];
				int shortfall = bytes - remaining;
				buffer.get(mergeData, 0, remaining);
				System.arraycopy(slice.data, slice.offset, mergeData, remaining, shortfall);
				slice.offset+= shortfall;
				slice.length-= shortfall;
				
				/* don't need to recurse here because the fact we're here says there's enough in place */
				recurse = false;
			} else {
				mergeData = new byte[remaining + slice.length];
				buffer.get(mergeData, 0, remaining);
				System.arraycopy(slice.data, slice.offset, mergeData, remaining, slice.length);
				rawData.remove(0);
			}

			buffer = ByteBuffer.wrap(mergeData);
		}

		/* set the byte order for our new buffer */
		buffer.order(byteOrder);

		/* recurse if our new buffer isn't big enough (should be rare) */
		if (recurse && buffer.remaining() < requiredBytes) {
			commit(bytes);
		}
	}

	public void reverseBytes(byte[] data) {
		for (int i = 0; i < data.length / 2; i++) {
			int temp = data[i];
			data[i] = data[(data.length - 1) - i];
			data[(data.length - 1) - i] = (byte) temp;
		}
	}

	public void add(byte[] data) {
		add(data, 0);
	}

	public void add(byte[] data, int offset) {
		if (data == null) {
			return;
		}
		
		add(data, offset, data.length - offset);
	}

	synchronized public void add(byte[] data, int offset, int length) {
		if (data == null || length == 0) {
			return;
		}

		if (data.length < offset + length) {
			throw new IndexOutOfBoundsException();
		}

		if (length < 0) {
			throw new IllegalArgumentException("can't add data with a negative size");
		}

		rawData.add(new Slice(data, offset, length));
		if (blocking) {
			rawData.notify();
		}
	}

	public byte get() {
		commit(1);
		return buffer.get();
	}

	public byte get(int index) {
		commit(index + 1);
		return buffer.get(index);
	}

	/* TODO: fix this so that it's not making the utf8 == ASCII assumption */
	public char getUTF8Char() {
		commit(1);
		return buffer.getChar();
	}

	public char getUTF8Char(int index) {
		commit(index + 1);
		return buffer.getChar(index);
	}

	public double getDouble() {
		commit(8);
		return buffer.getDouble();
	}

	public double getDouble(int index) {
		commit(index + 8);
		return buffer.getDouble(index);
	}

	public float getFloat() {
		commit(8);
		return buffer.getFloat();
	}

	public float getFloat(int index) {
		commit(index + 8);
		return buffer.getFloat(index);
	}

	public int getInt() {
		commit(4);
		return buffer.getInt();
	}

	public int getInt(int index) {
		commit(index + 4);
		return buffer.getInt(index);
	}

	public long getUnsignedInt() {
		commit(4);

		byte data[] = new byte[4];
		buffer.get(data);

		return (byteOrder == ByteOrder.LITTLE_ENDIAN) ? ((((long) data[3] << 24) & 0xff000000L) | (((long) data[2] << 16) & 0xff0000L) | (((long) data[1] << 8) & 0xff00L) | (((long) data[0]) & 0xffL)) : ((((long) data[0] << 24) & 0xff000000L) | (((long) data[1] << 16) & 0xff0000L) | (((long) data[2] << 8) & 0xff00L) | (((long) data[3]) & 0xffL));
	}

	public long getUnsignedInt(int index) {
		commit(index + 4);

		byte data[] = new byte[index + 4];
		peek(data);

		return (byteOrder == ByteOrder.LITTLE_ENDIAN) ? ((((long) data[index + 3] << 24) & 0xff000000L) | (((long) data[index + 2] << 16) & 0xff0000L) | (((long) data[index + 1] << 8) & 0xff00L) | ((long) data[index + 0] & 0xffL)) : ((((long) data[index + 0] << 24) & 0xff000000L) | (((long) data[index + 1] << 16) & 0xff0000L) | (((long) data[index + 2] << 8) & 0xff00L) | ((long) data[index + 3] & 0xffL));
	}

	public long getLong() {
		commit(8);
		return buffer.getLong();
	}

	public long getLong(int index) {
		commit(index + 8);
		return buffer.getLong(index);
	}

	public short getShort() {
		commit(2);
		return buffer.getShort();
	}

	public short getShort(int index) {
		commit(index + 2);
		return buffer.getShort(index);
	}

	public String getASCIIString(int length) {
		commit(length);
		byte stringBytes[] = new byte[length];

		buffer.get(stringBytes);

		int cursor = 0;
		for (cursor = 0; cursor < stringBytes.length && stringBytes[cursor] != '\0'; cursor++)
			;
		/* specify our encoding as we could be on an EBCDIC platform,
		 * we are assuming all strings we will need to format are in
		 * ASCII.
		 */
		try {
			return new String(stringBytes, 0, cursor, "US-ASCII");
		} catch (UnsupportedEncodingException e) {
			// US-ASCII will be supported
			return new String(stringBytes, 0, cursor);
		}
	}

	public String getASCIIString() {
		if (buffer == null) {
			try {
				/* 20's a guess as to a good buffer size:
				 * small enough that it should have minimal impact on the amount of data
				 * head in memory at any given time
				 * large enough to significantly reduce the number of calls when processing
				 * a string.
				 */
				commit(20);
			} catch (BufferUnderflowException e) {
				if (buffer == null) {
					throw e;
				}
			}
		}

		/* first check to see if it's entirely present in the buffer, this
		 * should be true for the majority of cases
		 */
		int length = 0;

		do {
			int s = buffer.position();
			byte b = 1;
			byte bufferArray[] = buffer.array();
			for (; length < buffer.remaining() && b != '\0'; length++) {
				b = bufferArray[s+length];
			}

			if (b == '\0') {
				/* we've got a string, length includes terminating char */
				String value;
				try {
					value = new String(bufferArray, s, length-1, "US-ASCII");
				} catch (UnsupportedEncodingException e) {
					// US-ASCII will be supported
					value = new String(bufferArray, s, length-1);
				}
				buffer.position(s+length);
				return value;
			}
			
			try {
				commit(buffer.remaining() + 20);
			} catch (BufferUnderflowException e) {
				/*
				 * commit may have appended what raw data it has but still underflow the request.
				 * If we failed to load more data throw underflow
				 */
				if (length == buffer.remaining()) {
					throw e;
				}
			}
		} while(true);
	}

	/**
	 * Constructs a string. This has a prerequisite that the string is
	 * prefixed with a short specifying it's length.
	 * 
	 * @return
	 */
	public String getUTF8String() throws UnsupportedEncodingException {
		commit(1);
		int length = getShort();
		byte data[] = new byte[length];

		commit(length);
		get(data);


		String s = new String(data, "UTF-8");

		/* defect 143460: work around for java5 presenting some strings in pseudo utf16 */
		/* if we've an even number of bytes and there are nulls within the string, try utf-16 */
		if (data.length %2 == 0) {
			boolean swap = false;
			boolean utf16 = false;

			for (int i = 0; i < data.length - 1; i++) {
				if (data[i] == 0) {
					utf16 = true;
					if (i % 2 != 0) {
						swap = true;
					}
				}
			}
			
			if (utf16) {
				if (swap) {
					/* swap bytes at odd/even indexes*/
					for (int i = 0; i < data.length - 1; i+= 2) {
						byte b = data[i+1];
						data[i+1] = data[i];
						data[i] = b;
					}
				}
				s = new String(data, "UTF-16");
			}
		}
		/* end of work around for 143460 */

		return s;
	}

	/**
	 * reads the specified number of bytes and turns them in to a positive
	 * BigInteger
	 * 
	 * @param bytes
	 * @return
	 */
	public BigInteger getBigInteger(int bytes) {
		commit(bytes);

		byte data[] = new byte[bytes];

		buffer.get(data);
		if (byteOrder != ByteOrder.BIG_ENDIAN) {
			reverseBytes(data);
		}

		return new BigInteger(1, data);
	}

	public void get(byte[] dst) {
		commit(dst.length);
		buffer.get(dst);
	}

	public void get(byte[] dst, int offset, int length) {
		commit(length);
		buffer.get(dst, offset, length);
	}

	public ByteOrder order() {
		return byteOrder;
	}

	/**
	 * Default order is big endian and that's what you'll get if you pass
	 * null in.
	 * 
	 * @param order
	 */
	synchronized public void order(ByteOrder order) {
		if (order == null) {
			byteOrder = ByteOrder.BIG_ENDIAN;
		} else {
			byteOrder = order;
		}

		if (buffer != null) {
			buffer.order(byteOrder);
		}
	}

	synchronized public void setGuardBytes(int bytes) {
		this.guardBytes = bytes;
	}

	public int getGuardBytes() {
		return guardBytes;
	}

	/**
	 * This allows you to strip off the last n bytes from the stream so long
	 * as they've not been reached. This will remove data until it hits the
	 * beginning of the stream, then will throw an exception. If an
	 * exception is thrown the the stream will be completely empty, all data
	 * discarded.
	 * 
	 * @param bytes
	 *                - the number of bytes to remove
	 * @throws BufferUnderflowException
	 */
	synchronized public void truncate(int bytes) {
		Slice s;

		if (bytes < 0) {
			throw new IllegalArgumentException();
		}

		for (int i = rawData.size() - 1; i >= 0 && bytes > 0; i--) {
			int reduce = bytes;
			s = (Slice)rawData.get(i);
			bytes -= s.length;
			if (reduce < s.length) {
				/* we're shortening this slice */
				s.length -= reduce;
			} else {
				/* we're dropping this slice */
				rawData.remove(i);
			}
		}

		if (bytes > 0 && buffer != null) {
			int remove = bytes;
			bytes -= buffer.remaining();
			/* the data's in the buffer if present */
			if (bytes < 0) {
				buffer.limit(buffer.limit() - remove);
			} else {
				buffer = null;
			}
		}

		if (bytes > 0) {
			throw new BufferUnderflowException();
		}
	}

	/**
	 * This allows you to strip off the last n bytes from the stream so long
	 * as they've not been reached. This will remove data until it hits the
	 * beginning of the stream, then will return. The data that's truncated
	 * will be placed into the provided array, filling from the end (i.e. if
	 * there's insufficient data in the stream to fill the array then the
	 * initial bytes in the array will be zero.
	 * 
	 * @param bytes
	 *                - the number of bytes to remove
	 * @returns - the number of bytes truncated
	 */
	synchronized public int truncate(byte bytes[]) {
		Slice s;
		int remaining = bytes.length;

		for (int i = rawData.size() - 1; i >= 0 && remaining > 0; i--) {
			int reduce = remaining;
			s = (Slice)rawData.get(i);
			remaining -= s.length;
			if (reduce < s.length) {
				/* we're shortening this slice */
				s.length -= reduce;
				System.arraycopy(s.data, s.offset + s.length, bytes, 0, reduce);
			} else {
				/* we're dropping this slice */
				rawData.remove(i);
				System.arraycopy(s.data, s.offset, bytes, remaining, s.length);
			}
		}

		if (remaining > 0 && buffer != null) {
			int remove = remaining;
			remaining -= buffer.remaining();
			/* the data's in the buffer if present */
			if (remaining < 0) {
				buffer.limit(buffer.limit() - remove);
				System.arraycopy(buffer.array(), buffer.arrayOffset() + buffer.position() + buffer.remaining(), bytes, 0, remove);
			} else {
				System.arraycopy(buffer.array(), buffer.arrayOffset() + buffer.position(), bytes, remaining, buffer.remaining());
				buffer = null;
			}
		}

		if (remaining < 0) {
			guardBytes -= bytes.length;
			return bytes.length;
		}

		guardBytes -= bytes.length - remaining;
		return bytes.length - remaining;
	}

	public int remaining() {
		int remaining = 0;
		if (buffer != null) {
			remaining += buffer.remaining();
		}

		for (int i = 0; i < rawData.size(); i++) {
			Slice s = (Slice)rawData.get(i);
			remaining += s.length;
		}

		remaining -= guardBytes;

		return remaining;
	}

	public byte peek() throws BufferUnderflowException {
		byte dest[] = new byte[1];
		if (peek(dest) != 1) {
			throw new BufferUnderflowException();
		}

		return dest[0];
	}

	public int peek(byte[] dest) {
		boolean currentBlocking = blocking;
		byte data[];
		blocking = false;
		int bytes = dest.length;
		int offset = 0;

		try {
			commit(dest.length);
		} catch (BufferUnderflowException e) {
			/* not enough data, so find out how much we do have */
			bytes = buffer.remaining();
		}

		data = buffer.array();
		offset = buffer.arrayOffset() + buffer.position();
		System.arraycopy(data, offset, dest, 0, bytes);

		blocking = currentBlocking;
		return bytes;
	}

	public void skip(int bytes) {
		commit(bytes);
		buffer.position(buffer.position() + bytes);
	}

	/**
	 * This method, in addition to putByte, is negatively indexed, i.e. it
	 * indexes from the end of the data. This is primarily because it's hard
	 * to be sure where the start of the data is in relation to the bytes we
	 * know we can modify, i.e. those bytes that are guarded.
	 * 
	 * put(data, -1) will insert the bytes in data in the last but one
	 * position of the stream, moving the last byte in the stream out by
	 * data.length and increase the count of guarded bytes by data.length
	 * 
	 * @param data
	 *                - the data to insert
	 * @param index
	 *                - the negative index at which to insert, must be
	 *                within the guard bytes
	 */
	synchronized public void put(byte data[], int index) {
		if (index >= 0) {
			throw new IndexOutOfBoundsException("Value must be negative: " + index);
		} else if (index < -guardBytes) {
			throw new IndexOutOfBoundsException("Insufficient guard bytes to insert at index: " + index);
		}

		/* is this in the current buffer or raw data? */
		int distance = -index;
		int guardAhead = guardBytes - distance;
		Slice slice = null;

		for (int i = rawData.size() - 1; i >= 0; i--, slice = null) {
			slice = (Slice)rawData.get(i);
			if (distance <= slice.length) {
				break;
			} else {
				distance -= slice.length;
			}
		}

		/* did we find a slice? */
		if (slice == null) {
			/*
			 * maybe it's in the current buffer. Should be because
			 * of the guard bytes, but sanity check
			 */
			if (buffer == null || distance > buffer.remaining()) {
				throw new IndexOutOfBoundsException("Index references past the begining of the data");
			}

			/* create a new slice out of the guard bytes */
			int guardStart = buffer.arrayOffset() + buffer.limit() - distance - guardAhead;
			int guardLength = distance + guardAhead;

			/*
			 * we create a new slice out of just the guard bytes
			 * rather than try to compose the insert data as well
			 * because it's simpler to use the logic for if we'd
			 * found it in a slice rather than a buffer anyway
			 */
			byte newdata[] = new byte[guardLength];
			System.arraycopy(buffer.array(), guardStart, newdata, 0, guardLength);
			slice = new Slice(newdata, 0, newdata.length);
			rawData.add(0, slice);
			buffer.limit(buffer.limit() - guardLength);
		}

		/* we've got a slice to operate on */
		byte newdata[] = new byte[slice.length + data.length];

		/* copy the prolog bytes */
		if (slice.length > distance) {
			System.arraycopy(slice.data, slice.offset, newdata, 0, slice.length - distance);
		}
		System.arraycopy(data, 0, newdata, slice.length - distance, data.length);
		System.arraycopy(slice.data, slice.offset + slice.length - distance, newdata, slice.length - distance + data.length, distance);

		/* expand the guard bytes to cover the new data */
		guardBytes += data.length;
		slice.data = newdata;
		slice.offset = 0;
		slice.length = newdata.length;

	}

	/**
	 * This takes a negative index and puts byte b in the index specified
	 * working back from the end of the rawData as if it were one large
	 * array allowing negative addressing. Positive indexes throw index out
	 * of bounds. It is worth noting that an index of -1 means the last byte
	 * present, i.e. we are not zero indexed.
	 * 
	 * @param index
	 * @param b
	 *                - the original byte we're replacing
	 */
	synchronized public byte put(byte b, int index) {
		if (index >= 0) {
			throw new IndexOutOfBoundsException("Value must be negative: " + index);
		} else if (index < -guardBytes) {
			throw new IndexOutOfBoundsException("Insufficient guard bytes to insert at index: " + index);
		}

		int distance = -index;
		byte original;
		byte[] data;
		Slice slice = null;
		int offset = 0;

		for (int i = rawData.size() - 1; i >= 0; i--, slice = null) {
			slice = (Slice)rawData.get(i);
			if (distance <= slice.length) {
				break;
			} else {
				distance -= slice.length;
			}
		}

		/* did we find our slice? */
		if (slice != null) {
			/* we found our slice */
			data = slice.data;
			offset = slice.offset + slice.length - distance;
		} else {
			/* maybe it's in the current buffer */
			if (buffer != null && distance <= buffer.remaining()) {
				/* it's in the buffer */
				data = buffer.array();
				offset = buffer.arrayOffset() + buffer.position() + buffer.remaining() - distance;
			} else {
				throw new IndexOutOfBoundsException("Index references past the begining of the data");
			}
		}

		original = data[offset];
		data[offset] = b;
		return original;
	}

}
