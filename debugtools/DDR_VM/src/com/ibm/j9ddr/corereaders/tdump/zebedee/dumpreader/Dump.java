/*******************************************************************************
 * Copyright (c) 2006, 2015 IBM Corp. and others
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

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.RandomAccessFile;
import java.lang.ref.SoftReference;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;
import javax.imageio.stream.ImageInputStreamImpl;
import javax.imageio.stream.MemoryCacheImageInputStream;

import com.ibm.j9ddr.corereaders.tdump.zebedee.util.CharConversion;
import com.ibm.j9ddr.corereaders.tdump.zebedee.util.FileFormatException;
import com.ibm.j9ddr.corereaders.tdump.zebedee.util.ObjectMap;

/**
 * This class represents an svcdump. It is is the main class in the dumpreader package and
 * provides low-level access to the contents of an svcdump (eg the ability to read the contents
 * of an address in a given address space).
 * <p>
 * Implementation note: currently implemented by linking to the old svcdump.jar code (so you
 * need to include svcdump.jar when using) but this dependency will eventually be removed.
 *
 * @has - - - com.ibm.zebedee.dumpreader.AddressSpace
 * @depend - - - com.ibm.zebedee.dumpreader.SearchListener
 * @depend - - - com.ibm.zebedee.util.Clib
 * @depend - - - com.ibm.zebedee.util.CompressedRecordArray
 */

public final class Dump extends ImageInputStreamImpl {
	
    /** The dump filename */
    private String filename;
    
    /** The input stream for reading the dump file */
    private ImageInputStream raf;
    private BufferedInputStream inputStream;
    private BufferedOutputStream outputStream;
    /** The array of AddressSpaces */
    private AddressSpace[] spaces;
    /** The title of the dump */
    private String title;
    /** The z/OS product name */
    private String productName;
    /** The z/OS product version */
    private String productVersion;
    /** The z/OS product release */
    private String productRelease;
    /** The z/OS product modification */
    private String productModification;
    /** The date when the dump was created */
    private Date creationDate;
    private long fileSize;
    /** This is the special "root" address space with asid 1. I don't know why, but for some
        low addresses you have to look in here. */
    AddressSpace rootSpace;
    /** This is the "High Virtual Shared Data" space. (I think) */
    AddressSpace highVirtualSharedSpace;
    /** The SystemTrace object - held as a SoftReference */
    private SoftReference<SystemTrace> systemTraceRef;
    /** Size of a block header */
    static final int HEADERSIZE = 0x40;
    /** Size of a block excluding header */
    public static final int DATABLOCKSIZE = 0x1000;
    /** Size of a block including header */
    static final int BLOCKSIZE = DATABLOCKSIZE + HEADERSIZE;
    private static final int DR1 = 0xc4d9f140;
    private static final int DR2 = 0xc4d9f240;
    /** Logger */
    private static Logger log = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);

    /**
     * Returns true if the given filename is a valid svcdump.
     */
    public static boolean isValid(String filename) {
        try {
            new Dump(filename, false);
            return true;
        } catch (FileNotFoundException e) {
            return false;
        } catch (FileFormatException e) {
            return false;
        }
    }

    /**
     * Creates a new instance of Dump from the given filename. The filename must represent
     * either an absolute or relative pathname of the dump file, or if running on z/os it can
     * be an MVS dataset name (eg DGRIFF.MYDUMP).
     * @param filename the name of the dump to open
     * @throws FileNotFoundException
     * @throws FileFormatException
     */
    public Dump(String filename) throws FileNotFoundException, FileFormatException {
        this(filename, true);
    }
    
    public Dump(ImageInputStream in) throws FileNotFoundException, FileFormatException {
        this(in, true);
    }

    /**
     * Creates a new instance of Dump from the given filename. The filename must represent
     * either an absolute or relative pathname of the dump file, or if running on z/os it can
     * be an MVS dataset name (eg DGRIFF.MYDUMP).
     * @param filename the name of the dump to open
     * @param initialize whether to initialize the dump as well
     * @throws FileNotFoundException
     * @throws FileFormatException
     */
    public Dump(ImageInputStream in, boolean initialize) throws FileNotFoundException, FileFormatException {
        log.fine("Initialising from input stream");
        this.filename = "DataStream";
        raf = in;
        fileSize = -1;
        readFirstBlock();
        if (initialize)
            initialize();
    }
    
    /**
     * Creates a new instance of Dump from the given filename. The filename must represent
     * either an absolute or relative pathname of the dump file, or if running on z/os it can
     * be an MVS dataset name (eg DGRIFF.MYDUMP).
     * @param filename the name of the dump to open
     * @param initialize whether to initialize the dump as well
     * @throws FileNotFoundException
     * @throws FileFormatException
     */
    public Dump(String filename, boolean initialize) throws FileNotFoundException, FileFormatException {
        log.fine("opening dump " + filename);
        this.filename = filename;
        try {
            raf = new FileImageInputStream(new RandomAccessFile(filename, "r"));
            fileSize = new File(filename).length();
        } catch (FileNotFoundException e) {
            if (System.getProperty("os.arch").indexOf("390") == -1) {
                throw e;
            }
            
            try {
                raf = new MVSFileReader(filename);
            } catch (Exception ex) {
            	log.info("could not load dump: " + ex.getMessage());
				throw e;
			}
        } catch (LinkageError le) {
            log.info("could not load recordio: " + le);
            throw le;
        }
        readFirstBlock();
        if (initialize)
            initialize();
    }

    /**
     * Creates a new instance of Dump from the given InputStream. The stream must represent
     * either an absolute or relative pathname of the dump file, or if running on z/os it can
     * be an MVS dataset name (eg DGRIFF.MYDUMP).
     * @param stream the stream to read from
     * @param initialize whether to initialize the dump as well
     * @throws FileNotFoundException
     * @throws FileFormatException
     */
    public Dump(InputStream stream, boolean initialize) throws FileNotFoundException, FileFormatException {
        log.fine("Creating dump image from input stream");
        this.filename = "stream";
        raf = new MemoryCacheImageInputStream(stream);
        readFirstBlock();
        if (initialize)
            initialize();
    }
    
    /**
     * Creates a new instance of Dump from the given input stream (constructor used by
     * J9 I think).
     * @param filename the name of the dump - also used to open the cache file
     * @param stream an ImageInputStream for the dump or null - used in preference to opening the file
     * @throws FileNotFoundException
     * @throws FileFormatException
     */
    public Dump(String filename, ImageInputStream stream) throws FileNotFoundException, FileFormatException {
        this.filename = filename;
        raf = stream;
        fileSize = new File(filename).length();
        readFirstBlock();
        initialize();
    }

    public Dump(BufferedInputStream inputStream, String filename) throws FileNotFoundException, FileFormatException {
        this.inputStream = inputStream;
        this.filename = filename;
        outputStream = new BufferedOutputStream(new FileOutputStream(filename));
        readFirstBlock();
        initialize();
    }


    private void readFirstBlock() throws FileNotFoundException, FileFormatException {
        try {
            byte[] blockHeader = new byte[0x200];
            if (inputStream != null) {
                inputStream.mark(blockHeader.length * 2);
            }
            readFully(blockHeader);
            /* Get the eyecatcher which should be either "DR1 " (old format) or "DR2 " */
            int eye = readInt(blockHeader, 0);
            if (eye != DR1 && eye != DR2) {
                throw new FileFormatException("bad eyecatcher " + hex(eye) + " in first block");
            }
            title = CharConversion.getEbcdicString(blockHeader, 0x58, 100);
            productName = CharConversion.getEbcdicString(blockHeader, 0xf4, 16);
            productVersion = CharConversion.getEbcdicString(blockHeader, 0x104, 2);
            productRelease = CharConversion.getEbcdicString(blockHeader, 0x106, 2);
            productModification = CharConversion.getEbcdicString(blockHeader, 0x108, 2);
            long c1 = readInt(blockHeader, 0x12 * 4);
            long c2 = readInt(blockHeader, 0x13 * 4);
            creationDate = mvsClockToDate((c1 << 32) | (c2 & 0xffffffffL));
            if (inputStream != null) {
                inputStream.reset();
            } else {
                raf.seek(0);
            }
        } catch (IOException e) {
            throw new FileFormatException("oops: " + e);
        }
    }

    /**
     * Scan the dump and build the array of AddressSpaces. Every block of the dump is read to build a
     * mapping of block memory addresses to file offsets, for each address space in the dump 
     * 
     * Each block has a header containing the AddressSpace ID and the memory address the block maps to.
     */
    public void initialize() throws FileNotFoundException, FileFormatException {
        try {
            log.fine("Scanning dump file");
            byte[] blockHeader = null;
            /* Address space map */
            ObjectMap asidMap = new ObjectMap();
            for (long offset = 0; ; offset += BLOCKSIZE) {
                /* Seek to this block header */
                if (inputStream == null) {
                    raf.seek(offset);
                }
                /*
                 * Read the block header.
                 */
                if (inputStream != null) {
                    if (blockHeader == null)
                        blockHeader = new byte[BLOCKSIZE];
                } else if (offset == 0)
                    /* We read a little bit more of the first block */
                    blockHeader = new byte[0x200];
                else if (offset == BLOCKSIZE)
                    blockHeader = new byte[HEADERSIZE];
                try {
                    readFully(blockHeader);
                } catch (EOFException eof) {
                    /* Reached the end of the file so break from the loop */
                    break;
                }
                if (inputStream != null) {
                    outputStream.write(blockHeader, 0, BLOCKSIZE);
                }

                /* Get the eyecatcher which should be either "DR1 " (old format) or "DR2 " */
                int eye = readInt(blockHeader, 0);
                boolean oldFormat = false;
                if (eye == DR1) {
                    oldFormat = true;
                } else if (eye != DR2) {
                    throw new FileFormatException("bad eyecatcher " + hex(eye) + " at offset " + offset);
                }
                
                /* Get the address space id */
                int asid = readInt(blockHeader, 12);
                /* Ignore the ones with weird asids (with the exception of IARHVSHR) */
                if (asid < 0 && asid != 0xc9c1d9c8) {
                    log.fine("ignoring asid " + hex(asid) + " " + hex(readInt(blockHeader, 13)));
                    continue;
                }
                /* Get the logical address */
                long address = oldFormat ? readInt(blockHeader, 20) : readLong(blockHeader, 20);
                if (log.isLoggable(Level.FINER))
                    log.finer("read block, asid = " + hex(asid) + " address = " + hex(address));
                /*
                 * Do we already have an AddressSpace object for this asid? If not, create one
                 */
                AddressSpace space = (AddressSpace)asidMap.get(asid);
                if (space == null) {
                    space = new AddressSpace(this, asid);
                    asidMap.put(asid, space);
                }
                /* Map the address to the current file position */
                space.mapAddressToFileOffset(address, offset + HEADERSIZE);
                /* Save the "root" address space */
                if (asid == 1)
                    rootSpace = space;
                /* Save the high virtual shared space - appears to be mapped by COMPDATA(IARHVSHR) */
                if (asid == 0xc9c1d9c8) {
                    highVirtualSharedSpace = space;
                }
            }
            spaces = (AddressSpace[])asidMap.toArray(new AddressSpace[0]);
            Arrays.sort(spaces, new Comparator<AddressSpace>() {
                public int compare(AddressSpace space1, AddressSpace space2) {
                	return space1.getAsid() - space2.getAsid();
                }
            });
            // Reset the file pointer for the Dump ImageInputStream
            seek(0);
            log.fine("scan complete");
        } catch (FileFormatException e) {
            throw e;
        } catch (Exception e) {
            log.logp(Level.WARNING,"com.ibm.j9ddr.corereaders.tdump.zebedee.dumpreader.Dump", "initialize","Unexpected exception",e);
            throw new Error("unexpected exception: " + e);
        }
    }


    /**
     * Read an int from a byte array at a given offset.
     * @param buf the byte array to read from
     * @param offset the offset in the byte array
     */
    static int readInt(byte[] buf, int offset) {
        int n = (((int)buf[offset] & 0xff) << 24) | (((int)buf[offset+1] & 0xff) << 16) | (((int)buf[offset+2] & 0xff) << 8) | ((int)buf[offset+3] & 0xff);
        return n;
    }

    /**
     * Read a long from a byte array at a given offset.
     * @param buf the byte array to read from
     * @param offset the offset in the byte array
     */
    static long readLong(byte[] buf, int offset) {
        long upper = readInt(buf, offset);
        long lower = readInt(buf, offset + 4);
        return (upper << 32) | (lower & 0xffffffffL);
    }

    /**
     * Seek to the given offset in the dump file. This also handles MVS datasets in an
     * efficient way (simply using fseek is too slow because the seek appears to read the
     * file from the beginning every time!).
     * @param offset the offset to seek to
     */
    // XXX why is this public? not thread safe - absorb into single read method
    public void seek(long offset) throws IOException {
        super.seek(offset);
        if (inputStream == null) {
            raf.seek(offset);
        }
    }

    /**
     * Reads an unsigned byte from the file
     * @return
     * @throws IOException
     */
    public int read() throws IOException {
        byte buf[] = new byte[1];
        int r = read(buf);
        if (r <= 0) return -1;
        return buf[0] & 0xff;
    }
    
    /**
     * Read from the dump file into the given buffer.
     * @param buf the buffer to read into
     * @param off the offset into the buffer to start at
     * @param len the maximum number of bytes to read
     */
    public int read(byte[] buf, int off, int len) throws IOException {
        //if (log.isLoggable(Level.FINER))
        //    log.finer("Read into "+buf+" "+hex(off)+" "+hex(len));
        int n;
        if (inputStream != null) {
            n = inputStream.read(buf, off, len);
        } else {
            n = raf.read(buf, off, len);
        }
        
        if (n > 0) streamPos += n;
        
        //if (log.isLoggable(Level.FINER))
        //    log.finer("read 0x" + hex(n) + " bytes");
        
        return n;
    }
    
    /**
     * Read from the dump file into the given buffer.
     * @param buf the buffer to read into
     */
    public int read(byte[] buf) throws IOException {
        //if (log.isLoggable(Level.FINER))
        //    log.finer("Read into "+buf+" "+hex(buf.length));
        int n;
        if (inputStream != null) {
            n = inputStream.read(buf);
        } else {
            n = raf.read(buf);
        }
        if (n > 0) streamPos += n;
        //if (log.isLoggable(Level.FINER))
        //    log.finer("read 0x" + hex(n) + " bytes");
        return n;
    }
    
    /**
     * Get the length of the dump file
     */
    public long length() {
        return fileSize;
    }

    /**
     * Close the file used to read the dump
     */
    public void close() throws IOException {
        if (inputStream != null) {
            inputStream.close();
            outputStream.close();
        } else {
            raf.close();
        }
    }    

    /**
     * Close the file used to read the dump (MVS datasets only).
     * Added this because need to let it know we have finished with the handle
     * for MVS only. Need to clean this up.
     */
    public void localClose() throws IOException {
        
    }    

    /**
     * Returns an array of all the address spaces in the dump.
     */
    public AddressSpace[] getAddressSpaces() {
        return spaces;
    }

    /**
     * Returns the address space with the given id or null if it can't be found.
     */
    public AddressSpace getAddressSpace(int id) {
        for (int i = 0; i < spaces.length; i++) {
            if (spaces[i].getAsid() == id) {
                return spaces[i];
            }
        }
        return null;
    }

    /**
     * Returns true if this is a 64-bit dump. Always returns false at the moment 'cos I
     * haven't yet got my paws on a 64-bit dump.
     */
    public boolean is64bit() {
        return false;
    }

    /**
     * Returns the title of the dump.
     */
    public String getTitle() {
        return title;
    }

    /**
     * Returns the z/OS product name.
     */
    public String getProductName() {
        return productName;
    }

    /**
     * Returns the z/OS product version.
     */
    public String getProductVersion() {
        return productVersion;
    }

    /**
     * Returns the z/OS product release.
     */
    public String getProductRelease() {
        return productRelease;
    }

    /**
     * Returns the z/OS product modification.
     */
    public String getProductModification() {
        return productModification;
    }

    /**
     * Convert an MVS style clock value to a Date object.
     */
    public static Date mvsClockToDate(long clock) {
        clock >>>= 12;
        clock /= 1000;
        long epoch = ((70 * 365) + 17) * 24;
        epoch *= 3600;
        epoch *= 1000;
        clock -= epoch;
        return new Date(clock);
    }

    /**
     * Returns the time when the dump was created.
     */
    public Date getCreationDate() {
        return creationDate;
    }

    /**
     * Returns an object representing the system trace table.
     */
    public SystemTrace getSystemTrace() throws IOException {
        SystemTrace systemTrace = (systemTraceRef == null ? null : (SystemTrace)systemTraceRef.get());
        if (systemTrace != null)
            return systemTrace;
        /* Search the address spaces for one with the TTCH eyecatcher in the first block */
		for (int i = 0; i < spaces.length; i++) {
            AddressSpace space = spaces[i];
            // read first block eyecatcher
            long lowestAddress = space.getLowestAddress();
            int eyecatcher = space.readInt(lowestAddress);
            if (eyecatcher == 0xe3e3c3c8) {    // TTCH
                systemTrace = new SystemTrace(space);
                systemTraceRef = new SoftReference<SystemTrace>(systemTrace);
                return systemTrace;
            }
        }
        throw new Error("no system trace found!");
    }

    /** Return the pathname for this dump */
    public String getName() {
        return filename;
    }

    static String hex(int n) {
        return Integer.toHexString(n);
    }

    static String hex(long n) {
        return Long.toHexString(n);
    }

    public boolean equals(Object obj) {
        Dump dump = (Dump)obj;
        return dump.filename.equals(filename);
    }

    public int hashCode() {
        return filename.hashCode();
    }
}

final class FileNotFoundError extends Error {
    /**
	 * 
	 */
	private static final long serialVersionUID = -1861283846089542171L;

	FileNotFoundError(String s) {
        super(s);
    }
}
