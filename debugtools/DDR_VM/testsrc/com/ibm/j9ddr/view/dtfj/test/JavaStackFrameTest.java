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

import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaLocation;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaStackFrame;
import com.ibm.dtfj.java.JavaThread;
import com.ibm.j9ddr.view.dtfj.comparators.JavaStackFrameComparator;

public class JavaStackFrameTest extends DTFJUnitTest {

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
			
			try {
				fillLists(ddrObjects, ddrThread.getStackFrames(), jextractObjects, jextractThread.getStackFrames(), null);
			} catch (AssertionError e) {
				System.err.println("Assertion failed in fillLists.");
				try {
					System.err.println("Processing DDRThread: " + Long.toHexString(ddrThread.getJNIEnv().getAddress()) + " and JExtract/DTFJ thread " + Long.toHexString(jextractThread.getJNIEnv().getAddress()));
				
					System.err.println("DDR trace is:");
					
					printStackTrace(ddrThread);
					
					System.err.println("J9JExtractTrace is:");
					
					printStackTrace(jextractThread);
				} catch (Exception e2) {
					e.printStackTrace();
				}
				
				throw e;
			}
		}
	}
	
	private void printStackTrace(JavaThread thread) throws CorruptDataException
	{
		Iterator<?> frames = thread.getStackFrames();
		
		int count = 0;
		
		while (frames.hasNext()) {
			Object o = frames.next();
			
			System.err.print(count + ":");
			
			if (o instanceof JavaStackFrame) {
				JavaStackFrame frame = (JavaStackFrame)o;
				JavaLocation location = frame.getLocation();
				
				int lineNumber = -1;
				String fileName = "<unknown>";
				try {
					lineNumber = location.getLineNumber();
				} catch (DataUnavailable e) {
					//deliberately do nothing
				}
				
				try {
					fileName = location.getFilename();
				} catch (DataUnavailable e) {
					//Deliberately do nothing
				}
				
				System.err.println(location.getMethod().getClass().getName() + "." + location.getMethod().getName() + "(" + fileName + ":" + lineNumber + ")");
			} else if (o instanceof CorruptData) {
				System.err.println("<Corrupt Data: " + o + " >");
			} else {
				System.err.println("<Unexpected type: " + o.getClass().getName() + ":  "+ o + " >");
			}
			
			count++;
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
	public void getBasePointerTest() {
		JavaStackFrameComparator comp = new JavaStackFrameComparator();
		
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			
			comp.testEquals(ddrObj,jextractObj,JavaStackFrameComparator.BASE_POINTER);
		}
	}
	
	@Test
	public void getHeapRootsTest() {
		JavaStackFrameComparator comp = new JavaStackFrameComparator();
		
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			
			comp.testEquals(ddrObj,jextractObj,JavaStackFrameComparator.HEAP_ROOTS);
		}
	}
	
	@Test
	public void getLocationTest() {
		JavaStackFrameComparator comp = new JavaStackFrameComparator();
		
		for (int i=0; i!=ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			
			comp.testEquals(ddrObj,jextractObj,JavaStackFrameComparator.LOCATION);
		}
	}
}
