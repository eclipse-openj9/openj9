/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
// Testing variables in Interfaces

package jit.test.jitt.invoke;

import org.testng.annotations.Test;
import org.testng.Assert;

public class InterfaceTest6 extends jit.test.jitt.Test {

   @Test(groups = { "level.sanity","component.jit" })
   public void testInterfaceTest6() {
   
	int int_result = 0;
	long long_result = 0;
	float float_result = 0;
	double double_result = 0;
   	
   	int_result = tstTest1();
	if (int_result != 0)
        	Assert.fail("InterfaceTest6-> Incorrect result value for test #1");
   	
   	long_result = tstTest2();
	if (long_result != 1L)
        	Assert.fail("InterfaceTest6-> Incorrect result value for test #2");

	float_result = tstTest3();
 	if (float_result != -9.9f)
        	Assert.fail("InterfaceTest6-> Incorrect result value for test #3"); 

	double_result = tstTest4();
	if (double_result != 24d)
        	Assert.fail("InterfaceTest6-> Incorrect result value for test #4");       	      	
		   	
   }

   public int tstTest1() {
	
	InterfaceTest6A a6 = new InterfaceTest6A();		
	return a6.tstNonInterfaceMethodA();

   }   
 
   public long tstTest2() {

	InterfaceTest6A a6 = new InterfaceTest6A();
	return a6.tstNonInterfaceMethodB();
   }
   
   public float tstTest3() {
   	
	InterfaceTest6A a6 = new InterfaceTest6A();
	return a6.tstNonInterfaceMethodC();
   }
   
   public double tstTest4() {

	InterfaceTest6A a6 = new InterfaceTest6A();
	return a6.tstNonInterfaceMethodD();
   }
   
}
