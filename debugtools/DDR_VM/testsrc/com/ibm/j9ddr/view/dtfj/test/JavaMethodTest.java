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
import java.util.Comparator;
import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaMethod;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.j9ddr.view.dtfj.comparators.JavaMethodComparator;

public class JavaMethodTest extends DTFJUnitTest {
	
	public static final Comparator<Object> COMPILIED_SECTIONS_SORT_ORDER = new Comparator<Object>() {
		public int compare(Object o1, Object o2) {
			ImageSection i1 = (ImageSection) o1;
			ImageSection i2 = (ImageSection) o2;
			return new Long(i1.getBaseAddress().getAddress()).compareTo(new Long(i2.getBaseAddress().getAddress()));
		}
	};
	
	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
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
			fillLists(ddrObjects, ddrClass.getDeclaredMethods(), jextractObjects, jextractClass.getDeclaredMethods(), null);
		}
	}
	
	// JavaMember Tests
	
	@Test
	public void equalsTest() {
		equalsTest(ddrTestObjects, ddrClonedObjects, jextractTestObjects);
	}
	
	@Test
	public void hashCodeTest() {
		hashCodeTest(ddrTestObjects, ddrClonedObjects);
	}
	
	@Test
	public void getDeclaringClassTest() {
		javaClassComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getDeclaringClass");
	}
	
	@Test
	public void getModifiersTest() {
		javaMethodComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getModifiers");
	}
	
	@Test
	public void getNameTest() {
		javaMethodComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getName");
	}
	
	@Test 
	public void getSignatureTest() {
		javaMethodComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getSignature");
	}
	
	// JavaMethod Tests
	
	@Test
	public void getBytecodeSectionsTest() {
		imageSectionComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getBytecodeSections", ImageSection.class);
	}
	
	@Test
	public void getCompiledSectionsTest() {
		// Have to iterate manually here because getCompiledMethods may return a different number of elements
		// between DDR and JExtract due to JExtract missing warm/cold methods.
		
		for (int i = 0; i < ddrTestObjects.size(); i++) {
			JavaMethod ddrJavaMethod = (JavaMethod) ddrTestObjects.get(i);
			JavaMethod jextractJavaMethod = (JavaMethod) jextractTestObjects.get(i);
			JavaMethodComparator.compareCompiledSections(ddrJavaMethod, jextractJavaMethod, imageSectionComparator);
		}
	}
}
