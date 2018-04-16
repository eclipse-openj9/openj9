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
package com.ibm.jvmti.tests.forceEarlyReturn;

public class fer002 
{

    static final int VALUE_INT_DEFAULT = 100;
    static final int VALUE_INT_FER = 4096;
    static final String VALUE_OBJ_DEFAULT = "default";
    static final String VALUE_OBJ_FER = "fer";
   
    
    static final int FER_INT    = 1;
    static final int FER_OBJECT = 2;
	
	native static boolean forceEarlyReturnInt(int retVal, Thread t, Class c);
	native static boolean forceEarlyReturnObject(Object retVal, Thread t, Class c);
    
	public boolean testReturnIntMethodExit() 
	{
		MethodExitTestThread t = new MethodExitTestThread(FER_INT);
		
		t.syncStart();
		
		forceEarlyReturnInt(VALUE_INT_FER, t, t.getClass());
		
		try {  t.join(); } catch (InterruptedException e) {}
		
		if (t.retInt != VALUE_INT_FER) {
			System.out.println("Expected " + VALUE_INT_FER + " got " + t.retInt);
			return false;
		}
					
		return true;
	}

	public String helpReturnIntMethodExit()
	{
		return "test correct return value passed back via the method exit event";
	}


	public boolean testReturnObjectMethodExit() 
	{
		MethodExitTestThread t = new MethodExitTestThread(FER_OBJECT);
		
		t.syncStart();
		
		forceEarlyReturnObject(VALUE_OBJ_FER, t, t.getClass());
		
		try {  t.join(); } catch (InterruptedException e) {}
		
		if (t.retString != VALUE_OBJ_FER) {
			System.out.println("Expected " + VALUE_OBJ_FER + " got " + t.retString);
			return false;
		}
					
		return true;
	}

	public String helpReturnObjectMethodExit()
	{
		return "test correct return value passed back via the method exit event";
	}



	
}
