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

/**
 *  Similar to IntegerArray but for longs.
 *
 *  @see <a href="http://w3.hursley.ibm.com/~dgriff/">Dave Griffiths home page</a>
 */

public final class LongArray extends PrimitiveArray {
    long chunks[][] = new long[1][CHUNK_SIZE];

    public LongArray() {}

    public LongArray(int length, long value) {
        for (int i = 0; i < length; i++)
            add(value);
    }

    public Object clone() {
        LongArray clone = new LongArray();
        clone.chunks = new long[chunks.length][CHUNK_SIZE];
        for (int i = 0; i < chunks.length; i++) {
            System.arraycopy(chunks[i], 0, clone.chunks[i], 0, CHUNK_SIZE);
        }
        clone.chunkOffset = chunkOffset;
        clone.size = size;
        return clone;
    }

    public long[] toArray() {
        long a[] = new long[size()];

        for (int i = 0; i < chunks.length; i++) {
            System.arraycopy(chunks[i], 0, a, i << shift, i == chunks.length - 1 ? chunkOffset : CHUNK_SIZE);
        }
        return a;
    }

    public void add(long value) {
        if (chunkOffset >= CHUNK_SIZE) {
            long newchunks[][] = new long[chunks.length + 1][];

            for (int i = 0; i < chunks.length; i++) {
                newchunks[i] = chunks[i];
            }
            newchunks[chunks.length] = new long[CHUNK_SIZE];
            chunks = newchunks;
            chunkOffset = 0;
        }
        chunks[chunks.length - 1][chunkOffset++] = value;
        size++;
    }

    public int size() {
        return size;
    }

    long lget(int index) {
        if (index >= size) throw new Error("Attempt to get " + index + " greater than size " + size);
        return chunks[index >> shift][index & mask];
    }

    public long get(int index) {
        return lget(index);
    }

    void lput(int index, long value) {
        if (index >= size) throw new Error("Attempt to put " + index + " greater than size " + size);
        chunks[index >> shift][index & mask] = value;
    }

    public void put(int index, long value) {
        lput(index, value);
    }

    public void add(int index, long value) {
        for (int i = 0; i < index + 1 - size; i++) {
            add(0);
        }
        put(index, value);
    }

    public void putAll(long value) {
        for (int i = 0; i < size; i++)
            put(i, value);
    }

    public int memoryUsage() {
        return chunks.length * CHUNK_SIZE * 8;
    }
}
