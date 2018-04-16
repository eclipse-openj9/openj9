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
// Testing implemented interface methods and non-interface methods. 

package jit.test.jitt.invoke;

import org.testng.annotations.Test;
import org.testng.Assert;

public class InterfaceTest4 extends jit.test.jitt.Test {

   @Test(groups = { "level.sanity","component.jit" })
   public void testInterfaceTest4() {
   	
   	int result = 0;
   	
 	result = tstTest1();
	if (result != 24)
        	Assert.fail("InterfaceTest4-> Incorrect result value");
	  	
   }

   public int tstTest1() {
		
	int result = 0;
	int i = 0;
	
	Interface4[] Interface4_Array = new Interface4[2];
	Interface4_Array[0] = new InterfaceTest4A();
	Interface4_Array[1] = new InterfaceTest4B();
	
	for(i=0; i < Interface4_Array.length; i++)
	{
		result = result + Interface4_Array[i].tstMethodA() + Interface4_Array[i].tstMethodB() /* + Interface4_Array[i].tstNonInterfaceMethod1() + Interface4_Array[i].tstNonInterfaceMethod2() */  ;		
	}
	
	return result;	
   }   
   
}
