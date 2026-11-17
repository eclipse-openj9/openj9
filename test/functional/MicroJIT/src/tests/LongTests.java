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

public class LongTests {

    @Tag("add<JJ>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1}) = {2}")
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
            result = add(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    @Tag("add<JJJ>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1},{2}) = {3}")
    @CsvSource({
        "0, 0, 0, 0",
        "0, 1, 1, 2",
        "1, 0, 1, 2",
        "5, 10, 15, 30",
        "3, 1, -1, 3"
    })
    public void testLAdd3(long arg1, long arg2, long arg3, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }

    @Tag("add<JII>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1},{2}) = {3}")
    @CsvSource({
        "0, 0, 0, 0",
        "0, 1, 1, 2",
        "1, 0, 1, 2",
        "5, 10, 15, 30",
        "3, 1, -1, 3"
    })
    public void testLAddJII(long arg1, int arg2, int arg3, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }

    @Tag("add<IJI>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1},{2}) = {3}")
    @CsvSource({
        "0, 0, 0, 0",
        "0, 1, 1, 2",
        "1, 0, 1, 2",
        "5, 10, 15, 30",
        "3, 1, -1, 3"
    })
    public void testLAddIJI(int arg1, long arg2, int arg3, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }

    @Tag("add<IIJ>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1},{2}) = {3}")
    @CsvSource({
        "0, 0, 0, 0",
        "0, 1, 1, 2",
        "1, 0, 1, 2",
        "5, 10, 15, 30",
        "3, 1, -1, 3"
    })
    public void testLAddIIJ(int arg1, int arg2, long arg3, long expected) {
        long result = 0L;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }

    @Tag("add<IJ>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, 1",
        "1, 0, 1",
        "2147483647, 1, 2147483648",
        "-2147483648, 2147483647, -1"
    })
    public void testIAdd(int arg1, long arg2, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    @Tag("add_<JII>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1},{2}) = {3}")
    @CsvSource({
        "0, 0, 0, 0",
        "0, 1, 1, 2",
        "1, 0, 1, 2",
        "5, 10, 15, 30",
        "3, 1, -1, 3"
    })
    public void testIAdd(long arg1, int arg2, int arg3, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add_(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }

    public static long add(long x, long y) {
        return x + y;
    }

    public static long add(int x, long y) {
        return x + y;
    }

    public static long add(long x, long y, long z) {
        return x + y + z;
    }

    public static int add_(long x, int y, int z) {
        return (int)x + y + z;
    }

    public static long add(long x, int y, int z) {
        return x + y + z;
    }

    public static long add(int x, long y, int z) {
        return x + y + z;
    }

    public static long add(int x, int y, long z) {
        return x + y + z;
    }

    @Tag("sub<JJ>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - sub({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, -1",
        "1, 0, 1",
        "-1, -1, 0",
        "101, 2, 99",
        "9223372036854775807, 1, 9223372036854775806",
        "-9223372036854775808, 1, 9223372036854775807",
        "9223372036854775807, -1, -9223372036854775808",
        "2,-9223372036854775807, -9223372036854775807"
    })
    public void testLSub(long arg1, long arg2, long expected) {
        long result = 0L;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = sub(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static long sub(long x, long y) {
        return x - y;
    }

    @Tag("mul<JJ>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - mul({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, 0",
        "10, 10, 100",
        "10, -10, -100",
        "-10, 10, -100",
        "-10, -10, 100",
        "9223372036854775807, 1,  9223372036854775807",
        "9223372036854775807, 0, 0",
        "-9223372036854775808, 1, -9223372036854775808"
    })
    public void testLMul(long arg1, long arg2, long expected) {
        long result = 0L;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = mul(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static long mul(long x, long y) {
        return x * y;
    }

    @Tag("div<JJ>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - div({0},{1}) = {2}")
    @CsvSource({
        "0, 1, 0",
        "100, 1, 100",
        "1000, 100, 10",
        "1000, -100, -10",
        "-1000, 100, -10",
        "-1000, -100, 10",
        "1000, 20, 50",
        "-256, 4, -64",
        "9223372036854775807, 9223372036854775807, 1",
        "-9223372036854775808, -9223372036854775808, 1",
        "9223372036854775807, 1, 9223372036854775807",

        /* If the dividend is the negative integer of largest possible magnitude
        for the long type and the divisor is -1, then overflow occurs and the
        result is equal to the dividend; despite the overflow, no exception
        is thrown in this case. */
        "-9223372036854775808, -1, -9223372036854775808"
    })
    public void testLDiv(long arg1, long arg2, long expected) {
        long result = 0L;
        for (int i = 0; i < Helper.invocations(); i++) {
        result = div(arg1, arg2);
        }
        assertEquals(expected, result);
    }

// Test commented out until we support exceptions like this
/*
    @Test
    @DisplayName("div(10,0)=divide by zero")
    public void testLDivBy0() {
        long result = 0L;
        for (int i = 0; i < Helper.invocations(); i++) {
            try {
                result = div(10L,0L);
            }
            catch (java.lang.ArithmeticException e) {
                assertEquals("divide by zero", e.getMessage());
            }
        }
    }
*/

    public static long div(long x, long y) {
        return x / y;
    }

    @Tag("rem<JJ>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - rem({0},{1}) = {2}")
    @CsvSource({
        "0, 1, 0",
        "1000, 100, 0",
        "29, 8, 5",
        "-29, 8, -5",
        "29, -8, 5",
        "-29, -8, -5",
        "9223372036854775807, 9223372036854775807, 0",
        "9223372036854775807, -1, 0",
        "-9223372036854775808, -9223372036854775808, 0",
        "-9223372036854775808, -1, 0"
    })
    public void testLRem(long arg1, long arg2, long expected) {
        long result = 0L;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = rem(arg1, arg2);
        }
        assertEquals(expected, result);
    }

// Test commented out until we support exceptions like this
/*
    @Test
    @DisplayName("rem(10,0)=divide by zero")
    public void testLRemBy0() {
        long result = 0L;
        for (int i = 0; i < Helper.invocations(); i++) {
            try {
                result = rem(10L,0L);
            }
            catch (java.lang.ArithmeticException e) {
                assertEquals("divide by zero", e.getMessage());
            }
        }
    }
*/

    public static long rem(long x, long y) {
        return x % y;
    }

    @Tag("neg<J>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - neg({0}) = {1}")
    @CsvSource({
        "1000, -1000",
        "100, -100",
        "-59, 59",
        "9223372036854775807, -9223372036854775807",
        "-9223372036854775808, -9223372036854775808",   // JVM Section 6.5: Negation of the maximum negative long results in that same maximum negative number
        "-1, 1",
        "3, -3",
        "0, 0"
    })
    public void testLNeg(long arg1, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = neg(arg1);
        }
        assertEquals(expected, result);
    }

    public static long neg(long x) {
        return -x;
    }

    @Tag("shl<JI>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - shl({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 63, 0",
        "1, 0, 1",
        "-1, 0, -1",
        "-1, 63, -9223372036854775808",
        "1, 63, -9223372036854775808",
        "1, 62, 4611686018427387904",
        "16, 2, 64",
        "-16, 2, -64",
        "1000000, 43, 8796093022208000000",
        "-1000000, 43, -8796093022208000000",
        "9223372036854775807, 0, 9223372036854775807",
        "-9223372036854775808, 0, -9223372036854775808"
    })
    public void testLShl(long arg1, int arg2, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = shl(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static long shl(long x, int y) {
        return x << y;
    }

    @Tag("shr<JI>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - shr({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 63, 0",
        "1, 0, 1",
        "-1, 0, -1",
        "1, 63, 0",
        "-1, 63, -1",
        "256, 2, 64",
        "-256, 2, -64",
        "100000000000, 20, 95367",
        "-100000000000, 20, -95368",
        "100000000000, 50, 0",
        "-100000000000, 50, -1",
        "9223372036854775807, 0, 9223372036854775807",
        "-9223372036854775808, 0, -9223372036854775808",
        "9223372036854775807, 1, 4611686018427387903",
        "-9223372036854775807, 1, -4611686018427387904",
        "-9223372036854775808, 1, -4611686018427387904",
        "-9223372036854775808, 63, -1",
        "9223372036854775807, 63, 0"
    })
    public void testLShr(long arg1, int arg2, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = shr(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static long shr(long x, int y) {
        return x >> y;
    }

    @Tag("ushr<JI>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - ushr({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 63, 0",
        "1, 0, 1",
        "-1, 0, -1",
        "1, 63, 0",
        "-1, 63, 1",
        "256, 2, 64",
        "-256, 2, 4611686018427387840",
        "100000000000, 20, 95367",
        "-100000000000, 20, 17592185949048",
        "100000000000, 50, 0",
        "-100000000000, 50, 16383",
        "9223372036854775807, 0, 9223372036854775807",
        "-9223372036854775808, 0, -9223372036854775808",
        "9223372036854775807, 1, 4611686018427387903",
        "-9223372036854775807, 1, 4611686018427387904",
        "-9223372036854775808, 1, 4611686018427387904",
        "-9223372036854775808, 63, 1",
        "9223372036854775807, 63, 0"
    })
    public void testLUshr(long arg1, int arg2, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = ushr(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static long ushr(long x, int y) {
        return x >>> y;
    }

    @Tag("and<JJ>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - and({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, 0",
        "1, 0, 0",
        "1, 1, 1",
        "-101, 101, 1",
        "9223372036854775807, 9223372036854775807, 9223372036854775807",
        "-9223372036854775808, 9223372036854775807, 0",
        "9223372036854775807, 1, 1",
        "2, 9223372036854775807, 2"
    })
    public void testLAnd(long arg1, long arg2, long expected) {
        long result = 0L;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = and(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    @Tag("and<JJJ>J")
    @Order(1)
    @Test
    @DisplayName("and(10,10,10)=10")
    public void testLAnd3() {
        long result = 0L;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = and(10L, 10L, 10L);
        }
        assertEquals(10L, result);
    }

    public static long and(long x, long y) {
        return x & y;
    }

    public static long and(long x, long y, long z) {
        return x & y & z;
    }

    @Tag("or<JJ>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - or({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, 1",
        "1, 0, 1",
        "1, 1, 1",
        "-101, 101, -1",
        "9223372036854775807, 9223372036854775807, 9223372036854775807",
        "-9223372036854775808, 9223372036854775807, -1",
        "9223372036854775807, 1, 9223372036854775807",
        "2, 9223372036854775807, 9223372036854775807"
    })
    public void testLOr(long arg1, long arg2, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = or(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static long or(long x, long y) {
        return x | y;
    }

    @Tag("xor<JJ>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - xor({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 1, 1",
        "1, 0, 1",
        "1, 1, 0",
        "-101, 101, -2",
        "9223372036854775807, 9223372036854775807, 0",
        "-9223372036854775808, 9223372036854775807, -1",
        "9223372036854775807, 1, 9223372036854775806",
        "2, 9223372036854775807, 9223372036854775805"
    })
    public void testLXor(long arg1, long arg2, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = xor(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static long xor(long x, long y) {
        return x ^ y;
    }

    @Tag("lconst_0<>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - lconst_0() = {0}")
    @CsvSource({
        "0"
    })
    public void testLConst_0(long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = lconst_0();
        }
        assertEquals(expected, result);
    }

    public static long lconst_0() {
        return (long)0;
    }

    @Tag("lconst_1<>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - lconst_1() = {0}")
    @CsvSource({
        "1"
    })
    public void testLConst_1(long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = lconst_1();
        }
        assertEquals(1, result);
    }

    public static long lconst_1() {
        return (long)1;
    }

    @Tag("lcmp<JJ>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - lcmp({0}, {1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "1, 0, 1",
        "0, 1, -1",
        "1, 1, 0",
        "45, 26, 1",
        "26, 45, -1",
        "7, 7, 0",
        "9223372036854775807, -1, 1",
        "9223372036854775807, 0, 1"
    })
    public void test_lcmp(long arg1, long arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = lcmp(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int lcmp(long a, long b) {
        return lcmp.lcmp(a, b);
    }

}
