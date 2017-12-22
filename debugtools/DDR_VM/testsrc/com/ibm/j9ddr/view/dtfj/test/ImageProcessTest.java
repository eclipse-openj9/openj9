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

import java.util.Comparator;
import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.j9ddr.view.dtfj.comparators.ImageProcessComparator;

public class ImageProcessTest extends DTFJUnitTest {
	
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
		fillLists(ddrObjects, ddrAddressSpace.getProcesses(), jextractObjects, jextractAddressSpace.getProcesses(), new Comparator<Object>(){

			public int compare(Object o1, Object o2)
			{
				ImageProcess p1 = (ImageProcess)o1;
				ImageProcess p2 = (ImageProcess)o2;
				
				try {
					return p1.getID().compareTo(p2.getID());
				} catch (Exception e) {
					return 0;
				}
			}});
	}
	
	@Test
	public void getCommandLineTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageProcessComparator.testEquals(ddrObj, jextractObj, ImageProcessComparator.COMMAND_LINE);
		}
	}
	
	@Test
	public void getCurrentThreadTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageProcessComparator.testEquals(ddrObj, jextractObj, ImageProcessComparator.CURRENT_THREAD);
		}
	}
	
	@Test
	public void getEnvironmentTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageProcessComparator.testEquals(ddrObj, jextractObj, ImageProcessComparator.ENVIRONMENT);
		}
	}

	@Test
	public void getExecutableTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageProcessComparator.testEquals(ddrObj, jextractObj, ImageProcessComparator.EXECUTABLE);
		}
	}

	@Test
	public void getIDTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageProcessComparator.testEquals(ddrObj, jextractObj, ImageProcessComparator.ID);
		}
	}

	@Test
	public void getLibrariesTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageProcessComparator.testEquals(ddrObj, jextractObj, ImageProcessComparator.LIBRARIES);
		}
	}

	@Test
	public void getPointerSizeTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageProcessComparator.testEquals(ddrObj, jextractObj, ImageProcessComparator.POINTER_SIZE);
		}
	}

	public void getRuntimesTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageProcessComparator.testEquals(ddrObj, jextractObj, ImageProcessComparator.RUNTIMES);
		}
	}

	@Test
	public void getSignalNameTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageProcessComparator.testEquals(ddrObj, jextractObj, ImageProcessComparator.SIGNAL_NAME);
		}
	}

	@Test
	public void getSignalNumberTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageProcessComparator.testEquals(ddrObj, jextractObj, ImageProcessComparator.SIGNAL_NUMBER);
		}
	}

	@Test
	public void getThreadsTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageProcessComparator.testEquals(ddrObj, jextractObj, ImageProcessComparator.THREADS);
		}
	}
}
