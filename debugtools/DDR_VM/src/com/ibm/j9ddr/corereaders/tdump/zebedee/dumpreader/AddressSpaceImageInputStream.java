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

import javax.imageio.stream.ImageInputStreamImpl;
import java.io.IOException;
import java.util.logging.*;

/**
 * This class implements ImageInputStream for an AddressSpace. You can use this to read data from
 * an AddressSpace as an alternative to using the methods defined in AddressSpace itself.
 * ImageInputStream provides a rich set of read methods. To use it you must first call seek() with
 * the address you wish to read from and then call the appropriate read method.
 * <p>
 * Note that at present it doesn't fully meet the ImageInputStream semantics in that it won't
 * skip over gaps in the AddressSpace seamlessly. I might add that functionality if the demand
 * is there. Also length returns -1 since it doesn't make sense to return a real value (?).
 */

public final class AddressSpaceImageInputStream extends ImageInputStreamImpl {

    private AddressSpace space;
    /** Logger */
    private static Logger log = Logger.getLogger(com.ibm.j9ddr.corereaders.ICoreFileReader.J9DDR_CORE_READERS_LOGGER_NAME);

    /**
     * Create a new AddressSpaceImageInputStream for the given AddressSpace.
     * @param space the AddressSpace which this stream reads
     */
    public AddressSpaceImageInputStream(AddressSpace space) {
        this.space = space;
    }

    /**
     * Read a byte from the current position. Also increments streamPos.
     */
    public int read() throws IOException {
        return space.read(streamPos++);
    }

    /**
     * Read some bytes from the current position and add len to streamPos.
     */
    public int read(byte[] b, int off, int len) throws IOException {
        space.read(streamPos, b, off, len);
        streamPos += len;
        return len;
    }

    public void seek(long pos) throws IOException {
        if (pos <= 0)
            throw new IOException("attempt to seek to invalid pos: 0x" + hex(pos));
        super.seek(pos);
        if (log.isLoggable(Level.FINER))
            log.finer("seek to 0x" + hex(pos) + " streamPos now 0x" + hex(streamPos));
    }

    /**
     * Read an int from the current position. Also increments streamPos.
     * For efficiency to avoid multiple reads via ImageInputStream
     */
    public int readInt() throws IOException {
        int ret = space.readInt(streamPos);
        streamPos += 4;
        return ret;
    }
    
    /**
     * Read an unsigned int from the current position. Also increments streamPos.
     * For efficiency to avoid multiple reads via ImageInputStream
     */
    public long readUnsignedInt() throws IOException {
        long ret = space.readUnsignedInt(streamPos);
        streamPos += 4;
        return ret;
    }

    /**
     * Read a long from the current position. Also increments streamPos.
     * For efficiency to avoid multiple reads via ImageInputStream
     */
    public long readLong() throws IOException {
        long ret = space.readLong(streamPos);
        streamPos += 8;
        return ret;
    }

    private static String hex(long i) {
        return Long.toHexString(i);
    }
}
