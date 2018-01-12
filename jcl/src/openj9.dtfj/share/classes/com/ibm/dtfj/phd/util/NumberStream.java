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

import java.util.NoSuchElementException;
import java.util.Random;

/**
 *  This class provides a stream of integers in a relatively compact format. The stream
 *  is first written to, then rewound and then read.
 */

public class NumberStream {
	private static final int TYPE_FIELD_BITS = 2;
	private static final int SIGN_FIELD_BITS = 1;
	private static final int REPEATED_DELTA = 0;
	private static final int REPEATED_DELTA_BITS = 32;
	private static final int SMALL_DELTA = 1;
	private static final int SMALL_DELTA_BITS = 11;
	private static final int MEDIUM_DELTA = 2;
	private static final int MEDIUM_DELTA_BITS = 31;
	private static final int LARGE_DELTA = 3;
	private static final int LARGE_DELTA_BITS = 63;
	BitStream bitStream = new BitStream();
	long lastn;
	int repeatCount;
	int elementCount;
	int readElementCount;
	static final boolean dbg = false;

	public NumberStream() {
	}

	public void setBitStream(BitStream bitStream) {
		this.bitStream = bitStream;
		repeatCount = 0;
		lastn = 0;
	}

	public void writeLong(long n) {
		if (dbg) System.err.println("\nwrite "+hex(n));
		elementCount++;
		long delta = n - lastn;
		if (delta == 0) {
			// Avoid overflow of repeat count field
			if (repeatCount == (1 << REPEATED_DELTA_BITS - 1) * 2 - 1) flush();
			repeatCount++;
			return;
		}
		flush();
		boolean isNegative = delta < 0;
		long saveDelta = delta;
		delta = Math.abs(delta);
		int type;
		if (delta < (1 << SMALL_DELTA_BITS)) {
			type = SMALL_DELTA;
			bitStream.writeIntBits(type, TYPE_FIELD_BITS);
			bitStream.writeIntBits(isNegative ? 1 : 0, SIGN_FIELD_BITS);    // sign bit
			bitStream.writeIntBits((int)delta, SMALL_DELTA_BITS);
		} else if (delta < (1L << MEDIUM_DELTA_BITS)) {
			type = MEDIUM_DELTA;
			bitStream.writeIntBits(type, TYPE_FIELD_BITS);
			bitStream.writeIntBits(isNegative ? 1 : 0, SIGN_FIELD_BITS);    // sign bit
			bitStream.writeIntBits((int)delta, MEDIUM_DELTA_BITS);
		} else {
			type = LARGE_DELTA;
			bitStream.writeIntBits(LARGE_DELTA, TYPE_FIELD_BITS);
			bitStream.writeIntBits(isNegative ? 1 : 0, SIGN_FIELD_BITS);    // sign bit
			bitStream.writeLongBits(delta, LARGE_DELTA_BITS);
		}
		if (dbg) System.err.println("delta = " + Long.toHexString(saveDelta));
		if (dbg) System.err.println("type = " + type +" isNegative = " + isNegative + " delta = "+ hex(delta) + " n = "+hex(n));
		lastn = n;
	}

	public long readLong() {
		if (dbg) System.err.println("\nread");
		if (!hasMore()) {
			throw new NoSuchElementException();
		}
		readElementCount--;
		if (repeatCount != 0) {
			if (dbg) System.err.println("next repeat count = " + repeatCount + " lastn = " + hex(lastn));
			repeatCount--;
			return lastn;
		}
		int type = bitStream.readIntBits(TYPE_FIELD_BITS);
		if (type == REPEATED_DELTA) {
			repeatCount = bitStream.readIntBits(REPEATED_DELTA_BITS);
			Assert(repeatCount != 0);
			if (dbg) System.err.println("init repeat count = " + repeatCount + " lastn = " + hex(lastn));
			repeatCount--;
			return lastn;
		}
		boolean isNegative = bitStream.readIntBits(SIGN_FIELD_BITS) != 0;
		long delta;
		if (type == SMALL_DELTA) {
			delta = bitStream.readIntBits(SMALL_DELTA_BITS);
			if (isNegative)
				delta = -delta;
		} else if (type == MEDIUM_DELTA) {
			delta = bitStream.readIntBits(MEDIUM_DELTA_BITS);
			if (isNegative)
				delta = -delta;
		} else {
			delta = bitStream.readLongBits(LARGE_DELTA_BITS);
			if (isNegative)
				delta = -delta;
		}
		lastn += delta;
		if (dbg) System.err.println("delta = " + hex(delta) + " lastn = " + hex(lastn));
		if (dbg) System.err.println("type = " + type);
		return lastn;
	}

	public void flush() {
		if (repeatCount != 0) {
			int type = REPEATED_DELTA;
			bitStream.writeIntBits(type, TYPE_FIELD_BITS);
			bitStream.writeIntBits(repeatCount, REPEATED_DELTA_BITS);
			if (dbg) System.err.println("type = " + type +" count = " + repeatCount);
			repeatCount = 0;
		}
	}

	void reset() {
		flush();
		bitStream.reset();
		repeatCount = 0;
		lastn = 0;
	}

	public void rewind() {
		reset();
		readElementCount = elementCount;
		if (dbg) System.err.println("rewind complete, elementCount = " + readElementCount);
	}

	public void clear() {
		rewind();
		bitStream.clear();
		readElementCount = elementCount = 0;
		if (dbg) System.err.println("clear");
	}

	public boolean hasMore() {
		return readElementCount != 0;
	}

	public int elementCount() {
		return elementCount;
	}

	public int[] toIntArray() {
		rewind();
		int[] array = new int[elementCount()];
		for (int i = 0; i < elementCount(); i++) {
			array[i] = (int)readLong();
		}
		return array;
	}

	public long[] toLongArray() {
		rewind();
		long[] array = new long[elementCount()];
		for (int i = 0; i < elementCount(); i++) {
			array[i] = readLong();
		}
		return array;
	}

	int memoryUsage() {
		return bitStream.memoryUsage();
	}

	static void Assert(boolean condition) {
		if (!condition)
			throw new Error("assert failed");
	}

	public static void main(String args[]) {
		int total = 0;
		long totalSize = 0L;
		long totalUsedSize = 0L;
		boolean sort = false;
		NumberStream ns = sort ? new SortedNumberStream() : new BufferedNumberStream();
		// Set up random number generator here so that multiple runs
		// have slightly different numbers
		Random r = new Random(1);
		for (int n = 0; n < 100; n++) {
			System.out.println("pass " + n);
			LongArray la = new LongArray();
			int count = 0x4000;
			ns.clear();

			for (int i = 0; i < count; i++) {
				long l = r.nextLong();
				//System.out.println("writeLong " + Long.toHexString(l));
				ns.writeLong(l);
				la.add(l);
				l += r.nextInt();
				//System.out.println("writeLong " + Long.toHexString(l));
				ns.writeLong(l);
				la.add(l);
				l += r.nextInt(0x800);
				//System.out.println("writeLong " + Long.toHexString(l));
				ns.writeLong(l);
				la.add(l);
				l -= r.nextInt(0x800);
				//System.out.println("writeLong " + Long.toHexString(l));
				ns.writeLong(l);
				la.add(l);
				// Add some 0s
				for (int j = 0; j < r.nextGaussian(); ++j) {
					l = 0;
					//System.out.println("writeLong " + Long.toHexString(l));
					ns.writeLong(l);
					la.add(l);
				}
				if ((i % 0x100) == 0) {
					for (int j = 0; j < i; j++) {
						//System.out.println("writeLong " + Long.toHexString(l));
						ns.writeLong(l);
						la.add(l);
					}
				}
			}

			//ns.bitStream.compact();
			ns.rewind();
			if (sort)
				la.sort();

			for (int i = 0; i < la.size(); i++) {
				long l = ns.readLong();
				if (l != la.get(i)) {
					System.err.println("At " + i + " expected 0x" + Long.toHexString(la.get(i)) + " got " + Long.toHexString(l));
					System.exit(1);
				}
			}

			totalSize += ns.memoryUsage();
			totalUsedSize += ns.bitStream.getOffset()*4;
			total += la.size();
		}
		System.out.println("Test succeeded for " + total + " pairs");
		System.out.println("Items "+total+", used bits per item "+(totalUsedSize)*8.0/total);
		ns.clear();
		final long VAL = 0x1234567890L;
		long v = VAL;
		final long MAXREPEAT = Math.min(0x80000000L, (1L << REPEATED_DELTA_BITS) + 1);
		for (long li = -2; li < MAXREPEAT; ++li) {
			if (li % 100000000L == 0) System.out.println("Write repeat "+li);
			ns.writeLong(li == -2 ? 0 : v);
		}
		ns.writeLong(0);
		//ns.bitStream.compact();
		ns.rewind();
		for (long li = -2; li < MAXREPEAT; ++li) {
			if (li % 100000000L == 0) System.out.println("Read repeat "+li);
			long l = ns.readLong();
			v = li == -2 ? 0 : VAL;
			if (l != v) {
				System.err.println("At " + li + " expected 0x" + Long.toHexString(v) + " got " + Long.toHexString(l));
				System.exit(1);
			}
		}
		v = 0;
		long l = ns.readLong();
		if (l != v) {
			System.err.println("At end expected 0x" + Long.toHexString(v) + " got " + Long.toHexString(l));
			System.exit(1);
		}
		if (ns.hasMore()) {
			l = ns.readLong();
			System.err.println("At end expected no more, got " + Long.toHexString(l));
			System.exit(1);
		}
		totalSize += ns.memoryUsage();
		totalUsedSize += ns.bitStream.getOffset()*4;
		total += MAXREPEAT + 3;
		System.out.println("Test succeeded for " + MAXREPEAT + " repeated values");
		System.out.println("Memory usage "+totalSize+" bytes, used bytes "+totalUsedSize);
	}

	static String hex(long i) {
		return Long.toHexString(i);
	}
}
