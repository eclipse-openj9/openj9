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

abstract public class TestRunner 
{
	public static void main(String[] args)
    {	
		final String TESTID="testid=";
		TestSuite testSuite;
		boolean passed = false;
		boolean onAttach = false;
		String testid = null;
		attachingAgent a1 = null, a2 = null;
		
		
		if (args.length > 0) {
			onAttach = args[0].startsWith(TESTID);
		}

		if (onAttach) {
			
			/* Need to attach the agent through late attach */
			testid = args[0].substring(TESTID.length(), args[0].length());
			a1 = new attachingAgent(testid);
			a2 = new attachingAgent(testid);
			a1.start();
			a2.start();
			try {
				a1.join();
				a2.join();
			} catch(Exception e) {
				System.err.println("exception thrown: " + e.getMessage());
				e.printStackTrace();
			}
		} 
		
		try {
			/* Create a new TestSuite and run it */
			testSuite = new TestSuite();
			passed = testSuite.run();
		} catch (Exception e) {
			System.err.println("exception thrown: " + e.getMessage());
			e.printStackTrace();
		} 
		
		exit(passed);
	}
	
			
	public static void exit(boolean passed)
	{
		int retCode;
				
		if (passed == false) {
			retCode = 1;
		} else {
			retCode = 0;
		}
				
		System.exit(retCode);
	}
}
