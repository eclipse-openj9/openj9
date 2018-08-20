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
package j9vm.test.nestedJar;

import j9vm.runner.Runner;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.File;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.file.attribute.FileTime;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.nio.file.Path;
import java.net.URI;
import java.net.URISyntaxException;

/**
 * This test is to ensure correct behavior of shared classes on nested jars.
 */
public class NestedJarFileTestRunner extends Runner {
	private static final int NUM_COMMANDS = 5;
	private static int commandIndex = 0;
	
	public NestedJarFileTestRunner(String className, String exeName,
			String bootClassPath, String userClassPath, String javaVersion) throws IOException {
		super(className, exeName, bootClassPath, userClassPath, javaVersion);
	}

	/* Overrides method in j9vm.runner.Runner. */
	public String getCustomCommandLineOptions() {
		String customOptions = super.getCustomCommandLineOptions();
		switch(commandIndex) {
		case 0:
			/* The classes from nested jar should be stored to the shared cache */
			customOptions += "-Xshareclasses:name=nestedjarfiletest,reset,verboseHelper ";
			break;
		case 1:
			/* The classes from nested jar should be found in the shared cache, add noTimestampChecks as this test 
			 * will fail if somebody else change the timestamp of VM_Test.jar */
			customOptions += "-Xshareclasses:name=nestedjarfiletest,noTimestampChecks,verboseIO ";
			break;
		case 2:
			/* touch the jar file and load another class */
			customOptions += "-Xshareclasses:name=nestedjarfiletest,verboseHelper ";
			break;
		case 3:
			/* Check stale classes */
			customOptions += "-Xshareclasses:name=nestedjarfiletest,printStats=stale+classpath ";
			break;
		case 4:
			/* cleanup - destroy the cache */
			customOptions += "-Xshareclasses:name=nestedjarfiletest,destroy ";
			break;
		}
		return customOptions;
	}
	
	/* Overrides method in j9vm.runner.Runner. */
	public String getTestClassArguments() {
		String arguments = "";
		switch(commandIndex) {
		case 0:
		case 1:
			arguments += "load3Classes";
			break;
		case 2:
			arguments += "load1Class";
			break;
		default:
			break;
		}
		return arguments;
	}
	
	public boolean touchCurrentJar() {
		URI JarPath;
		try {
			JarPath = NestedJarFileTestRunner.class.getProtectionDomain().getCodeSource().getLocation().toURI();
		} catch (NullPointerException | URISyntaxException e) {
			e.printStackTrace();
			System.out.println("Error: Exception: Failed to get the path of the current jar");	
			return false;
		}
		if (null != JarPath) {
			Path p = Paths.get(JarPath);
			try {
				FileTime fileTime = Files.getLastModifiedTime(p);
				long newTime = fileTime.toMillis() + 1000;
				Files.setLastModifiedTime(p, FileTime.fromMillis(newTime));
			} catch (IOException e) {
				e.printStackTrace();
				System.out.println("Error: IOException: Failed to touch " + JarPath);
				return false;
			}
		}
		return true;
	}

	/* Overrides method in j9vm.runner.Runner. */
	public boolean run() {
		boolean success = false;
		for (int i = 0; i < NUM_COMMANDS; i++) {
			commandIndex = i;
			if (2 == i) {
				touchCurrentJar();
			}
			success = super.run();
			if ((commandIndex == NUM_COMMANDS -2) || (commandIndex == NUM_COMMANDS - 1)) {
				/* -Xshareclasses:printStats and -Xshareclasses:destroy returns non-zero exit code */
				if (!success) {
					success = true;
				}
			}
			if ((success == true) && (commandIndex != NUM_COMMANDS - 1)) {
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
		boolean result = true;
		String line = in.readLine();
		if (1 == commandIndex) {
			String romClassList[] = {"TestA2"};
			/* Check class TestA2 only here. Because classes TestA1, TestC1 are the first class from their URL entry. The first class from a URL will always be 
			 * loaded from disk even it is in the shared cache. 
			 */ 
			boolean romClassFound[] = new boolean[romClassList.length];

			while (line != null) {
				for (int i = 0; i < romClassList.length; i++) {
					if (line.indexOf("Found class " + romClassList[i] + " in shared cache") != -1) {
						romClassFound[i] = true;
					}	
				}
				line = in.readLine();
			}
			for (int i = 0; i < romClassList.length; i++) {
				if (!romClassFound[i]) {
					System.out.println("Error: Class " + romClassList[i] + " should be found in the shared cache");
					result = false;
				}
			}
		} else if (2 == commandIndex) {
			boolean loadedTestB1 = false;
			while (line != null) {
				if (line.indexOf("loadded TestB1") != -1) {
					loadedTestB1 = true;
				}
				line = in.readLine();
			}
			if (!loadedTestB1) {
				System.out.println("Error: Failed to load class TestB1");
				result = false;
			}
		} else if (3 == commandIndex) {
			String nestJarURLs[] = {"VM_Test.jar!/InvalidClasspathResource1.jar", "VM_Test.jar!/InvalidClasspathResource2.jar", "VM_Test.jar!/InvalidClasspathResource3.jar"};
			String staleRomClassList[] = {"TestA1", "TestA2", "TestC1"};
			String NonStaleRomClass = "TestB1";
			boolean staleClassFound[] = new boolean[staleRomClassList.length];
			boolean nestJarURLsFound[] = new boolean[nestJarURLs.length];

			while (line != null) {
				if (line.indexOf("ROMCLASS: " + NonStaleRomClass) != -1) {
					System.out.println("Error: Class " + NonStaleRomClass + " should not be marked as stale");
					result = false;
				} else {
					for (int i = 0; i < staleRomClassList.length; i++) {
						if (line.indexOf("ROMCLASS: " + staleRomClassList[i]) != -1) {
							staleClassFound[i] = true;
						}	
					}
					for (int i = 0; i < nestJarURLs.length; i++) {
						if (File.separatorChar != '/') {
							nestJarURLs[i] = nestJarURLs[i].replace('/', '\\');
						}
						if (line.indexOf(nestJarURLs[i]) != -1) {
							nestJarURLsFound[i] = true;
						}	
					}
				}
				line = in.readLine();
			}
			for (int i = 0; i < staleRomClassList.length; i++) {
				if (!staleClassFound[i]) {
					System.out.println("Error: Class " + staleRomClassList[i] + " should be marked as stale");
					result = false;
				}
			}
			for (int i = 0; i < nestJarURLs.length; i++) {
				if (!nestJarURLsFound[i]) {
					System.out.println("Error: Classpath " + nestJarURLs[i] + " should be found in the cache");
					result = false;
				}
			}
		}
		return result;
	}
}
