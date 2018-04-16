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

import java.lang.reflect.Field;

import com.ibm.jvmti.tests.util.Util;


public class rc010
{
	public static native boolean redefineClass(Class name, int classBytesSize, byte[] classBytes);

	public boolean setup(String args)
	{
		return true;
	}

	private static Field getField(Class klass, String fieldName)
	{
		try {
			Field field = klass.getField(fieldName);
			return field;
		} catch (Exception e) {
			e.printStackTrace();
		}
		return null;
	}
	
	// rather than doing (getField()==null), this specifically checks for a NoSuchFieldException
	private static boolean noSuchField(Class klass, String fieldName)
	{
		try {
			klass.getField(fieldName);
		} catch (NoSuchFieldException e) {
			return true;
		} catch (Exception e) {
		}
		return false;
	}
	
	public boolean testRedefineEnums()
	{
		Class originalClass = rc010_testRedefineEnums_O1.class;

		if (!rc010_testRedefineEnums_O1.class.isEnum()) {
			return false;
		}

		boolean redefined = Util.redefineClass(getClass(), rc010_testRedefineEnums_O1.class, rc010_testRedefineEnums_R1.class);
		if (!redefined) {
			return false;
		}

		if (!rc010_testRedefineEnums_O1.class.isEnum()) {
			return false;
		}

		if (!originalClass.equals(rc010_testRedefineEnums_O1.class)) {
			return false;
		}

		return true;
	}

	public String helpRedefineEnums()
	{
		return "Test redefinition of enums."; 
	}
	
	public boolean testAddEnums()
	{
		byte classBytes[];

		Class originalClass = rc010_testRedefineEnums_O1.class;
		Class redefinedClass = rc010_testRedefineEnums_R2.class;
		
		if (!rc010_testRedefineEnums_O1.class.isEnum()) {
			return false;
		}

		if (!noSuchField(originalClass, "TYPE_D")) {
			return false;
		}
		
		if ((classBytes = Util.loadRedefinedClassBytesWithOriginalClassName(originalClass, redefinedClass)) == null) {
			return false;
		}
		
		boolean redefined = redefineClass(rc010_testRedefineEnums_O1.class, classBytes.length, classBytes);
		if (!redefined) {
			return false;
		}

		if (!rc010_testRedefineEnums_O1.class.isEnum()) {
			return false;
		}
		
		Field f = getField(rc010_testRedefineEnums_O1.class, "TYPE_D");
		if (f == null || !f.isEnumConstant()) {
			return false;
		}

		if (!originalClass.equals(rc010_testRedefineEnums_O1.class)) {
			return false;
		}
		
		return true;
	}

	public String helpAddEnums()
	{
		return "Test addition of enums in a redefine. This will fail on a SUN VM."; 
	}
}
