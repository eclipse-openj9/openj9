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

public class BranchTests {

    @Tag("gotoTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - gotoTest({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 0",
        "2, 1",
        "5, 10"
    })
    public void test_goto(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = gotoTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int gotoTest(int n) {
        int result = 0;
        for (int i = 0; i < n ; i++) {
            result += i;
        }
        return result;
    }

    @Tag("ifeqTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - ifeqTest({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 0",
        "2, 1",
        "5, 10"
    })
    public void test_ifeq(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = ifeqTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int ifeqTest(int n) {
        int result = 0;
        for (int i = 0; i < n; i++) {
            if (i != 0) {
                result += i;
            }
        }
        return result;
    }

    @Tag("ifneTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - ifneTest({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 1",
        "2, 2",
        "5, 11"
    })
    public void test_ifne(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = ifneTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int ifneTest(int n) {
        int result = 0;
        for (int i = 0; i < n; i++) {
            if (i == 0) {
                result += 1;
            }
            else {
                result += i;
            }
        }
        return result;
    }

    @Tag("ifltTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - ifltTest({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 0",
        "2, 1",
        "5, 10"
    })
    public void test_iflt(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = ifltTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int ifltTest(int n) {
        int result = 0;
        for (int i = 0; i < n; i++) {
            if (i >= 0) {
                result += i;
            }
        }
        return result;
    }

    @Tag("ifleTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - ifleTest({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 0",
        "2, 1",
        "5, 10"
    })
    public void test_ifle(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = ifleTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int ifleTest(int n) {
        int result = 0;
        for (int i = 0; i < n; i++) {
            if (i > 0) {
                result += i;
            }
        }
        return result;
    }

    @Tag("ifgtTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - ifgtTest({0}) = {1}")
    @CsvSource({
        "0, -1",
        "1, -1",
        "2, 0",
        "5, 9"
    })
    public void test_ifgt(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = ifgtTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int ifgtTest(int n) {
        int result = 0;
        for (int i = -1; i < n; i++) {
            if (i <= 0) {
                result += i;
            }
            else {
                result += i;
            }
        }
        return result;
    }

    @Tag("ifgeTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - ifgeTest({0}) = {1}")
    @CsvSource({
        "0, -1",
        "1, -1",
        "2, 0",
        "5, 9"
    })
    public void test_ifge(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = ifgeTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int ifgeTest(int n) {
        int result = 0;
        for (int i = -1; i < n; i++) {
            if (i < 0) {
                result += i;
            }
            else {
                result += i;
            }
        }
        return result;
    }

    @Tag("if_icmpeqTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - if_icmpeqTest({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 2",
        "2, 2",
        "5,11"
    })
    public void test_if_icmpeq(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = if_icmpeqTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int if_icmpeqTest(int n) {
        int result = 0;
        for (int i = 0; i < n ; i++) {
            if (i != n-2) {
                result += 2;
            }
            else {
                result += i;
            }
        }
        return result;
    }

    @Tag("if_icmpneTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - if_icmpneTest({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 0",
        "2, 3",
        "5, 9"
    })
    public void test_if_icmpne(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = if_icmpneTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int if_icmpneTest(int n) {
        int result = 0;
        for (int i = 0; i < n ; i++) {
            if (i == n-2) {
                result += 2;
            }
            else {
                result += i;
            }
        }
        return result;
    }

    @Tag("if_icmpltTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - if_icmpltTest({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 2",
        "2, 4",
        "5, 7"
    })
    public void test_if_icmplt(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = if_icmpltTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int if_icmpltTest(int n) {
        int result = 0;
        for (int i = 0; i < n ; i++) {
            if (i >= n-2) {
                result += 2;
            }
            else {
                result += i;
            }
        }
        return result;
    }

    @Tag("if_icmpleTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - if_icmpleTest({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 2",
        "2, 2",
        "5, 8"
    })
    public void test_if_icmple(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = if_icmpleTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int if_icmpleTest(int n) {
        int result = 0;
        for (int i = 0; i < n ; i++) {
            if (i > n-2) {
                result += 2;
            }
            else {
                result += i;
            }
        }
        return result;
    }

    @Tag("if_icmpgtTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - if_icmpgtTest({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 0",
        "2, 3",
        "5, 12"
    })
    public void test_if_icmpgt(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = if_icmpgtTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int if_icmpgtTest(int n) {
        int result = 0;
        for (int i = 0; i < n ; i++) {
            if (i <= n-2) {
                result += 2;
            }
            else {
                result += i;
            }
        }
        return result;
    }

    @Tag("if_icmpgeTest<I>I")
    @Order(1)
    @ParameterizedTest(name = "#{index} - if_icmpgeTest({0}) = {1}")
    @CsvSource({
        "0, 0",
        "1, 0",
        "2, 1",
        "5, 13"
    })
    public void test_if_icmpge(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            result = if_icmpgeTest(arg1);
        }
        assertEquals(expected, result);
    }

    public static int if_icmpgeTest(int n) {
        int result = 0;
        for (int i = 0; i < n ; i++) {
            if (i < n-2) {
                result += 2;
            }
            else {
                result += i;
            }
        }
        return result;
    }

}
