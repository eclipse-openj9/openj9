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
public class Testfmadd{

    private String[] inputString;
    private float [] A1;
    private int   [] Aint;
    private float [] B1;
    private float [] C1;
    private float [] addResult;
    private float [] subResult;
    private float [] inverseSubResult;
    private float [] addIntResult;
    private float [] subIntResult;
    private float [] constaddResult;
    private float [] negfmaAddResult;
    private float [] negfmaSubResult;
    private float [] negfmaMulResult;
    private float [] fmaNegSubResult;
    private static Logger logger = Logger.getLogger(Testfmadd.class);

    public Testfmadd(String x){ inputString = new String[2];}

    private static float dotprod(float a[], float b[]) {
        double r = 0.0;
        double t = 0.0;
        for (int i = 0; i < a.length; ++i) {
            r += (double)a[i]*b[i];
        }
        return (float)(t+r);
    }
    private static float negdotprod(float a[], float b[]) {
        double r = 0.0;
        double t = 0.0;
        for (int i = 0; i < a.length; ++i) {
            r = r- (double)a[i]*b[i];
        }
        return (float)(t+r);
    }
    
    static int initAddLimit(float[] Add,float[] B,float[] C,int size){
      int i,start,end,index;
      float[] values = { 0, 24, 1, -1, 130, -130, Float.MAX_VALUE,
                         Float.MIN_VALUE, Float.NEGATIVE_INFINITY, Float.POSITIVE_INFINITY,
                         1/Float.NEGATIVE_INFINITY, 1/Float.POSITIVE_INFINITY};

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

    static int initLimitPermutations(int[] Aint,float[] A,float[] B,int index){
      int i,start,end;
      float[] values = { 0, 24, 1, -1, 130, -130, Float.MAX_VALUE,
                         Float.MIN_VALUE, Float.NEGATIVE_INFINITY, Float.POSITIVE_INFINITY,
                         1/Float.NEGATIVE_INFINITY, 1/Float.POSITIVE_INFINITY};
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

    static int initLimit(float[] array, int index){
      array[index++] = 0;
      array[index++] = 24;
      array[index++] = 1;
      array[index++] = -1;
      array[index++] =  130;
      array[index++] = -130;
      array[index++] = Float.MAX_VALUE;
      array[index++] = Float.MIN_VALUE;
      array[index++] = Float.NEGATIVE_INFINITY;
      array[index++] = Float.POSITIVE_INFINITY;
      array[index++] = 1/Float.NEGATIVE_INFINITY; // -0
      array[index++] = 1/Float.POSITIVE_INFINITY; // +0
      return index;
    }

    /**
     * fill in arrays with limits and float values and create a
     * results array.  Take care that these are the 'real' results
     * that the JIT doesn't optimize ie that.  Assumed all arrays
     * are the same length, and are at least
     */
    static int initializeArray(int[] Aint,float[] A,float[] B,float[] C,float[] addResult,
			       float[] subResult, float[] inverseSResult,float[] intAddResult, float[] intSubResult,
			       float[] constaddResult){
      int arrayLength = A.length;
      // first, lets fill in a bunch of limits
      int lastIndex = initLimitPermutations(Aint,A,B,0);
      int lastAddIndex = initAddLimit(C,A,B,lastIndex);
	
      for(int i=0;i< lastAddIndex;++i){
	constaddResult[i] = A[i] * 24;
	subResult[i] = addResult[i] = A[i] * B[i];
	inverseSResult[i] = -addResult[i];
	intSubResult[i] = intAddResult[i] = Aint[i] * B[i];
      }
      java.util.Random randGen = new java.util.Random(lastAddIndex);
      int shiftCount=1;
      for(int i= lastAddIndex; i < addResult.length;++i){
	int aInt = randGen.nextInt();
	float  a = randGen.nextFloat();
	float  b = randGen.nextFloat();
	float  c = randGen.nextFloat();
	Aint[i] = aInt;
	A[i] = a;
	B[i] = b;
	C[i] = c;
	subResult[i] = addResult[i] = a*b;
	inverseSResult[i] = -addResult[i];
	intSubResult[i] = intAddResult[i] = aInt*b;
	constaddResult[i] = a * 24;
      }
      // some instructions to prevent fusing--just swap two elements
      float t = C[103];
      C[103] = C[202];;
      C[202] = t;

      lastAddIndex = addResult.length;
      // separate into separate array to prevent fmadd
      for(int i=0;i< lastAddIndex;++i){
	constaddResult[i] += C[i];
	subResult[i] = addResult[i] - C[i];
	inverseSResult[i] = inverseSResult[i] + C[i];
	addResult[i] += C[i];
	intSubResult[i] = intAddResult[i] - C[i];
	intAddResult[i] += C[i];
      }
      return lastAddIndex;
    }


    private static float negevalsub1a(float a, float b, float c) {
        return -(a*2.0f-c);
    }
    private static float evalsub1a(float a, float b, float c) {
        return a*2.0f-c;
    }
    private static float evalsub2a(float a, float b) {
        return a*2.0f;
    }
    private static float evalsub3a(float a, float b) {
        return a-b;
    }
    private static float negevalsub4a(float a, float b, float c) {
        return -(evalsub2a(a,2.0f)-c);
    }
    private static float evalsub4a(float a, float b, float c) {
        return evalsub2a(a,2.0f)-c;
    }
    private static float evalsub5a(float a, float b, float c) {
        return evalsub3a(a*2.0f,c);
    }
    private static float negevalsub5a(float a, float b, float c) {
        return -evalsub3a(a*2.0f,c);
    }

    //-----------------
    private static float negeval1a(float a, float b, float c) {
        return -(a*2.0f+c);
    }
    private static float eval1a(float a, float b, float c) {
        return a*2.0f+c;
    }
    private static float eval2a(float a, float b) {
        return a*2.0f;
    }
    private static float eval3a(float a, float b) {
        return a+b;
    }
    private static float negeval4a(float a, float b, float c) {
        return -(eval2a(a,2.0f)+c);
    }
    private static float eval4a(float a, float b, float c) {
        return eval2a(a,2.0f)+c;
    }
    private static float eval5a(float a, float b, float c) {
        return eval3a(a*2.0f,c);
    }
    private static float negeval5a(float a, float b, float c) {
        return -eval3a(a*2.0f,c);
    }
    static void verify(float testValue, float expectedResult){
      if(!Float.isNaN(testValue) && !Float.isNaN(expectedResult)){
	AssertJUnit.assertEquals(expectedResult,testValue,0);
	AssertJUnit.assertEquals(1/expectedResult,1/testValue,0);
      }
      else AssertJUnit.assertFalse(Float.isNaN(testValue) != Float.isNaN(expectedResult));
    }

    static void performIntSubTest(int a,float b,float c,float subResult){
      float d = (a*b)-c;
      verify(d,subResult);

    }
    static void performSubTest(float a,float b,float c,float subResult){
      float d = (a*b)-c;
      verify(d,subResult);
    }
    static void performInverseSubTest(float a,float b,float c,float subResult){
      float d = c-(a*b);
      verify(d,subResult);
    }

    static void performConstAddTest(float a,float c,float addResult){
      float d = (a*24)+c;
      verify(d,addResult);
    }
    static void performAddTest(float a,float b,float c,float addResult){
      float d = c+(a*b);
      verify(d,addResult);
    }

    static void performIntAddTest(int a,float b,float c,float addResult){
      float d = (a*b)+c;
      verify(d,addResult);
    }

 @BeforeMethod
 public void setUp(){
    final int arraySize = /*min size*/12*12*12 + 10;
    A1 = new float[arraySize]; 
    Aint = new int[arraySize]; 
    B1 = new float[arraySize]; 
    C1 = new float[arraySize]; 
    addResult = new float[arraySize];  // A*B+C
    subResult = new float[arraySize];  // A*B-C
    inverseSubResult = new float[arraySize];  // A*B-C
    addIntResult = new float[arraySize]; // A * (int)B + C
    subIntResult = new float[arraySize]; // A * (int)B - C
    constaddResult = new float[arraySize];  // A * const + C

    initializeArray(Aint,A1,B1,C1,addResult,subResult,inverseSubResult,addIntResult,subIntResult,
			       constaddResult);
    initNegFMATests();

 }

  private float[] negFMAAValues = { 
                        Float.POSITIVE_INFINITY,
                        Float.NEGATIVE_INFINITY,
                        1/Float.POSITIVE_INFINITY, // aka +0
                        1/Float.NEGATIVE_INFINITY, // aka -0

                        Float.POSITIVE_INFINITY,
                        Float.NEGATIVE_INFINITY,
                        1/Float.POSITIVE_INFINITY, // aka +0
                        1/Float.NEGATIVE_INFINITY, // aka -0

                        Float.POSITIVE_INFINITY,
                        Float.NEGATIVE_INFINITY,
                        1/Float.POSITIVE_INFINITY, // aka +0
                        1/Float.NEGATIVE_INFINITY, // aka -0

                        Float.POSITIVE_INFINITY,
                        Float.NEGATIVE_INFINITY,
                        1/Float.POSITIVE_INFINITY, // aka +0
                        1/Float.NEGATIVE_INFINITY, // aka -0

                        Float.POSITIVE_INFINITY,
                        Float.NEGATIVE_INFINITY,
                        1/Float.POSITIVE_INFINITY, // aka +0
                        1/Float.NEGATIVE_INFINITY, // aka -0

                        1,
                        100,
                        140,
                        12};
  private float[] negFMABValues = { 
                        Float.POSITIVE_INFINITY,
                        Float.POSITIVE_INFINITY,
                        Float.POSITIVE_INFINITY,
                        Float.POSITIVE_INFINITY,

                        Float.NEGATIVE_INFINITY,
                        Float.NEGATIVE_INFINITY,
                        Float.NEGATIVE_INFINITY,
                        Float.NEGATIVE_INFINITY,

                        1/Float.POSITIVE_INFINITY, // aka +0
                        1/Float.POSITIVE_INFINITY, // aka +0
                        1/Float.POSITIVE_INFINITY, // aka +0
                        1/Float.POSITIVE_INFINITY, // aka +0

                        1/Float.NEGATIVE_INFINITY, // aka -0
                        1/Float.NEGATIVE_INFINITY, // aka -0
                        1/Float.NEGATIVE_INFINITY, // aka -0
                        1/Float.NEGATIVE_INFINITY, // aka -0
                    
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


   negfmaAddResult = new float[negFMAAValues.length];
   negfmaSubResult = new float[negFMAAValues.length];
   negfmaMulResult = new float[negFMAAValues.length];
   fmaNegSubResult = new float[negFMAAValues.length];
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
    for(int i=0;i < 100;++i){ // ensure it gets compiled
      runNegFMATests();
    }
 }

 private void runNegFMATests() {
   float a,b,c;
   for(int i=0;i < negfmaAddResult.length;++i){
     a = negFMAAValues[i];
     b = negFMABValues[i];
     c = -(a+b);
     
     verify(c,negfmaAddResult[i]);
     c = -(a-b);
     verify(c,negfmaSubResult[i]);
     c = -(a*b);
     verify(c,negfmaMulResult[i]);
     c = (-a)-b;
     verify(c,fmaNegSubResult[i]);
   }
 }
 @Test
 public void runFusedTests(){
    for(int i=0;i < A1.length;++i){
       logger.debug("Testing "+i+" A:"+A1[i]+" B:"+B1[i]+" C:"+C1[i]+" expect:"+inverseSubResult[i]);
       // none of these should trigger a conversion
       performAddTest(A1[i],B1[i],C1[i],addResult[i]);
       performSubTest(A1[i],B1[i],C1[i],subResult[i]);
       performInverseSubTest(A1[i],B1[i],C1[i],inverseSubResult[i]);
       performIntAddTest(Aint[i],B1[i],C1[i],addIntResult[i]);
       performIntSubTest(Aint[i],B1[i],C1[i],subIntResult[i]);
       performConstAddTest(A1[i],C1[i],constaddResult[i]);
    }
 }
 @Test
 public void testEvals(){
      float f1 = Float.MAX_VALUE+inputString.length;
      float f2 = 2.0F+inputString.length;
      float f3 = Float.MIN_VALUE+inputString.length;
      for (int i = 0; i < 1000; ++i) {
        testEvals(f1,f2,f3);
        testNegEvals(f1,f2,f3);
      }
    }
    
    private void testNegEvals(float f1,float f2,float f3){
      float f;
      f = negeval1a(f1, f2, f3);
      AssertJUnit.assertEquals(Float.NEGATIVE_INFINITY,f,0);
      f = negeval4a(f1, f2, f3);
      AssertJUnit.assertEquals(Float.NEGATIVE_INFINITY,f,0);
      f = negeval5a(f1, f2, f3);
      AssertJUnit.assertEquals(Float.NEGATIVE_INFINITY,f,0);

      f = negevalsub1a(f1, f2, f3);
      AssertJUnit.assertEquals(Float.NEGATIVE_INFINITY,f,0);
      f = negevalsub4a(f1, f2, f3);
      AssertJUnit.assertEquals(Float.NEGATIVE_INFINITY,f,0);
      f = negevalsub5a(f1, f2, f3);
      AssertJUnit.assertEquals(Float.NEGATIVE_INFINITY,f,0);
    }

    private void testEvals(float f1,float f2,float f3){
      float f;
      f = eval1a(f1, f2, f3);
      AssertJUnit.assertEquals(Float.POSITIVE_INFINITY,f,0);
      f = eval4a(f1, f2, f3);
      AssertJUnit.assertEquals(Float.POSITIVE_INFINITY,f,0);
      f = eval5a(f1, f2, f3);
      AssertJUnit.assertEquals(Float.POSITIVE_INFINITY,f,0);

      f = evalsub1a(f1, f2, f3);
      AssertJUnit.assertEquals(Float.POSITIVE_INFINITY,f,0);
      f = evalsub4a(f1, f2, f3);
      AssertJUnit.assertEquals(Float.POSITIVE_INFINITY,f,0);
      f = evalsub5a(f1, f2, f3);
      AssertJUnit.assertEquals(Float.POSITIVE_INFINITY,f,0);
    }

  @Test
  public void testDotProd(){

    float[] E = new float[1000];
    float[] F = new float[1000];
    java.util.Random randGen = new java.util.Random(1313);
    for(int i= 0; i < E.length;++i){
      E[i] = randGen.nextFloat();
      F[i] = randGen.nextFloat();
    }
    double sum = 0;
    double negsum = 0;
    double origsum = dotprod(E,F);// should have interpreted result
    double negorigsum = negdotprod(E,F);// should have interpreted result
    for (int i = 0; i < E.length; ++i) {
       sum = dotprod(E,F);
       AssertJUnit.assertEquals(origsum,sum,0);
       AssertJUnit.assertEquals(1/origsum,1/sum,0);
       // should not happen unless negate fma is available
       negsum = negdotprod(E,F);
       AssertJUnit.assertEquals(negorigsum,negsum,0);
       AssertJUnit.assertEquals(1/negorigsum,1/negsum,0);
    }
    logger.debug("dotprod is "+ dotprod(E,F)+" ="+sum);

    // by this point dotprod should be compiled.  Now we must compare against expected results
    float[] AValues = { 0,  Float.MAX_VALUE,
                       Float.MIN_VALUE, Float.NEGATIVE_INFINITY, Float.POSITIVE_INFINITY,
                       1/Float.NEGATIVE_INFINITY, 1/Float.POSITIVE_INFINITY};


    float[] BValues = { 1,2,3,4,5,6,7};

    double[] CValues = { 528.0, Float.POSITIVE_INFINITY, 528.0, Float.NEGATIVE_INFINITY, Float.POSITIVE_INFINITY, 528.0, 528.0};
   for(int i=0;i < AValues.length;++i) {
     float [] H = new float[2];
     float [] I = new float[2];
     float [] J = new float[2];
     H[0] = 22;
     I[0] = 24;
     H[1] = AValues[i];
     I[1] = BValues[i];
     float result= dotprod(H,I);
     AssertJUnit.assertEquals(CValues[i],result,0);
   }
  }
}
