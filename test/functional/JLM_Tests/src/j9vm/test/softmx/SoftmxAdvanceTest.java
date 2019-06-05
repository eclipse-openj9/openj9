/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

package j9vm.test.softmx;

import org.testng.annotations.Optional;
import org.testng.annotations.Parameters;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.AssertJUnit;
import j9vm.test.softmx.MemoryExhauster;

import java.lang.management.ManagementFactory;
import java.lang.management.RuntimeMXBean;
import java.util.Iterator;
import java.util.List;

public class SoftmxAdvanceTest {

	private static final Logger logger = Logger.getLogger(SoftmxAdvanceTest.class);

	private static final String DISCLAIM_MEMORY = "-XX:+DisclaimVirtualMemory";

	private static final String NOT_DISCLAIM_MEMORY = "-XX:-DisclaimVirtualMemory";

	private static final String SOFTMX = "-Xsoftmx";

	private java.lang.management.MemoryMXBean mb = ManagementFactory.getMemoryMXBean();

	// cast downwards so that we can use the IBM-specific functions
	private com.ibm.lang.management.MemoryMXBean ibmMemoryMBean = (com.ibm.lang.management.MemoryMXBean)mb;

	private com.ibm.lang.management.OperatingSystemMXBean ibmOSMBean = (com.ibm.lang.management.OperatingSystemMXBean)ManagementFactory.getOperatingSystemMXBean();

	private static int returnVal = 0;

	@Parameters({ "SoftmxAdvanceTest_iteration" })
	@Test(groups = { "level.extended" })
	public void testDisclaimMemoryEffects(@Optional("5") int iter) {
		SoftmxAdvanceTest test = new SoftmxAdvanceTest();
		int iteration = iter;
		boolean passed = false;

		for (int i = 0; i < iteration; i++) {
			runTestDisclaimMemoryEffects();
			if (returnVal == 0) {
				passed = true;
				break;
			}
		}
		AssertJUnit.assertTrue("Test has failed after " + iteration + " attempts", passed);
	}

	/*
	 * start JVM with -XX:+DisclaimVirtualMemory or -XX:-DisclaimVirtualMemory, force memory used by the heap to 80% of current max heap size, get OS memory size( X )
	 *  then reset max heap size to 50% of original size, trigger gc to shrink heap, validate memory released back to OS (memory size >X)
	 *  with -XX:+DisclaimVirtualMemory; validate memory not returned with -XX:-DisclaimVirtualMemory.
	 *
	 */
	private void runTestDisclaimMemoryEffects() {

		long original_softmx_value = ibmMemoryMBean.getMaxHeapSize();

		logger.debug("	Starting Object allocation until used memory reaches 80% of current max heap size.");

		boolean exhausted = new MemoryExhauster().usePercentageOfHeap(0.8);

		if (exhausted == false) {
			returnVal = -1;
			return;
		}

		logger.debug("	Now we have used approximately 80% of current max heap size: "
				+ ibmMemoryMBean.getHeapMemoryUsage().getCommitted() + " bytes");

		//get current OS free memory size before reset softmx
		long preMemSize = ibmOSMBean.getFreePhysicalMemorySize();

		logger.debug("	Before resetting softmx value, the OS free physical memory size is: " + preMemSize + " bytes");

		long new_softmx_value = (long)(original_softmx_value * 0.5);
		logger.debug("	Resetting maximum heap size to 50% of original size: " + new_softmx_value);
		ibmMemoryMBean.setMaxHeapSize(new_softmx_value);

		logger.debug("	Forcing Aggressive GC. Will wait a maximum of 5 minutes for heap shrink...");
		TestNatives.setAggressiveGCPolicy();

		/* figure out DisclaimVirtualMemory setting from VM arguments.
		 * The arguments will contain -Xsoftmx and -XX:+DisclaimVirtualMemory/-XX:-DisclaimVirtualMemory
		 */
		boolean enableDisclaimMemory = false;
		boolean disableDisclaimMemory = false;
		boolean softmxExist = false;

		RuntimeMXBean RuntimemxBean = ManagementFactory.getRuntimeMXBean();
		List<String> arguments = RuntimemxBean.getInputArguments();

		Iterator it = arguments.iterator();

		while (it.hasNext()) {
			String s = (String)it.next();
			if (s.equalsIgnoreCase(DISCLAIM_MEMORY)) {
				enableDisclaimMemory = true;
			}
			if (s.equalsIgnoreCase(NOT_DISCLAIM_MEMORY)) {
				disableDisclaimMemory = true;
			}
			if (s.contains(SOFTMX)) {
				softmxExist = true;
			}
		}

		long startTime = System.currentTimeMillis();
		boolean isShrink = false;

		//waiting for maximum 5 min (300000ms)
		while ((System.currentTimeMillis() - startTime) < 300000) {
			if (ibmMemoryMBean.getHeapMemoryUsage().getCommitted() < new_softmx_value) {
				if (enableDisclaimMemory) {
					if (ibmOSMBean.getFreePhysicalMemorySize() > preMemSize) {
						isShrink = true;
					}
				} else {
					isShrink = true;
				}
				if (isShrink == true) {
					logger.debug("	PASS: Heap has shrunk to " + ibmMemoryMBean.getHeapMemoryUsage().getCommitted()
							+ " bytes" + " in " + (System.currentTimeMillis() - startTime) + " mSeconds");
					break;
				}
			} else {
				try {
					Thread.sleep(2000);
					logger.debug("	Current committed memory:  " + ibmMemoryMBean.getHeapMemoryUsage().getCommitted()
							+ " bytes");
					logger.debug("	Heap did not shrink yet, forcing Aggressive GC again...");
					TestNatives.setAggressiveGCPolicy();
				} catch (InterruptedException e) {
					logger.error("	FAIL: Catch InterruptedException", e);
					returnVal = -1;
					return;
				}
			}
		}

		if (!isShrink) {
			logger.warn("	Generate Java core dump after forcing GC.");
			com.ibm.jvm.Dump.JavaDump();
			com.ibm.jvm.Dump.HeapDump();
			com.ibm.jvm.Dump.SystemDump();
		}

		if (isShrink == false) {
			logger.error("	FAIL: Heap can't shrink to the reset max heap size within 5 minutes!");
			returnVal = -1;
			return;
		}

		/* add a waiting time between performing GC and checking for free physical memory in the OS*/
		try {
			logger.debug("Going to sleep for 5 seconds after Aggressive GC..");
			Thread.currentThread().sleep(5000);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}

		//get current OS free memory size after forcing heap to shrink to the reset softmx
		long postMemSize = ibmOSMBean.getFreePhysicalMemorySize();

		logger.debug(
				"	After resetting softmx and performing an aggressive GC to force heap to shrink, the OS free physical memory size is: "
						+ postMemSize + " bytes");

		/*if -Xsoftmx is not present on the command line, -XX option will be ignored, use default setting, which are different on different OS's.
		 * On Windows, Linux & AIX - set default value to advise OS about vmem that is no longer needed
		 * z/OS - set default value to not advise OS about vmem that is no longer needed (but this functionality is not yet implemented on z/OS, and may not be)*/
		long diff = postMemSize - preMemSize;

		boolean isMemRelease = false;

		if (softmxExist) {
			if (disableDisclaimMemory) {
				/*If the released memory is less than 20% of the original softmx value, the test assumes that
				 *  memory release to the OS is being prevented*/
				isMemRelease = true;
				if (diff < new_softmx_value * 0.20) {
					logNotRelease(postMemSize, preMemSize);
					isMemRelease = false;
				}
				if (isMemRelease != false) {
					logger.error("	FAIL: Memory shouldn't release back to OS! Post memory size " + postMemSize
							+ " is greater than previous memory size " + preMemSize);
					returnVal = -1;
					return;
				}
			} else {
				/*If the released memory is more than 10% of the original softmx value, the test assumes that
				 *  disclaim memory is happening*/
				if (diff > new_softmx_value * 0.10) {
					logRelease(postMemSize, preMemSize);
					isMemRelease = true;
				}

				if (isMemRelease != true) {
					logger.error("	FAIL: Memory didn't release back to OS! Post memory size " + postMemSize
							+ " is less than or equal to previous memory size " + preMemSize);
					returnVal = -1;
					return;
				}
			}
		}
		returnVal = 0;
	}

	private void logRelease(long postMemSize, long preMemSize) {
		logger.debug("	PASS: Memory is released back to OS! Post memory size " + postMemSize
				+ " is greater than previous memory size " + preMemSize + " by at least 10% of the new softmx value.");
	}

	private void logNotRelease(long postMemSize, long preMemSize) {
		logger.debug(
				"	PASS: Memory is not released back to OS! The difference between post-memory size + " + postMemSize
						+ " and " + "pre-memory size " + preMemSize + " is less than 20% of the new softmx value.");
	}

}
