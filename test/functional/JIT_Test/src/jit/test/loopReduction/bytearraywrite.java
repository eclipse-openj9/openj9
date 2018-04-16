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

import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class bytearraywrite {
   public static byte[] ba;

   public static ByteBuffer bb;

   public static ByteBuffer dbb;

   public static int alignedOffset = 0x100;

   public static int misalignedOffset = 0x111;

   public static byte byteVal = 0x12;

   public static short shortVal = 0x7890;

   public static char charVal = 0x1234;

   public static int intVal = 0x789ABCDE;

   public static int intValDup8 = 0x78BCBCDE;

   public static long longVal = 0x123456789ABCDEF0L;

   public static final int arrayItersMultiplier  = 5000;
   public static final int bufferItersMultiplier = 2000;

   static {
      bb = ByteBuffer.allocate(176543);
      dbb = ByteBuffer.allocateDirect(181818);
      ba = new byte[18097];
   }

   public void testByteBufferWriteLELong(Context c) {
      bb.order(ByteOrder.LITTLE_ENDIAN);
      for (int m = 0; m < bufferItersMultiplier/8; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            bb.position(alignedOffset);
            bb.putLong(longVal);
            bb.putLong(longVal);
            bb.putLong(misalignedOffset, longVal);
            bb.position(misalignedOffset + 8);
            bb.putLong(longVal);
         }
      if (c.verify()) {
         long valA = bb.getLong(alignedOffset);
         long valB = bb.getLong(alignedOffset + 8);
         long valC = bb.getLong(misalignedOffset);
         long valD = bb.getLong(misalignedOffset + 8);
         if (valA != longVal || valB != longVal || valC != longVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Values: " + valA + "," + valB + "," + valC + "," + valD);
         }
      }
   }

   public void testByteBufferWriteBELong(Context c) {
      bb.order(ByteOrder.BIG_ENDIAN);
      for (int m = 0; m < bufferItersMultiplier/8; ++m)
         for (long iters = 0; iters < c.iterations(); ++iters) {
            bb.position(alignedOffset);
            bb.putLong(longVal);
            bb.putLong(longVal);
            bb.putLong(misalignedOffset, longVal);
            bb.position(misalignedOffset + 8);
            bb.putLong(longVal);
         }
      if (c.verify()) {
         long valA = bb.getLong(alignedOffset);
         long valB = bb.getLong(alignedOffset + 8);
         long valC = bb.getLong(misalignedOffset);
         long valD = bb.getLong(misalignedOffset + 8);
         if (valA != longVal || valB != longVal || valC != longVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Values: " + valA + "," + valB + "," + valC + "," + valD);
         }
      }
   }

   public void testByteBufferWriteLEInt(Context c) {
      bb.order(ByteOrder.LITTLE_ENDIAN);

      for (int m = 0; m < bufferItersMultiplier/4; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            bb.position(alignedOffset);
            bb.putInt(intVal);
            bb.putInt(intVal);
            bb.putInt(misalignedOffset, intVal);
            bb.putInt(misalignedOffset + 4, intVal);
         }
      if (c.verify()) {
         int valA = bb.getInt(alignedOffset);
         int valB = bb.getInt(alignedOffset + 4);
         int valC = bb.getInt(misalignedOffset);
         int valD = bb.getInt(misalignedOffset + 4);
         if (valA != intVal || valB != intVal || valC != intVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Values: " + valA + "," + valB + "," + valC + "," + valD);
         }
      }
   }

   public void testByteBufferWriteBEInt(Context c) {
      bb.order(ByteOrder.BIG_ENDIAN);

      for (int m = 0; m < bufferItersMultiplier / 4; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            bb.position(alignedOffset);
            bb.putInt(intVal);
            bb.putInt(intVal);
            bb.putInt(misalignedOffset, intVal);
            bb.putInt(misalignedOffset + 4, intVal);
         }
      if (c.verify()) {
         int valA = bb.getInt(alignedOffset);
         int valB = bb.getInt(alignedOffset + 4);
         int valC = bb.getInt(misalignedOffset);
         int valD = bb.getInt(misalignedOffset + 4);
         if (valA != intVal || valB != intVal || valC != intVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Values: " + valA + "," + valB + "," + valC + "," + valD);
         }
      }
   }

   public void testByteBufferWriteLEShort(Context c) {
      bb.order(ByteOrder.LITTLE_ENDIAN);

      for (int m = 0; m < bufferItersMultiplier/2; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            bb.position(alignedOffset);
            bb.putShort(shortVal);
            bb.putShort(shortVal);
            bb.putShort(misalignedOffset, shortVal);
            bb.putShort(misalignedOffset + 2, shortVal);
         }
      if (c.verify()) {
         short valA = bb.getShort(alignedOffset);
         short valB = bb.getShort(alignedOffset + 2);
         short valC = bb.getShort(misalignedOffset);
         short valD = bb.getShort(misalignedOffset + 2);
         if (valA != shortVal || valB != shortVal || valC != shortVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Values: " + valA + "," + valB + "," + valC + "," + valD);
         }
      }
   }

   public void testByteBufferWriteBEShort(Context c) {
      bb.order(ByteOrder.BIG_ENDIAN);
      for (int m = 0; m < bufferItersMultiplier/2; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            bb.position(alignedOffset);
            bb.putShort(shortVal);
            bb.putShort(shortVal);
            bb.putShort(misalignedOffset, shortVal);
            bb.putShort(misalignedOffset + 2, shortVal);
         }
      if (c.verify()) {
         short valA = bb.getShort(alignedOffset);
         short valB = bb.getShort(alignedOffset + 2);
         short valC = bb.getShort(misalignedOffset);
         short valD = bb.getShort(misalignedOffset + 2);
         if (valA != shortVal || valB != shortVal || valC != shortVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Values: " + valA + "," + valB + "," + valC + "," + valD);
         }
      }
   }

   public void testByteBufferWriteLEChar(Context c) {
      bb.order(ByteOrder.LITTLE_ENDIAN);

      for (int m = 0; m < bufferItersMultiplier/8; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            bb.position(alignedOffset);
            bb.putChar(charVal);
            bb.putChar(charVal);
            bb.putChar(misalignedOffset, charVal);
            bb.putChar(misalignedOffset + 2, charVal);
         }
      if (c.verify()) {
         char valA = bb.getChar(alignedOffset);
         char valB = bb.getChar(alignedOffset + 2);
         char valC = bb.getChar(misalignedOffset);
         char valD = bb.getChar(misalignedOffset + 2);
         if (valA != charVal || valB != charVal || valC != charVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Values: " + valA + "," + valB + "," + valC + "," + valD);
         }
      }
   }

   public void testByteBufferWriteBEChar(Context c) {
      bb.order(ByteOrder.BIG_ENDIAN);
      for (int m = 0; m < bufferItersMultiplier/8; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            bb.position(alignedOffset);
            bb.putChar(charVal);
            bb.putChar(charVal);
            bb.putChar(misalignedOffset, charVal);
            bb.putChar(misalignedOffset + 2, charVal);
         }
      if (c.verify()) {
         char valA = bb.getChar(alignedOffset);
         char valB = bb.getChar(alignedOffset + 2);
         char valC = bb.getChar(misalignedOffset);
         char valD = bb.getChar(misalignedOffset + 2);
         if (valA != charVal || valB != charVal || valC != charVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Values: " + valA + "," + valB + "," + valC + "," + valD);
         }
      }
   }

   public void testByteArrayWriteBEShort(Context c) {

      for (int m = 0; m < arrayItersMultiplier; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i + 0] = (byte) ((shortVal >>> 8) & 0xFF);
            ba[i + 1] = (byte) (shortVal & 0xFF);
         }
      if (c.verify()) {
         short valA = (short) (((ba[alignedOffset + 0] & 0xff) << 8) | (ba[alignedOffset + 1] & 0xff));
         if (valA != shortVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

   public void testByteArrayWriteLEShort(Context c) {

      for (int m = 0; m < arrayItersMultiplier/2; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i + 1] = (byte) ((shortVal >>> 8) & 0xFF);
            ba[i + 0] = (byte) (shortVal & 0xFF);
         }
      if (c.verify()) {
         short valA = (short) (((ba[alignedOffset + 1] & 0xff) << 8) | (ba[alignedOffset + 0] & 0xff));
         if (valA != shortVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

   public void testByteArrayWriteLEChar(Context c) {

      for (int m = 0; m < arrayItersMultiplier/2; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i + 1] = (byte) ((charVal >>> 8) & 0xFF);
            ba[i + 0] = (byte) (charVal & 0xFF);
         }
      if (c.verify()) {
         char valA = (char) (((ba[alignedOffset + 1] & 0xff) << 8) | (ba[alignedOffset + 0] & 0xff));
         if (valA != charVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

   public void testByteArrayWriteBEInt(Context c) {

      for (int m = 0; m < arrayItersMultiplier/4; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i] = (byte) (intVal >>> 24);
            ba[i + 1] = (byte) (intVal >>> 16);
            ba[i + 2] = (byte) ((intVal >>> 8) & 0xFF);
            ba[i + 3] = (byte) (intVal & 0xFF);
         }
      if (c.verify()) {
         int valA = (ba[alignedOffset] << 24) | ((ba[alignedOffset + 1] & 0xff) << 16) | ((ba[alignedOffset + 2] & 0xff) << 8)
                  | (ba[alignedOffset + 3] & 0xff);
         if (valA != intVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

   public void testByteArrayWriteLEInt(Context c) {

      for (int m = 0; m < arrayItersMultiplier/4; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i + 3] = (byte) (intVal >>> 24);
            ba[i + 2] = (byte) ((intVal >> 16) & 0xff);
            ba[i + 1] = (byte) (intVal >>> 8);
            ba[i] = (byte) (intVal & 0xFF);
         }
      if (c.verify()) {
         int valA = (ba[alignedOffset + 3] << 24) | ((ba[alignedOffset + 2] & 0xff) << 16) | ((ba[alignedOffset + 1] & 0xff) << 8)
                  | (ba[alignedOffset] & 0xff);
         if (valA != intVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

   public void testByteArrayWriteConstBEInt(Context c) {

      for (int m = 0; m < arrayItersMultiplier*50; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i] = (byte) (0x12345678 >>> 24);
            ba[i + 1] = (byte) (0x12345678 >>> 16);
            ba[i + 2] = (byte) ((0x12345678 >>> 8) & 0xFF);
            ba[i + 3] = (byte) (0x12345678 & 0xFF);
         }
      if (c.verify()) {
         int valA = (ba[alignedOffset] << 24) | ((ba[alignedOffset + 1] & 0xff) << 16) | ((ba[alignedOffset + 2] & 0xff) << 8)
                  | (ba[alignedOffset + 3] & 0xff);
         if (valA != 0x12345678) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

   public void testByteArrayWriteConstLEInt(Context c) {

      for (int m = 0; m < arrayItersMultiplier*50; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i + 3] = (byte) (0x12345678 >>> 24);
            ba[i + 2] = (byte) ((0x12345678 >> 16) & 0xff);
            ba[i + 1] = (byte) (0x12345678 >>> 8);
            ba[i] = (byte) (0x12345678 & 0xFF);
         }
      if (c.verify()) {
         int valA = (ba[alignedOffset + 3] << 24) | ((ba[alignedOffset + 2] & 0xff) << 16) | ((ba[alignedOffset + 1] & 0xff) << 8)
                  | (ba[alignedOffset] & 0xff);
         if (valA != 0x12345678) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

   public void testByteArrayWriteConstLELong(Context c) {

      for (int m = 0; m < arrayItersMultiplier*25; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i + 23] = (byte) (0x0034567890ABCDEFL >>> 56);
            ba[i + 22] = (byte) (0x0034567890ABCDEFL >>> 48);
            ba[i + 20] = (byte) (0x0034567890ABCDEFL >>> 32);
            ba[i + 21] = (byte) (0x0034567890ABCDEFL >>> 40);
            ba[i + 19] = (byte) (0x0034567890ABCDEFL >>> 24);
            ba[i + 18] = (byte) ((0x0034567890ABCDEFL >> 16) & 0xff);
            ba[i + 17] = (byte) (0x0034567890ABCDEFL >>> 8);
            ba[i + 16] = (byte) (0x0034567890ABCDEFL & 0xFF);
         }
      if (c.verify()) {
         long valC = (((long) ba[alignedOffset + 23]) << 56) | ((((long) ba[alignedOffset + 22]) & 0xff) << 48)
                  | ((((long) ba[alignedOffset + 21]) & 0xff) << 40) | ((((long) ba[alignedOffset + 20]) & 0xff) << 32)
                  | ((((long) ba[alignedOffset + 19]) & 0xff) << 24) | ((((long) ba[alignedOffset + 18]) & 0xff) << 16)
                  | ((((long) ba[alignedOffset + 17]) & 0xff) << 8) | (((long) ba[alignedOffset + 16]) & 0xff);
         if (valC != 0x0034567890ABCDEFL) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value C: " + valC);
         }
      }
   }

   public void testByteArrayWriteConstLETripleLong(Context c) {

      for (int m = 0; m < arrayItersMultiplier*25; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i + 7] = (byte) (0x1234567890ABCDEFL >>> 56);
            ba[i + 6] = (byte) (0x1234567890ABCDEFL >>> 48);
            ba[i + 5] = (byte) (0x1234567890ABCDEFL >>> 40);
            ba[i + 4] = (byte) (0x1234567890ABCDEFL >>> 32);
            ba[i + 3] = (byte) (0x1234567890ABCDEFL >>> 24);
            ba[i + 2] = (byte) ((0x1234567890ABCDEFL >> 16) & 0xff);
            ba[i + 1] = (byte) (0x1234567890ABCDEFL >>> 8);
            ba[i] = (byte) (0x1234567890ABCDEFL & 0xFF);
            ba[i + 15] = (byte) (0x2334567890ABCDEFL >>> 56);
            ba[i + 14] = (byte) (0x2334567890ABCDEFL >>> 48);
            ba[i + 12] = (byte) (0x2334567890ABCDEFL >>> 32);
            ba[i + 13] = (byte) (0x2334567890ABCDEFL >>> 40);
            ba[i + 11] = (byte) (0x2334567890ABCDEFL >>> 24);
            ba[i + 10] = (byte) ((0x2334567890ABCDEFL >> 16) & 0xff);
            ba[i + 9] = (byte) (0x2334567890ABCDEFL >>> 8);
            ba[i + 8] = (byte) (0x2334567890ABCDEFL & 0xFF);
            ba[i + 23] = (byte) (0x0034567890ABCDEFL >>> 56);
            ba[i + 22] = (byte) (0x0034567890ABCDEFL >>> 48);
            ba[i + 20] = (byte) (0x0034567890ABCDEFL >>> 32);
            ba[i + 21] = (byte) (0x0034567890ABCDEFL >>> 40);
            ba[i + 19] = (byte) (0x0034567890ABCDEFL >>> 24);
            ba[i + 18] = (byte) ((0x0034567890ABCDEFL >> 16) & 0xff);
            ba[i + 17] = (byte) (0x0034567890ABCDEFL >>> 8);
            ba[i + 16] = (byte) (0x0034567890ABCDEFL & 0xFF);
         }
      if (c.verify()) {
         long valA = (((long) ba[alignedOffset + 7]) << 56) | ((((long) ba[alignedOffset + 6]) & 0xff) << 48)
                  | ((((long) ba[alignedOffset + 5]) & 0xff) << 40) | ((((long) ba[alignedOffset + 4]) & 0xff) << 32)
                  | ((((long) ba[alignedOffset + 3]) & 0xff) << 24) | ((((long) ba[alignedOffset + 2]) & 0xff) << 16)
                  | ((((long) ba[alignedOffset + 1]) & 0xff) << 8) | (((long) ba[alignedOffset]) & 0xff);
         long valB = (((long) ba[alignedOffset + 15]) << 56) | ((((long) ba[alignedOffset + 14]) & 0xff) << 48)
                  | ((((long) ba[alignedOffset + 13]) & 0xff) << 40) | ((((long) ba[alignedOffset + 12]) & 0xff) << 32)
                  | ((((long) ba[alignedOffset + 11]) & 0xff) << 24) | ((((long) ba[alignedOffset + 10]) & 0xff) << 16)
                  | ((((long) ba[alignedOffset + 9]) & 0xff) << 8) | (((long) ba[alignedOffset + 8]) & 0xff);
         long valC = (((long) ba[alignedOffset + 23]) << 56) | ((((long) ba[alignedOffset + 22]) & 0xff) << 48)
                  | ((((long) ba[alignedOffset + 21]) & 0xff) << 40) | ((((long) ba[alignedOffset + 20]) & 0xff) << 32)
                  | ((((long) ba[alignedOffset + 19]) & 0xff) << 24) | ((((long) ba[alignedOffset + 18]) & 0xff) << 16)
                  | ((((long) ba[alignedOffset + 17]) & 0xff) << 8) | (((long) ba[alignedOffset + 16]) & 0xff);
         if (valA != 0x1234567890ABCDEFL || valB != 0x2334567890ABCDEFL || valC != 0x0034567890ABCDEFL) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Values A: " + valA + " B: " + valB + " C: " + valC);
         }
      }
   }

   public void testByteMixedArrayWriteBEInt(Context c) {

      for (int m = 0; m < arrayItersMultiplier/4; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i + 1] = (byte) (intVal >>> 16);
            ba[i + 2] = (byte) (intVal >>> 8);
            ba[i] = (byte) (intVal >>> 24);
            ba[i + 3] = (byte) (intVal & 0xFF);
         }
      if (c.verify()) {
         int valA = (ba[alignedOffset] << 24) | ((ba[alignedOffset + 1] & 0xff) << 16) | ((ba[alignedOffset + 2] & 0xff) << 8)
                  | (ba[alignedOffset + 3] & 0xff);
         if (valA != intVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

   public void testByteMixedArrayWriteLEInt(Context c) {
      for (int m = 0; m < arrayItersMultiplier/4; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i + 3] = (byte) (intVal >>> 24);
            ba[i + 1] = (byte) (intVal >>> 8);
            ba[i + 2] = (byte) (intVal >>> 16);
            ba[i] = (byte) (intVal);
         }
      if (c.verify()) {
         int valA = (ba[alignedOffset + 3] << 24) | ((ba[alignedOffset + 2] & 0xff) << 16) | ((ba[alignedOffset + 1] & 0xff) << 8)
                  | (ba[alignedOffset] & 0xff);
         if (valA != intVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

   public void testByteArrayWriteBELong(Context c) {

      for (int m = 0; m < arrayItersMultiplier/8; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i] = (byte) (longVal >>> 56);
            ba[i + 1] = (byte) (longVal >>> 48);
            ba[i + 2] = (byte) (longVal >>> 40);
            ba[i + 3] = (byte) (longVal >>> 32);
            ba[i + 4] = (byte) (longVal >>> 24);
            ba[i + 5] = (byte) (longVal >>> 16);
            ba[i + 6] = (byte) (longVal >>> 8);
            ba[i + 7] = (byte) (longVal & 0xFF);
         }
      if (c.verify()) {
         long valA = (((long) ba[alignedOffset]) << 56) | ((((long) ba[alignedOffset + 1]) & 0xff) << 48)
                  | ((((long) ba[alignedOffset + 2]) & 0xff) << 40) | ((((long) ba[alignedOffset + 3]) & 0xff) << 32)
                  | ((((long) ba[alignedOffset + 4]) & 0xff) << 24) | ((((long) ba[alignedOffset + 5]) & 0xff) << 16)
                  | ((((long) ba[alignedOffset + 6]) & 0xff) << 8) | (((long) ba[alignedOffset + 7]) & 0xff);
         if (valA != longVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

   public void testByteArrayWriteLELong(Context c) {
      for (int m = 0; m < arrayItersMultiplier/8; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i + 7] = (byte) (longVal >>> 56);
            ba[i + 6] = (byte) (longVal >>> 48);
            ba[i + 5] = (byte) (longVal >>> 40);
            ba[i + 4] = (byte) (longVal >>> 32);
            ba[i + 3] = (byte) (longVal >>> 24);
            ba[i + 2] = (byte) (longVal >>> 16);
            ba[i + 1] = (byte) (longVal >>> 8);
            ba[i] = (byte) (longVal);
         }
      if (c.verify()) {
         long valA = (((long) ba[alignedOffset + 7]) << 56) | ((((long) ba[alignedOffset + 6]) & 0xff) << 48)
                  | ((((long) ba[alignedOffset + 5]) & 0xff) << 40) | ((((long) ba[alignedOffset + 4]) & 0xff) << 32)
                  | ((((long) ba[alignedOffset + 3]) & 0xff) << 24) | ((((long) ba[alignedOffset + 2]) & 0xff) << 16)
                  | ((((long) ba[alignedOffset + 1]) & 0xff) << 8) | (((long) ba[alignedOffset]) & 0xff);
         if (valA != longVal) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

   public void testByteArrayNoSequentialWriteBEInt(Context c) {
      for (int m = 0; m < arrayItersMultiplier/4; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i] = (byte) (intVal >>> 24);
            ba[i + 1] = (byte) (intVal >>> 8);
            ba[i + 2] = (byte) (intVal >>> 8);
            ba[i + 3] = (byte) (intVal & 0xFF);
         }
      if (c.verify()) {
         int valA = (ba[alignedOffset] << 24) | ((ba[alignedOffset + 1] & 0xff) << 16) | ((ba[alignedOffset + 2] & 0xff) << 8)
                  | (ba[alignedOffset + 3] & 0xff);

         if (valA != intValDup8) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

   public void testByteArrayNoSideEffectsWriteBEInt(Context c) {
      ba[alignedOffset] = (byte) 0;
      ba[alignedOffset + 1] = -1;
      ba[alignedOffset + 2] = -1;
      ba[alignedOffset + 3] = -1;
      for (int m = 0; m < arrayItersMultiplier/4; ++m)
         for (int iters = 0; iters < c.iterations(); ++iters) {
            int i = alignedOffset;
            ba[i] = (byte) (ba[i] >>> 24);
            ba[i + 1] = (byte) (ba[i] >>> 16);
            ba[i + 2] = (byte) (ba[i] >>> 8);
            ba[i + 3] = (byte) (ba[i] & 0xFF);
         }
      if (c.verify()) {
         int valA = (ba[alignedOffset] << 24) | ((ba[alignedOffset + 1] & 0xff) << 16) | ((ba[alignedOffset + 2] & 0xff) << 8)
                  | (ba[alignedOffset + 3] & 0xff);

         if (valA != 0) {
            c.printerr("Byte array does not have the correct value");
            c.printerr("Value: " + valA);
         }
      }
   }

}
