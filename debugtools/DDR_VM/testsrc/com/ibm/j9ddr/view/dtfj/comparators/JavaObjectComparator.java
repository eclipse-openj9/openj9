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

import static org.junit.Assert.*;

import java.util.List;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;

public class JavaObjectComparator extends DTFJComparator {

	public static final int ARRAY_SIZE = 1;
	public static final int HEAP = 2;
	public static final int ID = 4;
	public static final int JAVA_CLASS = 8;
	public static final int REFERENCES = 16;
	public static final int SECTIONS = 32;
	public static final int SIZE = 64;
	public static final int IS_ARRAY = 128;
	
	// getArraySize()
	// getHeap()
	// getID()
	// getJavaClass()
	// getReferences()
	// getSections()
	// getSize()
	// isArray()
	
	// what about
	
	// getHashcode()
	// getPersistentHashcode()
	
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaObject ddrJavaObject = (JavaObject) ddrObject;
		JavaObject jextractJavaObject = (JavaObject) jextractObject;
		
		// isArray()
		if ((IS_ARRAY & members) != 0)
			testJavaEquals(ddrJavaObject, jextractJavaObject, "isArray");

		// getArraySize()
		if ((ARRAY_SIZE & members) != 0) {
			try {
				if (ddrJavaObject.isArray()) {
					testJavaEquals(ddrJavaObject, jextractJavaObject, "getArraySize");
				}
			} catch (CorruptDataException e) {
				fail(String.format("Problem comparing JavaObject %s", ddrJavaObject));
			}
		}
		
		// getID()
		if ((ID & members) != 0)
			new ImagePointerComparator().testComparatorEquals(ddrJavaObject, jextractJavaObject, "getID");
		
		// getJavaClass()
		if ((JAVA_CLASS & members) != 0)
			new JavaClassComparator().testComparatorEquals(ddrJavaObject, jextractJavaObject, "getJavaClass");
		
		// getReferences()
		if ((REFERENCES & members) != 0)
			new JavaReferenceComparator().testComparatorIteratorEquals(ddrJavaObject, jextractJavaObject, "getReferences", JavaReference.class);
		
		// getSections()
		if ((SECTIONS & members) != 0)
			new ImageSectionComparator().testComparatorIteratorEquals(ddrJavaObject, jextractJavaObject, "getSections", ImageSection.class);
		
		// getHeap()
		if ((HEAP & members) != 0)
			new JavaHeapComparator().testComparatorEquals(ddrJavaObject, jextractJavaObject, "getHeap");
	}

	public int getDefaultMask() {
		return ID | IS_ARRAY | ARRAY_SIZE;
	}
	
	@SuppressWarnings("unchecked")
	public void testArraySize(List ddrObjects, List jextractObjects) throws Exception {
		assertEquals(jextractObjects.size(), ddrObjects.size());
		for (int i = 0; i < ddrObjects.size(); i++) {
			JavaObject ddrJavaObject = (JavaObject) ddrObjects.get(i);
			JavaObject jextractJavaObject = (JavaObject) jextractObjects.get(i);
			
			if(ddrJavaObject.isArray() == jextractJavaObject.isArray()) {
				int ddrSize = 0;
				try {
					ddrSize = ddrJavaObject.getArraySize();
				} catch (IllegalArgumentException e) {
					if(ddrJavaObject.isArray()) {
						throw e;
					}
				}
				int jextractSize = 0;
				try {
					jextractSize = jextractJavaObject.getArraySize();
				} catch (IllegalArgumentException e) {
					if(jextractJavaObject.isArray()) {
						throw e;
					}
				}
				assertEquals("ddr array size not equal to jextract array size", jextractSize, ddrSize);
			}
		}
	}
}
