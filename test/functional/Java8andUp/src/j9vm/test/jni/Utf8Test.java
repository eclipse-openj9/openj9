/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.io.UnsupportedEncodingException;
import java.util.Random;

@Test(groups = { "level.extended" })
public class Utf8Test {

	private static final Logger logger = Logger.getLogger(Utf8Test.class);

	private static final String THREAD_NAME = "CMVC170872";
	String TEST_LIBRARY = "j9ben";

	native int testAttachCurrentThreadAsDaemon(byte[] utf8String);

	Thread currentThread;

	@BeforeMethod
	protected void setUp() {
		try {
			System.loadLibrary(TEST_LIBRARY);
		} catch (UnsatisfiedLinkError e) {
			Assert.fail(e.getMessage() + "\nlibrary path = " + System.getProperty("java.library.path"));
		}
		currentThread = Thread.currentThread();
	}


	public void testJniUtf8Sanity() {
		String testName = printMethodHeader("test with well-formed ASCII");
		byte[] nameBytes = getUTF8Bytes(THREAD_NAME);
		int result = runTest(nameBytes);
		checkResult(testName, result);
	}


	public void testJniUtf8EmptyString() {
		String testName = printMethodHeader("test with zero-length string");
		byte[] nameBytes = getUTF8Bytes("");
		int result = runTest(nameBytes);
		checkResult(testName, result);
	}


	public void testJniUtf8OneCharString() {
		String testName = printMethodHeader("test with one byte string");
		byte[] nameBytes = getUTF8Bytes("x");
		int result = runTest(nameBytes);
		checkResult(testName, result);
	}


	public void testJniUtf8TwoByteChar() {
		String testName = printMethodHeader("test with two-byte UTF8");
		byte[] nameBytes = getUTF8Bytes("\u03ff");
		int result = runTest(nameBytes);
		checkResult(testName, result);
	}


	public void testJniUtf8ThreeByteChar() {
		String testName = printMethodHeader("test with three-byte UTF8");
		byte[] nameBytes = getUTF8Bytes("\u3fff");
		int result = runTest(nameBytes);

		checkResult(testName, result);
	}


	public void testBadUtf8() {
		int NAME_LEN = 40;
		String testName = printMethodHeader("test with random bad UTF8");
		Random rgen = new Random(314159265);
		byte[] nameBytes = new byte[NAME_LEN];
		rgen.nextBytes(nameBytes);
		nameBytes[NAME_LEN - 1] = 0;
		dumpStringBytes(nameBytes);
		int result = testAttachCurrentThreadAsDaemon(nameBytes);
		dumpThreadNames();
		checkResult(testName, result);
	}


	public void testBadUtf8AtStart() {
		String testName = printMethodHeader("test with bad UTF8 at the start");
		byte[] nameBytes = getUTF8Bytes("abc");
		nameBytes[0] = (byte) 0xc1;
		dumpStringBytes(nameBytes);
		int result = testAttachCurrentThreadAsDaemon(nameBytes);
		dumpThreadNames();
		checkResult(testName, result);
	}


	public void testBadUtf8InMiddle() {
		String testName = printMethodHeader("test with bad UTF8 in the middle");
		byte[] nameBytes = getUTF8Bytes("abc");
		nameBytes[1] = (byte) 0xc1;
		dumpStringBytes(nameBytes);
		int result = testAttachCurrentThreadAsDaemon(nameBytes);
		dumpThreadNames();
		checkResult(testName, result);
	}


	public void testBadUtf8AtEnd() {
		String testName = printMethodHeader("test with bad UTF8 at the end");
		byte[] nameBytes = getUTF8Bytes("abc");
		nameBytes[2] = (byte) 0xc1;
		dumpStringBytes(nameBytes);
		int result = testAttachCurrentThreadAsDaemon(nameBytes);
		dumpThreadNames();
		checkResult(testName, result);
	}

	private void checkResult(String testName, int result) {
		AssertJUnit.assertEquals("result from " + testName, 0, result);
	}

	private byte[] getUTF8Bytes(String nameString) {
		try {
			return nameString.getBytes("UTF8");
		} catch (UnsupportedEncodingException e) {
			Assert.fail(e.getMessage());
			return null; /* this won't happen */
		}
	}

	private static String printMethodHeader(String msg) {
		StackTraceElement[] myStack = Thread.currentThread().getStackTrace();
		logger.debug("\n----------------------------------------------------------------------------------------\n");
		String methodName = myStack[3].getMethodName();
		logger.debug("Starting " + methodName);
		logger.debug(msg);
		return methodName;
	}

	private int runTest(byte[] nameBytes) {
		dumpStringBytes(nameBytes);
		int result = testAttachCurrentThreadAsDaemon(nameBytes);
		dumpThreadNames();
		return result;
	}

	private void dumpThreadNames() {
		ThreadGroup myGroup = currentThread.getThreadGroup();
		Thread[] peerThreads = new Thread[myGroup.activeCount()];
		myGroup.enumerate(peerThreads);
		for (Thread t : peerThreads) {
			logger.debug("thread: " + t.getId() + " " + t.getName());
		}
	}

	private void dumpStringBytes(byte[] nameBytes) {
		StringBuilder sb = new StringBuilder();
		for (byte b : nameBytes) {
			String hexString = Integer.toHexString(b & 255);
			sb.append("0x" + hexString + " ");
		}
		logger.debug(sb.toString());
	}

}
