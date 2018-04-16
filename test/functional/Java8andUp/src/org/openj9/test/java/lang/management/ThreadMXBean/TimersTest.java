/*******************************************************************************
 * Copyright (c) 2017, 2018 IBM Corp. and others
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
import java.lang.management.ThreadMXBean;

import org.testng.Assert;
import org.testng.annotations.Optional;
import org.testng.annotations.Parameters;
import org.testng.annotations.Test;

public class TimersTest extends ThreadMXBeanTestCase {
	static final String notSupported = "TimersTest thread cpuTime not supported";
	static final String nowEnabled = "TimersTest thread cpuTime now enabled";
	static final String nowDisabled = "TimersTest thread cpuTime now disabled";
	static final String noThreads = "Null test - no threads started";
	static final String tenThreads = "Null test - 10 threads started";
	static final String badCpuTime = "Error: bad Cpu time ";
	static final String badUserTime = "Error: bad User time ";
	static final String timerTestStart = "Starting timer test for thread id ";

	int threadCount = 3;
	long msecDelay = 300;
	long threadMsecDelay = 900;
	int repeatCount = 2;
	ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
	Thread[] t = new Thread[10];
	long startWallClockTime = 0;
	final long[] id = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	boolean supportsUserTimes = false;
	boolean ignoreLinuxVersionCheck = false;
	
	@Parameters( {"threadcountPara", "msecdelayPara", "repeatcountPara", "ignoreCheckPara"} )
	@Test(groups = { "level.extended" })
	public void testTimersTest(@Optional("") String threadcountPara, @Optional("") String msecdelayPara, @Optional("") String repeatcountPara, @Optional("") String ignoreCheckPara) {
		Object exitBarrier = new Object();

		/* Check the parameters */
		if (!threadcountPara.equals("")) {
			threadCount = Integer.parseInt(threadcountPara);
			if (threadCount == 0) {
				logger.debug(TimersTest.noThreads);
				return;
			}
			if (threadCount > 10) {
				logger.debug(TimersTest.tenThreads);
				threadCount = 10;
			}
		}
		
		if (!msecdelayPara.equals("")) {
			msecDelay = Integer.parseInt(msecdelayPara);
			threadMsecDelay = msecDelay * 3;
		}
		
		if (!repeatcountPara.equals("")) {
			repeatCount = Integer.parseInt(repeatcountPara);
		}
		
		if (repeatcountPara.equals("true")) {
			/* ignore checks for unsupported functionality */
			ignoreLinuxVersionCheck = true;
		}	

		/* Check that thread cpu time is supported and enabled. */
		if (!mxb.isThreadCpuTimeSupported()
				|| !mxb.isCurrentThreadCpuTimeSupported()) {
			logger.debug(TimersTest.notSupported);
			return;
		}

		String osName = System.getProperty("os.name").toLowerCase();
		if (osName.contains("linux")) {
			String osVersion = System.getProperty("os.version");
			String osArch = System.getProperty("os.arch").toLowerCase();
			
			logger.debug("Linux Info: " + osName + " " + osArch + " " + osVersion);

			if (osArch.contains("x86") || osArch.contains("amd64")) {
				if (!isLinuxVersionPostRedhat5(osVersion)) {
					logger.debug(osName + " " + osArch + " " + osVersion + " does not report correct CPU times.");
					if (ignoreLinuxVersionCheck) {
						logger.debug("... ignoring warning. Continuing.");
					} else {
						logger.debug("Terminating test.");
						return;
					}
				}
			}
		}
		if (osName.contains("win") || osName.contains("aix")) {
			supportsUserTimes = true;
		}
		logger.debug("os.name: " + osName + " supportsUserTimes: "
				+ supportsUserTimes);

		/* Get the wall clock time. And wait a bit. */
		startWallClockTime = System.currentTimeMillis();
		try {
			Thread.sleep(msecDelay);
		} catch (InterruptedException e) {
			/* Doesn't matter if it interrupts earlier than expected. */
		}
		
		/* Enable thread timer functions */
		if (!mxb.isThreadCpuTimeEnabled()) {
			logger.debug(TimersTest.nowEnabled);
			mxb.setThreadCpuTimeEnabled(true);
		}

		synchronized (exitBarrier) {
			/* Start the threads. */
			for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
				t[threadIndex] = new ATimerThread(threadIndex, exitBarrier);
				id[threadIndex] = t[threadIndex].getId();
				t[threadIndex].start();
				if (mxb.getThreadInfo(id[threadIndex]) == null) {
					this.setFailed();
				}
			}

			for (int i = 0; i < repeatCount; i++) {
				logger.debug("TimersTest mainTimerLoop repeat " + i);
				for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
					logger.debug("TimersTest mainTimerLoop thread id = "
							+ id[threadIndex] + " index = " + threadIndex);

					if (this.mainTimerLoop(id, threadIndex) == false) {
						this.setFailed();
						break;
					}
				}

			}
		}

		/* wait for child threads to stop */
		for (int threadIndex = 0; threadIndex < threadCount; threadIndex++) {
			try {
				t[threadIndex].join();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
		
		/* Disable thread timer functions */
		if (mxb.isThreadCpuTimeEnabled()) {
			logger.debug(TimersTest.nowDisabled);
			mxb.setThreadCpuTimeEnabled(false);
		}

		Assert.assertEquals(this.getExitStatus(), ExitStatus.PASS);
	}

	/**
	 * Validate the user and total CPU times for a given thread.
	 * 
	 * @param id
	 * @param threadIndex
	 * @return pass or fail
	 */
	public boolean mainTimerLoop(long[] id, int threadIndex) {
		boolean retCode = true;
		long currentWallClockTime;
		long elapsedWallClockTime;
		long userTime;
		long cpuTime;

		logger.debug(TimersTest.timerTestStart + id[threadIndex]);

		for (long ii = 0; ii < 100000; ii++) {
			int jj = 0;
			jj++;
		}
		try {
			Thread.sleep(msecDelay);
		} catch (InterruptedException e) {
			/* Doesn't matter if it interrupts earlier than expected. */
		}

		logger.debug("TimersTest mainTimerLoop: awakened from 1st sleep.");

		userTime = mxb.getThreadUserTime(id[threadIndex]);
		for (long ii = 0; ii < 100000; ii++) {
			int jj = 0;
			jj++;
		}

		cpuTime = mxb.getThreadCpuTime(id[threadIndex]);
		for (long ii = 0; ii < 100000; ii++) {
			int jj = 0;
			jj++;
		}

		currentWallClockTime = System.currentTimeMillis();
		elapsedWallClockTime = currentWallClockTime - startWallClockTime;
		elapsedWallClockTime *= 1000 * 1000; /* convert to ns */

		retCode = areTimesCorrect("TimersTest mainTimeLoop: thread Id = "
				+ id[threadIndex], userTime, cpuTime, elapsedWallClockTime);

		/* Allow a delay at end of thread timer test before starting next. */
		for (long ii = 0; ii < 100000; ii++) {
			int jj = 0;
			jj++;
		}

		return retCode;
	}

	class ATimerThread extends Thread {
		Object exitBarrier;
		boolean retCode;

		ATimerThread(int index, Object exitBarrier) {
			super("TimersTest child " + index);
			this.exitBarrier = exitBarrier;
		}

		@Override
		public void run() {
			for (long ii = 0; ii < 100000; ii++) {
				int jj = 0;
				jj++;
			}

			/* Check cpu and user times for the current thread. */
			for (int i = 0; i < repeatCount; i++) {
				logger.debug("TimersTest threadTimerLoop repeat " + i
						+ " for thread id " + this.getId());
				if (this.threadTimerLoop() == false) {
					setFailed();
				}
			}

			/* Keep the thread alive until the main thread is done with it */
			synchronized (exitBarrier) {
				exitBarrier.notify();
			}
		}

		/**
		 * Validate the user and total CPU times for the current thread.
		 * 
		 * @return pass or fail
		 */
		public boolean threadTimerLoop() {
			boolean retCode = true;
			long threadCurrentWallClockTime;
			long threadElapsedWallClockTime;
			long threadUserTime = 0;
			long threadCpuTime = 0;

			/*
			 * Force a big delay so that GetThreadTimes() for Windows can get a
			 * non-zero user time.
			 */
			for (long ii = 0; ii < 100000; ii++) {
				int jj = 0;
				jj++;
			}
			try {
				Thread.sleep(threadMsecDelay);
			} catch (InterruptedException e) {
			}

			logger.debug("TimersTest threadTimerLoop: thread id = "
					+ this.getId() + " awakened from 1st sleep.");

			threadUserTime = mxb.getCurrentThreadUserTime();
			for (long ii = 0; ii < 100000; ii++) {
				int jj = 0;
				jj++;
			}

			threadCpuTime = mxb.getCurrentThreadCpuTime();
			for (long ii = 0; ii < 100000; ii++) {
				int jj = 0;
				jj++;
			}

			threadCurrentWallClockTime = System.currentTimeMillis();
			threadElapsedWallClockTime = threadCurrentWallClockTime
					- startWallClockTime;
			threadElapsedWallClockTime *= 1000 * 1000; /* convert to ns */

			retCode = areTimesCorrect(
					"TimersTest threadTimerLoop: thread id = " + this.getId(),
					threadUserTime, threadCpuTime, threadElapsedWallClockTime);

			for (long ii = 0; ii < 100000; ii++) {
				int jj = 0;
				jj++;
			}

			return retCode;
		}
	}

	/**
	 * Validate times for a given thread. All times should be in nanoseconds.
	 * 
	 * @param msgPrefix
	 * @param userTime
	 * @param totalTime
	 * @param wallTime
	 * @return
	 */
	private boolean areTimesCorrect(String msgPrefix, long userTime,
			long totalTime, long wallTime) {
		boolean ok = true;

		logger.debug(msgPrefix + " user(ns): " + userTime + " total(ns): "
				+ totalTime + " wall(ns): " + wallTime);

		/* Total time must not be more than wall time */
		if (totalTime > wallTime) {
			logger.error(msgPrefix + " " + TimersTest.badCpuTime
					+ " cpuTime(ns) = " + totalTime
					+ " elapsedWallClockTime(ns) = " + wallTime);
			ok = false;
		}
		/* User time must not be more than total time */
		if (userTime > totalTime) {
			logger.error(msgPrefix + " " + TimersTest.badUserTime
					+ " userTime(ns) = " + userTime + " cpuTime(ns) = "
					+ totalTime);
			ok = false;
		}

		/*
		 * On Win32 and AIX, user time must be valid. On other platforms, -1 is
		 * acceptable.
		 */
		if (supportsUserTimes) {
			if (userTime == -1) {
				logger.error(msgPrefix + " " + TimersTest.badUserTime
						+ userTime);
				ok = false;
			}
		}

		return ok;
	}
}
