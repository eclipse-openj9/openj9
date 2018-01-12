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

import java.util.ArrayList;
import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaRuntime;

public class JavaReferenceTest extends DTFJUnitTest {
	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects)
	{
		List<Object> ddrClassLoaders = new ArrayList<Object>();
		List<Object> jextractClassLoaders = new ArrayList<Object>();
		
		fillLists(ddrClassLoaders, ddrRuntime.getJavaClassLoaders(), jextractClassLoaders, jextractRuntime.getJavaClassLoaders(), null);

		List<Object> ddrClasses = new ArrayList<Object>();
		List<Object> jextractClasses = new ArrayList<Object>();

		for (int i = 0; i < ddrClassLoaders.size(); i++) {
			JavaClassLoader ddrClassLoader = (JavaClassLoader) ddrClassLoaders.get(i);
			JavaClassLoader jextractClassLoader = (JavaClassLoader) jextractClassLoaders.get(i);
			fillLists(ddrClasses, ddrClassLoader.getDefinedClasses(), jextractClasses, jextractClassLoader.getDefinedClasses(), null);
		}
		
		for (int i = 0; i < ddrClasses.size(); i++) {
			JavaClass ddrClass = (JavaClass) ddrClasses.get(i);
			JavaClass jextractClass = (JavaClass) jextractClasses.get(i);
			fillLists(ddrObjects, ddrClass.getReferences(), jextractObjects, jextractClass.getReferences(), JavaClassTest.REFERENCES_SORT_ORDER);
		}
	}
	
	@Test
	public void getDescriptionTest() {
		javaReferenceComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getDescription");
	}
	
	@Test
	public void getReachabilityTest() {
		javaReferenceComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getReachability");
	}
	
	@Test
	public void getReferenceTypeTest() {
		javaReferenceComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getReferenceType");
	}
	
	@Test
	public void getRootTypeTest() {
		javaReferenceComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getRootType");
	}
	
	@Test
	public void getSourceTest() {
		for (int i = 0; i < ddrTestObjects.size(); i++) {
			Object ddrObject = ddrTestObjects.get(i);
			Object jextractObject = jextractTestObjects.get(i);
			
			try {
				javaClassComparator.testComparatorEquals(ddrObject, jextractObject, "getTarget");
				return;
			} catch (ClassCastException e) {
				// Do nothing.
			}

			// We only get here if the target was NOT a JavaClass
			try {
				javaObjectComparator.testComparatorEquals(ddrObject, jextractObject, "getTarget");
				return;
			} catch (ClassCastException e) {
				// Do nothing
			}
			
			// We only get here if the target was NOT a JavaClass or a JavaObject
			javaStackFrameComparator.testComparatorEquals(ddrObject, jextractObject, "getTarget");
		}
	}
	
	@Test
	public void getTargetTest() {
		for (int i = 0; i < ddrTestObjects.size(); i++) {
			Object ddrObject = ddrTestObjects.get(i);
			Object jextractObject = jextractTestObjects.get(i);
			
			try {
				javaClassComparator.testComparatorEquals(ddrObject, jextractObject, "getTarget");
				return;
			} catch (ClassCastException e) {
				// Do nothing.
			}

			// We only get here if the target was NOT a JavaClass
			javaObjectComparator.testComparatorEquals(ddrObject, jextractObject, "getTarget");
		}
	}
	
	@Test
	public void isClassReferenceTest() {
		javaReferenceComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "isClassReference");
	}
	
	@Test
	public void isObjectReferenceTest() {
		javaReferenceComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "isObjectReference");
	}
}
