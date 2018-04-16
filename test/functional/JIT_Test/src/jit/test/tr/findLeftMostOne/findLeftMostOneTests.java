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
package jit.test.tr.findLeftMostOne;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.util.*;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity","component.jit" })
public class findLeftMostOneTests 
   {
   private static Logger logger = Logger.getLogger(findLeftMostOneTests.class);
   
   public static final int numIters=1000;
   public static final int numTests=100;
	
   public static long[] randLHOB = new long[numTests];
   public static long[] randLNOLZ = new long[numTests];
   public static long[] randLNOTZ = new long[numTests];
   public static int[] randIHOB = new int[numTests];
   public static int[] randINOLZ = new int[numTests];
   public static int[] randINOTZ = new int[numTests];
	
   public static long[] randLHOBRes = new long[numTests];
   public static int[] randLNOLZRes = new int[numTests];
   public static int[] randLNOTZRes = new int[numTests];
   public static int[] randIHOBRes = new int[numTests];
   public static int[] randINOLZRes = new int[numTests];
   public static int[] randINOTZRes = new int[numTests];

   @Test
   public void testFunctionality ()
      {
	
      // Generate random longs and ints...
      generateRandomNumbers();
      generateRandomResults();

      // Run tests enough time to force compilation
      // of each testBit/Rand* such that we inline
      // the zSeries FLOGR instruction (and not the API call)	
	
      for (int i=0; i < numIters; i++)
         testBitLHOB();
	
      for (int i=0; i < numIters; i++)
         testRandLHOB();
		
      for (int i=0; i < numIters; i++)
         testBitLNOLZ();
		
      for (int i=0; i < numIters; i++)
         testRandLNOLZ();
		
      for (int i=0; i < numIters; i++)
         testBitLNOTZ();
		
      for (int i=0; i < numIters; i++)
         testRandLNOTZ();
		
      // Integer tests...
      for (int i=0; i < numIters; i++)
         testBitIHOB();
		
      for (int i=0; i < numIters; i++)
         testRandIHOB();

      for (int i=0; i < numIters; i++)
         testBitINOLZ();

      for (int i=0; i < numIters; i++)
         testRandINOLZ();

      for (int i=0; i < numIters; i++)
         testBitINOTZ();

      for (int i=0; i < numIters; i++)
         testRandINOTZ();
      }

   /*
   * 
   * LONG API TESTS
   * 
   * 
   */

   private void generateRandomNumbers()
      {
      Random rand = new Random(System.currentTimeMillis());
      for (int i=0; i < numTests; i++){
         randLHOB[i] = rand.nextLong();
         randLNOLZ[i] = rand.nextLong();
         randLNOTZ[i] = rand.nextLong();
         randIHOB[i] = rand.nextInt();
         randINOLZ[i] = rand.nextInt();
         randINOTZ[i] = rand.nextInt();
         }
      }

   private void generateRandomResults(){
      for (int i=0; i < numTests; i++){
         randLHOBRes[i] = Long.highestOneBit(randLHOB[i]);
         randLNOLZRes[i] = Long.numberOfLeadingZeros(randLNOLZ[i]);
         randLNOTZRes[i] = Long.numberOfTrailingZeros(randLNOTZ[i]);
         randIHOBRes[i] = Integer.highestOneBit(randIHOB[i]);
         randINOLZRes[i] = Integer.numberOfLeadingZeros(randINOLZ[i]);
         randINOTZRes[i] = Integer.numberOfTrailingZeros(randINOTZ[i]);
         }
      }

   private void testBitLHOB()
      {

      // test highestOneBit by right shifting
      // Long.MIN_VALUE to the right 63 times
      long testVal = Long.MIN_VALUE;
      long testBit = 0x8000000000000000L;
      long lRes = 0;
      long lNum2 = 0;

      for (int i=0; i < 63; i++){
         lRes = Long.highestOneBit(testVal);
         lNum2 = (testVal & testBit);
         if (lRes != lNum2){
            logger.error("Failed Long.highestOneBit: "+testVal);
            logger.error("Exp: "+lNum2 + " Rec: "+lRes);
            AssertJUnit.assertTrue(false);
            }
         testVal >>>=1;
         testBit >>>=1;
         }

      // test highestOneBit on 0
      AssertJUnit.assertTrue(Long.highestOneBit(0) == 0);
      }

   private void testRandLHOB()
      {

      //random tests
      long testVal =0;
      long lRes=0;
      for (int i=0; i < numTests; i++){
         testVal = randLHOB[i];
         lRes = Long.highestOneBit(testVal);
         if (lRes != randLHOBRes[i]){
            logger.error("Failed random Long.highestOneBit: "+testVal);
            logger.error("Exp: "+randLHOBRes[i]+ " Rec: "+lRes);
            AssertJUnit.assertTrue(false);
            }
         }  
      }

   private void testBitLNOLZ()
      {

      // test numberOfLeadingZeros by left shifting
      // 0x0000000000000001 to the right 63 times
      long testVal = 0x0000000000000001L;
      long lRes=0;
      int numZeros = 63;
      for (int i=0; i < 63; i++){
         lRes = Long.numberOfLeadingZeros(testVal);
         if (lRes != numZeros){
            logger.error("Failed Long.numberOfLeadingZeros: "+testVal);
            logger.error("Exp: "+ numZeros + " Rec: "+lRes);
            AssertJUnit.assertTrue(false);
            }
         testVal <<=1;
         numZeros--;
         }

      //test numberOfLeadingZeros on 0
      AssertJUnit.assertTrue(Long.numberOfLeadingZeros(0) == 64);
      }

   private void testRandLNOLZ()
      {

      // random tests
      long testVal=0;
      long lRes=0;
      for (int i=0; i < numTests; i++){
         testVal = randLNOLZ[i];
         lRes = Long.numberOfLeadingZeros(testVal);
         if ( lRes != randLNOLZRes[i]){
            logger.error("Failed random Long.numberOfLeadingZeros: "+testVal);
            logger.error("Exp: "+ randLNOLZRes[i] + " Rec: "+lRes);
            AssertJUnit.assertTrue(false);
            }
         }
      }

   private void testBitLNOTZ()
      {

      // test numberOfTrailingZeros by left shifting
      // 0x0000000000000001 to the right 63 times
      long testVal = 0x0000000000000001L;
      int numZeros = 0;
      int iRes=0;

      for (int i=0; i < 63; i++){
         iRes =Long.numberOfTrailingZeros(testVal);
         if ( iRes != numZeros){
            logger.error("Failed Long.numberOfTrailingZeros: "+testVal);
            logger.error("Exp: "+ numZeros + " Rec: "+iRes);
            AssertJUnit.assertTrue(false);
            }
         testVal <<=1;
         numZeros++;
         }

      // test numberOfTrailingZeros on 0
      AssertJUnit.assertTrue(Long.numberOfTrailingZeros(0) == 64);
      }

   private void testRandLNOTZ()
      {

      long testVal = 0;
      int iRes=0;

      //random tests
      for (int i=0; i < numTests; i++){
         testVal = randLNOTZ[i];
         iRes = Long.numberOfTrailingZeros(testVal);
         if ( iRes != randLNOTZRes[i]){
            logger.error("Failed random Long.numberOfTrailingZeros: "+testVal);
            logger.error("Exp: "+ randLNOTZRes[i] + " Rec: "+iRes);
            AssertJUnit.assertTrue(false);
            }
         }
      }

   /*
   * 
   * INTEGER API TESTS
   * 
   * 
   */

   private void testBitIHOB()
      {

      // test highestOneBit by right shifting
      // Integer.MIN_VALUE to the right 31 times
      int testVal = Integer.MIN_VALUE;
      int testBit = 0x80000000;
      int iRes=0;

      for (int i=0; i < 31; i++){
         iRes = Integer.highestOneBit(testVal);
         if ( iRes != (testVal & testBit)){
            logger.error("Failed Integer.highestOneBit: "+testVal);
            logger.error("Exp: "+(testVal & testBit) + " Rec: "+iRes);
            AssertJUnit.assertTrue(false);
            }
         testVal >>>=1;
         testBit >>>=1;
         }

      // test highestOneBit on 0
      AssertJUnit.assertTrue(Integer.highestOneBit(0) == 0);
      }

   private void testRandIHOB()
      {

      int testVal=0;
      int iRes=0;

      // random tests
      for (int i=0; i < numTests; i++){
         testVal = randIHOB[i];
         iRes=Integer.highestOneBit(testVal);
         if (iRes != randIHOBRes[i]){
            logger.error("Failed random Integer.highestOneBit: "+testVal);
            logger.error("Exp: "+randIHOBRes[i] + " Rec: "+iRes);
            AssertJUnit.assertTrue(false);
            }
         }
      }

   private  void testBitINOLZ()
      {

      // test numberOfLeadingZeros by left shifting
      // 0x0000000000000001 to the right 31 times
      int testVal = 0x00000001;
      int numZeros = 31;
      int iRes=0;

      for (int i=0; i < 31; i++){
         iRes = Integer.numberOfLeadingZeros(testVal);
         if ( iRes != numZeros){
            logger.error("Failed Integer.numberOfLeadingZeros: "+testVal);
            logger.error("Exp: "+ numZeros + " Rec: "+iRes);
            AssertJUnit.assertTrue(false);
            }
         testVal <<=1;
         numZeros--;
      }

      // test numberOfLeadingZeros on 0
      AssertJUnit.assertTrue(Integer.numberOfLeadingZeros(0) == 32);
      }

   private void testRandINOLZ()
      {

      int testVal =0;
      int iRes=0;

      // random tests
      for (int i=0; i < numTests; i++){
         testVal = randINOLZ[i];
         iRes = Integer.numberOfLeadingZeros(testVal);
         if ( iRes !=randINOLZRes[i]){
            logger.error("Failed random Integer.numberOfLeadingZeros: "+testVal);
            logger.error("Exp: "+ randINOLZRes[i]+ " Rec: "+iRes);
            AssertJUnit.assertTrue(false);
            }
         }
      }

   private void testBitINOTZ()
      {

      // test numberOfTrailingZeros by left shifting
      // 0x00000001 to the right 31 times
      int testVal = 0x00000001;
      int numZeros = 0;
      int iRes =0;

      for (int i=0; i < 31; i++){
         iRes = Integer.numberOfTrailingZeros(testVal);
         if (iRes != numZeros){
            logger.error("Failed Integer.numberOfTrailingZeros: "+testVal);
            logger.error("Exp: "+ numZeros + " Rec: "+iRes);
            AssertJUnit.assertTrue(false);
            }
         testVal <<=1;
         numZeros++;
      }

      // test numberOfTrailingZeros on 0
      AssertJUnit.assertTrue(Integer.numberOfTrailingZeros(0) == 32);
      }

   private void testRandINOTZ()
      {

      int testVal =0;
      int iRes =0;

      // random tests
      for (int i=0; i < numTests; i++){
         testVal = randINOTZ[i];
         iRes = Integer.numberOfTrailingZeros(testVal);
         if (iRes!= randINOTZRes[i]){
            logger.error("Failed random Integer.numberOfTrailingZeros: "+testVal);
            logger.error("Exp: "+randINOTZRes[i] + " Rec: "+iRes);
            AssertJUnit.assertTrue(false);
            }
         }
      }
   }
