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
package com.ibm.j9.numa;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class NumaTest {

	static final String NATIVE_LIBRARY_NAME = "j9thrnumanatives" + System.getProperty("com.ibm.oti.vm.library.version");
	static {
		System.loadLibrary(NATIVE_LIBRARY_NAME);
	}
	
	static int nodesInMainThread = -1;
	static int nodesInSecondaryThread = -1;
	static int nodesInMainThreadChildProcess = -1;
	static int nodesInSecondaryThreadChildProcess = -1;
	
	static class ProcessOutputThread extends Thread {
		InputStream theInput;
		OutputStream theOutput;
		
		ProcessOutputThread(InputStream input, OutputStream output){
			this.theInput = input;
			this.theOutput = output;
		}
		
		public void run() {
			try {
				byte buffer[] = new byte[1000];
				while(true){
					int length = theInput.read(buffer, 0,1000);
					if (length > 0){
						theOutput.write(buffer, 0, length);
					} else {
						break;
					}
				}
			} catch (Exception e) {
				e.printStackTrace();
			}
		}
	}

	
	static String exeName = null;
	
	public static void main(String[] args) throws InterruptedException {
		int numArgs = args.length;
		for (int i = 0; i < numArgs; i++)  {
			if (args[i].startsWith("-exe="))  {
				exeName = args[i].substring(5);
				if (null == exeName) {
					usage();
				}
				for (i = i + 1; i < numArgs; i++) {
					exeName += " " + args[i];
				}
				System.err.println("NumaTest argument: " + exeName);
			} else  {
				System.err.println("Unrecognized option: \"" + args[i] + "\"");
				usage();
			}
		}
		
		if (null == exeName) {
			exeName = System.getProperty("user.dir");
			if (!exeName.endsWith(System.getProperty("file.separator")))  {
				exeName = exeName + System.getProperty("file.separator");
			}
			exeName = exeName + "java";
		}
		
		System.err.println("Parent process started");
		
		nodesInMainThread = getUsableNodeCount("Parent Main Thread");
		nodesInMainThreadChildProcess = execChild("Parent Main Thread");
		
		/* the main thread may not be affinitized. Try execing from another thread */
		Thread thread = new Thread() {
			public void run() {
				nodesInSecondaryThread = getUsableNodeCount("Parent Secondary Thread");
				nodesInSecondaryThreadChildProcess = execChild("Parent Secondary Thread");
			}
		};
		thread.start();
		thread.join();
		
		System.err.println("\n======== RESULTS ========");
		if ((nodesInMainThread == 0) && (nodesInMainThreadChildProcess == 0) && (nodesInSecondaryThread == 0) && (nodesInSecondaryThreadChildProcess == 0)) {
			System.err.println("Node details not supported on this machine");
		} else {
			System.err.println("Found " + nodesInMainThread + " usable nodes when main thread asked");
			System.err.println("Found " + nodesInMainThreadChildProcess + " usable nodes when child process exec'ed from main thread asked");
			System.err.println("Found " + nodesInSecondaryThread + " usable nodes when secondary thread asked");
			System.err.println("Found " + nodesInSecondaryThreadChildProcess + " usable nodes when child process exec'ed from secondary thread asked");
		}
		
		try {
			if (-1 == nodesInMainThread) throw new Error("Failed to count nodes in main thread");
			if (-1 == nodesInMainThreadChildProcess) throw new Error("Failed to count nodes in main thread's child process");
			if (-1 == nodesInSecondaryThread) throw new Error("Failed to count nodes in secondary thread");
			if (-1 == nodesInSecondaryThreadChildProcess) throw new Error("Failed to count nodes in main thread");
			
			if (nodesInMainThread != nodesInMainThreadChildProcess) throw new Error("Found different number of nodes when child process exec'ed from main thread asked");
			if (nodesInMainThread != nodesInSecondaryThreadChildProcess) throw new Error("Found different number of nodes when secondary thread asked");
			if (nodesInMainThread != nodesInSecondaryThread) throw new Error("Found different number of nodes when child process exec'ed from secondary thread asked");
		} catch (Error e) {
			e.printStackTrace();
			System.err.println("NumaTest - TEST FAILED");
			System.exit(1);
		}

		System.err.println("NumaTest - TEST PASSED");
		System.exit(0);
	}

	public static void usage()  {
		System.err.println("Usage: java [vmOptions] " + NumaTest.class.getName() + "[testOptions]");
		System.err.println("testOptions:");
		System.err.println("  -exe=<exeFileName>     Override default VM executable name");
		System.exit(1);
	}
	
	public static native int getUsableNodeCount(String processName);

	static int execChild(String processName) {
		int result = -1;
		
		String command = "";
		command += exeName;
		command += " -cp ";
		command += System.getProperty("java.class.path");
		command += " ";
		command += NumaChild.class.getName();
		
		try {
			Process child = Runtime.getRuntime().exec(command);
			new ProcessOutputThread(child.getErrorStream(), System.err).start();
			new ProcessOutputThread(child.getInputStream(), System.out).start();
			System.err.println("Waiting for child to complete");
			result = child.waitFor();
		} catch (IOException e) {
			e.printStackTrace();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}

		return result;
	}

}
