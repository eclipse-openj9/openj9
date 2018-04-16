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

public abstract class base {
   private static int ret;
   public static final int MINIMUM_DEFAULT_ARRAY_LENGTH = 2049;

   /**
    * Runs the test routine with the given context for the user specified maximum 
    * length.  The tested lengths start at 0, and is incremented by one until either
    * the threshold value or maximum length are met.  Once length exceeds the threshold
    * parameter length are exponentially scaled by 2x. 
    *
    * @param context    the loop reduction Context
    * @param threshold  the threshold length value after which tested lengths are 
    *                   exponentially scaled.
    * @param maxLength  the maximum length to test.
    */
   public void runTest(Context context, int threshold, int maxLength) {

      for (int len = 0; len <= maxLength; ){
         ret = test(context, len);

         if (len >= threshold) 
            len *= 2;
         else 
            len++;
      }

   }

   /**
    * Runs the test routine with the given context with tested length starting at 0,
    * incrementally until length of 16, then exponentially scaled by 2x until 2048.
    *
    * @param context    the loop reduction Context
    */   
   public void runTest(Context context) { 
      runTest(context, 16, MINIMUM_DEFAULT_ARRAY_LENGTH-1); 
   }

   /**
    * abstract method to launch the scenario under test.
    *
    * @param context    the loop reduction Context
    * @param length     the length to be tested.
    */  
   public abstract int test(Context context, int length);
}
