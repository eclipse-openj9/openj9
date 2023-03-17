/*******************************************************************************
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package org.openj9.criu;

import jdk.internal.misc.Unsafe;
import openj9.internal.criu.InternalCRIUSupport;

public class JDK11UpTimeoutAdjustmentTest {
	private static final long MILLIS_PER_SECOND = 1000L;
	private static final long NANOS_PER_MILLI = 1000_000L;
	private static final long NANOS_PER_SECOND = 1000_000_000L;

	private static Unsafe unsafe = Unsafe.getUnsafe();

	public static void main(String[] args) throws InterruptedException {
		if (args.length == 0) {
			throw new RuntimeException("Test name required");
		} else {
			String testName = args[0];
			new JDK11UpTimeoutAdjustmentTest().test(testName);
		}
	}

	private static Object objWait = new Object();
	// 5s time in ms
	private static final long msWaitNotify5s = 5 * MILLIS_PER_SECOND;

	private void test(String testName) throws InterruptedException {
		System.out.println("Start test name: " + testName);
		CRIUTestUtils.showThreadCurrentTime("Before starting " + testName);
		switch (testName) {
		case "testThreadPark":
			testThreadParkHelper("testThreadPark NO C/R");
			testThreadPark();
			break;
		case "testThreadSleep":
			testThreadParkHelper("testThreadSleep NO C/R");
			testThreadSleep();
			break;
		case "testObjectWaitNotify":
			testObjectWaitNotify();
			break;
		case "testObjectWaitTimedV1":
			testObjectWaitTimedHelper("testObjectWaitTimedV1 NO C/R", msSleepTime10s, 0);
			testObjectWaitTimedV1();
			break;
		case "testObjectWaitTimedV2":
			testObjectWaitTimedHelper("testObjectWaitTimedV2 NO C/R", msSleepTime10s, 500000);
			testObjectWaitTimedV2();
			break;
		default:
			throw new RuntimeException("Unrecognized test name: " + testName);
		}

		// delay 1s to allow the test thread start before checkPointJVM()
		Thread.sleep(1 * MILLIS_PER_SECOND);
		CRIUTestUtils.checkPointJVM(CRIUTestUtils.imagePath, false);

		switch (testName) {
		case "testThreadPark":
			testThreadParkHelper("testThreadPark NO C/R");
			break;
		case "testThreadSleep":
			testThreadSleepHelper("testThreadSleep NO C/R");
			break;
		case "testObjectWaitNotify":
			Thread.sleep(msWaitNotify5s);
			CRIUTestUtils.showThreadCurrentTime("Before objWait.notify()");
			synchronized (objWait) {
				objWait.notify();
			}
			Thread.sleep(5 * MILLIS_PER_SECOND);
			break;
		case "testObjectWaitTimedV1":
			testObjectWaitTimedHelper("testObjectWaitTimedV1", msSleepTime10s, 0);
			break;
		case "testObjectWaitTimedV2":
			testObjectWaitTimedHelper("testObjectWaitTimedV2", msSleepTime10s, 500000);
			break;
		default:
		}

		// maximum test running time is 12s, sleep another 2s
		Thread.sleep(2 * MILLIS_PER_SECOND);
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
		System.out.println(logStr + expectedTime + " " + (isMillis ? "ms" : "ns")
				+ ", but the actual elapsed time was: " + elapsedTime + "ns (~" + (elapsedTime / NANOS_PER_MILLI)
				+ "ms) with startNanoTime = " + startNanoTime + "ns, and endNanoTime = " + endNanoTime
				+ "ns, CheckpointRestoreNanoTimeDelta: " + crDeltaNs + "ns (~" + (crDeltaNs / NANOS_PER_MILLI) + "ms)");
	}

	// 10s parkt time in ns
	private static final long nsParkTime10s = 10 * NANOS_PER_SECOND;

	private void testThreadParkHelper(String testName) {
		CRIUTestUtils.showThreadCurrentTime(testName + " before park()");
		final long startNanoTime = System.nanoTime();
		unsafe.park(false, nsParkTime10s);
		final long endNanoTime = System.nanoTime();
		final long elapsedTime = endNanoTime - startNanoTime;
		boolean pass = false;
		if (elapsedTime >= nsParkTime10s) {
			pass = true;
		}
		CRIUTestUtils.showThreadCurrentTime(testName + " after park()");
		if (pass) {
			showMessages("PASSED: expected park time ", nsParkTime10s, false, elapsedTime, startNanoTime, endNanoTime);
		} else {
			showMessages("FAILED: expected park time ", nsParkTime10s, false, elapsedTime, startNanoTime, endNanoTime);
		}
	}

	private void testThreadPark() {
		new Thread(() -> testThreadParkHelper("testThreadPark")).start();
	}

	// 10s sleep time in ms
	private static final long msSleepTime10s = 10 * MILLIS_PER_SECOND;

	private void testThreadSleepHelper(String testName) {
		CRIUTestUtils.showThreadCurrentTime(testName + " before sleep()");
		final long startNanoTime = System.nanoTime();
		try {
			Thread.sleep(msSleepTime10s);
			boolean pass = false;
			final long endNanoTime = System.nanoTime();
			final long elapsedTime = endNanoTime - startNanoTime;
			if (elapsedTime >= msSleepTime10s) {
				pass = true;
			}
			CRIUTestUtils.showThreadCurrentTime(testName + " after sleep()");
			if (pass) {
				showMessages("PASSED: expected sleep time ", msSleepTime10s, true, elapsedTime, startNanoTime,
						endNanoTime);
			} else {
				showMessages("FAILED: expected sleep time ", msSleepTime10s, true, elapsedTime, startNanoTime,
						endNanoTime);
			}
		} catch (InterruptedException ie) {
			ie.printStackTrace();
		}
	}

	private void testThreadSleep() {
		new Thread(() -> testThreadParkHelper("testThreadSleep")).start();
	}

	private void testObjectWaitNotify() {
		Thread threadWait = new Thread(new Runnable() {
			public void run() {
				CRIUTestUtils.showThreadCurrentTime("testObjectWaitNotify() before wait()");
				try {
					final long startNanoTime = System.nanoTime();
					synchronized (objWait) {
						objWait.wait();
					}
					boolean pass = false;
					final long endNanoTime = System.nanoTime();
					final long elapsedTime = endNanoTime - startNanoTime;
					if (elapsedTime >= msWaitNotify5s) {
						pass = true;
					}
					CRIUTestUtils.showThreadCurrentTime("testObjectWaitNotify() after wait()");
					if (pass) {
						showMessages("PASSED: expected wait time ", msWaitNotify5s, true, elapsedTime, startNanoTime,
								endNanoTime);
					} else {
						showMessages("FAILED: expected wait time ", msWaitNotify5s, true, elapsedTime, startNanoTime,
								endNanoTime);
					}
				} catch (InterruptedException ie) {
					ie.printStackTrace();
				}
			}
		});
		threadWait.setDaemon(true);
		threadWait.start();
	}

	private void testObjectWaitTimedHelper(String testName, long ms, int ns) {
		CRIUTestUtils.showThreadCurrentTime(testName + " before wait(" + ms + ", " + ns + ")");
		try {
			final long startNanoTime = System.nanoTime();
			synchronized (objWait) {
				objWait.wait(ms, ns);
			}
			boolean pass = false;
			final long endNanoTime = System.nanoTime();
			final long elapsedTime = endNanoTime - startNanoTime;
			final long nsSleepTime = ms * NANOS_PER_MILLI + ns;
			if (elapsedTime >= nsSleepTime) {
				pass = true;
			}
			CRIUTestUtils.showThreadCurrentTime(testName + " after wait(" + ms + ", " + ns + ")");
			if (pass) {
				showMessages("PASSED: expected wait time ", nsSleepTime, false, elapsedTime, startNanoTime,
						endNanoTime);
			} else {
				showMessages("FAILED: expected wait time ", nsSleepTime, false, elapsedTime, startNanoTime,
						endNanoTime);
			}
		} catch (InterruptedException ie) {
			ie.printStackTrace();
		}
	}

	private void testObjectWaitTimedV1() {
		new Thread(() -> testObjectWaitTimedHelper("testObjectWaitTimedV1", msSleepTime10s, 0)).start();
	}

	private void testObjectWaitTimedV2() {
		new Thread(() -> testObjectWaitTimedHelper("testObjectWaitTimedV2", msSleepTime10s, 500000)).start();
	}
}
