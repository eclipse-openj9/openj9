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

public class MEMCPYChar2Byte2 extends base {
   static char array1[] = new char [10000];
   static byte array2[] = new byte [10000];

   public int test(Context c, int len) {
      int j = -1, len2 = len*2;
      long start, end, elapsed;
      start = System.currentTimeMillis();
      for (int iters = 0; iters < c.iterations()*50; iters++){
         j = 0;
         for (int i = 0; i < len; ){
            if (j > len2) break;
            char T = array1[i++];
	    array2[j++] = (byte)(T >> 8);
	    array2[j++] = (byte)(T & 0xFF);
         }
      }
      end = System.currentTimeMillis();
      elapsed = end - start;
      c.println(this.getClass().getName() + ": len="+ len + ", test took " + elapsed + " millis");
      return j;
   }
}
