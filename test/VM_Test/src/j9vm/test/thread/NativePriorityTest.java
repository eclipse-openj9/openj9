/*******************************************************************************
 * Copyright (c) 2001, 2012 IBM Corp. and others
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
package j9vm.test.thread;

import j9vm.test.thread.TestNatives;

/**
 * Test thread scheduling policy
 */
public class NativePriorityTest {
	
	static final int NUM_THREADS = Thread.MAX_PRIORITY - Thread.MIN_PRIORITY;
	
	/* OS Specific Priorities */
	static final int WINDOWS_THREAD_PRIORITY_LOWEST = -2;
	static final int WINDOWS_THREAD_PRIORITY_BELOW_NORMAL = -1;
	static final int WINDOWS_THREAD_PRIORITY_NORMAL = 0;
	static final int WINDOWS_THREAD_PRIORITY_ABOVE_NORMAL = 1;
	
	enum OS_NAMES {Linux, AIX, Windows};
	static final int[][] OS_EXPECTED_PRIORITIES = {
		{0,0,0,0,0,0,0,0,0}, // Linux
		{41,43,45,47,49,51,53,55,57}, // AIX
		{WINDOWS_THREAD_PRIORITY_LOWEST, // Windows
			WINDOWS_THREAD_PRIORITY_BELOW_NORMAL,
			WINDOWS_THREAD_PRIORITY_BELOW_NORMAL,
			WINDOWS_THREAD_PRIORITY_BELOW_NORMAL,
			WINDOWS_THREAD_PRIORITY_NORMAL,
			WINDOWS_THREAD_PRIORITY_ABOVE_NORMAL,
			WINDOWS_THREAD_PRIORITY_ABOVE_NORMAL,
			WINDOWS_THREAD_PRIORITY_ABOVE_NORMAL,
			WINDOWS_THREAD_PRIORITY_ABOVE_NORMAL}
	};
	
	OS_NAMES currentOperatingSystem;
	int[] expectedPriorities = null;
	
	/**
	 * The main test driver method. Creates a new instance of this
	 * class and runs the test.
	 * 
	 * @param args The arguments passed by the test suite to the main method. 
	 * 				They are not currently being used. 
	 */
	public static void main(String[] args) {
		String realtimeProperty = System.getProperty("com.ibm.jvm.realtime");
		if (null != realtimeProperty) {
			System.out.println("Skipping test because we're running in a realtime VM");
			System.exit(0);
		}
		
		String osName = System.getProperty("os.name");
		System.out.println("osName = " + osName);
		OS_NAMES currentOS = null;
		
		if (osName.contains(OS_NAMES.Windows.toString())) {
			currentOS = OS_NAMES.Windows;
		} else {
			try {
				currentOS = OS_NAMES.valueOf(osName);
			} catch (IllegalArgumentException e) {
				System.out.println("Skipping test because it is not configured to run on this OS.");
				System.exit(0);
			}
		}
		
		NativePriorityTest nativePriorityTest = new NativePriorityTest(currentOS);
		
		nativePriorityTest.runTest();
	}
	
	/**
	 * Constructor. Initializes private members.
	 * @param currentOperatingSystem  This is the detected OS that this test is running under. 
	 */
	public NativePriorityTest(OS_NAMES currentOperatingSystem) {
		this.currentOperatingSystem = currentOperatingSystem;
		this.expectedPriorities = OS_EXPECTED_PRIORITIES[currentOperatingSystem.ordinal()];
	}

	/**
	 * Launches as many threads as there are Java priorities.
	 * Assigns a different priority to each one using Thread.setPriority().
	 * Retrieves the native priority of each thread and checks that
	 * it matches the expected value.
	 */
	public void runTest() {		
		/* Print priorities */
		for (int i = 0 ; i < NUM_THREADS ; i++) {
			final int javaPriority = Thread.MIN_PRIORITY + i;
			final int expectedNativePriority = expectedPriorities[i];
			Thread thread = new Thread("NativePriorityTestThread" + javaPriority) {
				public void run() {
					setPriority(javaPriority);
					int nativePriority = TestNatives.getSchedulingPriority(this);
					System.out.println("Thread[" + javaPriority + "]: " + nativePriority);
					if (nativePriority != expectedNativePriority) {
						fail("nativePriority != expectedNativePriority, \n\tnativePriority=" 
								+ nativePriority + " expectedNativePriority=" + expectedNativePriority
								+ "\n\tjavaPriority=" + javaPriority);
					}
				}
			};
			thread.start();
			try {
				thread.join();
			} catch (InterruptedException e) {
				e.printStackTrace();
				System.exit(1);
			}
		}
		
	}

	
	/**
	 * This method outputs a failure message and throws a RuntimeException
	 * causing the test harness to detect that this test has failed.
	 * @param message  The error message to print 
	 */
	private void fail(String message) {
		System.out.println("NativePriorityTest FAILED");
		throw new RuntimeException(message);
	}

}
