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

import com.ibm.dtfj.java.JavaReference;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class JavaReferenceComparator extends DTFJComparator {

	public static final int DESCRIPTION = 1;
	public static final int REACHABILITY = 2;
	public static final int REFERENCE_TYPE = 4;
	public static final int ROOT_TYPE = 8;
	public static final int SOURCE = 16;
	public static final int TARGET = 32;
	public static final int IS_CLASS_REFERENCE = 64;
	public static final int IS_OBJECT_REFERENCE = 128;
	
	private JavaClassComparator javaClassComparator = new JavaClassComparator();
	private JavaObjectComparator javaObjectComparator = new JavaObjectComparator();
	private JavaStackFrameComparator javaStackFrameComparator = new JavaStackFrameComparator();

	// getDescription
	// getReachability
	// getReferenceType
	// getRootType
	// getSource
	// getTarget
	// isClassReference
	// isObjectReference
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaReference ddrJavaReference = (JavaReference) ddrObject;
		JavaReference jextractJavaReference = (JavaReference) jextractObject;
		
		// getDescription
		if ((DESCRIPTION & members) != 0)
			testJavaEquals(ddrJavaReference, jextractJavaReference, "getDescription");
		
		// getReachability
		if ((REACHABILITY & members) != 0)
			testJavaEquals(ddrJavaReference, jextractJavaReference, "getReachability");

		// getReferenceType
		if ((REFERENCE_TYPE & members) != 0)
			testJavaEquals(ddrJavaReference, jextractJavaReference, "getReferenceType");
		
		// getRootType
		if ((ROOT_TYPE & members) != 0)
			testJavaEquals(ddrJavaReference, jextractJavaReference, "getRootType");
		
		// getSource
		if ((SOURCE & members) != 0) {
			try {
				javaClassComparator.testComparatorEquals(ddrObject, jextractObject, "getSource");
				return;
			} catch (ClassCastException e) {
				// Do nothing.
			}

			// We only get here if the target was NOT a JavaClass
			try {
				javaObjectComparator.testComparatorEquals(ddrObject, jextractObject, "getSource");
				return;
			} catch (ClassCastException e) {
				// Do nothing
			}
			
			// We only get here if the target was NOT a JavaClass or a JavaObject
			javaStackFrameComparator.testComparatorEquals(ddrObject, jextractObject, "getSource", JavaStackFrameComparator.BASE_POINTER + JavaStackFrameComparator.LOCATION);
		}
		
		// getTarget
		if ((TARGET & members) != 0) {
			try {
				javaClassComparator.testComparatorEquals(ddrObject, jextractObject, "getTarget");
				return;
			} catch (ClassCastException e) {
				// Do nothing.
			}

			// We only get here if the target was NOT a JavaClass
			javaObjectComparator.testComparatorEquals(ddrObject, jextractObject, "getTarget");
		}
			
		// isClassReference
		if ((IS_CLASS_REFERENCE & members) != 0)
			testJavaEquals(ddrJavaReference, jextractJavaReference, "isClassReference");
		
		// isObjectReference
		if ((IS_OBJECT_REFERENCE & members) != 0)
			testJavaEquals(ddrJavaReference, jextractJavaReference, "isObjectReference");
	}

}
