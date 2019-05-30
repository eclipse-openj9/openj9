/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.dtfj.java.JavaVMOption;

public class JavaVMInitArgsTest extends DTFJUnitTest {
	
	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		
		// TODO: Can probably implement this better or re-use something like DTFJunitTest.invokeMethod()
		boolean ddrDataUnavailable = false;
		boolean ddrCorruptData = false;
		boolean jextractDataUnavailable = false;
		boolean jextractCorruptData = false;
		
		try {
			ddrObjects.add(ddrJavaRuntime.getJavaVMInitArgs());
		} catch (DataUnavailable e) {
			ddrDataUnavailable = true;
		} catch (CorruptDataException e) {
			ddrCorruptData = true;
		}
		
		try {
			jextractObjects.add(jextractJavaRuntime.getJavaVMInitArgs());
		} catch (DataUnavailable e) {
			jextractDataUnavailable = true;
		} catch (CorruptDataException e) {
			jextractCorruptData = true;
		}
		
		assertEquals(jextractDataUnavailable, ddrDataUnavailable);
		assertEquals(jextractCorruptData, ddrCorruptData);
	}
	
	@Test
	public void getVersion() {
		javaVMInitArgsComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getVersion");
	}
	
	@Test
	public void getIgnoreUnrecognized() {
		javaVMInitArgsComparator.testJavaEquals(ddrTestObjects, jextractTestObjects, "getIgnoreUnrecognized");
	}

	@Test
	public void getOptions() {
		javaVMOptionComparator.testComparatorIteratorEquals(ddrTestObjects, jextractTestObjects, "getOptions", JavaVMOption.class);
	}
}
