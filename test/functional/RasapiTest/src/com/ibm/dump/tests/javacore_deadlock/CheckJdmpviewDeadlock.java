/*******************************************************************************
 * Copyright IBM Corp. and others 2016
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package com.ibm.dump.tests.javacore_deadlock;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * This class check a javacore for a few pre-setup deadlock cycles.
 * 
 * @author hhellyer
 * 
 */
public class CheckJdmpviewDeadlock {

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

		passed &= checkDeadlockSection(in);

		if( passed ) {
			System.err.println("Test passed.");
			System.exit(0);
		} else {
			System.err.println("Test failed.");
			System.exit(1);
		}
	}
	
	private static boolean checkDeadlockSection(BufferedReader in) {
		String line;
		boolean passed = true;
		int deadlockCount = 0;
		final int expectedDeadlocks = 3;
		
		// Fast forward to "deadlock loop:"
		// (Fail if we don't find it.)
		try {
			line = readLine(in);
			while( line != null ) {
				while (line != null && !line.startsWith("deadlock loop:") && deadlockCount < expectedDeadlocks) {
					line = readLine(in);
				}
				if( line == null ) {
					System.err.println("Null line");
					passed = false;
					break;
				}
				if( deadlockCount == expectedDeadlocks ) {
					break;
				}
				deadlockCount++;
				System.err.println("Found a deadlock, line: " + line);
				line = readLine(in);
				// Now validate the deadlock is reported correctly.
				List<Integer> threadNums = new ArrayList<Integer>();
				List<String> threadNames = new ArrayList<String>();
				while(true) {
					if( line.startsWith("thread: ") ) {
						String threadName = line.substring(line.indexOf("thread: ") + "thread: ".length(), line.indexOf("id: "));
						int threadNo = getThreadNumber(threadName);
						threadNames.add(threadName);
						threadNums.add(threadNo);
					} else {
						break; // Check we have a valid cycle outside this loop.
					}
					line = readLine(in);
				}
				// The threads should all be in a cycle waiting on each other in order.
				// Validate they are all the same kind of thread and the numbers are in order.
				int currentThread = threadNums.get(0);
				int firstThread = currentThread; 
				String currentName = threadNames.get(0);
				String firstName = currentName;
				String threadPrefix = currentName.substring( 0, currentName.indexOf("##")).trim();
				// Length of the deadlock chain is (threadNums.size() - 1) since the first thread repeats to complete the chain.
				System.err.println("Checking deadlock chain for \"" + threadPrefix + "\" threads that is " + (threadNums.size() - 1) + " threads long");
				for( int i = 1; i < threadNums.size(); i++ ) {
					int nextThread = threadNums.get(i);
					String nextThreadName = threadNames.get(i);
					if( nextThread != currentThread + 1 && nextThread != 0 ) {
						System.err.println("Threads out of order: " + threadNames.get(i) + " after " + currentThread );
						passed &= false;
						break;
					}
					if( !nextThreadName.startsWith(threadPrefix)) {
						System.err.println("Wrong thread type: " + threadNames.get(i) + " does not start with " + currentThread );
						passed &= false;
						break;
					}
					currentThread = nextThread;
					currentName = nextThreadName;
				}
				if( firstThread == currentThread ) {
					System.err.println("Complete thread cycle found, confirmed deadlock for " + threadPrefix + " threads" );
				}
				System.err.println("------------");
				
			}
		} catch (IOException e) {
			System.err.println("Error reading javacore file.");
			System.exit(1);
		}
		if( deadlockCount == expectedDeadlocks ) {
			passed &= true;
		}
		
		return passed;
	}

	// Wrap this up as it's worth trimming the whitespace off the start of each jdmpview line.
	private static String readLine(BufferedReader in) throws IOException {
		String line = in.readLine();
		return line!=null?line.trim():null;
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
