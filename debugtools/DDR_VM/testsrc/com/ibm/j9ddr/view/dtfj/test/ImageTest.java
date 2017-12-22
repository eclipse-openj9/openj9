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

import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.j9ddr.view.dtfj.comparators.ImageComparator;

public class ImageTest extends DTFJUnitTest {

	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		ddrObjects.add(ddrImage);
		jextractObjects.add(jextractImage);
	}

	@Test
	public void getAddressSpaceTest() {
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Image ddrImg = (Image) ddrTestObjects.get(i);
			Image jextractImg = (Image) jextractTestObjects.get(i);
			imageAddressSpaceComparator.testComparatorIteratorEquals(ddrImg, jextractImg, "getAddressSpaces", ImageAddressSpace.class);
		}
	}

	@Test
	public void getCreationTimeTest() {
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Image ddrImg = (Image) ddrTestObjects.get(i);
			Image jextractImg = (Image) jextractTestObjects.get(i);
			imageComparator.testEquals(ddrImg, jextractImg, ImageComparator.CREATION_TIME);
		}
	}
	@Test
	public void getHostNameTest() {
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Image ddrImg = (Image) ddrTestObjects.get(i);
			Image jextractImg = (Image) jextractTestObjects.get(i);
			imageComparator.testEquals(ddrImg, jextractImg, ImageComparator.HOST_NAME);
		}
	}
	@Test
	public void getInstalledMemoryTest() {
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Image ddrImg = (Image) ddrTestObjects.get(i);
			Image jextractImg = (Image) jextractTestObjects.get(i);
			imageComparator.testEquals(ddrImg, jextractImg, ImageComparator.INSTALLED_MEMORY);
		}
	}
	@Test
	public void getIPAddressesTest() {
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Image ddrImg = (Image) ddrTestObjects.get(i);
			Image jextractImg = (Image) jextractTestObjects.get(i);
			imageComparator.testEquals(ddrImg, jextractImg, ImageComparator.IP_ADDRESSES);
		}
	}
	@Test
	public void getProcessorCountTest() {
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Image ddrImg = (Image) ddrTestObjects.get(i);
			Image jextractImg = (Image) jextractTestObjects.get(i);
			imageComparator.testEquals(ddrImg, jextractImg, ImageComparator.PROCESSOR_COUNT);
		}
	}
	@Test
	public void getProcessorSubTypeTest() {
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Image ddrImg = (Image) ddrTestObjects.get(i);
			Image jextractImg = (Image) jextractTestObjects.get(i);
			imageComparator.testEquals(ddrImg, jextractImg, ImageComparator.PROCESSOR_SUB_TYPE);
		}
	}
	@Test
	public void getProcessorTypeTest() {
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Image ddrImg = (Image) ddrTestObjects.get(i);
			Image jextractImg = (Image) jextractTestObjects.get(i);
			imageComparator.testEquals(ddrImg, jextractImg, ImageComparator.PROCESSOR_TYPE);
		}
	}
	@Test
	public void getSystemSubTypeTest() {
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Image ddrImg = (Image) ddrTestObjects.get(i);
			Image jextractImg = (Image) jextractTestObjects.get(i);
			imageComparator.testEquals(ddrImg, jextractImg, ImageComparator.SYSTEM_SUB_TYPE);
		}
	}
	@Test
	public void getSystemTypeTest() {
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Image ddrImg = (Image) ddrTestObjects.get(i);
			Image jextractImg = (Image) jextractTestObjects.get(i);
			imageComparator.testEquals(ddrImg, jextractImg, ImageComparator.SYSTEM_TYPE);
		}
	}
}
