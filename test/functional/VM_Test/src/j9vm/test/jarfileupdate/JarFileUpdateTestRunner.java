/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package j9vm.test.jarfileupdate;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

import j9vm.runner.Runner;

/**
 * This test ensure that any update to a jar file are picked up by classloaders (see CMVC 201090).
 * This test runs following commands:
 * 1. First command creates a new shared cache and loads a class 'C' from a jar file 'F' using custom classloader.
 * 2. Second commands re-uses the shared cache. This command creates two classloaders. 
 *    Each classloader loads same class 'C'. 
 *    The first classloader loads the class from the shared cache. 
 *    After the first classloader has completed loading the class, the container jar file is updated by a different version of 'C'.
 *    Then the second classloader tries to load the class.
 *    This test ensures that the second classloader does not find the class 'C' in shared cache, 
 *    but instead it loads the modified version of the class from the jar file present on the disk.
 * 
 * @author Ashutosh
 */
public class JarFileUpdateTestRunner extends Runner {
	private static final int NUM_COMMANDS = 3;
	private static int commandIndex = 0;
	private static String resourceJar1 = "JarFileUpdateTestRunnerResource1";
	private static String resourceJar2 = "JarFileUpdateTestRunnerResource2";
	private static String jarFile1, jarFile2;
	
	/* This is same string as printed by Sample.foo() present in data/JarFileUpdateTestRunnerResource1.jar */
	private static String EXPECTED_OUTPUT_FROM_JAR1 = "In Sample.foo";
	
	private static String FAILED_TO_FIND_CLASS_IN_CACHE = "Failed to find class in shared class cache, loading from disk";
	
	/* This is same string as printed by Sample.foo() present in data/JarFileUpdateTestRunnerResource2.jar */
	private static String EXPECTED_OUTPUT_FROM_JAR2 = "In modified Sample.foo";

	public String copyResourceJar(String jarfile) throws IOException {
		InputStream in = this.getClass().getClassLoader().getResourceAsStream(jarfile + ".jar");
		ByteArrayOutputStream baos = new ByteArrayOutputStream();
		int count = -1;
		byte[] data = new byte[128];
		while ((count = in.read(data)) != -1) {
			baos.write(data, 0, count);
		}
		in.close();
		
		File tempFile = File.createTempFile(jarfile, ".jar");
		tempFile.deleteOnExit();
		FileOutputStream fos = new FileOutputStream(tempFile);
		fos.write(baos.toByteArray());
		fos.close();
		
		return tempFile.getAbsolutePath();
	}
	
	public JarFileUpdateTestRunner(String className, String exeName,
			String bootClassPath, String userClassPath, String javaVersion) throws IOException {
		super(className, exeName, bootClassPath, userClassPath, javaVersion);
		jarFile1 = copyResourceJar(resourceJar1);
		System.out.println("Copied " + resourceJar1 + " to " + jarFile1);
		jarFile2 = copyResourceJar(resourceJar2);
		System.out.println("Copied " + resourceJar2 + " to " + jarFile2);
	}
	
	/* Overrides method in j9vm.runner.Runner. */
	public String getCustomCommandLineOptions() {
		String customOptions = super.getCustomCommandLineOptions();
		switch(commandIndex) {
		case 0:
			/* create a cold cache */
			customOptions += "-Xshareclasses:name=jarfileupdatetest,reset ";
			break;
		case 1:
			/* use warm cache created by previous command */
			customOptions += "-Xshareclasses:name=jarfileupdatetest ";
			break;
		case 2:
			/* cleanup - destroy the cache */
			customOptions += "-Xshareclasses:name=jarfileupdatetest,destroy ";
			break;
		}
		return customOptions;
	}
	
	/* Overrides method in j9vm.runner.Runner. */
	public String getTestClassArguments() {
		String arguments = "";
		switch(commandIndex) {
		case 0:
			arguments += "coldcache " + jarFile1;
			break;
		case 1:
			arguments += "warmcache " + jarFile1 + " " + jarFile2;
			break;
		default:
			break;
		}
		return arguments;
	}
	
	/* Overrides method in j9vm.runner.Runner. */
	public boolean run() {
		boolean success = false;
		for (int i = 0; i < NUM_COMMANDS; i++) {
			commandIndex = i;
			success = super.run();
			if (commandIndex == 2) {
				/* -Xshareclasses:destroy returns non-zero exit code */
				if (!success) {
					success = true;
				}
			} else if (success == true) {
				byte[] stdOut = inCollector.getOutputAsByteArray();
				byte[] stdErr = errCollector.getOutputAsByteArray();
				try {
					success = analyze(stdOut, stdErr);
				} catch (Exception e) {
					success = false;
					System.out.println("Unexpected Exception:");
					e.printStackTrace();
				}
			}
			if (success == false) {
				break;
			}
		}
		return success;
	}
	
	public boolean analyze(byte[] stdOut, byte[] stdErr) throws IOException {
		BufferedReader in = new BufferedReader(new InputStreamReader(
				new ByteArrayInputStream(stdErr)));
		boolean match = false;
		String line = in.readLine();
		while (line != null) {
			if (commandIndex == 0) {
				if (line.equals(EXPECTED_OUTPUT_FROM_JAR1)) {
					return true;
				}
			} else if (commandIndex == 1) {
				if (line.equals(FAILED_TO_FIND_CLASS_IN_CACHE)) {
					match = true;
				}
				if (match && line.equals(EXPECTED_OUTPUT_FROM_JAR2)) {
					return true;
				}
			}
			line = in.readLine();
		}
		return match;
	}
}
