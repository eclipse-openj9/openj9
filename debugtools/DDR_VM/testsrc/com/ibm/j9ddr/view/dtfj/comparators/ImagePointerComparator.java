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

import com.ibm.dtfj.image.ImagePointer;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class ImagePointerComparator extends DTFJComparator {

	public static final int ADDRESS = 1;
	public static final int ADDRESS_SPACE = 2;
	public static final int IS_EXECUTABLE = 4;
	public static final int IS_READ_ONLY = 8;
	public static final int IS_SHARED = 16;
	
	// address
	// addressSpace
	// isExecutable
	// isReadOnly
	// isShared
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		
		ImagePointer ddrImagePointer = (ImagePointer) ddrObject;
		ImagePointer jextractImagePointer = (ImagePointer) jextractObject;

		// getAddress()
		if ((members & ADDRESS) != 0)
			testJavaEquals(ddrImagePointer, jextractImagePointer, "getAddress");
		
		// getAddressSpace() (but not deeply to avoid circular reference)
		if ((members & ADDRESS_SPACE) != 0)
			new ImageAddressSpaceComparator().testComparatorEquals(ddrImagePointer, jextractImagePointer, "getAddressSpace");
		
		// isExecutable
		if ((members & IS_EXECUTABLE) != 0)
			testJavaEquals(ddrImagePointer, jextractImagePointer, "isExecutable");
		
		// isShared
		if ((members & IS_SHARED) != 0)
			testJavaEquals(ddrImagePointer, jextractImagePointer, "isShared");

		// isReadOnly
		if ((members & IS_READ_ONLY) != 0)
			testJavaEquals(ddrImagePointer, jextractImagePointer, "isReadOnly");
	}

	@Override
	public int getDefaultMask() {
		return ADDRESS | ADDRESS_SPACE;
	}
}
