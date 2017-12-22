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

import com.ibm.dtfj.image.ImageSection;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class ImageSectionComparator extends DTFJComparator {

	public static final int BASE_ADDRESS = 1;
	public static final int NAME = 2;
	public static final int SIZE = 4;
	public static final int IS_EXECUTABLE = 8;
	public static final int IS_READONLY = 16;
	public static final int IS_SHARED = 32;
	
	// getBaseAddress()
	// getName()
	// getSize()
	// isExecutable()
	// isReadOnly()
	// isShared()
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		ImageSection ddrImageSection = (ImageSection) ddrObject;
		ImageSection jextractImageSection = (ImageSection) jextractObject;
		
		// getBaseAddress
		if ((BASE_ADDRESS & members) != 0) 
			new ImagePointerComparator().testComparatorEquals(ddrImageSection, jextractImageSection, "getBaseAddress");

		// getName()
		if ((NAME & members) != 0) 
			testJavaEquals(ddrImageSection, jextractImageSection, "getName");
		
		// getSize()
		if ((SIZE & members) != 0) 
			testJavaEquals(ddrImageSection, jextractImageSection, "getSize");

		// isExecutable()	
		if ((IS_EXECUTABLE & members) != 0) 
			testJavaEquals(ddrImageSection, jextractImageSection, "isExecutable");
		
		// isShared()
		if ((IS_SHARED  & members) != 0) 
			testJavaEquals(ddrImageSection, jextractImageSection, "isShared");
		
		// isReadOnly()
		if ((IS_READONLY & members) != 0) 
			testJavaEquals(ddrImageSection, jextractImageSection, "isReadOnly");
	}

	@Override
	public int getDefaultMask() {
		return ~(IS_SHARED | IS_READONLY | IS_EXECUTABLE);
	}
}
