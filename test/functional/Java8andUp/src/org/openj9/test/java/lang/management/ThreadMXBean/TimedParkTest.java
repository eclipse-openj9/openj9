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
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;
import java.util.concurrent.locks.LockSupport;

import org.testng.annotations.Test;
import org.testng.asserts.SoftAssert;

/**
 * Tests {@link ThreadMXBean} thread state monitoring for {@link LockSupport}
 * parking operations, and thread contention statistics for blocking and waiting
 * states.
 */
public class TimedParkTest extends ThreadMXBeanTestCase {
	private SoftAssert softAssert = new SoftAssert();

	ThreadMXBean mxb;
	boolean isContentionMonitoringSupported = false;
	boolean isBadGettimeofdayPlatform = false;

	/*
	 * Time error tolerance: Wait timer resolution varies by platform. There may
	 * be rounding when getting the start or end wait times, and also in the
	 * timing of the wait. Add a bit more for clock skew. These issues can cause
	 * the waited time to be either longer or shorter than expected.
	 * 
	 * If the test is run without -Xthr:minimizeUserCPU, then spinning may cause
	 * the waited time to be shorter than expected.
	 * 
	 * "Occasional" early wakeup is allowed by the spec.
	 * 
	 * Also allow time for the waiting thread to become runnable again, which
	 * includes VM overhead and scheduling delay.
	 */
	long minUnderWaitMillis = 500;
	long maxOverWaitMillis = 2000;

	TimedParkTest() {
		mxb = ManagementFactory.getThreadMXBean();
		if (mxb.isThreadContentionMonitoringSupported()) {
			mxb.setThreadContentionMonitoringEnabled(true);
			isContentionMonitoringSupported = true;
		}
		isBadGettimeofdayPlatform = isBadGettimeofdayPlatform();
	}

	/**
	 * Validates {@link ThreadInfo#getWaitedTime}.
	 * 
	 * @param oldti
	 *            {@link ThreadInfo} before waiting (optional). If supplied,
	 *            this method validates (newti.getWaitedTime() -
	 *            oldti.getWaitedTime()).
	 * @param newti
	 *            {@link ThreadInfo} after waiting (mandatory).
	 * @param expMillis
	 *            Expected waited time, in milliseconds.
	 * @return true if waited time is ok, false otherwise.
	 */
	boolean isWaitedTimeOK(ThreadInfo oldti, ThreadInfo newti, long expMillis, String errMsg) {

		boolean rc = true;

		if (isContentionMonitoringSupported) {
			long waitedTime = newti.getWaitedTime();
			long waitedCount = newti.getWaitedCount();

			if (null != oldti) {
				waitedTime -= oldti.getWaitedTime();
			}

			/*
			 * Is waitedTime unreasonably short?
			 * 
			 * Timing errors can accumulate over multiple timed waits.
			 * 
			 * Since we stopped using gettimeofday for measuring waitedTime and
			 * blockedTime, we shouldn't need to skip this check for bad
			 * gettimeofday platforms.
			 */
			if (waitedTime < (expMillis - minUnderWaitMillis * waitedCount)) {
				rc = false;
				if (errMsg != null) {
					logger.error(errMsg
							+ "Waited time is lower than expected");
					logger.error("  actual: " + waitedTime
							+ " expected: " + expMillis + " tolerance: "
							+ (minUnderWaitMillis * waitedCount));
				}
			}

			/* Is waitedTime unreasonably long? */
			if (waitedTime > (expMillis + maxOverWaitMillis * waitedCount)) {
				rc = false;
				if (errMsg != null) {
					logger.error(errMsg
							+ "Waited time is higher than expected");
					logger.error("  actual: " + waitedTime
							+ " expected: " + expMillis + " tolerance: "
							+ (maxOverWaitMillis * waitedCount));
				}
			}
		}
		return rc;
	}

	/**
	 * Validates {@link ThreadInfo#getBlockedTime}.
	 * 
	 * @param oldti
	 *            {@link ThreadInfo} before blocking (optional). If supplied,
	 *            this method validates (newti.getBlockedTime() -
	 *            oldti.getBlockedTime()).
	 * @param newti
	 *            {@link ThreadInfo} after blocking (mandatory).
	 * @param expMillis
	 *            Expected blocked time, in milliseconds.
	 * @return true if blocked time is ok, false otherwise.
	 */
	boolean isBlockedTimeOK(ThreadInfo oldti, ThreadInfo newti, long expMillis, String errMsg) {

		boolean rc = true;

		if (isContentionMonitoringSupported) {
			long blockedTime = newti.getBlockedTime();
			long blockedCount = newti.getBlockedCount();

			if (null != oldti) {
				blockedTime -= oldti.getBlockedTime();
			}

			/* is blockedTime unreasonably short? */
			if (blockedTime < (expMillis - minUnderWaitMillis * blockedCount)) {
				rc = false;
				if (errMsg != null) {
					logger.error(errMsg
							+ "Blocked time is lower than expected");
					logger.error("  actual: " + blockedTime
							+ " expected: " + expMillis + " tolerance: "
							+ (minUnderWaitMillis * blockedCount));
				}
			}

			/* is blockedTime unreasonably long? */
			if (blockedTime > (expMillis + maxOverWaitMillis * blockedCount)) {
				rc = false;
				if (errMsg != null) {
					logger.error(errMsg
							+ "Blocked time is higher than expected");
					logger.error("  actual: " + blockedTime
							+ " expected: " + expMillis + " tolerance: "
							+ (maxOverWaitMillis * blockedCount));
				}
			}
		}
		return rc;
	}

	@Test(groups = { "level.extended" })
	public void testTimedParkTest() {
		if (isBadGettimeofdayPlatform) {
			logger.warn("WARNING: Bad gettimeofday() platform detected. Wait times may be incorrect.");
		}

		softAssert.assertEquals(testParkSelf(), ExitStatus.PASS);

		softAssert.assertEquals(testTimedPark(), ExitStatus.PASS);

		softAssert.assertEquals(testParkUntil(), ExitStatus.PASS);

		softAssert.assertEquals(testParkPast(), ExitStatus.PASS);

		softAssert.assertEquals(testTimedWait(), ExitStatus.PASS);

		softAssert.assertEquals(testTimedBlock(), ExitStatus.PASS);

		softAssert.assertAll();
	}

	/**
	 * Test contention stats for {@link LockSupport#parkNanos(long)} on the
	 * current thread.
	 * 
	 * @return {@link ExitStatus}
	 */
	ExitStatus testParkSelf() {
		logger.debug("== testParkSelf ==");
		ExitStatus rc = ExitStatus.PASS;
		long tid = Thread.currentThread().getId();
		long millis = 0;
		int parkcount = 0;
		ThreadInfo oldti;
		ThreadInfo newti;

		oldti = mxb.getThreadInfo(tid, 8);
		LockSupport.parkNanos(50 * 1000000);
		millis += 50;
		parkcount++;
		LockSupport.parkNanos(3 * 1000000);
		millis += 3;
		parkcount++;
		LockSupport.parkNanos(2000 * 1000000);
		millis += 2000;
		parkcount++;

		newti = mxb.getThreadInfo(tid, 8);

		if ((newti.getWaitedCount() - oldti.getWaitedCount()) < parkcount) {
			logger.error("ERROR: wrong wait count");
			rc = ExitStatus.FAIL;
		}
		if (!isWaitedTimeOK(oldti, newti, millis, "ERROR: ")) {
			rc = ExitStatus.FAIL;
		}

		if (rc != ExitStatus.PASS) {
			printThreadInfo("BEFORE PARKING", oldti);
			printThreadInfo("AFTER PARKING", newti);
		}
		return rc;
	}

	/**
	 * Test thread state and contention stats for
	 * {@link LockSupport#parkNanos(long)} on a different thread.
	 * 
	 * @return {@link ExitStatus}
	 */
	ExitStatus testTimedPark() {
		logger.debug("== testTimedPark ==");

		ExitStatus rc = ExitStatus.PASS;
		long millis = 5 * 1000;
		long nanos = millis * 1000 * 1000;
		Object exitsig = new Object();
		Thread th = new ParkWithTimeout("park 5s", nanos, exitsig);
		ThreadInfo ti;
		ThreadInfo ti2;
		ThreadInfo ti3;

		try {
			synchronized (exitsig) {
				synchronized (th) {
					th.start();

					do {
						Thread.sleep(100);
						ti = mxb.getThreadInfo(th.getId(), 8);
					} while ((ti.getThreadState() == Thread.State.NEW)
							|| (ti.getThreadState() == Thread.State.RUNNABLE));

					/* thread should still be parked */
					switch (ti.getThreadState()) {
					case TIMED_WAITING:
						/* ok */
						break;
					case BLOCKED:
						logger.warn("WARNING: 1.missed park interval");
						/* might be ok */
						break;
					default:
						rc = ExitStatus.FAIL;
						logger.error("ERROR: 1.wrong thread state");
						break;
					}

					/* wait until thread should have woken */
					Thread.sleep(millis * 2);
					ti2 = mxb.getThreadInfo(th.getId(), 8);

					/* thread should be blocked */
					switch (ti2.getThreadState()) {
					case BLOCKED:
						/* ok */
						break;
					default:
						rc = ExitStatus.FAIL;
						logger.error("ERROR: 2.wrong thread state");
						break;
					}
					th.wait();
				}

				/* thread must have definitely finished parking */
				ti3 = mxb.getThreadInfo(th.getId(), 8);
				if (ti3.getWaitedCount() != 1) {
					rc = ExitStatus.FAIL;
					logger.error("ERROR: 3.wrong waited count");
				}
				if (!isWaitedTimeOK(null, ti3, millis, "ERROR: 3.")) {
					rc = ExitStatus.FAIL;
				}

				exitsig.wait();
			}
			th.join();

			if (rc != ExitStatus.PASS) {
				printThreadInfo("DURING PARK", ti);
				printThreadInfo("AFTER PARK INTERVAL", ti2);
				printThreadInfo("AFTER WAKE FROM PARK", ti3);
			}

		} catch (InterruptedException e) {
			rc = ExitStatus.INTERRUPTED;
		}
		return rc;
	}

	/**
	 * Test thread state and contention stats for
	 * {@link LockSupport#parkUntil(long)} on a different thread.
	 * 
	 * @return {@link ExitStatus}
	 */
	ExitStatus testParkUntil() {
		logger.debug("== testParkUntil ==");

		ExitStatus rc = ExitStatus.PASS;
		Object exitsig = new Object();
		long deltaMillis = 10 * 1000;
		ParkUntil th = new ParkUntil("park 10s", deltaMillis, exitsig);
		ThreadInfo ti;
		ThreadInfo ti2;

		try {
			synchronized (exitsig) {
				synchronized (th) {
					th.start();
					do {
						Thread.sleep(100);
						ti = mxb.getThreadInfo(th.getId(), 8);
					} while ((ti.getThreadState() == Thread.State.NEW)
							|| (ti.getThreadState() == Thread.State.RUNNABLE));

					/* thread should still be parked */
					switch (ti.getThreadState()) {
					case TIMED_WAITING:
						/* ok */
						break;
					case BLOCKED:
						logger.warn("WARNING: missed park interval");
						/* might be ok */
						break;
					default:
						rc = ExitStatus.FAIL;
						logger.error("ERROR: wrong thread state");
						break;
					}

					/* wait for thread to wake */
					th.wait();
					/* check ThreadInfo after thread has finished parking */
					ti2 = mxb.getThreadInfo(th.getId(), 8);
				}
				exitsig.wait();
			}
			th.join();

			if (ti2.getWaitedCount() != 1) {
				rc = ExitStatus.FAIL;
				logger.error("ERROR: wrong waited count");
			}

			if (th.getWakeMillis() < (th.getDeadlineMillis() - minUnderWaitMillis)) {
				logger.error("ERROR: woke too early");
				if (isBadGettimeofdayPlatform) {
					logger.debug(" ignoring because this is a bad gettimeofday platform");
				} else {
					rc = ExitStatus.FAIL;
				}
			}

			if (th.getWakeMillis() > th.getDeadlineMillis()) {
				if (isContentionMonitoringSupported) {
					if (ti2.getWaitedTime() <= 0) {
						rc = ExitStatus.FAIL;
						logger.error("ERROR: expected >0 waited time");
					}
				}
			}

			/*
			 * We will warn if the waited time isn't as expected, but we won't
			 * consider it a test failure because waited time could be affected
			 * by system calendar time changes.
			 */
			if (!isWaitedTimeOK(null, ti2, deltaMillis, "WARNING: ")) {
				logger.debug("Note: The waited time may be affected by system date/time adjustments.");
			}

			if (rc != ExitStatus.PASS) {
				logger.error("deadline: " + th.getDeadlineMillis());
				logger.error("woke at:  " + th.getWakeMillis());
				printThreadInfo(ti);
				printThreadInfo(ti2);
			}
		} catch (InterruptedException e) {
			rc = ExitStatus.INTERRUPTED;
		}
		return rc;
	}

	/**
	 * Test contention stats for {@link LockSupport#parkUntil(long)} on a
	 * deadline in the past.
	 * 
	 * @return {@link ExitStatus}
	 */
	ExitStatus testParkPast() {
		logger.debug("== testParkPast ==");

		ExitStatus rc = ExitStatus.PASS;
		long tid = Thread.currentThread().getId();
		long delta = -10 * 1000;
		long now = System.currentTimeMillis();

		ThreadInfo oldti = mxb.getThreadInfo(tid, 8);
		LockSupport.parkUntil(now + delta);
		ThreadInfo newti = mxb.getThreadInfo(tid, 8);

		/* the wait count increases even if the deadline is past */
		if ((newti.getWaitedCount() - oldti.getWaitedCount()) != 1) {
			rc = ExitStatus.FAIL;
			logger.error("ERROR: wrong waited count");
		}
		if (isContentionMonitoringSupported) {
			if (newti.getWaitedTime() != oldti.getWaitedTime()) {
				rc = ExitStatus.FAIL;
				logger.error("ERROR: wrong waited time, expected " + oldti.getWaitedTime());
			}
		}

		if (rc != ExitStatus.PASS) {
			printThreadInfo(oldti);
			printThreadInfo(newti);
		}
		return rc;
	}

	/**
	 * Test contention statistics (waitedCount, waitedTime) for
	 * {@link Object#wait(long)}.
	 * 
	 * @return {@link ExitStatus}
	 */
	ExitStatus testTimedWait() {
		logger.debug("== testTimedWait ==");

		ExitStatus rc = ExitStatus.PASS;
		long tid = Thread.currentThread().getId();
		long millis = 5 * 1000;
		ThreadInfo ti;
		ThreadInfo ti2;

		try {
			synchronized (this) {
				ti = mxb.getThreadInfo(tid, 8);
				this.wait(millis);
			}
			ti2 = mxb.getThreadInfo(tid, 8);

			if (1 != (ti2.getWaitedCount() - ti.getWaitedCount())) {
				rc = ExitStatus.FAIL;
				logger.error("ERROR: wrong waited count");
			}
			if (!isWaitedTimeOK(ti, ti2, millis, "ERROR: ")) {
				rc = ExitStatus.FAIL;
			}

			if (rc != ExitStatus.PASS) {
				printThreadInfo("BEFORE WAIT", ti);
				printThreadInfo("AFTER WAIT", ti2);
			}

		} catch (InterruptedException e) {
			rc = ExitStatus.INTERRUPTED;
		}
		return rc;
	}

	/**
	 * Test contention statistics (blockedCount, blockedTime) for blocking on a
	 * monitor.
	 * 
	 * @return {@link ExitStatus}
	 */
	ExitStatus testTimedBlock() {
		logger.debug("== testTimedBlock ==");

		ExitStatus rc = ExitStatus.PASS;
		long millis = 5 * 1000;
		Object exitSig = new Object();
		Inflater inflater = new Inflater("inflater", exitSig);
		Blocker th = new Blocker("block 5s", exitSig);
		ThreadInfo[] ti;

		try {
			/*
			 * The monitor must be inflated so that the test thread doesn't
			 * spend time spinning before blocking.
			 */
			inflater.init();

			synchronized (exitSig) {
				/*
				 * Start test thread and wait for it to be created. Thread
				 * creation can be slow. We want to avoid including it in the
				 * intended blocking time.
				 */
				synchronized (th) {
					th.start();
					th.wait();
				}

				/* park holding the monitor, forcing the test thread to block */
				LockSupport.parkNanos(millis * 1000 * 1000);
			}

			/* release monitor and wait for test thread to exit */
			th.join();
			ti = th.getThreadInfo();

			if (1 != (ti[1].getBlockedCount() - ti[0].getBlockedCount())) {
				rc = ExitStatus.FAIL;
				logger.error("ERROR: wrong blocked count");
			}
			if (!isBlockedTimeOK(ti[0], ti[1], millis, "ERROR: ")) {
				rc = ExitStatus.FAIL;
			}

			if (rc != ExitStatus.PASS) {
				printThreadInfo("BEFORE BLOCKING", ti[0]);
				printThreadInfo("AFTER BLOCKING", ti[1]);
			}

			/* terminate the inflater */
			inflater.finish();
		} catch (InterruptedException e) {
			rc = ExitStatus.INTERRUPTED;
		}
		return rc;
	}

	/**
	 * Helper thread for {@link TimedParkTest#testTimedPark()}, which does
	 * {@link LockSupport#parkNanos(long)}.
	 */
	class ParkWithTimeout extends Thread {
		long nanos = 0;

		Object exitsig;

		ParkWithTimeout(String name, long nanos, Object exitsig) {
			super(name);
			this.nanos = nanos;
			this.exitsig = exitsig;
		}

		public void run() {
			LockSupport.parkNanos(nanos);
			synchronized (this) {
				this.notify();
			}
			synchronized (exitsig) {
				exitsig.notify();
			}
		}
	}

	/**
	 * Helper thread for {@link TimedParkTest#testParkUntil()}, which does
	 * {@link LockSupport#parkUntil(long)}. It records the intended deadline and
	 * the actual wake time.
	 */
	class ParkUntil extends Thread {
		long deltaMillis = -1;

		/** park deadline, calculated by (now + deltaMillis) */
		long deadlineMillis = -1;

		/** time when thread wakes from parkUntil() */
		long wakeMillis = 0;

		Object exitsig;

		/**
		 * @param name
		 *            thread name
		 * @param deltaMillis
		 *            set the park deadline to (now + deltaMillis)
		 * @param exitsig
		 *            monitor to hold the thread alive so that the main test
		 *            thread can collect its {@link ThreadMXBean.ThreadInfo}
		 */
		ParkUntil(String name, long deltaMillis, Object exitsig) {
			super(name);
			this.deltaMillis = deltaMillis;
			this.exitsig = exitsig;
		}

		public void run() {
			deadlineMillis = System.currentTimeMillis() + deltaMillis;
			LockSupport.parkUntil(deadlineMillis);
			wakeMillis = System.currentTimeMillis();
			synchronized (this) {
				this.notify();
			}
			synchronized (exitsig) {
				exitsig.notify();
			}
		}

		public long getWakeMillis() {
			return wakeMillis;
		}

		public long getDeadlineMillis() {
			return deadlineMillis;
		}
	}

	/**
	 * Helper thread for {@link TimedParkTest#testTimedBlock()}. Blocks on a
	 * monitor, and gathers contention statistics from before and after the
	 * blocking operation.
	 */
	class Blocker extends Thread {
		/** collected contention statistics */
		ThreadInfo[] ti = new ThreadInfo[2];

		/** monitor to block on */
		Object exitSig;

		long tid = this.getId();

		/**
		 * @param name
		 *            thread name
		 * @param exitSig
		 *            monitor to block on
		 */
		Blocker(String name, Object exitSig) {
			super(name);
			this.exitSig = exitSig;
		}

		/**
		 * Blocks on exitSig.
		 * 
		 * @see java.lang.Thread#run()
		 */
		public void run() {
			synchronized (this) {
				this.notify();
			}
			ti[0] = mxb.getThreadInfo(tid, 8);
			synchronized (exitSig) {
				ti[1] = mxb.getThreadInfo(tid, 8);
			}
		}

		/**
		 * Returns {@link ThreadInfo}, containing contention statistics,
		 * collected before and after blocking.
		 * 
		 * @return {@link ThreadInfo}
		 */
		public ThreadInfo[] getThreadInfo() {
			return ti;
		}
	}

	/**
	 * Helper thread for {@link TimedParkTest#testTimedBlock()}. Inflates a
	 * given monitor by waiting on it.
	 */
	class Inflater extends Thread {
		/** monitor to inflate */
		Object mon;

		/** flag indicating thread has acquired the monitor */
		volatile boolean gotMon = false;

		/**
		 * Create Inflater object.
		 * 
		 * @param name
		 *            thread name
		 * @param mon
		 *            monitor to inflate
		 */
		Inflater(String name, Object mon) {
			super(name);
			this.mon = mon;
		}

		/**
		 * Start the inflater thread and wait for it to inflate the monitor.
		 * 
		 * @throws InterruptedException
		 */
		public void init() throws InterruptedException {
			this.start();
			synchronized (mon) {
				while (false == gotMon) {
					mon.wait(1000);
				}
			}
		}

		/**
		 * Unblock the inflater thread and wait for it to terminate.
		 * 
		 * @throws InterruptedException
		 */
		public void finish() throws InterruptedException {
			synchronized (mon) {
				mon.notifyAll();
			}
			this.join();
		}

		/**
		 * Inflater thread waits indefinitely on the monitor to be inflated.
		 * 
		 * @see java.lang.Thread#run()
		 */
		public void run() {
			try {
				synchronized (mon) {
					gotMon = true;
					mon.wait();
				}
			} catch (InterruptedException e) {
				System.out
						.println("ERROR: inflater was interrupted. monitor may be deflated.");
			}
		}
	}
}
