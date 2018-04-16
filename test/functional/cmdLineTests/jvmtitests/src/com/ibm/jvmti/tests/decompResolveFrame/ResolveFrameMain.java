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
package com.ibm.jvmti.tests.decompResolveFrame;

public class ResolveFrameMain 
{
    static Integer i1 = new Integer(1234);
    static double  d1 = 2863311530.0;
    static long    l1 = 1234567;

    static float   f1 = (float)   1.0000001;
    static float   f2 = (float)   2.0000002;
    static float   f3 = (float)   3.0000003;
    static float   f4 = (float)   4.0000005;
    static float   f5 = (float)   5.0000006;
    static float   f6 = (float)   6.0000007;
    static float   f7 = (float)   7.0000008;
    static float   f8 = (float)   8.0000009;
    static float   f9 = (float)   9.0000011;
    static float   f10 = (float) 10.0000012;
    static float   f11 = (float) 11.0000013;
    static float   f12 = (float) 12.0000014;
    static float   f13 = (float) 13.0000015; 
	
	static public boolean resolveFrame_testInterfaceMethod()
	{
		ResolveFrame_TestInterfaceMethod i = new ResolveFrame_TestInterfaceMethod();
		
		/* The ResolveFrameClassloader will butt in and force a decompile via single step when
		 * this method is about to be resolved. We want a JIT resolve frame on the stack */
		
		boolean ret = i.testMethod(d1, i1, l1, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13);
		
		return ret;
	}
	
	static public boolean resolveFrame_testMethod()
	{		
		ResolveFrame_TestMethod i = new ResolveFrame_TestMethod();
				
		boolean ret = i.testMethod(d1, i1, l1, f1, f2, f3, f4, f5, f6, f7, f8, f9, f10, f11, f12, f13);
		
		return ret;
	}
	
	
}
