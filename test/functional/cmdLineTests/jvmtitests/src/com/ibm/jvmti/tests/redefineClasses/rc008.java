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


public class rc008
{
	public static native boolean redefineClass(Class name, int classBytesSize, byte[] classBytes);

	public boolean setup(String args)
	{
		return true;
	}

	public boolean testUpdatesToInstanceFields()
	{
		rc008_testUpdatesToInstanceFields_O1 t_pre = new rc008_testUpdatesToInstanceFields_O1();
		
		if (t_pre.f != 105 || t_pre.b != 5 || t_pre.c != 0x20) {
			return false;
		}

		boolean redefined = Util.redefineClass(getClass(), rc008_testUpdatesToInstanceFields_O1.class, rc008_testUpdatesToInstanceFields_R1.class);
		if (!redefined) {
			return false;
		}

		if (t_pre.f != 105 || t_pre.b != 5 || t_pre.c != 0x20) {
			return false;
		}
	
		rc008_testUpdatesToInstanceFields_O1 t_post = new rc008_testUpdatesToInstanceFields_O1();

		if (t_post.f != -105 || t_post.b != -5 || t_post.c != 'a') {
			return false;
		}

		// now redefine it back to the original version
		redefined = Util.redefineClass(getClass(), rc008_testUpdatesToInstanceFields_O1.class, rc008_testUpdatesToInstanceFields_O1.class);
		if (!redefined) {
			return false;
		}

		rc008_testUpdatesToInstanceFields_O1 t_post2 = new rc008_testUpdatesToInstanceFields_O1();

		if (t_post2.f != 105 || t_post2.b != 5 || t_post2.c != 0x20) {
			return false;
		}
		
		return true;
	}

	public String helpUpdatesToInstanceFields()
	{
		return "Test updates to instance fields."; 
	}

	public boolean testConstructorUpdates()
	{
		rc008_testConstructorUpdates_O1 t_pre = new rc008_testConstructorUpdates_O1();
		
		if (t_pre.f1 != 24 || t_pre.b != 42) {
			return false;
		}

		boolean redefined = Util.redefineClass(getClass(), rc008_testConstructorUpdates_O1.class, rc008_testConstructorUpdates_R1.class);
		if (!redefined) {
			return false;
		}

		if (t_pre.f1 != 24 || t_pre.b != 42) {
			return false;
		}
	
		rc008_testConstructorUpdates_O1 t_post = new rc008_testConstructorUpdates_O1();

		if (t_post.f1 != 124 || t_post.b != 1) {
			return false;
		}
		
		// now redefine it back to the original version
		redefined = Util.redefineClass(getClass(), rc008_testConstructorUpdates_O1.class, rc008_testConstructorUpdates_O1.class);
		if (!redefined) {
			return false;
		}

		rc008_testConstructorUpdates_O1 t_post2 = new rc008_testConstructorUpdates_O1();

		if (t_post2.f1 != 24 || t_post2.b != 42) {
			return false;
		}

		return true;
	}

	public String helpConstructorUpdates()
	{
		return "Test updates to constructors."; 
	}

	public boolean testRedefineNewInstance() throws IllegalAccessException, InstantiationException {
		String result = rc008_testRedefineNewInstance_O1.class.newInstance().getValue();
		if (!result.equals("before")) {
			return false;
		}

		boolean redefined = Util.redefineClass(getClass(), rc008_testRedefineNewInstance_O1.class, rc008_testRedefineNewInstance_R1.class);
		if (!redefined) {
			return false;
		}

		result = rc008_testRedefineNewInstance_O1.class.newInstance().getValue();
		if (!result.equals("after")) {
			return false;
		}

		// now redefine it back to the original version
		redefined = Util.redefineClass(getClass(), rc008_testRedefineNewInstance_O1.class, rc008_testRedefineNewInstance_O1.class);
		if (!redefined) {
			return false;
		}

		result = rc008_testRedefineNewInstance_O1.class.newInstance().getValue();
		if (!result.equals("before")) {
			return false;
		}

		return true;
	}

	public String helpRedefineNewInstance() {
		return "Test that redefining default constructors works correclty for Class.newInstance()";
	}
}
