/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.javacore_thread;

import java.io.BufferedInputStream;
import java.io.BufferedReader;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.HashMap;
import java.util.Map;

/**
 * This class check a javacore for a set of thread with known names and checks
 * that they are correctly marked as blocked by other threads.
 * 
 * @author hhellyer
 * 
 */
public class CheckJavaCore {

	private static final String THREADINFO_TAG = "3XMTHREADINFO ";

	private static final String THREADBLOCK_TAG = "3XMTHREADBLOCK";
	
	/**
	 * @param args
	 */
	public static void main(String[] args) {

		if (args.length < 1) {
			System.err.println("An input javacore is required to check.");
			System.exit(1);
		}
		String fileName = args[0];

		BufferedReader in = null;
		try {
			in = new BufferedReader(new FileReader(fileName));
		} catch (FileNotFoundException e) {
			System.err.println("File: " + fileName + " not found.");
			System.exit(1);
		}
		if (in == null) {
			System.err.println("Failed to open file: " + fileName);
			System.exit(1);
		}

		boolean passed = true;

		passed &= checkThreadSection(in);
		try {
			in = new BufferedReader(new FileReader(fileName));
		} catch (FileNotFoundException e) {
			System.err.println("File: " + fileName + " not found on second read.");
			System.exit(1);
		}
		System.err.println();
		passed &= checkMonitorSection(in);
		if( passed ) {
			System.err.println("Test passed.");
			System.exit(0);
		} else {
			System.err.println("Test failed.");
			System.exit(1);
		}
	}
	

   /*
    * 2LKMONINUSE      sys_mon_t:0x00007F6B903C5E98 infl_mon_t: 0x00007F6B903C5F10:
    * 3LKMONOBJECT       java/lang/String@0x00007F6B68CFB210: Flat locked by "BlockedOnSynchronized: lock owner" (0x00007F6B14007400), entry count 1
    * 3LKWAITERQ            Waiting to enter:
    * 3LKWAITER                "BlockedOnSynchronized: waiting thread" (0x00007F6B1400EA00)
    */
	private static boolean checkMonitorSection(BufferedReader in) {
		String line;
		// Increment this every time we find one of our threads.
		int lockCount = 0;
		boolean passed = true;
		Map threadNameToCreator = new HashMap();
		
		System.err.println("Checking Monitors");
		
		for(CoreThreadCreator job: CoreThreadCreator.setupJobs ) {
			if( job.isSynchronizedLock() ) {
				threadNameToCreator.put(job.getThreadNameToCheck(), job);
			}
		}
		final int expectedLockCount = threadNameToCreator.size();
		
		try {
			line = in.readLine();
			while (line != null) {
				String lockOwnerLine = null;
				String blockingThreadName = null;
				String blockedThreadName = null;
				if( !line.startsWith("2LKMONINUSE")) {
					// Skip boring lines.
					line = in.readLine();
					continue;
				}
				line = in.readLine();
				if( line.startsWith("3LKMONOBJECT")) {
					getQuotedThreadName(line);
					blockingThreadName = getQuotedThreadName(line);
					if( blockingThreadName == null ) {
						blockingThreadName = "<unowned>";
					}
					lockOwnerLine = line;
				}
				line = in.readLine(); // 3LKNOTIFYQ, 3LKWAITERQ
				line = in.readLine(); // 3LKWAITNOTIFY, 3LKWAITER
				if( line.startsWith("3LKWAIT")) {
					blockedThreadName = getQuotedThreadName(line);
				}
				
				CoreThreadCreator threadCreator = (CoreThreadCreator)threadNameToCreator.remove(blockedThreadName);
				if( threadCreator == null ) {
					line = in.readLine();
					System.err.println("Found no entry for " + blockedThreadName);
					continue;
				}
				System.err.println("Found expected lock for - \"" + threadCreator.getThreadNameToCheck() + "\"");
				if( !blockingThreadName.equals(threadCreator.getOwnerName())){
					System.err.println("Did not find expected owning thread \"" + threadCreator.getOwnerName() + "\"");
					System.err.println("*** FAIL ***");
					passed = false;
				} else {
					System.err.println("Confirmed " + blockingThreadName + " held the lock blocking " + blockedThreadName);
					System.err.println("Pass");
				}
				lockCount++;
				
				
				line = in.readLine();
				System.err.println("------------");
 			}
		} catch (IOException e) {
			System.err.println("Error reading javacore file.");
			System.exit(1);
		}
		System.err.println("Checked " + lockCount + " blocked threads out of " + expectedLockCount + " expected blocked threads");
		if( threadNameToCreator.size() != 0 ) {
			System.err.println("*** FAIL ***");
			passed = false;
			System.err.println("Failed to find all expected threads blocked: ");
			for( Object key: threadNameToCreator.keySet() ) {
				System.err.println("Missing thread: \"" + key + "\"");
			}
		} else {
			System.err.println("Found and checked all expected threads.");
		}
		return passed;
	}

	private static boolean checkThreadSection(BufferedReader in) {
		String line;
		// Increment this every time we find one of our threads.
		int threadCount = 0;
		boolean passed = true;
		Map threadNameToCreator = new HashMap();
		
		System.err.println("Checking Thread Blockers");
		
		for(CoreThreadCreator job: CoreThreadCreator.setupJobs ) {
			threadNameToCreator.put(job.getThreadNameToCheck(), job);
		}
		final int expectedThreadCount = threadNameToCreator.size();
		
		try {
			line = in.readLine();
			while (line != null) {
				if( !line.startsWith(THREADINFO_TAG)) {
					// Skip boring lines.
					line = in.readLine();
					continue;
				}
				String threadName = getQuotedThreadName(line);
				CoreThreadCreator threadCreator = (CoreThreadCreator)threadNameToCreator.remove(threadName);
				if( threadName == null || threadCreator == null ) {
					line = in.readLine();
					continue;
				}
				System.err.println("Found expected thread - \"" + threadCreator.getThreadNameToCheck() + "\"");
				threadCount++;
				while( !line.startsWith(THREADBLOCK_TAG) ) {
					// If we reach NULL we know we've gone off the end of this thread and should fail.
					if( line.startsWith("NULL")) {
						break;
					}
					line = in.readLine();
				}
				if( !line.startsWith(THREADBLOCK_TAG)) {
					System.err.println("*** FAIL ***");
					passed = false;
					System.err.println("No " + THREADBLOCK_TAG + " line for thread " + threadName);
					System.err.println("Fail");
					System.err.println("------------");
					// This isn't a blocked thread so carry on.
					line = in.readLine();
					continue;
				}
				System.err.println("Blocked line: " + line);
				if( threadCreator.checkBlockingLine(line) ) {
					System.err.println("Pass");
				} else {
					System.err.println("*** FAIL ***");
					passed = false;
				}
				System.err.println("------------");
				line = in.readLine();
 			}
		} catch (IOException e) {
			System.err.println("Error reading javacore file.");
			System.exit(1);
		}
		System.err.println("Checked " + threadCount + " threads out of " + expectedThreadCount + " expected threads");
		if( threadNameToCreator.size() != 0 ) {
			passed = false;
			System.err.println("Failed to find all expected threads: ");
			for( Object key: threadNameToCreator.keySet() ) {
				System.err.println("Missing thread: \"" + key + "\"");
			}
		} else {
			System.err.println("Found and checked all expected threads.");
		}
		return passed;
	}
	
	/**
	 * Given a string, returns the contents of the first quoted string.
	 * (Which in java cores is generally the thread name.
	 * @param line
	 * @return
	 */
	public static String getQuotedThreadName(String line) {
		// Pull the thread name out of a line that looks like:
		// 3XMTHREADINFO      "main" J9VMThread:0x00129100, j9thread_t:0x002B5A30, java/lang/Thread:0x00DDC4F8, state:R, prio=5
		// There are threads that don't have quotes, but they aren't Java threads so we aren't
		// interested here.
		int openQuote = -1;
		int closeQuote = -1;
		openQuote = line.indexOf('"');
		closeQuote = line.indexOf('"', openQuote + 1);
		if( openQuote > -1 && closeQuote > -1 ) {
			String threadName = line.substring(openQuote + 1, closeQuote);
			return threadName;
		} else {
			return null;
		}
	}
}
