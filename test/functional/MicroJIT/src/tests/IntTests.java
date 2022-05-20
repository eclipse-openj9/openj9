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

public class IntTests {

    @Tag("add<II>I")
    @Order(1)
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
            result = add(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    @Tag("add<III>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1},{2}) = {3}")
    @CsvSource({
        "0, 0, 0, 0",
        "0, 1, 1, 2",
        "1, 0, 1, 2",
        "5, 10, 15, 30",
        "3, 1, -1, 3"
    })
    public void testAddThree(int arg1, int arg2, int arg3, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }

    @Tag("add<IIII>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - add({0},{1},{2},{3}) = {4}")
    @CsvSource({
        "0, 0, 0, 0, 0",
        "0, 1, 1, 1, 3",
        "1, 0, 1, 3, 5",
        "5, 10, 15, 5, 35",
        "3, 1, -1, 0, 3"
    })
    public void testIAddFour(int arg1, int arg2, int arg3, int arg4, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = add(arg1, arg2, arg3, arg4);
        }
        assertEquals(expected, result);
    }

    public static int add(int x, int y) {
        return x + y;
    }

    public static int add(int x, int y, int z) {
        return x + y + z;
    }

    public static int add(int w, int x, int y, int z) {
        return w + x + y + z;
    }

    @Tag("sub<II>I")
    @Order(1)
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
            result = sub(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int sub(int x, int y) {
        return x - y;
    }

    @Tag("mul<II>I")
    @Order(1)
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
            result = mul(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int mul(int x, int y) {
        return x * y;
    }

    @Tag("div<II>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - div({0},{1}) = {2}")
    @CsvSource({
        "1000, 100, 10",
        "1000, -100, -10",
        "-1000, 100, -10",
        "-1000, -100, 10",
        "2147483647, 2147483647, 1",
        "-2147483648, -2147483648, 1",
        "0, 1, 0",

        /* If the dividend is the negative integer of largest possible magnitude
        for the int type and the divisor is -1, then overflow occurs and the
        result is equal to the dividend; despite the overflow, no exception
        is thrown in this case. */
        "-2147483648, -1, -2147483648"
    })
    public void testIDiv(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = div(arg1, arg2);
        }
        assertEquals(expected, result);
    }

// Test commented out until we support exceptions like this
/*
    @Test
    @DisplayName("div(10,0)=divide by zero")
    public void testIDivBy0() {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            try {
                result = div(10,0);
            }
            catch (java.lang.ArithmeticException e) {
                assertEquals("divide by zero", e.getMessage());
            }
        }
    }
*/

    public static int div(int x, int y) {
        return x / y;
    }

    @Tag("rem<II>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - rem({0},{1}) = {2}")
    @CsvSource({
        "0, 1, 0",
        "1000, 100, 0",
        "29, 8, 5",
        "-29, 8, -5",
        "29, -8, 5",
        "-29, -8, -5",
        "2147483647, 2147483647, 0",
        "2147483647, -1, 0",
        "-2147483648, -2147483648, 0",
        "-2147483648, -1, 0"
    })
    public void testIRem(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = rem(arg1, arg2);
        }
        assertEquals(expected, result);
    }

// Test commented out until we support exceptions like this
/*
    @Test
    @DisplayName("rem(10,0)=divide by zero")
    public void testIRemBy0() {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            try {
                result = rem(10,0);
            }
            catch (java.lang.ArithmeticException e) {
                assertEquals("divide by zero", e.getMessage());
            }
        }
    }
*/

    public static int rem(int x, int y) {
        return x % y;
    }

    @Tag("inc<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - inc({0}) = {1}")
    @CsvSource({
        "0, 3",
        "-1, 2",
        "10, 13",
        "2147483640, 2147483643",
        "2147483647, -2147483646",
        "-2147483648, -2147483645"
    })
    public void testIInc(int arg, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = inc(arg);
        }
        assertEquals(expected, result);
    }

    public static int inc(int x) {
        int v = x;
        v += 3;
        return v;
    }

    @Tag("neg<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - neg({0}) = {1}")
    @CsvSource({
        "1000, -1000",
        "100, -100",
        "-59, 59",
        "2147483647, -2147483647",
        "-2147483648, -2147483648",   // JVM Section 6.5: Negation of the maximum negative int results in that same maximum negative number
        "3, -3",
        "0, 0"
    })
    public void testINeg(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = neg(arg1);
        }
        assertEquals(expected, result);
    }

    public static int neg(int x) {
        return -x;
    }

    @Tag("shl<II>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - shl({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 31, 0",
        "1, 0, 1",
        "-1, 0, -1",
        "-1, 31, -2147483648",
        "1, 31, -2147483648",
        "16, 2, 64",
        "-16, 2, -64",
        "1000, 20, 1048576000",
        "-1000, 20, -1048576000",
        "1000, 21, 2097152000",
        "-1000, 21, -2097152000",
        "2147483647, 0, 2147483647",
        "-2147483648, 0, -2147483648"
    })
    public void testIShl(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = shl(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int shl(int x, int y) {
        return x << y;
    }

    @Tag("shr<II>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - shr({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 31, 0",
        "1, 0, 1",
        "-1, 0, -1",
        "1, 31, 0",
        "-1, 31, -1",
        "256, 2, 64",
        "-256, 2, -64",
        "1000, 20, 0",
        "-1000, 20, -1",
        "1000, 5, 31",
        "-1000, 5, -32",
        "2147483647, 0, 2147483647",
        "-2147483648, 0, -2147483648",
        "2147483647, 1, 1073741823",
        "-2147483647, 1, -1073741824",
        "-2147483648, 1, -1073741824",
        "-2147483648, 31, -1",
        "2147483647, 31, 0"
    })
    public void testIShr(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = shr(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int shr(int x, int y) {
        return x >> y;
    }

    @Tag("ushr<II>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - ushr({0},{1}) = {2}")
    @CsvSource({
        "0, 0, 0",
        "0, 31, 0",
        "1, 0, 1",
        "-1, 0, -1",
        "1, 31, 0",
        "-1, 31, 1",
        "256, 2, 64",
        "-256, 2, 1073741760",
        "1000, 20, 0",
        "-1000, 20, 4095",
        "1000, 5, 31",
        "-1000, 5, 134217696",
        "2147483647, 0, 2147483647",
        "-2147483648, 0, -2147483648",
        "2147483647, 1, 1073741823",
        "-2147483647, 1, 1073741824",
        "-2147483648, 1, 1073741824",
        "-2147483648, 31, 1",
        "2147483647, 31, 0"
    })
    public void testIUshr(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = ushr(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int ushr(int x, int y) {
        return x >>> y;
    }

    @Tag("and<II>I")
    @Order(1)
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
            result = and(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int and(int x, int y) {
        return x & y;
    }

    @Tag("or<II>I")
    @Order(1)
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
            result = or(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int or(int x, int y) {
        return x | y;
    }

    @Tag("xor<II>I")
    @Order(1)
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
            result = xor(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int xor(int x, int y) {
        return x ^ y;
    }

    @Tag("iconst_m1<>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - iconst_m1() = {0}")
    @CsvSource({
        "-1"
    })
    public void testIConst_m1(int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = iconst_m1();
        }
        assertEquals(expected, result);
    }

    public static int iconst_m1() {
        return -1;
    }

    @Tag("iconst_0<>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - iconst_0() = {0}")
    @CsvSource({
        "0"
    })
    public void testIConst_0(int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = iconst_0();
        }
        assertEquals(expected, result);
    }

    public static int iconst_0() {
        return 0;
    }

    @Tag("iconst_1<>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - iconst_1() = {0}")
    @CsvSource({
        "1"
    })
    public void testIConst_1(int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = iconst_1();
        }
        assertEquals(1, result);
    }

    public static int iconst_1() {
        return 1;
    }

    @Tag("iconst_2<>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - iconst_2() = {0}")
    @CsvSource({
        "2"
    })
    public void testIConst_2(int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = iconst_2();
        }
        assertEquals(expected, result);
    }

    public static int iconst_2() {
        return 2;
    }

    @Tag("iconst_3<>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - iconst_3() = {0}")
    @CsvSource({
        "3"
    })
    public void testIConst_3(int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = iconst_3();
        }
        assertEquals(expected, result);
    }

    public static int iconst_3() {
        return 3;
    }

    @Tag("iconst_4<>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - iconst_4() = {0}")
    @CsvSource({
        "4"
    })
    public void testIConst_4(int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = iconst_4();
        }
        assertEquals(expected, result);
    }

    public static int iconst_4() {
        return 4;
    }

    @Tag("iconst_5<>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - iconst_5() = {0}")
    @CsvSource({
        "5"
    })
    public void testIConst_5(int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = iconst_5();
        }
        assertEquals(expected, result);
    }

    public static int iconst_5() {
        return 5;
    }

    /* Outer bounds tested only to avoid writing replicated code */
    /* Note that -1 to 5 will result in iconst_<i> being called, not bipush */

    @Tag("bipush_m32<>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - bipush_m32() = {0}")
    @CsvSource({
       "-32"
    })
    public void testBIPush_m32(int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations() ; i++) {
            result = bipush_m32();
        }
        assertEquals(expected, result);
    }

    public static int bipush_m32() {
        return -32;
    }

    @Tag("bipush_31<>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - bipush_31() = {0}")
    @CsvSource({
       "31"
    })
    public void testBIPush_31(int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations() ; i++) {
            result = bipush_31();
        }
        assertEquals(expected, result);
    }

    public static int bipush_31() {
        return 31;
    }

    @Tag("sipush_32767<>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - sipush_32767({0}, {1}) = {0}")
    @CsvSource({
       "32767"
    })
    public void testSIPush_32767(int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations() ; i++) {
            result = sipush_32767();
        }
        assertEquals(expected, result);
    }

    public static int sipush_32767() {
        return 32767;
    }

    @Tag("sipush_m32768<>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - sipush_m32768({0}, {1}) = {0}")
    @CsvSource({
       "-32768"
    })
    public void testSIPush_m32768(int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations() ; i++) {
            result = sipush_m32768();
        }
        assertEquals(expected, result);
    }

    public static int sipush_m32768() {
        return -32768;
    }

}
