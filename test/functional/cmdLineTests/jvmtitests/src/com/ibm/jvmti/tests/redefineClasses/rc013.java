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

import java.io.UnsupportedEncodingException;
import java.lang.reflect.Method;

import com.ibm.jvmti.tests.util.Util;

public class rc013 {
	public static native boolean redefineClass(Class name, int classBytesSize, byte[] classBytes);

	public boolean testRemovingInterfaceMethods() {
		rc013_testRemovingInterfaceMethods_O1 obj = new rc013_testRemovingInterfaceMethods_O2();

		if (!"bar".equals(obj.getBar())) {
			System.out.println("Failed 1: " + obj.getBar());
			return false;
		}

		// Redefine the interface through which we're calling the methods...
		boolean redefined = Util.redefineClass(getClass(), rc013_testRemovingInterfaceMethods_O1.class, rc013_testRemovingInterfaceMethods_R1.class);
		if (!redefined) {
			return false;
		}

		if (!"bar".equals(obj.getBar())) {
			System.out.println("Failed 2: " + obj.getBar());
			return false;
		}

		// Call the method that no longer exists on the interface - expect NoSuchMethodError.
		boolean gotNoSuchMethodError = false;
		try {
			obj.getFoo();
		} catch (NoSuchMethodError e) {
		
			gotNoSuchMethodError = true;
		}
		if (!gotNoSuchMethodError) {
			System.out.println("Didn't see expected NoSuchMethodError!");
			return false;
		}

		return true;
	}

	public String helpRemovingInterfaceMethods() {
		return "Test removing interface methods. (J9 extension)"; 
	}

	private String callViaReflection(Object obj, Class interfaceClass, String methodName) {
		String result = null;
		try {
			System.out.printf("Calling %s() via reflection.\n", methodName);
			Method fooMethod = interfaceClass.getMethod(methodName, new Class[0]);
			result = (String) fooMethod.invoke(obj, new Object[0]);
		} catch (Exception e) {
			System.out.println("Got exception:");
			e.printStackTrace();
		}
		return result;
	}

	public boolean testAddingInterfaceMethods() {
		Class<?> interfaceClass = rc013_testAddingInterfaceMethods_O1.class;
		rc013_testAddingInterfaceMethods_O1 obj = new rc013_testAddingInterfaceMethods_O2();
		rc013_testAddingInterfaceMethodsHelper helper = new rc013_testAddingInterfaceMethodsHelper(obj);

		// Make sure we can call getFoo() at the beginning.
		if (!"foo".equals(obj.getFoo())) {
			System.out.println("Failed 1: " + obj.getFoo());
			return false;
		}
		if (!"foo".equals(helper.callInterfaceMethod())) {
			System.out.println("Failed 2: " + helper.callInterfaceMethod());
			return false;
		}
		if (!"foo".equals(callViaReflection(obj, interfaceClass, "getFoo"))) {
			System.out.println("Failed 3: " + callViaReflection(obj, interfaceClass, "getFoo"));
			return false;
		}

		// Redefine the interface to add method getBar().
		System.out.println("Redefining rc011_testAddingInterfaceMethods_O1.");
		boolean redefined = Util.redefineClass(getClass(), rc013_testAddingInterfaceMethods_O1.class, rc013_testAddingInterfaceMethods_R1.class);
		if (!redefined) {
			return false;
		}

		// Make sure we can call still call getFoo().
		if (!"foo".equals(obj.getFoo())) {
			System.out.println("Failed 4: " + obj.getFoo());
			return false;
		}
		if (!"foo".equals(helper.callInterfaceMethod())) {
			System.out.println("Failed 5: " + helper.callInterfaceMethod());
			return false;
		}
		if (!"foo".equals(callViaReflection(obj, interfaceClass, "getFoo"))) {
			System.out.println("Failed 6: " + callViaReflection(obj, interfaceClass, "getFoo"));
			return false;
		}

		// Redefine helper so it will call getBar() via invokeinterface instead of getFoo.
		System.out.println("Redefining rc011_testAddingInterfaceMethodsHelper.");
		byte[] helperBytes = Util.getClassBytes(rc013_testAddingInterfaceMethodsHelper.class);
		try {
			helperBytes = Util.replaceAll(helperBytes, "getFoo".getBytes("UTF-8"), "getBar".getBytes("UTF-8"));
		} catch (UnsupportedEncodingException e) {
			e.printStackTrace();
			return false;
		}
		redefined = redefineClass(rc013_testAddingInterfaceMethodsHelper.class, helperBytes.length, helperBytes);
		if (!redefined) {
			return false;
		}

		// Make sure we can now call getBar() via both invokeinterface using the helper and via reflection.
		if (!"bar".equals(helper.callInterfaceMethod())) {
			System.out.println("Failed 7: " + helper.callInterfaceMethod());
			return false;
		}
		if (!"bar".equals(callViaReflection(obj, interfaceClass, "getBar"))) {
			System.out.println("Failed 8: " + callViaReflection(obj, interfaceClass, "getBar"));
			return false;
		}

		return true;
	}

	public String helpAddingInterfaceMethods() {
		return "Test adding an interface method and calling it. (J9 extension)"; 
	}
}
