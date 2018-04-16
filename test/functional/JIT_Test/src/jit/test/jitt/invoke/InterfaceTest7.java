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
// Testing extended interfaces

package jit.test.jitt.invoke;

import org.testng.annotations.Test;
import org.testng.Assert;

public class InterfaceTest7 extends jit.test.jitt.Test {

   @Test(groups = { "level.sanity","component.jit" })
   public void testInterfaceTest7() {
		
	int result = 0;

	result = tstTest1();
 	if (result != 46)
        	Assert.fail("InterfaceTest7-> Incorrect result value for test #1"); 
        
	result = tstTest2();
	if (result != 52)
        	Assert.fail("InterfaceTest7-> Incorrect result value for test #1");       	      	
        	      	      	
   }   

   public int tstTest1() {
   	
   	int value = 0;
   	
	Interface7 var_7 = new InterfaceTest7_First();
	Interface7A var_7A = new InterfaceTest7A();
	Interface7B var_7B = new InterfaceTest7B();
	Interface7C var_7C = new InterfaceTest7C();

	value = value + var_7.tstMethodA() + var_7.tstMethodB();
	value = value + var_7A.tstMethodA() + var_7A.tstMethodB() + var_7A.tstMethodC();
	value = value + var_7B.tstMethodA() + var_7B.tstMethodB() + var_7B.tstMethodD();
	value = value + var_7C.tstMethodA() + var_7C.tstMethodB() + var_7C.tstMethodD();

	return value;
	
   }
  
   public int tstTest2() {
   	
   	int i = 0;
   	int value = 0;
   		
	// Testing Interface polymorphism
	Interface7[] Interface7_Array = new Interface7[4];
	Interface7_Array[0] = new InterfaceTest7_First();
	Interface7_Array[1] = new InterfaceTest7A();
	Interface7_Array[2] = new InterfaceTest7B();
	Interface7_Array[3] = new InterfaceTest7C();			

	for(i=0; i<4; i++)
	{
		value = value + Interface7_Array[i].tstMethodA() + Interface7_Array[i].tstMethodB();
	}
	
	return value;
   }  
   
}
