/*******************************************************************************
 * Copyright (c) 2005, 2019 IBM Corp. and others
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
import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.util.HashSet;
import java.util.Hashtable;
import java.util.Iterator;
import java.util.Map;

import javax.management.Attribute;
import javax.management.AttributeList;
import javax.management.AttributeNotFoundException;
import javax.management.InstanceNotFoundException;
import javax.management.IntrospectionException;
import javax.management.InvalidAttributeValueException;
import javax.management.MBeanAttributeInfo;
import javax.management.MBeanConstructorInfo;
import javax.management.MBeanException;
import javax.management.MBeanInfo;
import javax.management.MBeanNotificationInfo;
import javax.management.MBeanOperationInfo;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import javax.management.ReflectionException;
import javax.management.openmbean.CompositeData;

import com.ibm.lang.management.ExtendedThreadInfo;
import com.ibm.lang.management.ThreadMXBean;

/**
 * @brief Unit test to test the functionality of the TestThreadMXBean class.
 */
@Test(groups = { "level.sanity" })
public class TestThreadMXBean {

	private static Logger logger = Logger.getLogger(TestThreadMXBean.class);
	private static final Map<String, AttributeData> attribs;
	private static final HashSet<String> ignoredAttributes;

	static {
		ignoredAttributes = new HashSet<>();
		ignoredAttributes.add("ObjectName");
		attribs = new Hashtable<String, AttributeData>();
		attribs.put("AllThreadIds", new AttributeData("[J", true, false, false));
		attribs.put("CurrentThreadCpuTime", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("CurrentThreadUserTime", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("DaemonThreadCount", new AttributeData(Integer.TYPE.getName(), true, false, false));
		attribs.put("PeakThreadCount", new AttributeData(Integer.TYPE.getName(), true, false, false));
		attribs.put("ThreadCount", new AttributeData(Integer.TYPE.getName(), true, false, false));
		attribs.put("TotalStartedThreadCount", new AttributeData(Long.TYPE.getName(), true, false, false));
		attribs.put("CurrentThreadCpuTimeSupported", new AttributeData(Boolean.TYPE.getName(), true, false, true));
		attribs.put("ThreadContentionMonitoringEnabled", new AttributeData(Boolean.TYPE.getName(), true, true, true));
		attribs.put("ThreadContentionMonitoringSupported",
				new AttributeData(Boolean.TYPE.getName(), true, false, true));
		attribs.put("ThreadCpuTimeEnabled", new AttributeData(Boolean.TYPE.getName(), true, true, true));
		attribs.put("ThreadCpuTimeSupported", new AttributeData(Boolean.TYPE.getName(), true, false, true));
		attribs.put("ObjectMonitorUsageSupported", new AttributeData(Boolean.TYPE.getName(), true, false, true));
		attribs.put("SynchronizerUsageSupported", new AttributeData(Boolean.TYPE.getName(), true, false, true));
	}// end static initializer

	private ThreadMXBean tb;

	private MBeanServer mbs;

	private ObjectName objName;

	@BeforeClass
	protected void setUp() throws Exception {
		tb = (ThreadMXBean)ManagementFactory.getThreadMXBean();
		try {
			objName = new ObjectName(ManagementFactory.THREAD_MXBEAN_NAME);
			mbs = ManagementFactory.getPlatformMBeanServer();
		} catch (Exception me) {
			Assert.fail("Unexpected exception in setting up ThreadMXBean test: " + me.getMessage());
		}
		logger.info("Starting TestThreadMXBean tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
	}

	@Test
	public final void testFindMonitorDeadlockedThreads() {
		// Check that if there are no deadlocked threads we get back
		// a null rather than a zero length array.
		long[] ids = tb.findMonitorDeadlockedThreads();
		if (ids != null) {
			AssertJUnit.assertTrue(ids.length != 0);
		}
	}

	@Test
	public final void testGetAllThreadIds() {
		int count = tb.getThreadCount();
		long[] ids = tb.getAllThreadIds();
		logger.debug("Current count = " + count);
		logger.debug("Current ids count = " + ids.length);

		AssertJUnit.assertNotNull(ids);
	}

	@Test
	public final void testGetAllNativeIds() {
		long[] threadIds = tb.getAllThreadIds();
		AssertJUnit.assertNotNull(threadIds);
		try {
			long[] nativeTIDs = tb.getNativeThreadIds(threadIds);
			AssertJUnit.assertNotNull(nativeTIDs);
			for (int iter = 0; iter < nativeTIDs.length; iter++) {
				/* Threads in the virtual machine can die off between us querying for JLM.tId and obtaining
				 * the corresponding native identifiers.  This is really not an error, then, if some of these
				 * slots are filled with -1 to represent this fact (that the thread is no longer available).
				 */
				if (nativeTIDs[iter] <= 0) {
					logger.debug("Thread corresponding to " + threadIds[iter] + " is no longer available with the OS.");
				} else {
					logger.debug("Native thread ID " + nativeTIDs[iter] + " assigned to thread: " + threadIds[iter]);
				}
			}
		} catch (Exception e) {
			/* Positive test case: exceptions shouldn't occur or it fails the test. */
			e.printStackTrace();
			Assert.fail("Unexpected exception occurred while executing getNativeThreadIds().");
		}
	}

	@Test
	public final void testGetAllNativeIdsNegative() {
		boolean failed = true;
		long[] threadIds = tb.getAllThreadIds();
		AssertJUnit.assertNotNull(threadIds);
		/* Corrupt the thread ID array with invalid values to check how the virtual machine
		 * handles this.  The requirement is that it should throw an IllegalArgumentException.
		 */

		/* Negative test scenario [1]: tid passed is 0. Must have at least 1 thread.  Set [0] to 0. */
		threadIds[0] = 0;
		try {
			long[] nativeTIDs = tb.getNativeThreadIds(threadIds);
			Assert.fail("Unreacheable code reached (exception thrown should bypass this)."); /* unreachable */
		} catch (Exception e) {
			/* Expect an IllegalArgumentException exception. */
			AssertJUnit.assertTrue(e instanceof java.lang.IllegalArgumentException);
			logger.debug("Exception occurred: as expected (caused by invalid TID '0' being passed).");
			failed = false;
		}
		if (failed) {
			/* Test failed if the expected exception didn't occur! */
			Assert.fail("Exception expected while executing getNativeThreadIds(), but was not thrown.");
		}

		/* Negative test scenario [2]: tid passed is -1. Must have at least 1 thread.  Set [0] to -1. */
		threadIds[0] = -1;
		failed = true;
		try {
			long[] nativeTIDs2 = tb.getNativeThreadIds(threadIds);
			Assert.fail("Unreacheable code reached (exception thrown should bypass this)."); /* unreachable */
		} catch (Exception e) {
			/* Expect an IllegalArgumentException exception. */
			AssertJUnit.assertTrue(e instanceof java.lang.IllegalArgumentException);
			logger.debug("Exception occurred: as expected (caused by invalid TID '-1' being passed).");
			failed = false;
		}
		if (failed) {
			/* Test failed if the expected exception didn't occur! */
			Assert.fail("Exception expected while executing getNativeThreadIds(), but was not thrown.");
		}
	}

	@Test
	public final void testGetNativeId() {
		long[] threadIds = tb.getAllThreadIds();
		AssertJUnit.assertNotNull(threadIds);
		/* Go through the array of JLM thread IDs and find the corresponding native thread IDs.
		 * Check for their validity and print them out.
		 */
		try {
			for (int iter = 0; iter < threadIds.length; iter++) {
				long nativeID = tb.getNativeThreadId(threadIds[iter]);
				if (nativeID <= 0) {
					/* Not a failure; just that the thread's not available with the OS. */
					logger.debug("Thread corresponding to " + threadIds[iter] + " is no longer available with the OS.");
				} else {
					logger.debug("Native thread ID: " + nativeID + " corresponds with the java/lang/Thread.getId(): "
							+ threadIds[iter]);
				}
			}
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception occurred while executing getNativeThreadId().");
		}
	}

	@Test
	public final void testGetNativeIdNegative() {
		boolean success = false;
		/* Negative tests: check how the virtual machine responds if invalid thread IDs are passed to it.
		 * The requirement is that an IllegalArgumentException exception should be thrown.
		 */
		logger.debug("Negative test scenario [1]: passing negative value for TID ...");
		try {
			logger.debug("Native ID [not printed]: " + tb.getNativeThreadId(-1));
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof java.lang.IllegalArgumentException);
			logger.debug("Exception occurred: as expected (caused by invalid TID '-1' being passed).");
			success = true;
		}
		if (!success) {
			/* Failure! */
			Assert.fail("Exception expected while executing getNativeThreadId(), but was not thrown.");
		}

		logger.debug("Negative test scenario [2]: passing 0 for TID ...");
		success = false;
		try {
			logger.debug("Native ID [not printed]: " + tb.getNativeThreadId(0));
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof java.lang.IllegalArgumentException);
			logger.debug("Exception occurred: as expected (caused by invalid TID '0' being passed).");
			success = true;
		}
		if (!success) {
			/* Failure! */
			Assert.fail("Exception expected while executing getNativeThreadId(), but was not thrown.");
		}
	}

	@Test
	public final void testDumpAll() {
		try {
			/* dumpAllExtendedThreads() fetches an array of ExtendedThreadInfo objects that we can examine. */
			ExtendedThreadInfo[] tinfo = tb.dumpAllExtendedThreads(true, true);
			AssertJUnit.assertNotNull(tinfo);

			/* Loop through the array obtained, examining each thread. */
			for (ExtendedThreadInfo iter : tinfo) {
				java.lang.management.ThreadInfo thr = iter.getThreadInfo();
				AssertJUnit.assertNotNull(thr);
				long thrId = thr.getThreadId();
				long nativeTid = iter.getNativeThreadId();
				if (nativeTid <= 0) {
					logger.debug("Thread corresponding to " + thrId + " is no longer available with the OS.");
				} else {
					logger.debug("Thread ID: " + thrId + ", thread name: " + thr.getThreadName() + ", native thread ID: "
							+ nativeTid);
				}
			}
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception occurred while executing dumpAllExtendedThreads().");
		}
	}

	@Test
	public final void testGetCurrentThreadCpuTime() {
		// Outcome depends on whether or not CPU time measurement is supported
		// and enabled.
		if (tb.isCurrentThreadCpuTimeSupported()) {
			if (tb.isThreadCpuTimeEnabled()) {
				AssertJUnit.assertTrue(tb.getCurrentThreadCpuTime() > -1);
			} else {
				AssertJUnit.assertTrue(tb.getCurrentThreadCpuTime() == -1);
			}
		} else {
			try {
				long tmp = tb.getCurrentThreadCpuTime();
				Assert.fail("Should have thrown an exception!");
			} catch (UnsupportedOperationException e) {
			}
		}
	}

	@Test
	public final void testGetCurrentThreadUserTime() {
		// Outcome depends on whether or not CPU time measurement is supported
		// and enabled.
		if (tb.isCurrentThreadCpuTimeSupported()) {
			if (tb.isThreadCpuTimeEnabled()) {
				AssertJUnit.assertTrue(tb.getCurrentThreadUserTime() > -1);
			} else {
				AssertJUnit.assertTrue(tb.getCurrentThreadUserTime() == -1);
			}
		} else {
			try {
				long tmp = tb.getCurrentThreadUserTime();
				Assert.fail("Should have thrown an exception!");
			} catch (UnsupportedOperationException e) {
			}
		}
	}

	@Test
	public final void testGetDaemonThreadCount() {
		AssertJUnit.assertTrue(tb.getDaemonThreadCount() > -1);
	}

	@Test
	public final void testGetPeakThreadCount() {
		AssertJUnit.assertTrue(tb.getPeakThreadCount() > -1);
	}

	@Test
	public final void testGetThreadCount() {
		AssertJUnit.assertTrue(tb.getThreadCount() > -1);
	}

	@Test
	public final void testGetThreadCpuTime() {
		// Outcome depends on whether or not CPU time measurement is supported
		// and enabled.
		if (tb.isThreadCpuTimeSupported()) {
			if (tb.isThreadCpuTimeEnabled()) {
				// Good case
				AssertJUnit.assertTrue(tb.getThreadCpuTime(Thread.currentThread().getId()) > -1);

				// Should throw a wobbler if a bad Thread id is passed in.
				try {
					long tmp = tb.getThreadCpuTime(-122);
					Assert.fail("Should have thrown an exception!");
				} catch (Exception e) {
					AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
				}

			} else {
				// Should return -1 if CPU time measurement is currently
				// disabled.
				AssertJUnit.assertTrue(tb.getThreadCpuTime(Thread.currentThread().getId()) == -1);
			}
		} else {
			try {
				long tmp = tb.getThreadCpuTime(100);
				Assert.fail("Should have thrown an exception!");
			} catch (UnsupportedOperationException e) {
			}
		}
	}

	/*
	 * Class under test for ThreadInfo getThreadInfo(long)
	 */
	@Test
	public final void testGetThreadInfolong() {
		// Should throw exception if a Thread id of 0 or less is input
		try {
			ThreadInfo tmp = tb.getThreadInfo(0);
			Assert.fail("Should have thrown an exception");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
		}

		// For now, just check we don't get a null returned if we pass in
		// a Thread id which is definitely valid (i.e. our Thread id)...
		AssertJUnit.assertNotNull(tb.getThreadInfo(Thread.currentThread().getId()));
	}

	@Test
	public final void testGetThreadInfoNullForUnstartedThread() {
		// Create another thread in the VM.
		Runnable r = new Runnable() {
			public void run() {
				logger.debug("Hi from the new thread!");
				try {
					Thread.sleep(2000);
				} catch (InterruptedException e) {
					// NO OP
				}
				logger.debug("Bye from the new thread!");
			}
		};
		Thread thread = new Thread(r);
		// deliberately not starting

		long thrdId = thread.getId();
		AssertJUnit.assertNull(tb.getThreadInfo(thrdId));
	}

	@Test
	public final void testGethThreadInfoNullForTerminatedThread() {
		// Create another thread in the VM.
		Runnable r = new Runnable() {
			public void run() {
				logger.debug("Hi from the new thread!");
				try {
					Thread.sleep(2000);
				} catch (InterruptedException e) {
					// NO OP
				}
				logger.debug("Bye from the new thread!");
			}
		};
		Thread thread = new Thread(r);
		thread.start();

		logger.debug("Waiting to join thread...");
		try {
			thread.join();
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		logger.debug("Joined.");
		logger.debug("State of thread is " + thread.getState());
		long thrdId = thread.getId();
		AssertJUnit.assertNull(tb.getThreadInfo(thrdId));
	}

	/*
	 * Class under test for ThreadInfo[] getThreadInfo(long[])
	 */
	@Test
	public final void testGetThreadInfolongArray() {
		// Should throw exception if a Thread id of 0 or less is input
		try {
			long[] input = new long[] { 0 };
			ThreadInfo[] tmp = tb.getThreadInfo(input);
			Assert.fail("Should have thrown an exception");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
		}

		// For now, just check we don't get a null returned if we pass in
		// a Thread id which is definitely valid (i.e. our Thread id)...
		long[] input = new long[] { Thread.currentThread().getId() };
		AssertJUnit.assertNotNull(tb.getThreadInfo(input));
	}

	/*
	 * Class under test for ThreadInfo[] getThreadInfo(long[], int)
	 */
	@Test
	public final void testGetThreadInfolongArrayint() {
		// Should throw exception if a Thread id of 0 or less is input
		try {
			long[] input = new long[] { 0 };
			ThreadInfo[] tmp = tb.getThreadInfo(input, 0);
			Assert.fail("Should have thrown an exception");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
		}

		// Should throw exception if maxDepth is negative
		try {
			long[] input = new long[] { Thread.currentThread().getId() };
			ThreadInfo[] tmp = tb.getThreadInfo(input, -2445);
			Assert.fail("Should have thrown an exception");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
		}

		// For now, just check we don't get a null returned if we pass in
		// a Thread id which is definitely valid (i.e. our Thread id)...
		long[] input = new long[] { Thread.currentThread().getId() };
		AssertJUnit.assertNotNull(tb.getThreadInfo(input, 0));
	}

	/*
	 * Class under test for ThreadInfo getThreadInfo(long, int)
	 */
	@Test
	public final void testGetThreadInfolongint() {
		// Should throw exception if a Thread id of 0 or less is input
		try {
			ThreadInfo tmp = tb.getThreadInfo(0, 0);
			Assert.fail("Should have thrown an exception");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
		}

		// Should throw exception if maxDepth is negative
		try {
			ThreadInfo tmp = tb.getThreadInfo(0, -44);
			Assert.fail("Should have thrown an exception");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
		}
		// For now, just check we don't get a null returned if we pass in
		// a Thread id which is definitely valid (i.e. our Thread id)...
		AssertJUnit.assertNotNull(tb.getThreadInfo(Thread.currentThread().getId(), 0));
	}

	@Test
	public final void testGetThreadUserTime() {
		// Outcome depends on whether or not CPU time measurement is supported
		// and enabled.
		if (tb.isThreadCpuTimeSupported()) {
			if (tb.isThreadCpuTimeEnabled()) {
				// Good case
				AssertJUnit.assertTrue(tb.getThreadUserTime(Thread.currentThread().getId()) > -1);

				// Should throw a wobbler if a bad Thread id is passed in.
				try {
					long tmp = tb.getThreadUserTime(-122);
					Assert.fail("Should have thrown an exception!");
				} catch (Exception e) {
					AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
				}

			} else {
				// Should return -1 if CPU time measurement is currently
				// disabled.
				AssertJUnit.assertTrue(tb.getThreadUserTime(Thread.currentThread().getId()) == -1);
			}
		} else {
			try {
				long tmp = tb.getThreadUserTime(100);
				Assert.fail("Should have thrown an exception!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
			}
		}
	}

	@Test
	public final void testGetTotalStartedThreadCount() {
		long before = tb.getTotalStartedThreadCount();
		logger.debug("Total started thread count = " + before);

		// Create another thread in the VM.
		Runnable r = new Runnable() {
			public void run() {
				logger.debug("Hi from the new thread!");
				try {
					Thread.sleep(2000);
				} catch (InterruptedException e) {
					// NO OP
				}
				logger.debug("Bye from the new thread!");
			}
		};
		Thread thread = new Thread(r);
		thread.start();

		try {
			Thread.sleep(1000);
		} catch (InterruptedException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}

		long after = tb.getTotalStartedThreadCount();
		logger.debug("Total started thread count = " + after);

		AssertJUnit.assertTrue(after > before);
	}

	@Test
	public final void testIsCurrentThreadCpuTimeSupported() {
		// Should get the same response as a call to the
		// method isThreadCpuTimeSupported().
		AssertJUnit.assertEquals(tb.isCurrentThreadCpuTimeSupported(), tb.isThreadCpuTimeSupported());
	}

	@Test
	public final void testIsThreadContentionMonitoringEnabled() {
		// Response depends on whether or not thread contention
		// monitoring is supported.
		if (tb.isThreadContentionMonitoringSupported()) {
			// Disable tcm
			while (tb.isThreadContentionMonitoringEnabled()) {
				tb.setThreadContentionMonitoringEnabled(false);
				Thread.yield();
			} // end while

			// Check that a ThreadInfo returns -1 where expected.
			ThreadInfo info = tb.getThreadInfo(Thread.currentThread().getId());
			AssertJUnit.assertEquals(-1, info.getBlockedTime());
			AssertJUnit.assertEquals(-1, info.getWaitedTime());

			// Enable tcm
			while (!tb.isThreadContentionMonitoringEnabled()) {
				tb.setThreadContentionMonitoringEnabled(true);
				Thread.yield();
			} // end while

			// Check that waited time and blocked time are now no longer
			// set to -1.
			ThreadInfo info2 = tb.getThreadInfo(Thread.currentThread().getId());
			AssertJUnit.assertTrue(info2.getBlockedTime() > -1);
			AssertJUnit.assertTrue(info2.getWaitedTime() > -1);
		} else {
			try {
				boolean tmp = tb.isThreadContentionMonitoringEnabled();
				Assert.fail("Should have thrown exception!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
			}
		}
	}

	@Test
	public final void testIsThreadContentionMonitoringSupported() {
		// TODO Set - test - reset when the VM supports me doing this
	}

	@Test
	public final void testIsThreadCpuTimeEnabled() {
		// Depends on whether or not Thread CPU timing is actually
		// supported on the VM.
		if (tb.isThreadCpuTimeSupported()) {
			// TODO Set - test - reset, when the VM allows me to do so.
		} else {
			try {
				tb.isThreadCpuTimeEnabled();
				Assert.fail("Should have thrown an exception!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
			}
		}
	}

	@Test
	public final void testIsThreadCpuTimeSupported() {
		// TODO Implement isThreadCpuTimeSupported().
		// Should get the same response as a call to the
		// method isCurrentThreadCpuTimeSupported().
		AssertJUnit.assertEquals(tb.isCurrentThreadCpuTimeSupported(), tb.isThreadCpuTimeSupported());
	}

	@Test
	public final void testResetPeakThreadCount() {
		// TODO Implement resetPeakThreadCount().
		int currentThreadCount = tb.getThreadCount();
		tb.resetPeakThreadCount();
		AssertJUnit.assertTrue(tb.getPeakThreadCount() == currentThreadCount);
	}

	@Test
	public final void testSetThreadContentionMonitoringEnabled() {
		// Depends if thread contention monitoring is supported or not.
		if (tb.isThreadContentionMonitoringSupported()) {
			// TODO Set - test - reset - test ; when VM allows me to.
		} else {
			try {
				tb.setThreadContentionMonitoringEnabled(false);
				Assert.fail("Should have thrown an exception!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
			}
		}
	}

	@Test
	public final void testSetThreadCpuTimeEnabled() {
		// Depends if thread CPU timing is supported or not.
		if (tb.isThreadCpuTimeSupported()) {
			boolean enabled = tb.isThreadCpuTimeEnabled();
			tb.setThreadCpuTimeEnabled(!enabled);
			AssertJUnit.assertEquals(!enabled, tb.isThreadCpuTimeEnabled());
		} else {
			try {
				tb.setThreadCpuTimeEnabled(false);
				Assert.fail("Should have thrown an exception!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof UnsupportedOperationException);
			}
		}
	}

	@Test
	public final void testGetAttribute() {
		try {
			// The good attributes...
			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "AllThreadIds"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "AllThreadIds") instanceof long[]);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "DaemonThreadCount"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "DaemonThreadCount") instanceof Integer);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "PeakThreadCount"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "PeakThreadCount") instanceof Integer);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "ThreadCount"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "ThreadCount") instanceof Integer);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "TotalStartedThreadCount"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "TotalStartedThreadCount") instanceof Long);

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "CurrentThreadCpuTimeSupported"));
			AssertJUnit.assertTrue(mbs.getAttribute(objName, "CurrentThreadCpuTimeSupported") instanceof Boolean);

			if ((Boolean)mbs.getAttribute(objName, "CurrentThreadCpuTimeSupported")) {
				AssertJUnit.assertNotNull(mbs.getAttribute(objName, "CurrentThreadCpuTime"));
				AssertJUnit.assertTrue(mbs.getAttribute(objName, "CurrentThreadCpuTime") instanceof Long);

				AssertJUnit.assertNotNull(mbs.getAttribute(objName, "CurrentThreadUserTime"));
				AssertJUnit.assertTrue(mbs.getAttribute(objName, "CurrentThreadUserTime") instanceof Long);
			} else {
				try {
					long t1 = (Long)mbs.getAttribute(objName, "CurrentThreadCpuTime");
				} catch (Exception e) {
					AssertJUnit.assertTrue(e instanceof MBeanException);
				}

				try {
					long t2 = (Long)mbs.getAttribute(objName, "CurrentThreadUserTime");
				} catch (Exception e) {
					AssertJUnit.assertTrue(e instanceof MBeanException);
				}
			}

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "ThreadContentionMonitoringSupported"));
			AssertJUnit
					.assertTrue((mbs.getAttribute(objName, "ThreadContentionMonitoringSupported")) instanceof Boolean);

			if ((Boolean)mbs.getAttribute(objName, "ThreadContentionMonitoringSupported")) {
				AssertJUnit.assertNotNull(mbs.getAttribute(objName, "ThreadContentionMonitoringEnabled"));
				AssertJUnit
						.assertTrue(mbs.getAttribute(objName, "ThreadContentionMonitoringEnabled") instanceof Boolean);
			} else {
				try {
					boolean b = ((Boolean)(mbs.getAttribute(objName, "ThreadContentionMonitoringEnabled")))
							.booleanValue();
				} catch (Exception e) {
					AssertJUnit.assertTrue(e instanceof MBeanException);
				}
			}

			AssertJUnit.assertNotNull(mbs.getAttribute(objName, "ThreadCpuTimeSupported"));
			AssertJUnit.assertTrue((mbs.getAttribute(objName, "ThreadCpuTimeSupported")) instanceof Boolean);

			if ((Boolean)mbs.getAttribute(objName, "ThreadCpuTimeSupported")) {
				AssertJUnit.assertNotNull(mbs.getAttribute(objName, "ThreadCpuTimeEnabled"));
				AssertJUnit.assertTrue((mbs.getAttribute(objName, "ThreadCpuTimeEnabled")) instanceof Boolean);
			} else {
				try {
					boolean b = ((Boolean)(mbs.getAttribute(objName, "ThreadCpuTimeEnabled"))).booleanValue();
				} catch (Exception e) {
					AssertJUnit.assertTrue(e instanceof MBeanException);
				}
			}
		} catch (AttributeNotFoundException e) {
			Assert.fail("Unexpected AttributeNotFoundException : " + e.getMessage());
		} catch (MBeanException e) {
			Assert.fail("Unexpected MBeanException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
		}

		// A nonexistent attribute should throw an AttributeNotFoundException
		try {
			long rpm = ((Long)(mbs.getAttribute(objName, "RPM")));
			Assert.fail("Should have thrown an AttributeNotFoundException.");
		} catch (Exception e1) {
		}

		// Type mismatch should result in a casting exception
		try {
			String bad = (String)(mbs.getAttribute(objName, "CurrentThreadUserTime"));
			Assert.fail("Should have thrown a ClassCastException");
		} catch (Exception e2) {
		}
	}

	@Test
	public final void testSetAttribute() {
		// There are only two writable attributes in this type.

		Attribute attr = new Attribute("ThreadContentionMonitoringEnabled", new Boolean(true));

		if (tb.isThreadContentionMonitoringSupported()) {
			try {
				// TODO : Set - test - Reset, when VM permits
				mbs.setAttribute(objName, attr);
			} catch (Exception e1) {
				if (e1 instanceof MBeanException) {
					Assert.fail("Unexpected exception : " + ((MBeanException)e1).getCause().getMessage());
				} else {
					Assert.fail("Unexpected exception : " + e1.getMessage());
				}
			}
		} else {
			try {
				mbs.setAttribute(objName, attr);
				Assert.fail("Should have thrown exception!");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof MBeanException);
			}
		}

		attr = new Attribute("ThreadCpuTimeEnabled", new Boolean(true));
		if (tb.isThreadCpuTimeSupported()) {
			try {
				// TODO : Set - test - Reset, when VM permits
				mbs.setAttribute(objName, attr);
			} catch (Exception e1) {
				logger.error("Unexpected exception.");
				e1.printStackTrace();
			}
		} else {
			try {
				// TODO : Set - test - Reset, when VM permits
				mbs.setAttribute(objName, attr);
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof MBeanException);
			}
		}

		// The rest of the attempted sets should fail
		attr = new Attribute("AllThreadIds", new long[] { 1L, 2L, 3L, 4L });
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof AttributeNotFoundException);
		}

		attr = new Attribute("CurrentThreadCpuTime", 1415L);
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof AttributeNotFoundException);
		}

		attr = new Attribute("DaemonThreadCount", 1415);
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof AttributeNotFoundException);
		}

		attr = new Attribute("PeakThreadCount", 1415);
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof AttributeNotFoundException);
		}

		attr = new Attribute("ThreadCount", 1415);
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof AttributeNotFoundException);
		}

		attr = new Attribute("TotalStartedThreadCount", 1415L);
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof AttributeNotFoundException);
		}

		attr = new Attribute("CurrentThreadCpuTimeSupported", true);
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof AttributeNotFoundException);
		}

		attr = new Attribute("ThreadContentionMonitoringEnabled", true);
		if (tb.isThreadContentionMonitoringSupported()) {
			try {
				mbs.setAttribute(objName, attr);
			} catch (Exception e) {
				Assert.fail("Should NOT have thrown an exception!");
			}
		} else {
			try {
				mbs.setAttribute(objName, attr);
				Assert.fail("Should have thrown an exception.");
			} catch (Exception e1) {
				if (tb.isThreadContentionMonitoringSupported()) {
					AssertJUnit.assertTrue(e1 instanceof AttributeNotFoundException);
				}
			}
		}

		attr = new Attribute("ThreadContentionMonitoringSupported", true);
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof AttributeNotFoundException);
		}

		attr = new Attribute("ThreadCpuTimeEnabled", true);
		if (tb.isThreadCpuTimeSupported()) {
			try {
				mbs.setAttribute(objName, attr);
			} catch (Exception e1) {
				Assert.fail("caught unexpected exception.");
			}
		} else {
			try {
				mbs.setAttribute(objName, attr);
				Assert.fail("Should have thrown an exception.");
			} catch (Exception e1) {
				AssertJUnit.assertTrue(e1 instanceof MBeanException);
			}
		}

		attr = new Attribute("ThreadCpuTimeSupported", true);
		try {
			mbs.setAttribute(objName, attr);
			Assert.fail("Should have thrown an exception.");
		} catch (Exception e1) {
			AssertJUnit.assertTrue(e1 instanceof AttributeNotFoundException);
		}

		// Try and set an attribute with an incorrect type.
		attr = new Attribute("ThreadContentionMonitoringEnabled", new Long(42));
		if (tb.isThreadContentionMonitoringSupported()) {
			try {
				mbs.setAttribute(objName, attr);
				Assert.fail("Should have thrown an exception");
			} catch (Exception e1) {
				AssertJUnit.assertTrue(e1 instanceof InvalidAttributeValueException);
			}
		}
	}

	@Test
	public final void testGetAttributes() {
		AttributeList attributes = null;
		try {
			attributes = mbs.getAttributes(objName, attribs.keySet().toArray(new String[] {}));
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
			return;
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertTrue(attribs.size() >= attributes.size());

		// Check through the returned values
		Iterator<?> it = attributes.iterator();
		while (it.hasNext()) {
			Attribute element = (Attribute)it.next();
			AssertJUnit.assertNotNull(element);
			String name = element.getName();
			Object value = element.getValue();
			try {
				if (attribs.containsKey(name)) {
					if (attribs.get(name).type.equals(Long.TYPE.getName())) {
						// Values of -1 are permitted for this kind of bean.
						// e.g. -1 can be returned from
						// getCurrentThreadCpuTime()
						// if CPU time measurement is currently disabled.
						AssertJUnit.assertTrue(((Long)(value)) > -2);
					} // end else a long expected
					else if ((attribs.get(name).type).equals(Boolean.TYPE.getName())) {
						boolean tmp = ((Boolean)value).booleanValue();
					} // end else a boolean expected
					else if (attribs.get(name).type.equals(Integer.TYPE.getName())) {
						// Values of -1 are permitted for this kind of bean.
						// e.g. -1 can be returned from
						// getCurrentThreadCpuTime()
						// if CPU time measurement is currently disabled.
						AssertJUnit.assertTrue(((Integer)(value)) > -2);
					} // end else a long expected
					else if (attribs.get(name).type.equals("[J")) {
						long[] tmp = (long[])value;
						AssertJUnit.assertNotNull(tmp);
					} // end else a String array expected
					else {
						Assert.fail("Unexpected attribute type returned! : " + name + " , value = " + value);
					}
				} // end if a known attribute
				else {
					Assert.fail("Unexpected attribute name returned!");
				} // end else an unknown attribute
			} catch (Exception e) {
				Assert.fail("Unexpected exception thrown : " + e.getMessage());
			}
		} // end while

		// A failing scenario - pass in an attribute that is not part of
		// the management interface.
		String[] badNames = { "Cork", "Galway" };
		try {
			attributes = mbs.getAttributes(objName, badNames);
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException: " + e.getMessage());
			return;
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(attributes);
		// No attributes will have been returned.
		AssertJUnit.assertTrue(attributes.size() == 0);
	}

	@Test
	public final void testSetAttributes() {
		// Ideal scenario...
		AttributeList attList = new AttributeList();
		Attribute tcme = new Attribute("ThreadContentionMonitoringEnabled", new Boolean(false));
		Attribute tcte = new Attribute("ThreadCpuTimeEnabled", new Boolean(true));
		attList.add(tcme);
		attList.add(tcte);
		AttributeList setAttrs = null;
		try {
			setAttrs = mbs.setAttributes(objName, attList);
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
			return;
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() <= 2);

		// TODO : Can't verify that the value has been set until we are *really*
		// setting property in the VM.

		// A failure scenario - a non-existent attribute...
		AttributeList badList = new AttributeList();
		Attribute garbage = new Attribute("Auchenback", new Long(2888));
		badList.add(garbage);
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
			return;
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 0);

		// Another failure scenario - a non-writable attribute...
		badList = new AttributeList();
		garbage = new Attribute("ThreadCount", new Long(2888));
		badList.add(garbage);
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
			return;
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 0);

		// Yet another failure scenario - a wrongly-typed attribute...
		badList = new AttributeList();
		garbage = new Attribute("ThreadCpuTimeEnabled", new Long(2888));
		badList.add(garbage);
		try {
			setAttrs = mbs.setAttributes(objName, badList);
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
			return;
		} catch (ReflectionException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected ReflectionException occurred: " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(setAttrs);
		AssertJUnit.assertTrue(setAttrs.size() == 0);
	}

	@Test
	public final void testInvoke() {
		// This type of bean has 8 different operations that can be invoked
		// on it.
		try {
			Object retVal = mbs.invoke(objName, "findMonitorDeadlockedThreads", new Object[] {}, null);
			// Can get a null return if there are currently no deadlocked
			// threads.
		} catch (InstanceNotFoundException e) {
			// An unlikely exception - if this occurs, we can't proceed with the test.
			Assert.fail("Unexpected InstanceNotFoundException occurred: " + e.getMessage());
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception: " + e.getMessage());
		}

		// Good case.
		try {
			Object retVal = mbs.invoke(objName, "getThreadCpuTime",
					new Object[] { new Long(Thread.currentThread().getId()) }, new String[] { Long.TYPE.getName() });
			AssertJUnit.assertNotNull(retVal);
			AssertJUnit.assertTrue(retVal instanceof Long);
		} catch (Exception e) {
			e.printStackTrace();
			Assert.fail("Unexpected exception: " + e.getMessage());
		}

		// Force exception by passing in a negative Thread id
		try {
			Object retVal = mbs.invoke(objName, "getThreadCpuTime", new Object[] { new Long(-757) },
					new String[] { Long.TYPE.getName() });
			Assert.fail("Should have thrown an exception!");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
		}

		long[] allThreadIds = tb.getAllThreadIds();
		AssertJUnit.assertNotNull(allThreadIds);
		long firstThreadId = allThreadIds[0];
		AssertJUnit.assertNotNull(firstThreadId);

		ThreadInfo info = tb.getThreadInfo(Thread.currentThread().getId());
		AssertJUnit.assertNotNull(info);
		logger.debug("For current thread : " + info.getThreadName());

		// Good case. long
		try {
			Object retVal = mbs.invoke(objName, "getThreadInfo",
					new Object[] { new Long(Thread.currentThread().getId()) }, new String[] { Long.TYPE.getName() });
			// TODO Can't test until we can get back ThreadInfo objects
			// from the getThreadInfo(long) method.
			AssertJUnit.assertNotNull(retVal);
			AssertJUnit.assertTrue(retVal instanceof CompositeData);
			CompositeData cd = (CompositeData)retVal;
			AssertJUnit.assertTrue(cd.containsKey("threadId"));
		} catch (MBeanException e) {
			Assert.fail("MBeanException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("ReflectionException : " + e.getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("InstanceNotFoundException : " + e.getMessage());
		}

		// Force exception by passing in a negative Thread id. long
		try {
			Object retVal = mbs.invoke(objName, "getThreadInfo", new Object[] { new Long(-5353) },
					new String[] { Long.TYPE.getName() });
			Assert.fail("Should have thrown an exception!");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
		}

		// Good case. long, int
		try {
			Object retVal = mbs.invoke(objName, "getThreadInfo",
					new Object[] { new Long(Thread.currentThread().getId()), new Integer(0) },
					new String[] { Long.TYPE.getName(), Integer.TYPE.getName() });
			// TODO Can't test until we can get back ThreadInfo objects
			// from the getThreadInfo(long) method.
			AssertJUnit.assertNotNull(retVal);
			AssertJUnit.assertTrue(retVal instanceof CompositeData);
			CompositeData cd = (CompositeData)retVal;
			AssertJUnit.assertTrue(cd.containsKey("threadId"));
		} catch (MBeanException e) {
			Assert.fail("MBeanException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("ReflectionException : " + e.getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("InstanceNotFoundException : " + e.getMessage());
		}

		// Force exception by passing in a negative Thread id. long, int
		try {
			Object retVal = mbs.invoke(objName, "getThreadInfo", new Object[] { new Long(-8467), new Integer(0) },
					new String[] { Long.TYPE.getName(), Integer.TYPE.getName() });
			Assert.fail("Should have thrown an exception!");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
		}

		// Good case. long[], int
		try {
			Object retVal = mbs.invoke(objName, "getThreadInfo",
					new Object[] { new long[] { Thread.currentThread().getId() }, new Integer(0) },
					new String[] { "[J", Integer.TYPE.getName() });
			// TODO Can't test until we can get back ThreadInfo objects
			// from the getThreadInfo(long) method.
			AssertJUnit.assertNotNull(retVal);
			AssertJUnit.assertTrue(retVal instanceof CompositeData[]);
			CompositeData[] cd = (CompositeData[])retVal;
			AssertJUnit.assertTrue(cd[0].containsKey("threadId"));
		} catch (MBeanException e) {
			Assert.fail("MBeanException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("ReflectionException : " + e.getCause().getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("InstanceNotFoundException : " + e.getMessage());
		}

		// Force exception by passing in a negative Thread id. long[], int
		try {
			Object retVal = mbs.invoke(objName, "getThreadInfo",
					new Object[] { new long[] { -54321L }, new Integer(0) },
					new String[] { "[J", Integer.TYPE.getName() });
			Assert.fail("Should have thrown an exception!");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
		}

		// Good case. long[]
		try {
			Object retVal = mbs.invoke(objName, "getThreadInfo",
					new Object[] { new long[] { Thread.currentThread().getId() } }, new String[] { "[J" });
			// TODO Can't test until we can get back ThreadInfo objects
			// from the getThreadInfo(long) method.
			AssertJUnit.assertNotNull(retVal);
			AssertJUnit.assertTrue(retVal instanceof CompositeData[]);
			CompositeData[] cd = (CompositeData[])retVal;
			AssertJUnit.assertTrue(cd[0].containsKey("threadId"));
		} catch (MBeanException e) {
			logger.error("Unexpected MBeanException: ");
			e.printStackTrace();
			Assert.fail("MBeanException : " + e.getCause().getMessage());
		} catch (ReflectionException e) {
			Assert.fail("ReflectionException : " + e.getCause().getMessage());
		} catch (InstanceNotFoundException e) {
			Assert.fail("InstanceNotFoundException : " + e.getMessage());
		}

		// Force exception by passing in a negative Thread id. long[]
		try {
			Object retVal = mbs.invoke(objName, "getThreadInfo", new Object[] { new long[] { -74747L } },
					new String[] { "[J" });
			Assert.fail("Should have thrown an exception!");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
		}

		// Good case.
		try {
			Object retVal = mbs.invoke(objName, "getThreadUserTime",
					new Object[] { new Long(Thread.currentThread().getId()) }, new String[] { Long.TYPE.getName() });
			AssertJUnit.assertNotNull(retVal);
			AssertJUnit.assertTrue(retVal instanceof Long);
		} catch (Exception e) {
			Assert.fail("Unexpected exception.");
		}

		// Force exception by passing in a negative Thread id
		try {
			Object retVal = mbs.invoke(objName, "getThreadUserTime", new Object[] { new Long(-757) },
					new String[] { Long.TYPE.getName() });
			Assert.fail("Should have thrown an exception!");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof javax.management.RuntimeMBeanException);
		}

		try {
			Object retVal = mbs.invoke(objName, "resetPeakThreadCount", new Object[] {}, null);
			AssertJUnit.assertNull(retVal);
			// Verify that after this operation is invoked, the
			// peak thread count equals the value of current thread count
			// taken prior to this method call.
			AssertJUnit.assertTrue(tb.getPeakThreadCount() == tb.getThreadCount());
		} catch (Exception e) {
			Assert.fail("Unexpected exception.");
		}
	}

	@Test
	public final void testGetMBeanInfo() {
		MBeanInfo mbi = null;
		try {
			mbi = mbs.getMBeanInfo(objName);
		} catch (InstanceNotFoundException e) {
			Assert.fail("Unexpected InstanceNotFoundException : " + e.getMessage());
			return;
		} catch (ReflectionException e) {
			Assert.fail("Unexpected ReflectionException : " + e.getMessage());
			return;
		} catch (IntrospectionException e) {
			Assert.fail("Unexpected IntrospectionException : " + e.getMessage());
			return;
		}
		AssertJUnit.assertNotNull(mbi);

		// Now make sure that what we got back is what we expected.

		// Class name
		AssertJUnit.assertTrue(mbi.getClassName().equals(tb.getClass().getName()));

		// No public constructors
		MBeanConstructorInfo[] constructors = mbi.getConstructors();
		AssertJUnit.assertNotNull(constructors);
		AssertJUnit.assertEquals(0, constructors.length);

		// 14 operations
		MBeanOperationInfo[] operations = mbi.getOperations();
		AssertJUnit.assertNotNull(operations);
		AssertJUnit.assertEquals(14, operations.length);

		// No notifications
		MBeanNotificationInfo[] notifications = mbi.getNotifications();
		AssertJUnit.assertNotNull(notifications);
		AssertJUnit.assertEquals(0, notifications.length);

		// Print description and the class name (not necessarily identical).
		logger.debug("MBean description for " + tb.getClass().getName() + ": " + mbi.getDescription());

		// 14 attributes
		MBeanAttributeInfo[] attributes = mbi.getAttributes();
		AssertJUnit.assertNotNull(attributes);
		AssertJUnit.assertEquals(15, attributes.length);
		for (int i = 0; i < attributes.length; i++) {
			MBeanAttributeInfo info = attributes[i];
			AssertJUnit.assertNotNull(info);
			AllManagementTests.validateAttributeInfo(info, TestThreadMXBean.ignoredAttributes, attribs);
		} // end for
	}

	//    /**
	//     * @tests
	//     * @link ThreadMXBean#isObjectMonitorUsageSupported()
	//     *
	//     */
	//    public void testisObjectMonitorUsageSupported() {
	//        AssertJUnit.assertTrue(tb.isObjectMonitorUsageSupported());
	//    }
	//
	//    /**
	//     * @tests
	//     * @link ThreadMXBean#isSynchronizerUsageSupported()
	//     *
	//     */
	//    public void testisSynchronizerUsageSupported() {
	//        AssertJUnit.assertTrue(tb.isSynchronizerUsageSupported());
	//    }
	//
	//    /**
	//     * @tests
	//     * @link ThreadMXBean#findDeadlockedThreads()
	//     *
	//     */
	//    public void testfindDeadlockedThreads_null() {
	//        final Object lock = new Object();
	//        long[] threadIDs = tb.findDeadlockedThreads();
	//        AssertJUnit.assertNull(threadIDs);
	//        new Thread() {
	//            @Override
	//            public void run() {
	//                try {
	//                    sleep(1000);
	//                } catch (InterruptedException e) {
	//                    // do nothing
	//                }
	//            }
	//        }.start();
	//        threadIDs = tb.findDeadlockedThreads();
	//        AssertJUnit.assertNull(threadIDs);
	//        try {
	//            Thread.sleep(1000);
	//        } catch (InterruptedException e) {
	//            // do nothing
	//        }
	//        synchronized (lock) {
	//            lock.notifyAll();
	//        }
	//    }
	//
	//    /**
	//     * @tests
	//     * @link ThreadMXBean#findDeadlockedThreads()
	//     *
	//     */
	//    public void testfindDeadlockedThreads() {
	//        final Object lock1 = new Object();
	//        final Object lock2 = new Object();
	//        long[] threadIDs;
	//        Thread t1 = new Thread() {
	//            @Override
	//            public void run() {
	//                try {
	//                    synchronized (lock1) {
	//                        sleep(1000);
	//                        synchronized (lock2) {
	//                            System.out.println("got locks in thread1");
	//                        }
	//                    }
	//                } catch (InterruptedException e) {
	//                    // do nothing
	//                }
	//            }
	//        };
	//        Thread t2 = new Thread() {
	//            @Override
	//            public void run() {
	//                try {
	//                    synchronized (lock2) {
	//                        sleep(1000);
	//                        synchronized (lock1) {
	//                            System.out.println("got locks in thread1");
	//                        }
	//                    }
	//                } catch (InterruptedException e) {
	//                    // do nothing
	//                }
	//            }
	//        };
	//        t1.start();
	//        t2.start();
	//        try {
	//            Thread.sleep(1000);
	//        } catch (InterruptedException e) {
	//            // do nothing
	//        }
	//        // RI fails here
	//        threadIDs = tb.findDeadlockedThreads();
	//        AssertJUnit.assertNotNull(threadIDs);
	//        t1.interrupt();
	//        t2.interrupt();
	//    }
	//
	//    /**
	//     * @tests
	//     * @link ThreadMXBean#findDeadlockedThreads()
	//     *
	//     */
	//    public void testdumpAllThreads() {
	//        final Object lock1 = new Object();
	//        ThreadInfo[] threadinfos = tb.dumpAllThreads(true, true);
	//        // must be more than 6 thread: ReaderThread,Attach Listener, Signal
	//        // Dispatcher, Finalizer, Reference Handler, main
	//        AssertJUnit.assertTrue(threadinfos.length >= 6);
	//        int threadsum = threadinfos.length;
	//        new Thread() {
	//            @Override
	//            public void run() {
	//                try {
	//                    synchronized (lock1) {
	//                        lock1.wait();
	//                    }
	//                } catch (InterruptedException e) {
	//                    // do nothing
	//                }
	//            }
	//        }.start();
	//        assertEquals(threadsum + 1, tb.dumpAllThreads(true, true).length);
	//        synchronized (lock1) {
	//            lock1.notifyAll();
	//        }
	//    }
	//
	//    /**
	//     * @tests
	//     * @link ThreadMXBean#findDeadlockedThreads()
	//     *
	//     */
	//    public void testgetThreadInfolongbooleanboolean() {
	//        long[] ids = tb.getAllThreadIds();
	//        ThreadInfo[] threadinfos = tb.getThreadInfo(ids, true, true);
	//        // must be more than 6 thread: ReaderThread,Attach Listener, Signal
	//        // Dispatcher, Finalizer, Reference Handler, main
	//        int threadsum = threadinfos.length;
	//        AssertJUnit.assertTrue(threadsum >= 6);
	//        for (int i = 0; i < threadinfos.length; i++) {
	//            logger.debug(threadinfos[i]);
	//        }
	//        assertEquals(threadsum, tb.getThreadInfo(ids, false, false).length);
	//        try {
	//            tb.getThreadInfo(null, true, true);
	//            fail("should throw NullPointerException");
	//        } catch (NullPointerException e) {
	//            // excepted
	//        }
	//        try {
	//            tb.getThreadInfo(null, false, false);
	//            fail("should throw NullPointerException");
	//        } catch (NullPointerException e) {
	//            // excepted
	//        }
	//    }
}
