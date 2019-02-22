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
package org.openj9.test.attachAPI;

import org.testng.Assert;
import org.testng.annotations.AfterMethod;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.Test;

@SuppressWarnings({"nls","boxing"})
public class AttachAPIStress extends AttachApiTest {
	public final int SEQ_ITERATIONS = 64;
	public final int MULT_ITERATIONS = 6;
	/* More than 32 parallel VMs causes out of memory errors on some machines */
	public final int PAR_ITERATIONS = Integer.getInteger("PAR_ITERATIONS", 4);
	public final int PAR_INSTANCES = Integer.getInteger("PAR_INSTANCES", 4);
	public final int MULT_ATTACHES = 100;
	public final String SEQ_ENABLE = System.getProperty("SEQ_ENABLE", "yes");
	public final String MULT_ENABLE = System.getProperty("MULT_ENABLE", "yes");
	public final String PAR_ENABLE = System.getProperty("PAR_ENABLE", "yes");
	private boolean printLogs = true;


	@AfterMethod
	protected void tearDown() {
		if (printLogs) {
			TargetManager.dumpLogs(printLogs); /*
												 * print the logs if test
												 * failed, then delete them
												 */
		}
	}

	@BeforeMethod
	protected void setUp() {
		logger.debug("Attaching process is "
				+ TargetManager.getProcessId());
		if (!TargetManager.waitForAttachApiInitialization()) {
			TargetManager.dumpLogs(true);
			Assert.fail("main process did not initialize attach API");
		}
		TargetManager.dumpLogs(false); /* delete the old logs */
		printLogs = true; /* if test passes, it resets this flag */
	}

	@Test
	public void testSequentialLaunches() {
		setVmOptions("-Dcom.ibm.tools.attach.logging=yes");
		if (!SEQ_ENABLE.equalsIgnoreCase("yes")) {
			logger.debug("skipped");
			return;
		}
		logger.debug("starting " + testName);
		for (int i = 0; i < SEQ_ITERATIONS; ++i) {
			launchAndTestTargets(1);
			logger.debug("waiting");
		}
		printLogs = false;
	}

	@Test
	public void testMultipleAttaches() {
		if (!MULT_ENABLE.equalsIgnoreCase("yes")) {
			logger.debug("skipped");
			return;
		}
		logger.debug("starting " + testName);
		for (int i = 0; i < MULT_ITERATIONS; ++i) {
			launchAndTestTargets(i, MULT_ATTACHES);
			logger.debug("waiting");
		}
		printLogs = false;
	}

	@Test
	public void testParallelLaunches() {
		if (!PAR_ENABLE.equalsIgnoreCase("yes")) {
			logger.debug("skipped");
			return;
		}

		logger.debug("starting " + testName);
		setVmOptions("-Xmx16M");
		setVmOptions("-Xms16M");
		int instances = PAR_INSTANCES;
		for (int i = 0; i < PAR_ITERATIONS; ++i) {
			logger.debug("\nlaunch " + instances);
			launchAndTestTargets(instances);
			instances = instances * 2;
		}
		printLogs = false;
	}

}
