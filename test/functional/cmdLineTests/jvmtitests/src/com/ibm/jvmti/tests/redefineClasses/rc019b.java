/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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
import java.lang.reflect.*;

public class rc019b {
	public static native boolean redefineClass(Class name, int classBytesSize,
			byte[] classBytes);
	public static native String getValue(Object o);

	public boolean testReflectAfterRedefine() {
		try {
			rc019_Super o = new rc019_Sub();
			System.out.println("Before redefine:");
			String direct = o.getValue();
			System.out.println("	Direct getValue() = " + direct);
			Util.redefineClass(getClass(), rc019_Super.class, rc019_Super.class);
			System.out.println("After redefine:");
			String afterDirect = o.getValue();
			String afterJNI = getValue(o);
			System.out.println("	Direct getValue() = " + afterDirect);
			System.out.println("	JNI    getValue() = " + afterJNI);
			if (afterDirect != afterJNI) {
				System.out.println("FAIL: values do not match");
				return false;
			}
			if (direct != afterJNI) {
				System.out.println("FAIL: before and after values do not match");
				return false;			
			}
			return true;
		} catch(Throwable t) {
			System.out.println("Exception during test:");
			t.printStackTrace();
			return false;
		}
	}

	public String helpReflectAfterRedefine() {
		return "Tests that reflect invoke works when used after class redefinition";
	}
}
