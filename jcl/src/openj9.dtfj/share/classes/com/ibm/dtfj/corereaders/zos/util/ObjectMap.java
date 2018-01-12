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


/**
 * This is a simple class to map an integer key to an object value. It has a smaller footprint
 * than the various standard Java classes owing to the use of open addressing (see description
 * of {@link com.ibm.dtfj.corereaders.zos.util.AbstractHashMap}).
 */

public final class ObjectMap extends AbstractHashMap {

    /** The array of values */
    Object[] values = new Object[INITIAL_SIZE];

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
     * Returns the value mapped by the given key.
     * @return the value or null if it cannot be found
     */
    public Object get(long key) {
        int index = getIndex(key) ;
        return index == -1 ? null : values[index];
    }

    /**
     * Add the key/value pair to the map.
     */
    public void put(long key, Object value) {
        values[putIndex(key)] = value;
        checkRehash();
    }

    /**
     * Remove the key from the map and return the old value.
     */
    public Object remove(long key) {
        int index = removeIndex(key) ;
        return index == -1 ? null : values[index];
    }

    /**
     * Returns an array containing the values where the returned runtime array type is derived from
     * the given array (or the given array is simply reused if there is room).
     */
    public Object[] toArray(Object[] a) {
        if (a.length < slotsInUse)
            a = (Object[])java.lang.reflect.Array.newInstance(a.getClass().getComponentType(), slotsInUse);
        for (int i = 0, j = 0; i < tableSize; i++) {
            if (state[i] == OCCUPIED) {
                a[j++] = values[i];
            }
        }
        if (a.length > slotsInUse)
            a[slotsInUse] = null;
        return a;
    }
}
