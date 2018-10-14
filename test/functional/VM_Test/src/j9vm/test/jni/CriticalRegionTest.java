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
package j9vm.test.jni;

import java.util.Random;

public class CriticalRegionTest
{
	private static native boolean testGetRelease(byte[] array);
	private static native boolean testModify(byte[] array, boolean commitChanges);
	private static native boolean testMemcpy(byte[] src, byte[] dest);

	private static native boolean testGetRelease(int[] array);
	private static native boolean testModify(int[] array, boolean commitChanges);
	private static native boolean testMemcpy(int[] src, int[] dest);

	private static native boolean testGetRelease(double[] array);
	private static native boolean testModify(double[] array, boolean commitChanges);
	private static native boolean testMemcpy(double[] src, double[] dest);
	
	private static native boolean testGetRelease(String string);
	private static native boolean testModify(String string);
	private static native boolean testMemcpy(String src, String dest);

	private static native boolean holdsVMAccess(byte[] array);
	private static native boolean returnsDirectPointer(byte[] array);
	private static native boolean acquireAndSleep(byte[] array, long millis);
	private static native boolean acquireAndCallIn(byte[] array, AcquireAndCallInThread thread);
	private static native boolean acquireDiscardAndGC(byte[] array, long[] addresses);

	private static Random random = new Random();
	private static boolean quiet = true;
	private static String currentTest;
	private static long sleepTime = 1000;
	private static byte[][] garbage;

	private abstract static class AcquireThread extends Thread
	{
		protected Object lock;
		protected int[] initCount;
		protected int[] completeCount;
		protected int id = -1;
		
		public AcquireThread()
		{
		}
		
		public void initialize(Object lock, int[] initCount, int[] completeCount)
		{
			this.lock = lock;
			this.initCount = initCount;
			this.completeCount = completeCount;
		}
		
		public void run()
		{
			synchronized(lock)
			{
				id = initCount[0]++;
				lock.notify();
			}

			doAcquire();
			
			synchronized (lock) {
				++completeCount[0];
			}
		}
		
		public abstract void doAcquire();
	}
	
	private static class AcquireAndSleepThread extends AcquireThread
	{	
		public void doAcquire()
		{
			acquireAndSleep(new byte[1], sleepTime);
			// Handy print, but can cause deadlocks since sysout uses synchronized methods
//			if(!quiet) {
//				System.out.print("<" + id + ">");
//			}
		}
	}
	
	private static class AcquireAndCallInThread extends AcquireThread
	{
		private boolean didCallIn = false;
		
		public void doAcquire()
		{
			try {
				didCallIn = false;
				acquireAndCallIn(new byte[1], this);
			} finally {
				if(!didCallIn) {
					throw new RuntimeException(currentTest + ": call-in didn't complete on thread " + id);
				}
			}
		}
		
		public void reportCallIn()
		{
			didCallIn = true;
			// Handy print, but can cause deadlocks since sysout uses synchronized methods
//			if(!quiet) {
//				System.out.print("<" + id + ">");
//			}
		}
	}
	
	private static class AcquireAndGCThread extends AcquireAndCallInThread
	{
		public void reportCallIn()
		{
			super.reportCallIn();
			System.gc();
		}
	}
	
	private static void discardAndGC()
	{
		// Double-GC to try for an aggressive collect
		garbage = null;
		System.gc();
		System.gc();
	}
	
	private static void reportJNIError()
	{
		throw new RuntimeException(currentTest + ": unexpected JNI error");
	}
	
	private static void reportError(String string)
	{
		throw new RuntimeException(currentTest + ": " + string);
	}
	
	private static void reportSuccess()
	{
		if(!quiet) {
			System.out.println(currentTest + ": passed");
		}
	}

	public static void main(String[] args) {
		if(args.length > 0) {
			if(args[0].equals("-v")) {
				quiet = false;
			} else if(args[0].equals("-q")) {
				quiet = true;
			}
		}
		try {
			System.loadLibrary("j9ben");
			
			runByteTests(16);
			runByteTests(127*1024);
			runByteTests(1024*1024);
			
			runIntTests(16);
			runIntTests(127*1024);
			runIntTests(1024*1024);

			runDoubleTests(16);
			runDoubleTests(127*1024);
			runDoubleTests(1024*1024);
			
			runStringTests(16);
			runStringTests(127*1024);
			runStringTests(1024*1024);

			runBlockingTests();
			
			if(holdsVMAccess(new byte[1])) {
				if(!quiet) {
					System.out.println("Skipping non-blocking tests -- VM access held over critical region");
				}
			} else {
				runNonBlockingTests();
			}	
			
			testObjectMovement();
			
		} catch (UnsatisfiedLinkError e) {
			System.out.println("Problem opening JNI library");
			e.printStackTrace();
			throw new RuntimeException();
		}
	}
	
	private static void setTestName(String baseTestName)
	{
		currentTest = baseTestName;
	}

	private static void setTestName(String baseTestName, int dataSize)
	{
		String suffix = "B";
		int value;
		
		value = dataSize;
		if(value % 1024 == 0) {
			value = value / 1024;
			suffix = "K";
		}
		if(value % 1024 == 0) {
			value = value / 1024;
			suffix = "M";
		}
		currentTest = baseTestName + "_" + value + suffix;
	}
	
	private static void runByteTests(int dataSize)
	{
		testGetRelease_byte(dataSize);
		testModify_byte(dataSize);
		testModifyAbort_byte(dataSize);
		testMemcpy_byte(dataSize);
	}
	
	private static byte[] getTestArray_byte(int dataSize)
	{
		byte[] result = new byte[dataSize / 1];
		for(int i = 0; i < result.length; i++) {
			result[i] = (byte)i;
		}
		return result;
	}
	
	private static void testGetRelease_byte(int dataSize)
	{
		byte[] testArray = getTestArray_byte(dataSize);
		byte[] originalArray = (byte[])testArray.clone();
		setTestName("testGetRelease_byte", dataSize);
		
		testGetRelease(testArray);
		for(int i = 0; i < originalArray.length; i++) {
			if(testArray[i] != originalArray[i]) {
				reportError("data modified at " + i + " from " + originalArray[i] + " to " + testArray[i]);
			}
		}
		reportSuccess();
	}
	
	private static void testModify_byte(int dataSize)
	{
		byte[] testArray = getTestArray_byte(dataSize);
		byte[] originalArray = (byte[])testArray.clone();
		setTestName("testModify_byte", dataSize);

		testModify(testArray, true);
		for(int i = 0; i < originalArray.length; i++) {
			if(testArray[i] == originalArray[i]) {
				reportError("data not modified at " + i);
			}
			if(testArray[i] != (byte)(originalArray[i] + 1)) {
				reportError("data incorrectly modified at " + i + " expected " + (byte)(originalArray[i] + 1) + " was " + testArray[i]);
			}
		}
		reportSuccess();
	}
	
	private static void testModifyAbort_byte(int dataSize)
	{
		byte[] testArray = getTestArray_byte(dataSize);
		byte[] originalArray = (byte[])testArray.clone();
		boolean isCopy;
		setTestName("testModify_byte", dataSize);

		isCopy = testModify(testArray, false);
		if(isCopy) {
			for(int i = 0; i < originalArray.length; i++) {
				if(testArray[i] != originalArray[i]) {
					reportError("data modified at " + i + " from " + originalArray[i] + " to " + testArray[i]);
				}
			}		
		} else {
			for(int i = 0; i < originalArray.length; i++) {
				if(testArray[i] == originalArray[i]) {
					reportError("data not modified at " + i);
				}
				if(testArray[i] != (byte)(originalArray[i] + 1)) {
					reportError("data incorrectly modified at " + i + " expected " + (byte)(originalArray[i] + 1) + " was " + testArray[i]);
				}
			}
		}
		reportSuccess();
	}
	
	private static void testMemcpy_byte(int dataSize)
	{
		byte[] srcArray = getTestArray_byte(dataSize);
		byte[] destArray = new byte[srcArray.length];
		setTestName("testMemcpy_byte", dataSize);

		for(int i = 0; i < destArray.length; i++) {
			destArray[i] = -1;
		}
		testMemcpy(srcArray, destArray);
		for(int i = 0; i < srcArray.length; i++) {
			if(srcArray[i] != destArray[i]) {
				reportError("data not modified at " + i);
			}
		}
		reportSuccess();
	}

	

	private static void runIntTests(int dataSize)
	{
		testGetRelease_int(dataSize);
		testModify_int(dataSize);
		testModifyAbort_int(dataSize);
		testMemcpy_int(dataSize);
	}
	
	private static int[] getTestArray_int(int dataSize)
	{
		int[] result = new int[dataSize / 4];
		for(int i = 0; i < result.length; i++) {
			result[i] = (int)i;
		}
		return result;
	}
	
	private static void testGetRelease_int(int dataSize)
	{
		int[] testArray = getTestArray_int(dataSize);
		int[] originalArray = (int[])testArray.clone();
		setTestName("testGetRelease_int", dataSize);
		
		testGetRelease(testArray);
		for(int i = 0; i < originalArray.length; i++) {
			if(testArray[i] != originalArray[i]) {
				reportError("data modified at " + i + " from " + originalArray[i] + " to " + testArray[i]);
			}
		}
		reportSuccess();
	}
	
	private static void testModify_int(int dataSize)
	{
		int[] testArray = getTestArray_int(dataSize);
		int[] originalArray = (int[])testArray.clone();
		setTestName("testModify_int", dataSize);

		testModify(testArray, true);
		for(int i = 0; i < originalArray.length; i++) {
			if(testArray[i] == originalArray[i]) {
				reportError("data not modified at " + i);
			}
			if(testArray[i] != (int)(originalArray[i] + 1)) {
				reportError("data incorrectly modified at " + i + " expected " + (int)(originalArray[i] + 1) + " was " + testArray[i]);
			}
		}
		reportSuccess();
	}
	
	private static void testModifyAbort_int(int dataSize)
	{
		int[] testArray = getTestArray_int(dataSize);
		int[] originalArray = (int[])testArray.clone();
		boolean isCopy;
		setTestName("testModify_int", dataSize);

		isCopy = testModify(testArray, false);
		if(isCopy) {
			for(int i = 0; i < originalArray.length; i++) {
				if(testArray[i] != originalArray[i]) {
					reportError("data modified at " + i + " from " + originalArray[i] + " to " + testArray[i]);
				}
			}		
		} else {
			for(int i = 0; i < originalArray.length; i++) {
				if(testArray[i] == originalArray[i]) {
					reportError("data not modified at " + i);
				}
				if(testArray[i] != (int)(originalArray[i] + 1)) {
					reportError("data incorrectly modified at " + i + " expected " + (int)(originalArray[i] + 1) + " was " + testArray[i]);
				}
			}
		}
		reportSuccess();
	}
	
	private static void testMemcpy_int(int dataSize)
	{
		int[] srcArray = getTestArray_int(dataSize);
		int[] destArray = new int[srcArray.length];
		setTestName("testMemcpy_int", dataSize);

		for(int i = 0; i < destArray.length; i++) {
			destArray[i] = -1;
		}
		testMemcpy(srcArray, destArray);
		for(int i = 0; i < srcArray.length; i++) {
			if(srcArray[i] != destArray[i]) {
				reportError("data not modified at " + i);
			}
		}
		reportSuccess();
	}

	private static void runDoubleTests(int dataSize)
	{
		testGetRelease_double(dataSize);
		testModify_double(dataSize);
		testModifyAbort_double(dataSize);
		testMemcpy_double(dataSize);
	}
	
	private static double[] getTestArray_double(int dataSize)
	{
		double[] result = new double[dataSize / 8];
		for(int i = 0; i < result.length; i++) {
			result[i] = (double)i;
		}
		return result;
	}
	
	private static void testGetRelease_double(int dataSize)
	{
		double[] testArray = getTestArray_double(dataSize);
		double[] originalArray = (double[])testArray.clone();
		setTestName("testGetRelease_double", dataSize);
		
		testGetRelease(testArray);
		for(int i = 0; i < originalArray.length; i++) {
			if(testArray[i] != originalArray[i]) {
				reportError("data modified at " + i + " from " + originalArray[i] + " to " + testArray[i]);
			}
		}
		reportSuccess();
	}
	
	private static void testModify_double(int dataSize)
	{
		double[] testArray = getTestArray_double(dataSize);
		double[] originalArray = (double[])testArray.clone();
		setTestName("testModify_double", dataSize);

		testModify(testArray, true);
		for(int i = 0; i < originalArray.length; i++) {
			if(testArray[i] == originalArray[i]) {
				reportError("data not modified at " + i);
			}
			if(testArray[i] != (double)(originalArray[i] + 1)) {
				reportError("data incorrectly modified at " + i + " expected " + (double)(originalArray[i] + 1) + " was " + testArray[i]);
			}
		}
		reportSuccess();
	}
	
	private static void testModifyAbort_double(int dataSize)
	{
		double[] testArray = getTestArray_double(dataSize);
		double[] originalArray = (double[])testArray.clone();
		boolean isCopy;
		setTestName("testModify_double", dataSize);

		isCopy = testModify(testArray, false);
		if(isCopy) {
			for(int i = 0; i < originalArray.length; i++) {
				if(testArray[i] != originalArray[i]) {
					reportError("data modified at " + i + " from " + originalArray[i] + " to " + testArray[i]);
				}
			}		
		} else {
			for(int i = 0; i < originalArray.length; i++) {
				if(testArray[i] == originalArray[i]) {
					reportError("data not modified at " + i);
				}
				if(testArray[i] != (double)(originalArray[i] + 1)) {
					reportError("data incorrectly modified at " + i + " expected " + (double)(originalArray[i] + 1) + " was " + testArray[i]);
				}
			}
		}
		reportSuccess();
	}
	
	private static void testMemcpy_double(int dataSize)
	{
		double[] srcArray = getTestArray_double(dataSize);
		double[] destArray = new double[srcArray.length];
		setTestName("testMemcpy_double", dataSize);

		for(int i = 0; i < destArray.length; i++) {
			destArray[i] = -1;
		}
		testMemcpy(srcArray, destArray);
		for(int i = 0; i < srcArray.length; i++) {
			if(srcArray[i] != destArray[i]) {
				reportError("data not modified at " + i);
			}
		}
		reportSuccess();
	}
	

	private static void runStringTests(int dataSize)
	{
		testGetRelease_String(dataSize);
		testModify_String(dataSize);
		testMemcpy_String(dataSize);
		testGetRelease_SubString(dataSize, true);
		testModify_SubString(dataSize, true);
		testMemcpy_SubString(dataSize, true);
		testGetRelease_SubString(dataSize, false);
		testModify_SubString(dataSize, false);
		testMemcpy_SubString(dataSize, false);
	}
	
	private static char[] getTestStringChars(int dataSize)
	{
		char[] chars = new char[dataSize / 2];
		for(int i = 0; i < chars.length; i++) {
			chars[i] = (char)('A' + (i % 26));
		}
		return chars;
	}

	private static void testGetRelease_String(int dataSize)
	{
		char[] originalChars = getTestStringChars(dataSize);
		char[] testChars = new char[originalChars.length];
		String testString = new String(originalChars);
		setTestName("testGetRelease_String", dataSize);
		
		testGetRelease(testString);
		testString.getChars(0, testChars.length, testChars, 0);
		for(int i = 0; i < testChars.length; i++) {
			if(testChars[i] != originalChars[i]) {
				reportError("data modified at " + i + " from " + originalChars[i] + " to " + testChars[i]);
			}
		}
		reportSuccess();
	}
	
	private static void testModify_String(int dataSize)
	{
		char[] originalChars = getTestStringChars(dataSize);
		char[] testChars = new char[originalChars.length];
		String testString = new String(originalChars);
		boolean isCopy;
		setTestName("testModify_String", dataSize);

		isCopy = testModify(testString);
		testString.getChars(0, testChars.length, testChars, 0);
		if(isCopy) {
			for(int i = 0; i < testChars.length; i++) {
				if(testChars[i] != originalChars[i]) {
					reportError("data modified at " + i + " from " + originalChars[i] + " to " + testChars[i]);
				}
			}
		} else {
			for(int i = 0; i < testChars.length; i++) {
				if(testChars[i] == originalChars[i]) {
					reportError("data not modified at " + i);
				}
				if(testChars[i] != Character.toLowerCase(originalChars[i])) {
					reportError("data incorrectly modified at " + i + " expected " + Character.toLowerCase(originalChars[i]) + " was " + testChars[i]);
				}
			}
		}
		reportSuccess();
	}
		
	private static void testMemcpy_String(int dataSize)
	{
		char[] srcChars = getTestStringChars(dataSize);
		char[] destChars = new char[srcChars.length];
		char[] copiedChars = new char[srcChars.length];
		String srcString = new String(srcChars);
		String destString;
		boolean isCopy;
		setTestName("testMemcpy_String", dataSize);

		for(int i = 0; i < destChars.length; i++) {
			destChars[i] = '!';
		}
		destString = new String(destChars);
		
		isCopy = testMemcpy(srcString, destString);
		destString.getChars(0, copiedChars.length, copiedChars, 0);	
		if(isCopy) {
			for(int i = 0; i < copiedChars.length; i++) {
				if(copiedChars[i] != destChars[i]) {
					reportError("data modified at " + i + " from " + destChars[i] + " to " + copiedChars[i]);
				}
			}
		} else {
			for(int i = 0; i < copiedChars.length; i++) {
				if(copiedChars[i] != srcChars[i]) {
					reportError("data not modified at " + i);
				}
			}
		}
		reportSuccess();
	}

	private static void testGetRelease_SubString(int dataSize, boolean shortString)
	{
		char[] originalChars = getTestStringChars(dataSize * 2);
		String containerString = new String(originalChars);
		int offset = dataSize / 6 + 1;
		int length = dataSize / 2;
		if(shortString) {
			length = Math.min(6, length);
		}
		String testString = containerString.substring(offset, offset+length);
		char[] testChars = new char[length];
		if(shortString) {
			setTestName("testGetRelease_SubStringShort", dataSize);
		} else {
			setTestName("testGetRelease_SubString", dataSize);
		}
		
		testGetRelease(testString);
		testString.getChars(0, testChars.length, testChars, 0);
		for(int i = 0; i < testChars.length; i++) {
			if(testChars[i] != originalChars[i+offset]) {
				reportError("data modified at " + i + " from " + originalChars[i+offset] + " to " + testChars[i]);
			}
		}
		reportSuccess();
	}
	
	private static void testModify_SubString(int dataSize, boolean shortString)
	{
		char[] originalChars = getTestStringChars(dataSize * 2);
		String containerString = new String(originalChars);
		int offset = dataSize / 6 + 1;
		int length = dataSize / 2;
		if(shortString) {
			length = Math.min(6, length);
		}
		String testString = containerString.substring(offset, offset+length);
		char[] testChars = new char[length];
		boolean isCopy;
		if(shortString) {
			setTestName("testModify_SubStringShort", dataSize);
		} else {
			setTestName("testModify_SubString", dataSize);
		}

		isCopy = testModify(testString);
		testString.getChars(0, testChars.length, testChars, 0);
		if(isCopy) {
			for(int i = 0; i < testChars.length; i++) {
				if(testChars[i] != originalChars[i+offset]) {
					reportError("data modified at " + i + " from " + originalChars[i+offset] + " to " + testChars[i]);
				}
			}
		} else {
			for(int i = 0; i < length; i++) {
				if(testChars[i] == originalChars[i+offset]) {
					reportError("data not modified at " + i);
				}
				if(testChars[i] != Character.toLowerCase(originalChars[i+offset])) {
					reportError("data incorrectly modified at " + i + " expected " + Character.toLowerCase(originalChars[i+offset]) + " was " + testChars[i]);
				}
			}
		}
		reportSuccess();
	}
		
	private static void testMemcpy_SubString(int dataSize, boolean shortString)
	{
		char[] originalChars = getTestStringChars(dataSize * 2);
		String containerString = new String(originalChars);
		int offset = dataSize / 6 + 1;
		int length = dataSize / 2;
		if(shortString) {
			length = Math.min(6, length);
		}
		String srcString = containerString.substring(offset, offset+length);
		char[] destChars = new char[length];
		char[] copiedChars = new char[length];
		String destString;
		boolean isCopy;
		if(shortString) {
			setTestName("testMemcpy_SubStringShort", dataSize);
		} else {
			setTestName("testMemcpy_SubString", dataSize);
		}

		for(int i = 0; i < destChars.length; i++) {
			destChars[i] = '!';
		}
		destString = new String(destChars);
		
		isCopy = testMemcpy(srcString, destString);
		destString.getChars(0, copiedChars.length, copiedChars, 0);	
		if(isCopy) {
			for(int i = 0; i < copiedChars.length; i++) {
				if(copiedChars[i] != destChars[i]) {
					reportError("data modified at " + i + " from " + destChars[i] + " to " + copiedChars[i]);
				}
			}
		} else {
			for(int i = 0; i < copiedChars.length; i++) {
				if(copiedChars[i] != originalChars[i+offset]) {
					reportError("data not modified at " + i);
				}
			}
		}
		reportSuccess();
	}

	private static void runBlockingTests()
	{
		testAcquireAndSleep(1, false);
		testAcquireAndSleep(1, true);
		testAcquireAndSleep(8, false);
		testAcquireAndSleep(8, true);
	}
	
	private static void runNonBlockingTests()
	{
		testAcquireAndCallIn(1);
		testAcquireAndCallIn(8);
		testAcquireAndGC(1,1);
		testAcquireAndGC(8,1);
		testAcquireAndGC(1,8);
		testAcquireAndGC(8,8);
	}
	
	private static void testAcquireAndSleep(int threadCount, boolean tryGC)
	{
		Object lock = new Object();
		AcquireThread[] threads = new AcquireAndSleepThread[threadCount];
		int[] initCount = new int[threadCount];
		int[] completeCount = new int[threadCount];
		String testName = "testAcquireAndSleep_Threads" + threadCount;
		if(tryGC) {
			testName += "_WithGC";
		}
		setTestName(testName);
		
		synchronized (lock) {
			// Start all threads
			for(int i = 0; i < threadCount; i++) {
				threads[i] = new AcquireAndSleepThread();
				threads[i].initialize(lock, initCount, completeCount);
				threads[i].start();
			}
			
			// Wait for all threads to initialize
			while(initCount[0] < threadCount) {
				try {
					lock.wait();
				} catch (InterruptedException e) {}
			}
		
			// Wait for a while; not expecting a notify
			try {
				lock.wait(sleepTime / 2);
				if(tryGC) {
					System.gc();
				}
				lock.wait(sleepTime * 2);
			} catch (InterruptedException e) {}
			
			// Did all threads check in?
			if(completeCount[0] < threadCount) {
				reportError("not all threads completed before the timeout (" + completeCount[0] + "/" + threadCount + ")");
			}
		}
		
		reportSuccess();
	}
	
	private static void testAcquireAndCallIn(int threadCount)
	{
		Object lock = new Object();
		AcquireThread[] threads = new AcquireAndCallInThread[threadCount];
		int[] initCount = new int[threadCount];
		int[] completeCount = new int[threadCount];
		String testName = "testAcquireAndCallIn_Threads" + threadCount;
		setTestName(testName);
		
		synchronized (lock) {
			// Start all threads
			for(int i = 0; i < threadCount; i++) {
				threads[i] = new AcquireAndCallInThread();
				threads[i].initialize(lock, initCount, completeCount);
				threads[i].start();
			}
			
			// Wait for all threads to initialize
			while(initCount[0] < threadCount) {
				try {
					lock.wait();
				} catch (InterruptedException e) {}
			}
		}
		
		// Wait for all threads to die, or time out trying
		try {
			//lock.wait(sleepTime * 2);
			for(int i = 0; i < threadCount; i++) {
				threads[i].join(sleepTime * 2);
			}
		} catch (InterruptedException e) {}
			
		synchronized (lock) {
			// Did all threads check in?
			if(completeCount[0] < threadCount) {
				reportError("not all threads completed before the timeout (" + completeCount[0] + "/" + threadCount + ")");
			}
		}
		
		reportSuccess();
	}
	
	private static void testAcquireAndGC(int sleeperCount, int gcCount)
	{
		Object lock = new Object();
		int threadCount = sleeperCount + gcCount;
		AcquireThread[] threads = new AcquireThread[threadCount];
		int[] initCount = new int[threadCount];
		int[] completeCount = new int[threadCount];
		String testName = "testAcquireAndGC_Sleepers" + sleeperCount + "_GCers" + gcCount;
		setTestName(testName);
		synchronized (lock) {
			// Start all threads
			for(int i = 0; i < sleeperCount; i++) {
				threads[i] = new AcquireAndSleepThread();
				threads[i].initialize(lock, initCount, completeCount);
				threads[i].start();
			}
			for(int i = sleeperCount; i < threadCount; i++) {
				threads[i] = new AcquireAndGCThread();
				threads[i].initialize(lock, initCount, completeCount);
				threads[i].start();
			}


			// Wait for all threads to initialize
			while(initCount[0] < threadCount) {
				try {
					lock.wait();
				} catch (InterruptedException e) {}
			}
		}

		// Wait for all threads to die, or time out trying
		try {
			//lock.wait(sleepTime * 2);
			for(int i = 0; i < threadCount; i++) {
				/* CMVC 195423 : Increase timeout to 5 seconds to allow all child threads to complete */
				threads[i].join(sleepTime * 5);
			}
		} catch (InterruptedException e) {}
			
		synchronized (lock) {
			// Did all threads check in?
			if(completeCount[0] < threadCount) {
				reportError("not all threads completed before the timeout (" + completeCount[0] + "/" + threadCount + ")");
			}
		}
		
		reportSuccess();
	}

	private static void testObjectMovement()
	{
		String testName = "testObjectMovement";
		setTestName(testName);
		
		// Get an address for it
		if(!returnsDirectPointer(new byte[1])) {
			// Copy...this test can't work
			if(!quiet) {
				System.out.println("Skipping object movement test -- JNI is copying");
			}
		} else {
			// First, create a bunch of objects
			garbage = new byte[128][1024];
			byte[] keeper = garbage[(int)(garbage.length * random.nextDouble())];
			long[] addresses = { 0, 0 };
		
			if(!acquireDiscardAndGC(keeper, addresses)) {
				reportError("object moved during critical region from " + Long.toHexString(addresses[0]) + " to " + Long.toHexString(addresses[1]));
			}
		}
		
		reportSuccess();
	}

}

