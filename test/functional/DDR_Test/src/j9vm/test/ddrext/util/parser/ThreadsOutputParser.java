/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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
package j9vm.test.ddrext.util.parser;

import java.util.StringTokenizer;

import org.testng.log4testng.Logger;

import j9vm.test.ddrext.Constants;

public class ThreadsOutputParser {
	
	private static Logger log = Logger.getLogger(FindVMOutputParser.class);
	/**
	 * Following index values represents the indexes when thread string is split
	 * by whitespaces Sample string : !stack 0x05583400 !j9vmthread 0x05583400
	 * !j9thread 0x0553cb90 tid 0x3f00 (16128) // (main)
	 */
	private static final int THREAD_STACK_INDEX = 0;
	private static final int THREAD_STACK_ADDR_INDEX = 1;
	private static final int THREAD_J9VMTHREAD_INDEX = 2;
	private static final int THREAD_J9VMTHREAD_ADDR_INDEX = 3;
	private static final int THREAD_J9THREAD_INDEX = 4;
	private static final int THREAD_J9THREAD_ADDR_INDEX = 5;
	private static final int THREAD_TID_INDEX = 6;
	private static final int THREAD_TID_VALUE_INDEX = 7;
	private static final int THREAD_TID_VALUE_DECIMAL_INDEX = 8;
	private static final int THREAD_NAME_INDEX = 10;

	/*
	 * Sample !threads output :
	 * 
	 * > !threads 
	 * !stack 0x05583400 !j9vmthread 0x05583400 !j9thread 0x0553cb90 tid 0x3f00 (16128) // (main) 
	 * !stack 0x0562d700 !j9vmthread 0x0562d700 !j9thread 0x056294f0 tid 0x3f02 (16130) // (JIT Compilation Thread-0)
	 * !stack 0x05636f00 !j9vmthread 0x05636f00 !j9thread 0x05629a70 tid 0x3f03 (16131) // (JIT Compilation Thread-1) 
	 * !stack 0x05641800 !j9vmthread 0x05641800 !j9thread 0x0563d580 tid 0x3f04 (16132) // (JIT Compilation Thread-2) 
	 * !stack 0x0564b000 !j9vmthread 0x0564b000 !j9thread 0x0563db00 tid 0x3f05 (16133) // (JIT Compilation Thread-3) !stack 0x0566e100
	 * !j9vmthread 0x0566e100 !j9thread 0x0566a270 tid 0x3f07 (16135) (IProfiler) 
	 * !stack 0x2aaad0092000 !j9vmthread 0x2aaad0092000 !j9thread 0x2aaad008de60 tid 0x3f08 (16136) // (Signal Dispatcher) 
	 * !stack 0x0588e400 !j9vmthread 0x0588e400 !j9thread 0x2aaad00aea50 tid 0x3f0a (16138) // (Concurrent Mark Helper) 
	 * !stack 0x05b0a100 !j9vmthread 0x05b0a100 !j9thread 0x2aaad00aefd0 tid 0x3f0b (16139) // (GC Slave)
	 * !stack 0x05b14300 !j9vmthread 0x05b14300 !j9thread 0x2aaad00afa60 tid 0x3f0c (16140) // (GC Slave) 
	 * !stack 0x05b2e600 !j9vmthread 0x05b2e600 !j9thread 0x2aaad00affe0 tid 0x3f0d (16141) // (GC Slave) 
	 * !stack 0x05b49800 !j9vmthread 0x05b49800 !j9thread 0x05b35690 tid 0x3f0e (16142) // (GC Slave) 
	 * !stack 0x2aaad01faf00 !j9vmthread 0x2aaad01faf00 !j9thread 0x05b1a5d0 tid 0x3f1b (16155) // (Attach API wait loop) 
	 * !stack 0x05b1e200 !j9vmthread 0x05b1e200 !j9thread 0x05b1a050 tid 0x3f1c (16156) // (Finalizer thread)
	 */

	/**
	 * This method splits the !threads output into separate thread info arrays.
	 * !threads output prints one line per thread in the DDR output.
	 * 
	 * @param threadsOutput
	 * @return array of strings for each thread in the !threads output.
	 */
	public static String[] getThreadsArray(String threadsOutput) {
		String threadsArray[] = threadsOutput.split("\n");
		for (int i = 0; i < threadsArray.length; i++) {
			threadsArray[i] = threadsArray[i].trim();
		}
		return threadsArray;
	}

	/**
	 * This methods returns the corresponding line from !threads output for the
	 * specified thread name.
	 * 
	 * @param threadsOutput
	 *            !threads output from DDR extensions
	 * @param threadName
	 *            Name of the thread whose thread info line is searched for in
	 *            !threads output
	 * @return A line of !threads output which corresponds to the searched
	 *         threads name.
	 */
	private static String getThreadStringForThreadName(String threadsOutput,
			String threadName) {
		
		if (null == threadsOutput) {
			log.error("!threads output is null");
			return null;
		}
		
		String threadString = null;
		String threadsArray[] = getThreadsArray(threadsOutput);

		/* Find the thread line for the threadName */
		for (int i = 0; i < threadsArray.length; i++) {
			/* Thread names are printed in between (<threadName>) */
			if (threadsArray[i].endsWith("(" + threadName + ")")) {
				threadString = threadsArray[i];
				break;
			}
		}
		return threadString;
	}

	/**
	 * This method finds the thread address of a specific thread by searching
	 * its name in !threads output.
	 * 
	 * @param threadsOutput
	 *            !threads output from DDR extensions
	 * @param threadName
	 *            Name of the thread whose thread address is being searched for
	 * @return String representation of the thread address
	 */
	public static String getJ9ThreadAddress(String threadsOutput,
			String threadName) {
		
		if (null == threadsOutput) {
			log.error("!threads output is null");
			return null;
		}

		/* Find the thread line for the threadName */
		String threadString = getThreadStringForThreadName(threadsOutput, threadName);

		/* Check whether the thread is found or not */
		if (null == threadString) {
			/* Thread does not exist in this !threads output */
			return null;
		}

		/* Split thread string by using whitespaces */
		StringTokenizer tokenizer = new StringTokenizer(threadString);

		/* Find the j9thread address by using its index in the tokens list */
		String token = null;
		for (int i = 0; i <= THREAD_J9THREAD_ADDR_INDEX; i++) {
			if (tokenizer.hasMoreElements()) {
				token = tokenizer.nextToken();
			} else {
				return null;
			}
		}
		return token;
	}

	/**
	 * This method finds the VM thread address of a specific thread by searching
	 * its name in !threads output.
	 * 
	 * @param threadsOutput
	 *            !threads output from DDR extensions
	 * @param threadName
	 *            name of the thread whose VM thread address is being searched
	 *            for
	 * @return String representation of the VM thread address
	 */
	public static String getJ9VMThreadAddress(String threadsOutput,
			String threadName) {
		
		if (null == threadsOutput) {
			log.error("!threads output is null");
			return null;
		}
		/* Find the thread line for the threadName */
		String threadString = getThreadStringForThreadName(threadsOutput, threadName);

		/* Check whether the thread is found or not */
		if (null == threadString) {
			/* Thread does not exist in this !threads output */
			return null;
		}

		/* Split thread string by using whitespaces */
		StringTokenizer tokenizer = new StringTokenizer(threadString);

		/* Find the j9thread address by using its index in the tokens list */
		String token = null;
		for (int i = 0; i <= THREAD_J9VMTHREAD_ADDR_INDEX; i++) {
			if (tokenizer.hasMoreElements()) {
				token = tokenizer.nextToken();
			} else {
				return null;
			}
		}
		return token;
	}

}
