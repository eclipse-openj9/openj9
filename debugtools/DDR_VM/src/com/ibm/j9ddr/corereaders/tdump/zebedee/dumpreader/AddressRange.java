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

package com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader;

/**
 * This class represents an address range. An address range is simply an address plus
 * a length.
 */

public class AddressRange {
    long startAddress;
    long length;

    /**
     * Create a new address range.
     * @param startAddress the lower bound of the address range
     * @param length the length of the address range
     */
    public AddressRange(long startAddress, long length) {
        this.startAddress = startAddress;
        this.length = length;
    }

    /**
     * Get the lower bound of this address range.
     * @return the start address
     */
    public long getStartAddress() {
        return startAddress;
    }

    /**
     * Get the upper bound of this address range (inclusive).
     * @return the end address
     */
    public long getEndAddress() {
        return startAddress + length - 1;
    }

    /**
     * Get the length of this address range.
     * @return the length
     */
    public long getLength() {
        return length;
    }
}
