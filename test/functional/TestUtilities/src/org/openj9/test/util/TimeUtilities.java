/*
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
 */
package org.openj9.test.util;

import java.time.Instant;
import java.util.ArrayList;
import java.util.Date;
import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Utility class to check time.
 */
public class TimeUtilities {
	public final static long TIME_NANOSECONDS_PER_MILLIS = 1000000;

	public static void showThreadCurrentTime(String msgHeader) {
		System.out.println(msgHeader + ", current thread name: " + Thread.currentThread().getName() + ", " + new Date()
				+ ", System.currentTimeMillis() " + System.currentTimeMillis() + ", System.nanoTime() "
				+ System.nanoTime());
	}

	public static boolean checkElapseTime(String testName, long startMillisTime, long startNanoTime,
			long minElapsedMillisTime, long maxElapsedMillisTime, long minElapsedNanoTimeInMillis,
			long maxElapsedNanoTimeInMillis) {
		boolean result = true;
		final long currentMillisTime = System.currentTimeMillis();
		final long currentNanoTime = System.nanoTime();
		showThreadCurrentTime(testName + " checkElapseTime starts");

		final long elapsedMillisTime = currentMillisTime - startMillisTime;
		final long elapsedNanoTime = currentNanoTime - startNanoTime;
		// Get the smallest value that is greater than or equal to the elapsed nano time
		// in milliseconds for the case that the elapsed nano
		// time is slightly less than the elapsedMillisTime, such as the elapsedNanoTime
		// (1999997603ns) less than elapsedMillisTime * TIME_NANOSECONDS_PER_MILLIS
		// (2000000000ns)
		// This behaviour was observed in both RI and OpenJ9.
		final long elapsedNanoTimeInMillis = elapsedNanoTime / TIME_NANOSECONDS_PER_MILLIS
				+ ((elapsedNanoTime % TIME_NANOSECONDS_PER_MILLIS == 0) ? 0 : 1);

		System.out.println(testName + ": startMillisTime (" + startMillisTime + "ms) startNanoTime (" + startNanoTime
				+ "ns)" + " currentMillisTime (" + currentMillisTime + "ms) currentNanoTime (" + currentNanoTime
				+ "ns) elapsedMillisTime (" + elapsedMillisTime + "ms) elapsedNanoTime (" + elapsedNanoTime + "ns)");
		if (elapsedMillisTime < minElapsedMillisTime) {
			result = false;
			System.out.println("FAILED: " + testName + " elapsedMillisTime (" + elapsedMillisTime
					+ "ms) should NOT be less than minElapsedMillisTime (" + minElapsedMillisTime + "ms)");
		} else if ((maxElapsedMillisTime > 0) && (elapsedMillisTime > maxElapsedMillisTime)) {
			result = false;
			System.out.println("FAILED: " + testName + " elapsedMillisTime (" + elapsedMillisTime
					+ "ms) should NOT be greater than maxElapsedMillisTime (" + maxElapsedMillisTime + "ms)");
		}
		if (elapsedNanoTimeInMillis < minElapsedNanoTimeInMillis) {
			result = false;
			System.out.println("FAILED: " + testName + " elapsedNanoTimeInMillis (" + elapsedNanoTimeInMillis
					+ "ms) should NOT be less than minElapsedNanoTimeInMillis (" + minElapsedNanoTimeInMillis + "ms)");
		} else if ((maxElapsedNanoTimeInMillis > 0) && (elapsedNanoTimeInMillis > maxElapsedNanoTimeInMillis)) {
			result = false;
			System.out.println("FAILED: " + testName + " elapsedNanoTimeInMillis (" + elapsedNanoTimeInMillis
					+ "ms) should NOT be greater than maxElapsedNanoTimeInMillis (" + maxElapsedNanoTimeInMillis
					+ "ms)");
		}
		showThreadCurrentTime(testName + " checkElapseTime ends");

		return result;
	}

	// The maximum elapsed time is not always guaranteed on some platforms.
	// https://github.com/eclipse-openj9/openj9/issues/18487#issuecomment-1829111868
	public static boolean checkElapseTime(String testName, long startMillisTime, long startNanoTime,
			long minElapsedMillisTime, long minElapsedNanoTimeInMillis) {
		return checkElapseTime(testName, startMillisTime, startNanoTime, minElapsedMillisTime, 0, minElapsedNanoTimeInMillis, 0);
	}

	public static long getCurrentTimeInNanoseconds() {
		Instant instant = Instant.now();
		return TimeUnit.SECONDS.toNanos(instant.getEpochSecond()) + instant.getNano();
	}

	private volatile boolean tasksPassed = true;
	private AtomicInteger taskRunning = new AtomicInteger(0);
	private AtomicInteger taskStarted = new AtomicInteger(0);
	private AtomicInteger taskExecuted = new AtomicInteger(0);
	private final ArrayList<Timer> timers = new ArrayList<Timer>();

	public void timerSchedule(String testName, long startMillisTime, long startNanoTime, long minElapsedMillisTime,
			long maxElapsedMillisTime, long minElapsedNanoTimeInMillis, long maxElapsedNanoTimeInMillis, long delay) {
		Timer timer = new Timer(testName);
		timer.schedule(new TimeTestTask(testName, startMillisTime, startNanoTime, minElapsedMillisTime,
				maxElapsedMillisTime, minElapsedNanoTimeInMillis, maxElapsedNanoTimeInMillis), delay);
		timers.add(timer);
	}

	public boolean getResultAndCancelTimers() {
		showThreadCurrentTime("getResultAndCancelTimers before Thread.yield() taskRunning = " + taskRunning.get()
				+ ", taskStarted: " + taskStarted.get() + ", taskExecuted: " + taskExecuted.get());
		while (taskRunning.get() > 0) {
			Thread.yield();
		}

		showThreadCurrentTime("getResultAndCancelTimers before timer.cancel() taskRunning = " + taskRunning.get()
		+ ", taskStarted: " + taskStarted.get() + ", taskExecuted: " + taskExecuted.get());
		for (Timer timer : timers) {
			timer.cancel();
		}

		showThreadCurrentTime("TimeTestTask tasksPassed: " + tasksPassed + ", taskStarted: " + taskStarted.get()
				+ ", taskExecuted: " + taskExecuted.get());
		return tasksPassed && (taskStarted.get() == taskExecuted.get());
	}

	class TimeTestTask extends TimerTask {

		private final String testName;
		private final long startMillisTime;
		private final long startNanoTime;
		private final long minElapsedMillisTime;
		private final long maxElapsedMillisTime;
		private final long minElapsedNanoTimeInMillis;
		private final long maxElapsedNanoTimeInMillis;

		TimeTestTask(String testName, long startMillisTime, long startNanoTime, long minElapsedMillisTime,
				long maxElapsedMillisTime, long minElapsedNanoTimeInMillis, long maxElapsedNanoTimeInMillis) {
			this.testName = testName;
			this.startMillisTime = startMillisTime;
			this.startNanoTime = startNanoTime;
			this.minElapsedMillisTime = minElapsedMillisTime;
			this.maxElapsedMillisTime = maxElapsedMillisTime;
			this.minElapsedNanoTimeInMillis = minElapsedNanoTimeInMillis;
			this.maxElapsedNanoTimeInMillis = maxElapsedNanoTimeInMillis;

			taskRunning.incrementAndGet();
			taskStarted.incrementAndGet();
		}

		public void run() {
			taskExecuted.incrementAndGet();
			if (!checkElapseTime(testName, startMillisTime, startNanoTime, minElapsedMillisTime, minElapsedNanoTimeInMillis)) {
				tasksPassed = false;
			}
			taskRunning.decrementAndGet();
			showThreadCurrentTime("TimeTestTask.run() taskRunning = " + taskRunning.get() + ", taskStarted: " + taskStarted.get()
					+ ", taskExecuted: " + taskExecuted.get());
		}
	}
}
