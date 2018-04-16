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

import org.testng.Assert;
import org.testng.annotations.Optional;
import org.testng.annotations.Parameters;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

public class StateStress {
	private static Logger logger = Logger.getLogger(StateStress.class);
	
	Inspector[] inspectOne;
	Inspector[] cycle;
	boolean pass = true;
	long newSamples = 0;
	long blockedSamples = 0;
	long runnableSamples = 0;
	long waitSamples = 0;
	long timedwaitSamples = 0;
	long terminatedSamples = 0;

	@Parameters( {"threadsPara", "timePara"} )
	@Test(groups = { "level.extended" })
	public void testStateBenchmark(@Optional("2") String threadsPara, @Optional("10") String timePara) {
		int numInspectors = Integer.parseInt(threadsPara);
		int sec = Integer.parseInt(timePara);

		if (numInspectors < 1) {
			numInspectors = 1;
		}
		if (sec < 1) {
			sec = 1;
		}

		Inspector[] cycle = new Inspector[numInspectors];
		cycle[0] = new Inspector();
		for (int i = 1; i < cycle.length; i++) {
			cycle[i] = new Inspector(cycle[i - 1]);
			cycle[i].start();
		}
		
		Inspector[] inspectOne = new Inspector[numInspectors];
		inspectOne[0] = new Inspector();
		inspectOne[0].setTarget(inspectOne[0]);
		inspectOne[0].start();
		for (int i = 1; i < inspectOne.length; i++) {
			inspectOne[i] = new Inspector(inspectOne[i - 1]);
			inspectOne[i].start();
		}
		
		System.out.print("Sleeping for " + sec + "s ");
		for (int i = 0; i < sec; i++) {
			try {
				Thread.sleep(1000);
				System.out.print(".");
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
		
		for (int i = 0; i < cycle.length; i++) {
			cycle[i].interrupt();
		}
		for (int i = 0 ; i < inspectOne.length; i++) {
			inspectOne[i].interrupt();
		}

		for (int i = 0; i < cycle.length; i++) {
			try {
				cycle[i].join();
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
		for (int i = 0 ; i < inspectOne.length; i++) {
			try {
				inspectOne[i].join();
			} catch (InterruptedException e) {
				// TODO Auto-generated catch block
				e.printStackTrace();
			}
		}
		
		logger.debug("\nSampled");
		logger.debug("\tnew: " + newSamples);
		logger.debug("\trunnable: " + runnableSamples);
		logger.debug("\tblocked: " + blockedSamples);
		logger.debug("\twait: " + waitSamples);
		logger.debug("\ttimedwait: " + timedwaitSamples);
		logger.debug("\tterminated: " + terminatedSamples);
		
		Assert.assertTrue(pass);
	}
	
	
	public boolean stateOk(Thread.State state) {
		boolean ok = true;
		
		if (state == Thread.State.TERMINATED) {
			terminatedSamples++;
		} else if (state == Thread.State.WAITING) {
			waitSamples++;
			ok = false;
		} else if (state == Thread.State.TIMED_WAITING) {
			timedwaitSamples++;
			ok = false;
		} else if (state == Thread.State.RUNNABLE) {
			runnableSamples++;
		} else if (state == Thread.State.BLOCKED) {
			blockedSamples++;
		} else if (state == Thread.State.NEW) {
			newSamples++;
		}
		return ok;
	}

	synchronized public void setFailed() {
		pass = false;
	}
	
	class Inspector extends Thread {
		Thread target;
		
		Inspector() {
			this.target = null;
		}
		
		Inspector(Thread target) {
			this.target = target;
		}
		
		public void run() {
			Thread.State state;
			while (!this.isInterrupted()) {
				state = target.getState();
				if (stateOk(state) != true) {
					setFailed();
				}
			}
		}
		
		public void setTarget(Thread t) {
			this.target = t;
		}
	}
}
