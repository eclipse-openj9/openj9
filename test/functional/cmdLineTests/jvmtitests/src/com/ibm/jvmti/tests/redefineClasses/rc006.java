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

public class rc006
{
	public static native boolean redefineClassAndTestMethodIDs(Class name, Object obj, int classBytesSize, byte[] classBytes);
	public static native boolean accessStoredIDs(Class name, Object obj);

	public boolean setup(String args)
	{
		return true;
	}

	public boolean testStaticMethodIDsAfterRedefine()
	{
		byte classBytes[];
	
		Class originalClass = rc006_testMethodIDsAfterRedefine_O1.class;
		Class redefinedClass = rc006_testMethodIDsAfterRedefine_R1.class;
		
		if ((classBytes = Util.loadRedefinedClassBytesWithOriginalClassName(originalClass, redefinedClass)) == null) {
			return false;
		}

		rc006_testMethodIDsAfterRedefine_O1 obj = new rc006_testMethodIDsAfterRedefine_O1();
		boolean result = redefineClassAndTestMethodIDs(originalClass, obj, classBytes.length, classBytes);
		if (result) {
			obj = new rc006_testMethodIDsAfterRedefine_O1();
			result = accessStoredIDs(originalClass, obj);
		}

		return result;
	}

	public String helpStaticMethodIDsAfterRedefine()
	{
		return "Test whether a jmethodID of a method in a class remains valid after a class redefine."; 
	}
}
