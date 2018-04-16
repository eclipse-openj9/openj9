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

import java.io.PrintStream;
import java.lang.management.*;
import java.util.ArrayList;
import java.util.concurrent.locks.*;

import org.testng.annotations.Test;
import org.testng.asserts.SoftAssert;
import org.testng.log4testng.Logger;

public class FindDeadlockTest extends ThreadMXBeanTestCase {
	private SoftAssert softAssert = new SoftAssert();

	ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
	private static final long SLEEP_INTERVAL = 10;

	@Test(groups = { "level.extended" })
	public void testFindDeadlockTest() {
		ExpectedDeadlocks expected = new ExpectedDeadlocks();
		ExpectedDeadlocks expectedSyncs = new ExpectedDeadlocks();

		try {
			startNoDeadlocks();

			/* let the threads run a bit */
			Thread.sleep(3000);
			softAssert.assertTrue(verifyMonitorDeadlocks(null), "findMonitorDeadlocks failed");
			softAssert.assertTrue(verifyDeadlocks(null), "findDeadlocks failed");

			startDeadlockedSynchronizers(expectedSyncs);
			startDeadlocks(expected);
			expectedSyncs.addAll(expected);

			softAssert.assertTrue(verifyMonitorDeadlocks(expected), "findMonitorDeadlocks failed");
			softAssert.assertTrue(verifyDeadlocks(expectedSyncs), "findDeadlocks failed");

		} catch (InterruptedException e) {
			e.printStackTrace();
		} catch (TestSetupFailedException e) {
			logger.error("TEST SETUP FAILED");
			e.printStackTrace();
		}

		softAssert.assertAll();
	}

	void startNoDeadlocks() throws InterruptedException {
		logger.debug("Starting no deadlocks...");

		/* unstarted thread */
		@SuppressWarnings("unused")
		Thread t_nostart = new Thread("nostart");

		/* threads that do nothing */
		Runnable r_sleep = new Runnable() {
			public void run() {
				try {
					while (true) {
						Thread.sleep(1000);
					}
				} catch (InterruptedException e) {
				}
			}
		};

		Thread t_sleep1 = new Thread(r_sleep, "sleep1");
		Thread t_sleep2 = new Thread(r_sleep, "sleep2");
		t_sleep1.start();
		t_sleep2.start();

		/* threads waiting for notification */
		Thread t_wait1 = new Thread("wait1") {
			public void run() {
				synchronized (this) {
					try {
						this.wait();
					} catch (InterruptedException e) {
					}
				}
			}
		};
		t_wait1.start();

		Thread t_wait2 = new Thread("wait2") {
			public void run() {
				synchronized (this) {
					try {
						this.wait();
					} catch (InterruptedException e) {
					}
				}
			}
		};
		t_wait2.start();

		/* Blocked threads */
		Object lock = new Object();
		Locker locker = new Locker("locker", lock);
		synchronized (locker) {
			locker.start();
			locker.wait();
		}

		Blocker b1 = new Blocker("b1", lock);
		Blocker b2 = new Blocker("b2", lock);
		Blocker b3 = new Blocker("b3", lock);

		b1.start();
		b2.start();
		b3.start();

		/* Chained blocked threads */
		Blocker c1 = new Blocker("c1", b1);
		Blocker c2 = new Blocker("c2", c1);
		Blocker c3 = new Blocker("c3", c2);

		c1.start();
		c2.start();
		c3.start();
	}

	void startDeadlocks(ExpectedDeadlocks expected)
			throws TestSetupFailedException {
		/* simple deadlock */
		logger.debug("Starting simple deadlock...");
		Object a = new Object();
		Object b = new Object();

		DThread d1 = new DThread("d1", a, b);
		DThread d2 = new DThread("d2", b, a);
		d1.start();
		d2.start();
		expected.add(new DeadlockEntry(d1, b, d2));
		expected.add(new DeadlockEntry(d2, a, d1));

		/* two disjoint deadlocks */
		logger.debug("Starting disjoint simple deadlocks...");
		Object c = new Object();
		Object d = new Object();
		DThread d3 = new DThread("d3", c, d);
		DThread d4 = new DThread("d4", d, c);
		d3.start();
		d4.start();
		expected.add(new DeadlockEntry(d3, d, d4));
		expected.add(new DeadlockEntry(d4, c, d3));

		/* many-to-one deadlock */
		logger.debug("Starting multiple deadlocks...");
		Object e = new Object();
		Object f = new Object();
		DThread d5 = new DThread("d5", e, a);
		DThread d6 = new DThread("d6", f, a);
		d5.start();
		d6.start();
		expected.add(new DeadlockEntry(d5, a, d1));
		expected.add(new DeadlockEntry(d6, a, d1));

		/* Chained deadlock */
		logger.debug("Starting chained deadlocks...");
		Blocker c1 = new Blocker("c1", a);
		Blocker c2 = new Blocker("c2", c1);
		Blocker c3 = new Blocker("c3", c2);
		Blocker c4 = new Blocker("c4", c3);
		c1.start();
		c2.start();
		c3.start();
		c4.start();
		expected.add(new DeadlockEntry(c1, a, d1));
		expected.add(new DeadlockEntry(c2, c1, c1));
		expected.add(new DeadlockEntry(c3, c2, c2));
		expected.add(new DeadlockEntry(c4, c3, c3));

		/* Indirect deadlock */
		logger.debug("Starting indirect deadlocks...");
		DThread j1 = new DThread("j1");
		DThread j2 = new DThread("j2");
		DThread j3 = new DThread("j3");
		DThread j4 = new DThread("j4");

		j1.setFirst(j1);
		j1.setSecond(j2);
		j2.setFirst(j2);
		j2.setSecond(j3);
		j3.setFirst(j3);
		j3.setSecond(j4);
		j4.setFirst(j4);
		j4.setSecond(j1);

		j1.start();
		j2.start();
		j3.start();
		j4.start();

		expected.add(new DeadlockEntry(j1, j2, j2));
		expected.add(new DeadlockEntry(j2, j3, j3));
		expected.add(new DeadlockEntry(j3, j4, j4));
		expected.add(new DeadlockEntry(j4, j1, j1));

		waitUntilBlocked(d1);
		waitUntilBlocked(d2);
		waitUntilBlocked(d3);
		waitUntilBlocked(d4);
		waitUntilBlocked(d5);
		waitUntilBlocked(d6);
		waitUntilBlocked(c1);
		waitUntilBlocked(c2);
		waitUntilBlocked(c3);
		waitUntilBlocked(c4);
		waitUntilBlocked(j1);
		waitUntilBlocked(j2);
		waitUntilBlocked(j3);
		waitUntilBlocked(j4);
	}

	void startDeadlockedSynchronizers(ExpectedDeadlocks expected)
			throws TestSetupFailedException {

		/* simple deadlock */
		logger.debug("Starting simple deadlock...");
		ReentrantLock a = new ReentrantLock();
		ReentrantLock b = new ReentrantLock();

		DSThread d1 = new DSThread("ds1", a, b);
		DSThread d2 = new DSThread("ds2", b, a);
		d1.start();
		d2.start();
		expected.add(new DeadlockEntry(d1, checkedGetBlocker(d1), d2));
		expected.add(new DeadlockEntry(d2, checkedGetBlocker(d2), d1));

		/* two disjoint deadlocks */
		logger.debug("Starting disjoint simple deadlocks...");
		ReentrantLock c = new ReentrantLock();
		ReentrantLock d = new ReentrantLock();
		DSThread d3 = new DSThread("ds3", c, d);
		DSThread d4 = new DSThread("ds4", d, c);
		d3.start();
		d4.start();
		expected.add(new DeadlockEntry(d3, checkedGetBlocker(d3), d4));
		expected.add(new DeadlockEntry(d4, checkedGetBlocker(d4), d3));

		/* many-to-one deadlock */
		logger.debug("Starting multiple deadlocks...");
		ReentrantLock e = new ReentrantLock();
		ReentrantLock f = new ReentrantLock();
		DSThread d5 = new DSThread("ds5", e, a);
		DSThread d6 = new DSThread("ds6", f, a);
		d5.start();
		d6.start();
		expected.add(new DeadlockEntry(d5, checkedGetBlocker(d5), d1));
		expected.add(new DeadlockEntry(d6, checkedGetBlocker(d6), d1));
	}

	boolean verifyDeadlocks(ExpectedDeadlocks expected) {
		boolean ok = true;
		long[] tids;
		ThreadInfo[] ti;

		tids = mxb.findDeadlockedThreads();
		if (tids == null) {
			if (expected == null) {
				return true;
			} else {
				logger.error("findDeadlockedThreads returned null");
				return false;
			}
		}

		ti = mxb.getThreadInfo(tids, true, true);
		ok = ok && matchDeadlocks(ti, expected);

		ti = mxb.getThreadInfo(tids, false, false);
		ok = ok && matchDeadlocks(ti, expected);

		ti = mxb.getThreadInfo(tids, true, false);
		ok = ok && matchDeadlocks(ti, expected);

		ti = mxb.getThreadInfo(tids, false, true);
		ok = ok && matchDeadlocks(ti, expected);

		return ok;
	}

	boolean verifyMonitorDeadlocks(ExpectedDeadlocks expected) {
		boolean ok = true;
		long[] tids;
		ThreadInfo[] ti;

		tids = mxb.findMonitorDeadlockedThreads();
		if (tids == null) {
			if (expected == null) {
				return true;
			} else {
				System.out
						.println("findMonitorDeadlockedThreads returned null");
				return false;
			}
		}

		ti = mxb.getThreadInfo(tids, true, true);
		ok = ok && matchDeadlocks(ti, expected);

		ti = mxb.getThreadInfo(tids, false, false);
		ok = ok && matchDeadlocks(ti, expected);

		ti = mxb.getThreadInfo(tids, true, false);
		ok = ok && matchDeadlocks(ti, expected);

		ti = mxb.getThreadInfo(tids, false, true);
		ok = ok && matchDeadlocks(ti, expected);

		return ok;
	}

	boolean matchDeadlocks(ThreadInfo[] ti, ExpectedDeadlocks _expected) {
		ExpectedDeadlocks expected = (ExpectedDeadlocks) _expected.clone();
		boolean ok = true;
		boolean[] found;

		if (ti == null) {
			if (expected == null) {
				return true;
			} else {
				return false;
			}
		}

		found = new boolean[ti.length];
		for (int i = 0; i < found.length; i++) {
			found[i] = false;
		}

		for (int i = 0; i < ti.length; ++i) {
			for (int j = 0; j < expected.size(); ++j) {
				DeadlockEntry ded = expected.get(j);
				if (ti[i].getThreadId() == ded.tid) {
					if (ti[i].getLockName().equals(ded.blocker)) {
						if (ti[i].getLockOwnerId() == ded.owner) {
							found[i] = true;
							expected.remove(j);
							break;
						}
					}
				}
			}
			if (!found[i]) {
				ok = false;
			}
		}

		if (!ok) {
			logger.error("Not deadlocks:");
			for (int i = 0; i < ti.length; ++i) {
				if (!found[i]) {
					printThreadInfo(ti[i]);
					ok = false;
				}
			}
		}

		if (!expected.isEmpty()) {
			ok = false;
			logger.error("Deadlocks not found:");
			for (int i = 0; i < expected.size(); ++i) {
				expected.get(i).print(System.out);
			}
		}
		return ok;
	}

	private void waitUntilBlocked(Thread t) throws TestSetupFailedException {
		boolean blocked = false;

		logger.debug("Waiting for " + t + " to block...");
		WaitUntilBlockedThread w = new WaitUntilBlockedThread(t);
		w.start();
		blocked = w.joinGetBlocked(3000);

		if (!blocked) {
			ThreadInfo ti = mxb.getThreadInfo(t.getId(), Integer.MAX_VALUE);
			if (ti == null) {
				throw new TestSetupFailedException("Unable to get state for "
						+ t);
			}

			if (ti.getThreadState() != Thread.State.BLOCKED) {
				TestSetupFailedException cause = new TestSetupFailedException(t
						+ " stack trace:");
				cause.setStackTrace(ti.getStackTrace());
				throw new TestSetupFailedException(t + " is not blocked.",
						cause);
			}
		}
	}

	private Object checkedGetBlocker(Thread t) throws TestSetupFailedException {
		Object blocker;
		WaitUntilParkedThread w = new WaitUntilParkedThread(t);
		w.start();
		blocker = w.joinGetBlocker(3000);

		if (blocker == null) {
			TestSetupFailedException cause = new TestSetupFailedException(t
					+ " stack trace:");
			cause.setStackTrace(t.getStackTrace());
			throw new TestSetupFailedException(t + " did not block.", cause);
		}
		return blocker;
	}

	class TestSetupFailedException extends Exception {
		private static final long serialVersionUID = 27927471102355761L;

		TestSetupFailedException(String s) {
			super(s);
		}

		TestSetupFailedException(String s, Throwable cause) {
			super(s, cause);
		}
	}

	class WaitUntilBlockedThread extends Thread {
		Thread t;
		volatile boolean blocked = false;

		WaitUntilBlockedThread(Thread t) {
			super("wait.blocked." + t);
			this.t = t;
		}

		public void run() {
			blocked = false;
			try {
				while (!blocked) {
					Thread.sleep(SLEEP_INTERVAL);
					if (t.getState() == Thread.State.BLOCKED) {
						blocked = true;
					}
				}
			} catch (InterruptedException e) {
			}
		}

		boolean joinGetBlocked(long timeout) {
			try {
				super.join(timeout);
			} catch (InterruptedException e) {
			}
			return blocked;
		}
	}

	class WaitUntilParkedThread extends Thread {
		Thread t;
		volatile Object blocker = null;

		WaitUntilParkedThread(Thread t) {
			super("wait.parked." + t);
			this.t = t;
		}

		public void run() {
			blocker = null;

			try {
				while (blocker == null) {
					Thread.sleep(SLEEP_INTERVAL);
					blocker = LockSupport.getBlocker(t);
				}
			} catch (InterruptedException e) {
			}
		}

		Object joinGetBlocker(long timeout) {
			try {
				super.join(timeout);
			} catch (InterruptedException e) {
			}
			return blocker;
		}
	}

	class DThread extends Thread {
		Object first, second;

		DThread(String name) {
			super(name);
		}

		DThread(String name, Object first, Object second) {
			super(name);
			this.first = first;
			this.second = second;
		}

		public void setFirst(Object o) {
			this.first = o;
		}

		public void setSecond(Object o) {
			this.second = o;
		}

		public void run() {
			try {
				synchronized (first) {
					logger.debug(this + " locked " + first);

					while (true) {
						Thread.sleep(SLEEP_INTERVAL);
						logger.debug(this + " locking " + second);
						synchronized (second) {
							logger.debug(this + " got " + second
									+ "too soon, releasing");
						}
					}
				}
			} catch (InterruptedException e) {
				logger.debug(e);
			}
		}
	}

	class DSThread extends Thread {
		ReentrantLock first, second;

		DSThread(String name) {
			super(name);
		}

		DSThread(String name, ReentrantLock first, ReentrantLock second) {
			super(name);
			this.first = first;
			this.second = second;
		}

		public void setFirst(ReentrantLock o) {
			this.first = o;
		}

		public void setSecond(ReentrantLock o) {
			this.second = o;
		}

		public void run() {
			try {
				first.lock();
				logger.debug(this + " locked " + first);

				while (true) {
					Thread.sleep(SLEEP_INTERVAL);
					logger.debug(this + " locking " + second);
					second.lockInterruptibly();
					logger.debug(this + " got " + second
							+ " too soon, releasing");
					second.unlock();
				}
			} catch (InterruptedException e) {
			}
		}
	}

	class Blocker extends Thread {
		Object lock;

		public void run() {
			try {
				synchronized (this) {
					while (true) {
						Thread.sleep(SLEEP_INTERVAL);
						logger.debug("Blocker " + this + " locking "
								+ lock);
						synchronized (lock) {
							logger.debug("Blocker " + this + " got "
									+ lock + " too soon, releasing");
						}
					}
				}
			} catch (InterruptedException e) {
			}
		}

		Blocker(String name, Object lock) {
			super(name);
			this.lock = lock;
		}
	}

	class Locker extends Thread {
		Object lock;

		public void run() {
			try {
				synchronized (lock) {
					logger.debug("Locker got " + lock);
					synchronized (this) {
						this.notify();
					}
					while (true) {
						Thread.sleep(1000);
					}
				}
			} catch (InterruptedException e) {
			}
		}

		Locker(String name, Object lock) {
			super(name);
			this.lock = lock;
		}
	}

	class DeadlockEntry {
		public long tid;
		public String name;
		public String blocker;
		public long owner;
		public String ownerName;

		public DeadlockEntry(Thread thread, Object blocker, Thread owner) {
			this.tid = thread.getId();
			this.name = thread.getName();
			this.blocker = blocker.getClass().getName() + '@'
					+ Integer.toHexString(System.identityHashCode(blocker));
			this.owner = owner.getId();
			this.ownerName = owner.getName();
		}

		public void print(PrintStream pr) {
			pr.println("tid: " + tid + " (" + name + ")");
			pr.println("blocker: " + blocker);
			pr.println("owner: " + owner + " (" + ownerName + ")");
			pr.println();
		}
	}

	class ExpectedDeadlocks extends ArrayList<DeadlockEntry> {
		private static final long serialVersionUID = 5521737345745330784L;
	}
}
