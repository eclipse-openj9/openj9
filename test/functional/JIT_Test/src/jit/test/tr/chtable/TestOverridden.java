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
package jit.test.tr.chtable;
import org.testng.annotations.Test;
import org.testng.AssertJUnit;

@Test(groups = { "level.sanity","component.jit" })
public class TestOverridden
   {
   private class TestBaseClass
      {
      public int foo()
        {
        return 1;
        }
      }

   private class TestExtendingClass extends TestBaseClass
      {
      public int foo()
         {
         return 2;
         }
      }

   private int getValue(TestBaseClass avalue)
      {
      return avalue.foo();
      }

  @Test
  public void testOverriddenDisableCHOpts() 
     {
      TestBaseClass a = new TestBaseClass();
      int v1 = getValue(a);

      TestExtendingClass b = new TestExtendingClass();
      int v2 = getValue(b);

     // Assert if we called wrong implementation of foo()
     AssertJUnit.assertTrue("v2 Expected: 2, Got: " + v2, v2 == 2);
     }
   }


