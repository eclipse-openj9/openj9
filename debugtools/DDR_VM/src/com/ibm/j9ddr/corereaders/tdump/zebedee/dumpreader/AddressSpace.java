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

import java.util.*;
import java.util.logging.*;

/**
 * This class represents an address space in an svcdump. It provides the ability to read
 * the contents of an address amongst other things. Note that in Zebedee as a whole, addresses
 * are always represented by a long (Java primitive) to avoid the overhead of continually
 * creating address objects.
 *
 * @has - - - com.ibm.zebedee.dumpreader.AddressRange
 * @assoc - - - com.ibm.zebedee.dumpreader.AddressSpaceImageInputStream
 * @has - - - com.ibm.zebedee.util.IntegerMap
 * @has - - - com.ibm.zebedee.util.ObjectLruCache
 * @depend - - - com.ibm.zebedee.util.CharConversion
 * @has - - - com.ibm.zebedee.util.MachineContext
 * @depend - - - com.ibm.zebedee.util.Template
 */
public class AddressSpace implements Serializable {
    /** The dump we belong to */
    protected Dump dump;
    /** Our address space id */
    protected int asid;
    /** Mapping of address to file offsets */
    protected IntegerMap addressMap = new IntegerMap();
    /** The number of times we found a block in the quick cache */
    private int quickCacheHits;
    /** The number of times we found a block in the cache */
    private int cacheHits;
    /** And the number of times we didn't */
    private int cacheMisses;
    /** The block cache - 500 blocks is normally plenty */
    private ObjectLruCache blockCache;
    /** The last block address requested (quick block cache) */
    protected long lastBlockAddress;
    /** The last block read (quick block cache) */
    private byte[] lastBlock;
    /** The last but one block address requested (quick block cache) */
    protected long lastButOneBlockAddress;
    /** The last but one block read (quick block cache) */
    private byte[] lastButOneBlock;
    /** Array of AddressRanges */
    protected AddressRange[] ranges;
    /** Whether we are a 64-bit address space */
    private boolean is64bit;
    /** A general purpose map that AddressSpace users can use for their own purposes */
    private HashMap userMap = new HashMap();
    /** The lowest address in this address space */
    private long lowestAddress = -1;
    /** Constant used to indicate an uninitialized pointer (for performance reasons
     *  we use primitive longs for pointers rather than objects) */
    public static final long WILD_POINTER = 0xbaadf00ddeadbeefL;
    /** Logger */
    private static Logger log = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);
    // Cache performance statistics. These are switched on by system property "zebedee.printStats". Note that cache
    // hit/miss counter increments are now commented out in getBlockFromQuickCache() and getBlockFromCacheOrDisk()
    // for performance reasons, and need to be un-commented if cache statistics are required.
    private static final boolean printStats = Boolean.getBoolean("zebedee.printStats");

    /**
     * Creates a new AddressSpace belonging to the given Dump and having the given id.
     * @param dump the {@link com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.Dump} this space belongs to
     * @param asid the address space id
     */
    public AddressSpace(Dump dump, int asid) {
        this.dump = dump;
        this.asid = asid;
        initialize();
    }

    /**
     * @return the Dump we belong to
     */
    public Dump getDump() {
        return dump;
    }

    /**
     * @return the root address space
     */
    public AddressSpace getRootAddressSpace() {
        return dump.rootSpace;
    }

    /**
     * @return the lowest address in this address space
     */
    public long getLowestAddress() {
        if (lowestAddress == -1) {
            /* Get the list of the start addresses of each block */
            long[] addresses = addressMap.getKeysArray();
            /* Sort it */
            Arrays.sort(addresses);
            /* Get the lowest */
            lowestAddress = addresses[0];
        }
        return lowestAddress;
    }

    /**
     * Do any transient initialization required. This method is called both from the
     * AddressSpace constructor and from the readObject method when the AddressSpace
     * is read from the cache.
     */
    private void initialize() {
        blockCache = new ObjectLruCache(500);
        userMap = new HashMap();
        lastBlockAddress = -1;
        lastButOneBlockAddress = -1;
        lowestAddress = -1;
        if (printStats && getClass() == AddressSpace.class) {
            Runtime.getRuntime().addShutdownHook(new Thread() {
                // Print the cache performance statistics. These are switched on by system property "zebedee.printStats". Note
                // that cache hit/miss counters are commented out in getBlockFromQuickCache() and getBlockFromCacheOrDisk() for
                // performance reasons, and need to be un-commented if cache statistics are required.
                public void run() {
                    synchronized (AddressSpace.class) {
                        System.out.println("stats for asid " + hex(asid) + " in dump " + dump.getName() + ":");
                        System.out.println("    cacheMisses = " + cacheMisses);
                        System.out.println("    cacheHits = " + cacheHits);
                        System.out.println("    quickCacheHits = " + quickCacheHits);
                    }
                }
            });
        }
    }

    /**
     * Returns a general purpose Map object that AddressSpace users can use as they see fit.
     * This map is provided as a convenience since users of AddressSpace often want to store
     * related info somewhere. Care should be taken that nobody else has used the same key -
     * one way to achieve this would be to use a class object as a key.
     */
    public Map getUserMap() {
        return userMap;
    }

    /**
     * Returns the MVS ASID (Address Space Identifier) for this address space.
     */
    public int getAsid() {
        return asid;
    }

    /**
     * Returns an array of all the {@link com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.AddressRange}s in this
     * address space. An address range is simply a pair of [low, high] addresses for which
     * memory is available in the dump.
     */
    public AddressRange[] getAddressRanges() {
        if (ranges != null)
            return ranges;
        /* Get the list of the start addresses of each block */
        long[] addresses = addressMap.getKeysArray();
        /* Sort it */
        Arrays.sort(addresses);
        /* Now go through the list and create a new range every time we find a gap */
        long startAddress = 0;
        long lastAddress = 0;
        Vector v = new Vector();
        for (int i = 0; i < addresses.length; i++) {
            if (i == 0) {
                startAddress = addresses[0];
            } else if (addresses[i] != lastAddress + Dump.DATABLOCKSIZE) {
                /* Found a discontinuity so create new AddressRange */
                v.add(new AddressRange(startAddress, lastAddress + 0x1000 - startAddress));
                startAddress = addresses[i];
            }
            lastAddress = addresses[i];
        }
        v.add(new AddressRange(startAddress, lastAddress + 0x1000 - startAddress));
        ranges = (AddressRange[])v.toArray(new AddressRange[0]);
        return ranges;
    }

    /**
     * Returns an array of all the unused address ranges in the address space. This is
     * the inverse of {@link #getAddressRanges}.
     */
    public AddressRange[] getUnusedAddressRanges() {
        getAddressRanges();
        AddressRange[] unused = new AddressRange[ranges.length - 1];
        for (int i = 0; i < unused.length; i++) {
            long start = ranges[i].getEndAddress() + 1;
            long end = ranges[i + 1].getStartAddress() - 1;
            unused[i] = new AddressRange(start, end - start);
        }
        return unused;
    }

    /**
     * Returns an {@link com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.AddressSpaceImageInputStream} for this
     * address space. The ImageInputStream interface provides a wealth of read methods.
     */
    public AddressSpaceImageInputStream getImageInputStream() {
        return new AddressSpaceImageInputStream(this);
    }

    /**
     * This method is for debugging purposes only and should eventually disappear. We use the
     * value of WILD_POINTER to indicate that a pointer has not been initialized (the chances
     * of getting this value by accident are one in a very large number indeed).
     */
    private void checkIfWild(long address) {
        assert address != WILD_POINTER;
        assert (int)address != (int)WILD_POINTER;
        /* Also check if the access is anywhere near the uninitialized value for the
         * time being. We may have done some pointer arithmetic with it. */
        assert (address < WILD_POINTER - 0x1000) || (address > WILD_POINTER + 0x1000) : hex(address);
        assert ((int)address < (int)WILD_POINTER - 0x1000) || ((int)address > (int)WILD_POINTER + 0x1000) : hex((int)address);
    }

    /**
     * Rounds the address to the base page address.
     */
    protected long roundToPage(long address) {
        checkIfWild(address);
        return address & 0xfffffffffffff000L;
    }

    /**
     * Get the offset in the dump for this address.
     */
    private long offset(long address) {
        /* Round down to the nearest block */
        long blockAddress = roundToPage(address);
        /* Get the offset in the dump file */
        long offset = addressMap.get(blockAddress);
        if (offset == -1) {
            /*
             * Now this is weird code and almost certainly the wrong way to do it,
             * but it seems to work: for certain low memory addresses, if they can't be
             * found in our address space, they can sometimes be found in what I call
             * the "root" address space with asid 1. Maybe this is kernel memory mapped into
             * all other spaces? Who knows, anyway let's go and try there before giving up.
             */
            if (asid != 1) {
                offset = dump.rootSpace.addressMap.get(blockAddress);
                if (offset != -1)
                    log.fine("found address " + hex(address) + " in root address space");
            }
            if (offset == -1 && asid != 0xc9c1d9c8 && dump.highVirtualSharedSpace != null) {
                /* Even weirder stuff - seems there is a "high virtual shared space" -
                 * appears to be mapped by COMPDATA(IARHVSHR). Don't know much about
                 * this but it appears to work. */
                offset = dump.highVirtualSharedSpace.addressMap.get(blockAddress);
                if (offset != -1)
                    log.fine("found address " + hex(address) + " in high virtual shared address space");
            }
            if (offset == -1) {
                log.fine("block address " + hex(address) + " not in dump");
            }
        }
        return offset;
    }

    /**
     * Read from the given address into the given buffer.
     * @param address the address to read from
     * @param buf the buffer to read into
     */
    private void read(long address, byte[] buf) throws IOException {
        assert buf.length <= Dump.DATABLOCKSIZE : buf.length;
        /* Get the offset in the dump file */
        long offset = offset(address);
        if (offset == -1) {
            throw new IOException("block address " + hex(address) + " not in dump");
        }
        synchronized (dump) {
            /* Now we seek to that offset to get to the actual data */
            dump.seek(offset);
            /* Finally read the requested bytes */
            dump.readFully(buf);
        }
    }

    /** Return true if the address is present in this address space */
    public boolean isValidAddress(long address) {
        try {
            return offset(address) != -1;
        } catch (Throwable e) {
            return false;
        }
    }

    /**
     * See if the block is in the quick cache.
     */
    final synchronized byte[] getBlockFromQuickCache(long address) {
        //assert (address & 0xfff) == 0;
        if (address == lastBlockAddress) {
            //quickCacheHits++;
            //if (log.isLoggable(Level.FINEST))
            //    log.finest("request to get block for address 0x" + hex(address) + " met by quick cache");
            return lastBlock;
        }
        if (address == lastButOneBlockAddress) {
            //quickCacheHits++;
            //if (log.isLoggable(Level.FINEST))
            //    log.finest("request to get block for address 0x" + hex(address) + " met by quick cache");
            return lastButOneBlock;
        }
        return null;
    }

    /**
     * Place block in the quick cache.
     */
    final synchronized void putBlockInQuickCache(long address, byte[] block) {
        assert (address & 0xfff) == 0;
        lastButOneBlockAddress = lastBlockAddress;
        lastButOneBlock = lastBlock;
        lastBlockAddress = address;
        lastBlock = block;
    }

    /**
     * Get the block of bytes for the given address. Usually we find this in the cache. A
     * block is 4096 bytes in length.
     */
    public final byte[] getBlock(long address) throws IOException {
        address = roundToPage(address);
        /* Quickly check the cached last block(s). This gets the vast majority */
        byte[] b = getBlockFromQuickCache(address);
        return b != null ? b : getBlockFromCacheOrDisk(address);
    }

    /**
     * Get the block either from the cache or from disk. This protected method is
     * provided so that MutableAddressSpace has the chance to look in its own cache first.
     */
    protected byte[] getBlockFromCacheOrDisk(long address) throws IOException {
        //assert (address & 0xfff) == 0;
        /* Now look to see if the block is in the lru cache */
        byte[] block = (byte[])blockCache.get(address);
        if (block == null) {
            //cacheMisses++;
            /* Not found in cache so read from the file itself */
            block = new byte[Dump.DATABLOCKSIZE];
            read(address, block);
            /* Put it in the cache */
            blockCache.put(address, block);
            //if (log.isLoggable(Level.FINEST))
            //    log.finest("request to get block for address 0x" + hex(address) + " met by file read");
        //} else {
            //cacheHits++;
            //if (log.isLoggable(Level.FINEST))
            //    log.finest("request to get block for address 0x" + hex(address) + " met by cache");
        }
        /* Store the last block read as a quick cache */
        putBlockInQuickCache(address, block);
        return block;
    }

    /**
     * Read an unsigned byte at the specified address.
     * @throws IOException if the given address is not present in this address space
     */
    public int read(long address) throws IOException {
        return (int)getBlock(address)[(int)address & 0xfff] & 0xff;
    }

    /**
     * Read an int at the specified address.
     * @throws IOException if the given address is not present in this address space
     */
    public int readInt(long address) throws IOException {
    	int offset = (int)address & 0xfff;
    	try {
    		if (offset <= 0xffc) {
    			return Dump.readInt(getBlock(address), offset);
    		}
    	} catch (ArrayIndexOutOfBoundsException e) {
    		// Can get this because MutableAddressSpace allocates partial blocks
    		throw new IOException("address " + hex(address) + " not in dump");
    	}
    	int ch1 = read(address++);
    	int ch2 = read(address++);
    	int ch3 = read(address++);
    	int ch4 = read(address++);
    	return (ch1 << 24) | (ch2 << 16) | (ch3 << 8) | ch4;
    }

    /**
     * Read an unsigned byte at the specified address.
     * @throws IOException if the given address is not present in this address space
     */
    public int readUnsignedByte(long address) throws IOException {
        return read(address);
    }

    /**
     * Read a (signed) byte at the specified address.
     * @throws IOException if the given address is not present in this address space
     */
    public byte readByte(long address) throws IOException {
        return (byte)readUnsignedByte(address);
    }

    /**
     * Read an unsigned short at the specified address.
     * @throws IOException if the given address is not present in this address space
     */
    public int readUnsignedShort(long address) throws IOException {
		int ch1 = read(address);
		int ch2 = read(address + 1);

		return (ch1 << 8) | ch2;
    }

    /**
     * Read a (signed) short at the specified address.
     * @throws IOException if the given address is not present in this address space
     */
    public short readShort(long address) throws IOException {
        return (short)readUnsignedShort(address);
    }
    
    /**
     * Read an unsigned int at the specified address.
     * @throws IOException if the given address is not present in this address space
     */
    public long readUnsignedInt(long address) throws IOException {
        long i = readInt(address);
        return i & 0xffffffffL;
    }
    
    /**
     * Read a long at the specified address.
     * @throws IOException if the given address is not present in this address space
     */
    public long readLong(long address) throws IOException {
    	int offset = (int)address & 0xfff;
    	try {
    		if (offset <= 0xff8) {
    			return Dump.readLong(getBlock(address), offset);
    		}
    	} catch (ArrayIndexOutOfBoundsException e) {
    		// Can get this because MutableAddressSpace allocates partial blocks
    		throw new IOException("address " + hex(address) + " not in dump");
    	}
      long upper = readInt(address);
      long lower = readInt(address + 4);
      return (upper << 32) | (lower & 0xffffffffL);
    }

    /**
     * Read a word at the specified address. A word is either 32 or 64 bits depending on
     * the machine context.
     */
    public long readWord(long address) throws IOException {
        return is64bit ? readLong(address) : (long)readInt(address) & 0xffffffffL;
    }

    /**
     * Read a word at the specified address. A word is either 32 or 64 bits depending on
     * the flag passed.
     */
    public long readWord(long address, boolean is64bitMode) throws IOException {
        return is64bitMode ? readLong(address) : (long)readInt(address) & 0xffffffffL;
    }

    /**
     * Returns the word length in bytes (either 4 or 8 bytes depending on the machine context).
     */
    public int getWordLength() {
        return is64bit ? 8 : 4;
    }

    /**
     * Reads len bytes of data from the given address into an array of bytes.
     * @param address the address to read from
     * @param b - the buffer into which the data is read
     * @param off - the start offset in array b at which the data is written
     * @param len - the number of bytes to read
     */
    public void read(long address, byte[] b, int off, int len) throws IOException {
        for (;;) {
            byte[] block = getBlock(address);
            int blockOffset = (int)(address & 0x7fffffff) % block.length;
            int blockRemainder = block.length - blockOffset;
            if (len <= blockRemainder) {
                System.arraycopy(block, blockOffset, b, off, len);
                return;
            }
            System.arraycopy(block, blockOffset, b, off, blockRemainder);
            address += blockRemainder;
            off += blockRemainder;
            len -= blockRemainder;
        }
    }

    /**
     * Read an EBCDIC String. This method expects to find a sequence of EBCDIC bytes at the
     * given address.
     * @param address the start address of the EBCDIC bytes
     * @param length the number of bytes to be read
     * @throws IOException if the given address is not present in this address space
     */
    public String readEbcdicString(long address, int length) throws IOException {
        byte[] b = new byte[length];
        read(address, b, 0, length);
        return CharConversion.getEbcdicString(b);
    }

    /**
     * Read an EBCDIC String. Assumes that address points to the halfword length immediately
     * followed by the EBCDIC bytes.
     * @param address the start address of the EBCDIC bytes
     * @throws IOException if the given address is not present in this address space
     */
    public String readEbcdicString(long address) throws IOException {
        int length = readUnsignedShort(address);
        return readEbcdicString(address + 2, length);
    }

    /**
     * Read an EBCDIC String. Assumes that address points to a null-terminated EBCDIC string.
     * @param address the start address of the EBCDIC bytes
     * @throws IOException if the given address is not present in this address space
     */
    public String readEbcdicCString(long address) throws IOException {
        int length = 0;
        for (long addr = address; read(addr) != 0; addr++, length++);
        return readEbcdicString(address, length);
    }

    /**
     * Read an ASCII String. This method expects to find a sequence of ASCII bytes at the
     * given address.
     * @param address the start address of the ASCII bytes
     * @param length the number of bytes to be read
     * @throws IOException if the given address is not present in this address space
     */
    public String readAsciiString(long address, int length) throws IOException {
        StringBuffer buf = new StringBuffer();

        for (int i = 0; i < length; i++) {
            buf.append((char)read(address++));
        }
        return buf.toString();
    }

    /**
     * Read the ASCII string at the given address. Assumes that address points to a
     * null-terminated ASCII C string.
     * @param address the start address of the ASCII bytes
     * @throws IOException if the given address is not present in this address space
     */
    public String readAsciiCString(long address) throws IOException {
        StringBuffer buf = new StringBuffer();
        int c;

        while ((c = read(address++)) != 0) {
            buf.append((char)c);
        }
        return buf.toString();
    }

    /**
     * Read an ASCII String. Assumes that address points to the halfword length immediately
     * followed by the ASCII bytes.
     * @param address the start address of the ASCII bytes
     * @throws IOException if the given address is not present in this address space
     */
    public String readAsciiString(long address) throws IOException {
        int length = readUnsignedShort(address);
        return readAsciiString(address + 2, length);
    }

    /**
     * Read a UTF8 String. Assumes that address points to the halfword length immediately
     * followed by the utf8 bytes.
     * XXX Despite the name no UTF8 conversion is done yet! (i.e. assumes ASCII)
     * @param address the start address of the structure
     * @throws IOException if the given address is not present in this address space
     */
    public String readUtf8String(long address) throws IOException {
        int length = readUnsignedShort(address);
        return readAsciiString(address + 2, length);
    }

    /**
     * Reads the value of a field in a template at the given address. Provided as a convenience.
     * @param address the address of the base of the template
     * @param template the template that describes the structure at that address
     * @param field the name of the field
     * @throws NoSuchFieldException if there is no such field in that type
     * @throws IOException if any errors occurred on reading from the stream
     */
    public long readLong(long address, Template template, String field) throws NoSuchFieldException, IOException {
        return template.getField(field).readLong(getImageInputStream(), address);
    }

    /**
     * Returns true if this is a 64-bit machine
     */
    public boolean is64bit() {
        return is64bit;
    }

    /**
     * Sets whether this is a 64-bit machine. Unfortunately there is no way of knowing at
     * the beginning if we are running in 64-bit mode, this is only discovered in le/Caa.
     * So perhaps this knowledge should only be in the le layer?
     */
    public void setIs64bit(boolean is64bit) {
        log.fine("Set address space " + asid + " as " + (is64bit ? "64-bit" : "32-bit"));
        this.is64bit = is64bit;
    }

    /**
     * Map an address to an offset in the dump file. This is the offset of the block header.
     */
    void mapAddressToFileOffset(long address, long offset) {
        /*
         * You can get the same address mapped to different offsets in the dump and the contents
         * vary slightly. I'm not sure why this is or what to do about it, but for now we just
         * ignore any duplicates.
         */
        if (addressMap.get(address) != -1) {
            if (log.isLoggable(Level.FINER))
                log.finer("duplicate address 0x" + hex(address) + " for asid " + asid);
            return;
        }
        if (log.isLoggable(Level.FINER))
            log.finer("mapping address 0x" + hex(address) + " to offset " + hex(offset) + " for asid " + asid);
        addressMap.put(address, offset);
    }

    private static String hex(int i) {
        return Integer.toHexString(i);
    }

    private static String hex(long i) {
        return Long.toHexString(i);
    }

    /**
     * Returns the address space id as a hex string.
     */
    public String toString() {
        return hex(asid);
    }

    /**
     * Sets the dump (only used when an AddressSpace has been deserialized).
     */
    void setDump(Dump dump) {
        this.dump = dump;
    }

    /**
     * Serialize this object.
     */
    private void writeObject(ObjectOutputStream out) throws IOException {
        out.writeInt(asid);
        out.writeObject(addressMap);
    }

    /**
     * Unserialize this object.
     */
    private void readObject(ObjectInputStream in) throws IOException, ClassNotFoundException {
        asid = in.readInt();
        addressMap = (IntegerMap)in.readObject();
        initialize();
    }

    public boolean equals(Object obj) {
        AddressSpace space = (AddressSpace)obj;
        return space.asid == asid && space.dump.equals(dump);
    }

    public int hashCode() {
        return asid * dump.hashCode();
    }
}
