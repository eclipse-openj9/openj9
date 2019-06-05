/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package com.ibm.jvmti.tests.classModificationAgent;

import java.lang.reflect.Method;
import com.ibm.jvmti.tests.util.Util;
import com.ibm.jvmti.tests.util.CustomClassLoader;

/* This test verifies that class bytes passed to ClassFileLoadHook callback when running with multiple agents are as expected. 
 * This test uses three agents: cma001, rca001 and ria001.
 * Please check jvmtitests.xml for its usage.
 */
public class cma001 {

	private static final String TEST_PACKAGE = "com.ibm.jvmti.tests.classModificationAgent";
	private static final String TEST_CLASS_NAME_WITHOUT_VERSION = "cma001_TestClass_V";
	
	private static final int ORIGINAL_VERSION = 1;
	private static final int REDEFINED_VERSION = 2;
	
	/* Following constants specify the stage of the test */
	private static final int CLASS_LOADING = 1;
	private static final int CLASS_RETRANSFORMING = 2;
	private static final int CLASS_REDEFINING = 3;
	
	static int stage;
	
	/* Class bytes of 'currentClassVersion' test class are passed to the ClassFileLoadHook callback.
	 * 
	 * During CLASS_LOADING stage it is initially set to ORIGINAL_VERSION.
	 * Each agent, in its ClassFileLoadHook callback, then sets it to the version specified by the its command line argument.
	 * 
	 * During CLASS_RETRANSFORMING stage it is set by native agent to the version specified by the its command line argument.
	 *    
	 * During CLASS_REDEFINING it is set to REDEFINED_VERSION.
	 * Each agent, in its ClassFileLoadHook callback, then sets it to the version specified by the its command line argument.
	 */
	static int currentClassVersion;

	static boolean retransformationIncapableHookCalled = false;
	
	/* This field is used by ClassFileLoadHook callbacks to indicate an error */
	static boolean classFileLoadHookCallbackResult = true;

	private native boolean retransformClass(Class originalClass);
	private native boolean redefineClass(Class originalClass, byte[] redefinedClassBytes);
	
	private static byte[] getClassBytes(String className) {
		byte[] classBytes = Util.getClassBytes(className);
		classBytes = Util.replaceClassNames(classBytes, TEST_CLASS_NAME_WITHOUT_VERSION + ORIGINAL_VERSION, Util.getSimpleName(className));
		return classBytes;
	}
	
	private static byte[] getClassBytesForVersion(int version) {
		String className = TEST_PACKAGE + "." + TEST_CLASS_NAME_WITHOUT_VERSION + version;
		return getClassBytes(className);
	}
	
	private static byte[] getRedefinedClass() {
		String className = TEST_PACKAGE + "." + TEST_CLASS_NAME_WITHOUT_VERSION + REDEFINED_VERSION;
		return getClassBytes(className);
	}
	
	private boolean verifyOutput(int value) {
		int expectedValue = currentClassVersion;
		if (expectedValue != value) {
			return false;
		}
		return true;
	}
	
	public String helpClassModification() {
		return "Tests class bytes recieved by class transformer are as per the specification";
	}
	
	/*
	 * Description of the testing done by cma001.testClassModification():
	 * 
	 * ria001 is a re-transformation incapable agent.
	 * rca001 is a re-transformation capable agent.
	 * Both these agents accept argument of the form V<n> indicating the version of class file to be used during transformation.
	 * 
	 * Assuming ria001 is passed Vi and rca001 is passed Vj (for actual values see the command line).
	 * 
	 * cma001 loads class cma001_TestClass_V1 and sets currentClassVersion = V1.
	 * ria001's ClassFileLoadHook callback is invoked that performs following actions:
	 *	- sets its expected version = cma001.currentClassVersion = V1, 
	 * 	- verifies bytes received correspond to its expected version i.e. V1,
	 *	- modifies the class bytes to Vi, 
	 *  - sets current version = Vi.
	 * rca001's ClassFileLoadHook callback is invoked that performs following actions:
	 * 	- sets its expected version = cma001.currentClassVersion = Vi,
	 * 	- verifies bytes received correspond to its expected version i.e. Vi,
	 *  - modifies the class bytes to Vj, 
	 *  - sets current version = Vj.
	 * 
	 * cma001 calls RetransformClasses() on cma001_TestClass_V1.
	 * rca001's ClassFileLoadHook callback is invoked that performs following actions:
	 * 	- verifies bytes received correspond to its expected version i.e. Vi,
	 *  - modifies the class bytes to Vj, 
	 *  - sets current version = Vj.
	 * 
	 * cma001 calls RedefineClasses() on cma001_TestClass_V1 with cma001_TestClass_V2 as new definition,
	 * and sets currentClassVersion = V2.
	 * ria001's ClassFileLoadHook callback is invoked that performs following actions:
	 * 	- sets its expected version = cma001.currentClassVersion = V2, 
	 * 	- verifies bytes received correspond to its expected version i.e. V2,
	 *  - modifies the class bytes to Vi, 
	 *  - sets current version = Vi.
	 * rca001's ClassFileLoadHook callback is invoked that performs following actions:
	 * 	- sets its expected version = cma001.currentClassVersion = Vi,
	 * 	- verifies bytes received correspond to its expected version i.e. Vi,
	 *  - modifies the class bytes to Vj, 
	 *  - sets current version = Vj.
	 * 
	 * cma001 calls RetransformClasses() on cma001_TestClass_V1.
	 * rca001's ClassFileLoadHook callback is invoked that performs following actions:
	 * 	- verifies bytes received correspond to its expected version i.e. Vi,
	 *  - modifies the class bytes to Vj, 
	 *  - sets current version = Vj.
	 */
	public boolean testClassModification() {
		Class originalClass;
		byte[] redefinedClassBytes;
		int value;
		
		CustomClassLoader classLoader = new CustomClassLoader(String.valueOf(ORIGINAL_VERSION));
		try {
			String classNameWithoutVersion = TEST_PACKAGE + "." + TEST_CLASS_NAME_WITHOUT_VERSION;
			currentClassVersion = ORIGINAL_VERSION;
			stage = CLASS_LOADING;
			retransformationIncapableHookCalled = false;
			originalClass = classLoader.loadClass(classNameWithoutVersion);
			Object obj = originalClass.newInstance();
			Method meth1 = originalClass.getMethod("getValue", (Class[]) null);
			if (false == classFileLoadHookCallbackResult) {
				System.err.println("classFileLoadHookCallback failed");
				return false;
			}
			value = (Integer)meth1.invoke(obj, (Object[]) null);
			System.out.print("Expected value: " + currentClassVersion + "\t Value recieved: " + value);
			if (verifyOutput(value) == false) {
				System.out.println("\t FAIL");
				return false;
			} else {
				System.out.println("\t PASS");
			}
			
			stage = CLASS_RETRANSFORMING;
			retransformationIncapableHookCalled = false;
			retransformClass(obj.getClass());
			if (false == classFileLoadHookCallbackResult) {
				System.err.println("classFileLoadHookCallback failed");
				return false;
			}
			System.out.print("Expected value: " + currentClassVersion + "\t Value recieved: " + value);
			if (verifyOutput(value) == false) {
				System.out.println("\t FAIL");
				return false;
			} else {
				System.out.println("\t PASS");
			}
			
			currentClassVersion = REDEFINED_VERSION;
			stage = CLASS_REDEFINING;
			retransformationIncapableHookCalled = false;
			redefinedClassBytes = getRedefinedClass();
			redefineClass(originalClass, redefinedClassBytes);
			if (false == classFileLoadHookCallbackResult) {
				System.err.println("classFileLoadHookCallback failed");
				return false;
			}
			System.out.print("Expected value: " + currentClassVersion + "\t Value recieved: " + value);
			if (verifyOutput(value) == false) {
				System.out.println("\t FAIL");
				return false;
			} else {
				System.out.println("\t PASS");
			}
			
			stage = CLASS_RETRANSFORMING;
			retransformationIncapableHookCalled = false;
			retransformClass(obj.getClass());
			if (false == classFileLoadHookCallbackResult) {
				System.err.println("classFileLoadHookCallback failed");
				return false;
			}
			System.out.print("Expected value: " + currentClassVersion + "\t Value recieved: " + value);
			if (verifyOutput(value) == false) {
				System.out.println("\t FAIL");
				return false;
			} else {
				System.out.println("\t PASS");
			}
		} catch (Exception e) {
			e.printStackTrace();
			return false;
		}
		return true;
	}
}
