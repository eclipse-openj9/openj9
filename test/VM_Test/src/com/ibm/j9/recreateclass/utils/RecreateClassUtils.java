/*******************************************************************************
 * Copyright (c) 2001, 2016 IBM Corp. and others
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
package com.ibm.j9.recreateclass.utils;

import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.net.JarURLConnection;
import java.net.URL;
import java.net.URLConnection;

import com.ibm.j9.cfdump.tests.utils.Utils;

import junit.framework.Assert;

/**
 * Provides a set of utility methods to be used by com.ibm.j9.recreateclass.tests classes.
 *
 * @author ashutosh
 */
@SuppressWarnings("nls")
public class RecreateClassUtils {

	private static final String cfdumpSuccess = "Wrote \\d+ bytes to output file \\S+\\.j9class";
	private static final String retainClassFiles = System.getProperty("com.ibm.j9.recreateClass.retainClassFiles");

	/* Checks cfdump output to verify class file recreation (or transformation)
	 * passed successfully.
	 */
	public static void checkCfdumpOutput(Process cfdump) throws Exception {
		try (BufferedReader in = Utils.getBufferedErrorStreamOnly(cfdump)) {
			for (String line; (line = in.readLine()) != null;) {
				System.out.println("cfdump: " + line);
				if (!line.matches(cfdumpSuccess)) {
					Assert.fail("cfdump did not return expected message: " + cfdumpSuccess);
					break;
				}
			}
		}

		/* Check for 0 exit code. */
		Assert.assertEquals(0, cfdump.waitFor());
	}

	/* Runs cfdump tool on a class file with given name to recreate the class data
	 * Recreated class file is created in current working directory
	 */
	public static void runCfdump(String classFileName) throws Exception {
		File classFile = new File(classFileName + ".class");
		Assert.assertTrue("Failed to create " + classFileName + ".class file",
				classFile.exists());
		String args = "-t:" + System.getProperty("user.dir")
				+ System.getProperty("file.separator") + classFileName;
		Process cfdump = Utils.invokeCfdumpOnFile(classFile, args);
		checkCfdumpOutput(cfdump);
	}

	/* Deletes the gives File provided the system property
	 * com.ibm.j9.recreateClass.retainClassFiles is set.
	 */
	public static void deleteFile(File f) {
		if ((retainClassFiles != null) && (retainClassFiles.equals("true"))) {
			/* retain class files */
			return;
		}
		if (f != null && f.exists()) {
			f.delete();
		}
	}

	/* Reads a class file from the jar file from which the class was loaded
	 * and creates a local copy in current working directory.
	 */
	public static void createClassFile(Class<?> testClass, String classFileName)
			throws Exception {
		URL url = testClass.getResource(classFileName + ".class");
		if (url != null) {
			URLConnection connection = url.openConnection();
			if (connection instanceof JarURLConnection) {
				JarURLConnection urlConnection = (JarURLConnection) connection;
				urlConnection.connect();
				try (InputStream inStream = urlConnection.getInputStream()) {
					File outFile = new File(classFileName + ".class");

					try (FileOutputStream outStream = new FileOutputStream(outFile)) {
						byte[] data = new byte[1024];
						int size;
						while ((size = inStream.read(data)) > 0) {
							outStream.write(data, 0, size);
						}
					}
				}
			}
		}
	}

	/* Stores a byte array representing the class data in a class file named 'classFileName'
	 * in current working directory.
	 */
	public static void createClassFile(byte[] classData, String classFileName)
			throws Exception {
		File classFile = new File(classFileName + ".class");
		try (BufferedOutputStream bos = new BufferedOutputStream(new FileOutputStream(classFile))) {
			bos.write(classData);
		}
	}

}
