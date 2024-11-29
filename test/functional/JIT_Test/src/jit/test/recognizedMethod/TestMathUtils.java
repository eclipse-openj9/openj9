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
import java.util.Random;
import org.testng.asserts.SoftAssert;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import org.testng.annotations.DataProvider;
import org.testng.AssertJUnit;

public class TestMathUtils {
    // constants used to various min/max tests
    private static final int  fqNaNBits = 0x7fcabdef;
    private static  final float  fqNaN = Float.intBitsToFloat(fqNaNBits);
    private static final int fsNaNBits = 0x7faedbaf;
    private static final float fsNaN = Float.intBitsToFloat(fsNaNBits);
    private static final int fnZeroBits = 0x80000000;
    private static final float fpInf = Float.POSITIVE_INFINITY;
    private static final float fnInf = Float.NEGATIVE_INFINITY;
    private static final int fpInfBits = Float.floatToRawIntBits(fpInf);
    private static final int fnInfBits = Float.floatToRawIntBits(fnInf);
    private static final int fquietBit = 0x00400000;
    private static final int fNaNExpStart = 0x7f800000;
    private static final int fNaNMantisaMax = 0x00400000;

    private static final long  dqNaNBits = 0x7ff800000000000fL;
    private static final double  dqNaN = Double.longBitsToDouble(dqNaNBits);
    private static final long dsNaNBits = 0x7ff400000000000fL;
    private static final double  dsNaN = Double.longBitsToDouble(dsNaNBits);
    private static final long dnZeroBits = 0x8000000000000000L;
    private static final double dpInf = Double.POSITIVE_INFINITY;
    private static final double dnInf = Double.NEGATIVE_INFINITY;
    private static final long dpInfBits = Double.doubleToRawLongBits(dpInf);
    private static final long dnInfBits = Double.doubleToRawLongBits(dnInf);
    private static final long dNaNMantisaMax = 0x0008000000000000L;
    private static final long dNaNExpStart = 0x7ff0000000000000L;
    private static final long  dquietBit = 0x0008000000000000L;
    private static final Random r = new Random();

    public static class NaNTestPair<T, BitType> {
        T first;
        T second;
        BitType bFirst;
        BitType bSecond;
        BitType expected;
    }

    @DataProvider(name="zeroProviderFD")
    public static Object[][] zeroProviderFD(){
        return new Object[][]{
            // arg1, arg2, expected
            {+0.0f, +0.0f, true},
            {-0.0f, -0.0f, true},
            {+0.0f, -0.0f, false},
            {-0.0f, +0.0f, true},

            {+0.0f, fpInf, true},
            {fpInf, +0.0f, false},
            {+0.0f, fnInf, false},
            {fnInf, +0.0f, true},
            {-0.0f, fpInf, true},
            {fpInf, -0.0f, false},
            {-0.0f, fnInf, false},
            {fnInf, -0.0f, true},

            {+0.0d, +0.0d, true},
            {-0.0d, -0.0d, true},
            {+0.0d, -0.0d, false},
            {-0.0d, +0.0d, true},

            {+0.0d, dpInf, true},
            {dpInf, +0.0d, false},
            {+0.0d, dnInf, false},
            {dnInf, +0.0d, true},
            {-0.0d, dpInf, true},
            {dpInf, -0.0d, false},
            {-0.0d, dnInf, false},
            {dnInf, -0.0d, true}
        };
    }

    @DataProvider(name="nanProviderFD")
    public static Object[][] nanProviderFD(){
        Object[][] constNanPairs = new Object[][] {
            {fqNaN, fqNaN, fqNaNBits},
            {fsNaN, fsNaN, fsNaNBits},
            {fqNaN, fsNaN, fqNaNBits},
            {fsNaN, fqNaN, fsNaNBits},

            {+0.0f, fqNaN, fqNaNBits},
            {fqNaN, +0.0f, fqNaNBits},
            {+0.0f, fsNaN, fsNaNBits},
            {fsNaN, +0.0f, fsNaNBits},
            {-0.0f, fqNaN, fqNaNBits},
            {fqNaN, -0.0f, fqNaNBits},
            {-0.0f, fsNaN, fsNaNBits},
            {fsNaN, -0.0f, fsNaNBits},

            {dqNaN, dqNaN, dqNaNBits},
            {dsNaN, dsNaN, dsNaNBits},
            {dqNaN, dsNaN, dqNaNBits},
            {dsNaN, dqNaN, dsNaNBits},

            {+0.0d, dqNaN, dqNaNBits},
            {dqNaN, +0.0d, dqNaNBits},
            {+0.0d, dsNaN, dsNaNBits},
            {dsNaN, +0.0d, dsNaNBits},
            {-0.0d, dqNaN, dqNaNBits},
            {dqNaN, -0.0d, dqNaNBits},
            {-0.0d, dsNaN, dsNaNBits},
            {dsNaN, -0.0d, dsNaNBits}
        };

        Object[][] nanPairs = new Object[60][3];
        int i = 0;
        while (i < constNanPairs.length) {
            nanPairs[i] = constNanPairs[i];
            i++;
        }
        while (i < nanPairs.length) {
            NaNTestPair<Float, Integer> fpair =  getFloatNaNPair();
            NaNTestPair<Double, Long> dpair =  getDoubleNaNPair();
            if (fpair == null) {
                continue;
            }
            nanPairs[i][0] = fpair.first;
            nanPairs[i][1] = fpair.second;
            nanPairs[i][2] = fpair.expected;
            i++;
            if (dpair == null | i >= nanPairs.length){
                continue;
            }
            nanPairs[i][0] = dpair.first;
            nanPairs[i][1] = dpair.second;
            nanPairs[i][2] = dpair.expected;
            i++;
        }
        return nanPairs;
    }

    @DataProvider(name="normalNumberProviderFD")
    public static Object[][] normalNumberProviderFD() {
        Object [][] numList = new Object[20][2];
        for (int i = 0; i < numList.length / 2; i++) {
            float a = r.nextFloat() * 1000000.0f;
            float b = r.nextFloat() * 1000000.0f;
            int sign1 = r.nextInt(2) == 0 ? -1 : 1;
            int sign2 = r.nextInt(2) == 0 ? -1 : 1;
            numList[i][0] = a * sign1;
            numList[i][1] = b * sign2;
        }
        for (int i = numList.length / 2; i < 20; i++) {
            double a = r.nextDouble() * 1000000000.0d;
            double b = r.nextDouble() * 1000000000.0d;
            int sign1 = r.nextInt(2) == 0 ? -1 : 1;
            int sign2 = r.nextInt(2) == 0 ? -1 : 1;
            numList[i][0] = a * sign1;
            numList[i][1] = b * sign2;
        }
        return numList;
    }

    private static NaNTestPair<Double, Long> getDoubleNaNPair(){
        NaNTestPair<Double, Long> p = new NaNTestPair<Double, Long>();
        double farg1, farg2;
        farg1 = farg2 = 0.0f;

        long iarg1;
        long min = 1L << 13;
        do {
            iarg1 = r.nextLong();
        } while (iarg1 < min);
        // shift right so that mantisa is non-zero
        // shift of 13 instead of 12 ensures that the quiet bit is not set
        iarg1 = iarg1 >> 13;

        int arg1Type = r.nextInt(3); // 0 -> quiet, 1 -> signalling
        if (arg1Type == 0) {
            iarg1 = iarg1 | dNaNExpStart | dquietBit;
            farg1 = Double.longBitsToDouble(iarg1);
        } else if (arg1Type == 1) {
            iarg1 = iarg1 | dNaNExpStart;
            farg1 = Double.longBitsToDouble(iarg1);
        } else {
            // Normal number (non NaN)
            farg1 = r.nextDouble() * 1000000.0 + 1.0;
        }

        // arg 2
        long iarg2;
        do {
            iarg2 = r.nextLong();
            iarg2 = iarg2 >> 13;
        } while (iarg2 < min);

        int arg2Type = r.nextInt(3);
        if (arg2Type == 0) {
            iarg2 = iarg2 | dNaNExpStart | dquietBit;
            farg2 = Double.longBitsToDouble(iarg2);
        }
        else if (arg2Type == 1) {
            iarg2 = iarg2 | dNaNExpStart;
            farg2 = Double.longBitsToDouble(iarg2);
        } else {
            if (arg1Type != 2) {
                farg2 = r.nextDouble() * 1000000.0 + 1;
                iarg2 = Double.doubleToRawLongBits(farg2);
            } else {
                return null;
            }
        }

        p.first = farg1;
        p.bFirst = iarg1;
        p.second = farg2;
        p.bSecond = iarg2;
        p.expected = (arg1Type != 2 ? iarg1 : iarg2);
        return p;
    }

    private static NaNTestPair<Float, Integer> getFloatNaNPair(){ // 0: max, 1: min
        NaNTestPair<Float, Integer> p = new NaNTestPair<Float, Integer>();
        float farg1, farg2;
        farg1 = farg2 = 0.0f;

        int iarg1 = r.nextInt(fNaNMantisaMax - 1) + 1;
        int arg1Type = r.nextInt(3); // 0 -> quiet, 1 -> signalling
        if (arg1Type == 0) {
            iarg1 = iarg1 | fNaNExpStart | fquietBit;
            farg1 = Float.intBitsToFloat(iarg1);
        } else if (arg1Type == 1) {
            iarg1 = iarg1 | fNaNExpStart;
            farg1 = Float.intBitsToFloat(iarg1);
        } else {
            // Normal number (non NaN)
            farg1 = r.nextFloat() * 1000000.0f + 1.0f;
        }

        // arg 2
        int arg2Type = r.nextInt(3);
        int iarg2 = r.nextInt(fNaNMantisaMax - 1) + 1;
        if (arg2Type == 0) {
            iarg2 = (r.nextInt(fNaNMantisaMax - 1) + 1) | fNaNExpStart | fquietBit;
            farg2 = Float.intBitsToFloat(iarg2);
        } else if (arg2Type == 1){
            iarg2 = (r.nextInt(fNaNMantisaMax - 1) + 1) | fNaNExpStart;
            farg2 = Float.intBitsToFloat(iarg2);
        } else {
            if (arg1Type != 2) {
                farg2 = r.nextFloat() * 1000000.0f + 1f;
                iarg2 = Float.floatToRawIntBits(farg2);
            } else {
                return null;
            }
        }

        p.first = farg1;
        p.second = farg2;
        p.bFirst = iarg1;
        p.bSecond = iarg2;
        p.expected = (arg1Type != 2 ? iarg1 : iarg2);
        return p;
    }

    public static void assertEquals(float actual, float expected){
        AssertJUnit.assertEquals(Float.floatToRawIntBits(expected), Float.floatToRawIntBits(actual));
    }

    public static void assertEquals(double actual, double expected){
        AssertJUnit.assertEquals(Double.doubleToRawLongBits(expected), Double.doubleToRawLongBits(actual));
    }
}
