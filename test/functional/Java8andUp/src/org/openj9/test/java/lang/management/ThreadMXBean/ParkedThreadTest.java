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

import java.lang.management.ThreadMXBean;
import java.lang.management.ThreadInfo;
import java.lang.management.ManagementFactory;
import java.util.Date;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.locks.*;

import org.testng.annotations.Test;
import org.testng.asserts.SoftAssert;
import org.testng.log4testng.Logger;

public class ParkedThreadTest extends ThreadMXBeanTestCase {
	private SoftAssert softAssert = new SoftAssert();

	public static enum ExitStatus { PASS, FAIL }

	ThreadMXBean mxb;

	void printTestDesc() {
		Throwable t = new Throwable();
		t.fillInStackTrace();
		logger.debug("Test: " + t.getStackTrace()[1].getMethodName());
	}
	
	void printResult(ExitStatus ok) {
		if (ok == ExitStatus.PASS) {
			logger.debug("PASSED");
		} else {
			logger.error("FAILED");
		}
	}

	ParkedThreadTest() {
		mxb = ManagementFactory.getThreadMXBean();
		if (mxb.isThreadContentionMonitoringSupported()) {
			mxb.setThreadContentionMonitoringEnabled(true);
		}
		if (mxb.isThreadCpuTimeSupported()) {
			mxb.setThreadCpuTimeEnabled(true);
		}
	}
	
	@Test(groups = { "level.extended" })
	public void testParkedThreadTest() {
		softAssert.assertEquals(testParkOnUnownedSynchronizer(), ExitStatus.PASS);
		softAssert.assertEquals(testParkNoBlocker(), ExitStatus.PASS);
		softAssert.assertEquals(testParkOnObject(), ExitStatus.PASS);
		softAssert.assertEquals(testParkOnCondition(), ExitStatus.PASS);
		softAssert.assertEquals(testParkOnOwnableSynchronizer(), ExitStatus.PASS);
		softAssert.assertEquals(testLockOwnableSynchronizer(), ExitStatus.PASS);
		softAssert.assertAll();
	}

	ExitStatus testParkNoBlocker() {
		ExitStatus rc = ExitStatus.PASS;
		ThreadInfo tinfo;
		Thread th = new ParkNoTimeout("park no blocker", null);
		th.start();
		printTestDesc();

		try {
			do {
				Thread.sleep(1000);
				tinfo = mxb.getThreadInfo(th.getId());
			} while ((tinfo.getThreadState() == Thread.State.NEW)
					|| (tinfo.getThreadState() == Thread.State.RUNNABLE));

			/*
			 * now that we know the thread is parked, wait and re-read the
			 * threadinfo so that the waited time should increase.
			 */
			Thread.sleep(1000);
			tinfo = mxb.getThreadInfo(th.getId(), 8);
			if (tinfo.getThreadState() != Thread.State.WAITING) {
				logger.error("wrong state");
				rc = ExitStatus.FAIL;
			}
			if (tinfo.getWaitedCount() <= 0) {
				logger.error("wrong wait count");
				rc = ExitStatus.FAIL;
			}
			if (mxb.isThreadContentionMonitoringEnabled()) {
				if (tinfo.getWaitedTime() <= 0) {
					logger.error("wrong wait time");
					rc = ExitStatus.FAIL;
				}
			}
			if (tinfo.getLockName() != null) {
				logger.error("wrong blocking object");
				rc = ExitStatus.FAIL;
			}
			if (tinfo.getLockOwnerId() != -1) {
				logger.error("wrong lock owner");
				rc = ExitStatus.FAIL;
			}

			if (rc != ExitStatus.PASS) {
				printThreadInfo(tinfo);
			}
			LockSupport.unpark(th);
			th.join();

		} catch (InterruptedException e) {
			e.printStackTrace();
			rc = ExitStatus.FAIL;
		}

		printResult(rc);
		return rc;
	}

	ExitStatus testParkOnObject() {
		ExitStatus rc = ExitStatus.PASS;
		ThreadInfo tinfo;
		Object obj = new Object();

		synchronized (obj) {
			Thread th = new ParkNoTimeout("park object", obj);
			th.start();
			printTestDesc();

			try {
				Thread.sleep(2000);
				tinfo = mxb.getThreadInfo(th.getId());

				do {
					Thread.sleep(1000);
					tinfo = mxb.getThreadInfo(th.getId());
				} while ((tinfo.getThreadState() == Thread.State.NEW)
						|| (tinfo.getThreadState() == Thread.State.RUNNABLE));

				/*
				 * now that we know the thread is parked, wait and re-read the
				 * threadinfo so that the waited time should increase.
				 */
				Thread.sleep(1000);
				tinfo = mxb.getThreadInfo(th.getId(), 8);
				if (tinfo.getThreadState() != Thread.State.WAITING) {
					logger.error("wrong state");
					rc = ExitStatus.FAIL;
				}
				if (tinfo.getWaitedCount() <= 0) {
					logger.error("wrong wait count");
					rc = ExitStatus.FAIL;
				}
				if (mxb.isThreadContentionMonitoringEnabled()) {
					if (tinfo.getWaitedTime() <= 0) {
						logger.error("wrong wait time");
						rc = ExitStatus.FAIL;
					}
				}
				if (!obj.toString().equals(tinfo.getLockName())) {
					logger.error("wrong blocking object");
					rc = ExitStatus.FAIL;
				}
				if (tinfo.getLockOwnerId() != -1) {
					logger.error("wrong lock owner");
					rc = ExitStatus.FAIL;
				}

				if (rc != ExitStatus.PASS) {
					printThreadInfo(tinfo);
				}

				LockSupport.unpark(th);
				th.join();
			} catch (InterruptedException e) {
				e.printStackTrace();
				rc = ExitStatus.FAIL;
			}
		}

		printResult(rc);
		return rc;
	}

	ExitStatus testParkOnCondition() {
		ExitStatus rc = ExitStatus.PASS;
		ThreadInfo tinfo;
		MyCondition cond = new MyCondition();
		synchronized (cond) {
			Thread th = new ParkNoTimeout("park cond", cond);
			th.start();
			printTestDesc();
			try {
				Thread.sleep(2000);
				tinfo = mxb.getThreadInfo(th.getId());

				do {
					Thread.sleep(1000);
					tinfo = mxb.getThreadInfo(th.getId());
				} while ((tinfo.getThreadState() == Thread.State.NEW)
						|| (tinfo.getThreadState() == Thread.State.RUNNABLE));

				/*
				 * now that we know the thread is parked, wait and re-read the
				 * threadinfo so that the waited time should increase.
				 */
				Thread.sleep(1000);
				tinfo = mxb.getThreadInfo(th.getId(), 8);
				if (tinfo.getThreadState() != Thread.State.WAITING) {
					logger.error("wrong state");
					rc = ExitStatus.FAIL;
				}
				if (tinfo.getWaitedCount() <= 0) {
					logger.error("wrong wait count");
					rc = ExitStatus.FAIL;
				}
				if (mxb.isThreadContentionMonitoringEnabled()) {
					if (tinfo.getWaitedTime() <= 0) {
						logger.error("wrong wait time");
						rc = ExitStatus.FAIL;
					}
				}
				if (!cond.toString().equals(tinfo.getLockName())) {
					logger.error("wrong blocking object");
					rc = ExitStatus.FAIL;
				}
				if (tinfo.getLockOwnerId() != -1) {
					logger.error("wrong lock owner");
					rc = ExitStatus.FAIL;
				}

				if (rc != ExitStatus.PASS) {
					printThreadInfo(tinfo);
				}

				LockSupport.unpark(th);
				th.join();
			} catch (InterruptedException e) {
				e.printStackTrace();
				rc = ExitStatus.FAIL;
			}
		}
		printResult(rc);
		return rc;
	}

	ExitStatus testParkOnOwnableSynchronizer() {
		ExitStatus rc = ExitStatus.PASS;
		ThreadInfo tinfo;
		final ReentrantLock rlock = new ReentrantLock();
		rlock.lock();
		Thread th = new ParkNoTimeout("park rlock", rlock);
		th.start();
		printTestDesc();

		try {
			Thread.sleep(2000);
			tinfo = mxb.getThreadInfo(th.getId());

			do {
				Thread.sleep(1000);
				tinfo = mxb.getThreadInfo(th.getId());
			} while ((tinfo.getThreadState() == Thread.State.NEW)
					|| (tinfo.getThreadState() == Thread.State.RUNNABLE));

			/*
			 * now that we know the thread is parked, wait and re-read the
			 * threadinfo so that the waited time should increase.
			 */
			Thread.sleep(1000);
			tinfo = mxb.getThreadInfo(th.getId(), 8);
			if (tinfo.getThreadState() != Thread.State.WAITING) {
				logger.error("wrong state");
				rc = ExitStatus.FAIL;
			}
			if (tinfo.getWaitedCount() <= 0) {
				logger.error("wrong wait count");
				rc = ExitStatus.FAIL;
			}
			if (mxb.isThreadContentionMonitoringEnabled()) {
				if (tinfo.getWaitedTime() <= 0) {
					logger.error("wrong wait time");
					rc = ExitStatus.FAIL;
				}
			}
			String name = rlock.getClass().getName() + '@'
					+ Integer.toHexString(System.identityHashCode(rlock));
			if (!name.equals(tinfo.getLockName())) {
				logger.error("wrong blocking object, expected " + name);
				rc = ExitStatus.FAIL;
			}
			if (tinfo.getLockOwnerId() != -1) {
				logger.error("wrong lock owner");
				rc = ExitStatus.FAIL;
			}

			if (rc != ExitStatus.PASS) {
				printThreadInfo(tinfo);
			}

			LockSupport.unpark(th);
			th.join();

		} catch (InterruptedException e) {
			e.printStackTrace();
			rc = ExitStatus.FAIL;
		}

		printResult(rc);
		return rc;
	}

	ExitStatus testLockOwnableSynchronizer() {
		ExitStatus rc = ExitStatus.PASS;
		ThreadInfo tinfo;
		final ReentrantLock rlock = new ReentrantLock();
		rlock.lock();

		Thread th = new Thread("block rlock") {
			public void run() {
				rlock.lock();
				rlock.unlock();
			}
		};
		th.start();
		printTestDesc();

		try {
			do {
				Thread.sleep(1000);
				tinfo = mxb.getThreadInfo(th.getId());
			} while ((tinfo.getThreadState() == Thread.State.NEW)
					|| (tinfo.getThreadState() == Thread.State.RUNNABLE));

			/*
			 * now that we know the thread is parked, wait and re-read the
			 * threadinfo so that the waited time should increase.
			 */
			Thread.sleep(1000);
			tinfo = mxb.getThreadInfo(th.getId(), 8);
			if (tinfo.getThreadState() != Thread.State.WAITING) {
				logger.error("wrong state");
				rc = ExitStatus.FAIL;
			}
			if (tinfo.getWaitedCount() <= 0) {
				logger.error("wrong wait count");
				rc = ExitStatus.FAIL;
			}
			if (mxb.isThreadContentionMonitoringEnabled()) {
				if (tinfo.getWaitedTime() <= 0) {
					logger.error("wrong wait time");
					rc = ExitStatus.FAIL;
				}
			}

			Object blocker = LockSupport.getBlocker(th);
			String name = blocker.getClass().getName() + '@'
					+ Integer.toHexString(System.identityHashCode(blocker));
			if (!name.equals(tinfo.getLockName())) {
				logger.error("wrong blocking object, expected " + name);
				rc = ExitStatus.FAIL;
			}
			if (tinfo.getLockOwnerId() != Thread.currentThread().getId()) {
				logger.error("wrong lock owner");
				rc = ExitStatus.FAIL;
			}

			if (rc != ExitStatus.PASS) {
				printThreadInfo(tinfo);
			}

			rlock.unlock();
			th.join();

		} catch (InterruptedException e) {
			e.printStackTrace();
			rc = ExitStatus.FAIL;
		}

		printResult(rc);
		return rc;
	}

	ExitStatus testParkOnUnownedSynchronizer() {
		printTestDesc();
		ExitStatus rc = ExitStatus.PASS;

		MySynchronizer s = new MySynchronizer();
		Thread t = new ParkNoTimeout("unowned sync", s);
		t.start();

		ThreadInfo tinfo;
		do {
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			tinfo = mxb.getThreadInfo(t.getId(), 5);
		} while (tinfo.getThreadState() != Thread.State.WAITING);

		String name = s.getClass().getName() + '@'
				+ Integer.toHexString(System.identityHashCode(s));
		if (!name.equals(tinfo.getLockName())) {
			logger.error("wrong blocking object, expected " + name);
			rc = ExitStatus.FAIL;
		}
		if (tinfo.getLockOwnerId() != -1) {
			logger.error("wrong lock owner");
			rc = ExitStatus.FAIL;
		}

		if (rc != ExitStatus.PASS) {
			printThreadInfo(tinfo);
		}
		
		LockSupport.unpark(t);
		try {
			t.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		
		printResult(rc);
		return rc;
	}

	class ParkNoTimeout extends Thread {
		Object blocker = null;

		ParkNoTimeout(String name) {
			super(name);
		}

		ParkNoTimeout(String name, Object blocker) {
			super(name);
			this.blocker = blocker;
		}

		public void run() {
			if (blocker == null) {
				LockSupport.park();
			} else {
				LockSupport.park(blocker);
			}
		}
	}

	class ParkWithTimeout extends Thread {
		Object blocker = null;

		long nanos = 0;

		ParkWithTimeout(String name, long nanos) {
			super(name);
			this.nanos = nanos;
		}

		ParkWithTimeout(String name, Object blocker, long nanos) {
			super(name);
			this.blocker = blocker;
			this.nanos = nanos;

		}

		public void run() {
			if (blocker == null) {
				LockSupport.parkNanos(nanos);
			} else {
				LockSupport.parkNanos(blocker, nanos);
			}
		}
	}

	class ParkUntil extends Thread {
		Object blocker = null;

		long deadline = 0;

		ParkUntil(String name, long deadline) {
			super(name);
			this.deadline = deadline;
		}

		ParkUntil(String name, Object blocker, long deadline) {
			super(name);
			this.blocker = blocker;
			this.deadline = deadline;

		}

		public void run() {
			if (blocker == null) {
				LockSupport.parkUntil(deadline);
			} else {
				LockSupport.parkUntil(blocker, deadline);
			}
		}
	}

	class MyCondition implements Condition {

		public void await() throws InterruptedException {
		}

		public boolean await(long arg0, TimeUnit arg1)
				throws InterruptedException {
			return false;
		}

		public long awaitNanos(long arg0) throws InterruptedException {
			return 0;
		}

		public void awaitUninterruptibly() {
		}

		public boolean awaitUntil(Date arg0) throws InterruptedException {
			return false;
		}

		public void signal() {
		}

		public void signalAll() {
		}
	}

	class MySynchronizer extends AbstractOwnableSynchronizer {
		private static final long serialVersionUID = 6448257960784838285L;

		MySynchronizer() {
			setExclusiveOwnerThread(null);
		};
	}
}
