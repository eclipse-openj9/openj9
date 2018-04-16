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
/* tests/junit/tr/test/explicitNewInit/ExplicitNewInitTest.java, jit.test.trs, tr.dev */
package jit.test.tr.explicitNewInit;
import org.testng.annotations.Test;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity","component.jit" })
public class ExplicitNewInitTest 
   {
   public static final int N = eNIConsts.NIters;

   class SampleObject
      {
      public SampleObject() {}
      public SampleObject(int a, float c)
         {
         this.f1 = a;
         this.f2 = 0; // explicit zero init
         this.f3 = c; 
         this.f4 = 0;
         this.size = a;
         a1 = new double[size];
         a2 = new double[size];
         this.is = new innerSample();
         }
      
      public double addAll(int f)
         {
         f1 = f+f2;
         f2 = 5;
         f4 = f1+(double)f3;
         for (int i = 0; i < size; i++)
            {
            a1[i] = (Integer.MAX_VALUE - f1)/f4;
            a2[i] = (Integer.MIN_VALUE + f1)/f4;
            }
         return a1[(f%size)];
         }
      
      public class innerSample
         {
         public innerSample () 
            {
            a2 = new double[size];
            a3 = new double[size];
            }
         public void setElement(int index, int val)
            {
            a2[index] = (double)val;
            a3[index] = (double)val;
            }
         public static final int size = 10;
         private double [] a2;
         private double [] a3;
         }
   
      public innerSample is;
      public int f1;
      public int f2;
      private float f3;
      private double f4;
      private double [] a1;
      private double [] a2;
      private int size;
      }

   class bigSample
      {
      public bigSample() {}
      public SampleObject s;
      }
   

   
   //**************************//
   // testing begins...
   //
   // test explicit new initialization -1
   @Test
   public void test001()
      {
      bigSample bs = new bigSample();
      double val = 0;
      // get to scorching
      for (int i = 0; i < this.N; i++)
          subTest001(val, bs);
      AssertJUnit.assertTrue((bs.s.f2==eNIConsts.C1));
      AssertJUnit.assertTrue((bs.s.f1==eNIConsts.C2));
      }
   private void subTest001(double val, bigSample bs)
      {
      SampleObject s = new SampleObject(10, (float)2.5);
      val = s.addAll(eNIConsts.C2);
      // fool escape analysis
      bs.s = s;
      }

   // test explicit new initialization -2
   @Test
   public void test002()
      {
      SampleObject s1 = new SampleObject(20, (float)3);
      SampleObject s2 = new SampleObject(20, (float)3);
      for (int i = 0; i < this.N; i++)
         subTest002(s1, s2);
      }
   private void subTest002(SampleObject s1, SampleObject s2)
      {
      SampleObject.innerSample si1 = s1.new innerSample();
      si1.setElement(2, 10);
      s2.is = si1; 
      }
   }

interface eNIConsts
   {
   final int C1 = 5;
   final int C2 = 22;
   final int NIters = 655360;
   }
