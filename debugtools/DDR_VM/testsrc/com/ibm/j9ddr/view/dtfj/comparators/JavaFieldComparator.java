/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.view.dtfj.comparators;

import com.ibm.dtfj.java.JavaField;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class JavaFieldComparator extends DTFJComparator {

	public static final int DECLARED_CLASS = 1;
	public static final int MODIFIERS = 2;
	public static final int NAME = 4;
	public static final int SIGNATURE = 8;
	public static final int BASIC_CHECK = MODIFIERS | NAME | SIGNATURE;
	
	// getDeclaringClass()
	// getModifiers()
	// getName()
	// getSignature
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaField ddrJavaField = (JavaField) ddrObject;
		JavaField jextractJavaField = (JavaField) jextractObject;
		
		// getDeclaringClass()
		if ((DECLARED_CLASS & members) != 0)
			new JavaClassComparator().testComparatorEquals(ddrJavaField, jextractJavaField, "getDeclaringClass");
		
		// getModifiers()
		if ((MODIFIERS & members) != 0)
			testJavaEquals(ddrJavaField, jextractJavaField, "getModifiers");
		
		// getName()
		if ((NAME & members) != 0)
			testJavaEquals(ddrJavaField, jextractJavaField, "getName");
		
		// getSignature()
		if ((SIGNATURE & members) != 0)
			testJavaEquals(ddrJavaField, jextractJavaField, "getSignature");
	}
}
