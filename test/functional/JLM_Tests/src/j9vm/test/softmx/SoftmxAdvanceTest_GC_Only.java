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

package j9vm.test.softmx;

import org.testng.annotations.AfterMethod;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeMethod;
import org.testng.Assert;
import org.testng.AssertJUnit;
import j9vm.test.softmx.MemoryExhauster;

import java.lang.management.ManagementFactory;
import java.util.Iterator;
import java.util.List;


public class SoftmxAdvanceTest_GC_Only{

	private static final Logger logger = Logger.getLogger(SoftmxAdvanceTest_GC_Only.class);

	private static final String DISCLAIM_MEMORY = "-XX:+DisclaimVirtualMemory";

	private static final String NOT_DISCLAIM_MEMORY = "-XX:-DisclaimVirtualMemory";

	private static final String SOFTMX = "-Xsoftmx";

	private java.lang.management.MemoryMXBean mb = ManagementFactory.getMemoryMXBean();

	// cast downwards so that we can use the IBM-specific functions
	private com.ibm.lang.management.MemoryMXBean ibmMemoryMBean = (com.ibm.lang.management.MemoryMXBean) mb;

	private com.ibm.lang.management.OperatingSystemMXBean ibmOSMBean = (com.ibm.lang.management.OperatingSystemMXBean)ManagementFactory.getOperatingSystemMXBean();

	/*
	 * Start JVM with -XX:+DisclaimVirtualMemory or -XX:-DisclaimVirtualMemory, force memory used by the heap to 80% of current max heap size, get OS memory size( X )
	 * then reset max heap size to 50% of original size, trigger gc to shrink heap, validate that heap releases memory.
	 */
	@Test(groups = { "level.extended" })
	public void testDisclaimMemoryEffects_GC_Test(){

		long original_softmx_value =  ibmMemoryMBean.getMaxHeapSize();

		logger.debug("	Starting Object allocation until used memory reaches 80% of current max heap size.");

		boolean exhausted = new MemoryExhauster().usePercentageOfHeap(0.8);

		if ( exhausted == false ) {
			Assert.fail("Catch OutOfMemoryError before reaching 80% of current max heap size.");
		}

		logger.debug( "	Now we have used approximately 80% of current max heap size: " +  ibmMemoryMBean.getHeapMemoryUsage().getCommitted() + " bytes");

		//get current OS free memory size before reset softmx
		long preMemSize = ibmOSMBean.getFreePhysicalMemorySize();

		logger.debug( "	Before resetting softmx value, the OS free physical memory size is: "+ preMemSize + " bytes");

		long new_softmx_value = (long) (original_softmx_value * 0.5);
		logger.debug("	Resetting maximum heap size to 50% of original size: " + new_softmx_value);
		ibmMemoryMBean.setMaxHeapSize(new_softmx_value);

		logger.debug("	Forcing Aggressive GC. Will wait a maximum of 5 minutes for heap shrink..." );
		TestNatives.setAggressiveGCPolicy();

		long startTime = System.currentTimeMillis();
		boolean isShrink = false;

		//waiting for maximum 5 min (300000ms)
		while((System.currentTimeMillis() - startTime) < 300000 ){
			if (ibmMemoryMBean.getHeapMemoryUsage().getCommitted() <= new_softmx_value) {
				isShrink = true;
				logger.debug( "	PASS: Heap has shrunk to " + ibmMemoryMBean.getHeapMemoryUsage().getCommitted() + " bytes"
				+ " in " + (System.currentTimeMillis() - startTime) + " mSeconds");
				break;
			} else {
				try {
					Thread.sleep(2000);
					logger.debug( "	Current committed memory:  " + ibmMemoryMBean.getHeapMemoryUsage().getCommitted() + " bytes");
					logger.debug("	Heap did not shrink yet, forcing Aggressive GC again..." );
					TestNatives.setAggressiveGCPolicy();
				} catch (InterruptedException e) {
					e.printStackTrace();
					Assert.fail("	FAIL: Catch InterruptedException");
				}
			}
		}

		if(!isShrink){
			logger.warn(    "	Generate Java core dump after forcing GC.");
			com.ibm.jvm.Dump.JavaDump();
			com.ibm.jvm.Dump.HeapDump();
			com.ibm.jvm.Dump.SystemDump();
		}

		AssertJUnit.assertTrue("	FAIL: Heap can't shrink to the reset max heap size within 5 minutes!",isShrink);
	}
}

