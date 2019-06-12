/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

package com.ibm.jvmti.tests.fieldwatch;
import java.util.concurrent.atomic.AtomicInteger;

public class fw001 {

	public static AtomicInteger numReports = new AtomicInteger();
	public static boolean success;
	public static int numExpectedReports = 5;
	public static int numTimeouts = 10;

	// Native methods to perform any initalization before running the test.
	public native boolean endTest(String name);
	public native boolean startTest(String name);

	// Add or remove field watches based on the information provided.
	public static native boolean modifyWatches(String className, String fieldName, String fieldSignature, boolean isStatic, boolean isRead, boolean isWrite, boolean isAdd);

	public static int incrementNumReports()
	{
		return numReports.incrementAndGet();
	}

	public static int getNumReports()
	{
		return numReports.get();
	}

	public static void setNumReports(int i)
	{
		numReports.set(i);
	}

	public String helpFieldWatch()
	{
		return "Fieldwatch test 1";
	}

	public boolean setup(String args)
	{
		return startTest(this.getClass().getName().replace('.', '/'));
	}

	public boolean teardown()
	{
		return endTest(this.getClass().getName().replace('.', '/'));
	}

	private boolean assertEquals(int expected, int actual)
	{
		if (expected != actual) {
			System.err.println("Expected: " + expected + ", but got: " + actual);

			try {
				throw new Exception("Test failure due to unexpected value.");
			} catch (Exception e) {
				e.printStackTrace();
			}

			success = false;
		}
		return success;
	}

	private boolean assertTrue(boolean condition) {
		if (!condition) {
			System.err.println("Expected TRUE instead got FALSE");
			try {
				throw new Exception("Test Failure");
			} catch (Exception e) {
				e.printStackTrace();
			}
			success = false;
		}
		return success;
	}

	public boolean testFieldWatch() throws InterruptedException
	{
		success = true;

		System.out.println("=============== Beginning - STATIC AND INSTANCE FIELD WATCH TESTS\n\n");

		boolean isRead = true;
		boolean isStatic = true;
		MyObject myObj = new MyObject();

		// Add watches and generate reports for static field reads/writes.
		runTest(myObj, isStatic, isRead);

		runTest(myObj, isStatic, !isRead);

		// Add watches and generate reports for instance field reads/writes.
		runTest(myObj, !isStatic, isRead);

		setNumReports(0);
		runTest(myObj, !isStatic, !isRead);

		System.out.println("=============== Ending - STATIC AND INSTANCE FIELD WATCH TESTS\n\n");
		return success;
	}

	public void runTest(MyObject myObj, boolean isStatic, boolean isRead) throws InterruptedException
	{
		// The fields that we will be adding watches for all start with the following prefix.
		String prefix = isStatic ? "static" : "instance";
		// List containing suffix of each field name and its signature.
		String[][] fieldData = {{"DoubleField","D"}, {"IntField","I"}, {"ObjField", "Ljava/lang/Object;"}, {"SingleField","F"}, {"LongField", "J"}};

		for (String[] currRow : fieldData) {
			// Reset number of reported events.
			setNumReports(0);

			System.out.println("===============adding " + (isRead ? "field read" : "field write") + " watches for " + prefix + currRow[0] + "\n\n");

			// Add field watch.
			if (!assertTrue(modifyWatches(myObj.getClass().getName().replace('.', '/'), prefix + currRow[0], currRow[1], isStatic, isRead, !isRead /*isWrite*/, true /* isAdd */))) {
				System.out.println("*ERROR*: Unable to add field watches for " + prefix + currRow[0] + "\n");
			}
			System.out.println("===============run\n\n");

			// Call Test routines that will invoke the watch events.
			for (int j = 0; j < numExpectedReports; j++) {
				TestDriver.jitme_write(myObj);
				TestDriver.jitme_read(myObj);
			}

			// Periodically poll to check if number of expected reports have been seen.
			int currTimeout = 0;

			do {
				System.out.println("Waiting for reports.. sleep iteration #" + currTimeout++ + "\n\n");
				Thread.sleep(700);
			} while (getNumReports() < numExpectedReports && currTimeout < numTimeouts);

			// Assert that we didn't timeout and the expected number of reports were seen.
			if (!assertEquals(numExpectedReports, getNumReports())) {
				System.out.println("*ERROR*: Invalid number of field reports detected: " + getNumReports() + ". Expected: " + numExpectedReports + "\n\n");
			}

			System.out.println("===============Disabling watches for " + prefix + currRow[0] + "\n\n");
			if (!assertTrue(modifyWatches(myObj.getClass().getName().replace('.', '/'), prefix + currRow[0], currRow[1], isStatic, isRead, !isRead, false /* isAdd */))) {
				System.out.println("*ERROR*: Unable to remove field watches for " + prefix + currRow[0] + "\n");
			}
		}
	}
}

