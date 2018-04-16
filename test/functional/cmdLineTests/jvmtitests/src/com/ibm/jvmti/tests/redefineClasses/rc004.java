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

import com.ibm.jvmti.tests.util.Util;

public class rc004 {
	public static native boolean redefineClass(Class name, int classBytesSize, byte[] classBytes);
	public static native boolean redefineClassAndTestFieldIDs(Class name, int classBytesSize, byte[] classBytes);
	public static native boolean accessStoredIDs(Class name);

	public boolean setup(String args) {
		return true;
	}

	public boolean testStaticFieldIDsAfterRedefine() {
		byte classBytes[];
	
		Class originalClass = rc004_testStaticFieldIDsAfterRedefine_O1.class;
		Class redefinedClass = rc004_testStaticFieldIDsAfterRedefine_R1.class;
		
		if ((classBytes = Util.loadRedefinedClassBytesWithOriginalClassName(originalClass, redefinedClass)) == null) {
			return false;
		}

		boolean result = redefineClassAndTestFieldIDs(originalClass, classBytes.length, classBytes);
		if (result) {
			result = accessStoredIDs(originalClass);
		}

		return result;
	}

	public String helpStaticFieldIDsAfterRedefine() {
		return "Test whether a jfieldID of a static Field in a class remains valid after a class redefine."; 
	}

	public boolean testReOrderingStaticFields() {	
		if (rc004_testReOrderingStaticFields_O1.int1 != 123 ||
			rc004_testReOrderingStaticFields_O1.c != 30 ||
			!rc004_testReOrderingStaticFields_O1.f1.equals("abc") ||
			rc004_testReOrderingStaticFields_O1.foo != 3.0)
		{
			return false;
		}

		boolean redefined = Util.redefineClass(getClass(), rc004_testReOrderingStaticFields_O1.class, rc004_testReOrderingStaticFields_R1.class);
		if (!redefined) {
			return false;
		}

		if (rc004_testReOrderingStaticFields_O1.int1 != 123 ||
			rc004_testReOrderingStaticFields_O1.c != 30 ||
			!rc004_testReOrderingStaticFields_O1.f1.equals("abc") ||
			rc004_testReOrderingStaticFields_O1.foo != 3.0)
		{
			return false;
		}

		rc004_testReOrderingStaticFields_O1.int1 = 345;
		rc004_testReOrderingStaticFields_O1.c = 40;
		rc004_testReOrderingStaticFields_O1.f1 = "xyz";
		rc004_testReOrderingStaticFields_O1.foo = 1.0;

		if (rc004_testReOrderingStaticFields_O1.int1 != 345 ||
			rc004_testReOrderingStaticFields_O1.c != 40 ||
			!rc004_testReOrderingStaticFields_O1.f1.equals("xyz") ||
			rc004_testReOrderingStaticFields_O1.foo != 1.0)
		{
			return false;
		}

		redefined = Util.redefineClass(getClass(), rc004_testReOrderingStaticFields_O1.class, rc004_testReOrderingStaticFields_R1.class);
		if (!redefined) {
			return false;
		}

		if (rc004_testReOrderingStaticFields_O1.int1 != 345 ||
			rc004_testReOrderingStaticFields_O1.c != 40 ||
			!rc004_testReOrderingStaticFields_O1.f1.equals("xyz") ||
			rc004_testReOrderingStaticFields_O1.foo != 1.0)
		{
			return false;
		}
	
		return true;
	}

	public String helpReOrderingStaticFields() {
		return "Redefines a class with the same static fields in a different order (a J9 extension)."; 
	}
}
