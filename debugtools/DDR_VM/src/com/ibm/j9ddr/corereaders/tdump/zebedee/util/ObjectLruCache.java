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

package com.ibm.j9ddr.corereaders.tdump.zebedee.util;

/**
 * This class provides an LRU (Least Recently Used) cache which maps integer keys to object values.
 * See the description in the superclass {@link com.ibm.j9ddr.corereaders.tdump.zebedee.util.AbstractLruCache} for more
 * details.
 */

public final class ObjectLruCache extends AbstractLruCache {

    /** The array of values */
    Object[] values = new Object[INITIAL_SIZE];

    /**
     * Create a new ObjectLruCache.
     * @param maxSize the maximum size the cache can grow to
     */
    public ObjectLruCache(int maxSize) {
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
        values = new Object[newSize];
    }

    /**
     * Overridden method to repopulate with key plus value at given offset.
     */
    void put(long key, Object oldvalues, int offset) {
        Object[] v = (Object[])oldvalues;
        put(key, v[offset]);
    }

    /**
     * Returns the value mapped by the given key. Also promotes this key to the most
     * recently used.
     * @return the value or null if it cannot be found
     */
    public synchronized Object get(long key) {
        int index = getIndexAndPromote(key) ;
        if (index != -1) {
            return values[index];
        }
        return null;
    }

    /**
     * Add the key/value pair to the map.
     */
    public synchronized void put(long key, Object value) {
        int index = putIndexAndPromote(key) ;
        values[index] = value;
        checkRehash();
    }
}
