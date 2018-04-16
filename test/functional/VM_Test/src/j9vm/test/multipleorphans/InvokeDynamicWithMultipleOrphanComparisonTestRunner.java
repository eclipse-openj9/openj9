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
package j9vm.test.multipleorphans;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStreamReader;

import j9vm.runner.Runner;

/**
 * This test is for Problem Report Jazz103 81977: [was 156993] Bytecode modification causes JVM crash on Liberty/Java 8.
 * It verifies that ROMClass comparison done during creation of a ROMClass does not invalidate parameters of invokedynamic bytecode.
 * ROMClass comparison is done when JVM finds one or more class of same name as the class being built in shared class cache. 
 * During comparison if the bytecode fixup is executed multiple times then the parameter of invokedynamic bytecode can exceed
 * the size of callsite table of the ROMClass.
 * 
 * This test runs following commands to verify above behavior in different scenarios:
 * 	1) Destroy any existing shared class cache.
 * 	2) Create a new shared cache using Java class with name C which has invokedynamic bytecode, 
 * 		but ensure that class C is only stored as ORPHAN in the shared cache.
 * 		An ORPHAN class in shared cache cannot be searched by JVM using class name. It can be re-used only if byte-by-byte comparison succeeds.
 * 	3) Run -Xshareclasses:printstats and verify that class C is present as ORPHAN in the cache.
 * 	4) Using same shared cache verify that another class with name C can be loaded by another JVM instance without any problems.
 * 		This step should result in loading C and performing byte-by-byte comparison with the ORPHAN for class C in the shared class cache.
 * 
 * @author Ashutosh
 */
public class InvokeDynamicWithMultipleOrphanComparisonTestRunner extends Runner {

	public static final int NUM_COMMANDS = 4;
	public static int commandIndex = 0;
	
	public InvokeDynamicWithMultipleOrphanComparisonTestRunner(
			String className, String exeName, String bootClassPath,
			String userClassPath, String javaVersion) {
		super(className, exeName, bootClassPath, userClassPath, javaVersion);
	}
	
	/* Overrides method in Runner. */
	public String getCustomCommandLineOptions() {
		String customOptions = super.getCustomCommandLineOptions();
		if (commandIndex == 0) {
			/* Enable j9bcu.131 tracepoint which asserts that invokedynamic parameter is a valid index in callsite table */ 
			customOptions += " -Xshareclasses:enableBCI,name=invokedynamictestcache,reset"
					+ " -Xtrace:print=tpnid{j9bcu.131}";
		} else if (commandIndex == 1) {
			customOptions += " -Xshareclasses:enableBCI,name=invokedynamictestcache,printStats=orphan+romclass";
		} else if (commandIndex == 2) {
			/* Enable j9bcu.131 tracepoint which asserts that invokedynamic parameter is a valid index in callsite table */
			customOptions += " -Xshareclasses:enableBCI,name=invokedynamictestcache"
					+ " -Xtrace:print=tpnid{j9bcu.131}";
		} else if (commandIndex == 3) {
			/* cleanup the cache */
			customOptions += " -Xshareclasses:enableBCI,destroy,name=invokedynamictestcache";
		}
		return customOptions;
	}
	
	/* Overrides method in j9vm.runner.Runner. */
	public boolean run() {
		boolean success = false;
		for (int i = 0; i < NUM_COMMANDS; i++) {
			commandIndex = i;
			success = super.run();
			if (commandIndex == NUM_COMMANDS-1) {
				/* Last command is -Xshareclasses:destroy which returns non-zero exit code */
				if (!success) {
					success = true;
				}
			} else {
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
				return false;
			}
		}
		return success;
	}

	public boolean analyze(byte[] stdOut, byte[] stdErr) throws IOException {
		BufferedReader in = new BufferedReader(new InputStreamReader(
				new ByteArrayInputStream(stdErr)));
		boolean result = false;
		String line = in.readLine();
		while (line != null) {
			line = line.trim();
			if (commandIndex == 0 || commandIndex == 2) {
				/* Did we hit the assert for invokedynamic parameter to be a valid index in callsite table? */
				if (line.indexOf("j9bcu.131") != -1) {
					System.err.println("FAIL: Assertion occurred in java process.");
					return false;
				}
				if (line.indexOf(InvokeDynamicWithMultipleOrphanComparisonTest.PASS_STRING) != -1) {
					System.out.println("PASS: pass string found");
					result = true;
				}
				if (line.indexOf(InvokeDynamicWithMultipleOrphanComparisonTest.FAIL_STRING) != -1) {
					System.err.println("FAIL: Exception occurred");
					return false;
				}
			} else if (commandIndex == 1) {
				/* Scan -Xshareclasses:printStats for an ORPHAN entry of the InvokeDynamicTest class */
				if (line.indexOf("ORPHAN: " + InvokeDynamicTestGenerator.GENERATED_CLASS_NAME.replace('.', '/') + " ") != -1) {
					System.out.println("PASS: Found ORPHAN entry for class InvokeDynamicTest in the shared class cache");
					result = true;
				}
				/* There should not be any ROMCLASS entry of the InvokeDynamicTest class in shared class cache */
				if (line.indexOf("ROMCLASS: " + InvokeDynamicTestGenerator.GENERATED_CLASS_NAME.replace('.', '/') + " ") != -1) {
					System.err.println("FAIL: Did not expect to find ROMCLASS entry for class InvokeDynamicTest in the shared class cache");
					return false;
				}
			}
			line = in.readLine();
		}
		if (!result) {
			if (commandIndex == 1) {
				System.err.println("FAIL: Did not find ORPHAN entry for class InvokeDynamicTest in the shared class cache");
			}
		}
		System.out.println();
		return result;
	}
}
