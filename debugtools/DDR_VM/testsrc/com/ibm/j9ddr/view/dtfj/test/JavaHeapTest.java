/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaRuntime;

/*
 * The heap unit test is not run on a gencon core file because DDR correctly returns 3 heaps through the runtime (2xnursery + tenured)
 * whereas DTFJ rolls this into a single heap. This means that the heap comparisons cannot be performed.
 */

public class JavaHeapTest extends DTFJUnitTest {
	private static boolean isgencon = false;
	
	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	@Override
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		String gcpolicy = getVMoption(ddrRuntime, "-Xgcpolicy:");
		isgencon = gcpolicy.equals("gencon");
		if(isgencon) {
			ddrObjects.add(gcpolicy);		//add a dummy object to pass other framework tests
		} else {
			fillLists(ddrObjects, ddrRuntime.getHeaps(), jextractObjects, jextractRuntime.getHeaps(), null);
		}
		
	}
	
	@Test
	public void equalsTest() {
		if(isgencon) {
			System.out.println("Skipping test as GC policy = gencon");
			return;
		}
		equalsTest(ddrTestObjects, ddrClonedObjects, jextractTestObjects);
	}
	
	@Test
	public void hashCodeTest() {
		if(isgencon) {
			System.out.println("Skipping test as GC policy = gencon");
			return;
		}
		hashCodeTest(ddrTestObjects, ddrClonedObjects);
	}
	
	@Test
	public void getNameTest() {
		if(isgencon) {
			System.out.println("Skipping test as GC policy = gencon");
			return;
		}
		javaHeapComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getName");
	}
	
	@Test
	public void getObjectsTest() {
		if(isgencon) {
			System.out.println("Skipping test as GC policy = gencon");
			return;
		}
		javaObjectComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getObjects", JavaObject.class);
	}
	
	@Test
	public void getSectionsTest() {
		if(isgencon) {
			System.out.println("Skipping test as GC policy = gencon");
			return;
		}
		imageSectionComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getSections", ImageSection.class);
	}
}
