/*******************************************************************************
 * Copyright IBM Corp. and others 2023
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 *******************************************************************************/
package org.openj9.test.jep425;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import static org.testng.Assert.fail;

import java.lang.reflect.*;
import java.lang.Thread;
import java.time.Duration;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.LockSupport;
import java.util.stream.IntStream;

/**
 * Test cases for JEP 425: Virtual Threads (Preview) Continuation execution
 * which verifies the basic cases including Continuation enter, yield, resume.
 */
@Test(groups = { "level.sanity" })
public class VirtualThreadTests {
	static {
		try {
			System.loadLibrary("j9ben");
		} catch (UnsatisfiedLinkError e) {
			System.out.println("No natives for JNI tests");
		}
	}

	public static native boolean lockSupportPark();

	private void incrementalWait(Thread t) throws InterruptedException {
		/* Incrementally wait for 10000 ms. */
		for (int i = 0; i < 200; i++) {
			Thread.sleep(50);
			if (Thread.State.WAITING == t.getState()) {
				break;
			}
		}
	}

	@Test
	public void test_basicVirtualthread() {
		var wrapper = new Object(){ boolean executed = false; };
		try {
			Thread t = Thread.ofVirtual().name("duke").unstarted(() -> {
				wrapper.executed = true;
			});

			t.start();
			t.join();

			AssertJUnit.assertTrue("Virtual Thread operation not executed", wrapper.executed);
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	@Test
	public void test_VirtualthreadYieldResume() {
		try (var executor = Executors.newVirtualThreadPerTaskExecutor()) {
			int numThreads = 6;
			int expectedThreadResult = 3;
			long estimatedThreadCompletionTime = 5L; /* seconds */
			int[] results = new int[numThreads];

			IntStream.range(0, numThreads).forEach(i -> {
					executor.submit(() -> {
							results[i] = 1;
							Thread.sleep(Duration.ofSeconds(1));
							results[i] += 1;
							Thread.sleep(Duration.ofSeconds(1));
							results[i] += 1;
							return i;
					});
			});

			/* Wait incrementally for the worst-case scenario where all virtual threads are
			 * executed sequentially. Exit the wait loop if the virtual threads finish early.
			 */
			for (int i = 0; i < numThreads; i++) {
				executor.awaitTermination(estimatedThreadCompletionTime, TimeUnit.SECONDS);
				boolean exit = true;
				for (int j = 0; j < numThreads; j++) {
					if (results[j] != expectedThreadResult) {
						exit = false;
					}
				}
				if (exit) {
					break;
				}
			}

			for (int i = 0; i < numThreads; i++) {
				AssertJUnit.assertTrue(
						"Virtual Thread " + i + ": incorrect result of " + results[i],
						(results[i] == expectedThreadResult));
			}
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	private static volatile boolean testSyncThreadReady = false;

	@Test
	public void test_synchronizedBlockFromVirtualthread() {
		try {
			Thread t = Thread.ofVirtual().name("synchronized").start(() -> {
				synchronized (VirtualThreadTests.class) {
					testSyncThreadReady = true;
					LockSupport.park();
				}
			});

			while (!testSyncThreadReady) {
				Thread.sleep(10);
			}
			/* Incrementally wait for 10000 ms to let the virtual thread park. */
			incrementalWait(t);
			Assert.assertEquals(t.getState(), Thread.State.WAITING);
			LockSupport.unpark(t);
			t.join();
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	private static volatile boolean testJNIThreadReady = false;

	@Test
	public void test_jniFromVirtualthread() {
		try {
			Thread t = Thread.ofVirtual().name("native").start(() -> {
				testJNIThreadReady = true;
				lockSupportPark();
			});

			while (!testJNIThreadReady) {
				Thread.sleep(10);
			}
			/* Incrementally wait for 10000 ms to let the virtual thread park. */
			incrementalWait(t);
			Assert.assertEquals(t.getState(), Thread.State.WAITING);
			LockSupport.unpark(t);
			t.join();
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	private static volatile boolean testThread1Ready = false;

	@Test
	public void test_YieldedVirtualThreadGetStackTrace() {
		/* The expected frame count is based on test's callstack */
		int expectedFrames = 6;
		String expectedMethodName = "park";

		try {
			Thread t = Thread.ofVirtual().name("yielded-stackwalker").start(() -> {
					testThread1Ready = true;
					LockSupport.park();
				});
			while (!testThread1Ready) {
				Thread.sleep(10);
			}

			/* Incrementally wait for 10000 ms to let the virtual thread park. */
			incrementalWait(t);

			StackTraceElement[] ste = t.getStackTrace();

			/* If the stacktrace doesn't match the expected result, then print out the stacktrace
			 * for debuggging.
			 */
			if ((expectedFrames != ste.length) || !ste[0].getMethodName().equals(expectedMethodName)) {
				for (StackTraceElement st : ste) {
					System.out.println(st);
				}
			}

			AssertJUnit.assertTrue(
					"Expected " + expectedFrames + " frames, got " + ste.length,
					(expectedFrames == ste.length));

			AssertJUnit.assertTrue(
					"Expected top frame to be " + expectedMethodName + ", got " + ste[0].getMethodName(),
					ste[0].getMethodName().equals(expectedMethodName));

			LockSupport.unpark(t);
			t.join();
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	private static volatile boolean testThread2_state = false;

	@Test
	public void test_RunningVirtualThreadGetStackTrace() {
		try {
			/* The expected frame count is based on test's callstack */
			int expectedFrames = 2;
			String expectedClassName = "org.openj9.test.jep425.VirtualThreadTests";

			Thread t = Thread.ofVirtual().name("running-stackwalker").start(() -> {
					testThread2_state = true;
					while (testThread2_state);
				});
			while (!testThread2_state) {
				Thread.sleep(10);
			}

			StackTraceElement[] ste = t.getStackTrace();

			/* If the stacktrace doesn't match the expected result, then print out the stacktrace
			 * for debuggging.
			 */
			if ((expectedFrames != ste.length) || !ste[0].getClassName().equals(expectedClassName)) {
				for (StackTraceElement st : ste) {
					System.out.println(st);
				}
			}

			Assert.assertEquals(ste.length, expectedFrames);
			Assert.assertEquals(ste[0].getClassName(), expectedClassName);

			testThread2_state = false;
			t.join();
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	private static int readVirtualThreadStates(Class vthreadCls, String fieldName) throws Exception {
		Field vthreadState = vthreadCls.getDeclaredField(fieldName);
		vthreadState.setAccessible(true);
		return vthreadState.getInt(null);
	}

	@Test
	public void test_verifyJVMTIMacros() {
		final int JVMTI_VTHREAD_STATE_NEW = 0;
		final int JVMTI_VTHREAD_STATE_STARTED = 1;
		final int JVMTI_VTHREAD_STATE_RUNNABLE= 2;
		final int JVMTI_VTHREAD_STATE_RUNNING = 3;
		final int JVMTI_VTHREAD_STATE_PARKING = 4;
		final int JVMTI_VTHREAD_STATE_PARKED = 5;
		final int JVMTI_VTHREAD_STATE_PINNED = 6;
		final int JVMTI_VTHREAD_STATE_TIMED_PARKING = 7;
		final int JVMTI_VTHREAD_STATE_TIMED_PARKED = 8;
		final int JVMTI_VTHREAD_STATE_TIMED_PINNED = 9;
		final int JVMTI_VTHREAD_STATE_YIELDING = 10;
		final int JVMTI_VTHREAD_STATE_TERMINATED = 99;
		final int JVMTI_VTHREAD_STATE_SUSPENDED = (1 << 8);

		int value = 0;

		try {
			Class<?> vthreadCls = Class.forName("java.lang.VirtualThread");

			value = readVirtualThreadStates(vthreadCls, "NEW");
			if (JVMTI_VTHREAD_STATE_NEW != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_NEW (" + JVMTI_VTHREAD_STATE_NEW + ") does not match VirtualThread.NEW (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "STARTED");
			if (JVMTI_VTHREAD_STATE_STARTED != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_STARTED (" + JVMTI_VTHREAD_STATE_STARTED + ") does not match VirtualThread.STARTED (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "RUNNABLE");
			if (JVMTI_VTHREAD_STATE_RUNNABLE != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_RUNNABLE (" + JVMTI_VTHREAD_STATE_RUNNABLE + ") does not match VirtualThread.RUNNABLE (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "RUNNING");
			if (JVMTI_VTHREAD_STATE_RUNNING != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_RUNNING (" + JVMTI_VTHREAD_STATE_RUNNING + ") does not match VirtualThread.RUNNING (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "PARKING");
			if (JVMTI_VTHREAD_STATE_PARKING != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_PARKING (" + JVMTI_VTHREAD_STATE_PARKING + ") does not match VirtualThread.PARKING (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "PARKED");
			if (JVMTI_VTHREAD_STATE_PARKED != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_PARKED (" + JVMTI_VTHREAD_STATE_PARKED + ") does not match VirtualThread.PARKED (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "PINNED");
			if (JVMTI_VTHREAD_STATE_PINNED != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_PINNED (" + JVMTI_VTHREAD_STATE_PINNED + ") does not match VirtualThread.PINNED (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "TIMED_PARKING");
			if (JVMTI_VTHREAD_STATE_TIMED_PARKING != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_TIMED_PARKING (" + JVMTI_VTHREAD_STATE_TIMED_PARKING + ") does not match VirtualThread.TIMED_PARKING (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "TIMED_PARKED");
			if (JVMTI_VTHREAD_STATE_TIMED_PARKED != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_TIMED_PARKED (" + JVMTI_VTHREAD_STATE_TIMED_PARKED + ") does not match VirtualThread.TIMED_PARKED (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "TIMED_PINNED");
			if (JVMTI_VTHREAD_STATE_TIMED_PINNED != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_TIMED_PINNED (" + JVMTI_VTHREAD_STATE_TIMED_PINNED + ") does not match VirtualThread.TIMED_PINNED (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "YIELDING");
			if (JVMTI_VTHREAD_STATE_YIELDING != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_YIELDING (" + JVMTI_VTHREAD_STATE_YIELDING + ") does not match VirtualThread.YIELDING (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "TERMINATED");
			if (JVMTI_VTHREAD_STATE_TERMINATED != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_TERMINATED (" + JVMTI_VTHREAD_STATE_TERMINATED + ") does not match VirtualThread.TERMINATED (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "SUSPENDED");
			if (JVMTI_VTHREAD_STATE_SUSPENDED != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_SUSPENDED (" + JVMTI_VTHREAD_STATE_SUSPENDED + ") does not match VirtualThread.SUSPENDED (" + value + ")");
			}
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}
}
