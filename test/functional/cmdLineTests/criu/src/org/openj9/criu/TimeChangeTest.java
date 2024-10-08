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

import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Date;
import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.TimeUnit;

import openj9.internal.criu.InternalCRIUSupport;
import org.eclipse.openj9.criu.CRIUSupport;

import org.openj9.test.util.PlatformInfo;
import org.openj9.test.util.TimeUtilities;
import org.testng.AssertJUnit;

public class TimeChangeTest {

	// maximum tardiness - 4 second
	private final static long MAX_TARDINESS_MS = 4000;
	private final static long MAX_TARDINESS_NS = MAX_TARDINESS_MS * 1000 * 1000;
	// delay 100 millisecond
	private final static long MILLIS_DELAY_BEFORECHECKPOINTDONE = 100;
	// delay 5000 millisecond
	private final static long MILLIS_DELAY_AFTERCHECKPOINTDONE = 5000;
	private final long startNanoTime = System.nanoTime();
	private final long currentTimeMillis = System.currentTimeMillis();
	private static final TestResult testResult = new TestResult(true, 0);

	public static void main(String args[]) throws InterruptedException {
		if (args.length == 0) {
			throw new RuntimeException("Test name required");
		} else {
			String testName = args[0];
			TimeChangeTest tct = new TimeChangeTest();
			switch (testName) {
			case "testGetLastRestoreTime":
				tct.testGetLastRestoreTime();
				break;
			case "testMXBeanUpTime":
				tct.testMXBeanUpTime();
				break;
			case "testSystemNanoTime":
				tct.testSystemNanoTime();
				break;
			case "testSystemNanoTimeJitPreCheckpointCompile":
				tct.testSystemNanoTimeJitPreCheckpointCompile();
				break;
			case "testSystemNanoTimeJitPostCheckpointCompile":
				tct.testSystemNanoTimeJitPostCheckpointCompile();
				break;
			case "testTimeCompensation":
				if (PlatformInfo.isS390()) {
					System.out.println("PASSED: testTimeCompensation - disabled temporarily for s390");
				} else {
					tct.testTimeCompensation();
				}
				break;
			default:
				// timer related tests
				tct.test(testName);
			}
		}
	}

	private void test(String testName) throws InterruptedException {
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		System.out.println("Start test name: " + testName);
		CRIUTestUtils.showThreadCurrentTime("Before starting " + testName);
		Timer timer = new Timer();
		switch (testName) {
		case "testMillisDelayBeforeCheckpointDone":
			// Timer.schedule(TimerTask task, long delay)
			timer.schedule(
					new TimeCompensationTimerTask(testName, true, startNanoTime, MILLIS_DELAY_BEFORECHECKPOINTDONE),
					MILLIS_DELAY_BEFORECHECKPOINTDONE);
			break;
		case "testMillisDelayAfterCheckpointDone":
			// Timer.schedule(TimerTask task, long delay)
			timer.schedule(
					new TimeCompensationTimerTask(testName, true, startNanoTime, MILLIS_DELAY_AFTERCHECKPOINTDONE),
					MILLIS_DELAY_AFTERCHECKPOINTDONE);
			break;
		case "testDateScheduledBeforeCheckpointDone":
			// expect the checkpoint takes longer than 10ms
			final long timeMillisScheduledBeforeCheckpointDone = currentTimeMillis + 10;
			// Timer.schedule(TimerTask task, Date time)
			timer.schedule(new TimeCompensationTimerTask(testName, false, 0, timeMillisScheduledBeforeCheckpointDone),
					new Date(timeMillisScheduledBeforeCheckpointDone));
			break;
		case "testDateScheduledAfterCheckpointDone":
			// expect the Checkpoint takes less than 2000ms
			final long timeMillisScheduledAfterCheckpointDone = currentTimeMillis + 2000;
			// Timer.schedule(TimerTask task, Date time)
			timer.schedule(new TimeCompensationTimerTask(testName, false, 0, timeMillisScheduledAfterCheckpointDone),
					new Date(timeMillisScheduledAfterCheckpointDone));
			break;
		default:
			throw new RuntimeException("Unrecognized test name: " + testName);
		}
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);

		while (testResult.lockStatus.get() == 0) {
			Thread.currentThread().yield();
		}

		timer.cancel();
		CRIUTestUtils.showThreadCurrentTime("End " + testName);
	}

	private void testTimeCompensation() throws InterruptedException {
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		if (criu == null) {
			// "CRIU is not enabled" is to appear and cause the test failure.
			return;
		}
		CRIUTestUtils.showThreadCurrentTime("testTimeCompensation() starts");
		final TimeUtilities tu = new TimeUtilities();
		long millisTimeStart = System.currentTimeMillis();
		long nanoTimeStart = System.nanoTime();

		millisTimeStart = System.currentTimeMillis();
		nanoTimeStart = System.nanoTime();
		Thread.currentThread().sleep(100);
		TimeUtilities.checkElapseTime("testTimeCompensation() sleep 100ms", millisTimeStart, nanoTimeStart, 100, 100);

		// this TimerTask is to run before CRIUR checkpoint
		tu.timerSchedule("testTimeCompensation() preCheckpoint timer delayed 100ms", System.currentTimeMillis(),
				System.nanoTime(), 100, 500, 100, 500, 100);

		// this TimerTask is to run before CRIUR checkpoint
		tu.timerSchedule("testTimeCompensation() preCheckpoint timer delayed 2s", System.currentTimeMillis(),
				System.nanoTime(), 2000, 3000, 2000, 3000, 2000);

		// this TimerTask is to run after CRIUR restore
		tu.timerSchedule("testTimeCompensation() preCheckpoint timer delayed 4s", System.currentTimeMillis(),
				System.nanoTime(), 6000, 10000, 4000, 8000, 4000);

		final long preCheckpointMillisTime = System.currentTimeMillis();
		final long preCheckpointNanoTime = System.nanoTime();

		Thread.currentThread().sleep(2000);

		criu.registerPreCheckpointHook(new Runnable() {
			public void run() {
				// check time elapsed within preCheckpoint hook
				TimeUtilities.checkElapseTime("testTimeCompensation() preCheckpointHook", preCheckpointMillisTime,
						preCheckpointNanoTime, 2000, 2000);

				// scheduled task can't run before the checkpoint because all threads are halted
				// in single threaded mode except the current thread performing CRIU checkpoit
				tu.timerSchedule("testTimeCompensation()() preCheckpointHook timer delayed 10ms",
						System.currentTimeMillis(), System.nanoTime(), 2000, 8000, 10, 500, 10);
				tu.timerSchedule("testTimeCompensation() preCheckpointHook timer delayed 4s",
						System.currentTimeMillis(), System.nanoTime(), 6000, 10000, 4000, 4500, 4000);
			}
		});
		// assuming 2s between CRIU checkpoint and restore at criuScript.sh
		criu.registerPostRestoreHook(new Runnable() {
			public void run() {
				// check time elapsed within postRestore hook
				TimeUtilities.checkElapseTime("testTimeCompensation() postRestoreHook", preCheckpointMillisTime,
						preCheckpointNanoTime, 4000, 2000);

				tu.timerSchedule("testTimeCompensation() postRestoreHook timer delayed 10ms",
						System.currentTimeMillis(), System.nanoTime(), 10, 800, 9, 800, 10);
				tu.timerSchedule("testTimeCompensation() postRestoreHook timer delayed 2s", System.currentTimeMillis(),
						System.nanoTime(), 2000, 3000, 2000, 3000, 2000);
			}
		});

		TimeUtilities.checkElapseTime("testTimeCompensation() preCheckpoint", preCheckpointMillisTime,
				preCheckpointNanoTime, 2000, 2000);

		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);

		TimeUtilities.checkElapseTime("testTimeCompensation() after CRIU restore", preCheckpointMillisTime,
				preCheckpointNanoTime, 2000, 2000);

		CRIUTestUtils.showThreadCurrentTime("testTimeCompensation() postRestore");
		final long postRestoreMillisTime = System.currentTimeMillis();
		final long postRestoreNanoTime = System.nanoTime();

		// following two timer tasks are expected to run after taking a checkpoint
		tu.timerSchedule("testTimeCompensation() postRestore timer delayed 100ms", System.currentTimeMillis(),
				System.nanoTime(), 100, 800, 100, 800, 100);

		tu.timerSchedule("testTimeCompensation() postRestore timer delayed 2s", System.currentTimeMillis(),
				System.nanoTime(), 2000, 3000, 2000, 3000, 2000);

		Thread.currentThread().sleep(2000);

		TimeUtilities.checkElapseTime("testTimeCompensation() postRestore", postRestoreMillisTime, postRestoreNanoTime,
				2000, 2000);

		if (tu.getResultAndCancelTimers()) {
			System.out.println("PASSED: " + "testTimeCompensation");
		} else {
			System.out.println("FAILED: " + "testTimeCompensation");
		}
	}

	public void testSystemNanoTime() {
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		final long beforeCheckpoint = System.nanoTime();
		System.out.println("System.nanoTime() before CRIU checkpoint: " + beforeCheckpoint);
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		final long afterRestore = System.nanoTime();
		final long elapsedTime = afterRestore - beforeCheckpoint;
		if (elapsedTime < MAX_TARDINESS_NS) {
			System.out.println("PASSED: System.nanoTime() after CRIU restore: " + afterRestore
					+ ", the elapse time is: " + elapsedTime + " ns");
		} else {
			System.out.println("FAILED: System.nanoTime() after CRIU restore: " + afterRestore
					+ ", the elapse time is: " + elapsedTime + " ns, w/ MAX_TARDINESS_NS : " + MAX_TARDINESS_NS);
		}
	}

	private void testGetLastRestoreTime() {
		long lastRestoreTime = InternalCRIUSupport.getLastRestoreTime();
		if (lastRestoreTime != -1) {
			System.out.println("FAILED: InternalCRIUSupport.getLastRestoreTime() - " + lastRestoreTime
					+ " should be -1 before restore");
		}
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		long beforeCheckpoint = TimeUtilities.getCurrentTimeInNanoseconds();
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		lastRestoreTime = InternalCRIUSupport.getLastRestoreTime();
		long afterRestore = TimeUtilities.getCurrentTimeInNanoseconds();
		if (beforeCheckpoint >= lastRestoreTime) {
			System.out.println("FAILED: InternalCRIUSupport.getLastRestoreTime() - " + lastRestoreTime
					+ " can't be less than the beforeCheckpoint time - " + beforeCheckpoint);
		} else if (lastRestoreTime > afterRestore) {
			System.out.println("FAILED: InternalCRIUSupport.getLastRestoreTime() - " + lastRestoreTime
					+ " can't be greater than the afterRestore time - " + afterRestore);
		} else {
			System.out.println("PASSED: InternalCRIUSupport.getLastRestoreTime() - " + lastRestoreTime
					+ " is between beforeCheckpoint time - " + beforeCheckpoint + " and afterRestore time - "
					+ afterRestore);
		}
	}

	private void testMXBeanUpTime() {
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		RuntimeMXBean mxb = ManagementFactory.getRuntimeMXBean();
		long uptimeBeforeCheckpoint = mxb.getUptime();
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		long uptimeAfterCheckpoint = mxb.getUptime();

		if (uptimeAfterCheckpoint <= uptimeBeforeCheckpoint) {
			System.out.println("FAILED: testMXBeanUpTime() - uptimeAfterCheckpoint " + uptimeAfterCheckpoint
					+ " can't be less than uptimeBeforeCheckpoint " + uptimeBeforeCheckpoint);
		} else {
			System.out.println("PASSED: testMXBeanUpTime() - uptimeAfterCheckpoint " + uptimeAfterCheckpoint
					+ " is greater than uptimeBeforeCheckpoint " + uptimeBeforeCheckpoint);
		}
	}

	public void testSystemNanoTimeJitPreCheckpointCompile() {
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		testSystemNanoTimeJitTestPreCheckpointPhase();
		testSystemNanoTimeJitWarmupPhase();
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		testSystemNanoTimeJitTestPostCheckpointPhase();
	}

	public void testSystemNanoTimeJitPostCheckpointCompile() {
		CRIUSupport criu = CRIUTestUtils.prepareCheckPointJVM(CRIUTestUtils.imagePath);
		testSystemNanoTimeJitTestPreCheckpointPhase();
		CRIUTestUtils.checkPointJVMNoSetup(criu, CRIUTestUtils.imagePath, false);
		testSystemNanoTimeJitWarmupPhase();
		testSystemNanoTimeJitTestPostCheckpointPhase();
	}

	private static void testSystemNanoTimeJitWarmupPhase() {
		for (int i = 0; i < 100000; i++) {
			nanoTimeJit();
		}
	}

	private void testSystemNanoTimeJitTestPreCheckpointPhase() {
		final long beforeCheckpoint = nanoTimeInt();
		System.out.println("System.nanoTime() before CRIU checkpoint: " + beforeCheckpoint);
	}

	private void testSystemNanoTimeJitTestPostCheckpointPhase() {
		final long afterRestoreJit = nanoTimeJit();
		final long afterRestoreInt = nanoTimeInt();
		if (afterRestoreJit <= afterRestoreInt) {
			System.out.println("PASSED: System.nanoTime() after CRIU restore: interpreter nanotime = " + afterRestoreInt + ", JIT nanotime = " + afterRestoreJit);
		} else {
			System.out.println("FAILED: System.nanoTime() after CRIU restore: interpreter nanotime = " + afterRestoreInt + ", JIT nanotime = " + afterRestoreJit);
		}
	}

	/*
	 * In order to test the JIT handling of System.nanoTime() these two methods,
	 * nanoTimeInt() and nanoTimeJit(), need special treatment.
	 * 1) nanoTimeInt() must not be compiled or inlined to ensure that when called it
	 * will invoke the non-JIT implementation of System.nanoTime().
	 * 2) nanoTimeJit() must not be inlined so that it can be warmed up effectively.
	 * The above is currently accomplished via Xjit exclude= and dontInline= options
	 * when the test is executed.
	 */
	private static long nanoTimeInt() {
		return System.nanoTime();
	}

	private static long nanoTimeJit() {
		return System.nanoTime();
	}

	private static class TimeCompensationTimerTask extends TimerTask {
		private final String testName;
		// true - Timer.schedule(TimerTask task, long delay)
		// false - Timer.schedule(TimerTask task, Date time)
		private final boolean isDelay;
		// ignored if isDelay is false
		private final long startNanoTime;
		// if isDelay is true, this is the expected elapsed time since startNanoTime
		// if isDelay is false, this is the expected time to run the timer task
		// this is in milliseconds
		private final long expectedTime;

		TimeCompensationTimerTask(String testName, boolean isDelay, long startNanoTime, long expectedTime) {
			this.testName = testName;
			this.isDelay = isDelay;
			this.startNanoTime = startNanoTime;
			this.expectedTime = expectedTime;
		}

		public void run() {
			if (isDelay) {
				CRIUTestUtils.showThreadCurrentTime(testName);
				final long endNanoTime = System.nanoTime();
				final long elapsedTime = TimeUnit.NANOSECONDS.toMillis(endNanoTime - startNanoTime);
				boolean pass = false;
				if (elapsedTime >= expectedTime) {
					pass = true;
				}
				if (pass) {
					System.out.println("PASSED: expected " + expectedTime + " ms, the actual elapsed time is: "
							+ elapsedTime + " ms");
				} else {
					System.out.println("FAILED: expected " + expectedTime + " ms, but the actual elapsed time is: "
							+ elapsedTime + " ms");
				}
			} else {
				CRIUTestUtils.showThreadCurrentTime(testName);
				final long taskRunTimeMillis = System.currentTimeMillis();
				boolean pass = false;
				if (taskRunTimeMillis >= expectedTime) {
					pass = true;
				}
				if (pass) {
					System.out.println("PASSED: expected to run after expectedTime " + expectedTime
							+ " ms, the actual taskRunTimeMillis is: " + taskRunTimeMillis + " ms");
				} else {
					System.out.println("FAILED: expected to run after expectedTime " + expectedTime
							+ " ms, but the actual taskRunTimeMillis is: " + taskRunTimeMillis + " ms");
				}
			}
			testResult.lockStatus.set(1);
		}
	}
}
