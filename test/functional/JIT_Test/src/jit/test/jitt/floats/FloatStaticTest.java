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
// Test file: FloatStaticTest.java
// Testing the assignment of static floats and static volatile floats

package jit.test.jitt.floats;

import org.testng.annotations.Test;
import org.testng.Assert;

@Test(groups = { "level.sanity","component.jit" })
public class FloatStaticTest extends jit.test.jitt.Test {

   @Test
   public void testFloatStaticTest() {
   	
        tstFStaticVolatile1(); 

        tstFStatic1();

        tstFStaticVolatile2(); 	
        if (FloatStaticTestHelperVolatile.value != FloatStaticTestHelperVolatile.properValue)
               Assert.fail("FloatStaticTest->tstFStaticVolatile: test case #3 failed!");
 
	tstFStatic2();
        if (FloatStaticTestHelper.value != FloatStaticTestHelper.properValue)
               Assert.fail("FloatStaticTest->tstFStaticVolatile: test case #4 failed!");


   }
	
	private void tstFStaticVolatile1() {
                FloatStaticTestHelperVolatile.value = FloatStaticTestHelperVolatile.properValue;
                if (FloatStaticTestHelperVolatile.value != FloatStaticTestHelperVolatile.properValue)
                        Assert.fail("FloatStaticTest->tstFStaticVolatile: test case #1 failed!");
        }
        
        private void tstFStatic1() {
        	FloatStaticTestHelper.value = FloatStaticTestHelper.properValue;
                if (FloatStaticTestHelper.value != FloatStaticTestHelper.properValue)
                        Assert.fail("FloatStaticTest->tstFStaticVolatile: test case #2 failed!");

        }

	private void tstFStaticVolatile2() {
		FloatStaticTestHelperVolatile.properValue=-3.3f;
                FloatStaticTestHelperVolatile.value = FloatStaticTestHelperVolatile.properValue;
       }	

       private void tstFStatic2() {
       		FloatStaticTestHelper.properValue = 9.9f;
        	FloatStaticTestHelper.value = FloatStaticTestHelper.properValue;
       }	

}
