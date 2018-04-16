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

public class Error extends Throwable
{
	int error;
	String errorName;
	String file;
	String function;
	int	line;
	int type;
	String message;
	
	public final static int JVMTI_TEST_ERR_TYPE_SOFT = 1;
	public final static int JVMTI_TEST_ERR_TYPE_HARD = 2;
	
	/**
	 * \brief Retrieve Error details
	 * @param errorIndex	index of the error. Index of 0 indicates the first error
	 * @return true on successful error retrieval or false in case the index does not indicate a valid error
	 * 
	 * Initialize the error detail fields of this <code>Error</code> instance. 
	 */
	public native boolean getErrorDetails(int errorIndex);
		
	public String getMessage()
	{
		String msg = "";
			
		msg += "\tError: " + error + "  " + errorName + "\n";		
		msg += "\t\t" + message + "\n";
		msg += "\t\tLocation: " + file + " -> [" + function + "():" + line + "]\n";
				
		return msg;
	}
	
	public boolean isHard()
	{
		return (type == JVMTI_TEST_ERR_TYPE_HARD);
	}
	
}
