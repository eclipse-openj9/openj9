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
import java.util.LinkedList;
import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Ignore;
import org.junit.Test;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaStackFrame;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.j9ddr.view.dtfj.comparators.JavaLocationComparator;

public class JavaLocationTest extends DTFJUnitTest {

	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		List<Object> ddrThreads = new LinkedList<Object>();
		List<Object> jextractThreads = new LinkedList<Object>();
		
		fillLists(ddrThreads, ddrRuntime.getThreads(), jextractThreads, jextractRuntime.getThreads(), null);
		
		for (int i = 0; i < ddrThreads.size(); i++) {
			JavaThread ddrThread = (JavaThread)ddrThreads.get(i);
			JavaThread jextractThread = (JavaThread)jextractThreads.get(i);
			
			Iterator<?> ddrStackFrameIt = ddrThread.getStackFrames();
			Iterator<?> jextractStackFrameIt = jextractThread.getStackFrames();
			
			while (ddrStackFrameIt.hasNext()) {
				assertTrue(jextractStackFrameIt.hasNext());
				
				Object ddrStackFrameObj = ddrStackFrameIt.next();
				Object jextractStackFrameObj = jextractStackFrameIt.next();
				
				if ((ddrStackFrameObj instanceof CorruptData) ^ (jextractStackFrameObj instanceof CorruptData)) {
					fail("Different CorruptData from DDR & jextract. DDR returned : " + ddrStackFrameObj + ", jextract returned " + jextractStackFrameObj);
				}
				
				if (ddrStackFrameObj instanceof CorruptData) {
					continue;
				}
				
				JavaStackFrame ddrStackFrame = (JavaStackFrame)ddrStackFrameObj;
				JavaStackFrame jextractStackFrame = (JavaStackFrame)jextractStackFrameObj;
				
				
				boolean ddrFaulted = false;
				boolean jextractFaulted = false;
				try {
					ddrObjects.add(ddrStackFrame.getLocation());
				} catch (CorruptDataException e) {
					e.printStackTrace();
					ddrFaulted = true;
				}
				
				try {
					jextractObjects.add(jextractStackFrame.getLocation());
				} catch (CorruptDataException e) {
					e.printStackTrace();
					jextractFaulted = true;
				}
				
				if (ddrFaulted ^ jextractFaulted) {
					fail("Getting location from stackframe behaved differently on DDR/jextract. DDR faulted: " + ddrFaulted + ", DDR object: " + ddrStackFrame + ", jextract faulted: " + jextractFaulted + " jextract object: " + jextractStackFrame);
				}
			}
		}
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
	public void getAddressTest() {
		JavaLocationComparator comp = new JavaLocationComparator();
		
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			
			comp.testEquals(ddrObj,jextractObj,JavaLocationComparator.ADDRESS);
		}
	}
	
	@Ignore("DTFJ/jextract gives wrong answer for getCompilationLevel")
	@Test
	public void getCompilationLevelTest() {
		JavaLocationComparator comp = new JavaLocationComparator();
		
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			
			comp.testEquals(ddrObj,jextractObj,JavaLocationComparator.COMPILATION_LEVEL);
		}
	}
	
	@Test
	public void getFilenameTest() {
		JavaLocationComparator comp = new JavaLocationComparator();
		
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			
			comp.testEquals(ddrObj,jextractObj,JavaLocationComparator.FILE_NAME);
		}
	}
	
	@Test
	public void getLineNumberTest() {
		JavaLocationComparator comp = new JavaLocationComparator();
		
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			
			comp.testEquals(ddrObj,jextractObj,JavaLocationComparator.LINE_NUMBER);
		}
	}
	
	@Test
	public void getMethodTest() {
		JavaLocationComparator comp = new JavaLocationComparator();
		
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			
			comp.testEquals(ddrObj,jextractObj,JavaLocationComparator.METHOD);
		}
	}
	
	@Test
	public void toStringTest() {
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			
			assertEquals(jextractObj.toString(), ddrObj.toString());
		}
	}
}
