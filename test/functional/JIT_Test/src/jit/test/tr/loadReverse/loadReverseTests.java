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
package jit.test.tr.loadReverse;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.util.*;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity","component.jit" })
public class loadReverseTests 
   {
   private static Logger logger = Logger.getLogger(loadReverseTests.class);
   
   public static long longReverse;
   public static int intReverse;
   public static short shortReverse;

   @Test
   public void testFunctionality ()
      {
      int count;
      for(count=0;count<10000;count++)
          {
          funcLongTests();
          funcIntegerTests();
          funcShortTests();
          }
      funcLongTests();
      funcIntegerTests();
      funcShortTests();
      }

   /*
   * 
   * LONG API TESTS
   * 
   * 
   */
    
   private void funcLongTests()
      {
      long orig;
      //Tests Long Function For Correctness
      //By Reversing Bytes Twice And Comparing With The Original
      long testVal = 2020630592L;
      orig = testVal;
      testVal = Long.reverseBytes(testVal);
      testVal = Long.reverseBytes(testVal);
      if (testVal !=orig)
         {
         logger.error("Long.reverseBytes fails");
         logger.error("Expected value: " + orig);
         logger.error("Actual value: " + testVal );
         AssertJUnit.assertTrue(false);
         }
      testVal = 9186917146605666304L;
      orig = testVal;
      testVal = Long.reverseBytes(testVal);
      testVal = Long.reverseBytes(testVal);
      if (testVal !=orig)
         {
         logger.error("Long.reverseBytes fails");
         logger.error("Expected value: " + orig);
         logger.error("Actual value: " +testVal );
         AssertJUnit.assertTrue(false);
         }
      testVal = -9186917146605666304L;
      orig = testVal;
      testVal = Long.reverseBytes(testVal);
      testVal = Long.reverseBytes(testVal);
      if (testVal !=orig)
         {
         logger.error("Long.reverseBytes fails");
         logger.error("Expected value: " + orig);
         logger.error("Actual value: " +testVal );
         AssertJUnit.assertTrue(false);
         }
      Random gen = new Random();
      int count;
      for(count=0;count<100;count++)
         {
         testVal = gen.nextLong();
         orig = testVal;
         testVal = Long.reverseBytes(testVal);
         testVal = Long.reverseBytes(testVal);
         if (testVal !=orig)
            {
            logger.error("Long.reverseBytes fails");
            logger.error("Expected value: " + orig);
            logger.error("Actual value: " +testVal );
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

   private void funcIntegerTests()
      {
      int orig;
      int testVal = 20063059;
      orig = testVal;
      testVal = Integer.reverseBytes(testVal);
      testVal = Integer.reverseBytes(testVal);
      if (testVal != orig)
         {
         logger.error("Integer.reverseBytes fails");
         logger.error("Expected value: " + orig);
         logger.error("Actual value: " +testVal );
         AssertJUnit.assertTrue(false);
         }
      testVal =216012414;
      orig = testVal;
      testVal = Integer.reverseBytes(testVal);
      testVal = Integer.reverseBytes(testVal);
      if (testVal !=orig)
         {
         logger.error("Integer.reverseBytes fails");
         logger.error("Expected value: " + orig);
         logger.error("Actual value: " +testVal );
         AssertJUnit.assertTrue(false);
         }
      testVal = -377284018;
      orig = testVal;
      testVal = Integer.reverseBytes(testVal);
      testVal = Integer.reverseBytes(testVal);
      if (testVal != orig)
         {
         logger.error("Integer.reverseBytes fails");
         logger.error("Expected value: " + orig);
         logger.error("Actual value: " +testVal );
         AssertJUnit.assertTrue(false);
         }
      Random gen = new Random();
      int count;
      for(count=0;count < 100;count++)
         {
         testVal = gen.nextInt();
         orig = testVal;
         testVal = Integer.reverseBytes(testVal);
         testVal = Integer.reverseBytes(testVal);
         if (testVal != orig)
            {
            logger.error("Integer.reverseBytes fails");
            logger.error("Expected value: " + orig);
            logger.error("Actual value: " +testVal );
            AssertJUnit.assertTrue(false);
            }
         }
      }

   /*
   *
   * SHORT API TESTS
   *
   *
   */

    private void funcShortTests()
       {
       short orig;
       short testVal = 28091;
       orig = testVal;
       testVal = Short.reverseBytes(testVal);
       testVal = Short.reverseBytes(testVal);
       if (testVal != orig)
          {
          logger.error("Short.reverseBytes fails");
          logger.error("Expected value: " + orig);
          logger.error("Actual value: " +testVal );
          AssertJUnit.assertTrue(false);
          }
       testVal = -4696;
       orig = testVal;
       testVal = Short.reverseBytes(testVal);
       testVal = Short.reverseBytes(testVal);
       if (testVal !=orig)
          {
          logger.error("Short.reverseBytes fails");
          logger.error("Expected value: " + orig);
          logger.error("Actual value: " +testVal );
          AssertJUnit.assertTrue(false);
          }
       testVal = 32760;
       orig = testVal;
       testVal = Short.reverseBytes(testVal);
       testVal = Short.reverseBytes(testVal);
       if (testVal != orig)
          {
          logger.error("Short.reverseBytes fails");
          logger.error("Expected value: " + orig);
          logger.error("Actual value: " +testVal );
          AssertJUnit.assertTrue(false);
          }
       Random gen = new Random();
       int count;
       for(count=0;count < 100;count++)
          {
          testVal = (short)gen.nextInt(32766);
          orig = testVal;
          testVal = Short.reverseBytes(testVal);
          testVal = Short.reverseBytes(testVal);
          if (testVal != orig)
             {
             logger.error("Short.reverseBytes fails");
             logger.error("Expected value: " + orig);
             logger.error("Actual value: " +testVal );
             AssertJUnit.assertTrue(false);
             }
          }
       }
   }

