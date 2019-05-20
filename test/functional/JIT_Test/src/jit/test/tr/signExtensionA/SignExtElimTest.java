/*******************************************************************************
 * Copyright (c) 2017, 2019 IBM Corp. and others
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
package jit.test.tr.signExtensionA;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeMethod;
import org.testng.AssertJUnit;
/**
 * tests propagation of i2l up expression trees
 * meant to run on 64 bit platforms
 * but will also test long vs int arrays & objects
 */
@Test(groups = { "level.sanity","component.jit" })
public class SignExtElimTest {
  private long[] resultsLongA;
  private long[] dummy;
  private long[] expectedResultsLongA;
  private int[] subscript;
  private int[] subscriptSub;
  private java.util.Random randomGen;
  private int[] overFlowResults;
  private int[] dummyOverflow;
  private int[] overFlowExpectedResults;
  private int overFlowOffset;
  private int underFlowOffset;
  private long[] overFlowResultsLong;

  @BeforeMethod
  protected void setUp(){
    final int size=100;
    resultsLongA = new long[size];
    dummy = new long[size];
    subscript = new int[size];
    subscriptSub = new int[size];
    expectedResultsLongA = new long[size];

    final int overflowSize=21;

    overFlowOffset = Integer.MAX_VALUE-1;
    underFlowOffset = -(Integer.MAX_VALUE-1);
    overFlowResults = new int[overflowSize];
    overFlowResultsLong = new long[overflowSize];
    overFlowExpectedResults = new int[overflowSize];
    dummyOverflow = new int[overflowSize];
    for(int i=0; i < overflowSize;++i){
      overFlowExpectedResults[i] = i+10;
    }

    randomGen = new java.util.Random();

    // long = sizeof ptr on 64 bit platform
    // ensure no need for sign extension elimination
    // in this loop
    for(long i=0;i < size;++i){
      expectedResultsLongA[(int)i] = i; // must be +ve
      subscript[(int)i] = 0;
      subscriptSub[(int)i] = (int)i*2;
    }
  }
  /**
   * test address calculations
   */
  @Test
  public void testAddressElim(){
     // make sure JIT gets to see the code
     for(int i=0; i < 1000;++i){ 
       testAddressElim1();
       testAddressElim2();
     }
  }

  /**
   * elim should not happen as indices designed
   * to overflow
   */
  @Test
  public void testOverflow(){
     // make sure JIT gets to see the code
     for(int i=0; i < 3000;++i){ 
       testOverflow1();
       testOverflow2();
     }
  }

  /**
   * test user i2ls
   */
  @Test
  public void testUseri2l(){
     // make sure JIT gets to see the code
     for(int i=0; i < 1000;++i){ 
       testUseri2lA();
     }
  }
  void testUseri2lA(){
    // assume we have no more than 20 gprs
    long a1 = negativeLong();
    long a2 = negativeLong();
    long a3 = negativeLong();
    long a4 = negativeLong();
    long a5 = negativeLong();
    long a6 = negativeLong();
    long a7 = negativeLong();
    long a8 = negativeLong();
    long a9 = negativeLong();
    long a0 = negativeLong();
    long aA = negativeLong();
    long aB = negativeLong();
    long aC = negativeLong();
    long aD = negativeLong();
    long aE = negativeLong();
    long aF = negativeLong();
    long aG = negativeLong();
    long aH = negativeLong();
    long aI = negativeLong();
    long aJ = negativeLong();

    // encourage GRA to stick 'em in regs
    for(int j=0;j< 30;++j){
      a1 += a1+a2+a3+a4+a5+a6+a7+a8+a9+aA+aB+aC+aD+aE+aF+aG+aH+aI+aJ;
      a2 += a2+a2+a3+a4+a5+a6+a7+a8+a9+aA+aB+aC+aD+aE+aF+aG+aH+aI+aJ;
      a3 += a3+a2+a3+a4+a5+a6+a7+a8+a9+aA+aB+aC+aD+aE+aF+aG+aH+aI+aJ;
      if(a1 > -1) a1=-a1;
      if(a2 > -1) a2=-a2;
      if(a3 > -1) a3=-a3;
    }

    long sum=0;
    for(int i=0;i < 21;++i){
      sum += i;
      sum += i;
      sum += i;
    }
    AssertJUnit.assertEquals(630L,sum);
    sum=9;
    int count=20;
    for(int i=Integer.MAX_VALUE;count >=0;++i){
      --count;
      sum += i;
      sum += i;
    }
    AssertJUnit.assertEquals( -81604378237L,sum);
    AssertJUnit.assertFalse(a1 > 0);
    AssertJUnit.assertFalse(a2 > 0);
    AssertJUnit.assertFalse(a3 > 0);
}

  void testOverflow2(){
   int count=0;
   long longIndex=0;
   final int halfway = overFlowResults.length/2;
   for(int i= java.lang.Integer.MAX_VALUE - halfway; longIndex != java.lang.Integer.MIN_VALUE + halfway;){
     longIndex=20 *(long)i ; 
     overFlowResultsLong[count] = longIndex;
     ++count;
     longIndex = ++i; // share i2l node, ensure bump is done as int and not long

     AssertJUnit.assertTrue(count<22);
   }
  }
  void testOverflow1(){
    final int halfway = overFlowResults.length/2;
    int index= Integer.MAX_VALUE  - halfway-1;
    for(int i=0;i < overFlowResults.length;++i){
      overFlowResults[index - overFlowOffset +halfway] = i+10;
      dummyOverflow[index + underFlowOffset +halfway] = 10; //encourage i2l sharing
      ++index;// should overflow
    }
    for(int i=0;i < overFlowResults.length;++i){
      AssertJUnit.assertEquals(overFlowExpectedResults[i],overFlowResults[i]);
    }
  }

  long negativeLong(){
    // returns number 0..1999
    long val = (long)randomGen.nextInt(1999);

    return -(++val); // return some number -1..-2000
  }
  // the test tries to load up high words of registers
  // and then use them in the addressing calculation
  // do this by setting -ve values into long sized regs
  private void testAddressElim1(){
    // assume we have no more than 20 gprs
    long a1 = negativeLong();
    long a2 = negativeLong();
    long a3 = negativeLong();
    long a4 = negativeLong();
    long a5 = negativeLong();
    long a6 = negativeLong();
    long a7 = negativeLong();
    long a8 = negativeLong();
    long a9 = negativeLong();
    long a0 = negativeLong();
    long aA = negativeLong();
    long aB = negativeLong();
    long aC = negativeLong();
    long aD = negativeLong();
    long aE = negativeLong();
    long aF = negativeLong();
    long aG = negativeLong();
    long aH = negativeLong();
    long aI = negativeLong();
    long aJ = negativeLong();

    // encourage GRA to stick 'em in regs
    for(int j=0;j< 130;++j){
      a1 += a1+a2+a3+a4+a5+a6+a7+a8+a9+aA+aB+aC+aD+aE+aF+aG+aH+aI+aJ;
      a2 += a2+a2+a3+a4+a5+a6+a7+a8+a9+aA+aB+aC+aD+aE+aF+aG+aH+aI+aJ;
      a3 += a3+a2+a3+a4+a5+a6+a7+a8+a9+aA+aB+aC+aD+aE+aF+aG+aH+aI+aJ;
      if(a1 > -1) a1=-a1;
      if(a2 > -1) a2=-a2;
      if(a3 > -1) a3=-a3;
    }

    // at this point, we'll assume that any registers we use will have bits in the upper word ie -ve
    for(int i=0; i < resultsLongA.length;++i){
       resultsLongA[i] = i;
       dummy[i] = -i; // trigger sharing of index*scale
       testArgStore(i,i,i,i,i,i,i,i,i,i,9999);
    }
    for(int i= resultsLongA.length-1;i>=0;--i){
       AssertJUnit.assertEquals(resultsLongA[i],expectedResultsLongA[i]);
    }
    // give 'em a use, but after the above test
    AssertJUnit.assertFalse(a1 > 0);
    AssertJUnit.assertFalse(a2 > 0);
    AssertJUnit.assertFalse(a3 > 0);
  }

  private void testAddressElim2(){
    // assume we have no more than 20 free gprs
    long a1 = negativeLong();
    long a2 = negativeLong();
    long a3 = negativeLong();
    long a4 = negativeLong();
    long a5 = negativeLong();
    long a6 = negativeLong();
    long a7 = negativeLong();
    long a8 = negativeLong();
    long a9 = negativeLong();
    long a0 = negativeLong();
    long aA = negativeLong();
    long aB = negativeLong();
    long aC = negativeLong();
    long aD = negativeLong();
    long aE = negativeLong();
    long aF = negativeLong();
    long aG = negativeLong();
    long aH = negativeLong();
    long aI = negativeLong();
    long aJ = negativeLong();

    // encourage GRA to stick 'em in regs
    for(int j=0;j< 130;++j){
      a1 += a1+a2+a3+a4+a5+a6+a7+a8+a9+aA+aB+aC+aD+aE+aF+aG+aH+aI+aJ;
      a2 += a2+a2+a3+a4+a5+a6+a7+a8+a9+aA+aB+aC+aD+aE+aF+aG+aH+aI+aJ;
      a3 += a3+a2+a3+a4+a5+a6+a7+a8+a9+aA+aB+aC+aD+aE+aF+aG+aH+aI+aJ;
      if(a1 > -1) a1=-a1;
      if(a2 > -1) a2=-a2;
      if(a3 > -1) a3=-a3;
    }

    // at this point, we'll assume that any registers we use will have bits in the upper word
    for(int i=0; i < resultsLongA.length;++i){
       dummy[i-subscript[i]] = -i; // trigger sharing of index*scale
       resultsLongA[i-subscript[i]] = i;
       dummy[-(i-subscriptSub[i])] = -i;
       resultsLongA[-(i-subscriptSub[i])] = i;
       testArgStore(i,i,i,i,i,i,i,i,i,i,9999);
    }
    for(int i= resultsLongA.length-1;i>=0;--i){
       AssertJUnit.assertEquals(resultsLongA[i],expectedResultsLongA[i]);
    }

    // give 'em a use, but after the above test
    AssertJUnit.assertFalse(a1 > 0);
    AssertJUnit.assertFalse(a2 > 0);
    AssertJUnit.assertFalse(a3 > 0);
  }

  /**
   * assume only 1st 4 args are passed in registers, the rest are on the stack.  Verify values are as expected
   */
  static void testArgStore(long a,int b,long c,long d,long e,long f,long g,long h, long i, int j,int corruptCheck){
    AssertJUnit.assertEquals((int)a,b);
    AssertJUnit.assertEquals(a,c);
    AssertJUnit.assertEquals(a,d);
    AssertJUnit.assertEquals(a,e);
    AssertJUnit.assertEquals(a,f);
    AssertJUnit.assertEquals(a,g);
    AssertJUnit.assertEquals(a,h);
    AssertJUnit.assertEquals(a,i);
    AssertJUnit.assertEquals((int)a,j);
    AssertJUnit.assertEquals(9999,corruptCheck);
  }

}
