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
package jit.test.tr.fprToGpr;

import org.testng.annotations.Test;
import org.testng.AssertJUnit;
import java.util.*;
import org.testng.log4testng.Logger;

@Test(groups = { "level.sanity","component.jit" })
public class fprToGprTests
   {
   public static long longval;
   public static int intval;
   public static double doubleval;
   public static float floatval;
   public static long randLToD[] = new long[100];
   public static double randDToL[] = new double[100];
   public static int randIToF[] = new int[100];
   public static float randFToI[] = new float[100];
   public static double randLToDRes[] = new double[100];
   public static long randDToLRes[] = new long[100];
   public static float randIToFRes[] = new float[100];
   public static int randFToIRes[] = new int[100];
   public static long randLToDToLRes[] = new long[100];
   public static double randDToLToDRes[] = new double[100];
   public static int randIToFToIRes[] = new int[100];
   public static float randFToIToFRes[] = new float[100];

   private static Logger logger = Logger.getLogger(fprToGprTests.class);
   
   @Test
   public void testFunctionality ()
      {
      generateRandomNumbers();
      generateRandomResults();
      for(int count=0;count< 1000;count++)
          {
          funcLong();
          funcInteger();
          }
      }

   private static void generateRandomNumbers()
      {
      Random rand = new Random(System.currentTimeMillis());
      randLToD[0] = 0L;
      randIToF[0] = 0;
      randDToL[0] = 0;
      randFToI[0] = 0;
      randLToD[1] = 0xBFF0000000000000L;
      randIToF[1] = 0xBF800000;
      randDToL[1] = -1.0;
      randFToI[1] = (float)-1.0;
      randLToD[2] = 0x3FF0000000000000L;
      randIToF[2] = 0x3F800000;
      randDToL[2] = 1.0;
      randFToI[2] = (float)1.0;
      randLToD[3] = 0xC08F400000000000L;;
      randIToF[3] = 0xC47A0000;
      randDToL[3] = Double.POSITIVE_INFINITY;
      randFToI[3] = Float.POSITIVE_INFINITY;
      randLToD[4] = 0x408F480000000000L;
      randIToF[4] = 0x447A4000;
      randDToL[4] = Double.NEGATIVE_INFINITY;
      randFToI[4] = Float.NEGATIVE_INFINITY;
      randLToD[5] = Long.MAX_VALUE;
      randIToF[5] = Integer.MAX_VALUE;
      randDToL[5] = Double.NaN;
      randFToI[5] = Float.NaN;
      for(int count=6;count <100;count++)
         {
         randLToD[count] = rand.nextLong();
         randIToF[count] = rand.nextInt();
         randDToL[count] = rand.nextDouble()* Double.MAX_VALUE;
         randFToI[count] = rand.nextFloat() * Float.MAX_VALUE;
         }
      }

private static void generateRandomResults()
     {
     int count;
     randLToDRes[0] = 0;
     randIToFRes[0] = 0;
     randDToLRes[0] = 0;
     randFToIRes[0] = 0;
     randLToDToLRes[0] = 0;
     randDToLToDRes[0] = 0;
     randIToFToIRes[0] = 0;
     randFToIToFRes[0] = 0;
     randLToDRes[1] = -1.0;
     randIToFRes[1] = (float)-1.0;
     randDToLRes[1] = 0xBFF0000000000000L;
     randFToIRes[1] = 0xBF800000;
     randLToDToLRes[1] = 0xBFF0000000000000L;
     randDToLToDRes[1] = -1.0;
     randIToFToIRes[1] = 0xBF800000;
     randFToIToFRes[1] = (float)-1.0;
     randLToDRes[2] = 1.0;
     randIToFRes[2] = (float)1.0;
     randDToLRes[2] = 0x3FF0000000000000L;;
     randFToIRes[2] = 0x3F800000;
     randLToDToLRes[2] = 0x3FF0000000000000L;
     randDToLToDRes[2] = 1.0 ;
     randIToFToIRes[2] = 0x3F800000;
     randFToIToFRes[2] = (float)1.0;
     randLToDRes[3] = -1000.0;
     randIToFRes[3] = (float)-1000.0;
     randFToIRes[3] = 2139095040;
     randDToLRes[3] = 0x7ff0000000000000L;
     randLToDToLRes[3] = 0xC08F400000000000L;;
     randDToLToDRes[3] = Double.POSITIVE_INFINITY;
     randIToFToIRes[3] = 0xC47A0000;
     randFToIToFRes[3] = Float.POSITIVE_INFINITY;
     randLToDRes[4] = 1001.0;
     randIToFRes[4] = (float)1001.0;
     randDToLRes[4] = 0xfff0000000000000L;
     randFToIRes[4] = 0xff800000;
     randLToDToLRes[4] = 0x408F480000000000L;
     randDToLToDRes[4] = Double.NEGATIVE_INFINITY;
     randIToFToIRes[4] =  0x447A4000;
     randFToIToFRes[4] = Float.NEGATIVE_INFINITY;
     randLToDRes[5] = Double.NaN;
     randIToFRes[5] = Float.NaN;
     randDToLRes[5] = 0x7ff8000000000000L;
     randFToIRes[5] = 0x7fc00000;
     randLToDToLRes[5] = Long.MAX_VALUE;
     randDToLToDRes[5] = Double.NaN;
     randIToFToIRes[5] = Integer.MAX_VALUE;
     randFToIToFRes[5] = Float.NaN;
     for(count=6;count < 50;count++)
        {
        randLToDRes[count] = Double.longBitsToDouble(randLToD[count]);
        randIToFRes[count] = Float.intBitsToFloat(randIToF[count]);
        randDToLRes[count] = Double.doubleToRawLongBits(randDToL[count]);
        randFToIRes[count] = Float.floatToRawIntBits(randFToI[count]);
        randLToDToLRes[count] = Double.doubleToRawLongBits(randLToDRes[count]);
        randDToLToDRes[count] = Double.longBitsToDouble(randDToLRes[count]);
        randIToFToIRes[count] = Float.floatToRawIntBits(randIToFRes[count]);
        randFToIToFRes[count] = Float.intBitsToFloat(randFToIRes[count]);
        }
     for(count=50;count < 100;count++)
        {
        randLToDRes[count] = Double.longBitsToDouble(randLToD[count]);
        randIToFRes[count] = Float.intBitsToFloat(randIToF[count]);
        randDToLRes[count] = Double.doubleToLongBits(randDToL[count]);
        randFToIRes[count] = Float.floatToIntBits(randFToI[count]);
        randLToDToLRes[count] = Double.doubleToLongBits(randLToDRes[count]);
        randDToLToDRes[count] = Double.longBitsToDouble(randDToLRes[count]);
        randIToFToIRes[count] = Float.floatToIntBits(randIToFRes[count]);
        randFToIToFRes[count] = Float.intBitsToFloat(randFToIRes[count]);
        }
     }

   /*
   *
   * LONG API TESTS
   *
   *
   */

    private void funcLong()
       {
       long lg, lgr;
       double dl, dlr;
       float ft, ftr;
       int it, itr, i;
       for(i=0;i<50;i++)
          {
          dl = Double.longBitsToDouble(randLToD[i]);
          lg = Double.doubleToRawLongBits(randDToL[i]);
          lgr = Double.doubleToRawLongBits(dl);
          dlr = Double.longBitsToDouble(lg);
          if((lg != randDToLRes[i]) || (dl != randLToDRes[i]) || (lgr != randLToDToLRes[i]) || (dlr != randDToLToDRes[i]))
             {
             if((!Double.isNaN(dl)) && (!Double.isNaN(randLToDRes[i])))
                {
            	logger.error("Failed Long Tests On i = " + i + "!" );
            	logger.error("Using Values: " + randDToL[i] + " " + randLToD[i] + " " + dl + " " + lg);
            	logger.error("Expected Results: D2L " + randDToLRes[i] + " L2D  " + randLToDRes[i] + " L2D2L " + randLToDToLRes[i] + " D2L2D " + randDToLToDRes[i]);
            	logger.error("Actual Results  : " + lg  + " " + dl + " " + lgr + " " + dlr );
                AssertJUnit.assertTrue(false);
                }
             }
          }
       for(i=50;i<100;i++)
          {
          dl = Double.longBitsToDouble(randLToD[i]);
          lg = Double.doubleToLongBits(randDToL[i]);
          lgr = Double.doubleToLongBits(dl);
          dlr = Double.longBitsToDouble(lg);
          if((lg != randDToLRes[i]) || (dl != randLToDRes[i]) || (lgr != randLToDToLRes[i]) || (dlr != randDToLToDRes[i]))
             {
             if((!Double.isNaN(dl)) && (!Double.isNaN(randLToDRes[i])))
                {
            	logger.error("Failed Long Tests On i = " + i + "!" );
            	logger.error("Using Values: " + randDToL[i] + " " + randLToD[i] + " " + dl + " " + lg);
            	logger.error("Expected Results: " + randDToLRes[i] + " " + randLToDRes[i] + " " + randLToDToLRes[i] + " " + randDToLToDRes[i]);
            	logger.error("Actual Results  : " + lg  + " " + dl + " " + lgr + " " + dlr );
                AssertJUnit.assertTrue(false);
                }
             }
          }
       }

   /*
   *
   * INTEGER API TESTS
   *
   *
   */

   private void funcInteger()
      {
      float ft, ftr;
      int it, itr,i;
      for(i=0;i<50;i++)
         {
         ft = Float.intBitsToFloat(randIToF[i]);
         it = Float.floatToRawIntBits(randFToI[i]);
         itr = Float.floatToRawIntBits(ft);
         ftr = Float.intBitsToFloat(it);
         if((it != randFToIRes[i]) || (ft != randIToFRes[i]) || (itr != randIToFToIRes[i]) || (ftr != randFToIToFRes[i]))
            {
            if((!Float.isNaN(ft)) && (!Float.isNaN(randIToFRes[i])))
               {
               logger.error("Failed Integer Tests On i = " + i + "!");
               logger.error("Using Values: " + randFToI[i] + " " + randIToF[i] + " " + ft + " " + it);
               logger.error("Expected Results: " + randFToIRes[i] + " " + randIToFRes[i] + " " + randIToFToIRes[i] + " " + randFToIToFRes[i]);
               logger.error("Actual Results  : " + it  + " " + ft + " " + itr + " " + ftr );
               AssertJUnit.assertTrue(false);
               }
            }
         }
      for(i=50;i<100;i++)
         {
         ft = Float.intBitsToFloat(randIToF[i]);
         it = Float.floatToIntBits(randFToI[i]);
         itr = Float.floatToIntBits(ft);
         ftr = Float.intBitsToFloat(it);
         if((it != randFToIRes[i]) || (ft != randIToFRes[i]) || (itr != randIToFToIRes[i]) || (ftr != randFToIToFRes[i]))
            {
            if((!Float.isNaN(ft)) && (!Float.isNaN(randIToFRes[i])))
               {
               logger.error("Failed Integer Tests On i = " + i + "!");
               logger.error("Using Values: " + randFToI[i] + " " + randIToF[i] + " " + ft + " " + it);
               logger.error("Expected Results: " + randFToIRes[i] + " " + randIToFRes[i] + " " + randIToFToIRes[i] + " " + randFToIToFRes[i]);
               logger.error("Actual Results  : " + it  + " " + ft + " " + itr + " " + ftr );
               AssertJUnit.assertTrue(false);
               }
            }
         }
      }
   }

