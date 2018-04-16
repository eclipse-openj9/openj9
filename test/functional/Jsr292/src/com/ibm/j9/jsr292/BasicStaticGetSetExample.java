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
package com.ibm.j9.jsr292;

/**
 * All the fields, constructors and methods in this class are used by the test case that validates
 * basic static set/get functionality ( test_GetterSetter_BasicGet and  test_GetterSetter_BasicSet 
 * in LookupAPITests_Find)
 */
public class BasicStaticGetSetExample {

	
	/* read through reflection to trigger static initialization */
	public static int intField;
	public static long longField;
	public static float floatField;
	public static double doubleField;
	public static byte byteField;
	public static short shortField;
	public static boolean booleanField;
	public static char charField;
	public static Object objectField;
	
	static {
		intField = 100;
		longField = 100;
		floatField = (float) 1.23456;
		doubleField = 1.23456;
		byteField = 100;
		shortField = 100;
		booleanField = true;
		charField = 100;
		objectField = new Object();
	}
	
}
