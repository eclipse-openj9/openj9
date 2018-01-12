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

import static org.junit.Assert.fail;

import java.util.ArrayList;
import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Ignore;
import org.junit.Test;

import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaRuntime;

public class JavaFieldTest extends DTFJUnitTest {
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
			fillLists(ddrObjects, ddrClass.getDeclaredFields(), jextractObjects, jextractClass.getDeclaredFields(), JavaClassTest.DECLARED_FIELDS_SORT_ORDER);
		}
	}
	
	// JavaMember Tests
	
	@Test
	public void equalsTest() {
		equalsTest(ddrTestObjects, ddrClonedObjects, jextractTestObjects, true);
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
		javaFieldComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getModifiers");
	}
	
	@Test
	public void getNameTest() {
		javaFieldComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getName");
	}
	
	@Test 
	public void getSignatureTest() {
		javaFieldComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getSignature");
	}

	// The following tests are all behavioral tests as opposed to state comparison tests.
	// Arguably they are test sufficiently by the DTFJ TCK.
	// If we do decide to test them ourselves then we need to grab some JObjects to get the values out of
	// as well as the actual field objects.

	// JavaField Tests
	@Ignore
	@Test
	public void getTest() {
	
		fail("Test not implemented");
	}
	
	@Ignore
	@Test
	public void getBooleanTest() {
	}
	
	@Ignore
	@Test
	public void getByteTest() {
		fail("Test not implemented");
	}
	
	@Ignore
	@Test
	public void getCharTest() {
		fail("Test not implemented");
	}
	
	@Ignore
	@Test
	public void getDoubleTest() {
		fail("Test not implemented");
	}
	
	@Ignore
	@Test
	public void getFloatTest() {
		fail("Test not implemented");
	}
	
	@Ignore
	@Test
	public void getIntTest() {
		fail("Test not implemented");
	}
	
	@Ignore
	@Test
	public void getLongTest() {
		fail("Test not implemented");
	}
	
	@Ignore
	@Test
	public void getShortTest() {
		fail("Test not implemented");
	}
	
	@Ignore
	@Test
	public void getStringTest() {
		fail("Test not implemented");
	}
}
