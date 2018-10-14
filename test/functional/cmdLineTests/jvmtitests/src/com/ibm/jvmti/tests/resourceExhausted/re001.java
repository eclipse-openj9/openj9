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
package com.ibm.jvmti.tests.resourceExhausted;

import java.util.Vector;


public class re001 
{
	public static native boolean hasBeenCalledBack();
	
	public boolean testOutOfMemoryString()
	{
		String str = "      ";
		boolean ret = false;
		
		/* Clear the flag */
		hasBeenCalledBack();
		
		try {
			while (true) {
				str = str + str;							
			}
		} 
		catch (OutOfMemoryError ex)
		{
			/* System.out.println("ex: " + ex.getMessage()); */
			
			/* Check if the callback was issued prior to the exception */
			ret = hasBeenCalledBack();
		}
		
		return ret;
	}
	
	public String helpOutOfMemoryString()
	{
		return "Tests whether the VM calls back with a Resource Exhausted event (with JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR flag set)";
	}
	
	
	
	
	public boolean testOutOfMemoryObject()
	{
		Vector objs = new Vector();
		boolean ret = false;
		
		/* Clear the flag */
		hasBeenCalledBack();
		
		try {
			while (true) {
				objs.add(new long[1024]);			
			}
		} 
		catch (OutOfMemoryError ex)
		{
			/* System.out.println("ex: " + ex.getMessage()); */
			
			/* Check if the callback was issued prior to the exception */
			ret = hasBeenCalledBack();
		}
		
		return ret;
	}
	
	public String helpOutOfMemoryObject()
	{
		return "Tests whether the VM calls back with a Resource Exhausted event (with JVMTI_RESOURCE_EXHAUSTED_OOM_ERROR flag set)";
	}

	
}
