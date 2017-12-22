/*[INCLUDE-IF DAA]*/
/*******************************************************************************
 * Copyright (c) 2013, 2017 IBM Corp. and others
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

import java.math.BigDecimal;
import java.math.BigInteger;
import java.util.Arrays;

import com.ibm.dataaccess.ByteArrayMarshaller;
import com.ibm.dataaccess.ByteArrayUnmarshaller;
import com.ibm.dataaccess.CommonData;
import com.ibm.dataaccess.PackedDecimal;

/**
 * Routines to convert between decimal data types stored in byte arrays and Java binary types.
 * 
 * <p>
 * All the converter routines require the precision of the decimal value to convert, which represents the number of
 * decimal digits in the decimal value, not including the sign.
 * </p>
 * 
 * <p>
 * Unicode Decimal values can be represented as either a char array or as a byte array where every Unicode character is
 * represented by a pair of adjacent bytes.
 * </p>
 * 
 * <p>
 * For embedded sign nibbles (4 bit integers representing values between <code>0x0</code> and <code>0xF</code>
 * inclusive) in External Decimal or Packed Decimal data, <code>0xB</code> and <code>0xD</code> represent a negative
 * sign. All other sign nibble values represent a positive sign. For operations that produce an External Decimal or
 * Packed Decimal result, the Data Access Accelerator library inserts the preferred positive sign code of
 * <code>0xC</code> if the result is positive, and a preferred negative sign code of <code>0xD</code> if the result is
 * negative. All values between <code>0x0</code> and <code>0xF</code> inclusive are interpreted as valid sign codes.
 * </p>
 * 
 * <p>
 * This library has full support for signed integers but only limited support for scale points (decimals). Scale points
 * and other unrecognized characters found in input External, Unicode, or Packed Decimal byte arrays are not supported,
 * and may cause IllegalArgumentExceptions or undefined results. BigDecimal <i>inputs</i> will have scale ignored (i.e.
 * -1.23 will be interpreted as -123). When converting to BigDecimal (as <i>output</i>), a scale value may be explicitly
 * specified as a separate parameter.
 * </p>
 * 
 * @author IBM
 * @version $Revision$ on $Date$
 */
public final class DecimalData
{
    /**
     * External Decimal data format where each byte is an EBCDIC character representing a decimal digit, the sign is
     * encoded in the top nibble of the last byte.
     */
    public static final int EBCDIC_SIGN_EMBEDDED_TRAILING = 1;

    /**
     * External Decimal data format where each byte is an EBCDIC character representing a decimal digit, the sign is
     * encoded in the top nibble of the first byte.
     */
    public static final int EBCDIC_SIGN_EMBEDDED_LEADING = 2;

    /**
     * External Decimal data format where each byte is an EBCDIC character representing a decimal digit, the sign is
     * encoded in a separate byte that comes after the last byte of the number.
     */
    public static final int EBCDIC_SIGN_SEPARATE_TRAILING = 3;

    /**
     * External Decimal data format where each byte is an EBCDIC character representing a decimal digit, the sign is
     * encoded in a separate byte that comes before the first byte of the number.
     */
    public static final int EBCDIC_SIGN_SEPARATE_LEADING = 4;

    /**
     * Unicode Decimal data format where each digit is a Unicode character, there is no sign.
     */
    public static final int UNICODE_UNSIGNED = 5;

    /**
     * Unicode Decimal data format where each digit is a Unicode character, the sign is stored in the first character.
     */
    public static final int UNICODE_SIGN_SEPARATE_LEADING = 6;

    /**
     * Unicode Decimal data format where each digit is a Unicode character, the sign is stored in the last character.
     */
    public static final int UNICODE_SIGN_SEPARATE_TRAILING = 7;

    /**
     * External Decimal format for positive separate sign
     */
    private static final byte EBCDIC_SIGN_POSITIVE = 0x4E;

    /**
     * External Decimal format for negative separate sign
     */
    private static final byte EBCDIC_SIGN_NEGATIVE = 0x60;

    /**
     * External Decimal High Nibble Mask
     */
    private static final byte EXTERNAL_HIGH_MASK = (byte) 0xF0;

    private static final byte UNICODE_HIGH_MASK = (byte) 0x30;

    static final int EXTERNAL_DECIMAL_MIN = 1;

    static final int EXTERNAL_DECIMAL_MAX = 4;

    private static final int EBCDIC_MIN = 1;

    private static final int EBCDIC_MAX = 4;

    private static final int UNICODE_MIN = 5;

    private static final int UNICODE_MAX = 7;

    private static byte[] PD2EDTranslationTable;
    private static byte[] ED2UDTranslationTable;

    private static char UNICODE_SIGN_MINUS = '-';
    private static char UNICODE_SIGN_PLUS = '+';
    private static char UNICODE_ZERO = '0';
    
    private static final boolean JIT_INTRINSICS_ENABLED = false;
    
    static {
        PD2EDTranslationTable = new byte[512];
        ED2UDTranslationTable = new byte[256];
        
        Arrays.fill(PD2EDTranslationTable, (byte) 0);
        Arrays.fill(ED2UDTranslationTable, (byte) 0);
        
        for (int tenDigit = 0; tenDigit < 10; ++tenDigit)
        for (int oneDigit = 0; oneDigit < 10; ++oneDigit)
        {
            int pdValue = tenDigit * 16 + oneDigit;
            // ED tenDigit
            PD2EDTranslationTable[pdValue*2  ] = (byte)(0xF0 | tenDigit);
            PD2EDTranslationTable[pdValue*2+1] = (byte)(0xF0 | oneDigit);
        }
        
        for (int digit = 0; digit < 10; ++digit)
        {
            int edValue = (0xF0 | digit);
            ED2UDTranslationTable[edValue + 1] = (byte)(0x30 | digit);
        }
        
        BigDecimal dummy = new BigDecimal("0");

        
    }

    // Private constructor, class contains only static methods.
    private DecimalData() {
    }

    private static final int PACKED_BYTES_LENGTH = 33;
    // Pre-compute all of the possible values for one byte of a Packed Decimal
    private static final byte[] PACKED_BYTES = new byte[200];
    static {
        int i = 100;
        for (int j = 0; j < 100; j++, i--) {
            int low = i % 10;
            int high = (i / 10) % 10;
            PACKED_BYTES[j] = (byte) ((high << 4) + low);
        }
        i = 0;
        for (int j = 100; j < 200; j++, i++) {
            int low = i % 10;
            int high = (i / 10) % 10;
            PACKED_BYTES[j] = (byte) ((high << 4) + low);
        }
    }
    
    /**
     * This method is recognized by the JIT. The ILGenerator and Walker will replace this method with an appropriate
     * iconst bytecode value corresponding to whether or not the architecture supports DAA JIT intrinsics. Currently
     * the JIT will generate an iconst 1 if and only if we are executing under zOS.
     * 
     * @return true if DAA JIT intrinsics are enabled on the current platform, false otherwise
     * 
     */
    private final static boolean JITIntrinsicsEnabled()
    {
        return JIT_INTRINSICS_ENABLED;
    }
    
    // Binary search on the number of digits in value
    private static int numDigits(int value)
    {
        value = (value == Integer.MIN_VALUE) ? Integer.MAX_VALUE : Math.abs(value);
        
        return (value >=      10000) ? 
               (value >=   10000000) ? 
               (value >=  100000000) ? 
               (value >= 1000000000) ? 10 : 9 : 8 :
               (value >=     100000) ? 
               (value >=    1000000) ?  7 : 6 : 5 :
               (value >=        100) ? 
               (value >=       1000) ?  4 : 3 :
               (value >=         10) ?  2 : 1;
    }
    
    // Binary search on the number of digits in value
    private static int numDigits(long value)
    {
        value = value == Long.MIN_VALUE ? Long.MAX_VALUE : Math.abs(value);
        
        return (value >=          1000000000L) ? 
               (value >=     100000000000000L) ? 
               (value >=   10000000000000000L) ? 
               (value >=  100000000000000000L) ? 
               (value >= 1000000000000000000L) ? 19 : 18 : 17 :
               (value >=    1000000000000000L) ? 16 : 15 :
               (value >=        100000000000L) ? 
               (value >=       1000000000000L) ? 
               (value >=      10000000000000L) ? 14 : 13 : 12 :
               (value >=         10000000000L) ? 11 : 10 :
               (value >=               10000L) ? 
               (value >=             1000000L) ? 
               (value >=            10000000L) ? 
               (value >=           100000000L) ?  9 :  8 :  7 :
               (value >=              100000L) ?  6 :  5 :
               (value >=                 100L) ? 
               (value >=                1000L) ?  4 :  3 :
               (value >=                  10L) ?  2 :  1;
    }

    /**
     * Converts a binary integer value into a signed Packed Decimal format. The Packed Decimal will be padded with zeros
     * on the left if necessary.
     * 
     * Overflow can happen if the resulting Packed Decimal does not fit into the result byte array, given the offset and
     * precision. In this case, when <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown,
     * when false a truncated or invalid result is returned.
     * 
     * @param integerValue
     *            the binary integer value to convert
     * @param packedDecimal
     *            byte array that will store the resulting Packed Decimal value
     * @param offset
     *            offset of the first byte of the Packed Decimal in <code>packedDecimal</code>
     * @param precision
     *            number of Packed Decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * 
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws NullPointerException
     *             if <code>packedDecimal</code> is null
     * @throws ArithmeticException
     *             if the <code>checkOverflow</code> parameter is true and overflow occurs
     */
    public static void convertIntegerToPackedDecimal(int integerValue,
            byte[] packedDecimal, int offset, int precision,
            boolean checkOverflow) {
       if ((offset + ((precision/ 2) + 1) > packedDecimal.length) || (offset < 0))
              throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
               "convertIntegerToPackedDecimal is trying to access packedDecimal[" + offset + "] to packedDecimal[" + (offset + (precision/ 2)) + "], " +
               " but valid indices are from 0 to " + (packedDecimal.length - 1) + ".");
       
       convertIntegerToPackedDecimal_(integerValue, packedDecimal, offset, precision, checkOverflow);
    }
    
    private static void convertIntegerToPackedDecimal_(int integerValue,
            byte[] packedDecimal, int offset, int precision,
            boolean checkOverflow) {
        int value;
        int bytes = CommonData.getPackedByteCount(precision);
        int last = offset + bytes - 1;
        int i;
        boolean evenPrecision = (precision % 2 == 0) ? true : false;

        // avoid invalid precision
        if (checkOverflow) {
            if (precision < 1)
                throw new ArithmeticException(
                        "Decimal overflow - Packed Decimal precision lesser than 1");

            if (numDigits(integerValue) > precision)
                throw new ArithmeticException(
                        "Decimal overflow - Packed Decimal precision insufficient");
        }
        if (integerValue < 0) {
            packedDecimal[last] = (byte) ((Math.abs(integerValue) % 10) << 4 | CommonData.PACKED_MINUS);
            value = Math.abs(integerValue / 10);
        } else {
            value = integerValue;
            packedDecimal[last] = (byte) ((value % 10) << 4 | CommonData.PACKED_PLUS);
            value = value / 10;
        }

        // fill in high/low nibble pairs from next-to-last up to first
        for (i = last - 1; i > offset && value != 0; i--) {
            packedDecimal[i] = CommonData
                    .getBinaryToPackedValues((int) (value % 100));
            value = value / 100;
        }

        if (i == offset && value != 0) {
            if (evenPrecision)
                packedDecimal[i] = (byte) (CommonData
                        .getBinaryToPackedValues((int) (value % 100)) & CommonData.LOWER_NIBBLE_MASK);
            else
                packedDecimal[i] = CommonData
                        .getBinaryToPackedValues((int) (value % 100));
            value = value / 100;
            i--;
        }

        if (checkOverflow && value != 0) {
            throw new ArithmeticException(
                    "Decimal overflow - Packed Decimal precision insufficient");
        }
        if (i >= offset) {
            for (int j = 0; j < i - offset + 1; ++j)
                packedDecimal[j+offset] = CommonData.PACKED_ZERO;
        }
    }

    /**
     * Converts an integer to an External Decimal in a byte array. The External Decimal will be padded with zeros on the
     * left if necessary.
     * 
     * Overflow can happen if the resulting External Decimal value does not fit into the byte array, given the precision
     * and offset. In this case, when <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown,
     * when false a truncated or invalid result is returned.
     * 
     * @param integerValue
     *            the value to convert
     * @param externalDecimal
     *            the byte array which will hold the External Decimal on a successful return
     * @param offset
     *            the offset in the byte array at which the External Decimal should be located
     * @param precision
     *            the number of decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmeticException</code>will be thrown if the designated array cannot hold the
     *            External Decimal.
     * @param decimalType
     *            constant value indicating the type of External Decimal
     * 
     * @throws NullPointerException
     *             if <code>externalDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if the <code>checkOverflow</code> parameter is true and overflow occurs
     * @throws IllegalArgumentException
     *             if <code>decimalType</code> or <code>precision</code> is invalid
     */
    public static void convertIntegerToExternalDecimal(int integerValue,
            byte[] externalDecimal, int offset, int precision,
            boolean checkOverflow, int decimalType) {
        if ((offset < 0)
                || (offset + CommonData.getExternalByteCounts(precision, decimalType) > externalDecimal.length))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. "
                    + "convertIntegerToExternalDecimal is trying to access externalDecimal[" + offset
                    + "] to externalDecimal[" + (offset + CommonData.getExternalByteCounts(precision, decimalType) - 1)
                    + "], " + " but valid indices are from 0 to " + (externalDecimal.length - 1) + ".");
        if (JITIntrinsicsEnabled()) {
            byte[] packedDecimal = new byte[precision / 2 + 1];
            convertIntegerToPackedDecimal_(integerValue, packedDecimal, 0, precision, checkOverflow);
            convertPackedDecimalToExternalDecimal_(packedDecimal, 0, externalDecimal, offset, precision, decimalType);
        } else {
            convertIntegerToExternalDecimal_(integerValue, externalDecimal, offset, precision, checkOverflow, decimalType);
        }
    }

    private static void convertIntegerToExternalDecimal_(int integerValue, byte[] externalDecimal, int offset,
            int precision, boolean checkOverflow, int decimalType) {
        int i;
        byte zoneVal = EXTERNAL_HIGH_MASK;
    
        int externalSignOffset = offset;
        if (decimalType == EBCDIC_SIGN_SEPARATE_LEADING)
            offset++;
        int end = offset + precision - 1;
    
        if (decimalType < EXTERNAL_DECIMAL_MIN || decimalType > EXTERNAL_DECIMAL_MAX)
            throw new IllegalArgumentException("invalid decimalType");
    
        if (checkOverflow) {
            if (precision < 1)
                throw new ArithmeticException("Decimal overflow - External Decimal precision lesser than 1");
    
            if (numDigits(integerValue) > precision)
                throw new ArithmeticException("Decimal overflow - External Decimal precision insufficient");
        }
    
        externalDecimal[end] = (byte) (zoneVal | Math.abs(integerValue % 10));
        int value = Math.abs(integerValue / 10);
        // fill in high/low nibble pairs from next-to-last up to first
        for (i = end - 1; i >= offset && value != 0; i--) {
            externalDecimal[i] = (byte) (zoneVal | (value % 10));
            value = value / 10;
        }
    
        if (i >= offset) {
            for (int j = offset; j <= i; j++) {
                externalDecimal[j] = (byte) (zoneVal | CommonData.PACKED_ZERO);
            }
        }
    
        switch (decimalType) {
        case EBCDIC_SIGN_EMBEDDED_TRAILING:
            externalSignOffset += precision - 1;
        case EBCDIC_SIGN_EMBEDDED_LEADING:
            byte sign;
            if (integerValue >= 0) {
                sign = (byte) (CommonData.PACKED_PLUS << 4);
            } else {
                sign = (byte) (CommonData.PACKED_MINUS << 4);
            }
            externalDecimal[externalSignOffset] = (byte) ((externalDecimal[externalSignOffset] & CommonData.LOWER_NIBBLE_MASK) | sign);
            break;
        case EBCDIC_SIGN_SEPARATE_TRAILING:
            externalSignOffset += precision;
        case EBCDIC_SIGN_SEPARATE_LEADING:
            if (integerValue >= 0)
                externalDecimal[externalSignOffset] = EBCDIC_SIGN_POSITIVE;
            else
                externalDecimal[externalSignOffset] = EBCDIC_SIGN_NEGATIVE;
            break;
        }
    }

    /**
     * Converts an integer to a Unicode Decimal in a char array
     * 
     * Overflow can happen if the resulting External Decimal value does not fit into the char array, given the offset and
     * precision. In this case, when <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown,
     * when false a truncated or invalid result is returned.
     * 
     * @param integerValue
     *            the long value to convert
     * @param unicodeDecimal
     *            the char array which will hold the Unicode Decimal on a successful return
     * @param offset
     *            the offset in the char array where the Unicode Decimal would be located
     * @param precision
     *            the number of decimal digits. Maximum valid precision is 253
     * @param checkoverflow
     *            if true, when the designated an <code>ArithmeticException</code>
     * @param unicodeType
     *            constant value indicating the type of Unicode Decimal
     * 
     * @throws NullPointerException
     *             if <code>unicodeDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * 
     * @throws ArithmeticException
     *             if the <code>checkOverflow</code> parameter is true and overflow occurs
     * @throws IllegalArgumentException
     *             if the <code>decimalType</code> or <code>precision</code> is invalid
     */
    public static void convertIntegerToUnicodeDecimal(int integerValue,
            char[] unicodeDecimal, int offset, int precision,
            boolean checkoverflow, int unicodeType) {
        int size = unicodeType == DecimalData.UNICODE_UNSIGNED ? precision : precision + 1;
        if ((offset + size > unicodeDecimal.length) || (offset < 0))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                 "convertIntegerToUnicodeDecimal is trying to access unicodeDecimal[" + offset + "] to unicodeDecimal[" + (offset + size - 1) + "], " +
                 " but valid indices are from 0 to " + (unicodeDecimal.length - 1) + ".");
        
        if (JITIntrinsicsEnabled()) {
            byte[] packedDecimal = new byte[precision / 2 + 1];
            convertIntegerToPackedDecimal_(integerValue, packedDecimal, 0, precision, checkoverflow);
            convertPackedDecimalToUnicodeDecimal_(packedDecimal, 0, unicodeDecimal, offset, precision, unicodeType);
        } else {
            convertIntegerToUnicodeDecimal_(integerValue, unicodeDecimal, offset, precision, checkoverflow, unicodeType);
        }
    }
    
    private static void convertIntegerToUnicodeDecimal_(int integerValue, char[] unicodeDecimal, int offset, int precision, boolean checkOverflow, int unicodeType) 
    {
        // Avoid invalid precisions
        if (checkOverflow) {
            if (precision < 1)
                throw new ArithmeticException("Decimal overflow - Unicode Decimal precision lesser than 1");

            if (precision < numDigits(integerValue))
                throw new ArithmeticException("Decimal overflow - Unicode Decimal precision insufficient");
        }
        
        switch (unicodeType)
        {
            case UNICODE_UNSIGNED: break;

            case UNICODE_SIGN_SEPARATE_LEADING:
                unicodeDecimal[offset++] = integerValue < 0 ? UNICODE_SIGN_MINUS : UNICODE_SIGN_PLUS;
            break;
            
            case UNICODE_SIGN_SEPARATE_TRAILING:
                unicodeDecimal[offset + precision] = integerValue < 0 ? UNICODE_SIGN_MINUS : UNICODE_SIGN_PLUS;
            break;
            
            default: throw new IllegalArgumentException("Invalid decimalType");
        }
        
        unicodeDecimal[offset + precision - 1] = (char) (UNICODE_HIGH_MASK | (Math.abs(integerValue) % 10));
        
        // Normalize the value
        integerValue = Math.abs(integerValue / 10);

        int i;
        
        // fill in high/low nibble pairs from next-to-last up to first
        for (i = offset + precision - 2; i >= offset && integerValue != 0; i--) {
            unicodeDecimal[i] = (char) (UNICODE_HIGH_MASK | (integerValue % 10));
            
            integerValue = integerValue / 10;
        }

        if (checkOverflow && integerValue != 0) {
            throw new ArithmeticException("Decimal overflow - Unicode Decimal precision insufficient");
        }
        
        for (; i >= offset; i--) {
            unicodeDecimal[i] = (char) UNICODE_HIGH_MASK;
        }
    }

    /**
     * Converts a binary long value into signed Packed Decimal format. The Packed Decimal will be padded with zeros on
     * the left if necessary.
     * 
     * Overflow can happen if the resulting Packed Decimal does not fit into the result byte array, given the offset and
     * precision . In this case, when <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown,
     * when false a truncated or invalid result is returned.
     * 
     * @param longValue
     *            the binary long value to convert
     * @param packedDecimal
     *            byte array that will store the resulting Packed Decimal value
     * @param offset
     *            offset of the first byte of the Packed Decimal in <code>packedDecimal</code>
     * @param precision
     *            number of Packed Decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow), otherwise a truncated value is returned
     * 
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws NullPointerException
     *             if <code>packedDecimal</code> is null
     * @throws ArithmeticException
     *             the <code>checkOverflow</code> parameter is true and overflow occurs
     */
    public static void convertLongToPackedDecimal(long longValue,
            byte[] packedDecimal, int offset, int precision,
            boolean checkOverflow) {
       if ((offset + ((precision/ 2) + 1) > packedDecimal.length) || (offset < 0))
              throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                      "convertLongToPackedDecimal is trying to access packedDecimal[" + offset + "] to packedDecimal[" + (offset + (precision/ 2)) + "], " +
                      " but valid indices are from 0 to " + (packedDecimal.length - 1) + ".");
       
       convertLongToPackedDecimal_(longValue, packedDecimal, offset,  precision, checkOverflow);
    }
    private static void convertLongToPackedDecimal_(long longValue,
            byte[] packedDecimal, int offset, int precision,
            boolean checkOverflow) {
        long value;
        int bytes = CommonData.getPackedByteCount(precision);
        int last = offset + bytes - 1;
        int i;
        boolean evenPrecision = (precision % 2 == 0) ? true : false;

        if (checkOverflow) {
            if (precision < 1)
                throw new ArithmeticException(
                        "Decimal overflow - Packed Decimal precision lesser than 1");

            if (numDigits(longValue) > precision)
                throw new ArithmeticException(
                        "Decimal overflow - Packed Decimal precision insufficient");
        }

        if (longValue < 0) {
            packedDecimal[last] = (byte) ((Math.abs(longValue) % 10) << 4 | CommonData.PACKED_MINUS);
            value = Math.abs(longValue / 10);
        } else {
            value = longValue;
            packedDecimal[last] = (byte) ((value % 10) << 4 | CommonData.PACKED_PLUS);
            value = value / 10;
        }

        // fill in high/low nibble pairs from next-to-last up to first
        for (i = last - 1; i > offset && value != 0; i--) {
            packedDecimal[i] = CommonData
                    .getBinaryToPackedValues((int) (value % 100));
            value = value / 100;
        }

        if (i == offset && value != 0) {
            if (evenPrecision)
                packedDecimal[i] = (byte) (CommonData
                        .getBinaryToPackedValues((int) (value % 100)) & CommonData.LOWER_NIBBLE_MASK);
            else
                packedDecimal[i] = CommonData
                        .getBinaryToPackedValues((int) (value % 100));
            value = value / 100;
            i--;
        }

        if (checkOverflow && value != 0) {
            throw new ArithmeticException(
                    "Decimal overflow - Packed Decimal precision insufficient");
        }
        if (i >= offset) {
            for (int j = 0; j < i - offset + 1; ++j)
                packedDecimal[j+offset] = CommonData.PACKED_ZERO;
        }
    }

    /**
     * Converts a long into an External Decimal in a byte array. The External Decimal will be padded with zeros on the
     * left if necessary.
     * 
     * Overflow can happen if the External Decimal value does not fit into the byte array, given its precision and offset.
     * In this case, when <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a
     * truncated or invalid result is returned.
     * 
     * @param longValue
     *            the value to convert
     * @param externalDecimal
     *            the byte array which will hold the External Decimal on a successful return
     * @param offset
     *            the offset into <code>externalDecimal</code> where External Decimal should be located
     * @param precision
     *            the number of decimal digits to convert. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmeticException</code> or <code>IllegalArgumentException</code> may be thrown
     * @param decimalType
     *            constant value indicating the type of External Decimal
     * 
     * @throws NullPointerException
     *             if <code>externalDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if the <code>checkOverflow</code> parameter is true and overflow occurs
     * @throws IllegalArgumentException
     *             if the <code>decimalType</code> or <code>precision</code> is invalid
     */
    public static void convertLongToExternalDecimal(long longValue, byte[] externalDecimal, int offset, int precision,
            boolean checkOverflow, int decimalType) {
        if ((offset < 0)
                || (offset + CommonData.getExternalByteCounts(precision, decimalType) > externalDecimal.length))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. "
                    + "convertLongToExternalDecimal is trying to access externalDecimal[" + offset
                    + "] to externalDecimal[" + (offset + CommonData.getExternalByteCounts(precision, decimalType) - 1)
                    + "], " + " but valid indices are from 0 to " + (externalDecimal.length - 1) + ".");
        if (JITIntrinsicsEnabled()) {
            byte[] packedDecimal = new byte[precision / 2 + 1];
            convertLongToPackedDecimal_(longValue, packedDecimal, 0, precision, checkOverflow);
            convertPackedDecimalToExternalDecimal_(packedDecimal, 0, externalDecimal, offset, precision, decimalType);
        } else {
            convertLongToExternalDecimal_(longValue, externalDecimal, offset, precision, checkOverflow, decimalType);
        }
    }

    private static void convertLongToExternalDecimal_(long longValue, byte[] externalDecimal, int offset,
            int precision, boolean checkOverflow, int decimalType) {
        int i;
        byte zoneVal = EXTERNAL_HIGH_MASK;
    
        int externalSignOffset = offset;
        if (decimalType == EBCDIC_SIGN_SEPARATE_LEADING)
            offset++;
        int end = offset + precision - 1;
    
        if (decimalType < EXTERNAL_DECIMAL_MIN || decimalType > EXTERNAL_DECIMAL_MAX)
            throw new IllegalArgumentException("invalid decimalType");
    
        if (checkOverflow) {
            if (precision < 1)
                throw new ArithmeticException("Decimal overflow - External Decimal precision lesser than 1");
    
            if (numDigits(longValue) > precision)
                throw new ArithmeticException("Decimal overflow - External Decimal precision insufficient");
        }
    
        externalDecimal[end] = (byte) (zoneVal | Math.abs(longValue % 10));
        long value = Math.abs(longValue / 10);
        // fill in high/low nibble pairs from next-to-last up to first
        for (i = end - 1; i >= offset && value != 0; i--) {
            externalDecimal[i] = (byte) (zoneVal | (value % 10));
            value = value / 10;
        }
        
        if (i >= offset) {
            for (int j = offset; j <= i; j++) {
                externalDecimal[j] = (byte) (zoneVal | CommonData.PACKED_ZERO);
            }
        }
    
        switch (decimalType) {
        case EBCDIC_SIGN_EMBEDDED_TRAILING:
            externalSignOffset += precision - 1;
        case EBCDIC_SIGN_EMBEDDED_LEADING:
            byte sign;
            if (longValue >= 0) {
                sign = (byte) (CommonData.PACKED_PLUS << 4);
            } else {
                sign = (byte) (CommonData.PACKED_MINUS << 4);
            }
            externalDecimal[externalSignOffset] = (byte) ((externalDecimal[externalSignOffset] & CommonData.LOWER_NIBBLE_MASK) | sign);
            break;
        case EBCDIC_SIGN_SEPARATE_TRAILING:
            externalSignOffset += precision;
        case EBCDIC_SIGN_SEPARATE_LEADING:
            if (longValue >= 0)
                externalDecimal[externalSignOffset] = EBCDIC_SIGN_POSITIVE;
            else
                externalDecimal[externalSignOffset] = EBCDIC_SIGN_NEGATIVE;
            break;
        }
    }

    /**
     * Converts a long to a Unicode Decimal in a char array
     * 
     * Overflow can happen if the resulting Unicode Decimal value does not fit into the char array, given its precision
     * and offset . In this case, when <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown,
     * when false a truncated or invalid result is returned.
     * 
     * @param longValue
     *            the long value to convert
     * @param unicodeDecimal
     *            the char array which will hold the Unicode Decimal on a successful return
     * @param offset
     *            the offset in the char array where the Unicode Decimal would be located
     * @param precision
     *            the number of Unicode Decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmeticException</code> or <code>IllegalArgumentException</code> may be thrown
     * @param decimalType
     *            constant value indicating the type of External Decimal
     * 
     * @throws NullPointerException
     *             if <code>unicodeDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if the <code>checkOverflow</code> parameter is true and overflow occurs
     * @throws IllegalArgumentException
     *             if <code>decimalType</code> or <code>precision</code> is invalid
     */
    public static void convertLongToUnicodeDecimal(long longValue,
            char[] unicodeDecimal, int offset, int precision,
            boolean checkOverflow, int decimalType) {
        int size = decimalType == DecimalData.UNICODE_UNSIGNED ? precision : precision + 1;
        if ((offset + size > unicodeDecimal.length) || (offset < 0))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                 "convertIntegerToUnicodeDecimal is trying to access unicodeDecimal[" + offset + "] to unicodeDecimal[" + (offset + size - 1) + "], " +
                 " but valid indices are from 0 to " + (unicodeDecimal.length - 1) + ".");
        
        if (JITIntrinsicsEnabled()) {
            byte[] packedDecimal = new byte[precision / 2 + 1];
            convertLongToPackedDecimal_(longValue, packedDecimal, 0, precision, checkOverflow);
            convertPackedDecimalToUnicodeDecimal_(packedDecimal, 0, unicodeDecimal, offset, precision, decimalType);
        } else {
            convertLongToUnicodeDecimal_(longValue, unicodeDecimal, offset, precision, checkOverflow, decimalType);
        }
    }
    
    private static void convertLongToUnicodeDecimal_(long longValue, char[] unicodeDecimal, int offset, int precision, boolean checkOverflow, int unicodeType) 
    {
        // Avoid invalid precisions
        if (checkOverflow) {
            if (precision < 1)
                throw new ArithmeticException("Decimal overflow - Unicode Decimal precision lesser than 1");

            if (precision < numDigits(longValue))
                throw new ArithmeticException("Decimal overflow - Unicode Decimal precision insufficient");
        }
        
        switch (unicodeType)
        {
            case UNICODE_UNSIGNED: break;

            case UNICODE_SIGN_SEPARATE_LEADING:
                unicodeDecimal[offset++] = longValue < 0 ? UNICODE_SIGN_MINUS : UNICODE_SIGN_PLUS;
            break;
            
            case UNICODE_SIGN_SEPARATE_TRAILING:
                unicodeDecimal[offset + precision] = longValue < 0 ? UNICODE_SIGN_MINUS : UNICODE_SIGN_PLUS;
            break;
            
            default: throw new IllegalArgumentException("Invalid decimalType");
        }
        
        unicodeDecimal[offset + precision - 1] = (char) (UNICODE_HIGH_MASK | (Math.abs(longValue) % 10));
        
        // Normalize the value
        longValue = Math.abs(longValue / 10);

        int i;
        
        // fill in high/low nibble pairs from next-to-last up to first
        for (i = offset + precision - 2; i >= offset && longValue != 0; i--) {
            unicodeDecimal[i] = (char) (UNICODE_HIGH_MASK | (longValue % 10));
            
            longValue = longValue / 10;
        }

        if (checkOverflow && longValue != 0) {
            throw new ArithmeticException("Decimal overflow - Unicode Decimal precision insufficient");
        }
        
        for (; i >= offset; i--) {
            unicodeDecimal[i] = (char) UNICODE_HIGH_MASK;
        }
    }

    /**
     * Converts a Packed Decimal value in a byte array into a binary integer. If the digital part of the input Packed
     * Decimal is not valid then the digital part of the output will not be valid. The sign of the input Packed Decimal
     * is assumed to to be positive unless the sign nibble contains one of the negative sign codes, in which case the
     * sign of the input Packed Decimal is interpreted as negative.
     * 
     * Overflow can happen if the Packed Decimal value does not fit into a binary integer. When
     * <code>checkOverflow</code> is true overflow results in an <code>ArithmeticException</code>, when false a
     * truncated or invalid result is returned.
     * 
     * @param packedDecimal
     *            byte array which contains the Packed Decimal value
     * @param offset
     *            offset of the first byte of the Packed Decimal in <code>packedDecimal</code>
     * @param precision
     *            number of Packed Decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmeticException</code> may be thrown
     * 
     * @return int the resulting binary integer value
     * 
     * @throws NullPointerException
     *             if <code>packedDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result does not fit into an int (overflow)
     */
    public static int convertPackedDecimalToInteger(byte[] packedDecimal,
            int offset, int precision, boolean checkOverflow) {
       if ((offset + ((precision/ 2) + 1) > packedDecimal.length) || (offset < 0))
              throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                      "convertPackedDecimalToInteger is trying to access packedDecimal[" + offset + "] to packedDecimal[" + (offset + (precision/ 2)) + "], " +
                      " but valid indices are from 0 to " + (packedDecimal.length - 1) + ".");
       
       return convertPackedDecimalToInteger_(packedDecimal, offset, precision, checkOverflow);
    }
    
    private static int convertPackedDecimalToInteger_(byte[] packedDecimal,
            int offset, int precision, boolean checkOverflow) {
        int bytes = CommonData.getPackedByteCount(precision);
        int end = offset + bytes - 1;
        long value = 0;// = (packedDecimal[end] & CommonData.INTEGER_MASK) >> 4;

        byte sign = CommonData.getSign((byte) (packedDecimal[end] & CommonData.LOWER_NIBBLE_MASK));
        
        // Skip the first byte if the precision is even and the low-order nibble is zero
        if (precision % 2 == 0 && (packedDecimal[offset] & CommonData.LOWER_NIBBLE_MASK) == 0x00)
        {
            precision--;
            offset++;
        }
        
        // Skip consecutive zero bytes
        for (; offset < end && packedDecimal[offset] == CommonData.PACKED_ZERO; offset++)
        {
            precision -= 2;
        }
        
        if (checkOverflow)
        {
            // Skip high-order zero if and only if precision is odd
            if (precision % 2 == 1 && (packedDecimal[offset] & CommonData.HIGHER_NIBBLE_MASK) == 0x00)
            {
                precision--;
            }
            
            // At this point we are guaranteed that the nibble pointed by a non-zero precision value is non-zero
            if (precision > 10)
                throw new ArithmeticException(
                        "Decimal overflow - Packed Decimal too large for an int");
        }
        
        // For checkOverflow == true at this point we are guaranteed that precision <= 10. The following loop
        // will never overflow because the long value can always contain an integer of precision 10.
        
        for (int pos = offset; pos <= end - 1; ++pos)
        {
            value = value * 100 + CommonData.getPackedToBinaryValues(packedDecimal[pos]);
        }
        
        value = value * 10 + ((packedDecimal[end] & CommonData.HIGHER_NIBBLE_MASK) >> 4);
        
        if (sign == CommonData.PACKED_MINUS)
            value = -1 * value;
        
        if (checkOverflow && (value > Integer.MAX_VALUE || value < Integer.MIN_VALUE))
        {
            throw new ArithmeticException(
                    "Decimal overflow - Packed Decimal too large for a int");
        }
        
        return (int)value;
    }

    /**
     * Converts a Packed Decimal value in a byte array into a binary long. If the digital part of the input Packed
     * Decimal is not valid then the digital part of the output will not be valid. The sign of the input Packed Decimal
     * is assumed to to be positive unless the sign nibble contains one of the negative sign codes, in which case the
     * sign of the input Packed Decimal is interpreted as negative.
     * 
     * Overflow can happen if the Packed Decimal value does not fit into a binary long. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param packedDecimal
     *            byte array which contains the Packed Decimal value
     * @param offset
     *            offset of the first byte of the Packed Decimal in <code>packedDecimal</code>
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmeticException</code> may be thrown
     * 
     * @return long the resulting binary long value
     * 
     * @throws NullPointerException
     *             if <code>packedDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result does not fit into a long (overflow)
     */
    public static long convertPackedDecimalToLong(byte[] packedDecimal,
            int offset, int precision, boolean checkOverflow) {
       if ((offset + ((precision/ 2) + 1) > packedDecimal.length) || (offset < 0))
              throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                     "convertPackedDecimalToLong is trying to access packedDecimal[" + offset + "] to packedDecimal[" + (offset + (precision/ 2)) + "], " +
                     " but valid indices are from 0 to " + (packedDecimal.length - 1) + ".");
       
       return convertPackedDecimalToLong_(packedDecimal, offset, precision, checkOverflow);
    }      
    
    private static long convertPackedDecimalToLong_(byte[] packedDecimal,
            int offset, int precision, boolean checkOverflow) {
        long value = 0;
        int bytes = CommonData.getPackedByteCount(precision);
        int end = offset + bytes - 1;
        int last = packedDecimal[end] & CommonData.INTEGER_MASK;
        byte sign = CommonData.getSign((byte) (last & CommonData.LOWER_NIBBLE_MASK));
        
        // Skip the first byte if the precision is even and the low-order nibble is zero
        if (precision % 2 == 0 && (packedDecimal[offset] & CommonData.LOWER_NIBBLE_MASK) == 0x00)
        {
            precision--;
            offset++;
        }
        
        // Skip consecutive zero bytes
        for (; offset < end && packedDecimal[offset] == CommonData.PACKED_ZERO; offset++)
        {
            precision -= 2;
        }
        
        if (checkOverflow)
        {
            // Skip high-order zero if and only if precision is odd
            if (precision % 2 == 1 && (packedDecimal[offset] & CommonData.HIGHER_NIBBLE_MASK) == 0x00)
            {
                precision--;
            }
            
            // At this point we are guaranteed that the nibble pointed by a non-zero precision value is non-zero
            if (precision > 19)
                throw new ArithmeticException(
                        "Decimal overflow - Packed Decimal too large for a long");
        }
        
        // For checkOverflow == true at this point we are guaranteed that precision <= 19. The following loop
        // may cause the signed long value to overflow. Because the first digit of Long.MAX_VALUE is a 9 the 
        // overflowed signed long value cannot overflow an unsigned long. This guarantees that if an overflow
        // occurs, value will be negative. We will use this fact along with the sign code calculated earlier
        // to determine whether overflow occurred.
        
        for (int pos = offset; pos <= end - 1; ++pos)
        {
            value = value * 100 + CommonData.getPackedToBinaryValues(packedDecimal[pos]);
        }

        value = value * 10 + ((last & CommonData.HIGHER_NIBBLE_MASK) >> 4);
        
        
        if (sign == CommonData.PACKED_MINUS)
            value = -value; 
        
        if (checkOverflow)
        {
            if (sign == CommonData.PACKED_PLUS && value < 0)
                throw new ArithmeticException(
                        "Decimal overflow - Packed Decimal too large for a long");
            else if (sign == CommonData.PACKED_MINUS && value > 0)
                throw new ArithmeticException(
                        "Decimal overflow - Packed Decimal too large for a long");
        }
        
        return value;
    }
    
    /**
     * Converts a Packed Decimal in a byte array into an External Decimal in another byte array. If the digital part of
     * the input Packed Decimal is not valid then the digital part of the output will not be valid. The sign of the
     * input Packed Decimal is assumed to to be positive unless the sign nibble contains one of the negative sign codes,
     * in which case the sign of the input Packed Decimal is interpreted as negative.
     * 
     * @param packedDecimal
     *            byte array that holds the Packed Decimal to be converted
     * @param packedOffset
     *            offset in <code>packedDecimal</code> where the Packed Decimal is located
     * @param externalDecimal
     *            byte array that will hold the External Decimal on a successful return
     * @param externalOffset
     *            offset in <code>externalOffset</code> where the External Decimal is expected to be located
     * @param precision
     *            number of decimal digits
     * @param decimalType
     *            constant indicating the type of the decimal
     * 
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws NullPointerException
     *             if <code>packedDecimal</code> or <code>externalDecimal</code> are null
     * @throws IllegalArgumentException
     *             if <code>precision</code> or <code>decimalType</code> is invalid
     */
    public static void convertPackedDecimalToExternalDecimal(
            byte[] packedDecimal, int packedOffset, byte[] externalDecimal,
            int externalOffset, int precision, int decimalType) {
        if ((packedOffset + ((precision/ 2) + 1) > packedDecimal.length) || (packedOffset < 0))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                     "convertPackedDecimalToExternalDecimal is trying to access packedDecimal[" + packedOffset + "] to packedDecimal[" + (packedOffset + (precision/ 2)) + "], " +
                     " but valid indices are from 0 to " + (packedDecimal.length - 1) + ".");
        if ((externalOffset < 0) || (externalOffset + CommonData.getExternalByteCounts(precision, decimalType) > externalDecimal.length))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                     "convertPackedDecimalToExternalDecimal is trying to access externalDecimal[" + externalOffset + "] to externalDecimal[" + (externalOffset + CommonData.getExternalByteCounts(precision, decimalType) - 1) + "], " +
                     " but valid indices are from 0 to " + (externalDecimal.length - 1) + ".");
        
        convertPackedDecimalToExternalDecimal_(packedDecimal, packedOffset, externalDecimal, 
                                               externalOffset, precision, decimalType);
    }

    private static void convertPackedDecimalToExternalDecimal_(
            byte[] packedDecimal, int packedOffset, byte[] externalDecimal,
            int externalOffset, int precision, int decimalType) {
    
        if (decimalType < EXTERNAL_DECIMAL_MIN
                || decimalType > EXTERNAL_DECIMAL_MAX)
            throw new IllegalArgumentException("invalid decimalType");
        if (precision <= 0)
            throw new IllegalArgumentException("negative precision");
    
        int externalSignOffset = externalOffset;
        if (decimalType == EBCDIC_SIGN_SEPARATE_LEADING)
            externalOffset++;
    
        int end = packedOffset + precision / 2;
        int edEnd = externalOffset + precision - 1;
    
        byte zoneVal = EXTERNAL_HIGH_MASK;
    
        // handle even precision
        if (precision % 2 == 0) {
            externalDecimal[externalOffset++] = (byte) (zoneVal | (packedDecimal[packedOffset++] & CommonData.LOWER_NIBBLE_MASK));
        }
    
        // compute all the intermediate digits
        for (int i = packedOffset; i < end; i++) {
            externalDecimal[externalOffset++] = (byte) (zoneVal | (((packedDecimal[i] & CommonData.HIGHER_NIBBLE_MASK) >> 4) & CommonData.LOWER_NIBBLE_MASK));
            externalDecimal[externalOffset++] = (byte) (zoneVal | (packedDecimal[i] & CommonData.LOWER_NIBBLE_MASK));
        }
    
        
        // deal with the last digit
        externalDecimal[edEnd] = (byte) (zoneVal | (((packedDecimal[end] & 0xF0) >> 4) & CommonData.LOWER_NIBBLE_MASK));
    
        //byte sign = (byte)((packedDecimal[end] & CommonData.LOWER_NIBBLE_MASK) << 4);
        byte sign = (byte)(CommonData.getSign(packedDecimal[end] & CommonData.LOWER_NIBBLE_MASK) << 4);
    
        switch (decimalType) {
        case EBCDIC_SIGN_SEPARATE_LEADING:
            if (sign == (byte) 0xC0)
                externalDecimal[externalSignOffset] = EBCDIC_SIGN_POSITIVE;
            else
                externalDecimal[externalSignOffset] = EBCDIC_SIGN_NEGATIVE;
            break;
        case EBCDIC_SIGN_EMBEDDED_LEADING:
            externalDecimal[externalSignOffset] = (byte) ((externalDecimal[externalSignOffset] & CommonData.LOWER_NIBBLE_MASK) | sign);
            break;
        case EBCDIC_SIGN_EMBEDDED_TRAILING:
            externalSignOffset += precision - 1;
            externalDecimal[externalSignOffset] = (byte) ((externalDecimal[externalSignOffset] & CommonData.LOWER_NIBBLE_MASK) | sign);
            break;
        case EBCDIC_SIGN_SEPARATE_TRAILING:
            externalSignOffset += precision;
            if (sign == (byte) 0xC0)
                externalDecimal[externalSignOffset] = EBCDIC_SIGN_POSITIVE;
            else
                externalDecimal[externalSignOffset] = EBCDIC_SIGN_NEGATIVE;
            break;
        default:
            //unreachable code
            //throw new IllegalArgumentException("invalid decimalType");
        }
    }

    /**
     * Convert a Packed Decimal in a byte array to a Unicode Decimal in a char array. If the digital part of the input
     * Packed Decimal is not valid then the digital part of the output will not be valid. The sign of the input Packed
     * Decimal is assumed to to be positive unless the sign nibble contains one of the negative sign codes, in which
     * case the sign of the input Packed Decimal is interpreted as negative.
     * 
     * @param packedDecimal
     *            byte array that holds a Packed Decimal to be converted
     * @param packedOffset
     *            offset in <code>packedDecimal</code> where the Packed Decimal is located
     * @param unicodeDecimal
     *            char array that will hold the Unicode Decimal on a successful return
     * @param unicodeOffset
     *            offset in the byte array where the Unicode Decimal is expected to be located
     * @param precision
     *            number of decimal digits
     * @param decimalType
     *            constant value indicating the type of the External Decimal
     * 
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws NullPointerException
     *             if <code>packedDecimal</code> or <code>unicodeDecimal</code> are null
     * @throws IllegalArgumentException
     *             if <code>precision</code> or <code>decimalType</code> is invalid
     */
    public static void convertPackedDecimalToUnicodeDecimal(
            byte[] packedDecimal, int packedOffset, char[] unicodeDecimal,
            int unicodeOffset, int precision, int decimalType) {
        int size = decimalType == DecimalData.UNICODE_UNSIGNED ? precision  : precision + 1;
        if ((unicodeOffset + size > unicodeDecimal.length) || (unicodeOffset < 0))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                 "convertPackedDecimalToUnicodeDecimal is trying to access unicodeDecimal[" + unicodeOffset + "] to unicodeDecimal[" + (unicodeOffset + size - 1) + "], " +
                 " but valid indices are from 0 to " + (unicodeDecimal.length - 1) + ".");
        if ((packedOffset < 0) || (packedOffset + ((precision/ 2) + 1) > packedDecimal.length))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                     "convertPackedDecimalToUnicodeDecimal is trying to access packedDecimal[" + packedOffset + "] to packedDecimal[" + (packedOffset + (precision/ 2)) + "], " +
                     " but valid indices are from 0 to " + (packedDecimal.length - 1) + ".");
        
        convertPackedDecimalToUnicodeDecimal_(packedDecimal, packedOffset, unicodeDecimal, 
                                              unicodeOffset, precision, decimalType);
    }

    private static void convertPackedDecimalToUnicodeDecimal_(
            byte[] packedDecimal, int packedOffset, char[] unicodeDecimal,
            int unicodeOffset, int precision, int decimalType) {
    
        if (precision <= 0)
            throw new IllegalArgumentException("negative precision");
    
        int externalSignOffset = -1;
        switch (decimalType) {
        case UNICODE_UNSIGNED:
            break;
        case UNICODE_SIGN_SEPARATE_LEADING:
            externalSignOffset = unicodeOffset++;
            break;
        case UNICODE_SIGN_SEPARATE_TRAILING:
            externalSignOffset = unicodeOffset + precision;
            break;
        default:
            throw new IllegalArgumentException("invalid decimalType");
        }
    
        byte zoneVal = UNICODE_HIGH_MASK;
    
        // Get sign from Packed Decimal
        int end = packedOffset + precision / 2;
        byte sign = (byte) (packedDecimal[end] & CommonData.LOWER_NIBBLE_MASK);
        sign = CommonData.getSign(sign);
    
        // handle even precision
        if (precision % 2 == 0) {
            unicodeDecimal[unicodeOffset] = (char) (zoneVal | (packedDecimal[packedOffset++] & CommonData.LOWER_NIBBLE_MASK));
            unicodeOffset++;
        }
    
        // compute all the intermediate digits
        for (int i = packedOffset; i < end; i++) {
            unicodeDecimal[unicodeOffset] = (char) (zoneVal | (((packedDecimal[i] & CommonData.HIGHER_NIBBLE_MASK) >> 4) & CommonData.LOWER_NIBBLE_MASK));
            unicodeOffset++;
            unicodeDecimal[unicodeOffset] = (char) (zoneVal | (packedDecimal[i] & CommonData.LOWER_NIBBLE_MASK));
            unicodeOffset++;
        }
    
        // deal with the last digit
        unicodeDecimal[unicodeOffset] = (char) (zoneVal | (((packedDecimal[end] & 0xF0) >> 4) & CommonData.LOWER_NIBBLE_MASK));
    
        // put the sign
        if (decimalType != UNICODE_UNSIGNED) {
            if (sign == CommonData.PACKED_PLUS) {
                // put 2B for positive
                unicodeDecimal[externalSignOffset] = 0x2B;
            } else {
                // put 2D for negative
                unicodeDecimal[externalSignOffset] = 0x2D;
            }
        }
    }

    /**
     * Convert a Packed Decimal in a byte array to a BigInteger. If the digital part of the input Packed Decimal is not
     * valid then the digital part of the output will not be valid. The sign of the input Packed Decimal is assumed to
     * to be positive unless the sign nibble contains one of the negative sign codes, in which case the sign of the
     * input Packed Decimal is interpreted as negative.
     * 
     * Overflow can happen if the Packed Decimal value does not fit into the BigInteger. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param packedDecimal
     *            byte array that holds the Packed Decimal to be converted
     * @param offset
     *            offset in <code>packedDecimal</code> where the Packed Decimal is located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * @return BigInteger the resulting BigIntger
     * 
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws NullPointerException
     *             if <code>packedDecimal</code> is null
     */
    public static BigInteger convertPackedDecimalToBigInteger(
            byte[] packedDecimal, int offset, int precision,
            boolean checkOverflow) {
        return convertPackedDecimalToBigDecimal(packedDecimal, offset,
                precision, 0, checkOverflow).toBigInteger();
    }

    /**
     * Convert a Packed Decimal in a byte array to a BigDecimal. If the digital part of the input Packed Decimal is not
     * valid then the digital part of the output will not be valid. The sign of the input Packed Decimal is assumed to
     * to be positive unless the sign nibble contains one of the negative sign codes, in which case the sign of the
     * input Packed Decimal is interpreted as negative.
     * 
     * Overflow can happen if the Packed Decimal value does not fit into the BigDecimal. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param packedDecimal
     *            byte array that holds the Packed Decimal to be converted
     * @param offset
     *            offset in <code>packedDecimal</code> where the Packed Decimal is located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param scale
     *            scale of the BigDecimal to be returned
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * 
     * @return BigDecimal the resulting BigDecimal
     * 
     * @throws NullPointerException
     *             if <code>packedDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             /requires rounding if an invalid array access occurs
     */
    public static BigDecimal convertPackedDecimalToBigDecimal(
            byte[] packedDecimal, int offset, int precision, int scale,
            boolean checkOverflow) {
    
        if (precision <= 9) {
            return BigDecimal.valueOf(
                    convertPackedDecimalToInteger(packedDecimal, offset, precision,
                            checkOverflow), scale);
        } else if (precision <= 18) {
            return BigDecimal.valueOf(
                    convertPackedDecimalToLong(packedDecimal, offset, precision,
                            checkOverflow), scale);
        }
        
        return slowSignedPackedToBigDecimal(packedDecimal, offset, precision,
                scale, checkOverflow);
    }

    /**
     * Converts an External Decimal value in a byte array into a binary integer. If the digital part of the input
     * External Decimal is not valid then the digital part of the output will not be valid. The sign of the input
     * External Decimal is assumed to to be positive unless the sign nibble or byte (depending on
     * <code>decimalType</code>) contains one of the negative sign codes, in which case the sign of the input External
     * Decimal is interpreted as negative.
     * 
     * Overflow can happen if the External Decimal value does not fit into a binary integer. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false the resulting number
     * will be wrapped around starting at the minimum/maximum possible integer value.
     * 
     * @param externalDecimal
     *            byte array which contains the External Decimal value
     * @param offset
     *            the offset where the External Decimal value is located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmeticException</code> or <code>IllegalArgumentException</code> may be thrown. If
     *            false and there is an overflow, the result is undefined.
     * @param decimalType
     *            constant value indicating the type of External Decimal
     * 
     * @return int the resulting binary integer
     * 
     * @throws NullPointerException
     *             if <code>externalDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result does not fit into a int (overflow)
     * @throws IllegalArgumentException
     *             if <code>precision</code> or <code>decimalType</code> is invalid
     */
    public static int convertExternalDecimalToInteger(byte[] externalDecimal,
            int offset, int precision, boolean checkOverflow, int decimalType) {
        if ((offset + CommonData.getExternalByteCounts(precision, decimalType) > externalDecimal.length) || (offset < 0))
         throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
               "convertExternalDecimalToInteger is trying to access externalDecimal[" + offset + "] to externalDecimal[" + (offset + CommonData.getExternalByteCounts(precision, decimalType) - 1) + "], " +
               " but valid indices are from 0 to " + (externalDecimal.length - 1) + ".");

        if (precision <= 0)
            throw new IllegalArgumentException("Precision can't be negative.");
        
        if (JITIntrinsicsEnabled()) {
            byte[] packedDecimal = new byte[precision / 2 + 1];
            convertExternalDecimalToPackedDecimal_(externalDecimal, offset, packedDecimal, 0, precision, decimalType);
            return convertPackedDecimalToInteger_(packedDecimal, 0, precision, checkOverflow);
        } else {
            return convertExternalDecimalToInteger_(externalDecimal, offset, precision, checkOverflow, decimalType);
        }
    }

    private static int convertExternalDecimalToInteger_(byte[] externalDecimal,
                int offset, int precision, boolean checkOverflow, int decimalType) {
        int end = (offset + CommonData.getExternalByteCounts(precision, decimalType) - 1);
        boolean isNegative = isExternalDecimalSignNegative(externalDecimal, offset, precision, decimalType);


        if (decimalType == EBCDIC_SIGN_SEPARATE_TRAILING) {
            end--;
        } else if (decimalType == EBCDIC_SIGN_SEPARATE_LEADING) {
            offset++;
        }

        int value = 0;
        if (isNegative)
        {
            if (precision < 10 || (checkOverflow == false)) //max/min values are -2,147,483,648 and 2,147,483,647, so no overflow possible
            {
                for (int i = offset; i <= end; i++)
                {
                    value = value * 10 - (externalDecimal[i] & 0x0F);
                }
            }
            else //checkOverflow true, precision >= 10
            {
                int tenthOffset = end-9; //location of 10th digit if existent
                int offsetMod = offset > tenthOffset ? offset : tenthOffset; //only read last 10 digits
                for (int i = offsetMod; i <= end; i++)
                {
                    value = value * 10 - (externalDecimal[i] & 0x0F);
                }
                //need value>0 for cases like 2,900,000,000 and digit>2 for 5,000,000,000
                if (value > 0 || (tenthOffset >= offset && (externalDecimal[tenthOffset] & 0x0F) > 2)) //check 10th digit
                {
                    throw new ArithmeticException(
                            "Decimal overflow - External Decimal too small for an int");
                }

                //any more digits are overflow
                for (int i = offset; i < offsetMod; i++)
                {
                    if ((externalDecimal[i] & 0x0F) > 0)
                    {
                        throw new ArithmeticException(
                            "Decimal overflow - External Decimal too small for an int");
                    }
                }
            }
        }
        else
        {
            if (precision < 10 || (checkOverflow == false)) //max/min values are -2,147,483,648 and 2,147,483,647, so no overflow possible
            {
                for (int i = offset; i <= end; i++)
                {
                    value = value * 10 + (externalDecimal[i] & 0x0F);
                }
            }
            else //checkOverflow true, precision >= 10
            {
                int tenthOffset = end-9; //location of 10th digit if existent
                int offsetMod = offset > tenthOffset ? offset : tenthOffset; //only read last 10 digits
                for (int i = offsetMod; i <= end; i++)
                {
                    value = value * 10 + (externalDecimal[i] & 0x0F);
                }
                //need value<0 for cases like 2,900,000,000 and digit>2 for 5,000,000,000
                if (value < 0 || (tenthOffset >= offset && (externalDecimal[tenthOffset] & 0x0F) > 2)) //check 10th digit
                {
                    throw new ArithmeticException(
                            "Decimal overflow - External Decimal too large for an int");
                }

                //any more digits are overflow
                for (int i = offset; i < offsetMod; i++)
                {
                    if ((externalDecimal[i] & 0x0F) > 0)
                    {
                        throw new ArithmeticException(
                            "Decimal overflow - External Decimal too large for an int");
                    }
                }
            }
        }
        return value;
    }

    /**
     * Converts an External Decimal value in a byte array into a long. If the digital part of the input External Decimal
     * is not valid then the digital part of the output will not be valid. The sign of the input External Decimal is
     * assumed to to be positive unless the sign nibble or byte (depending on <code>decimalType</code>) contains one of
     * the negative sign codes, in which case the sign of the input External Decimal is interpreted as negative.
     * 
     * Overflow can happen if the External Decimal value does not fit into a binary long. When
     * <code>checkOverflow</code> is true overflow results in an <code>ArithmeticException</code>, when false the
     * resulting number will be wrapped around starting at the minimum/maximum possible long value.
     * 
     * @param externalDecimal
     *            byte array which contains the External Decimal value
     * @param offset
     *            offset of the first byte of the Packed Decimal in the <code>externalDecimal</code>
     * @param precision
     *            number of External Decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmeticException</code> will be thrown when the converted value cannot fit into
     *            designated External Decimal array. If false and there is an overflow, the result is undefined.
     * @param decimalType
     *            constant value indicating the type of External Decimal
     * 
     * @return long the resulting binary long value
     * 
     * @throws NullPointerException
     *             if <code>externalDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result does not fit into a long (overflow)
     * @throws IllegalArgumentException
     *             if <code>precision</code> or <code>decimalType</code> is invalid
     */
    public static long convertExternalDecimalToLong(byte[] externalDecimal,
            int offset, int precision, boolean checkOverflow, int decimalType) {
        if ((offset + CommonData.getExternalByteCounts(precision, decimalType) > externalDecimal.length) || (offset < 0))
         throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
               "convertExternalDecimalToLong is trying to access externalDecimal[" + offset + "] to externalDecimal[" + (offset + CommonData.getExternalByteCounts(precision, decimalType) - 1) + "], " +
               " but valid indices are from 0 to " + (externalDecimal.length - 1) + ".");

        if (precision <= 0)
            throw new IllegalArgumentException("Precision can't be negative.");
        
        if (JITIntrinsicsEnabled()) {
            byte[] packedDecimal = new byte[precision / 2 + 1];
            convertExternalDecimalToPackedDecimal_(externalDecimal, offset, packedDecimal, 0, precision, decimalType);
            return convertPackedDecimalToLong_(packedDecimal, 0, precision, checkOverflow);
        } else {
            return convertExternalDecimalToLong_(externalDecimal, offset, precision, checkOverflow, decimalType);
        }
    }

    private static long convertExternalDecimalToLong_(byte[] externalDecimal,
                int offset, int precision, boolean checkOverflow, int decimalType) {
        int end = (offset + CommonData.getExternalByteCounts(precision, decimalType) - 1);
        boolean isNegative = isExternalDecimalSignNegative(externalDecimal, offset, precision, decimalType);

        if (decimalType == EBCDIC_SIGN_SEPARATE_TRAILING) {
            end--;
        } else if (decimalType == EBCDIC_SIGN_SEPARATE_LEADING) {
            offset++;
        }

        long value = 0;
        if (isNegative)
        {
            if (precision < 19 || (checkOverflow == false)) //max/min values are -9,223,372,036,854,775,808 and 9,223,372,036,854,775,807, so no overflow possible
            {
                for (int i = offset; i <= end; i++)
                {
                    value = value * 10 - (externalDecimal[i] & 0x0F);
                }
            }
            else //checkOverflow true, precision >= 19
            {
                int offsetMod = offset > end-18 ? offset : end-18; //only read last 19 digits
                for (int i = offsetMod; i <= end; i++)
                {
                    value = value * 10 - (externalDecimal[i] & 0x0F);
                }
                if (value > 0) //check 19th digit
                {
                    throw new ArithmeticException(
                            "Decimal overflow - External Decimal too small for a long");
                }

                //any more digits are overflow
                for (int i = offset; i < offsetMod; i++)
                {
                    if ((externalDecimal[i] & 0x0F) > 0)
                    {
                        throw new ArithmeticException(
                            "Decimal overflow - External Decimal too small for a long");
                    }
                }
            }
        }
        else
        {
            if (precision < 19 || (checkOverflow == false)) //max/min values are -9,223,372,036,854,775,808 and 9,223,372,036,854,775,807, so no overflow possible
            {
                for (int i = offset; i <= end; i++)
                {
                    value = value * 10 + (externalDecimal[i] & 0x0F);
                }
            }
            else //checkOverflow true, precision >= 19
            {
                int offsetMod = offset > end-18 ? offset : end-18; //only read last 19 digits
                for (int i = offsetMod; i <= end; i++)
                {
                    value = value * 10 + (externalDecimal[i] & 0x0F);
                }
                if (value < 0) //check 19th digit
                {
                    throw new ArithmeticException(
                            "Decimal overflow - External Decimal too large for a long");
                }

                //any more digits are overflow
                for (int i = offset; i < offsetMod; i++)
                {
                    if ((externalDecimal[i] & 0x0F) > 0)
                    {
                        throw new ArithmeticException(
                            "Decimal overflow - External Decimal too large for a long");
                    }
                }
            }
        }
        return value;
    }

    /**
     * Converts an External Decimal in a byte array to a Packed Decimal in another byte array. If the digital part of
     * the input External Decimal is not valid then the digital part of the output will not be valid. The sign of the
     * input External Decimal is assumed to to be positive unless the sign nibble or byte (depending on
     * <code>decimalType</code>) contains one of the negative sign codes, in which case the sign of the input External
     * Decimal is interpreted as negative.
     * 
     * @param externalDecimal
     *            byte array holding the External Decimal to be converted
     * @param externalOffset
     *            offset in <code>externalDecimal</code> where the External Decimal is located
     * @param packedDecimal
     *            byte array which will hold the Packed Decimal on a successful return
     * @param packedOffset
     *            offset in <code>packedDecimal</code> where the Packed Decimal is expected to be located
     * @param precision
     *            the number of decimal digits
     * @param decimalType
     *            constant value indicating the type of External Decimal
     * 
     * 
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws NullPointerException
     *             if <code>packedDecimal</code> or <code>externalDecimal</code> is null
     * @throws IllegalArgumentException
     *             if <code>precision</code> or <code>decimalType</code> is invalid
     */
    public static void convertExternalDecimalToPackedDecimal(
            byte[] externalDecimal, int externalOffset, byte[] packedDecimal,
            int packedOffset, int precision, int decimalType) {
        if ((externalOffset + CommonData.getExternalByteCounts(precision, decimalType) > externalDecimal.length) || (externalOffset < 0))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                 "convertExternalDecimalToPackedDecimal is trying to access externalDecimal[" + externalOffset + "] to externalDecimal[" + (externalOffset + CommonData.getExternalByteCounts(precision, decimalType) - 1) + "], " +
                 " but valid indices are from 0 to " + (externalDecimal.length - 1) + ".");
        
        if ((packedOffset < 0) || (packedOffset + ((precision/ 2) + 1) > packedDecimal.length))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                     "convertExternalDecimalToPackedDecimal is trying to access packedDecimal[" + packedOffset + "] to packedDecimal[" + (packedOffset + (precision/ 2)) + "], " +
                     " but valid indices are from 0 to " + (packedDecimal.length - 1) + ".");
        
        convertExternalDecimalToPackedDecimal_(externalDecimal, externalOffset, packedDecimal, 
                                               packedOffset, precision, decimalType);
    }

    private static void convertExternalDecimalToPackedDecimal_(
            byte[] externalDecimal, int externalOffset, byte[] packedDecimal,
            int packedOffset, int precision, int decimalType) {
    
        boolean isNegative = isExternalDecimalSignNegative(externalDecimal, externalOffset, precision, decimalType);
        
        if (decimalType < EXTERNAL_DECIMAL_MIN
                || decimalType > EXTERNAL_DECIMAL_MAX)
            throw new IllegalArgumentException("invalid decimalType");
        if (precision <= 0)
            throw new IllegalArgumentException("negative precision");
    
        int end = packedOffset + precision / 2;
    
        // deal with sign leading
        if (decimalType == EBCDIC_SIGN_SEPARATE_LEADING) {
            externalOffset++;
        }
    
        // handle even precision
        if (precision % 2 == 0) {
            packedDecimal[packedOffset++] = (byte) (externalDecimal[externalOffset++] & CommonData.LOWER_NIBBLE_MASK);
        }
    
        for (int i = packedOffset; i < end; i++) {
            byte top = (byte) ((externalDecimal[externalOffset++] & CommonData.LOWER_NIBBLE_MASK) << 4);
            byte bottom = (byte) (externalDecimal[externalOffset++] & CommonData.LOWER_NIBBLE_MASK);
            packedDecimal[i] = (byte) (top | bottom);
        }
    
        // deal with the last digit
        packedDecimal[end] = (byte) ((externalDecimal[externalOffset] & CommonData.LOWER_NIBBLE_MASK) << 4);
    
        // Compute the sign
        if (isNegative)
            packedDecimal[end] |= CommonData.PACKED_MINUS;
        else
            packedDecimal[end] |= CommonData.PACKED_PLUS;
    }

    /**
     * Convert an External Decimal in a byte array to a BigInteger. The sign of the input External Decimal is assumed to
     * to be positive unless the sign nibble or byte (depending on <code>decimalType</code>) contains one of the
     * negative sign codes, in which case the sign of the input External Decimal is interpreted as negative.
     * 
     * Overflow can happen if the External Decimal value does not fit into the BigInteger. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param externalDecimal
     *            byte array that holds the Packed Decimal to be converted
     * @param offset
     *            offset in <code>externalDecimal</code> where the Packed Decimal is located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * @param decimalType
     *            constant value indicating the type of External Decimal
     * 
     * @return BigInteger the resulting BigInteger
     * 
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws NullPointerException
     *             if <code>externalDecimal</code> is null
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result overflows
     * @throws IllegalArgumentException
     *             if <code>decimalType</code> or <code>precision</code> is invalid, or the digital part of the input is
     *             invalid.
     */
    public static BigInteger convertExternalDecimalToBigInteger(
            byte[] externalDecimal, int offset, int precision,
            boolean checkOverflow, int decimalType) {
        byte[] packedDecimal = new byte[precision / 2 + 1];
        convertExternalDecimalToPackedDecimal(externalDecimal, offset,
                packedDecimal, 0, precision, decimalType);
        
        int cc = PackedDecimal.checkPackedDecimal(packedDecimal, 0, precision,true, true); 
        if (cc != 0)
           throw new IllegalArgumentException("The input External Decimal is not valid.");
    
        return convertPackedDecimalToBigDecimal(packedDecimal, 0, precision, 0,
                checkOverflow).toBigInteger();
    }

    /**
     * Converts an External Decimal in a byte array to a BigDecimal. The sign of the input External Decimal is assumed
     * to to be positive unless the sign nibble or byte (depending on <code>decimalType</code>) contains one of the
     * negative sign codes, in which case the sign of the input External Decimal is interpreted as negative.
     * 
     * Overflow can happen if the External Decimal value does not fit into the BigDecimal. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param externalDecimal
     *            byte array holding the External Decimal to be converted
     * @param offset
     *            offset in <code>externalDecimal</code> where the External Decimal is located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param scale
     *            scale of the BigDecimal
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * @param decimalType
     *            constant value that indicates the type of External Decimal
     * 
     * @return BigDecimal the resulting BigDecimal
     * 
     * @throws NullPointerException
     *             if <code>externalDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result overflows
     * @throws IllegalArgumentException
     *             if <code>checkOverflow</code> is true and the Packed Decimal is in an invalid format, or the digital
     *             part of the input is invalid.
     */
    public static BigDecimal convertExternalDecimalToBigDecimal(
            byte[] externalDecimal, int offset, int precision, int scale,
            boolean checkOverflow, int decimalType) {
        if (precision <= 9) {
            return BigDecimal.valueOf(
                    convertExternalDecimalToInteger(externalDecimal, offset, precision,
                            checkOverflow, decimalType), scale);
        } else if (precision <= 18) {
            return BigDecimal.valueOf(
                    convertExternalDecimalToLong(externalDecimal, offset, precision,
                            checkOverflow, decimalType), scale);
        }
    	
        byte[] packedDecimal = new byte[precision / 2 + 1];
        convertExternalDecimalToPackedDecimal(externalDecimal, offset,
                packedDecimal, 0, precision, decimalType);
                
        int cc = PackedDecimal.checkPackedDecimal(packedDecimal, 0, precision,true, true); 
        if (cc != 0)
           throw new IllegalArgumentException("The input External Decimal is not valid.");
    
        return convertPackedDecimalToBigDecimal(packedDecimal, 0, precision,
                scale, checkOverflow);
    }

    private static boolean isExternalDecimalSignNegative(byte[] externalDecimal, int externalOffset, 
                                                         int precision, int decimalType)
    {
        byte signByte = 0;
        switch (decimalType)
        {
        case EBCDIC_SIGN_EMBEDDED_LEADING:
            signByte = (byte) (externalDecimal[externalOffset] & EXTERNAL_HIGH_MASK); 
            if (signByte == CommonData.EXTERNAL_EMBEDDED_SIGN_MINUS ||
                signByte == CommonData.EXTERNAL_EMBEDDED_SIGN_MINUS_ALTERNATE_B)
                return true;
            break;
            
        case EBCDIC_SIGN_EMBEDDED_TRAILING:
            signByte = (byte)(externalDecimal[externalOffset + precision - 1] & EXTERNAL_HIGH_MASK); 
            if (signByte == CommonData.EXTERNAL_EMBEDDED_SIGN_MINUS ||
                signByte == CommonData.EXTERNAL_EMBEDDED_SIGN_MINUS_ALTERNATE_B)
                return true;
            break;
            
        case EBCDIC_SIGN_SEPARATE_LEADING:
            signByte = externalDecimal[externalOffset]; 
            if (signByte == CommonData.EXTERNAL_SIGN_MINUS)
                return true;
            break;
            
        case EBCDIC_SIGN_SEPARATE_TRAILING:
            signByte = externalDecimal[externalOffset + precision];
            if (signByte == CommonData.EXTERNAL_SIGN_MINUS)
               return true;
            break;
            
        default:
            throw new IllegalArgumentException("Invalid decimal sign type.");
        }
        
        return false;
    }
    
    /**
     * Converts a Unicode Decimal value in a char array into a binary integer. The sign of the input Unicode Decimal is
     * assumed to to be positive unless the sign char contains the negative sign code, in which case the sign of the
     * input Unicode Decimal is interpreted as negative.
     * 
     * Overflow can happen if the Unicode Decimal value does not fit into a binary int. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param unicodeDecimal
     *            char array which contains the Unicode Decimal value
     * @param offset
     *            the offset where the Unicode Decimal value is located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmeticException</code> or <code>IllegalArgumentException</code> may be thrown
     * @param unicodeType
     *            constant value indicating the type of External Decimal
     * 
     * @return int the resulting binary integer
     * 
     * @throws NullPointerException
     *             if <code>unicodeDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result does not fit into a long (overflow)
     * @throws IllegalArgumentException
     *             if <code>precision</code> or <code>decimalType</code> is invalid, or the digital part of the input is
     *             invalid.
     */
    public static int convertUnicodeDecimalToInteger(char[] unicodeDecimal,
            int offset, int precision, boolean checkOverflow, int unicodeType) {
        int size = unicodeType == DecimalData.UNICODE_UNSIGNED ? precision : precision + 1;
        if ((offset + size > unicodeDecimal.length) || (offset < 0))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                 "convertUnicodeDecimalToInteger is trying to access unicodeDecimal[" + offset + "] to unicodeDecimal[" + (offset + size - 1) + "], " +
                 " but valid indices are from 0 to " + (unicodeDecimal.length - 1) + ".");
        
        if (JITIntrinsicsEnabled()) {
            byte[] packedDecimal = new byte[precision / 2 + 1];
            convertUnicodeDecimalToPackedDecimal_(unicodeDecimal, offset,
                    packedDecimal, 0, precision, unicodeType);
            int cc = PackedDecimal.checkPackedDecimal(packedDecimal, 0,
                    precision, true, true);
            if (cc != 0)
                throw new IllegalArgumentException(
                        "The input Unicode is not valid");

            return convertPackedDecimalToInteger_(packedDecimal, 0, precision,
                    checkOverflow);
        } else {
            return convertUnicodeDecimalToInteger_(unicodeDecimal, offset,
                    precision, checkOverflow, unicodeType);
        }
    }

    private static int convertUnicodeDecimalToInteger_(char[] unicodeDecimal,
            int offset, int precision, boolean checkOverflow, int unicodeType) {        
        // Validate precision
        if (precision <= 0)
            throw new IllegalArgumentException("invalid precision");
        
        // Validate decimalType and determine sign
        boolean positive = true;
        switch (unicodeType) {
        case UNICODE_UNSIGNED:
            break;
        case UNICODE_SIGN_SEPARATE_LEADING:
            positive = unicodeDecimal[offset++] == UNICODE_SIGN_PLUS;
            break;
        case UNICODE_SIGN_SEPARATE_TRAILING:
            positive = unicodeDecimal[offset + precision] == UNICODE_SIGN_PLUS;
            break;
        default:
            throw new IllegalArgumentException("invalid decimalType");
        }
        
        int end = offset + precision;
        long val = 0;
        
        // Trim leading 0s
        for (; offset < end && unicodeDecimal[offset] == UNICODE_ZERO; offset++) {
            precision--;
        }
        
        // Pre-emptive check overflow: 10 digits in Integer.MAX_VALUE
        if (checkOverflow && precision > 10) {
            throw new ArithmeticException(
                "Decimal overflow - Unicode Decimal too large for an int");
        }
        
        // Working in negatives as Integer.MIN_VALUE has a greater
        // distance from 0 than Integer.MAX_VALUE
        for ( ; offset < end; ++offset) {
            val *= 10;
            val -= unicodeDecimal[offset] & CommonData.LOWER_NIBBLE_MASK;
        }
        
        // Normalize result
        val = positive ? -val : val;
        
        // Check overflow
        if (checkOverflow && (val > Integer.MAX_VALUE || val < Integer.MIN_VALUE)) {
            throw new ArithmeticException(
                    "Decimal overflow - Unicode Decimal too large for a int");
        }
        
        return (int) val;
    }

    /**
     * Converts a Unicode Decimal value in a char array into a binary long. The sign of the input Unicode Decimal is
     * assumed to to be positive unless the sign char contains the negative sign code, in which case the sign of the
     * input Unicode Decimal is interpreted as negative.
     * 
     * Overflow can happen if the Unicode Decimal value does not fit into a binary long. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param unicodeDecimal
     *            char array which contains the External Decimal value
     * @param offset
     *            offset of the first byte of the Unicode Decimal in <code>unicodeDecimal</code>
     * @param precision
     *            number of Unicode Decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmeticException</code> will be thrown when
     * @param unicodeType
     *            constant value indicating the type of External Decimal
     * 
     * @return long the resulting binary long value
     * 
     * @throws NullPointerException
     *             if <code>unicodeDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result does not fit into a long (overflow)
     * @throws IllegalArgumentException
     *             if <code>unicodeType</code> or <code>precision</code> is invalid, or the digital part of the input is
     *             invalid.
     */
    public static long convertUnicodeDecimalToLong(char[] unicodeDecimal,
            int offset, int precision, boolean checkOverflow, int unicodeType) {
        int size = unicodeType == DecimalData.UNICODE_UNSIGNED ? precision : precision + 1;
        if ((offset + size > unicodeDecimal.length) || (offset < 0))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                 "convertUnicodeDecimalToLong is trying to access unicodeDecimal[" + offset + "] to unicodeDecimal[" + (offset + size - 1) + "], " +
                 " but valid indices are from 0 to " + (unicodeDecimal.length - 1) + ".");
        
        if (JITIntrinsicsEnabled()) {
            byte[] packedDecimal = new byte[precision / 2 + 1];
            convertUnicodeDecimalToPackedDecimal_(unicodeDecimal, offset,
                    packedDecimal, 0, precision, unicodeType);
        
            int cc = PackedDecimal.checkPackedDecimal(packedDecimal, 0, precision,true, true); 
            if (cc != 0)
               throw new IllegalArgumentException("The input Unicode is not valid");
        
            return convertPackedDecimalToLong_(packedDecimal, 0, precision,
                    checkOverflow);
        } else {
            return convertUnicodeDecimalToLong_(unicodeDecimal, offset, precision, checkOverflow, unicodeType);
        }
    }
    
    private static long convertUnicodeDecimalToLong_(char[] unicodeDecimal,
            int offset, int precision, boolean checkOverflow, int unicodeType) {
        // Validate precision
        if (precision <= 0)
            throw new IllegalArgumentException("invalid precision");
        
        // Validate decimalType and determine sign
        boolean positive = true;
        switch (unicodeType) {
        case UNICODE_UNSIGNED:
            break;
        case UNICODE_SIGN_SEPARATE_LEADING:
            positive = unicodeDecimal[offset++] == UNICODE_SIGN_PLUS;
            break;
        case UNICODE_SIGN_SEPARATE_TRAILING:
            positive = unicodeDecimal[offset + precision] == UNICODE_SIGN_PLUS;
            break;
        default:
            throw new IllegalArgumentException("invalid decimalType");
        }
        
        int end = offset + precision;
        long val = 0;
        
        // Trim leading 0s
        for (; offset < end && unicodeDecimal[offset] == UNICODE_ZERO; offset++) {
            precision--;
        }
        
        // Pre-emptive check overflow: 19 digits in Long.MAX_VALUE
        if (checkOverflow && precision > 19) {
            throw new ArithmeticException(
                "Decimal overflow - Unicode Decimal too large for a long");
        }
        
        // Working in negatives as Long.MIN_VALUE has a greater
        // distance from 0 than Long.MAX_VALUE
        for ( ; offset < end; ++offset) {
            val *= 10;
            val -= unicodeDecimal[offset] & CommonData.LOWER_NIBBLE_MASK;
        }
        
        // Normalize result
        val = positive ? -val : val;
        
        // Check overflow
        if (checkOverflow) {
            if (positive && val < 0) {
                throw new ArithmeticException(
                        "Decimal overflow - Unicode Decimal too large for a long");
            } else if (!positive && val > 0) {
                throw new ArithmeticException(
                        "Decimal overflow - Unicode Decimal too small for a long");
            }
        }
        
        return val;
    }

    /**
     * Converts an Unicode Decimal in a char array to a Packed Decimal in a byte array. If the digital part of the input
     * Unicode Decimal is not valid then the digital part of the output will not be valid. The sign of the input Unicode
     * Decimal is assumed to to be positive unless the sign byte contains the negative sign code, in which case the sign
     * of the input Unicode Decimal is interpreted as negative.
     * 
     * @param unicodeDecimal
     *            char array that holds the Unicode Decimal to be converted
     * @param unicodeOffset
     *            offset in <code>unicodeDecimal</code> at which the Unicode Decimal is located
     * @param packedDecimal
     *            byte array that will hold the Packed Decimal on a successful return
     * @param packedOffset
     *            offset in <code>packedDecimal</code> where the Packed Decimal is expected to be located
     * @param precision
     *            number of decimal digits
     * @param decimalType
     *            constant value indicating the type of External Decimal
     * 
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws NullPointerException
     *             if <code>packedDecimal</code> or <code>unicodeDecimal</code> are null
     * @throws IllegalArgumentException
     *             if <code>precision</code> or <code>decimalType</code> is invalid
     */
    public static void convertUnicodeDecimalToPackedDecimal(
            char[] unicodeDecimal, int unicodeOffset, byte[] packedDecimal,
            int packedOffset, int precision, int decimalType) {
        int size = decimalType == DecimalData.UNICODE_UNSIGNED ? precision : precision + 1;
        if ((unicodeOffset + size > unicodeDecimal.length) || (unicodeOffset < 0))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                 "convertUnicodeDecimalToPackedDecimal is trying to access unicodeDecimal[" + unicodeOffset + "] to unicodeDecimal[" + (unicodeOffset + size - 1) + "], " +
                 " but valid indices are from 0 to " + (unicodeDecimal.length - 1) + ".");
        if ((packedOffset < 0) || (packedOffset + ((precision/ 2) + 1) > packedDecimal.length))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                     "convertUnicodeDecimalToPackedDecimal is trying to access packedDecimal[" + packedOffset + "] to packedDecimal[" + (packedOffset + (precision/ 2)) + "], " +
                     " but valid indices are from 0 to " + (packedDecimal.length - 1) + ".");
        
        convertUnicodeDecimalToPackedDecimal_(unicodeDecimal, unicodeOffset, 
                                              packedDecimal, packedOffset, precision, decimalType);
    }
    
    private static void convertUnicodeDecimalToPackedDecimal_(
            char[] unicodeDecimal, int unicodeOffset, byte[] packedDecimal,
            int packedOffset, int precision, int decimalType) {

        if (precision <= 0)
            throw new IllegalArgumentException("invalid precision");

        int end = packedOffset + precision / 2;
        int signOffset = unicodeOffset;
        if (decimalType == UNICODE_SIGN_SEPARATE_LEADING) {
            unicodeOffset++;
        }
        // handle even precision
        if (precision % 2 == 0) {
            packedDecimal[packedOffset++] = (byte) (unicodeDecimal[unicodeOffset++] & CommonData.LOWER_NIBBLE_MASK);
        }

        // compute all the intermediate digits
        for (int i = packedOffset; i < end; i++) {
            byte top = (byte) ((unicodeDecimal[unicodeOffset++] & CommonData.LOWER_NIBBLE_MASK) << 4);
            byte bottom = (byte) (unicodeDecimal[unicodeOffset++] & CommonData.LOWER_NIBBLE_MASK);
            packedDecimal[i] = (byte) (top | bottom);
        }
        packedDecimal[end] = (byte) ((unicodeDecimal[unicodeOffset++] & CommonData.LOWER_NIBBLE_MASK) << 4);

        switch (decimalType) 
        {
        case UNICODE_SIGN_SEPARATE_LEADING:
            if (unicodeDecimal[signOffset] == '-')
                packedDecimal[end] |= CommonData.PACKED_MINUS;
            else
                packedDecimal[end] |= CommonData.PACKED_PLUS;
            break;
            
        case UNICODE_SIGN_SEPARATE_TRAILING:
            if (unicodeDecimal[unicodeOffset] == '-')
                packedDecimal[end] |= CommonData.PACKED_MINUS;
            else
                packedDecimal[end] |= CommonData.PACKED_PLUS;
            break;
            
        case UNICODE_UNSIGNED:
            packedDecimal[end] |= CommonData.PACKED_PLUS;
            break;
            
        default:
            throw new IllegalArgumentException("invalid decimalType");
        }
    }

    /**
     * Convert a Unicode Decimal in a char array to a BigInteger. The sign of the input Unicode Decimal is assumed to to
     * be positive unless the sign byte contains the negative sign code, in which case the sign of the input Unicode
     * Decimal is interpreted as negative.
     * 
     * Overflow can happen if the Unicode Decimal value does not fit into a binary long. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param unicodeDecimal
     *            char array that holds the Packed Decimal to be converted
     * @param offset
     *            offset into <code>unicodeDecimal</code> where the Packed Decimal is located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * @param decimalType
     *            constant value indicating the type of External Decimal
     * 
     * @return BigInteger the resulting BigInteger
     * 
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws NullPointerException
     *             if <code>unicodeDecimal</code> is null
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result overflows
     * @throws IllegalArgumentException
     *             if <code>decimalType</code> or <code>precision</code> is invalid, or the digital part of the input is
     *             invalid.
     */
    public static BigInteger convertUnicodeDecimalToBigInteger(
            char[] unicodeDecimal, int offset, int precision,
            boolean checkOverflow, int decimalType) {
        byte[] packedDecimal = new byte[precision / 2 + 1];
        convertUnicodeDecimalToPackedDecimal(unicodeDecimal, offset,
                packedDecimal, 0, precision, decimalType);

        int cc = PackedDecimal.checkPackedDecimal(packedDecimal, 0, precision,true, true); 
        if (cc != 0)
           throw new IllegalArgumentException("The input Unicode is not valid");

        return convertPackedDecimalToBigDecimal(packedDecimal, 0, precision, 0,
                checkOverflow).toBigInteger();
    }

    /**
     * Converts a Unicode Decimal in a char array to a BigDecimal. The sign of the input Unicode Decimal is assumed to
     * to be positive unless the sign byte contains the negative sign code, in which case the sign of the input Unicode
     * Decimal is interpreted as negative.
     * 
     * Overflow can happen if the Unicode Decimal value does not fit into the BigDecimal. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param unicodeDecimal
     *            char array that holds the Unicode Decimal
     * @param offset
     *            offset in <code>unicodeDecimal</code> where the Unicode Decimal is located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param scale
     *            scale of the returned BigDecimal
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * @param decimalType
     *            constant value indicating the type of External Decimal
     * 
     * @return BigDecimal
     * 
     * @throws NullPointerException
     *             if <code>unicodeDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result overflows
     * @throws IllegalArgumentException
     *             if <code>checkOverflow</code> is true and the Packed Decimal is in an invalid format, or the digital
     *             part of the input is invalid.
     */
    public static BigDecimal convertUnicodeDecimalToBigDecimal(
            char[] unicodeDecimal, int offset, int precision, int scale,
            boolean checkOverflow, int decimalType) {
        byte[] packedDecimal = new byte[precision / 2 + 1];
        convertUnicodeDecimalToPackedDecimal(unicodeDecimal, offset,
                packedDecimal, 0, precision, decimalType);
    
    
        int cc = PackedDecimal.checkPackedDecimal(packedDecimal, 0, precision,true, true); 
        if (cc != 0)
           throw new IllegalArgumentException("The input Unicode is not valid");
    
        return convertPackedDecimalToBigDecimal(packedDecimal, 0, precision,
                scale, checkOverflow);
    }

    /**
     * Converts a BigInteger value into a Packed Decimal in a byte array
     * 
     * Overflow can happen if the BigInteger does not fit into the byte array. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param bigIntegerValue
     *            BigInteger value to be converted
     * @param packedDecimal
     *            byte array which will hold the Packed Decimal on a successful return
     * @param offset
     *            offset into <code>packedDecimal</code> where the Packed Decimal is expected to be located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253s
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * 
     * @throws NullPointerException
     *             if <code>packedDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result overflows
     */
    public static void convertBigIntegerToPackedDecimal(
            BigInteger bigIntegerValue, byte[] packedDecimal, int offset,
            int precision, boolean checkOverflow) {
        convertBigDecimalToPackedDecimal(new BigDecimal(bigIntegerValue),
                packedDecimal, offset, precision, checkOverflow);
    }

    /**
     * Converts a BigInteger value into an External Decimal in a byte array
     * 
     * Overflow can happen if the BigInteger does not fit into the byte array. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param bigIntegerValue
     *            BigInteger value to be converted
     * @param externalDecimal
     *            byte array which will hold the External Decimal on a successful return
     * @param offset
     *            offset into <code>externalDecimal</code> where the External Decimal is expected to be located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * @param decimalType
     *            constant value that indicates the type of External Decimal
     * 
     * @throws NullPointerException
     *             if <code>externalDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result overflows
     * @throws IllegalArgumentException
     *             if <code>decimalType</code> or <code>precision</code> is invalid
     */
    public static void convertBigIntegerToExternalDecimal(
            BigInteger bigIntegerValue, byte[] externalDecimal, int offset,
            int precision, boolean checkOverflow, int decimalType) {
        byte[] packedDecimal = new byte[precision / 2 + 1];
        convertBigDecimalToPackedDecimal(new BigDecimal(bigIntegerValue),
                packedDecimal, 0, precision, checkOverflow);
        convertPackedDecimalToExternalDecimal(packedDecimal, 0,
                externalDecimal, offset, precision, decimalType);
    }

    /**
     * Converts a BigInteger value to a Unicode Decimal in a char array
     * 
     * Overflow can happen if the BigInteger does not fit into the char array. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param bigIntegerValue
     *            BigInteger value to be converted
     * @param unicodeDecimal
     *            char array that will hold the Unicde decimal on a successful return
     * @param offset
     *            offset into <code>unicodeDecimal</code> where the Unicode Decimal is expected to be located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * @param decimalType
     *            constant indicating the type of External Decimal
     * 
     * @throws NullPointerException
     *             if <code>unicodeDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result overflows
     * @throws IllegalArgumentException
     *             if <code>decimalType</code> or <code>precision</code> is invalid
     */
    public static void convertBigIntegerToUnicodeDecimal(
            BigInteger bigIntegerValue, char[] unicodeDecimal, int offset,
            int precision, boolean checkOverflow, int decimalType) {
        byte[] packedDecimal = new byte[precision / 2 + 1];
        convertBigDecimalToPackedDecimal(new BigDecimal(bigIntegerValue),
                packedDecimal, 0, precision, checkOverflow);
        convertPackedDecimalToUnicodeDecimal(packedDecimal, 0, unicodeDecimal,
                offset, precision, decimalType);
    }

    /**
     * Converts a BigDecimal into a Packed Decimal in a byte array
     * 
     * Overflow can happen if the BigDecimal does not fit into the result byte array. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param bigDecimalValue
     *            the BigDecimal value to be converted
     * @param packedDecimal
     *            byte array which will hold the Packed Decimal on a successful return
     * @param offset
     *            desired offset in <code>packedDecimal</code> where the Packed Decimal is expected to be located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * 
     * @throws NullPointerException
     *             if <code>packedDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result overflows
     */
    public static void convertBigDecimalToPackedDecimal(
            BigDecimal bigDecimalValue, byte[] packedDecimal, int offset,
            int precision, boolean checkOverflow) {
    
        int bdprec = bigDecimalValue.precision();
        if (bdprec <=9)
        {
            convertIntegerToPackedDecimal((int) bigDecimalValue.unscaledValue().longValue(),
                                           packedDecimal, offset, precision, checkOverflow);
            return;
        }
        if (bdprec <= 18)
        {
           convertLongToPackedDecimal(bigDecimalValue.unscaledValue().longValue(),
        		   	                  packedDecimal, offset, precision, checkOverflow);
           return;
        }
    
        slowBigDecimalToSignedPacked(bigDecimalValue, packedDecimal, offset,
                precision, checkOverflow);
    }

    /**
     * Converts a BigDecimal value to an External Decimal in a byte array
     * 
     * Overflow can happen if the BigDecimal does not fit into the result byte array. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param bigDecimalValue
     *            BigDecimal value to be converted
     * @param externalDecimal
     *            byte array that will hold the External Decimal on a successful return
     * @param offset
     *            offset in <code>externalDecimal</code> where the External Decimal is expected to be located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * @param decimalType
     *            constant value indicating the External Decimal type
     * 
     * @throws NullPointerException
     *             if <code>externalDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result overflows
     * @throws IllegalArgumentException
     *             if <code>precision</code> or <code>decimalType</code> is invalid
     */
    public static void convertBigDecimalToExternalDecimal(
            BigDecimal bigDecimalValue, byte[] externalDecimal, int offset,
            int precision, boolean checkOverflow, int decimalType) {
    	
    	int bdprec = bigDecimalValue.precision();
        if (bdprec <=9)
        {
            convertIntegerToExternalDecimal((int) bigDecimalValue.unscaledValue().longValue(),
            		externalDecimal, offset, precision, checkOverflow, decimalType);
            return;
        }
        if (bdprec <= 18)
        {
           convertLongToExternalDecimal(bigDecimalValue.unscaledValue().longValue(),
        		   externalDecimal, offset, precision, checkOverflow, decimalType);
           return;
        }
    
        byte[] packedDecimal = new byte[precision / 2 + 1];
        convertBigDecimalToPackedDecimal(bigDecimalValue, packedDecimal, 0,
                precision, checkOverflow);
        convertPackedDecimalToExternalDecimal(packedDecimal, 0,
                externalDecimal, offset, precision, decimalType);
    }

    /**
     * Converts a BigDecimal value to a Unicode Decimal in a char array
     * 
     * Overflow can happen if the BigDecimal does not fit into the result char array. In this case, when
     * <code>checkOverflow</code> is true an <code>ArithmeticException</code> is thrown, when false a truncated or
     * invalid result is returned.
     * 
     * @param bigDecimalValue
     *            BigDecimal value to be converted
     * @param unicodeDecimal
     *            char array which will hold the Unicode Decimal on a successful return
     * @param offset
     *            offset in <code>unicodeDecimal</code> where the Unicode Decimal is expected to be located
     * @param precision
     *            number of decimal digits. Maximum valid precision is 253
     * @param checkOverflow
     *            if true an <code>ArithmenticException</code> will be thrown if the decimal value does not fit in the
     *            specified precision (overflow)
     * @param decimalType
     *            constant value that indicates the type of External Decimal
     * 
     * @throws NullPointerException
     *             if <code>unicodeDecimal</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if <code>checkOverflow</code> is true and the result overflows
     * @throws IllegalArgumentException
     *             if <code>decimalType</code> or <code>precision</code> is invalid
     */
    public static void convertBigDecimalToUnicodeDecimal(
            BigDecimal bigDecimalValue, char[] unicodeDecimal, int offset,
            int precision, boolean checkOverflow, int decimalType) {
        byte[] packedDecimal = new byte[precision / 2 + 1];
        convertBigDecimalToPackedDecimal(bigDecimalValue, packedDecimal, 0,
                precision, checkOverflow);
        convertPackedDecimalToUnicodeDecimal(packedDecimal, 0, unicodeDecimal,
                offset, precision, decimalType);
    }

    // below is code taken from BigDecimalConverters
    // these are special functions recognized by the jit
    private static boolean DFPFacilityAvailable() {
        return false;
    }

    private static boolean DFPUseDFP() {
        return false;
    }

    private static BigDecimal createZeroBigDecimal() {
        return new BigDecimal(0); // don't use BigDecimal.ZERO, need a new
                                  // object every time
    }

    private static boolean DFPConvertPackedToDFP(BigDecimal bd, long pack,
            int scale, boolean signed) {
        return false;
    }

    private static long DFPConvertDFPToPacked(long dfpValue, boolean signed) {
        return Long.MAX_VALUE;
    }

    private final static int getflags() {
        return 0x3;
    }

    private final static long getlaside(BigDecimal bd) {
        return Long.MIN_VALUE;
    }

    private static BigDecimal slowSignedPackedToBigDecimal(byte[] byteArray,
            int offset, int precision, int scale, boolean exceptions) {


        int length = (precision + 2) / 2;

        int sn = byteArray[offset + length - 1] & CommonData.LOWER_NIBBLE_MASK;

        char[] temp = new char[length * 2];
        temp[0] = (sn == 0x0D || sn == 0x0B) ? '-' : '0';
        for (int i = 0; i < length - 1; i++) {
            temp[2 * i + 1] = (char) ('0' + (byteArray[i + offset] >>> 4 & CommonData.LOWER_NIBBLE_MASK));
            temp[2 * i + 2] = (char) ('0' + (byteArray[i + offset] & CommonData.LOWER_NIBBLE_MASK));
        }
        temp[length * 2 - 1] = (char) ('0' + (byteArray[offset + length - 1] >>> 4 & CommonData.LOWER_NIBBLE_MASK));

        return new BigDecimal(new BigInteger(new String(temp)), scale);
    }

    private static void slowBigDecimalToSignedPacked(BigDecimal bd,
            byte[] byteArray, int offset, int precision, boolean checkOverflow) {

        // digits are right justified in the return byte array
        if (checkOverflow && precision < bd.precision())
            throw new ArithmeticException(
                    "Decimal overflow - precision of result Packed Decimal lesser than BigDecimal precision");

        BigInteger value = bd.unscaledValue();
        char[] buffer = value.abs().toString().toCharArray();
        int numDigitsLeft = buffer.length - 1;
        int endPosition = numDigitsLeft % 2;
        int length = (precision + 2) / 2;
        int index = length - 2;

        // take care of the sign nibble and the right most digit
        byteArray[offset + length - 1] = (byte) ((buffer[numDigitsLeft] - '0') << 4);
        byteArray[offset + length - 1] |= ((value.signum() == -1) ? 0x0D : 0x0C);

        // compact 2 digits into each byte
        for (int i = numDigitsLeft - 1; i >= endPosition
            && (offset + index >= 0); i -= 2, --index) {
            byteArray[offset + index] = (byte) (buffer[i] - '0');
            byteArray[offset + index] |= (byte) ((buffer[i - 1] - '0') << 4);
        }

        // if there's a left over digit, put it in the last byte
        if (endPosition > 0 && (offset + index >= 0)) {
            byteArray[offset + index] = (byte) (buffer[0] - '0');
        }
    }

    /*
     * Converts the Packed Decimal to equivalent long (in hex).
     */
    private static long convertPackedToLong(byte[] byteArray, int offset,
            int length) {
        if (length == 8) {
            return ByteArrayUnmarshaller.readLong(byteArray, offset, true);
        } else if (length == 4) {
            // We need to zero extend the int to long.
            return ((long) ByteArrayUnmarshaller.readInt(byteArray, offset,
                    true)) & 0xFFFFFFFFl;
        }

        else {
            // slow way to load value into dfp
            long value = 0;
            for (int i = 0; i < length; ++i) {
                value = value << 8;
                value += ((long) (byteArray[offset + i] & 0xFF));
            }
            return value;
        }
    }

    private static void convertLongToPacked(long value, byte[] byteArray,
            int offset, int length) {
        if (length == 8) {
            ByteArrayMarshaller.writeLong(value, byteArray, offset, true);
        } else if (length == 4) {
            ByteArrayMarshaller.writeInt((int) value, byteArray, offset, true);
        } else {
            for (int i = length - 1; i <= 0; i--) {
                byteArray[offset + i] = (byte) ((value) & 0xFF);
                value = value >> 8;
            }
        }
    }

}
