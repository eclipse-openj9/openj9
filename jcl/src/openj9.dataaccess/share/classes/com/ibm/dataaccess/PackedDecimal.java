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

import java.math.BigInteger;
import java.util.Arrays;

import com.ibm.dataaccess.CommonData;

/**
 * Arithmetic, copying and shifting operations for Packed Decimal data.
 * 
 * @author IBM
 * @version $Revision$ on $Date$ 
 */
public final class PackedDecimal {

    /**
     * Private constructor, class contains only static methods.
     */
    private PackedDecimal() {
        super();
    }

    private static final ThreadLocal<PackedDecimalOperand> op1_threadLocal = new ThreadLocal<PackedDecimalOperand>() {
        protected PackedDecimalOperand initialValue()
        {
        return new PackedDecimalOperand();
        }
    };
    private static final ThreadLocal<PackedDecimalOperand> op2_threadLocal = new ThreadLocal<PackedDecimalOperand>() {
        protected PackedDecimalOperand initialValue()
        {
        return new PackedDecimalOperand();
        }
    };
    private static final ThreadLocal<PackedDecimalOperand> sum_threadLocal = new ThreadLocal<PackedDecimalOperand>() {
        protected PackedDecimalOperand initialValue()
        {
        return new PackedDecimalOperand();
        }
    };

    /**
     * Checks the validity of a Packed Decimal, return code indicating the status of the Packed Decimal.
     * 
     * @param byteArray
     *            the source container array
     * @param offset
     *            starting offset of the Packed Decimal
     * @param precision
     *            precision of the Packed Decimal. Maximum valid precision is 253
     * @param ignoreHighNibbleForEvenPrecision
     *            if true, ignore to check if the top nibble (first 4 bits) of the input is an invalid sign value in the
     *            case of even precision
     * @param canOverwriteHighNibbleForEvenPrecision
     *            if true, change the high nibble to a zero in case of even precision
     * @return the condition code: 0 All digit codes and the sign valid 1 Sign invalid 2 At least one digit code invalid
     *         3 Sign invalid and at least one digit code invalid
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static int checkPackedDecimal(byte[] byteArray, int offset,
            int precision, boolean ignoreHighNibbleForEvenPrecision,
            boolean canOverwriteHighNibbleForEvenPrecision) {
        if ((offset + ((precision / 2) + 1) > byteArray.length) || (offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"checkPackedDecimal is trying to access byteArray[" + offset + "] to byteArray[" + (offset + (precision / 2)) + "]" +
        		" but valid indices are from 0 to " + (byteArray.length - 1) + ".");
     
        return checkPackedDecimal_(byteArray, offset, precision, 
                ignoreHighNibbleForEvenPrecision, canOverwriteHighNibbleForEvenPrecision);
    }
    
    private static int checkPackedDecimal_(byte[] byteArray, int offset,
            int precision, boolean ignoreHighNibbleForEvenPrecision,
            boolean canOverwriteHighNibbleForEvenPrecision) {
        
    	if (precision < 1)
            throw new IllegalArgumentException("Illegal Precision.");
    	
    	boolean evenPrecision = precision % 2 == 0 ? true : false;
        int signOffset = offset + CommonData.getPackedByteCount(precision) - 1;

        int returnCode = 0;

        if (canOverwriteHighNibbleForEvenPrecision && evenPrecision) {
            byteArray[offset] = (byte) (byteArray[offset] & CommonData.LOWER_NIBBLE_MASK);
        }

        if (evenPrecision && ignoreHighNibbleForEvenPrecision)
        {
        if  ((byteArray[offset] & CommonData.LOWER_NIBBLE_MASK) > 0x09)
           returnCode = 2;
        offset++;
        }


        // ordinary checking places here:
        int i;
        for (i = offset; i < signOffset && byteArray[i] == CommonData.PACKED_ZERO; i++)
            ;
        
        for (; i < signOffset; i++)
        {
        	if ((byteArray[i] & CommonData.LOWER_NIBBLE_MASK) > 0x09 || 
        			(byteArray[i] & CommonData.HIGHER_NIBBLE_MASK & 0xFF) > 0x90)
        	{
        		returnCode = 2;
            	break;
        	}
        }
        
        if (i == signOffset && (byteArray[signOffset] & CommonData.HIGHER_NIBBLE_MASK & 0xFF) > 0x90)
        	returnCode = 2;
        
        // Check sign nibble
        if ((byteArray[signOffset] & CommonData.LOWER_NIBBLE_MASK) < 0x0A)
        {
        	returnCode++;
        } 
        return returnCode;
    }

    // Assuming canOverwriteMostSignificantEvenNibble == false
    /**
     * Checks the validity of a Packed Decimal, return code indicating the status of the Packed Decimal. The most
     * significant nibble cannot be overwritten.
     * 
     * @param byteArray
     *            the source container array
     * @param offset
     *            starting offset of the Packed Decimal
     * @param precision
     *            precision of the Packed Decimal
     * @param ignoreHighNibbleForEvenPrecision
     *            if true, ignore the high nibble in the case of even precision
     * @return the condition code: 0 All digit codes and the sign valid 1 Sign invalid 2 At least one digit code invalid
     *         3 Sign invalid and at least one digit code invalid
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static int checkPackedDecimal(byte[] byteArray, int offset,
            int precision, boolean ignoreHighNibbleForEvenPrecision) {
        return checkPackedDecimal(byteArray, offset, precision,
        		ignoreHighNibbleForEvenPrecision, false);
    }

    // Assuming canOverwriteMostSignificantEvenNibble == false
    /**
     * Checks the validity of a Packed Decimal, return code indicating the status of the Packed Decimal. Don't ignore
     * the most significant nibble. The most significant nibble cannot be overwritten.
     * 
     * @param byteArray
     *            the source container array
     * @param offset
     *            starting offset of the Packed Decimal
     * @param precision
     *            precision of the Packed Decimal
     * @return the condition code: 0 All digit codes and the sign valid 1 Sign invalid 2 At least one digit code invalid
     *         3 Sign invalid and at least one digit code invalid
     * 
     * @throws NullPointerException
     *             if <code>byteArray</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static int checkPackedDecimal(byte[] byteArray, int offset,
            int precision) {
        return checkPackedDecimal(byteArray, offset, precision, false, false);
    }

    private static void copyRemainingDigits(PackedDecimalOperand op1,
            PackedDecimalOperand op2, boolean checkOverflow)
            throws ArithmeticException {
        PackedDecimalOperand sum = sum_threadLocal.get();

        int bytes;
        // copy any high order digits left in larger value to result
        if (op1.currentOffset >= op1.offset) {

            bytes = op1.currentOffset - op1.offset + 1;
            int sumBytes = sum.currentOffset - sum.offset + 1;
            if (bytes >= sumBytes) {
                if (checkOverflow && bytes > sumBytes)
                    throw new ArithmeticException(
                            "Decimal overflow during addition/subtraction.");
                else
                    bytes = sum.currentOffset - sum.offset + 1;
                sum.currentOffset = sum.currentOffset - bytes + 1;
                op1.currentOffset += -bytes + 1;
                System.arraycopy(op1.byteArray, op1.currentOffset,
                        sum.byteArray, sum.currentOffset, bytes);
                if (sum.precision % 2 == 0) {
                    byte highNibble = (byte) (sum.byteArray[sum.currentOffset] & CommonData.HIGHER_NIBBLE_MASK);
                    if (checkOverflow && bytes == sumBytes
                            && highNibble != 0x00)
                        throw new ArithmeticException(
                                "Decimal overflow during addition/subtraction.");
                    else
                        sum.byteArray[sum.currentOffset] &= CommonData.LOWER_NIBBLE_MASK;
                }
            } else {
                sum.currentOffset = sum.currentOffset - bytes + 1;
                System.arraycopy(op1.byteArray, op1.offset, sum.byteArray,
                        sum.currentOffset, bytes);
            }
            sum.currentOffset--;

        }
        // pad high order result bytes with zeros
        if (sum.currentOffset >= sum.offset) {
            bytes = sum.currentOffset - sum.offset + 1;
            Arrays.fill(sum.byteArray, sum.offset, sum.offset + bytes, CommonData.PACKED_ZERO);
        }
    }

    /**
     * Add two Packed Decimals in byte arrays. The sign of an input Packed Decimal is assumed to to be positive unless
     * the sign nibble contains one of the negative sign codes, in which case the sign of the respective input Packed
     * Decimal is interpreted as negative.
     * 
     * @param result
     *            byte array that will hold the sum of the two operand Packed Decimals
     * @param resultOffset
     *            offset into <code>result</code> where the sum Packed Decimal begins
     * @param resultPrecision
     *            number of Packed Decimal digits for the sum. Maximum valid precision is 253
     * @param op1Decimal
     *            byte array that holds the first operand Packed Decimal.
     * @param op1Offset
     *            offset into <code>op1Decimal</code> where the Packed Decimal. is located
     * @param op1Precision
     *            number of Packed Decimal digits for the first operand. Maximum valid precision is 253
     * @param op2Decimal
     *            byte array that holds the second operand Packed Decimal
     * @param op2Offset
     *            offset into <code>op2Decimal</code> where the Packed Decimal is located
     * @param op2Precision
     *            number of Packed Decimal digits for the second operand. Maximum valid precision is 253
     * @param checkOverflow
     *            check for overflow
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if an overflow occurs during the computation of the sum
     */
    public static void addPackedDecimal(byte[] result, int resultOffset,
            int resultPrecision, byte[] op1Decimal, int op1Offset,
            int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision, boolean checkOverflow) throws ArithmeticException {
        if ((resultOffset + ((resultPrecision / 2) + 1) > result.length) || (resultOffset < 0))
    		throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"addPackedDecimal is trying to access result[" + resultOffset + "] to result[" + (resultOffset + (resultPrecision / 2)) + "]" +
        		" but valid indices are from 0 to " + (result.length - 1) + ".");
            
        if ((op1Offset < 0)    || (op1Offset    + ((op1Precision    / 2) + 1) > op1Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"addPackedDecimal is trying to access op1Decimal[" + op1Offset + "] to op1Decimal[" + (op1Offset + (op1Precision / 2)) + "]" +
        		" but valid indices are from 0 to " + (op1Decimal.length - 1) + ".");
            
        if ((op2Offset < 0)    || (op2Offset    + ((op2Precision    / 2) + 1) > op2Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            	"addPackedDecimal is trying to access op2Decimal[" + op2Offset + "] to op2Decimal[" + (op2Offset + (op2Precision / 2)) + "]" +
            	" but valid indices are from 0 to " + (op2Decimal.length - 1) + ".");
        
        addPackedDecimal_(result, resultOffset, resultPrecision, op1Decimal, op1Offset, 
                          op1Precision, op2Decimal, op2Offset, op2Precision, checkOverflow);
    }
    
    private static void addPackedDecimal_(byte[] result, int resultOffset,
            int resultPrecision, byte[] op1Decimal, int op1Offset,
            int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision, boolean checkOverflow) throws ArithmeticException {
        // capture result type information
        sum_threadLocal.get().setSumOperand(result, resultOffset,
                resultPrecision);
        // ignore leading zeros in operand values
        op1_threadLocal.get().setOperand(op1Decimal, op1Offset, op1Precision);
        op2_threadLocal.get().setOperand(op2Decimal, op2Offset, op2Precision);
        // add values
        computeValue(checkOverflow);
    }

    /**
     * Subtracts two Packed Decimals in byte arrays. The sign of an input Packed Decimal is assumed to to be positive
     * unless the sign nibble contains one of the negative sign codes, in which case the sign of the respective input
     * Packed Decimal is interpreted as negative.
     * 
     * @param result
     *            byte array that will hold the difference of the two operand Packed Decimals
     * @param resultOffset
     *            offset into <code>result</code> where the result Packed Decimal is located
     * @param resultPrecision
     *            number of Packed Decimal digits for the result. Maximum valid precision is 253
     * @param op1Decimal
     *            byte array that holds the first Packed Decimal operand
     * @param op1Offset
     *            offset into <code>op1Decimal</code> where the first operand is located
     * @param op1Precision
     *            number of Packed Decimal digits for the first operand. Maximum valid precision is 253
     * @param op2Decimal
     *            byte array that holds the second Packed Decimal operand
     * @param op2Offset
     *            offset into <code>op2Decimal</code> where the second operand is located
     * @param op2Precision
     *            number of Packed Decimal digits for the second operand. Maximum valid precision is 253
     * @param checkOverflow
     *            check for overflow
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if an overflow occurs during the computation of the difference
     */
    public static void subtractPackedDecimal(byte[] result, int resultOffset,
            int resultPrecision, byte[] op1Decimal, int op1Offset,
            int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision, boolean checkOverflow) throws ArithmeticException {
        if ((resultOffset + ((resultPrecision / 2) + 1) > result.length) || (resultOffset < 0))
    		throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"subtractPackedDecimal is trying to access result[" + resultOffset + "] to result[" + (resultOffset + (resultPrecision / 2)) + "]" +
        		" but valid indices are from 0 to " + (result.length - 1) + ".");
            
        if ((op1Offset < 0)    || (op1Offset    + ((op1Precision    / 2) + 1) > op1Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"subtractPackedDecimal is trying to access op1Decimal[" + op1Offset + "] to op1Decimal[" + (op1Offset + (op1Precision / 2)) + "]" +
        		" but valid indices are from 0 to " + (op1Decimal.length - 1) + ".");
            
        if ((op2Offset < 0)    || (op2Offset    + ((op2Precision    / 2) + 1) > op2Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            	"subtractPackedDecimal is trying to access op2Decimal[" + op2Offset + "] to op2Decimal[" + (op2Offset + (op2Precision / 2)) + "]" +
            	" but valid indices are from 0 to " + (op2Decimal.length - 1) + ".");
        
        subtractPackedDecimal_(result, resultOffset, resultPrecision, op1Decimal, op1Offset, 
                op1Precision, op2Decimal, op2Offset, op2Precision, checkOverflow);
    }
    
    private static void subtractPackedDecimal_(byte[] result, int resultOffset,
            int resultPrecision, byte[] op1Decimal, int op1Offset,
            int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision, boolean checkOverflow) throws ArithmeticException {

        PackedDecimalOperand sum = sum_threadLocal.get();
        PackedDecimalOperand op1 = op1_threadLocal.get();
        PackedDecimalOperand op2 = op2_threadLocal.get();

        // capture result type information
        sum.setSumOperand(result, resultOffset, resultPrecision);
        // ignore leading zeros in operand values
        op1.setOperand(op1Decimal, op1Offset, op1Precision);
        op2.setOperand(op2Decimal, op2Offset, op2Precision);
        // change op2 sign for subtraction
        if ((op2.sign & CommonData.LOWER_NIBBLE_MASK) == CommonData.PACKED_PLUS)
            op2.sign = (op2.sign & CommonData.HIGHER_NIBBLE_MASK)
                    | CommonData.PACKED_MINUS;
        else
            op2.sign = (op2.sign & CommonData.HIGHER_NIBBLE_MASK)
                    | CommonData.PACKED_PLUS;
        // add values
        computeValue(checkOverflow);
    }

    /**
     * Create a positive Packed Decimal representation of zero.
     * 
     * @param byteArray
     *            byte array which will hold the packed zero
     * @param offset
     *            offset into <code>toBytes</code> where the packed zero begins
     * @param len
     *            length of the packed zero. Maximum valid length is 253
     * 
     * @throws NullPointerException
     *             if <code>toBytes</code> is null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static void setPackedZero(byte[] byteArray, int offset, int len) {
        int byteLen = CommonData.getPackedByteCount(len);
        Arrays.fill(byteArray, offset, offset + byteLen - 1, CommonData.PACKED_ZERO);
        byteArray[offset + byteLen - 1] = CommonData.PACKED_PLUS;
    }

    private static void computeSum(PackedDecimalOperand op1,
            PackedDecimalOperand op2, boolean checkOverflow)
            throws ArithmeticException {

        PackedDecimalOperand sum = sum_threadLocal.get();

        boolean carry;// add op2 sign digit to op1 sign digit
        sum.indexValue = ((op1.signDigit + op2.signDigit) << 1) & 0x3FF;
        sum.byteValue = CommonData.getPackedSumValues(sum.indexValue);
        carry = sum.byteValue < op1.signDigit;
        sum.byteArray[sum.signOffset] = (byte) (sum.byteValue | (op1.sign & CommonData.LOWER_NIBBLE_MASK));

        // add op2 digits to op1 digits
        for (; op2.currentOffset >= op2.offset
                && sum.currentOffset >= sum.offset; op2.currentOffset -= 1, op1.currentOffset -= 1, sum.currentOffset -= 1) {
            if (checkOverflow && sum.currentOffset < sum.offset) {
                throw new ArithmeticException(
                        "Decimal overflow in addPackedDecimal.");
            }
            ;
            op1.byteValue = op1.byteArray[op1.currentOffset]
                    & CommonData.INTEGER_MASK;
            op2.byteValue = op2.byteArray[op2.currentOffset]
                    & CommonData.INTEGER_MASK;
            op1.indexValue = op1.byteValue
                    + (op1.byteValue & CommonData.HIGHER_NIBBLE_MASK);
            op2.indexValue = op2.byteValue
                    + (op2.byteValue & CommonData.HIGHER_NIBBLE_MASK);
            sum.indexValue = op1.indexValue + op2.indexValue;
            if (carry)
                sum.byteValue = CommonData
                        .getPackedSumPlusOneValues(sum.indexValue);
            else
                sum.byteValue = CommonData.getPackedSumValues(sum.indexValue);
            carry = (sum.byteValue & CommonData.INTEGER_MASK) < ((op1.byteValue & CommonData.INTEGER_MASK) + (op2.byteValue & CommonData.INTEGER_MASK));
            sum.byteArray[sum.currentOffset] = (byte) (sum.byteValue);
        }
        // if carry, add one to remaining high order digits from op1
        for (; carry && op1.currentOffset >= op1.offset
                && sum.currentOffset >= sum.offset; op1.currentOffset -= 1, sum.currentOffset -= 1) {
            if (checkOverflow && sum.currentOffset < sum.offset) {
                throw new ArithmeticException(
                        "Decimal overflow in addPackedDecimal");
            }
            sum.byteValue = CommonData
                    .getPackedAddOneValues(op1.byteArray[op1.currentOffset])
                    & CommonData.INTEGER_MASK;
            carry = (sum.byteValue == 0x00);
            sum.byteArray[sum.currentOffset] = (byte) (sum.byteValue);
        }
        if (sum.currentOffset < sum.offset) // reached the end
        {
            if ((carry && checkOverflow)
                    || (checkOverflow && op1.currentOffset >= op1.offset))
                throw new ArithmeticException(
                        "Decimal overflow in addPackedDecimal");
            else if (sum.precision % 2 == 0) {
                if ((byte) (sum.byteArray[sum.offset] & CommonData.HIGHER_NIBBLE_MASK) > (byte) 0x00
                        && checkOverflow)
                    throw new ArithmeticException(
                            "Decimal overflow in addPackedDecimal");
                sum.byteArray[sum.offset] &= CommonData.LOWER_NIBBLE_MASK;
            }
            return;
        }
        // if still carry, add high order one to sum
        if (carry) { // carry
            sum.byteArray[sum.currentOffset] = 1;
            sum.currentOffset -= 1;
        }
        // copy any remaining digits
        copyRemainingDigits(op1, op2, checkOverflow);
    }

    private static void computeDifference(PackedDecimalOperand op1,
            PackedDecimalOperand op2, boolean checkOverflow)
            throws ArithmeticException {

        PackedDecimalOperand sum = sum_threadLocal.get();

        boolean borrow;
        // compute difference from sign byte
        borrow = op1.signDigit < op2.signDigit;

        sum.byteValue = Math.abs((op1.signDigit >> 4) - (op2.signDigit >> 4)
                + 10) % 10;
        sum.byteArray[sum.signOffset] = (byte) ((sum.byteValue << 4) | (op1.sign & CommonData.LOWER_NIBBLE_MASK));

        // compute op1 digit values - op2 digit values ;
        for (op2.currentOffset = op2.signOffset - 1, op1.currentOffset = op1.signOffset - 1; op2.currentOffset >= op2.offset
                && sum.currentOffset >= sum.offset; op2.currentOffset -= 1, op1.currentOffset -= 1, sum.currentOffset -= 1) {
            if (checkOverflow && sum.currentOffset < sum.offset) {
                throw new ArithmeticException(
                        "Decimal overflow in subtractPackedDecimal");
            }

            op1.byteValue = op1.byteArray[op1.currentOffset]
                    & CommonData.INTEGER_MASK;
            op2.byteValue = op2.byteArray[op2.currentOffset]
                    & CommonData.INTEGER_MASK;
            op1.indexValue = op1.byteValue
                    + (op1.byteValue & CommonData.HIGHER_NIBBLE_MASK);
            op2.indexValue = op2.byteValue
                    + (op2.byteValue & CommonData.HIGHER_NIBBLE_MASK);
            sum.indexValue = (op1.indexValue - op2.indexValue) & 0x3FF;
            if (borrow)
                sum.byteValue = CommonData
                        .getPackedDifferenceMinusOneValues(sum.indexValue);
            else
                sum.byteValue = CommonData
                        .getPackedDifferenceValues(sum.indexValue);
            borrow = (sum.byteValue & CommonData.INTEGER_MASK) > ((op1.byteValue & CommonData.INTEGER_MASK) - (op2.byteValue & CommonData.INTEGER_MASK));
            if (sum.currentOffset >= sum.offset)
                sum.byteArray[sum.currentOffset] = (byte) (sum.byteValue);
        }
        // if borrow, subtract one from remaining high order digits
        for (; borrow && op1.currentOffset >= op1.offset
                && sum.currentOffset >= sum.offset; op1.currentOffset -= 1, sum.currentOffset -= 1) {
            if (checkOverflow && sum.currentOffset < sum.offset) {
                throw new ArithmeticException(
                        "Decimal overflow in subtractPackedDecimal");
            }
            sum.byteValue = CommonData
                    .getPackedBorrowOneValues(op1.byteArray[op1.currentOffset])
                    & CommonData.INTEGER_MASK;
            borrow = (sum.byteValue == 0x99);
            if (sum.currentOffset >= sum.offset)
                sum.byteArray[sum.currentOffset] = (byte) (sum.byteValue);
        }

        copyRemainingDigits(op1, op2, checkOverflow);
    }

    private static void computeValue(boolean checkOverflow)
            throws ArithmeticException {

        PackedDecimalOperand sum = sum_threadLocal.get();
        PackedDecimalOperand op1 = op1_threadLocal.get();
        PackedDecimalOperand op2 = op2_threadLocal.get();

        if ((op1.sign & CommonData.LOWER_NIBBLE_MASK) == (op2.sign & CommonData.LOWER_NIBBLE_MASK)) {
            // signs are same, compute sum of values
            // add less bytes to more bytes
            if (op1.bytes < op2.bytes)
                computeSum(op2, op1, checkOverflow);
            else
                computeSum(op1, op2, checkOverflow);
        } else { // signs are different, compute difference of values
                 // subtract smaller value from larger value so we will always
                 // have one to borrow
            if (op1.bytes < op2.bytes)
                computeDifference(op2, op1, checkOverflow); // op2 has more
                                                            // non-zero bytes
            else if (op1.bytes > op2.bytes)
                computeDifference(op1, op2, checkOverflow); // op2 has more
                                                            // non-zero bytes
            else {
                // compare values to find which is larger.
                // adjust offsets at same time - difference between equal bytes
                // is 0
                findDifference: {
                    for (; op1.offset < op1.signOffset; op1.offset += 1, op2.offset += 1) {
                        op1.byteValue = op1.byteArray[op1.offset];
                        op2.byteValue = op2.byteArray[op2.offset];
                        if (op1.byteValue != op2.byteValue)
                            break findDifference;
                    }
                    op1.byteValue = op1.byteArray[op1.offset]
                            & CommonData.HIGHER_NIBBLE_MASK;
                    op2.byteValue = op2.byteArray[op2.offset]
                            & CommonData.HIGHER_NIBBLE_MASK;
                    if (op1.byteValue == op2.byteValue) {
                        // values are equal, difference is zero
                        setPackedZero(sum.byteArray, sum.offset, sum.precision);
                        return;
                    }
                }
                if ((op1.byteValue & CommonData.INTEGER_MASK) > (op2.byteValue & CommonData.INTEGER_MASK))
                    computeDifference(op1, op2, checkOverflow);
                else
                    computeDifference(op2, op1, checkOverflow);
            }
        }
    }

    // The implementations of multiply, divide and remainder are based on
    // BigDecimal
    // for simplicity and also because it has been shown that they have
    // acceptable performance.

    private static final int MULTIPLY = 1, DIVIDE = 2, REMAINDER = 3;

    /**
     * Multiplies two Packed Decimals in byte arrays. The sign of an input Packed Decimal is assumed to to be positive
     * unless the sign nibble contains one of the negative sign codes, in which case the sign of the respective input
     * Packed Decimal is interpreted as negative.
     * 
     * @param result
     *            byte array that will hold the product Packed Decimal
     * @param resultOffset
     *            offset into <code>result</code> where the product Packed Decimal is located
     * @param resultPrecision
     *            the length (number of digits) of the product Packed Decimal. Maximum valid precision is 253
     * @param op1Decimal
     *            byte array that will hold the multiplicand Packed Decimal
     * @param op1Offset
     *            offset into <code>op1Decimal</code> where the multiplicand is located
     * @param op1Precision
     *            the length (number of digits) of the multiplicand Packed Decimal. Maximum valid precision is 253
     * @param op2Decimal
     *            byte array that will hold the multiplier
     * @param op2Offset
     *            offset into <code>op2Decimal</code> where the multiplier is located
     * @param op2Precision
     *            the length (number of digits) of the multiplier Packed Decimal. Maximum valid precision is 253
     * @param checkOverflow
     *            if set to true, Packed Decimals are validated before multiplication and an
     *            <code>IllegalArgumentException</code> or <code>ArithmeticException</code> may be thrown. If not, the
     *            result can be invalid
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws IllegalArgumentException
     *             if an overflow occurs during multiplication
     * @throws ArithmeticException
     *             if any of the Packed Decimal operands are invalid
     */
    public static void multiplyPackedDecimal(byte[] result, int resultOffset,
            int resultPrecision, byte[] op1Decimal, int op1Offset,
            int op1Precision, // multiplicand
            byte[] op2Decimal, int op2Offset, int op2Precision, // multiplier
            boolean checkOverflow) {
        if ((resultOffset + ((resultPrecision / 2) + 1) > result.length) || (resultOffset < 0))
    		throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"multiplyPackedDecimal is trying to access result[" + resultOffset + "] to result[" + (resultOffset + (resultPrecision / 2)) + "]" +
        		" but valid indices are from 0 to " + (result.length - 1) + ".");
            
        if ((op1Offset < 0)    || (op1Offset    + ((op1Precision    / 2) + 1) > op1Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"multiplyPackedDecimal is trying to access op1Decimal[" + op1Offset + "] to op1Decimal[" + (op1Offset + (op1Precision / 2)) + "]" +
        		" but valid indices are from 0 to " + (op1Decimal.length - 1) + ".");
            
        if ((op2Offset < 0)    || (op2Offset    + ((op2Precision    / 2) + 1) > op2Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            	"multiplyPackedDecimal is trying to access op2Decimal[" + op2Offset + "] to op2Decimal[" + (op2Offset + (op2Precision / 2)) + "]" +
            	" but valid indices are from 0 to " + (op2Decimal.length - 1) + ".");
        
        multiplyPackedDecimal_(result, resultOffset, resultPrecision, op1Decimal, op1Offset, 
                          op1Precision, op2Decimal, op2Offset, op2Precision, checkOverflow);
    }
    
    private static void multiplyPackedDecimal_(byte[] result, int resultOffset,
            int resultPrecision, byte[] op1Decimal, int op1Offset,
            int op1Precision, // multiplicand
            byte[] op2Decimal, int op2Offset, int op2Precision, // multiplier
            boolean checkOverflow) {
        packedDecimalBinaryOp(MULTIPLY, result, resultOffset, resultPrecision,
                op1Decimal, op1Offset, op1Precision, op2Decimal, op2Offset,
                op2Precision, checkOverflow);
    }

    /**
     * Divides two Packed Decimals is byte arrays. The sign of an input Packed Decimal is assumed to to be positive
     * unless the sign nibble contains one of the negative sign codes, in which case the sign of the respective input
     * Packed Decimal is interpreted as negative.
     * 
     * @param result
     *            byte array that will hold the quotient Packed Decimal
     * @param resultOffset
     *            offset into <code>result</code> where the quotient Packed Decimal is located
     * @param resultPrecision
     *            the length (number of digits) of the quotient Packed Decimal. Maximum valid precision is 253
     * @param op1Decimal
     *            byte array that will hold the dividend Packed Decimal
     * @param op1Offset
     *            offset into <code>op1Decimal</code> where the dividend is located
     * @param op1Precision
     *            the length (number of digits) of the dividend Packed Decimal. Maximum valid precision is 253
     * @param op2Decimal
     *            byte array that will hold the divisor
     * @param op2Offset
     *            offset into <code>op2Decimal</code> where the divisor is located
     * @param op2Precision
     *            the length (number of digits) of the divisor Packed Decimal. Maximum valid precision is 253
     * @param checkOverflow
     *            if set to true, Packed Decimals are validated before division and an
     *            <code>IllegalArgumentException</code> or <code>ArithmeticException</code> may be thrown. If not, the
     *            result can be invalid
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws IllegalArgumentException
     *             if an overflow occurs during division
     * @throws ArithmeticException
     *             if any of the Packed Decimal operands are invalid
     */
    public static void dividePackedDecimal(byte[] result, int resultOffset,
            int resultPrecision, byte[] op1Decimal, int op1Offset,
            int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision, boolean checkOverflow) {
        if ((resultOffset + ((resultPrecision / 2) + 1) > result.length) || (resultOffset < 0))
    		throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"dividePackedDecimal is trying to access result[" + resultOffset + "] to result[" + (resultOffset + (resultPrecision / 2)) + "]" +
        		" but valid indices are from 0 to " + (result.length - 1) + ".");
            
        if ((op1Offset < 0)    || (op1Offset    + ((op1Precision    / 2) + 1) > op1Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"dividePackedDecimal is trying to access op1Decimal[" + op1Offset + "] to op1Decimal[" + (op1Offset + (op1Precision / 2)) + "]" +
        		" but valid indices are from 0 to " + (op1Decimal.length - 1) + ".");
            
        if ((op2Offset < 0)    || (op2Offset    + ((op2Precision    / 2) + 1) > op2Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            	"dividePackedDecimal is trying to access op2Decimal[" + op2Offset + "] to op2Decimal[" + (op2Offset + (op2Precision / 2)) + "]" +
            	" but valid indices are from 0 to " + (op2Decimal.length - 1) + ".");
        
        dividePackedDecimal_(result, resultOffset, resultPrecision, op1Decimal, op1Offset, 
                op1Precision, op2Decimal, op2Offset, op2Precision, checkOverflow);
    }
    
    private static void dividePackedDecimal_(byte[] result, int resultOffset,
            int resultPrecision, byte[] op1Decimal, int op1Offset,
            int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision, boolean checkOverflow) {
        packedDecimalBinaryOp(DIVIDE, result, resultOffset, resultPrecision,
                op1Decimal, op1Offset, op1Precision, op2Decimal, op2Offset,
                op2Precision, checkOverflow);

    }

    /**
     * Calculates the remainder resulting from the division of two Packed Decimals in byte arrays. The sign of an input
     * Packed Decimal is assumed to to be positive unless the sign nibble contains one of the negative sign codes, in
     * which case the sign of the respective input Packed Decimal is interpreted as negative.
     * 
     * @param result
     *            byte array that will hold the remainder Packed Decimal
     * @param resultOffset
     *            offset into <code>result</code> where the remainder Packed Decimal is located
     * @param resultPrecision
     *            number of Packed Decimal digits for the remainder. Maximum valid precision is 253
     * @param op1Decimal
     *            byte array that will hold the dividend Packed Decimal
     * @param op1Offset
     *            offset into <code>op1Decimal</code> where the dividend is located
     * @param op1Precision
     *            number of Packed Decimal digits for the dividend. Maximum valid precision is 253
     * @param op2Decimal
     *            byte array that will hold the divisor
     * @param op2Offset
     *            offset into <code>op2Decimal</code> where the divisor is located
     * @param op2Precision
     *            number of Packed Decimal digits for the divisor. Maximum valid precision is 253
     * @param checkOverflow
     *            if set to true, Packed Decimals are validated before division and an
     *            <code>IllegalArgumentException</code> or <code>ArithmeticException</code> may be thrown. If not, the
     *            result can be invalid
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws IllegalArgumentException
     *             if an overflow occurs during division
     * @throws ArithmeticException
     *             if any of the Packed Decimal operands are invalid
     */
    public static void remainderPackedDecimal(byte[] result, int resultOffset,
            int resultPrecision, byte[] op1Decimal, int op1Offset,
            int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision, boolean checkOverflow) {
        if ((resultOffset + ((resultPrecision / 2) + 1) > result.length) || (resultOffset < 0))
    		throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"remainderPackedDecimal is trying to access result[" + resultOffset + "] to result[" + (resultOffset + (resultPrecision / 2)) + "]" +
        		" but valid indices are from 0 to " + (result.length - 1) + ".");
            
        if ((op1Offset < 0)    || (op1Offset    + ((op1Precision    / 2) + 1) > op1Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"remainderPackedDecimal is trying to access op1Decimal[" + op1Offset + "] to op1Decimal[" + (op1Offset + (op1Precision / 2)) + "]" +
        		" but valid indices are from 0 to " + (op1Decimal.length - 1) + ".");
            
        if ((op2Offset < 0)    || (op2Offset    + ((op2Precision    / 2) + 1) > op2Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            	"remainderPackedDecimal is trying to access op2Decimal[" + op2Offset + "] to op2Decimal[" + (op2Offset + (op2Precision / 2)) + "]" +
            	" but valid indices are from 0 to " + (op2Decimal.length - 1) + ".");
        
        remainderPackedDecimal_(result, resultOffset, resultPrecision, op1Decimal, op1Offset, 
                op1Precision, op2Decimal, op2Offset, op2Precision, checkOverflow);
    }
    private static void remainderPackedDecimal_(byte[] result, int resultOffset,
            int resultPrecision, byte[] op1Decimal, int op1Offset,
            int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision, boolean checkOverflow) {
        packedDecimalBinaryOp(REMAINDER, result, resultOffset, resultPrecision,
                op1Decimal, op1Offset, op1Precision, op2Decimal, op2Offset,
                op2Precision, checkOverflow);
    }

    private static BigInteger getBigInteger(byte[] pd, int offset, int length) {
        int end = offset + length - 1;
        StringBuilder sb = new StringBuilder();

        if ((pd[end] & CommonData.LOWER_NIBBLE_MASK) == 0x0B
                || (pd[end] & CommonData.LOWER_NIBBLE_MASK) == 0x0D)
            sb.append('-');

        for (int i = offset; i < end; i++) {
            sb.append((char) ('0' + (pd[i] >> 4 & CommonData.LOWER_NIBBLE_MASK)));
            sb.append((char) ('0' + (pd[i] & CommonData.LOWER_NIBBLE_MASK)));
        }
        sb.append((char) ('0' + (pd[end] >> 4 & CommonData.LOWER_NIBBLE_MASK)));
        return new BigInteger(sb.toString());
    }

    private static void putBigInteger(byte[] pd, int offset, int length,
            BigInteger bigInt, boolean checkOverflow)
            throws ArithmeticException {
        int end = offset + length - 1;

        // could use abs(), but want to avoid creating another BigInteger object
        char[] chars = bigInt.toString().toCharArray();
        boolean neg = chars[0] == '-';

        int charStart = neg ? 1 : 0;
        int charEnd = chars.length - 1;

        pd[end--] = (byte) ((neg ? 0x0D : 0x0C) | (chars[charEnd--] - '0' << 4));

        while (end >= offset) {
            byte b = 0;
            if (charEnd >= charStart) {
                b = (byte) (chars[charEnd--] - '0');
                if (charEnd >= charStart) {
                    b |= chars[charEnd--] - '0' << 4;
                }
            }
            pd[end--] = b;
        }

        if (checkOverflow && charEnd < charStart) {
            while (charEnd >= charStart) {
                if (chars[charEnd--] != '0') {
                    throw new ArithmeticException(
                            "Packed Decimal overflow during multiplication/division, non-zero digits lost");
                }
            }
        }
    }

    private static void zeroTopNibbleIfEven(byte[] bytes, int offset, int prec) {
        if (prec % 2 == 0)
            bytes[offset] &= CommonData.LOWER_NIBBLE_MASK;
    }

    private static void packedDecimalBinaryOp(int op, byte[] result,
            int offsetResult, int precResult, byte[] op1, int op1Offset,
            int precOp1, byte[] op2, int op2Offset, int precOp2,
            boolean checkOverflow) {

        zeroTopNibbleIfEven(op1, op1Offset, precOp1);
        zeroTopNibbleIfEven(op2, op2Offset, precOp2);

        // check for long
        switch (op) {
        case MULTIPLY:
            if (precOp1 + precOp2 <= 18 && precResult <= 18) {
                long op1long = DecimalData.convertPackedDecimalToLong(op1,
                        op1Offset, precOp1, checkOverflow);
                long op2long = DecimalData.convertPackedDecimalToLong(op2,
                        op2Offset, precOp2, checkOverflow);
                long resultLong = op1long * op2long;
                DecimalData.convertLongToPackedDecimal(resultLong, result,
                        offsetResult, precResult, checkOverflow);
                if (resultLong == 0)
                    forceSign(result, offsetResult, precResult, op1, op1Offset,
                            precOp1, op2, op2Offset, precOp2);
                return;
            }
            break;
        case DIVIDE:
        case REMAINDER:
            if (precOp1 <= 18 && precOp2 <= 18 && precResult <= 18) {
                long op1long = DecimalData.convertPackedDecimalToLong(op1,
                        op1Offset, precOp1, checkOverflow);
                long op2long = DecimalData.convertPackedDecimalToLong(op2,
                        op2Offset, precOp2, checkOverflow);
                long resultLong;
                if (op == DIVIDE)
                    resultLong = op1long / op2long;
                else
                    resultLong = op1long % op2long;
                DecimalData.convertLongToPackedDecimal(resultLong, result,
                        offsetResult, precResult, checkOverflow);
                if (resultLong == 0)
                    forceSign(result, offsetResult, precResult, op1, op1Offset,
                            precOp1, op2, op2Offset, precOp2);
                return;
            }
            break;
        }

        // long is too small
        BigInteger op1BigInt;
        BigInteger op2BigInt;
        try {
            op1BigInt = getBigInteger(op1, op1Offset,
                    precisionToByteLength(precOp1));
            op2BigInt = getBigInteger(op2, op2Offset,
                    precisionToByteLength(precOp2));
        } catch (NumberFormatException e) {
            if (checkOverflow)
                throw new IllegalArgumentException("Invalid packed data value",
                        e);
            return;
        }

        BigInteger resultBigInt = null;
        switch (op) {
        case MULTIPLY:
            resultBigInt = op1BigInt.multiply(op2BigInt);
            break;
        case DIVIDE:
            resultBigInt = op1BigInt.divide(op2BigInt);
            break;
        case REMAINDER:
            resultBigInt = op1BigInt.remainder(op2BigInt);
            break;
        }

        putBigInteger(result, offsetResult, precisionToByteLength(precResult),
                resultBigInt, checkOverflow);

        // force the sign because BigInteger will never produce negative zero
        if (BigInteger.ZERO.equals(resultBigInt)) {
            forceSign(result, offsetResult, precResult, op1, op1Offset,
                    precOp1, op2, op2Offset, precOp2);
        }
    }

    /**
     * Using BigInteger or long will never produce negative zero, so we need to
     * make sure to set the correct sign code.
     */
    private static void forceSign(byte[] result, int offsetResult,
            int precResult, byte[] op1, int op1Offset, int precOp1, byte[] op2,
            int op2Offset, int precOp2) {

        int endOp1 = op1Offset + precisionToByteLength(precOp1) - 1;
        int endOp2 = op2Offset + precisionToByteLength(precOp2) - 1;
        int endResult = offsetResult + precisionToByteLength(precResult) - 1;
        result[endResult] |= 0x0F;
        result[endResult] &= signMask(op1[endOp1], op2[endOp2]);
    }

    private static int precisionToByteLength(int precision) {
        return (precision + 2) / 2;
    }

    private static byte signMask(byte a, byte b) {
        return (byte) (sign(a) * sign(b) > 0 ? 0xFC : 0xFD);
    }

    private static int sign(byte b) {
        return (CommonData.getSign(b & CommonData.LOWER_NIBBLE_MASK) == CommonData.PACKED_MINUS) ? -1 : 1;
    }

    /**
     * Checks if the first Packed Decimal operand is lesser than the second one.
     * 
     * @param op1Decimal
     *            byte array that holds the first Packed Decimal operand
     * @param op1Offset
     *            offset into <code>op1Decimal</code> where the first operand is located
     * @param op1Precision
     *            number of Packed Decimal digits for the first operand
     * @param op2Decimal
     *            byte array that holds the second Packed Decimal operand
     * @param op2Offset
     *            offset into <code>op2Decimal</code> where the second operand is located
     * @param op2Precision
     *            number of Packed Decimal digits for the second operand
     * 
     * @return true if <code>op1Decimal &lt; op2Decimal</code>, false otherwise
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static boolean lessThanPackedDecimal(byte[] op1Decimal,
            int op1Offset, int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision) {
        if ((op1Offset    + ((op1Precision    / 2) + 1) > op1Decimal.length) || (op1Offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"lessThanPackedDecimal is trying to access op1Decimal[" + op1Offset + "] to op1Decimal[" + (op1Offset + (op1Precision / 2)) + "]" +
        		" but valid indices are from 0 to " + (op1Decimal.length - 1) + ".");
            
        if ((op2Offset < 0)    || (op2Offset    + ((op2Precision    / 2) + 1) > op2Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            	"lessThanPackedDecimal is trying to access op2Decimal[" + op2Offset + "] to op2Decimal[" + (op2Offset + (op2Precision / 2)) + "]" +
            	" but valid indices are from 0 to " + (op2Decimal.length - 1) + ".");
        
        return lessThanPackedDecimal_(op1Decimal, op1Offset, op1Precision, 
                                     op2Decimal, op2Offset, op2Precision);
    }
    
    private static boolean lessThanPackedDecimal_(byte[] op1Decimal,
            int op1Offset, int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision) {
    	
    	return greaterThanPackedDecimal_(op2Decimal, op2Offset, op2Precision, op1Decimal, 
    			op1Offset, op1Precision);
    }

    /**
     * Checks if the first Packed Decimal operand is less than or equal to the second one.
     * 
     * @param op1Decimal
     *            byte array that holds the first Packed Decimal operand
     * @param op1Offset
     *            offset into <code>op1Decimal</code> where the first operand is located
     * @param op1Precision
     *            number of Packed Decimal digits for the first operand
     * @param op2Decimal
     *            byte array that holds the second Packed Decimal operand
     * @param op2Offset
     *            offset into <code>op2Decimal</code> where the second operand is located
     * @param op2Precision
     *            number of Packed Decimal digits for the second operand
     * 
     * @return true if <code>op1Decimal &lt;= op2Decimal</code>, false otherwise
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static boolean lessThanOrEqualsPackedDecimal(byte[] op1Decimal,
            int op1Offset, int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision) {
        if ((op1Offset    + ((op1Precision    / 2) + 1) > op1Decimal.length) || (op1Offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"lessThanOrEqualsPackedDecimal is trying to access op1Decimal[" + op1Offset + "] to op1Decimal[" + (op1Offset + (op1Precision / 2)) + "]" +
        		" but valid indices are from 0 to " + (op1Decimal.length - 1) + ".");
            
        if ((op2Offset < 0)    || (op2Offset    + ((op2Precision    / 2) + 1) > op2Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            	"lessThanOrEqualsPackedDecimal is trying to access op2Decimal[" + op2Offset + "] to op2Decimal[" + (op2Offset + (op2Precision / 2)) + "]" +
            	" but valid indices are from 0 to " + (op2Decimal.length - 1) + ".");
        
        return lessThanOrEqualsPackedDecimal_(op1Decimal, op1Offset, op1Precision, 
                                     op2Decimal, op2Offset, op2Precision);
        
    }
    private static boolean lessThanOrEqualsPackedDecimal_(byte[] op1Decimal,
            int op1Offset, int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision) {
        return !greaterThanPackedDecimal_(op1Decimal, op1Offset, op1Precision,
                op2Decimal, op2Offset, op2Precision);
    }

    /**
     * Checks if the first Packed Decimal operand is greater than the second one.
     * 
     * @param op1Decimal
     *            byte array that holds the first Packed Decimal operand
     * @param op1Offset
     *            offset into <code>op1Decimal</code> where the first operand is located
     * @param op1Precision
     *            number of Packed Decimal digits for the first operand
     * @param op2Decimal
     *            byte array that holds the second Packed Decimal operand
     * @param op2Offset
     *            offset into <code>op2Decimal</code> where the second operand is located
     * @param op2Precision
     *            number of Packed Decimal digits for the second operand
     * 
     * @return true if <code>op1Decimal &gt; op2Decimal</code>, false otherwise
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static boolean greaterThanPackedDecimal(byte[] op1Decimal,
            int op1Offset, int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision) {
        if ((op1Offset    + ((op1Precision    / 2) + 1) > op1Decimal.length) || (op1Offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"greaterThanPackedDecimal is trying to access op1Decimal[" + op1Offset + "] to op1Decimal[" + (op1Offset + (op1Precision / 2)) + "]" +
        		" but valid indices are from 0 to " + (op1Decimal.length - 1) + ".");
            
        if ((op2Offset < 0)    || (op2Offset    + ((op2Precision    / 2) + 1) > op2Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            	"greaterThanPackedDecimal is trying to access op2Decimal[" + op2Offset + "] to op2Decimal[" + (op2Offset + (op2Precision / 2)) + "]" +
            	" but valid indices are from 0 to " + (op2Decimal.length - 1) + ".");
        
        return greaterThanPackedDecimal_(op1Decimal, op1Offset, op1Precision, 
                op2Decimal, op2Offset, op2Precision);
    }
    
    private static boolean greaterThanPackedDecimal_(byte[] op1Decimal,
            int op1Offset, int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision) {
        
    	if (op1Precision < 1 || op2Precision < 1)
    		throw new IllegalArgumentException("Invalid Precision for an operand");
    	
    	//boolean result = false;
        int op1End = op1Offset + precisionToByteLength(op1Precision) - 1;
        int op2End = op2Offset + precisionToByteLength(op2Precision) - 1;
        
        //check signs, if a different signs we're done!
        byte op1Sign = CommonData.getSign((byte)(op1Decimal[op1End] & CommonData.LOWER_NIBBLE_MASK));
        byte op2Sign = CommonData.getSign((byte)(op2Decimal[op2End] & CommonData.LOWER_NIBBLE_MASK));
        
        boolean isNegative = (op1Sign == CommonData.PACKED_MINUS) ? true : false;
        
        //skip leading zeros
        for (; op1Offset < op1End && op1Decimal[op1Offset] == CommonData.PACKED_ZERO; op1Offset++)
        	;
        for (; op2Offset < op2End && op2Decimal[op2Offset] == CommonData.PACKED_ZERO; op2Offset++)
        	;
        
        int op1Length = op1End - op1Offset;
        int op2Length = op2End - op2Offset;
        
        if (op1Length + op2Length == 0)
        {
        	int op1LastDigit = (op1Decimal[op1Offset] >> 4) & CommonData.LOWER_NIBBLE_MASK;
            int op2LastDigit = (op2Decimal[op2Offset] >> 4) & CommonData.LOWER_NIBBLE_MASK;
            if (op1LastDigit + op2LastDigit == 0) //both zeros
            	return false;
            if (op1Sign < op2Sign)
            	return true;
            else if (op1Sign > op2Sign)
            	return false;
            if ((op1LastDigit > op2LastDigit && !isNegative) || (op1LastDigit < op2LastDigit && isNegative))
            	return true;
            
            return false;
        }
        
        if (op1Sign < op2Sign)
        	return true;
        else if (op1Sign > op2Sign)
        	return false;
        
        if ((op1Length > op2Length && !isNegative) || (op1Length < op2Length && isNegative))
        	return true;
        else if ((op1Length < op2Length && !isNegative) || (op1Length > op2Length && isNegative))
        	return false;
        
        while(op1Offset < op1End && op2Offset < op2End)
        {
        	int op1Byte = (op1Decimal[op1Offset] & 0xFF);
        	int op2Byte = (op2Decimal[op2Offset] & 0xFF);
        	int opDiff = op1Byte - op2Byte;
        	
        	if ((opDiff < 0 && !isNegative) || (opDiff > 0 && isNegative))
        		return false;
        	else if ((opDiff > 0 && !isNegative) || (opDiff < 0 && isNegative))
        		return true;
        	
        	op1Offset++;
        	op2Offset++;
        }
        //last digit
        int op1LastDigit = (op1Decimal[op1Offset] >> 4) & CommonData.LOWER_NIBBLE_MASK;
        int op2LastDigit = (op2Decimal[op2Offset] >> 4) & CommonData.LOWER_NIBBLE_MASK;
        
        if ((op1LastDigit > op2LastDigit && !isNegative) || (op1LastDigit < op2LastDigit && isNegative))
        	return true;
        
        return false; //either equal or less than
        
    }

    /**
     * Checks if the first Packed Decimal operand is greater than or equal to the second one.
     * 
     * @param op1Decimal
     *            byte array that holds the first Packed Decimal operand
     * @param op1Offset
     *            offset into <code>op1Decimal</code> where the first operand is located
     * @param op1Precision
     *            number of Packed Decimal digits for the first operand
     * @param op2Decimal
     *            byte array that holds the second Packed Decimal operand
     * @param op2Offset
     *            offset into <code>op2Decimal</code> where the second operand is located
     * @param op2Precision
     *            number of Packed Decimal digits for the second operand
     * 
     * @return true if <code>op1Decimal &gt;= op2Decimal</code>, false otherwise
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static boolean greaterThanOrEqualsPackedDecimal(byte[] op1Decimal,
            int op1Offset, int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision) {
        if ((op1Offset    + ((op1Precision    / 2) + 1) > op1Decimal.length) || (op1Offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"greaterThanOrEqualsPackedDecimal is trying to access op1Decimal[" + op1Offset + "] to op1Decimal[" + (op1Offset + (op1Precision / 2)) + "]" +
        		" but valid indices are from 0 to " + (op1Decimal.length - 1) + ".");
            
        if ((op2Offset < 0)    || (op2Offset    + ((op2Precision    / 2) + 1) > op2Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            	"greaterThanOrEqualsPackedDecimal is trying to access op2Decimal[" + op2Offset + "] to op2Decimal[" + (op2Offset + (op2Precision / 2)) + "]" +
            	" but valid indices are from 0 to " + (op2Decimal.length - 1) + ".");
        
        return greaterThanOrEqualsPackedDecimal_(op1Decimal, op1Offset, op1Precision, 
                op2Decimal, op2Offset, op2Precision);
    }
    
    private static boolean greaterThanOrEqualsPackedDecimal_(byte[] op1Decimal,
            int op1Offset, int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision) {
        return !lessThanPackedDecimal_(op1Decimal, op1Offset, op1Precision,
                op2Decimal, op2Offset, op2Precision);
    }

    /**
     * Checks if the two Packed Decimal operands are equal.
     * 
     * @param op1Decimal
     *            byte array that holds the first Packed Decimal operand
     * @param op1Offset
     *            offset into <code>op1Decimal</code> where the first operand is located
     * @param op1Precision
     *            number of Packed Decimal digits for the first operand
     * @param op2Decimal
     *            byte array that holds the second Packed Decimal operand
     * @param op2Offset
     *            offset into <code>op2Decimal</code> where the second operand is located
     * @param op2Precision
     *            number of Packed Decimal digits for the second operand
     * 
     * @return true if op1Decimal == op2Decimal, false otherwise
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static boolean equalsPackedDecimal(byte[] op1Decimal, int op1Offset,
            int op1Precision, byte[] op2Decimal, int op2Offset, int op2Precision) {
        if ((op1Offset    + ((op1Precision    / 2) + 1) > op1Decimal.length) || (op1Offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"equalsPackedDecimal is trying to access op1Decimal[" + op1Offset + "] to op1Decimal[" + (op1Offset + (op1Precision / 2)) + "]" +
        		" but valid indices are from 0 to " + (op1Decimal.length - 1) + ".");
            
        if ((op2Offset < 0)    || (op2Offset    + ((op2Precision    / 2) + 1) > op2Decimal.length))
            throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
                "equalsPackedDecimal is trying to access op2Decimal[" + op2Offset + "] to op2Decimal[" + (op2Offset + (op2Precision / 2)) + "]" +
                " but valid indices are from 0 to " + (op2Decimal.length - 1) + ".");
        
        return equalsPackedDecimal_(op1Decimal, op1Offset, op1Precision, 
                                    op2Decimal, op2Offset, op2Precision);
    }
    
    private static boolean equalsPackedDecimal_(byte[] op1Decimal, int op1Offset, int op1Precision, 
                                                byte[] op2Decimal, int op2Offset, int op2Precision) {
        if (op1Precision < 1 || op2Precision < 1)
            throw new IllegalArgumentException("Invalid Precision for an operand");
        
        int op1End = op1Offset + precisionToByteLength(op1Precision) - 1;
        int op2End = op2Offset + precisionToByteLength(op2Precision) - 1;
        
        byte op1Sign = CommonData.getSign((byte) (op1Decimal[op1End] & CommonData.LOWER_NIBBLE_MASK));
        byte op2Sign = CommonData.getSign((byte) (op2Decimal[op2End] & CommonData.LOWER_NIBBLE_MASK));
        
        boolean op1IsEven = op1Precision % 2 == 0;
        boolean op2IsEven = op2Precision % 2 == 0;
        
        // Must handle the +0 and -0 case
        if (op1Sign != op2Sign) 
            return checkZeroBetweenOpOffsetAndOpEnd (op1Decimal, op1Offset, op1End, op1IsEven, true) &&
                   checkZeroBetweenOpOffsetAndOpEnd (op2Decimal, op2Offset, op2End, op2IsEven, true);
        
        // Check if least significant digit is different
        if ((op1Decimal[op1End] & CommonData.HIGHER_NIBBLE_MASK) != 
            (op2Decimal[op2End] & CommonData.HIGHER_NIBBLE_MASK)) {
            return false;
        }
        
        // Avoid decrementing if op1End == op1Offset || op2End == op2Offset
        if (op1End > op1Offset && op2End > op2Offset) {
            op1End--;
            op2End--;
        }
        
        while (op1End > op1Offset && op2End > op2Offset) {
            // Check if intermediate digits are different
            if (op1Decimal[op1End] != op2Decimal[op2End])
                return false;
            
            op1End--;
            op2End--;
        }
        
        // At this point it is true that op1End == op1Offset || op2End == op2Offset
        if (op1End == op1Offset) {
            if (op1IsEven) {
                // Check if most significant digit is different
                if ((op1Decimal[op1End] & CommonData.LOWER_NIBBLE_MASK) == 
                    (op2Decimal[op2End] & CommonData.LOWER_NIBBLE_MASK)) {
                    return checkZeroBetweenOpOffsetAndOpEnd (op2Decimal, op2Offset, op2End, op2IsEven, !op2IsEven || op2End != op2Offset);
                }
            } else {
                // Check if two most significant digits are different
                if (op1Decimal[op1End] == op2Decimal[op2End]) 
                    return checkZeroBetweenOpOffsetAndOpEnd (op2Decimal, op2Offset, op2End, op2IsEven, false);
            }
        } else {
            if (op2IsEven) {
                // Check if most significant digit is different
                if ((op1Decimal[op1End] & CommonData.LOWER_NIBBLE_MASK) == 
                    (op2Decimal[op2End] & CommonData.LOWER_NIBBLE_MASK)) {
                    return checkZeroBetweenOpOffsetAndOpEnd (op1Decimal, op1Offset, op1End, op1IsEven, !op1IsEven || op1End != op1Offset);
                }
            } else {
                // Check if two most significant digits are different
                if (op1Decimal[op1End] == op2Decimal[op2End]) 
                    return checkZeroBetweenOpOffsetAndOpEnd (op1Decimal, op1Offset, op1End, op1IsEven, false);
            }
        }
        
        return false;
    }
    
    /**
     * Checks if the Packed Decimal digits between <code>opDecimal[opOffset]</code> and <code>opDecimal[opEnd]</code>
     * (inclusive depending on checkHighNibbleAtOpEnd) are all zeros.
     * 
     * @param opDecimal
     *            byte array that holds the Packed Decimal operand
     * @param opOffset
     *            offset into <code>opDecimal</code> where the first byte of the Packed Decimal is located
     * @param opEnd
     *            offset into <code>opDecimal</code> where the last byte to be checked is located
     * @param opIsEven
     *            boolean is true if the <code>opDecimal</code> has even precision
     * @param checkHighNibbleAtOpEnd
     *            boolean indicates whether the high nibble of <code>opDecimal[opEnd]</code> should be checked that it
     *            contains a zero. If false, opEnd will be decremented and this will be the new starting point of the
     *            algorithm which verifies that the remaining Packed Decimal digits are zeros.
     * 
     * @return true if all digits from opOffset to opEnd are zero, false otherwise
     * 
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    private static boolean checkZeroBetweenOpOffsetAndOpEnd(byte[] opDecimal, int opOffset, int opEnd, boolean opIsEven, boolean checkHighNibbleAtOpEnd) {
        if (checkHighNibbleAtOpEnd && (opDecimal[opEnd] & CommonData.HIGHER_NIBBLE_MASK) != 0x00)
            return false;
        
        if (opEnd-- > opOffset) {
            // Handle intermediate bytes
            while (opEnd > opOffset) {
                if (opDecimal[opEnd--] != 0x00)
                    return false;
            }
            
            // Handle the most significant nibble/byte
            if (opIsEven) {
                if ((opDecimal[opEnd] & CommonData.LOWER_NIBBLE_MASK) != 0x00)
                    return false;
            } else {
                if (opDecimal[opEnd] != 0x00)
                    return false;
            }
        }
        
        return true;
    }
    
    
    /**
     * Checks if the two Packed Decimal operands are unequal.
     * 
     * @param op1Decimal
     *            byte array that holds the first Packed Decimal operand
     * @param op1Offset
     *            offset into <code>op1Decimal</code> where the first operand is located
     * @param op1Precision
     *            number of Packed Decimal digits for the first operand
     * @param op2Decimal
     *            byte array that holds the second Packed Decimal operand
     * @param op2Offset
     *            offset into <code>op2Decimal</code> where the second operand is located
     * @param op2Precision
     *            number of Packed Decimal digits for the second operand
     * 
     * @return true if op1Decimal != op2Decimal, false otherwise
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     */
    public static boolean notEqualsPackedDecimal(byte[] op1Decimal,
            int op1Offset, int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision) {
        if ((op1Offset    + ((op1Precision    / 2) + 1) > op1Decimal.length) || (op1Offset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"notEqualsPackedDecimal is trying to access op1Decimal[" + op1Offset + "] to op1Decimal[" + (op1Offset + (op1Precision / 2)) + "]" +
        		" but valid indices are from 0 to " + (op1Decimal.length - 1) + ".");
            
        if ((op2Offset < 0)    || (op2Offset    + ((op2Precision    / 2) + 1) > op2Decimal.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            	"notEqualsPackedDecimal is trying to access op2Decimal[" + op2Offset + "] to op2Decimal[" + (op2Offset + (op2Precision / 2)) + "]" +
            	" but valid indices are from 0 to " + (op2Decimal.length - 1) + ".");
        
        return notEqualsPackedDecimal_(op1Decimal, op1Offset, op1Precision, 
                op2Decimal, op2Offset, op2Precision);
    }
    
    private static boolean notEqualsPackedDecimal_(byte[] op1Decimal,
            int op1Offset, int op1Precision, byte[] op2Decimal, int op2Offset,
            int op2Precision) {
        return !equalsPackedDecimal(op1Decimal, op1Offset, op1Precision,
                op2Decimal, op2Offset, op2Precision);
    }

    private static void roundUpPackedDecimal(byte[] packedDecimal, int offset,
            int end, int precision, int roundingDigit, boolean checkOverflow) {
    	
    	packedDecimal[end] = CommonData.getPackedAddOneSignValues(packedDecimal[end]);
        
    	if ((byte) (packedDecimal[end] & CommonData.HIGHER_NIBBLE_MASK) == (byte) 0x00) 
        {        
        	byte[] addTenArray = { 0x01, packedDecimal[end] };
            addPackedDecimal(packedDecimal, offset, precision,
                    packedDecimal, offset, precision, addTenArray, 0, 2,
                    checkOverflow);
        }
    }
    
    /**
     * In case of a shift operation, this method is called to ensure if the resulting value of the shift is
     * zero that it is converted to a positive zero, meaning {0x....0C}. This is done to match the expected
     * result in COBOL.
     */
    private static void checkIfZero(byte[] packedDecimal, int offset, int end, int precision, boolean isEvenPrecision) {
    	
    	if (CommonData.getSign(packedDecimal[end] & CommonData.LOWER_NIBBLE_MASK) != CommonData.PACKED_MINUS)
    		return;
    	if (isEvenPrecision && ((packedDecimal[offset] & CommonData.LOWER_NIBBLE_MASK) != 0x00))
    		return;
    	//else if (packedDecimal[offset] != 0x00)
    	//	return;
    	int i = offset;// + 1;
    	for (; i < end && packedDecimal[i] == 0x00; i++) ;
    	if (i < end)
    		return;
    	else if ((packedDecimal[end] & CommonData.HIGHER_NIBBLE_MASK) == 0x00)
    		packedDecimal[end] = CommonData.PACKED_PLUS;

    	return;
    }

    /**
     * Right shift, and optionally round, a Packed Decimal. If the resultant is zero, 
     * it can either be a positive zero 0x0C or a negative zero 0x0D.
     *
     * @param destination
     *            byte array that holds the result of the right shift (and rounding)
     * @param destinationOffset
     *            offset into <code>destination</code> where the result Packed Decimal is located
     * @param destinationPrecision
     *            number of Packed Decimal digits in the destination
     * @param source
     *            byte array that holds the Packed Decimal operand to be right shifted
     * @param sourceOffset
     *            offset into <code>source</code> where the operand is located
     * @param sourcePrecision
     *            number of Packed Decimal digits in the operand
     * @param shiftAmount
     *            amount by which the source will be right shifted
     * @param round
     *            if set to true, causes rounding to occur
     * @param checkOverflow
     *            if set to true, check for overflow
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if a decimal overflow occurs 
     * @throws IllegalArgumentException
     * 	           if the <code>shiftAmount</code> or <code>sourcePrecision</code> parameter is invalid or 
     *             the <code>source</code> packed decimal contains invalid sign or digit code
     */
    public static void shiftRightPackedDecimal(byte[] destination,
            int destinationOffset, int destinationPrecision, byte[] source,
            int sourceOffset, int sourcePrecision, int shiftAmount,
            boolean round, boolean checkOverflow) {
        
        if ((destinationOffset    + ((destinationPrecision    / 2) + 1) > destination.length) || (destinationOffset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"shiftRightPackedDecimal is trying to access destinationDecimal[" + destinationOffset + "] to destinationDecimal[" + (destinationOffset + (destinationPrecision / 2)) + "]" +
        		" but valid indices are from 0 to " + (destination.length - 1) + ".");
            
        if ((sourceOffset < 0)    || (sourceOffset    + ((sourcePrecision    / 2) + 1) > source.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            	"shiftRightPackedDecimal is trying to access sourceDecimal[" + sourceOffset + "] to sourceDecimal[" + (sourceOffset + (sourcePrecision / 2)) + "]" +
            	" but valid indices are from 0 to " + (source.length - 1) + ".");
        
        shiftRightPackedDecimal_(destination, destinationOffset, destinationPrecision, 
                                source, sourceOffset, sourcePrecision, shiftAmount, 
                                round, checkOverflow);
    }
    
    private static void shiftRightPackedDecimal_(byte[] destination,
            int destinationOffset, int destinationPrecision, byte[] source,
            int sourceOffset, int sourcePrecision, int shiftAmount,
            boolean round, boolean checkOverflow) {

        // Figure out the sign of the source Packed Decimal
        int end = sourceOffset + precisionToByteLength(sourcePrecision) - 1;
        byte sign = CommonData.getSign(source[end] & CommonData.LOWER_NIBBLE_MASK);
        int newDstOffset = destinationOffset;
        int newDstPrec = destinationPrecision;
        int dstEnd = destinationOffset
                + precisionToByteLength(destinationPrecision) - 1;
        int highDigit = sourcePrecision - 1;
        int lowDigit = 0;
        int precDiff = sourcePrecision - destinationPrecision;
        ;
        int newSrcOffset;
        int newSrcEnd;
        boolean evenPrecision = sourcePrecision % 2 == 0;
        boolean isLeastSigDigitInHighNibble = false;
        boolean isMostSigDigitInLowNibble = false;
        int roundingDigit = 0;
        boolean overRanPD = false; // when the shifting left no digits to copy

        if (destinationPrecision < 1 || sourcePrecision < 1 || shiftAmount < 0) {
            throw new IllegalArgumentException(
                    "Invalid Precisions or shift amount");
        }
        
        if(checkPackedDecimal_(source, sourceOffset, sourcePrecision, true, false) != 0) {
        	throw new IllegalArgumentException(
                    "Invalid sign or digit code in input packed decimal");
        }
        
        lowDigit += shiftAmount;
        precDiff -= shiftAmount; // every time we shift, we lose precision
        if (precDiff > 0) // if we still need to lose some precision, take off
                          // higher digits
        {
            highDigit -= precDiff;
            if (checkOverflow) {
                int bytes;
                int iter = 0;
                if (evenPrecision) {
                    precDiff--;
                    iter++;
                    if ((byte) (source[sourceOffset] & CommonData.LOWER_NIBBLE_MASK) != 0x00)
                        throw new ArithmeticException(
                                "Decimal overflow in shiftRightPackedDecimal.");
                }
                bytes = (precDiff) / 2;
                precDiff -= bytes * 2;

                for (int i = 0; i < bytes; i++) {
                    if (source[i + sourceOffset + iter] != 0x00)
                        throw new ArithmeticException(
                                "Decimal overflow in shiftRightPackedDecimal.");
                }

                if (precDiff == 1
                        && (byte) (source[bytes + sourceOffset] & CommonData.HIGHER_NIBBLE_MASK) != 0x00)
                    throw new ArithmeticException(
                            "Decimal overflow in shiftRightPackedDecimal.");

            }
        }
        // add zeros if precDiff < 0, do this at end
        int newDifference = (highDigit + 1) - lowDigit; // might want to deal
                                                        // with diff = 1
        if (checkOverflow && newDifference < 1)
            overRanPD = true;
        int highDiff = sourcePrecision - (highDigit + 1);

        if (evenPrecision && newDifference % 2 == 0) // still even
        {
            if (highDiff % 2 == 0) // both low and high moved even
            {
                isMostSigDigitInLowNibble = true; // the highest digit is on a
                                                  // low nibble
                isLeastSigDigitInHighNibble = true; // the lowest digit is on a
                                                    // high nibble
                newSrcOffset = sourceOffset + (highDiff) / 2;
                newSrcEnd = end - (lowDigit) / 2;
            } else // high moved odd, low moved odd
            {
                newSrcOffset = sourceOffset + (highDiff + 1) / 2;
                newSrcEnd = end - (lowDigit + 1) / 2;
            }
        } else if (evenPrecision && newDifference % 2 != 0) {
            if (highDiff % 2 != 0) // high moved odd, low moved even
            {
                isLeastSigDigitInHighNibble = true;
                newSrcOffset = sourceOffset + (highDiff + 1) / 2;
                newSrcEnd = end - (lowDigit) / 2;
            } else // high moved even, low moved odd, perfect scenario
            {
                isMostSigDigitInLowNibble = true;
                newSrcOffset = sourceOffset + (highDiff) / 2;
                newSrcEnd = end - (lowDigit + 1) / 2;
            }
        } else if (!evenPrecision && newDifference % 2 == 0) {
            if (highDiff % 2 == 0) // high moved even, low moved odd
            {
                newSrcOffset = sourceOffset + (highDiff) / 2;
                newSrcEnd = end - (lowDigit + 1) / 2;
            } else // high moved odd, low moved even
            {
                isMostSigDigitInLowNibble = true; // get this high nibble
                                                  // individually
                isLeastSigDigitInHighNibble = true; // get this lower nibble
                                                    // individually
                newSrcOffset = sourceOffset + (highDiff) / 2;
                newSrcEnd = end - (lowDigit) / 2;
            }
        } else {
            if (highDiff % 2 == 0) // both moved even, perfect scenario
            {
                isLeastSigDigitInHighNibble = true;
                newSrcOffset = sourceOffset + (highDiff) / 2;
                newSrcEnd = end - (lowDigit) / 2;
            } else // both moved odd
            {
                isMostSigDigitInLowNibble = true;
                newSrcOffset = sourceOffset + (highDiff) / 2;
                newSrcEnd = end - (lowDigit + 1) / 2;
            }
        }

        // find rounding digit
        if (round && shiftAmount > 0 && newDifference > -1) {
            if (isLeastSigDigitInHighNibble)
            	roundingDigit = (source[newSrcEnd])
                        & CommonData.LOWER_NIBBLE_MASK;
            else
            	roundingDigit = (source[newSrcEnd + 1] >> 4)
                        & CommonData.LOWER_NIBBLE_MASK;
            ;
        }

        while (precDiff < 0) // deal with dst prec being higher, put leading
                             // zeros
        {
            destination[newDstOffset] = 0x00;
            if (newDstPrec % 2 == 0 || precDiff == -1) {
                if (newDstPrec % 2 == 0)
                    newDstOffset++;
                precDiff++;
                newDstPrec--;

            } else if ((precDiff / -2) >= 1) {
                precDiff += 2;
                newDstPrec -= 2;
                newDstOffset++;
            }
        }

        if (isLeastSigDigitInHighNibble && !overRanPD) // can do arrayCopy,
                                                       // instead of individual
                                                       // nibbles
        {
            if (!isMostSigDigitInLowNibble) // just a simple arrayCopy
                System.arraycopy(source, newSrcOffset, destination,
                        newDstOffset, newSrcEnd - newSrcOffset + 1);
            else {
                destination[newDstOffset] = (byte) ((source[newSrcOffset] & CommonData.LOWER_NIBBLE_MASK));
                newDstOffset++;
                newSrcOffset++;
                System.arraycopy(source, newSrcOffset, destination,
                        newDstOffset, newSrcEnd - newSrcOffset + 1);
            }
        } else if (!overRanPD) // must copy each nibble over
        {
            int newDstEnd = newDstOffset + precisionToByteLength(newDstPrec)
                    - 1;
            byte shiftByte = newSrcEnd < sourceOffset ? 0x00 : source[newSrcEnd];
            destination[newDstEnd] = (byte) ((shiftByte << 4) & CommonData.HIGHER_NIBBLE_MASK);
            newDstEnd--;
            newDstPrec--;
            byte current_nibble = 0;
            while ((newSrcEnd > newSrcOffset) && (newDstEnd > newDstOffset)) {
                current_nibble = (byte) ((source[newSrcEnd] >> 4) & CommonData.LOWER_NIBBLE_MASK);
                newSrcEnd--;
                current_nibble |= (byte) ((source[newSrcEnd] << 4) & CommonData.HIGHER_NIBBLE_MASK);
                destination[newDstEnd] = current_nibble;
                newDstEnd--;
                newDstPrec -= 2;
            }
            if (newDstPrec > 0) {
                destination[newDstEnd] = (byte) ((source[newSrcEnd] >> 4) & CommonData.LOWER_NIBBLE_MASK);
                newSrcEnd--;
                newDstPrec--;
                if (newDstPrec > 0 && isMostSigDigitInLowNibble)
                    destination[newDstEnd] |= (byte) ((source[newSrcEnd] << 4) & CommonData.HIGHER_NIBBLE_MASK);
            }
        }
        // sign assignment
        if(overRanPD && roundingDigit < 5 && sign != CommonData.PACKED_PLUS)
        	destination[dstEnd] = (byte) ((destination[dstEnd] & CommonData.HIGHER_NIBBLE_MASK) | sign);
        else
        {
        	destination[dstEnd] = (byte) ((destination[dstEnd] & CommonData.HIGHER_NIBBLE_MASK) | (sign));
        	if (round && roundingDigit >= 5)
        		roundUpPackedDecimal(destination, destinationOffset, dstEnd,
                    destinationPrecision, roundingDigit, checkOverflow);
        	if (checkOverflow || round)
        	checkIfZero(destination, destinationOffset, dstEnd, destinationPrecision, (destinationPrecision % 2 == 0 ? true : false));
        	
        }
    }

    /**
     * Left shift a Packed Decimal appending zeros to the right. In the absence of overflow, the sign
     * of a zero can either be positive 0x0C or negative 0x0D.
     * 
     * @param destination
     *            byte array that holds the result of the left shift
     * @param destinationOffset
     *            offset into <code>destination</code> where the result Packed Decimal is located
     * @param destinationPrecision
     *            number of Packed Decimal digits in the destination
     * @param source
     *            byte array that holds the Packed Decimal operand to be left shifted
     * @param sourceOffset
     *            offset into <code>source</code> where the operand is located
     * @param sourcePrecision
     *            number of Packed Decimal digits in the operand
     * @param shiftAmount
     *            amount by which the source will be left shifted
     * @param checkOverflow
     *            if set to true, check for overflow
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws ArithmeticException
     *             if a decimal overflow occurs
     * @throws IllegalArgumentException
     * 	           if the <code>shiftAmount</code> or <code>sourcePrecision</code> parameter is invalid or 
     *             the <code>source</code> packed decimal contains invalid sign or digit code
     */
    public static void shiftLeftPackedDecimal(byte[] destination,
            int destinationOffset, int destinationPrecision, byte[] source,
            int sourceOffset, int sourcePrecision, int shiftAmount,
            boolean checkOverflow) {
        if ((destinationOffset    + ((destinationPrecision    / 2) + 1) > destination.length) || (destinationOffset < 0))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
        		"shiftLeftPackedDecimal is trying to access destinationDecimal[" + destinationOffset + "] to destinationDecimal[" + (destinationOffset + (destinationPrecision / 2)) + "]" +
        		" but valid indices are from 0 to " + (destination.length - 1) + ".");
            
        if ((sourceOffset < 0)    || (sourceOffset    + ((sourcePrecision    / 2) + 1) > source.length))
        	throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
            	"shiftLeftPackedDecimal is trying to access sourceDecimal[" + sourceOffset + "] to sourceDecimal[" + (sourceOffset + (sourcePrecision / 2)) + "]" +
            	" but valid indices are from 0 to " + (source.length - 1) + ".");
        
        shiftLeftPackedDecimal_(destination, destinationOffset, destinationPrecision, 
                                source, sourceOffset, sourcePrecision, shiftAmount, 
                                checkOverflow);
    }
    
    private static void shiftLeftPackedDecimal_(byte[] destination,
            int destinationOffset, int destinationPrecision, byte[] source,
            int sourceOffset, int sourcePrecision, int shiftAmount,
            boolean checkOverflow) {

        if (destinationPrecision < 1 || sourcePrecision < 1 || shiftAmount < 0) {
            throw new IllegalArgumentException(
                    "Invalid Precisions or shift amount");
        }
        
        if(checkPackedDecimal_(source, sourceOffset, sourcePrecision, true, false) != 0) {
        	throw new IllegalArgumentException(
                    "Invalid sign or digit code in input packed decimal");
        }
        // Figure out the sign of the source Packed Decimal
        int end = sourceOffset + precisionToByteLength(sourcePrecision) - 1;
        byte sign = CommonData.getSign(source[end] & CommonData.LOWER_NIBBLE_MASK);
        boolean overRanPD = false;
        int sourceLength = precisionToByteLength(sourcePrecision);
        int destinationLength = precisionToByteLength(destinationPrecision);
        int shiftByte = shiftAmount / 2;
        int zerosBytesAtFront = 0;
        int totalZeros = 0;

        // odd Precision
        if (sourcePrecision % 2 != 0) {

            // Figure out how many 0x00s are at the front starting from the
            // offset
            for (zerosBytesAtFront = 0; zerosBytesAtFront < sourceLength; zerosBytesAtFront++) {
                if (source[zerosBytesAtFront + sourceOffset] != 0x00)
                    break;
            }

            // Figure out if 0x0*
            if ((byte) (source[zerosBytesAtFront + sourceOffset] & CommonData.HIGHER_NIBBLE_MASK) == 0x00)
                totalZeros++;

        }// even precision
        else {
            // Figure out if bottom nibble of offset is 0 if it is then count
            // how many zero bytes there are at the front
            if ((byte) (source[sourceOffset] & CommonData.LOWER_NIBBLE_MASK) == 0x00) {
                totalZeros++;
                for (int i = 1; i < sourceLength; i++) {
                    if (source[i + sourceOffset] != 0x00)
                        break;
                    else
                        zerosBytesAtFront++;
                }
                if ((byte) (source[zerosBytesAtFront + sourceOffset + 1] & CommonData.HIGHER_NIBBLE_MASK) == 0x00)
                    totalZeros++;
            }
        }
        // Add up all the zeros
        totalZeros += zerosBytesAtFront * 2;

        // Check for overflow
        if (checkOverflow
                && ((destinationPrecision + totalZeros < sourcePrecision
                        + shiftAmount)))
            throw new ArithmeticException(
                    "Overflow - Destination precision not enough to hold the result of the shift operation");

        // Zero out destination array in range
        Arrays.fill(destination, destinationOffset, destinationOffset
                + destinationLength, (byte) 0x00);

        int startIndexSource = zerosBytesAtFront + sourceOffset;
        int diff = destinationLength - sourceLength;
        int startIndexDestination = destinationOffset + zerosBytesAtFront
                - shiftByte + diff;
        int numberLength = sourceLength - zerosBytesAtFront; // non-zeros digits
                                                             // length in bytes
        int movedSignIndex = startIndexDestination + numberLength - 1; // where
                                                                       // the
                                                                       // moved
                                                                       // sign
                                                                       // is
                                                                       // displaced
                                                                       // to

     // if all digits are completely shifted out do nothing
        if (destinationPrecision <= shiftAmount)
        {
        	if (checkOverflow && sign != CommonData.PACKED_PLUS)
        		overRanPD = true;
        }
        else 
        {
	        // Even shift, can move byte by byte
	        if (shiftAmount % 2 == 0) 
	        {
	            if (startIndexDestination < destinationOffset) 
	            {
	                int difference = sourcePrecision - (destinationPrecision - shiftAmount);
	                int sourceIndex;
	                if (sourcePrecision % 2 == 0) 
	                   sourceIndex = sourceOffset + ((difference + 1) / 2);
	                else 
	                    sourceIndex = sourceOffset + (difference / 2);
	                
	                System.arraycopy(source, sourceIndex, destination, destinationOffset, destinationLength
	                                - (shiftAmount / 2));
	            } 
	            else 
	                System.arraycopy(source, startIndexSource, destination,
	                        startIndexDestination, sourceLength - zerosBytesAtFront);
	
	            // Strip off moved sign
	            if (movedSignIndex >= 0)
	                destination[movedSignIndex] = (byte) (destination[movedSignIndex] & CommonData.HIGHER_NIBBLE_MASK);
	        } 
	        else 
	        {
	            byte top;
	            byte bottom;
	
	            // Move each nibble by appropriate index
	            for (int i = 0; i < numberLength; i++) {
	                top = (byte) (source[startIndexSource + i] & CommonData.HIGHER_NIBBLE_MASK);
	                bottom = (byte) (source[startIndexSource + i] & CommonData.LOWER_NIBBLE_MASK);
	
	                if (startIndexDestination - 1 + i >= destinationOffset) {
	                    destination[startIndexDestination - 1 + i] = (byte) ((top >> 4 & CommonData.LOWER_NIBBLE_MASK) | (byte) (destination[startIndexDestination
	                            - 1 + i] & CommonData.HIGHER_NIBBLE_MASK));
	                }
	                if (startIndexDestination + i >= destinationOffset) {
	                    destination[startIndexDestination + i] = (byte) (bottom << 4);
	                }
	
	            }
	            // Strip off moved sign
	            if (movedSignIndex >= 0)
	                destination[movedSignIndex] = CommonData.PACKED_ZERO;
	        }
        }
        int destinationEnd = destinationOffset + destinationLength - 1;
        boolean isDestionationEvenPrecision = destinationPrecision % 2 == 0 ? true : false;
        // Mask top Nibble
        if (isDestionationEvenPrecision)
            destination[destinationOffset] = (byte) (destination[destinationOffset] & CommonData.LOWER_NIBBLE_MASK);
        // Put sign in proper position
        
        if (overRanPD)
        	destination[destinationEnd] = (byte) (CommonData.PACKED_PLUS | destination[destinationEnd]);
        else
        {
        	destination[destinationEnd] = (byte) (sign | destination[destinationEnd]);
        	//checkOverflow will generate pdshlOverflow, which will cause it to reset sign. pdshl will not
        	if (checkOverflow) 
        		checkIfZero(destination, destinationOffset, destinationEnd, destinationPrecision, isDestionationEvenPrecision);
        }
    }

    /**
     * Creates a copy of a Packed Decimal.
     * 
     * @param destination
     *            byte array representing the destination
     * @param destinationOffset
     *            offset into destination <code>destination</code> where the Packed Decimal is located
     * @param destinationPrecision
     *            number of Packed Decimal digits for the destination
     * @param source
     *            byte array which holds the source Packed Decimal
     * @param sourceOffset
     *            offset into <code>source</code> where the Packed Decimal is located
     * @param sourcePrecision
     *            number of Packed Decimal digits for the source. Maximum valid precision is 253
     * @param checkOverflow
     *            if set to true, moved Packed Decimals are validated, and an <code>IllegalArgumentException</code> or
     *            <code>ArithmeticException</code> is thrown if non-zero nibbles are truncated during the move
     *            operation. If set to false, no validating is conducted.
     * 
     * @throws NullPointerException
     *             if any of the byte arrays are null
     * @throws ArrayIndexOutOfBoundsException
     *             if an invalid array access occurs
     * @throws IllegalArgumentException
     *             if the source Packed Decimal is invalid
     * @throws ArithmeticException
     *             if a decimal overflow occurs
     */
    public static void movePackedDecimal(byte[] destination,
            int destinationOffset, int destinationPrecision, byte[] source,
            int sourceOffset, int sourcePrecision, boolean checkOverflow) {
        shiftLeftPackedDecimal(destination, destinationOffset, 
                destinationPrecision, source, sourceOffset, 
                        sourcePrecision, 0, checkOverflow);  

    }

    private static class PackedDecimalOperand {

    	PackedDecimalOperand() {
            super();
    	}

        private static final byte PACKED_ZERO = 0x00;

        byte[] byteArray;
        int offset;
        int precision;
        int bytes;
        int signOffset;
        int currentOffset;
        int signByteValue;
        int sign;
        int signDigit;
        int byteValue;
        int indexValue;

        /**
         * Sets up the attributes of a Packed Decimal operand. Truncates leading zeros. Captures the sign value.
         * 
         * @param byteArray
         *            byte array which holds the Packed Decimal
         * @param offset
         *            offset in <code>byteArray</code> where the Packed Decimal begins
         * @param precision
         *            number of Packed Decimal digits. Maximum valid precision is 253
         * 
         * @throws NullPointerException
         *             if <code>byteArray</code> is null
         * @throws ArrayIndexOutOfBounds
         *             if an invalid array access occurs
         */
        public void setOperand(byte[] byteArray, int offset, int precision) {

            this.byteArray = byteArray;
            this.offset = offset;
            this.precision = precision;

            // truncate leading zeros
            bytes = CommonData.getPackedByteCount(precision);
            signOffset = this.offset + bytes - 1;
            currentOffset = signOffset - 1;
            for (; byteArray[this.offset] == PACKED_ZERO
                    && this.offset < signOffset; this.offset++)
                ;
            bytes = signOffset - this.offset + 1;

            // capture sign values
            signByteValue = this.byteArray[signOffset]
                    & CommonData.INTEGER_MASK;
            signDigit = signByteValue & CommonData.HIGHER_NIBBLE_MASK;
            sign = CommonData.getSign(signByteValue & CommonData.LOWER_NIBBLE_MASK); 

        }

        /**
         * Sets up attributes of a Packed Decimal which is about to be the result operand holding the sum of two Packed
         * Decimal operands. Does not truncate leading zeroes. Does not capture the sign.
         * 
         * @param byteArray
         *            byte array which holds the Packed Decimal
         * @param offset
         *            offset in <code>byteArray</code> where the Packed Decimal begins
         * @param precision
         *            number of Packed Decimal digits. Maximum valid precision is 253
         * 
         * @throws NullPointerException
         *             if <code>byteArray</code> is null
         * @throws ArrayIndexOutOfBounds
         *             if an invalid array access occurs
         */
        public void setSumOperand(byte[] byteArray, int offset, int precision) {

            this.byteArray = byteArray;
            this.offset = offset;
            this.precision = precision;

            bytes = CommonData.getPackedByteCount(precision);
            signOffset = this.offset + bytes - 1;
            currentOffset = signOffset - 1;

        }

    }

}
