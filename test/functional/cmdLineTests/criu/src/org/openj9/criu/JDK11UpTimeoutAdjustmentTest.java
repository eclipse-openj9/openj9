/*
 * Copyright IBM Corp. and others 2022
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
 */
package org.openj9.criu;

import java.util.Date;
import jdk.internal.misc.Unsafe;
import openj9.internal.criu.InternalCRIUSupport;
import org.eclipse.openj9.criu.CRIUSupport;

public class JDK11UpTimeoutAdjustmentTest {
	private static final long MILLIS_PER_SECOND = 1000L;
	private static final long NANOS_PER_MILLI = 1000_000L;
	private static final long NANOS_PER_SECOND = 1000_000_000L;

	private static final int nsTime500kns = 500000;
	private static final long msTime2s = 2 * MILLIS_PER_SECOND;
	private static final long nsTime2s = 2 * NANOS_PER_SECOND;
	private static final long msTime5s = 5 * MILLIS_PER_SECOND;
	private static final long nsTime5s = 5 * NANOS_PER_SECOND;

	private static final Object objWait = new Object();
	private static final TestResult testResult = new TestResult(true, 0);
	private static final Unsafe unsafe = Unsafe.getUnsafe();

	public static void main(String[] args) throws InterruptedException {
		if (args.length == 0) {
			throw new RuntimeException("Test name required");
		} else {
			String testName = args[0];
			new JDK11UpTimeoutAdjustmentTest().test(testName);
		}
	}

	private void test(String testName) throws InterruptedException {
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		System.out.println("Start test name: " + testName);
		CRIUTestUtils.showThreadCurrentTime("Before starting " + testName);
		Thread testThread;
		switch (testName) {
		case "testThreadPark":
			testThreadParkHelper("testThreadPark NO C/R");
			testResult.lockStatus.set(0);
			testThread = testThreadPark();
			break;
		case "testThreadSleep":
			testThreadSleepHelper("testThreadSleep NO C/R");
			testResult.lockStatus.set(0);
			testThread = testThreadSleep();
			break;
		case "testObjectWaitNotify":
			testThread = testObjectWaitNotify();
			break;
		case "testObjectWaitTimedNoNanoSecond":
			testObjectWaitTimedHelper("testObjectWaitTimedNoNanoSecond NO C/R", msTime2s, 0);
			testResult.lockStatus.set(0);
			testThread = testObjectWaitTimedNoNanoSecond();
			break;
		case "testObjectWaitTimedWithNanoSecond":
			testObjectWaitTimedHelper("testObjectWaitTimedWithNanoSecond NO C/R", msTime2s, nsTime500kns);
			testResult.lockStatus.set(0);
			testThread = testObjectWaitTimedWithNanoSecond();
			break;
		default:
			throw new RuntimeException("Unrecognized test name: " + testName);
		}

		while (testResult.lockStatus.get() == 0) {
			Thread.currentThread().yield();
		}
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);

		switch (testName) {
		case "testThreadPark":
			testThreadParkHelper("testThreadPark NO C/R");
			break;
		case "testThreadSleep":
			testThreadSleepHelper("testThreadSleep NO C/R");
			break;
		case "testObjectWaitNotify":
			Thread.sleep(msTime5s);
			synchronized (objWait) {
				objWait.notify();
			}
			break;
		case "testObjectWaitTimedNoNanoSecond":
			testObjectWaitTimedHelper("testObjectWaitTimedNoNanoSecond NO C/R", msTime2s, 0);
			break;
		case "testObjectWaitTimedWithNanoSecond":
			testObjectWaitTimedHelper("testObjectWaitTimedWithNanoSecond NO C/R", msTime2s, nsTime500kns);
			break;
		default:
		}
		CRIUTestUtils.showThreadCurrentTime("After run test : " + testName);

		if (testThread != null) {
			try {
				testThread.join();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
		CRIUTestUtils.showThreadCurrentTime("End " + testName);
	}

	/**
	 * Show PASS/FAIL messages.
	 *
	 * @param logStr        A log message
	 * @param expectedTime  Expected time in ms or ns according to isMillis
	 * @param isMillis      Time in ms or ns for expectedTime
	 * @param elapsedTime   Elapsed time in ns
	 * @param startNanoTime Start time in ns
	 * @param endNanoTime   End time in ns
	 */
	public static void showMessages(String logStr, long expectedTime, boolean isMillis, long elapsedTime,
			long startNanoTime, long endNanoTime) {
		long crDeltaNs = InternalCRIUSupport.getCheckpointRestoreNanoTimeDelta();
		System.out.println(Thread.currentThread().getName() + ": " + new Date() + ", "
				+ logStr + expectedTime + " " + (isMillis ? "ms" : "ns")
				+ ", but the actual elapsed time was: " + elapsedTime + "ns (~" + (elapsedTime / NANOS_PER_MILLI)
				+ "ms) with startNanoTime = " + startNanoTime + "ns, and endNanoTime = " + endNanoTime
				+ "ns, CheckpointRestoreNanoTimeDelta: " + crDeltaNs + "ns (~" + (crDeltaNs / NANOS_PER_MILLI) + "ms)");
	}

	private void testThreadParkHelper(String testName) {
		CRIUTestUtils.showThreadCurrentTime(testName + " before park()");
		final long startNanoTime = System.nanoTime();
		testResult.lockStatus.set(1);
		unsafe.park(false, nsTime5s);
		final long endNanoTime = System.nanoTime();
		CRIUTestUtils.showThreadCurrentTime(testName + " after park()");
		final long nsElapsedTime = endNanoTime - startNanoTime;
		if (nsElapsedTime < nsTime5s) {
			showMessages("FAILED: expected park time ", nsTime5s, false, nsElapsedTime, startNanoTime, endNanoTime);
		} else {
			showMessages("PASSED: expected park time ", nsTime5s, false, nsElapsedTime, startNanoTime, endNanoTime);
		}
	}

	private Thread testThreadPark() {
		Thread parkThread = new Thread(() -> testThreadParkHelper("testThreadPark"));
		parkThread.start();
		return parkThread;
	}

	private void testThreadSleepHelper(String testName) {
		CRIUTestUtils.showThreadCurrentTime(testName + " before sleep()");
		final long startNanoTime = System.nanoTime();
		try {
			testResult.lockStatus.set(1);
			Thread.sleep(msTime5s);
			final long endNanoTime = System.nanoTime();
			CRIUTestUtils.showThreadCurrentTime(testName + " after sleep()");
			final long nsElapsedTime = endNanoTime - startNanoTime;
			if (nsElapsedTime < nsTime5s) {
				showMessages("FAILED: expected sleep time ", msTime5s, true, nsElapsedTime, startNanoTime,
						endNanoTime);
			} else {
				showMessages("PASSED: expected sleep time ", msTime5s, true, nsElapsedTime, startNanoTime,
						endNanoTime);
			}
		} catch (InterruptedException ie) {
			ie.printStackTrace();
		}
	}

	private Thread testThreadSleep() {
		Thread sleepThread = new Thread(() -> testThreadSleepHelper("testThreadSleep"));
		sleepThread.start();
		return sleepThread;
	}

	private Thread testObjectWaitNotify() {
		Thread threadWait = new Thread(new Runnable() {
			public void run() {
				CRIUTestUtils.showThreadCurrentTime("testObjectWaitNotify() before wait()");
				try {
					final long startNanoTime;
					synchronized (objWait) {
						startNanoTime = System.nanoTime();
						testResult.lockStatus.set(1);
						objWait.wait();
					}
					final long endNanoTime = System.nanoTime();
					CRIUTestUtils.showThreadCurrentTime("testObjectWaitNotify() after wait()");
					final long msElapsedTime = (endNanoTime - startNanoTime) / NANOS_PER_MILLI;
					if (msElapsedTime < msTime5s) {
						showMessages("FAILED: expected wait time ", msTime5s, true, msElapsedTime, startNanoTime,
								endNanoTime);
					} else {
						showMessages("PASSED: expected wait time ", msTime5s, true, msElapsedTime, startNanoTime,
								endNanoTime);
					}
				} catch (InterruptedException ie) {
					ie.printStackTrace();
				}
			}
		});
		threadWait.start();

		return threadWait;
	}

	private void testObjectWaitTimedHelper(String testName, long ms, int ns) {
		CRIUTestUtils.showThreadCurrentTime(testName + " before wait(" + ms + ", " + ns + ")");
		try {
			final long startNanoTime;
			synchronized (objWait) {
				startNanoTime = System.nanoTime();
				testResult.lockStatus.set(1);
				objWait.wait(ms, ns);
			}
			final long endNanoTime = System.nanoTime();
			CRIUTestUtils.showThreadCurrentTime(testName + " after wait(" + ms + ", " + ns + ")");
			final long nsElapsedTime = endNanoTime - startNanoTime;
			final long nsSleepTime = ms * NANOS_PER_MILLI + ns;
			if (nsElapsedTime < nsSleepTime) {
				showMessages("FAILED: expected wait time ", nsSleepTime, false, nsElapsedTime, startNanoTime,
						endNanoTime);
			} else {
				showMessages("PASSED: expected wait time ", nsSleepTime, false, nsElapsedTime, startNanoTime,
						endNanoTime);
			}
		} catch (InterruptedException ie) {
			ie.printStackTrace();
		}
	}

	private Thread testObjectWaitTimedNoNanoSecond() {
		Thread waitThread = new Thread(() -> testObjectWaitTimedHelper("testObjectWaitTimedNoNanoSecond", msTime5s, 0));
		waitThread.start();
		return waitThread;
	}

	private Thread testObjectWaitTimedWithNanoSecond() {
		Thread waitThread = new Thread(() -> testObjectWaitTimedHelper("testObjectWaitTimedWithNanoSecond", msTime5s, nsTime500kns));
		waitThread.start();
		return waitThread;
	}
}
