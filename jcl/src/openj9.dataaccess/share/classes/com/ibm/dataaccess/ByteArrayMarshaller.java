/*[INCLUDE-IF DAA]*/
/*******************************************************************************
 * Copyright (c) 2013, 2015 IBM Corp. and others
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

package com.ibm.dataaccess;

/**
 * Conversion routines to marshall Java binary types (short, int, long, float,
 * double) to byte arrays.
 *
 * @author IBM
 * @version $Revision$ on $Date$ 
 */
public class ByteArrayMarshaller {

    private ByteArrayMarshaller() {
    }

    /**
     * Copies the short value into two consecutive bytes of the byte array
     * starting at the offset.
     * 
     * @param value
     *            the short value to marshall
     * @param byteArray
     *            destination
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static void writeShort(short value, byte[] byteArray, int offset,
            boolean bigEndian) {
        if ((offset + 2 > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"writeShort is trying to access byteArray[" + offset + "] and byteArray[" + (offset + 1) + "], " +
        		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");
        
        if (bigEndian)
        {
        	writeShort_(value, byteArray, offset, true);
        }
        else
        {
        	writeShort_(value, byteArray, offset, false);
        }
    }
    
    private static void writeShort_(short value, byte[] byteArray, int offset,
            boolean bigEndian) 
    {
        if (bigEndian) {
            byteArray[offset] = (byte) (value >> 8);
            byteArray[offset + 1] = (byte) (value);
        } else {
            byteArray[offset + 1] = (byte) (value >> 8);
            byteArray[offset] = (byte) (value);
        }
    }
   
    
    /**
     * Copies zero to two bytes of the short value into the byte array starting
     * at the offset.
     * 
     * @param value
     *            the short value to marshall
     * @param byteArray
     *            destination
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * @param numBytes
     *            the number of bytes to marshal, must be 0-2 inclusive
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws IllegalArgumentException
     *             if <code>numBytes &lt; 0</code> or
     *             <code>numBytes &gt; 2</code>
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static void writeShort(short value, byte[] byteArray, int offset,
            boolean bigEndian, int numBytes) {
        if ((offset + numBytes > byteArray.length) && (numBytes > 0) && (numBytes <= 2))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            		"writeShort is trying to access byteArray[" + offset + "] to byteArray[" + (offset + numBytes - 1) + "]" +
            		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");
        if (offset < 0)
            throw new ArrayIndexOutOfBoundsException("Access index must be positive or zero.");
       
        if (numBytes < 0 || numBytes > 2)
        	throw new IllegalArgumentException("numBytes == " + numBytes);
        
        if (bigEndian)
            writeShort_(value, byteArray, offset, true, numBytes);
        else
            writeShort_(value, byteArray, offset, false, numBytes);
    }
    
    private static void writeShort_(short value, byte[] byteArray, int offset,
            boolean bigEndian, int numBytes) {
        int i = offset;
        switch (numBytes) {
        case 0:
            break;
        case 1:
            byteArray[i] = (byte) value;
            break;
        case 2:
            if (bigEndian) {
                byteArray[i] = (byte) (value >> 8);
                byteArray[i + 1] = (byte) (value);
            } else {
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i] = (byte) (value);
            }
            break;  
        }
    }

    /**
     * Copies an int value into four consecutive bytes of the byte array
     * starting at the offset.
     * 
     * @param value
     *            the int value to marshall
     * @param byteArray
     *            destination
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static void writeInt(int value, byte[] byteArray, int offset,
            boolean bigEndian) {
        if ((offset + 4 > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            		"writeInt is trying to access byteArray[" + offset + "] to byteArray[" + (offset + 3) + "], " +
            		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");
        
        if (bigEndian)
        {
        	writeInt_(value, byteArray, offset, true);
        }
        else
        {
        	writeInt_(value, byteArray, offset, false);
        }
    }

    private static void writeInt_(int value, byte[] byteArray, int offset,
            boolean bigEndian) {
        if (bigEndian) {
            byteArray[offset] = (byte) (value >> 24);
            byteArray[offset + 1] = (byte) (value >> 16);
            byteArray[offset + 2] = (byte) (value >> 8);
            byteArray[offset + 3] = (byte) (value);
        } else {
            byteArray[offset + 3] = (byte) (value >> 24);
            byteArray[offset + 2] = (byte) (value >> 16);
            byteArray[offset + 1] = (byte) (value >> 8);
            byteArray[offset] = (byte) (value);
        }
    }
    
    /**
     * Copies zero to four bytes of the int value into the byte array starting
     * at the offset.
     * 
     * @param value
     *            the int value to marshall
     * @param byteArray
     *            destination
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * @param numBytes
     *            the number of bytes to marshall, must be 0-4 inclusive
     * 
     * @throws NullPointerException
     *             if byteArray is null
     * @throws IllegalArgumentException
     *             if <code>numBytes &lt; 0</code> or
     *             <code>numBytes &gt; 4</code>
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static void writeInt(int value, byte[] byteArray, int offset,
            boolean bigEndian, int numBytes) {
        if ((offset + numBytes > byteArray.length) && (numBytes <= 4) && (numBytes > 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            		"writeInt is trying to access byteArray[" + offset + "] to byteArray[" + (offset + numBytes - 1) + "]" +
            		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");
        if (offset < 0)
            throw new ArrayIndexOutOfBoundsException("Access index must be positive or zero.");
        if (numBytes < 0 || numBytes > 4)
        	throw new IllegalArgumentException("numBytes == " + numBytes);
       
        if (bigEndian)
        {
        	writeInt_(value, byteArray, offset, true, numBytes);
        }
        else
        {
        	writeInt_(value, byteArray, offset, false, numBytes);
        }
    }
    
    private static void writeInt_(int value, byte[] byteArray, int offset,
            boolean bigEndian, int numBytes) {
        int i = offset;
        switch (numBytes) {
        case 0:
            break;
        case 1:
            byteArray[i] = (byte) value;
            break;
        case 2:
            if (bigEndian) {
                byteArray[i] = (byte) (value >> 8);
                byteArray[i + 1] = (byte) (value);
            } else {
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i] = (byte) (value);
            }
            break;
        case 3:
            if (bigEndian) {
                byteArray[i] = (byte) (value >> 16);
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i + 2] = (byte) (value);
            } else {
                byteArray[i + 2] = (byte) (value >> 16);
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i] = (byte) (value);
            }
            break;
        case 4:
            if (bigEndian) {
                byteArray[i] = (byte) (value >> 24);
                byteArray[i + 1] = (byte) (value >> 16);
                byteArray[i + 2] = (byte) (value >> 8);
                byteArray[i + 3] = (byte) (value);
            } else {
                byteArray[i + 3] = (byte) (value >> 24);
                byteArray[i + 2] = (byte) (value >> 16);
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i] = (byte) (value);
            }
            break;
        }
    }

    /**
     * Copies the long value into eight consecutive bytes of the byte array
     * starting at the offset.
     * 
     * @param value
     *            the long value to marshall
     * @param byteArray
     *            destination
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static void writeLong(long value, byte[] byteArray, int offset,
            boolean bigEndian) {
        if ((offset + 8 > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            		"writeLong is trying to access byteArray[" + offset + "] to byteArray[" + (offset + 7) + "], " +
            		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");
        
        if (bigEndian)
        {
        	writeLong_(value, byteArray, offset, true);
        }
        else
        {
        	writeLong_(value, byteArray, offset, false);
        }
    }
    
    private static void writeLong_(long value, byte[] byteArray, int offset,
            boolean bigEndian) {
        if (bigEndian) {
            byteArray[offset] = (byte) (value >> 56);
            byteArray[offset + 1] = (byte) (value >> 48);
            byteArray[offset + 2] = (byte) (value >> 40);
            byteArray[offset + 3] = (byte) (value >> 32);
            byteArray[offset + 4] = (byte) (value >> 24);
            byteArray[offset + 5] = (byte) (value >> 16);
            byteArray[offset + 6] = (byte) (value >> 8);
            byteArray[offset + 7] = (byte) (value);
        } else {
            byteArray[offset + 7] = (byte) (value >> 56);
            byteArray[offset + 6] = (byte) (value >> 48);
            byteArray[offset + 5] = (byte) (value >> 40);
            byteArray[offset + 4] = (byte) (value >> 32);
            byteArray[offset + 3] = (byte) (value >> 24);
            byteArray[offset + 2] = (byte) (value >> 16);
            byteArray[offset + 1] = (byte) (value >> 8);
            byteArray[offset] = (byte) (value);
        }
    }

    /**
     * Copies zero to eight bytes of the long value into the byte array starting
     * at the offset.
     * 
     * @param value
     *            the long value to marshall
     * @param byteArray
     *            destination
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * @param numBytes
     *            the number of bytes to marshal, must be 0-8 inclusive
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws IllegalArgumentException
     *             if <code>numBytes &lt; 0</code> or
     *             <code>numBytes &gt; 8</code>
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static void writeLong(long value, byte[] byteArray, int offset,
            boolean bigEndian, int numBytes) {
        if ((offset + numBytes > byteArray.length) && (numBytes > 0) && (numBytes <=8))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            		"writeLong is trying to access byteArray[" + offset + "] to byteArray[" + (offset + numBytes - 1) + "]" +
            		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");
        if (offset < 0)
        	throw new ArrayIndexOutOfBoundsException("Access index must be positive or zero.");
       
        if (numBytes < 0 || numBytes > 8)
        	throw new IllegalArgumentException("numBytes == " + numBytes);
        
        if (bigEndian)
            writeLong_(value, byteArray, offset, true, numBytes);
        else
            writeLong_(value, byteArray, offset, false, numBytes);
    }

    private static void writeLong_(long value, byte[] byteArray, int offset,
            boolean bigEndian, int numBytes) {
        int i = offset;
        switch (numBytes) {
        case 0:
            break;
        case 1:
            byteArray[i] = (byte) value;
            break;
        case 2:
            if (bigEndian) {
                byteArray[i] = (byte) (value >> 8);
                byteArray[i + 1] = (byte) (value);
            } else {
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i] = (byte) (value);
            }
            break;
        case 3:
            if (bigEndian) {
                byteArray[i] = (byte) (value >> 16);
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i + 2] = (byte) (value);
            } else {
                byteArray[i + 2] = (byte) (value >> 16);
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i] = (byte) (value);
            }
            break;
        case 4:
            if (bigEndian) {
                byteArray[i] = (byte) (value >> 24);
                byteArray[i + 1] = (byte) (value >> 16);
                byteArray[i + 2] = (byte) (value >> 8);
                byteArray[i + 3] = (byte) (value);
            } else {
                byteArray[i + 3] = (byte) (value >> 24);
                byteArray[i + 2] = (byte) (value >> 16);
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i] = (byte) (value);
            }
            break;
        case 5:
            if (bigEndian) {
                byteArray[i] = (byte) (value >> 32);
                byteArray[i + 1] = (byte) (value >> 24);
                byteArray[i + 2] = (byte) (value >> 16);
                byteArray[i + 3] = (byte) (value >> 8);
                byteArray[i + 4] = (byte) (value);
            } else {
                byteArray[i + 4] = (byte) (value >> 32);
                byteArray[i + 3] = (byte) (value >> 24);
                byteArray[i + 2] = (byte) (value >> 16);
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i] = (byte) (value);
            }
            break;
        case 6:
            if (bigEndian) {
                byteArray[i] = (byte) (value >> 40);
                byteArray[i + 1] = (byte) (value >> 32);
                byteArray[i + 2] = (byte) (value >> 24);
                byteArray[i + 3] = (byte) (value >> 16);
                byteArray[i + 4] = (byte) (value >> 8);
                byteArray[i + 5] = (byte) (value);
            } else {
                byteArray[i + 5] = (byte) (value >> 40);
                byteArray[i + 4] = (byte) (value >> 32);
                byteArray[i + 3] = (byte) (value >> 24);
                byteArray[i + 2] = (byte) (value >> 16);
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i] = (byte) (value);
            }
            break;
        case 7:
            if (bigEndian) {
                byteArray[i] = (byte) (value >> 48);
                byteArray[i + 1] = (byte) (value >> 40);
                byteArray[i + 2] = (byte) (value >> 32);
                byteArray[i + 3] = (byte) (value >> 24);
                byteArray[i + 4] = (byte) (value >> 16);
                byteArray[i + 5] = (byte) (value >> 8);
                byteArray[i + 6] = (byte) (value);
            } else {
                byteArray[i + 6] = (byte) (value >> 48);
                byteArray[i + 5] = (byte) (value >> 40);
                byteArray[i + 4] = (byte) (value >> 32);
                byteArray[i + 3] = (byte) (value >> 24);
                byteArray[i + 2] = (byte) (value >> 16);
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i] = (byte) (value);
            }
            break;
        case 8:
            if (bigEndian) {
                byteArray[i] = (byte) (value >> 56);
                byteArray[i + 1] = (byte) (value >> 48);
                byteArray[i + 2] = (byte) (value >> 40);
                byteArray[i + 3] = (byte) (value >> 32);
                byteArray[i + 4] = (byte) (value >> 24);
                byteArray[i + 5] = (byte) (value >> 16);
                byteArray[i + 6] = (byte) (value >> 8);
                byteArray[i + 7] = (byte) (value);
            } else {
                byteArray[i + 7] = (byte) (value >> 56);
                byteArray[i + 6] = (byte) (value >> 48);
                byteArray[i + 5] = (byte) (value >> 40);
                byteArray[i + 4] = (byte) (value >> 32);
                byteArray[i + 3] = (byte) (value >> 24);
                byteArray[i + 2] = (byte) (value >> 16);
                byteArray[i + 1] = (byte) (value >> 8);
                byteArray[i] = (byte) (value);
            }
            break;
        }

    }    
    
    /**
     * Copies the float value into four consecutive bytes of the byte array
     * starting at the offset.
     * 
     * @param value
     *            the float value to marshall
     * @param byteArray
     *            destination
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static void writeFloat(float value, byte[] byteArray, int offset,
            boolean bigEndian) {
        if ((offset + 4 > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            		"writeFloat is trying to access byteArray[" + offset + "] to byteArray[" + (offset + 3) + "], " +
            		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");
        if (bigEndian)
            writeFloat_(value, byteArray, offset, true);
        else
            writeFloat_(value, byteArray, offset, false);
    }
    
    private static void writeFloat_(float value, byte[] byteArray, int offset,
            boolean bigEndian) {
        writeInt(Float.floatToIntBits(value), byteArray, offset, bigEndian);
    }

    /**
     * Copies the double value into eight consecutive bytes of the byte array
     * starting at the offset.
     * 
     * @param value
     *            the double value to marshall
     * @param byteArray
     *            destination
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static void writeDouble(double value, byte[] byteArray, int offset,
            boolean bigEndian) {
        if ((offset + 8 > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            		"writeDouble is trying to access byteArray[" + offset + "] to byteArray[" + (offset + 7) + "], " +
            		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");

        if (bigEndian)
            writeDouble_(value, byteArray, offset, true);
        else
            writeDouble_(value, byteArray, offset, false);
    }
    
    private static void writeDouble_(double value, byte[] byteArray, int offset,
            boolean bigEndian) {
        writeLong(Double.doubleToLongBits(value), byteArray, offset, bigEndian);
    }

}
