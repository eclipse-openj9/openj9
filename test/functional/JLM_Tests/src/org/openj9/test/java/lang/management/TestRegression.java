/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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

package org.openj9.test.java.lang.management;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.io.IOException;
import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;

import javax.management.MBeanServer;
import javax.management.MBeanServerConnection;

/**
 * Tests to catch regressions on reported bugs.
 */
@Test(groups = { "level.sanity" })
public class TestRegression {

	private static Logger logger = Logger.getLogger(TestRegression.class);

	@Test
	public final void testBug452AgainstLocalVM() {
		MBeanServerConnection mbs = null;
		ThreadMXBean threadBean = null;

		mbs = ManagementFactory.getPlatformMBeanServer();

		// Use the connection to the Platform Server on the monitored JVM to get
		// a proxy to the
		// monitor JVMS Thread MXBean. The method available on the Thread MXBean
		// can be call using
		// the proxy object
		try {
			threadBean = ManagementFactory.newPlatformMXBeanProxy(mbs, ManagementFactory.THREAD_MXBEAN_NAME,
					ThreadMXBean.class);
		} catch (IOException ie) {
			ie.printStackTrace();
		}

		// The Thread MXBean can also return an array of the ids for all the
		// existing threads....

		long[] threadIDs = threadBean.getAllThreadIds();

		ThreadInfo[] threadDataset = threadBean.getThreadInfo(threadIDs);

		for (ThreadInfo threadData : threadDataset) {
			logger.debug("ThreadInfo = " + threadData.toString());
			logger.debug("Thread Name = " + threadData.getThreadName());
			if (threadData.getLockName() != null) {
				logger.debug("Lock Name = " + threadData.getLockName());
				logger.debug("Lock Owner Name = " + threadData.getLockOwnerName());
				logger.debug("Lock Owner ID = " + threadData.getLockOwnerId());
			}
			logger.debug("\n");
		}
	}

	@Test
	public final void testBug93006() {
		MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();

		System.setSecurityManager(new SecurityManager());
		AssertJUnit.assertNotNull(System.getSecurityManager());

		try {
			MBeanServer mbs2 = ManagementFactory.getPlatformMBeanServer();
			Assert.fail("Should have thrown a SecurityException");
		} catch (Exception e) {
			AssertJUnit.assertTrue(e instanceof SecurityException);
		}
	}
}
