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

import org.testng.annotations.Optional;
import org.testng.annotations.Parameters;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

public class StateBenchmark {
	private static Logger logger = Logger.getLogger(StateBenchmark.class);

	@Parameters( {"loopCountPara"} )
	@Test(groups = { "level.extended" })
	public void testStateBenchmark(@Optional("1000000") String loopCountPara) {
		long loopCount = Long.parseLong(loopCountPara);
		GetSelfState t = new GetSelfState(loopCount);
		t.run();
		try {
			t.join();
			logger.debug("GetSelfState" + "\tloop: " + loopCount + "\tdelta: " + t.getDelta());
		} catch (InterruptedException e) {
			logger.error("GetSelfState interrupted");
		}
		
		YieldThread y = new YieldThread();
		y.start();

		GetOtherState ot = new GetOtherState(loopCount, y);
		ot.run();
		try {
			ot.join();
			logger.debug("GetOtherState" + "\tloop: " + loopCount + "\tdelta: " + ot.getDelta());
		} catch (InterruptedException e) {
			logger.error("GetOtherState interrupted");
		}
		y.interrupt();
		try {
			y.join();
		} catch (InterruptedException e) {
			logger.error("YieldThread interrupted");
		}
	}
	
	class GetSelfState extends Thread {
		long loopCount;
		long startTime;
		long endTime;
		Thread.State state;
		
		GetSelfState(long iterations) {
			this.loopCount = iterations;
		}
		
		public void run() {
			this.startTime = System.currentTimeMillis();
			for (int i = 0; i < this.loopCount; i++) {
				state = this.getState();
			}
			this.endTime = System.currentTimeMillis();
		}

		public long getDelta() {
			return this.endTime - this.startTime;
		}
	}

	class GetOtherState extends Thread {
		long loopCount;
		long startTime;
		long endTime;
		Thread.State state;
		Thread other;
		
		GetOtherState(long iterations, Thread other) {
			this.loopCount = iterations;
			this.other = other;
		}
		
		public void run() {
			this.startTime = System.currentTimeMillis();
			for (int i = 0; i < this.loopCount; i++) {
				state = other.getState();
			}
			this.endTime = System.currentTimeMillis();
		}
		public long getDelta() {
			return this.endTime - this.startTime;
		}
	}

	class YieldThread extends Thread {
		public void run() {
			while (!this.isInterrupted()) {
				Thread.yield();
			}
		}
	}
}
