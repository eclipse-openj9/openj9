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
package com.ibm.jvmti.tests.redefineBreakpointCombo;

public class rbc001_testClass_V1 {
	static int iter = 0;
	public int meth1()
	{
		return 11;
	}
	
	public long meth2()
	{
		long methodID;
		rbc001 test = new rbc001();
		
		if (!test.callRedefineClass()) {
			return 0;
		}
		
		methodID = test.callGetMethodIDFromStack(2); //this frame
		test.callSetBreakpointInMethodID(methodID, 35); // return 22
		
		return 22;
	}
	public int meth3()
	{
		long methodID;
		rbc001 test = new rbc001();
		iter++;
		if (!test.callRedefineClass()) {
			return 0;
		}
		
		if (10 > iter) {
			if (33 != meth3()) {
				return 0;
			}
		} else {
			test.prepareNonEquivalentBytecodes();
			
			if (!test.callRedefineClass()) {
				return 0;
			}
			
			methodID = test.callGetMethodIDFromStack(2); //this frame
			test.callSetBreakpointInMethodID(methodID, 74); // 74 is bytecode for return 33
		}
		return 33;
	}
}
