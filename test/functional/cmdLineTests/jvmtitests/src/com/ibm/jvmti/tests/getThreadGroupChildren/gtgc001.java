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
package com.ibm.jvmti.tests.getThreadGroupChildren;

import com.ibm.jvmti.tests.util.AgentHardException;
import com.ibm.jvmti.tests.util.SyncThread;

public class gtgc001 
{
	native static boolean checkAssignment(ThreadGroup threadGroup, int expectedThreadCount, int expectedGroupCount, String groupName);
	
	public boolean testThreadGroupAssignment() throws Exception
	{
		boolean result;
		
		ThreadGroup groupT = new ThreadGroup("groupT");
        ThreadGroup group1 = new ThreadGroup(groupT, "group1");
        ThreadGroup group2 = new ThreadGroup(groupT, "group2");

        SyncThread threadList[] = new SyncThread[5];
        
        threadList[0] = new SyncThread(group1, "thread1 in group1");
        threadList[1] = new SyncThread(group1, "thread2 in group1");
        threadList[2] = new SyncThread(group2, "thread3 in group2");
        threadList[3] = new SyncThread(group2, "thread4 in group2");
        threadList[4] = new SyncThread(groupT, "thread5 in top group");
        
        threadList[0] = new SyncThread(group1, "thread1");
        for (int i = 0; i < threadList.length; i++) {
            threadList[i].start();
        }
        
        
        result = checkAssignment(groupT, 1, 2, new String("groupT"));
        if (result == false) {
        	return false;
        }
        
        result = checkAssignment(group1, 2, 0, new String("group1"));
        if (result == false) {
        	return false;
        }
        
        for (int i = 0; i < threadList.length; i++) {
            threadList[i].interrupt();
        }
        
        for (int i = 0; i < threadList.length; i++) {
        	try {
        		threadList[i].join();
        	} catch (InterruptedException ie) {
        		throw new AgentHardException("Interrupted when joining " + threadList[i], ie);
        	}
        }
        
        
        return true;
	}
	
	public String helpThreadGroupAssignment()
	{
		return "Test if children threads are assigned to the proper thread groups";
	}

}
