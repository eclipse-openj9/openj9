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

public class longMemCpy extends base {
   static long array1[] = new long [10000];
   static long array2[] = new long [10000];

   public int test(Context c, int len) {
      int j = -1;
      long start, end, elapsed;
      start = System.currentTimeMillis();

     /* Test IV post increment from 0 to length */
      for (int iters = 0; iters < c.iterations()*50; iters++) {
         for (j = 0; j < len; j++){
            array1[j] = array2[j];
         }
      }
      if (c.verify())
         if (j != len)
            c.printerr("byteMemCpy (Case 1) got: " + j + " expected: " + len);

      /* Test IV post decrement from length-1 to 0. */
      for (int iters = 0; iters < c.iterations()*50; iters++) {
         for (j = len - 1; j >= 0; j--){
            array1[j] = array2[j];
         }
      }
      if (c.verify())
         if (j != -1)
            c.printerr("byteMemCpy (Case 2) got: " + j + " expected: -1");

      /* Test IV pre increment from -1 to len -1 */
      for (int iters = 0; iters < c.iterations()*50; iters++) {
         j = -1;
         while (j < len - 1) {
            j++;
            array1[j] = array2[j];
         }
      }
      
      if (c.verify())
         if (j != len - 1)
            c.printerr("byteMemCpy (Case 3) got: " + j + " expected: " + (len -1));

      /* Test IV pre decrement from len to 0 */
      for (int iters = 0; iters < c.iterations()*50; iters++) {
         j = len;
         while (j > 0) {
            j--;
            array1[j] = array2[j];
         }
      }

      if (c.verify())
         if (j != 0)
            c.printerr("byteMemCpy (Case 4) got: " + j + " expected: 0");

      end = System.currentTimeMillis();
      elapsed = end - start;
      c.println(this.getClass().getName() + ": len="+ len + ", test took " + elapsed + " millis");
      return j;
   }
}
