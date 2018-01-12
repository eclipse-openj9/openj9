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

import static org.junit.Assert.*;

import java.util.Iterator;
import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.j9ddr.view.dtfj.comparators.JavaClassComparator;

public class JavaClassLoaderTest extends DTFJUnitTest {
	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		fillLists(ddrObjects, ddrRuntime.getJavaClassLoaders(), jextractObjects, jextractRuntime.getJavaClassLoaders(), null);
	}
	
	@Test
	public void equalsTest() {
		equalsTest(ddrTestObjects, ddrClonedObjects, jextractTestObjects);
	}
	
	@Test
	public void hashCodeTest() {	
		hashCodeTest(ddrTestObjects, ddrClonedObjects);
	}
	
	@SuppressWarnings("unchecked")
	@Test
	public void findClassTest() throws Exception {
		JavaClassComparator comparator = new JavaClassComparator();
		assertEquals(jextractTestObjects.size(), ddrTestObjects.size());
		for(int i = 0; i < ddrTestObjects.size(); i++) {
			JavaClassLoader ddrLoader = (JavaClassLoader) ddrTestObjects.get(i);
			JavaClassLoader jextractLoader = (JavaClassLoader) jextractTestObjects.get(i);
			Iterator ddrClasses = ddrLoader.getDefinedClasses();
			Iterator jextractClasses = jextractLoader.getDefinedClasses();	//loop the defined classes trying to find each one
			while(ddrClasses.hasNext() || jextractClasses.hasNext()) {
				assertTrue("DDR classloader has less items than the jextract equivalent : " + ddrLoader.toString(), ddrClasses.hasNext());
				assertTrue("Jextract classloader has less items than the jextract equivalent : " + jextractLoader.toString(), jextractClasses.hasNext());
				JavaClass ddrClass = (JavaClass) ddrClasses.next();
				JavaClass jextractClass = (JavaClass) jextractClasses.next();
				assertEquals("Different class names : ddr = " + ddrClass.getName() + " jx = " + jextractClass.getName(), jextractClass.getName(), ddrClass.getName());
				String name = ddrClass.getName();
				JavaClass ddrFound = ddrLoader.findClass(name);
				assertNotNull("DDR loader could not find class : " + name, ddrFound);
				JavaClass jextractFound = jextractLoader.findClass(name);
				assertNotNull("JX loader could not find class : " + name, jextractFound);
				comparator.testEquals(ddrFound, jextractFound, javaClassComparator.getDefaultMask());
			}
		}
	}
	
	@Test
	public void getCachedClassesTest() {
		javaClassComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getCachedClasses", JavaClass.class);
	}
	
	@Test
	public void getDefinedClassesTest() {
		javaClassComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getDefinedClasses", JavaClass.class);
	}
	
	@Test
	public void getObjectTest() {
		javaObjectComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getObject");
	}
}
