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
import org.testng.AssertJUnit;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity","component.jit" })
public class FloatSimplifySub {
	private static Logger logger = Logger.getLogger(FloatSimplifySub.class);
	
 @Test
 public void testFloatSimplifySub(){

    int size = 125;
    logger.debug("Running with arrays of size "+ size + " iterations....");

    long delta,start, stop;
    delta = java.lang.System.currentTimeMillis();
    start = java.lang.System.currentTimeMillis();
    delta = start-delta;

    float[] arrayA = new float[size];
    float[] arrayB = new float[size];
    float[] results = new float[size];
    float[] correctAnswer = new float[size];

    // test A - -B -> A +B
    initAsubsubB(arrayA,arrayB,correctAnswer,size);
   
    logger.debug("Running A-(-B)->A+B tests...");
    start = java.lang.System.currentTimeMillis();

    for(int j = 0; j < 1000;++j)
      testAplusB(arrayA,arrayB,results,size);
    stop = java.lang.System.currentTimeMillis();
    logger.debug("Finished.");

    logger.debug("Total time = " + ((float)((stop-start)/1000)) + " and " + (stop-start)+" milliseconds");

    verify(correctAnswer,results,size);

    // test -A - -B -> B-A
    initNegAsubNegB(arrayA,arrayB,correctAnswer,size);
   
    logger.debug("Running -A-(-B)->B-A tests...");
    start = java.lang.System.currentTimeMillis();

    for(int j = 0; j < 1000;++j)
      testBsubA(arrayA,arrayB,results,size);
    stop = java.lang.System.currentTimeMillis();
    logger.debug("Finished.");

    logger.debug("Total time = " + ((float)((stop-start)/1000)) + " and " + (stop-start)+" milliseconds");

    verify(correctAnswer,results,size);
    
  }

//------------------------------------------------------------------
//------------------------------------------------------------------
  private void verify(float[] a1,float[] a2 ,int size){
    for(int i=0;i < size;++i){
      if(Float.isNaN(a1[i])){
        AssertJUnit.assertEquals(Float.isNaN(a1[i]),Float.isNaN(a2[i]));
        continue;
      }
      if(a1[i] != a2[i]) logger.error("Error: Element "+i+":"+a1[i]+" != "+a2[i]);
      AssertJUnit.assertEquals("Values not equal",a1[i],a2[i],0);

      if(1/a1[i] != 1/a2[i]) logger.error("Error: Element "+i+": 1/"+a1[i]+" != 1/"+a2[i]);
      AssertJUnit.assertEquals("Reciprocals not equal",1/a1[i],1/a2[i],0); // will catch 0 != -0
    }
  }
//------------------------------------------------------------------
  void testBsubA(float[] A,float[] B,float[] results,int size){
    float a,b;
    for(int i=0;i < size;++i){
      a = A[i];
      b = B[i];
      b = -b;
      a = -a;
      results[i] =  a-b;
    }
     
  }

//------------------------------------------------------------------
  void initNegAsubNegB(float[] A,float[] B,float[] results,int size){
    java.util.Random randGen = new java.util.Random(size);
    int i=0;
    float a,b;
//--
    a = (1+i)*(1/Float.POSITIVE_INFINITY);
    b = (1+i)*(1/Float.POSITIVE_INFINITY);
    A[i] =a; B[i] = b;
    results[i] = b-a;
    ++i;
//--
    a = (1+i)* (1/Float.POSITIVE_INFINITY);
    b = (1+i)* (1/Float.NEGATIVE_INFINITY);
    A[i] =a; B[i] = b;
    results[i] = b-a;
    ++i;
//--
    a = (1+i)* (1/Float.NEGATIVE_INFINITY);
    b = (1+i)* (1/Float.POSITIVE_INFINITY);
    A[i] =a; B[i] = b;
    results[i] = b-a;
    ++i;
//--
    a = (1+i)* (1/Float.NEGATIVE_INFINITY);
    b = (1+i)* (1/Float.NEGATIVE_INFINITY);
    A[i] =a; B[i] = b;
    results[i] = b-a;
    ++i;
//--
    a = (1+i)* Float.NEGATIVE_INFINITY;
    b = (1+i)* Float.NEGATIVE_INFINITY;
    A[i] =a; B[i] = b;
    results[i] = b-a;
    ++i;
//--
    a = (1+i)* Float.POSITIVE_INFINITY;
    b = (1+i)* Float.NEGATIVE_INFINITY;
    A[i] =a; B[i] = b;
    results[i] = b-a; 
    ++i;
//--
    a = (1+i)* Float.POSITIVE_INFINITY;
    b = (1+i)* Float.POSITIVE_INFINITY;
    A[i] =a; B[i] = b;
    results[i] = b-a;
    ++i;
//--
    a = (1+i)*Float.NEGATIVE_INFINITY;
    b = (1+i)*Float.POSITIVE_INFINITY;
    A[i] =a; B[i] = b;
    results[i] = b-a;
    ++i;

    for(;i < size;++i){
      a = randGen.nextFloat();
      b = randGen.nextFloat();
      a = (1+i)* a;
      b = (1+i)* b;
      A[i] = a;
      B[i] = b;
      results[i] =  b-a;
    }
  }
//------------------------------------------------------------------
//------------------------------------------------------------------
//------------------------------------------------------------------
  void initAsubsubB(float[] A,float[] B,float[] results,int size){
    java.util.Random randGen = new java.util.Random(size);
    for(int i=0;i < size;++i){
      float a = randGen.nextFloat();
      float b = randGen.nextFloat();
      a = (1+i)* a;
      b = (1+i)* b;
      A[i] = a;
      B[i] = b;
      results[i] = a + b;
    }
  }

  void testAplusB(float[] A,float[] B,float[] results,int size){
    float a,b;
    for(int i=0;i < size;++i){
      a = A[i];
      b = B[i];
      b = -b;
      results[i] = a - b;
      logger.debug("results["+i+"] = "+(a+b)+" = "+results[i]);
    }
     
  }
};
