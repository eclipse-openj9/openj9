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

public class GetPutFieldTests {

    // For basic test with ints
    int test = 7;

    // For testing with arithmetic
    int a = 2;
    int b = 0;

    @Tag("getField<>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - getPutField({0}) = {1}")
    @CsvSource({
        "5, 5",
        "9, 9",
        "2, 2",
        "3, 3"
    })
    public void testGetPutField(int arg1, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            putField(arg1);
            result = getField();
        }
        assertEquals(expected, result);
    }

    public void putField(int val) {
        this.test = val;
    }

    public int getField() {
        return this.test;
    }

    @Tag("add<>I")
    @Order(2)
    @ParameterizedTest(name = "#{index} - add({0}, {1}) = {2}")
    @CsvSource({
        "8, 8, 16",
        "0, 1, 1",
        "1, 0, 1",
        "2147483647, 2147483647, -2",
        "-2147483648, 2147483647, -1"
    })
    public void testadd(int arg1, int arg2, int expected) {
        int result = 0;
        for (int i = 0; i < Helper.invocations(); i++) {
            setInts(arg1, arg2);
            result = add();
        }
        assertEquals(expected, result);
    }

    private int add() {
        return this.a + this.b;
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
            result = (a == b);
        }
        assertEquals(true, result);
    }

    public void setInts(int x, int y) {
        a = x;
        b = y;
    }

    // For basic test with longs
    long testL = 17L;

    // For testing with arithmetic
    long aL = 2L;
    long bL = 0L;

    @Tag("getFieldL<>J")
    @Order(2)
    @ParameterizedTest(name = "#{index} - getPutFieldL({0}) = {1}")
    @CsvSource({
        "0, 0",
        "-1, -1",
        "9223372036854775807, 9223372036854775807",
        "-9223372036854775808, -9223372036854775808"
    })
    public void testGetPutFieldL(long arg1, long expected) {
        long result = 0L;
        for (int i = 0; i < Helper.invocations(); i++) {
            putFieldL(arg1);
            result = getFieldL();
        }
        assertEquals(expected, result);
    }

    public void putFieldL(long val) {
        this.testL = val;
    }

    public long getFieldL() {
        return this.testL;
    }

    @Tag("addL<>J")
    @Order(2)
    @ParameterizedTest(name = "#{index} - addL({0}, {1}) = {2}")
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
    public void testaddL(long arg1, long arg2, long expected) {
        long result = 0L;
        for (int i = 0; i < Helper.invocations(); i++) {
            setLongs(arg1, arg2);
            result = addL();
        }
        assertEquals(expected, result);
    }

    private long addL() {
        return this.aL + this.bL;
    }

    @Tag("setLongs<JJ>V")
    @Order(1)
    @ParameterizedTest(name = "#{index} - setLongs({0},{0})")
    @CsvSource({
        "0",
        "-1",
        "9223372036854775807",
        "-9223372036854775808"
    })
    public void testSetLongs(long arg1) {
        boolean result = false;
        for (int i = 0; i < Helper.invocations(); i++) {
            setLongs(arg1, arg1);
            result = (aL == bL);
        }
        assertEquals(true, result);
    }

    public void setLongs(long x, long y) {
        aL = x;
        bL = y;
    }

    // For basic test with floats
    float testF = 17.23F;

    // For testing with arithmetic
    float aF = 2.3F;
    float bF = 0.2F;

    @Tag("getFieldF<>F")
    @Order(2)
    @ParameterizedTest(name = "#{index} - getPutFieldF({0}) = {1}")
    @CsvSource({
        "5.3, 5.3",
        "-9.234567, -9.234567",
        "3.4E+38, 3.4E+38",
        "NaN, NaN",
        "-Infinity, -Infinity"
    })
    public void testgetPutFieldF(float arg1, float expected) {
        float result = 0F;
        for (int i = 0; i < Helper.invocations(); i++) {
            putFieldF(arg1);
            result = getFieldF();
        }
        assertEquals(expected, result);
    }

    public void putFieldF(float val) {
        this.testF = val;
    }

    public float getFieldF() {
        return this.testF;
    }

    @Tag("addF<>F")
    @Order(2)
    @ParameterizedTest(name = "#{index} - addF({0}, {1}) = {2}")
    @CsvSource({
        "99.26485999, 2, 101.264860",
        "101.26485999, 101.26485999, 202.529720",
        "3.4E+38, 1, 3.4E+38",
        "3.4E+38, 0.0000001E+38, 3.4E38",
        "NaN, 10, NaN",
        "Infinity, -Infinity, NaN"
    })
    public void testaddF(float arg1, float arg2, float expected) {
        float result = 0F;
        for (int i = 0; i < Helper.invocations(); i++) {
            setFloats(arg1, arg2);
            result = addF();
        }
        assertEquals(expected, result);
    }

    private float addF() {
        return this.aF + this.bF;
    }

    @Tag("setFloats<FF>V")
    @Order(1)
    @ParameterizedTest(name = "#{index} - setFloats({0},{0})")
    @CsvSource({
        "99.26485999",
        "101.26485999",
        "3.4E+38",
        "Infinity"
    })
    public void testSetFloats(float arg1) {
        boolean result = false;
        for (int i = 0; i < Helper.invocations(); i++) {
            setFloats(arg1, arg1);
            result = (aF == bF);
        }
        assertEquals(true, result);
    }

    public void setFloats(float x, float y) {
        aF = x;
        bF = y;
    }

    // For basic test with doubles
    double testD = 17.23D;

    // For testing with arithmetic
    double aD = 2.3D;
    double bD = 0.2D;

    @Tag("getFieldD<>D")
    @Order(2)
    @ParameterizedTest(name = "#{index} - getPutFieldD({0}) = {1}")
    @CsvSource({
        "99.264860, 99.264860",
        "1000, 1000",
        "1.79E+308, 1.79E+308",
        "-4.94E-324, -4.94E-324",
        "NaN, NaN",
        "-Infinity, -Infinity"
    })
    public void testgetPutFieldD(double arg1, double expected) {
        double result = 0D;
        for (int i = 0; i < Helper.invocations(); i++) {
            putFieldD(arg1);
            result = getFieldD();
        }
        assertEquals(expected, result);
    }

    public void putFieldD(double val) {
        this.testD = val;
    }

    public double getFieldD() {
        return this.testD;
    }

    @Tag("addD<>D")
    @Order(2)
    @ParameterizedTest(name = "#{index} - addD({0}, {1}) = {2}")
    @CsvSource({
        "99.264860, 2, 101.264860",
        "1000, 43, 1043",
        "101.26485238, 101.26485238, 202.52970476",
        "1.79E+308, 1, 1.79E+308",
        "-4.94E-324, 4.94E-324, 0",
        "NaN, 10, NaN",
        "Infinity, -Infinity, NaN"
    })
    public void testaddD(double arg1, double arg2, double expected) {
        double result = 0D;
        for (int i = 0; i < Helper.invocations(); i++) {
            setDoubles(arg1, arg2);
            result = addD();
        }
        assertEquals(expected, result);
    }

    private double addD() {
        return this.aD + this.bD;
    }

    @Tag("setDoubles<DD>V")
    @Order(1)
    @ParameterizedTest(name = "#{index} - setDoubles({0},{0})")
    @CsvSource({
        "99.264860",
        "1000",
        "1.79E+308",
        "-4.94E-324",
        "Infinity"
    })
    public void testSetDoubles(double arg1) {
        boolean result = false;
        for (int i = 0; i < Helper.invocations(); i++) {
            setDoubles(arg1, arg1);
            result = (aD == bD);
        }
        assertEquals(true, result);
    }

    public void setDoubles(double x, double y) {
        aD = x;
        bD = y;
    }

}
