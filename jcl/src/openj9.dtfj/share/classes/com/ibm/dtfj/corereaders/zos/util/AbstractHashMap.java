/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

import java.io.Serializable;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 * This is the superclass of a variety of subclasses which provide hash map functionality
 * using integer keys rather than objects. This class uses open addressing with double hashing
 * and has a reasonably small footprint.
 * @depend - - - com.ibm.dtfj.corereaders.zos.util.IntEnumeration
 */

public abstract class AbstractHashMap implements Serializable {

    static final int INITIAL_SIZE = 17;
    /** The array of keys */
    long[] keys = new long[INITIAL_SIZE];
    /** The length of the keys array */
    int tableSize = INITIAL_SIZE;
    /** The number of slots currently in use */
    int slotsInUse;
    /** The number of deleted slots */
    int deletedSlots;
    /** The state of each slot */
    byte[] state = new byte[INITIAL_SIZE];
    static final byte EMPTY = 0;
    static final byte OCCUPIED = 1;
    static final byte DELETED = 2;
    /** Doing rehash */
    private boolean inRehash;
    /** Logger */
    private static Logger log = Logger.getLogger(AbstractHashMap.class.getName());
    private static final boolean debug = log.isLoggable(Level.FINER);
    private static int callsToGetIndex;
    private static int callsToGetIndexSuccess;
    private static int loopsInGetIndex;
    private static int loopsBecauseNotEqual;
    private static int loopsBecauseDeleted;
    private static int callsToRehash;

    /**
     * This method must be overridden by subclasses to return the array of values. Used
     * during rehashing.
     * @return the array of values
     */
    abstract Object getValuesArray();

    /**
     * This method must be overridden by subclasses to allocate a new empty array of values
     * of the given size. Used during rehashing.
     * @param newSize the size of the value array to allocate
     */
    abstract void allocNewValuesArray(int newSize);

    /**
     * This method must be overridden by subclasses which must then do a put operation for
     * the given key and the value at the given offset in the oldvalues array. It is used
     * during rehashing to rebuild the map.
     * @param key the key to do the put on
     * @param oldvalues the old array of values
     * @param offset the offset into the oldvalues array
     */
    abstract void put(long key, Object oldvalues, int offset);

    /**
     * Rebuild the hash table.
     */
    void rehash() {
        inRehash = true;
        if (debug) {
            callsToRehash++;
            log.finer("rehashing, current size " + tableSize + " slots in use " + slotsInUse + " deleted " + deletedSlots);
            printStats();
        }
        /* Save the old fields */
        long[] oldkeys = keys;
        Object oldvalues = getValuesArray();
        byte[] oldstate = state;
        if (tableNeedsResize()) {
            /* The new table size must be a prime number */
            tableSize <<= 1;
            while (!isprime(++tableSize));
            if (debug) log.finer("new table size is " + tableSize);
        }
        /* Allocate the new fields */
        keys = new long[tableSize];
        allocNewValuesArray(tableSize);
        state = new byte[tableSize];
        /* Repopulate */
        slotsInUse = 0;
        deletedSlots = 0;
        for (int i = 0; i < oldkeys.length; i++) {
            if (oldstate[i] == OCCUPIED) {
                put(oldkeys[i], oldvalues, i);
            }
        }
        inRehash = false;
    }

    /**
     * Get the index for the given key.
     * @return the index or -1 if it cannot be found
     */
    int getIndex(long key) {
        if (debug) callsToGetIndex++;
        for (int i = 0; i < tableSize; i++) {
            int index = h(key & 0x7fffffffffffffffL, i);
            if (state[index] == EMPTY) {
                return -1;
            } else if (state[index] == OCCUPIED && keys[index] == key) {
                if (debug) callsToGetIndexSuccess++;
                return index;
            }
            if (debug) {
                loopsInGetIndex++;
                if (state[index] == OCCUPIED)
                    loopsBecauseNotEqual++;
                else
                    loopsBecauseDeleted++;
            }
        }
        return -1;
    }

    /**
     * Log some useful stats
     */
    private static void printStats() {
        log.finer("callsToGetIndex = " + callsToGetIndex);
        log.finer("callsToGetIndexSuccess = " + callsToGetIndexSuccess);
        log.finer("loopsInGetIndex = " + loopsInGetIndex);
        log.finer("loopsBecauseNotEqual = " + loopsBecauseNotEqual);
        log.finer("loopsBecauseDeleted = " + loopsBecauseDeleted);
        log.finer("callsToRehash = " + callsToRehash);
    }

    /**
     * Remove the given key from the map and return its old index.
     * @return the index or -1 if it cannot be found
     */
    int removeIndex(long key) {
        int index = getIndex(key);
        if (index != -1) {
            state[index] = DELETED;
            deletedSlots++;
            slotsInUse--;
        }
        return index;
    }

    /**
     * Does the table need resizing? This occurs if the table is more than 2/3 full.
     */
    private boolean tableNeedsResize() {
        return slotsInUse > ((tableSize * 2) / 3);
    }

    /**
     * Check to see if we need to rehash. This occurs when the table is more than 2/3 full.
     */
    void checkRehash() {
        if (tableNeedsResize() || deletedSlots > (tableSize / 3)) {
            if (!inRehash)
                rehash();
        }
    }

    /**
     * Return the index to be used for the next put operation. Similar to getIndex except that
     * it marks the slot as occupied and increments the use count.
     * @return the index to be used for this put operation
     */
    int putIndex(long key) {
        for (int i = 0; i < tableSize; i++) {
            int index = h(key & 0x7fffffffffffffffL, i);
            if (state[index] != OCCUPIED) {
                keys[index] = key;
                if (state[index] == DELETED)
                    deletedSlots--;
                state[index] = OCCUPIED;
                ++slotsInUse;
                return index;
            } else if (keys[index] == key) {
                return index;
            }
        }
        throw new Error("table full! key = " + key + " tableSize = " + tableSize + " slotsInUse = " + slotsInUse);
    }

    /**
     * Internal class used to implement IntEnumeration.
     * @hidden
     */
    private class KeyEnum implements IntEnumeration {

        int index = 0;
        boolean hasMore = true;

        KeyEnum() {
            reset();
        }

        void next() {
            while (index < tableSize && state[index] != OCCUPIED) index++;
            if (index == tableSize) hasMore = false;
        }

        public boolean hasMoreElements() {
            return hasMore;
        }

        public Object nextElement() {
            return Long.valueOf(nextInt());
        }

        public long nextInt() {
            long key = keys[index++];
            next();
            return key;
        }

        public long peekInt() {
            return keys[index];
        }

        public void reset() {
            index = 0;
            hasMore = true;
            next();
        }
    }

    /**
     * Return an enumeration of the keys in this map. {@link IntEnumeration} is similar to
     * Enumeration except that it also provides a nextInt() method to return a primitive value.
     */
    public IntEnumeration getKeys() {
        return new KeyEnum();
    }

    /**
     * Return an array containing the keys.
     */
    public long[] getKeysArray() {
        long[] akeys = new long[slotsInUse];
        for (int i = 0, j = 0; i < tableSize; i++) {
            if (state[i] == OCCUPIED)
                akeys[j++] = keys[i];
        }
        return akeys;
    }

    /**
     * The double hashing method.
     */
    private int h(long k, int i) {
        long l = h1(k) + ((long)i * (long)h2(k));
        return (int)(l % tableSize);
    }

    private int h1(long k) {
        return (int)(k % tableSize);
    }

    private int h2(long k) {
        return (int)(1 + (k % (tableSize - 2)));
    }

    private static final int[] smallPrimes = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37 };

    /**
     * Does exactly what it says on the tin.
     */
    private static boolean isprime(int x) {
        int div = 0, stop;
        for (int i = 0; i < smallPrimes.length; ++i ) {
            div = smallPrimes[i];
            if (x % div == 0)
                return false;
        }
        for ( stop = x; x / stop < stop; stop >>= 1 ) ;
        stop <<= 1;
        for (div += 2; div < stop; div += 2)
            if ( x % div == 0 )
                return false;
        return true;
    }
}
