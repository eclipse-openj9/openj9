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

public class WaitingThread extends ThreadMXBeanTestCase {
	@Parameters( {"timeoutPara"} )
	@Test(groups = { "level.extended" })
	public void testWaitingThread(@Optional("60") String timeoutPara) {
		int timeout = Integer.parseInt(timeoutPara);

		if (timeout <= 0) {
			timeout = 1;
		}

		ExitStatus exitStatus = ExitStatus.PASS;
		Object sync = new Object();
		Waiter waiter = new Waiter(sync, Thread.currentThread());
		Inspector inspector = new Inspector(waiter);

		logger.debug("Lock: " + sync.toString());

		inspector.start();
		waiter.start();

		try {
			/*
			 * Every 100ms, notify the waiter and check that the inspector
			 * hasn't detected an invalid state.
			 */
			for (int i = 0; i < timeout * 10; i++) {
				inspector.join(100);

				if (!inspector.isAlive()) {
					break;
				}
				waiter.testnotify(false);
			}
			System.out.println("");

			/* Shut down the test */
			logger.debug("Shutting down test");
			inspector.shutdown();
			waiter.testnotify(true);

			/* Wait for the threads to shut down */
			inspector.join(60 * 1000);
			waiter.join(60 * 1000);

		} catch (InterruptedException e) {
			logger.error("\nTest was interrupted");
			exitStatus = ExitStatus.INTERRUPTED;
		}

		if (inspector.isAlive()) {
			logger.error("Inspector failed to shut down.");
			exitStatus = ExitStatus.TIMEDOUT;
		}
		if (waiter.isAlive()) {
			logger.error("Waiter failed to shut down.");
			exitStatus = ExitStatus.TIMEDOUT;
		}
		if (inspector.getExitStatus() != ExitStatus.PASS) {
			logger.error("Inspector error.");
			exitStatus = inspector.getExitStatus();
		}

		waiter.printSampleCounts();

		if (exitStatus != ExitStatus.PASS) {
			ThreadMXBean mxb = ManagementFactory.getThreadMXBean();

			logger.error("====================");
			logger.error("Last inspected state");
			logger.error("====================");
			printThreadInfo(inspector.getLastInfo());

			logger.error("=====================");
			logger.error("Current thread states");
			logger.error("=====================");
			printThreadInfo(mxb.getThreadInfo(mxb.getAllThreadIds(), 3));
		}

		Assert.assertEquals(getExitStatus(), ExitStatus.PASS);
	}

	class Inspector extends Thread {
		private Waiter target;
		private ThreadInfo info;
		private volatile boolean done = false;
		private volatile ExitStatus exitStatus = ExitStatus.PASS;

		Inspector(Waiter target) {
			super("Inspector");
			this.target = target;
		}

		public void run() {
			logger.debug("Inspector started");
			ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
			long targetId = target.getId();

			try {
				while (!done) {
					for (int i = 0; i < 100; ++i) {
						info = mxb.getThreadInfo(targetId);

						if (!target.isStateOk(info)) {
							exitStatus = ExitStatus.FAIL;
							done = true;
							break;
						}
					}
					logger.debug("o");
					Thread.sleep(7);
				}
			} catch (OutOfMemoryError e) {
				logger.error("out of memory");
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			logger.debug("Inspector finishing");
		}

		void shutdown() {
			done = true;
		}

		ThreadInfo getLastInfo() {
			return info;
		}

		ExitStatus getExitStatus() {
			return exitStatus;
		}
	}

	class Waiter extends Thread {
		private Object sync;
		private Thread notifier;
		private int[] sampleCounts = new int[Thread.State.values().length];
		private boolean done = false;
		private boolean started = false;
		private boolean exited = false;

		Waiter(Object sync, Thread notifier) {
			super("Waiter");
			this.sync = sync;
			this.notifier = notifier;
		}

		public void run() {
			logger.debug("Waiter started");
			try {
				synchronized (sync) {
					while (!done) {
						sync.wait();
					}
				}
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			logger.debug("Waiter finishing");
		}

		void testnotify(boolean doShutdown) {
			synchronized (sync) {
				System.out.print(".");
				if (doShutdown) {
					done = true;
				}
				sync.notify();
			}
		}

		boolean isStateOk(ThreadInfo info) {
			boolean ok = true;

			if (info == null) {
				ok = true;
				if (started) {
					if (!exited) {
						exited = true;
					}
				}
				return ok;
			}

			if (!started) {
				started = true;
				exited = false;
			} else {
				if (exited) {
					ok = false;
					logger.error("ThreadInfo should be null");
					return ok;
				}
			}

			this.sampleCounts[info.getThreadState().ordinal()]++;
			if (ok) {
				switch (info.getThreadState()) {
				case RUNNABLE:
				case NEW:
				case TERMINATED:
					if (info.getLockOwnerId() != -1) {
						ok = false;
					}
					if (info.getLockName() != null) {
						ok = false;
					}
					break;
				case BLOCKED:
					if (info.getLockOwnerId() != notifier.getId()) {
						ok = false;
					}
					if (!sync.toString().equals(info.getLockName())) {
						ok = false;
					}
					break;
				case WAITING:
					if ((info.getLockOwnerId() != notifier.getId())
							&& (info.getLockOwnerId() != -1)) {
						ok = false;
					}
					if (!sync.toString().equals(info.getLockName())) {
						ok = false;
					}
					break;
				case TIMED_WAITING:
				default:
					ok = false;
					break;
				}
			}
			return ok;
		}

		void printSampleCounts() {
			logger.debug("Sampled");
			for (int i = 0; i < Thread.State.values().length; i++) {
				logger.debug(" " + Thread.State.values()[i] + ": " + sampleCounts[i]);
			}
			logger.debug("\n");
		}
	}
}
