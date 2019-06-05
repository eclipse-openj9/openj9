/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;

import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.j9ddr.view.dtfj.test.DTFJComparator;
import com.ibm.j9ddr.view.dtfj.test.JavaMethodTest;
import com.ibm.j9ddr.view.dtfj.test.DTFJUnitTest.InvalidObjectException;

public class JavaMethodComparator extends DTFJComparator {

	public static final int BYTE_CODE_SECTIONS = 1;
	public static final int COMPILED_SECTIONS = 2;
	public static final int NAME = 4;
	
	// getByteCodeSections()
	// getCompiledSections()
	public void testEquals(Object ddrObject, Object jextractObject, int members) {
		JavaMethod ddrJavaMethod = (JavaMethod) ddrObject;
		JavaMethod jextractJavaMethod = (JavaMethod) jextractObject;
		
		ImageSectionComparator imageSectionComparator = new ImageSectionComparator();

		// getByteCodeSections()
		if ((BYTE_CODE_SECTIONS & members) != 0)
			imageSectionComparator.testComparatorIteratorEquals(ddrJavaMethod, jextractJavaMethod, "getBytecodeSections", ImageSection.class);
		
		// getCompiledSections()
		if ((COMPILED_SECTIONS & members) != 0) {
			compareCompiledSections(ddrJavaMethod, jextractJavaMethod, imageSectionComparator);
		}
		
		// getName()
		if ((NAME & members) != 0)
			testJavaEquals(ddrJavaMethod, jextractJavaMethod, "getName");
	}

	// JEXTRACT has a bug where warm/cold compiled sections are not handled properly.
	// DDR handles these sections properly so may return more sections than jextract.
	
	// When number of returned values are the same test that they are all the same.
	// If the numbers differ, remove all cold sections from DDR output and make sure remainders
	// match jextract output.  In this scenario we must ignore the size of the jextract output since
	// it too is incorrect
	
	public static void compareCompiledSections(JavaMethod ddrJavaMethod, JavaMethod jextractJavaMethod, DTFJComparator imageSectionComparator) {
		List<Object> ddrList = new LinkedList<Object>();
		List<Object> jextractList = new LinkedList<Object>();
		
		try {
			ddrList = DTFJComparator.getList(ImageSection.class, ddrJavaMethod.getCompiledSections(), null);
			jextractList = DTFJComparator.getList(ImageSection.class, jextractJavaMethod.getCompiledSections(), null);
		} catch (InvalidObjectException e) {
			fail("Could not initialize test");
		}

		if (ddrList.size() == jextractList.size()) {
			imageSectionComparator.testComparatorIteratorEquals(ddrJavaMethod, jextractJavaMethod, "getCompiledSections", ImageSection.class, JavaMethodTest.COMPILIED_SECTIONS_SORT_ORDER, imageSectionComparator.getDefaultMask());
		} else {
		
			// Handle case where DTFJ is correctly detected warm/cold paths and jextract is not.
			// Remove cold paths and then compare.
			
			List<Object> newDDRList = new LinkedList<Object>();
			
			for (Object object : ddrList) {
				if (object instanceof ImageSection) {
					ImageSection sectionObject = (ImageSection) object;
					if (!sectionObject.getName().contains("cold")) {
						newDDRList.add(object);
					}
				}
			}
			
			assertEquals("JavaMethod.getCompiledSections() returns different size even ignoring cold sections", jextractList.size(), newDDRList.size());

			Object[] ddrArray = newDDRList.toArray();
			Object[] jextractArray = jextractList.toArray();
			Arrays.sort(ddrArray, JavaMethodTest.COMPILIED_SECTIONS_SORT_ORDER);
			Arrays.sort(jextractArray, JavaMethodTest.COMPILIED_SECTIONS_SORT_ORDER);
			
			for (int j = 0; j < ddrArray.length; j++) {
				imageSectionComparator.testEquals(ddrArray[j], jextractArray[j], ImageSectionComparator.BASE_ADDRESS | ImageSectionComparator.NAME);
			}
		}
	}
}
