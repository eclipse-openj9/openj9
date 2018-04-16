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
// Testing multiple implementations of InterfaceTest3. 

package jit.test.jitt.invoke;

import org.testng.annotations.Test;
import org.testng.Assert;

public class InterfaceTest3 extends jit.test.jitt.Test {

   @Test(groups = { "level.sanity","component.jit" })
   public void testInterfaceTest3() {
   	int result = 0;
   	
	result = tstTest1();
	if (result != 52)
        	Assert.fail("InterfaceTest3-> Incorrect result value");

   }
	   
   public int tstTest1() {
		
	int result = 0;
	int i = 0;
	
	Interface3[] Interface3_Array = new Interface3[4];
	Interface3_Array[0] = new InterfaceTest3A();
	Interface3_Array[1] = new InterfaceTest3B();
	Interface3_Array[2] = new InterfaceTest3C();
	Interface3_Array[3] = new InterfaceTest3D();
	
	for(i=0; i < Interface3_Array.length; i++)
	{
		result = result + Interface3_Array[i].tstMethodA() + Interface3_Array[i].tstMethodB();		
	}
	
	return result;

   }   
   
}
