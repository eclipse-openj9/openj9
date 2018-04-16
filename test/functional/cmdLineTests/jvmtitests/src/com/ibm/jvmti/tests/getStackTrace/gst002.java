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
package com.ibm.jvmti.tests.getStackTrace;

public class gst002 {
	
	native static boolean check(Thread t);

	public boolean testGetUnstartedThreadStackTrace() 
	{
		Thread t = new Thread() { public void run() { } };
				
	    return check(t);
	}
	
	public String helpGetUnstartedThreadStackTrace()
	{
		return "Check return of an empty stack for a thread that has not yet been started.";		
	}
	

	public boolean testGetDeadThreadStackTrace() 
	{
		Thread t = new Thread() { public void run() { } };
		
		t.start();
		
		try {
			t.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		
		// We still have a reference in hand, the VM should have cleaned the vmthread at
		// this point tho. Test whether we properly handle cases where thread->threadObject
		// is NULL
		
	    return check(t);
	}
	
	public String helpGetDeadThreadStackTrace()
	{
		return "Check return of an empty stack for a dead thread.";		
	}
	

	
	
	
}
