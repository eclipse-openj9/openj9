/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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
package com.ibm.j9ddr.tools.xml;


import java.io.File;
import java.util.HashSet;
import java.util.Iterator;

import org.junit.After;
import org.junit.Before;

import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.java.JavaClass;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaObject;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageFactory;

public class DTFJHeapUnitTest extends DTFJTest {
	public static final String PROPERTY_CORE_FILE_PATH="core";
	public static final String PROPERTY_OUTPUT_PATH="output";
	
	private boolean testJ9DDR = false;
	private String[] filesToCompare = new String[2];
	
	private HashSet<JavaClass> componentTypes = new HashSet<JavaClass>();
	
	@Override
	public String getConfigPath() {
		return "data/xpath configs/DTFJHeapUnitTest.properties";
	}

	@Override
	public String[] getFilesToCompare() {
		return filesToCompare;
	}

	@Override
	public ImageFactory getImageFactory() {
		if(testJ9DDR) {
			return new J9DDRImageFactory();
		} else {
			return new com.ibm.dtfj.image.j9.ImageFactory();
		}
	}
	
	@Before
	public void setUp() throws Exception {
		File core = parseCoreFilePath(getSystemProperty(PROPERTY_CORE_FILE_PATH));
		File output = new File(getSystemProperty(PROPERTY_OUTPUT_PATH));
		if(!output.exists()) {
			output.mkdirs();
		}
		JavaRuntime rt = getRuntime(core);
		File jxoutput = new File(output,"dtfj.xml");
		filesToCompare[0] = jxoutput.getPath();
		generateXML(jxoutput, rt);
		testJ9DDR = true;
		rt = getRuntime(core);
		File ddroutput = new File(output,"ddr.xml");
		filesToCompare[1] = ddroutput.getPath();
		generateXML(ddroutput, rt);
	}

	private String getSystemProperty(String key) {
		String value = System.getProperty(key);
		if(value == null) {
			throw new IllegalArgumentException("The system property " + key + " has not been defined");
		}
		System.out.println("Property " + key + " = " + value);
		return value;
	}
	
	@SuppressWarnings("unchecked")
	public void generateXML(File path, JavaRuntime rt) throws Exception {
		createWriter(path);
		startTag("<heaps>\n");
		for(Iterator heaps = rt.getHeaps(); heaps.hasNext(); ) {
			JavaHeap heap = (JavaHeap) heaps.next();
			writeIndent();
			startTag("<heap name=\"" + heap.getName() + "\">\n");
			long count = 0;
			for(Iterator objects = heap.getObjects(); objects.hasNext(); count++ ) {
				try {
					JavaObject obj = (JavaObject)objects.next();
					if(((count % 100) == 0) || (obj.isArray())) {
							writeObject(obj, count);
					}
				} catch (CorruptDataException e) {
					write("<!-- corrupt object @ " + e.getCorruptData().getAddress() + " -->");
				}
			}
			startTag("<objects count=\"" + count + "\">\n");
			endTag("</objects>\n");
			endTag("</heap>\n");
		}
		endTag("</heaps>\n");
		closeWriter();
	}
	
	@After
	public void tearDown() throws Exception {
	}
	
}
