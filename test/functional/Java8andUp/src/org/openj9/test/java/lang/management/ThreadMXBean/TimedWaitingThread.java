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

public class TimedWaitingThread extends ThreadMXBeanTestCase {
	@Parameters( {"timeoutPara"} )
	@Test(groups = { "level.extended" })
	public void testTimedWaitingThread(@Optional("120") String timeoutPara) {
		int timeout = Integer.parseInt(timeoutPara);

		Object sync = new Object();
		Notifier notifier = new Notifier(sync);
		Waiter waiter = new Waiter(sync, notifier, Thread.currentThread());
		Inspector inspector = new Inspector(waiter);

		logger.debug("Lock: " + sync.toString());

		inspector.start();
		waiter.start();
		notifier.start();

		try {
			for (int i = 0; i < timeout; i++) {
				if (this.getExitStatus() != ExitStatus.PASS) {
					break;
				}
				Thread.sleep(1000);
				System.out.print(".");
			}
			System.out.println("");
			inspector.interrupt();
			notifier.interrupt();
			
			inspector.join();
			notifier.join();
			
			/* Kill the waiter last because internal locks might get used when
			 * shutting down the test.
			 */
			waiter.interrupt();
			waiter.join();
			
			waiter.printSampleCounts();
			printThreadInfo(ManagementFactory.getThreadMXBean()
					.getThreadInfo(Thread.currentThread().getId(), 3));
			printThreadInfo(inspector.getLastInfo());
		} catch (InterruptedException e) {
			setExitStatus(ExitStatus.INTERRUPTED);
		}
		Assert.assertEquals(getExitStatus(), ExitStatus.PASS);
	}

	class Inspector extends Thread {
		Waiter target;
		ThreadInfo info;
		boolean done;

		Inspector(Waiter target) {
			super("Inspector");
			this.target = target;
		}

		public void run() {
			ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
			while (!done) {
				try {
					this.info = mxb.getThreadInfo(this.target.getId(), 10);
	
					if (!this.target.isStateOk(this.info)) {
						printThreadInfo(mxb.getThreadInfo(this.getId(), 2));
						printThreadInfo(this.info);
						setFailed();
						done = true;
					}
				} catch (OutOfMemoryError e) {
					System.gc();
				}
				if (this.isInterrupted()) {
					done = true;
				}
			}
		}

		public ThreadInfo getLastInfo() {
			return this.info;
		}
	}
	
	class Notifier extends Thread {
		Object sync;
		
		Notifier(Object sync) {
			super("Notifier");
			this.sync = sync;
		}
		
		public void run() {
			int sleeptime = 10;
			
			try {
				while (true) {
					Thread.sleep(sleeptime);
					synchronized (sync) {
						sync.notify();
					}
				}
			} catch (InterruptedException e) {}
		}
	}

	class Waiter extends Thread {
		Object sync;
		Thread notifier;
		Thread interrupter;
		int[] sampleCounts;
		boolean started = false;
		boolean exited = false;

		Waiter(Object sync, Thread notifier, Thread interrupter) {
			super("Waiter");
			this.sync = sync;
			this.notifier = notifier;
			this.interrupter = interrupter;
			this.sampleCounts = new int[Thread.State.values().length];
		}

		public void run() {
			try {
				boolean done = false;
				while (!done) {
					synchronized (sync) {
						sync.wait(100);
					}
				}
			} catch (InterruptedException e) {
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
					logger.debug("ThreadInfo should be null");
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
				case TIMED_WAITING:
					if ((info.getLockOwnerId() != notifier.getId())
							&& (info.getLockOwnerId() != interrupter.getId())
							&& (info.getLockOwnerId() != -1)) {
						ok = false;
					}
					if (!sync.toString().equals(info.getLockName())) {
						ok = false;
					}
					break;
				case WAITING:
				default:
					ok = false;
					break;
				}
			}
			return ok;
		}

		void printSampleCounts() {
			logger.debug("\nSampled");
			for (int i = 0; i < Thread.State.values().length; i++) {
				logger.debug(" " + Thread.State.values()[i] + ": "
						+ this.sampleCounts[i]);
			}
			logger.debug("\n");
		}
	}
}
