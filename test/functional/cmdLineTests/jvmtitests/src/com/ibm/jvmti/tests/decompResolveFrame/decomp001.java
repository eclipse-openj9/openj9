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

public class decomp001 
{
	static public native boolean checkFrame();

	private boolean triggerDecompile(String onClass, String onMethod)
	{
		boolean ret = false;
		ResolveFrameClassloader loader = new ResolveFrameClassloader(onClass);
		
		Class decompClass;
		
		try {
			decompClass = Class.forName("com.ibm.jvmti.tests.decompResolveFrame.ResolveFrameMain", false, loader);
		} catch (ClassNotFoundException e) {
			e.printStackTrace();
			return false;
		}
		
				
		java.lang.reflect.Method main;
		try {
			main = decompClass.getMethod(onMethod, new Class[] {});
			ret = (Boolean) main.invoke(null, new Object[] {});
		} catch (Exception e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		return ret;
	}
	
	
	/* Use a custom classloader (ResolveFrameClassloader) to trigger decompilation 
	 * while we have a specific resolve frame on the stack. The classloader loadClass()
	 * code will wait until we resolve try to resolve a method in the specified class 
	 * and force a decompilation via single stepping */
	
	public boolean testInterfaceMethodResolveFrame()
	{		
		boolean ret = triggerDecompile("com.ibm.jvmti.tests.decompResolveFrame.ResolveFrame_TestInterfaceMethod",
									   "resolveFrame_testInterfaceMethod");
		
		return ret;
	}	
	public String helpInterfaceMethodResolveFrame()
	{
		return "decompile a interface resolve frame making sure that the passed in arguments are maintained"; 
	}

	
	
	public boolean testMethodResolveFrame()
	{		
		boolean ret = triggerDecompile("com.ibm.jvmti.tests.decompResolveFrame.ResolveFrame_TestMethod", 
									   "resolveFrame_testMethod");
		
		return ret;
	}
	public String helpMethodResolveFrame()
	{
		return "decompile a regular method resolve frame making sure that the passed in arguments are maintained"; 
	}
	
	
	
	
}
