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
package com.ibm.dump.tests.javacore_lock_context;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.Map;
import java.util.Stack;

/**
 * This class check a javacore for a few pre-setup deadlock cycles.
 * 
 * @author hhellyer
 * 
 */
public class CheckJavaCore {

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

		// We get the expected results by re-running the threads.
		CreateJavaCore.setupThreads();
		
		boolean passed = true;

		passed &= checkThreadSection(in);

		if( passed ) {
			System.err.println("Test passed.");
			System.exit(0);
		} else {
			System.err.println("Test failed.");
			System.exit(1);
		}
	}
	
	private static boolean checkThreadSection(BufferedReader in) {
		String line;
		boolean passed = true;
		
		Map<String, StackCreator> threadsToEntries = CreateJavaCore.threadsToEntries;
		final int threadsToCheck = threadsToEntries.size();
		int threadsChecked = 0;
		
		// Fast forward to 1XMTHDINFO     Thread Details
		// (Fail if we don't find it.)
		try {
			line = in.readLine();
			while( line != null ) {
				// Look for "3XMTHREADINFO " (with space) so we don't match "3XMTHREADINFO1 2 or 3"
				while (line != null && !line.startsWith("3XMTHREADINFO ")) {
					line = in.readLine();
				}
				if( line == null ) {
					System.err.println("Null line");
					passed = false;
					break;
				}
				
				String threadName = getQuotedThreadName(line);
				line = in.readLine();
				StackCreator stackCreator = threadsToEntries.get(threadName);
				if( stackCreator == null ) {
					// Not one of our threads, skip this one.
					continue;
				}
				System.err.println("Checking " + threadName);
				Stack<EntryRecord> entryStack  = new Stack<EntryRecord>();
				entryStack.addAll(stackCreator.getEntryRecords());
				EntryRecord nextEntry = entryStack.pop();
				// Walk the thread stack until we get to the end, look for "NULL"
				while (line != null && !line.startsWith("NULL")) {
					if( line != null && nextEntry != null && line.startsWith("4XESTACKTRACE") && line.contains(nextEntry.getMethodName() )) {
						// Get the method name, see if it's the next one we are looking for.
						// (Not all entries in the stack will have entry records.)
						// System.err.println("Found: " + line + " looking for " + nextEntry.getMethodName());
						
						// Next line should contain an entry record, read and check.
						line = in.readLine();
						while( line.startsWith("5XESTACKTRACE")) {
							if( !line.startsWith("5XESTACKTRACE                   (entered lock: " + nextEntry.getLockName())) {
								System.err.println("Fail - Didn't find entry record for " + nextEntry.getMethodName());
								System.err.println("Expected: 5XESTACKTRACE                   (entered lock: " + nextEntry.getLockName());
								// System.err.println("Line: " + line);
							} else	if( !line.contains("entry count: " + nextEntry.getEntryCount() + ")")) {
								System.err.println("Fail - Didn't find expected enter count " + "entry count: " + nextEntry.getEntryCount() + ")");
								// System.err.println("Line: " + line);
							} else {
								// System.err.println("Found expected entry.");
							}
							nextEntry = entryStack.isEmpty()?null:entryStack.pop();
							line = in.readLine();
						} 
					} else {
						line = in.readLine();
					}
				}
				threadsChecked++;
				if( !entryStack.isEmpty() ) {
					System.err.println("Failed to find all lock entry records for " + threadName);
					passed = false;
				} else {
					System.err.println("Found correct lock entry records for " + threadName);
				}
				System.err.println("------------");
				if( threadsChecked == threadsToCheck ) {
					break; // Finished. (There will be other threads but we are only interested in ours.)
				}
				
			}
		} catch (IOException e) {
			System.err.println("Error reading javacore file.");
			System.exit(1);
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
	
	public static int getThreadNumber(String threadName) {
		// Pull the thread number out of a name that looks like:
		// Deadlock Thread ##6##
		int openQuote = -1;
		int closeQuote = -1;
		openQuote = threadName.indexOf("##");
		closeQuote = threadName.indexOf("##", openQuote + 1);
		if( openQuote > -1 && closeQuote > -1 ) {
			String threadNo_string = threadName.substring(openQuote + 2, closeQuote);
			int threadNo = Integer.parseInt(threadNo_string);
			return threadNo;
		} else {
			return Integer.MIN_VALUE;
		}
	}
}
