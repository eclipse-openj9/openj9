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
public class CheckJdmpviewInfoThread {

	private static final String THREADINFO_TAG = "    name:          ";

	private static final String[] THREADBLOCK_TAGS = {"      parked on: ","      waiting to be notified on: ","      waiting to enter: " };

//	private static Map threadNameToCreator = new HashMap();
	
	/**
	 * @param args
	 */
	public static void main(String[] args) {

		if (args.length < 1) {
			System.err.println("An input file is required to check.");
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
				// The parked thread isn't actual locked on anything, it's just parked.
				if( !(job instanceof CoreThreadCreator.ParkedThread) ) {
					threadNameToCreator.put(job.getThreadNameToCheck(), job);
				}
			}
			final int expectedLockCount = threadNameToCreator.size();
			
			try {
				line = in.readLine();
				// Fast forward through the system monitors.
				while (line != null &&  !line.startsWith("Object Locks in use")) {
					line = in.readLine();
				}
				 
				/* Now looking at something like:
				 * java/lang/String@0x7fbb7cadba80
				 * 	owner thread id: 20107 name: WaitingOnSynchronized: lock owner
				 * 	waiting thread id: 20094 name: WaitingOnSynchronized: waiting thread
				 */
				String lock = null;
				String blockingThreadName = "<unowned>";
				String blockedThreadName = null;
				while ( (line = in.readLine()) != null ) {
					if( line.contains("@")) {
						// New lock.
						lock = line;
						blockingThreadName = "<unowned>";
						blockedThreadName = null;
						continue; // Processed this line.
					}
					if( line.contains("owner thread id:")) {
						blockingThreadName = line.substring( line.indexOf("name: ") + "name: ".length());
						continue; // Processed this line.
					}
					if( line.contains("waiting thread id:")) {
						blockedThreadName = line.substring( line.indexOf("name: ") + "name: ".length());
						// Do checks here now we know the id of the blocked thread.
						CoreThreadCreator threadCreator = (CoreThreadCreator)threadNameToCreator.remove(blockedThreadName);
						if( threadCreator == null ) {
							System.err.println("Found no entry for " + blockedThreadName);
							continue; // Processed this line.
						}
						System.err.println("Found expected lock for - \"" + threadCreator.getThreadNameToCheck() + "\"");
						if( !blockingThreadName.equals(threadCreator.getOwnerName()) ){
							System.err.println("Did not find expected owning thread \"" + threadCreator.getOwnerName() + "\"");
							System.err.println("Found: \"" + blockingThreadName + "\"");
							System.err.println("*** FAIL ***");
							passed = false;
							System.err.println("------------");
						} else {
							System.err.println("Confirmed " + blockingThreadName + " held the lock blocking " + blockedThreadName);
							System.err.println("Pass");
							lockCount++;					
							System.err.println("------------");
						}
						continue;
					}
					if( line.startsWith("java.util.concurrent locks in use")) {
						break; // Done with monitors, juc locks next.
					}
				}
				while ( (line = in.readLine()) != null ) {
					if( line.contains("@")) {
						// New lock.
						lock = line;
						blockingThreadName = "<unowned>";
						blockedThreadName = null;
						continue; // Processed this line.
					}
					if( line.contains("locked by java thread id:")) {
						blockingThreadName = line.substring( line.indexOf("name: ") + "name: ".length());
						continue; // Processed this line.
					}
					if( line.contains("waiting thread id:")) {
						blockedThreadName = line.substring( line.indexOf("name: ") + "name: ".length());
						// Do checks here now we know the id of the blocked thread.
						CoreThreadCreator threadCreator = (CoreThreadCreator)threadNameToCreator.remove(blockedThreadName);
						if( threadCreator == null ) {
							System.err.println("Found no entry for " + blockedThreadName);
							continue; // Processed this line.
						}
						System.err.println("Found expected lock for - \"" + threadCreator.getThreadNameToCheck() + "\"");
						if( !blockingThreadName.equals(threadCreator.getOwnerName()) ){
							System.err.println("Did not find expected owning thread \"" + threadCreator.getOwnerName() + "\"");
							System.err.println("Found: \"" + blockingThreadName + "\"");
							System.err.println("*** FAIL ***");
							passed = false;
							System.err.println("------------");
						} else {
							System.err.println("Confirmed " + blockingThreadName + " held the lock blocking " + blockedThreadName);
							System.err.println("Pass");
							lockCount++;					
							System.err.println("------------");
						}
						continue;
					}
					if( line.startsWith("java.util.concurrent locks in use")) {
						break; // Done with monitors, juc locks next.
					}
				}
			} catch (IOException e) {
				System.err.println("Error reading jdmpview output file.");
				System.exit(1);
			}
			System.err.println("Checked " + lockCount + " blocked threads out of " + expectedLockCount + " expected blocked threads");
			if( threadNameToCreator.size() != 0 ) {
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
				String threadName = getThreadNameFromThreadInfo(line);
				CoreThreadCreator threadCreator = (CoreThreadCreator)threadNameToCreator.remove(threadName);
				if( threadName == null || threadCreator == null ) {
					line = in.readLine();
					continue;
				}
				System.err.println("Found expected thread - \"" + threadCreator.getThreadNameToCheck() + "\"");
				threadCount++;
				
				// Skip to monitor line
				while( (findMatchingStartTag(line, THREADBLOCK_TAGS) == null) && !line.startsWith("    Java stack frames:")) {
					line = in.readLine();
				}
				if( (findMatchingStartTag(line, THREADBLOCK_TAGS) == null)) {
					passed = false;
					System.err.println("No blocking information line for thread " + threadName);
					System.err.println("Fail");
					System.err.println("------------");
					// This isn't a blocked thread so carry on.
					line = in.readLine();
					continue;
				}
				System.err.println("Blocked line: " + line);
				if( threadCreator.checkJdmpviewInfoThreadLine(line.substring(findMatchingStartTag(line, THREADBLOCK_TAGS).length() )) ) {
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
	
	public static String getThreadNameFromThreadInfo(String threadInfoLine) {
		// Pull the thread name out of a line that looks like:
		//     name:          Deadlock Thread 2
		return threadInfoLine.substring(THREADINFO_TAG.length() );
	}
	
	private static String findMatchingStartTag(String toCheck, String[] possibilities) {
		for( String s: possibilities ) {
			if( toCheck.startsWith(s) ) {
				return s;
			}
		}
		return null;
	}
}
