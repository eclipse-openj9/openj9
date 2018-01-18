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
 * This is a lightweight class to hold a set of integer values.
 */

public final class IntegerSet extends AbstractHashMap {

    /**
     * Overridden method to return values array.
     */
    Object getValuesArray() {
        return null;    // not used
    }

    /**
     * Overridden method to allocate new values array.
     */
    void allocNewValuesArray(int newSize) {
        // not used
    }

    /**
     * Overridden method to repopulate with key plus value at given offset.
     */
    void put(long key, Object oldvalues, int offset) {
        putIndex(key);      // occupy a slot
    }

    /**
     * @return true if the set contains the given key
     */
    public boolean contains(long key) {
        return getIndex(key) != -1;
    }

    /**
     * Add the key to the set.
     */
    public void add(long key) {
        putIndex(key);
        checkRehash();
    }

    /**
     * Remove the key from the set.
     */
    public void remove(long key) {
        removeIndex(key) ;
    }

    public int memoryUsage() {
        return tableSize * 9;
    }
}
