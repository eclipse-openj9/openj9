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
package com.ibm.jvmti.tests.getOwnedMonitorStackDepthInfo;

import com.ibm.jvmti.tests.util.SyncThread;

public class ThreadNDepth extends SyncThread 
{
	Object lock = new Object();
	
	Object monitorA = new Object();
	Object monitorB = new Object();
		
	private int maxRecursionDepth = 0;

	native static boolean verify(Thread t, int expectedMonitorCount, int expectedDepth, Object monitor);
	native static boolean verifyDeeper(Thread t, int expectedMonitorCount, int expectedDepthA, int expectedDepthB, Object monitorA, Object monitorB);
	
	public ThreadNDepth(int depth)
	{
		maxRecursionDepth = depth;
	}

	
	public boolean work()
	{	
		boolean ret;
		
		synchronized(monitorA) {
			if (maxRecursionDepth == 0) {				
				ret = verify(this, 1, 1, monitorA);
			} else {		
				ret = recurse(1);
			}
		}
		
		return ret;
	}
	
	private boolean recurse(int depth)
	{	
		if (maxRecursionDepth >= depth) {
			synchronized(monitorB) {				
				return verifyDeeper(this, 2, 1, 2, monitorA, monitorB);
			}
		}
		
		return recurse(++depth);
	}
	
	
}
