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

public class rc005 
{
	public static native boolean redefineClass(Class name, int classBytesSize, byte[] classBytes);
	
	public boolean setup(String args)
	{
		return true;
	}

	// the sufficiently different method bodies and the fact that their order
	// has been changed cause various changes to the constant pool, but should
	// not stop redefineClasses from working
	public boolean testConstantPoolChanges()
	{
		rc005_testConstantPoolChanges_O1 t_pre = new rc005_testConstantPoolChanges_O1();
		if (t_pre.meth1() != 100) {
			return false;
		}
		if (t_pre.meth2() != 200) {
			return false;
		}
		if (rc005_testConstantPoolChanges_O1.meth3() != 300) {
			return false;
		}
		if (t_pre.meth4() != 42) {
			return false;
		}
		if (rc005_testConstantPoolChanges_O1.staticInt != 0xED) {
			return false;
		}
	
		// set a cookie
		t_pre.int1 = 0xCAFEBABE;

		boolean redefined = Util.redefineClass(getClass(), rc005_testConstantPoolChanges_O1.class, rc005_testConstantPoolChanges_R1.class);
		if (!redefined) {
			return false;
		}

		rc005_testConstantPoolChanges_O1 t_post = new rc005_testConstantPoolChanges_O1();

		// check the return values for methods on the new object
		if (t_post.meth1() != 123) {
			System.out.println("Post Replace meth1: " + t_pre.meth1());
			return false;
		}
		if (t_post.meth2() != 246) {
			System.out.println("Post Replace meth2: " + t_pre.meth2());
			return false;
		}
		if (rc005_testConstantPoolChanges_O1.meth3() != 0xED) {
			System.out.println("Post Replace meth3: " + rc005_testConstantPoolChanges_O1.meth3());
			return false;
		}
		if (t_post.meth4() != 42) {
			System.out.println("Post Replace meth4: " + t_pre.meth4());
			return false;
		}

		// check that int1 retained its old value as per the spec
		if (t_pre.int1 != 0xCAFEBABE) {
			return false;
		}
		// check that static initializers were not run as per the spec
		if (rc005_testConstantPoolChanges_O1.staticInt != 0xED) {
			return false;
		}
	
		return true;		
	}
	
	public String helpConstantPoolChanges()
	{
		return "Tests modifications to the constant pool.";
	}	
}
