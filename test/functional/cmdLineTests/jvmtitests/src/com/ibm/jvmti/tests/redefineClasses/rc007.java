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

import java.lang.reflect.*;

import com.ibm.jvmti.tests.util.Util;

public class rc007 
{
	public static native boolean redefineClasses(
			Class c1, int size1, byte[] bytes1,
			Class c2, int size2, byte[] bytes2);
	
	public boolean setup(String args)
	{
		return true;
	}

	private static Method findMethod(Class klass, String methodName, Class[] parameterTypes)
	{
		try {
			Method method = klass.getMethod(methodName, parameterTypes);
			return method;
		} catch (Exception e) {
			e.printStackTrace();
		}
		return null;
	}
	
	// Invokes a method returning an integer that takes no parameters.
	// This returns Integer.MIN_VALUE on error (as this value would
	// never be returned normally by these test cases).
	private int invokeMethod(Method method, Object obj) {
		try {
			Object result = method.invoke(obj, (Object[]) null);
			return ((Integer) result).intValue();
		} catch (Exception e) {
			return Integer.MIN_VALUE;
		}
	}
	
	public boolean testMultipleClassRedefinesToThemselves()
	{
		byte classBytes1[];
		byte classBytes2[];

		if ((classBytes1 = Util.getClassBytes(rc007_testMultipleClassRedefines_O1.class)) == null) {
			return false;
		}
		if ((classBytes2 = Util.getClassBytes(rc007_testMultipleClassRedefines_O2.class)) == null) {
			return false;
		}
		
		Class class1 = rc007_testMultipleClassRedefines_O1.class;
		Class class2 = rc007_testMultipleClassRedefines_O2.class;
		boolean result = redefineClasses(
				class1, classBytes1.length, classBytes1,
				class2, classBytes2.length, classBytes2);
		if (result == false) {
			return false;
		}

		Method m1_pre = findMethod(class1, "meth1", new Class[0]);
		if (m1_pre == null) {
			return false;
		}
		Method m2_pre = findMethod(class2, "meth1", new Class[0]);
		if (m2_pre == null) {
			return false;
		}

		rc007_testMultipleClassRedefines_O1 obj1 = new rc007_testMultipleClassRedefines_O1();
		rc007_testMultipleClassRedefines_O2 obj2 = new rc007_testMultipleClassRedefines_O2();
		Method m1 = findMethod(class1, "meth1", new Class[0]);
		if (m1 == null) {
			return false;
		}
		Method m2 = findMethod(class2, "meth1", new Class[0]);
		if (m2 == null) {
			return false;
		}

		if (invokeMethod(m1, obj1) != 178) {
			return false;
		}
		if (invokeMethod(m1_pre, obj1) != 178) {
			return false;
		}
		if (invokeMethod(m2, obj2) != 100) {
			return false;
		}
		if (invokeMethod(m2_pre, obj2) != 100) {
			return false;
		}

		return true;		
	}
	
	public String helpMultipleClassRedefinesToThemselves()
	{
		return "Tests the simultaneous redefinition of several classes referencing each other to themselves.";
	}
	

	public boolean testMultipleClassRedefinesToNewVersions()
	{
		Class original, redefined;
		byte classBytes1[];
		byte classBytes2[];

		original = rc007_testMultipleClassRedefines_O1.class;
		redefined = rc007_testMultipleClassRedefines_R1.class;
		if ((classBytes1 = Util.loadRedefinedClassBytesWithOriginalClassName(original, redefined)) == null) {
			return false;
		}
		original = rc007_testMultipleClassRedefines_O2.class;
		redefined = rc007_testMultipleClassRedefines_R2.class;
		if ((classBytes2 = Util.loadRedefinedClassBytesWithOriginalClassName(original, redefined)) == null) {
			return false;
		}

		rc007_testMultipleClassRedefines_O1 obj1 = new rc007_testMultipleClassRedefines_O1();
		rc007_testMultipleClassRedefines_O2 obj2 = new rc007_testMultipleClassRedefines_O2();

		Class class1 = rc007_testMultipleClassRedefines_O1.class;
		Class class2 = rc007_testMultipleClassRedefines_O2.class;

		Method m1 = findMethod(class1, "meth1", new Class[0]);
		if (m1 == null) {
			return false;
		}
		Method m2 = findMethod(class2, "meth1", new Class[0]);
		if (m2 == null) {
			return false;
		}

		boolean result = redefineClasses(
				class1, classBytes1.length, classBytes1,
				class2, classBytes2.length, classBytes2);
		if (result == false) {
			return false;
		}
		
		Method rm1 = findMethod(class1, "meth1", new Class[0]);
		if (m1 == null) {
			return false;
		}
		Method rm2 = findMethod(class2, "meth1", new Class[0]);
		if (m2 == null) {
			return false;
		}
		
		int val1 = invokeMethod(m1, obj1);
		if (val1 != 102) {
			System.out.println("Return value of R1's meth1 on old instance was " + val1 + " (102 expected)");
			return false;
		}
		if (val1 != invokeMethod(rm1, obj1)) {
			return false;
		}

		int val2 = invokeMethod(m2, obj2);
		if (val2 != 149) {
			System.out.println("Return value of R2's meth1 on old instance was " + val2 + " (149 expected)");
			return false;
		}
		if (val2 != invokeMethod(rm2, obj2)) {
			return false;
		}

		rc007_testMultipleClassRedefines_O1 robj1 = new rc007_testMultipleClassRedefines_O1();
		rc007_testMultipleClassRedefines_O2 robj2 = new rc007_testMultipleClassRedefines_O2();
		
		val1 = invokeMethod(m1, robj1);
		if (val1 != 132) {
			System.out.println("Return value of R1's meth1 on new instance was " + val1 + " (132 expected)");
			System.out.println("It should come from the sum R2's f1=77 + R1's f2=55...");
			return false;
		}
		if (val1 != invokeMethod(rm1, robj1)) {
			return false;
		}

		val2 = invokeMethod(m2, robj2);
		if (val2 != 137) {
			System.out.println("Return value of R2's meth1 on new instance was " + val2 + " (137 expected)");
			System.out.println("It should come from the sum R1's f1=99 + R2's f2=38...");
			return false;
		}
		if (val2 != invokeMethod(rm2, robj2)) {
			return false;
		}
	
		return true;		
	}
	
	public String helpMultipleClassRedefinesToNewVersions()
	{
		return "Tests the simultaneous redefinition of several classes referencing each other to new versions.";
	}

	public boolean testMultipleRedefinesFromSameHierarchy()
	{
		Class original, redefined;
		byte classBytes1[];
		byte classBytes2[];

		rc007_MultipleRedefinesFromSameHierarchyD_O1 obj1 = new rc007_MultipleRedefinesFromSameHierarchyD_O1();

		if (obj1.getAnswer() != 42) {
			return false;
		}
		if (obj1.getPi() != 3.0) {
			return false;
		}
		if (obj1.getPrecision() != 4) {
			return false;
		}
		if (obj1.meth1() != 30000) {
			return false;
		}
		
		original = rc007_MultipleRedefinesFromSameHierarchyD_O1.class;
		redefined = rc007_MultipleRedefinesFromSameHierarchyD_R1.class;
		if ((classBytes1 = Util.loadRedefinedClassBytesWithOriginalClassName(original, redefined)) == null) {
			return false;
		}
		original = rc007_MultipleRedefinesFromSameHierarchyB_O1.class;
		redefined = rc007_MultipleRedefinesFromSameHierarchyB_R1.class;
		if ((classBytes2 = Util.loadRedefinedClassBytesWithOriginalClassName(original, redefined)) == null) {
			return false;
		}
		
		Class class1 = rc007_MultipleRedefinesFromSameHierarchyD_O1.class;
		Class class2 = rc007_MultipleRedefinesFromSameHierarchyB_O1.class;

		boolean result = redefineClasses(
				class1, classBytes1.length, classBytes1,
				class2, classBytes2.length, classBytes2);
		if (result == false) {
			return false;
		}

		if (obj1.getAnswer() != 0) {
			System.out.println("obj1.getAnswer() = " + obj1.getAnswer());
			return false;
		}
		if (obj1.getPi() != 3.0) {
			System.out.println("obj1.getPi() = " + obj1.getPi());
			return false;
		}
		if (obj1.getPrecision() != 8) {
			System.out.println("obj1.getPrecision() = " + obj1.getPrecision());
			return false;
		}
		if (obj1.meth1() != 30000) {
			System.out.println("obj1.meth1() = " + obj1.meth1());
			return false;
		}

		// make a new instance, and verify its values
		obj1 = new rc007_MultipleRedefinesFromSameHierarchyD_O1();
		
		if (obj1.getAnswer() != 0) {
			System.out.println("new instance obj1.getAnswer() = " + obj1.getAnswer());
			return false;
		}
		if (obj1.getPi() != Math.PI) {
			System.out.println("new instance obj1.getPi() = " + obj1.getPi());
			return false;
		}
		if (obj1.getPrecision() != 8) {
			System.out.println("new instance obj1.getPrecision() = " + obj1.getPrecision());
			return false;
		}
		if (obj1.meth1() != 31415) {
			System.out.println("new instance obj1.meth1() = " + obj1.meth1());
			return false;
		}

		return true;
	}
	
	public String helpMultipleRedefinesFromSameHierarchy()
	{
		return "Tests the simultaneous redefinition of several classes from the same inheritance hierarchy.";
	}
}
