/*******************************************************************************
 * Copyright (c) 2006, 2018 IBM Corp. and others
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

package jit.test.vich;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import jit.test.vich.utils.Timer;

public class CurrentTimeMillis {

private static Logger logger = Logger.getLogger(CurrentTimeMillis.class);
Timer timer;

public CurrentTimeMillis() {
	timer = new Timer ();
}
/*
 * This test is flaky on virtualized hardware.
 * CurrentTimeMillis removed: defect 158622
 */
@Test(groups = { "level.sanity","component.jit" })
public void testCurrentTimeMillis() {
	long lastTime;
	long currTime, startTime;
	int iterations, milliIterations;

	for (int wait = 1000; wait <= 3000; wait += 1000) {
		iterations = 0;
		milliIterations = 0;
		timer.reset();
		lastTime = startTime = System.currentTimeMillis();
		while (true) {
			iterations++;
			currTime = System.currentTimeMillis();
			if (currTime < lastTime) {
				/* 
				 * This can happen on some linux and/or PPC based platforms... there is no intention of fixing this
				 * behaviour in the medium term.  As such we will not throw an exception here.
				 * 
				 * throw new RuntimeException("System.currentTimeMillis() is not monotonically increasing.");
				 */
			} else if (currTime == lastTime) {
				/* The Windows JIT can complete approx 8 000 000 iterations / second. 
				 * 10 000 000 is long enough to be confident that time isn't moving, but may
				 * need to be increased again in the future.
				 */
				if (++milliIterations >= 10000000) {
					throw new RuntimeException("No time elapsed after " + milliIterations + " iterations.");
				}				
			} else {
				milliIterations = 0;
			}
			if (currTime - startTime >= wait) {
				break;
			}
			lastTime = currTime;
		}		
		timer.mark();
		logger.info("System.currentTimeMillis() running for " + timer.delta() + " milliseconds: iterations = " + Long.toString(iterations));
	}
	return;
}
}
