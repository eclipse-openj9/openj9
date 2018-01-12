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
package com.ibm.dtfj.corereaders.zos.dumpreader;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.io.RandomAccessFile;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Date;
import java.util.logging.Level;
import java.util.logging.Logger;

import javax.imageio.stream.FileImageInputStream;
import javax.imageio.stream.ImageInputStream;
import javax.imageio.stream.ImageInputStreamImpl;

import com.ibm.dtfj.corereaders.zos.util.CharConversion;
import com.ibm.dtfj.corereaders.zos.util.Clib;
import com.ibm.dtfj.corereaders.zos.util.CompressedRecordArray;
import com.ibm.dtfj.corereaders.zos.util.ObjectMap;

/**
 * This class represents an svcdump. It is is the main class in the dumpreader package and
 * provides low-level access to the contents of an svcdump (eg the ability to read the contents
 * of an address in a given address space).
 * <p>
 * Implementation note: currently implemented by linking to the old svcdump.jar code (so you
 * need to include svcdump.jar when using) but this dependency will eventually be removed.
 * Note: zebedee dependency on svcdump.jar has been removed
 *
 * @has - - - com.ibm.dtfj.corereaders.zos.dumpreader.AddressSpace
 * @depend - - - com.ibm.dtfj.corereaders.zos.dumpreader.SearchListener
 * @depend - - - com.ibm.dtfj.corereaders.zos.util.Clib
 * @depend - - - com.ibm.dtfj.corereaders.zos.util.CompressedRecordArray
 */

public final class Dump extends ImageInputStreamImpl {

    //com.ibm.jvm.ras.svcdump.Dump dump;
    /** The dump filename */
    private String filename;
    /** The file pointer if this is an MVS dataset */
    private long fp;
    /** True if this is an MVS dataset */
    private boolean isMvsDataset;
    /** The file object to use if not an MVS dataset */
    private ImageInputStream raf;
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
    /** This is the special "root" address space with asid 1. I don't know why, but for some
        low addresses you have to look in here. */
    AddressSpace rootSpace;
    /** Contains the fpos positions for every block in an MVS dataset */
    private CompressedRecordArray fposArray;
    private int[] fpos = new int[8];
    /** Size of a block header */
    static final int HEADERSIZE = 0x40;
    /** Size of a block excluding header */
    static final int DATABLOCKSIZE = 0x1000;
    /** Size of a block including header */
    static final int BLOCKSIZE = DATABLOCKSIZE + HEADERSIZE;
    private static final int DR1 = 0xc4d9f140;
    private static final int DR2 = 0xc4d9f240;
    /** Logger */
    private static Logger log = Logger.getLogger(Dump.class.getName());

    /**
     * Returns true if the given filename is a valid svcdump.
     */
    public static boolean isValid(String filename) {
        try {
        	// RNC: never create .zcache files in DTFJ
            new Dump(filename, null, false, null, true);
            return true;
        } catch (FileNotFoundException e) {
            return false;
        }
    }

    /**
     * Creates a new instance of Dump from the given filename. The filename must represent
     * either an absolute or relative pathname of the dump file, or if running on z/os it can
     * be an MVS dataset name (eg DGRIFF.MYDUMP).
     * @param filename the name of the dump to open
     * @throws FileNotFoundException
     */
    public Dump(String filename) throws FileNotFoundException {
    	// RNC: never create .zcache files in DTFJ
        this(filename, null, true, null, false);
    }
    
    /**
     * Creates a new instance of Dump from the given filename. The filename must represent
     * either an absolute or relative pathname of the dump file, or if running on z/os it can
     * be an MVS dataset name (eg DGRIFF.MYDUMP).
     * Alternatively an ImageInputStream can be provided if the file is already open.
     * @param filename the name of the dump to open, also used to open the cache file
     * @param stream an ImageInputStream for the dump or null - used in preference to opening the file
     * @param createCacheFile Create a cache file based on the filename
     * @throws FileNotFoundException
     */
    public Dump(String filename, ImageInputStream stream, boolean createCacheFile) throws FileNotFoundException {
    	// RNC: never create .zcache files in DTFJ
        this(filename, null, true, stream, createCacheFile);
    }

    /**
     * Creates a new instance of Dump from the given filename. The filename must represent
     * either an absolute or relative pathname of the dump file, or if running on z/os it can
     * be an MVS dataset name.
     * <p>
     * This constructor also supplies an array of {@link SearchListener}s
     * to inform the caller of matches found whilst scanning the dump. Note that this will
     * force a scan of the dump which is otherwise normally only done the first time this dump
     * is opened. (XXX can probably avoid this by cacheing earlier search results).
     * @param filename the name of the dump to open
     * @param listeners the array of {@link SearchListener}s to be notified of interesting matches
     * @throws FileNotFoundException
     */
    public Dump(String filename, SearchListener[] listeners) throws FileNotFoundException {
    	// RNC: never create .zcache files in DTFJ
        this(filename, listeners, true, null, true);
    }

    /**
     * This is the private constructor where the real work is done.
     * @param filename the name of the dump to open
     * @param listeners the array of {@link SearchListener}s to be notified of interesting matches
     * @param buildAddressSpaces true if the list of address spaces is also to be built
     * @param createCacheFile Whether to create a cache file
     * @throws FileNotFoundException
     */
    private Dump(String filename, SearchListener[] listeners, boolean buildAddressSpaces, ImageInputStream instream, boolean createCacheFile) throws FileNotFoundException {
        log.fine("opening dump " + filename+" stream "+instream);
        this.filename = filename;
        if (listeners != null)
            throw new Error("tbc");
        try {
            //dump = new com.ibm.jvm.ras.svcdump.Dump(filename);
            //log.fine("finished opening old svcdump");
        	if (instream != null) {
        		raf = instream;
        	} else { 
        		raf = new FileImageInputStream(new RandomAccessFile(filename, "r"));
        	}
            if (buildAddressSpaces) {
                buildAddressSpaces(createCacheFile);
                log.fine("finished building address spaces");
            }
            return;
        } catch (Exception e) {
            if (!System.getProperty("os.arch").equals("390")) {
                FileNotFoundException e2 = new FileNotFoundException("Could not find: " + filename+ " stream " + instream);
                e2.initCause(e);
                throw e2;
            }
        }
        openMvsDataset();
        if (buildAddressSpaces) {
            buildAddressSpaces(createCacheFile);
            log.fine("finished building address spaces");
        }
    }

    private void openMvsDataset() throws FileNotFoundException {
        try {
            /*
             * Ok, we couldn't find the file and we're running on z/OS so maybe this is
             * an MVS dataset. Load the native library and try to open it with fopen.
             */

            /*
             * Following code allows us to bundle the lib inside the jar file. Wish there was
             * an easier way!
             */
            log.fine("trying to open dump as an MVS dataset");
            String version = System.getProperty("java.version");
            InputStream is = getClass().getResourceAsStream("libzebedee" + version.charAt(2) + ".so");
            BufferedInputStream bis = new BufferedInputStream(is);
            File tmp = File.createTempFile("lib", ".so");
            java.lang.Process proc = Runtime.getRuntime().exec("chmod +x " + tmp.getPath());
            proc.waitFor();
            int exitcode = proc.exitValue();
            FileOutputStream fos = new FileOutputStream(tmp);
            BufferedOutputStream bos = new BufferedOutputStream(fos);
            byte[] b = new byte[10000];
            for (int n = 0; (n = bis.read(b)) != -1;) {
                bos.write(b, 0, n);
            }
            bos.close();
            tmp.deleteOnExit();
            System.load(tmp.getPath());
            log.fine("lib loaded ok");
            /*
             * Phew, we've loaded the library at last, now call fopen.
             */
            fp = Clib.fopen("//'" + filename + "'", "rb");
            if (fp == 0) {
                throw new FileNotFoundException("Could not find either " + filename + " or //" + filename);
            }
            isMvsDataset = true;
            log.fine("MVS dataset opened ok");
        } catch (Exception e) {
            throw new FileNotFoundException("Could not find: " + filename);
        }
    }

    /**
     * Build the array of AddressSpaces. If this is the first time this dump has been opened
     * (no cache file exists) then every block of the dump is scanned to build a mapping for
     * each AddressSpace of address to block number (each block has a header containing the
     * AddressSpace id and the address it maps to). At the same time if this is an MVS dataset,
     * the table of fpos info is built. 
     * <p>
     * If a cache file exists then the AddressSpace and fpos info is read from that instead.
     * @param createCacheFile Whether to create a new cache file
     */
    private void buildAddressSpaces(boolean createCacheFile) {
        try {
            try {
                /*
                 * First try and get everything from the cache file.
                 */
            	if (filename == null) throw new FileNotFoundException("No filename supplied so no cache available");
            	File fn = new File(filename + ".zcache");
                readCacheFile(fn);
            } catch (Exception e) {
                log.log(Level.FINE, "cache read failed: ", e);
                /*
                 * Well reading from the cache file didn't work so create a new cache file
                 * and scan the dump.
                 */
                log.fine("no cache file, scanning file");
                byte[] blockHeader = null;
                /* fpos stuff is for MVS datasets only */
                fposArray = new CompressedRecordArray(5, fpos.length);
                /* Address space map */
                ObjectMap asidMap = new ObjectMap();
                for (long offset = 0; ; offset += BLOCKSIZE) {
                    /* Seek to this block header */
                    if (isMvsDataset) {
                        if (offset > 0) {
                            /*
                             * For MVS datasets we have to be careful how we do the seek.
                             * fseek is basically broken, it gives horrendous results when
                             * you seek to an absolute position owing to the fact it must scan
                             * from the start of the file each time to cater for "holes" in
                             * the file. So we seek from the current position (just after the
                             * last read).
                             */
                            int rc = Clib.fseek(fp, BLOCKSIZE - HEADERSIZE, Clib.SEEK_CUR);
                            assert rc == 0 : rc;
                        }
                    } else {
                        raf.seek(offset);
                    }
                    /*
                     * Read the block header.
                     */
                    if (offset == 0)
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

                    if (isMvsDataset) {
                        /*
                         * For MVS datasets we now squirrel away the fpos info. Once we're done
                         * scanning the file, from then on we will use fsetpos rather than fseek
                         * due to the aforementioned performance problems with fseek. The fpos
                         * structure contains sufficient info for fsetpos to go straight there.
                         */
                        int rc = Clib.fgetpos(fp, fpos);
                        assert rc == 0 : rc;
                        fposArray.add(fpos);
                    }
                    /* Get the eyecatcher which should be either "DR1 " (old format) or "DR2 " */
                    int eye = readInt(blockHeader, 0);
                    boolean oldFormat = false;
                    if (eye == DR1) {
                        oldFormat = true;
                    } else if (eye != DR2) {
                        throw new Error("bad eyecatcher " + hex(eye) + " at offset " + offset);
                    }
                    /* Get the address space id */
                    int asid = readInt(blockHeader, 12);
                    /* Ignore the ones with weird asids */
                    if (asid < 0)
                        continue;
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
                    if (offset == 0) {
                        /* This is the first block so save away the date and title */
                        byte[] titlebuf = new byte[100];
                        System.arraycopy(blockHeader, 0x58, titlebuf, 0, 100);
                        title = CharConversion.getEbcdicString(titlebuf);
                        productName = CharConversion.getEbcdicString(blockHeader, 0xf4, 16);
                        productVersion = CharConversion.getEbcdicString(blockHeader, 0x104, 2);
                        productRelease = CharConversion.getEbcdicString(blockHeader, 0x106, 2);
                        long c1 = readInt(blockHeader, 0x12 * 4);
                        long c2 = readInt(blockHeader, 0x13 * 4);
                        creationDate = mvsClockToDate((c1 << 32) | (c2 & 0xffffffffL));
                    }
                }
                fposArray.close();
                if (isMvsDataset) log.fine("fposArray used " + fposArray.memoryUsage());
                spaces = (AddressSpace[])asidMap.toArray(new AddressSpace[0]);
                Arrays.sort(spaces, new Comparator() {
                    public int compare(Object o1, Object o2) {
                        AddressSpace space1 = (AddressSpace)o1;
                        AddressSpace space2 = (AddressSpace)o2;
                        return space1.getAsid() - space2.getAsid();
                    }
                });
                // Reset the file pointer for the Dump ImageInputStream
                seek(0);
                log.fine("scan complete");
                // Create the cache if required
                if (createCacheFile && filename != null) {
                	File cacheFile = new File(filename + ".zcache");
                	createCacheFile(cacheFile);
                }
            }
        } catch (IOException e) {
            throw new Error("Unexpected exception reading address spaces", e);
        }
    }

	/**
	 * Read the cache file in
	 * @param cacheFile
	 * @throws FileNotFoundException
	 * @throws IOException
	 * @throws ClassNotFoundException
	 */
	private void readCacheFile(File cacheFile) throws FileNotFoundException,
			IOException, ClassNotFoundException {
		FileInputStream fis = new FileInputStream(cacheFile);
		ObjectInputStream ois = new ObjectInputStream(fis);
		log.fine("cache file exists, reading contents");
		spaces = (AddressSpace[])ois.readObject();
		/* Got the spaces, now set the dump to us */
		for (int i = 0; i < spaces.length; i++) {
		    spaces[i].setDump(this);
		    /* Save the "root" address space */
		    if (spaces[i].getAsid() == 1)
		        rootSpace = spaces[i];
		}
		fposArray = (CompressedRecordArray)ois.readObject();
		title = (String)ois.readObject();
		creationDate = (Date)ois.readObject();
        productName = (String)ois.readObject();
        productVersion = (String)ois.readObject();
        productRelease = (String)ois.readObject();
        productModification = (String)ois.readObject();
		ois.close();
	}

	/**
	 * Create a cache file for the dump
	 * @param cacheFile
	 */
	private void createCacheFile(File cacheFile) {
		try {                		
			FileOutputStream fos = new FileOutputStream(cacheFile);
			try {
				ObjectOutputStream oos = new ObjectOutputStream(fos);
				try {
					oos.writeObject(spaces);
					oos.writeObject(fposArray);
					oos.writeObject(title);
					oos.writeObject(creationDate);
					oos.writeObject(productName);
                    oos.writeObject(productVersion);
                    oos.writeObject(productRelease);
                    oos.writeObject(productModification);
					log.fine("created cache file "+cacheFile);
				} finally {
					oos.close();
				}
			} catch (IOException e) {
				log.log(Level.FINE, "Unable to create cache file "+cacheFile, e);
				// An incomplete cache file is useless, so delete it
				cacheFile.delete();                		
			} finally {
				fos.close();
			}
		} catch (IOException e) {
			// Can't create the cache file, so give up
			log.log(Level.FINE, "Unable to create cache file "+cacheFile, e);
		}
	}

    /**
     * Read an int from a byte array at a given offset.
     * @param buf the byte array to read from
     * @param offset the offset in the byte array
     */
    static int readInt(byte[] buf, int offset) {
        int n = (((int)buf[offset] & 0xff) << 24) | (((int)buf[offset+1] & 0xff) << 16) | (((int)buf[offset+2] & 0xff) << 8) | ((int)buf[offset+3] & 0xff);
        if (log.isLoggable(Level.FINEST))
            log.finest("read int 0x" + hex(n) + " at offset 0x" + hex(offset));
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
        if (isMvsDataset) {
            /* Get the fpos that we squirreled away earlier */
            long blockNumber = offset / BLOCKSIZE;
            fposArray.get((int)blockNumber, fpos);
            /* Go straight to that position (much faster than fseek) */
            int rc = Clib.fsetpos(fp, fpos);
            if (rc != 0)
                throw new IOException("fsetpos failed");
            /*
             * We are now positioned at the correct block just after the header. If this is
             * not the desired position, adjust.
             */
            long remainder = offset - ((blockNumber * BLOCKSIZE) + HEADERSIZE);
            if (remainder != 0) {
throw new Error("TEMP!"); // XXX
                //rc = Clib.fseek(fp, remainder, Clib.SEEK_CUR);
                //if (rc != 0)
                    //throw new IOException("fseek failed");
            }
        } else {
            if (log.isLoggable(Level.FINER))
                log.finer("raf seek to offset 0x" + hex(offset));
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
        if (log.isLoggable(Level.FINER))
        	log.finer("Read into "+buf+" "+hex(off)+" "+hex(len));
    	int n;
        if (isMvsDataset) {
        	if (off == 0) {
        		n = Clib.fread(buf, 1, len, fp);
        	} else {
        		byte buf2[] = new byte[len];
        		n = Clib.fread(buf2, 1, len, fp);
        		if (n > 0) {
        			System.arraycopy(buf2, 0, buf, off, n);
        		}
        	}
        } else {
            n = raf.read(buf, off, len);
        }
        
        if (n > 0) streamPos += n;
        
        if (log.isLoggable(Level.FINER))
            log.finer("read 0x" + hex(n) + " bytes");
        
        return n;
    }
    
    /**
     * Read from the dump file into the given buffer.
     * @param buf the buffer to read into
     */
    public int read(byte[] buf) throws IOException {
        if (log.isLoggable(Level.FINER))
        	log.finer("Read into "+buf+" "+hex(buf.length));
    	int n;
        if (isMvsDataset) {
            n = Clib.fread(buf, 1, buf.length, fp);
        } else {
            n = raf.read(buf);
        }
        if (n > 0) streamPos += n;
        if (log.isLoggable(Level.FINER))
            log.finer("read 0x" + hex(n) + " bytes");
        return n;
    }
    
    /**
     * Get the length of the dump file
     */
    public long length() {
        if (isMvsDataset) {
            return -1;
        } else {
        	try {
        		return raf.length();
        	} catch (IOException e) {
        		return -1;
        	}
        }    	
    }

    /**
     * Returns an array of all the address spaces in the dump.
     */
    public AddressSpace[] getAddressSpaces() {
        return spaces;
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

    static String hex(int n) {
        return Integer.toHexString(n);
    }

    static String hex(long n) {
        return Long.toHexString(n);
    }
}
