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

import java.util.Collections;
import java.util.Comparator;
import java.util.Iterator;
import java.util.List;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.ibm.dtfj.image.ImageModule;
import com.ibm.dtfj.image.ImageSymbol;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.j9ddr.view.dtfj.comparators.ImageSymbolComparator;

public class ImageSymbolTest extends DTFJUnitTest {
	
	@BeforeClass
	public static void classSetup() {
		init();
	}
	
	@Before
	public void setup() {
		initTestObjects();
	}
	
	protected void loadTestObjects(JavaRuntime ddrRuntime, List<Object> ddrObjects, JavaRuntime jextractRuntime, List<Object> jextractObjects) {
		
		Exception ddrException = null;
		Exception jextractException = null;
		
		Iterator<?> ddrLibIt = null;
		try {
			ddrLibIt = ddrProcess.getLibraries();
		} catch (Exception e) {
			e.printStackTrace();
			ddrException = e;
		}
		
		Iterator<?> jextractLibIt = null;
		try {
			jextractLibIt = jextractProcess.getLibraries();
		} catch (Exception e) {
			e.printStackTrace();
			jextractException = e;
		}
		
		if (ddrException != null || jextractException != null) {
			if (ddrException != null && jextractException != null) {
				assertEquals(jextractException.getClass(), ddrException.getClass());
				return;
			}
			
			if (ddrException != null) {
				fail("DDR threw an exception getting libraries and JExtract didn't");
			}
			
			if (jextractException != null) {
				fail("JExtract threw an exception getting libraries and DDR didn't");
			}
		}
		
		slurpSymbolsFromLibraryIterator(ddrLibIt, ddrObjects);
		slurpSymbolsFromLibraryIterator(jextractLibIt, jextractObjects);
		
		Comparator<Object> c = new Comparator<Object>(){

			public int compare(Object o1, Object o2)
			{
				ImageSymbol sym1 = (ImageSymbol)o1;
				ImageSymbol sym2 = (ImageSymbol)o2;
				
				
				return (int)Long.signum(sym1.getAddress().getAddress() - sym2.getAddress().getAddress());
			}
			
		};
		
		Collections.sort(ddrObjects,c);
		Collections.sort(jextractObjects,c);
	}

	private void slurpSymbolsFromLibraryIterator(Iterator<?> it,
			List<Object> list)
	{
		while (it.hasNext()) {
			Object obj = it.next();
			
			if (obj instanceof ImageModule) {
				ImageModule mod = (ImageModule)obj;
				
				Iterator<?> symIt = mod.getSymbols();
				
				while (symIt.hasNext()) {
					Object symObj = symIt.next();
					if (symObj instanceof ImageSymbol) {
						list.add(symObj);
					} else {
						System.err.println("Skipping symbol object: " + symObj + ", type " + symObj.getClass().getName());
					}
				}
			} else {
				System.err.println("Skipping module object: " + obj + ", type " + obj.getClass().getName());
			}
		}
	}

	@Test
	public void getAddressTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageSymbolComparator.testEquals(ddrObj, jextractObj, ImageSymbolComparator.ADDRESS);
		}
	}

	@Test
	public void getNameTest() {
		for (int i=0; i != ddrTestObjects.size(); i++) {
			Object ddrObj = ddrTestObjects.get(i);
			Object jextractObj = jextractTestObjects.get(i);
			imageSymbolComparator.testEquals(ddrObj, jextractObj, ImageSymbolComparator.NAME);
		}
	}
}
