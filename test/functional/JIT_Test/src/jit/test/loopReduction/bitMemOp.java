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

public class bitMemOp extends base {

   public int test(Context c, int len) {

      long start, end, elapsed;
      int j;
      byte array1[] = new byte [len];
      byte array2[] = new byte [len];

      // Initialize the array 
      for (j = 0; j < len; j++)  {
         array1[j] = (byte) 0xf0;
         array2[j] = (byte) 0x1f;
      }

      start = System.currentTimeMillis();
      for (int iters = 0; iters < c.iterations()*50 + 1; iters++){
         reductionTest(array1, array2);
      }
      end = System.currentTimeMillis();

      if (c.verify()) 
         for (int i = 0; i < len; i++) {
            if (array1[i] != (byte) 0xef) {
               c.printerr(this.getClass().getName() + " : array1[" + i + "] got "+ array1[i] + " but expected: 0xef");
               return -1;
            }
         }

      elapsed = end - start;
      if (c.timed())
         c.println(this.getClass().getName() + ": len="+ len + ", test took " + elapsed + " millis");
      return 0;
   }

   public int reductionTest (byte[]a, byte[]b) {
      int i = 0;
      int j = 0;
      if (a.length == b.length) {
         while (i < b.length) {
            a[i] = (byte) ((int) a[i] ^ (int) b[j]);
            i++;
            j++;
         }
      }
      return i;
   }
}
