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
package com.ibm.jvmti.tests.util;

public class ErrorControl 
{
	public int hardErrorCount;
	public int softErrorCount;
	
	
	public native boolean hasErrors();
	
	public native int getErrorCount();
	
	
	
	/**
	 * \brief Check the agent for any queued errors and throw an exception
	 * @throws AgentHardException
	 * 
	 * 	Checks the agent error management facility for any queued errors. 
	 *	Any errors and messages will be returned via an AgentException. 
	 */
	public void checkErrors() throws AgentHardException
	{
		hardErrorCount = 0;
		softErrorCount = 0;
		
		if (hasErrors()) {
			String errorMessages = "";
			int hardErrorCount = 0;
			
			int errorIndex = 0;
			while (true) {
				Error e = new Error();
				boolean ret = e.getErrorDetails(errorIndex);
				if (ret == false)
					break;
			
				if (e.isHard()) {
					hardErrorCount++;
				} else {
					softErrorCount++;
				}
				
				errorIndex++;
				
				errorMessages = "type: " + e.type + errorMessages + e.getMessage();
			}
			
			if (hardErrorCount > 0) {
				throw new AgentHardException("\n" + errorMessages);
			}
			
			if (softErrorCount > 0) {
				throw new AgentSoftException("\n" + errorMessages);				
			}
			
		}
		
		return;
	}
	
	
	
}
