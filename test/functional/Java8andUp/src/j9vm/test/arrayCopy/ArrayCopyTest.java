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
package j9vm.test.arrayCopy;

import org.testng.Assert;
import org.testng.annotations.BeforeClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

/**
 * version 2.0 Test correctness of ArrayCopy implementations - References must be
 * copied atomically - Test Forward and Backward copy directions
 */
public class ArrayCopyTest {

	private static final Logger logger = Logger.getLogger(ArrayCopyTest.class);

	static int[] testSizes = { 1, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41,
			43, 47, 53, 59 };

	/* Reference Array Copy test data */
	static Object[] a = new Object[61];
	static Object firstString = "First";
	static Object secondString = "Second";

	/* eXtra Large Array Copy test data */
	/* 5M(32bit, cr) 10M (64bit) array */
	static Object[] xLarge = new Object[1310720];
	static int xLargeStartTestArea = 1048563;
	static int xLargeTestAreaSize = 61;

	/* Array of shorts test data */
	static short firstShort = -8531;
	static short secondShort = -16657;
	static short[] shorts = { firstShort, 0, secondShort, 0, firstShort, 0,
			secondShort, 0, firstShort, 0, secondShort, 0, firstShort, 0,
			secondShort, 0, firstShort, 0, secondShort, 0, firstShort, 0,
			secondShort, 0, firstShort, 0, secondShort, 0, firstShort, 0,
			secondShort, 0, firstShort, 0, secondShort, 0, firstShort, 0,
			secondShort, 0, firstShort, 0, secondShort, 0, firstShort, 0,
			secondShort, 0, firstShort, 0, secondShort, 0, firstShort, 0,
			secondShort, 0, firstShort, 0, secondShort, 0, firstShort };

	/* Array of ints test data */
	static int firstInt = -559038737;
	static int secondInt = 305419896;
	static int[] ints = { firstInt, 0, secondInt, 0, firstInt, 0, secondInt, 0,
			firstInt, 0, secondInt, 0, firstInt, 0, secondInt, 0, firstInt, 0,
			secondInt, 0, firstInt, 0, secondInt, 0, firstInt, 0, secondInt, 0,
			firstInt, 0, secondInt, 0, firstInt, 0, secondInt, 0, firstInt, 0,
			secondInt, 0, firstInt, 0, secondInt, 0, firstInt, 0, secondInt, 0,
			firstInt, 0, secondInt, 0, firstInt, 0, secondInt, 0, firstInt, 0,
			secondInt, 0, firstInt };

	/* Array of longs data */
	static long firstLong = 1311768467463790320L;
	static long secondLong = -81985529216486896L;
	static long[] longs = { firstLong, 0, secondLong, 0, firstLong, 0,
			secondLong, 0, firstLong, 0, secondLong, 0, firstLong, 0,
			secondLong, 0, firstLong, 0, secondLong, 0, firstLong, 0,
			secondLong, 0, firstLong, 0, secondLong, 0, firstLong, 0,
			secondLong, 0, firstLong, 0, secondLong, 0, firstLong, 0,
			secondLong, 0, firstLong, 0, secondLong, 0, firstLong, 0,
			secondLong, 0, firstLong, 0, secondLong, 0, firstLong, 0,
			secondLong, 0, firstLong, 0, secondLong, 0, firstLong };

	/* Errors */
	static boolean errorReferenceForward = false;
	static boolean errorReferenceBackward = false;

	static boolean errorShortsForward = false;
	static boolean errorShortsBackward = false;

	static boolean errorIntsForward = false;
	static boolean errorIntsBackward = false;

	static boolean errorLongsForward = false;
	static boolean errorLongsBackward = false;

	static boolean errorXtraLargeForward = false;
	static boolean errorXtraLargeBackward = false;

	/* Tests */
	static boolean referencesTest = false;
	static boolean shortsTest = false;
	static boolean intsTest = false;
	static boolean longsTest = false;
	static boolean xtraLargeTest = false;

	/* static boolean trace = false; */
	static boolean trace = false;

	/* System */
	static boolean mode64bit = false;

	/*
	 * Default test time 2 minutes
	 * Default run 4 total tests, long tests determined by is 64 bit
	 */
	static int secondsToSpin = 120; //
	static int totalTests = 0;
	/*
	 * This function is used by TestNG to mimic the main function on setting up environment
	 * Previous command line to execute the test:
	 * "$JAVA_HOME/java" (or with -Xint)  -Xgcpolicy:optthruput  -Xnocompressedrefs \
	 * -classpath "<path_to_jar>/<this_jar>.jar" j9vm.test.arrayCopy.ArrayCopyTest
	 *
	 * We use TestNG to execute this test by using default, which mean no input arguments
	 */
	@BeforeClass(alwaysRun = true)
	public static void setUp() {


		/* Is it 64-bit IBM Java VM? */
		String vmBits = System.getProperty("com.ibm.vm.bitmode");
		if (vmBits.equals("64")) {
			mode64bit = true;
		} else if (vmBits.equals("32")) {
			mode64bit = false;
		} else {
			Assert.fail("!!! Can not recognize IBM Java VM !!!");
		}

		totalTests = 4;
		/* if run long test */
		if (mode64bit) {
			longsTest = true;
			totalTests++;
		}
	}

	/* Launch Reference Array Copy test */
	@Test(groups = { "level.sanity" })
	public static void testReferenceArrayCopy() {
		testReferenceArrayCopy(secondsToSpin / totalTests);
		Assert.assertFalse(errorReferenceForward, "!!! Test Failed: Forward Reference Array Copy Error !!!"); //$NON-NLS-1$
		Assert.assertFalse(errorReferenceBackward, "!!! Test Failed: Backward Reference Array Copy Error !!!"); //$NON-NLS-1$
	}

	/* Launch Reference Array Copy test */
	@Test(groups = { "level.sanity" })
	public static void testXtraLargeReferenceArrayCopy() {
		testXtraLargeReferenceArrayCopy(secondsToSpin / totalTests);
		Assert.assertFalse(errorXtraLargeForward, "!!! Test Failed: Forward eXtra Large Reference Array Copy Error !!!"); //$NON-NLS-1$
		Assert.assertFalse(errorXtraLargeBackward, "!!! Test Failed: Backward eXtra Large Reference Array Copy Error !!!"); //$NON-NLS-1$
	}

	/* Launch Array of shorts Copy test */
	@Test(groups = { "level.sanity" })
	public static void testArrayOfShortsCopy() {
		testArrayOfShortsCopy(secondsToSpin / totalTests);
		Assert.assertFalse(errorShortsForward, "!!! Test Failed: Forward Array of shorts Copy Error !!!"); //$NON-NLS-1$
		Assert.assertFalse(errorShortsBackward, "!!! Test Failed: Backward Array of shorts Copy Error !!!"); //$NON-NLS-1$
	}

	/* Launch Array of ints Copy test */
	@Test(groups = { "level.sanity" })
	public static void testArrayOfIntsCopy() {
		testArrayOfIntsCopy(secondsToSpin / totalTests);
		Assert.assertFalse(errorIntsForward, "!!! Test Failed: Forward Array of ints Copy Error !!!"); //$NON-NLS-1$
		Assert.assertFalse(errorIntsBackward, "!!! Test Failed: Backward Array of ints Copy Error !!!"); //$NON-NLS-1$
	}

	/* Launch Array of longs Copy test */
	@Test(groups = { "level.sanity" })
	public static void testArrayOfLongsCopy() {
		if(longsTest){
			testArrayOfLongsCopy(secondsToSpin / totalTests);
			Assert.assertFalse(errorLongsForward, "!!! Test Failed: Forward Array of longs Copy Error !!!"); //$NON-NLS-1$
			Assert.assertFalse(errorLongsBackward, "!!! Test Failed: Backward Array of longs Copy Error !!!"); //$NON-NLS-1$
		}
	}

	public static void main(String[] args) {

		/* Default test time 2 minutes */

		/* Is it 64-bit IBM Java VM? */
		String vmBits = System.getProperty("com.ibm.vm.bitmode");
		if (vmBits.equals("64")) {
			mode64bit = true;
		} else if (vmBits.equals("32")) {
			mode64bit = false;
		} else {
			System.err.println("!!! Can not recognize IBM Java VM !!!");
			System.exit(6);
		}

		/*
		 * if first parameter is specified it is custom test time or "h" for
		 * help
		 */
		if (1 <= args.length) {

			/* first parameter might be a help request */
			if ((-1 != args[0].indexOf("H")) || (-1 != args[0].indexOf("h"))) {

				System.err.println(" ");
				System.err.println("   ArrayCopyTest, version 2.0");
				System.err.println("       Test System.arraycopy for atomic update of array elements");
				System.err.println(" ");
				System.err.println("   Using: java -cp ArrayCopyTest.jar ArrayCopyTest [<test_time> <test_targets>]");
				System.err.println(" ");
				System.err.println("       <test_time> - desired test time in seconds larger or equal then 10 seconds");
				System.err.println("                     test time will be split equally between requested tests");
				System.err.println("                     Default test time is 120 seconds");
				System.err.println(" ");
				System.err.println("       <test_targets> - any combination from RXSILT (upper or low case):");
				System.err.println("           R - Reference array test");
				System.err.println("           X - eXtra large reference array test");
				System.err.println("           S - array of Shorts test");
				System.err.println("           I - array of Ints test");
				System.err.println("           L - array of Longs test");
				System.err.println("           T - Trace: show tests execution step by step");
				System.err.println("               Default test mode is rxsil on 64-bit VM and rxsi on 32-bit VM");
				System.err.println(" ");
				System.err.println("   Example: java -cp ArrayCopyTest.jar ArrayCopyTest 60 rxsil");
				System.err.println(
						"       This test does Reference, eXtra large, Shorts, Ints, Longs arrays for 60 seconds");
				System.exit(2);
			}

			try {
				secondsToSpin = Integer.parseInt(args[0]);
			} catch (Exception e) {
				System.err.println("!!! first parameter must be numeric (test time in seconds) or h for help !!!");
				System.exit(5);
			}

			/* and it can not be shorter then 10 seconds */
			if (secondsToSpin < 10) {
				secondsToSpin = 10;
			}
		}

		/* parse test modes */
		/* if second parameter is specified its overwrites test mode */
		if (2 <= args.length) {

			if (3 <= args.length) {
				System.err.println(
						"!!! Too many parameters, for help call java -cp ArrayCopyTest.jar ArrayCopyTest help !!!");
				System.exit(4);
			}

			if ((-1 != args[1].indexOf("T")) || (-1 != args[1].indexOf("t"))) {
				trace = true;
			}

			if ((-1 != args[1].indexOf("R")) || (-1 != args[1].indexOf("r"))) {
				referencesTest = true;
				totalTests++;
			}

			if ((-1 != args[1].indexOf("S")) || (-1 != args[1].indexOf("s"))) {
				shortsTest = true;
				totalTests++;
			}

			if ((-1 != args[1].indexOf("I")) || (-1 != args[1].indexOf("i"))) {
				intsTest = true;
				totalTests++;
			}

			if ((-1 != args[1].indexOf("L")) || (-1 != args[1].indexOf("l"))) {
				longsTest = true;
				totalTests++;
			}

			if ((-1 != args[1].indexOf("X")) || (-1 != args[1].indexOf("x"))) {
				xtraLargeTest = true;
				totalTests++;
			}

			if (0 == totalTests) {
				System.err.println(
						"Invalid option given for tests  Value given must have test request form list: RXSILT");
				System.exit(3);
			}

		} else {
			/* Test mode is not specified, set default */

			referencesTest = true;
			xtraLargeTest = true;
			shortsTest = true;
			intsTest = true;

			totalTests = 4;

			if (mode64bit) {
				longsTest = true;
				totalTests++;
			}

		}

		/* Execute requested tests */

		if (referencesTest) {
			/* Launch Reference Array Copy test */
			testReferenceArrayCopy(secondsToSpin / totalTests);
		}

		if (xtraLargeTest) {
			/* Launch Reference Array Copy test */
			testXtraLargeReferenceArrayCopy(secondsToSpin / totalTests);
		}

		if (shortsTest) {
			/* Launch Array of shorts Copy test */
			testArrayOfShortsCopy(secondsToSpin / totalTests);
		}

		if (intsTest) {
			/* Launch Array of ints Copy test */
			testArrayOfIntsCopy(secondsToSpin / totalTests);
		}

		if (longsTest) {
			/* Launch Array of longs Copy test */
			testArrayOfLongsCopy(secondsToSpin / totalTests);
		}

		/* Check test results */
		if (errorsInTest()) {
			System.err.println("!!! Test Failed !!!");
			System.exit(1);
		}

		System.out.println("--- All Array Copy Tests Passed ---");
		System.exit(0);
	}

	/*
	 * Check test results
	 */
	static public boolean errorsInTest() {
		boolean error = false;

		System.out.println("------------------------------------------------------------------------");

		/* Reference */
		if (referencesTest) {
			if (errorReferenceForward) {
				error = true;
				System.err.println("!!! Test Failed: Forward Reference Array Copy Error !!!");
			} else {
				System.out.println("--- Test Passed: Forward Reference Array Copy ---");
			}

			if (errorReferenceBackward) {
				error = true;
				System.err.println("!!! Test Failed: Backward Reference Array Copy Error !!!");
			} else {
				System.out.println("--- Test Passed: Backward Reference Array Copy ---");
			}
		}

		if (xtraLargeTest) {
			if (errorXtraLargeForward) {
				error = true;
				System.err.println("!!! Test Failed: Forward eXtra Large Reference Array Copy Error !!!");
			} else {
				System.out.println("--- Test Passed: Forward eXtra Large Reference Array Copy ---");
			}

			if (errorXtraLargeBackward) {
				error = true;
				System.err.println("!!! Test Failed: Backward eXtra Large Reference Array Copy Error !!!");
			} else {
				System.out.println("--- Test Passed: Backward eXtra Large Reference Array Copy ---");
			}
		}

		if (shortsTest) {
			/* Shorts */
			if (errorShortsForward) {
				error = true;
				System.err.println("!!! Test Failed: Forward Array of shorts Copy Error !!!");
			} else {
				System.out.println("--- Test Passed: Forward Array of shorts Copy ---");
			}

			if (errorShortsBackward) {
				error = true;
				System.err.println("!!! Test Failed: Backward Array of shorts Copy Error !!!");
			} else {
				System.out.println("--- Test Passed: Backward Array of shorts Copy ---");
			}
		}

		if (intsTest) {
			/* Ints */
			if (errorIntsForward) {
				error = true;
				System.err.println("!!! Test Failed: Forward Array of ints Copy Error !!!");
			} else {
				System.out.println("--- Test Passed: Forward Array of ints Copy ---");
			}

			if (errorIntsBackward) {
				error = true;
				System.err.println("!!! Test Failed: Backward Array of ints Copy Error !!!");
			} else {
				System.out.println("--- Test Passed: Backward Array of ints Copy ---");
			}
		}

		if (longsTest) {
			/* Longs */
			if (mode64bit) {
				if (errorLongsForward) {
					error = true;
					System.err.println("!!! Test Failed: Forward Array of longs Copy Error !!!");
				} else {
					System.out.println("--- Test Passed: Forward Array of longs Copy ---");
				}

				if (errorLongsBackward) {
					error = true;
					System.err.println("!!! Test Failed: Backward Array of longs Copy Error !!!");
				} else {
					System.out.println("--- Test Passed: Backward Array of longs Copy ---");
				}
			} else {
				if (errorLongsForward) {
					System.out.println("--- Forward Array of longs Copy does not pass but it is a 32-bit VM ---");
				} else {
					System.out.println("--- Test Passed: Forward Array of longs Copy ---");
				}

				if (errorLongsBackward) {
					System.out.println("--- Backward Array of longs Copy does not pass but it is a 32-bit VM ---");
				} else {
					System.out.println("--- Test Passed: Backward Array of longs Copy ---");
				}
			}
		}

		System.out.println("------------------------------------------------------------------------");
		return error;
	}

	/*
	 * --------------- Reference Array Copy test
	 * -----------------------------------
	 */

	static public void testReferenceArrayCopy(int secondsToSpin) {
		long halfTime = System.currentTimeMillis() + (secondsToSpin * 500L);
		long finishTime = halfTime + secondsToSpin * 500;

		/* Array initialization */
		initReferenceArray(a, 0, a.length);

		/* Create slave thread(s) for constant reading */
		ReadReferenceArrayInThread t = new ReadReferenceArrayInThread(a, "ThreadReference", 0, a.length, firstString,
				secondString);

		/* run tests */

		int i;

		if (trace) {
			System.out.println("----- Reference FWD start");
		}

		/* run Array Copy Forward test */
		while ((System.currentTimeMillis() < halfTime) && !errorReferenceForward) {
			for (i = 0; i < testSizes.length; i++) {
				testReferenceArrayCopyForward(testSizes[i]);
				for (int j = 0; j < a.length; j++) {
					testReferenceArrayCopyForwardNoOverlap(testSizes[i]);
				}
			}
			errorReferenceForward = t.isErrorFound();
		}

		if (trace) {
			System.out.println("----- Reference FWD end");
		}

		if (errorReferenceForward) {
			if (trace) {
				System.err.println("-!!!- Reference FWD error");
			}
			/* clear error state in reader */
			t.clearError();
		}

		if (trace) {
			System.out.println("----- Reference BWD start");
		}

		/* run Array Copy Backward test */
		while ((System.currentTimeMillis() < finishTime) && !errorReferenceBackward) {
			for (i = 0; i < testSizes.length; i++) {
				testReferenceArrayCopyBackward(testSizes[i]);
				for (int j = 0; j < a.length; j++) {
					testReferenceArrayCopyBackwardNoOverlap(testSizes[i]);
				}
			}
			errorReferenceBackward = t.isErrorFound();
		}

		if (trace) {
			System.out.println("----- Reference BWD end");
		}

		if (errorReferenceBackward) {
			if (trace) {
				System.err.println("-!!!- Reference BWD error");
			}
			/* clear error state in reader */
			t.clearError();
		}

		/* Shutdown slave thread(s) */
		t.endThread();
	}

	/* Reference Array initialization */
	static public void initReferenceArray(Object[] object, int start, int size) {
		int iterations = size / 4;
		int reminder = size % 4;
		int i = start;

		/* fill *4 part */
		while (iterations-- > 0) {
			object[i++] = firstString;
			object[i++] = null;
			object[i++] = secondString;
			object[i++] = null;
		}

		/* fill reminder */

		if (reminder-- > 0) {
			object[i++] = firstString;
		}

		if (reminder-- > 0) {
			object[i++] = null;
		}

		if (reminder-- > 0) {
			object[i++] = secondString;
		}
	}


	static public void testReferenceArrayCopyForward(int size) {
		/* size can not be larger then array size - 1 */
		if (size >= a.length) {
			size = a.length - 1;
		}

		/* copy until wraps around */
		for (int i = 0; i < a.length; i++) {
			/* calculate number of chunks and size of reminder */
			int iterations = (a.length - 1) / size;
			int reminder = a.length - (iterations * size) - 1;

			/* save first */
			Object tmp = a[0];

			/* copy by 'size' chunks */
			int j = 1;
			while (iterations-- > 0) {
				System.arraycopy(a, j, a, j - 1, size);
				j = j + size;
			}

			/* copy reminder if necessary */
			if (0 != reminder) {
				System.arraycopy(a, j, a, j - 1, reminder);
			}

			/* first to last */
			a[a.length - 1] = tmp;
		}
	}


	static public void testReferenceArrayCopyForwardNoOverlap(int size) {
		/* size can not be larger then array size - 1 */
		if (size >= a.length) {
			size = a.length - 1;
		}

		int iterations = a.length / size;
		int reminder = a.length - (iterations * size);

		/* size can not be larger then half of array */
		if (iterations > 1) {

			/* save first elements in temp array */
			Object[] tmp = new Object[size];

			System.arraycopy(a, 0, tmp, 0, size);

			/* Copy array by not overlaped chunks */
			for (int i = 0; i < iterations - 1; i++) {
				System.arraycopy(a, (i + 1) * size, a, i * size, size);
			}

			/* copy reminder if necessary */
			if (0 != reminder) {
				System.arraycopy(a, iterations * size, a, (iterations - 1)
						* size, reminder);
			}

			/* restore first elements stored in temp to last */
			System.arraycopy(tmp, 0, a, a.length - size, size);
		}
	}


	static public void testReferenceArrayCopyBackward(int size) {
		/* size can not be larger then array size - 1 */
		if (size >= a.length) {
			size = a.length - 1;
		}

		/* copy until wraps around */
		for (int i = 0; i < a.length; i++) {
			/* calculate number of chunks and size of reminder */
			int iterations = (a.length - 1) / size;
			int reminder = a.length - (iterations * size) - 1;

			/* save last */
			Object tmp = a[a.length - 1];

			/* copy by 'size' chunks */
			int j = a.length - 1 - size;
			while (iterations-- > 0) {
				System.arraycopy(a, j, a, j + 1, size);
				j = j - size;
			}

			/* copy reminder if necessary */
			if (0 != reminder) {
				System.arraycopy(a, 0, a, 1, reminder);
			}

			/* last to first */
			a[0] = tmp;
		}
	}


	static public void testReferenceArrayCopyBackwardNoOverlap(int size) {
		/* size can not be larger then array size - 1 */
		if (size >= a.length) {
			size = a.length - 1;
		}

		int iterations = a.length / size;
		int reminder = a.length - (iterations * size);

		/* size can not be larger then half of array */
		if (iterations > 1) {

			/* save first elements in temp array */
			Object[] tmp = new Object[size];

			System.arraycopy(a, a.length - size, tmp, 0, size);

			/* Copy array by not overlaped chunks */
			for (int i = 1; i < iterations; i++) {
				System.arraycopy(a, a.length - (i + 1) * size, a, a.length - i
						* size, size);
			}

			/* copy reminder if necessary */
			if (0 != reminder) {
				System.arraycopy(a, 0, a, size, reminder);
			}

			/* restore first elements stored in temp to last */
			System.arraycopy(tmp, 0, a, 0, size);
		}
	}

	/*
	 * --------------- eXtra Large Reference Array Copy test
	 * -----------------------------------
	 */

	static public void testXtraLargeReferenceArrayCopy(int secondsToSpin) {
		long halfTime = System.currentTimeMillis() + (secondsToSpin * 500L);
		long finishTime = halfTime + secondsToSpin * 500;

		/* Array initialization */
		initReferenceArray(xLarge, xLargeStartTestArea, xLargeTestAreaSize);

		/* Create slave thread(s) for constant reading */
		/* Set test area: start at (1M - 13) elements, size = 61 elements */
		ReadReferenceArrayInThread t = new ReadReferenceArrayInThread(xLarge,
				"ThreadXLargeReference", xLargeStartTestArea,
				xLargeTestAreaSize, firstString, secondString);

		/* run tests */

		int i;

		if (trace) {
			System.out.println("----- xLarge FWD start");
		}

		/* run Array Copy Forward test */
		while ((System.currentTimeMillis() < halfTime)
				&& !errorXtraLargeForward) {
			for (i = 0; i < testSizes.length; i++) {
				testXtraLargeReferenceArrayCopyForward(testSizes[i]);
			}
			errorXtraLargeForward = t.isErrorFound();
		}

		if (trace) {
			System.out.println("----- xLarge FWD end");
		}

		if (errorXtraLargeForward) {
			if (trace) {
				System.err.println("-!!!- xLarge FWD error");
			}
			/* clear error state in reader */
			t.clearError();
		}

		if (trace) {
			System.out.println("----- xLarge BWD start");
		}

		/* run Array Copy Backward test */
		while ((System.currentTimeMillis() < finishTime)
				&& !errorXtraLargeBackward) {
			for (i = 0; i < testSizes.length; i++) {
				testXtraLargeReferenceArrayCopyBackward(testSizes[i]);
			}
			errorXtraLargeBackward = t.isErrorFound();
		}

		if (trace) {
			System.out.println("----- xLarge BWD end");
		}

		if (errorXtraLargeBackward) {
			if (trace) {
				System.err.println("-!!!- xLarge BWD error");
			}
			/* clear error state in reader */
			t.clearError();
		}

		/* Shutdown slave thread(s) */
		t.endThread();
	}


	static public void testXtraLargeReferenceArrayCopyForward(int size) {
		/* size can not be larger then test area size - 1 */
		if (size >= xLargeTestAreaSize) {
			size = xLargeTestAreaSize - 1;
		}

		/* copy until wraps around */
		for (int i = xLargeStartTestArea; i < (xLargeStartTestArea + xLargeTestAreaSize); i++) {
			/* calculate number of chunks and size of reminder */
			int iterations = (xLargeTestAreaSize - 1) / size;
			int reminder = xLargeTestAreaSize - (iterations * size) - 1;

			/* save first */
			Object tmp = xLarge[xLargeStartTestArea];

			/* copy by 'size' chunks */
			int j = xLargeStartTestArea + 1;
			while (iterations-- > 0) {
				System.arraycopy(xLarge, j, xLarge, j - 1, size);
				j = j + size;
			}

			/* copy reminder if necessary */
			if (0 != reminder) {
				System.arraycopy(xLarge, j, xLarge, j - 1, reminder);
			}

			/* first to last */
			xLarge[xLargeStartTestArea + xLargeTestAreaSize - 1] = tmp;
		}
	}


	static public void testXtraLargeReferenceArrayCopyBackward(int size) {
		/* size can not be larger then test area size - 1 */
		if (size >= xLargeTestAreaSize) {
			size = xLargeTestAreaSize - 1;
		}

		for (int i = xLargeStartTestArea; i < (xLargeStartTestArea + xLargeTestAreaSize); i++) {
			/* calculate number of chunks and size of reminder */
			int iterations = (xLargeTestAreaSize - 1) / size;
			int reminder = xLargeTestAreaSize - (iterations * size) - 1;

			/* save last */
			Object tmp = xLarge[xLargeStartTestArea + xLargeTestAreaSize - 1];

			/* copy by 'size' chunks */
			int j = xLargeStartTestArea + xLargeTestAreaSize - 1 - size;
			while (iterations-- > 0) {
				System.arraycopy(xLarge, j, xLarge, j + 1, size);
				j = j - size;
			}

			/* copy reminder if necessary */
			if (0 != reminder) {
				System.arraycopy(xLarge, xLargeStartTestArea, xLarge,
						xLargeStartTestArea + 1, reminder);
			}

			/* last to first */
			xLarge[xLargeStartTestArea] = tmp;
		}
	}

	/*
	 * --------------- Array of shorts Copy test
	 * ----------------------------------------------------------
	 */

	static public void testArrayOfShortsCopy(int secondsToSpin) {
		long halfTime = System.currentTimeMillis() + (secondsToSpin * 500L);
		long finishTime = halfTime + secondsToSpin * 500;

		/* Create slave thread(s) for constant reading */
		ReadArrayOfShortsInThread t = new ReadArrayOfShortsInThread(shorts,
				"ThreadShorts", firstShort, secondShort);

		/* run tests */

		int i;

		if (trace) {
			System.out.println("----- Shorts FWD start");
		}

		/* run Array Copy Forward test */
		while ((System.currentTimeMillis() < halfTime) && !errorShortsForward) {
			for (i = 0; i < testSizes.length; i++) {
				testArrayOfShortsCopyForward(testSizes[i]);
			}
			errorShortsForward = t.isErrorFound();
		}

		if (trace) {
			System.out.println("----- Shorts FWD end");
		}

		if (errorShortsForward) {
			if (trace) {
				System.err.println("-!!!- Shorts FWD error");
			}
			/* clear error state in reader */
			t.clearError();
		}

		if (trace) {
			System.out.println("----- Shorts BWD start");
		}

		/* run Array Copy Backward test */
		while ((System.currentTimeMillis() < finishTime)
				&& !errorShortsBackward) {
			for (i = 0; i < testSizes.length; i++) {
				testArrayOfShortsCopyBackward(testSizes[i]);
			}
			errorShortsBackward = t.isErrorFound();
		}

		if (trace) {
			System.out.println("----- Shorts BWD end");
		}

		if (errorShortsBackward) {
			if (trace) {
				System.err.println("-!!!- Shorts BWD error");
			}
			/* clear error state in reader */
			t.clearError();
		}

		/* Shutdown slave thread(s) */
		t.endThread();
	}


	static public void testArrayOfShortsCopyForward(int size) {
		/* size can not be larger then array size - 1 */
		if (size >= shorts.length) {
			size = shorts.length - 1;
		}

		/* copy until wraps around */
		for (int i = 0; i < shorts.length; i++) {
			/* calculate number of chunks and size of reminder */
			int iterations = (shorts.length - 1) / size;
			int reminder = shorts.length - (iterations * size) - 1;

			/* save first */
			short tmp = shorts[0];

			/* copy by 'size' chunks */
			int j = 1;
			while (iterations-- > 0) {
				System.arraycopy(shorts, j, shorts, j - 1, size);
				j = j + size;
			}

			/* copy reminder if necessary */
			if (0 != reminder) {
				System.arraycopy(shorts, j, shorts, j - 1, reminder);
			}

			/* first to last */
			shorts[shorts.length - 1] = tmp;
		}
	}


	static public void testArrayOfShortsCopyBackward(int size) {
		/* size can not be larger then array size - 1 */
		if (size >= shorts.length) {
			size = shorts.length - 1;
		}

		/* copy until wraps around */
		for (int i = 0; i < shorts.length; i++) {
			/* calculate number of chunks and size of reminder */
			int iterations = (shorts.length - 1) / size;
			int reminder = shorts.length - (iterations * size) - 1;

			/* save last */
			short tmp = shorts[shorts.length - 1];

			/* copy by 'size' chunks */
			int j = shorts.length - 1 - size;
			while (iterations-- > 0) {
				System.arraycopy(shorts, j, shorts, j + 1, size);
				j = j - size;
			}

			/* copy reminder if necessary */
			if (0 != reminder) {
				System.arraycopy(shorts, 0, shorts, 1, reminder);
			}

			/* last to first */
			shorts[0] = tmp;
		}
	}

	/*
	 * --------------- Array of ints Copy test
	 * ---------------------------------------------------
	 */

	static public void testArrayOfIntsCopy(int secondsToSpin) {
		long halfTime = System.currentTimeMillis() + (secondsToSpin * 500L);
		long finishTime = halfTime + secondsToSpin * 500;

		/* Create slave thread(s) for constant reading */
		ReadArrayOfIntsInThread t = new ReadArrayOfIntsInThread(ints,
				"ThreadInts", firstInt, secondInt);

		/* run tests */

		int i;

		if (trace) {
			System.out.println("----- Ints FWD start");
		}

		/* run Array Copy Forward test */
		while ((System.currentTimeMillis() < halfTime) && !errorIntsForward) {
			for (i = 0; i < testSizes.length; i++) {
				testArrayOfIntsCopyForward(testSizes[i]);
			}
			errorIntsForward = t.isErrorFound();
		}

		if (trace) {
			System.out.println("----- Ints FWD end");
		}

		if (errorIntsForward) {
			if (trace) {
				System.err.println("-!!!- Ints FWD error");
			}
			/* clear error state in reader */
			t.clearError();
		}

		if (trace) {
			System.out.println("----- Ints BWD start");
		}

		/* run Array Copy Backward test */
		while ((System.currentTimeMillis() < finishTime) && !errorIntsBackward) {
			for (i = 0; i < testSizes.length; i++) {
				testArrayOfIntsCopyBackward(testSizes[i]);
			}
			errorIntsBackward = t.isErrorFound();
		}

		if (trace) {
			System.out.println("----- Ints BWD end");
		}

		if (errorIntsBackward) {
			if (trace) {
				System.err.println("-!!!- Ints BWD error");
			}
			/* clear error state in reader */
			t.clearError();
		}

		/* Shutdown slave thread(s) */
		t.endThread();
	}


	static public void testArrayOfIntsCopyForward(int size) {
		/* size can not be larger then array size - 1 */
		if (size >= ints.length) {
			size = ints.length - 1;
		}

		/* copy until wraps around */
		for (int i = 0; i < ints.length; i++) {
			/* calculate number of chunks and size of reminder */
			int iterations = (ints.length - 1) / size;
			int reminder = ints.length - (iterations * size) - 1;

			/* save first */
			int tmp = ints[0];

			/* copy by 'size' chunks */
			int j = 1;
			while (iterations-- > 0) {
				System.arraycopy(ints, j, ints, j - 1, size);
				j = j + size;
			}

			/* copy reminder if necessary */
			if (0 != reminder) {
				System.arraycopy(ints, j, ints, j - 1, reminder);
			}

			/* first to last */
			ints[ints.length - 1] = tmp;
		}
	}


	static public void testArrayOfIntsCopyBackward(int size) {
		/* size can not be larger then array size - 1 */
		if (size >= ints.length) {
			size = ints.length - 1;
		}

		/* copy until wraps around */
		for (int i = 0; i < ints.length; i++) {
			/* calculate number of chunks and size of reminder */
			int iterations = (ints.length - 1) / size;
			int reminder = ints.length - (iterations * size) - 1;

			/* save last */
			int tmp = ints[ints.length - 1];

			/* copy by 'size' chunks */
			int j = ints.length - 1 - size;
			while (iterations-- > 0) {
				System.arraycopy(ints, j, ints, j + 1, size);
				j = j - size;
			}

			/* copy reminder if necessary */
			if (0 != reminder) {
				System.arraycopy(ints, 0, ints, 1, reminder);
			}

			/* last to first */
			ints[0] = tmp;
		}
	}

	/*
	 * --------------- Array of longs Copy test
	 * ---------------------------------------------------
	 */

	static public void testArrayOfLongsCopy(int secondsToSpin) {
		long halfTime = System.currentTimeMillis() + (secondsToSpin * 500L);
		long finishTime = halfTime + secondsToSpin * 500;

		/* Create slave thread(s) for constant reading */
		ReadArrayOfLongsInThread t = new ReadArrayOfLongsInThread(longs,
				"ThreadLongs", firstLong, secondLong);

		/* run tests */

		int i;

		if (trace) {
			System.out.println("----- Longs FWD start");
		}

		/* run Array Copy Forward test */
		while ((System.currentTimeMillis() < halfTime) && !errorLongsForward) {
			for (i = 0; i < testSizes.length; i++) {
				testArrayOfLongsCopyForward(testSizes[i]);
			}
			errorLongsForward = t.isErrorFound();
		}

		if (trace) {
			System.out.println("----- Longs FWD end");
		}

		if (errorLongsForward) {
			if (trace) {
				if (mode64bit) {
					System.err.println("-!!!- Longs FWD error");
				} else {
					System.out.println("-+++- Longs FWD error on 32-bit VM");
				}
			}
			/* clear error state in reader */
			t.clearError();
		}

		if (trace) {
			System.out.println("----- Longs BWD start");
		}

		/* run Array Copy Backward test */
		while ((System.currentTimeMillis() < finishTime) && !errorLongsBackward) {
			for (i = 0; i < testSizes.length; i++) {
				testArrayOfLongsCopyBackward(testSizes[i]);
			}
			errorLongsBackward = t.isErrorFound();
		}

		if (trace) {
			System.out.println("----- Longs BWD end");
		}

		if (errorLongsBackward) {
			if (trace) {
				if (mode64bit) {
					System.err.println("-!!!- Longs BWD error");
				} else {
					System.out.println("-+++- Longs BWD error on 32-bit VM");
				}
			}
			/* clear error state in reader */
			t.clearError();
		}

		/* Shutdown slave thread(s) */
		t.endThread();
	}


	static public void testArrayOfLongsCopyForward(int size) {
		/* size can not be larger then array size - 1 */
		if (size >= longs.length) {
			size = longs.length - 1;
		}

		/* copy until wraps around */
		for (int i = 0; i < longs.length; i++) {
			/* calculate number of chunks and size of reminder */
			int iterations = (longs.length - 1) / size;
			int reminder = longs.length - (iterations * size) - 1;

			/* save first */
			long tmp = longs[0];

			/* copy by 'size' chunks */
			int j = 1;
			while (iterations-- > 0) {
				System.arraycopy(longs, j, longs, j - 1, size);
				j = j + size;
			}

			/* copy reminder if necessary */
			if (0 != reminder) {
				System.arraycopy(longs, j, longs, j - 1, reminder);
			}

			/* first to last */
			longs[longs.length - 1] = tmp;
		}
	}


	static public void testArrayOfLongsCopyBackward(int size) {
		/* size can not be larger then array size - 1 */
		if (size >= longs.length) {
			size = longs.length - 1;
		}

		/* copy until wraps around */
		for (int i = 0; i < longs.length; i++) {
			/* calculate number of chunks and size of reminder */
			int iterations = (longs.length - 1) / size;
			int reminder = longs.length - (iterations * size) - 1;

			/* save last */
			long tmp = longs[longs.length - 1];

			/* copy by 'size' chunks */
			int j = longs.length - 1 - size;
			while (iterations-- > 0) {
				System.arraycopy(longs, j, longs, j + 1, size);
				j = j - size;
			}

			/* copy reminder if necessary */
			if (0 != reminder) {
				System.arraycopy(longs, 0, longs, 1, reminder);
			}

			/* last to first */
			longs[0] = tmp;
		}
	}
}

/*
 * --------------- Reader implementation
 * --------------------------------------------
 */
/*
 * Abstract part of Reader
 */
abstract class ReadArrayInThread implements Runnable {
	Thread t;
	volatile boolean error = false;
	volatile boolean shutdown = false;

	public abstract void run();

	public boolean isErrorFound() {
		return error;
	}

	public void clearError() {
		error = false;
	}

	public void endThread() {
		/* request slave thread to stop */
		shutdown = true;

		try {
			t.join();
		} catch (InterruptedException e) {
		}
	}
}

/*
 * Reader for Reference array
 */
class ReadReferenceArrayInThread extends ReadArrayInThread {
	Object[] object;
	Object first;
	Object second;
	int startIndex;
	int sizeOfTestArea;

	ReadReferenceArrayInThread(Object[] givenObject, String name,
			int givenStartIndex, int givenSize, Object givenFirst,
			Object givenSecond) {
		object = givenObject;
		startIndex = givenStartIndex;
		sizeOfTestArea = givenSize;
		first = givenFirst;
		second = givenSecond;
		error = false;
		shutdown = false;
		t = new Thread(this, name);
		t.start();
	}

	public void run() {
		while (!shutdown) {
			for (int i = startIndex; i < (startIndex + sizeOfTestArea); i++) {
				Object tmp = object[i];
				if (null != tmp) {
					if (tmp == first) {
					} else if (tmp == second) {
					} else {
						error = true;
					}
				}
			}
		}
	}
}

/*
 * Reader for Array of Shorts
 */
class ReadArrayOfShortsInThread extends ReadArrayInThread {
	short[] a;
	short first;
	short second;

	ReadArrayOfShortsInThread(short[] givenObject, String name,
			short givenFirst, short givenSecond) {
		a = givenObject;
		first = givenFirst;
		second = givenSecond;
		error = false;
		shutdown = false;
		t = new Thread(this, name);
		t.start();
	}

	public void run() {
		while (!shutdown) {
			for (int i = 0; i < a.length; i++) {
				short tmp = a[i];
				if (0 != tmp) {
					if (tmp == first) {
					} else if (tmp == second) {
					} else {
						error = true;
					}
				}
			}
		}
	}
}

/*
 * Reader for Array of Ints
 */
class ReadArrayOfIntsInThread extends ReadArrayInThread {
	int[] a;
	int first;
	int second;

	ReadArrayOfIntsInThread(int[] givenObject, String name, int givenFirst,
			int givenSecond) {
		a = givenObject;
		first = givenFirst;
		second = givenSecond;
		error = false;
		shutdown = false;
		t = new Thread(this, name);
		t.start();
	}

	public void run() {
		while (!shutdown) {
			for (int i = 0; i < a.length; i++) {
				int tmp = a[i];
				if (0 != tmp) {
					if (tmp == first) {
					} else if (tmp == second) {
					} else {
						error = true;
					}
				}
			}
		}
	}
}

/*
 * Reader for Array of Longs
 */
class ReadArrayOfLongsInThread extends ReadArrayInThread {
	long[] a;
	long first;
	long second;

	ReadArrayOfLongsInThread(long[] givenObject, String name, long givenFirst,
			long givenSecond) {
		a = givenObject;
		first = givenFirst;
		second = givenSecond;
		error = false;
		shutdown = false;
		t = new Thread(this, name);
		t.start();
	}

	public void run() {
		while (!shutdown) {
			for (int i = 0; i < a.length; i++) {
				long tmp = a[i];
				if (0 != tmp) {
					if (tmp == first) {
					} else if (tmp == second) {
					} else {
						error = true;
					}
				}
			}
		}
	}
}
