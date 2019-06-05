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

import j9vm.test.softmx.MemoryExhauster;
import java.lang.management.ManagementFactory;

import org.testng.Assert;
import org.testng.log4testng.Logger;

public class RemoteTestServer {

	private static final Logger logger = Logger.getLogger(RemoteTestServer.class);

	// Limit the test execution within 5 minutes
	private static final long TIMEOUT = 5 * 60 * 1000;

	com.ibm.lang.management.MemoryMXBean ibmBean;

	/*
	 * Start the RemoteTestServer with softmx enabled and jmx enabled, for example:
	 * -Xmx1024m -Xsoftmx512m -Dcom.sun.management.jmxremote.port=9999 -Dcom.sun.management.jmxremote.authenticate=false
	 * -Dcom.sun.management.jmxremote.ssl=false
	 */

	public static void main(String[] args) {

		log("=========RemoteTestServer Starts!=========");

		java.lang.management.MemoryMXBean mb = ManagementFactory.getMemoryMXBean();

		// cast downwards so that we can use the IBM-specific functions
		com.ibm.lang.management.MemoryMXBean aBean = (com.ibm.lang.management.MemoryMXBean)mb;

		long preMx = aBean.getMaxHeapSize();
		long startTime = System.currentTimeMillis();

		MemoryExhauster memoryExhauster = new MemoryExhauster();

		//check every second to see if max heap size is reduced by 1024 bytes
		log("sleep until max heap size is reset to max heap size limit.");
		log("Current Max heap size: " + aBean.getMaxHeapSize());
		log("Max heap size limit:  " + aBean.getMaxHeapSizeLimit());
		while ((preMx != aBean.getMaxHeapSizeLimit()) && (System.currentTimeMillis() - startTime) < TIMEOUT) {
			try {
				preMx = aBean.getMaxHeapSize();
				Thread.sleep(100);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}

		//exist with -1 if max heap size is not changed during TIMEOUT
		if (preMx != aBean.getMaxHeapSizeLimit()) {
			log("Didn't receive signal - max heap size is set to max heap limit during " + TIMEOUT
					+ "ms! System exit with -1.");
			Assert.fail("RemoteTestServer: Didn't receive signal - max heap size is set to max heap limit during "
					+ TIMEOUT + "ms! System exit with -1.");
		}

		//once the max heap size is reduced by 1024 bytes, start allocate object to 80% of max heap size
		log("Current max heap size is " + aBean.getMaxHeapSize());

		memoryExhauster.usePercentageOfHeap(0.8);

		log("Now we have used approximately 80% of current max heap size:  " + aBean.getHeapMemoryUsage().getUsed());
		log("Current committed max heap size is " + aBean.getHeapMemoryUsage().getCommitted());

		//decrease max heap size 1024 bytes to notify SoftRemoteTest that allocation is done
		preMx = aBean.getMaxHeapSize() - 1024;
		aBean.setMaxHeapSize(preMx);

		//check every second to see if max heap size is reset to 50% of original max size;
		//the actual max heap size maybe not exactly equal to 50% of original size
		log("sleep until max heap size is reduced to 50% of original size.");
		while ((Math.abs(aBean.getMaxHeapSize() - (long)preMx * 0.5) > 1024)
				&& (System.currentTimeMillis() - startTime) < TIMEOUT) {
			try {
				log("Current max heap size is " + aBean.getMaxHeapSize());
				Thread.sleep(1000);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}

		//exist with -1 if max heap size is not changed during TIMEOUT
		if ((Math.abs(aBean.getMaxHeapSize() - (long)preMx * 0.5) > 1024)) {
			log("Didn't receive signal - max heap size is reduced to 50% of original size during " + TIMEOUT
					+ "ms! System exit with -1.");
			Assert.fail(
					"RemoteTestServer: Didn't receive signal - max heap size is reduced to 50% of original size during "
							+ TIMEOUT + "ms! System exit with -1.");
		}

		log("Force Aggressive GC. Waiting for heap shrink...");
		TestNatives.setAggressiveGCPolicy();

		boolean isShrink = false;

		//waiting for maximum 5 min (300000ms)
		while ((System.currentTimeMillis() - startTime) < 300000) {
			if ((aBean.getHeapMemoryUsage().getCommitted() - aBean.getMaxHeapSize()) <= 1024) {
				logger.debug("	Heap shrink to " + aBean.getHeapMemoryUsage().getCommitted() + " bytes" + " in "
						+ (System.currentTimeMillis() - startTime) + " mSeconds");
				isShrink = true;
				break;
			} else {
				try {
					Thread.sleep(2000);
					log("Current committed memory:  " + aBean.getHeapMemoryUsage().getCommitted() + " bytes");
					log("max heap size " + aBean.getMaxHeapSize());
					log("Force Aggressive GC. Waiting for heap shrink...");
					TestNatives.setAggressiveGCPolicy();
				} catch (InterruptedException e) {
					isShrink = false;
					e.printStackTrace();
				}
			}
		}

		//if heap size shrank to reset max heap size, continue test heap can't expand beyond current max heap size
		if (isShrink) {
			//Try to use 120% of reset max heap size, expect to get OutOfMemoryError
			log("Starting Object allocation until OOM error or used memory reaching 120% of current max heap size.");

			boolean OOMNotReached = memoryExhauster.usePercentageOfHeap(1.2);

			if (OOMNotReached == false) {
				log("PASS: Heap can't expand beyond reset max heap size. Catch OutOfMemoryError.");
			}

			if (OOMNotReached) {
				log("FAIL: Heap can expand beyond reset max heap size! Generate Java core dump.");
				com.ibm.jvm.Dump.JavaDump();
				com.ibm.jvm.Dump.HeapDump();
				com.ibm.jvm.Dump.SystemDump();
			} else {
				//increase max heap size 1024 bytes to notify SoftmxRemoteTest that expected OOM is thrown
				aBean.setMaxHeapSize(aBean.getMaxHeapSize() + 1024);
			}

		} else {
			log("	FAIL: Heap can't shrink to the reset max heap size within 5 minutes!");
		}

		try {
			Thread.sleep(60000);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}

		log("RemoteTestServer Stops!");
	}

	public RemoteTestServer(com.ibm.lang.management.MemoryMXBean aBean) {
		ibmBean = aBean;
	}

	private static void log(String aString) {
		logger.debug("RemoteTestServer: " + aString);
	}
}
