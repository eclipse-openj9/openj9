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
package jit.test.loopReduction;

public class arrayset {
   public static byte[] byteArr = new byte[18097];

   public static char[] charArr = new char[18097];

   public static byte[] byteArrA = new byte[18097];

   public static byte[] byteArrB = new byte[18097];

   public static byte[] byteArrABase = new byte[18097];

   public static byte[] byteArrBBase = new byte[18097];

   public static double[] doubleArr = new double[18097];

   public static float[] floatArr = new float[18097];

   public static int[] intArr = new int[18097];

   public static long[] longArr = new long[18097];

   public static short[] shortArr = new short[18097];

   public static byte[] smallArr = new byte[10];

   public static byte[][] twoDByteArr = { new byte[400], new byte[400], new byte[400] };

   class initTest {
      public int field1;

      public int field2;

      public int field3;

      public long field4;

      public char field5;
   }

   public static initTest initTestObject = new arrayset().new initTest();

   public void testByteArraySeriesInit(Context c) {
      byte initVal = 0x61;
      for (int iters = 0; iters < c.iterations()*1000; ++iters) {
         byteArr[0] = initVal;
         byteArr[1] = initVal;
         byteArr[2] = initVal;
         byteArr[3] = initVal;
         byteArr[4] = initVal;
         byteArr[5] = initVal;
         byteArr[6] = initVal;
         byteArr[7] = initVal;
         byteArr[8] = initVal;
         byteArr[9] = initVal;
      }
      if (c.verify())
         for (int i = 9; i >= 0; --i) {
            if (byteArr[i] != initVal) {
               c.printerr("Byte array does not have the correct value");
            }
         }
   }

   public void testObjectInit(Context c) {
      byte initVal = -1;
      boolean fail = false;
      for (int iters = 0; iters < c.iterations()*1000; ++iters) {
         initTestObject.field1 = initVal;
         initTestObject.field2 = initVal;
         initTestObject.field3 = initVal;
         initTestObject.field4 = initVal;
         initTestObject.field5 = (char) initVal;
      }
      if (c.verify()) {
         if (initTestObject.field1 != initVal)
            fail = true;
         if (initTestObject.field2 != initVal)
            fail = true;
         if (initTestObject.field3 != initVal)
            fail = true;
         if (initTestObject.field4 != initVal)
            fail = true;
         if (initTestObject.field5 != (char) initVal)
            fail = true;
         if (fail) {
            c.printerr("Byte array does not have the correct value");
         }
      }
   }

   public void testByteLoopDownKnownBoundsMedium(Context c) {
      byte initVal = (byte) 0x61;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 800; i >= 27; --i) {
            byteArr[i] = 0x61;
         }
      if (c.verify())
         for (int i = 800; i >= 27; --i) {
            if (byteArr[i] != initVal) {
               c.printerr("Byte array does not have the correct value");
            }
         }
   }

   public void testLoopDownByte(Context c) {
      byte initVal = (byte) 0x1;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = byteArr.length - 1; i >= 0; --i) {
            byteArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < byteArr.length; ++i) {
            if (byteArr[i] != initVal) {
               c.printerr("Byte array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopDownChar(Context c) {
      char initVal = (char) 0x1;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = charArr.length - 1; i >= 0; --i) {
            charArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < charArr.length; ++i) {
            if (charArr[i] != initVal) {
               c.printerr("Char array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopDownDouble(Context c) {
      double initVal = 0x1;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = doubleArr.length - 1; i >= 0; --i) {
            doubleArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < doubleArr.length; ++i) {
            if (doubleArr[i] != initVal) {
               c.printerr("Double array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopDownFloat(Context c) {
      float initVal = 0x1;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = floatArr.length - 1; i >= 0; --i) {
            floatArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < floatArr.length; ++i) {
            if (floatArr[i] != initVal) {
               c.printerr("Float array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopDownInductionVariable(Context c) {
      int initVal = 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters) {
         int i;
         for (i = intArr.length - 1; i >= 0; --i) {
            intArr[i] = initVal;
         }
         if (c.verify())
            if (i != -1) {
               c.printerr("Induction variable does not have correct value after loop. It has value " + i);
            }
      }
   }

   public void testLoopDownInt(Context c) {
      int initVal = 0x1;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = intArr.length - 1; i >= 0; --i) {
            intArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < intArr.length; ++i) {
            if (intArr[i] != initVal) {
               c.printerr("Int array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopDownLong(Context c) {
      long initVal = 0x1;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = longArr.length - 1; i >= 0; --i) {
            longArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < longArr.length; ++i) {
            if (longArr[i] != initVal) {
               c.printerr("Long array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopUpByte(Context c) {
      byte initVal = (byte) 0x1;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < byteArr.length; ++i) {
            byteArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < byteArr.length; ++i) {
            if (byteArr[i] != initVal) {
               c.printerr("Byte array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopUpChar(Context c) {
      char initVal = (char) 0x1;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < charArr.length; ++i) {
            charArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < charArr.length; ++i) {
            if (charArr[i] != initVal) {
               c.printerr("Char array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopUpComplexValueChar(Context c) {
      char initVal = (char) charArr.length;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < charArr.length; ++i) {
            charArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < charArr.length; ++i) {
            if (charArr[i] != initVal) {
               c.printerr("Char array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopUpComplexValueDouble(Context c) {
      double initVal = doubleArr.length;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < doubleArr.length; ++i) {
            doubleArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < doubleArr.length; ++i) {
            if (doubleArr[i] != initVal) {
               c.printerr("Double array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopUpComplexValueFloat(Context c) {
      float initVal = floatArr.length;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < floatArr.length; ++i) {
            floatArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < floatArr.length; ++i) {
            if (floatArr[i] != initVal) {
               c.printerr("Float array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopUpComplexValueInt(Context c) {
      int initVal = intArr.length;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < intArr.length; ++i) {
            intArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < intArr.length; ++i) {
            if (intArr[i] != initVal) {
               c.printerr("Int array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopUpComplexValueLong(Context c) {
      long initVal = longArr.length;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < longArr.length; ++i) {
            longArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < longArr.length; ++i) {
            if (longArr[i] != initVal) {
               c.printerr("Long array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopUpDouble(Context c) {
      double initVal = 0x1;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < doubleArr.length; ++i) {
            doubleArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < doubleArr.length; ++i) {
            if (doubleArr[i] != initVal) {
               c.printerr("Double array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopUpFloat(Context c) {
      float initVal = 0x1;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < floatArr.length; ++i) {
            floatArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < floatArr.length; ++i) {
            if (floatArr[i] != initVal) {
               c.printerr("Float array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopUpInt(Context c) {
      int initVal = 0x1;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < intArr.length; ++i) {
            intArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < intArr.length; ++i) {
            if (intArr[i] != initVal) {
               c.printerr("Int array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopUpKnownBounds(Context c) {
      int initVal = 0x3;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 4; i < 4099; ++i) {
            intArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 4; i < 4099; ++i) {
            if (intArr[i] != initVal) {
               c.printerr("Int array does not have correct value in element " + i);
            }
         }
   }

   public void testLoopUpLong(Context c) {
      long initVal = 0x1;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < longArr.length; ++i) {
            longArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < longArr.length; ++i) {
            if (longArr[i] != initVal) {
               c.printerr("Long array does not have correct value in element " + i);
            }
         }
   }

   public void testNestedLoopUp(Context c) {
      for (int iters = 0; iters < c.iterations() * 10; ++iters)
         for (int i = 0; i < twoDByteArr.length; ++i) {
            for (int j = 0; j < twoDByteArr[i].length; ++j) {
               twoDByteArr[i][j] = (byte) 0;
            }
         }
      if (c.verify())
         for (int i = 0; i < twoDByteArr.length; ++i) {
            for (int j = 0; j < twoDByteArr[i].length; ++j) {
               if (twoDByteArr[i][j] != (byte) 0) {
                  c.printerr("Byte array does not have correct value in element " + i);
               }
            }
         }
   }

   public void testSimpleNestedLoopUp(Context c) {
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < 1000; ++i) {
            for (int j = 0; j < smallArr.length; ++j) {
               smallArr[j] = (byte) 0;
            }
         }
      if (c.verify())
         for (int j = 0; j < smallArr.length; ++j) {
            if (smallArr[j] != (byte) 0) {
               c.printerr("Byte array does not have correct value in element " + j);
            }
         }
   }

   public void testZeroByteLoopUpKnownBoundsSmall(Context c) {
      byte initVal = (byte) 0x0;
      for (int iters = 0; iters < c.iterations() * 50; ++iters)
         for (int i = 47; i < 97; ++i) {
            byteArr[i] = 0;
         }
      if (c.verify())
         for (int i = 47; i < 97; ++i) {
            if (byteArr[i] != initVal) {
               c.printerr("Byte array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopDownByte(Context c) {
      byte initVal = (byte) 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = byteArr.length - 1; i >= 0; --i) {
            byteArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < byteArr.length; ++i) {
            if (byteArr[i] != initVal) {
               c.printerr("Byte array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopDownChar(Context c) {
      char initVal = (char) 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = charArr.length - 1; i >= 0; --i) {
            charArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < charArr.length; ++i) {
            if (charArr[i] != initVal) {
               c.printerr("Char array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopDownDouble(Context c) {
      double initVal = 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = doubleArr.length - 1; i >= 0; --i) {
            doubleArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < doubleArr.length; ++i) {
            if (doubleArr[i] != initVal) {
               c.printerr("Double array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopDownFloat(Context c) {
      float initVal = 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = floatArr.length - 1; i >= 0; --i) {
            floatArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < floatArr.length; ++i) {
            if (floatArr[i] != initVal) {
               c.printerr("Float array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopDownInt(Context c) {
      int initVal = 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = intArr.length - 1; i >= 0; --i) {
            intArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < intArr.length; ++i) {
            if (intArr[i] != initVal) {
               c.printerr("Int array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopDownLong(Context c) {
      long initVal = 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = longArr.length - 1; i >= 0; --i) {
            longArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < longArr.length; ++i) {
            if (longArr[i] != initVal) {
               c.printerr("Long array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopUpByte(Context c) {
      byte initVal = (byte) 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < byteArr.length; ++i) {
            byteArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < byteArr.length; ++i) {
            if (byteArr[i] != initVal) {
               c.printerr("Byte array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopUpChar(Context c) {
      char initVal = (char) 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < charArr.length; ++i) {
            charArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < charArr.length; ++i) {
            if (charArr[i] != initVal) {
               c.printerr("Char array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopUpDouble(Context c) {
      double initVal = 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < doubleArr.length; ++i) {
            doubleArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < doubleArr.length; ++i) {
            if (doubleArr[i] != initVal) {
               c.printerr("Double array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopUpFloat(Context c) {
      float initVal = 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < floatArr.length; ++i) {
            floatArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < floatArr.length; ++i) {
            if (floatArr[i] != initVal) {
               c.printerr("Float array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopUpInductionVariable(Context c) {
      for (int iters = 0; iters < c.iterations(); ++iters) {
         int i;
         for (i = 47; i < intArr.length; ++i) {
            intArr[i] = 0;
         }
         if (c.verify())
            if (i != intArr.length) {
               c.printerr("Induction variable does not have correct value after loop. It has value " + i);
            }
      }
   }

   public void testZeroLoopUpInt(Context c) {
      int initVal = 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < intArr.length; ++i) {
            intArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < intArr.length; ++i) {
            if (intArr[i] != initVal) {
               c.printerr("Int array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopUpKnownBoundsLarge(Context c) {
      int initVal = 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 47; i < 8080; ++i) {
            intArr[i] = 1;
         }
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 47; i < 8080; ++i) {
            intArr[i] = 0;
         }
      if (c.verify())
         for (int i = 47; i < 8080; ++i) {
            if (intArr[i] != initVal) {
               c.printerr("Int array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopUpKnownBoundsMiddle(Context c) {
      int initVal = 0x0;
      for (int iters = 0; iters < c.iterations() * 20; ++iters)
         for (int i = 47; i < 786; ++i) {
            intArr[i] = 0;
         }
      if (c.verify())
         for (int i = 47; i < 786; ++i) {
            if (intArr[i] != initVal) {
               c.printerr("Int array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopUpKnownBoundsSmall(Context c) {
      int initVal = 0x0;
      for (int iters = 0; iters < c.iterations() * 50; ++iters)
         for (int i = 47; i < 97; ++i) {
            intArr[i] = 0;
         }
      if (c.verify())
         for (int i = 47; i < 97; ++i) {
            if (intArr[i] != initVal) {
               c.printerr("Int array does not have correct value in element " + i);
            }
         }
   }

   public void testZeroLoopUpLong(Context c) {
      long initVal = 0x0;
      for (int iters = 0; iters < c.iterations(); ++iters)
         for (int i = 0; i < longArr.length; ++i) {
            longArr[i] = initVal;
         }
      if (c.verify())
         for (int i = 0; i < longArr.length; ++i) {
            if (longArr[i] != initVal) {
               c.printerr("Long array does not have correct value in element " + i);
            }
         }
   }
   
   /**
    * testCountDownLoopBeforeGT tests a count down arrayset loop with 
    * a decrement induction variable update before the array store,
    * and the loop exit test is >.
    * @param c The test harness context
    */
   public void testCountDownLoopBeforeGT(Context c) {
      for (int iters = 0; iters < c.iterations(); ++iters) {
         arraysetBeforeGT(c,8,5);
         arraysetBeforeGT(c,1,0);
      }
   }

   private void arraysetBeforeGT(Context c, int i, int j) {
      int[] array = new int[i];
      int i_orig = i;
      do {
         i--;
         array[i] = 1;
      } while (i > j);

      validateArraysetBeforeGT(c, array, i_orig, i, j);
      return;
   }

   private void validateArraysetBeforeGT(Context c, int[] array, int i, int final_i, int j) {
      for (int k = i - 1; k >= j; k--) {
         if (array[k] != 1)
            c.printerr("Error:arraysetBeforeGT @ array[" + k + "] expected: 1, got:" + array[k]);
         }
      for (int k = j - 1; k >= 0; k--) {
         if (array[k] != 0)
            c.printerr("Error:arraysetBeforeGT @ array[" + k + "] expected: 0, got:" + array[k]);
      }

      if (final_i != j)
         c.printerr("Error:arraysetBeforeGT @ Final induction var value.  expected: " + j + " got:" + final_i);
      return;
   }
   
   /**
    * testCountDownLoopsBeforeGTPlus1 tests a count down arrayset loop with 
    * a decrement induction variable update before the array store,
    * the loop exit test is >, and the index used to store the array
    * is the original value of i (before the update).  This will test
    * how idiom recognition handles commoning.
    * @param c The test harness context
    */
   public void testCountDownLoopsBeforeGTPlus1(Context c) {
      for (int iters = 0; iters < c.iterations(); ++iters) {
         arraysetBeforeGTPlus1(c,8,5);
         arraysetBeforeGTPlus1(c,1,0);
      }
   }
   
   private void arraysetBeforeGTPlus1(Context c, int i, int j) {
      int[] array = new int[i+1];
      int i_orig = i;
      do {
         i--;
         array[i+1] = 1;
      } while (i > j);

      validateArraysetBeforeGTPlus1(c, array, i_orig, i, j);
      return;
   }

   private void validateArraysetBeforeGTPlus1(Context c, int[] array, int i, int final_i, int j) {
      for (int k = i ; k >= j + 1; k--) {
         if (array[k] != 1)
        c.printerr("Error:arraysetBeforeGT @ array[" + k + "] expected: 1, got:" + array[k]);
         }
      for (int k = j; k >= 0; k--) {
         if (array[k] != 0)
            c.printerr("Error:arraysetBeforeGT @ array[" + k + "] expected: 0, got:" + array[k]);
      }

      if(final_i != j)
         c.printerr("Error:arraysetBeforeGT @ Final induction var value.  expected: " + j + " got:" + final_i);
      return;
   }


   /**
    * testCountDownLoopBeforeGE tests a count down arrayset loop with 
    * a decrement induction variable update before the array store,
    * and the loop exit test is >=.
    * @param c The test harness context
    */
   public void testCountDownLoopBeforeGE(Context c) {
      for (int iters = 0; iters < c.iterations(); ++iters) {
         arraysetBeforeGE(c,8,5);
         arraysetBeforeGE(c,2,1);
      }
   }
   
   private void arraysetBeforeGE(Context c, int i, int j) {

      int[] array = new int[i];
      int i_orig = i;
      do {
         i--;
         array[i] = 1;
         } while (i >= j);
      validateArraysetBeforeGE(c, array, i_orig, i, j);
      return;
   }

   private void validateArraysetBeforeGE(Context c, int[] array, int i, int final_i, int j) {
      for (int k = i - 1; k >= j - 1; k--) {
         if (array[k] != 1)
        c.printerr("Error:arraysetBeforeGE @ array[" + k + "] expected: 1, got:" + array[k]);
         }
      for (int k = j - 2; k >= 0; k--) {
         if (array[k] != 0)
            c.printerr("Error:arraysetBeforeGE @ array[" + k + "] expected: 0, got:" + array[k]);
      }

      if(final_i != j - 1)
         c.printerr("Error:arraysetBeforeGE @ Final induction var value.  expected: " + (j - 1) + " got:" + final_i);
      return;
   }

   /**
    * testCountDownLoopsBeforeGEPlus1 tests a count down arrayset loop with 
    * a decrement induction variable update before the array store,
    * the loop exit test is >=, and the index used to store the array
    * is the original value of i (before the update).  This will test
    * how idiom recognition handles commoning.
    * @param c The test harness context
    */
   public void testCountDownLoopsBeforeGEPlus1(Context c) {
      for (int iters = 0; iters < c.iterations(); ++iters) {
         arraysetBeforeGEPlus1(c,8,5);
         arraysetBeforeGEPlus1(c,1,0);
      }
   }

   private void arraysetBeforeGEPlus1(Context c, int i, int j) {

      int[] array = new int[i+1];
      int i_orig = i;
      do {
         i--;
         array[i+1] = 1;
         } while (i >= j);
      validateArraysetBeforeGEPlus1(c, array, i_orig, i, j);
      return;
   }

   private void validateArraysetBeforeGEPlus1(Context c, int[] array, int i, int final_i, int j) {
      for (int k = i; k >= j; k--) {
         if (array[k] != 1)
            c.printerr("Error:arraysetBeforeGEPlus1 @ array[" + k + "] expected: 1, got:" + array[k]);
         }
      for (int k = j - 1; k >= 0; k--) {
         if (array[k] != 0)
            c.printerr("Error:arraysetBeforeGEPlus1 @ array[" + k + "] expected: 0, got:" + array[k]);
      }

      if(final_i != j - 1)
         c.printerr("Error:arraysetBeforeGEPlus1 @ Final induction var value.  expected: " + (j - 1) + " got:" + final_i);
      return;
   }

   /**
    * testCountDownLoopAfterGT tests a count down arrayset loop with 
    * a decrement induction variable update after  the array store,
    * and the loop exit test is >.
    * @param c The test harness context
    */
   public void testCountDownLoopAfterGT(Context c) {
      for (int iters = 0; iters < c.iterations(); ++iters) {
         arraysetAfterGT(c,8,5);
         arraysetAfterGT(c,1,0);
      }
   }
   
   private void arraysetAfterGT(Context c, int i, int j) {

      int[] array = new int[i+1];
      int i_orig = i;
      do {
         array[i] = 1;
         i--;
      } while (i > j);

      validateArraysetAfterGT(c, array, i_orig, i, j);
   }

   private void validateArraysetAfterGT(Context c, int[] array, int i, int final_i, int j) {
      for (int k = i ; k >= j + 1; k--) {
         if (array[k] != 1)
            c.printerr("Error:arraysetAfterGT @ array[" + k + "] expected: 1, got:" + array[k]);
         }
      for (int k = j; k >= 0; k--) {
         if (array[k] != 0)
            c.printerr("Error:arraysetAfterGT @ array[" + k + "] expected: 0, got:" + array[k]);
      }

      if(final_i != j)
         c.printerr("Error:arraysetAfterGT @ Final induction var value.  expected: " + j + " got:" + final_i);
      return;
   }

   /**
    * testCountDownLoopAfterGE tests a count down arrayset loop with 
    * a decrement induction variable update after  the array store,
    * and the loop exit test is >=.
    * @param c The test harness context
    */
   public void testCountDownLoopAfterGE(Context c) {
      for (int iters = 0; iters < c.iterations(); ++iters) {
         arraysetAfterGT(c,8,5);
         arraysetAfterGT(c,2,1);
      }
   }

   private void arraysetAfterGE(Context c, int i, int j) {

      int[] array = new int[i+1];
      int i_orig = i;
      do {
         array[i] = 1;
         i--;
      } while (i >= j);
      validateArraysetAfterGE(c, array, i_orig, i, j);
   }

   private void validateArraysetAfterGE(Context c, int[] array, int i, int final_i, int j) {
      for (int k = i ; k >= j; k--) {
         if (array[k] != 1)
            c.printerr("Error:arraysetAfterGE @ array[" + k + "] expected: 1, got:" + array[k]);
         }
      for (int k = j - 1; k >= 0; k--) {
         if (array[k] != 0)
            c.printerr("Error:arraysetAfterGE @ array[" + k + "] expected: 0, got:" + array[k]);
      }

      if(final_i != j - 1)
         c.printerr("Error:arraysetAfterGE @ Final induction var value.  expected: " + (j - 1) + " got:" + final_i);
      return;
   }




   private void initByte(byte[] a, byte value) {
      for (int i = 0; i < a.length; ++i) {
         a[i] = value;
      }
   }

   public void testJ2SEByteSet(Context c) {
      initByte(byteArrA, (byte) 74);
      initByte(byteArrABase, (byte) 1);
      initByte(byteArrB, (byte) -73);
      initByte(byteArrBBase, (byte) -1);
      for (int iters = 0; iters < c.iterations()/4; ++iters) {
         // do basic comparisons
         java.util.Arrays.fill(byteArrA, (byte) 1);
         if (!java.util.Arrays.equals(byteArrA, byteArrABase)) {
            c.printerr("arrA/arrABase compares equal - wrong - on iter" + iters);
         }
         // do basic comparisons
         java.util.Arrays.fill(byteArrB, (byte) -1);
         if (!java.util.Arrays.equals(byteArrB, byteArrBBase)) {
            c.printerr("arrB/arrBBase compares equal - wrong - on iter" + iters);
         }
      }
   }
}
