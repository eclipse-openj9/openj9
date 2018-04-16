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
package com.ibm.j9.numa;

import j9vm.runner.OutputCollector;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;

public class NumaTestWrapper {
	public static void main(String[] args) {
		int errorCode = 0;
		String command = parseCommand(args);
		
		try {
			String platform = System.getProperty("os.name").toLowerCase();
			boolean checkTracepoints = ((null != platform) && (command.indexOf("gcpolicy:balanced") != -1) && (platform.indexOf("linux") != -1));
			
			Process proc = Runtime.getRuntime().exec(command);
			InputStream errStream = proc.getErrorStream();
			OutputCollector errCollector = new OutputCollector(new BufferedInputStream(errStream));
			
			errCollector.start();
			errorCode = proc.waitFor();
			errCollector.join();
			
			if (0 != errorCode) {
				throw new Error("Process for NumaTest didn't run properly.");
			}
			
			/* Analyze the error stream to check if forkAndExecNativeV8 entry and exit tracepoints were executed */
			if (checkTracepoints) {
				if (!verifyTracepoints(errCollector.getOutputAsByteArray())) {
					throw new Error("MM_RuntimeExecManager::forkAndExecNativeV8 wasn't run. "
							+ "In Oracle JCL, the definition or the location of native method forkAndExec may have changed. "
							+ "Otherwise, it may just be an issue with tracepoints.");
				}
			}
			
			System.err.println("NumaTestWrapper - TEST PASSED");
			System.exit(0);
		} catch (IOException e) {
			e.printStackTrace();
		} catch (InterruptedException e) {
			e.printStackTrace();
		} catch (Error e) {
			e.printStackTrace();
		}
		System.err.println("NumaTestWrapper - TEST FAILED");
		System.exit(1);
	}
	
	static boolean verifyTracepoints(byte[] stdErr) throws IOException {
		boolean result = true;
		int entryCount = 0;
		int exitCount = 0;
		
		BufferedReader in = new BufferedReader(new InputStreamReader(new ByteArrayInputStream(stdErr)));
		String line = in.readLine();
		
		while (line != null) {
			line = line.trim();
			/* Check for forkAndExecNativeV8 entry tracepoint j9mm.625 */
			if ((line.indexOf("j9mm.625") != -1) && (line.indexOf("MM_RuntimeExecManager::forkAndExecNativeV8") != -1)) {
				entryCount++;
			}
			/* Check for forkAndExecNativeV8 exit tracepoint j9mm.626 */
			if ((line.indexOf("j9mm.626") != -1) && (line.indexOf("MM_RuntimeExecManager::forkAndExecNativeV8") != -1)) {
				exitCount++;
			}
			line = in.readLine();
		}
		
		if ((entryCount != 2) || (exitCount != 2)) {
			result = false;
			System.err.println("\n======== NumaTestWrapper Output ========");
			System.err.println("Entry Tracepoint Count = " + entryCount);
			System.err.println("Exit Tracepoint Count = " + exitCount);
			System.err.println("Entry Tracepoint Count and Exit Tracepoint Count should be equal to 2"); 
		}
		
		return result;
	}

	public static String parseCommand(String[] args) {
		int numArgs = args.length;
		String cmd = null;
		
		for (int i = 0; i < numArgs; i++)  {
			if (args[i].startsWith("-cmd="))  {
				cmd = args[i].substring(5);
				if (null == cmd) {
					usage();
				}
				for (i = i + 1; i < numArgs; i++) {
					cmd += " " + args[i];
				}
				System.err.println("NumaTestWrapper argument: " + cmd);
			} else  {
				System.err.println("Unrecognized option: \"" + args[i] + "\"");
				usage();
			}
		}
		
		return cmd;
	}
	
	public static void usage()  {
		System.err.println("Usage: java [vmOptions] " + NumaTestWrapper.class.getName() + "[testOptions]");
		System.err.println("testOptions:");
		System.err.println("  -cmd=<numa_test_command>");
		System.exit(1);
	}
}
