/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
// Test file: IntStaticTest.java
// Testing the assignment of static integer and static volatile integers

package jit.test.jitt.math2;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class IntStaticTest extends jit.test.jitt.Test {

   @Test
   public void testIntStaticTest() {
   	
        tstIntStaticVolatile1(); 

        tstIntStatic1();
        
        tstIntStaticVolatile2(); 	
        if (IntStaticTestHelperVolatile.value != IntStaticTestHelperVolatile.properValue)
               Assert.fail("IntStaticTest->tstIntStaticVolatile: test case #3 failed!");
 
	tstIntStatic2();
        if (IntStaticTestHelper.value != IntStaticTestHelper.properValue)
               Assert.fail("IntStaticTest->tstIntStaticVolatile: test case #4 failed!");

   }
	
	public void tstIntStaticVolatile1() {
                IntStaticTestHelperVolatile.value = IntStaticTestHelperVolatile.properValue;
                if (IntStaticTestHelperVolatile.value != IntStaticTestHelperVolatile.properValue)
                        Assert.fail("IntStaticTest->tstIntStaticVolatile: test case #1 failed!");
        }
        
        public void tstIntStatic1() {
        	IntStaticTestHelper.value = IntStaticTestHelper.properValue;
                if (IntStaticTestHelper.value != IntStaticTestHelper.properValue)
                        Assert.fail("IntStaticTest->tstIntStaticVolatile: test case #2 failed!");

        }

	public void tstIntStaticVolatile2() {
		IntStaticTestHelperVolatile.properValue=-3;
                IntStaticTestHelperVolatile.value = IntStaticTestHelperVolatile.properValue;
       }	

       public void tstIntStatic2() {
       		IntStaticTestHelper.properValue = 9;
        	IntStaticTestHelper.value = IntStaticTestHelper.properValue;
       }	

}
