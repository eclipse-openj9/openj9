/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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

package com.ibm.j9.recreateclass.tests;

import java.io.File;
import java.lang.reflect.Method;
import java.nio.file.Files;

import junit.framework.TestCase;

import com.ibm.j9.javaagent.JavaAgent;
import com.ibm.j9.recreateclass.testclasses.JSRRETStressTestGenerator;
import com.ibm.j9.recreateclass.testclasses.JSRRETStressTestLoader;
import com.ibm.j9.recreateclass.utils.RecreateClassUtils;

/**
 * This class has a test to verify class file recreation code for a class file
 * containing multiple occurrences of JSR-RET bytecodes.
 * The test works as follows:
 * 	1. Create a class file JSRRETStressTest which has JSR-RET bytecodes using ASM library.
 *  2. Call JSRRETStressTest.method(int) multiple times with different argument values.
 *  3. Use cfdump tool to recreate JSRRETStressTest.
 *  4. Redefine JSRRETStressTest by using the recreated class file data.
 *  5. Call JSRRETStressTest.method(int) multiple times with different argument values, and verify
 *     the returned values match with those obtained in step 2.
 *     
 * Original and recreated class files are created in current working directory.
 * By default, these class files are deleted once the test is over.
 * To retain the .class files, set the property -Dcom.ibm.j9.recreateClass.retainClassFiles=true.
 *     
 * @author ashutosh
 * 
 */
public class JSRRETRecreateClassTest extends TestCase {
	File classFile;
	File j9classFile;
	String classFileName = "JSRRETStressTest";

	private void deleteClassFiles() {
		RecreateClassUtils.deleteFile(classFile);
		RecreateClassUtils.deleteFile(j9classFile);
	}

	public byte[] getRecreatedClassData() throws Exception {
		RecreateClassUtils.runCfdump(classFileName);
		classFile = new File(classFileName + ".class");
		j9classFile = new File(classFileName + ".j9class");
		assertTrue("cfdump succeeded but " + classFileName
				+ ".j9class does not exist", j9classFile.exists());
		return Files.readAllBytes(j9classFile.toPath());
	}

	public void setUp() {
		System.out.println("\n");
	}

	public void testJSRRET() throws Exception {
		boolean expected[], found[];
		System.out.println("Start testJSRRET");
		try {
			byte classData[] = JSRRETStressTestGenerator.generateClassData();
			RecreateClassUtils.createClassFile(classData, classFileName);
			JSRRETStressTestLoader loader = new JSRRETStressTestLoader(
					classData);
			Class<?> JSRRETStressTest = loader
					.loadClass("com.ibm.j9.recreateclasscompare.testclasses.JSRRETStressTest");
			Object obj = JSRRETStressTest.newInstance();
			Method method = JSRRETStressTest.getMethod("method",
					new Class[] { int.class });
			expected = new boolean[10];
			for (int i = 0; i < 10; i++) {
				expected[i] = (Boolean) method.invoke(obj,
						new Object[] { Integer.valueOf(i) });
			}
			byte[] recreatedClassData = getRecreatedClassData();
			JavaAgent.redefineClass(JSRRETStressTest, recreatedClassData);
			found = new boolean[10];
			for (int i = 0; i < 10; i++) {
				found[i] = (Boolean) method.invoke(obj,
						new Object[] { Integer.valueOf(i) });
				System.out.println("Invoking JSRRETStressTest.method(" + i
						+ ")\t expected: " + expected[i] + "\t found: "
						+ found[i]);
				assertEquals(expected[i], found[i]);
			}
		} finally {
			deleteClassFiles();
		}
		System.out.println("End testJSRRET");
	}

}
