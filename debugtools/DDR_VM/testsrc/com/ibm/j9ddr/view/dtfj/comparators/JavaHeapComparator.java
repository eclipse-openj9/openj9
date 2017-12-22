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
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class JavaHeapComparator extends DTFJComparator {

	public static final int NAME = 1;
	public static final int OBJECTS = 2;
	public static final int SECTIONS = 4;
	
	// getName()
	// getObjects()
	// getSections()
	
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaHeap ddrjJavaHeap = (JavaHeap) ddrObject;
		JavaHeap jextractJavaHeap = (JavaHeap) jextractObject;
		
		// getName()
		if ((NAME & members) != 0)
			testJavaEquals(ddrjJavaHeap, jextractJavaHeap, "getName");
		
		// getObjects()
		if ((OBJECTS & members) != 0)
			new JavaObjectComparator().testComparatorIteratorEquals(ddrjJavaHeap, jextractJavaHeap, "getObjects", JavaObject.class);
		
		// getSections
		if ((SECTIONS & members) != 0)
			new ImageSectionComparator().testComparatorIteratorEquals(ddrjJavaHeap, jextractJavaHeap, "getSections", ImageSection.class);
	}
	
	public int getDefaultMask() {
		return NAME | SECTIONS;
	}

}
