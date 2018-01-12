/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2000, 2016 IBM Corp. and others
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
package com.ibm.jvm.format;

import java.io.IOException;
import java.io.RandomAccessFile;
import java.math.BigInteger;

/** 
 * Encapsulates a trace "file" - In this case it is a single file on the disk.
 *
 * @author Tim Preece
 */
final public class TraceFile extends RandomAccessFile {

    private                 boolean           bigendian = true;    // assume bigendian until we know otherwise
    protected               TraceFileHeader   traceFileHeader;
    private                 String            filespec;
    protected               BigInteger        lastWriteSystem = BigInteger.ZERO;
    protected               long              wrapOffset = 0;

    public TraceFile(String filespec, String mode) throws Exception
    {
        super(filespec,mode);
        this.filespec = filespec;
        traceFileHeader = new TraceFileHeader(this);
    }


    final protected int readI() throws IOException
    {
        int temp = readInt();
        //Util.Debug.println("SingleTraceFile: bigendian:       " + TraceFileHeader.bigendian);
        return( bigendian  ) ? temp : Util.convertEndian(temp) ;

    }

    final protected long readL() throws IOException
    {
        long temp = readLong();
        long temp1 =
        (temp >>> 56)
        | (temp << 56)
        |((temp >> 40)  & 0x0000000000FF00L )
        |((temp >> 24)  & 0x00000000FF0000L )
        |((temp >>  8)  & 0x000000FF000000L )
        |((temp <<  8)  & 0x0000FF00000000L )
        |((temp << 24)  & 0x00FF0000000000L )
        |((temp << 40)  & 0xFF000000000000L );
        //Util.Debug.println("SingleTraceFile: readLong: temp  " + Long.toHexString(temp));
        //Util.Debug.println("SingleTraceFile: readLong: temp1 " + Long.toHexString(temp1));
        return( bigendian  ) ? temp : temp1 ;
    }

    final protected BigInteger readBigInteger(int size)throws IOException
    {
        byte[] bytes = new byte[size];
        read(bytes);
        //for (int i =0;i<size;i++) {
        //    Util.Debug.println("readBigInteger bytes["+i+"] = " + bytes[i]);
        //}
        if ( bigendian == false ) reverseByteOrder(bytes);
        return new BigInteger(1, bytes); // 1 means positive
    }

    /** Reverses the order of the bytes in the byte array
     *
     * @param   data  an array of bytes to be reversed
     */
    final static private void reverseByteOrder(byte[] data)
    {
        for ( int i = 0; i < data.length / 2 ; i++ ) {
            int temp    = data[i];
            data[i]     = data[(data.length - 1) - i];
            data[(data.length - 1) - i] = (byte) temp;
        }
    }

    final protected long readULong() throws IOException
    {
        long temp = readLong();
        long temp1 =
        (temp >>> 56)
        | (temp << 56)
        |((temp >> 40)  & 0x0000000000FF00L )
        |((temp >> 24)  & 0x00000000FF0000L )
        |((temp >>  8)  & 0x000000FF000000L )
        |((temp <<  8)  & 0x0000FF00000000L )
        |((temp << 24)  & 0x00FF0000000000L )
        |((temp << 40)  & 0xFF000000000000L )
        ;
        //     Util.Debug.println("SingleTraceFile: readULong: xxxx  " + Long.toHexString(temp >>> 56));
        //     Util.Debug.println("SingleTraceFile: readULong: xxxx  " + Long.toHexString(temp << 56));
        //     Util.Debug.println("SingleTraceFile: readULong: xxxx  " + Long.toHexString((temp >> 40)  & 0x0000000000FF00L));
        //     Util.Debug.println("SingleTraceFile: readULong: xxxx  " + Long.toHexString((temp >> 24)  & 0x00000000FF0000L));
        //     Util.Debug.println("SingleTraceFile: readULong: xxxx  " + Long.toHexString((temp >>  8)  & 0x000000FF000000L));
        //     Util.Debug.println("SingleTraceFile: readULong: xxxx  " + Long.toHexString((temp <<  8)  & 0x0000FF00000000L));
        //     Util.Debug.println("SingleTraceFile: readULong: xxxx  " + Long.toHexString((temp << 24)  & 0x00FF0000000000L));
        //     Util.Debug.println("SingleTraceFile: readULong: xxxx  " + Long.toHexString((temp << 40)  & 0xFF000000000000L));
        //     Util.Debug.println("SingleTraceFile: readULong: temp  " + Long.toHexString(temp));
        //     Util.Debug.println("SingleTraceFile: readULong: temp1 " + Long.toHexString(temp1));
        return( bigendian  ) ? temp : temp1 ;
    }

    // readString will check for null terminators.
    // readString assumes the character is in ASCII
    final protected String readString(int Length) throws IOException
    {
        byte[] buffer = new byte[Length];
        int endOfString;
        read(buffer,0,Length);

        // see if there is a null terminator before the end of the buffer
        for ( endOfString = 0; endOfString < Length; endOfString++) {
              if ( buffer[endOfString]==0 ) break;
        }

        return new String(buffer,0,endOfString,"8859_1");
    }

    final protected void setBigEndian(boolean endian)
    {
        bigendian = endian;
        return ;
    }

    final protected String formatFileName()
    {
        return traceFileHeader.formatFileName();
    }
    
    final protected String getVMLevel()
    {
    	return traceFileHeader.getVMLevel();
    }

    final protected String getNextFormatFileName(String previous)
    {
    	return traceFileHeader.getNextFormatFileName(previous);
    }    
    
    final public String toString()
    {
        return filespec;
    }
    
    final public TraceFileHeader getHeader(){
    	return traceFileHeader;
    }

}
