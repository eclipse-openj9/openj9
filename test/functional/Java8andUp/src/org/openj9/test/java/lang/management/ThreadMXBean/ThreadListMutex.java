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
import java.lang.management.ThreadMXBean;

import org.testng.annotations.Optional;
import org.testng.annotations.Parameters;
import org.testng.annotations.Test;

/* Tests contention on vm->vmThreadListMutex and vm->managementDataLock */ 
public class ThreadListMutex extends ThreadMXBeanTestCase {	
	@Parameters( {"timeoutPara"} )
	@Test(groups = { "level.extended" })
	public void testThreadListMutex(@Optional("10") String timeoutPara) {
		int sec = Integer.parseInt(timeoutPara);
		
		if (sec < 1) {
			sec = 1;
		}

		ToggleContentionMonitoring thr = new ToggleContentionMonitoring();
		thr.start();
		
		Inspector inspector = new Inspector(thr);
		inspector.start();
		
		logger.debug("Sleeping for " + sec + "s ");
		for (int i = 0; i < sec; i++) {
			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
			System.out.print(".");
		}
		System.out.println("");
		
		thr.interrupt();
		inspector.interrupt();
		
		try {
			thr.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
		try {
			inspector.join();
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}

	class Inspector extends Thread {
		Thread target;
		Inspector(Thread target) {
			this.target = target;
		}
		public void run() {
			ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
			while (!Thread.interrupted()) {
				mxb.getThreadInfo(this.target.getId());
			}
		}
	}

	class ToggleContentionMonitoring extends Thread {
		public void run() {
			boolean enabled = false;
			ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
			while (!Thread.interrupted()) {
				mxb.setThreadContentionMonitoringEnabled(enabled);
				enabled = !enabled;
			}
		}
	}
}
