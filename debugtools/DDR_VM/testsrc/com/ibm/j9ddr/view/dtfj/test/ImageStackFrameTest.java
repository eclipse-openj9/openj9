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

import static com.ibm.j9ddr.view.dtfj.test.TestUtil.slurpIterator;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.fail;

import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.j9ddr.view.dtfj.comparators.ImageStackFrameComparator;

public class ImageStackFrameTest extends DTFJUnitTest  {
	
	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		//Extract stack frames from all threads
		List<Object> ddrThreads = new LinkedList<Object>();
		List<Object> jextractThreads = new LinkedList<Object>();
		
		Comparator<Object> c = new Comparator<Object>(){

			public int compare(Object o1, Object o2)
			{
				if (o1 instanceof ImageThread && o2 instanceof ImageThread) {
					ImageThread t1 = (ImageThread)o1;
					ImageThread t2 = (ImageThread)o2;
					
					try {
						return t1.getID().compareTo(t2.getID());
					} catch (CorruptDataException e) {
						return 0;
					}
				} else {
					return 0;
				}
			}};
		
		fillLists(ddrThreads, ddrProcess.getThreads(), jextractThreads, jextractProcess.getThreads(), c);
		
		assertEquals("Different numbers of threads", jextractThreads.size(), ddrThreads.size());
		
		Collections.sort(ddrThreads,c);
		Collections.sort(jextractThreads,c);
		
		for (int i=0; i!= ddrThreads.size(); i++) {
			ImageThread ddrThreadObj = (ImageThread) ddrThreads.get(i);
			ImageThread jextractThreadObj = (ImageThread) jextractThreads.get(i);
			
			Exception ddrException = null;
			Exception jextractException = null;
			
			try {
				Iterator<?> it = ddrThreadObj.getStackFrames();
				slurpIterator(it, ddrObjects);
			} catch (DataUnavailable e) {
				e.printStackTrace();
				ddrException = e;
			}
			
			try {
				Iterator<?> it = jextractThreadObj.getStackFrames();
				slurpIterator(it, jextractObjects);
			} catch (DataUnavailable e) {
				e.printStackTrace();
				jextractException = e;
			}
			
			if (ddrException != null || jextractException != null) {
				if (ddrException != null && jextractException != null) {
					assertEquals("JExtract and DDR threw different exceptions", jextractException.getClass(), ddrException.getClass());
				} else {
					if (ddrException != null) {
						fail("DDR threw an exception and jextract didn't");
					} else {
						fail("Jextract threw an exception and DDR didn't");
					}
				}
			}
		}
	}

	@Test
	public void getBasePointerTest() {
		for (int i=0; i!= ddrTestObjects.size(); i++) {
			Object ddrObject = ddrTestObjects.get(i);
			Object jextractObject = jextractTestObjects.get(i);
			imageStackFrameComparator.testEquals(ddrObject, jextractObject, ImageStackFrameComparator.BASE_POINTER);
		}
	}

	@Test
	public void getProcedureAddressTest() {
		for (int i=0; i!= ddrTestObjects.size(); i++) {
			Object ddrObject = ddrTestObjects.get(i);
			Object jextractObject = jextractTestObjects.get(i);
			imageStackFrameComparator.testEquals(ddrObject, jextractObject, ImageStackFrameComparator.PROCEDURE_ADDRESS);
		}
	}

	@Test
	public void getProcedureNameTest() throws Throwable {
		for (int i=0; i!= ddrTestObjects.size(); i++) {
			Object ddrObject = ddrTestObjects.get(i);
			Object jextractObject = jextractTestObjects.get(i);
			try {
				imageStackFrameComparator.testEquals(ddrObject, jextractObject, ImageStackFrameComparator.PROCEDURE_NAME);
			} catch (Throwable t) {
				System.err.println("Throwable caught comparing " + ddrObject + " and " + jextractObject);
				throw t;
			}
		}
	}


	
}
