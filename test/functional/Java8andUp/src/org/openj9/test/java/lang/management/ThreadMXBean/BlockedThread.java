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

import org.testng.Assert;
import org.testng.annotations.Optional;
import org.testng.annotations.Parameters;
import org.testng.annotations.Test;

public class BlockedThread extends ThreadMXBeanTestCase {
	@Parameters( {"timeoutPara", "repeatcountPara"} )
	@Test(groups = { "level.extended" })
	public void testBlockedThread(@Optional("900") String timeoutPara, @Optional("100") String repeatcountPara) {
		int timeoutSec = Integer.parseInt(timeoutPara);
		int repeatCount = Integer.parseInt(repeatcountPara);

		if (timeoutSec < 1) {
			timeoutSec = 1;
		}
		if (repeatCount < 0) {
			repeatCount = 0;
		}

		TestThread thr = new TestThread(repeatCount);
		thr.start();

		ExitStatus status;
		try {
			thr.join(timeoutSec * 1000);
			if (thr.isAlive()) {
				status = ExitStatus.TIMEDOUT;
				thr.interrupt();
				thr.join();
			} else {
				status = getExitStatus();
			}
		} catch (InterruptedException e) {
			status = ExitStatus.INTERRUPTED;
		}
		
		Assert.assertEquals(status, ExitStatus.PASS);
	}

	ExitStatus runTestLoop(int repeatCount) {
		ExitStatus status = ExitStatus.PASS;

		for (int i = 0; i < repeatCount; i++) {
			status = runTestOnce();
			if (status != ExitStatus.PASS)
				break;
		}
		return status;
	}

	ExitStatus runTestOnce() {
		ExitStatus status = ExitStatus.PASS;
		ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
		ThreadInfo info = null;
		boolean done = false;
		Object lockerStart = new Object();
		Object lockerExit = new Object();
		Object lock = new Object();

		Locker locker = new Locker(lockerStart, lockerExit, lock);
		Blocker blocker = new Blocker(lock);

		/* start locker */
		try {
			synchronized (lockerStart) {
				locker.start();
				lockerStart.wait();
			}
		} catch (InterruptedException e) {
			return ExitStatus.INTERRUPTED;
		}
		logger.debug("Locker started");

		/* start blocker */
		blocker.start();

		/* wait for blocker to become BLOCKED */
		done = false;
		while (!done) {
			info = mxb.getThreadInfo(blocker.getId());
			if ((info != null) && (info.getThreadState() != Thread.State.NEW)
					&& (info.getThreadState() != Thread.State.RUNNABLE)) {
				done = true;
			}
			if (Thread.interrupted()) {
				return ExitStatus.INTERRUPTED;
			}
		}
		logger.debug("Blocker started");

		/* blocker must be BLOCKED on lock */
		if (info == null) {
			status = ExitStatus.FAIL;
		} else if (info.getThreadState() != Thread.State.BLOCKED) {
			status = ExitStatus.FAIL;
		} else if (!lock.toString().equals(info.getLockName())) {
			status = ExitStatus.FAIL;
		} else if (info.getLockOwnerId() != locker.getId()) {
			status = ExitStatus.FAIL;
		}
		if (status != ExitStatus.PASS) {
			printThreadInfo(info);
			return status;
		}

		/* tell locker to release */
		synchronized (lockerExit) {
			lockerExit.notify();
		}

		/* wait for blocker to unblock from lock */
		done = false;
		while (!done) {
			info = mxb.getThreadInfo(blocker.getId());
			if ((info == null)
					|| (info.getThreadState() != Thread.State.BLOCKED)
					|| (!lock.toString().equals(info.getLockName()))
					|| (info.getLockOwnerId() != locker.getId())) {
				done = true;
			}
			if (Thread.interrupted()) {
				return ExitStatus.INTERRUPTED;
			}
		}
		logger.debug("Blocker unblocked");

		/*
		 * Check blocker's thread info again.
		 */
		if (info == null) {
			status = ExitStatus.PASS;
		} else if (info.getThreadState() == Thread.State.TERMINATED) {
			status = ExitStatus.PASS;
		} else if (info.getThreadState() == Thread.State.RUNNABLE) {
			if (info.getLockOwnerId() != -1) {
				status = ExitStatus.FAIL;
			} else if (info.getLockName() != null) {
				status = ExitStatus.FAIL;
			} else {
				status = ExitStatus.PASS;
			}
		} else if (info.getThreadState() == Thread.State.BLOCKED) {
			if (lock.toString().equals(info.getLockName())) {
				status = ExitStatus.FAIL;
			}
		}

		if (status != ExitStatus.PASS) {
			printThreadInfo(info);
			return status;
		}

		try {
			locker.join();
			blocker.join();
		} catch (InterruptedException e) {
			/* do nothing */
		}
		return status;
	}

	class TestThread extends Thread {
		int repeatCount;

		TestThread(int repeatCount) {
			super("TestThread");
			this.repeatCount = repeatCount;
		}

		public void run() {
			ExitStatus status = runTestLoop(this.repeatCount);
			setExitStatus(status);
		}
	}

	class Locker extends Thread {
		Object startSig;

		Object exitSig;

		Object lock;

		Locker(Object startSig, Object exitSig, Object lock) {
			super("Locker");
			this.lock = lock;
			this.startSig = startSig;
			this.exitSig = exitSig;
		}

		public void run() {
			synchronized (lock) {
				synchronized (exitSig) {
					synchronized (startSig) {
						startSig.notify();
					}

					try {
						exitSig.wait();
					} catch (InterruptedException e) {
						/* do nothing */
					}
				}
			}
		}
	}

	class Blocker extends Thread {
		Object blockOn;

		Blocker(Object lock) {
			super("Blocker");
			this.blockOn = lock;
		}

		public void run() {
			synchronized (blockOn) {
				for (int i = 0; i < 100; i++)
					;
			}
		}
	}
}
