/*******************************************************************************
 * Copyright (c) 2006, 2013 IBM Corp. and others
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

package com.ibm.j9ddr.corereaders.tdump.zebedee.util;

import java.io.*;

/**
 * This class represents an array of records which are stored in a compressed format whilst
 * still allowing random access to them. Each record in turn is simply an array of ints. Each
 * record must be the same length. To implement this we divide the array of records up into
 * blocks. There is an index and a bit stream. The index gives the start of each block in the
 * bit stream. Each block contains a set of records stored in an encoded format. A header at
 * the beginning defines the encoding used. The encoding is chosen dynamically to give the
 * best compression. Deltas (ie the differences between values in adjacent records) are stored
 * rather than the values themselves which gives good results for certain types of data.
 * The number of records per block is
 * configurable and there is a space/time trade-off to be made because a large number of
 * records per block will give better compression at the cost of more time to extract each
 * record (because you have to start at the beginning of the block and then uncompress each
 * record in turn until you reach the one you want).
 * <p>
 * I wrote a test to measure the performance on some real life data
 * (in fact this data is the reason I wrote this class in the first place). The data
 * consists of a file containing z/OS fpos_t objects obtained by calling fgetpos
 * sequentially for every block (4060 bytes) in an svcdump. Each fpos_t object is
 * actually an array of 8 ints containing obscure info about the disk geometry or
 * something, but the important thing is that it changes in a reasonably regular
 * fashion and so is a good candidate for compression via deltas. The original file
 * had a length of 3401088. Here are the results which suggest that a block size of
 * 32 (log2 of 5) is a good choice (the time is that taken to write the data and then read it
 * back again to check):
 * <p>
 * <table border="1"><tr><th>log2</th><th>block size</th><th>memory usage</th><th>time (ms)</th></tr>
 * <tr><td>0</td><td>1</td><td>4191388</td><td>782</td></tr>
 * <tr><td>1</td><td>2</td><td>2706992</td><td>691</td></tr>
 * <tr><td>2</td><td>4</td><td>1217920</td><td>621</td></tr>
 * <tr><td>3</td><td>8</td><td>790472</td><td>620</td></tr>
 * <tr><td>4</td><td>16</td><td>516772</td><td>721</td></tr>
 * <tr><td>5</td><td>32</td><td>340448</td><td>942</td></tr>
 * <tr><td>6</td><td>64</td><td>334304</td><td>1362</td></tr>
 * <tr><td>7</td><td>128</td><td>334304</td><td>2223</td></tr>
 * <tr><td>8</td><td>256</td><td>340448</td><td>3966</td></tr>
 * <tr><td>9</td><td>512</td><td>355808</td><td>7470</td></tr>
 * </table>
 * <p>
 *
 * @has - - - com.ibm.zebedee.util.BitStream
 */

public final class CompressedRecordArray implements Serializable {

    /** Log2 of the size of each block */
    private int blockSizeLog2;
    /** Size of each block */
    private int blockSize;
    /** Size of each record array */
    private int recordSize;
    /** The index into bitStreamIndex */
    private int index;
    /** Index into the bit stream */
    private int[] bitStreamIndex = new int[16];
    /** The current block we are writing into */
    private int[][] currentBlock;
    /** Index of record in this block */
    private int currentBlockIndex;
    /** The bit stream where most of the data is actually stored */
    private BitStream bits = new BitStream();
    /*
     * Following are all working variables that could be local but are made
     * global to save on GC.
     */
    private boolean[][] isNegative;
    private int[] lastValue;
    private int[] maxDelta;
    private boolean[] allSameDelta;
    private boolean[] allPositive;
    private byte[] encoding;
    /** Set to true when closed and ready for reading */
    private boolean closed = false;
    /** Total number of records written */
    private int numRecords;
    /* Possible encoding types: */
    private static final int GOLOMB2 = 0;
    private static final int GOLOMB7 = 1;
    private static final int GOLOMB8 = 2;
    private static final int VARIABLE_BYTE = 3;
    /* Following are for stat collecting only */
    private static int numGolomb2;
    private static int numGolomb7;
    private static int numGolomb8;
    private static int numVariableByte;
    private static int numAllSameDelta;
    private static int numNotAllSameDelta;
    private static int numNegative;
    private static int numAllPositive;

    /**
     * Create a new CompressedRecordArray. A size of 5 for blockSizeLog2 gives good results.
     * @param blockSizeLog2 the number of records in each block expressed as a power of 2
     * @param recordSize the number of ints in each record
     */
    public CompressedRecordArray(int blockSizeLog2, int recordSize) {
        this.blockSizeLog2 = blockSizeLog2;
        blockSize = 1 << blockSizeLog2;
        this.recordSize = recordSize;
        currentBlock = new int[blockSize][recordSize];
        isNegative = new boolean[blockSize][recordSize];
        lastValue = new int[recordSize];
        maxDelta = new int[recordSize];
        allSameDelta = new boolean[recordSize];
        allPositive = new boolean[recordSize];
        encoding = new byte[recordSize];
    }

    /**
     * Add a new record. Data is copied from the given array.
     * @param record an array of ints which forms the record to be added
     */
    public void add(int[] record) {
        assert !closed;
        for (int i = 0; i < recordSize; i++) {
            currentBlock[currentBlockIndex][i] = record[i];
        }
        if (++currentBlockIndex == blockSize) {
            flushCurrentBlock();
            currentBlockIndex = 0;
        }
        numRecords++;
    }

    /**
     * Close this CompressedRecordArray. This must be called before any reading is done
     * and no more records may be added afterwards.
     */
    public void close() {
        flushCurrentBlock();
        bits.rewind();
        closed = true;
    }

    /**
     * Flush the current block
     */
    private void flushCurrentBlock() {
        /*
         * Save the current index for this block
         */
        bitStreamIndex[index++] = bits.getIndex();
        if (index == bitStreamIndex.length) {
            int[] tmp = new int[index * 2];
            System.arraycopy(bitStreamIndex, 0, tmp, 0, index);
            bitStreamIndex = tmp;
        }
        /*
         * Output the compressed block to the bit stream
         */
        compressBlock();
        /*
         * Realign bit stream at the next word
         */
        bits.nextWord();
    }

    /**
     * Compress the current block of records
     */
    private void compressBlock() {
        /*
         * Normalize data by converting to deltas and then making numbers absolute. We also
         * gather some stats to help decide which encoding to use. We store deltas (ie the
         * difference between consecutive values) rather than the values themselves because
         * we are often storing structured data where a value is increasing (or decreasing)
         * with each record. If this is not the case, it does no harm to store deltas anyway.
         */
        for (int recordIndex = 0; recordIndex < recordSize; recordIndex++) {
            lastValue[recordIndex] = currentBlock[0][recordIndex];
            maxDelta[recordIndex] = 0;
            allSameDelta[recordIndex] = true;
            allPositive[recordIndex] = true;
            int lastDelta = 0;
            for (int blockIndex = 1; blockIndex < currentBlockIndex; blockIndex++) {
                int delta = currentBlock[blockIndex][recordIndex] - lastValue[recordIndex];
                if (delta < 0) {
                    /*
                     * Found a negative delta. Make absolute and also set the allPositive
                     * flag to indicate we need to store sign bits.
                     */
                    isNegative[blockIndex][recordIndex] = true;
                    delta = -delta;
                    allPositive[recordIndex] = false;
                    numNegative++;
                } else {
                    isNegative[blockIndex][recordIndex] = false;
                }
                if (delta > maxDelta[recordIndex])
                    maxDelta[recordIndex] = delta;
                if (blockIndex > 1 && delta != lastDelta) {
                    /*
                     * Not all deltas are the same so reset the flag.
                     */
                    allSameDelta[recordIndex] = false;
                    numNotAllSameDelta++;
                }
                /*
                 * Save the last value and also replace the current value with its delta.
                 */
                lastValue[recordIndex] = currentBlock[blockIndex][recordIndex];
                lastDelta = delta;
                currentBlock[blockIndex][recordIndex] = delta;
            }
            if (allPositive[recordIndex])
                numAllPositive++;
            if (allSameDelta[recordIndex])
                numAllSameDelta++;
        }
        /*
         * Now figure out what encoding to use and output the block header.
         */
        for (int recordIndex = 0; recordIndex < recordSize; recordIndex++) {
            /* We can skip the sign bit if all are positive */
            bits.writeIntBits(allPositive[recordIndex] ? 1 : 0, 1);
            /* We can skip most of the record altogether if all delta values are the same */
            bits.writeIntBits(allSameDelta[recordIndex] ? 1 : 0, 1);
            /* There's probably a more mathematical way but this will do for now */
            if (maxDelta[recordIndex] < 24) {
                encoding[recordIndex] = GOLOMB2;
                numGolomb2++;
            } else if (maxDelta[recordIndex] < 384) {
                encoding[recordIndex] = GOLOMB7;
                numGolomb7++;
            } else if (maxDelta[recordIndex] < 2048) {
                encoding[recordIndex] = GOLOMB8;
                numGolomb8++;
            } else {
                encoding[recordIndex] = VARIABLE_BYTE;
                numVariableByte++;
            }
            bits.writeIntBits(encoding[recordIndex], 2);
        }
        /*
         * Ok, now we can output the block data itself.
         */
        for (int blockIndex = 0; blockIndex < currentBlockIndex; blockIndex++) {
            for (int recordIndex = 0; recordIndex < recordSize; recordIndex++) {
                /* The first record always uses variable byte */
                if (blockIndex == 0) {
                    bits.writeVariableByte(currentBlock[blockIndex][recordIndex]);
                /* Only output remainder if not the same as first delta */
                } else if (blockIndex == 1 || !allSameDelta[recordIndex]) {
                    /* Output the sign bit if any */
                    if (!allPositive[recordIndex]) {
                        bits.writeIntBits(isNegative[blockIndex][recordIndex] ? 1 : 0, 1);
                    }
                    switch (encoding[recordIndex]) {
                    case GOLOMB2:
                        bits.writeGolombRice(currentBlock[blockIndex][recordIndex], 2);
                        break;
                    case GOLOMB7:
                        bits.writeGolombRice(currentBlock[blockIndex][recordIndex], 7);
                        break;
                    case GOLOMB8:
                        bits.writeGolombRice(currentBlock[blockIndex][recordIndex], 8);
                        break;
                    case VARIABLE_BYTE:
                        bits.writeVariableByte(currentBlock[blockIndex][recordIndex]);
                        break;
                    }
                }
            }
        }
    }

    /**
     * Get the given record number. To save on GC overhead the user supplies the
     * int array to copy the record into.
     * @param recordNumber the sequential number of the record to read
     * @param record the array to copy the record into
     */
    public void get(int recordNumber, int[] record) {
        assert closed ;
        assert recordNumber < numRecords;
        /*
         * First position the bit stream at the block header
         */
        index = recordNumber >> blockSizeLog2;
        bits.setIndex(bitStreamIndex[index]);

        /* Get the index within the block */
        int desiredBlockIndex = recordNumber & (blockSize - 1);

        /*
         * Read the block header containing flags plus the encoding.
         */
        for (int recordIndex = 0; recordIndex < recordSize; recordIndex++) {
            allPositive[recordIndex] = (bits.readIntBits(1) == 1);
            allSameDelta[recordIndex] = (bits.readIntBits(1) == 1);
            encoding[recordIndex] = (byte)bits.readIntBits(2);
        }

        /* 
         * Now scan through decompressing as we go until we reach the desired record 
         */
        for (int blockIndex = 0; blockIndex <= desiredBlockIndex; blockIndex++) {
            for (int recordIndex = 0; recordIndex < recordSize; recordIndex++) {
                /* The first record always uses variable byte */
                if (blockIndex == 0) {
                    currentBlock[blockIndex][recordIndex] = bits.readVariableByte();
                /* Only read remainder if not the same as first delta */
                } else if (blockIndex == 1 || !allSameDelta[recordIndex]) {
                    int delta = 0;
                    boolean isNegative = false;
                    /* Input the sign bit if any */
                    if (!allPositive[recordIndex]) {
                        isNegative = (bits.readIntBits(1) == 1);
                    }
                    /*
                     * Read the delta using the appropriate encoding.
                     */
                    switch (encoding[recordIndex]) {
                    case GOLOMB2:
                        delta = bits.readGolombRice(2);
                        break;
                    case GOLOMB7:
                        delta = bits.readGolombRice(7);
                        break;
                    case GOLOMB8:
                        delta = bits.readGolombRice(8);
                        break;
                    case VARIABLE_BYTE:
                        delta = bits.readVariableByte();
                        break;
                    }
                    /* Apply sign bit */
                    if (isNegative)
                        delta = -delta;
                    /* Add delta to previous value to restore original value */
                    currentBlock[blockIndex][recordIndex] = currentBlock[blockIndex-1][recordIndex] + delta;
                    /* Reuse the lastValue array to hold the last delta */
                    lastValue[recordIndex] = delta;
                } else {
                    /*
                     * All the deltas are the same from the second record on so just reuse
                     * the delta we saved in lastValue.
                     */
                    currentBlock[blockIndex][recordIndex] = currentBlock[blockIndex-1][recordIndex] + lastValue[recordIndex];
                }
            }
        }
        /*
         * Finally copy the restored record into the supplied array.
         */
        for (int i = 0; i < recordSize; i++) {
            record[i] = currentBlock[desiredBlockIndex][i];
        }
    }

    /**
     * Give a rough estimate of how many bytes of storage we use. This is the actual storage
     * allocated so may be more that what is in use at any one time.
     */
    public int memoryUsage() {
        return bits.memoryUsage() + (bitStreamIndex.length * 4) + (blockSize * recordSize * 4 * 2)
               + (recordSize * (4 + 4 + 1 + 1 + 1));
    }

    /**
     * This method is provided to test the CompressedRecordArray. We need to change this to use
     * unit tests at some point.
     * @exclude
     * @throws Exception if anything bad happens
     */
    public static void main(String[] args) throws Exception {
        if (args.length != 0) {
            fpostest(args[0]);
            return;
        }
        for (int blockSizeLog2 = 0; blockSizeLog2 < 10; blockSizeLog2++) {
            int blockSize = 1 << blockSizeLog2;
            for (int recordSize = 1; recordSize < 20; recordSize++) {
                System.out.println("doing blockSize " + blockSize + " recordSize " + recordSize);
                CompressedRecordArray ra = new CompressedRecordArray(blockSizeLog2, recordSize);
                int[] record = new int[recordSize];
                for (int i = 0; i < blockSize*10 + recordSize; i++) {
                    for (int j = 0; j < recordSize; j++) {
                        int value = i*i*j;
                        if ((recordSize % 3) == 0 && (j % 3) == 0)
                            // this should give all same deltas
                            value = i * blockSize * recordSize;
                        else if ((j % 5) == 0)
                            value = -value;
                        record[j] = value;
                    }
                    ra.add(record);
                }
                ra.close();
                for (int i = 0; i < blockSize*10 + recordSize; i++) {
                    ra.get(i, record);
                    for (int j = 0; j < recordSize; j++) {
                        int value = i*i*j;
                        if ((recordSize % 3) == 0 && (j % 3) == 0)
                            // this should give all same deltas
                            value = i * blockSize * recordSize;
                        else if ((j % 5) == 0)
                            value = -value;
                        if (record[j] != value)
                            throw new Error("found " + record[j] + " expected " + value);
                    }
                }
            }
        }
        System.out.println("numGolomb2 = " + numGolomb2);
        System.out.println("numGolomb7 = " + numGolomb7);
        System.out.println("numGolomb8 = " + numGolomb8);
        System.out.println("numVariableByte = " + numVariableByte);
        System.out.println("numAllSameDelta = " + numAllSameDelta);
        System.out.println("numNotAllSameDelta = " + numNotAllSameDelta);
        System.out.println("numNegative = " + numNegative);
        System.out.println("numAllPositive = " + numAllPositive);
    }

    /**
     * This is the test method I wrote to measure the performance on some real life data.
     * See the discussion in the class overview for more info.
     * @throws Exception if the file can't be found
     */
    private static void fpostest(String filename) throws Exception {
        File file = new File(filename);
        byte[] bytes = new byte[(int)file.length()];
        FileInputStream fis = new FileInputStream(file);
        BufferedInputStream bis = new BufferedInputStream(fis);
        DataInputStream dis = new DataInputStream(bis);
        dis.readFully(bytes);
        ByteArrayInputStream bais = new ByteArrayInputStream(bytes);
        dis = new DataInputStream(bais);
        int[] record = new int[8];
        for (int blockSizeLog2 = 0; blockSizeLog2 < 10; blockSizeLog2++) {
            dis.reset();
            long then = System.currentTimeMillis();
            CompressedRecordArray ra = new CompressedRecordArray(blockSizeLog2, 8);
            boolean eof = false;
            for (int r = 0;; r++) {
                for (int i = 0; i < 8; i++) {
                    try {
                        record[i] = dis.readInt();
                    } catch (EOFException e) {
                        eof = true;
                        break;
                    }
                }
                if (eof) break;
                ra.add(record);
            }
            ra.close();
            dis.reset();
            eof = false;
            for (int r = 0;; r++) {
                for (int i = 0; i < 8; i++) {
                    try {
                        int n = dis.readInt();
                        if (i == 0)
                            ra.get(r, record);
                        if (record[i] != n)
                            throw new Error("data mismatch!");
                    } catch (EOFException e) {
                        eof = true;
                        break;
                    }
                }
                if (eof) break;
            }
            long now = System.currentTimeMillis();
            System.out.println("block size " + (1 << blockSizeLog2) + " uses " + ra.memoryUsage() + " took " + (now - then) + " ms");
        }
    }
}
