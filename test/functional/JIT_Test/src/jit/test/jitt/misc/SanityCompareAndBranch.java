/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
package jit.test.jitt.misc;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity", "component.jit"})
public class SanityCompareAndBranch extends jit.test.jitt.Test {

   /**
    * Following method when JIT compiled deterministically (Fix optimization level), contains a tree
    * that compares two VFT pointers, one of which is stored on temporary java stack slots while other
    * from the object. 
    * On 64-Bit platform with Compressed references, this will test the code path to generate instruction
    * for `if` tree where one child is 8 byte memory load while other child is 4 byte memory load.
    *
    * For more details see eclipse/omr#4763
    */
   private int testAddressCompare(Object obj1, Object obj2) {
      int retVal = 0;
      if (obj1.getClass() == java.lang.String.class) {
         String temp = (String)obj1;
         for (int i=0; i < temp.length(); i++) {
            if (temp.charAt(i) == 'i')
               retVal++;
         }
      }
      if (obj1.getClass() == obj2.getClass())
         retVal = 0;
      return retVal;
   }

   @Test
   public void testCompareAndBranch() {
      SanityCompareAndBranch x = new SanityCompareAndBranch();
      String a1 = "This is Demo String a1";
      String a2 = "This is Demo String a2";
      Assert.assertEquals(0, testAddressCompare(a1,a2));
      String a3 = "This is Demo String a3";
      String a4 = "This is Demo String a4";
      Assert.assertEquals(0, testAddressCompare(a3, a4));
   }
}
