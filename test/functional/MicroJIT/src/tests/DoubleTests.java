/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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

import static org.junit.jupiter.api.Assertions.assertEquals;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.Tag;
import org.junit.jupiter.api.Order;
import org.junit.jupiter.api.DisplayName;
import org.junit.jupiter.params.ParameterizedTest;
import org.junit.jupiter.params.provider.CsvSource;

/* Tests that have dependencies must be labelled as @Order(2), @Order(3)... */
/* Tests with no dependencies are labelled as @Order(1) */

public class DoubleTests {

    @Tag("add<DD>D")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1}) = {2}")
    @CsvSource({
        "99.264860, 2, 101.264860",
        "1000, 43, 1043",
        "101.26485238, 101.26485238, 202.52970476",
        "1.79E+308, 1, 1.79E+308",
        "2, 1.79E+308, 1.79E+308",
        "1.79E+308, 0, 1.79E+308",
        "NaN, 10, NaN",                         // If either value1 or value2 is NaN, the result is NaN
        "1024, NaN, NaN",                       // If either value1 or value2 is NaN, the result is NaN
        "NaN, NaN, NaN",                        // If either value1 or value2 is NaN, the result is NaN
        "Infinity, -Infinity, NaN",             // The sum of two infinities of opposite sign is NaN
        "-4.94E-324, 4.94E-324, 0",
        "Infinity, Infinity, Infinity",         // The sum of two infinities of the same sign is the infinity of that sign
        "-Infinity, -Infinity, -Infinity",      // The sum of two infinities of the same sign is the infinity of that sign
        "Infinity, 1.0247E+200, Infinity",      // The sum of an infinity and any finite value is equal to the infinity
        "-Infinity, -1.0247E+200, -Infinity",   // The sum of an infinity and any finite value is equal to the infinity
        "0, -0, 0",                             // The sum of two zeroes of opposite sign is positive zero
        "0, 0, 0",                              // The sum of two zeroes of the same sign is the zero of that sign
        "-0, -0, -0",                           // The sum of two zeroes of the same sign is the zero of that sign
        "0, 1.111111111, 1.111111111",          // The sum of a zero and a nonzero finite value is equal to the nonzero value
        "1.1234567890, 0, 1.1234567890",        // The sum of a zero and a nonzero finite value is equal to the nonzero value
        "1.79E+308, -1.79E+308, 0",             // The sum of two nonzero finite values of the same magnitude and opposite sign is positive zero
        "1.79E+308, 1.79E+308, Infinity",       // Magnitude too large, hence operation overflow results in an infinity of appropriate sign
        "-1.79E+308, -0.01E+308, -Infinity",    // Magnitude too large, hence operation overflow results in an infinity of appropriate sign
        "4.94E-325, 0, 0",                      // Magnitude too small, hence operation underflow results in a zero of appropriate sign
        "-4.94E-325, -0, -0"                    // Magnitude too small, hence operation underflow results in a zero of appropriate sign
    })
    public void testDAdd(double arg1, double arg2, double expected) {
        double result = 0D;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static double add(double x, double y) {
        return x + y;
    }

    @Tag("sub<DD>D")
    @Order(1)
    @ParameterizedTest(name = "#{index} - sub({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, -1",
        "1, 0, 1",
        "-1, -1, 0",
        "101.26485238, 2, 99.26485238",
        "1.79E+308, 1.79E+308, 0",
        "1.79E+308, -1.79E+308, Infinity",      // Magnitude too large, hence operation overflow results in an infinity of appropriate sign
        "-4.94E-324, -4.94E-324, 0",
        "-1.79E+308, 1.79E+308, -Infinity",     // Magnitude too large, hence operation overflow results in an infinity of appropriate sign
        "Infinity, -Infinity, Infinity",
        "Infinity, Infinity, NaN",
        "4.94E-324, 0, 4.94E-324",
        "4.94E-325, 0, 0",                      // Magnitude too small, hence operation underflow results in a zero of appropriate sign
        "0, 0, 0",
        "0, -0, 0",
        "-0, 0, -0",
        "0, -4.94E-324, 4.94E-324",
        "0, 4.94E-324, -4.94E-324"
    })
    public void testDSub(double arg1, double arg2, double expected) {
        double result = 0D;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = sub(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static double sub(double x, double y) {
        return x - y;
    }

    @Tag("mul<DD>D")
    @Order(1)
    @ParameterizedTest(name = "#{index} - mul({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, 0",
        "100, 40, 4000",
        "1.79E+308, 1, 1.79E+308",
        "1.79E+308, 0, 0",
        "-1.79E+308, 1, -1.79E+308",
        "-4.94E-324, 0, -0",
        "NaN, 10.1234567890, NaN",              // If either value1 or value2 is NaN, the result is NaN
        "1024.56745355, NaN, NaN",              // If either value1 or value2 is NaN, the result is NaN
        "NaN, NaN, NaN",                        // If either value1 or value2 is NaN, the result is NaN
        "Infinity, 0, NaN",                     // Multiplication of an infinity by a zero results in NaN
        "Infinity, -4.94E-324, -Infinity",      // Multiplication of an infinity by a finite value results in a signed infinity
        "-Infinity, 4.94E-324, -Infinity",      // Multiplication of an infinity by a finite value results in a signed infinity
        "Infinity, 4.94E-324, Infinity",        // Multiplication of an infinity by a finite value results in a signed infinity
        "-Infinity, -4.94E-324, Infinity",      // Multiplication of an infinity by a finite value results in a signed infinity
        "-4.94E-324, 4.94E-324, -0",            // Magnitude too small, hence operation underflow results in a zero
        "1.79E+308, 1.79E+308, Infinity"        // Magnitude too large, hence operation overflow results in an infinity
    })
    public void testDMul(double arg1, double arg2, double expected) {
        double result = 0D;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = mul(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static double mul(double x, double y) {
        return x * y;
    }

    @Tag("div<DD>D")
    @Order(1)
    @ParameterizedTest(name = "#{index} - div({0},{1}) = {2}")
    @CsvSource({
        "1.4E+18, 1, 1.4E+18",
        "1.4E+18, 100, 1.4E+16",
        "1.4E+18, -100, -1.4E+16",
        "-1.4E+18, 100, -1.4E+16",
        "-1.4E+18, -100, 1.4E+16",
        "1.4E+18, 2, 0.7E+18",
        "1.79E+308, 1.79E+308, 1",
        "-1.79E+308, -1.79E+308, 1",
        "1.79E+308, 1, 1.79E+308",
        "NaN, 1.2E+40, NaN",                    // If either value1 or value2 is NaN, the result is NaN
        "1.2E+40, NaN, NaN",                    // If either value1 or value2 is NaN, the result is NaN
        "NaN, NaN, NaN",                        // If either value1 or value2 is NaN, the result is NaN
        "Infinity, Infinity, NaN",              // Division of an infinity by an infinity results in NaN
        "Infinity, -4.94E-324, -Infinity",      // Division of an infinity by a finite value results in a signed infinity
        "-Infinity, 4.94E-324, -Infinity",      // Division of an infinity by a finite value results in a signed infinity
        "Infinity, 4.94E-324, Infinity",        // Division of an infinity by a finite value results in a signed infinity
        "-Infinity, -4.94E-324, Infinity",      // Division of an infinity by a finite value results in a signed infinity
        "1.79E+308, Infinity, 0",               // Division of a finite value by an infinity results in a signed zero
        "1.79E+308, -Infinity, -0",             // Division of a finite value by an infinity results in a signed zero
        "0, 0, NaN",                            // Division of a zero by a zero results in NaN
        "0, 1.2E+40, 0",                        // Division of zero by any other finite value results in a signed zero
        "-1.2E+40, 0, -Infinity",               // Division of a nonzero finite value by a zero results in a signed infinity
        "-4.94E-324, 4.94E+324, -0",            // Magnitude too small, hence operation underflow results in a zero
        "1.79E+308, 1.79E-308, Infinity"        // Magnitude too large, hence operation overflow results in an infinity
    })
    public void testDDiv(double arg1, double arg2, double expected) {
        double result = 0D;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = div(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static double div(double x, double y) {
        return x / y;
    }

    @Tag("rem<DD>D")
    @Order(1)
    @ParameterizedTest(name = "#{index} - rem({0},{1}) = {2}")
    @CsvSource({
        "10.0, 9.0, 1.0",
        "3.0, 2.0, 1.0",
        "10.0, 7.0, 3.0",
        "9, 5, 4",
        "9, -1, 0",
        "100, 35, 30",
        "100, -35, 30",
        "-100, 35, -30",
        "-100, -35, -30",
        "21, 7, 0.0",
        "21, -7, 0.0",
        "-21, 7, -0.0",
        "-21, -7, -0.0",
        "31.34, 2.2, 0.5399999999999974",
        "0, 1, 0",
        "1000, 100, 0",
        "1.79E+308, 1.79E+308, 0",
        "-4.94E-324, -1, -4.94E-324",
        "-4.94E-324, -4.94E-324, -0",
        "4.94E-324, -1, 4.94E-324",
        "-16.3, 4.1, -4.000000000000002",
        "17.876543, 4, 1.8765430000000016",
        "17.8, 4, 1.8000000000000007",
        "17.8, 4.1, 1.4000000000000021",
        "-17.8, 4.1, -1.4000000000000021",
        "17.8, -4.1, 1.4000000000000021",
        "-17.8, -4.1, -1.4000000000000021",
        "60984.1, -497.99, 229.31999999999744",
        "-497.99, 60984.1, -497.99",

        "NaN, 10E+20, NaN",                   // If either value1
        "10E+20, NaN, NaN",                   // or value2 is NaN,
        "NaN, NaN, NaN",                      // the result is NaN

        "Infinity, -1.79E+308, NaN",          // If the dividend is an infinity
        "4.94E-324, 0, NaN",                  // or the divisor is a zero
        "Infinity, 0, NaN",                   // or both, the result is NaN
        "Infinity, -0, NaN",
        "-Infinity, 2.3, NaN",                // If the dividend is an infinity
        "2.34, -0, NaN",                      // or the divisor is a zero
        "-Infinity, -0, NaN",                 // or both, the result is NaN
        "-Infinity, 0, NaN",

        "-2.34, Infinity, -2.34",             // If the dividend is finite
        "2.34, Infinity, 2.34",               // and the divisor is an infinity,
        "-2.34, -Infinity, -2.34",            // the result equals the dividend
        "2.34, -Infinity, 2.34",

        "0, -2.34, 0",                        // If the dividend is a zero
        "-0, -2.34, -0",                      // and the divisor is finite,
        "0, 2.34, 0",                         // the result equals the dividend
        "-0, 2.34, -0"
    })
    public void testDRem(double arg1, double arg2, double expected) {
        double result = 0D;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = rem(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static double rem(double x, double y) {
        return x % y;
    }

    @Tag("neg<D>D")
    @Order(1)
    @ParameterizedTest(name = "#{index} - neg({0}) = {1}")
    @CsvSource({
        "0, -0",                    // If the operand is a zero,
        "-0, 0",                    // the result is the zero of opposite sign.
        "NaN, NaN",                 // If the operand is NaN, the result is NaN.
        "Infinity, -Infinity",      // If the operand is an infinity,
        "-Infinity, Infinity",      // the result is the infinity of opposite sign.
        "-1.147483648, 1.147483648",
        "1.147483648, -1.147483648",
        "-4.94E-324, 4.94E-324",
        "1.79E+308, -1.79E+308"
    })
    public void testDNeg(double arg1, double expected) {
        double result = 0D;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = neg(arg1);
        }
        assertEquals(expected, result);
    }

    public static double neg(double x) {
        return -x;
    }

    @Tag("dconst_0<>D")
    @Order(1)
    @ParameterizedTest(name = "#{index} - dconst_0() = {0}")
    @CsvSource({
        "0"
    })
    public void testdconst_0(double expected) {
        double result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dconst_0();
        }
        assertEquals(expected, result);
    }

    public static double dconst_0() {
        return 0;
    }

    @Tag("dconst_1<>D")
    @Order(1)
    @ParameterizedTest(name = "#{index} - dconst_1() = {0}")
    @CsvSource({
        "1"
    })
    public void testdconst_1(double expected) {
        double result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dconst_1();
        }
        assertEquals(expected, result);
    }

    public static double dconst_1() {
        return 1;
    }

    @Tag("dcmpl<DD>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - dcmpl({0},{1}) = {2}")
    @CsvSource({
        "99.26485999, 2, 1",
        "101.26485999, 101.26485999, 0",
        "3.4E+38, 1, 1",
        "2, 3.4E+38, -1",
        "3.4E+38, 0.0000001E+38, 1",
        "3.4E+38, 3.4E38, 0",
        "NaN, 10, -1",                        // If either value1 or value2 is NaN, the result is -1
        "1024, NaN, -1",                      // If either value1 or value2 is NaN, the result is -1
        "NaN, NaN, -1",                       // If either value1 or value2 is NaN, the result is -1
        "Infinity, 3.4E+38, 1",               // Positive infinity is greater than all finite values
        "3.4E+38, -Infinity, 1",              // Negative infinity is less than all finite values
        "3.4E+38, Infinity, -1",              // Positive infinity is greater than all finite values
        "-Infinity, 3.4E+38, -1",             // Negative infinity is less than all finite values
        "0.0, -0.0, 0",                       // Positive zero and negative zero are considered equal
        "Infinity, -Infinity, 1",
        "1.79E+308, 1, 1",
        "1.79E+308, 0, 1",
        "-1.79E+308, 1, -1"
    })
    public void testDcmpl(double arg1, double arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dcmpl(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int dcmpl(double x, double y) {
        return dcmp.dcmpl(x, y);
    }

    @Tag("dcmpg<DD>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - dcmpg({0},{1}) = {2}")
    @CsvSource({
        "99.26485999, 2, 1",
        "101.26485999, 101.26485999, 0",
        "3.4E+38, 1, 1",
        "3.4E+38, -1, 1",
        "2, 3.4E+38, -1",
        "3.4E+38, 0.0000001E+38, 1",
        "3.4E+38, 3.4E38, 0",
        "NaN, 10, 1",                         // If either value1 or value2 is NaN, the result is 1
        "1024, NaN, 1",                       // If either value1 or value2 is NaN, the result is 1
        "NaN, NaN, 1",                        // If either value1 or value2 is NaN, the result is 1
        "Infinity, 3.4E+38, 1",               // Positive infinity is greater than all finite values
        "3.4E+38, -Infinity, 1",              // Negative infinity is less than all finite values
        "3.4E+38, Infinity, -1",              // Positive infinity is greater than all finite values
        "-Infinity, 3.4E+38, -1",             // Negative infinity is less than all finite values
        "0.0, -0.0, 0",                       // Positive zero and negative zero are considered equal
        "Infinity, -Infinity, 1",
        "1.79E+308, 1, 1",
        "1.79E+308, 0, 1",
        "-1.79E+308, 1, -1"
    })
    public void testDcmpg(double arg1, double arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dcmpg(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int dcmpg(double x, double y) {
        return dcmp.dcmpg(x, y);
    }

}
