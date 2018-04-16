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

public class arraycompare
{
   public static byte[] byteArrA = new byte[8097];
   public static byte[] byteArrB = new byte[8097];
   public static byte[] byteArrC = new byte[8097];
   public static char[] charArrA = new char[8097];
   public static char[] charArrB = new char[8097];
   public static char[] charArrC = new char[8097];
   public static double[] doubleArrA = new double[8097];
   public static double[] doubleArrB = new double[8097];
   public static double[] doubleArrC = new double[8097];
   public static float[] floatArrA = new float[8097];
   public static float[] floatArrB = new float[8097];
   public static float[] floatArrC = new float[8097];
   public static int[] intArrA = new int[8097];
   public static int[] intArrB = new int[8097];
   public static int[] intArrC = new int[8097];
   public static long[] longArrA = new long[8097];
   public static long[] longArrB = new long[8097];
   public static long[] longArrC = new long[8097];
   public static short[] shortArrA = new short[8097];
   public static short[] shortArrB = new short[8097];
   public static short[] shortArrC = new short[8097];
   public static byte[] smallArrA = new byte[40];
   public static byte[] smallArrB = new byte[20];
   public static byte[] smallArrC = new byte[20];
   public static byte[][] twoDByteArrA = { new byte[400], new byte[400], new byte[400] };
   public static byte[][] twoDByteArrB = { new byte[400], new byte[400], new byte[400] };
   public static byte[][] twoDByteArrC = { new byte[400], new byte[400], new byte[400] };
   public static String stringA = null;
   public static String stringB = null;
   public static String stringC = null;
   public arraycompare()
   {
      super();
   }
   private void initByte(byte[] byteArr, byte value)
   {
      for (int i = 0; i < byteArr.length; ++i)
      {
         byteArr[i] = value;
      }
   }
   private void initChar(char[] charArr, char value)
   {
      for (int i = 0; i < charArr.length; ++i)
      {
         charArr[i] = value;
      }
   }
   private void initDouble(double[] doubleArr, double value)
   {
      for (int i = 0; i < doubleArr.length; ++i)
      {
         doubleArr[i] = value;
      }
   }
   private void initFloat(float[] floatArr, float value)
   {
      for (int i = 0; i < floatArr.length; ++i)
      {
         floatArr[i] = value;
      }
   }
   private void initInt(int[] intArr, int value)
   {
      for (int i = 0; i < intArr.length; ++i)
      {
         intArr[i] = value;
      }
   }
   private void initLong(long[] longArr, long value)
   {
      for (int i = 0; i < longArr.length; ++i)
      {
         longArr[i] = value;
      }
   }
   private void initShort(short[] shortArr, short value)
   {
      for (int i = 0; i < shortArr.length; ++i)
      {
         shortArr[i] = value;
      }
   }
   private String createString(char c)
   {
      StringBuffer buff = new StringBuffer(1024);
      for (int i = 0; i < 1024; ++i)
      {
         buff.append(c);
      }
      return new String(buff);
   }
   private String createString(char c, int altIndex, char altC)
   {
      StringBuffer buff = new StringBuffer(1024);
      for (int i = 0; i < 1024; ++i)
      {
         if (altIndex == i)
         {
            buff.append(altC);
         } else
         {
            buff.append(c);
         }
      }
      return new String(buff);
   }
   public void testNotByteCompare(Context c)
   {
      for (int iters = 0; iters < c.iterations(); ++iters)
      {
         int i = 0;
         for (; i < byteArrA.length; ++i)
         {
            if (byteArrA[i] != byteArrB[i])
            {
               break;
            }
         }
         if (i == 27)
         {
            System.out.println("special case");
         }
      }
   }
   public void testNotByteCompareV2(Context c) {
      for (int iters = 0; iters < c.iterations(); ++iters) {
         int i = 0;
         for (; i < byteArrA.length; ++i) {
            if (byteArrA[i] != byteArrB[i]) {
               break;
            }
         }
         if (i != byteArrA.length) {
         } else if (i == 23) {
            System.out.println("another special case");
         }
      }
   }
   public void testNotByteCompareV3(Context c) {
      int i = 0;
      for (int iters = 0; iters < c.iterations(); ++iters) {
         i = 0;
         for (; i < byteArrA.length; ++i) {
            if (byteArrA[i] != byteArrB[i]) {
               break;
            }
         }
         if (i != byteArrA.length) {
         }
      }
      if (i == 23) {
         System.out.println("yet another special case");
      }
   }
   public void testByteKnownSizeCompareBig(Context c)
   {
      initByte(byteArrA, (byte) 1);
      initByte(byteArrB, (byte) 2);
      initByte(byteArrC, (byte) 1);
      for (int iters = 0; iters < c.iterations(); ++iters)
      {
         if (byteArrA.length != byteArrC.length)
         {
            c.printerr("Byte array A/C compare is not equal");
            return;
         }
         int i;
         for (i = 0; i < 8097; ++i)
         {
            if (byteArrA[i] != byteArrC[i])
            {
               break;
            }
         }
         if (i != 8097)
         {
            c.printerr("Byte array A/C compare is not equal");
            return;
         }
      }
   }
   public void testByteKnownSizeCompareMedium(Context c)
   {
      initByte(byteArrA, (byte) 1);
      initByte(byteArrB, (byte) 2);
      initByte(byteArrC, (byte) 1);
      for (int iters = 0; iters < c.iterations() * 50; ++iters)
      {
         if (byteArrA.length != byteArrC.length)
         {
            c.printerr("Byte array A/C compare is not equal");
            return;
         }
         int i;
         for (i = 0; i < 257; ++i)
         {
            if (byteArrA[i] != byteArrC[i])
            {
               break;
            }
         }
         if (i != 257)
         {
            c.printerr("Byte array A/C compare is not equal");
            return;
         }
      }
   }
   public void testByteKnownSizeCompareSmall(Context c)
   {
      initByte(byteArrA, (byte) 1);
      initByte(byteArrB, (byte) 2);
      initByte(byteArrC, (byte) 1);
      for (int iters = 0; iters < c.iterations() * 500; ++iters)
      {
         if (byteArrA.length != byteArrC.length)
         {
            c.printerr("Byte array A/C compare is not equal");
            return;
         }
         int i;
         for (i = 0; i < 25; ++i)
         {
            if (byteArrA[i] != byteArrC[i])
            {
               break;
            }
         }
         if (i != 25)
         {
            c.printerr("Byte array A/C compare is not equal");
            return;
         }
      }
   }
   public void testByteSingleCompare(Context c)
   {
      initByte(byteArrA, (byte) 1);
      initByte(byteArrB, (byte) 2);
      initByte(byteArrC, (byte) 1);
      for (int iters = 0; iters < c.iterations(); ++iters)
      {
         if (!equals(byteArrA, byteArrC))
         {
            c.printerr("Byte array A/C compare is not equal");
         }
      }
   }
   public void testByteCompare(Context c)
   {
      initByte(byteArrA, (byte) 1);
      initByte(byteArrB, (byte) 2);
      initByte(byteArrC, (byte) 1);
      for (int iters = 0; iters < c.iterations(); ++iters)
      {
         if (equals(byteArrA, byteArrB))
         {
            c.printerr("Byte array A/B compare is not unequal");
         }
         if (!equals(byteArrA, byteArrC))
         {
            c.printerr("Byte array A/C compare is not equal");
         }
         byte orig = byteArrC[byteArrC.length >> 1];
         // msf - was / 2 but changed to work around bug
         byteArrC[byteArrC.length - 1] = 0;
         if (equals(byteArrA, byteArrC))
         {
            c.printerr("Byte array A/C compare is not unequal");
         }
         byteArrC[byteArrC.length - 1] = orig;
         byteArrC[0] = 0;
         if (equals(byteArrA, byteArrC))
         {
            c.printerr("Byte array A/C second compare is not unequal");
         }
         byteArrC[0] = orig;
      }
   }
   private boolean equals(byte[] arrA, byte[] arrB)
   {
      if (arrA.length != arrB.length)
      {
         return false;
      }
      int i;
      for (i = 0; i < arrA.length; ++i)
      {
         if (arrA[i] != arrB[i])
         {
            break;
         }
      }
      return i == arrA.length;
   }
   public void testCharSingleCompare(Context c)
   {
      initChar(charArrA, (char) 1);
      initChar(charArrB, (char) 2);
      initChar(charArrC, (char) 1);
      for (int iters = 0; iters < c.iterations()/2; ++iters)
      {
         if (!equals(charArrA, charArrC))
         {
            c.printerr("Char array A/C compare is not equal");
         }
      }
   }
   public void testCharCompare(Context c)
   {
      initChar(charArrA, (char) 1);
      initChar(charArrB, (char) 2);
      initChar(charArrC, (char) 1);
      for (int iters = 0; iters < c.iterations()/2; ++iters)
      {
         if (equals(charArrA, charArrB))
         {
            c.printerr("Char array A/B compare is not unequal");
         }
         if (!equals(charArrA, charArrC))
         {
            c.printerr("Char array A/C compare is not equal");
         }
         char orig = charArrC[charArrC.length >> 1];
         charArrC[charArrC.length - 1] = 0;
         if (equals(charArrA, charArrC))
         {
            c.printerr("Char array A/C compare is not unequal");
         }
         charArrC[charArrC.length - 1] = orig;
         charArrC[0] = 0;
         if (equals(charArrA, charArrC))
         {
            c.printerr("Char array A/C second compare is not unequal");
         }
         charArrC[0] = orig;
      }
   }
   private boolean equals(char[] arrA, char[] arrB)
   {
      if (arrA.length != arrB.length)
      {
         return false;
      }
      for (int i = 0; i < arrA.length; ++i)
      {
         if (arrA[i] != arrB[i])
         {
            return false;
         }
      }
      return true;
   }
   public void testSingleDoubleCompare(Context c)
   {
      initDouble(doubleArrA, 1);
      initDouble(doubleArrB, 2);
      initDouble(doubleArrC, 1);
      for (int iters = 0; iters < c.iterations()/8; ++iters)
      {
         if (!equals(doubleArrA, doubleArrC))
         {
            c.printerr("Double array A/C compare is not equal");
         }
      }
   }
   public void testDoubleCompare(Context c)
   {
      initDouble(doubleArrA, 1);
      initDouble(doubleArrB, 2);
      initDouble(doubleArrC, 1);
      for (int iters = 0; iters < c.iterations()/8; ++iters)
      {
         if (equals(doubleArrA, doubleArrB))
         {
            c.printerr("Double array A/B compare is not unequal");
         }
         if (!equals(doubleArrA, doubleArrC))
         {
            c.printerr("Double array A/C compare is not equal");
         }
         double orig = doubleArrC[doubleArrC.length >> 1];
         doubleArrC[doubleArrC.length - 1] = 0;
         if (equals(doubleArrA, doubleArrC))
         {
            c.printerr("Double array A/C compare is not unequal");
         }
         doubleArrC[doubleArrC.length - 1] = orig;
         doubleArrC[0] = 0;
         if (equals(doubleArrA, doubleArrC))
         {
            c.printerr("Double array A/C second compare is not unequal");
         }
         doubleArrC[0] = orig;
      }
   }
   private boolean equals(double[] arrA, double[] arrB)
   {
      if (arrA.length != arrB.length)
      {
         return false;
      }
      for (int i = 0; i < arrA.length; ++i)
      {
         if (arrA[i] != arrB[i])
         {
            return false;
         }
      }
      return true;
   }
   public void testSingleFloatCompare(Context c)
   {
      initFloat(floatArrA, 1);
      initFloat(floatArrB, 2);
      initFloat(floatArrC, 1);
      for (int iters = 0; iters < c.iterations()/4; ++iters)
      {
         if (!equals(floatArrA, floatArrC))
         {
            c.printerr("Float array A/C compare is not equal");
         }
      }
   }
   public void testFloatCompare(Context c)
   {
      initFloat(floatArrA, 1);
      initFloat(floatArrB, 2);
      initFloat(floatArrC, 1);
      for (int iters = 0; iters < c.iterations()/4; ++iters)
      {
         if (equals(floatArrA, floatArrB))
         {
            c.printerr("Float array A/B compare is not unequal");
         }
         if (!equals(floatArrA, floatArrC))
         {
            c.printerr("Float array A/C compare is not equal");
         }
         float orig = floatArrC[floatArrC.length >> 1];
         floatArrC[floatArrC.length - 1] = 0;
         if (equals(floatArrA, floatArrC))
         {
            c.printerr("Float array A/C compare is not unequal");
         }
         floatArrC[floatArrC.length - 1] = orig;
         floatArrC[0] = 0;
         if (equals(floatArrA, floatArrC))
         {
            c.printerr("Float array A/C second compare is not unequal");
         }
         floatArrC[0] = orig;
      }
   }
   private boolean equals(float[] arrA, float[] arrB)
   {
      if (arrA.length != arrB.length)
      {
         return false;
      }
      for (int i = 0; i < arrA.length; ++i)
      {
         if (arrA[i] != arrB[i])
         {
            return false;
         }
      }
      return true;
   }
   public void testIntCompare(Context c)
   {
      initInt(intArrA, 1);
      initInt(intArrB, 2);
      initInt(intArrC, 1);
      for (int iters = 0; iters < c.iterations()/4; ++iters)
      {
         if (equals(intArrA, intArrB))
         {
            c.printerr("Int array A/B compare is not unequal");
         }
         if (!equals(intArrA, intArrC))
         {
            c.printerr("Int array A/C compare is not equal");
         }
         int orig = intArrC[intArrC.length >> 1];
         intArrC[intArrC.length - 1] = 0;
         if (equals(intArrA, intArrC))
         {
            c.printerr("Int array A/C compare is not unequal");
         }
         intArrC[intArrC.length - 1] = orig;
         intArrC[0] = 0;
         if (equals(intArrA, intArrC))
         {
            c.printerr("Int array A/C second compare is not unequal");
         }
         intArrC[0] = orig;
      }
   }
   public void testSingleIntCompare(Context c)
   {
      initInt(intArrA, 1);
      initInt(intArrB, 2);
      initInt(intArrC, 1);
      for (int iters = 0; iters < c.iterations()/4; ++iters)
      {
         if (!equals(intArrA, intArrC))
         {
            c.printerr("Int array A/C compare is not equal");
         }
      }
   }
   private boolean equals(int[] arrA, int[] arrB)
   {
      if (arrA.length != arrB.length)
      {
         return false;
      }
      for (int i = 0; i < arrA.length; ++i)
      {
         if (arrA[i] != arrB[i])
         {
            return false;
         }
      }
      return true;
   }
   public void testSingleLongCompare(Context c)
   {
      initLong(longArrA, 1);
      initLong(longArrB, 2);
      initLong(longArrC, 1);
      for (int iters = 0; iters < c.iterations()/8; ++iters)
      {
         if (!equals(longArrA, longArrC))
         {
            c.printerr("Long array A/C compare is not equal");
         }
      }
   }
   public void testLongCompare(Context c)
   {
      initLong(longArrA, 1);
      initLong(longArrB, 2);
      initLong(longArrC, 1);
      for (int iters = 0; iters < c.iterations()/8; ++iters)
      {
         if (equals(longArrA, longArrB))
         {
            c.printerr("Long array A/B compare is not unequal");
         }
         if (!equals(longArrA, longArrC))
         {
            c.printerr("Long array A/C compare is not equal");
         }
         long orig = longArrC[longArrC.length >> 1];
         longArrC[longArrC.length - 1] = 0;
         if (equals(longArrA, longArrC))
         {
            c.printerr("Long array A/C compare is not unequal");
         }
         longArrC[longArrC.length - 1] = orig;
         longArrC[0] = 0;
         if (equals(longArrA, longArrC))
         {
            c.printerr("Long array A/C second compare is not unequal");
         }
         longArrC[0] = orig;
      }
   }
   private boolean equals(long[] arrA, long[] arrB)
   {
      if (arrA.length != arrB.length)
      {
         return false;
      }
      for (int i = 0; i < arrA.length; ++i)
      {
         if (arrA[i] != arrB[i])
         {
            return false;
         }
      }
      return true;
   }
   public void testShortCompare(Context c)
   {
      initShort(shortArrA, (short) 1);
      initShort(shortArrB, (short) 2);
      initShort(shortArrC, (short) 1);
      for (int iters = 0; iters < c.iterations()/2; ++iters)
      {
         if (equals(shortArrA, shortArrB))
         {
            c.printerr("Short array A/B compare is not unequal");
         }
         if (!equals(shortArrA, shortArrC))
         {
            c.printerr("Short array A/C compare is not equal");
         }
         short orig = shortArrC[shortArrC.length >> 1];
         shortArrC[shortArrC.length - 1] = 0;
         if (equals(shortArrA, shortArrC))
         {
            c.printerr("Short array A/C compare is not unequal");
         }
         shortArrC[shortArrC.length - 1] = orig;
         shortArrC[0] = 0;
         if (equals(shortArrA, shortArrC))
         {
            c.printerr("Short array A/C second compare is not unequal");
         }
         shortArrC[0] = orig;
      }
   }
   public void testSingleShortCompare(Context c)
   {
      initShort(shortArrA, (short) 1);
      initShort(shortArrB, (short) 2);
      initShort(shortArrC, (short) 1);
      for (int iters = 0; iters < c.iterations()/8; ++iters)
      {
         if (!equals(shortArrA, shortArrC))
         {
            c.printerr("Short array A/C compare is not equal");
         }
      }
   }
   private boolean equals(short[] arrA, short[] arrB)
   {
      if (arrA.length != arrB.length)
      {
         return false;
      }
      for (int i = 0; i < arrA.length; ++i)
      {
         if (arrA[i] != arrB[i])
         {
            return false;
         }
      }
      return true;
   }
   public void testTwoDCompare(Context c)
   {
      inittwoDByte(twoDByteArrA, (byte) 1);
      inittwoDByte(twoDByteArrB, (byte) 2);
      inittwoDByte(twoDByteArrC, (byte) 1);
      for (int iters = 0; iters < c.iterations()/2; ++iters)
      {
         if (twoDEquals(twoDByteArrA, twoDByteArrB))
         {
            c.printerr("twoDByte array A/B compare is not unequal");
         }
         if (!twoDEquals(twoDByteArrA, twoDByteArrC))
         {
            c.printerr("twoDByte array A/C compare is not equal on iter " + iters);
         }
         byte orig = twoDByteArrC[twoDByteArrC.length >> 1][twoDByteArrC[twoDByteArrC.length >> 1].length >> 1];
         twoDByteArrC[twoDByteArrC.length >> 1][twoDByteArrC[twoDByteArrC.length >> 1].length >> 1] = 0;
         if (twoDEquals(twoDByteArrA, twoDByteArrC))
         {
            c.printerr("twoDByte array A/C compare is not unequal");
         }
         twoDByteArrC[twoDByteArrC.length >> 1][twoDByteArrC[twoDByteArrC.length >> 1].length >> 1] = orig;
         twoDByteArrC[0][0] = 0;
         if (twoDEquals(twoDByteArrA, twoDByteArrC))
         {
            c.printerr("twoDByte array A/C second compare is not unequal");
         }
         twoDByteArrC[0][0] = orig;
      }
   }
   private boolean twoDEquals(byte[][] twoDByteArrA, byte[][] twoDByteArrB)
   {
      if (twoDByteArrA.length != twoDByteArrB.length)
      {
         return false;
      }
      for (int i = 0; i < twoDByteArrA.length; ++i)
      {
         if (twoDByteArrA[i].length != twoDByteArrB[i].length)
         {
            return false;
         }
         for (int j = 0; j < twoDByteArrA[i].length; ++j)
         {
            if (twoDByteArrA[i][j] != twoDByteArrB[i][j])
            {
               return false;
            }
         }
      }
      return true;
   }
   private void inittwoDByte(byte[][] byteArr, byte b)
   {
      for (int i = 0; i < byteArr.length; ++i)
      {
         for (int j = 0; j < byteArr[i].length; ++j)
         {
            byteArr[i][j] = b;
         }
      }
   }
   public void testJ2SEByteCompare(Context c)
   {
      initByte(byteArrA, (byte) 1);
      initByte(byteArrB, (byte) 1);
      initByte(byteArrC, (byte) 77);
      for (int iters = 0; iters < c.iterations(); ++iters)
      {
         // do basic comparisons
         if (java.util.Arrays.equals(byteArrA, byteArrC))
         {
            c.printerr("arrA/arrC compares equal - wrong - on iter" + iters);
         }
         if (!java.util.Arrays.equals(byteArrA, byteArrB))
         {
            c.printerr("arrA/arrB compares unequal - wrong - on iter" + iters);
         }
         // force an unequal comparison in the middle, end, start to ensure
         // the entire array is being compared
         byte orig = byteArrA[byteArrA.length >> 1];
         byteArrA[byteArrA.length >> 1] = 6;
         if (java.util.Arrays.equals(byteArrA, byteArrB))
         {
            c.printerr("byteArrA/byteArrB compares equal after mod 1 - wrong - on iter" + iters);
         }
         byteArrA[byteArrA.length >> 1] = orig;
         orig = byteArrA[byteArrA.length - 1];
         byteArrA[byteArrA.length - 1] = -4;
         if (java.util.Arrays.equals(byteArrA, byteArrB))
         {
            c.printerr("byteArrA/byteArrB compares equal after mod 2 - wrong - on iter" + iters);
         }
         byteArrA[byteArrA.length - 1] = orig;
         orig = byteArrA[0];
         byteArrA[0] = 25;
         if (java.util.Arrays.equals(byteArrA, byteArrB))
         {
            c.printerr("byteArrA/byteArrB compares equal after mod 3 - wrong - on iter" + iters);
         }
         byteArrA[0] = orig;
      }
   }
   public void testJ2SEStringCompare(Context c)
   {
      stringA = createString((char) 1);
      stringB = createString((char) 2);
      stringC = createString((char) 1);
      String stringStart = createString((char) 1, 0, (char) 6);
      String stringMiddle = createString((char) 1, stringA.length() >> 1, (char) 7);
      String stringEnd = createString((char) 1, stringA.length() - 1, (char) 8);
      underlyingTestJ2SEStringCompare(c, stringA, stringB, stringC, stringStart, stringMiddle, stringEnd);
   }
   public void underlyingTestJ2SEStringCompare(
      Context c,
      String stringA,
      String stringB,
      String stringC,
      String stringStart,
      String stringMiddle,
      String stringEnd)
   {
      for (int iters = 0; iters < c.iterations(); ++iters)
      {
         // do basic comparisons
         if (stringA.equals(stringB))
         {
            c.printerr("stringA/stringB compares equal - wrong - on iter" + iters);
         }
         if (!stringA.equals(stringC))
         {
            c.printerr("stringA/stringC compares unequal - wrong - on iter" + iters);
         }
         // force an unequal comparison in the start, middle, end to ensure
         // the entire array is being compared
         if (stringA.equals(stringStart))
         {
            c.printerr("stringA compares equal for startA after " + iters);
         }
         if (stringA.equals(stringMiddle))
         {
            c.printerr("stringA compares equal for middleA after " + iters);
         }
         if (stringA.equals(stringEnd))
         {
            c.printerr("stringA compares equal for endA after " + iters);
         }
      }
   }
}
