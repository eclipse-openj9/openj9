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

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.java.JavaClassLoader;
import com.ibm.dtfj.java.JavaRuntime;

public class ImagePointerTest extends DTFJUnitTest {
	
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

		for (int i = 0; i < ddrClassLoaders.size(); i++) {
			JavaClassLoader ddrClassLoader = (JavaClassLoader) ddrClassLoaders.get(i);
			JavaClassLoader jextractClassLoader = (JavaClassLoader) jextractClassLoaders.get(i);
			
			try {
				ddrObjects.add(ddrClassLoader.getObject().getID());
				jextractObjects.add(jextractClassLoader.getObject().getID());
			} catch (CorruptDataException e) {
				fail("Problem initializing test");
			}
		}
	}
	
	@Test
	public void addTest() {
		imagePointerComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "add", new Class[]{long.class}, new Object[]{0x10});
	}

	@Ignore
	public void equalsTest() {
		// the equalsTest ensures that
		// DTFJ objects loaded by same DDR image factory but different class loaders are never equal
		// but the J9DDRImagePointer class is shared across class loaders and so will be equal as they are not VM version specific
		//equalsTest(ddrTestObjects, ddrClonedObjects, ddrSameButNotEqualObjects, jextractTestObjects, true);
	}
	
	@Test
	public void hashCodeTest() {
		hashCodeTest(ddrTestObjects, ddrClonedObjects);
	}
	
	@Test
	public void getAddressTest() {
		imagePointerComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getAddress");
	}

	@Test
	public void getAddressSpaceTest() {
		imageAddressSpaceComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getAddressSpace");
	}

	@Test
	public void getByteAtTest() {
		imagePointerComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getByteAt", new Class[]{long.class}, new Object[]{0});
	}

	@Test
	public void getDoubleAtTest() {
		imagePointerComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getDoubleAt", new Class[]{long.class}, new Object[]{0});
	}

	@Test
	public void getFloatAtTest() {
		imagePointerComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getFloatAt", new Class[]{long.class}, new Object[]{0});
	}

	@Test
	public void getIntAtTest() {
		imagePointerComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getIntAt", new Class[]{long.class}, new Object[]{0});
	}

	@Test
	public void getLongAtTest() {
		imagePointerComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getLongAt", new Class[]{long.class}, new Object[]{0});
	}

	@Test
	public void getPointerAtTest() {
		imagePointerComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getPointerAt", new Class[]{long.class}, new Object[]{0});
	}

	@Test
	public void getShortAtTest() {
		imagePointerComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getShortAt", new Class[]{long.class}, new Object[]{0});
	}

	@Test
	public void isExecutableTest() {
		imagePointerComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "isExecutable");
	}

	@Test
	public void isReadOnlyTest() {
		imagePointerComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "isReadOnly");
	}

	@Test
	public void isSharedTest() {
		imagePointerComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "isShared");
	}
}
