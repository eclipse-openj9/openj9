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
package com.ibm.j9ddr.view.dtfj.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Ignore;
import org.junit.Test;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaReference;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.j9ddr.view.dtfj.comparators.JavaHeapComparator;
import com.ibm.j9ddr.view.dtfj.comparators.JavaObjectComparator;

public class JavaObjectTest extends DTFJUnitTest {
	private static String gcpolicy = null;

	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	/**
	 * Fills each list with a limited number of objects from the respective iterator.
	 * Does not add CorruptData objects from the Iterator
	 * If one iterator returns a CorruptData insures that the other iterator returns a CorruptData
	 * 
	 * Sorts the output if a sort order is provided.  
	 * 
	 * @param ddrList
	 * @param ddrIterator
	 * @param jextractList
	 * @param jextractIterator
	 * @param sortOrder
	 * @param maxCount the maximum number of items to populate the lists with
	 */
	@SuppressWarnings("unchecked")
	public static void fillObjectList(List<Object> ddrList, Iterator ddrIterator, List<Object> jextractList, Iterator jextractIterator, Comparator sortOrder, int maxCount, boolean arrayList) {
		List<Object> ddrObjects = new ArrayList<Object>();
		List<Object> jextractObjects = new ArrayList<Object>();
		
		while(ddrIterator.hasNext()) {
			assertTrue("DDR returned more elements than jextract", jextractIterator.hasNext());
			Object ddrObject = ddrIterator.next();
			Object jextractObject = jextractIterator.next();
		
			ddrObjects.add(ddrObject);
			jextractObjects.add(jextractObject);
		}
		
		//removed assertion below because when using a sample rather than an exhaustive list one list can be larger than the other so long
		//as the order is the same for the super set of list elements.
		//assertFalse("jextract returned more elements than ddr", jextractIterator.hasNext());

		Object[] ddrObjectsArray = ddrObjects.toArray();
		Object[] jextractObjectsArray = jextractObjects.toArray();
		
		if (sortOrder != null) {
			Arrays.sort(ddrObjectsArray, sortOrder);
			Arrays.sort(jextractObjectsArray, sortOrder);
		}
		
		int count = 0;
		for (int i = 0; (i < ddrObjectsArray.length) && (count < maxCount); i++) {
			Object ddrObject = ddrObjectsArray[i];
			Object jextractObject = jextractObjectsArray[i];
			
			if (!(ddrObject instanceof CorruptData) && !(jextractObject instanceof CorruptData)) {
				JavaObject ddrJavaObject = (JavaObject) ddrObject; 
				JavaObject jextractJavaObject = (JavaObject) jextractObject;
				try {
					if((ddrJavaObject.isArray() && jextractJavaObject.isArray()) == arrayList) {
						ddrList.add(ddrObject);
						jextractList.add(jextractObject);
						count++;
					}
				} catch (CorruptDataException e) {
					fail("CDE when populating heap list : " + e.getMessage());
				}
				continue;
			}
			
			if (ddrObject instanceof CorruptData && jextractObject instanceof CorruptData) {
				continue;
			}
			
			fail("If DDR object is a CorruptData then jextract object must also be a CorruptData");
		}
		assertEquals("jextract and ddr returned iterators of different sizes", jextractList.size(), ddrList.size());
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		List<Object> ddrHeaps = new ArrayList<Object>();
		List<Object> jextractHeaps = new ArrayList<Object>();
		
		gcpolicy = getVMoption(ddrRuntime, "-Xgcpolicy:");
		if(gcpolicy.equals("gencon")) {
			ddrHeaps.add(ddrRuntime.getHeaps().next());
			jextractHeaps.add(jextractRuntime.getHeaps().next());
		} else {
			fillLists(ddrHeaps, ddrRuntime.getHeaps(), jextractHeaps, jextractRuntime.getHeaps(), null);
		}
		
		for (int i = 0; i < ddrHeaps.size(); i++) {
			JavaHeap ddrHeap = (JavaHeap) ddrHeaps.get(i);
			JavaHeap jextractHeap = (JavaHeap) jextractHeaps.get(i);
			
			//this is a very simple grab of the first 10 objects off the heap which are not arrays and comparing them
			fillObjectList(ddrObjects, ddrHeap.getObjects(), jextractObjects, jextractHeap.getObjects(), null, 10, false);
		}
	}
	
	@Ignore
	public void arraycopyTest() throws Exception {
		fail("The output of arrayCopy is tested by the TCK and is beyond the scope of this unit test.");
	}
	
	@Test
	public void equalsTest() {
		equalsTest(ddrTestObjects, ddrClonedObjects, jextractTestObjects);
	}
	
	@Test
	public void hashCodeTest() {
		hashCodeTest(ddrTestObjects, ddrClonedObjects);
	}
	
	public void getArraySizeTest() throws Exception {
		new JavaObjectComparator().testArraySize(ddrTestObjects, jextractTestObjects);
	}
	
	@Test
	public void getHashcodeTest() {
		javaObjectComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getHashcode");
	}
	
	@Test
	public void getHeapTest() {
		if(gcpolicy.equals("gencon")) {
			//for gencon only check the heap name as DTFJ and DDR report different heaps
			javaHeapComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getHeap", JavaHeapComparator.NAME);
		} else {
			javaHeapComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getHeap");
		}
	}
	@Test
	public void getIDTest() {
		imagePointerComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getID");
	}
	@Test
	public void getJavaClassTest() {
		javaClassComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getJavaClass");
	}
	
	@Test
	public void getPersistentHashcodeTest() {
		javaObjectComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getPersistentHashcode");
	}
	
	@Test
	public void getReferencesTest() {
		javaReferenceComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getReferences", JavaReference.class);
	}
	
	@Test
	public void getSectionsTest() {
		imageSectionComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getSections", ImageSection.class);
	}
	
	@Test
	public void getSizeTest() {
		javaObjectComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getSize");
	}
	
	@Test
	public void isArrayTest() {
		javaObjectComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "isArray");
	}
}
