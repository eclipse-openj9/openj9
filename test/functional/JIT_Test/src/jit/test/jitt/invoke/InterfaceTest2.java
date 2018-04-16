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
// Testing multiple implementations of InterfaceTest2. 

package jit.test.jitt.invoke;

import org.testng.annotations.Test;
import org.testng.Assert;

public class InterfaceTest2 extends jit.test.jitt.Test implements Interface2  {
	
   public int tstMethodA() { return 4;  }
   public int tstMethodB() { return 3; }
   
   @Test(groups = { "level.sanity","component.jit" })
   public void testInterfaceTest2() {
		
	int result = 0;
	int i = 1;
	
	Interface2 A = new InterfaceTest2A();
	Interface2 B = new InterfaceTest2B();
	Interface2 C = new InterfaceTest2C();

	for(i=1; i < 9; i++) {	 	
		result = result + tstCall(i);
	}
	
	if (result != 52)
        	Assert.fail("InterfaceTest2-> Incorrect return value!");

   }   

   public int tstCall(int arg1) {
	
	int value=0;
	Interface2 A = new InterfaceTest2A();
	Interface2 B = new InterfaceTest2B();
	Interface2 C = new InterfaceTest2C();
	
	switch(arg1) {
	
	case 1:
		value = tstMethodA();
		break;	
	case 2:
		value = tstMethodB();
		break;
	case 3: 
		value = A.tstMethodA();
		break;
	case 4: 
		value = A.tstMethodB();
		break;	
    	case 5: 
    		value = B.tstMethodA();
    		break;
	case 6:
        	value = B.tstMethodB();
		break;     
	case 7:	
		value = C.tstMethodA();
        	break;
        case 8:		
		value = C.tstMethodB();
		break;
	}
   	
   	return value;
   	
   }

   
}
