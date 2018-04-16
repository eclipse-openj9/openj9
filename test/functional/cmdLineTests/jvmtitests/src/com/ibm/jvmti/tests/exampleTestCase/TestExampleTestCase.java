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
package com.ibm.jvmti.tests.exampleTestCase;


/*  READ THE README FOR COMPLETE INSTRUCTIONS ON HOW TO ADD TESTS */

public class TestExampleTestCase 
{
	/** A test case method. The method name must start with 'test'  
	 *  
	 * @return  return true if test case passes
	 */
	public boolean testCase1()
	{
		return true;
	}
	/** A test case help method. The method name must start with 'help' and the remainder
	 *  of the name must match the postfix of the test case method (ie ie whatever comes
	 *  after the 'test' prefix. ex:  testCase1 -> helpCase1)
	 *   
	 * @return a String containing test case description
	 */
	public String helpCase1()
	{
		return "Mandatory help/description for testCase1";
	}
	
	
	public boolean testCase2()
	{
		return true;
	}
	public String helpCase2()
	{
		return "Mandatory help/description for testCase2";
	}
		
	public boolean testCase3()
	{
		return false;
	}
	public String helpCase3()
	{
		return "Mandatory help/description for testCase3";
	}
	
	
	/** An optional setup method. Executed only once and prior to running test cases in this class. */
	public boolean setup(String args)
	{
		return true;
	}
	
	/** An optional teardown method. Executed after all test cases in this class have been ran */
	public boolean teardown()
	{
		return true;
	}
	
}
