/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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

import java.lang.management.ManagementFactory;
import java.lang.management.MemoryMXBean;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

/**
 * MemoryMXBean is a NotificationEmitter; in OpenJ9 a separate thread sends
 * those notifications and another thread is used as a shutdown hook to signal
 * the first thread to terminate. If MemoryMXBean is first used after the VM
 * has started to shutdown, attempting to register a shutdown hook throws
 * an IllegalStateException which was not being caught (see [1]). This test
 * verifies that that IllegalStateException is now caught and the VM shuts
 * down properly.
 *
 * [1] https://github.com/eclipse/openj9/issues/932
 */
@Test(groups = { "level.extended" })
public final class MemoryMXBeanShutdownNotification {

	private static final Logger logger = Logger.getLogger(MemoryMXBeanShutdownNotification.class);

	private static void hookAction() {
		logger.info("The hook thread is running.");

		try {
			MemoryMXBean bean = ManagementFactory.getMemoryMXBean();

			logger.info("Bean successfully initialized " + bean.getObjectPendingFinalizationCount());
		} catch (Exception e) {
			unexpected(e);
		}
	}

	@Test
	public void testNoExceptions() {
		// Add a shutdown hook that will be the first use of the MemoryMXBean.
		Runtime.getRuntime().addShutdownHook(new Thread(MemoryMXBeanShutdownNotification::hookAction));

		// Wait a second, then let shutdown happen naturally.
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e) {
			unexpected(e);
		}
	}

	private static void unexpected(Exception e) {
		e.printStackTrace();
		Assert.fail("Unexpected exception: " + e.getLocalizedMessage(), e);
	}

}
