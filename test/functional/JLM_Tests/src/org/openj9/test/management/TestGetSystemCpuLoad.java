/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
	private boolean supported;
	private final double MIN_LOAD = 0.0;
	private final double MAX_LOAD = 1.0;
	private final double NO_ERROR = -100; /* should never get this */

	@BeforeMethod
	protected void setUp() throws Exception {
		String osName = System.getProperty("os.name");
		if ((null == osName) || osName.equalsIgnoreCase("z/OS")) {
			supported = false;
		} else {
			supported = true;
		}
	}

	@Test
	public void testSingleCpuLoadObject() {
		logger.debug("Starting testSingleCpuLoadObject");
		com.ibm.lang.management.OperatingSystemMXBean ibmBean;
		try {
			ibmBean = ManagementFactory.getPlatformMXBean(com.ibm.lang.management.OperatingSystemMXBean.class);
		} catch (IllegalArgumentException e) {
			Assert.fail("com.ibm.lang.management.OperatingSystemMXBean is not available" + e, e);
			return;
		}
		double load = ibmBean.getSystemCpuLoad();
		if (!supported) {
			validateLoad(load, true, CpuLoadCalculationConstants.UNSUPPORTED_VALUE, "initial call");
			return;
		} else {
			validateLoad(load, true, CpuLoadCalculationConstants.ERROR_VALUE, "initial call");
		}
		load = ibmBean.getSystemCpuLoad();
		if (load < 0.0) { /* normal case (insufficient time since last call) */
			validateLoad(load, true, CpuLoadCalculationConstants.ERROR_VALUE, "call getSystemCpuLoad immediately after the previous call");
		} else { /* possible but unlikely: test stalled for an extended time */
			validateLoad(load, false, NO_ERROR, "call getSystemCpuLoad immediately after the previous call");
		}

		delayMillis(MINIMUM_INTERVAL);
		load = ibmBean.getSystemCpuLoad();
		validateLoad(load, false, NO_ERROR, "call getSystemCpuLoad after the minumum interval");

		delayMillis(1000);
		load = ibmBean.getSystemCpuLoad();
		validateLoad(load, false, NO_ERROR, "call getSystemCpuLoad 1 second after previous call");

		delayMillis(1);
		load = ibmBean.getSystemCpuLoad();
		validateLoad(load, false, NO_ERROR, "call getSystemCpuLoad 1 millisecond after previous call");
	}

	@Test
	public void testMxBean() {
		logger.debug("Starting testMxBean");
		java.lang.management.OperatingSystemMXBean osBean = ManagementFactory.getOperatingSystemMXBean();
		String beanClass = osBean.getClass().getName();
		logger.debug("osBean class=" + beanClass);
		Class<?> ibmBeanClass = com.ibm.lang.management.OperatingSystemMXBean.class;

		if (ibmBeanClass.isInstance(osBean)) {
			com.ibm.lang.management.OperatingSystemMXBean ibmBean1 =
					(com.ibm.lang.management.OperatingSystemMXBean) osBean;
			double load = ibmBean1.getSystemCpuLoad();
			if (!supported) {
				validateLoad(load, true, CpuLoadCalculationConstants.UNSUPPORTED_VALUE, "initial getSystemCpuLoad");
				return;
			} else if (load < 0.0) { /* normal case (insufficient time since last call) */
				validateLoad(load, true, CpuLoadCalculationConstants.ERROR_VALUE, "initial getSystemCpuLoad");
			} else {
				validateLoad(load, false, NO_ERROR, "initial getSystemCpuLoad");
			}
			delayMillis(MINIMUM_INTERVAL);
			load = ibmBean1.getSystemCpuLoad();
			validateLoad(load, false, NO_ERROR, "getSystemCpuLoad after the minumum interval");

			load = ibmBean1.getSystemCpuLoad();
			validateLoad(load, false, NO_ERROR, "getSystemCpuLoad again");

			delayMillis(MINIMUM_INTERVAL);
			load = ibmBean1.getSystemCpuLoad();
			validateLoad(load, false, NO_ERROR, "getSystemCpuLoad again");

			delayMillis(1000);
			load = ibmBean1.getSystemCpuLoad();
			validateLoad(load, false, NO_ERROR, "getSystemCpuLoad after 1 second");
		} else {
			Assert.fail("OperatingSystemMXBean is wrong type: " + osBean.getClass().getName());
		}
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

	private void validateLoad(double load, boolean expectError, double expectedErrorValue, String msg) {
		try {
			if (expectError) {
				AssertJUnit.assertEquals("unexpected error value", expectedErrorValue, load, 0);
			} else {
				AssertJUnit.assertTrue("load < 0%", load >= MIN_LOAD);
				AssertJUnit.assertTrue("load > 100 %", load <= MAX_LOAD);
			}
		} finally {
			logger.debug(msg + " load = " + load);
		}
	}

}
