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


public class rc009 {
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
	
	public boolean testInterfaceUpdates() {
		boolean redefined;
		byte classBytes[];

		Class class1 = rc009_testInterfaceUpdates_O1.class;
		Class class2 = rc009_testInterfaceUpdates_R1.class;

		rc009_testInterfaceUpdates_O3 t_pre = new rc009_testInterfaceUpdates_O3();
		
		t_pre.doStuff("A");
		
		if (!(t_pre instanceof rc009_testInterfaceUpdates_O1)) {
			return false;
		}
		
		// method doesn't exist in the original interface
		if (methodExists(class1, "toString", new Class[0])) {
			return false;
		}
		
		if (!(t_pre instanceof rc009_testInterfaceUpdates_O1)) {
			return false;
		}
		
		if ((classBytes = Util.loadRedefinedClassBytesWithOriginalClassName(class1, class2)) == null) {
			return false;
		}

		if (!t_pre.getS().equals("A")) {
			return false;
		}
		
		redefined = redefineClass(class1, classBytes.length, classBytes);
		if (!redefined) {
			return false;
		}

		// method now exists in the redefined interface
		if (!methodExists(class1, "toString", new Class[0])) {
			return false;
		}

		if (!(t_pre instanceof rc009_testInterfaceUpdates_O1)) {
			return false;
		}
		
		if (!t_pre.getS().equals("A")) {
			return false;
		}

		return true;
	}

	public String helpInterfaceUpdates() {
		return "Test redefines to an interface. This will not work on a SUN VM."; 
	}
	
	public boolean testInterfaceUpdatesRevert() {
		boolean redefined;
		byte classBytes[];
		byte originalBytes[];

		Class class1 = rc009_testInterfaceUpdatesRevert_O1.class;
		Class class2 = rc009_testInterfaceUpdatesRevert_R1.class;

		if ((originalBytes = Util.getClassBytes(class1)) == null) {
			return false;
		}
		
		rc009_testInterfaceUpdatesRevert_O3 t_pre = new rc009_testInterfaceUpdatesRevert_O3();
		
		t_pre.doStuff("A");
		
		if (!(t_pre instanceof rc009_testInterfaceUpdatesRevert_O1)) {
			return false;
		}
		
		// method doesn't exist in the original interface
		if (methodExists(class1, "toString", new Class[0])) {
			return false;
		}
		
		if (!(t_pre instanceof rc009_testInterfaceUpdatesRevert_O1)) {
			return false;
		}
		
		if ((classBytes = Util.loadRedefinedClassBytesWithOriginalClassName(class1, class2)) == null) {
			return false;
		}

		if (!t_pre.getS().equals("A")) {
			return false;
		}

		System.out.println("Redefine to add methods");
		redefined = redefineClass(class1, classBytes.length, classBytes);
		if (!redefined) {
			return false;
		}

		// method now exists in the redefined interface
		if (!methodExists(class1, "toString", new Class[0])) {
			return false;
		}

		if (!(t_pre instanceof rc009_testInterfaceUpdatesRevert_O1)) {
			return false;
		}
		
		if (!t_pre.getS().equals("A")) {
			return false;
		}

		// redefine it back to the original
		System.out.println("Redefine back");
		redefined = redefineClass(class1, originalBytes.length, originalBytes);
		if (!redefined) {
			return false;
		}
		
		// method doesn't exist in the original interface
		if (methodExists(class1, "toString", new Class[0])) {
			return false;
		}
		
		if (!(t_pre instanceof rc009_testInterfaceUpdatesRevert_O1)) {
			return false;
		}
		
		if (!t_pre.getS().equals("A")) {
			return false;
		}

		return true;
	}
	
	public String helpInterfaceUpdatesRevert() {
		return "Test interface redefine and revert back. This will not work on a SUN VM.";
	}
}
