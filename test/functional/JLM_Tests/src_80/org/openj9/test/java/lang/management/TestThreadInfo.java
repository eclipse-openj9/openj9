
/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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

package org.openj9.test.java.lang.management;

import org.testng.annotations.AfterClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeClass;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.management.LockInfo;
import java.lang.management.ManagementFactory;
import java.lang.management.MonitorInfo;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;

import javax.management.openmbean.ArrayType;
import javax.management.openmbean.CompositeData;
import javax.management.openmbean.CompositeDataSupport;
import javax.management.openmbean.CompositeType;
import javax.management.openmbean.OpenDataException;
import javax.management.openmbean.OpenType;
import javax.management.openmbean.SimpleType;

/**
 * Unit tests for java.lang.management.ThreadInfo
 */
@Test(groups = { "level.extended" })
public class TestThreadInfo {

	private static Logger logger = Logger.getLogger(TestThreadInfo.class);

	private static final boolean GOOD_SUSPENDED = false;

	private static final boolean GOOD_IN_NATIVE = false;

	private static final int GOOD_STACK_SIZE = 3;

	private static final boolean GOOD_STACK_NATIVEMETHOD = false;

	private static final int GOOD_STACK_LINENUMBER = 2100;

	private static final String GOOD_STACK_FILENAME = "Blobby.java";

	private static final String GOOD_STACK_METHODNAME = "takeOverWorld";

	private static final String GOOD_STACK_CLASSNAME = "foo.bar.Blobby";

	private static final String GOOD_THREAD_STATE = "RUNNABLE";

	private static final String GOOD_THREAD_NAME = "Marty";

	private static final int GOOD_THREAD_ID = 46664;

	private static final String GOOD_LOCK_OWNER_NAME = "Noam Chomsky";

	private static final int GOOD_LOCK_OWNER_ID = 24141;

	private static final String GOOD_LOCK_HASH = "1234567";

	private static final String GOOD_LOCK_CLASSNAME = "foo.Bar";

	private static final String GOOD_LOCK_NAME = GOOD_LOCK_CLASSNAME + "@" + GOOD_LOCK_HASH;

	private static final int GOOD_WAITED_TIME = 3779;

	private static final int GOOD_WAITED_COUNT = 21;

	private static final int GOOD_BLOCKED_TIME = 3309;

	private static final int GOOD_BLOCKED_COUNT = 250;

	protected static String[] typeNamesGlobal = null;

	protected static String[] badTypeNames = null;

	protected static String[] typeDescsGlobal = null;

	protected static Object[] valuesCompositeDataGlobal = null;

	protected static OpenType[] typeTypesGlobal = null;

	protected static Object[] valuesGoodCompositeDataNoLockInfo = null;

	protected static CompositeData[] monitors = null;

	protected static CompositeData[] synchronizers = null;

	CompositeData goodCD = null;

	ThreadInfo goodThreadInfo = null;

	/**
	 * Initialize global data structures
	 */
	protected static void initGlobalDataStructures() {
		monitors = new CompositeData[3];
		for (int i = 0; i < monitors.length; i++) {
			monitors[i] = TestMonitorInfo.createGoodCompositeData();
		}

		synchronizers = new CompositeData[3];
		for (int i = 0; i < synchronizers.length; i++) {
			synchronizers[i] = TestLockInfo.createGoodCompositeData();
		}

		String[] typeNamesGlobalTmp = { "threadId", "threadName",
				"threadState", "suspended", "inNative", "blockedCount",
				"blockedTime", "waitedCount", "waitedTime", "lockInfo",
				"lockName", "lockOwnerId", "lockOwnerName", "stackTrace",
				"lockedMonitors", "lockedSynchronizers" };
		typeNamesGlobal = typeNamesGlobalTmp;

		String[] badTypeNamesTmp = { "BADthreadId", "threadName",
				"threadState", "suspended", "inNative", "blockedCount",
				"blockedTime", "waitedCount", "waitedTime", "lockInfo", "lockName",
				"lockOwnerId", "lockOwnerName", "stackTrace", "lockedMonitors",
				"lockedSynchronizers" };
		badTypeNames = badTypeNamesTmp;

		String[] typeDescsGlobalTmp = { "blah threadId",
				"blah threadName", "blah threadState", "blah suspended",
				"blah inNative", "blah blockedCount", "blah blockedTime",
				"blah waitedCount", "blah waitedTime", "blah lockInfo",
				"blah lockName", "blah lockOwnerId", "blah lockOwnerName",
				"blah stackTrace", "blah lockedMonitors",
				"blah lockedSynchronizers" };
		typeDescsGlobal = typeDescsGlobalTmp;

		Object[] valuesCompositeDataGlobalTmp = {
				/* threadId */new Long(GOOD_THREAD_ID),
				/* threadName */new String(GOOD_THREAD_NAME),
				/* threadState */new String(GOOD_THREAD_STATE),
				/* suspended */new Boolean(GOOD_SUSPENDED),
				/* inNative */new Boolean(GOOD_IN_NATIVE),
				/* blockedCount */new Long(GOOD_BLOCKED_COUNT),
				/* blockedTime */new Long(GOOD_BLOCKED_TIME),
				/* waitedCount */new Long(GOOD_WAITED_COUNT),
				/* waitedTime */new Long(GOOD_WAITED_TIME),
				/* lockInfo */TestLockInfo.createGoodCompositeData(),
				/* lockName */new String(GOOD_LOCK_NAME),
				/* lockOwnerId */new Long(GOOD_LOCK_OWNER_ID),
				/* lockOwnerName */new String(GOOD_LOCK_OWNER_NAME),
				/* stackTrace */createGoodStackTraceCompositeData(),
				/* lockedMonitors */monitors,
				/* lockedSynchronizers */synchronizers };
		valuesCompositeDataGlobal = valuesCompositeDataGlobalTmp;

		Object[] valuesGoodCompositeDataNoLockInfoTmp = {
				/* threadId */new Long(GOOD_THREAD_ID),
				/* threadName */new String(GOOD_THREAD_NAME),
				/* threadState */new String(GOOD_THREAD_STATE),
				/* suspended */new Boolean(GOOD_SUSPENDED),
				/* inNative */new Boolean(GOOD_IN_NATIVE),
				/* blockedCount */new Long(GOOD_BLOCKED_COUNT),
				/* blockedTime */new Long(GOOD_BLOCKED_TIME),
				/* waitedCount */new Long(GOOD_WAITED_COUNT),
				/* waitedTime */new Long(GOOD_WAITED_TIME),
				/* lockInfo */createDummyCompositeData(),
				/* lockName */new String(GOOD_LOCK_NAME),
				/* lockOwnerId */new Long(GOOD_LOCK_OWNER_ID),
				/* lockOwnerName */new String(GOOD_LOCK_OWNER_NAME),
				/* stackTrace */createGoodStackTraceCompositeData(),
				/* lockedMonitors */monitors,
				/* lockedSynchronizers */synchronizers };
		valuesGoodCompositeDataNoLockInfo = valuesGoodCompositeDataNoLockInfoTmp;

		try {
			OpenType[] typeTypesGlobalTmp = {
					SimpleType.LONG,
					SimpleType.STRING,
					SimpleType.STRING,
					SimpleType.BOOLEAN,
					SimpleType.BOOLEAN,
					SimpleType.LONG,
					SimpleType.LONG,
					SimpleType.LONG,
					SimpleType.LONG,
					TestLockInfo.createGoodLockInfoCompositeType(),
					SimpleType.STRING,
					SimpleType.LONG,
					SimpleType.STRING,
					new ArrayType(1, createGoodStackTraceElementCompositeType()),
					new ArrayType(1, TestMonitorInfo.createGoodMonitorInfoCompositeType()),
					new ArrayType(1, TestLockInfo.createGoodLockInfoCompositeType()) };
			typeTypesGlobal = typeTypesGlobalTmp;
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
	}

	@BeforeClass
	protected void setUp() throws Exception {
		initGlobalDataStructures();
		goodCD = createGoodCompositeData();
		goodThreadInfo = ThreadInfo.from(goodCD);
		AssertJUnit.assertNotNull(goodThreadInfo);
		logger.info("Starting TestThreadInfo tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getBlockedCount()'
	 */
	@Test
	public final void testGetBlockedCount() {
		AssertJUnit.assertEquals(GOOD_BLOCKED_COUNT, goodThreadInfo.getBlockedCount());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getBlockedTime()'
	 */
	@Test
	public final void testGetBlockedTime() {
		AssertJUnit.assertEquals(GOOD_BLOCKED_TIME, goodThreadInfo.getBlockedTime());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getLockName()'
	 */
	@Test
	public final void testGetLockName() {
		AssertJUnit.assertEquals(GOOD_LOCK_NAME, goodThreadInfo.getLockName());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getLockOwnerId()'
	 */
	@Test
	public final void testGetLockOwnerId() {
		AssertJUnit.assertEquals(GOOD_LOCK_OWNER_ID, goodThreadInfo.getLockOwnerId());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getLockOwnerName()'
	 */
	@Test
	public final void testGetLockOwnerName() {
		AssertJUnit.assertEquals(GOOD_LOCK_OWNER_NAME, goodThreadInfo.getLockOwnerName());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getStackTrace()'
	 */
	@Test
	public final void testGetStackTrace() {
		StackTraceElement[] stack = goodThreadInfo.getStackTrace();
		AssertJUnit.assertEquals(GOOD_STACK_SIZE, stack.length);
		for (StackTraceElement element : stack) {
			AssertJUnit.assertEquals(GOOD_STACK_CLASSNAME, element.getClassName());
			AssertJUnit.assertEquals(GOOD_STACK_NATIVEMETHOD, element.isNativeMethod());
			AssertJUnit.assertEquals(GOOD_STACK_FILENAME, element.getFileName());
			AssertJUnit.assertEquals(GOOD_STACK_LINENUMBER, element.getLineNumber());
			AssertJUnit.assertEquals(GOOD_STACK_METHODNAME, element.getMethodName());
		}
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getThreadId()'
	 */
	@Test
	public final void testGetThreadId() {
		AssertJUnit.assertEquals(GOOD_THREAD_ID, goodThreadInfo.getThreadId());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getThreadName()'
	 */
	@Test
	public final void testGetThreadName() {
		AssertJUnit.assertEquals(GOOD_THREAD_NAME, goodThreadInfo.getThreadName());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getThreadState()'
	 */
	@Test
	public final void testGetThreadState() {
		AssertJUnit.assertEquals(Thread.State.valueOf(GOOD_THREAD_STATE), goodThreadInfo.getThreadState());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getWaitedCount()'
	 */
	@Test
	public final void testGetWaitedCount() {
		AssertJUnit.assertEquals(GOOD_WAITED_COUNT, goodThreadInfo.getWaitedCount());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getWaitedTime()'
	 */
	@Test
	public final void testGetWaitedTime() {
		AssertJUnit.assertEquals(GOOD_WAITED_TIME, goodThreadInfo.getWaitedTime());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.isInNative()'
	 */
	@Test
	public final void testIsInNative() {
		AssertJUnit.assertFalse(goodThreadInfo.isInNative());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.isSuspended()'
	 */
	@Test
	public final void testIsSuspended() {
		AssertJUnit.assertFalse(goodThreadInfo.isSuspended());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.from(CompositeData cd)'
	 */
	@Test
	public final void testFrom() {
		// Ensure that from() balks with bad data
		CompositeData badCD = createBadCompositeData();
		// Below should fail...
		try {
			ThreadInfo badTI = ThreadInfo.from(badCD);
			Assert.fail("from() should have thrown an IllegalArgumentException.");
		} catch (IllegalArgumentException e) {
			logger.debug("IllegalArgumentException occurred, as expected: " + e.getMessage());
		}

		// Ensure from returns null for a null input
		AssertJUnit.assertNull(ThreadInfo.from(null));
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.toString()'
	 */
	@Test
	public void testToString() {
		AssertJUnit.assertTrue(goodThreadInfo.toString().startsWith(getGoodToStringVal()));
	}

	String getGoodToStringVal() {
		StringBuilder result = new StringBuilder();
		// The format in which ThreadInfo.toString() returns a thread's readable representation.
		result.append(GOOD_THREAD_NAME + " " + GOOD_THREAD_ID + " " + GOOD_THREAD_STATE);
		return result.toString();
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getLockInfo()'
	 */
	@Test
	public void testGetLockInfo() {
		LockInfo info = goodThreadInfo.getLockInfo();
		AssertJUnit.assertEquals(TestLockInfo.GOOD_CD_CLASSNAME, info.getClassName());
		AssertJUnit.assertEquals(TestLockInfo.GOOD_CD_IDHASHCODE, info.getIdentityHashCode());
		String lockName = TestLockInfo.GOOD_CD_CLASSNAME + "@" + Integer.toHexString(TestLockInfo.GOOD_CD_IDHASHCODE);
		AssertJUnit.assertEquals(lockName, info.toString());

		// if the CompositeData does not contains any information about
		// LockInfo, use LockName to build LockInfo instead
		goodThreadInfo = ThreadInfo.from(createGoodCompositeDataNoLockInfo());
		AssertJUnit.assertNotNull(goodThreadInfo);
		info = goodThreadInfo.getLockInfo();
		AssertJUnit.assertEquals(GOOD_LOCK_CLASSNAME, info.getClassName());
		AssertJUnit.assertEquals(Integer.parseInt(GOOD_LOCK_HASH), info.getIdentityHashCode());
		AssertJUnit.assertEquals(GOOD_LOCK_CLASSNAME + "@" + Integer.toHexString(Integer.parseInt(GOOD_LOCK_HASH)),
				info.toString());
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getLockedMonitors()'
	 */
	@Test
	public void testGetLockedMonitors() {
		MonitorInfo[] infos = goodThreadInfo.getLockedMonitors();
		AssertJUnit.assertEquals(3, infos.length);
		for (MonitorInfo info : infos) {
			AssertJUnit.assertEquals(TestMonitorInfo.GOOD_CD_CLASSNAME, info.getClassName());
			AssertJUnit.assertEquals(TestMonitorInfo.GOOD_CD_IDHASHCODE, info.getIdentityHashCode());
			AssertJUnit.assertEquals(TestMonitorInfo.GOOD_CD_STACKDEPTH, info.getLockedStackDepth());
			String lockName = TestMonitorInfo.GOOD_CD_CLASSNAME + "@"
					+ Integer.toHexString(TestMonitorInfo.GOOD_CD_IDHASHCODE);
			AssertJUnit.assertEquals(lockName, info.toString());
			StackTraceElement frame = info.getLockedStackFrame();
			AssertJUnit.assertEquals("foo.bar.Blobby", frame.getClassName());
			AssertJUnit.assertEquals(false, frame.isNativeMethod());
			AssertJUnit.assertEquals("Blobby.java", frame.getFileName());
			AssertJUnit.assertEquals(2100, frame.getLineNumber());
			AssertJUnit.assertEquals("takeOverWorld", frame.getMethodName());
		}
	}

	/*
	 * Test method for
	 * 'java.lang.management.ThreadInfo.getLockedSynchronizers()'
	 */
	@Test
	public void testGetLockedSynchronizers() {
		LockInfo[] infos = goodThreadInfo.getLockedSynchronizers();
		AssertJUnit.assertEquals(3, infos.length);
		for (LockInfo info : infos) {
			AssertJUnit.assertEquals(TestLockInfo.GOOD_CD_CLASSNAME, info.getClassName());
			AssertJUnit.assertEquals(TestLockInfo.GOOD_CD_IDHASHCODE, info.getIdentityHashCode());
			String lockName = TestLockInfo.GOOD_CD_CLASSNAME + "@"
					+ Integer.toHexString(TestLockInfo.GOOD_CD_IDHASHCODE);
			AssertJUnit.assertEquals(lockName, info.toString());
		}
	}

	/*
	 * Test method for 'java.lang.management.ThreadInfo.getLockInfo()' in real
	 * thread
	 */
	@Test
	public void testGetLockInfo_RealThread() throws Exception {
		// current thread
		ThreadMXBean mb = ManagementFactory.getThreadMXBean();
		ThreadInfo ti = mb.getThreadInfo(Thread.currentThread().getId());
		AssertJUnit.assertEquals(-1, ti.getLockOwnerId());
		AssertJUnit.assertNull(ti.getLockName());
		AssertJUnit.assertNull(ti.getLockOwnerName());
		AssertJUnit.assertEquals(Thread.State.RUNNABLE, ti.getThreadState());
		logger.debug("Current thread name: " + ti.getThreadName());
		AssertJUnit.assertEquals(false, ti.isSuspended());
		AssertJUnit.assertEquals(0, ti.getStackTrace().length);
		AssertJUnit.assertEquals(0, ti.getLockedMonitors().length);
		AssertJUnit.assertEquals(0, ti.getLockedSynchronizers().length);
		AssertJUnit.assertNull(ti.getLockInfo());

		// a waiting thread
		final Object lock = new Object();
		class Waiting extends Thread {
			public void run() {
				synchronized (lock) {
					try {
						lock.wait();
					} catch (InterruptedException e) {
						//ignore
					}
				}
			}
		}
		Thread thread = new Waiting();
		thread.start();
		Thread.sleep(1000);
		ti = mb.getThreadInfo(thread.getId());
		String className = "java.lang.Object";
		int hashCode = System.identityHashCode(lock);
		String lockName = className + "@" + Integer.toHexString(hashCode);
		AssertJUnit.assertEquals(-1, ti.getLockOwnerId());
		AssertJUnit.assertEquals(lockName, ti.getLockName());
		AssertJUnit.assertNull(ti.getLockOwnerName());
		AssertJUnit.assertEquals(Thread.State.WAITING, ti.getThreadState());
		AssertJUnit.assertEquals(false, ti.isSuspended());
		AssertJUnit.assertEquals(0, ti.getStackTrace().length);
		AssertJUnit.assertEquals(0, ti.getLockedMonitors().length);
		AssertJUnit.assertEquals(0, ti.getLockedSynchronizers().length);

		LockInfo lockInfo = ti.getLockInfo();
		AssertJUnit.assertEquals(className, lockInfo.getClassName());
		AssertJUnit.assertEquals(hashCode, lockInfo.getIdentityHashCode());
		AssertJUnit.assertEquals(lockName, lockInfo.toString());

		synchronized (lock) {
			lock.notifyAll();
		}
	}

	/**
	 * @return a new <code>CompositeData</code> instance representing a
	 *         <code>ThreadInfo</code>.
	 */
	protected static CompositeData createGoodCompositeData() {
		CompositeData result = null;
		CompositeType cType = createGoodThreadInfoCompositeType();
		try {
			result = new CompositeDataSupport(cType, typeNamesGlobal,
					valuesCompositeDataGlobal);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	/**
	 * @return <code>CompositeType</code> for use when wrapping up
	 *         <code>ThreadInfo</code> objects in <code>CompositeData</code>
	 *         s.
	 */
	protected static CompositeType createGoodThreadInfoCompositeType() {
		CompositeType result = null;
		try {
			result = new CompositeType(ThreadInfo.class.getName(),
					ThreadInfo.class.getName(), typeNamesGlobal,
					typeDescsGlobal, typeTypesGlobal);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	/**
	 * @return a new <code>CompositeData</code> instance representing a
	 *         <code>ThreadInfo</code>, does not contain 'LockInfo'.
	 */
	protected static CompositeData createGoodCompositeDataNoLockInfo() {
		CompositeData result = null;
		CompositeType cType = createGoodThreadInfoCompositeTypeNoLockInfo();
		try {
			result = new CompositeDataSupport(cType, typeNamesGlobal,
					valuesGoodCompositeDataNoLockInfo);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	static CompositeData createDummyCompositeData() {
		CompositeData result = null;
		String[] names = { "className", "identityHashCode" };
		Object[] values = {
				/* className */new String(GOOD_LOCK_CLASSNAME),
				/* identityHashCode */new Integer(GOOD_LOCK_HASH) };
		CompositeType cType = TestLockInfo.createGoodLockInfoCompositeType();
		try {
			result = new CompositeDataSupport(cType, names, values);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	/**
	 * @return <code>CompositeType</code> for use when wrapping up
	 *         <code>ThreadInfo</code> objects in <code>CompositeData</code>
	 *         s, does not contain 'LockInfo'.
	 */
	protected static CompositeType createGoodThreadInfoCompositeTypeNoLockInfo() {
		CompositeType result = null;
		try {
			result = new CompositeType(ThreadInfo.class.getName(),
					ThreadInfo.class.getName(), typeNamesGlobal,
					typeDescsGlobal, typeTypesGlobal);
		} catch (OpenDataException e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception occurred.");
		}
		return result;
	}

	/**
	 * Returns a new <code>CompositeData</code> instance which has been
	 * intentionally constructed with attributes that <i>should</i> prevent the
	 * creation of a new <code>ThreadInfo</code>.
	 *
	 * @return a new <code>CompositeData</code> instance representing a
	 *         <code>ThreadInfo</code>.
	 */
	protected static CompositeData createBadCompositeData() {
		CompositeData result = null;
		CompositeType cType = createBadThreadInfoCompositeType();
		try {
			result = new CompositeDataSupport(cType, badTypeNames,
					valuesCompositeDataGlobal);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	/**
	 * Returned value is <i>intentionally</i> bad.
	 *
	 * @return <code>CompositeType</code> for use when wrapping up
	 *         <code>ThreadInfo</code> objects in <code>CompositeData</code>
	 *         s.
	 */
	protected static CompositeType createBadThreadInfoCompositeType() {
		CompositeType result = null;
		try {
			result = new CompositeType(ThreadInfo.class.getName(),
					ThreadInfo.class.getName(), badTypeNames, typeDescsGlobal,
					typeTypesGlobal);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	protected static CompositeType createGoodStackTraceElementCompositeType() {
		CompositeType result = null;
		String[] typeNames = { "className", "methodName", "fileName", "lineNumber", "nativeMethod" };
		String[] typeDescs = { "className", "methodName", "fileName", "lineNumber", "nativeMethod" };
		OpenType[] typeTypes = {
				SimpleType.STRING,
				SimpleType.STRING,
				SimpleType.STRING,
				SimpleType.INTEGER,
				SimpleType.BOOLEAN };
		try {
			result = new CompositeType(StackTraceElement.class.getName(),
					StackTraceElement.class.getName(), typeNames, typeDescs,
					typeTypes);
		} catch (OpenDataException e) {
			e.printStackTrace();
		}
		return result;
	}

	/**
	 * @return new array of <code>CompositeData</code> representing an array
	 *         of <code>StackTraceElement</code>.
	 */
	protected static CompositeData[] createGoodStackTraceCompositeData() {
		// Let's make the array have three elements. Doesn't matter that
		// they are all identical.
		CompositeData[] result = new CompositeData[3];
		CompositeType cType = createGoodStackTraceElementCompositeType();
		String[] names = { "className", "methodName", "fileName", "lineNumber", "nativeMethod" };
		Object[] values = {
				new String("foo.bar.Blobby"),
				new String("takeOverWorld"),
				new String("Blobby.java"),
				new Integer(2100),
				new Boolean(false) };

		for (int i = 0; i < result.length; i++) {
			try {
				result[i] = new CompositeDataSupport(cType, names, values);
			} catch (OpenDataException e) {
				e.printStackTrace();
			}
		} // end for
		return result;
	}
}
