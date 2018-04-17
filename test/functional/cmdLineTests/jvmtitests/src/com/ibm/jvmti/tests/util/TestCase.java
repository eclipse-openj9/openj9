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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;

public class TestCase 
{
	HashMap<String, SubTest> tests;
	public String testClassName = null;
	public String testArguments = null;
	public String subTest = null;
	
	int testsRun = 0;
	int testsPassed = 0;
	int testsFailed = 0;
	
	
	boolean testRequiresSetup = false;
	boolean testRequiresTeardown = false;
	
	/** All test case methods must begin with this prefix */ 
	private static final String testMethodNamePrefix = "test";
	
	/** A test case must implement a help method. The help method
	 * name is prefixed with 'help' instead of 'test' */  
	private static final String testClassHelpMethodPrefix = "help";
	
	/** A test package might optionally implement a setup and teardown 
	 * method if needed */
	private static final String testClassSetupMethod = "setup";
	private static final String testClassTeardownMethod = "teardown";
	
	
	public static native String getSelectedTestArguments();
	public static native String getSpecificSubTestName();
	
	
	public TestCase(String className) throws SecurityException, NoSuchMethodException, ClassNotFoundException, AgentHardException
	{		
		testClassName = className;

		subTest = getSpecificSubTestName();

		detectSubTests();
		
		testArguments = getSelectedTestArguments();
	}
	
	/**
	 * Create a list of implemented tests as well as an optional setup, teardown and help method
	 * 
	 * @throws NoSuchMethodException 
	 * @throws SecurityException 
	 * @throws ClassNotFoundException 
	 * @throws AgentHardException 
	 * @throws Error 
	 */
	private void detectSubTests() throws SecurityException, NoSuchMethodException, ClassNotFoundException, AgentHardException
	{
		tests = new HashMap<String, SubTest>();

		Class testClass = Class.forName(testClassName);
		
	    Method[] theMethods = testClass.getDeclaredMethods();
	    for (int i = 0; i < theMethods.length; i++) {
	    	String methodName = theMethods[i].getName();
	    	
	    	//System.out.println("method: " + theMethods[i].toGenericString());
	    	
	    	if (methodName.startsWith(testMethodNamePrefix) &&
	    		(subTest.length() == 0 || methodName.equals(subTest))) {
	    		String helpMethodName = "";
	    		SubTest t = new SubTest(methodName);
	    		tests.put(methodName, t);
	    		
	    		try {
	    			
	    			if (methodName.startsWith(testMethodNamePrefix, 0)) {
	    				String fragment = methodName.substring(testMethodNamePrefix.length(), methodName.length());
	    				helpMethodName = testClassHelpMethodPrefix + fragment;
	    				testClass.getMethod(helpMethodName, (Class[]) null);	    		
		    			t.setHelpMethodName(helpMethodName);
	    			}
	    			
	    			// dont use replaceFirst since its not implemented by the foundation jcl and we 
	    			// still want to be able to run our TI testing against ME jcls
	    			//String helpMethodName = methodName.replaceFirst(testMethodNamePrefix, testClassHelpMethodPrefix);
	    			
	    		} catch (NoSuchMethodException ex) {
	    			throw new AgentHardException("Test " + testClass.getName() + " must implement a " + helpMethodName + "() method");
	    		}
	    		
	    	}

	    	/* See if the test defines a setup method */
	    	if (methodName.compareTo(testClassSetupMethod) == 0) {
	    		testRequiresSetup = true;
	    	}
	    	
	    	/* See if the test defines a teardown method */
	    	if (methodName.compareTo(testClassTeardownMethod) == 0) {
	    		testRequiresTeardown = true;
	    	}	    		        		    	
	    }
	    	    	    
		return;
	}
	


	private Object invokeMethod(Object o, String methodName) throws SecurityException, NoSuchMethodException, IllegalArgumentException, IllegalAccessException, InvocationTargetException
	{
		Class c = o.getClass();
		Method method;
		Boolean r = java.lang.Boolean.FALSE;
		
		method = c.getMethod(methodName, (Class[]) null);				
		r = (Boolean) method.invoke(o, new Object[] {});
		
		return r;		
	}

	
	private boolean runTestSetup(Object o) throws SecurityException, IllegalArgumentException, NoSuchMethodException, IllegalAccessException, InvocationTargetException
	{
		Boolean ret;
		Method method;
		Class c = o.getClass();		
		
		//System.out.println("runTestSetup.. args="+testArguments);
		
		method = c.getMethod(testClassSetupMethod, new Class[] {String.class});				
		ret = (Boolean) method.invoke(o, new Object[] {testArguments});
		
		return ret.booleanValue();
	}
	
	private boolean runTestTeardown(Object o) throws SecurityException, IllegalArgumentException, NoSuchMethodException, IllegalAccessException, InvocationTargetException
	{
		Boolean ret = (Boolean) invokeMethod(o, testClassTeardownMethod);
		
		return ret.booleanValue();
	}
	

	/**
	 * \brief Run all sub tests in this Test Case 
	 * 
	 * @return true on no errors, false on one or more errors 
	 * 
	 * @throws InvocationTargetException 
	 * @throws IllegalAccessException 
	 * @throws IllegalArgumentException 
	 * @throws NoSuchMethodException 
	 * @throws SecurityException 
	 * @throws ClassNotFoundException 
	 * @throws InstantiationException 
	 */
	
	public boolean run() throws IllegalArgumentException, IllegalAccessException, InvocationTargetException, SecurityException, NoSuchMethodException, InstantiationException, ClassNotFoundException
	{	
		Method method;
		boolean result;
		int totalTests = tests.size();
				
		Object testClass = Class.forName(testClassName).newInstance();
		
		if (testRequiresSetup == true) {
			runTestSetup(testClass);
		}

		// Sort tests by name so that order so that their order is the same between runs.
		List<String> testNames = new ArrayList<String>();
		testNames.addAll(tests.keySet());
		Collections.sort(testNames);

		for (String key : testNames) {
			SubTest t = tests.get(key);

//			if (t.getMethodName().startsWith("testStaticFixups") == false)
//				continue;
			
			System.out.println("*** Testing [" + (testsRun + 1) + "/"
					+ totalTests + "]:\t" + t.getMethodName());

			/* Run the test case method */
			long startTime = System.currentTimeMillis();
			method = testClass.getClass().getMethod(t.getMethodName(), (Class[]) null);
			Boolean r = (Boolean) method.invoke(testClass, new Object[] {});
			long endTime = System.currentTimeMillis();
			result = r.booleanValue();

			/* Save the result */
			t.setResult(result);

			System.out.println("*** Test took " + (endTime - startTime) + " milliseconds");

			/* Update the totals */
			testsRun++;
			if (result == true) {
				System.out.println("OK\n");
				testsPassed++;
			} else {
				System.out.println("FAILED\n");
				testsFailed++;
			}
		}
		
		if (testRequiresTeardown) {
			runTestTeardown(testClass);
		}
								
		if (testsFailed > 0) {
			return false;
		} else {
			return true;
		}
			
	}
		

	public void printTestCaseMethods()
	{			
		System.out.println("Testcases for Class [" + testClassName + "]");
		
		Iterator it = tests.keySet().iterator();
		while (it.hasNext()) {				
			Object key = (String) it.next();
			SubTest t = (SubTest) tests.get(key);
			System.out.println("\t" + t.getMethodName());
		}
	}

}
