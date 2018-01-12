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

import com.ibm.dtfj.java.JavaVMOption;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class JavaVMOptionComparator extends DTFJComparator {

	public static final int EXTRA_INFO = 1;
	public static final int OPTION_STRING = 2;
	
	// getExtraInfo
	// getOptionString
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaVMOption ddrVMOption = (JavaVMOption) ddrObject;
		JavaVMOption jextractVMOption = (JavaVMOption) jextractObject;
		
		// getExtraInfo
		if ((EXTRA_INFO & members) != 0)
			new ImagePointerComparator().testComparatorEquals(ddrVMOption, jextractVMOption, "getExtraInfo");
		
		// getOptionString
		if ((OPTION_STRING & members) != 0)
			testJavaEquals(ddrVMOption, jextractVMOption, "getOptionString");
		
	}

}
