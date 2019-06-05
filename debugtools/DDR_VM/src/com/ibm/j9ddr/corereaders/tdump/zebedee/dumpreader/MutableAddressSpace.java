/*******************************************************************************
 * Copyright (c) 2006, 2019 IBM Corp. and others
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

import java.io.*;

import com.ibm.j9ddr.corereaders.tdump.zebedee.util.*;

import java.util.logging.*;

/**
 * Provides the ability to write as well as read to a copy of the address space. This class
 * is provided for use by the {@link com.ibm.j9ddr.corereaders.tdump.zebedee.util.Emulator}.
 */

public final class MutableAddressSpace extends AddressSpace implements Emulator.ImageSpace {
    /** Our very own cache of blocks */
    private ObjectMap mutableBlockCache = new ObjectMap();
    /** Parent address space */
    private AddressSpace parentSpace;
    /** The last mutable block address requested (quick block cache) */
    protected long lastMutableBlockAddress;
    /** The last mutable block read (quick block cache) */
    private byte[] lastMutableBlock;
    /** Logger */
    private static Logger log = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);

    /**
     * Creates a new MutableAddressSpace from the given AddressSpace. Updates to this "forked"
     * copy will not be reflected in the parent AddressSpace.
     * @param AddressSpace the address space we read from
     */
    public MutableAddressSpace(AddressSpace space) {
        super(space.dump, space.asid);
        addressMap = space.addressMap;
        ranges = space.getAddressRanges();
        parentSpace = space;
        setIs64bit(space.is64bit());
    }

    /**
     * Get the block of bytes for the given address. We look first in our own cache and if
     * its not there call the superclass method.
     */
    protected byte[] getBlockFromCacheOrDisk(long address) throws IOException {
        assert (address & 0xfff) == 0;
        byte[] block = (byte[])mutableBlockCache.get(address);
        if (block == null) {
            /* Get the block from the parent address space */
            block = parentSpace.getBlock(address);
        }
        putBlockInQuickCache(address, block);
        return block;
    }

    /**
     * Get a block that we can write into and store in our cache.
     */
    private byte[] getMutableBlock(long address) throws IOException {
        address = roundToPage(address);
        byte[] block;
        if (address == lastMutableBlockAddress) {
            block = lastMutableBlock;
        } else {
            block = (byte[])mutableBlockCache.get(address);
            if (block == null) {
                /* Get the block from the parent address space */
                byte[] tmp = parentSpace.getBlock(address);
                block = new byte[Dump.DATABLOCKSIZE];
                System.arraycopy(tmp, 0, block, 0, block.length);
                mutableBlockCache.put(address, block);
            }
            /* XXX can move quick cache stuff into the map itself? */
            lastMutableBlock = block;
            lastMutableBlockAddress = address;
        }
        putBlockInQuickCache(address, block);
        return block;
    }

    /**
     * Write the contents of the byte array to the specified address.
     * @throws IOException if the given address (of offset from there) is not present in this address space
     */
    public void writeBytes(long address, byte[] array) throws IOException {
        for (int i = 0; i < array.length; i++) {
            writeByte(address++, array[i]);
        }
    }

    /**
     * Write a word at the specified address. A word is either 32 or 64 bits depending on
     * the machine context.
     */
    public void writeWord(long address, long value) throws IOException {
        if (is64bit()) {
            writeInt(address, (int)(value >>> 32));
            writeInt(address + 4, (int)value);
        } else {
            writeInt(address, (int)value);
        }
    }

    /* Emulator.ImageSpace methods: */

    /**
     * Write a byte at the specified address.
     * @throws IOException if the given address is not present in this address space
     */
    public void writeByte(long address, int value) throws IOException {
        getMutableBlock(address)[(int)address & 0xfff] = (byte)value;
    }

    /**
     * Write a short at the specified address.
     * @throws IOException if the given address is not present in this address space
     */
    public void writeShort(long address, int value) throws IOException {
        writeByte(address, value >> 8);
        writeByte(address + 1, value);
    }

    /**
     * Write an int at the specified address.
     * @throws IOException if the given address is not present in this address space
     */
    public void writeInt(long address, int value) throws IOException {
        int offset = (int)address & 0xfff;
        if (offset < 0xffd) {
            // whole word fits in this block
            byte[] b = getMutableBlock(address);
            b[offset] = (byte)(value >> 24);
            b[offset + 1] = (byte)(value >> 16);
            b[offset + 2] = (byte)(value >> 8);
            b[offset + 3] = (byte)(value);
        } else {
            writeByte(address, value >>> 24);
            writeByte(address + 1, value >>> 16);
            writeByte(address + 2, value >>> 8);
            writeByte(address + 3, value);
        }
    }

    /**
     * Write a long at the specified address.
     * @throws IOException if the given address is not present in this address space
     */
    public void writeLong(long address, long value) throws IOException {
        writeInt(address, (int)(value >>> 32));
        writeInt(address + 4, (int)value);
    }

    /**
     * Allocate a chunk of unused memory. This searches the known address ranges and tries
     * to find an unallocated area of the given size. If successful, this area effectively
     * becomes part of this ImageSpace and may be read and written to as normal.
     * This method is used by users of the Emulator for things like allocating
     * stack, arguments etc.
     * @return the address of the allocated memory
     */
    public long malloc(int size) throws IOException {
        AddressRange[] unused = getUnusedAddressRanges();
        /* Squirrel away the returned pointer plus the size in the first two words */
        size += 8;
        /* Need to allocate one more word than we really need because of the way (eg)
         * readUnsignedByte uses readInt and can go beyond the end of the allocated area. */
        size += 4;
        int roundSize = (size + Dump.DATABLOCKSIZE - 1) & ~(Dump.DATABLOCKSIZE - 1);
        int count = roundSize / Dump.DATABLOCKSIZE;
        assert unused[0].getLength() > 16;
        for (int i = 0; i < unused.length; i++) {
            long startAddress = unused[i].getStartAddress();
            if (size < unused[i].getLength() && mutableBlockCache.get(startAddress) == null) {
                for (int j = 0; j < count; j++) {
                    byte[] block = new byte[Dump.DATABLOCKSIZE];
                    mutableBlockCache.put(startAddress + (j * Dump.DATABLOCKSIZE), block);
                }
                writeInt(startAddress, (int)startAddress);   // for sanity check only
                writeInt(startAddress + 4, size);
                log.fine("allocated 0x" + hex(size - 12) + " bytes at 0x" + hex(startAddress + 8));
                return startAddress + 8;
            }
        }
        throw new IOException("could not malloc " + (size - 12));
    }

    /* Like the above but starts from high addresses. Useful if you add an extra malloc
     * and want to compare instruction trace output. */
    public long rmalloc(int size) throws IOException {
        AddressRange[] unused = getUnusedAddressRanges();
        size += 8;
        size += 4;
        int roundSize = (size + Dump.DATABLOCKSIZE - 1) & ~(Dump.DATABLOCKSIZE - 1);
        int count = roundSize / Dump.DATABLOCKSIZE;
        for (int i = unused.length - count; i >= 0; i--) {
            long startAddress = unused[i].getStartAddress();
            if (size < unused[i].getLength() && mutableBlockCache.get(startAddress) == null) {
                for (int j = 0; j < count; j++) {
                    byte[] block = new byte[Dump.DATABLOCKSIZE];
                    mutableBlockCache.put(startAddress + (j * Dump.DATABLOCKSIZE), block);
                }
                writeInt(startAddress, (int)startAddress);   // for sanity check only
                writeInt(startAddress + 4, size);
                log.fine("allocated 0x" + hex(size - 12) + " bytes at 0x" + hex(startAddress + 8));
                return startAddress + 8;
            }
        }
        throw new IOException("could not malloc " + (size - 12));
    }

    /**
     * Free the memory that was previously allocated by malloc.
     */
    public void free(long ptr) throws IOException {
        ptr -= 8;
        if (readInt(ptr) != ptr) {
            // maybe this wasn't allocated by us after all?
            return;
        }
        int size = readInt(ptr + 4);
        assert size >= 0 && size < 0x10000000;
        int roundSize = (size + Dump.DATABLOCKSIZE - 1) & ~(Dump.DATABLOCKSIZE - 1);
        int count = roundSize / Dump.DATABLOCKSIZE;
        for (int j = 0; j < count; j++) {
            Object block = mutableBlockCache.remove(ptr + (j * Dump.DATABLOCKSIZE));
            assert block != null;
        }
    }

    /**
     * Allocate a page of memory at the given address. The address must not already be in use.
     * A page is 4096 bytes in length. Address must be on a page boundary.
     */
    public void allocPage(long address) throws IOException {
        /* Check address is on a page boundary */
        if ((address % Dump.DATABLOCKSIZE) != 0)
            throw new IOException("address " + hex(address) + " not on page boundary");
        /* Check the address isn't in use */
        if (isValidAddress(address) || mutableBlockCache.get(address) != null)
            throw new IOException("address " + hex(address) + " already in use");
        /* Allocate the page */
        mutableBlockCache.put(address, new byte[Dump.DATABLOCKSIZE]);
    }

    private static String hex(long i) {
        return Long.toHexString(i);
    }

    private static String hex(int i) {
        return Integer.toHexString(i);
    }
}
