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

import com.ibm.dtfj.java.JavaVMInitArgs;
import com.ibm.dtfj.java.JavaVMOption;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class JavaVMInitArgsComparator extends DTFJComparator {

	public static final int IGNORE_UNRECOGNIZED = 1;
	public static final int OPTIONS = 2;
	public static final int VERSION = 4;
	
	// getIgnoreUnrecognized
	// getOptions
	// getVersion
	
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaVMInitArgs ddrArgs = (JavaVMInitArgs) ddrObject;
		JavaVMInitArgs jextractArgs = (JavaVMInitArgs) jextractObject;
		
		// getIgnoreUnrecognized
		if ((IGNORE_UNRECOGNIZED & members) != 0)
			testJavaEquals(ddrArgs, jextractArgs, "getIgnoreUnrecognized");
		
		// getOptions
		if ((OPTIONS & members) != 0)
			new JavaVMOptionComparator().testComparatorIteratorEquals(ddrArgs, jextractArgs, "getOptions", JavaVMOption.class);
		
		// getVersion
		if ((VERSION & members) != 0)
			testJavaEquals(ddrArgs, jextractArgs, "getVersion");
	}

}
