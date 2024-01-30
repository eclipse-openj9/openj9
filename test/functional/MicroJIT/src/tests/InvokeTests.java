/*******************************************************************************
 * Copyright (c) 2023, 2023 IBM Corp. and others
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

public class InvokeTests {

    @Tag("indirectAdd<II>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - add({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, 1",
        "1, 0, 1",
        "2147483647, 2147483647, -2",
        "-2147483648, 2147483647, -1"
    })
    public void _testIAdd(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectAdd(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int indirectAdd(int a, int b) {
        return IntTests.add(a, b);
    }

    @Tag("indirectSub<II>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - sub({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, -1",
        "1, 0, 1",
        "-1, -1, 0",
        "2147483647, 2147483647, 0",
        "-2147483648, 2147483647, 1"
    })
    public void testISub(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectSub(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int indirectSub(int a, int b) {
        return IntTests.sub(a, b);
    }

    @Tag("indirectMul<II>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - mul({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "10, 10, 100",
        "10, -10, -100",
        "-10, 10, -100",
        "-10, -10, 100",
        "2147483647, 1,  2147483647",
        "2147483647, 0, 0",
        "0, 1, 0"
    })
    public void testIMul(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectMul(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int indirectMul(int a, int b) {
        return IntTests.mul(a, b);
    }

    @Tag("indirectDiv<II>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - div({0},{1}) = {2}")
    @CsvSource({
        "1000, 100, 10",
        "1000, -100, -10",
        "-1000, 100, -10",
        "-1000, -100, 10",
        "2147483647, 2147483647, 1",
        "-2147483648, -2147483648, 1",
        "0, 1, 0"
    })
    public void testIDiv(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectDiv(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int indirectDiv(int x, int y) {
        return IntTests.div(x, y);
    }

    @Tag("indirectLAdd<JJ>J")
    @Order(2)
    @ParameterizedTest(name = "#{index} - indirectLAdd({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, 1",
        "1, 0, 1",
        "2147483647, 2147483647, 4294967294",
        "-2147483648, 2147483647, -1"
    })
    public void _testLAdd(long arg1, long arg2, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectLAdd(arg1, arg2);
        }
        assertEquals(expected, result);
    }
    public static long indirectLAdd(long a, long b) {
        return LongTests.add(a, b);
    }

    @Tag("indirectFAdd<FF>F")
    @Order(2)
    @ParameterizedTest(name = "#{index} - indirectFAdd({0},{1}) = {2}")
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
    public void _testFAdd(float arg1, float arg2, float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectFAdd(arg1, arg2);
        }
        assertEquals(expected, result);
    }
    public static float indirectFAdd(float a, float b) {
        return FloatTests.add(a, b);
    }

    @Tag("indirectDAdd<DD>D")
    @Order(2)
    @ParameterizedTest(name = "#{index} - indirectDAdd({0},{1}) = {2}")
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
    public void _testDAdd(double arg1, double arg2, double expected) {
        double result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectDAdd(arg1, arg2);
        }
        assertEquals(expected, result);
    }
    public static double indirectDAdd(double a, double b) {
        return DoubleTests.add(a, b);
    }

    @Tag("indirectAdd<IIII>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - indirectAdd({0},{1},{2},{3}) = {4}")
    @CsvSource({
        "0, 0, 0, 0, 0",
        "0, 1, 1, 1, 3",
        "1, 0, 1, 3, 5",
        "5, 10, 15, 5, 35",
        "3, 1, -1, 0, 3"
    })
    public void _testAddFour(int arg1, int arg2, int arg3, int arg4, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectAdd(arg1, arg2, arg3, arg4);
        }
        assertEquals(expected, result);
    }
    public static int indirectAdd(int a, int b, int c, int d) {
        return IntTests.add(a, b, c, d);
    }

    @Tag("indirectAdd<III>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - indirectAdd({0},{1},{2}) = {3}")
    @CsvSource({
        "0, 0, 0, 0",
        "0, 1, 1, 2",
        "1, 0, 1, 2",
        "5, 10, 15, 30",
        "3, 1, -1, 3"
    })
    public void _testAddThree(int arg1, int arg2, int arg3, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectAdd(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }
    public static int indirectAdd(int a, int b, int c) {
        return IntTests.add(a, b, c);
    }

    @Tag("indirectAdd<IJ>J")
    @Order(2)
    @ParameterizedTest(name = "#{index} - add({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, 1",
        "1, 0, 1",
        "2147483647, 1, 2147483648",
        "-2147483648, 2147483647, -1"
    })
    public void _testIAdd(int arg1, long arg2, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectAdd(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static long indirectAdd(int a, long b) {
        return LongTests.add(a, b);
    }

    @Tag("indirectAdd<IIJ>J")
    @Order(2)
    @ParameterizedTest(name = "#{index} - indirectAdd({0},{1},{2}) = {3}")
    @CsvSource({
        "0, 0, 0, 0",
        "0, 1, 1, 2",
        "1, 0, 1, 2",
        "5, 10, 15, 30",
        "3, 1, -1, 3"
    })
    public void _testAddThree(int arg1, int arg2, long arg3, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectAdd(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }
    public static long indirectAdd(int a, int b, long c) {
        return LongTests.add(a, b, c);
    }

    @Tag("indirectAdd<JII>J")
    @Order(2)
    @ParameterizedTest(name = "#{index} - indirectAdd({0},{1},{2}) = {3}")
    @CsvSource({
        "0, 0, 0, 0",
        "0, 1, 1, 2",
        "1, 0, 1, 2",
        "5, 10, 15, 30",
        "3, 1, -1, 3"
    })
    public void _testAddThree(long arg1, int arg2, int arg3, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectAdd(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }
    public static long indirectAdd(long a, int b, int c) {
        return LongTests.add(a, b, c);
    }

    @Tag("indirectAdd_<JII>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - indirectAdd({0},{1},{2}) = {3}")
    @CsvSource({
        "0, 0, 0, 0",
        "0, 1, 1, 2",
        "1, 0, 1, 2",
        "5, 10, 15, 30",
        "3, 1, -1, 3"
    })
    public void _testAddThree(long arg1, int arg2, int arg3, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = indirectAdd_(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }
    public static int indirectAdd_(long a, int b, int c) {
        return LongTests.add_(a, b, c);
    }

}
