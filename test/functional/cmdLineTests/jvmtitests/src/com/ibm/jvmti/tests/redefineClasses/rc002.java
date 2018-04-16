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
package com.ibm.jvmti.tests.redefineClasses;

import java.lang.reflect.Method;

import com.ibm.jvmti.tests.util.Util;

public class rc002 {
	public static native boolean redefineClass(Class name, int classBytesSize, byte[] classBytes);

	public boolean setup(String args) {
		return true;
	}

	private static boolean methodExists(Class klass, String methodName, Class[] parameterTypes) {
		try {
			Method method = klass.getMethod(methodName, parameterTypes);
			if (method != null) {
				return true;
			}
		} catch (Exception e) {
		}
		return false;
	}

	// rather than calling !methodExists(), this specifically checks for a NoSuchMethodException
	private static boolean noSuchMethod(Class klass, String methodName, Class[] parameterTypes) {
		try {
			klass.getMethod(methodName, parameterTypes);
		} catch (NoSuchMethodException e) {
			return true;
		} catch (Exception e) {
		}
		return false;
	}

	//****************************************************************************************
	// 
	// 

	public boolean testAddMethod() {
		rc002_testAddMethod_O1 t_pre = new rc002_testAddMethod_O1();

		// original version has a meth1 but no meth2
		if (!methodExists(t_pre.getClass(), "meth1", null)) {
			return false;
		}
		if (!noSuchMethod(t_pre.getClass(), "meth2", null)) {
			return false;
		}

		boolean redefined = Util.redefineClass(getClass(), rc002_testAddMethod_O1.class, rc002_testAddMethod_R1.class);
		if (!redefined) {
			return false;
		}

		// redefined version has a meth1 and a meth2
		if (!methodExists(t_pre.getClass(), "meth1", null)) {
			return false;
		}
		if (!methodExists(t_pre.getClass(), "meth2", null)) {
			return false;
		}

		return true;
	}

	public String helpAddMethod() {
		return "Exercise the extended redefine by adding a new Method. Note that this will fail on a SUN vm."; 
	}

	//****************************************************************************************
	// 
	// 

	public boolean testDeleteMethod() {
		rc002_testDeleteMethod_O1 t_pre = new rc002_testDeleteMethod_O1();

		// original version has a meth1 and a meth2
		if (!methodExists(t_pre.getClass(), "meth1", null)) {
			return false;
		}
		if (!methodExists(t_pre.getClass(), "meth2", null)) {
			return false;
		}

		boolean redefined = Util.redefineClass(getClass(), rc002_testDeleteMethod_O1.class, rc002_testDeleteMethod_R1.class);
		if (!redefined) {
			return false;
		}

		// redefined version has a meth1 but no meth2
		if (!methodExists(t_pre.getClass(), "meth1", null)) {
			return false;
		}
		if (!noSuchMethod(t_pre.getClass(), "meth2", null)) {
			return false;
		}

		return true;
	}
	
	public String helpDeleteMethod()
	{
		return "Exercise the extended redefine by deleting a Method. Note that this will fail on a SUN vm."; 
	}

	//****************************************************************************************
	// 
	// 

	public boolean testAddMethodThenRevertBack() {
		rc002_testAddMethodThenRevertBack_O1 t_pre = new rc002_testAddMethodThenRevertBack_O1();

		// original version has a meth1 but no meth2
		if (!methodExists(t_pre.getClass(), "meth1", null)) {
			return false;
		}
		if (!noSuchMethod(t_pre.getClass(), "meth2", null)) {
			return false;
		}

		boolean redefined = Util.redefineClass(getClass(), rc002_testAddMethodThenRevertBack_O1.class, rc002_testAddMethodThenRevertBack_R1.class);
		if (!redefined) {
			return false;
		}

		// redefined version has a meth1 and a meth2
		if (!methodExists(t_pre.getClass(), "meth1", null)) {
			return false;
		}
		if (!methodExists(t_pre.getClass(), "meth2", null)) {
			return false;
		}

		// now, revert it back... load the original class bytes
		redefined = Util.redefineClass(getClass(), rc002_testAddMethodThenRevertBack_O1.class, rc002_testAddMethodThenRevertBack_O1.class);
		if (!redefined) {
			return false;
		}

		// original version has a meth1 but no meth2
		if (!methodExists(t_pre.getClass(), "meth1", null)) {
			return false;
		}
		if (!noSuchMethod(t_pre.getClass(), "meth2", null)) {
			return false;
		}

		return true;
	}
	
	public String helpAddMethodThenRevertBack()
	{
		return "Exercise the extended redefine by adding a Method and then reverting back." +
			" Note that this will fail on a SUN vm."; 
	}

	//****************************************************************************************
	// 
	// 

	public boolean testDeleteMethodThenRevertBack() {
		rc002_testDeleteMethodThenRevertBack_O1 t_pre = new rc002_testDeleteMethodThenRevertBack_O1();

		// original version has a meth1 and a meth2
		if (!methodExists(t_pre.getClass(), "meth1", null)) {
			return false;
		}
		if (!methodExists(t_pre.getClass(), "meth2", null)) {
			return false;
		}

		boolean redefined = Util.redefineClass(getClass(), rc002_testDeleteMethodThenRevertBack_O1.class, rc002_testDeleteMethodThenRevertBack_R1.class);
		if (!redefined) {
			return false;
		}

		// redefined version has a meth1 but no meth2
		if (!methodExists(t_pre.getClass(), "meth1", null)) {
			return false;
		}
		if (!noSuchMethod(t_pre.getClass(), "meth2", null)) {
			return false;
		}

		// now, revert it back... load the original class bytes
		redefined = Util.redefineClass(getClass(), rc002_testDeleteMethodThenRevertBack_O1.class, rc002_testDeleteMethodThenRevertBack_O1.class);
		if (!redefined) {
			return false;
		}

		// original version has a meth1 and a meth2
		if (!methodExists(t_pre.getClass(), "meth1", null)) {
			return false;
		}
		if (!methodExists(t_pre.getClass(), "meth2", null)) {
			return false;
		}

		return true;
	}

	public String helpDeleteMethodThenRevertBack()
	{
		return "Exercise the extended redefine by deleting a Method and then reverting back." +
			" Note that this will fail on a SUN vm."; 
	}
}
