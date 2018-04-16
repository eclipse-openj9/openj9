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

import org.testng.Assert;
import org.testng.annotations.Optional;
import org.testng.annotations.Parameters;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

public class BenchCurrentThreadCpuTime {
	private static Logger logger = Logger.getLogger(BenchCurrentThreadCpuTime.class);
	ThreadMXBean mxb;

	public BenchCurrentThreadCpuTime() {
		mxb = ManagementFactory.getThreadMXBean();
	}

	@Parameters("countPara")
	@Test(groups = { "level.extended" })
	public void testBenchCurrentThreadCpuTime(@Optional("1000000") String countPara) {
		int count = Integer.parseInt(countPara);
		int reps = 10;
		int calls = count * reps;
		long time = 0;
		long accumTime = 0;

		logger.debug("CurrentThreadCpuTime supported: "
				+ mxb.isCurrentThreadCpuTimeSupported());
		logger.debug("CurrentThreadCpuTime enabled: " + mxb.isThreadCpuTimeEnabled());

		Assert.assertTrue(mxb.isCurrentThreadCpuTimeSupported(), "Unable to run benchmark.");

		mxb.setThreadCpuTimeEnabled(true);

		while (reps-- > 0) {
			time = benchCurrentCpuTime(count);
			accumTime += time;
			logger.debug("calls: " + count + " time: " + time + "ms");
		}
		double avg = (double) accumTime / calls;
		logger.debug("total: " + accumTime + "ms" + " per call: " + avg + "ms");
		logger.debug("current cpu time: "
				+ ((double) mxb.getCurrentThreadCpuTime() / 1000000.0) + "ms");
	}

	long benchCurrentCpuTime(int count) {
		long start, end;
		long cputime = 0;

		start = System.currentTimeMillis();
		while (count-- > 0) {
			cputime = mxb.getCurrentThreadCpuTime() / 1000000;
		}
		end = System.currentTimeMillis();

		assert (cputime > (end - start));

		return (end - start);
	}
}
