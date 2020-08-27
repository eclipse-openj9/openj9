/*******************************************************************************
 * Copyright (c) 2020, 2020 IBM Corp. and others
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
package com.ibm.jvmti.tests.retransformClasses;

import com.ibm.jvmti.tests.util.Util;

public class rtc002 {
	public static native boolean retransformClass(Class clazz, String name, byte[] classBytes);
	
	public boolean setup(String args) {
		return true;
	}
	
	public boolean testRedefineWithRuntimeVisibleAnnotations()	{
		int pre, post;

		rtc002_runtimeTypeVisibleAnnotationsA t_pre = new rtc002_runtimeTypeVisibleAnnotationsA();
		pre = t_pre.meth1();

		byte[] transformedClassBytes = Util.replaceClassNames(Util.getClassBytes(rtc002_runtimeTypeVisibleAnnotationsB.class),  rtc002_runtimeTypeVisibleAnnotationsA.class.getSimpleName(), rtc002_runtimeTypeVisibleAnnotationsB.class.getSimpleName());

		boolean redefined = retransformClass(rtc002_runtimeTypeVisibleAnnotationsA.class, rtc002_runtimeTypeVisibleAnnotationsA.class.getName().replace('.', '/'), transformedClassBytes);
		if (!redefined) {
			return false;
		}

		rtc002_runtimeTypeVisibleAnnotationsA t_post = new rtc002_runtimeTypeVisibleAnnotationsA();
		post = t_post.meth1();
		
		System.out.println("Pre  Replace: " + pre);
		System.out.println("Post Replace: " + post);
		
		if (pre != 0) {
			return false;
		}
		
		if (post != 1) {
			return false;
		}
		
		return true;
	}
	
	public String helpRedefineWithRuntimeVisibleAnnotations() {
		return "Tests class redefinition where the original class has a runtime visible code type annotation and the redefined one does not.";
	}
}
