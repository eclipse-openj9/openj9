/*
 * Copyright IBM Corp. and others 1991
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.j9ddr.tools.xml;


import java.io.File;
import java.util.Iterator;

import org.junit.After;
import org.junit.Before;

import com.ibm.dtfj.image.ImageFactory;
import com.ibm.dtfj.image.ImageSection;
import com.ibm.dtfj.java.JavaHeap;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.j9ddr.view.dtfj.image.J9DDRImageFactory;

public class DTFJHeapSectionUnitTest extends DTFJTest {
	public static final String PROPERTY_CORE_FILE_PATH="core";
	public static final String PROPERTY_OUTPUT_PATH="output";
	
	private boolean testJ9DDR = false;
	private String[] filesToCompare = new String[2];

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
			startTag("<heap name=\"" + heap.getName() + "\">\n");
			writeSections(heap);
			endTag("</heap>\n");
		}
		endTag("</heaps>\n");
		closeWriter();
	}
	
	@SuppressWarnings("unchecked")
	private void writeSections(JavaHeap heap) throws Exception {
		for(Iterator sections = heap.getSections(); sections.hasNext(); ) {
			ImageSection section = (ImageSection) sections.next();
			writeSection(section);
		}
	}
	
	@After
	public void tearDown() throws Exception {
	}
	
}
