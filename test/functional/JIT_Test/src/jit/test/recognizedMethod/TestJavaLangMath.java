/*
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
 */

package jit.test.recognizedMethod;
import org.testng.AssertJUnit;
import org.testng.annotations.Test;
import java.util.Random;
import org.testng.asserts.SoftAssert;
import static jit.test.recognizedMethod.TestMathUtils.*;
import org.testng.annotations.DataProvider;
import org.testng.annotations.Listeners;
import org.testng.AssertJUnit;



@Test(singleThreaded=true)
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

    @Test(groups = {"level.sanity"}, invocationCount=2, dataProvider="zeroProviderFD", dataProviderClass=TestMathUtils.class)
    public void test_java_lang_Math_min_zeros_FD(Number a, Number b, boolean isFirstArg) {
        if (a instanceof Float) {
            float f1 = a.floatValue();
            float f2 = b.floatValue();
            assertEquals(Math.min(f1, f2), isFirstArg ? f1 : f2);
        } else {
            double f1 = a.doubleValue();
            double f2 = b.doubleValue();
            assertEquals(Math.min(f1, f2), isFirstArg ? f1 : f2);
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2, dataProvider="zeroProviderFD", dataProviderClass=TestMathUtils.class)
    public void test_java_lang_Math_max_zeros_FD(Number a, Number b, boolean isFirstArg) {
        if (a instanceof Float) {
            float f1 = a.floatValue();
            float f2 = b.floatValue();
            assertEquals(Math.max(f1, f2), isFirstArg ? f2 : f1);
        } else {
            double f1 = a.doubleValue();
            double f2 = b.doubleValue();
            assertEquals(Math.max(f1, f2), isFirstArg ? f2 : f1);
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2, dataProvider="nanProviderFD", dataProviderClass=TestMathUtils.class)
    public void test_java_lang_Math_min_nan_FD(Number a, Number b, Number expected) {
        if (a instanceof Float) {
            float f1 = a.floatValue();
            float f2 = b.floatValue();
            AssertJUnit.assertTrue(Float.isNaN(Math.min(f1, f2)));
        } else {
            double f1 = a.doubleValue();
            double f2 = b.doubleValue();
            AssertJUnit.assertTrue(Double.isNaN(Math.min(f1, f2)));
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2, dataProvider="nanProviderFD", dataProviderClass=TestMathUtils.class)
    public void test_java_lang_Math_max_nan_FD(Number a, Number b, Number expected) {
        if (a instanceof Float) {
            float f1 = a.floatValue();
            float f2 = b.floatValue();
            AssertJUnit.assertTrue(Float.isNaN(Math.max(f1, f2)));
        } else {
            double f1 = a.doubleValue();
            double f2 = b.doubleValue();
            AssertJUnit.assertTrue(Double.isNaN(Math.max(f1, f2)));
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2, dataProvider="normalNumberProviderFD", dataProviderClass=TestMathUtils.class)
    public void test_java_lang_Math_min_normal_FD(Number a, Number b){
        if (a instanceof Float) {
            float f1 = a.floatValue();
            float f2 = b.floatValue();
            AssertJUnit.assertEquals(Math.min(f1, f2), f1 <= f2 ? f1 : f2);
        } else {
            double f1 = a.doubleValue();
            double f2 = b.doubleValue();
            AssertJUnit.assertEquals(Math.min(f1, f2), f1 <= f2 ? f1 : f2);
        }
    }

    @Test(groups = {"level.sanity"}, invocationCount=2, dataProvider="normalNumberProviderFD", dataProviderClass=TestMathUtils.class)
    public void test_java_lang_Math_max_normal_FD(Number a, Number b){
        if (a instanceof Float) {
            float f1 = a.floatValue();
            float f2 = b.floatValue();
            AssertJUnit.assertEquals(Math.max(f1, f2), f1 >= f2 ? f1 : f2);
        } else {
            double f1 = a.doubleValue();
            double f2 = b.doubleValue();
            AssertJUnit.assertEquals(Math.max(f1, f2), f1 >= f2 ? f1 : f2);
        }
    }
}
