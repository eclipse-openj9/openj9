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
package com.ibm.jvmti.tests.getJ9vmThread;

public class gjvmt001 
{
	/**
	 * Test the extended function jvmtiGetJ9vmThread()
	 * 
	 *  
	 * @return true on pass
	 */
	
	public static native boolean validateGJVMT001();
	
	
	public boolean testGetJ9vmThread() 
	{
		boolean rc = true;
		
		if (!validateGJVMT001()) {
			System.err.println("extension getJ9vmThread failed to return valid J9VMThread");
			rc = false;
		}
		
		return rc;
	}
	
	public String helpGetJ9vmThread()
	{
		return "Check getJ9vmThread returns valid J9VMThread";		
	}
	
}
