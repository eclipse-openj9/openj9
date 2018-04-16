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

import org.testng.asserts.SoftAssert;

/* getThreadInfo() for a thread that's not started. */
public class JoiningThread extends ThreadMXBeanTestCase {
	private SoftAssert softAssert = new SoftAssert();

	public void runJoiningThreadTest() {
		ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
		LiveThread liver = new LiveThread("liver");
		Joiner joiner = new Joiner("joiner", liver);
		TimedJoiner timedjoiner = new TimedJoiner("tbone", liver);
		ThreadInfo jinfo = null;
		ThreadInfo tjinfo = null;

		if (mxb.isThreadContentionMonitoringSupported()) {
			mxb.setThreadContentionMonitoringEnabled(true);
		}

		try {
			/* wait for liver to start */
			logger.debug("liver starting");
			synchronized (liver) {
				liver.start();
				liver.wait();
			}

			/* test my waited counts and times */
			int failures = 0;
			long tid = Thread.currentThread().getId();
			ThreadInfo oldinfo = mxb.getThreadInfo(tid);
			
			logger.debug("main thread timed join on liver");
			liver.join(300);
			
			ThreadInfo newinfo = mxb.getThreadInfo(tid);
			if ((newinfo.getBlockedCount() - oldinfo.getBlockedCount()) != 0) {
				logger.debug("bad blocked count");
				failures++;
			}
			if (mxb.isThreadContentionMonitoringEnabled()) {
				if ((newinfo.getBlockedTime() - oldinfo.getBlockedTime()) != 0) {
					logger.debug("bad blocked time");
					failures++;
				}
			}
			if ((newinfo.getWaitedCount() - oldinfo.getWaitedCount()) <= 0) {
				logger.debug("bad waited count");
				failures++;
			}
			if (mxb.isThreadContentionMonitoringEnabled()) {
				if ((newinfo.getWaitedTime() - oldinfo.getWaitedTime()) <= 0) {
					logger.debug("bad waited time");
					failures++;
				}
			}
			
			if (failures > 0) {
				printThreadInfo(oldinfo);
				printThreadInfo(newinfo);
				softAssert.fail();
			}

			/* test other threads' waiting states */
			failures = 0;
			
			/* 
			 * start the joining threads separately so that there is no contention
			 * over liver's object lock.
			 */
			logger.debug("joiner starting");
			joiner.start();

			/* wait up to 10s for joiner to get into wait state */
			boolean waitForJoinerTimedOut = false;
			for (int i = 0; i < 10; i++) {
				jinfo = mxb.getThreadInfo(joiner.getId(), 8);
				if (jinfo != null) {
					if ((jinfo.getThreadState() != Thread.State.NEW)
							&& (jinfo.getThreadState() != Thread.State.RUNNABLE)) {
						break;
					}
				}
				Thread.sleep(1000);
				if (i == 10) {
					waitForJoinerTimedOut = true;
				}
			}

			logger.debug("timed joiner starting");
			timedjoiner.start();
			
			boolean waitForTimedJoinerTimedOut = false;
			for (int i = 0; i < 10; i++) {
				tjinfo = mxb.getThreadInfo(timedjoiner.getId(), 8);
				if (tjinfo != null) {
					if ((tjinfo.getThreadState() != Thread.State.NEW)
							&& (tjinfo.getThreadState() != Thread.State.RUNNABLE)) {
						break;
					}
				}
				Thread.sleep(700);
				if (i == 10) {
					waitForTimedJoinerTimedOut = true;
				}
			}
			
			/* This test is still a little flaky. If something else is busying the system,
			 * it's possible that the joining threads may not reach a waiting state within
			 * 10 seconds. The polling thread may also miss the windows during which the
			 * timed joining thread is waiting.
			 */

			if (jinfo == null) {
				logger.debug("joiner: no ThreadInfo");
				failures++;
			} else if (jinfo.getThreadState() != Thread.State.WAITING) {
				if (waitForJoinerTimedOut) {
					logger.debug("WARNING: wait for joiner timed out");
				} else {
					logger.debug("joiner: wrong thread state");
					failures++;
				}
			}

			if (tjinfo == null) {
				logger.debug("timedjoiner: no ThreadInfo");
				failures++;
			} else if (tjinfo.getThreadState() != Thread.State.TIMED_WAITING) {
				if (waitForTimedJoinerTimedOut) {
					logger.debug("WARNING: wait for timed joiner timed out");
				} else {
					logger.debug("timedjoiner: wrong thread state");
					failures++;
				}
			}

			if ((failures > 0) || waitForJoinerTimedOut || waitForTimedJoinerTimedOut) {
				printThreadInfo(jinfo);
				printThreadInfo(tjinfo);
				if (failures > 0) {
					softAssert.fail();
				}
			}

			logger.debug("shutting down test");
			liver.interrupt();
			logger.debug("waiting for liver");
			liver.join();
			logger.debug("waiting for joiner");
			joiner.join();
			logger.debug("waiting for timedjoiner");
			timedjoiner.interrupt();
			timedjoiner.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
			softAssert.fail("ExitStatus.INTERRUPTED");
		}
		softAssert.assertAll();
	}

	class LiveThread extends Thread {
		LiveThread(String name) {
			super(name);
		}

		public void run() {
			try {
				synchronized (this) {
					this.notifyAll();
				}
				while (true) {
					Thread.sleep(10 * 1000);
				}
			} catch (InterruptedException e) {
			}
		}
	}

	class Joiner extends Thread {
		Thread target;

		Joiner(String name, Thread target) {
			super(name);
			this.target = target;
		}

		public void run() {
			try {
				target.join();
			} catch (InterruptedException e) {
			}
		}
	}

	class TimedJoiner extends Thread {
		Thread target;

		TimedJoiner(String name, Thread target) {
			super(name);
			this.target = target;
		}

		public void run() {
			try {
				while (true) {
					target.join(1000, 100);
					if (Thread.interrupted()) {
						break;
					}
				}
			} catch (InterruptedException e) {
			}
		}
	}
}
