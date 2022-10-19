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
 * [2] http://openjdk.java.net/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
package org.openj9.criu;

import jdk.internal.misc.Unsafe;
import openj9.internal.criu.InternalCRIUSupport;

public class JDK11UpTimeoutAdjustmentTest {
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
	private static final long msWaitNotify5s = 5000;

	private void test(String testName) throws InterruptedException {
		System.out.println("Start test name: " + testName);
		CRIUTestUtils.showThreadCurrentTime("Before starting " + testName);
		switch (testName) {
		case "testThreadPark":
			testThreadPark();
			break;
		case "testThreadSleep":
			testThreadSleep();
			break;
		case "testObjectWaitNotify":
			testObjectWaitNotify();
			break;
		case "testObjectWaitTimedV1":
			testObjectWaitTimedV1();
			break;
		case "testObjectWaitTimedV2":
			testObjectWaitTimedV2();
			break;
		default:
			throw new RuntimeException("Unrecognized test name: " + testName);
		}
		// delay 1s to allow the test thread start before checkPointJVM()
		Thread.sleep(1000);
		CRIUTestUtils.checkPointJVM(CRIUTestUtils.imagePath, false);
		if ("testObjectWaitNotify".equalsIgnoreCase(testName)) {
			Thread.sleep(msWaitNotify5s);
			CRIUTestUtils.showThreadCurrentTime("Before objWait.notify()");
			synchronized (objWait) {
				objWait.notify();
			}
		}
		// maximum test running time is 12s
		Thread.sleep(12000);
		CRIUTestUtils.showThreadCurrentTime("End " + testName);
	}

	public static void showMessages(String logStr, long expectedTime, boolean isMillis, long elapsedTime,
			long startNanoTime, long endNanoTime) {
		System.out.println(logStr + expectedTime + " " + (isMillis ? "ms" : "ns")
				+ ", but the actual elapsed time was: " + elapsedTime + (isMillis ? "ms" : "ns") + " with startNanoTime = " + startNanoTime
				+ ", and endNanoTime = " + endNanoTime + ", CheckpointRestoreNanoTimeDelta: "
				+ InternalCRIUSupport.getCheckpointRestoreNanoTimeDelta());
	}

	// 10s parkt time in ns
	private static final long nsParkTime10s = 10000000000L;

	private void testThreadPark() {
		Thread threadPark = new Thread(new Runnable() {
			public void run() {
				CRIUTestUtils.showThreadCurrentTime("testThreadPark() before park()");
				final long startNanoTime = System.nanoTime();
				unsafe.park(false, nsParkTime10s);
				final long endNanoTime = System.nanoTime();
				final long elapsedTime = endNanoTime - startNanoTime;
				boolean pass = false;
				if (elapsedTime >= nsParkTime10s) {
					pass = true;
				}
				CRIUTestUtils.showThreadCurrentTime("testThreadPark() after park()");
				if (pass) {
					showMessages("PASSED: expected park time ", nsParkTime10s, false, elapsedTime, startNanoTime,
							endNanoTime);
				} else {
					showMessages("FAILED: expected park time ", nsParkTime10s, false, elapsedTime, startNanoTime,
							endNanoTime);
				}
			}
		});
		threadPark.setDaemon(false);
		threadPark.start();
	}

	// 10s sleep time in ms
	private static final long msSleepTime10s = 10000;

	private void testThreadSleep() {
		Thread threadSleep = new Thread(new Runnable() {
			public void run() {
				CRIUTestUtils.showThreadCurrentTime("testThreadSleep() before sleep()");
				final long startNanoTime = System.nanoTime();
				try {
					Thread.sleep(msSleepTime10s);
					boolean pass = false;
					final long endNanoTime = System.nanoTime();
					final long elapsedTime = endNanoTime - startNanoTime;
					if (elapsedTime >= msSleepTime10s) {
						pass = true;
					}
					CRIUTestUtils.showThreadCurrentTime("testThreadSleep() after sleep()");
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
		});
		threadSleep.start();
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

	private void testObjectWaitTimedV1() {
		Thread threadWait = new Thread(new Runnable() {
			public void run() {
				CRIUTestUtils.showThreadCurrentTime("testObjectWaitTimedV1() before wait(ms)");
				try {
					final long startNanoTime = System.nanoTime();
					synchronized (objWait) {
						objWait.wait(msSleepTime10s);
					}
					boolean pass = false;
					final long endNanoTime = System.nanoTime();
					final long elapsedTime = (endNanoTime - startNanoTime) / 1000000;
					if (elapsedTime >= msSleepTime10s) {
						pass = true;
					}
					CRIUTestUtils.showThreadCurrentTime("testObjectWaitTimedV1() after wait(ms)");
					if (pass) {
						showMessages("PASSED: expected wait time ", msSleepTime10s, true, elapsedTime, startNanoTime,
								endNanoTime);
					} else {
						showMessages("FAILED: expected wait time ", msSleepTime10s, true, elapsedTime, startNanoTime,
								endNanoTime);
					}
				} catch (InterruptedException ie) {
					ie.printStackTrace();
				}
			}
		});
		threadWait.start();
	}

	private void testObjectWaitTimedV2() {
		Thread threadWait = new Thread(new Runnable() {
			public void run() {
				CRIUTestUtils.showThreadCurrentTime("testObjectWaitTimedV2() before wait(ms)");
				try {
					final long startNanoTime = System.nanoTime();
					synchronized (objWait) {
						objWait.wait(msSleepTime10s, 500000);
					}
					boolean pass = false;
					final long endNanoTime = System.nanoTime();
					final long elapsedTime = endNanoTime - startNanoTime;
					if (elapsedTime >= (msSleepTime10s * 1000000 + 500000)) {
						pass = true;
					}
					CRIUTestUtils.showThreadCurrentTime("testObjectWaitTimedV2() after wait(ms)");
					if (pass) {
						showMessages("PASSED: expected wait time ", msSleepTime10s, true, elapsedTime, startNanoTime,
								endNanoTime);
					} else {
						showMessages("FAILED: expected wait time ", msSleepTime10s, true, elapsedTime, startNanoTime,
								endNanoTime);
					}
				} catch (InterruptedException ie) {
					ie.printStackTrace();
				}
			}
		});
		threadWait.start();
	}
}
