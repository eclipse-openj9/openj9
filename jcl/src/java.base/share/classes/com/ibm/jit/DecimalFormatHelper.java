/*[INCLUDE-IF Sidecar17]*/
package com.ibm.jit;

import java.text.Format;
import java.text.NumberFormat;
import java.text.NumberFormat.Field;
import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.math.BigDecimal;
import java.lang.StringBuilder;
import java.lang.String;
import java.lang.RuntimeException;
/*******************************************************************************
 * Copyright (c) 2011, 2018 IBM Corp. and others
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

/**
 * In specjEnterprise, about 4% of the time is spent in executing nf.format(bd.doubleValue())
 * and nf.format(bd.floatValue()) in which nf is of type DecimalFormat and bd is of type
 * BigDecimal.
 * <p>
 * Normally, either of the patterns above are executed as follows
 * 1) convert BigDecimal to String 
 * 2) convert String to double (or float)  
 * 3) convert double (or float) to String (which calls dtoa)
 * 4) parse String to get its digits and exponent
 * 5) format digits and exponent to generate final result
 * <p>
 * To improve the performance of the patterns, we'd like to change the above algorithm to
 * 1) get the digits and exponent of the BigDecimal object, if LL (long lookaside) representation is used
 * 2) format digits and exponent to generate the final result
 * <p>
 * In order to make such a change, the jit detects the patterns above and converts them to
 * DecimalFormatHelper.formatAsDouble(nf, bd) or DecimalFormatHelper.foratAsFloat(nf, bd). 
 * Since we need to access some private fields/methods/statics of other objects
 * in this method, we have defined corresponding dummy fields/methods/statics in this class.
 * During jitting, we replace all these dummy accesses with accesses to the real field/method/static.
 **/

public class DecimalFormatHelper {

    //dummy class for interface java.text.Format$FieldDelegate so that the following code compiles 
    final class FieldDelegate {
    }
  
    //dummy class for interface java.text.FieldPosition
    final class FieldPosition {
    	void setBeginIndex(int i) {throw new RuntimeException("JIT Helper expected");} //$NON-NLS-1$
    	void setEndIndex(int i) {throw new RuntimeException("JIT Helper expected");} //$NON-NLS-1$
    	FieldDelegate getFieldDelegate() {throw new RuntimeException("JIT Helper expected");} //$NON-NLS-1$
    	Format.Field getFieldAttribute() {throw new RuntimeException("JIT Helper expected");} //$NON-NLS-1$
    }
    
    //dummy class for java.text.DigitList
    private final class DigitList {
    	int decimalAt;
    	int count;
    	char[] digits;
    	boolean isZero() {throw new RuntimeException("JIT Helper expected");} //$NON-NLS-1$
    	void set(boolean isNegative, long source) {throw new RuntimeException("JIT Helper expected");} //$NON-NLS-1$
    }

    private static DigitList digitList; //a field of DecimalFormat
    private static int multiplier; // a field of DecimalFormat
	private static DecimalFormatSymbols symbols; // a field of DecimalFormat
	private static boolean isCurrencyFormat; // a field of DecimalFormat
	private static String negativePrefix; // a field of DecimalFormat
	private static String negativeSuffix; // a field of DecimalFormat
	private static String positivePrefix; // a field of DecimalFormat
	private static String positiveSuffix; // a field of DecimalFormat
	private static byte groupingSize; // a field of DecimalFormat
	private static byte minExponentDigits; // a field of DecimalFormat
	private static boolean decimalSeparatorAlwaysShown; // a field of DecimalFormat
	private static boolean useExponentialNotation; // a field of DecimalFormat
    private static FieldPosition INSTANCE; //a static in java.text.DontCareFieldPosition
    private static char []doubleDigitsOnes; //a static of BigDecimal
    private static char []doubleDigitsTens; //a static of BigDecimal
    private static int flags; //a field of BigDecimal
    private static long laside; // a field of BigDecimal
    private static int scale; //a field of BigDecimal

    /**
     * The implementation of format needs to follow the implementation of
     * java.text.DecimalFormat.format methods in regards to setting
     * maxIntDigits, minIntDigits, etc.
     *
     * The only difference between formatAsDouble and formatAsFloat is what they
     * do when the representation of number is not LL (long lookaside). There are
     * two ways to avoid this duplication and both involve more work that duplicating
     * the code, i.e., the approach chosen here.
     * 1) passing a third parameter to DecimalFormatHelper.format from the jit. Since
     *    the jit cannot simply add one child to an existing call node, we need to 
     *    create a new node with one more child and then copy everything over from the original
     *    node. Then we need to fix up all the places referring to the old node, i.e., too much work
     * 
     * 2) we could have two format methods in DecimalFormatHelper but refactor their common
     *    code. The problem with this approach is that we need to make sure we inline
     *    the method that contains the common code. That's fairly complicated to achieve.
     * 
     *
     * @param df   the object on which format was called 
     * @param number   the object on which doubleValue or floatValue() is called
     * @return the formatted string
     **/
    private static String formatAsDouble(DecimalFormat df, BigDecimal number)  {
    	if ((flags & 0x00000001) != 0) {
    		//The optimization is possible if the representation of the BigDecimal object,
    		//number, is long lookaside (LL)
    		INSTANCE.setBeginIndex(0);
    		INSTANCE.setEndIndex(0);
    		if (multiplier != 1) 
    			number = number.multiply(getBigDecimalMultiplier(df));
    		boolean isNegative = number.signum() == -1;
    		synchronized(digitList) {
    			int maxIntDigits = df.getMaximumIntegerDigits();
    			int minIntDigits = df.getMinimumIntegerDigits();
    			int maxFraDigits = df.getMaximumFractionDigits();
    			int minFraDigits = df.getMinimumFractionDigits();

    			if (laside == 0) {
    				digitList.digits[0] = '0';
    				digitList.count = 1;
    				digitList.decimalAt = 0;
    			}
    			else {
    				long intVal = laside;
    				long intTmpVal;
    				long sign= ( laside >> 63) | ((-laside) >>> 63);
    				int length = number.precision();
    				if (sign == -1) 
    					intVal = -intVal;
    				int d = length - 1;
    				while (intVal != 0) {
    					intTmpVal = intVal;
    					intVal = intVal/100;
    					int rem = (byte)(intTmpVal - ((intVal << 6) + (intVal << 5) + (intVal << 2)));
    					digitList.digits[d--] = doubleDigitsOnes[rem];
    					if (d >= 0)
    						digitList.digits[d--] = doubleDigitsTens[rem];
    				}
    				digitList.count = length;
    				digitList.decimalAt = -scale + length;
    			}
    			/*
    			return subformat(df, new StringBuffer(), INSTANCE.getFieldDelegate(), isNegative, false,
    					maxIntDigits, minIntDigits, maxFraDigits, minFraDigits).toString();
    					*/
    			/* Original DecimalFormat.subformat was called as above, with a synchronized StringBuffer object
				   that we know will never be accessed by more than one thread. In order to get rid of that
				   we duplicate the subformat code (and all other methods it calls that need symbol substitution)
				   here and replace StringBuffer with StringBuilder.
				   We also omit calls to the delegate because it 1) it expects a StringBuffer and 2) it is
				   assumed to be a DontCareFieldDelegate, which does nothing. */
    			/*
				private static StringBuilder subformat(DecimalFormat df, StringBuilder result, FieldDelegate delegate,
													   boolean isNegative, boolean isInteger,
													   int maxIntDigits, int minIntDigits,
													   int maxFraDigits, int minFraDigits)
    			 									   */
    			{
    				final boolean isInteger = false;
    				StringBuilder result = new StringBuilder();
    				FieldDelegate delegate = INSTANCE.getFieldDelegate();
    				// NOTE: This isn't required anymore because DigitList takes care of this.
    				//
    				//  // The negative of the exponent represents the number of leading
    				//  // zeros between the decimal and the first non-zero digit, for
    				//  // a value < 0.1 (e.g., for 0.00123, -fExponent == 2).  If this
    				//  // is more than the maximum fraction digits, then we have an underflow
    				//  // for the printed representation.  We recognize this here and set
    				//  // the DigitList representation to zero in this situation.
    				//
    				//  if (-digitList.decimalAt >= getMaximumFractionDigits())
    				//  {
    				//      digitList.count = 0;
    				//  }

    				char zero = symbols.getZeroDigit();
    				int zeroDelta = zero - '0'; // '0' is the DigitList representation of zero
    				char grouping = symbols.getGroupingSeparator();
    				char decimal = isCurrencyFormat ?
    						symbols.getMonetaryDecimalSeparator() :
    							symbols.getDecimalSeparator();

					/* Per bug 4147706, DecimalFormat must respect the sign of numbers which
					 * format as zero.  This allows sensible computations and preserves
					 * relations such as signum(1/x) = signum(x), where x is +Infinity or
					 * -Infinity.  Prior to this fix, we always formatted zero values as if
					 * they were positive.  Liu 7/6/98.
					 */
					if (digitList.isZero()) {
						digitList.decimalAt = 0; // Normalize
					}
    			
					/*
					private static void append(DecimalFormat df, StringBuilder result, String string,
											   FieldDelegate delegate,
											   FieldPosition[] positions,
											   Format.Field signAttribute)
					 */
					{
						String string;
						//FieldPosition[] positions;
						//Format.Field signAttribute = Field.SIGN;

						if (isNegative) {
							string = negativePrefix;
							//positions = getNegativePrefixFieldPositions(df);
						}
						else {
							string = positivePrefix;
							//positions = getPositivePrefixFieldPositions(df);
						}

						//int start = result.length();

						if (string.length() > 0) {
							result.append(string);
							/*
							for (int counter = 0, max = positions.length; counter < max;
								 counter++) {
								FieldPosition fp = positions[counter];
								Format.Field attribute = fp.getFieldAttribute();

								if (attribute == Field.SIGN) {
									attribute = signAttribute;
							}*/
							/* XXX: Assumed to be a nop
							   delegate.formatted(attribute, attribute,
							   start + fp.getBeginIndex(),
							   start + fp.getEndIndex(), result);
							 */
							//}
						}
					}

					if (useExponentialNotation) {
						int iFieldStart = result.length();
						int iFieldEnd = -1;
						int fFieldStart = -1;

						// Minimum integer digits are handled in exponential format by
						// adjusting the exponent.  For example, 0.01234 with 3 minimum
						// integer digits is "123.4E-4".

						// Maximum integer digits are interpreted as indicating the
						// repeating range.  This is useful for engineering notation, in
						// which the exponent is restricted to a multiple of 3.  For
						// example, 0.01234 with 3 maximum integer digits is "12.34e-3".
						// If maximum integer digits are > 1 and are larger than
						// minimum integer digits, then minimum integer digits are
						// ignored.
						int exponent = digitList.decimalAt;
						int repeat = maxIntDigits;
						int minimumIntegerDigits = minIntDigits;
						if (repeat > 1 && repeat > minIntDigits) {
							// A repeating range is defined; adjust to it as follows.
							// If repeat == 3, we have 6,5,4=>3; 3,2,1=>0; 0,-1,-2=>-3;
							// -3,-4,-5=>-6, etc. This takes into account that the
							// exponent we have here is off by one from what we expect;
							// it is for the format 0.MMMMMx10^n.
							if (exponent >= 1) {
								exponent = ((exponent - 1) / repeat) * repeat;
							} else {
								// integer division rounds towards 0
								exponent = ((exponent - repeat) / repeat) * repeat;
							}
							minimumIntegerDigits = 1;
						} else {
							// No repeating range is defined; use minimum integer digits.
							exponent -= minimumIntegerDigits;
						}

						// We now output a minimum number of digits, and more if there
						// are more digits, up to the maximum number of digits.  We
						// place the decimal point after the "integer" digits, which
						// are the first (decimalAt - exponent) digits.
						int minimumDigits = minIntDigits + minFraDigits;
						if (minimumDigits < 0) {    // overflow?
							minimumDigits = Integer.MAX_VALUE;
						}

						// The number of integer digits is handled specially if the number
						// is zero, since then there may be no digits.
						int integerDigits = digitList.isZero() ? minimumIntegerDigits :
							digitList.decimalAt - exponent;
						if (minimumDigits < integerDigits) {
							minimumDigits = integerDigits;
						}
						int totalDigits = digitList.count;
						if (minimumDigits > totalDigits) {
							totalDigits = minimumDigits;
						}
						boolean addedDecimalSeparator = false;

						for (int i=0; i<totalDigits; ++i) {
							if (i == integerDigits) {
								// Record field information for caller.
								iFieldEnd = result.length();

								result.append(decimal);
								addedDecimalSeparator = true;

								// Record field information for caller.
								fFieldStart = result.length();
							}
							result.append((i < digitList.count) ?
									(char)(digitList.digits[i] + zeroDelta) :
										zero);
						}

						if (decimalSeparatorAlwaysShown && totalDigits == integerDigits) {
							// Record field information for caller.
							iFieldEnd = result.length();

							result.append(decimal);
							addedDecimalSeparator = true;

							// Record field information for caller.
							fFieldStart = result.length();
						}

						// Record field information
						if (iFieldEnd == -1) {
							iFieldEnd = result.length();
						}
						/* XXX: Assumed to be a nop
						   delegate.formatted(INTEGER_FIELD, Field.INTEGER, Field.INTEGER,
						   iFieldStart, iFieldEnd, result);
						   if (addedDecimalSeparator) {
						   delegate.formatted(Field.DECIMAL_SEPARATOR,
						   Field.DECIMAL_SEPARATOR,
						   iFieldEnd, fFieldStart, result);
						   }
						 */
						if (fFieldStart == -1) {
							fFieldStart = result.length();
						}
						/* XXX: Assumed to be a nop
						   delegate.formatted(FRACTION_FIELD, Field.FRACTION, Field.FRACTION,
						   fFieldStart, result.length(), result);
						 */

						// The exponent is output using the pattern-specified minimum
						// exponent digits.  There is no maximum limit to the exponent
						// digits, since truncating the exponent would result in an
						// unacceptable inaccuracy.
						int fieldStart = result.length();

						result.append(symbols.getExponentSeparator());

						/* XXX: Assumed to be a nop
						   delegate.formatted(Field.EXPONENT_SYMBOL, Field.EXPONENT_SYMBOL,
						   fieldStart, result.length(), result);
						 */

						// For zero values, we force the exponent to zero.  We
						// must do this here, and not earlier, because the value
						// is used to determine integer digit count above.
						if (digitList.isZero()) {
							exponent = 0;
						}

						boolean negativeExponent = exponent < 0;
						if (negativeExponent) {
							exponent = -exponent;
							fieldStart = result.length();
							result.append(symbols.getMinusSign());
							/* XXX: Assumed to be a nop
							   delegate.formatted(Field.EXPONENT_SIGN, Field.EXPONENT_SIGN,
							   fieldStart, result.length(), result);
							 */
						}
						digitList.set(negativeExponent, exponent);

						int eFieldStart = result.length();

						for (int i=digitList.decimalAt; i<minExponentDigits; ++i) {
							result.append(zero);
						}
						for (int i=0; i<digitList.decimalAt; ++i) {
							result.append((i < digitList.count) ?
									(char)(digitList.digits[i] + zeroDelta) : zero);
						}
						/* XXX: Assumed to be a nop
						   delegate.formatted(Field.EXPONENT, Field.EXPONENT, eFieldStart,
						   result.length(), result);
						 */
					} else {
						int iFieldStart = result.length();

						// Output the integer portion.  Here 'count' is the total
						// number of integer digits we will display, including both
						// leading zeros required to satisfy getMinimumIntegerDigits,
						// and actual digits present in the number.
						int count = minIntDigits;
						int digitIndex = 0; // Index into digitList.fDigits[]
						if (digitList.decimalAt > 0 && count < digitList.decimalAt) {
							count = digitList.decimalAt;
						}

						// Handle the case where getMaximumIntegerDigits() is smaller
						// than the real number of integer digits.  If this is so, we
						// output the least significant max integer digits.  For example,
						// the value 1997 printed with 2 max integer digits is just "97".
						if (count > maxIntDigits) {
							count = maxIntDigits;
							digitIndex = digitList.decimalAt - count;
						}

						int sizeBeforeIntegerPart = result.length();
						for (int i=count-1; i>=0; --i) {
							if (i < digitList.decimalAt && digitIndex < digitList.count) {
								// Output a real digit
								result.append((char)(digitList.digits[digitIndex++] + zeroDelta));
							} else {
								// Output a leading zero
								result.append(zero);
							}

							// Output grouping separator if necessary.  Don't output a
							// grouping separator if i==0 though; that's at the end of
							// the integer part.
							if (isGroupingUsed(df) && i>0 && (groupingSize != 0) &&
									(i % groupingSize == 0)) {
								int gStart = result.length();
								result.append(grouping);
								/* XXX: Assumed to be a nop
								   delegate.formatted(Field.GROUPING_SEPARATOR,
								   Field.GROUPING_SEPARATOR, gStart,
								   result.length(), result);
								 */
							}
						}

						// Determine whether or not there are any printable fractional
						// digits.  If we've used up the digits we know there aren't.
						boolean fractionPresent = (minFraDigits > 0) ||
								(!isInteger && digitIndex < digitList.count && !digitList.isZero());

						// If there is no fraction present, and we haven't printed any
						// integer digits, then print a zero.  Otherwise we won't print
						// _any_ digits, and we won't be able to parse this string.
						if (!fractionPresent && result.length() == sizeBeforeIntegerPart) {
							result.append(zero);
						}

						/* XXX: Assumed to be a nop
						   delegate.formatted(INTEGER_FIELD, Field.INTEGER, Field.INTEGER,
						   iFieldStart, result.length(), result);
						 */

						// Output the decimal separator if we always do so.
						int sStart = result.length();
						if (decimalSeparatorAlwaysShown || fractionPresent) {
							result.append(decimal);
						}

						/* XXX: Assumed to be a nop
						   if (sStart != result.length()) {
						   delegate.formatted(Field.DECIMAL_SEPARATOR,
						   Field.DECIMAL_SEPARATOR,
						   sStart, result.length(), result);
						   }*/
						int fFieldStart = result.length();

						for (int i=0; i < maxFraDigits; ++i) {
							// Here is where we escape from the loop.  We escape if we've
							// output the maximum fraction digits (specified in the for
							// expression above).
							// We also stop when we've output the minimum digits and either:
							// we have an integer, so there is no fractional stuff to
							// display, or we're out of significant digits.
							if (i >= minFraDigits &&
									(isInteger || digitIndex >= digitList.count)) {
								break;
							}

							// Output leading fractional zeros. These are zeros that come
							// after the decimal but before any significant digits. These
							// are only output if abs(number being formatted) < 1.0.
							if (-1-i > (digitList.decimalAt-1)) {
								result.append(zero);
								continue;
							}

							// Output a digit, if we have any precision left, or a
							// zero if we don't.  We don't want to output noise digits.
							if (!isInteger && digitIndex < digitList.count) {
								result.append((char)(digitList.digits[digitIndex++] + zeroDelta));
							} else {
								result.append(zero);
							}
						}

						// Record field information for caller.
						/* XXX: Assumed to be a nop
						   delegate.formatted(FRACTION_FIELD, Field.FRACTION, Field.FRACTION,
						   fFieldStart, result.length(), result);
						 */
					}

					/*
					private static void append(DecimalFormat df, StringBuilder result, String string,
											   FieldDelegate delegate,
											   FieldPosition[] positions,
											   Format.Field signAttribute)
					 */
					{
						String string;
						//FieldPosition[] positions;
						//Format.Field signAttribute = Field.SIGN;

						if (isNegative) {
							string = negativeSuffix;
							//positions = getNegativeSuffixFieldPositions(df);
						}
						else {
							string = positiveSuffix;
							//positions = getPositiveSuffixFieldPositions(df);
						}

						//int start = result.length();

						if (string.length() > 0) {
							result.append(string);
							/*
							for (int counter = 0, max = positions.length; counter < max;
								 counter++) {
								FieldPosition fp = positions[counter];
								Format.Field attribute = fp.getFieldAttribute();

								if (attribute == Field.SIGN) {
									attribute = signAttribute;
								}*/
								/* XXX: Assumed to be a nop
								   delegate.formatted(attribute, attribute,
								   start + fp.getBeginIndex(),
								   start + fp.getEndIndex(), result);
								 */
							//}
						}
					}

					return result.toString();
    			}
    		}
    	}
    	else {
    		return df.format(number.doubleValue());
    	}
    }


    /**
     * The implementation of format needs to follow the implementation of
     * java.text.DecimalFormat.format methods in regards to setting
     * maxIntDigits, minIntDigits, etc.
     *
     * @see #formatAsDouble(DecimalFormat df, BigDecimal number) for the difference
     * between formatAsDouble and formatAsFloat.
     *
     * @param df   the object on which format was called 
     * @param number   the object on which doubleValue or floatValue() is called
     * @return the formatted string
     **/
    private static String formatAsFloat(DecimalFormat df, BigDecimal number)  {
    	if ((flags & 0x00000001) == 0x00000001) {
    		//The optimization is possible if the representation of the BigDecimal object,
    		//number, is long lookaside (LL)
    		INSTANCE.setBeginIndex(0);
    		INSTANCE.setEndIndex(0);
    		if (multiplier != 1) 
    			number = number.multiply(getBigDecimalMultiplier(df));
    		boolean isNegative = number.signum() == -1;
    		synchronized(digitList) {
    			int maxIntDigits = df.getMaximumIntegerDigits();
    			int minIntDigits = df.getMinimumIntegerDigits();
    			int maxFraDigits = df.getMaximumFractionDigits();
    			int minFraDigits = df.getMinimumFractionDigits();

    			if (laside == 0) {
    				digitList.digits[0] = '0';
    				digitList.count = 1;
    				digitList.decimalAt = 0;
    			}
    			else {
    				long intVal = laside;
    				long intTmpVal;
    				long sign= ( laside >> 63) | ((-laside) >>> 63);
    				int length = number.precision();
    				if (sign == -1) 
    					intVal = -intVal;
    				int d = length - 1;
    				while (intVal != 0) {
    					intTmpVal = intVal;
    					intVal = intVal/100;
    					int rem = (byte)(intTmpVal - ((intVal << 6) + (intVal << 5) + (intVal << 2)));
    					digitList.digits[d--] = doubleDigitsOnes[rem];
    					if (d >= 0)
    						digitList.digits[d--] = doubleDigitsTens[rem];
    				}
    				digitList.count = length;
    				digitList.decimalAt = -scale + length;
    			}
    			 /*
    			return subformat(df, new StringBuffer(), INSTANCE.getFieldDelegate(), isNegative, false,
    					maxIntDigits, minIntDigits, maxFraDigits, minFraDigits).toString();
    					 */
    			/* Original DecimalFormat.subformat was called as above, with a synchronized StringBuffer object
                that we know will never be accessed by more than one thread. In order to get rid of that
                we duplicate the subformat code (and all other methods it calls that need symbol substitution)
                here and replace StringBuffer with StringBuilder.
                We also omit calls to the delegate because it 1) it expects a StringBuffer and 2) it is
                assumed to be a DontCareFieldDelegate, which does nothing. */
    			/*
             	private static StringBuilder subformat(DecimalFormat df, StringBuilder result, FieldDelegate delegate,
                                                    boolean isNegative, boolean isInteger,
                                                    int maxIntDigits, int minIntDigits,
                                                    int maxFraDigits, int minFraDigits)
    			 */
    			{
    				final boolean isInteger = false;
    				StringBuilder result = new StringBuilder();
    				FieldDelegate delegate = INSTANCE.getFieldDelegate();
    				// NOTE: This isn't required anymore because DigitList takes care of this.
    				//
    				//  // The negative of the exponent represents the number of leading
    				//  // zeros between the decimal and the first non-zero digit, for
    				//  // a value < 0.1 (e.g., for 0.00123, -fExponent == 2).  If this
    				//  // is more than the maximum fraction digits, then we have an underflow
    				//  // for the printed representation.  We recognize this here and set
    				//  // the DigitList representation to zero in this situation.
    				//
    				//  if (-digitList.decimalAt >= getMaximumFractionDigits())
    				//  {
    				//      digitList.count = 0;
    				//  }

    				char zero = symbols.getZeroDigit();
    				int zeroDelta = zero - '0'; // '0' is the DigitList representation of zero
    				char grouping = symbols.getGroupingSeparator();
    				char decimal = isCurrencyFormat ?
    						symbols.getMonetaryDecimalSeparator() :
    							symbols.getDecimalSeparator();

					/* Per bug 4147706, DecimalFormat must respect the sign of numbers which
					 * format as zero.  This allows sensible computations and preserves
					 * relations such as signum(1/x) = signum(x), where x is +Infinity or
					 * -Infinity.  Prior to this fix, we always formatted zero values as if
					 * they were positive.  Liu 7/6/98.
					 */
					if (digitList.isZero()) {
						digitList.decimalAt = 0; // Normalize
					}

					/*
           			private static void append(DecimalFormat df, StringBuilder result, String string,
											   FieldDelegate delegate,
											   FieldPosition[] positions,
											   Format.Field signAttribute)
					 */
					{
						String string;
						//FieldPosition[] positions;
						//Format.Field signAttribute = Field.SIGN;

						if (isNegative) {
							string = negativePrefix;
							//positions = getNegativePrefixFieldPositions(df);
						}
						else {
							string = positivePrefix;
							//positions = getPositivePrefixFieldPositions(df);
						}

						//int start = result.length();

						if (string.length() > 0) {
							result.append(string);
							/*
                  			for (int counter = 0, max = positions.length; counter < max;
		                           counter++) {
		                         FieldPosition fp = positions[counter];
		                         Format.Field attribute = fp.getFieldAttribute();
		
		                         if (attribute == Field.SIGN) {
		                            attribute = signAttribute;
		                         }*/
								/* XXX: Assumed to be a nop
	                            delegate.formatted(attribute, attribute,
	                            start + fp.getBeginIndex(),
	                            start + fp.getEndIndex(), result);
								 */
    							//}
						}
					}

					if (useExponentialNotation) {
						int iFieldStart = result.length();
						int iFieldEnd = -1;
						int fFieldStart = -1;

						// Minimum integer digits are handled in exponential format by
						// adjusting the exponent.  For example, 0.01234 with 3 minimum
						// integer digits is "123.4E-4".

						// Maximum integer digits are interpreted as indicating the
						// repeating range.  This is useful for engineering notation, in
						// which the exponent is restricted to a multiple of 3.  For
						// example, 0.01234 with 3 maximum integer digits is "12.34e-3".
						// If maximum integer digits are > 1 and are larger than
						// minimum integer digits, then minimum integer digits are
						// ignored.
						int exponent = digitList.decimalAt;
						int repeat = maxIntDigits;
						int minimumIntegerDigits = minIntDigits;
						if (repeat > 1 && repeat > minIntDigits) {
							// A repeating range is defined; adjust to it as follows.
							// If repeat == 3, we have 6,5,4=>3; 3,2,1=>0; 0,-1,-2=>-3;
							// -3,-4,-5=>-6, etc. This takes into account that the
							// exponent we have here is off by one from what we expect;
							// it is for the format 0.MMMMMx10^n.
							if (exponent >= 1) {
								exponent = ((exponent - 1) / repeat) * repeat;
							} else {
								// integer division rounds towards 0
								exponent = ((exponent - repeat) / repeat) * repeat;
							}
							minimumIntegerDigits = 1;
						} else {
							// No repeating range is defined; use minimum integer digits.
							exponent -= minimumIntegerDigits;
						}

						// We now output a minimum number of digits, and more if there
						// are more digits, up to the maximum number of digits.  We
						// place the decimal point after the "integer" digits, which
						// are the first (decimalAt - exponent) digits.
						int minimumDigits = minIntDigits + minFraDigits;
						if (minimumDigits < 0) {    // overflow?
							minimumDigits = Integer.MAX_VALUE;
						}

						// The number of integer digits is handled specially if the number
						// is zero, since then there may be no digits.
						int integerDigits = digitList.isZero() ? minimumIntegerDigits :
							digitList.decimalAt - exponent;
						if (minimumDigits < integerDigits) {
							minimumDigits = integerDigits;
						}
						int totalDigits = digitList.count;
						if (minimumDigits > totalDigits) {
							totalDigits = minimumDigits;
						}
						boolean addedDecimalSeparator = false;

						for (int i=0; i<totalDigits; ++i) {
							if (i == integerDigits) {
								// Record field information for caller.
								iFieldEnd = result.length();

								result.append(decimal);
								addedDecimalSeparator = true;

								// Record field information for caller.
								fFieldStart = result.length();
							}
							result.append((i < digitList.count) ?
									(char)(digitList.digits[i] + zeroDelta) :
										zero);
						}

						if (decimalSeparatorAlwaysShown && totalDigits == integerDigits) {
							// Record field information for caller.
							iFieldEnd = result.length();

							result.append(decimal);
							addedDecimalSeparator = true;

							// Record field information for caller.
							fFieldStart = result.length();
						}

						// Record field information
						if (iFieldEnd == -1) {
							iFieldEnd = result.length();
						}
						/* XXX: Assumed to be a nop
	                      delegate.formatted(INTEGER_FIELD, Field.INTEGER, Field.INTEGER,
	                      iFieldStart, iFieldEnd, result);
	                      if (addedDecimalSeparator) {
	                      delegate.formatted(Field.DECIMAL_SEPARATOR,
	                      Field.DECIMAL_SEPARATOR,
	                      iFieldEnd, fFieldStart, result);
	                      }
						 */
						if (fFieldStart == -1) {
							fFieldStart = result.length();
						}
						/* XXX: Assumed to be a nop
	                      delegate.formatted(FRACTION_FIELD, Field.FRACTION, Field.FRACTION,
	                      fFieldStart, result.length(), result);
						 */

						// The exponent is output using the pattern-specified minimum
						// exponent digits.  There is no maximum limit to the exponent
						// digits, since truncating the exponent would result in an
						// unacceptable inaccuracy.
						int fieldStart = result.length();

						result.append(symbols.getExponentSeparator());

						/* XXX: Assumed to be a nop
	                      delegate.formatted(Field.EXPONENT_SYMBOL, Field.EXPONENT_SYMBOL,
	                      fieldStart, result.length(), result);
						 */

						// For zero values, we force the exponent to zero.  We
						// must do this here, and not earlier, because the value
						// is used to determine integer digit count above.
						if (digitList.isZero()) {
							exponent = 0;
						}

						boolean negativeExponent = exponent < 0;
						if (negativeExponent) {
							exponent = -exponent;
							fieldStart = result.length();
							result.append(symbols.getMinusSign());
							/* XXX: Assumed to be a nop
	                         delegate.formatted(Field.EXPONENT_SIGN, Field.EXPONENT_SIGN,
	                         fieldStart, result.length(), result);
							 */
						}
						digitList.set(negativeExponent, exponent);

						int eFieldStart = result.length();

						for (int i=digitList.decimalAt; i<minExponentDigits; ++i) {
							result.append(zero);
						}
						for (int i=0; i<digitList.decimalAt; ++i) {
							result.append((i < digitList.count) ?
									(char)(digitList.digits[i] + zeroDelta) : zero);
						}
						/* XXX: Assumed to be a nop
	                      delegate.formatted(Field.EXPONENT, Field.EXPONENT, eFieldStart,
	                      result.length(), result);
						 */
					} else {
						int iFieldStart = result.length();

						// Output the integer portion.  Here 'count' is the total
						// number of integer digits we will display, including both
						// leading zeros required to satisfy getMinimumIntegerDigits,
						// and actual digits present in the number.
						int count = minIntDigits;
						int digitIndex = 0; // Index into digitList.fDigits[]
						if (digitList.decimalAt > 0 && count < digitList.decimalAt) {
							count = digitList.decimalAt;
						}

						// Handle the case where getMaximumIntegerDigits() is smaller
						// than the real number of integer digits.  If this is so, we
						// output the least significant max integer digits.  For example,
						// the value 1997 printed with 2 max integer digits is just "97".
						if (count > maxIntDigits) {
							count = maxIntDigits;
							digitIndex = digitList.decimalAt - count;
						}

						int sizeBeforeIntegerPart = result.length();
						for (int i=count-1; i>=0; --i) {
							if (i < digitList.decimalAt && digitIndex < digitList.count) {
								// Output a real digit
								result.append((char)(digitList.digits[digitIndex++] + zeroDelta));
							} else {
								// Output a leading zero
								result.append(zero);
							}

							// Output grouping separator if necessary.  Don't output a
							// grouping separator if i==0 though; that's at the end of
							// the integer part.
							if (isGroupingUsed(df) && i>0 && (groupingSize != 0) &&
									(i % groupingSize == 0)) {
								int gStart = result.length();
								result.append(grouping);
								/* XXX: Assumed to be a nop
		                            delegate.formatted(Field.GROUPING_SEPARATOR,
		                            Field.GROUPING_SEPARATOR, gStart,
		                            result.length(), result);
								 */
							}
						}

						// Determine whether or not there are any printable fractional
						// digits.  If we've used up the digits we know there aren't.
						boolean fractionPresent = (minFraDigits > 0) ||
								(!isInteger && digitIndex < digitList.count && !digitList.isZero());

						// If there is no fraction present, and we haven't printed any
						// integer digits, then print a zero.  Otherwise we won't print
						// _any_ digits, and we won't be able to parse this string.
						if (!fractionPresent && result.length() == sizeBeforeIntegerPart) {
							result.append(zero);
						}

						/* XXX: Assumed to be a nop
	                      delegate.formatted(INTEGER_FIELD, Field.INTEGER, Field.INTEGER,
	                      iFieldStart, result.length(), result);
						 */

						// Output the decimal separator if we always do so.
						int sStart = result.length();
						if (decimalSeparatorAlwaysShown || fractionPresent) {
							result.append(decimal);
						}

						/* XXX: Assumed to be a nop
	                      if (sStart != result.length()) {
	                      delegate.formatted(Field.DECIMAL_SEPARATOR,
	                      Field.DECIMAL_SEPARATOR,
	                      sStart, result.length(), result);
	                      }*/
						int fFieldStart = result.length();

						for (int i=0; i < maxFraDigits; ++i) {
							// Here is where we escape from the loop.  We escape if we've
							// output the maximum fraction digits (specified in the for
							// expression above).
							// We also stop when we've output the minimum digits and either:
							// we have an integer, so there is no fractional stuff to
							// display, or we're out of significant digits.
							if (i >= minFraDigits &&
									(isInteger || digitIndex >= digitList.count)) {
								break;
							}

							// Output leading fractional zeros. These are zeros that come
							// after the decimal but before any significant digits. These
							// are only output if abs(number being formatted) < 1.0.
							if (-1-i > (digitList.decimalAt-1)) {
								result.append(zero);
								continue;
							}

							// Output a digit, if we have any precision left, or a
							// zero if we don't.  We don't want to output noise digits.
							if (!isInteger && digitIndex < digitList.count) {
								result.append((char)(digitList.digits[digitIndex++] + zeroDelta));
							} else {
								result.append(zero);
							}
						}

						// Record field information for caller.
						/* XXX: Assumed to be a nop
	                      delegate.formatted(FRACTION_FIELD, Field.FRACTION, Field.FRACTION,
	                      fFieldStart, result.length(), result);
						 */
					}

					/*
	                private static void append(DecimalFormat df, StringBuilder result, String string,
                                   FieldDelegate delegate,
                                   FieldPosition[] positions,
                                   Format.Field signAttribute)
					 */
					{
						String string;
						//FieldPosition[] positions;
						//Format.Field signAttribute = Field.SIGN;

						if (isNegative) {
							string = negativeSuffix;
							//positions = getNegativeSuffixFieldPositions(df);
						}
						else {
							string = positiveSuffix;
							//positions = getPositiveSuffixFieldPositions(df);
						}

						//int start = result.length();

						if (string.length() > 0) {
							result.append(string);
							/*
	                      	for (int counter = 0, max = positions.length; counter < max;
	                           counter++) {
	                         FieldPosition fp = positions[counter];
	                         Format.Field attribute = fp.getFieldAttribute();
	
	                         if (attribute == Field.SIGN) {
	                            attribute = signAttribute;
	                         }*/
	    					/* XXX: Assumed to be a nop
	                            delegate.formatted(attribute, attribute,
	                            start + fp.getBeginIndex(),
	                            start + fp.getEndIndex(), result);
	    					*/
	    					//}
						}
					}

					return result.toString();
    			}
    		}
    	}
    	else {
    		return df.format(number.floatValue());
    	}
    }

    
	private static BigDecimal getBigDecimalMultiplier(DecimalFormat df) {
		throw new RuntimeException("JIT Helper expected"); //$NON-NLS-1$
	}

	private static FieldPosition[] getNegativePrefixFieldPositions(DecimalFormat df) {
    	throw new RuntimeException("JIT Helper expected"); //$NON-NLS-1$
    }
    
	private static FieldPosition[] getNegativeSuffixFieldPositions(DecimalFormat df) {
		throw new RuntimeException("JIT Helper expected"); //$NON-NLS-1$
	}

	private static FieldPosition[] getPositivePrefixFieldPositions(DecimalFormat df) {
		throw new RuntimeException("JIT Helper expected"); //$NON-NLS-1$
	}

	private static FieldPosition[] getPositiveSuffixFieldPositions(DecimalFormat df) {
		throw new RuntimeException("JIT Helper expected"); //$NON-NLS-1$
	}

	private static boolean isGroupingUsed(NumberFormat nf) {
    	throw new RuntimeException("JIT Helper expected"); //$NON-NLS-1$
    }
}
