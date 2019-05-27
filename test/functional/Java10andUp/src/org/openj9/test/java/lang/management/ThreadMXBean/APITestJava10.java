/*******************************************************************************
 * Copyright (c) 2018, 2019 IBM Corp. and others
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
package org.openj9.test.java.lang.management.ThreadMXBean;


import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Objects;
import java.util.function.UnaryOperator;

import static org.testng.Assert.fail;

import org.testng.Assert;
import org.testng.annotations.Test;

/*
 * Verify Java 10 additions, specifically depth-limited stack traces.
 */
public class APITestJava10 extends ThreadMXBeanTestCase {
	private static final int MAXDEPTH = 10;
	public Object waitObject = new Object();
	ThreadMXBean myBean;

	public APITestJava10() {
		myBean = ManagementFactory.getThreadMXBean();
	}

	class TestThread extends Thread{
		int stackDepth;
		public TestThread(int depth) {
			stackDepth = depth;
			this.setDaemon(true); // Prevent this from blocking test exit.
		}

		public void run() {
			logger.debug("start TestThread #"+Thread.currentThread().getId());
			if (stackDepth > 0) {
				myMethod(stackDepth - 1);
			} else { // Create the shortest stack possible.
				synchronized (waitObject) {
					try {
						waitObject.wait();
					} catch (InterruptedException e) {
						// ignore
					}
				}
			}
			logger.debug("end TestThread #"+Thread.currentThread().getId());
		}

		/*
		 * Build up a stack of specified depth.
		 */
		public Integer myMethod(Integer depth) {
			if (depth > 0) {
				myMethod(depth - 1);
			} else {
				synchronized (waitObject) {
					try {
						waitObject.wait();
					} catch (InterruptedException e) {
						// ignore
					}
				}
			}
			return Integer.valueOf(0);
		}
	}

	class MethodHandleTestThread extends TestThread {
		public MethodHandleTestThread(int depth) {
			super(depth);
		}

		@Override
		public void run() {
			try {
				Method m = TestThread.class.getMethod("myMethod", Integer.class);
				m.invoke(this, Integer.valueOf(stackDepth));
			} catch (NoSuchMethodException | IllegalAccessException | IllegalArgumentException | InvocationTargetException e) {
				fail("Unexpected exception", e);
			}
		}
	}

	class LambdaTestThread extends TestThread {
		public LambdaTestThread(int depth) {
			super(depth);
		}

		@Override
		public void run() {
			try {
				UnaryOperator<Integer> myLambda = s -> myMethod(s);
				myLambda.apply(Integer.valueOf(stackDepth));
			} catch (IllegalArgumentException e) {
				fail("Unexpected exception", e);
			}
		}
	}

	@Test(groups = { "level.extended" })
	public void testDumpLimits() throws InterruptedException {
		logger.debug("testDumpLimits");
		ArrayList<Thread> myThreads = new ArrayList<>();
		Thread currentThread = Thread.currentThread();
		long myId = currentThread.getId();
		long ids[] = new long[MAXDEPTH];
		HashMap<Long, Integer> expectedDepths = new HashMap<>();
		for (int requestedDepth = 0; requestedDepth < MAXDEPTH; requestedDepth++) {
			Thread t = new TestThread(requestedDepth);
			myThreads.add(t);
			long id = t.getId();
			logger.debug("start "+id);
			t.start();
			while (t.getState() != Thread.State.WAITING) {
				try {
					Thread.sleep(1);
				} catch (InterruptedException e) {
					// ignore
				}
			}
			ids[requestedDepth] = id;
			/*
			 * every thread will have one stack frame for the run() method,
			 * plus the specified number of extra frames.
			 */
			expectedDepths.put(id, requestedDepth + 1);
		}

		for (int maxDepth = 0; maxDepth < MAXDEPTH; maxDepth++) {
			logger.debug("maxDepth = "+maxDepth);
			for (ThreadInfo ti: myBean.dumpAllThreads(false, false, maxDepth)) {
				long id = ti.getThreadId();
				Integer requestedDepthTemp = expectedDepths.get(id);
				if (Objects.isNull(requestedDepthTemp)) {
					/* non-test thread */
					continue;
				}
				StackTraceElement[] trace = ti.getStackTrace();
				int traceLength = trace.length;
				Assert.assertTrue(traceLength <= maxDepth, 
						"dumpAllThreads: Wrong stack size ("
								+ traceLength + ") for thread "+
								ti.getThreadName()
								+ " when maxDepth = "+maxDepth);
				int requestedDepth = requestedDepthTemp.intValue();
				checkDepth(requestedDepth, maxDepth, trace, "dumpAllThreads");
			}
			for (ThreadInfo ti: myBean.getThreadInfo(ids, maxDepth)) {
				StackTraceElement[] trace = ti.getStackTrace();
				Assert.assertTrue(trace.length <= maxDepth, "getThreadInfo: Wrong stack size when maxDepth = "+maxDepth);
				long id = ti.getThreadId();
				Integer requestedDepthTemp = expectedDepths.get(id);
				if (null == requestedDepthTemp) {
					/* non-test thread */
					continue;
				}
				int requestedDepth = requestedDepthTemp;
				checkDepth(requestedDepth, maxDepth, trace, "getThreadInfo");
			}
		}
		synchronized (waitObject) {
			waitObject.notifyAll();
		}
		for (Thread t: myThreads) {
			logger.debug("wait for "+t.getId());
			t.join(10000);
		}
	}

	@Test(groups = { "level.extended" })
	public void testDumpLimitsWithMethodHandles() {
		final int REQUESTED_DEPTH = 5;
		Thread t = new MethodHandleTestThread(REQUESTED_DEPTH - 1); // Subtract 1 for the run() method.
		checkThreadInfo(REQUESTED_DEPTH, t);
	}

	@Test(groups = { "level.extended" })
	public void testDumpLimitsWithLambdas() {
		final int REQUESTED_DEPTH = 5;
		Thread t = new LambdaTestThread(REQUESTED_DEPTH - 2); // Subtract 1 for the run() method and 1 for the lambda.
		checkThreadInfo(REQUESTED_DEPTH, t);
	}

	private void checkThreadInfo(int requestedDepth, Thread t) {
		t.start();
		final long ids[] = {t.getId()};
		while (t.getState() != Thread.State.WAITING) {
			try {
				Thread.sleep(1);
			} catch (InterruptedException e) {
				// ignore
			}
		}

		for (int maxDepth = 0; maxDepth <= requestedDepth + 1; ++maxDepth) {

			for (ThreadInfo ti: myBean.dumpAllThreads(false, false, maxDepth)) {
				StackTraceElement[] trace = ti.getStackTrace();
				long id = ti.getThreadId();
				if (id != ids[0]) {
					continue;
				} else {
					checkDepth(requestedDepth, maxDepth, trace, "dumpAllThreads");
				}
			}
			ThreadInfo ti = (myBean.getThreadInfo(ids, maxDepth))[0];
			StackTraceElement[] trace = ti.getStackTrace();
			checkDepth(requestedDepth, maxDepth, trace, "getThreadInfo");
		}
	}

	private void checkDepth(int requestedDepth, int maxDepth, StackTraceElement[] trace, String msg) {
		/* the actual depth may be greater than the requested depth due to calls inside wait() */
		if (requestedDepth >= maxDepth) {
			Assert.assertEquals(trace.length, maxDepth, msg+": Wrong stack size when requested depth > maxDepth = "+maxDepth);
		} else {
			Assert.assertTrue(trace.length <= maxDepth, msg+": Stack exceeds maxDepth = "+maxDepth);
			Assert.assertTrue(trace.length >= requestedDepth, msg+": Stack less than minimum expected");
		}
	}
}
