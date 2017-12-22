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

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaStackFrame;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.j9ddr.view.dtfj.comparators.JavaStackFrameComparator;

public class JavaThreadTest extends DTFJUnitTest {

	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		fillLists(ddrObjects, ddrRuntime.getThreads(), jextractObjects, jextractRuntime.getThreads(), null);
	}
	
	@Test
	public void equalsTest() {
		equalsTest(ddrTestObjects, ddrClonedObjects, jextractTestObjects);
	}
	
	@Test
	public void hashCodeTest() {
		hashCodeTest(ddrTestObjects, ddrClonedObjects);
	}
	
	@Test
	// JEXTRACT: only EVER throws DataUnavailableException with TCK core.  We do better.
	public void getImageThreadTest() {
		for (int i = 0; i < jextractTestObjects.size(); i++) {
			JavaThread t1 = (JavaThread) jextractTestObjects.get(i);
			ImageThread i1;
			try {
				i1 = (ImageThread) t1.getImageThread();
//				System.out.println(i1.getID());
			} catch (CorruptDataException e) {
				System.out.println("CDE on thread: " + i);
			} catch (DataUnavailable e) {
				System.out.println("DUA on thread: " + i);
			}
		}
		imageThreadComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getImageThread");
	}
	
	@Test
	public void getJNIEnvTest() {
		imagePointerComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getJNIEnv");
	}
	
	@Test
	public void getNameTest() {
		javaThreadComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getName");
	}
	
	@Test
	public void getObjectTest() {
		javaObjectComparator.testComparatorEquals(ddrTestObjects, jextractTestObjects, "getObject");
	}
	
	@Test
	public void getPriorityTest() {
		javaThreadComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getPriority");
	}
	
	@Test
	public void getStackFramesTest() {
		javaStackFrameComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getStackFrames", JavaStackFrame.class, JavaStackFrameComparator.BASE_POINTER);
	}
	
	@Test
	public void getStackSectionsTest() {
		imageSectionComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getStackSections", ImageSection.class);
	}
	
	@Test
	public void getStateTest() {
		// TODO: did anybody implement getState for Java 7?
		javaThreadComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getState");
	}
}
