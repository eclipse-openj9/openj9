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
package j9vm.test.invalidclasspath;

import j9vm.runner.Runner;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;

/**
 * This test is to ensure correct behavior of com.ibm.oti.shared.SharedClassURLClasspathHelper
 * when its classpath is updated to include an invalid URL
 * using com.ibm.oti.shared.SharedClassURLClasspathHelper.setClasspath() API.
 * 
 * Note that shared class cache recognizes only "file:" or "jar:" protocols in URLs. 
 * Rest all protocols are considered invalid by shared class cache.
 * 
 * After an invalid URL is added to the classpath of com.ibm.oti.shared.SharedClassURLClasspathHelper,
 * any subsequent calls to SharedClassURLClasspathHelper.findSharedClass() and SharedClassURLClasspathHelper.storeSharedClass()
 * should fail immediately, without calling helper native methods.
 * 
 * @author Ashutosh
 */
public class SetClasspathTestRunner extends Runner {
	private static final int NUM_COMMANDS = 3;
	private static int commandIndex = 0;
	
	String invalidURLMsgFromValidateURL = "URL "+ SetClasspathTest.INVALID_URL1 + " does not have required file or jar protocol.";
	String invalidURLMsgFromFind = "Classpath contains an invalid URL. Returning null.";
	String invalidURLMsgFromStore = "Classpath contains an invalid URL. Returning false.";
	String invalidURLMsgFromNative = "j9jcl.423";	/* Trc_JCL_com_ibm_oti_shared_getCpeTypeForProtocol_UnknownProtocol */
	
	/* Following classes are loaded by SetClasspathTest and get stored in shared cache as ROMCLASS */ 
	private static String romClassList[] = { "TestA1", "TestB3", "TestA4" };
	
	/* Following classes are loaded by SetClasspathTest and get stored in shared cache as ORPHAN */
	private static String orphanClassList[] = { "TestA2", "TestB1", "TestB2", "TestC1", "TestA3" };
	
	/*
	 * Number of times SetClasspathTest modifies classpath of SharedClassURLClasspathHelper 
	 * such that its classpath contains atleast one invalid URL.
	 */
	private static int numInvalidURLTransformations = 4;
	
	public SetClasspathTestRunner(String className, String exeName,
			String bootClassPath, String userClassPath, String javaVersion) throws IOException {
		super(className, exeName, bootClassPath, userClassPath, javaVersion);
	}

	/* Overrides method in j9vm.runner.Runner. */
	public String getCustomCommandLineOptions() {
		String customOptions = super.getCustomCommandLineOptions();
		switch(commandIndex) {
		case 0:
			/* create a cold cache */
			customOptions += "-Xshareclasses:name=setclasspathtest,reset,verboseHelper -Xtrace:print={j9jcl.423} ";
			break;
		case 1:
			/* display orphan and romclass entries in the cache */
			customOptions += "-Xshareclasses:name=setclasspathtest,printAllStats=orphan+romclass ";
			break;
		case 2:
			/* cleanup - destroy the cache */
			customOptions += "-Xshareclasses:name=setclasspathtest,destroy ";
			break;
		}
		return customOptions;
	}
	
	/* Overrides method in j9vm.runner.Runner. */
	public boolean run() {
		boolean success = false;
		for (int i = 0; i < NUM_COMMANDS; i++) {
			commandIndex = i;
			success = super.run();
			if ((commandIndex == 1) || (commandIndex == NUM_COMMANDS - 1)) {
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
		int invalidURLMsgFromValidateURLFound = 0, invalidURLMsgFromFindFound = 0, invalidURLMsgFromStoreFound = 0;
		boolean invalidURLMsgFromNativeFound = false;
		
		if (commandIndex == 0) {
			while (line != null) {
				/* analyze output of running SetClasspathTest */
				if (line.indexOf(invalidURLMsgFromValidateURL) != -1) {
					invalidURLMsgFromValidateURLFound += 1;
				}
				if (line.indexOf(invalidURLMsgFromFind) != -1) {
					invalidURLMsgFromFindFound += 1;
				}
				if (line.indexOf(invalidURLMsgFromStore) != -1) {
					invalidURLMsgFromStoreFound += 1;
				}
				if (line.indexOf(invalidURLMsgFromNative) != -1) {
					invalidURLMsgFromNativeFound = true;
				}
				line = in.readLine();
			}
			System.out.println("\n");
			if (invalidURLMsgFromValidateURLFound != numInvalidURLTransformations) {
				System.out.println("Error:: Expected message \""
						+ invalidURLMsgFromValidateURL + "\" to appear "
						+ numInvalidURLTransformations
						+ " times but found " + invalidURLMsgFromValidateURLFound + " times");
				result = false;
			}
			if (invalidURLMsgFromFindFound != orphanClassList.length) {
				System.out.println("Error:: Expected message \""
						+ invalidURLMsgFromFind + "\" to appear "
						+ orphanClassList.length
						+ " times but found " + invalidURLMsgFromFindFound + " times");
				result = false;
			}
			if (invalidURLMsgFromStoreFound != orphanClassList.length) {
				System.out.println("Error:: Expected message \""
						+ invalidURLMsgFromFind + "\" to appear "
						+ orphanClassList.length
						+ " times but found " + invalidURLMsgFromStoreFound + " times");
				result = false;
			}
			if (invalidURLMsgFromNativeFound) {
				System.out.println("Error:: Did not expect message: \""
						+ invalidURLMsgFromNative + "\" to appear.");
				result = false;
			}
		} else if (commandIndex == 1) {
			boolean validROMClassListFlag[] = new boolean[romClassList.length];
			boolean orphanClassListFlag[] = new boolean[orphanClassList.length];
			boolean invalidROMClassListFlag[] = new boolean[orphanClassList.length];
			
			while (line != null) {
				for (int i = 0; i < romClassList.length; i++) {
					if (line.indexOf("ROMCLASS: " + romClassList[i]) != -1) {
						validROMClassListFlag[i] = true;
					}
				}				
				for (int i = 0; i < orphanClassList.length; i++) {
					if (line.indexOf("ORPHAN: " + orphanClassList[i]) != -1) {
						orphanClassListFlag[i] = true;
					}
					if (line.indexOf("ROMCLASS: " + orphanClassList[i]) != -1) {
						invalidROMClassListFlag[i] = true;
					}
				}
				line = in.readLine();
			}
			System.out.println("\n");
			for (int i = 0; i < validROMClassListFlag.length; i++) {
				if (validROMClassListFlag[i]) {
					System.out.println("Info:: Found ROMClass entry for " + romClassList[i] + " in shared cache");	
				} else {
					System.out.println("Error:: Did not find ROMClass entry for " + romClassList[i] + " in shared cache");
					result = false;
				}
			}
			for (int i = 0; i < orphanClassListFlag.length; i++) {
				if (orphanClassListFlag[i]) {
					System.out.println("Info:: Found Orphan entry for " + orphanClassList[i] + " in shared cache");	
				} else {
					System.out.println("Error:: Did not find Orphan entry for " + orphanClassList[i] + " in shared cache");
					result = false;
				}
			}
			for (int i = 0; i < invalidROMClassListFlag.length; i++) {
				if (invalidROMClassListFlag[i]) {
					System.out.println("Error:: Did not expect to find ROMClass entry for " + orphanClassList[i] + " in shared cache");
					result = false;
				}
			}
		}
		return result;
	}
}
