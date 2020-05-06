/*[INCLUDE-IF DAA]*/
/*******************************************************************************
 * Copyright (c) 2013, 2020 IBM Corp. and others
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
 * Conversion routines to unmarshall Java binary types (short, int, long, float,
 * double) from byte arrays.
 * 
 * <p>
 * With sign extensions enabled, the marshalled data is interpreted as signed
 * and the data will be appropriately converted into the return type container.
 * With sign extensions disabled, unfilled bits in the container will be set to
 * zero.  For example, <code>-1</code> as one signed byte is <code>0xFF</code>.  
 * Using <code>readShort</code> with <code>signExtend</code> true, the resultant 
 * short will contain <code>0xFFFF</code>, which is <code>-1</code> in Java's 
 * signed format.  With <code>signExtend</code> false, the resultant short will 
 * contain <code>0x00FF</code>, which is <code>255</code>.
 * </p>
 * 
 * @author IBM
 * @version $Revision$ on $Date$ 
 */
public final class ByteArrayUnmarshaller {

    // private constructor, class contains only static methods.
    private ByteArrayUnmarshaller() {
    	super();
    }

    /**
     * Returns a short value copied from two consecutive bytes of the byte array
     * starting at the offset.
     * 
     * @param byteArray
     *            source
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * 
     * @return short
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static short readShort(byte[] byteArray, int offset,
            boolean bigEndian) {
        if ((offset + 2 > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            		"readShort is trying to access byteArray[" + offset + "] and byteArray[" + (offset + 1) + "], " +
            		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");
        
        if (bigEndian)
        	return readShort_(byteArray, offset, true);
        else
        	return readShort_(byteArray, offset, false);
    }

    private static short readShort_(byte[] byteArray, int offset,
            boolean bigEndian) {
        if (bigEndian) {
            return (short) (((byteArray[offset] & 0xFF) << 8) | ((byteArray[offset + 1] & 0xFF)));
        } else {
            return (short) (((byteArray[offset + 1] & 0xFF) << 8) | ((byteArray[offset] & 0xFF)));
        }
    }
    
    /**
     * Returns a short value copied from zero to two consecutive bytes of the
     * byte array starting at the offset.
     * 
     * @param byteArray
     *            source
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * @param numBytes
     *            the number of bytes to unmarshall, must be 0-2 inclusive
     * @param signExtend
     *            if true and <code>numBytes &lt; 2</code> then the topmost
     *            bytes of the returned short will be sign extended
     * 
     * @return long
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws IllegalArgumentException
     *             if <code>numBytes &lt; 0</code> or
     *             <code>numBytes &gt; 2</code>
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static short readShort(byte[] byteArray, int offset,
            boolean bigEndian, int numBytes, boolean signExtend) 
    {
        if ((offset + numBytes > byteArray.length) || offset < 0)
        	throw new ArrayIndexOutOfBoundsException("Access offset must be positive or zero and last byte must be in range.");
        if (numBytes < 0 || numBytes > 2)
        	throw new IllegalArgumentException("numBytes == " + numBytes);
        
        if (bigEndian)
        {
           if (signExtend)
              return readShort_(byteArray, offset, true, numBytes, true);
           else
              return readShort_(byteArray, offset, true, numBytes, false);
        }
        else
        {
           if (signExtend)
              return readShort_(byteArray, offset, false, numBytes, true);
           else
              return readShort_(byteArray, offset, false, numBytes, false);
        }
    }
    
    private static short readShort_(byte[] byteArray, int offset,
            boolean bigEndian, int numBytes, boolean signExtend) {
        int i = offset;
        switch (numBytes) {
        case 0:
            return 0;
        case 1:
            if (signExtend)
                return byteArray[i];
            else
                return (short) (byteArray[i] & 0x00FF);
        case 2:
            if (bigEndian) {
                return (short) (((byteArray[i] & 0xFF) << 8) | ((byteArray[i + 1] & 0xFF)));
            } else {
                return (short) (((byteArray[i + 1] & 0xFF) << 8) | ((byteArray[i] & 0xFF)));
            }
        default:
        	return 0;
        }
    }

    /**
     * Returns an int value copied from four consecutive bytes starting at the
     * offset.
     * 
     * @param byteArray
     *            source
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * 
     * @return int
     * 
     * @throws NullPointerException
     *             if byteArray is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static int readInt(byte[] byteArray, int offset, boolean bigEndian) {
        if ((offset + 4 > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            		"readInt is trying to access byteArray[" + offset + "] to byteArray[" + (offset + 3) + "], " +
            		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");
        
        if (bigEndian)
        {
        	return readInt_(byteArray, offset, true);
        }
        else
        {
        	return readInt_(byteArray, offset, false);
        }
    }

    private static int readInt_(byte[] byteArray, int offset, boolean bigEndian) {
        if (bigEndian) {
            return ((byteArray[offset] & 0xFF) << 24)
                    | ((byteArray[offset + 1] & 0xFF) << 16)
                    | ((byteArray[offset + 2] & 0xFF) << 8)
                    | ((byteArray[offset + 3] & 0xFF));
        } else {
            return ((byteArray[offset + 3] & 0xFF) << 24)
                    | ((byteArray[offset + 2] & 0xFF) << 16)
                    | ((byteArray[offset + 1] & 0xFF) << 8)
                    | ((byteArray[offset] & 0xFF));
        }
    }
    
    /**
     * Returns an int value copied from zero to four consecutive bytes of the
     * byte array starting at the offset.
     * 
     * @param byteArray
     *            source
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * @param numBytes
     *            the number of bytes to unmarshall, must be 0-4 inclusive
     * @param signExtend
     *            if true and <code>numBytes &lt; 4</code> then the topmost
     *            bytes of the returned int will be sign extended
     * 
     * @return int
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws IllegalArgumentException
     *             if <code>numBytes &lt; 0</code> or
     *             <code>numBytes &gt; 4</code>
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static int readInt(byte[] byteArray, int offset, boolean bigEndian,
            int numBytes, boolean signExtend)
    {
        if ((offset + numBytes > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Access offset must be positive or zero and last byte must be in range.");
        if (numBytes < 0 || numBytes > 4)
        	throw new IllegalArgumentException("numBytes == " + numBytes);
        
        if (bigEndian)
        {
           if (signExtend)
              return readInt_(byteArray, offset, true, numBytes, true);
           else
              return readInt_(byteArray, offset, true, numBytes, false);
        }
        else
        {
           if (signExtend)
              return readInt_(byteArray, offset, false, numBytes, true);
           else
              return readInt_(byteArray, offset, false, numBytes, false);
        }
    }
    
    private static int readInt_(byte[] byteArray, int offset, boolean bigEndian,
            int numBytes, boolean signExtend) {
        int i = offset;
        int answer;
        switch (numBytes) {
        case 0:
            return 0;
        case 1:
            if (signExtend)
                return byteArray[i];
            else
                return byteArray[i] & 0x000000FF;
        case 2:
            if (bigEndian) {
                answer = (((byteArray[i] & 0xFF) << 8) | (byteArray[i + 1] & 0xFF));
            } else {
                answer = (((byteArray[i + 1] & 0xFF) << 8) | (byteArray[i] & 0xFF));
            }
            if (signExtend)
                return (short) answer;
            else
                return answer & 0x0000FFFF;
        case 3:
            if (bigEndian) {
                answer = ((byteArray[i] & 0xFF) << 16)
                        | ((byteArray[i + 1] & 0xFF) << 8)
                        | ((byteArray[i + 2] & 0xFF));
            } else {
                answer = ((byteArray[i + 2] & 0xFF) << 16)
                        | ((byteArray[i + 1] & 0xFF) << 8)
                        | ((byteArray[i] & 0xFF));
            }
            /* if the most significant bit is 1, we need to do sign extension */
            if (signExtend && (answer & (1 << (3 * 8 - 1))) != 0) {
                answer |= 0xFF000000;
            }
            return answer;
        case 4:
            if (bigEndian) {
                return ((byteArray[i] & 0xFF) << 24)
                        | ((byteArray[i + 1] & 0xFF) << 16)
                        | ((byteArray[i + 2] & 0xFF) << 8)
                        | ((byteArray[i + 3] & 0xFF));
            } else {
                return ((byteArray[i + 3] & 0xFF) << 24)
                        | ((byteArray[i + 2] & 0xFF) << 16)
                        | ((byteArray[i + 1] & 0xFF) << 8)
                        | ((byteArray[i] & 0xFF));
            }
        default:
        	return 0;
        }
    }

    /**
     * Returns a long value copied from eight consecutive bytes of the byte
     * array starting at the offset.
     * 
     * @param byteArray
     *            source
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * 
     * @return long
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static long readLong(byte[] byteArray, int offset, boolean bigEndian) {
        if ((offset + 8 > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            		"readLong is trying to access byteArray[" + offset + "] to byteArray[" + (offset + 7) + "], " +
            		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");

        if (bigEndian)
        {
        	return readLong_(byteArray, offset, true);
        }
        else
        {
        	return readLong_(byteArray, offset, false);
        }
    }
    
    private static long readLong_(byte[] byteArray, int offset, boolean bigEndian) {
        if (bigEndian) {
            return (((long) (byteArray[offset] & 0xFF)) << 56)
                    | (((long) (byteArray[offset + 1] & 0xFF)) << 48)
                    | (((long) (byteArray[offset + 2] & 0xFF)) << 40)
                    | (((long) (byteArray[offset + 3] & 0xFF)) << 32)
                    | (((long) (byteArray[offset + 4] & 0xFF)) << 24)
                    | (((byteArray[offset + 5] & 0xFF)) << 16)
                    | (((byteArray[offset + 6] & 0xFF)) << 8)
                    | (((byteArray[offset + 7] & 0xFF)));
        } else {
            return (((long) (byteArray[offset + 7] & 0xFF)) << 56)
                    | (((long) (byteArray[offset + 6] & 0xFF)) << 48)
                    | (((long) (byteArray[offset + 5] & 0xFF)) << 40)
                    | (((long) (byteArray[offset + 4] & 0xFF)) << 32)
                    | (((long) (byteArray[offset + 3] & 0xFF)) << 24)
                    | (((byteArray[offset + 2] & 0xFF)) << 16)
                    | (((byteArray[offset + 1] & 0xFF)) << 8)
                    | (((byteArray[offset] & 0xFF)));
        }
    }

    /**
     * Returns a long value copied from zero to eight consecutive bytes of the
     * byte array starting at the offset.
     * 
     * @param byteArray
     *            source
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * @param numBytes
     *            the number of bytes to unmarshall, must be 0-8 inclusive
     * @param signExtend
     *            if true and <code>numBytes &lt; 8</code> then the topmost
     *            bytes of the returned long will be sign extended
     * 
     * @return long
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws IllegalArgumentException
     *             if <code>numBytes &lt; 0</code> or
     *             <code>numBytes &gt; 8</code>
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static long readLong(byte[] byteArray, int offset,
            boolean bigEndian, int numBytes, boolean signExtend) {
        if ((offset + numBytes > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Access offset must be positive or zero and last byte must be in range.");
        if (numBytes < 0 || numBytes >  8)
            throw new IllegalArgumentException("numBytes == " + numBytes);
        
        if (bigEndian)
        {
           if (signExtend)
              return readLong_(byteArray, offset, true, numBytes, true);
           else
              return readLong_(byteArray, offset, true, numBytes, false);
        }
        else
        {
           if (signExtend)
              return readLong_(byteArray, offset, false, numBytes, true);
           else
              return readLong_(byteArray, offset, false, numBytes, false);
        }
    }
    private static long readLong_(byte[] byteArray, int offset,
            boolean bigEndian, int numBytes, boolean signExtend) {
        int i = offset;
        long answer;
        switch (numBytes) {
        case 0:
            return 0;
        case 1:
            if (signExtend)
                return byteArray[i];
            else
                return byteArray[i] & 0x00000000000000FFl;
        case 2:
            if (bigEndian) {
                answer = (((byteArray[i] & 0xFF) << 8) | (byteArray[i + 1] & 0xFF));
            } else {
                answer = (((byteArray[i + 1] & 0xFF) << 8) | (byteArray[i] & 0xFF));
            }
            if (signExtend)
                return (short) answer;
            else
                return answer & 0x000000000000FFFFl;
        case 3:
            if (bigEndian) {
                answer = (((byteArray[i]) & 0xFF) << 16)
                        | (((byteArray[i + 1]) & 0xFF) << 8)
                        | (((byteArray[i + 2]) & 0xFF));

            } else {
                answer = (((byteArray[i + 2]) & 0xFF) << 16)
                        | (((byteArray[i + 1]) & 0xFF) << 8)
                        | (((byteArray[i]) & 0xFF));

            }

            if (signExtend) {
                answer = (answer << 40) >> 40;
            }

            return answer;
        case 4:
            if (bigEndian) {
                answer = ((((long) byteArray[i]) & 0xFF) << 24)
                        | (((byteArray[i + 1]) & 0xFF) << 16)
                        | (((byteArray[i + 2]) & 0xFF) << 8)
                        | (((byteArray[i + 3]) & 0xFF));

            } else {
                answer = ((((long) byteArray[i + 3]) & 0xFF) << 24)
                        | (((byteArray[i + 2]) & 0xFF) << 16)
                        | (((byteArray[i + 1]) & 0xFF) << 8)
                        | (((byteArray[i]) & 0xFF));

            }

            if (signExtend) {
                answer = (answer << 32) >> 32;
            }

            return answer;
        case 5:
            if (bigEndian) {
                answer = ((((long) byteArray[i]) & 0xFF) << 32)
                        | ((((long) byteArray[i + 1]) & 0xFF) << 24)
                        | (((byteArray[i + 2]) & 0xFF) << 16)
                        | (((byteArray[i + 3]) & 0xFF) << 8)
                        | (((byteArray[i + 4]) & 0xFF));

            } else {
                answer = ((((long) byteArray[i + 4]) & 0xFF) << 32)
                        | ((((long) byteArray[i + 3]) & 0xFF) << 24)
                        | (((byteArray[i + 2]) & 0xFF) << 16)
                        | (((byteArray[i + 1]) & 0xFF) << 8)
                        | (((byteArray[i]) & 0xFF));

            }

            if (signExtend) {
                answer = (answer << 24) >> 24;
            }
            return answer;
        case 6:
            if (bigEndian) {
                answer = ((((long) byteArray[i]) & 0xFF) << 40)
                        | ((((long) byteArray[i + 1]) & 0xFF) << 32)
                        | ((((long) byteArray[i + 2]) & 0xFF) << 24)
                        | (((byteArray[i + 3]) & 0xFF) << 16)
                        | (((byteArray[i + 4]) & 0xFF) << 8)
                        | (((byteArray[i + 5]) & 0xFF));

            } else {
                answer = ((((long) byteArray[i + 5]) & 0xFF) << 40)
                        | ((((long) byteArray[i + 4]) & 0xFF) << 32)
                        | ((((long) byteArray[i + 3]) & 0xFF) << 24)
                        | (((byteArray[i + 2]) & 0xFF) << 16)
                        | (((byteArray[i + 1]) & 0xFF) << 8)
                        | (((byteArray[i]) & 0xFF));
            }

            if (signExtend) {
                answer = (answer << 16) >> 16;
            }
            return answer;
        case 7:
            if (bigEndian) {
                answer = ((((long) byteArray[i]) & 0xFF) << 48)
                        | ((((long) byteArray[i + 1]) & 0xFF) << 40)
                        | ((((long) byteArray[i + 2]) & 0xFF) << 32)
                        | ((((long) byteArray[i + 3]) & 0xFF) << 24)
                        | (((byteArray[i + 4]) & 0xFF) << 16)
                        | (((byteArray[i + 5]) & 0xFF) << 8)
                        | (((byteArray[i + 6]) & 0xFF));

            } else {
                answer = ((((long) byteArray[i + 6]) & 0xFF) << 48)
                        | ((((long) byteArray[i + 5]) & 0xFF) << 40)
                        | ((((long) byteArray[i + 4]) & 0xFF) << 32)
                        | ((((long) byteArray[i + 3]) & 0xFF) << 24)
                        | (((byteArray[i + 2]) & 0xFF) << 16)
                        | (((byteArray[i + 1]) & 0xFF) << 8)
                        | (((byteArray[i]) & 0xFF));

            }

            if (signExtend) {
                answer = (answer << 8) >> 8;
            }

            return answer;
        case 8:
            if (bigEndian) {
                return ((((long) byteArray[i]) & 0xFF) << 56)
                        | ((((long) byteArray[i + 1]) & 0xFF) << 48)
                        | ((((long) byteArray[i + 2]) & 0xFF) << 40)
                        | ((((long) byteArray[i + 3]) & 0xFF) << 32)
                        | ((((long) byteArray[i + 4]) & 0xFF) << 24)
                        | (((byteArray[i + 5]) & 0xFF) << 16)
                        | (((byteArray[i + 6]) & 0xFF) << 8)
                        | (((byteArray[i + 7]) & 0xFF));
            } else {
                return ((((long) byteArray[i + 7]) & 0xFF) << 56)
                        | ((((long) byteArray[i + 6]) & 0xFF) << 48)
                        | ((((long) byteArray[i + 5]) & 0xFF) << 40)
                        | ((((long) byteArray[i + 4]) & 0xFF) << 32)
                        | ((((long) byteArray[i + 3]) & 0xFF) << 24)
                        | (((byteArray[i + 2]) & 0xFF) << 16)
                        | (((byteArray[i + 1]) & 0xFF) << 8)
                        | (((byteArray[i]) & 0xFF));
            }
        default:
          	return 0;
        }
    }

    /**
     * Returns a float value copied from four consecutive bytes of the byte
     * array starting at the offset.
     * 
     * @param byteArray
     *            source
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * 
     * @return float
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static float readFloat(byte[] byteArray, int offset,
            boolean bigEndian) {
        if ((offset + 4 > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            		"readFloat is trying to access byteArray[" + offset + "] to byteArray[" + (offset + 3) + "], " +
            		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");
     
        if (bigEndian)
           return readFloat_(byteArray, offset, true);
        else
           return readFloat_(byteArray, offset, false);
    }
            
    private static float readFloat_(byte[] byteArray, int offset,
            boolean bigEndian) {
        return Float.intBitsToFloat(readInt(byteArray, offset, bigEndian));
    }

    /**
     * Returns a double value copied from eight consecutive bytes of the byte
     * array starting at the offset.
     * 
     * @param byteArray
     *            source
     * @param offset
     *            offset in the byte array
     * @param bigEndian
     *            if false the bytes will be copied in reverse (little endian)
     *            order
     * 
     * @return double
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static double readDouble(byte[] byteArray, int offset,
            boolean bigEndian) {
        if ((offset + 8 > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            		"readDouble is trying to access byteArray[" + offset + "] to byteArray[" + (offset + 7) + "], " +
            		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");
        
        if (bigEndian)
           return readDouble_(byteArray, offset, true);
        else
           return readDouble_(byteArray, offset, false);
    }
    private static double readDouble_(byte[] byteArray, int offset,
            boolean bigEndian) {
        return Double.longBitsToDouble(readLong(byteArray, offset, bigEndian));
    }
}
