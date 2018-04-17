package org.openj9.test.java.lang.management.ThreadMXBean;

import java.lang.management.ManagementFactory;
import java.lang.management.ThreadInfo;
import java.lang.management.ThreadMXBean;

import org.testng.Assert;
import org.testng.annotations.Optional;
import org.testng.annotations.Parameters;
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
import org.testng.annotations.Test;

public class Cycle extends ThreadMXBeanTestCase {
	@Parameters( {"timeoutPara", "numthreadsPara"} )
	@Test(groups = { "level.extended" })
	public void testCycle(@Optional("30") String timeoutPara, @Optional("5") String numthreadsPara) {
		int sec = Integer.parseInt(timeoutPara);
		int numthreads = Integer.parseInt(numthreadsPara);
		
		if (sec < 1) {
			sec = 1;
		}
		if (numthreads < 1) {
			numthreads = 1;
		}
		
		runTestNoSync(sec, numthreads);
		runTestSync(sec, numthreads);
		Assert.assertEquals(getExitStatus(), ExitStatus.PASS);
	}

	volatile boolean done;
	synchronized boolean isDone() {
		return done;
	}

	void runTestNoSync(int sec, int numthreads) {
		QueryThread[] th = new QueryThread[numthreads];

		done = false;
		for (int i = 0; i < numthreads; i++) {
			th[i] = new QueryThread(this);
			th[i].setBlip(String.valueOf(i));
			if (i > 0) {
				th[i].setTarget(th[i - 1]);
				th[i].start();
			}
		}
		th[0].setTarget(th[numthreads - 1]);
		th[0].start();

		logger.debug("Sleeping for " + sec + "s ");
		try {
			for (int i = 0; i < sec; i++) {
				Thread.sleep(1000);
				System.out.print(".");
			}
			System.out.println("");
		} catch (InterruptedException e) {
		}
		synchronized (this) {
			done = true;
		}
		try {
			for (int i = 0; i < numthreads; i++) {
				th[i].join();
			}
		} catch (InterruptedException e) {
		}
	}
	
	void runTestSync(int sec, int numthreads) {
		SyncQueryThread[] th = new SyncQueryThread[numthreads];

		done = false;
		for (int i = 0; i < numthreads; i++) {
			th[i] = new SyncQueryThread(this);
			if (i > 0) {
				th[i].setTarget(th[i - 1]);
				th[i].start();
			}
		}
		th[0].setTarget(th[numthreads - 1]);
		th[0].start();

		logger.debug("Sleeping for " + sec + "s ");
		try {
			for (int i = 0; i < sec; i++) {
				Thread.sleep(1000);
				System.out.print(".");
			}
			System.out.println("");
		} catch (InterruptedException e) {
		}
		synchronized (this) {
			done = true;
		}
		try {
			for (int i = 0; i < numthreads; i++) {
				th[i].join();
			}
		} catch (InterruptedException e) {
		}
	}	
	class QueryThread extends Thread {
		Cycle ctl = null;
		Thread target = null;
		String blip = null;

		QueryThread(Cycle ctl) {
			this.ctl = ctl;
		}

		public void setTarget(Thread t) {
			this.target = t;
		}

		public void setBlip(String s) {
			this.blip = s;
		}

		public void run() {
			ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
			while (ctl.isDone() == false) {
				ThreadInfo ti = mxb.getThreadInfo(target.getId(), 1);
				logger.debug(blip);
				if (ti != null) {
					if ((ti.getThreadId() != target.getId())
							|| (ti.getThreadName().equals(target.getName()) == false)) {
						logger.debug("Incorrect ThreadInfo");
						ctl.setFailed();
					}
				}
			}
		}
	}

	class SyncQueryThread extends Thread {
		Cycle ctl;
		Thread target;

		SyncQueryThread(Cycle ctl) {
			this.ctl = ctl;
		}

		public void setTarget(Thread t) {
			this.target = t;
		}

		public void run() {
			ThreadMXBean mxb = ManagementFactory.getThreadMXBean();
			while (ctl.isDone() == false) {
				ThreadInfo ti;
				synchronized (target) {
					ti = mxb.getThreadInfo(target.getId(), 1);
				}
				synchronized (this) {
					if (ti != null) {
						if (ti.getThreadId() != target.getId()) {
							logger.debug("Incorrect ThreadInfo");
							ctl.setFailed();
						}
					}
				}
			}
		}
	}
}
