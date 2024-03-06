/*******************************************************************************
 * Copyright IBM Corp. and others 2019
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/

package jit.test.recognizedMethod;
import org.testng.AssertJUnit;
import org.testng.annotations.Test;
import java.util.Random;

public class TestJavaLangMath {

    /**
    * Tests the constant corner cases defined by the {@link Math.sqrt} method.
    * <p>
    * The JIT compiler will transform calls to {@link Math.sqrt} within this test
    * into the following tree sequence:
    *
    * <code>
    * dsqrt
    *   dconst <x>
    * </code>
    *
    * Subsequent tree simplification passes will attempt to reduce this constant
    * operation to a <code>dsqrt</code> IL by performing the square root at compile
    * time. The transformation will be performed when the function get executed 
    * twice, therefore, the "invocationCount=2" is needed. However we must ensure the
    * result of the square root done by the compiler at compile time will be exactly 
    * the same as the result had it been done by the Java runtime at runtime. This 
    * test validates the results are the same.
    */
    @Test(groups = {"level.sanity"}, invocationCount=2)
    public void test_java_lang_Math_sqrt() {
        AssertJUnit.assertTrue(Double.isNaN(Math.sqrt(Double.NEGATIVE_INFINITY)));
        AssertJUnit.assertTrue(Double.isNaN(Math.sqrt(-42.25d)));
        AssertJUnit.assertEquals(-0.0d, Math.sqrt(-0.0d));
        AssertJUnit.assertEquals(+0.0d, Math.sqrt(+0.0d));
        AssertJUnit.assertEquals(7.5d, Math.sqrt(56.25d));
        AssertJUnit.assertEquals(Double.POSITIVE_INFINITY, Math.sqrt(Double.POSITIVE_INFINITY));
        AssertJUnit.assertTrue(Double.isNaN(Math.sqrt(Double.NaN)));
    }

    /**
    * Tests all execution paths defined by the {@link Math.max} method, for float and double data types.
    */
    @Test(groups = {"level.sanity"}, invocationCount=2)
    public void test_java_lang_Math_max() {
        // Test Math.max for double type with NaN & +0/-0 values
        AssertJUnit.assertTrue(Double.isNaN(Math.max(Double.NaN, Double.NaN)));
        AssertJUnit.assertTrue(Double.isNaN(Math.max(Double.NaN, 0.0)));
        AssertJUnit.assertTrue(Double.isNaN(Math.max(0.0, Double.NaN)));
        AssertJUnit.assertTrue(Double.isNaN(Math.max(Double.NaN, -0.0)));
        AssertJUnit.assertTrue(Double.isNaN(Math.max(-0.0, Double.NaN)));
        AssertJUnit.assertEquals(0.0, Math.max(0.0, -0.0), 0.0);
        AssertJUnit.assertEquals(0.0, Math.max(-0.0, 0.0), 0.0);

        // Test Math.max for float type with NaN & +0/-0 values
        AssertJUnit.assertTrue(Float.isNaN(Math.max(Float.NaN, Float.NaN)));
        AssertJUnit.assertTrue(Float.isNaN(Math.max(Float.NaN, 0.0f)));
        AssertJUnit.assertTrue(Float.isNaN(Math.max(0.0f, Float.NaN)));
        AssertJUnit.assertTrue(Float.isNaN(Math.max(Float.NaN, -0.0f)));
        AssertJUnit.assertTrue(Float.isNaN(Math.max(-0.0f, Float.NaN)));
        AssertJUnit.assertEquals(0.0f, Math.max(0.0f, -0.0f), 0.0f);
        AssertJUnit.assertEquals(0.0f, Math.max(-0.0f, 0.0f), 0.0f);

        //Test Math.max with variation of random negative & positive doubles
        Random random = new Random();
        double d1 = -random.nextDouble() * 100; // ensures number is negative and within a reasonable range
        double d2 = -random.nextDouble() * 100;
        double d3 = random.nextDouble() * 100; // ensures number is positive and within a reasonable range
        double d4 = random.nextDouble() * 100;
        AssertJUnit.assertEquals(Math.max(d1, d2), (d1 > d2) ? d1 : d2, 0.0);
        AssertJUnit.assertEquals(Math.max(d2, d3), (d2 > d3) ? d2 : d3, 0.0);
        AssertJUnit.assertEquals(Math.max(d3, d4), (d3 > d4) ? d3 : d4, 0.0);
        AssertJUnit.assertEquals(Math.max(d1, d4), (d1 > d4) ? d1 : d4, 0.0);

        //Test Math.max with variation of random negative & positive floats
        float f1 = -random.nextFloat() * 100; // ensures number is negative and within a reasonable range
        float f2 = -random.nextFloat() * 100;
        float f3 = random.nextFloat() * 100; // ensures number is positive and within a reasonable range
        float f4 = random.nextFloat() * 100;
        AssertJUnit.assertEquals(Math.max(f1, f2), (f1 > f2) ? f1 : f2, 0.0f);
        AssertJUnit.assertEquals(Math.max(f2, f3), (f2 > f3) ? f2 : f3, 0.0f);
        AssertJUnit.assertEquals(Math.max(f3, f4), (f3 > f4) ? f3 : f4, 0.0f);
        AssertJUnit.assertEquals(Math.max(f1, f4), (f1 > f4) ? f1 : f4, 0.0f);
    }

    /**
    * Tests all execution paths defined by the {@link Math.min} method, for float and double data types.
    */
    @Test(groups = {"level.sanity"}, invocationCount=2)
    public void test_java_lang_Math_min() {
        // Test Math.min for double type with NaN & +0/-0 values
        AssertJUnit.assertTrue(Double.isNaN(Math.min(Double.NaN, Double.NaN)));
        AssertJUnit.assertTrue(Double.isNaN(Math.min(Double.NaN, 0.0)));
        AssertJUnit.assertTrue(Double.isNaN(Math.min(0.0, Double.NaN)));
        AssertJUnit.assertTrue(Double.isNaN(Math.min(Double.NaN, -0.0)));
        AssertJUnit.assertTrue(Double.isNaN(Math.min(-0.0, Double.NaN)));
        AssertJUnit.assertEquals(-0.0, Math.min(0.0, -0.0), 0.0);
        AssertJUnit.assertEquals(-0.0, Math.min(-0.0, 0.0), 0.0);

        // Test Math.min for float type with NaN & +0/-0 values
        AssertJUnit.assertTrue(Float.isNaN(Math.min(Float.NaN, Float.NaN)));
        AssertJUnit.assertTrue(Float.isNaN(Math.min(Float.NaN, 0.0f)));
        AssertJUnit.assertTrue(Float.isNaN(Math.min(0.0f, Float.NaN)));
        AssertJUnit.assertTrue(Float.isNaN(Math.min(Float.NaN, -0.0f)));
        AssertJUnit.assertTrue(Float.isNaN(Math.min(-0.0f, Float.NaN)));
        AssertJUnit.assertEquals(-0.0f, Math.min(0.0f, -0.0f), 0.0f);
        AssertJUnit.assertEquals(-0.0f, Math.min(-0.0f, 0.0f), 0.0f);

        //Test Math.min with variation of random negative & positive doubles
        Random random = new Random();
        double d1 = -random.nextDouble() * 100; // ensures number is negative and within a reasonable range
        double d2 = -random.nextDouble() * 100;
        double d3 = random.nextDouble() * 100; // ensures number is positive and within a reasonable range
        double d4 = random.nextDouble() * 100;
        AssertJUnit.assertEquals(Math.min(d1, d2), (d1 < d2) ? d1 : d2, 0.0);
        AssertJUnit.assertEquals(Math.min(d2, d3), (d2 < d3) ? d2 : d3, 0.0);
        AssertJUnit.assertEquals(Math.min(d3, d4), (d3 < d4) ? d3 : d4, 0.0);
        AssertJUnit.assertEquals(Math.min(d1, d4), (d1 < d4) ? d1 : d4, 0.0);

        //Test Math.min with variation of random negative & positive floats
        float f1 = -random.nextFloat() * 100; // ensures number is negative and within a reasonable range
        float f2 = -random.nextFloat() * 100;
        float f3 = random.nextFloat() * 100; // ensures number is positive and within a reasonable range
        float f4 = random.nextFloat() * 100;
        AssertJUnit.assertEquals(Math.min(f1, f2), (f1 < f2) ? f1 : f2, 0.0f);
        AssertJUnit.assertEquals(Math.min(f2, f3), (f2 < f3) ? f2 : f3, 0.0f);
        AssertJUnit.assertEquals(Math.min(f3, f4), (f3 < f4) ? f3 : f4, 0.0f);
        AssertJUnit.assertEquals(Math.min(f1, f4), (f1 < f4) ? f1 : f4, 0.0f);
    }
}
