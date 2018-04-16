/*******************************************************************************
 * Copyright (c) 2013, 2018 IBM Corp. and others
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

public class rc016 {
	public static native boolean redefineClass(Class name, int classBytesSize, byte[] classBytes);

	public boolean testReflectMethodIDRedefineExtended()	{
		Class noparams[] = {};
		try {
			Method m = rc015_testReflectMethodIDRedefine_Super_1.class.getDeclaredMethod("m", noparams);

			rc015_testReflectMethodIDRedefine_Super_1 sub = new rc015_testReflectMethodIDRedefine_Sub();

			Boolean pre = (Boolean)m.invoke(sub);

			if (!pre.booleanValue()) {
				System.out.println("rc015_testReflectMethodIDRedefine_Super.m() returned true!! something is really wrong");
				return false;
			}

			boolean redefined = Util.redefineClass(getClass(), rc015_testReflectMethodIDRedefine_Super_1.class, rc015_testReflectMethodIDRedefine_Super_2.class);

			if (!redefined) {
				System.out.println("Please verify HCR extended mode is enabled");
				return false;
			}

			Boolean post = (Boolean)m.invoke(sub);
			if (!post.booleanValue()) {
				System.out.println("JNI Method ID is corrupted by fixJNIMethodID during fastHCR class redefine. See CMVC 198908");
				return false;
			}

			return true;

		} catch (Exception e) {
			System.out.println("rc016 encountered unexpected exception "+ e.getMessage());
			return false;
		}
	}

	public String helpReflectMethodIDRedefineExtended() {
		return "Tests whether invocation of reflected method is virtually dispatched after redfining the class in exteded mode";
	}



}
