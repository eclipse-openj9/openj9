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

/* Stress test. Many threads inspect one thread. */
public class ManyInspectOne extends ThreadMXBeanTestCase {
	volatile boolean done = false;

	@Parameters( {"timeoutPara", "numInspectorsPara"} )
	@Test(groups = { "level.extended" })
	public void testManyInspectOne(@Optional("60") String timeoutPara, @Optional("5") String numInspectorsPara) {
		int sec = Integer.parseInt(timeoutPara);
		int numInspectors = Integer.parseInt(numInspectorsPara);

		if (sec < 1)
			sec = 1;
		if (numInspectors < 1)
			numInspectors = 1;

		done = false;
		/* Create threads */
		SimpleThread target = new SimpleThread(this);
		InspectThread[] insp = new InspectThread[numInspectors];
		ThreadMXBean mxb = ManagementFactory.getThreadMXBean();

		for (int i = 0; i < numInspectors; i++) {
			insp[i] = new InspectThread(this, target);
			insp[i].start();
		}
		target.start();

		logger.debug("Running for " + sec + "s ");
		try {
			for (int i = 0; i < sec; i++) {
				Thread.sleep(1000);
				System.out.print('.');
			}
			System.out.println("");
		} catch (InterruptedException e) {
			/* do nothing */
		}

		logger.debug("");
		printThreadInfo(mxb.getThreadInfo(mxb.getAllThreadIds()));

		synchronized (this) {
			done = true;
		}

		long dead[] = mxb.findMonitorDeadlockedThreads();
		if (dead != null) {
			logger.debug("Deadlocks!");
			printThreadInfo(mxb.getThreadInfo(dead));
		}

		/* wait to terminate */
		try {
			for (int i = 0; i < numInspectors; i++) {
				insp[i].join();
			}
			target.join();
		} catch (InterruptedException e) {
		}
		Assert.assertEquals(getExitStatus(), ExitStatus.PASS);
	}

	synchronized boolean isDone() {
		return this.done;
	}

	public static boolean isThreadInfoCorrect(ThreadInfo ti, Thread th) {
		if (ti.getThreadId() == th.getId()) {
			return true;
		} else {
			return false;
		}
	}

	class SimpleThread extends Thread {
		ManyInspectOne ctl;

		SimpleThread(ManyInspectOne ctl) {
			this.ctl = ctl;
		}

		synchronized void func() {
			Thread.yield();
		}

		public void run() {
			while (ctl.isDone() == false) {
				this.func();
			}
		}
	}

	class InspectThread extends Thread {
		ManyInspectOne ctl;

		Thread th;

		ThreadMXBean mxb;

		InspectThread(ManyInspectOne ctl, Thread th) {
			this.ctl = ctl;
			this.th = th;
			mxb = ManagementFactory.getThreadMXBean();
		}

		public void run() {
			while (ctl.isDone() == false) {
				ThreadInfo ti = mxb.getThreadInfo(th.getId(), 1);
				if (ti != null) {
					if (ManyInspectOne.isThreadInfoCorrect(ti, th) == false) {
						logger.debug("FAIL. Wrong ThreadInfo.");
						ctl.setFailed();
					}
				}
				Thread.yield();
			}
		}
	}
}
