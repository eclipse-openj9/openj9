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
package j9vm.test.thread;

import java.io.BufferedReader;
import java.io.InputStreamReader;

/**
 * Test thread scheduling policy
 */
public class SchedulingTest {
	
	static final String CHILD_ARG = "-child";
	static final String GC_POLICY_METRONOME = "-Xgcpolicy:metronome";
	
	static final String SCHED_OTHER_STRING = "SCHED_OTHER";
	static final String SCHED_RR_STRING = "SCHED_RR";
	static final String SCHED_FIFO_STRING = "SCHED_FIFO";
	
	static final String MAIN_THREAD_STRING = "main";
	static final String CHILD_THREAD_STRING = "child";
	static final String GC_ALARM_THREAD_STRING = "GC Alarm";
	
	static final String POLICY_SEPARATOR = ",";
	
	/**
	 * @param args
	 */
	public static void main(String[] args) {
		if (1 == args.length) {
			if (args[0].equalsIgnoreCase(CHILD_ARG)) {
				new SchedulingTest().runChild();
			}
		} else {
			String realtimeProperty = System.getProperty("com.ibm.jvm.realtime");
			if (null != realtimeProperty && realtimeProperty.contains("soft")) {
				new SchedulingTest().runTests();
			} else {
				System.out.println("Skipping test because we're not running in SRT");
				System.exit(0);
			}
		}
	}

	public void runTests() {
		testSchedulingPolicy("", false);
		testSchedulingPolicy(GC_POLICY_METRONOME, false);
		testSchedulingPolicy("", true);
		testSchedulingPolicy(GC_POLICY_METRONOME, true);
	}

	/**
	 * Launches a child JVM process using chrt to make it use SCHED_RR as the 
	 * scheduling policy and checks that the scheduling policies of the child 
	 * JVM's main thread and child thread use the expected thread policies and 
	 * priorities based on whether or not the JVM is running in SRT i.e. whether 
	 * or not -Xgcpolicy:metronome was specified on the command-line.
	 *  
	 * @param jvmOptions  Command-line options to pass to the child JVM 
	 * @param incrementMaxPriority  Boolean specifying whether or not to set the IBM_J9_THREAD_INCREMENT_MAX_PRIORITY environment variable
	 */
	private void testSchedulingPolicy(String jvmOptions, boolean incrementMaxPriority) {
		final boolean RUNNING_IN_SRT = jvmOptions.contains(GC_POLICY_METRONOME);
		System.out.println("Running test testSchedulingPolicy(\"" + jvmOptions + "\", " + incrementMaxPriority + ") ...");
				
		try {
			final int CHRT_PRIORITY = 6;
			final String className = this.getClass().getCanonicalName();
			final String javaHome = System.getProperty("java.home") + "/bin";
			final String command = "chrt -r " + CHRT_PRIORITY + " " + javaHome + "/java " + jvmOptions + " -cp " + System.getProperty("java.class.path") + " " + className + " " + CHILD_ARG; 
			String[] environment = null;
			
			if (incrementMaxPriority) {
				environment = new String[1];
				environment[0] = "IBM_J9_THREAD_INCREMENT_MAX_PRIORITY=1";
			}
			
			Process childProcess = Runtime.getRuntime().exec(command, environment);
						
			childProcess.waitFor(); /* wait for the child process to finish */

			/* Check if the child process terminated incorrectly */
			int childExitValue = childProcess.exitValue();
			if (0 != childExitValue) {
				fail("Child did not exit properly, childExitValue = " + childExitValue, jvmOptions, incrementMaxPriority);
			}
			
			/* Read the child process' standard output and check that the scheduling 
			 * policies match the expected ones
			 */
			String line = null;
			final int NUM_EXPECTED_MATCHES = RUNNING_IN_SRT ? 3 : 2;
			int numMatches = 0;
			BufferedReader reader = new BufferedReader(new InputStreamReader(childProcess.getInputStream()));
			while (null != (line = reader.readLine())) {
				System.out.println(line); /* output the line to standard out */
				
				if (line.startsWith(MAIN_THREAD_STRING)) {
					parseAndTestValues(jvmOptions, incrementMaxPriority, line, MAIN_THREAD_STRING, SCHED_RR_STRING, CHRT_PRIORITY);
					numMatches++;
				} else if (line.startsWith(CHILD_THREAD_STRING)) {
					final String EXPECTED_CHILD_THREAD_POLICY = RUNNING_IN_SRT ? SCHED_RR_STRING : SCHED_OTHER_STRING;
					int expectedPriority = RUNNING_IN_SRT ? CHRT_PRIORITY : 0;
					parseAndTestValues(jvmOptions, incrementMaxPriority, line, CHILD_THREAD_STRING, EXPECTED_CHILD_THREAD_POLICY, expectedPriority);
					numMatches++;
				} else if (RUNNING_IN_SRT && line.startsWith(GC_ALARM_THREAD_STRING)) {
					int expectedPriority = incrementMaxPriority ? (CHRT_PRIORITY + 1) : CHRT_PRIORITY;
					parseAndTestValues(jvmOptions, incrementMaxPriority, line, GC_ALARM_THREAD_STRING, SCHED_RR_STRING, expectedPriority);
					numMatches++;
				}
			}
			
			if (NUM_EXPECTED_MATCHES != numMatches) {
				fail("Expected the child process to output " + NUM_EXPECTED_MATCHES + 
						" correct scheduling policies and priorities but got " + numMatches + " instead.",
						jvmOptions, incrementMaxPriority);
			}
			
		} catch (Exception e) {
			e.printStackTrace();
			fail("", jvmOptions, incrementMaxPriority);
		}
	}
	
	/**
	 * Helper method called by @ref testSchedulingPolicy() to parse a line of
	 * output from a child process' thread and check that the thread in question's 
	 * scheduling policy and priority match the expected ones. If the values
	 * don't match this method will call @ref fail()
	 * 
	 * @param jvmOptions The JVM options that were passed in to the testSchedulingPolicy() method
	 * @param incrementMaxPriority  A boolean indicating whether or not IBM_J9_THREAD_INCREMENT_MAX_PRIORITY is set
	 * @param line  The line of output to parse
	 * @param threadName  The name of the thread that the line of output belongs to
	 * @param expectedPolicy  The expected scheduling policy that the thread should have
	 * @param expectedPriority  The expected scheduling priority that the thread should have
	 */
	private void parseAndTestValues(String jvmOptions, boolean incrementMaxPriority, String line, String threadName, String expectedPolicy, int expectedPriority) {
		String[] splitLine = line.split(POLICY_SEPARATOR);
		String actualSchedulingPolicy = splitLine[1];
		if (!expectedPolicy.equals(actualSchedulingPolicy)) {
			fail("The expected " + threadName + " thread scheduling policy is not equal to the actual " +
					threadName + " thread scheduling policy.\n" +
					"Expected: " + expectedPolicy + "\n" +
					"Actual: " + actualSchedulingPolicy,
					jvmOptions, incrementMaxPriority);
		}
		
		int actualPriority = Integer.valueOf(splitLine[2]);
		if (expectedPriority != actualPriority) {
			fail("The expected " + threadName + " thread scheduling priority is not equal to the actual " +
					threadName + " thread scheduling priority.\n" +
					"Expected: " + expectedPriority + "\n" +
					"Actual: " + actualPriority,
					jvmOptions, incrementMaxPriority);
		}
	}
	
	/**
	 * This method outputs a failure message and throws a RuntimeException
	 * causing the test harness to detect that this test has failed.
	 * @param message  The error message to print 
	 * @param jvmOptions  The jvmOptions that were passed in to the failing method
	 * @param incrementMaxPriority  A boolean indicating whether or not IBM_J9_THREAD_INCREMENT_MAX_PRIORITY is set
	 */
	private void fail(String message, String jvmOptions, boolean incrementMaxPriority) {
		System.out.println("testSchedulingPolicy(\"" + jvmOptions +"\", " + incrementMaxPriority + ") FAILED");
		System.out.println("***FAILED***");
		throw new RuntimeException(message);
	}
	
	/**
	 * Helper method called by the child process JVMs.
	 * Creates a child thread, gets the scheduling policy of the main 
	 * thread and the child thread and outputs them to standard out.
	 */
	private void runChild() {
		int schedulingPolicy = TestNatives.getSchedulingPolicy(Thread.currentThread());
		int schedulingPriority = TestNatives.getSchedulingPriority(Thread.currentThread());
		
		System.out.print(MAIN_THREAD_STRING + POLICY_SEPARATOR);
		printSchedulingPolicy(schedulingPolicy);
		System.out.println(POLICY_SEPARATOR + schedulingPriority);
		
		Thread childThread = new Thread() {
			public void run() {
				int schedulingPolicy = TestNatives.getSchedulingPolicy(Thread.currentThread());
				int schedulingPriority = TestNatives.getSchedulingPriority(Thread.currentThread());
				
				System.out.print(CHILD_THREAD_STRING + POLICY_SEPARATOR);
				printSchedulingPolicy(schedulingPolicy);
				System.out.println(POLICY_SEPARATOR + schedulingPriority);
			}
		};
		
		childThread.start();
		
		try {
			childThread.join();
		} catch (Exception e) {
			e.printStackTrace();
			System.exit(1);
		}
		
		/* GC Alarm Thread */
		schedulingPolicy = TestNatives.getGCAlarmSchedulingPolicy();
		System.out.print(GC_ALARM_THREAD_STRING + POLICY_SEPARATOR);
		printSchedulingPolicy(schedulingPolicy);
		schedulingPriority = TestNatives.getGCAlarmSchedulingPriority();
		System.out.println(POLICY_SEPARATOR + schedulingPriority);
		
		System.exit(0); /* normal exit */
	}
	
	/**
	 * Helper method used to print the linux scheduling
	 * policy string to standard out based on the passed 
	 * in scheduling policy identifier
	 * 
	 * @param schedulingPolicy The integer value representing the scheduling policy to print
	 */
	private void printSchedulingPolicy(int schedulingPolicy) {
		String policyString = "unknown";
		switch (schedulingPolicy) {
		case TestNatives.TESTNATIVES_SCHED_OTHER:
			policyString = SCHED_OTHER_STRING;
			break;
		case TestNatives.TESTNATIVES_SCHED_RR:
			policyString = SCHED_RR_STRING;
			break;
		case TestNatives.TESTNATIVES_SCHED_FIFO:
			policyString = SCHED_FIFO_STRING;
			break;
		default:
			break;
		}
		
		System.out.print(policyString);
	}
}
