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

package com.ibm.j9ddr.corereaders.tdump.zebedee.mvs;

import com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.*;

import java.io.*;

/**
 * This class represents Linkage Stack Entry (LSE).
 *
 * @depend - - - com.ibm.zebedee.dumpreader.AddressSpace
 * @has - - - com.ibm.zebedee.util.Template
 */

public class Lse {

    /** The AddressSpace we belong to */
    private AddressSpace space;
    /** The AddressSpace ImageInputStream */
    private AddressSpaceImageInputStream inputStream;
    /** The address of our lse structure */
    private long address;
    private long lsesptr;
    /** PC STATE entry type */
    public static final int LSEDPC = 5;
    /** PC STATE entry type */
    public static final int LSED1PC = 13;
    /** BAKR STATE entry type */
    public static final int LSED1BAKR = 12;

    /**
     * Create a new Linkage Stack Entry
     */
    public Lse(AddressSpace space, long address) {
        this.space = space;
        inputStream = space.getImageInputStream();
        this.address = address;
        lsesptr = address + LseslsedTemplate.length();
    }

    /**
     * Returns the address of this Lse
     */
    public long address() {
        return address;
    }

    /**
     * Returns the entry type
     */
    public int lsestyp7() throws IOException {
        return (int)LseslsedTemplate.getLsestyp7(inputStream, lsesptr + LsestateTemplate.getLses_ed$offset());
    }

    /**
     * Returns the entry type
     */
    public int lses1typ7() throws IOException {
        return (int)Lses1lsed1Template.getLses1typ7(inputStream, lsesptr + Lsestate1Template.getLses1_ed1$offset());
    }

    /**
     * Returns the target routine indication
     */
    public long lsestarg() throws IOException {
        return LsestateTemplate.getLsestarg(inputStream, lsesptr);
    }

    /**
     * Returns the psw
     */
    public long lsespsw() throws IOException {
        return LsestateTemplate.getLsespsw(inputStream, lsesptr);
    }

    /**
     * Returns the psw
     */
    public long lses1pswh() throws IOException {
        return Lsestate1Template.getLses1pswh(inputStream, lsesptr);
    }

    /**
     * Returns the given GPR
     * @param gpr the index of the GPR to return. Must be in the range 0-15.
     */
    public long lsesgrs(int gpr) throws IOException {
        if (gpr < 0 || gpr > 15)
            throw new IndexOutOfBoundsException("gpr out of range 0-15: " + gpr);
        return space.readLong(lsesptr + LsestateTemplate.getLsesgrs$offset() + gpr*4);
    }

    /**
     * Returns the given GPR
     * @param gpr the index of the GPR to return. Must be in the range 0-15.
     */
    public long lses1grs(int gpr) throws IOException {
        if (gpr < 0 || gpr > 15)
            throw new IndexOutOfBoundsException("gpr out of range 0-15: " + gpr);
        return space.readLong(lsesptr + Lsestate1Template.getLses1grs$offset() + gpr*8);
    }

    /**
     * Returns the pasn of the calling program
     */
    public int lses1pasn() throws IOException {
        return (int)Lsestate1Template.getLses1pasn(inputStream, lsesptr);
    }

    /** 
     * Returns true if this is Z/Architecture (ie the variant with all the 1's)
     */
    public boolean isZArchitecture() throws IOException {
        long entrysize = LsedTemplate.getLsednes(inputStream, address);
        return entrysize == Lsestate1Template.length();
    }

    private static String hex(long n) {
        return Long.toHexString(n);
    }
}
