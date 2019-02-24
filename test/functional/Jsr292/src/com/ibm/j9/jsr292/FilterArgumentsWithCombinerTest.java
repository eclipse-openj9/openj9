/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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
package com.ibm.j9.jsr292;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodHandles.Lookup;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;
import java.lang.reflect.Method;
import java.util.Arrays;

/* Tests MethodHandles.filterArgumentsWithCombiner() with the following FieldTypes for (Parameter/Return Descriptors): Z, B, S, C, I, F, J, D, Ljava/lang/Double;
 *
 * For each FieldType, MethodHandles.foldArguments() is tested for boundary and intermediate conditions. Combiner placement and argument indices are also verified.
 */

 public class FilterArgumentsWithCombinerTest {
    /* test values */
    private final static byte constByte1 = (byte) 10;
    private final static byte constByte2 = (byte) -10;
    private final static short constShort1 = (short) -12096;
    private final static short constShort2 = (short) -21510;
    private final static char constChar1 = (char) 31911;
    private final static char constChar2 = (char) 3763;
    private final static int constInt1 = (int) 1213988863;
    private final static int constInt2 = (int) -1993736192;
    private final static long constLong1 = (long) 4.21368040135852e+018;
    private final static long constLong2 = (long) 1.26663739519795e+018;
    private final static float constFloat1 = (float) 1.53640056404114e+038F;
    private final static float constFloat2 = (float) 6.12483306976318e+037F;
    private final static double constDouble1 = (double) 7.33110759312901e+307;
    private final static double constDouble2 = (double) 1.10819706189633e+307;
    private final static boolean constBoolMax = true;
    private final static boolean constBoolMin = false;

    private static Class<?> mhClazz = MethodHandles.class;
    private static Method fawcMethod;

    static {
        try {
            fawcMethod = mhClazz.getDeclaredMethod("filterArgumentsWithCombiner", MethodHandle.class, int.class, MethodHandle.class, int[].class);
            fawcMethod.setAccessible(true);
        } catch(Throwable e) {
            AssertJUnit.fail("FilterArgumentsWithCombinerTest setup failure" + e.getMessage());
        }
    }

    /* test filterArgumentsWithCombiner for data type byte */
    @Test(groups = { "level.extended" })
    public void test_filterArgumentsWithCombiner_Byte() throws WrongMethodTypeException, Throwable {
        /* handle & filter */
        MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "targetByte", MethodType.methodType(byte[].class, byte.class, byte.class, byte.class));
        MethodHandle combiner = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "combinerByte", MethodType.methodType(byte.class, byte.class, byte.class));

        /* verify maximum boundary, filter position is the middle handle parameter */
        MethodHandle handle = (MethodHandle)fawcMethod.invoke(mhClazz, target, 1, combiner, new int[]{2, 0});
        byte[] result = (byte[])handle.invokeExact(Byte.MAX_VALUE, constByte2, Byte.MAX_VALUE);
        AssertJUnit.assertTrue(Arrays.equals(new byte[]{Byte.MAX_VALUE, Byte.MAX_VALUE, Byte.MAX_VALUE}, result));

        /* verify minimum boundary, filter position is first handle parameter */
        MethodHandle handle2 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 0, combiner, new int[]{0, 2}); 
        byte[] result2 = (byte[])handle2.invokeExact(constByte2, Byte.MIN_VALUE, constByte1);
        AssertJUnit.assertTrue(Arrays.equals(new byte[]{constByte1, Byte.MIN_VALUE, constByte1}, result2));

        /* verify medium boundary, filter position is last handle parameter */
        MethodHandle handle3 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 2, combiner, new int[]{1, 2});
        byte[] result3 = (byte[])handle3.invokeExact(Byte.MAX_VALUE, constByte1, constByte2);
        AssertJUnit.assertTrue(Arrays.equals(new byte[]{Byte.MAX_VALUE, constByte1, constByte1}, result3));
    }

    /* combiner returns highest byte value */
    public static byte combinerByte(byte v1, byte v2) {
        return (v1 > v2) ? v1 : v2;
    }

    /* return an array of the arguments */
    public static byte[] targetByte(byte v1, byte v2, byte v3) {
       return new byte[]{v1, v2, v3};
    }

    /* test filterArgumentsWithCombiner for data type short */
    @Test(groups = { "level.extended" })
    public void test_filterArgumentsWithCombiner_Short() throws WrongMethodTypeException, Throwable {
        /* handle & filter */
        MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "targetShort", MethodType.methodType(short[].class, short.class, short.class, short.class));
        MethodHandle combiner = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "combinerShort", MethodType.methodType(short.class, short.class, short.class));

        /* verify maximum boundary, filter position is the middle handle parameter */
        MethodHandle handle = (MethodHandle)fawcMethod.invoke(mhClazz, target, 1, combiner, new int[]{2, 0});
        short[] result = (short[])handle.invokeExact(Short.MAX_VALUE, constShort2, Short.MAX_VALUE);
        AssertJUnit.assertTrue(Arrays.equals(new short[]{Short.MAX_VALUE, Short.MAX_VALUE, Short.MAX_VALUE}, result));

        /* verify minimum boundary, filter position is first handle parameter */
        MethodHandle handle2 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 0, combiner, new int[]{0, 2});
        short[] result2 = (short[])handle2.invokeExact(constShort2, Short.MIN_VALUE, constShort1);
        AssertJUnit.assertTrue(Arrays.equals(new short[]{constShort1, Short.MIN_VALUE, constShort1}, result2));

        /* verify medium boundary, filter position is last handle parameter */
        MethodHandle handle3 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 2, combiner, new int[]{1, 2});
        short[] result3 = (short[])handle3.invokeExact(Short.MAX_VALUE, constShort1, constShort2);
        AssertJUnit.assertTrue(Arrays.equals(new short[]{Short.MAX_VALUE, constShort1, constShort1}, result3));
    }

    /* combiner returns highest short value */
    public static short combinerShort(short v1, short v2) {
        return (v1 > v2) ? v1 : v2;
    }

    /* return an array of the arguments */
    public static short[] targetShort(short v1, short v2, short v3) {
       return new short[]{v1, v2, v3};
    }

    /* test filterArgumentsWithCombiner for data type char */
    @Test(groups = { "level.extended" })
    public void test_filterArgumentsWithCombiner_Character() throws WrongMethodTypeException, Throwable {
        /* handle & filter */
        MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "targetChar", MethodType.methodType(char[].class, char.class, char.class, char.class));
        MethodHandle combiner = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "combinerChar", MethodType.methodType(char.class, char.class, char.class));

        /* verify maximum boundary, filter position is the middle handle parameter */
        MethodHandle handle = (MethodHandle)fawcMethod.invoke(mhClazz, target, 1, combiner, new int[]{2, 0});
        char[] result = (char[])handle.invokeExact(Character.MAX_VALUE, constChar2, Character.MAX_VALUE);
        AssertJUnit.assertTrue(Arrays.equals(new char[]{Character.MAX_VALUE, Character.MAX_VALUE, Character.MAX_VALUE}, result));

        /* verify minimum boundary, filter position is first handle parameter */
        MethodHandle handle2 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 0, combiner, new int[]{0, 2});
        char[] result2 = (char[])handle2.invokeExact(constChar2, Character.MIN_VALUE, constChar1);
        AssertJUnit.assertTrue(Arrays.equals(new char[]{constChar1, Character.MIN_VALUE, constChar1}, result2));

        /* verify medium boundary, filter position is last handle parameter */
        MethodHandle handle3 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 2, combiner, new int[]{1, 2});
        char[] result3 = (char[])handle3.invokeExact(Character.MAX_VALUE, constChar1, constChar2);
        AssertJUnit.assertTrue(Arrays.equals(new char[]{Character.MAX_VALUE, constChar1, constChar1}, result3));
    }

    /* combiner returns highest char value */
    public static char combinerChar(char v1, char v2) {
        return (v1 > v2) ? v1 : v2;
    }

    /* return an array of the arguments */
    public static char[] targetChar(char v1, char v2, char v3) {
       return new char[]{v1, v2, v3};
    }

    /* test filterArgumentsWithCombiner for data type int */
    @Test(groups = { "level.extended" })
    public void test_filterArgumentsWithCombiner_Integer() throws WrongMethodTypeException, Throwable {
        /* handle & filter */
        MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "targetInt", MethodType.methodType(int[].class, int.class, int.class, int.class));
        MethodHandle combiner = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "combinerInt", MethodType.methodType(int.class, int.class, int.class));

        /* verify maximum boundary, filter position is the middle handle parameter */
        MethodHandle handle = (MethodHandle)fawcMethod.invoke(mhClazz, target, 1, combiner, new int[]{2, 0});
        int[] result = (int[])handle.invokeExact(Integer.MAX_VALUE, constInt2, Integer.MAX_VALUE);
        AssertJUnit.assertTrue(Arrays.equals(new int[]{Integer.MAX_VALUE, Integer.MAX_VALUE, Integer.MAX_VALUE}, result));

        /* verify minimum boundary, filter position is first handle parameter */
        MethodHandle handle2 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 0, combiner, new int[]{0, 2});
        int[] result2 = (int[])handle2.invokeExact(constInt2, Integer.MIN_VALUE, constInt1);
        AssertJUnit.assertTrue(Arrays.equals(new int[]{constInt1, Integer.MIN_VALUE, constInt1}, result2));

        /* verify medium boundary, filter position is last handle parameter */
        MethodHandle handle3 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 2, combiner, new int[]{1, 2});
        int[] result3 = (int[])handle3.invokeExact(Integer.MAX_VALUE, constInt1, constInt2);
        AssertJUnit.assertTrue(Arrays.equals(new int[]{Integer.MAX_VALUE, constInt1, constInt1}, result3));
    }

    /* combiner returns highest int value */
    public static int combinerInt(int v1, int v2) {
        return (v1 > v2) ? v1 : v2;
    }

    /* return an array of the arguments */
    public static int[] targetInt(int v1, int v2, int v3) {
       return new int[]{v1, v2, v3};
    }

    /* test filterArgumentsWithCombiner for data type long */
    @Test(groups = { "level.extended" })
    public void test_filterArgumentsWithCombiner_Long() throws WrongMethodTypeException, Throwable {
        /* handle & filter */
        MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "targetLong", MethodType.methodType(long[].class, long.class, long.class, long.class));
        MethodHandle combiner = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "combinerLong", MethodType.methodType(long.class, long.class, long.class));

        /* verify maximum boundary, filter position is the middle handle parameter */
        MethodHandle handle = (MethodHandle)fawcMethod.invoke(mhClazz, target, 1, combiner, new int[]{2, 0});
        long[] result = (long[])handle.invokeExact(Long.MAX_VALUE, constLong2, Long.MAX_VALUE);
        AssertJUnit.assertTrue(Arrays.equals(new long[]{Long.MAX_VALUE, Long.MAX_VALUE, Long.MAX_VALUE}, result));

        /* verify minimum boundary, filter position is first handle parameter */
        MethodHandle handle2 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 0, combiner, new int[]{0, 2});
        long[] result2 = (long[])handle2.invokeExact(constLong2, Long.MIN_VALUE, constLong1);
        AssertJUnit.assertTrue(Arrays.equals(new long[]{constLong1, Long.MIN_VALUE, constLong1}, result2));

        /* verify medium boundary, filter position is last handle parameter */
        MethodHandle handle3 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 2, combiner, new int[]{1, 2});
        long[] result3 = (long[])handle3.invokeExact(Long.MAX_VALUE, constLong1, constLong2);
        AssertJUnit.assertTrue(Arrays.equals(new long[]{Long.MAX_VALUE, constLong1, constLong1}, result3));
    }

    /* combiner returns highest long value */
    public static long combinerLong(long v1, long v2) {
        return (v1 > v2) ? v1 : v2;
    }

    /* return an array of the arguments */
    public static long[] targetLong(long v1, long v2, long v3) {
       return new long[]{v1, v2, v3};
    }

    /* test filterArgumentsWithCombiner for data type float */
    @Test(groups = { "level.extended" })
    public void test_filterArgumentsWithCombiner_Float() throws WrongMethodTypeException, Throwable {
        /* handle & filter */
        MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "targetFloat", MethodType.methodType(float[].class, float.class, float.class, float.class));
        MethodHandle combiner = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "combinerFloat", MethodType.methodType(float.class, float.class, float.class));

        /* verify maximum boundary, filter position is the middle handle parameter */
        MethodHandle handle = (MethodHandle)fawcMethod.invoke(mhClazz, target, 1, combiner, new int[]{2, 0});
        float[] result = (float[])handle.invokeExact(Float.MAX_VALUE, constFloat2, Float.MAX_VALUE);
        AssertJUnit.assertTrue(Arrays.equals(new float[]{Float.MAX_VALUE, Float.MAX_VALUE, Float.MAX_VALUE}, result));

        /* verify minimum boundary, filter position is first handle parameter */
        MethodHandle handle2 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 0, combiner, new int[]{0, 2});
        float[] result2 = (float[])handle2.invokeExact(constFloat2, Float.MIN_VALUE, constFloat1);
        AssertJUnit.assertTrue(Arrays.equals(new float[]{constFloat1, Float.MIN_VALUE, constFloat1}, result2));

        /* verify medium boundary, filter position is last handle parameter */
        MethodHandle handle3 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 2, combiner, new int[]{1, 2});
        float[] result3 = (float[])handle3.invokeExact(Float.MAX_VALUE, constFloat1, constFloat2);
        AssertJUnit.assertTrue(Arrays.equals(new float[]{Float.MAX_VALUE, constFloat1, constFloat1}, result3));
    }

    /* combiner returns highest float value */
    public static float combinerFloat(float v1, float v2) {
        return (v1 > v2) ? v1 : v2;
    }

    /* return an array of the arguments */
    public static float[] targetFloat(float v1, float v2, float v3) {
       return new float[]{v1, v2, v3};
    }

    /* test filterArgumentsWithCombiner for data type double */
    @Test(groups = { "level.extended" })
    public void test_filterArgumentsWithCombiner_Double() throws WrongMethodTypeException, Throwable {
        /* handle & filter */
        MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "targetDouble", MethodType.methodType(double[].class, double.class, double.class, double.class));
        MethodHandle combiner = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "combinerDouble", MethodType.methodType(double.class, double.class, double.class));

        /* verify maximum boundary, filter position is the middle handle parameter */
        MethodHandle handle = (MethodHandle)fawcMethod.invoke(mhClazz, target, 1, combiner, new int[]{2, 0});
        double[] result = (double[])handle.invokeExact(Double.MAX_VALUE, constDouble2, Double.MAX_VALUE);
        AssertJUnit.assertTrue(Arrays.equals(new double[]{Double.MAX_VALUE, Double.MAX_VALUE, Double.MAX_VALUE}, result));

        /* verify minimum boundary, filter position is first handle parameter */
        MethodHandle handle2 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 0, combiner, new int[]{0, 2});
        double[] result2 = (double[])handle2.invokeExact(constDouble2, Double.MIN_VALUE, constDouble1);
        AssertJUnit.assertTrue(Arrays.equals(new double[]{constDouble1, Double.MIN_VALUE, constDouble1}, result2));

        /* verify medium boundary, filter position is last handle parameter */
        MethodHandle handle3 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 2, combiner, new int[]{1, 2});
        double[] result3 = (double[])handle3.invokeExact(Double.MAX_VALUE, constDouble1, constDouble2);
        AssertJUnit.assertTrue(Arrays.equals(new double[]{Double.MAX_VALUE, constDouble1, constDouble1}, result3));
    }

    /* combiner returns highest double value */
    public static double combinerDouble(double v1, double v2) {
        return (v1 > v2) ? v1 : v2;
    }

    /* return an array of the arguments */
    public static double[] targetDouble(double v1, double v2, double v3) {
       return new double[]{v1, v2, v3};
    }

    /* test filterArgumentsWithCombiner for java.lang.Double */
    @Test(groups = { "level.extended" })
    public void test_filterArgumentsWithCombiner_DoubleObject() throws WrongMethodTypeException, Throwable {
        /* object constants */
        Double d1 = new Double (constDouble1);
        Double d2 = new Double (constDouble2);
        Double dMax = new Double (Double.MAX_VALUE);
        Double dMin = new Double (Double.MIN_VALUE);

        /* handle & filter */
        MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "targetJLDouble", MethodType.methodType(Double[].class, Double.class, Double.class, Double.class));
        MethodHandle combiner = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "combinerJLDouble", MethodType.methodType(Double.class, Double.class, Double.class));

        /* verify maximum boundary, filter position is the middle handle parameter */
        MethodHandle handle = (MethodHandle)fawcMethod.invoke(mhClazz, target, 1, combiner, new int[]{2, 0});
        Double[] result = (Double[])handle.invokeExact(dMax, d2, dMax);
        AssertJUnit.assertTrue(Arrays.equals(new Double[]{dMax, dMax, dMax}, result));

        /* verify minimum boundary, filter position is first handle parameter */
        MethodHandle handle2 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 0, combiner, new int[]{0, 2});
        Double[] result2 = (Double[])handle2.invokeExact(d2, dMin, d1);
        AssertJUnit.assertTrue(Arrays.equals(new Double[]{d1, dMin, d1}, result2));

        /* verify medium boundary, filter position is last handle parameter */
        MethodHandle handle3 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 2, combiner, new int[]{1, 2});
        Double[] result3 = (Double[])handle3.invokeExact(dMax, d1, d2);
        AssertJUnit.assertTrue(Arrays.equals(new Double[]{dMax, d1, d1}, result3));
    }

    /* combiner returns highest java.lang.Double value */
    public static Double combinerJLDouble(Double v1O, Double v2O) {
        return (v1O.doubleValue() > v2O.doubleValue()) ? v1O : v2O;
    }

    /* return an array of the arguments */
    public static Double[] targetJLDouble(Double v1O, Double v2O, Double v3O) {
        return new Double[]{v1O, v2O, v3O};
    }

    /* test filterArgumentsWithCombiner for data type boolean */
    @Test(groups = { "level.extended" })
    public void test_filterArgumentsWithCombiner_Boolean() throws WrongMethodTypeException, Throwable {
        /* handle & filter */
        MethodHandle target = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "targetBoolean", MethodType.methodType(boolean[].class, boolean.class, boolean.class, boolean.class));
        MethodHandle combiner = MethodHandles.publicLookup().findStatic(FilterArgumentsWithCombinerTest.class, 
            "combinerBoolean", MethodType.methodType(boolean.class, boolean.class, boolean.class));

        /* filter position is the middle handle parameter */
        MethodHandle handle = (MethodHandle)fawcMethod.invoke(mhClazz, target, 1, combiner, new int[]{2, 0});
        boolean[] result = (boolean[])handle.invokeExact(constBoolMax, constBoolMin, constBoolMax);
        AssertJUnit.assertTrue(Arrays.equals(new boolean[]{constBoolMax, constBoolMax, constBoolMax}, result));

        /* filter position is first handle parameter */
        MethodHandle handle2 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 0, combiner, new int[]{0, 2});
        boolean[] result2 = (boolean[])handle2.invokeExact(constBoolMax, constBoolMax, constBoolMin);
        AssertJUnit.assertTrue(Arrays.equals(new boolean[]{constBoolMin, constBoolMax, constBoolMin}, result2));

        /* filter position is last handle parameter */
        MethodHandle handle3 = (MethodHandle)fawcMethod.invoke(mhClazz, target, 2, combiner, new int[]{1, 2});
        boolean[] result3 = (boolean[])handle3.invokeExact(constBoolMax, constBoolMin, constBoolMin);
        AssertJUnit.assertTrue(Arrays.equals(new boolean[]{constBoolMax, constBoolMin, constBoolMin}, result3));
    }

    /* return the last boolean argument */
    public static boolean combinerBoolean(boolean v1, boolean v2) {
        return v2;
    }

    /* return an array of the arguments */
    public static boolean[] targetBoolean(boolean v1, boolean v2, boolean v3) {
        return new boolean[]{v1, v2, v3};
    }
 }