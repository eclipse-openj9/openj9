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

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

public class Cmvc106800 {
	private static Logger logger = Logger.getLogger(Cmvc106800.class);

	@Test(groups = { "level.extended" })
	public void testCmvc106800() {
		Object monitor = new Object();
		Blocker blocker = new Blocker();
		blocker.synObj = monitor;

		synchronized(monitor) {
			logger.debug("Starting the blocker thread " + blocker + "(id = " + blocker.getId() + ")");
			blocker.start();
			try { Thread.sleep(2*1000); } catch (InterruptedException ie) {}

			Locker locker = new Locker();
			locker.m_keepLocking = true;
			locker.synThread = blocker;
			locker.start();
			try { Thread.sleep(2*1000); } catch (InterruptedException ie) {}

			logger.debug("Getting ThreadMXBean");
			ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
			logger.debug("Getting ThreadInfo on blocker thread");
			ThreadInfo ti = mxb.getThreadInfo(blocker.getId());
			logger.debug("done");

			// Shut things down
			locker.m_keepLocking = false;
		}
	}

	class Blocker extends Thread {
		public Object synObj;
		public void run() {
			logger.debug("Blocker " + this + " (id = " + this.getId() + ") synchronizing on " + synObj);
			synchronized (synObj) {
				logger.debug("Blocker synchronized on " + synObj);
			}
		}
	}

	class Locker extends Thread {
		public Thread synThread;
		public volatile boolean m_keepLocking;
		public void run() {
			logger.debug("Locker " + this + " synchronizing on " + synThread);
			synchronized(synThread) {
				logger.debug("Locker synchronized on " + synThread);
				while(m_keepLocking) {
					try { sleep(1000); } catch (InterruptedException ie) {}
				}
			}
		}
	}	
}
