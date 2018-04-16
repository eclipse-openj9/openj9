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

import java.lang.management.*;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

public class Deadlocks {
	private static Logger logger = Logger.getLogger(Deadlocks.class);

	class Locker extends Thread {
		public Object firstLock;

		public Object secondLock;

		public void run() {
			logger.debug(this + " locking " + firstLock);
			synchronized (firstLock) {
				logger.debug(this + " locked " + firstLock);
				try {
					sleep(1000);
				} catch (InterruptedException ie) {
				}
				logger.debug(this + " locking " + secondLock);
				synchronized (secondLock) {
					logger.debug(this + " locked " + secondLock);
				}
			}
		}
	}

	class DoNothing extends Thread {
		private volatile boolean m_keepRunning = true;

		public void stopRunning() {
			m_keepRunning = false;
		}

		private long m_count;

		public void run() {
			while (m_keepRunning) {
				m_count++;
			}
		}
	}
	
	@Test(groups = { "level.extended" })
	public void testDeadlocks() {
		ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
		logger.debug("\nTesting two deadlocked threads");
		Object first = new Object();
		Object second = new Object();

		Locker locker1 = new Locker();
		locker1.setDaemon(true);
		locker1.firstLock = first;
		locker1.secondLock = second;

		Locker locker2 = new Locker();
		locker1.setDaemon(true);
		locker2.firstLock = second;
		locker2.secondLock = first;

		locker1.start();
		locker2.start();

		try {
			Thread.sleep(5 * 1000);
		} catch (InterruptedException ie) {
		}

		long threadIds[] = mxb.findMonitorDeadlockedThreads();
		logger.debug("\tfindMonitorDeadlockedThreads returned '"
				+ threadIds + "': ");
		Assert.assertNotEquals(threadIds, null, "\t\tFAIL - threadIds is null");
		Assert.assertEquals(threadIds.length, 2, "\t\tFAIL - # deadlocked threads = " + threadIds.length);

		for (int i = 0; i < threadIds.length; i++) {
			logger.debug("\t\t" + threadIds[i] + " ThreadInfo: "
					+ mxb.getThreadInfo(threadIds[i], 3));
		}

		// It's deadlocked.. but is the information right?
		boolean idsOk = (((threadIds[0] == locker1.getId()) && (threadIds[1] == locker2
				.getId())) || ((threadIds[0] == locker2.getId()) && (threadIds[1] == locker1
				.getId())));
		Assert.assertTrue(idsOk, "\t\tFAIL - deadlocked thread ids are incorrect");

		// Check the thread info we're getting back
		ThreadInfo[] threads = mxb.getThreadInfo(threadIds);
		idsOk = ((threads[0].getThreadId() == threads[1].getLockOwnerId()) && (threads[1]
				.getThreadId() == threads[0].getLockOwnerId()));
		Assert.assertTrue(idsOk, "\t\tFAIL - ThreadInfo thread ids are incorrect");

		// Thread state is good?
		Assert.assertFalse((threads[0].getThreadState() != Thread.State.BLOCKED)
				|| (threads[1].getThreadState() != Thread.State.BLOCKED), "\t\tFAIL - ThreadInfo thread states incorrect");
	}
}
