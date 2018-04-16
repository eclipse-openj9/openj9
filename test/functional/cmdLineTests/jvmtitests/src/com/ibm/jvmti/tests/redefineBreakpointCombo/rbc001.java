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
package com.ibm.jvmti.tests.redefineBreakpointCombo;

import com.ibm.jvmti.tests.util.Util;
import com.ibm.jvmti.tests.util.CustomClassLoader;

import java.lang.reflect.*;

/* This test runs various combinations of re-definition and breakpoints.
 * Each test loads class rbc001_testClass_V1 by creating a new classloader and loading the class bytes.
 */

public class rbc001 {
	protected final int BREAKPOINTMETHOD1POS = 2;

	public static int breakpointCounter = 0;
	public static Class currentClass = null;
	public static Object currentInstance = null;

	private final int ORIGINAL_VERSION = 1;
	private final int VERSION1 = 1;
	private final int VERSION2 = 2;
	private final String TEST_PACKAGE = "com.ibm.jvmti.tests.redefineBreakpointCombo";
	private final String TEST_CLASS_NAME_WITHOUT_VERSION = "rbc001_testClass_V";

	private final int VERSION1_METHOD1_RET = 11;
	private final int VERSION2_METHOD1_RET = 111;
	private final int VERSION1_METHOD2_RET = 22;
	private final int VERSION2_METHOD2_RET = 222;
	private final int VERSION1_METHOD3_RET = 33;
	private final int VERSION2_METHOD3_RET = 333;


	private int currentVersion = ORIGINAL_VERSION;
	private static byte[] redefinedClassBytes;
	private static String redefinedClassName;
	private native boolean setTransformerData(String className, byte[] transformedClassBytes, int transformerAction);
	private native boolean redefineClass(Class originalClass, byte[] redefinedClassBytes);
	protected native boolean setBreakpoint(Class clazz, String methodName, String sig, long position);
	private native boolean clearBreakpoint(Class clazz, String methodName, String sig, long position);
	private native long getMethodID(Class clazz, String methodName, String sig);
	private native int callIntMethod(Object instance, long methodID);
	private native boolean setBreakpointInMethodID(long methodID, long position);
	protected native long getMethodIDFromStack(int framePos);

	public boolean setup(String args) {
		return true;
	}

	public static void main(String[] args) {
		rbc001 test = new rbc001();

		if(!test.testRedefineClass1()) System.out.println("test Fail 1");

		if(!test.testRedefineClass2()) System.out.println("test Fail 2");

		if(!test.testRedefineClass3()) System.out.println("test Fail 3");

		if(!test.testRedefineClass4()) System.out.println("test Fail 4");

		if(!test.testRedefineClassAndBreakpoint1()) System.out.println("test Fail 5");

		if(!test.RedefineClassAndBreakpoint2()) System.out.println("test Fail 6");

		if(!test.RedefineClassAndBreakpoint3()) System.out.println("test Fail 7");

		if(!test.BreakpointInObsoleteMethod1()) System.out.println("test Fail 8");

		if(!test.BreakpointInObsoleteMethod2()) System.out.println("test Fail 9");


	}

	public void prepareEquivalentBytecodes() {
		redefinedClassName = TEST_PACKAGE + "." + TEST_CLASS_NAME_WITHOUT_VERSION + VERSION1;
		redefinedClassBytes = Util.getClassBytes(redefinedClassName);
	}

	public void prepareNonEquivalentBytecodes() {
		redefinedClassName = TEST_PACKAGE + "." + TEST_CLASS_NAME_WITHOUT_VERSION + VERSION2;
		redefinedClassBytes = Util.getClassBytes(redefinedClassName);
		redefinedClassBytes = Util.replaceClassNames(redefinedClassBytes, TEST_CLASS_NAME_WITHOUT_VERSION + VERSION1, TEST_CLASS_NAME_WITHOUT_VERSION + VERSION2);
	}

	private void prepareClass() {
		CustomClassLoader classLoader = new CustomClassLoader(String.valueOf(ORIGINAL_VERSION));
		String classNameWithoutVersion = TEST_PACKAGE + "." + TEST_CLASS_NAME_WITHOUT_VERSION;
		try {
			currentClass = classLoader.loadClass(classNameWithoutVersion);
			currentInstance = currentClass.newInstance();
		} catch (ClassNotFoundException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (InstantiationException e) {
			e.printStackTrace();
		}
	}

	private int callMethod1() {
		Method meth1;
		try {
			meth1 = currentClass.getMethod("meth1", new Class[] {});
			return (Integer)meth1.invoke(currentInstance, new Object[] {});
		} catch (SecurityException e) {
			e.printStackTrace();
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}
		return 0;
	}

	private long callMethod2() {
		Method meth2;
		try {
			meth2 = currentClass.getMethod("meth2", new Class[] {});
			return (Long) meth2.invoke(currentInstance, new Object[] {});
		} catch (SecurityException e) {
			e.printStackTrace();
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}
		return 0;
	}

	private int callMethod3() {
		Method meth3;
		try {
			meth3 = currentClass.getMethod("meth3", new Class[] {});
			return (Integer) meth3.invoke(currentInstance, new Object[] {});
		} catch (SecurityException e) {
			e.printStackTrace();
		} catch (NoSuchMethodException e) {
			e.printStackTrace();
		} catch (IllegalArgumentException e) {
			e.printStackTrace();
		} catch (IllegalAccessException e) {
			e.printStackTrace();
		} catch (InvocationTargetException e) {
			e.printStackTrace();
		}
		return 0;
	}

	private boolean redefineClassMultipleTimes(int iter) {
		for (int i = 0; i < iter; i++) {
			getMethodID(currentClass, "meth1", "()I");
			getMethodID(currentClass, "meth2", "()J");
			getMethodID(currentClass, "meth3", "()I");
			if (!redefineClass(currentClass, redefinedClassBytes)) {
				return false;
			}
		}
		return true;
	}

	public String helpRedefineClass1() {
		return "testRedefineClass1";
	}

	/**
	 * Redefine class with equivalent bytecodes, test to see method returns the same value
	 */
	public boolean testRedefineClass1() {
		boolean rc = true;
		int oldValue;
		int newValue;
		prepareClass();
		prepareEquivalentBytecodes();

		if (VERSION1_METHOD1_RET != callMethod1()) {
			return false;
		}

		if (!redefineClass(currentClass, redefinedClassBytes)) {
			return false;
		}

		if (VERSION1_METHOD1_RET != callMethod1()) {
			return false;
		}

		return rc;
	}

	public String helpRedefineClass2() {
		return "testRedefineClass2";
	}

	/**
	 * Redefine class with non-equivalent bytecodes, test to see method returns a different value
	 */
	public boolean testRedefineClass2() {
		boolean rc = true;
		int oldValue;
		int newValue;
		prepareClass();
		prepareNonEquivalentBytecodes();

		if (VERSION1_METHOD1_RET != callMethod1()) {
			return false;
		}

		if (!redefineClass(currentClass, redefinedClassBytes)) {
			return false;
		}

		if (VERSION2_METHOD1_RET != callMethod1()) {
			return false;
		}

		return rc;
	}

	public String helpRedefineClass3() {
		return "testRedefineClass3";
	}

	/**
	 * Redefine class with equivalent bytecodes, test to see if methodID is the same
	 */
	public boolean testRedefineClass3() {
		boolean rc = true;
		long oldValue;
		long newValue;
		prepareClass();
		prepareEquivalentBytecodes();

		oldValue = getMethodID(currentClass, "meth1", "()I");

		if (!redefineClass(currentClass, redefinedClassBytes)) {
			return false;
		}

		newValue = getMethodID(currentClass, "meth1", "()I");

		if (oldValue != newValue) {
			System.out.println("FAIL: old val " + oldValue + ", newval " + newValue);
			rc = false;
		}

		return rc;
	}

	public String helpRedefineClass4() {
		return "testRedefineClass4";
	}

	/**
	 * Redefine class with equivalent different bytecodes, test to see if methodID is the same
	 */
	public boolean testRedefineClass4() {
		boolean rc = true;
		long oldValue;
		long newValue;
		prepareClass();
		prepareNonEquivalentBytecodes();

		oldValue = getMethodID(currentClass, "meth1", "()I");

		if (!redefineClass(currentClass, redefinedClassBytes)) {
			return false;
		}

		newValue = getMethodID(currentClass, "meth1", "()I");

		if (oldValue != newValue) {
			System.out.println("FAIL: old val " + oldValue + ", newval " + newValue);
			rc = false;
		}

		return rc;
	}

	public String helpRedefineClassAndBreakpoint1() {
		return "testRedefineClassAndBreakpoint1";
	}


	/**
	 * Redefine the class 10 times with identical bytecodes, then set a breakpoint on method
	 */
	public boolean testRedefineClassAndBreakpoint1() {
		boolean rc = true;
		breakpointCounter = 0;
		prepareClass();
		prepareEquivalentBytecodes();

		if (!redefineClassMultipleTimes(10)) {
			return false;
		}

		if (!setBreakpoint(currentClass, "meth1", "()I", BREAKPOINTMETHOD1POS)) {
			return false;
		}

		callMethod1();

		if (1 != breakpointCounter) {
			return false;
		}

		if (!clearBreakpoint(currentClass, "meth1", "()I", BREAKPOINTMETHOD1POS)) {
			return false;
		}

		return rc;
	}

	public String helpRedefineClassAndBreakpoint2() {
		return "testRedefineClassAndBreakpoint2";
	}

	/**
	 * Redefine the class 10 times with non identical bytecodes, then set a breakpoint on method
	 */
	public boolean RedefineClassAndBreakpoint2() {
		boolean rc = true;
		breakpointCounter = 0;
		prepareClass();
		prepareNonEquivalentBytecodes();

		if (!redefineClassMultipleTimes(10)) {
			return false;
		}

		if (!setBreakpoint(currentClass, "meth1", "()I", BREAKPOINTMETHOD1POS)) {
			return false;
		}

		int val = callMethod1();

		if (1 != breakpointCounter) {
			return false;
		}

		if (!clearBreakpoint(currentClass, "meth1", "()I", BREAKPOINTMETHOD1POS)) {
			return false;
		}

		return rc;
	}

	public String helpRedefineClassAndBreakpoint3() {
		return "testRedefineClassAndBreakpoint3";
	}

	/**
	 * Redefine the class 10 times with identical bytecodes then 10 times with
	 * non identical bytecodes, then set a breakpoint on method
	 */
	public boolean RedefineClassAndBreakpoint3() {
		boolean rc = true;
		breakpointCounter = 0;
		prepareClass();
		prepareEquivalentBytecodes();

		if (!redefineClassMultipleTimes(10)) {
			return false;
		}

		prepareNonEquivalentBytecodes();

		if (!redefineClassMultipleTimes(10)) {
			return false;
		}

		if (!setBreakpoint(currentClass, "meth1", "()I", BREAKPOINTMETHOD1POS)) {
			return false;
		}

		int val = callMethod1();

		if (1 != breakpointCounter) {
			return false;
		}

		if (!clearBreakpoint(currentClass, "meth1", "()I", BREAKPOINTMETHOD1POS)) {
			return false;
		}

		return rc;
	}

	public String helpBreakpointInObsoleteMethod1() {
		return "testBreakpointInObsoleteMethod1";
	}

	/**
	 * Test breakpoint in obsolete method. This will be done by:
	 * 1) run method
	 * 2) redefine method
	 * 3) while redefining method get obsolete methodID from stack
	 * 4) set breakpoint
	 */
	public boolean BreakpointInObsoleteMethod1() {
		boolean rc = true;
		breakpointCounter = 0;
		prepareClass();
		prepareNonEquivalentBytecodes();

		/* before redefinition */
		if (VERSION1_METHOD2_RET != callMethod2()) {
			return false;
		}

		/* after redefinition */
		if (VERSION2_METHOD2_RET != callMethod2()) {
			return false;
		}

		if (1 != breakpointCounter) {
			return false;
		}

		return rc;
	}

	public String helpBreakpointInObsoleteMethod2() {
		return "testBreakpointInObsoleteMethod2";
	}

	/**
	 * Test breakpoint in obsolete method. This will be done by:
	 * 1) run method
	 * 2) redefine with equivalent bytecodes run method gain, repeat 10 times
	 * 3) redefine with different bytecodes
	 * 4) within method redefine method
	 * 5) get obsolete methodID from stack
	 * 6) set breakpoint
	 */
	public boolean BreakpointInObsoleteMethod2() {
		boolean rc = true;
		breakpointCounter = 0;
		prepareClass();
		prepareEquivalentBytecodes();

		/* before redefinition */
		if (VERSION1_METHOD3_RET != callMethod3()) {
			return false;
		}

		/* after redefinition */
		if (VERSION2_METHOD3_RET != callMethod3()) {
			return false;
		}

		/* note Oracle does not put breakpoints in equivalent versions of an obsolete method
		 * Running this on Oracle will result in 1 breakpoint, on J9 there are 10.
		 * */
		if (0 == breakpointCounter) {
			return false;
		}

		return rc;
	}

	public boolean callRedefineClass() {
		return redefineClass(currentClass, redefinedClassBytes);
	}

	public boolean callSetBreakpoint(String methodName, String sig, int position) {
		return setBreakpoint(currentClass, methodName, sig, position);
	}

	public long callGetMethodIDFromStack(int position) {
		return getMethodIDFromStack(position);
	}

	public boolean callSetBreakpointInMethodID(long methodID, long position) {
		return setBreakpointInMethodID(methodID, position);
	}
}
