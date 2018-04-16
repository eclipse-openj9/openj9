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

@Test(groups = { "level.sanity","component.jit" })
public class TestFsqrt{
   private float  [] sqrtDataFlt;
   private float  [] sqrtExpectedResult;
   private double [] sqrtDataDbl = {
      Float.MAX_VALUE,
      Float.MIN_VALUE,
      Float.POSITIVE_INFINITY,
      Float.NEGATIVE_INFINITY,
      100,
      1201.1,
      4000,
      4,
      0.12345,
      1,
      2,
      0
   };

   @BeforeMethod
   public void setUp() {
     sqrtDataFlt = new float[sqrtDataDbl.length];
     sqrtExpectedResult = new float[sqrtDataDbl.length];
     float a;double b;
     for(int i=0;i < sqrtDataDbl.length;++i){
        sqrtDataFlt[i] = (float)sqrtDataDbl[i];
        b = (float)sqrtDataDbl[i]; //chop of excess bits
        sqrtExpectedResult[i] = (float)java.lang.Math.sqrt(b);
     }
      
   }
  static void verify(float testValue, float expectedResult){
      if(!Float.isNaN(testValue) && !Float.isNaN(expectedResult)){
	AssertJUnit.assertEquals(expectedResult,testValue,0);
	AssertJUnit.assertEquals(1/expectedResult,1/testValue,0);
      }
      else AssertJUnit.assertFalse(Float.isNaN(testValue) != Float.isNaN(expectedResult));
  }
  
  void testsqrt1(){
     for(int i=0;i < sqrtDataDbl.length;++i){
       float b;
       float a= sqrtDataFlt[i];
       b = (float)java.lang.Math.sqrt(a);
       verify(b,sqrtExpectedResult[i]);
     }
 }

  void testsqrt2(){
     for(int i=0;i < sqrtDataDbl.length;++i){
       float b,c,d,e;
       float a= sqrtDataFlt[i];
       b = (float)java.lang.Math.sqrt(a);
       c = (float)java.lang.Math.sqrt(a);
       d = (float)java.lang.Math.sqrt(a);
       verify(b,sqrtExpectedResult[i]);
       verify(c,sqrtExpectedResult[i]);
       verify(d,sqrtExpectedResult[i]);
     }

  }

  @Test
  public void runsinglesqrt(){
    for(int i=0;i < 100;++i){// ensure it gets compiled
      testsqrt1();
      testsqrt2();
    }
  }
}
