/*******************************************************************************
 * Copyright (c) 2000, 2018 IBM Corp. and others
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

public class writeUTF extends base {
   static byte array2[] = new byte [base.MINIMUM_DEFAULT_ARRAY_LENGTH];
   static int startOffset0 = 0, startOffset1 = 1;

   public int test(Context c, int len) {
      int i = -1;
      int j = -1;
      long start, end, elapsed;
      for (j = 0; j < len; j++) array2[j] = 0x20;
      String str = new String(array2, 0, len);
      start = System.currentTimeMillis();
      for (int iters = 0; iters < c.iterations()*50; iters++){
         i = startOffset1;
         for (j = startOffset0; j < len; ){
            char T = str.charAt(j);
            if (1 > T || T > 177) break;
            array2[i++] = (byte)T;
            j++;
         }
      }
      end = System.currentTimeMillis();
      if (c.verify()) {
         if (startOffset0 >= len) {
            if (j != startOffset0)
               c.printerr(this.getClass().getName() + " j: got: " + j + " expected: " + startOffset0);

            if (i != startOffset1)
               c.printerr(this.getClass().getName() + " i: got: " + i + " expected: " + startOffset1);

         } else {
            if (j != len)
               c.printerr(this.getClass().getName() + " j: got: " + j + " expected: " + len);

            if (i != len + 1)
               c.printerr(this.getClass().getName() + " i: got: " + i + " expected: " + (len + 1));
         }
      }
      elapsed = end - start;
      c.println(this.getClass().getName() + ": len="+ len + ", test took " + elapsed + " millis");
      return j;
   }
}
