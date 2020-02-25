/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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
package org.openj9.test.management;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.management.ManagementFactory;

import com.ibm.lang.management.CpuLoadCalculationConstants;

@SuppressWarnings({ "nls", "restriction" })
@Test(groups = { "level.extended" })
public class TestGetSystemCpuLoad {

	private static final Logger logger = Logger.getLogger(TestGetSystemCpuLoad.class);

	/* convert ns to ms and round up */
	private static final long MINIMUM_INTERVAL = (long) Math.ceil(CpuLoadCalculationConstants.MINIMUM_INTERVAL / 1e6);
	private static final double MIN_LOAD = 0.0;
	private static final double MAX_LOAD = 1.0;
	private static final double NO_ERROR = -100; /* should never get this */

	@Test
	public void testSingleCpuLoadObject() {
		logger.debug("Starting testSingleCpuLoadObject");
		try {
			com.ibm.lang.management.OperatingSystemMXBean ibmBean = ManagementFactory.getPlatformMXBean(com.ibm.lang.management.OperatingSystemMXBean.class);
			testMxBeanImpl(ibmBean);
		} catch (IllegalArgumentException e) {
			Assert.fail("com.ibm.lang.management.OperatingSystemMXBean is not available" + e, e);
		}
	}

	@Test
	public void testMxBean() {
		logger.debug("Starting testMxBean");
		java.lang.management.OperatingSystemMXBean osBean = ManagementFactory.getOperatingSystemMXBean();
		String beanClass = osBean.getClass().getName();
		logger.debug("osBean class = " + beanClass);
		Class<?> ibmBeanClass = com.ibm.lang.management.OperatingSystemMXBean.class;

		if (ibmBeanClass.isInstance(osBean)) {
			testMxBeanImpl((com.ibm.lang.management.OperatingSystemMXBean)osBean);
		} else {
			Assert.fail("OperatingSystemMXBean is wrong type: " + osBean.getClass().getName());
		}
	}

	private static void testMxBeanImpl(com.ibm.lang.management.OperatingSystemMXBean ibmBean) {
		double load = ibmBean.getSystemCpuLoad();
		if (load < 0.0) { /* normal case (insufficient time since last call) */
			validateLoad(load, true, CpuLoadCalculationConstants.ERROR_VALUE, "initial getSystemCpuLoad");
		} else {
			validateLoad(load, false, NO_ERROR, "initial getSystemCpuLoad");
		}
		delayMillis(MINIMUM_INTERVAL);
		load = ibmBean.getSystemCpuLoad();
		validateLoad(load, false, NO_ERROR, "getSystemCpuLoad after the minumum interval");

		load = ibmBean.getSystemCpuLoad();
		validateLoad(load, false, NO_ERROR, "getSystemCpuLoad immediately");

		delayMillis(MINIMUM_INTERVAL);
		load = ibmBean.getSystemCpuLoad();
		validateLoad(load, false, NO_ERROR, "getSystemCpuLoad after the minumum interval");

		delayMillis(1000);
		load = ibmBean.getSystemCpuLoad();
		validateLoad(load, false, NO_ERROR, "getSystemCpuLoad 1 second after previous call");

		delayMillis(1);
		load = ibmBean.getSystemCpuLoad();
		validateLoad(load, false, NO_ERROR, "getSystemCpuLoad 1 millisecond after previous call");
	}

	/**
	 * Do a sleep based on time in ns, which should be monotonic
	 * and consistent with the port library.
	 * @param intervalMs delay in ms
	 */
	private static void delayMillis(long intervalMs) {
		long endNanoTime = System.nanoTime() + (intervalMs * 1000000);
		long currentNanoTime;
		do {
			try {
				Thread.sleep(1);
			} catch (InterruptedException e) {
				Assert.fail("Unexpected InterruptedException");
			}
			currentNanoTime = System.nanoTime();
		} while (currentNanoTime < endNanoTime);
	}

	private static void validateLoad(double load, boolean expectError, double expectedErrorValue, String msg) {
		try {
			if (expectError) {
				AssertJUnit.assertEquals("unexpected error value", expectedErrorValue, load, 0);
			} else {
				AssertJUnit.assertTrue("load < 0%", load >= MIN_LOAD);
				AssertJUnit.assertTrue("load > 100%", load <= MAX_LOAD);
			}
		} finally {
			logger.debug(msg + "  load = " + load);
		}
	}
}
