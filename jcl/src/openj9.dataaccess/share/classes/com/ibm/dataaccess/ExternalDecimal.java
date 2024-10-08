/*[INCLUDE-IF DAA]*/
/*
 * Copyright IBM Corp. and others 2023
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.dataaccess;

public class ExternalDecimal {
	/**
	 * Private constructor, class contains only static methods.
	 */
	private ExternalDecimal() {
		super();
	}

	private static byte EXTERNAL_DECIMAL_SPACE_SYMBOL = (byte)0x40;
	private static int EXTERNAL_DECIMAL_INVALID_DIGIT = 2;
	private static int EXTERNAL_DECIMAL_INVALID_SIGN = 1;

	/**
	 * Checks the validity of a External Decimal, returns an integer indicating the status of the External Decimal.
	 *
	 * @param byteArray
	 *            source array.
	 * @param offset
	 *            starting offset of the External Decimal.
	 * @param precision
	 *            number of digits to be verified.
	 * @param decimalType
	 *            constant indicating the type of the decimal. Supported values are
	 *            DecimalData.EBCDIC_SIGN_EMBEDDED_TRAILING, DecimalData.EBCDIC_SIGN_EMBEDDED_LEADING,
	 *            DecimalData.EBCDIC_SIGN_SEPARATE_TRAILING, DecimalData.EBCDIC_SIGN_SEPARATE_LEADING.
	 * @param bytesWithSpaces
	 *            represents number of left most bytes containing EBCDIC space character if sign is
	 *            embedded, ignored otherwise.
	 *
	 * @return the condition code:
	 *          0 if all digit codes and the sign are valid,
	 *          1 if the sign is invalid,
	 *          2 if at least one digit code is invalid,
	 *          3 if sign and at least one digit code is invalid.
	 *
	 * @throws NullPointerException
	 *             if <code>byteArray</code> is null.
	 * @throws ArrayIndexOutOfBoundsException
	 *             if an invalid array access occurs.
	 * @throws IllegalArgumentException
	 *             if the precision is less than 1.
	 * @throws IllegalArgumentException
	 *             if the offset is less than 0.
	 * @throws IllegalArgumentException
	 *             if <code>decimalType</code> is invalid.
	 * @throws IllegalArgumentException
	 *             if <code>bytesWithSpaces</code> is less than 0 when sign is embedded.
	 */
	public static int checkExternalDecimal(byte[] byteArray, int offset,
		int precision, int decimalType, int bytesWithSpaces) {
		if (precision < 1)
			throw new IllegalArgumentException("Precision must be greater than 0.");

		if (offset < 0)
			throw new IllegalArgumentException("Offset must be non-negative integer.");

		if (decimalType < DecimalData.EXTERNAL_DECIMAL_MIN || decimalType > DecimalData.EXTERNAL_DECIMAL_MAX)
			throw new IllegalArgumentException("Invalid decimalType.");

		final int lengthNeeded = offset + CommonData.getExternalByteCounts(precision, decimalType);
		if (lengthNeeded > byteArray.length)
			throw new ArrayIndexOutOfBoundsException("Array access index out of bounds. " +
				"checkExternalDecimal is trying to access byteArray[" + offset + "] to byteArray[" + (lengthNeeded - 1) + "]" +
				" but valid indices are from 0 to " + (byteArray.length - 1) + ".");

		return checkExternalDecimal_(byteArray, offset, precision, decimalType, bytesWithSpaces);
	}

	private static int checkExternalDecimal_(byte[] byteArray, int offset,
		int precision, int decimalType, int bytesWithSpaces) {

		// Digit is invalid if:
		// - number of non space digits is equal to 0 (precision == bytesWithSpaces)
		// - number of non space digits is greater than precision (bytesWithSpaces < 0)
		if (decimalType == DecimalData.EBCDIC_SIGN_EMBEDDED_TRAILING
			&& (precision == bytesWithSpaces || bytesWithSpaces < 0)) {
			return 3;
		}

		int startIndex = offset; // start of sign-digit/zone-digit bytes
		int endIndex = startIndex + CommonData.getExternalByteCounts(precision, decimalType) - 1;
		boolean isSignEmbedded = false;
		byte signByte;
		switch (decimalType) {
		case DecimalData.EBCDIC_SIGN_EMBEDDED_TRAILING:
			isSignEmbedded = true;
			signByte = byteArray[endIndex];
			--endIndex;
			startIndex += bytesWithSpaces;
			break;
		case DecimalData.EBCDIC_SIGN_SEPARATE_TRAILING:
			signByte = byteArray[endIndex];
			--endIndex;
			break;
		case DecimalData.EBCDIC_SIGN_EMBEDDED_LEADING:
			isSignEmbedded = true;
			signByte = byteArray[startIndex];
			++startIndex;
			break;
		case DecimalData.EBCDIC_SIGN_SEPARATE_LEADING:
			signByte = byteArray[startIndex];
			++startIndex;
			break;
		default:
			signByte = (byte)0x9F; // random invalid sign byte
			break;
		}

		int returnCode = 0;
		if (decimalType == DecimalData.EBCDIC_SIGN_EMBEDDED_TRAILING && bytesWithSpaces > 0) {
			// Validate bytes before digits
			for (int index = offset; index < startIndex; ++index) {
				// Only EXTERNAL_DECIMAL_SPACE_SYMBOL and F[0-9] are considered valid
				if (byteArray[index] != EXTERNAL_DECIMAL_SPACE_SYMBOL
					&& (((byteArray[index] & (byte)CommonData.HIGHER_NIBBLE_MASK) & 0xF0) != 0xF0 // zone nibble
						|| (byteArray[index] & (byte)CommonData.LOWER_NIBBLE_MASK) > 0x09)) { // digit nibble
					returnCode = EXTERNAL_DECIMAL_INVALID_DIGIT;
				}
			}
		}

		// Validate digits
		for (int index = startIndex; index <= endIndex && returnCode < EXTERNAL_DECIMAL_INVALID_DIGIT; ++index) {
			// Only valid digits are F[0-9]
			if (byteArray[index] == EXTERNAL_DECIMAL_SPACE_SYMBOL
				|| ((byteArray[index] & (byte)CommonData.HIGHER_NIBBLE_MASK) & 0xF0) != 0xF0 // zone nibble
				|| (byteArray[index] & (byte)CommonData.LOWER_NIBBLE_MASK) > 0x09) { // digit nibble
				returnCode = EXTERNAL_DECIMAL_INVALID_DIGIT;
			}
		}

		// Validate sign byte
		if (isSignEmbedded) {
			// Check digit nibble embedded in sign byte
			if ((signByte & (byte)CommonData.LOWER_NIBBLE_MASK) > 0x09) returnCode = EXTERNAL_DECIMAL_INVALID_DIGIT;

			// Check sign nibble for preferred and alternate representations
			if (((signByte & (byte)CommonData.HIGHER_NIBBLE_MASK) & 0xF0) < 0xA0) returnCode |= EXTERNAL_DECIMAL_INVALID_SIGN;
		}
		else if (!isSignEmbedded
			&& signByte != CommonData.EXTERNAL_SIGN_MINUS
			&& signByte != CommonData.EXTERNAL_SIGN_PLUS) {
			returnCode |= EXTERNAL_DECIMAL_INVALID_SIGN;
		}
		return returnCode;
	}
}
