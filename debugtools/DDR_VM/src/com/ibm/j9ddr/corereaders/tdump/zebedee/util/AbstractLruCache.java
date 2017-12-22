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
 * This class is the superclass of the LRU cache classes. It provides the LRU (Least Recently Used)
 * infrastructure to map integer keys to objects or primitive values.
 * It acts just like a hash map except that once it has reached its maximum size, any further
 * puts will displace the least recently used entry. It uses a doubly linked list to keep track
 * of which entries are most used - an entry is moved to the front of the list every time it
 * is used. Note that the LRU info is lost when the cache is rehashed, but I don't think that's
 * a problem because once the cache reaches its maximum size (and steady state) it will no longer rehash.
 */

public abstract class AbstractLruCache extends AbstractHashMap {

    /** The maximum size of the cache */
    int maxSize;
    /** The index of the first (most recently used) entry in the list */
    int head = -1;
    /** The index of the last (least recently used) entry in the list */
    int tail = -1;
    /** The array of next pointers */
    int[] next = new int[INITIAL_SIZE];
    /** The array of prev pointers */
    int[] prev = new int[INITIAL_SIZE];

    /**
     * Create a new AbstractLruCache.
     * @param maxSize the maximum size the cache can grow to
     */
    protected AbstractLruCache(int maxSize) {
        this.maxSize = maxSize;
    }

    /**
     * Overridden method which the subclass must also override to allocate new values array.
     * We also reallocate the next and prev
     * arrays at this point (just prior to repopulating the cache).
     */
    void allocNewValuesArray(int newSize) {
        next = new int[newSize];
        prev = new int[newSize];
        head = tail = -1;
    }

    /**
     * Returns the index for the value mapped by the given key. Also promotes this key to the most
     * recently used.
     * @return the index or -1 if it cannot be found
     */
    protected int getIndexAndPromote(long key) {
        int index = getIndex(key) ;
        if (index != -1) {
            if (head != index) {
                /*
                 * Not currently the head so unlink from where we are now...
                 */
                int n = next[index];
                int p = prev[index];
                next[p] = n;
                if (n != -1) {
                    prev[n] = prev[index];
                } else {
                    tail = p;
                }
                /*
                 * ... and make us the new head
                 */
                prev[index] = -1;
                next[index] = head;
                prev[head] = index;
                head = index;
            }
            return index;
        }
        return -1;
    }

    /**
     * Get the index for a put operation. Takes care of promotion and removing the LRU entry
     * if necessary.
     */
    protected int putIndexAndPromote(long key) {
        int index = getIndex(key);
        if (index == -1) {
            /* Not currently present */
            if (slotsInUse < maxSize) {
                /* Don't need to chuck anything out */
                index = putIndex(key);
                /* Make us the new head */
                prev[index] = -1;
                next[index] = head;
                if (slotsInUse == 1) {
                    tail = index;
                } else {
                    prev[head] = index;
                }
                head = index;
                return index;
            } else {
                /* Cache is full - chuck out the LRU */
                state[tail] = DELETED;
                deletedSlots++;
                slotsInUse--;
                tail = prev[tail];
                next[tail] = -1;
                /* Try again now there's room */
                return putIndexAndPromote(key);
            }
        } else {
            /* Just replace current value */
            return index;
        }
    }
}
