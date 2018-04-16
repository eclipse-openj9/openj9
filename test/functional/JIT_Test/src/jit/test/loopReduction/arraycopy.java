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

public class arraycopy {
   public arraycopy() {
      super();
   }

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

   public static arraycopy[][] twoDByteArrA = { new arraycopy[4], new arraycopy[12], new arraycopy[2] };

   public static byte[][] twoDByteArrB = { new byte[400], new byte[400], new byte[400] };

   public static byte[][] twoDByteArrC = { new byte[400], new byte[400], new byte[400] };

   public static Object[] objArrA = { new byte[400], new char[400], new int[20], new short[0], new long[237], new Object() };
   public static Object[] objArrB = { new byte[400], new char[400], new int[20], new short[0], new long[237], new Object() };
   public static Object[] objArrC = { new byte[400], new char[400], new int[20], new short[0], new long[237], new Object() };

   public static Object[] strArrA = { new String("Hello"), new String("I Love You"), new String("Won't you tell me your name"), new String("Hello I Love you won't you jump in my game") };
   public static Object[] strArrB = { new String("We"), new String("All"), new String("Live in a Yellow Submarine") };
   public static Object[] strArrC = { new String("Who"), new String("are you"), new String("Who hoo Who hoo") };
   public static Object[] strArrD = { new String("I"), new String("Can't Get No"), new String("Satisfaction") };
   public static Object[] strArrTarget = new Object[1000];
   
   private void initByte(byte[] byteArr, byte value) {
      for (int i = 0; i < byteArr.length; ++i) {
         byteArr[i] = value;
      }
   }

   private void initChar(char[] charArr, char value) {
      for (int i = 0; i < charArr.length; ++i) {
         charArr[i] = value;
      }
   }

   public void testByteCopyConstantLength(Context c) {
      for (int iters = 0; iters < c.iterations() / 3; ++iters) {
         int i;
         byte initVal = 2;
         initByte(byteArrA, (byte) 1);
         initByte(byteArrB, (byte) 2);
         System.arraycopy(byteArrB, 0, byteArrA, 0, 100);
         if (c.verify())
            for (i = 0; i < 100; ++i) {
               if (byteArrA[i] != initVal) {
                  c.printerr("Byte array does not have the correct value on element " + i + " iteration " + iters);
                  break;
               }
            }
      }
   }

   public void testByteCopy(Context c) {
      for (int iters = 0; iters < c.iterations() / 3; ++iters) {
         int i;
         byte initVal = 2;
         initByte(byteArrA, (byte) 1);
         initByte(byteArrB, (byte) 2);
         System.arraycopy(byteArrB, 0, byteArrA, 0, byteArrA.length);
         if (c.verify())
            for (i = 0; i < byteArrA.length; ++i) {
               if (byteArrA[i] != initVal) {
                  c.printerr("Byte array does not have the correct value on element " + i + " iteration " + iters);
                  break;
               }
            }
      }
   }

   public void testByteCopyFixedLength(Context c) {
      for (int iters = 0; iters < c.iterations() / 3; ++iters) {
         int i;
         char initVal = 2;
         initChar(charArrA, (char) 1);
         initChar(charArrB, (char) 2);
         System.arraycopy(charArrA, 20, charArrA, 40, 513);
         System.arraycopy(charArrA, 40, charArrA, 20, 513);
         System.arraycopy(charArrB, 20, charArrB, 40, 513);
         System.arraycopy(charArrB, 40, charArrB, 20, 513);
         System.arraycopy(charArrB, 0, charArrA, 0, 513);
         if (c.verify())
            for (i = 0; i < 513; ++i) {
               if (charArrA[i] != initVal) {
                  c.printerr("Char array does not have the correct value on element " + i + " iteration " + iters);
                  break;
               }
            }
      }
   }

   public void testHandWrittenBackwardsCharToCharCopy(Context c) {
      for (int iters = 0; iters < c.iterations() / 6; ++iters) {
         char initVal = 3738;
         initChar(charArrA, (char) 1);
         initChar(charArrB, (char) 3738);
         for (int i = charArrA.length / 2 - 1; i > -1; --i) {
            charArrA[i] = charArrB[i];
         }
         for (int i = charArrA.length - 1; i >= charArrA.length / 2; --i) {
            charArrA[i] = charArrB[i];
         }
         if (c.verify())
            for (int i = 0; i < charArrA.length; ++i) {
               if (charArrA[i] != initVal) {
                  c.printerr("Backwards Char array does not have the correct value on element " + i + " iteration " + iters);
                  break;
               }
            }
      }
   }

   public void testHandWrittenCharToCharCopy(Context c) {
      for (int iters = 0; iters < c.iterations() / 3; ++iters) {
         char initVal = 2;
         initChar(charArrA, (char) 1);
         initChar(charArrB, (char) 2);
         for (int i = 0; i < charArrA.length / 2; ++i) {
            charArrA[i] = charArrB[i];
         }
         for (int i = charArrA.length / 2; i < charArrA.length; ++i) {
            charArrA[i] = charArrB[i];
         }
         if (c.verify())
            for (int i = 0; i < charArrA.length; ++i) {
               if (charArrA[i] != initVal) {
                  c.printerr("Char array does not have the correct value on element " + i + " iteration " + iters);
                  break;
               }
            }
      }
   }

   public void testHandWrittenByteToByteCopy(Context c) {
      for (int iters = 0; iters < c.iterations() / 3; ++iters) {
         byte initVal = 2;
         initByte(byteArrA, (byte) 1);
         initByte(byteArrB, (byte) 2);
         for (int i = 0; i < byteArrA.length / 2; ++i) {
            byteArrA[i] = byteArrB[i];
         }
         for (int i = byteArrA.length / 2; i < byteArrA.length; ++i) {
            byteArrA[i] = byteArrB[i];
         }
         if (c.verify())
            for (int i = 0; i < byteArrA.length; ++i) {
               if (byteArrA[i] != initVal) {
                  c.printerr("byte array does not have the correct value on element " + i + " iteration " + iters);
                  break;
               }
            }
      }
   }

   public static void referenceArraycopy(Object[] srcArr, int srcPos, Object[] destArr, int destPos, int length) {
      if (srcArr != destArr || srcPos > destPos || srcPos + length < destPos) {
         for (int i = 0; i < length; ++i) {
            destArr[i + destPos] = srcArr[i + srcPos];
         }
      } else {
         for (int i = length - 1; i >= 0; --i) {
            destArr[i + destPos] = srcArr[i + srcPos];
         }
      }
   }

   public void testSimpleObjToObjCopy(Context c) {
      for (int iters = 0; iters < c.iterations() * 500; ++iters) {
         referenceArraycopy(twoDByteArrB, 0, twoDByteArrC, 0, twoDByteArrB.length);
         if (c.verify())
            for (int i = 0; i < twoDByteArrA.length; ++i) {
               if (twoDByteArrC[i] != twoDByteArrB[i]) {
                  c.printerr("obj array does not have the correct value on element " + i + " iteration " + iters);
                  break;
               }
            }
      }
   }

   public void testObjToObjCopy(Context c) {
      for (int iters = 0; iters < c.iterations() * 500; ++iters) {
         System.arraycopy(twoDByteArrB, 0, twoDByteArrC, 0, twoDByteArrB.length);
         if (c.verify())
            for (int i = 0; i < twoDByteArrA.length; ++i) {
               if (twoDByteArrC[i] != twoDByteArrB[i]) {
                  c.printerr("obj array does not have the correct value on element " + i + " iteration " + iters);
                  break;
               }
            }
      }
   }

   public void testHandWrittenTwoDObjToObjCopy(Context c) 
   {
      for (int iters = 0; iters < c.iterations() * 100; ++iters) {
         for (int i = 0; i < twoDByteArrA.length; ++i) {
            twoDByteArrB[i] = twoDByteArrC[i];
         }
         if (c.verify())
            for (int i = 0; i < twoDByteArrA.length; ++i) {
               if (twoDByteArrB[i] != twoDByteArrC[i]) {
                  c.printerr("obj array does not have the correct value on element " + i + " iteration " + iters);
                  break;
               }
            }
      }
   }

   public void testHandWrittenSimpleCharToCharCopy(Context c) {
      for (int iters = 0; iters < c.iterations() / 3; ++iters) {
         char initVal = 2;
         initChar(charArrA, (char) 1);
         initChar(charArrB, (char) 2);
         for (int i = 0; i < charArrA.length; ++i) {
            charArrA[i] = charArrB[i];
         }
         if (c.verify())
            for (int i = 0; i < charArrA.length; ++i) {
               if (charArrA[i] != initVal) {
                  c.printerr("Char array does not have the correct value on element " + i + " iteration " + iters);
                  break;
               }
            }
      }
   }

   public void testHandWrittenSimpleByteToByteCopy(Context c) {
      for (int iters = 0; iters < c.iterations() / 3; ++iters) {
         byte initVal = 2;
         initByte(byteArrA, (byte) 1);
         initByte(byteArrB, (byte) 2);
         for (int i = 0; i < byteArrA.length; ++i) {
            byteArrA[i] = byteArrB[i];
         }
         if (c.verify())
            for (int i = 0; i < byteArrA.length; ++i) {
               if (byteArrA[i] != initVal) {
                  c.printerr("byte array does not have the correct value on element " + i + " iteration " + iters);
                  break;
               }
            }
      }
   }

   public void testHandWrittenByteToCharCopy(Context c) {
      for (int iters = 0; iters < c.iterations() / 3; ++iters) {
         char initVal = (256 - 67) * 256 + (256 - 67);
         initChar(charArrA, (char) 6363);
         initByte(byteArrB, (byte) -67);
         int byteIndex = 0;
         for (int i = 1; i < charArrA.length / 4; ++i) {
            charArrA[i] = (char) (((byteArrB[byteIndex] & 0xFF) << 8) | (byteArrB[byteIndex + 1] & 0xFF));
            byteIndex += 2;
         }
         for (int i = charArrA.length / 4; i < charArrA.length / 2; ++i) {
            charArrA[i] = (char) ((byteArrB[byteIndex] & 0xFF) << 8);
            charArrA[i] |= (byteArrB[byteIndex + 1] & 0xFF);
            byteIndex += 2;
         }
         if (c.verify()) {
            for (int i = 1; i < charArrA.length / 2; ++i) {
               if (charArrA[i] != initVal) {
                  c.printerr("char array does not have the correct value on element " + i + " iteration " + iters
                           + ". It has value " + (int) charArrA[i] + " instead of value " + (int) initVal);
                  break;
               }
            }
            if (charArrA[0] != charArrA[charArrA.length / 2] || charArrA[0] != 6363) {
               c.printerr("char array has had its start or end value changed");
            }
         }
      }
   }

   public void testHandWrittenCharToByteCopy(Context c) {
      for (int iters = 0; iters < c.iterations() / 3; ++iters) {
         byte initVal = (byte) -1;
         initChar(charArrA, (char) -1);
         initByte(byteArrA, (byte) 67);
         int byteIndex = 2;
         for (int i = 1; i < charArrA.length / 4; ++i) {
            byteArrA[byteIndex] = (byte) ((charArrA[i] & 0xFF00) >> 8);
            byteArrA[byteIndex + 1] = (byte) (charArrA[i] & 0xFF);
            byteIndex += 2;
         }
         for (int i = charArrA.length / 4; i < charArrA.length / 2 - 1; ++i) {
            byteArrA[byteIndex] = (byte) ((charArrA[i] & 0xFF00) >> 8);
            byteArrA[byteIndex + 1] = (byte) (charArrA[i] & 0xFF);
            byteIndex += 2;
         }
         if (c.verify()) {
            for (int i = 2; i < charArrA.length / 2 - 1; ++i) {
               if (byteArrA[i] != initVal) {
                  c.printerr("byte array does not have the correct value on element " + i + " iteration " + iters
                           + ". It has value " + (int) charArrA[i] + " instead of value " + (int) initVal);
                  break;
               }
            }
            if (byteArrA[0] != byteArrA[charArrA.length - 1] || byteArrA[0] != 67) {
               c.printerr("byte array has had its start or end value changed");
            }
         }
      }
   }

   public void testHandWrittenNestedByteToCharCopy(Context c) {
      boolean bigEndian;
      for (int iters = 0; iters < c.iterations() / 3; ++iters) {
         char initVal = (256 - 67) * 256 + (256 - 67);
         initChar(charArrA, (char) 6363);
         initByte(byteArrB, (byte) -67);
         int byteIndex = (charArrA[0] - (char) 6363); // complicated way to write 0
         int i = byteIndex + 1;
         if (iters % 2 == 0) {
            bigEndian = true;
            byteIndex += 2;
            ++i;
            charArrA[1] = initVal; // do it manually
         } else {
            bigEndian = false;
         }
         if (bigEndian) {
            for (; i < charArrA.length / 2; ++i) {
               charArrA[i] = (char) (((byteArrB[byteIndex] & 0xFF) << 8) | (byteArrB[byteIndex + 1] & 0xFF));
               byteIndex += 2;
            }
         } else {
            for (; i < charArrA.length / 2; ++i) {
               charArrA[i] = (char) (((byteArrB[byteIndex + 1] & 0xFF) << 8) | (byteArrB[byteIndex] & 0xFF));
               byteIndex += 2;
            }
         }
         if (c.verify()) {
            for (i = 1; i < charArrA.length / 2; ++i) {
               if (charArrA[i] != initVal) {
                  c.printerr("char array does not have the correct value on element " + i + " iteration " + iters
                           + ". It has value " + (int) charArrA[i] + " instead of value " + (int) initVal);
                  break;
               }
            }
         }
      }
   }

   public void testHandWrittenNestedCharToByteCopy(Context c) {
      boolean bigEndian;
      for (int iters = 0; iters < c.iterations() / 3; ++iters) {
         byte initVal = (byte) -1;
         initChar(charArrA, (char) -1);
         initByte(byteArrA, (byte) 67);
         int byteIndex = 2;
         if (iters % 2 == 0) {
            bigEndian = true;
         } else {
            bigEndian = false;
         }
         if (bigEndian) {
            for (int i = 1; i < charArrA.length / 2; ++i) {
               byteArrA[byteIndex] = (byte) ((charArrA[i] & 0xFF00) >> 8);
               byteArrA[byteIndex + 1] = (byte) (charArrA[i] & 0xFF);
               byteIndex += 2;
            }
         } else {
            for (int i = 1; i < charArrA.length / 2; ++i) {
               byteArrA[byteIndex] = (byte) (charArrA[i] & 0xFF);
               byteArrA[byteIndex + 1] = (byte) (charArrA[i] >> 8); // turns out that & 0xFF00 is a nop due to signedness
               byteIndex += 2;
            }
         }
         if (c.verify()) {
            for (int i = 2; i < charArrA.length / 2 - 1; ++i) {
               if (byteArrA[i] != initVal) {
                  c.printerr("byte array does not have the correct value on element " + i + " iteration " + iters
                           + ". It has value " + (int) charArrA[i] + " instead of value " + (int) initVal);
                  break;
               }
            }
         }
      }
   }

   public void testCharCopyProfiledFixedLength(Context c) {
      for (int iters = 0; iters < c.iterations() / 3; ++iters) {
         char initVal = 2;
         initChar(charArrA, (char) 1);
         initChar(charArrB, (char) 2);
         int length;
         for (int j = 0; j < 200; ++j) {
            if (j == 0) {
               length = 20;
            } else {
               length = 10;
            }
            java.lang.System.arraycopy(charArrB, 20 + j, charArrA, 40 + j, length);
         }
         if (c.verify()) {
            for (int i = 40; i < 249; ++i) {
               if (charArrA[i] != initVal) {
                  c.printerr("Char array does not have the correct value on element " + i + " iteration " + iters);
                  break;
               }
            }
         }
      }
   }

   
   private boolean noStringMatch() {
      return (!strArrTarget[0].equals(strArrA[0]) || !strArrTarget[strArrA.length].equals(strArrB[0])
            || !strArrTarget[strArrA.length + strArrB.length].equals(strArrC[0])
            || !strArrTarget[strArrA.length + strArrB.length + strArrC.length].equals(strArrD[0])
            || !strArrTarget[strArrA.length + strArrB.length + strArrC.length - 1].equals(strArrC[strArrC.length - 1]));
   }

   
   public void testHandWrittenStrArrToArrCopy(Context c) {
      for (int iters = 0; iters < c.iterations() * 500; ++iters) {

         int base = 0;
         for (int i = 0; i < strArrA.length; ++i) {
            strArrTarget[i + base] = strArrA[i];
         }
         base += strArrA.length;
         for (int i = 0; i < strArrB.length; ++i) {
            strArrTarget[i + base] = strArrB[i];
         }
         base += strArrB.length;
         for (int i = 0; i < strArrC.length; ++i) {
            strArrTarget[i + base] = strArrC[i];
         }
         base += strArrC.length;
         for (int i = 0; i < strArrD.length; ++i) {
            strArrTarget[i + base] = strArrD[i];
         }

         if (c.verify()) {
            if (noStringMatch()) {
               c.printerr("str array does not have the correct values on iteration " + iters);
               break;
            }
         }
      }
   }

   public void testMixedHandWrittenStrArrToArrCopy(Context c) {
      for (int iters = 0; iters < c.iterations() * 500; ++iters) {

         int base = 0;
         for (int i = strArrA.length-1; i>=0; --i) {
            strArrTarget[i + base] = strArrA[i];
         }
         base += strArrA.length;
         for (int i = 0; i < strArrB.length; ++i) {
            strArrTarget[i + base] = strArrB[i];
         }
         base += strArrB.length;
         for (int i = strArrC.length-1; i>=0; --i) {
            strArrTarget[i + base] = strArrC[i];
         }
         base += strArrC.length;
         for (int i = 0; i < strArrD.length; ++i) {
            strArrTarget[i + base] = strArrD[i];
         }

         if (c.verify()) {
            if (noStringMatch()) {
               c.printerr("str array does not have the correct values on iteration " + iters);
               break;
            }
         }
      }
   }

   public void testExplicitStrArrToArrCopy(Context c) {
      for (int iters = 0; iters < c.iterations() * 500; ++iters) {
         System.arraycopy(strArrA, 0, strArrTarget, 0, strArrA.length);
         System.arraycopy(strArrB, 0, strArrTarget, strArrA.length, strArrB.length);
         System.arraycopy(strArrC, 0, strArrTarget, strArrA.length+strArrB.length, strArrC.length);
         System.arraycopy(strArrD, 0, strArrTarget, strArrA.length+strArrB.length+strArrC.length, strArrD.length);

         if (c.verify()) {
            if (noStringMatch()) {
               c.printerr("str array does not have the correct values on iteration " + iters);
               break;
            }
         }
      }
   }
   
}
