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

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.j9ddr.view.dtfj.comparators.ImageModuleComparator;

public class ImageModuleTest extends DTFJUnitTest {
	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		try {
			fillLists(ddrObjects, ddrProcess.getLibraries(), jextractObjects, jextractProcess.getLibraries(), new Comparator<Object>(){

				public int compare(Object o1, Object o2)
				{
					ImageModule mod1 = (ImageModule)o1;
					ImageModule mod2 = (ImageModule)o2;
					
					try {
						return mod1.getName().compareTo(mod2.getName());
					} catch (CorruptDataException e) {
						throw new RuntimeException(e);
					}
				}});
		} catch (Exception ex) {
			throw new RuntimeException(ex);
		}
	}
	
	@Test
	public void getNameTest() throws Throwable {
		try {
			for (int i=0; i != ddrTestObjects.size(); i++) {
				Object ddrObj = ddrTestObjects.get(i);
				Object jextractObj = jextractTestObjects.get(i);
				imageModuleComparator.testEquals(ddrObj, jextractObj, ImageModuleComparator.NAME);
			}
		} catch (Throwable t) {
			
			System.err.println("Throwable thrown comparing module names");
			
			System.err.println("DDR gave us: ");
			
			for (Object ddrObj : ddrTestObjects) {
				ImageModule mod = (ImageModule)ddrObj;
				
				System.err.println(" * " + mod.getName());
			}
			
			System.err.println("JExtract gave us: ");
			
			for (Object ddrObj : jextractTestObjects) {
				ImageModule mod = (ImageModule)ddrObj;
				
				System.err.println(" * " + mod.getName());
			}
			
			throw t;
		}
	}

	@Test
	public void getPropertiesTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageModuleComparator.testEquals(ddrObj, jextractObj, ImageModuleComparator.PROPERTIES);
		}
	}

	@Test
	public void getSectionsTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageModuleComparator.testEquals(ddrObj, jextractObj, ImageModuleComparator.SECTIONS);
		}
	}

	@Test
	public void getSymbolsTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageModuleComparator.testEquals(ddrObj, jextractObj, ImageModuleComparator.SYMBOLS);
		}
	}
}
