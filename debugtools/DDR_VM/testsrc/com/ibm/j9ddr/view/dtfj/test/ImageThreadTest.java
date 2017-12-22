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

import com.ibm.dtfj.image.ImageRegister;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageStackFrame;
import com.ibm.dtfj.java.JavaRuntime;

public class ImageThreadTest extends DTFJUnitTest {
	
	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		fillLists(ddrObjects, ddrProcess.getThreads(), jextractObjects, jextractProcess.getThreads(), null);
	}
	
	@Test
	public void getIDTest() {
		imageThreadComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getID");
	}
	
	@Test
	public void getPropertiesTest() {
		imageThreadComparator.testPropertiesEquals(ddrTestObjects, jextractTestObjects, "getProperties");
	}
	
	@Test
	public void getRegistersTest() {
		imageRegisterComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getRegisters", ImageRegister.class);
	}
	
	@Test
	public void getStackFramesTest() {
		imageStackFrameComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getStackFrames", ImageStackFrame.class);
	}
	
	@Test
	public void getStackSections() {
		imageSectionComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getStackSections", ImageSection.class);
	}
}
