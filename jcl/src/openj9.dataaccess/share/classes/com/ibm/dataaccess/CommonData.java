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

import java.util.Arrays;

/**
 * Common data to assist conversions between binary, packed decimal & zoned
 * decimal representations and arithmetic operations on packed decimals.
 *  *
 * @author IBM
 * @version $Revision$ on $Date$ 
 */

class CommonData {

	public final static int HIGHER_NIBBLE_MASK = 0xF0;
    public final static int LOWER_NIBBLE_MASK = 0x0F;
    public final static int INTEGER_MASK = 0xFF; // used for converting byte to
                                                 // integer
    /** Constant representing a packed zero */
    final static byte PACKED_ZERO = (byte) (0x00);

    final static byte PACKED_SIGNED_ZERO = (byte) 0x0C;
    final static byte PACKED_PLUS = (byte) 0x0C;
    final static byte PACKED_MINUS = (byte) 0x0D;
    private static final byte PACKED_ALT_PLUS = (byte) 0x0F;
    private static final byte PACKED_ALT_PLUS1 = (byte) 0x0E;
    private static final byte PACKED_ALT_PLUS2 = (byte) 0x0A;
    private static final byte PACKED_ALT_MINUS = (byte) 0x0B;
    protected static final byte EXTERNAL_SIGN_PLUS = (byte)0x4e;
    protected static final byte EXTERNAL_SIGN_MINUS = (byte)0x60;
    protected static final byte EXTERNAL_EMBEDDED_SIGN_PLUS = (byte)0xC0;
    protected static final byte EXTERNAL_EMBEDDED_SIGN_MINUS = (byte)0xD0;
        
    protected static final byte EXTERNAL_EMBEDDED_SIGN_PLUS_ALTERNATE_A = (byte) 0xA0;    
    protected static final byte EXTERNAL_EMBEDDED_SIGN_PLUS_ALTERNATE_E = (byte) 0xE0; 
    protected static final byte EXTERNAL_EMBEDDED_SIGN_PLUS_ALTERNATE_F = (byte) 0xF0; 
    protected static final byte EXTERNAL_EMBEDDED_SIGN_MINUS_ALTERNATE_B = (byte) 0xB0; 
        
    protected final static byte PACKED_INVALID_DIGIT = (byte) (0xff);
    
    /**
     * The map table's for calculating byte arithmetics. sized to be 2^10 as we
     * need to consider carries/borrows.
     */
    private static final int BYTE_ARITHMETICS_TABLE_LENGTH = 1024;
    
    /**
     * Outputs the sum of two bytes
     */
    public static int getPackedSumValues(int input) {
        return packedSumValues[input] & INTEGER_MASK;
    }
    
    /**
     * counts how many bytes are there in an external decimal.
     */
    public static int getExternalByteCounts(int precision, int decimalType)
    {
        switch (decimalType)
        {
        case DecimalData.EBCDIC_SIGN_EMBEDDED_TRAILING:
        case DecimalData.EBCDIC_SIGN_EMBEDDED_LEADING:
            return precision;
        case DecimalData.EBCDIC_SIGN_SEPARATE_TRAILING:
        case DecimalData.EBCDIC_SIGN_SEPARATE_LEADING:
            return precision+1;
        default:
            throw new IllegalArgumentException("illegal decimalType.");
        }
    }

    /**
     * Outputs the sum of two bytes plus one
     */
    public static byte getPackedSumPlusOneValues(int input) {
        return packedSumPlusOneValues[input];
    }
    
    /**
     * Outputs the difference of two bytes
     */
    public static byte getPackedDifferenceValues(int input) {
        return packedDifferenceValues[input];
    }

    /**
     * Outputs the difference of two bytes minus one
     */
    public static byte getPackedDifferenceMinusOneValues(int input) {
        return packedDifferenceMinusOneValues[input];
    }
    
    /**
     * Outputs the sum of the input and one
     */
    public static byte getPackedAddOneValues(byte input) {
        int carryHighDigit = ((input & CommonData.HIGHER_NIBBLE_MASK) >> 4);
        int carryLowDigit = (input & CommonData.LOWER_NIBBLE_MASK) + 1;
        if (carryLowDigit == 10) {
            carryLowDigit = 0;
            carryHighDigit = carryHighDigit + 1;
            if (carryHighDigit == 10) {
                carryHighDigit = 0;
            }
        }
        return (byte) ((carryHighDigit << 4) + carryLowDigit);
    }

    /**
     * Outputs the difference of two bytes
     */
    public static byte getPackedBorrowOneValues(byte input) {
        int borrowHighDigit = ((input & CommonData.HIGHER_NIBBLE_MASK) >> 4);
        int borrowLowDigit = (input & CommonData.LOWER_NIBBLE_MASK) - 1;
        if (borrowLowDigit < 0) {
            borrowLowDigit = 9;
            borrowHighDigit = borrowHighDigit - 1;
            if (borrowHighDigit < 0) {
                borrowHighDigit = 9;
            }
        }
        return (byte) ((borrowHighDigit << 4) + borrowLowDigit);
    }

    /**
     * Converts a packed byte to binary value
     */
    public static int getPackedToBinaryValues(int input) {
    	return (((input & CommonData.HIGHER_NIBBLE_MASK) >> 4) * 10) + 
    	(input & CommonData.LOWER_NIBBLE_MASK);
    }

    /**
     * Converts a binary value to a packed byte
     */
    public static byte getBinaryToPackedValues(int input) {
    	int value = ((input/10) << 4) + (input % 10);
    	return (byte)value;
    }

    /**
     * Normalizes the input sign code to the preferred sign code.
     * 
     * @return PACKED_MINUS if and only if the input is PACKED_MINUS or PACKED_ALT_MINUS, otherwise PACKED_PLUS
     */
    public static byte getSign(int i) {
        return (i == PACKED_MINUS || i == PACKED_ALT_MINUS) ? PACKED_MINUS : PACKED_PLUS;
    }
    
    /**
     * Returns the number of bytes required for storing a packed decimal of a
     * given precision
     * 
     * @param precision
     *            number of packed decimal digits
     * @return number of bytes required for storing the packed decimal
     * 
     * @throws ArrayIndexOutOfBoundsException
     *             if the precision value is invalid
     */
    public static int getPackedByteCount(int precision) {
        return ((precision / 2) + 1);
    }
    
    /**
     * Outputs the sum of the input and one taking into consideration the sign
     * of the input
     */
    public static byte getPackedAddOneSignValues(byte input) {
    	int digit = (input & CommonData.HIGHER_NIBBLE_MASK) >> 4;
    	digit++;
    	if (digit == 10)
            digit = 0;
    	return (byte) (digit << 4 | (input & CommonData.LOWER_NIBBLE_MASK));
    }
    /**
     * Byte array of single byte packed decimals. Each packed decimal is the sum
     * of two packed decimals operands. The index is a function of the operand
     * values.
     */
    private static final byte[] packedSumValues = new byte[BYTE_ARITHMETICS_TABLE_LENGTH];

    /**
     * Byte array of single byte packed decimals. Each packed decimal is the sum
     * of two packed decimals operands and a carry from the previous byte. The
     * index is a function of the operand values.
     */
    private static final byte[] packedSumPlusOneValues = new byte[BYTE_ARITHMETICS_TABLE_LENGTH];

    /**
     * Byte array of single byte packed decimals. Each packed decimal is the
     * difference of two packed decimal operands. The index is a function of the
     * operand values.
     */
    private static final byte[] packedDifferenceValues = new byte[BYTE_ARITHMETICS_TABLE_LENGTH];

    /**
     * Byte array of single byte packed decimals. Each packed decimal is the
     * difference of two packed decimal operands and a borrow from the previous
     * byte. The index is a function of the operand values.
     */
    private static final byte[] packedDifferenceMinusOneValues = new byte[BYTE_ARITHMETICS_TABLE_LENGTH];

    static {
        int i, j, m, n;
        
        Arrays.fill(packedSumValues, PACKED_INVALID_DIGIT);
        Arrays.fill(packedSumPlusOneValues, PACKED_INVALID_DIGIT);
        Arrays.fill(packedDifferenceValues, PACKED_INVALID_DIGIT);
        Arrays.fill(packedDifferenceMinusOneValues, PACKED_INVALID_DIGIT);
        // setting valid elements
        for (i = 0; i < 10; i++) {
            for (j = 0; j < 10; j++) {
                for (m = 0; m < 10; m++) {
                    for (n = 0; n < 10; n++) {
                        setPackedSumArrays(i, j, m, n);
                    }
                }
            }
        }
    }

    /**
     * Creates arrays holding results of simple operations on bytes comprising
     * packed decimal digits.
     * 
     * @param i
     *            first nibble (digit) of the first byte
     * @param j
     *            second nibble (digit) of the first byte
     * @param m
     *            first nibble (digit) of the second byte
     * @param n
     *            second nibble (digit) of the second byte
     */
    public static void setPackedSumArrays(int i, int j, int m, int n) {
        int opAIndexValue, opBIndexValue;
        int valueIdx, onesValue, tensValue;
        byte byteValue;
        // allow 5 bits for both tens sum and ones sum to get unique index
        opAIndexValue = (i << 5) + j;
        opBIndexValue = (m << 5) + n;
        valueIdx = opAIndexValue + opBIndexValue;
        // a + b or b + a values
        onesValue = (j + n) % 10;
        if (onesValue < j)
            tensValue = (i + m + 1) % 10;
        else
            tensValue = (i + m) % 10;
        byteValue = (byte) ((tensValue << 4) + onesValue);
        packedSumValues[valueIdx] = byteValue;

        // a + b + 1(carry to previous byte) values
        onesValue = (j + n + 1) % 10;
        if (onesValue <= j)
            tensValue = (i + m + 1) % 10;
        else
            tensValue = (i + m) % 10;
        byteValue = (byte) ((tensValue << 4) + onesValue);
        packedSumPlusOneValues[valueIdx] = byteValue;

        valueIdx = (opAIndexValue - opBIndexValue) & 0x3FF;
        // a - b values
        onesValue = (j - n + 10) % 10;
        if (onesValue > j) // borrow required;
            tensValue = (i - m - 1 + 10) % 10;
        else
            tensValue = (i - m + 10) % 10;
        byteValue = (byte) ((tensValue << 4) + onesValue);

        if (packedDifferenceValues[valueIdx] == PACKED_INVALID_DIGIT)
            packedDifferenceValues[valueIdx] = byteValue;

        // a - b - 1 (borrow from previous byte) values
        onesValue = (j - n - 1 + 10) % 10;
        if (onesValue >= j) // borrow required;
            tensValue = (i - m - 1 + 10) % 10;
        else
            tensValue = (i - m + 10) % 10;
        byteValue = (byte) ((tensValue << 4) + onesValue);
        packedDifferenceMinusOneValues[valueIdx] = byteValue;
    }

}
