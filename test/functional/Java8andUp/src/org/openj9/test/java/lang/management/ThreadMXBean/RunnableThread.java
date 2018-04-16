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

/* getThreadInfo() on a runnable thread */
public class RunnableThread extends ThreadMXBeanTestCase {
	@Parameters( {"timeoutPara"} )
	@Test(groups = { "level.extended" })
	public void testRunnableThread(@Optional("10") String timeoutPara) {
		int timeoutSec = Integer.parseInt(timeoutPara);
	
		if (timeoutSec < 1) {
			timeoutSec = 1;
		}
		
		InspectorThread inspector = new InspectorThread();
		try {
			inspector.join(timeoutSec * 1000);
			if (inspector.isAlive()) {
				inspector.interrupt();
				inspector.join();
			}
		} catch (InterruptedException e) {
			setExitStatus(ExitStatus.INTERRUPTED);
		}
		Assert.assertEquals(getExitStatus(), ExitStatus.PASS);
	}

	class RunningThread extends Thread {
		public void run() {
			for (int i = 0; i < 1000000; i++) {
				Thread.yield();
			}
		}
	}

	class InspectorThread extends Thread {
		Thread target;

		InspectorThread() {
			this.target = new RunningThread();
		}

		public void run() {
			ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
			ThreadInfo info = null;
			boolean done = false;

			target.start();

			/* loop until getThreadInfo() returns non-null */
			while (!done) {
				info = mxb.getThreadInfo(this.target.getId());
				if (info != null) {
					done = true;
				} else if (this.isInterrupted()) {
					done = true;
				}
			}

			/* fail if we timed out before the target exited */
			if (info == null) {
				setFailed();
			} else if (info.getThreadId() != this.target.getId()) {
				setFailed();
			} else if (info.getThreadState() != Thread.State.RUNNABLE) {
				setFailed();
			} else if (info.getLockName() != null) {
				setFailed();
			} else if (info.getLockOwnerId() != -1) {
				setFailed();
			}
			
			/* wait for thread to exit */
			try {
				target.join();
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
	}
}
