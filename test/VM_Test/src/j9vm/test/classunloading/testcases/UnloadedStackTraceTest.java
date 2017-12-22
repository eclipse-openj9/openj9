/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
/*
 * Created on May 31, 2004
 *
 * To change the template for this generated file go to
 * Window>Preferences>Java>Code Generation>Code and Comments
 */
package j9vm.test.classunloading.testcases;

import j9vm.test.classunloading.ClassUnloadingTestParent;
import j9vm.test.classunloading.SimpleJarClassLoader;

/**
 * @author PBurka
 *
 * To change the template for this generated type comment go to
 * Window>Preferences>Java>Code Generation>Code and Comments
 */
public class UnloadedStackTraceTest extends ClassUnloadingTestParent {
	
	public static class MyError extends Error {
		public MyError() {
			super();
		}
		public MyError(String msg) {
			super(msg);
		}
	}
	
	private MyError throwable; 
	private volatile Class clazz;
	
	private static final String className = "j9vm.test.classunloading.classestoload.ClassToLoadWithException";
	
	public UnloadedStackTraceTest() {
		/* maximum waitTime is set to 6 mins*/
		waitTime = 360000;
	}
		
	public static void main(String[] args) throws Exception {
		new UnloadedStackTraceTest().runTest();
	}
	
	protected String[] unloadableItems() { 
		return new String[] {"ClassLoader", className};
	}
	protected String[] itemsToBeUnloaded() { 
		return new String[] {"ClassLoader", className};
	}
	protected void createScenario() throws Exception {
		clazz = new SimpleJarClassLoader("ClassLoader", jarFileName).loadClass(className);
		try {
			clazz.newInstance();
		} catch (MyError ex) {
			throwable = ex;

			System.err.println("******** BEFORE UNLOAD ********");
			throwable.printStackTrace(System.err);
			System.err.println("*******************************");
		}
		if (throwable == null) {
			throw new Error(className + " did not throw expected exception");
		}

		// now allow the class to be collected
		clazz = null; 
	}
	
	/* This is the meat of the test case.
	 * Check that we can print the stack trace without crashing
	 */
	protected void afterFinalization() throws Exception {
		System.err.println("******** AFTER UNLOAD ********");
		throwable.printStackTrace(System.err);
		System.err.println("*******************************");

	}
	
}
