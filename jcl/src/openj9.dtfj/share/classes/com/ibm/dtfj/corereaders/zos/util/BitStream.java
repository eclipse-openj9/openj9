/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2006, 2017 IBM Corp. and others
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
package com.ibm.dtfj.corereaders.zos.util;

import java.util.Random;
import java.io.Serializable;

/**
 * This class provides a mechanism for writing and reading numbers in a bit stream. In other
 * words you can for instance write the number 3 in 2 bits followed by the number 1 in 3 bits
 * and so on, then rewind the stream and read a 2 bit number followed by a 3 bit number etc.
 * The stream is implemented as an array of 32-bit ints (hereafter referred to as "words")
 * and methods are provided to move to the next word and to get and set the current index into
 * the array of words. This enables you to (eg) store a number of sequences of numbers in the same
 * BitStream and to store the index for each sequence externally somewhere.
 * <p>
 * In addition to writing and reading numbers of fixed length, this stream also supports
 * variable length numbers using a variety of encodings. A good overview of the encodings
 * used can be found <a href="http://www.csee.umbc.edu/~ian/irF02/lectures/05Compression-for-IR.pdf">here</a>.
 * This <a href="http://en.wikipedia.org/wiki/Universal_code">Wikipedia article</a> also has
 * some cool graphs showing the scalability of the encodings.
 * <p>
 * Note that all ints are treated as unsigned. You will still get back what you put in if you
 * write a negative number but if you're using encoded numbers then bear in mind that (eg) -1
 * will be output as 0xffffffff.
 * <p>
 * Todo: might be good to wrap this class round a generic Input/OutputStream.
 */

public final class BitStream implements Serializable {
    private int[] bits;
    /** Index into the bits array */
    private int index;
    /** Offset of bit in the current int */
    private int bitOffset;
    /** Indicates whether we are currently reading or writing */
    private boolean writing = true;
    /** The current int we are reading from */
    private int currentWord;

    /**
     * Create a new BitStream with a default size of 100 bytes.
     */
    public BitStream() {
        this(25);
    }

    /**
     * Create a new BitStream containing the given number of ints. The BitStream is
     * implemented as an array of ints which will grow if necessary.
     * @param numInts the initial size of the array of ints
     */
    public BitStream(int numInts) {
        bits = new int[numInts];
    }

    /**
     * Rewinds the stream ready for reading.
     */
    public void rewind() {
        index = 0;
        bitOffset = 0;
        writing = false;
        currentWord = bits[index];
    }

    /**
     * Resets the stream so that it may be used again for writing.
     */
    public void reset() {
        rewind();
        bits[0] = 0;
        writing = true;
    }

    /**
     * Write the number "n" into "len" bits in the current word.
     */
    private void writeIntInWord(int n, int len) {
        assert len > 0 : len;
        if (len < 32)
            n &= ((1 << len) - 1);
        int b = bits[index];
        b |= (n << (32 - (bitOffset + len)));
        bits[index] = b;
        bitOffset += len;
    }

    /**
     * Write a number into the given number of bits.
     * @param n the number to write
     * @param len the number of bits to write the number to
     */
    public void writeLongBits(long n, int len) {
        assert len < 64 : len;
        writeIntBits((int)(n >> 32), len - 32);
        writeIntBits((int)n, 32);
    }

    /**
     * Write a number into the given number of bits. Note that there is no requirement
     * that the given number be small enough to fit, the number written will be masked
     * appropriately.
     * @param n the number to write
     * @param len the number of bits to write the number to
     */
    public void writeIntBits(int n, int len) {
        assert len > 0 && len <= 32 : len;
        if (len < (32 - bitOffset)) {
            writeIntInWord(n, len);
        } else {
            int firstlen = 32 - bitOffset;
            int secondlen = len + bitOffset - 32;
            if (firstlen > 0) {
                writeIntInWord(n >>> secondlen, firstlen);
            }
            if (secondlen > 0) {
                nextWord();
                bits[index] = 0;
                writeIntInWord(n, secondlen);
            }
        }
    }

    /**
     * Move to the next full word boundary.
     */
    public void nextWord() {
        index++;
        if (index >= bits.length) {
            int[] tmp = new int[bits.length * 2];
            System.arraycopy(bits, 0, tmp, 0, bits.length);
            bits = tmp;
        } else if (writing)
            bits[index] = 0;
        currentWord = bits[index];
        bitOffset = 0;
    }

    /**
     * Returns the current word index
     */
    public int getIndex() {
        return index;
    }

    /**
     * Sets the word index to the given value. This also resets the bit offset to the
     * beginning of the word. This method must only be used when reading to start reading
     * a number stream from a saved position.
     */
    public void setIndex(int index) {
        assert !writing;
        this.index = index;
        currentWord = bits[index];
        bitOffset = 0;
    }

    /**
     * Reads an int in the current word from the given number of bits.
     */
    private int readIntInWord(int len) {
        int n = (currentWord >> (32 - (bitOffset + len)));
        if (len < 32)
            n &= ((1 << len) - 1);
        bitOffset += len;
        return n;
    }

    /**
     * Read an int from given number of bits.
     * @param len the number of bits to read the number from
     */
    public int readIntBits(int len) {
        assert len > 0 && len <= 32 : len;
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
                nextWord();
                second = readIntInWord(secondlen);
            }
            return (first << secondlen) | second;
        }
    }

    /**
     * Read a long from given number of bits.
     * @param len the number of bits to read the number from
     */
    public long readLongBits(int len) {
        assert len < 64 : len;
        if (len <= 32)
            return readIntBits(len);
        long upper = readIntBits(len - 32);
        assert upper >= 0 : upper;
        long lower = readIntBits(32);
        return (upper << 32) | (lower & 0xffffffffL);
    }

    /**
     * Returns the log2 of the given number.
     */
    public static int log2(int n) {
        int bit = 0;
        while ((n & 0xffffff00 ) != 0) {
            bit += 8;
            n >>>= 8;
        }
        return bit + log2bytes[n];
    }

    private static final int log2bytes[] = {
        -1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
    };

    /**
     * Outputs the unsigned integer using delta coding. Delta coding gives better results
     * than gamma when larger numbers are more frequent.
     * @return the number of bits used
     */
    public int writeDelta(int n) {
        int msb = log2(++n );
        int r = writeGamma(msb);
        if (msb != 0)
            writeIntBits(n, msb);
        return r + msb;
    }

    /**
     * Outputs the given unsigned integer using gamma coding. Gamma coding is good when you
     * know small numbers are much more frequent than large numbers.
     * @return the number of bits used
     */
    public int writeGamma(int n) {
        int msb = log2(++n);
        int r = writeUnary(msb);
        if (msb != 0)
            writeIntBits(n, msb);
        return r + msb;
    }

    /**
     * Outputs the given unsigned integer as a unary number. Unary numbers are good for
     * small integers.
     * @return the number of bits used
     */
    public int writeUnary(int n) {
        for (int i = 0; i < n; i++)
            writeIntBits(1, 1);
        writeIntBits(0, 1);
        return n + 1;
    }

    /**
     * Reads a delta coded integer.
     */
    public int readDelta() {
        int msb = readGamma();
        return msb == 0 ? 0 : ( (1 << msb) | readIntBits(msb) ) - 1;
    }

    /**
     * Reads a gamma coded integer.
     */
    public int readGamma() {
        int msb = readUnary();
        return msb == 0 ? 0 : ( (1 << msb) | readIntBits(msb) ) - 1;
    }

    final static int[] nextUnsetBit = new int[256];

    static {
        for (int i = 0; i < 256; i++) {
            nextUnsetBit[i] = -1;
            for (int j = 0; j < 8; j++) {
                if (((0x80 >> j) & i) == 0) {
                    nextUnsetBit[i] = j;
                    break;
                }
            }
        }
    }

    /**
     * Read a unary coded integer.
     */

    /* 
     * XXX inefficient - needs to be improved like this:
     *
     * for
     *     get next whole word (shift left current, shift right next, or)
     *     for each byte in word
     *         get count from lookup table and add to sum
     *         if count not 8
     *             break
     */
    public int readUnary() {
        int word = currentWord;
        int offset = bitOffset;
        int result = 0;
        while (offset <= 24) {
            int b = nextUnsetBit[(word >> (24 - offset)) & 0xff];
            if (b != -1) {
                bitOffset = offset + b + 1;
                return result + b;
            }
            offset += 8;
            result += 8;
        }
        for (;; result++, offset++) {
            if (offset == 32) {
                nextWord();
                word = currentWord;
                offset = 0;
            }
            if ((word & (0x80000000 >>> offset)) == 0) {
                bitOffset = offset + 1;
                return result;
            }
        }
    }

    /**
     * Write a number using variable byte code. This is good when you have lots of
     * medium size numbers.
     * @return the number of bits used
     */
    public int writeVariableByte(int n) {
        return writevb(n, true);
    }

    private int writevb(int n, boolean end) {
        int r = 0;
        if (n >= 0x80 || n < 0)
            r += writevb(n >>> 7, false);
        n &= 0x7f;
        if (end) n |= 0x80;
        writeIntBits(n, 8);
        return r + 8;
    }

    /**
     * Read a number using variable byte code.
     */
    public int readVariableByte() {
        for (int n = 0;; n <<= 7) {
            int b = readIntBits(8);
            n |= (b & 0x7f);
            if ((b & 0x80) != 0)
                return n;
        }
    }

    /**
     * Write a number using Golomb-Rice coding. Good all rounder if you choose
     * the right value of k, absolute rubbish otherwise!
     * @param n the unsigned number to write
     * @param k the number of bits to use for the remainder
     * @return the number of bits used
     */
    public int writeGolombRice(int n, int k) {
        int r = writeUnary(n >>> k);
        writeIntBits(n, k);
        return r + k;
    }

    /**
     * Read a number using Golomb-Rice coding.
     * @param k the number of bits to use for the remainder
     */
    public int readGolombRice(int k) {
        int n = readUnary() << k;
        return n | readIntBits(k);
    }

    /**
     * Give a rough estimate of how many bytes of storage we use. This is the actual storage
     * allocated so may be more that what is in use at any one time.
     */
    public int memoryUsage() {
        return bits.length * 4;
    }

    /**
     * This method is provided to test the BitStream. We need to change this to use unit
     * tests at some point.
     */
    public static void main(String[] args) {
        final int DELTA = 1;
        final int GAMMA = 2;
        final int FIXED_BITS = 3;
        final int VARIABLE_BYTE = 4;
        final int GOLOMB = 5;
        final int MAX = 5;
        final int COUNT = 8000;
        BitStream bs = new BitStream();
        Random rand = new Random(0);

        if (args.length != 0) {
            /* Print some stats to give an idea which encoding is best */
            for (int i = 0; i < 4000; i++) {
                System.out.println("" + i + ": gamma " + bs.writeGamma(i) + " delta " + bs.writeDelta(i) + " variable " + bs.writeVariableByte(i) + " g(2) " + bs.writeGolombRice(i, 2) + " g(3) " + bs.writeGolombRice(i, 3) + " g(5) " + bs.writeGolombRice(i, 5) + " g(7) " + bs.writeGolombRice(i, 7) + " g(8) " + bs.writeGolombRice(i, 8));
                bs.reset();
            }
            return;
        }

        for (int i = 0; i < 10; i++) {
            bs.reset();
            int[] trueNumbers = new int[COUNT];
            byte[] codes = new byte[COUNT];
            byte[] numBits = new byte[COUNT];
            for (int j = 0; j < COUNT; j++) {
                int bits = rand.nextInt(31);
                int max = 1 << bits;
                //int n = rand.nextInt(max);
                int n = rand.nextInt();
                trueNumbers[j] = n;
                codes[j] = (byte)(rand.nextInt(MAX) + 1);
                switch (codes[j]) {
                case DELTA:
                    System.out.println("doing " + j + " n " + n + " code " + codes[j]);
                    bs.writeDelta(n);
                    break;
                case GAMMA:
                    System.out.println("doing " + j + " n " + n + " code " + codes[j]);
                    bs.writeGamma(n);
                    break;
                case FIXED_BITS:
                    numBits[j] = (byte)(rand.nextInt(32) + 1);
                    bs.writeIntBits(n, numBits[j]);
                    int mask = (int)((1L << numBits[j]) - 1);
                    trueNumbers[j] &= mask;
                    System.out.println("doing " + j + " n " + n + " code " + codes[j] + " bits " + numBits[j]);
                    break;
                case VARIABLE_BYTE:
                    System.out.println("doing " + j + " n " + n + " code " + codes[j]);
                    bs.writeVariableByte(n);
                    break;
                case GOLOMB:
                    numBits[j] = (byte)(rand.nextInt(8) + 15);
                    System.out.println("doing " + j + " n " + n + " code " + codes[j] + " k " + numBits[j]);
                    bs.writeGolombRice(n, numBits[j]);
                    break;
                }
            }
            System.out.println("done write for " + i);
            bs.rewind();
            for (int j = 0; j < COUNT; j++) {
                int n = trueNumbers[j];
                int k;
                switch (codes[j]) {
                case DELTA:
                    k = bs.readDelta();
                    System.out.println("doing " + j + " k " + k + " code " + codes[j]);
                    if (k != n)
                        throw new Error("delta: expected " + n + " but got " + k);
                    break;
                case GAMMA:
                    k = bs.readGamma();
                    System.out.println("doing " + j + " k " + k + " code " + codes[j]);
                    if (k != n)
                        throw new Error("gamma: expected " + n + " but got " + k);
                    break;
                case FIXED_BITS:
                    k = bs.readIntBits(numBits[j]);
                    System.out.println("doing " + j + " k " + k + " code " + codes[j] + " bits " + numBits[j]);
                    if (k != n)
                        throw new Error("fixed bits: expected " + n + " but got " + k);
                    break;
                case VARIABLE_BYTE:
                    k = bs.readVariableByte();
                    System.out.println("doing " + j + " k " + k + " code " + codes[j]);
                    if (k != n)
                        throw new Error("variable byte: expected " + n + " but got " + k);
                    break;
                case GOLOMB:
                    k = bs.readGolombRice(numBits[j]);
                    System.out.println("doing " + j + " k " + k + " code " + codes[j]);
                    if (k != n)
                        throw new Error("golomb: expected " + n + " but got " + k);
                    break;
                }
            }
            System.out.println("done " + i);
        }
    }
}
