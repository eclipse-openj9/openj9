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

/**
 *  This class provides a mechanism for writing numbers in a bit stream.
 */

public final class BitStream {
	
	private int[] bits = new int[100];
	private int wordOffset;
	private int bitOffset;
	private static final boolean dbg = false;

	void reset() {
		wordOffset = 0;
		bitOffset = 0;
	}

	public void rewind() {
		reset();
	}

	public void clear() {
		reset();
		// Reset the array size if we are using more than 1Mb.
		if (bits.length > 262144) {
			bits = new int[100];
		}
		bits[0] = 0;
	}

	private void writeIntInWord(int n, int len) {
		if (len < 32) {
			n &= ((1 << len) - 1);
		}
		if (len <= 0) {
			throw new Error("bad length: " + len);
		}

		int b = bits[wordOffset];
		if (dbg) System.out.println("IntInWord: n = " + hex(n) + " len = " + len + " word offset = " + wordOffset + " old bits = " + hex(b));
		b |= (n << (32 - (bitOffset + len)));

		bits[wordOffset] = b;
		bitOffset += len;
	}

	public void writeLongBits(long n, int len) {
		Assert(len < 64);
		writeIntBits((int)(n >> 32), len - 32);
		writeIntBits((int)n, 32);
	}

	public void writeIntBits(int n, int len) {
		if (len > 32 || len < 0) throw new Error("bad length: " + len);
		if (len < (32 - bitOffset)) {
			writeIntInWord(n, len);
		} else {
			int firstlen = 32 - bitOffset;
			int secondlen = len + bitOffset - 32;
			if (firstlen > 0) {
				writeIntInWord(n >>> secondlen, firstlen);
			}
			if (secondlen > 0) {
				nextWord(true);
				bits[wordOffset] = 0;
				writeIntInWord(n, secondlen);
			}
		}
	}

	public void writeIntBits(int n, int len, int wordOff, int bitOff) {
		int saveWordOffset = wordOffset;
		int saveBitOffset = bitOffset;
		wordOffset = wordOff;
		bitOffset = bitOff;
		writeIntBits(n, len);
		wordOffset = saveWordOffset;
		bitOffset = saveBitOffset;
	}

	public void nextWord(boolean write) {
		wordOffset++;
		if (wordOffset >= bits.length) {
            int[] tmp = new int[(bits.length * 3 + 1) / 2];
			System.arraycopy(bits, 0, tmp, 0, bits.length);
			bits = tmp;
		} else if (write) {
			bits[wordOffset] = 0;
		}
		bitOffset = 0;
	}

	public void compact() {
		int[] tmp = new int[wordOffset+1];
		System.arraycopy(bits, 0, tmp, 0, tmp.length);
		bits = tmp;
	}

	public int getOffset() {
		return wordOffset;
	}

	public void setOffset(int offset) {
		wordOffset = offset;
		bitOffset = 0;
	}

	private int readIntInWord(int len) {
		Assert(len > 0);
		int n = (bits[wordOffset] >> (32 - (bitOffset + len)));
		if (len < 32)
			n &= ((1 << len) - 1);
		bitOffset += len;
		if (dbg) System.err.println("IntInWord: n = " + Integer.toHexString(n) + " len = " + len);
		return n;
	}

	public int readIntBits(int len) {
		if (len > 32 || len <= 0) { 
			throw new Error("bad length: " + len);
		}
		if (len < (32 - bitOffset)) {
			return readIntInWord(len);
		} else {
			int firstlen = 32 - bitOffset;
			int secondlen = len + bitOffset - 32;
			int first = 0;
			if (firstlen > 0) {
				first = readIntInWord(firstlen);
			}
			int second = 0;
			if (secondlen > 0) {
				nextWord(false);
				second = readIntInWord(secondlen);
			}
			return (first << secondlen) | second;
		}
	}

	public long readLongBits(int len) {
		Assert(len < 64);
		if (len <= 32) {
			return readIntBits(len);
		}
		long upper = readIntBits(len - 32);
		Assert(upper >= 0);
		long lower = readIntBits(32);
		return (upper << 32) | (lower & 0xffffffffL);
	}

	public int readIntBits(int len, int wordOff, int bitOff) {
		int saveWordOffset = wordOffset;
		int saveBitOffset = bitOffset;
		wordOffset = wordOff;
		bitOffset = bitOff;
		int r = readIntBits(len);
		wordOffset = saveWordOffset;
		bitOffset = saveBitOffset;
		return r;
	}

	public int memoryUsage() {
		return bits.length * 4;
	}

	static void Assert(boolean condition) {
		if (!condition) {
			throw new Error("assert failed");
		}
	}

	static String hex(long i) {
		return Long.toHexString(i);
	}
}
