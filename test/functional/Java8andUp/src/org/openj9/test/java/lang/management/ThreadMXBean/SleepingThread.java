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
import org.testng.annotations.Test;

public class SleepingThread extends ThreadMXBeanTestCase {
	@Test(groups = { "level.extended" })
	public void testSleepingThread() {
		Assert.assertEquals(testSelf(), ExitStatus.PASS);
		Assert.assertEquals(testThread(), ExitStatus.PASS);
	}

	/*
	 * Test that waited counts and times ARE updated when the current thread
	 * sleeps
	 */
	ExitStatus testSelf() {
		logger.debug("testSelf");
		
		long tid = Thread.currentThread().getId();
		ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
		ThreadInfo oldinfo;
		ThreadInfo newinfo;
		ExitStatus rc = ExitStatus.PASS;

		if (mxb.isThreadContentionMonitoringSupported()) {
			mxb.setThreadContentionMonitoringEnabled(true);
		}
		
		try {
			oldinfo = mxb.getThreadInfo(tid);
			Thread.sleep(1000);
			newinfo = mxb.getThreadInfo(tid);
			if (newinfo.getWaitedCount() != oldinfo.getWaitedCount()+1) {
				logger.error("wrong waited count");
				rc = ExitStatus.FAIL;
			}
			if (mxb.isThreadContentionMonitoringEnabled()) {
				if (newinfo.getWaitedTime() <= oldinfo.getWaitedTime()) {
					logger.error("wrong waited time");
					rc = ExitStatus.FAIL;
				}
			}
			if (rc != ExitStatus.PASS) {
				logger.error("Old ThreadInfo:");
				printThreadInfo(oldinfo);
				logger.error("New ThreadInfo:");
				printThreadInfo(newinfo);
			}
		} catch (InterruptedException e) {
			return ExitStatus.INTERRUPTED;
		}
		return rc;
	}

	/*
	 * Tests that sleeping thread state is reported as TIMED_WAITING
	 */
	ExitStatus testThread() {
		logger.debug("testThread");
		
		ExitStatus rc = ExitStatus.PASS;
		ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
		
		Thread sleeper = new Thread("sleeper") {
			public void run() {
				try {
					Thread.sleep(5 * 60 * 1000);
				} catch (InterruptedException e) {
				}
			}
		};
		sleeper.start();

		try {
			Thread.sleep(1000);
		} catch (InterruptedException e1) {
			e1.printStackTrace();
		}
		ThreadInfo ti = mxb.getThreadInfo(sleeper.getId(), 8);
		
		if ((ti.getThreadState() != Thread.State.TIMED_WAITING)
			|| (ti.getLockOwnerName() != null) || (ti.getLockName() != null)) {
			printThreadInfo(ti);
			rc = ExitStatus.FAIL;
		}
		sleeper.interrupt();
		try {
			sleeper.join();
		} catch (InterruptedException e) {
		}
		
		return rc;
	}
}
