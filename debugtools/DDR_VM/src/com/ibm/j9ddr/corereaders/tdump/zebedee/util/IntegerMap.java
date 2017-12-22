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
import java.util.HashMap;

/**
 * This is a simple class to map one integer to another but without the overhead of using
 * a Hashtable. There is one limitation at present which is that the value of -1 can't be
 * used since this is currently used to indicate value not found.
 */

public final class IntegerMap extends AbstractHashMap {

    /** The array of values */
    long[] values = new long[INITIAL_SIZE];

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
        values = new long[newSize];
    }

    /**
     * Overridden method to repopulate with key plus value at given offset.
     */
    void put(long key, Object oldvalues, int offset) {
        long[] v = (long[])oldvalues;
        put(key, v[offset]);
    }

    /**
     * Returns the value mapped by the given key.
     * @return the value or -1 if it cannot be found
     */
    public synchronized long get(long key) {
        int index = getIndex(key) ;
        return index == -1 ? -1 : values[index];
    }

    /**
     * Add the key/value pair to the map. The value must not be -1.
     */
    public synchronized void put(long key, long value) {
        values[putIndex(key)] = value;
        checkRehash();
    }

    /**
     * Remove the key from the map and return the old value.
     */
    public synchronized long remove(long key) {
        int index = removeIndex(key) ;
        return index == -1 ? -1 : values[index];
    }

    public int memoryUsage() {
        return tableSize * 17;
    }

    private boolean doCheck = true;

    /**
     * Test method.
     */
    private void test() {
        Random rand = new Random(23);
        HashMap check = new HashMap();
        for (int i = 0; i < 500000; i++) {
            long key = rand.nextLong();
            long value = rand.nextLong();
            boolean remove = ((i % 3) == 0);
            if (get(key) != -1) {
                continue;
            }
            put(key, value);
            if (doCheck) {
                check.put(new Long(key), new Long(value));
                if (get(key) != value) {
                    throw new Error("found " + get(key) + " expected " + value);
                }
                if (remove) {
                    remove(key);
                    check.remove(new Long(key));
                }
            }
        }
        if (doCheck) {
            Long[] keys = (Long[])check.keySet().toArray(new Long[0]);
            for (int i = 0; i < keys.length; i++) {
                long key = keys[i].longValue();
                long value = ((Long)check.get(keys[i])).longValue();
                if (get(key) != value) {
                    throw new Error("at " + i + " found " + get(key) + " expected " + value + " key " + key);
                }
            }
        }
    }

    /**
     * Run some basic tests on this class.
     *
     * @exclude
     */
    public static void main(String args[]) {
        IntegerMap map = new IntegerMap();
        map.test();
        for (IntEnumeration e = map.getKeys(); e.hasMoreElements(); ) {
            long key = e.nextInt();
            if (map.get(key) == -1) throw new Error("uh oh");
        }
        System.out.println("finished!");
    }
}
