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

public class GetPutStaticTests {

    static int x = -1;
    static int y = 1;

    static long a = -1;
    static long b = 1;

    static float m = -1;
    static float n = 1;

    static double s = -1;
    static double t = 1;

    @Tag("add<>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - add({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, 1",
        "1, 0, 1",
        "2147483647, 2147483647, -2",
        "-2147483648, 2147483647, -1"
    })
    public void testIAdd(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            setInts(arg1, arg2);
            result = add();
        }
        assertEquals(expected, result);
    }

    public static int add() {
        return x + y;
    }

    @Tag("sub<>I")
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
            setInts(arg1, arg2);
            result = sub();
        }
        assertEquals(expected, result);
    }

    public static int sub() {
        return x - y;
    }

    @Tag("mul<>I")
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
            setInts(arg1, arg2);
            result = mul();
        }
        assertEquals(expected, result);
    }

    public static int mul() {
        return x * y;
    }

    @Tag("div<>I")
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
            setInts(arg1, arg2);
            result = div();
        }
        assertEquals(expected, result);
    }

    public static int div() {
        return x / y;
    }

    @Tag("and<>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - and({0},{1}) = {2}")
    @CsvSource({
        "1, 1, 1",
        "1, 0, 0",
        "0, 1, 0",
        "5, 10, 0",
        "5, 5, 5",
        "3, 5, 1",
        "5743251, 0, 0"
    })
    public void testIAnd(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            setInts(arg1, arg2);
            result = and();
        }
        assertEquals(expected, result);
    }

    public static int and() {
        return x & y;
    }

    @Tag("or<>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - or({0},{1}) = {2}")
    @CsvSource({
        "1, 1, 1",
        "1, 0, 1",
        "0, 1, 1",
        "5, 10, 15",
        "5, 5, 5",
        "3, 5, 7",
        "5743251, 0, 5743251"
    })
    public void testIOr(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            setInts(arg1, arg2);
            result = or();
        }
        assertEquals(expected, result);
    }

    public static int or() {
        return x | y;
    }

    @Tag("xor<>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - xor({0},{1}) = {2}")
    @CsvSource({
        "1, 1, 0",
        "1, 0, 1",
        "0, 1, 1",
        "5, 10, 15",
        "5, 5, 0",
        "3, 5, 6",
        "5743251, 0, 5743251"
    })
    public void testIXor(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            setInts(arg1, arg2);
            result = xor();
        }
        assertEquals(expected, result);
    }

    public static int xor() {
        return x ^ y;
    }

    @Tag("addLong<>J")
    @Order(2)
    @ParameterizedTest(name = "#{index} - addLong({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, 1",
        "1, 0, 1",
        "101, -2, 99",
        "-101, 101, 0",
        "9223372036854775807, 9223372036854775807, -2",
        "-9223372036854775808, 9223372036854775807, -1",
        "9223372036854775807, 1, -9223372036854775808",
        "2, 9223372036854775807, -9223372036854775807"
    })
    public void testLAdd(long arg1, long arg2, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            setLongs(arg1, arg2);
            result = addLong();
        }
        assertEquals(expected, result);
    }

    public static long addLong() {
        return a + b;
    }

    @Tag("addFloat<>F")
    @Order(2)
    @ParameterizedTest(name = "#{index} - addFloat({0},{1}) = {2}")
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
            setFloats(arg1, arg2);
            result = addFloat();
        }
        assertEquals(expected, result);
    }

    public static float addFloat() {
        return m + n;
    }

    @Tag("addDouble<>D")
    @Order(2)
    @ParameterizedTest(name = "#{index} - addDouble({0},{1}) = {2}")
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
        double result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            setDoubles(arg1, arg2);
            result = addDouble();
        }
        assertEquals(expected, result);
    }

    public static double addDouble() {
        return s + t;
    }

    @Tag("setInts<II>V")
    @Order(1)
    @ParameterizedTest(name = "#{index} - setInts({0},{0})")
    @CsvSource({
        "1",
        "100",
        "-5",
        "5743251"
    })
    public void testSetInts(int arg1) {
        boolean result = false;
        for (int i = 0; i < Helper.invocations(); i++) {
            setInts(arg1, arg1);
            result = (x == y);
        }
        assertEquals(true, result);
    }

    public static void setInts(int arg1, int arg2) {
        x = arg1;
        y = arg2;
    }

    @Tag("setLongs<JJ>V")
    @Order(1)
    @ParameterizedTest(name = "#{index} - setLongs({0},{0})")
    @CsvSource({
        "1",
        "100",
        "-5",
        "5743251"
    })
    public void testSetLongs(long arg1) {
        boolean result = false;
        for (int i = 0; i < Helper.invocations(); i++) {
            setLongs(arg1, arg1);
            result = (a == b);
        }
        assertEquals(true, result);
    }

    public static void setLongs(long arg1, long arg2) {
        a = arg1;
        b = arg2;
    }

    @Tag("setFloats<FF>V")
    @Order(1)
    @ParameterizedTest(name = "#{index} - setFloats({0},{0})")
    @CsvSource({
        "1",
        "100",
        "-5",
        "5743251",
        "99.26485999",
        "101.26485999",
        "3.4E+38",
        "Infinity"
    })
    public void testSetFloats(float arg1) {
        boolean result = false;
        for (int i = 0; i < Helper.invocations(); i++) {
            setFloats(arg1, arg1);
            result = (m == n);
        }
        assertEquals(true, result);
    }

    public static void setFloats(float arg1, float arg2) {
        m = arg1;
        n = arg2;
    }

    @Tag("setDoubles<DD>V")
    @Order(1)
    @ParameterizedTest(name = "#{index} - setDoubles({0},{0})")
    @CsvSource({
        "1",
        "100",
        "-5",
        "5743251"
    })
    public void testSetDoubles(double arg1) {
        boolean result = false;
        for (int i = 0; i < Helper.invocations(); i++) {
            setDoubles(arg1, arg1);
            result = (s == t);
        }
        assertEquals(true, result);
    }

    public static void setDoubles(double arg1, double arg2) {
        s = arg1;
        t = arg2;
    }

}
