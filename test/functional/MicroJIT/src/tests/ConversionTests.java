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

public class ConversionTests {

    @Tag("i2l<I>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - i2l({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 1",
        "2147483647, 2147483647",
        "-2147483648, -2147483648"
    })
    public void testI2L(int arg1, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = i2l(arg1);
        }
        assertEquals(expected, result);
    }

    public static long i2l(int x) {
        return (long)x;
    }

    @Tag("l2i<J>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - l2i({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 1",
        "9223372036854775807, -1",
        "-9223372036854775808, 0"
    })
    public void testL2I(long arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = l2i(arg1);
        }
        assertEquals(expected, result);
    }

    public static int l2i(long x) {
        return (int)x;
    }

    @Tag("i2b<I>B")
    @Order(1)
    @ParameterizedTest(name = "#{index} - i2b({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 1",
        "2147483647, -1",
        "-2147483648, 0"
    })
    public void testI2B(int arg1, byte expected) {
        byte result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = i2b(arg1);
        }
        assertEquals(expected, result);
    }

    public static byte i2b(int x) {
        return (byte)x;
    }

    @Tag("i2s<I>S")
    @Order(1)
    @ParameterizedTest(name = "#{index} - i2s({0}) = {1}")
    @CsvSource({
        "-1, -1",
        "0, 0",
        "1, 1",
        "2147483647, -1",
        "-2147483648, 0",
        "1050656, 2080"
    })
    public void testI2S(int arg1, short expected) {
        short result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = i2s(arg1);
        }
        assertEquals(expected, result);
    }

    public static short i2s(int x) {
        return (short)x;
    }

    @Tag("i2c<I>C")
    @Order(1)
    @ParameterizedTest(name = "#{index} - i2c({0}) = {1}")
    @CsvSource({
        "68, \u0044",
        "70, \u0046",
        "68, \u0044",
        "68, \u0044"
    })
    public void testI2C(int arg1, char expected) {
        char result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = i2c(arg1);
        }
        assertEquals(expected, result);
    }

    public static char i2c(int x) {
        return (char)x;
    }

    @Tag("i2d<I>D")
    @Order(1)
    @ParameterizedTest(name = "#{index} - i2d({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 1",
        "2147483647, 2147483647",
        "-2147483648, -2147483648"
    })
    public void testI2D(int arg1, double expected) {
        double result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = i2d(arg1);
        }
        assertEquals(expected, result);
    }

    public static double i2d(int x) {
        return (double)x;
    }

    @Tag("l2d<J>D")
    @Order(1)
    @ParameterizedTest(name = "#{index} - l2d({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 1",
        "9223372036854775807, 9223372036854775807",
        "-9223372036854775808, -9223372036854775808"
    })
    public void testL2D(long arg1, double expected) {
        double result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = l2d(arg1);
        }
        assertEquals(expected, result);
    }

    public static double l2d(long x) {
        return (double)x;
    }

    @Tag("d2i<D>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - d2i({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 1",
        "9223372036854775807, 2147483647",
        "2147483648, 2147483647",
        "NaN, 0",                                           // If the value' is NaN, the result of the conversion is an int 0
        "309.9999999, 309",                                 // Rounding towards zero using IEEE 754 round towards zero mode
        "Infinity, 2147483647",                             // Result is the largest representable value of type int
        "-Infinity, -2147483648",                           // Result is the smallest representable value of type int
        "2147483647.5678, 2147483647",
        "-9223372036854775808, -2147483648"
    })
    public void testD2I(double arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = d2i(arg1);
        }
        assertEquals(expected, result);
    }

    public static int d2i(double x) {
        return (int)x;
    }

    @Tag("d2l<D>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - d2l({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 1",
        "9223372036854775807, 9223372036854775807",
        "1.147483648, 1",
        "309.999, 309",                                         // Rounding towards zero using IEEE 754 round towards zero mode
        "NaN, 0",                                               // If the value' is NaN, the result of the conversion is a long 0
        "-Infinity, -9223372036854775808",                      // Result is the smallest representable value of type long
        "Infinity, 9223372036854775807",                        // Result is the largest representable value of type long
        "-9223372036854775808, -9223372036854775808"
    })
    public void testD2L(double arg1, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = d2l(arg1);
        }
        assertEquals(expected, result);
    }

    public static long d2l(double x) {
        return (long)x;
    }

    @Tag("i2f<I>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - i2f({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 1",
        "-1, -1",
        "2147483647, 2.14748365E9",
        "-2147483648, -2.14748365E9"
    })
    public void testI2F(int arg1, float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = i2f(arg1);
        }
        assertEquals(expected, result);
    }

    public static float i2f(int x) {
        return (float)x;
    }

    @Tag("f2i<F>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - f2i({0}) = {1}")
    @CsvSource({
        "0, 0",
        "-0, 0",
        "1, 1",
        "-1, -1",
        "9223372036854775807, 2147483647",
        "-9223372036854775807, -2147483648",
        "2147483648, 2147483647",
        "-2147483648, -2147483648",
        "3.4E+38, 2147483647",
        "1.18E-38, 0",
        "-1.18E-38, 0",
        "1.4E-45, 0",
        "NaN, 0",                                           // If the value' is NaN, the result of the conversion is an int 0
        "309.9999, 309",                                    // Rounding towards zero using IEEE 754 round towards zero mode
        "-309.9999, -309",                                  // Rounding towards zero using IEEE 754 round towards zero mode
        "Infinity, 2147483647",                             // Result is the largest representable value of type int
        "-Infinity, -2147483648",                           // Result is the smallest representable value of type int
        "2147483650.5678, 2147483647",
        "-2147483650.5678, -2147483648",
        "2147483650.1234, 2147483647",
        "-2147483650.1234, -2147483648",
        "9223372036854775808, 2147483647",
        "-9223372036854775808, -2147483648"
    })
    public void testF2I(float arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = f2i(arg1);
        }
        assertEquals(expected, result);
    }

    public static int f2i(float x) {
        return (int)x;
    }

    @Tag("l2f<J>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - l2f({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 1",
        "-1, -1",
        "2147483647, 2.14748365E9",                 // Loss of precision
        "-2147483648, -2.14748365E9",               // because values of
        "9223372036854775807, 9.223372E18",         // type float have
        "-9223372036854775808, -9.223372E18"        // only 24 significand bits
    })
    public void testL2F(long arg1, float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = l2f(arg1);
        }
        assertEquals(expected, result);
    }

    public static float l2f(long x) {
        return (float)x;
    }

    @Tag("f2l<F>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - f2l({0}) = {1}")
    @CsvSource({
        "0, 0",
        "-0, 0",
        "1, 1",
        "-1, -1",
        "9223372036854775807, 9223372036854775807",
        "-9223372036854775807, -9223372036854775808",
        "9223372036854775808, 9223372036854775807",
        "-9223372036854775808, -9223372036854775808",
        "2147483647, 2147483648",
        "-2147483648, -2147483648",
        "3.4E+38, 9223372036854775807",
        "-3.4E+38, -9223372036854775808",
        "1.18E-38, 0",
        "-1.18E-38, 0",
        "1.4E-45, 0",
        "NaN, 0",                                           // If the value' is NaN, the result of the conversion is a long 0
        "309.9999, 309",                                    // Rounding towards zero using IEEE 754 round towards zero mode
        "-309.9999, -309",                                  // Rounding towards zero using IEEE 754 round towards zero mode
        "Infinity, 9223372036854775807",                    // Result is the largest representable value of type long
        "-Infinity, -9223372036854775808"                   // Result is the smallest representable value of type long
    })
    public void testF2L(float arg1, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = f2l(arg1);
        }
        assertEquals(expected, result);
    }

    public static long f2l(float x) {
        return (long)x;
    }

    @Tag("d2f<D>F")
    @Order(1)
    @ParameterizedTest(name = "#{index} - d2f({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 1",
        "-4.94E-324, -0",           // A finite value too small to be represented as a float is converted to a zero of the same sign
        "1.79E+308, Infinity",      // A finite value too large to be represented as a float is converted to an infinity of the same sign
        "NaN, NaN",                 // A double NaN is converted to a float NaN
        "1.18E-38, 1.18E-38",
        "1.4E-45, 1.4E-45",
        "1.4E-46, 0"
    })
    public void testD2F(double arg1, float expected) {
        float result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = d2f(arg1);
        }
        assertEquals(expected, result);
    }

    public static float d2f(double x) {
        return (float)x;
    }

    @Tag("f2d<F>D")
    @Order(1)
    @ParameterizedTest(name = "#{index} - f2d({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 1",
        "1.18E-38, 1.179999945774631E-38",  // Because f2d conversions are exact
        "1.4E-45, 1.401298464324817E-45",   // Because f2d conversions are exact
        "3.4E+38, 3.3999999521443642E38"    // Because f2d conversions are exact
    })
    public void testF2D(float arg1, double expected) {
        double result = 0D;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = f2d(arg1);
        }
        assertEquals(expected, result);
    }

    public static double f2d(float x) {
        return (double)x;
    }

}
