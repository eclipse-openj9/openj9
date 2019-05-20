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
package com.ibm.jvmti.tests.redefineClasses;

import java.lang.reflect.Method;

import com.ibm.jvmti.tests.util.Util;

public class rc011 {
	public static boolean redefineClass(Class name, int classBytesSize, byte[] classBytes) {
		return redefineClass1(name, classBytesSize, classBytes);
	}
	public static native boolean redefineClass1(Class name, int classBytesSize, byte[] classBytes);
	public static native boolean redefineClass3(Class name, int classBytesSize, byte[] classBytes,
			Class name2, int classBytesSize2, byte[] classBytes2,
			Class name3, int classBytesSize3, byte[] classBytes3
	);

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
	
	public boolean testITableUpdates() {
		boolean redefined;
		byte classBytesO1[];
		byte classBytesO2[];
		byte classBytesO3[];
		byte classBytesO4[];

		Class classO1o = rc011_testITableUpdates_O1.class;
		Class classO1r = rc011_testITableUpdates_R1.class;
		
		Class classO2o = rc011_testITableUpdates_O2.class;
		Class classO2r = rc011_testITableUpdates_R2.class;
		
		Class classO3o = rc011_testITableUpdates_O3.class;
		Class classO3r = rc011_testITableUpdates_R3.class;

		Class classO4o = rc011_testITableUpdates_O4.class;
		Class classO4r = rc011_testITableUpdates_R4.class;

			
		if ((classBytesO1 = Util.loadRedefinedClassBytesWithOriginalClassName(classO1o, classO1r)) == null) {
			return false;
		}
		if ((classBytesO2 = Util.loadRedefinedClassBytesWithOriginalClassName(classO2o, classO2r)) == null) {
			return false;
		}
		if ((classBytesO3 = Util.loadRedefinedClassBytesWithOriginalClassName(classO3o, classO3r)) == null) {
			return false;
		}
		if ((classBytesO4 = Util.loadRedefinedClassBytesWithOriginalClassName(classO4o, classO4r)) == null) {
			return false;
		}
		
		
		System.out.println("redef O3");
		redefined = redefineClass1(classO3o, classBytesO3.length, classBytesO3);
		if (!redefined) {
			return false;
		}


		System.out.println("redef O4");
		redefined = redefineClass1(classO4o, classBytesO4.length, classBytesO4);
		if (!redefined) {
			return false;
		}
				
		System.out.println("redef O1");
		redefined = redefineClass1(classO1o, classBytesO1.length, classBytesO1);
		if (!redefined) {
			return false;
		}
		
		System.out.println("redef O2");
		redefined = redefineClass1(classO2o, classBytesO2.length, classBytesO2);
		if (!redefined) {
			return false;
		}
		
		System.gc();
		
		// redefineClass1(classO4o, 0, classBytesO4);
		
		
		return true;
	}

	public String helpITableUpdates() {
		return "Test redefines to an interface. This will not work on a SUN VM."; 
	}

	private boolean verify(rc011_testReorderingInterfaceMethods_O1 obj, rc011_testReorderingInterfaceMethods_O5 obj2, int stage) {
		if (!"foo".equals(obj.getFoo()) || !"bar".equals(obj.getBar())) {
			System.out.println("Failed " + stage + ": " + obj.getFoo() + " " + obj.getBar());
			return false;
		}
		if (!"foo".equals(obj2.getFoo()) || !"bar".equals(obj2.getBar())) {
			System.out.println("Failed " + stage + " (obj2): " + obj2.getFoo() + " " + obj2.getBar());
			return false;
		}
		if (!"foo2".equals(obj2.getFoo2()) || !"bar2".equals(obj2.getBar2())) {
			System.out.println("Failed " + stage + " (obj2): " + obj2.getFoo2() + " " + obj2.getBar2());
			return false;
		}
		return true;
	}

	public boolean testReorderingInterfaceMethods() throws ClassNotFoundException {
		// Ensure two implementers of the tested interfaces to prevent the
		// JIT from optimizing away the invokeinterface.
		Class.forName("com.ibm.jvmti.tests.redefineClasses.rc011_testReorderingInterfaceMethods_O2b");
		Class.forName("com.ibm.jvmti.tests.redefineClasses.rc011_testReorderingInterfaceMethods_O6b");
		rc011_testReorderingInterfaceMethods_O1 obj = new rc011_testReorderingInterfaceMethods_O2();
		rc011_testReorderingInterfaceMethods_O5 obj2 = new rc011_testReorderingInterfaceMethods_O6();

		if (!verify(obj, obj2, 1)) {
			return false;
		}

		// Redefine the interface through which we're calling the methods...
		boolean redefined = Util.redefineClass(getClass(), rc011_testReorderingInterfaceMethods_O1.class, rc011_testReorderingInterfaceMethods_R1.class);
		if (!redefined) {
			return false;
		}

		if (!verify(obj, obj2, 2)) {
			return false;
		}

		obj = new rc011_testReorderingInterfaceMethods_O3();
		if (!"foot".equals(obj.getFoo()) || !"boot".equals(obj.getBar())) {
			System.out.println("Failed 3: " + obj.getFoo() + " " + obj.getBar());
			return false;
		}

		obj = new rc011_testReorderingInterfaceMethods_O4();
		if (!"foot".equals(obj.getFoo()) || !"boot".equals(obj.getBar())) {
			System.out.println("Failed 4: " + obj.getFoo() + " " + obj.getBar());
			return false;
		}

		return true;
	}

	public String helpReorderingInterfaceMethods() {
		return "Test redefining an interfaces that re-orders its methods."; 
	}
}

