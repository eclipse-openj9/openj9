/*[INCLUDE-IF]*/
package com.ibm.jit;

import java.math.BigDecimal;
import java.math.BigInteger;
import com.ibm.jit.BigDecimalExtension;
/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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

public class BigDecimalConverters extends BigDecimalExtension {

   public static BigDecimal signedPackedDecimalToBigDecimal(byte[] packedDecimal, int scale)
   {
      if (DFPFacilityAvailable() && DFPUseDFP() &&
         (packedDecimal.length <= 8 && -scale >= -398 && -scale < 369))
      {
         // fast conversion with hardware packed to DFP instruction
         long longPack = 0;
         if (packedDecimal.length == 8)
         {
            // explicitly unrolled loop to take advantage of sequential load
            longPack = ((long)(packedDecimal[0] & 0xFF) << 56)
                + ((long)(packedDecimal[1] & 0xFF) << 48)
                + ((long)(packedDecimal[2] & 0xFF) << 40)
                + ((long)(packedDecimal[3] & 0xFF) << 32)
                + ((long)(packedDecimal[4] & 0xFF) << 24)
                + ((long)(packedDecimal[5] & 0xFF) << 16)
                + ((long)(packedDecimal[6] & 0xFF) << 8)
                + ((long)(packedDecimal[7] & 0xFF));
         }
         else
         {
            // slow way to load value into dfp
            for(int i = 0; i < packedDecimal.length; ++i)
            {
               longPack = longPack << 8;
               longPack += ((long)(packedDecimal[i] & 0xFF));
            }
         }

         BigDecimal bd = createZeroBigDecimal();
         if (DFPConvertPackedToDFP(bd, longPack, 398 - scale, true))
         {
            // successfully converted to dfp
            setflags(bd, 0x00);
            return bd;
         }
      }
      return slowSignedPackedToBigDecimal(packedDecimal, scale);
   }

   private static BigDecimal slowSignedPackedToBigDecimal(byte[] packedDecimal, int scale)
   {
      String buffer = new String();
      // convert the packed decimal to string buffer
      for(int i = 0; i < packedDecimal.length - 1; ++i)
      {
         buffer += ((packedDecimal[i] & 0xF0) >> 4) + "" + (packedDecimal[i] & 0x0F); //$NON-NLS-1$
      }
      buffer += ((packedDecimal[packedDecimal.length - 1] & 0xF0) >> 4);

      // check to see if the packed decimal is negative
      byte sign = (byte)(packedDecimal[packedDecimal.length - 1] & 0x0F);
      if (sign == 0x0B || sign == 0x0D)
      {
         buffer = "-" + buffer; //$NON-NLS-1$
      }
      return new BigDecimal(new BigInteger(new String(buffer)), scale);
   }

   public static byte[] BigDecimalToSignedPackedDecimal(BigDecimal bd)
   {
      if (DFPFacilityAvailable() && ((getflags(bd) & 0x3) == 0x0))
      {
         long longPack = DFPConvertDFPToPacked(getlaside(bd), true);
         if (longPack != Long.MAX_VALUE)
         {
            // conversion from DFP to packed decimal was successful
            byte [] packedDecimal = new byte[8];
            packedDecimal[0] = (byte)((longPack >> 56) & 0xFF);
            packedDecimal[1] = (byte)((longPack >> 48) & 0xFF);
            packedDecimal[2] = (byte)((longPack >> 40) & 0xFF);
            packedDecimal[3] = (byte)((longPack >> 32) & 0xFF);
            packedDecimal[4] = (byte)((longPack >> 24) & 0xFF);
            packedDecimal[5] = (byte)((longPack >> 16) & 0xFF);
            packedDecimal[6] = (byte)((longPack >> 8) & 0xFF);
            packedDecimal[7] = (byte)((longPack) & 0xFF);
            return packedDecimal;
         }
      }

      // slow path to manually convert BigDecimal to packed decimal
      return slowBigDecimalToSignedPacked(bd);
   }

   private static byte [] slowBigDecimalToSignedPacked(BigDecimal bd)
   {
      // digits are right justified in the return byte array
      BigInteger value = bd.unscaledValue();
      char [] buffer = value.abs().toString().toCharArray();
      int size = buffer.length / 2 + 1;
      byte [] packedDecimal = new byte[size];

      int numDigitsLeft= buffer.length - 1;
      int endPosition = numDigitsLeft % 2;
      int index = size - 2;

      // take care of the sign nibble and the right most digit
      packedDecimal[size - 1] = (byte)((buffer[numDigitsLeft] - '0') << 4);
      packedDecimal[size - 1] |= ((value.signum() == -1) ? 0x0D : 0x0C);

      // compact 2 digits into each byte
      for(int i = numDigitsLeft - 1; i >= endPosition; i -= 2, --index)
      {
          packedDecimal[index] = (byte)(buffer[i] - '0');
          packedDecimal[index] |= (byte)((buffer[i - 1] - '0') << 4);
       }

       // if there's a left over digit, put it in the last byte
       if(endPosition > 0)
       {
          packedDecimal[index] = (byte)(buffer[0] - '0');
       }

       return packedDecimal;
    }
 }
