/*******************************************************************************
 * Copyright (c) 2022, 2022 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package org.openj9.test.jep425;

import org.testng.annotations.Test;
import org.testng.Assert;
import org.testng.AssertJUnit;
import static org.testng.Assert.fail;

import java.lang.Thread;
import java.lang.reflect.*;
import java.time.Duration;
import java.util.concurrent.Executor;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.stream.IntStream;
import java.util.concurrent.locks.LockSupport;

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
			int[] results = new int[6];
			IntStream.range(0, 6).forEach(i -> {
					executor.submit(() -> {
							results[i] = 1;
							Thread.sleep(Duration.ofSeconds(1));
							results[i] += 1;
							Thread.sleep(Duration.ofSeconds(1));
							results[i] += 1;
							return i;
					});
			});
			executor.awaitTermination(5L, TimeUnit.SECONDS);
			for (int i = 0; i < 6; i++) {
				AssertJUnit.assertTrue("Virtual Thread " + i + ": incorrect result of " + results[i], (results[i] == 3));
			}
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	private static volatile boolean testSyncThread_ready = false;

	@Test
	public void test_synchronizedBlockFromVirtualthread() {
		try {
			Thread t = Thread.ofVirtual().name("synchronized").unstarted(() -> {
				synchronized(VirtualThreadTests.class) {
					testSyncThread_ready = true;
					LockSupport.park();
				}
			});

			t.start();
			while (!testSyncThread_ready) {
				Thread.sleep(1);
			}
			/* Let virtual thread park */
			Thread.sleep(500);
			AssertJUnit.assertTrue("Virtual Thread state should be WAITING", (t.getState() == Thread.State.WAITING));
			LockSupport.unpark(t);
			t.join();
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	private static volatile boolean testJNIThread_ready = false;

	@Test
	public void test_jniFromVirtualthread() {
		try {
			Thread t = Thread.ofVirtual().name("native").unstarted(() -> {
				testJNIThread_ready = true;
				lockSupportPark();
			});

			t.start();
			while (!testJNIThread_ready) {
				Thread.sleep(1);
			}
			/* Let virtual thread park */
			Thread.sleep(500);
			AssertJUnit.assertTrue("Virtual Thread state should be WAITING", (t.getState() == Thread.State.WAITING));
			LockSupport.unpark(t);
			t.join();
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}

	private static volatile boolean testThread1_ready = false;

	@Test
	public void test_YieldedVirtualThreadGetStackTrace() {
		try {
			Thread t = Thread.ofVirtual().name("yielded-stackwalker").start(() -> {
					testThread1_ready = true;
					LockSupport.park();
				});
			while (!testThread1_ready) {
				Thread.sleep(10);
			}
			/* Let virtual thread park */
			Thread.sleep(500);

			StackTraceElement[] ste = t.getStackTrace();
			AssertJUnit.assertTrue("Expected 11 frames, got " + ste.length, (11 == ste.length));
			AssertJUnit.assertTrue("Expected top frame to be yieldImpl, got " + ste[0].getMethodName(), ste[0].getMethodName().equals("yieldImpl"));
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
			Thread t = Thread.ofVirtual().name("running-stackwalker").start(() -> {
					testThread2_state = true;
					while (testThread2_state);
				});
			while (!testThread2_state) {
				Thread.sleep(10);
			}

			StackTraceElement[] ste = t.getStackTrace();
			AssertJUnit.assertTrue("Expected 4 frames, got " + ste.length, (4 == ste.length));
			AssertJUnit.assertTrue("Expected top frame to be VirtualThreadTests class, got " + ste[0].toString(), ste[0].getClassName().equals("org.openj9.test.jep425.VirtualThreadTests"));

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
		final int JVMTI_VTHREAD_STATE_YIELDING = 7;
		final int JVMTI_VTHREAD_STATE_TERMINATED = 99;
		final int JVMTI_VTHREAD_STATE_SUSPENDED = (1 << 8);
		final int JVMTI_VTHREAD_STATE_RUNNABLE_SUSPENDED = (JVMTI_VTHREAD_STATE_RUNNABLE | JVMTI_VTHREAD_STATE_SUSPENDED);
		final int JVMTI_VTHREAD_STATE_PARKED_SUSPENDED = (JVMTI_VTHREAD_STATE_PARKED | JVMTI_VTHREAD_STATE_SUSPENDED);

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

			value = readVirtualThreadStates(vthreadCls, "RUNNABLE_SUSPENDED");
			if (JVMTI_VTHREAD_STATE_RUNNABLE_SUSPENDED != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_RUNNABLE_SUSPENDED (" + JVMTI_VTHREAD_STATE_RUNNABLE_SUSPENDED + ") does not match VirtualThread.RUNNABLE_SUSPENDED (" + value + ")");
			}

			value = readVirtualThreadStates(vthreadCls, "PARKED_SUSPENDED");
			if (JVMTI_VTHREAD_STATE_PARKED_SUSPENDED != value) {
				Assert.fail("JVMTI_VTHREAD_STATE_PARKED_SUSPENDED (" + JVMTI_VTHREAD_STATE_PARKED_SUSPENDED + ") does not match VirtualThread.PARKED_SUSPENDED (" + value + ")");
			}
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
	}
}
