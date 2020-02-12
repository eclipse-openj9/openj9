/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
package org.openj9.test.java.lang;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;

public class Test_Math_Fma 
    {
    public static void commoning_double(double a)
        {
        double b = 2.0, c = 3.0; 
        double r = Math.fma(a, b, c) + Math.fma(b, c, a);
        double expected = 18.0;
        AssertJUnit.assertEquals(r, expected);
        }

    public static void commoning_float(float a)
        {
        float b = 2.0f, c = 3.0f; 
        float r = Math.fma(a, b, c) + Math.fma(b, c, a);
        float expected = 18.0f;
        AssertJUnit.assertEquals(r, expected);
        }

    @Test(groups={ "level.sanity" }, invocationCount=2)
	public void test_Math_fma_double() 
        {
        double random_double = 0.9085095723204865;
        /**
         * Testing special case: 
         * If any argument is NaN, the result is NaN.
         */
        double a, b, c, r;
        a = Double.NaN;
        b = random_double; 
        c = random_double;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = random_double; 
        b = Double.NaN;
        c = random_double;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = random_double; 
        b = random_double; 
        c = Double.NaN;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = Double.NaN;
        b = 0; 
        c = random_double;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = 0; 
        b = Double.NaN;
        c = Double.POSITIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = Double.NEGATIVE_INFINITY; 
        b = 0; 
        c = Double.NaN;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        /**
         * Testing special case: 
         * If one of the first two arguments is infinite and the other is zero, the result is NaN.
         */
        a = Double.POSITIVE_INFINITY;
        b = 0; 
        c = random_double;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = 0; 
        b = Double.POSITIVE_INFINITY; 
        c = random_double;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = Double.NEGATIVE_INFINITY;
        b = 0; 
        c = random_double;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = 0;
        b = Double.NEGATIVE_INFINITY; 
        c = random_double;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        /**
         * Testing special case: 
         * If the exact product of the first two arguments is infinite (in other words, at least one of the arguments is infinite and
         * the other is neither zero nor NaN) and the third argument is an infinity of the opposite sign, the result is NaN.
         */
        a = Double.POSITIVE_INFINITY;
        b = random_double; 
        c = Double.NEGATIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = Double.NEGATIVE_INFINITY;
        b = random_double; 
        c = Double.POSITIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = random_double;
        b = Double.POSITIVE_INFINITY;
        c = Double.NEGATIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = random_double;
        b = Double.NEGATIVE_INFINITY; 
        c = Double.POSITIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = -random_double;
        b = Double.NEGATIVE_INFINITY;
        c = Double.NEGATIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = -random_double;
        b = Double.POSITIVE_INFINITY; 
        c = Double.POSITIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = -random_double;
        b = Double.NEGATIVE_INFINITY;
        c = Double.NEGATIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        a = -random_double;
        b = Double.POSITIVE_INFINITY; 
        c = Double.POSITIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isNaN(r));

        /**
         * Testing infinity numbers: 
         */
        a = Double.POSITIVE_INFINITY;
        b = 2.2; 
        c = 3.3;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isInfinite(r));

        a = Double.POSITIVE_INFINITY;
        b = Double.NEGATIVE_INFINITY; 
        c = 3.3;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isInfinite(r));

        a = Double.POSITIVE_INFINITY;
        b = Double.NEGATIVE_INFINITY; 
        c = Double.NEGATIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isInfinite(r));
        
        a = Double.POSITIVE_INFINITY;
        b = Double.NEGATIVE_INFINITY; 
        c = Double.NEGATIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Double.isInfinite(r));

        /**
         * Testing against random numbers: 
         */
        a = 0.9085095723204865;
        b = 0.08818918433337497;
        c = 0.6513665215590198;
        r = Math.fma(a, b, c);
        double expected = 0.7314872397010268;
        AssertJUnit.assertEquals(r, expected);

        a = 0.590462214807549;
        b = 0.15430281193816775;
        c = 0.17457974769099827;
        r = Math.fma(a, b, c);
        expected = 0.2656897277790415;
        AssertJUnit.assertEquals(r, expected);

        a = 0.04441749995782596;
        b = 0.08992714669433366;
        c = 0.25876787066490525;
        r = Math.fma(a, b, c);
        expected = 0.2627622096994082;
        AssertJUnit.assertEquals(r, expected);

        /**
         * Testing with children being commoned in JIT compiler
         */
        commoning_double(3.0);

        /**
         * Testing against numbers that invoke rounding: 
         */
        a = 0.3333333333333333;
        b = 0.3333333333333333;
        c = 1.0000000000000000;
        r = Math.fma(a, b, c);
        //result with higher precision = 1.11111111111111108888888888889
        expected = 1.1111111111111112;
        AssertJUnit.assertEquals(r, expected);

        a = 0.6666666666666666;
        b = 0.6666666666666666;
        c = 0.1111111111111111;
        r = Math.fma(a, b, c);
        //result with higher precision = 0.555555555555555455555555555556
        expected = 0.5555555555555555;
        AssertJUnit.assertEquals(r, expected);

        a = 0.1428571428571428;
        b = 0.1428571428571427;
        c = 0.0000000000000000;
        r = Math.fma(a, b, c);
        //result with higher presicion = 0.02040816326530609183673469387756
        expected = 0.020408163265306093;
        AssertJUnit.assertEquals(r, expected);

        /**
         * Testing against boundaries: 
         */
        double max = Double.MAX_VALUE;
        double min = Double.MIN_VALUE;

        a = max;
        b = max;
        c = max;
        r = Math.fma(a, b, c);
        expected = Double.POSITIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = min;
        b = min;
        c = min;
        r = Math.fma(a, b, c);
        expected = 4.9E-324;
        AssertJUnit.assertEquals(r, expected);

        a = max - min;
        b = max - min;
        c = max - min;
        r = Math.fma(a, b, c);
        expected = Double.POSITIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = max;
        b = max;
        c = max;
        r = Math.fma(-a, -b, -c);
        expected = Double.POSITIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = min;
        b = min;
        c = min;
        r = Math.fma(-a, -b, -c);
        expected = -4.9E-324;
        AssertJUnit.assertEquals(r, expected);

        a = max - min;
        b = max - min;
        c = max - min;
        r = Math.fma(a, b, -c);
        expected = Double.POSITIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = max;
        b = max;
        c = max;
        r = Math.fma(a, -b, c);
        expected = Double.NEGATIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = max;
        b = max;
        c = max;
        r = Math.fma(a, b, -c);
        expected = Double.POSITIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = max;
        b = max;
        c = max;
        r = Math.fma(a, -b, -c);
        expected = Double.NEGATIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = min * 2;
        b = min * 2;
        c = min * 2;
        r = Math.fma(a, b, c);
        expected = 1.0E-323;
        AssertJUnit.assertEquals(r, expected);

        a = min * 2;
        b = min * 2;
        c = min * 2;
        r = Math.fma(-a, -b, -c);
        expected = -1.0E-323;
        AssertJUnit.assertEquals(r, expected);
        }

    @Test(groups={ "level.sanity" }, invocationCount=2)
    public void test_Math_fma_float() 
        {
        float random_float = 0.5877134f;
        /**
         * Testing special case: 
         * If any argument is NaN, the result is NaN.
         */
        float a, b, c, r;
        a = Float.NaN;
        b = random_float; 
        c = random_float;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = random_float; 
        b = Float.NaN;
        c = random_float;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = random_float; 
        b = random_float; 
        c = Float.NaN;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = Float.NaN;
        b = 0; 
        c = random_float;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = 0; 
        b = Float.NaN;
        c = Float.POSITIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = Float.NEGATIVE_INFINITY; 
        b = 0; 
        c = Float.NaN;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        /**
         * Testing special case: 
         * If one of the first two arguments is infinite and the other is zero, the result is NaN.
         */
        a = Float.POSITIVE_INFINITY;
        b = 0; 
        c = random_float;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = 0; 
        b = Float.POSITIVE_INFINITY; 
        c = random_float;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = Float.NEGATIVE_INFINITY;
        b = 0; 
        c = random_float;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = 0;
        b = Float.NEGATIVE_INFINITY; 
        c = random_float;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        /**
         * Testing special case: 
         * If the exact product of the first two arguments is infinite (in other words, at least one of the arguments is infinite and
         * the other is neither zero nor NaN) and the third argument is an infinity of the opposite sign, the result is NaN.
         */
        a = Float.POSITIVE_INFINITY;
        b = random_float; 
        c = Float.NEGATIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = Float.NEGATIVE_INFINITY;
        b = random_float; 
        c = Float.POSITIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = random_float;
        b = Float.POSITIVE_INFINITY;
        c = Float.NEGATIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = random_float;
        b = Float.NEGATIVE_INFINITY; 
        c = Float.POSITIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = -random_float;
        b = Float.NEGATIVE_INFINITY;
        c = Float.NEGATIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = -random_float;
        b = Float.POSITIVE_INFINITY; 
        c = Float.POSITIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = -random_float;
        b = Float.NEGATIVE_INFINITY;
        c = Float.NEGATIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        a = -random_float;
        b = Float.POSITIVE_INFINITY; 
        c = Float.POSITIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isNaN(r));

        /**
         * Testing infinity numbers: 
         */
        a = Float.POSITIVE_INFINITY;
        b = 2.2f; 
        c = 3.3f;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isInfinite(r));

        a = Float.POSITIVE_INFINITY;
        b = Float.NEGATIVE_INFINITY; 
        c = 3.3f;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isInfinite(r));

        a = Float.POSITIVE_INFINITY;
        b = Float.NEGATIVE_INFINITY; 
        c = Float.NEGATIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isInfinite(r));
        
        a = Float.POSITIVE_INFINITY;
        b = Float.NEGATIVE_INFINITY; 
        c = Float.NEGATIVE_INFINITY;
        r = Math.fma(a, b, c);
        AssertJUnit.assertTrue(Float.isInfinite(r));

        /**
         * Testing against random numbers: 
         */
        a = 0.8862447f;
        b = 0.576474f;
        c = 0.5877134f;
        r = Math.fma(a, b, c);
        float expected = 1.0986105f;
        AssertJUnit.assertEquals(r, expected);

        a = 0.9664105f;
        b = 0.96392494f;
        c = 0.6710392f;
        r = Math.fma(a, b, c);
        expected = 1.6025864f;
        AssertJUnit.assertEquals(r, expected);

        a = 0.7614613f;
        b = 0.9316474f;
        c = 0.47867614f;
        r = Math.fma(a, b, c);
        expected = 1.1880896f;
        AssertJUnit.assertEquals(r, expected);

        /**
         * Testing with children being commoned in JIT compiler
         */
        commoning_float(3.0f);
        
        /**
         * Testing against numbers that invoke rounding: 
         */
        a = 0.3333333333333333f;
        b = 0.3333333333333333f;
        c = 1.0000000000000000f;
        r = Math.fma(a, b, c);
        //result with higher precision = 1.11111111111111108888888888889
        expected = 1.1111112f;
        AssertJUnit.assertEquals(r, expected);

        a = 0.6666666666666666f;
        b = 0.6666666666666666f;
        c = 0.1111111111111111f;
        r = Math.fma(a, b, c);
        //result with higher precision = 0.555555555555555455555555555556
        expected = 0.5555556f;
        AssertJUnit.assertEquals(r, expected);

        a = 0.1428571428571428f;
        b = 0.1428571428571427f;
        c = 0.0000000000000000f;
        r = Math.fma(a, b, c);
        //result with higher presicion = 0.02040816326530609183673469387756
        expected = 0.020408165f;
        AssertJUnit.assertEquals(r, expected);


        /**
         * Testing against boundaries: 
         */
        float max = Float.MAX_VALUE;
        float min = Float.MIN_VALUE;

        a = max;
        b = max;
        c = max;
        r = Math.fma(a, b, c);
        expected = Float.POSITIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = min;
        b = min;
        c = min;
        r = Math.fma(a, b, c);
        expected = 1.4E-45f;
        AssertJUnit.assertEquals(r, expected);

        a = max - min;
        b = max - min;
        c = max - min;
        r = Math.fma(a, b, c);
        expected = Float.POSITIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = max;
        b = max;
        c = max;
        r = Math.fma(-a, -b, -c);
        expected = Float.POSITIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = min;
        b = min;
        c = min;
        r = Math.fma(-a, -b, -c);
        expected = -1.4E-45f;
        AssertJUnit.assertEquals(r, expected);

        a = max - min;
        b = max - min;
        c = max - min;
        r = Math.fma(a, b, -c);
        expected = Float.POSITIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = max;
        b = max;
        c = max;
        r = Math.fma(a, -b, c);
        expected = Float.NEGATIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = max;
        b = max;
        c = max;
        r = Math.fma(a, b, -c);
        expected = Float.POSITIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = max;
        b = max;
        c = max;
        r = Math.fma(a, -b, -c);
        expected = Float.NEGATIVE_INFINITY;
        AssertJUnit.assertEquals(r, expected);

        a = min * 2;
        b = min * 2;
        c = min * 2;
        r = Math.fma(a, b, c);
        expected = 2.8E-45f;
        AssertJUnit.assertEquals(r, expected);

        a = min * 2;
        b = min * 2;
        c = min * 2;
        r = Math.fma(-a, -b, -c);
        expected = -2.8E-45f;
        AssertJUnit.assertEquals(r, expected);
        }
}
