/*******************************************************************************
 * Copyright (c) 2017, 2017 IBM Corp. and others
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
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
 *******************************************************************************/
package org.openj9.test.java.lang.management.ThreadMXBean;

import java.lang.management.ThreadInfo;

import org.testng.log4testng.Logger;

public abstract class ThreadMXBeanTestCase {
	protected Logger logger = Logger.getLogger(getClass());
	
	public static enum ExitStatus {
		PASS, FAIL, INTERRUPTED, TIMEDOUT, BADARGS
	}

	private volatile ExitStatus rc = ExitStatus.PASS;

	public synchronized final ExitStatus getExitStatus() {
		return rc;
	}

	protected synchronized final void setFailed() {
		this.setExitStatus(ExitStatus.FAIL);
	}

	protected synchronized final void setExitStatus(ExitStatus status) {
		rc = status;
	}

	/* osVersion should be a uname version string in the form x.x.x* */
	public static boolean isLinuxVersionPostRedhat5(String osVersion) {
		boolean rc = false;
		int ver1, ver2, ver3;

		String[] subver = osVersion.split("[^0-9]");
		if ((subver == null) || (subver.length < 3)) {
			return false;
		}

		ver1 = Integer.parseInt(subver[0]);
		ver2 = Integer.parseInt(subver[1]);
		ver3 = Integer.parseInt(subver[2]);
		if ((ver1 >= 2) && (ver2 >= 6) && (ver3 >= 11)) {
			rc = true;
		}
		return rc;
	}

	public static boolean isBadGettimeofdayPlatform() {
		String osName = System.getProperty("os.name").toLowerCase();
		if (osName.contains("linux")) {
			String osVersion = System.getProperty("os.version");
			String osArch = System.getProperty("os.arch").toLowerCase();
			if (osArch.contains("x86") || osArch.contains("amd64")) {
				if (!isLinuxVersionPostRedhat5(osVersion)) {
					return true;
				}
			}
		}
		return false;
	}

	public void printThreadInfo(String title, ThreadInfo ti) {
		logger.debug(title);
		printThreadInfo(ti);
	}

	public void printThreadInfo(ThreadInfo ti) {
		if (ti != null) {
			logger.debug("Thread " + ti.getThreadId());
			logger.debug("\tthread name      " + ti.getThreadName());
			logger.debug("\tthread state     " + ti.getThreadState());
			logger.debug("\tthread suspended " + ti.isSuspended());
			logger.debug("\tthread in native " + ti.isInNative());
			logger.debug("\twaited count     " + ti.getWaitedCount());
			logger.debug("\twaited time      " + ti.getWaitedTime());
			logger.debug("\tblocked count    " + ti.getBlockedCount());
			logger.debug("\tblocked time     " + ti.getBlockedTime());
			logger.debug("\tlock name        " + ti.getLockName());
			logger.debug("\tlock owner id    " + ti.getLockOwnerId());
			logger.debug("\tlock owner name  " + ti.getLockOwnerName());
			
			logger.debug("\tstack trace: (" + ti.getStackTrace().length + ")");
			for (int i = 0; i < ti.getStackTrace().length; i++) {
				logger.debug(ti.getStackTrace()[i].toString());
			}
			
			logger.debug("\tlocked monitors: (" + ti.getLockedMonitors().length + ")");
			for (int i = 0; i < ti.getLockedMonitors().length; i++) {
				logger.debug(ti.getLockedMonitors()[i].toString());
				logger.debug("\tdepth: " + ti.getLockedMonitors()[i].getLockedStackDepth());
				logger.debug("\tframe: " + ti.getLockedMonitors()[i].getLockedStackFrame());
			}

			logger.debug("\tlocked synchronizers: (" + ti.getLockedSynchronizers().length + ")");
			for (int i = 0; i < ti.getLockedSynchronizers().length; i++) {
				logger.debug(ti.getLockedSynchronizers()[i].toString());
			}
		}
	}

	public void printThreadInfo(ThreadInfo[] ti) {
		if (ti != null) {
			for (int i = 0; i < ti.length; i++) {
				printThreadInfo(ti[i]);
			}
		}
	}

	public void printThread(Thread thr) {
		if (thr != null) {
			logger.debug("Thread id: " + thr.getId() + "\tname: " + thr.getName());
		}
	}

	public static interface TimeoutNotifier {
		public void timeout();
	}

	public static class TimeoutThread extends Thread {
		int timeoutSec;
		TimeoutNotifier notifier;

		public TimeoutThread(int timeoutSec, TimeoutNotifier notifier) {
			super("Timeout");
			this.timeoutSec = timeoutSec;
			this.notifier = notifier;
		}

		public void run() {
			try {
				Thread.sleep(timeoutSec * 1000);
			} catch (InterruptedException e) {
			}
			notifier.timeout();
		}
	}

	public static class VerboseTimeoutThread extends TimeoutThread {
		public VerboseTimeoutThread(int timeoutSec, TimeoutNotifier notifier) {
			super(timeoutSec, notifier);
		}

		public void run() {
			System.out.println("\n Sleeping for " + timeoutSec + "s ");
			try {
				for (int i = 0; i < timeoutSec; i++) {
					Thread.sleep(1000);
					System.out.print(".");
				}
				System.out.println("");
			} catch (InterruptedException e) {
			}
			notifier.timeout();
		}
	}
}
