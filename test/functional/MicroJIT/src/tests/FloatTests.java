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

public class FloatTests {

    @Tag("add<FF>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1}) = {2}")
    @CsvSource({
        "99.26485999, 2, 101.264860",
        "101.26485999, 101.26485999, 202.529720",
        "3.4E+38, 1, 3.4E+38",
        "2, 3.4E+38, 3.4E38",
        "3.4E+38, 0.0000001E+38, 3.4E38",
        "3.4E+38, 0, 3.4E38",
        "NaN, 10, NaN",                       // If either value1 or value2 is NaN, the result is NaN
        "1024, NaN, NaN",                     // If either value1 or value2 is NaN, the result is NaN
        "NaN, NaN, NaN",                      // If either value1 or value2 is NaN, the result is NaN
        "Infinity, -Infinity, NaN",           // The sum of two infinities of opposite sign is NaN
        "-1.18E-38, 1.18E-38, 0",
        "Infinity, Infinity, Infinity",       // The sum of two infinities of the same sign is the infinity of that sign
        "-Infinity, -Infinity, -Infinity",    // The sum of two infinities of the same sign is the infinity of that sign
        "Infinity, 1.0247E+10, Infinity",     // The sum of an infinity and any finite value is equal to the infinity
        "-Infinity, -1.0247E+10, -Infinity",  // The sum of an infinity and any finite value is equal to the infinity
        "0, -0, 0",                           // The sum of two zeroes of opposite sign is positive zero
        "0, 0, 0",                            // The sum of two zeroes of the same sign is the zero of that sign
        "-0, -0, -0",                         // The sum of two zeroes of the same sign is the zero of that sign
        "0, 1.1, 1.1",                        // The sum of a zero and a nonzero finite value is equal to the nonzero value
        "1.123, 0, 1.123",                    // The sum of a zero and a nonzero finite value is equal to the nonzero value
        "3.4E+38, -3.4E+38, 0",               // The sum of two nonzero finite values of the same magnitude and opposite sign is positive zero
        "3.4E+38, 3.4E+38, Infinity",         // Magnitude too large, hence operation overflow results in an infinity of appropriate sign
        "-3.4E+38, -0.01E+38, -Infinity",     // Magnitude too large, hence operation overflow results in an infinity of appropriate sign
        "1.18E-46, 0, 0",                     // Magnitude too small, hence operation underflow results in a zero of appropriate sign
        "-1.18E-46, -0, -0"                   // Magnitude too small, hence operation underflow results in a zero of appropriate sign
    })
    public void testFAdd(float arg1, float arg2, float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    @Tag("add<FII>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1},{2}) = {3}")
    @CsvSource({
        "0.23, 0, 0, 0.23",
        "0.23, 1, 1, 2.23",
        "1.23, 0, 1, 2.23",
        "5.23, 10, 15, 30.23",
        "3.23, 1, -1, 3.23"
    })
    public void testFAdd3(float arg1, int arg2, int arg3, float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }

    @Tag("add<FFFFFFFFFF>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1},{2},{3},{4},{5},{6},{7},{8},{9}) = {10}")
    @CsvSource({
        "0.23, 0, 0, 0.23, 1, 2, 3, 4, 5, 6, 21.46"
    })
    public void testFAdd10(float arg1, float arg2, float arg3, float arg4, float arg5, float arg6, float arg7, float arg8, float arg9, float arg10, float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
        }
        assertEquals(expected, result);
    }

    @Tag("add<FFIIDDJJFFIIDDJJ>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1},{2},{3},{4},{5},{6},{7},{8},{9},{10},{11},{12},{13},{14},{15}) = {16}")
    @CsvSource({
        "5.23, 10, 15, 30, 2, 2, 1024, 1, 3, 4, 5, 6, 7, 8, 9, 10, 1141.23"
    })
    public void testFAdd16(float arg1, float arg2, int arg3, int arg4, double arg5, double arg6, long arg7, long arg8, float arg9, float arg10, int arg11, int arg12, double arg13, double arg14, long arg15, long arg16, float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15, arg16);
        }
        assertEquals(expected, result);
    }

    public static float add(float x, float y) {
        return x + y;
    }

    public static float add(float x, int y, int z) {
        return x + y + z;
    }

    public static float add(float x1, float x2, float x3, float x4, float x5, float x6, float x7, float x8, float x9, float x10) {
        return x1 + x2 + x3 + x4 + x5 + x6 + x7 + x8 + x9 + x10;
    }

    public static float add(float x1, float x2, int x3, int x4, double x5, double x6, long x7, long x8, float x9, float x10, int x11, int x12, double x13, double x14, long x15, long x16) {
        return x1 + x2 + x3 + x4 + (float)x5 + (float)x6 + x7 + x8 + x9 + x10 + x11 + x12 + (float)x13 + (float)x14 + x15 + x16;
    }

    @Tag("sub<FF>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - sub({0},{1}) = {2}")
    @CsvSource({
        "100, 40, 60",
        "1000, 43, 957",
        "0, 0, 0",
        "0, 1, -1",
        "1, 0, 1",
        "-1, -1, 0",
        "101, 2, 99",
        "3.4E+38, 3.4E+38, 0",
        "3.4E+38, -3.4E+38, Infinity",        // Magnitude too large, hence operation overflow results in an infinity of appropriate sign
        "-3.4E+38, -3.4E+38, 0",
        "-3.4E+38, 3.4E+38, -Infinity",       // Magnitude too large, hence operation overflow results in an infinity of appropriate sign
        "Infinity, -Infinity, Infinity",
        "Infinity, Infinity, NaN",
        "1.4E-45, 0, 1.4E-45",
        "1.4E-46, 0, 0",                      // Magnitude too small, hence operation underflow results in a zero
        "0, 0, 0",
        "0, -0, 0",
        "-0, 0, -0",
        "0, -1.18E-38, 1.18E-38",
        "0, 1.18E-38, -1.18E-38"
    })
    public void testFSub(float arg1, float arg2, float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = sub(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static float sub(float x, float y) {
        return x - y;
    }

    @Tag("mul<FF>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - mul({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, 0",
        "100, 40, 4000",
        "3.4E+38, 1,  3.4E+38",
        "3.4E+38, 0, 0",
        "-3.4E+38, 1, -3.4E+38",
        "-3.4E+38, 0, -0",
        "NaN, 10, NaN",                       // If either value1 or value2 is NaN, the result is NaN
        "1024, NaN, NaN",                     // If either value1 or value2 is NaN, the result is NaN
        "NaN, NaN, NaN",                      // If either value1 or value2 is NaN, the result is NaN
        "Infinity, 0, NaN",                   // Multiplication of an infinity by a zero results in NaN
        "Infinity, -1.18E-38, -Infinity",     // Multiplication of an infinity by a finite value results in a signed infinity
        "-Infinity, 1.18E-38, -Infinity",     // Multiplication of an infinity by a finite value results in a signed infinity
        "Infinity, 1.18E-38, Infinity",       // Multiplication of an infinity by a finite value results in a signed infinity
        "-Infinity, -1.18E-38, Infinity",     // Multiplication of an infinity by a finite value results in a signed infinity
        "-1.18E-38, 1.18E-38, -0",            // Magnitude too small, hence operation underflow results in a zero
        "3.4E+38, 3.4E+38, Infinity"          // Magnitude too large, hence operation overflow results in an infinity
    })
    public void testFMul(float arg1, float arg2, float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = mul(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static float mul(float x, float y) {
        return x * y;
    }

    @Tag("div<FF>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - div({0},{1}) = {2}")
    @CsvSource({
        "1.4E+8, 1, 1.4E+8",
        "1.4E+8, 100, 1.4E+6",
        "1.4E+8, -100, -1.4E+6",
        "-1.4E+8, 100, -1.4E+6",
        "-1.4E+8, -100, 1.4E+6",
        "1.4E+8, 2, 7.0E7",
        "3.4E+38, 3.4E+38, 1",
        "-3.4E+38, -3.4E+38, 1",
        "3.4E+38, 1, 3.4E+38",
        "NaN, 10E+20, NaN",                   // If either value1 or value2 is NaN, the result is NaN
        "10E+20, NaN, NaN",                   // If either value1 or value2 is NaN, the result is NaN
        "NaN, NaN, NaN",                      // If either value1 or value2 is NaN, the result is NaN
        "Infinity, Infinity, NaN",            // Division of an infinity by an infinity results in NaN
        "Infinity, -1.18E-38, -Infinity",     // Division of an infinity by a finite value results in a signed infinity
        "-Infinity, 1.18E-38, -Infinity",     // Division of an infinity by a finite value results in a signed infinity
        "Infinity, 1.18E-38, Infinity",       // Division of an infinity by a finite value results in a signed infinity
        "-Infinity, -1.18E-38, Infinity",     // Division of an infinity by a finite value results in a signed infinity
        "1.18E+38, Infinity, 0",              // Division of a finite value by an infinity results in a signed zero
        "1.18E+38, -Infinity, -0",            // Division of a finite value by an infinity results in a signed zero
        "0, 0, NaN",                          // Division of a zero by a zero results in NaN
        "0, 1.4E+8, 0",                       // Division of zero by any other finite value results in a signed zero
        "-1.4E+8, 0, -Infinity",              // Division of a nonzero finite value by a zero results in a signed infinity
        "-1.18E-38, 1.18E+38, -0",            // Magnitude too small, hence operation underflow results in a zero
        "3.4E+38, 3.4E-38, Infinity"          // Magnitude too large, hence operation overflow results in an infinity
    })
    public void testFDiv(float arg1, float arg2, float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = div(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static float div(float x, float y) {
        return x / y;
    }

    @Tag("rem<FF>F")
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
        "31.34, 2.2, 0.5399995",
        "0, 1, 0",
        "1000, 100, 0",
        "3.4E+38, 3.4E+38, 0",
        "-1.18E-38, -1, -1.18E-38",
        "-1.18E-38, -1.18E-38, -0",
        "1.4E-45, -1, 1.4E-45",
        "-16.3, 4.1, -3.9999995",
        "17.876543, 4, 1.876543",
        "17.8, 4, 1.7999992",
        "17.8, 4.1, 1.3999996",
        "-17.8, 4.1, -1.3999996",
        "17.8, -4.1, 1.3999996",
        "-17.8, -4.1, -1.3999996",
        "60984.1, -497.99, 229.32275",
        "-497.99, 60984.1, -497.99",

        "NaN, 10E+20, NaN",                   // If either value1
        "10E+20, NaN, NaN",                   // or value2 is NaN,
        "NaN, NaN, NaN",                      // the result is NaN

        "Infinity, -3.4E+38, NaN",            // If the dividend is an infinity
        "1.18E-38, 0, NaN",                   // or the divisor is a zero
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
    public void testFRem(float arg1, float arg2, float expected) {
        float result = 0F;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = rem(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static float rem(float x, float y) {
        return x % y;
    }

    @Tag("neg<F>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - neg({0}) = {1}")
    @CsvSource({
        "0, -0",                    // If the operand is a zero,
        "-0, 0",                    // the result is the zero of opposite sign.
        "NaN, NaN",                 // If the operand is NaN, the result is NaN.
        "Infinity, -Infinity",      // If the operand is an infinity,
        "-Infinity, Infinity",      // the result is the infinity of opposite sign.
        "1.482261, -1.482261",
        "-1.18E-38, 1.18E-38",
        "3.4E+38, -3.4E+38"
    })
    public void testFNeg(float arg1, float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = neg(arg1);
        }
        assertEquals(expected, result);
    }

    public static float neg(float x) {
        return -x;
    }

    @Tag("fconst_0<>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - fconst_0() = {0}")
    @CsvSource({
        "0"
    })
    public void testFConst_0(float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = fconst_0();
        }
        assertEquals(expected, result);
    }

    public static float fconst_0() {
        return 0;
    }

    @Tag("fconst_1<>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - fconst_1() = {0}")
    @CsvSource({
        "1"
    })
    public void testFConst_1(float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = fconst_1();
        }
        assertEquals(expected, result);
    }

    public static float fconst_1() {
        return 1;
    }

    @Tag("fconst_2<>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - fconst_2() = {0}")
    @CsvSource({
        "2"
    })
    public void testFConst_2(float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = fconst_2();
        }
        assertEquals(expected, result);
    }

    public static float fconst_2() {
        return 2;
    }

    @Tag("fcmpl<FF>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - fcmpl({0},{1}) = {2}")
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
        "Infinity, -Infinity, 1"
    })
    public void testFcmpl(float arg1, float arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = fcmpl(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int fcmpl(float x, float y) {
        return fcmp.fcmpl(x, y);
    }

    @Tag("fcmpg<FF>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - fcmpg({0},{1}) = {2}")
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
        "Infinity, -Infinity, 1"
    })
    public void testFcmpg(float arg1, float arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = fcmpg(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int fcmpg(float x, float y) {
        return fcmp.fcmpg(x, y);
    }

}
