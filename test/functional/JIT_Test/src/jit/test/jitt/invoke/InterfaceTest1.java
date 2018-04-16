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
// Testing the simplest interface test, with only one implementation of an interface

package jit.test.jitt.invoke;

import org.testng.annotations.Test;
import org.testng.Assert;

public class InterfaceTest1 extends jit.test.jitt.Test implements Interface1  {
	
   public int tstMethodA() { return 5;  }
   public int tstMethodB() { return -4; }
   
   @Test(groups = { "level.sanity","component.jit" })
   public void testInterfaceTest1() {
		
	int result = 0;
	
	result = tstMethodA();
	if (result != 5)
        	Assert.fail("InterfaceTest1-> Incorrect return value for test #1!");

	result = tstMethodB();
	if (result != -4)
        	Assert.fail("InterfaceTest1-> Incorrect return value for test #2!");
	

   }
}
