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

import java.util.HashMap;
import java.util.Iterator;
import java.util.Vector;


public class TestSuite 
{
	ErrorControl ec = new ErrorControl();
	
	HashMap tests;
	String testClassName;		
	Vector testCases;


	public TestSuite() throws Exception
	{	
		/* Check for errors logged while initializing the Agent */
		try {
			ec.checkErrors();
		} catch (AgentSoftException e) {
			
		}
		
		/* Query the agent for the name of the class containing our testcase code.
		 * The agent maps the short testcase name (ex: 'fer001') to the full class name */
		testClassName = getSelectedTestClassName();
		
		/* Add the testcase. We execute only one testcase (with its subtests) per run for now */ 
		testCases = new Vector();
		testCases.addElement(new TestCase(testClassName));
			
	}
		
	public boolean run(TestCase testCase) throws Exception
	{
		/* Check for errors logged while running each TestCase */
		try {
			ec.checkErrors();
		} catch (AgentSoftException e) {
			/* Soft exceptions are thrown only if there are no Hard exceptions. We can 
			 * safely return a passing rc here without actually ignoring any hard errors */
			return true;
		}
		
		return testCase.run();
	}
		
	public boolean run() throws Exception
	{
		boolean ret = false;
		int failureCount = 0;
		Iterator t = testCases.iterator();
		
		while (t.hasNext()) {
			TestCase testCase = (TestCase) t.next();
			ret = run(testCase);
			if (ret == false) {
				failureCount++;
			}
		}
		
		/* Check for errors logged while running the last TestCase */
		try {
			ec.checkErrors();
		} catch (AgentSoftException e) {			
			return true;
		}
		
		return (failureCount < 1);
	}
	
/*	
	public void printTotals()
	{
		if (testsFailed > 0) {
			System.out.println("----------------------------------------------------------");
			System.out.println("Failed testcases:\n");
			Iterator it = tests.keySet().iterator();
			while (it.hasNext()) {				
				Object key = (String) it.next();
				TestCase t = (TestCase) tests.get(key);
				if (t.getResult() == false) {
					System.out.println("\t" + t.getMethodName());
				}
			}
		}
		System.out.println("----------------------------------------------------------");
		System.out.println("total tests:  " + testsRun);
		System.out.println("total passed: " + testsPassed);
		System.out.println("total failed: " + testsFailed);
	}
*/	
	
	public static native int getTestCount();	
	public static native String getTestClassName(int testNumber);
	public static native String getSelectedTestClassName();
	
	
}
