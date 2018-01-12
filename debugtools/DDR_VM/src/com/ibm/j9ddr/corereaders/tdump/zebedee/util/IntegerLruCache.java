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

import java.util.Random;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * This class provides an LRU (Least Recently Used) cache which maps integer keys to int values.
 * See the description in the superclass {@link com.ibm.j9ddr.corereaders.tdump.zebedee.util.AbstractLruCache} for more
 * details.
 * Like its sibling {@link com.ibm.j9ddr.corereaders.tdump.zebedee.util.IntegerMap} it is low footprint. Note that the value -1 is not allowed
 * (see discussion in {@link com.ibm.j9ddr.corereaders.tdump.zebedee.util.IntegerMap}).
 */

public final class IntegerLruCache extends AbstractLruCache {

    /** The array of values */
    int[] values = new int[INITIAL_SIZE];

    /**
     * Create a new IntegerLruCache.
     * @param maxSize the maximum size the cache can grow to
     */
    public IntegerLruCache(int maxSize) {
        super(maxSize);
    }

    /**
     * Overridden method to return values array.
     */
    Object getValuesArray() {
        return values;
    }

    /**
     * Overridden method to allocate new values array.
     */
    void allocNewValuesArray(int newSize) {
        super.allocNewValuesArray(newSize);
        values = new int[newSize];
    }

    /**
     * Overridden method to repopulate with key plus value at given offset.
     */
    void put(long key, Object oldvalues, int offset) {
        int[] v = (int[])oldvalues;
        put(key, v[offset]);
    }

    /**
     * Returns the value mapped by the given key. Also promotes this key to the most
     * recently used.
     * @return the value or -1 if it cannot be found
     */
    public synchronized int get(long key) {
        int index = getIndexAndPromote(key) ;
        if (index != -1) {
            return values[index];
        }
        return -1;
    }

    /**
     * Add the key/value pair to the map. The value must not be -1.
     */
    public synchronized void put(long key, int value) {
        int index = putIndexAndPromote(key) ;
        values[index] = value;
        checkRehash();
    }

    private boolean doCheck = true;

    /**
     * Test method.
     */
    private void test() {
        Random rand = new Random(23);
        LruCache check = new LruCache(1000);
        for (int i = 0; i < 50000; i++) {
            long key = rand.nextLong();
            int value = rand.nextInt();
            if (get(key) != -1) {
                continue;
            }
            put(key, value);
            if (doCheck) {
                check.put(new Long(key), new Integer(value));
                if (get(key) != value) {
                    throw new Error("found " + get(key) + " expected " + value);
                }
            }
        }
        if (doCheck) {
            Long[] keys = (Long[])check.keySet().toArray(new Long[0]);
            for (int i = 0; i < keys.length; i++) {
                long key = keys[i].longValue();
                int value = ((Integer)check.get(keys[i])).intValue();
                if (get(key) != value) {
                    throw new Error("at " + i + " found " + get(key) + " expected " + value + " key " + key);
                }
            }
        }
    }

    /**
     * @hidden
     */
    private class LruCache extends LinkedHashMap {
        int maxSize;

        LruCache(int maxSize) {
            super(16, (float)0.75, true);
            this.maxSize = maxSize;
        }

        protected boolean removeEldestEntry(Map.Entry eldest) {
            return size() > maxSize;
        }
    }

    /**
     * Run some basic tests on this class.
     *
     * @exclude
     */
    public static void main(String args[]) {
        IntegerLruCache map = new IntegerLruCache(20);
        for (int i = 0; i < 20; i++) {
            map.put(i, i);
        }
        for (int i = 19; i >= 0; i--) {
            int j = map.get(i);
            if (i != j) {
                throw new Error("j is " + j + " for i " + i);
            }
        }
        for (int i = 20; i < 30; i++) {
            map.put(i, i);
        }
        for (int i = 0; i < 30; i++) {
            int j = map.get(i);
            if (i >= 20 || i < 10) {
                if (i != j) {
                    throw new Error("j is " + j + " for i " + i);
                }
            } else if (j != -1) {
                throw new Error("j is " + j + " for i " + i);
            }
        }
        map = new IntegerLruCache(1000);
        map.test();
        for (IntEnumeration e = map.getKeys(); e.hasMoreElements(); ) {
            long key = e.nextInt();
            if (map.get(key) == -1) throw new Error("uh oh");
        }
        System.out.println("finished!");
    }
}
