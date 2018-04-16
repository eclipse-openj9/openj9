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

public class rc003
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

	// careful: choose float values that can be safely compared using == (as floats can have errors in representation)
	private static boolean checkFieldFloatValue(Field field, float value, String fieldName) {
		try {
			float fieldValue = field.getFloat(null);
			if (fieldValue != value) {
				System.out.println("Static field " + fieldName + " value is not " + value);
				return false;
			}
		} catch (Exception e) {
			e.printStackTrace();
			return false;
		}
		return true;
	}
	
	//****************************************************************************************
	// 
	// 

	public boolean testAddField()
	{
		rc003_testAddField_O1 t_pre = new rc003_testAddField_O1();

		// original version has f1 but no f2
		if (getField(t_pre.getClass(), "f1") == null) {
			return false;
		}
		if (!noSuchField(t_pre.getClass(), "f2")) {
			return false;
		}

		boolean redefined = Util.redefineClass(getClass(), rc003_testAddField_O1.class, rc003_testAddField_R1.class);
		if (!redefined) {
			return false;
		}

		// redefined version has f1 and f2
		if (getField(t_pre.getClass(), "f1") == null) {
			return false;
		}
		if (getField(t_pre.getClass(), "f2") == null) {
			return false;
		}

		return true;
	}

	public String helpAddField()
	{
		return "Exercise the extended redefine by adding a new Field. Note that this will fail on a SUN vm."; 
	}

	public boolean testRemoveField()
	{
		rc003_testRemoveField_O1 t_pre = new rc003_testRemoveField_O1();

		// original version has f1 and f2
		if (getField(t_pre.getClass(), "f1") == null) {
			System.out.println("Field f1 not found in original class");
			return false;
		}
		if (getField(t_pre.getClass(), "f2") == null) {
			System.out.println("Field f2 not found in original class");
			return false;
		}

		boolean redefined = Util.redefineClass(getClass(), rc003_testRemoveField_O1.class, rc003_testRemoveField_R1.class);
		if (!redefined) {
			return false;
		}

		// redefined version has f1 but no f2
		if (getField(t_pre.getClass(), "f1") == null) {
			System.out.println("Field f1 not found in redefined class");
			return false;
		}
		if (!noSuchField(t_pre.getClass(), "f2")) {
			System.out.println("Field f2 exists (but shouldn't) in redefined class");
			return false;
		}

		return true;
	}

	public String helpRemoveField()
	{
		return "Exercise the extended redefine by removing a Field. Note that this will fail on a SUN vm."; 
	}

	public boolean testFieldPersistence()
	{
		rc003_testFieldPersistence_O1 t_pre = new rc003_testFieldPersistence_O1();
		
		Field field = getField(t_pre.getClass(), "f1");
		if (field == null) {
			System.out.println("Original static field f1 is null");
			return false;
		}

		if (!checkFieldFloatValue(field, 1f, "f1")) {
			return false;
		}

		boolean redefined = Util.redefineClass(getClass(), rc003_testFieldPersistence_O1.class, rc003_testFieldPersistence_R1.class);
		if (!redefined) {
			return false;
		}

		Field field2 = getField(t_pre.getClass(), "f1");
		if (field2 == null) {
			System.out.println("Redefined R1 static field f1 is null");
			return false;
		}
		
		// check that both field references work as they should
		if (!checkFieldFloatValue(field, 1f, "f1")) {
			return false;
		}
		
		if (!checkFieldFloatValue(field2, 1f, "f1")) {
			return false;
		}

		redefined = Util.redefineClass(getClass(), rc003_testFieldPersistence_O1.class, rc003_testFieldPersistence_R2.class);
		if (!redefined) {
			return false;
		}

		Field field3 = getField(t_pre.getClass(), "f1");
		if (field3 == null) {
			System.out.println("Redefined R2 static field f1 is null");
			return false;
		}

		// check that all three field references work as they should
		if (!checkFieldFloatValue(field, 1f, "f1")) {
			return false;
		}
		
		if (!checkFieldFloatValue(field2, 1f, "f1")) {
			return false;
		}

		if (!checkFieldFloatValue(field3, 1f, "f1")) {
			return false;
		}

		// set the value on the most recent reference
		try {
			field3.setFloat(null, 2f);
		} catch (Exception e) {
			e.printStackTrace();
			return false;
		}

		// check if all references are updated correctly
		if (!checkFieldFloatValue(field, 2f, "f1")) {
			return false;
		}
		
		if (!checkFieldFloatValue(field2, 2f, "f1")) {
			return false;
		}

		if (!checkFieldFloatValue(field3, 2f, "f1")) {
			return false;
		}

		// set the value on the oldest reference
		try {
			field3.setFloat(null, 3f);
		} catch (Exception e) {
			e.printStackTrace();
			return false;
		}

		// check if all references are updated correctly
		if (!checkFieldFloatValue(field, 3f, "f1")) {
			return false;
		}
		
		if (!checkFieldFloatValue(field2, 3f, "f1")) {
			return false;
		}

		if (!checkFieldFloatValue(field3, 3f, "f1")) {
			return false;
		}
		
		// set the value on middle reference
		try {
			field3.setFloat(null, 5f);
		} catch (Exception e) {
			e.printStackTrace();
			return false;
		}

		// check if all references are updated correctly
		if (!checkFieldFloatValue(field, 5f, "f1")) {
			return false;
		}
		
		if (!checkFieldFloatValue(field2, 5f, "f1")) {
			return false;
		}

		if (!checkFieldFloatValue(field3, 5f, "f1")) {
			return false;
		}
		
		return true;
	}
	
	public String helpFieldPersistence()
	{
		return "Test whether a Field exists and is the same, after one and two redefines."; 
	}
}
