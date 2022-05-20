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

public class StackTests {

    @Tag("pop<I>V")
    @Order(1)
    @ParameterizedTest(name = "#{index} - popTest({0})")
    @CsvSource({
        "0",
        "1",
        "2",
        "2147483647"
    })
    public void testPop(int arg1) {
        for (int i = 0; i < Helper.invocations(); i++) {
            pop(arg1);
        }
    }

    public static int return_int(int x) {
        return x;
    }

    public static void pop(int a) {
        return_int(a);
    }

    @Tag("pop2<J>V")
    @Order(1)
    @ParameterizedTest(name = "#{index} - pop2Test({0})")
    @CsvSource({
        "0",
        "1",
        "2",
        "2147483647",
        "9223372036854775807"
    })
    public void testPop2(long arg1) {
        for (int i = 0; i < Helper.invocations(); i++) {
            pop2(arg1);
        }
    }

    public static long return_long(long x) {
        return x;
    }

    public static void pop2(long a) {
        return_long(a);
    }

    @Tag("swap<III>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - swap({0}, {1}, {2}) = {3}")
    @CsvSource({
        "0, 1, 1, 2",
        "1, 1, 2, 3",
        "2, 7, 9, 16",
        "10, 5, 15, 20"
    })
    public void testSwap(int arg1, int arg2, int arg3, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = swap(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }

    public static int swap(int x, int y, int z) {
        return Swap.swap(x, y, z);
    }

    @Tag("dup<II>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - dup({0}, {1}) = {2}")
    @CsvSource({
        "0, 1, 1",
        "1, 1, 2",
        "2, 7, 9",
        "10, 5, 15"
    })
    public void testDup(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dup(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int dup(int a, int b) {
        return a += b;
    }

    @Tag("dup2Form1<II>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - dup2Form1({0}, {1}) = {2}")
    @CsvSource({
        "0, 1, 2",
        "1, 1, 4",
        "2, 7, 18",
        "10, 5, 30"
    })
    public void testDup2Form1(int arg1, int arg2, int expected) {
        long result = 0;
        for (long i = 0; i < Helper.invocations(); i++) {
            result = dup2Form1(arg1, arg2);

        }
        assertEquals(expected, result);
    }

    public static int dup2Form1(int a, int b) {
        return Dup.dup2Form1(a, b);
    }

    @Tag("dup2Form2<JJ>J")
    @Order(1)
    @ParameterizedTest(name = "#{index} - dup2Form2({0}, {1}) = {2}")
    @CsvSource({
        "0, 1, 1",
        "1, 1, 2",
        "2, 7, 9",
        "10, 5, 15"
    })
    public void testDup2Form2(long arg1, long arg2, long expected) {
        long result = 0;
        for (long i = 0; i < Helper.invocations(); i++) {
            result = dup2Form2(arg1, arg2);

        }
        assertEquals(expected, result);
    }

    public static long dup2Form2(long a, long b) {
        return a += b;
    }

    @Tag("dup_x1<II>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - dup_x1({0}, {1}) = {2}")
    @CsvSource({
        "0, 1, 2",
        "1, 1, 3",
        "2, 7, 16",
        "10, 5, 20"
    })
    public void testDup_x1(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dup_x1(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int dup_x1(int x, int y) {
        return Dup.dup_x1(x, y);
    }

    @Tag("dup_x2Form1<III>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - dup_x2Form1({0}, {1}, {2}) = {3}")
    @CsvSource({
        "0, 1, 1, 3",
        "1, 1, 2, 6",
        "2, 7, 9, 27",
        "10, 5, 15, 45"
    })
    public void testDup_x2Form1(int arg1, int arg2, int arg3, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dup_x2Form1(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }

    public static int dup_x2Form1(int x, int y, int z) {
        return Dup.dup_x2Form1(x, y, z);
    }

    @Tag("dup_x2Form2<JI>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - dup_x2Form2({0}, {1}) = {2}")
    @CsvSource({
        "0, 1, 2",
        "1, 1, 3",
        "2, 7, 16",
        "10, 5, 20"
    })
    public void testDup_x2Form2(long arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dup_x2Form2(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static int dup_x2Form2(long x, int y) {
        return Dup.dup_x2Form2(x, y);
    }

    @Tag("dup2_x1Form1<III>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - dup2_x1Form1({0}, {1}, {2}) = {3}")
    @CsvSource({
        "0, 1, 1, 4",
        "1, 1, 2, 7",
        "2, 7, 9, 34",
        "10, 5, 15, 50"
    })
    public void testDup2_x1Form1(int arg1, int arg2, int arg3, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dup2_x1Form1(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }

    public static int dup2_x1Form1(int x, int y, int z) {
        return Dup.dup2_x1Form1(x, y, z);
    }

    @Tag("dup2_x1Form2<IJ>J")
    @Order(2)
    @ParameterizedTest(name = "#{index} - dup2_x1Form2({0}, {1}) = {2}")
    @CsvSource({
        "2, 7, 16",
        "0, 1, 2",
        "1, 1, 3",
        "10, 5, 20"
    })
    public void testDup2_x1Form2(int arg1, long arg2, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dup2_x1Form2(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static long dup2_x1Form2(int x, long y) {
        return Dup.dup2_x1Form2(x, y);
    }

    @Tag("dup2_x2Form1<IIII>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - dup2_x2Form1({0}, {1}, {2}, {3}) = {4}")
    @CsvSource({
        "1, 1, 2, 3, 12",
        "0, 1, 1, 6, 15",
        "2, 7, 9, 10, 47",
        "10, 5, 15, 20, 85"
    })
    public void testDup2_x2Form1(int arg1, int arg2, int arg3, int arg4, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dup2_x2Form1(arg1, arg2, arg3, arg4);
        }
        assertEquals(expected, result);
    }

    public static int dup2_x2Form1(int w, int x, int y, int z) {
        return Dup.dup2_x2Form1(w, x, y, z);
    }

    @Tag("dup2_x2Form2<IIJ>J")
    @Order(2)
    @ParameterizedTest(name = "#{index} - dup2_x2Form2({0}, {1}, {2}) = {3}")
    @CsvSource({
        "0, 1, 1, 3",
        "1, 1, 2, 6",
        "2, 7, 9, 27",
        "10, 5, 15, 45"
    })
    public void testDup2_x2Form2(int arg1, int arg2, long arg3, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dup2_x2Form2(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }

    public static long dup2_x2Form2(int x, int y, long z) {
        return Dup.dup2_x2Form2(x, y, z);
    }

    @Tag("dup2_x2Form3<JII>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - dup2_x2Form3({0}, {1}, {2}) = {3}")
    @CsvSource({
        "0, 1, 1, 4",
        "1, 1, 2, 7",
        "2, 7, 9, 34",
        "10, 5, 15, 50"
    })
    public void testDup2_x2Form3(long arg1, int arg2, int arg3, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dup2_x2Form3(arg1, arg2, arg3);
        }
        assertEquals(expected, result);
    }

    public static int dup2_x2Form3(long x, int y, int z) {
        return Dup.dup2_x2Form3(x, y, z);
    }

    @Tag("dup2_x2Form4<JJ>J")
    @Order(2)
    @ParameterizedTest(name = "#{index} - dup2_x2Form4({0}, {1}) = {2}")
    @CsvSource({
        "0, 1, 2",
        "1, 1, 3",
        "2, 7, 16",
        "10, 5, 20"
    })
    public void testDup2_x2Form4(long arg1, long arg2, long expected) {
        long result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = dup2_x2Form4(arg1, arg2);
        }
        assertEquals(expected, result);
    }

    public static long dup2_x2Form4(long x, long y) {
        return Dup.dup2_x2Form4(x, y);
    }

}
