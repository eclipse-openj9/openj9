/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package org.openj9.test.modularity;

import static org.testng.Assert.assertNotNull;
import static org.testng.Assert.assertTrue;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.util.Arrays;

import org.openj9.test.util.FileUtilities;
import org.testng.annotations.AfterMethod;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.BeforeTest;
import org.testng.annotations.Test;

@Test(groups = { "level.extended" })
public class TestClassLoaderFindResource {
	private static final byte[] HELLO_WORLD_BYTES = "hello, world\n".getBytes();
	private static final String TESTFILE_TXT = "testfile.txt";
	private static final File TEMP_DIR = new File(System.getProperty("java.io.tmpdir"));
	private File workDir;
	private File testFile;

	@Test
	public void testFindresourceNullModule() throws IOException {
		try (TestClassLoader ldr = new TestClassLoader(createURLList())) {
			URL res = ldr.findResource(null, TESTFILE_TXT);
			checkResource(res);
		}
	}

	@Test
	public void testFindresourceNonModular() throws IOException {
		try (TestClassLoader ldr = new TestClassLoader(createURLList())) {
			URL res = ldr.findResource(TESTFILE_TXT);
			checkResource(res);
		}
	}

	void checkResource(URL res) throws FileNotFoundException, IOException {
		assertNotNull(res, "Resource not found");
		FileInputStream resStream = new FileInputStream(res.getFile());
		byte[] resultBytes = resStream.readAllBytes();
		resStream.close();
		assertTrue(Arrays.equals(HELLO_WORLD_BYTES, resultBytes));
	}
	
	@BeforeMethod
	public void setup(Method testMethod) throws FileNotFoundException, IOException {
		String methodName = testMethod.getName();
		workDir = new File(TEMP_DIR, methodName);
		workDir.deleteOnExit();
		workDir.mkdir();
		testFile = new File(workDir, TESTFILE_TXT);
		testFile.createNewFile();
		testFile.deleteOnExit();
		try (FileOutputStream s = new FileOutputStream(testFile)) {
			s.write(HELLO_WORLD_BYTES);
		}
	}

	private URL[] createURLList() throws MalformedURLException {
		URL urlList[] = new URL[1];
		urlList[0] = workDir.toURI().toURL();
		return urlList;
	}

	@AfterMethod
	public void teardown() throws IOException {
		FileUtilities.deleteRecursive(workDir);
	}
}
