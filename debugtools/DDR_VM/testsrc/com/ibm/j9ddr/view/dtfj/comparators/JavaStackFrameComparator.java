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

import java.util.Comparator;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.dtfj.java.JavaStackFrame;
import com.ibm.j9ddr.corereaders.memory.Addresses;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class JavaStackFrameComparator extends DTFJComparator {

	public static final int BASE_POINTER = 1;
	public static final int HEAP_ROOTS = 2;
	public static final int LOCATION = 4;
	
	// getBasePointer()
	// getHeapRoots()
	// getLocation()
	
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaStackFrame ddrJavaStackFrame = (JavaStackFrame) ddrObject;
		JavaStackFrame jextractJavaStackFrame = (JavaStackFrame) jextractObject;
		
		// getBasePointer()
		if ((BASE_POINTER & members) != 0)
			new ImagePointerComparator().testComparatorEquals(ddrJavaStackFrame, jextractJavaStackFrame, "getBasePointer");
		
		// getHeapRoots()

		if ((HEAP_ROOTS & members) != 0) {
			new JavaReferenceComparator().testComparatorIteratorEquals(ddrJavaStackFrame, jextractJavaStackFrame, "getHeapRoots", JavaReference.class, referenceComparator, JavaReferenceComparator.ALL_MEMBERS);
		}

		// getLocation()
		if ((LOCATION & members) != 0)
			new JavaLocationComparator().testComparatorEquals(ddrJavaStackFrame, jextractJavaStackFrame, "getLocation");
	}

	private static Comparator<Object> referenceComparator = new Comparator<Object>(){

		public int compare(Object o1, Object o2)
		{
			if (o1 instanceof JavaReference && o2 instanceof JavaReference) {
				JavaReference ref1 = (JavaReference)o1;
				JavaReference ref2 = (JavaReference)o2;
				
				return compareReferences(ref1, ref2);
			} else {
				if (o1 instanceof JavaReference) {
					return 1;
				} else if (o2 instanceof JavaReference) {
					return -1;
				} else {
					//They are both CorruptData
					return 0;
				}
			}
		}

		private int compareReferences(JavaReference ref1, JavaReference ref2)
		{
			Object t1 = null;
			Object t2 = null;
			
			try {
				t1 = ref1.getTarget();
			} catch (Exception e) {
				//Do nothing
			}
			
			try {
				t2 = ref2.getTarget();
			} catch (Exception e) {
				//Do nothing
			}
			
			if (t1 == null || t2 == null) {
				if (t1 == null && t2 == null) {
					return 0;
				} else if (t1 == null) {
					return -1;
				} else {
					return 1;
				}
			}
			
			if (t1 instanceof CorruptData || t2 instanceof CorruptData) {
				if (t1 instanceof CorruptData && t2 instanceof CorruptData) {
					return 0;
				} else if (t1 instanceof CorruptData) {
					return -1;
				} else {
					return 1;
				}
			} else if (hasAddress(t1) || hasAddress(t2)) {
				if (hasAddress(t1) && hasAddress(t2)) {
					long addr1 = getAddress(t1);
					long addr2 = getAddress(t2);
					
					if (Addresses.greaterThan(addr1, addr2)) {
						return 1;
					} else if (addr1 == addr2) {
						return 0;
					} else {
						return -1;
					}
				} else if (hasAddress(t1)) {
					return 1;
				} else {
					return -1;
				}
			} else {
				throw new IllegalArgumentException("Unexpected comparison class (not JavaObject or JavaClass). o1 isa " + t1.getClass().getName() + ", o2 isa " + t2.getClass().getName());
			}
		}

		private boolean hasAddress(Object o)
		{
			return o instanceof JavaObject || o instanceof JavaClass;
		}
		
		private long getAddress(Object o)
		{
			if (o instanceof JavaObject) {
				JavaObject obj = (JavaObject)o;
				return obj.getID().getAddress();
			} else if (o instanceof JavaClass) {
				JavaClass clazz = (JavaClass) o;
				return clazz.getID().getAddress();
			} else {
				throw new IllegalArgumentException("Unexpected type " + o.getClass().getName());
			}
		}
	};
}
