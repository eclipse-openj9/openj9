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
package j9vm.test.intermediateclasscreate;

import j9vm.runner.Runner;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.JarURLConnection;
import java.net.URL;
import java.util.jar.JarFile;

import com.ibm.j9.javaagent.JavaAgent;

/**
 * This test checks that intermediate ROMClass creation failure does not affect VM.
 * It uses a java agent which may or may not be retransformation capable to modify the 
 * class during load and retransformation.
 * It spawns a new java process with java agent(s). 
 * This new java process tries to load a class using invalid classfile bytes provided by a custom classloader. 
 * Java agents intercept the class load event and provides valid classfile bytes.
 * Invalid classfile bytes during class load forces intermediate ROMClass creation process in VM to fail,
 * and causes VM to store the invalid classfile bytes as intermediate class data.
 * 
 * This test runs following commands to verify above behavior in different scenarios:
 * 1) Start the java process without shared class cache. Both retransformation incapable and capable agents are used.
 * 		Retransformation incapable agent (RIA) provides invalid classfile bytes.
 * 		Here it verifies that intermediate ROMClass creation fails and subsequent retransformations are performed correctly. 
 * 2) Start the java process to create a new share class cache.
 * 		This command only serves to create a new shared cache to be used by subsequent commands.
 * 		Note that no java agents are used, hence intermediate ROMClass is not created.
 * 3) Start the java process to re-use a shared cache. Only retransformation capable agent is used.
 * 		This only verifies that retransformations are performed correctly.
 * 		Existing ROMClass in shared cache is used as intermediate ROMClass.
 * 4) Start the java process to re-use a shared cache. Both retransformation incapable and capable agents are used.
 * 		Here it verifies that intermediate ROMClass creation fails and subsequent retransformations are performed correctly.
 * 
 * @author Ashutosh
 *
 */
public class IntermediateClassCreateTestRunner extends Runner {
	private static final int NUM_COMMANDS = 5;
	private static int commandIndex = 0;
	
	public IntermediateClassCreateTestRunner(String className, String exeName,
			String bootClassPath, String userClassPath, String javaVersion) throws IOException {
		super(className, exeName, bootClassPath, userClassPath, javaVersion);
	}

	/* Overrides method in j9vm.runner.Runner. */
	public String getCustomCommandLineOptions() {
		String customOptions = super.getCustomCommandLineOptions();
		URL agentLoc = JavaAgent.class.getResource('/' + JavaAgent.class.getName().replace('.', '/') + ".class");
		JarFile agentJar = null;
		try {
			JarURLConnection connection = (JarURLConnection) agentLoc.openConnection();
			agentJar = connection.getJarFile();
		} catch (IOException e) {
			System.out.println("Unexpected Exception:");
			e.printStackTrace();
		}
		switch(commandIndex) {
		case 0:
			/* run with both retransformation incapable and capable agents but no shared cache */
			customOptions += "-Xtrace:print=tpnid{j9bcu.210}" + " -javaagent:" + agentJar.getName();
			break;
		case 1:
			/* create a new shared cache with no class modification (i.e. no java agents) */
			customOptions += "-Xshareclasses:name=intermediateclasscreatetest,reset";
			break;
		case 2:
			/* reuse shared cache created for 'case 1' with retransformation capable agent. 
			 * Note that this command is not expected to hit tracepoint j9bcu.210
			 */
			customOptions += "-Xnoaot -Xshareclasses:name=intermediateclasscreatetest" + " -javaagent:" + agentJar.getName();
			break;
		case 3:
			/* reuse shared cache created for 'case 1' with both retransformation incapable and capable agents */
			customOptions += "-Xnoaot -Xshareclasses:name=intermediateclasscreatetest -Xtrace:print=tpnid{j9bcu.210}" + " -javaagent:" + agentJar.getName();
			break;
		case 4:
			/* cleanup - destroy the cache */
			customOptions += "-Xshareclasses:name=intermediateclasscreatetest,destroy ";
			break;
		}
		return customOptions;
	}
	
	/* Overrides method in j9vm.runner.Runner. */
	public String getTestClassArguments() {
		String arguments = Integer.toString(commandIndex) + " ";
		switch(commandIndex) {
		case 0:
			/* enable both retransformation incapable and capable agents */
			arguments += IntermediateClassCreateTest.ENABLE_RCT + " " + IntermediateClassCreateTest.ENABLE_RIT;
			break;
		case 2:
			/* enable retransformation capable agent */
			arguments += IntermediateClassCreateTest.ENABLE_RCT;
			break;
		case 3:
			/* enable both retransformation incapable and capable agents */
			arguments += IntermediateClassCreateTest.ENABLE_RCT + " " + IntermediateClassCreateTest.ENABLE_RIT;
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
			if (commandIndex == NUM_COMMANDS-1) {
				/* last command is -Xshareclasses:destroy which returns non-zero exit code */
				if (!success) {
					success = true;
				}
			} else if (commandIndex == 1) {
				/* nothing to analyze here */
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
		BufferedReader in = new BufferedReader(new InputStreamReader(new ByteArrayInputStream(stdErr)));
		boolean result = true;
		boolean foundTrace = false;
		String line = in.readLine();
		while (line != null) {
			line = line.trim();
			/*
			 * Example of j9bcu.210: Trc_BCU_callDynamicLoader_IntermediateROMClassCreationFailed
			 * 	BCU callDynamicLoader: Intermediate ROMClass creation failed with loadData=%p, error code=%zd; using classfile as intermediate data
			 */
			if (line.indexOf("BCU callDynamicLoader: Intermediate ROMClass creation failed") != -1) {
				foundTrace = true;
			}
			if (line.indexOf("FAIL") != -1) {
				result = false;
				break;
			}
			line = in.readLine();
		}
		if (commandIndex == 2 && foundTrace) {
			System.err.println("FAIL: Didn't expect to find trace point j9bcu.210");
			result = false;
		} else if (commandIndex != 2 && !foundTrace) {
			System.err.println("FAIL: Expected trace point j9bcu.210 was not generated");
			result = false;
		}
		
		return result;
	}
}
