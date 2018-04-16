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
// Testing partial implementations of an interface, thus requiring an abstract class and
// inheritance of the abstract class to fully implement an interface 

package jit.test.jitt.invoke;

import org.testng.annotations.Test;
import org.testng.Assert;

public class InterfaceTest5 extends jit.test.jitt.Test {

   @Test(groups = { "level.sanity","component.jit" })
   public void testInterfaceTest5() {
   	int result = 0;
   	
   	result = tstTest1();
	if (result != 34)
        	Assert.fail("InterfaceTest5-> Incorrect result value");
   	
   }

   public int tstTest1() {
		
	int result_child = 0;
	
	Interface5 a5_child = new InterfaceTest5A_Child();
	Interface5 b5_child = new InterfaceTest5B_Child();
		
	result_child = a5_child.tstMethodA() + a5_child.tstMethodB() + b5_child.tstMethodA() + b5_child.tstMethodB();
	
	return result_child;

   }   
   
}
