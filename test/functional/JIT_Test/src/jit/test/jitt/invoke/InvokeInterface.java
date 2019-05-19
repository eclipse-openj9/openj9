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
package jit.test.jitt.invoke;

import org.testng.annotations.Test;
import org.testng.Assert;

public class InvokeInterface extends jit.test.jitt.Test {

	static interface I {
		public int tstInterfaceMethod(int numToReturn);
	}
	
	static class X implements I {
		
		public int tstInterfaceMethod(int numToReturn) {
			return numToReturn;
		}	
	}

	static class Y implements I {
		public void foo1() {}

		public int tstInterfaceMethod(int numToReturn) {
			return -numToReturn;
		}	
	}
	
	static class Z implements I {
		public void foo1() {}
		public void foo2() {}
		public int tstInterfaceMethod(int numToReturn) {
			return ~numToReturn;
		}	
	}

	public int tstSendingInterfaceMethod(boolean run, int numToReturn, I interf) {
		if (run) {
			return interf.tstInterfaceMethod(numToReturn);
		} else {
			return 0;
		}
	}

	@Test(groups = { "level.sanity","component.jit" })
	public void testInvokeInterface() {
		for (int j = 0; j <= sJitThreshold ; j++) {
			tstSendingInterfaceMethod(false, 0, null);
		}
		
		/* exercise the PIC by using three different 
		 * implementers, and invoking each one twice 
		 * */
		if(5 != tstSendingInterfaceMethod(true, 5, new X())) {
			Assert.fail("did not get back what was passed (send #1)");
		}
		if(-5 != tstSendingInterfaceMethod(true, 5, new Y())) {
			Assert.fail("did not get back what was passed (send #2)");
		}
		if(~5 != tstSendingInterfaceMethod(true, 5, new Z())) {
			Assert.fail("did not get back what was passed (send #3)");
		}
		if(5 != tstSendingInterfaceMethod(true, 5, new X())) {
			Assert.fail("did not get back what was passed (send #4)");
		}
		if(-5 != tstSendingInterfaceMethod(true, 5, new Y())) {
			Assert.fail("did not get back what was passed (send #5)");
		}
		if(~5 != tstSendingInterfaceMethod(true, 5, new Z())) {
			Assert.fail("did not get back what was passed (send #6)");
		}
	}
}
