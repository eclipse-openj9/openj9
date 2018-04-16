/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
package jit.test.tr.FPSimplify;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.testng.AssertJUnit;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity","component.jit" })
public class Testdmadd {
    private static Logger logger = Logger.getLogger(Testdmadd.class);
    String[] inputString;
    private static double dotprod(double a[], double b[]) {
        double r = 0.0;
        double t = 0.0;
        for (int i = 0; i < a.length; ++i) {
            r += (double)a[i]*b[i];
        }
        return (double)(t+r);

    }
    
    static int initAddLimit(double[] Add,double[] B,double[] C,int size){
      int i,start,end,index;
      double[] values = { 0, 24, 1, -1, 130, -130, Double.MAX_VALUE,-1099511627776.0,
                         Double.MIN_VALUE, Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY,
                         1/Double.NEGATIVE_INFINITY, 1/Double.POSITIVE_INFINITY};

      i = 0;
      for(int j=0; j< values.length;++j){
	 index = i;
         for(;i< index+size;++i){
	   B[i] = B[i%size];
	   C[i] = C[i%size];
           Add[i] = values[j];
	   i++;
         }
      }
      return i;
    }

    static int initLimitPermutations(int[] Aint,double[] A,double[] B,int index){
      int i,start,end;
      double[] values = { 0, 24, 1, -1, 130, -130, Double.MAX_VALUE,
                         Double.MIN_VALUE, Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY,
                         1/Double.NEGATIVE_INFINITY, 1/Double.POSITIVE_INFINITY};
      int[] Intvalues = { 0, 24, 1, -1, 130, -130, Integer.MAX_VALUE,
                         Integer.MIN_VALUE, Integer.MIN_VALUE, Integer.MIN_VALUE,
                         Integer.MIN_VALUE, Integer.MIN_VALUE};

      for(int j=0; j< values.length;++j){
         start = index;
         end=initLimit(B,start);
         for(i=start;i< end;++i)
           A[index] = values[j];
           Aint[index] = Intvalues[j];
	   ++index;
         }
      return index;
    }

    static int initLimit(double[] array, int index){
      array[index++] = 0;
      array[index++] = 24;
      array[index++] = 1;
      array[index++] = -1;
      array[index++] =  130;
      array[index++] = -130;
      array[index++] = Double.MAX_VALUE;
      array[index++] = Double.MIN_VALUE;
      array[index++] = Double.NEGATIVE_INFINITY;
      array[index++] = Double.POSITIVE_INFINITY;
      array[index++] = 1/Double.NEGATIVE_INFINITY; // -0
      array[index++] = 1/Double.POSITIVE_INFINITY; // +0
      return index;
    }

    /**
     * fill in arrays with limits and double values and create a
     * results array.  Take care that these are the 'real' results
     * that the JIT doesn't optimize ie that.  Assumed all arrays
     * are the same length, and are at least
     */
    static int initializeArray(int[] Aint,double[] A,double[] B,double[] C,double[] addResult,
			       double[] subResult, double[] intAddResult, double[] intSubResult,
			       double[] constaddResult){
      int arrayLength = A.length;
      // first, lets fill in a bunch of limits
      int lastIndex = initLimitPermutations(Aint,A,B,0);
      int lastAddIndex = initAddLimit(C,A,B,lastIndex);
	
      for(int i=0;i< lastAddIndex;++i){
	constaddResult[i] = A[i] * 24;
	subResult[i] = addResult[i] = A[i] * B[i];
	intSubResult[i] = intAddResult[i] = Aint[i] * B[i];
      }
      java.util.Random randGen = new java.util.Random(lastAddIndex);
      int shiftCount=1;
      for(int i= lastAddIndex; i < addResult.length;++i){
	int aInt = randGen.nextInt();
	double  a = randGen.nextDouble();
	double  b = randGen.nextDouble();
	double  c = randGen.nextDouble();
	Aint[i] = aInt;
	A[i] = a;
	B[i] = b;
	C[i] = c;
	subResult[i] = addResult[i] = a*b;
	intSubResult[i] = intAddResult[i] = aInt*b;
	constaddResult[i] = a * 24;
      }
      // some instructions to prevent fusing--just swap two elements
      double t = C[103];
      C[103] = C[202];;
      C[202] = t;

      lastAddIndex = addResult.length;
      // separate into separate array to prevent fmadd
      for(int i=0;i< lastAddIndex;++i){
	constaddResult[i] += C[i];
	subResult[i] = addResult[i] - C[i];
	addResult[i] += C[i];
	intSubResult[i] = intAddResult[i] - C[i];
	intAddResult[i] += C[i];
      }
      return lastAddIndex;
    }


    private static double negevalsub1a(double a, double b, double c) {
        return -(a*2.0f-c);
    }
    private static double evalsub1a(double a, double b, double c) {
        return a*2.0f-c;
    }
    private static double evalsub2a(double a, double b) {
        return a*2.0f;
    }
    private static double evalsub3a(double a, double b) {
        return a-b;
    }
    private static double negevalsub4a(double a, double b, double c) {
        return -(evalsub2a(a,2.0f)-c);
    }
    private static double negevalsub5a(double a, double b, double c) {
        return -evalsub3a(a*2.0f,c);
    }
    private static double evalsub4a(double a, double b, double c) {
        return evalsub2a(a,2.0f)-c;
    }
    private static double evalsub5a(double a, double b, double c) {
        return evalsub3a(a*2.0f,c);
    }

    private static double negeval1a(double a, double b, double c) {
        return -(a*2.0f+c);
    }
    private static double eval1a(double a, double b, double c) {
        return a*2.0f+c;
    }
    private static double eval2a(double a, double b) {
        return a*2.0f;
    }
    private static double eval3a(double a, double b) {
        return a+b;
    }
    private static double negeval4a(double a, double b, double c) {
        return -(eval2a(a,2.0f)+c);
    }
    private static double negeval5a(double a, double b, double c) {
        return -eval3a(a*2.0f,c);
    }
    private static double eval4a(double a, double b, double c) {
        return eval2a(a,2.0f)+c;
    }
    private static double eval5a(double a, double b, double c) {
        return eval3a(a*2.0f,c);
    }
    static void performIntSubTest(int a,double b,double c,double subResult){
      double d = (a*b)-c;
      logger.debug("A "+a+" B:"+b+" C:"+c+" D:"+d);
      if(!Double.isNaN(d) && !Double.isNaN(subResult)){
	AssertJUnit.assertEquals(subResult,d,0);
	AssertJUnit.assertEquals(1/subResult,1/d,0);
        if(subResult!=d) logger.error("int subResults don't match for "+d+" != "+subResult);
        if(1/subResult!=1/d) logger.error("int subResults don't match for  1/"+d+" != 1/"+subResult);
      }
      else if(Double.isNaN(d) != Double.isNaN(subResult))
        AssertJUnit.assertTrue("sub NaNs don't match for "+d+" != "+subResult,false);

      double e = -((a*b)-c);
      logger.debug("A "+a+" B:"+b+" C:"+c+" D:"+d);
      if(!Double.isNaN(e) && !Double.isNaN(subResult)){
	AssertJUnit.assertEquals(-subResult,e,0);
	AssertJUnit.assertEquals(-1/subResult,1/e,0);
      }
      else 
        AssertJUnit.assertTrue("sub NaNs don't match for "+e+" != "+subResult,
                   (Double.isNaN(e) == Double.isNaN(subResult)));
    }
    static void performSubTest(double a,double b,double c,double subResult){
      double d = (a*b)-c;
      logger.debug("A "+a+" B:"+b+" C:"+c+" D:"+d);
      if(!Double.isNaN(d) && !Double.isNaN(subResult)){
	AssertJUnit.assertEquals(subResult,d,0);
	AssertJUnit.assertEquals(1/subResult,1/d,0);
      }
      else AssertJUnit.assertTrue(Double.isNaN(d) == Double.isNaN(subResult));
    }
 
    // Defect 81236: Add strictfp modifier. This method is expecting a strictfp 
    // result for (Double.MAX_VALUE * 24) + Double.NEGATIVE_INFINITY.
    static strictfp void performConstAddTest(double a,double c,double addResult){
      double d = (a*24)+c;
      if(!Double.isNaN(d) && !Double.isNaN(addResult)){
        AssertJUnit.assertEquals(addResult,d,0);
        AssertJUnit.assertEquals(1/addResult,1/d,0);
      }
      else AssertJUnit.assertTrue(Double.isNaN(d) == Double.isNaN(addResult));
    }

    static void performAddTest(double a,double b,double c,double addResult){
      double d = (a*b)+c;
      logger.debug("A "+a+" B:"+b+" C:"+c+" D:"+d);
      if(!Double.isNaN(d) && !Double.isNaN(addResult)){
	AssertJUnit.assertEquals(addResult,d,0);
	AssertJUnit.assertEquals(1/addResult,1/d,0);
      }
      else AssertJUnit.assertTrue(Double.isNaN(d) == Double.isNaN(addResult));
    }

    static void performIntAddTest(int a,double b,double c,double addResult){
      double d = (a*b)+c;
      logger.debug("A "+a+" B:"+b+" C:"+c+" D:"+d);
      if(!Double.isNaN(d) && !Double.isNaN(addResult)){
	AssertJUnit.assertEquals(addResult,d,0);
	AssertJUnit.assertEquals(1/addResult,1/d,0);
      }
      else AssertJUnit.assertTrue(Double.isNaN(d) == Double.isNaN(addResult));
    }

  static int initializeShortArray(short[] shortArrayA,double[] dblArrayC,double[] s2dResults){
    int i=0;
    shortArrayA[i] = Short.MAX_VALUE;
    dblArrayC[i] = Double.MAX_VALUE;
    s2dResults[i] = (Short.MAX_VALUE * 2.1) + Double.MAX_VALUE;
    ++i;

    shortArrayA[i] = Short.MAX_VALUE;
    dblArrayC[i] = Double.MAX_VALUE;
    s2dResults[i] = (Short.MAX_VALUE * -2.1) + Double.MAX_VALUE;
    ++i;

    shortArrayA[i] = Short.MIN_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    s2dResults[i] = (Short.MIN_VALUE * 2.1) + Double.MIN_VALUE;
    ++i;

    shortArrayA[i] = Short.MIN_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    s2dResults[i] = (Short.MIN_VALUE * -2.1) + Double.MIN_VALUE;
    ++i;

    shortArrayA[i] = Short.MAX_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    s2dResults[i] = (Short.MAX_VALUE * -2.1) + Double.MIN_VALUE;
    ++i;

    shortArrayA[i] = Short.MAX_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    s2dResults[i] = (Short.MAX_VALUE * 2.1) + Double.MIN_VALUE;
    ++i;

    shortArrayA[i] = Short.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    s2dResults[i] = (Short.MAX_VALUE * 274877906943.0) + 2005.0;
    ++i;

    shortArrayA[i] = Short.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    s2dResults[i] = (Short.MAX_VALUE * 412316860415.0) + 2005.0;
    ++i;

    shortArrayA[i] = Short.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    s2dResults[i] = (Short.MAX_VALUE * 549755813887.0) + 2005.0;
    ++i;
    return i;
  }
  static int initializeCharArray(char[] charArrayA,double[] dblArrayC,double[] c2dResults){
    int i=0;
    charArrayA[i] = Character.MAX_VALUE;
    dblArrayC[i] = Double.MAX_VALUE;
    c2dResults[i] = (Character.MAX_VALUE * 2.1) + Double.MAX_VALUE;
    ++i;

    charArrayA[i] = Character.MAX_VALUE;
    dblArrayC[i] = Double.MAX_VALUE;
    c2dResults[i] = (Character.MAX_VALUE * -2.1) + Double.MAX_VALUE;
    ++i;

    charArrayA[i] = Character.MIN_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    c2dResults[i] = (Character.MIN_VALUE * 2.1) + Double.MIN_VALUE;
    ++i;

    charArrayA[i] = Character.MIN_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    c2dResults[i] = (Character.MIN_VALUE * -2.1) + Double.MIN_VALUE;
    ++i;

    charArrayA[i] = Character.MAX_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    c2dResults[i] = (Character.MAX_VALUE * -2.1) + Double.MIN_VALUE;
    ++i;

    charArrayA[i] = Character.MAX_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    c2dResults[i] = (Character.MAX_VALUE * 2.1) + Double.MIN_VALUE;
    ++i;

    charArrayA[i] = Character.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    c2dResults[i] = (Character.MAX_VALUE * 274877906943.0) + 2005.0;
    ++i;

    charArrayA[i] = Character.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    c2dResults[i] = (Character.MAX_VALUE * 412316860415.0) + 2005.0;
    ++i;

    charArrayA[i] = Character.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    c2dResults[i] = (Character.MAX_VALUE * 549755813887.0) + 2005.0;
    ++i;
    return i;
  }
  static int initializeByteArray(byte[] byteArrayA,double[] dblArrayC,double[] b2dResults){
    int i=0;
    byteArrayA[i] = Byte.MAX_VALUE;
    dblArrayC[i] = Double.MAX_VALUE;
    b2dResults[i] = (Byte.MAX_VALUE * 2.1) + Double.MAX_VALUE;
    ++i;

    byteArrayA[i] = Byte.MAX_VALUE;
    dblArrayC[i] = Double.MAX_VALUE;
    b2dResults[i] = (Byte.MAX_VALUE * -2.1) + Double.MAX_VALUE;
    ++i;

    byteArrayA[i] = Byte.MIN_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    b2dResults[i] = (Byte.MIN_VALUE * 2.1) + Double.MIN_VALUE;
    ++i;

    byteArrayA[i] = Byte.MIN_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    b2dResults[i] = (Byte.MIN_VALUE * -2.1) + Double.MIN_VALUE;
    ++i;

    byteArrayA[i] = Byte.MAX_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    b2dResults[i] = (Byte.MAX_VALUE * -2.1) + Double.MIN_VALUE;
    ++i;

    byteArrayA[i] = Byte.MAX_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    b2dResults[i] = (Byte.MAX_VALUE * 2.1) + Double.MIN_VALUE;
    ++i;

    byteArrayA[i] = Byte.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    b2dResults[i] = (Byte.MAX_VALUE * 17592186044415.0) + 2005.0;
    ++i;

    byteArrayA[i] = Byte.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    b2dResults[i] = (Byte.MAX_VALUE * 35184372088831.0) + 2005.0;
    ++i;

    byteArrayA[i] = Byte.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    b2dResults[i] = (Byte.MAX_VALUE * 70368744177663.0) + 2005.0;
    ++i;
    return i;
  }

  // play with the constants with # bits <  and > 53-23 (
  static int initializeFltArray(float[] fltArrayA,double[] dblArrayC,double[] f2dResults){
    int i=0;
    fltArrayA[i] = Float.MAX_VALUE;
    dblArrayC[i] = Double.MAX_VALUE;
    f2dResults[i] = (Float.MAX_VALUE * 2.1) + Double.MAX_VALUE;
    ++i;

    fltArrayA[i] = Float.MAX_VALUE;
    dblArrayC[i] = Double.MAX_VALUE;
    f2dResults[i] = (Float.MAX_VALUE * -2.1) + Double.MAX_VALUE;
    ++i;

    fltArrayA[i] = Float.MIN_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    f2dResults[i] = (Float.MIN_VALUE * 2.1) + Double.MIN_VALUE;
    ++i;

    fltArrayA[i] = Float.MIN_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    f2dResults[i] = (Float.MIN_VALUE * -2.1) + Double.MIN_VALUE;
    ++i;

    fltArrayA[i] = Float.MAX_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    f2dResults[i] = (Float.MAX_VALUE * -2.1) + Double.MIN_VALUE;
    ++i;

    fltArrayA[i] = Float.MAX_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    f2dResults[i] = (Float.MAX_VALUE * 2.1) + Double.MIN_VALUE;
    ++i;

    fltArrayA[i] = Float.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    f2dResults[i] = (Float.MAX_VALUE * (536870911.0/*0x1fffffff*/)) + 2005.0;
    ++i;

    fltArrayA[i] = Float.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    f2dResults[i] = (Float.MAX_VALUE * (1073741823.0/*0x3fffffff*/)) + 2005.0;
    ++i;

    fltArrayA[i] = Float.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    f2dResults[i] = (Float.MAX_VALUE * (2147483647.0/*0x7fffffff*/)) + 2005.0;
    ++i;
    return i;
  }

  // play with the constants with # bits <  and > 53-31 (
  static int initializeIntArray(int[] intArrayA,double[] dblArrayC,double[] i2dResults){
    int i=0;
    intArrayA[i] = Integer.MAX_VALUE;
    dblArrayC[i] = Double.MAX_VALUE;
    i2dResults[i] = (Integer.MAX_VALUE * 2.1) + Double.MAX_VALUE;
    ++i;

    intArrayA[i] = Integer.MAX_VALUE;
    dblArrayC[i] = Double.MAX_VALUE;
    i2dResults[i] = (Integer.MAX_VALUE * -2.1) + Double.MAX_VALUE;
    ++i;

    intArrayA[i] = Integer.MIN_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    i2dResults[i] = (Integer.MIN_VALUE * 2.1) + Double.MIN_VALUE;
    ++i;

    intArrayA[i] = Integer.MIN_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    i2dResults[i] = (Integer.MIN_VALUE * -2.1) + Double.MIN_VALUE;
    ++i;

    intArrayA[i] = Integer.MAX_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    i2dResults[i] = (Integer.MAX_VALUE * -2.1) + Double.MIN_VALUE;
    ++i;

    intArrayA[i] = Integer.MAX_VALUE;
    dblArrayC[i] = Double.MIN_VALUE;
    i2dResults[i] = (Integer.MAX_VALUE * 2.1) + Double.MIN_VALUE;
    ++i;

    intArrayA[i] = Integer.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    i2dResults[i] = (Integer.MAX_VALUE * (4292935648.0/*0xffe0ffe0*/)) + 2005.0;
    ++i;

    intArrayA[i] = Integer.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    i2dResults[i] = (Integer.MAX_VALUE * (4292935649.0/*0xffe0ffe1*/)) + 2005.0;
    ++i;

    intArrayA[i] = Integer.MAX_VALUE;
    dblArrayC[i] = 2005.0;
    i2dResults[i] = (Integer.MAX_VALUE * (4292935663.0/*0xffe0ffef*/)) +2005.0;
    ++i;
    return i;
  }

  static void checkIt(double a,double b){
   logger.debug("A "+a+" B:"+b);
   if(!Double.isNaN(a) && !Double.isNaN(b)){
     AssertJUnit.assertEquals(b,a,0);
     AssertJUnit.assertEquals(1/b,1/a,0);
   }
   else AssertJUnit.assertTrue(Double.isNaN(b) == Double.isNaN(a));
  }
  static void performf2dTest(float[] a,double[] c,double[] expectedValue){
      int i=0;
      double d;
      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;
      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*(double)(536870911.0))+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*(1073741823.0))+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*(double)(2147483647.0))+c[i];
      checkIt(d,expectedValue[i]);
  }
  static void performi2dTest(int[] a,double[] c,double[] expectedValue){
      int i=0;
      double d;
      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;
      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*(double)(4292935648.0))+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*(double)(4292935649.0))+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*(double)(4292935663.0))+c[i];
      checkIt(d,expectedValue[i]);
  }
  // should not happen
  static void performl2dTest(long[] a,double[] c,double[] expectedValue){
      int i=0;
      double d;
      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*(double)(4292935648.0))+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*(double)(4292935649.0))+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*(double)(4292935663.0))+c[i];
      checkIt(d,expectedValue[i]);
  }

  static void performs2dTest(short[] a,double[] c,double[] expectedValue){
      int i=0;
      double d;
      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;
      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*274877906943.0)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*412316860415.0)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*549755813887.0)+c[i];
      checkIt(d,expectedValue[i]);
  }
  static void performc2dTest(char[] a,double[] c,double[] expectedValue){
      int i=0;
      double d;
      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;
      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*274877906943.0)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*412316860415.0)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*549755813887.0)+c[i];
      checkIt(d,expectedValue[i]);
  }
  static void performb2dTest(byte[] a,double[] c,double[] expectedValue){
      int i=0;
      double d;
      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;
      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*17592186044415.0)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*35184372088831.0)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*70368744177663.0)+c[i];
      checkIt(d,expectedValue[i]);
  }

  static void performc2dTest(short[] a,double[] c,double[] expectedValue){
      int i=0;
      double d;
      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*-2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*2.1)+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*(double)(4292935648.0))+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*(double)(4292935649.0))+c[i];
      checkIt(d,expectedValue[i]);
      ++i;

      d = (a[i]*(double)(4292935663.0))+c[i];
      checkIt(d,expectedValue[i]);
  }

  public Testdmadd(String x){inputString = new String[1];}

    private double [] _A1;
    private int   []  _Aint;
    private double [] _B1;
    private double [] _C1;
    private double [] _addResult;
    private double [] _subResult;
    private double [] _addIntResult;
    private double [] _subIntResult;
    private double [] _constaddResult;

    private double [] _i2dResults;
    private double [] _f2dResults;
    private double [] _b2dResults;
    private double [] _c2dResults;
    private double [] _s2dResults;
    private int    [] _intArrayA;
    private float  [] _fltArrayA;
    private double [] _dblArrayC;
    private byte   [] _byteArrayA;
    private char   [] _charArrayA;
    private short  [] _shortArrayA;
    private double [] negfmaAddResult;
    private double [] negfmaSubResult;
    private double [] negfmaMulResult;
    private double [] fmaNegSubResult;

  @BeforeMethod
  protected void setUp(){
    final int arraySize = /*min size*/12*12*12 + 10;
    _A1 = new double[arraySize]; 
    _Aint = new int[arraySize]; 
    _B1 = new double[arraySize]; 
    _C1 = new double[arraySize]; 
    _addResult = new double[arraySize];  // A*B+C
    _subResult = new double[arraySize];  // A*B-C
    _addIntResult = new double[arraySize]; // A * (int)B + C
    _subIntResult = new double[arraySize]; // A * (int)B - C
    _constaddResult = new double[arraySize];  // A * const + C

    final int x2dArraySize = 3000;
    _i2dResults = new double[x2dArraySize];
    _f2dResults = new double[x2dArraySize];
    _b2dResults = new double[x2dArraySize];
    _c2dResults = new double[x2dArraySize];
    _s2dResults = new double[x2dArraySize];
    _intArrayA  = new int[x2dArraySize];
    _fltArrayA  = new float[x2dArraySize];
    _dblArrayC  = new double[x2dArraySize];
    _byteArrayA  = new byte[x2dArraySize];
    _charArrayA  = new char[x2dArraySize];
    _shortArrayA  = new short[x2dArraySize];

    initializeIntArray(_intArrayA,_dblArrayC,_i2dResults);
    initializeFltArray(_fltArrayA,_dblArrayC,_f2dResults);
    initializeByteArray(_byteArrayA,_dblArrayC,_b2dResults);
    initializeCharArray(_charArrayA,_dblArrayC,_c2dResults);
    initializeShortArray(_shortArrayA,_dblArrayC,_s2dResults);

    int size = initializeArray(_Aint,_A1,_B1,_C1,_addResult,_subResult,_addIntResult,_subIntResult,
			       _constaddResult);
    initNegFMATests();
  }
  private double[] negFMAAValues = { 
                        Double.POSITIVE_INFINITY,
                        Double.NEGATIVE_INFINITY,
                        1/Double.POSITIVE_INFINITY, // aka +0
                        1/Double.NEGATIVE_INFINITY, // aka -0

                        Double.POSITIVE_INFINITY,
                        Double.NEGATIVE_INFINITY,
                        1/Double.POSITIVE_INFINITY, // aka +0
                        1/Double.NEGATIVE_INFINITY, // aka -0

                        Double.POSITIVE_INFINITY,
                        Double.NEGATIVE_INFINITY,
                        1/Double.POSITIVE_INFINITY, // aka +0
                        1/Double.NEGATIVE_INFINITY, // aka -0

                        Double.POSITIVE_INFINITY,
                        Double.NEGATIVE_INFINITY,
                        1/Double.POSITIVE_INFINITY, // aka +0
                        1/Double.NEGATIVE_INFINITY, // aka -0

                        Double.POSITIVE_INFINITY,
                        Double.NEGATIVE_INFINITY,
                        1/Double.POSITIVE_INFINITY, // aka +0
                        1/Double.NEGATIVE_INFINITY, // aka -0

                        1,
                        100,
                        140,
                        12};
  private double[] negFMABValues = { 
                        Double.POSITIVE_INFINITY,
                        Double.POSITIVE_INFINITY,
                        Double.POSITIVE_INFINITY,
                        Double.POSITIVE_INFINITY,

                        Double.NEGATIVE_INFINITY,
                        Double.NEGATIVE_INFINITY,
                        Double.NEGATIVE_INFINITY,
                        Double.NEGATIVE_INFINITY,

                        1/Double.POSITIVE_INFINITY, // aka +0
                        1/Double.POSITIVE_INFINITY, // aka +0
                        1/Double.POSITIVE_INFINITY, // aka +0
                        1/Double.POSITIVE_INFINITY, // aka +0

                        1/Double.NEGATIVE_INFINITY, // aka -0
                        1/Double.NEGATIVE_INFINITY, // aka -0
                        1/Double.NEGATIVE_INFINITY, // aka -0
                        1/Double.NEGATIVE_INFINITY, // aka -0
                    
                        112,
                        114,
                        512,
                        132,

                        3,
                        4,
                        5,
                        1};
 /**
  * tests for:
    -(a+b) -> nfmadd(a,1.0,b)
    -(a-b) -> nfmasub(a,1.0,b)
    -(a*b) -> nfmasub(a,b,0)
    -a-b   ->  fmasub(a,-1,b) <-- can do this one without negatefma
    Test boundary conditions and a couple samples
  */
 void initNegFMATests(){


   negfmaAddResult = new double[negFMAAValues.length];
   negfmaSubResult = new double[negFMAAValues.length];
   negfmaMulResult = new double[negFMAAValues.length];
   fmaNegSubResult = new double[negFMAAValues.length];
   for(int i=0;i < negFMAAValues.length;++i){
      negfmaAddResult[i] = negFMAAValues[i] + negFMABValues[i];
      negfmaSubResult[i] = negFMAAValues[i] - negFMABValues[i];
      negfmaMulResult[i] = negFMAAValues[i] * negFMABValues[i];
      fmaNegSubResult[i] = -negFMAAValues[i];
   }
   // prevent fusing and the simplification
   AssertJUnit.assertEquals(negFMAAValues.length,negFMABValues.length);
   
   // now negate
   for(int i=0;i < negFMAAValues.length;++i){
      negfmaAddResult[i] = - negfmaAddResult[i];  
      negfmaSubResult[i] = - negfmaSubResult[i];  
      negfmaMulResult[i] = - negfmaMulResult[i];  
      fmaNegSubResult[i] = fmaNegSubResult[i] - negFMABValues[i];
   }
 }
 @Test
 public void runSimplify2FMATests() {
    for(int i=0;i < 100;++i) // ensure it gets compiled
      runNegFMATests();
 }

 public void runNegFMATests() {
   double a,b,c;
   for(int i=0;i < negfmaAddResult.length;++i){
     a = negFMAAValues[i];
     b = negFMABValues[i];
     c = -(a+b);
     
     checkIt(c,negfmaAddResult[i]);
     c = -(a-b);
     checkIt(c,negfmaSubResult[i]);
     c = -(a*b);
     checkIt(c,negfmaMulResult[i]);
     c = (-a)-b;
     checkIt(c,fmaNegSubResult[i]);
   }
 }
 @Test
 public void testFusedMultiplyAddDouble() {

    for(int i=0;i < _A1.length;++i){
       logger.debug("Testing "+i+" A:"+_A1[i]+" B:"+_B1[i]+" C:"+_C1[i]+" expect:"+_addResult[i]);
       // none of these should trigger a conversion
       performAddTest(_A1[i],_B1[i],_C1[i],_addResult[i]);
       performSubTest(_A1[i],_B1[i],_C1[i],_subResult[i]);
       performIntAddTest(_Aint[i],_B1[i],_C1[i],_addIntResult[i]);
       performIntSubTest(_Aint[i],_B1[i],_C1[i],_subIntResult[i]);
       performConstAddTest(_A1[i],_C1[i],_constaddResult[i]);
    }


    }

   @Test
   public void testx2dConverts(){
    for(int j=0;j<600;++j){
      performi2dTest(_intArrayA,_dblArrayC,_i2dResults);
      performf2dTest(_fltArrayA,_dblArrayC,_f2dResults);
      performb2dTest(_byteArrayA,_dblArrayC,_b2dResults);
      performc2dTest(_charArrayA,_dblArrayC,_c2dResults);
      performs2dTest(_shortArrayA,_dblArrayC,_s2dResults);
    }
   }

    @Test
    public void testEvals(){
      double f1 = Double.MAX_VALUE+inputString.length;
      double f2 = 2.1F+inputString.length;
      double f3 = Double.MIN_VALUE+inputString.length;
      for (int i = 0; i < 1000; ++i) {
        testEval(f1,f2,f3);
        testNegEval(f1,f2,f3);
      }
    }  
    void testNegEval(double f1,double f2,double f3){
      double f;
      f = negeval1a(f1, f2, f3);
      AssertJUnit.assertEquals(Double.NEGATIVE_INFINITY,f,0);
      f = negeval4a(f1, f2, f3);
      AssertJUnit.assertEquals(Double.NEGATIVE_INFINITY,f,0);
      f = negeval5a(f1, f2, f3);
      AssertJUnit.assertEquals(Double.NEGATIVE_INFINITY,f,0);

      f = negevalsub1a(f1, f2, f3);
      AssertJUnit.assertEquals(Double.NEGATIVE_INFINITY,f,0);
      f = negevalsub4a(f1, f2, f3);
      AssertJUnit.assertEquals(Double.NEGATIVE_INFINITY,f,0);
      f = negevalsub5a(f1, f2, f3);
      AssertJUnit.assertEquals(Double.NEGATIVE_INFINITY,f,0);
    }
    void testEval(double f1,double f2,double f3){
      double f;
      f = eval1a(f1, f2, f3);
      AssertJUnit.assertEquals(Double.POSITIVE_INFINITY,f,0);
      f = eval4a(f1, f2, f3);
      AssertJUnit.assertEquals(Double.POSITIVE_INFINITY,f,0);
      f = eval5a(f1, f2, f3);
      AssertJUnit.assertEquals(Double.POSITIVE_INFINITY,f,0);

      f = evalsub1a(f1, f2, f3);
      AssertJUnit.assertEquals(Double.POSITIVE_INFINITY,f,0);
      f = evalsub4a(f1, f2, f3);
      AssertJUnit.assertEquals(Double.POSITIVE_INFINITY,f,0);
      f = evalsub5a(f1, f2, f3);
      AssertJUnit.assertEquals(Double.POSITIVE_INFINITY,f,0);
    }
  ///

  @Test
  public void testDotProd(){

    double[] E = new double[10000];
    double[] F = new double[10000];
    java.util.Random randGen = new java.util.Random(1313);
    for(int i= 0; i < E.length;++i){
      E[i] = randGen.nextDouble();
      F[i] = randGen.nextDouble();
    }
    double sum = 0;
    double origsum = dotprod(E,F);// should have interpreted result
    for (int i = 0; i < E.length; ++i) {
       sum = dotprod(E,F);
       AssertJUnit.assertEquals(origsum,sum,0);
       AssertJUnit.assertEquals(1/origsum,1/sum,0);
    }
    logger.debug("dotprod is "+ dotprod(E,F)+" ="+sum);

    // by this point dotprod should be compiled.  Now we must compare against expected results
    double[] AValues = { 0,  Double.MAX_VALUE,
                       Double.MIN_VALUE, Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY,
                       1/Double.NEGATIVE_INFINITY, 1/Double.POSITIVE_INFINITY};


    double[] BValues = { 1,2,3,4,5,6,7};

    double[] CValues = { 528.0, Double.POSITIVE_INFINITY, 528.0, Double.NEGATIVE_INFINITY, Double.POSITIVE_INFINITY, 528.0, 528.0};
   for(int i=0;i < AValues.length;++i) {
     double [] H = new double[2];
     double [] I = new double[2];
     double [] J = new double[2];
     H[0] = 22;
     I[0] = 24;
     H[1] = AValues[i];
     I[1] = BValues[i];
     double result= dotprod(H,I);
     AssertJUnit.assertEquals(CValues[i],result,0);
   }
  }
}
