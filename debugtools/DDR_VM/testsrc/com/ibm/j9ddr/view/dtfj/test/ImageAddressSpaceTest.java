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

import static org.junit.Assert.*;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;

public class ImageAddressSpaceTest extends DTFJUnitTest {
	
	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		ddrObjects.add(ddrAddressSpace);
		jextractObjects.add(jextractAddressSpace);
	}
	
	@Test
	public void getCurrentProcessTest() {
		imageProcessComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getCurrentProcess");
	}
	
	@Test
	public void getImageSectionTest() {
		//TODO - the absolute list isn't particularly important. But we should see the same coverage.
		fail("Implement test");
	}
	
	@Test
	public void getPointerTest() {
		//TODO implement
		fail("Implement Test");
	}
	
	@Test
	public void getProcessesTest() {
		imageProcessComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getProcesses", ImageProcess.class);
	}
	
	@Test
	public void canTestMultipleAddressSpaces() {
		//TODO implement
		fail("Change test to be able to test multiple address spaces");
	}
}
