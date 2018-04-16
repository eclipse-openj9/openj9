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
package com.ibm.jvmti.tests.getMethodAndClassNames;

public class gmcpn001 {
	
	native static boolean check(int type);
	
	final static private int type_single = 1;
	final static private int type_multiple = 2;
	final static private int type_invalid_single = 3;
	
	public boolean testSingleRamMethod()
	{		
		return check(type_single);
	}

	public String helpSingleRamMethod()
	{
		return "tests retrieval of class name, method name and package name for a single ram method pointer";
	}

	
	public boolean testMultipleRamMethod()
	{		
		return check(type_multiple);
	}

	public String helpMultipleRamMethod()
	{
		return "tests retrieval of class name, method name and package name for multiple ram method pointers";
	}

	
	public void singleMethod1()
	{
		
	}
	public void singleMethod2()
	{
		
	}
	public void singleMethod3()
	{
		
	}

}
