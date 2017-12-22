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

import com.ibm.dtfj.java.JavaLocation;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class JavaLocationComparator extends DTFJComparator {

	public static final int ADDRESS = 1;
	public static final int COMPILATION_LEVEL = 2;
	public static final int FILE_NAME = 4;
	public static final int LINE_NUMBER = 8;
	public static final int METHOD = 16;
	
	// getAddress()
	// getCompilationLevel()
	// getFileName()
	// getLineNumber()
	// getMethod()

	
	
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaLocation ddrJavaLocation = (JavaLocation) ddrObject;
		JavaLocation jextractJavaLocation = (JavaLocation) jextractObject;
	
		// getAddress()
		if ((ADDRESS & members) != 0)
		new ImagePointerComparator().testComparatorEquals(ddrJavaLocation, jextractJavaLocation, "getAddress");
		
		// getCompilationLevel()
		if ((COMPILATION_LEVEL & members) != 0)
			testJavaEquals(ddrJavaLocation, jextractJavaLocation, "getCompilationLevel");
		
		// getFileName()
		if ((FILE_NAME & members) != 0)
			testJavaEquals(ddrJavaLocation, jextractJavaLocation, "getFilename");
		
		// getLineNumber
		if ((LINE_NUMBER & members) != 0)
			testJavaEquals(ddrJavaLocation, jextractJavaLocation, "getLineNumber");

		// getMethod
		if ((METHOD & members) != 0)
			new JavaMethodComparator().testComparatorEquals(ddrJavaLocation, jextractJavaLocation, "getMethod");
	}

	@Override
	public int getDefaultMask()
	{
		/* Default mask doesn't include compilation level. DTFJ/JExtract gives the wrong answer */
		return ADDRESS | FILE_NAME | LINE_NUMBER | METHOD;
	}
}
