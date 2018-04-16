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
package com.ibm.jvmti.tests.retransformRedefineCombo;

import com.ibm.jvmti.tests.util.DefineAnonClass;
import com.ibm.jvmti.tests.util.Util;
import com.ibm.jvmti.tests.util.CustomClassLoader;
import java.lang.reflect.*;

/* This test runs various combinations of re-transformation and re-definition.
 * Each test loads class rrc001_testClass_V1 by creating its object and then re-transforms or re-defines it.
 * Class bytes for re-transformation or re-definition are obtained from rrc001_testClass_V<n>.java where n = 2, 3, ...
 * Each class named 'rrc001_testClass_V<n>.java' has only one method named "meth1".
 * This method returns an integer same as the version 'n' of the class in its name rrc001_testClass_V<n>.
 * 
 * Each combination creates a list of action to be performed by the native agent/transformer.
 * An action indicates whether the transformer should modify the class bytes or not in its ClassFileLoadHook callback
 * which gets invoked for class-loading/re-transformation/redefinition.
 * For each action in the list, the test determines 
 * 	- which version of the class and class data is to be passed to the transformer.
 *  - which version of the class is expected to be used by JVM after re-transformation/re-definition.
 * 
 * To determine if re-transformation/re-definition worked as expected,
 * it verifies that value returned by the call to object.meth1() is same as version of the class expected to be used by JVM. 
 */

public class rrc001 {
	
	/* Following constants specifies action to be taken by native class transformer */
	private final int MODIFY = 0;
	private final int DONT_MODIFY = 1;
	private final int REDEFINE_MODIFY = 2;
	private final int REDEFINE_DONT_MODIFY = 3;
	
	private final int ORIGINAL_VERSION = 1;
	private final String TEST_PACKAGE = "com.ibm.jvmti.tests.retransformRedefineCombo";
	private final String TEST_CLASS_NAME_WITHOUT_VERSION = "rrc001_testClass_V";
	
	private int currentVersion = ORIGINAL_VERSION;
	private byte[] transformedClassBytes, redefinedClassBytes;
	private String redefinedClassName, expectedUsedClassName;
	private boolean isClassRedefined;
	private int transformerAction;
	
	private native boolean setTransformerData(String className, byte[] transformedClassBytes, int transformerAction);
	private native boolean retransformClass(Class originalClass, String className, byte[] transformedClassBytes, int transformerAction);
	private native boolean redefineClass(Class originalClass, String className, byte[] redefinedClassBytes, byte[] transformedClassBytes, int transformerAction);
		
	public boolean setup(String args) {
		return true;
	}
	
	private String getNextClassVersion() {
		currentVersion++;
		return TEST_PACKAGE + "." + TEST_CLASS_NAME_WITHOUT_VERSION + currentVersion;
	}
	
	private String getClassNameForRedefinition() {
		return getNextClassVersion();
	}
	
	private String getClassNameForTransformation() {
		if ((transformerAction == DONT_MODIFY) || (transformerAction == REDEFINE_DONT_MODIFY)) {
			return null;
		}
		return getNextClassVersion();
	}
	
	private String getExpectedUsedClass() {
		return TEST_PACKAGE + "." + TEST_CLASS_NAME_WITHOUT_VERSION + currentVersion;
	}
	
	/* This method is used to set context for current phase of the test.
	 * This involves getting the class version and its class bytes to be used by the transformer,
	 * It also determines the class version expected to be used by JVM after current phase. This is used to verify the output. 
	 */
	private void setContext(int action) {
		String transformedClassName;
		
		transformerAction = action;
		
		if ((transformerAction == REDEFINE_DONT_MODIFY) || (transformerAction == REDEFINE_MODIFY)) {
			redefinedClassName = getClassNameForRedefinition();
			redefinedClassBytes = Util.getClassBytes(redefinedClassName);
			redefinedClassBytes = Util.replaceClassNames(redefinedClassBytes, TEST_CLASS_NAME_WITHOUT_VERSION + ORIGINAL_VERSION, Util.getSimpleName(redefinedClassName));
		}
		
		transformedClassName = getClassNameForTransformation();
		if (null != transformedClassName) {
			transformedClassBytes = Util.getClassBytes(transformedClassName);
			transformedClassBytes = Util.replaceClassNames(transformedClassBytes, TEST_CLASS_NAME_WITHOUT_VERSION + ORIGINAL_VERSION, Util.getSimpleName(transformedClassName));
		} else {
			transformedClassBytes = null;
		}
		
		if (DONT_MODIFY == action) {
			if (false == isClassRedefined) {
				expectedUsedClassName = TEST_PACKAGE + "." + TEST_CLASS_NAME_WITHOUT_VERSION + ORIGINAL_VERSION;
			} else {
				expectedUsedClassName = redefinedClassName;
			}
		} else {
			expectedUsedClassName = TEST_PACKAGE + "." + TEST_CLASS_NAME_WITHOUT_VERSION + currentVersion;
		}
		
		System.out.println("expectedUsedClassName:" + expectedUsedClassName);
	}
	
	private boolean verifyOutput(int value) {
		try {
			/* Expected value is the version of the class file i.e. the last character in the class name */
			int expectedValue = Integer.parseInt(expectedUsedClassName.substring(expectedUsedClassName.length()-1, expectedUsedClassName.length()));
			System.out.print("\tExpected value: " + expectedValue);
			if (expectedValue != value) {
				System.out.print("\tFAIL\n");
				return false;
			} else {
				System.out.print("\tPASS\n");
			}
		} catch (Exception e) {
			e.printStackTrace();
			return false;
		}
		return true;
	}
	
	private boolean runTestCombo(int actionList[], boolean anonTest)
	{
		int value;
		boolean rc;
		Class originalClass;
		String originalClassName = TEST_PACKAGE + "." + TEST_CLASS_NAME_WITHOUT_VERSION + ORIGINAL_VERSION;
		
		currentVersion = ORIGINAL_VERSION;
		isClassRedefined = false;
		CustomClassLoader classLoader = new CustomClassLoader(String.valueOf(ORIGINAL_VERSION));
		try {
			System.out.println("Call 1...");
			setContext(actionList[0]);
			setTransformerData(originalClassName.replace('.', '/'), transformedClassBytes, transformerAction);
			String classNameWithoutVersion = TEST_PACKAGE + "." + TEST_CLASS_NAME_WITHOUT_VERSION;
			originalClass = classLoader.loadClass(classNameWithoutVersion);
			if (anonTest) {
				originalClass = DefineAnonClass.defineAnonClass(originalClass, rrc001.class);
			}
			Object obj = originalClass.newInstance();
			Method meth1 = originalClass.getMethod("meth1", new Class[] {});
			value = (Integer)meth1.invoke(obj, new Object[] {});
			System.out.print("value: "  + value);
			rc = verifyOutput(value);
			if (!rc) {
				return false;
			}
	
			System.out.println("Call 2...");
			setContext(actionList[1]);
			rc = retransformClass(originalClass, originalClassName.replace('.', '/'), transformedClassBytes, transformerAction);
			if (!rc) {
				return false;
			}
			value = (Integer)meth1.invoke(obj, new Object[] {});
			System.out.print("value: "  + value);
			rc = verifyOutput(value);
			if (!rc) {
				return false;
			}
			
			System.out.println("Call 3...");
			setContext(actionList[2]);
			rc = redefineClass(obj.getClass(), originalClassName.replace('.', '/'), redefinedClassBytes, transformedClassBytes, transformerAction);
			if (!rc) {
				return false;
			}
			value = (Integer)meth1.invoke(obj, new Object[] {});
			System.out.print("value: "  + value);
			rc = verifyOutput(value);
			if (!rc) {
				return false;
			}
			isClassRedefined = true;
			
			System.out.println("Call 4...");
			setContext(actionList[3]);
			rc = retransformClass(obj.getClass(), originalClassName.replace('.', '/'), transformedClassBytes, transformerAction);
			if (!rc) {
				return false;
			}
			value = (Integer)meth1.invoke(obj, new Object[] {});
			System.out.print("value: "  + value);
			rc = verifyOutput(value);
			if (!rc) {
				return false;
			}
			return true;
			
		} catch (Exception e) {
			e.printStackTrace();
			return false;
		}
	}

	public String helpRetransformationIncapable() {
		return "Tests RetransformationIncapable.";
	}
	
	public boolean testRetransformationIncapable()
	{
		int actionList[] = { MODIFY, DONT_MODIFY, REDEFINE_DONT_MODIFY, DONT_MODIFY };
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}

	public String helpRetransformRedefineCombo0() {
		return "Tests RetransformRedefineCombo0.";
	}

	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Do not modify class during class load
	 * 		Expected class version: V1 
	 * 	- Do not modify class during first re-transformation
	 * 		Expected class version: V1
	 * 	- Do not modify class during re-definition
	 * 		Expected class version: V2
	 * 	- Do not modify class during second re-transformation 
	 * 		Expected class version: V2
	 */
	public boolean testRetransformRedefineCombo0() {
		int actionList[] = {DONT_MODIFY, DONT_MODIFY, REDEFINE_DONT_MODIFY, DONT_MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo1() {
		return "Tests RetransformRedefineCombo1.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Do not modify class during class load
	 * 		Expected class version: V1
	 * 	- Do not modify class during first re-transformation
	 * 		Expected class version: V1
	 * 	- Do not modify class during re-definition
	 * 		Expected class version: V2
	 * 	- Modify class during second re-transformation 
	 * 		Expected class version: V3
	 */
	public boolean testRetransformRedefineCombo1() {
		int actionList[] = {DONT_MODIFY, DONT_MODIFY, REDEFINE_DONT_MODIFY, MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo2() {
		return "Tests RetransformRedefineCombo2.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Do not modify class during class load
	 * 		Expected class version: V1
	 * 	- Do not modify class during first re-transformation
	 * 		Expected class version: V1
	 * 	- Modify class during re-definition
	 * 		Expected class version: V3
	 * 	- Do not modify class during second re-transformation
	 * 		Expected class version: V3
	 */
	public boolean testRetransformRedefineCombo2() {
		int actionList[] = {DONT_MODIFY, DONT_MODIFY, REDEFINE_MODIFY, DONT_MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo3() {
		return "Tests RetransformRedefineCombo3.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Do not modify class during class load
	 * 		Expected class version: V1
	 * 	- Do not modify class during first re-transformation
	 * 		Expected class version: V1
	 * 	- Modify class during re-definition
	 * 		Expected class version: V3
	 * 	- Modify class during second re-transformation
	 * 		Expected class version: V4
	 */
	public boolean testRetransformRedefineCombo3() {
		int actionList[] = {DONT_MODIFY, DONT_MODIFY, REDEFINE_MODIFY, MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo4() {
		return "Tests RetransformRedefineCombo4.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Do not modify class during class load
	 * 		Expected class version: V1
	 * 	- Modify class during first re-transformation
	 * 		Expected class version: V2
	 * 	- Do not modify class during re-definition
	 * 		Expected class version: V3
	 * 	- Do not modify class during second re-transformation
	 * 		Expected class version: V3
	 */
	public boolean testRetransformRedefineCombo4() {
		int actionList[] = {DONT_MODIFY, MODIFY, REDEFINE_DONT_MODIFY, DONT_MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo5() {
		return "Tests RetransformRedefineCombo5.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Do not modify class during class load
	 * 		Expected class version: V1
	 * 	- Modify class during first re-transformation
	 * 		Expected class version: V2
	 * 	- Do not modify class during re-definition
	 * 		Expected class version: V3
	 * 	- Modify class during second re-transformation
	 * 		Expected class version: V4
	 */
	public boolean testRetransformRedefineCombo5() {
		int actionList[] = {DONT_MODIFY, MODIFY, REDEFINE_DONT_MODIFY, MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo6() {
		return "Tests RetransformRedefineCombo6.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Do not modify class during class load
	 * 		Expected class version: V1
	 * 	- Modify class during first re-transformation
	 * 		Expected class version: V2
	 * 	- Modify class during re-definition
	 * 		Expected class version: V4
	 * 	- Do not modify class during second re-transformation
	 * 		Expected class version: V4
	 */
	public boolean testRetransformRedefineCombo6() {
		int actionList[] = {DONT_MODIFY, MODIFY, REDEFINE_MODIFY, DONT_MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo7() {
		return "Tests RetransformRedefineCombo7.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Do not modify class during class load
	 * 		Expected class version: V1
	 * 	- Modify class during first re-transformation
	 * 		Expected class version: V2
	 * 	- Modify class during re-definition
	 * 		Expected class version: V4
	 * 	- Modify class during second re-transformation 
	 * 		Expected class version: V5
	 */
	public boolean testRetransformRedefineCombo7() {
		int actionList[] = {DONT_MODIFY, MODIFY, REDEFINE_MODIFY, MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo8() {
		return "Tests RetransformRedefineCombo8.";
	}

	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Modify class during class load
	 * 		Expected class version: V2
	 * 	- Do not modify class during first re-transformation
	 * 		Expected class version: V2
	 * 	- Do not modify class during re-definition
	 * 		Expected class version: V3
	 * 	- Do not modify class during second re-transformation
	 * 		Expected class version: V3
	 */
	public boolean testRetransformRedefineCombo8() {
		int actionList[] = {MODIFY, DONT_MODIFY, REDEFINE_DONT_MODIFY, DONT_MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo9() {
		return "Tests RetransformRedefineCombo9.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Modify class during class load
	 * 		Expected class version: V2
	 * 	- Do not modify class during first re-transformation
	 * 		Expected class version: V2
	 * 	- Do not modify class during re-definition
	 * 		Expected class version: V3
	 * 	- Modify class during second re-transformation
	 * 		Expected class version: V4
	 */
	public boolean testRetransformRedefineCombo9() {
		int actionList[] = {MODIFY, DONT_MODIFY, REDEFINE_DONT_MODIFY, MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo10() {
		return "Tests RetransformRedefineCombo10.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Modify class during class load
	 * 		Expected class version: V2
	 * 	- Do not modify class during first re-transformation
	 * 		Expected class version: V2
	 * 	- Modify class during re-definition
	 * 		Expected class version: V4
	 * 	- Do not modify class during second re-transformation
	 * 		Expected class version: V4
	 */
	public boolean testRetransformRedefineCombo10() {
		int actionList[] = {MODIFY, DONT_MODIFY, REDEFINE_MODIFY, DONT_MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo11() {
		return "Tests RetransformRedefineCombo11.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Modify class during class load
	 * 		Expected class version: V2
	 * 	- Do not modify class during first re-transformation
	 * 		Expected class version: V2
	 * 	- Modify class during re-definition
	 * 		Expected class version: V4
	 * 	- Modify class during second re-transformation
	 * 		Expected class version: V5
	 */
	public boolean testRetransformRedefineCombo11() {
		int actionList[] = {MODIFY, DONT_MODIFY, REDEFINE_MODIFY, MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo12() {
		return "Tests RetransformRedefineCombo12.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Modify class during class load
	 * 		Expected class version: V2
	 * 	- Modify class during first re-transformation
	 * 		Expected class version: V3
	 * 	- Do not modify class during re-definition
	 * 		Expected class version: V4
	 * 	- Do not modify class during second re-transformation
	 * 		Expected class version: V4
	 */
	public boolean testRetransformRedefineCombo12() {
		int actionList[] = {MODIFY, MODIFY, REDEFINE_DONT_MODIFY, DONT_MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo13() {
		return "Tests RetransformRedefineCombo13.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Modify class during class load
	 * 		Expected class version: V2
	 * 	- Modify class during first re-transformation
	 * 		Expected class version: V3
	 * 	- Do not modify class during re-definition
	 * 		Expected class version: V4
	 * 	- Modify class during second re-transformation
	 * 		Expected class version: V5
	 */
	public boolean testRetransformRedefineCombo13() {
		int actionList[] = {MODIFY, MODIFY, REDEFINE_DONT_MODIFY, MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo14() {
		return "Tests RetransformRedefineCombo14.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Modify class during class load
	 * 		Expected class version: V2
	 * 	- Modify class during first re-transformation
	 * 		Expected class version: V3
	 * 	- Modify class during re-definition
	 * 		Expected class version: V5
	 * 	- Do not modify class during second re-transformation
	 * 		Expected class version: V5
	 */
	public boolean testRetransformRedefineCombo14() {
		int actionList[] = {MODIFY, MODIFY, REDEFINE_MODIFY, DONT_MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}
	
	public String helpRetransformRedefineCombo15() {
		return "Tests RetransformRedefineCombo15.";
	}
	
	/**
	 * This test exercises following case:
	 * Class loaded is V1
	 * 	- Modify class during class load
	 * 		Expected class version: V2
	 * 	- Modify class during first re-transformation
	 * 		Expected class version: V3
	 * 	- Modify class during re-definition
	 * 		Expected class version: V5
	 * 	- Modify class during second re-transformation
	 * 		Expected class version: V6
	 */
	public boolean testRetransformRedefineCombo15() {
		int actionList[] = {MODIFY, MODIFY, REDEFINE_MODIFY, MODIFY};
		boolean rc = runTestCombo(actionList, false);
		return rc;
	}

}
