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

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.AssertJUnit;
import java.io.IOException;
import java.lang.management.ManagementFactory;

import com.ibm.lang.management.MemoryMXBean;

@SuppressWarnings({ "nls", "restriction" })
@Test(groups = { "level.extended" })
public class SoftmxRASTest2 {

	public static final Logger logger = Logger.getLogger(SoftmxRASTest2.class);

	private final MemoryMXBean ibmMemoryMBean = (MemoryMXBean) ManagementFactory.getMemoryMXBean();

	/**
	 * RAS test case 2: Validate correct softmx value is reported in javacore, when set through JMX.
	 * Testcase: Start a secondary JVM without -Xsoftmx**** on command line, set softmx value in the program.
	 * Trigger OOM exception, verify that correct softmx value is reported in the javacore.
	 */
	@Test
	public void testSoftmx_RAS_Test_2() {
		try {
			final int MegaByte = 1024 * 1024;
			long maxHeapSize = ibmMemoryMBean.getMaxHeapSize() / MegaByte; // get the value in MB
			long maxHeapSizeForSecondaryJVM = maxHeapSize / 2;

			logger.info("Starting RAS testcase 2:");

			int exitValueOfSecondJVM = SoftmxRASTest1.startSecondJVM(maxHeapSizeForSecondaryJVM, -1, OOMGenerator_RAS_Test2.class);

			logger.debug("After expected crash, exit value: " + exitValueOfSecondJVM);

			logger.debug("Analyzing javacore..");

			long softmxValue_from_javacore = SoftmxRASTest1.extractSoftmxVal_From_Javacore() / MegaByte;
			long newSoftmxValue_set_via_jmx_by_OOMGeneratorWithSoftmxSet = maxHeapSizeForSecondaryJVM / 2;

			logger.debug("Softmx value found in javacore: " + softmxValue_from_javacore + "M");
			logger.debug("Softmx value set via JMX: " + newSoftmxValue_set_via_jmx_by_OOMGeneratorWithSoftmxSet + "M");

			AssertJUnit.assertTrue("Softmx value found in javacore is bigger than the value set via JMX",
					softmxValue_from_javacore <= newSoftmxValue_set_via_jmx_by_OOMGeneratorWithSoftmxSet);
		} catch (InterruptedException e) {
			logger.error("Unexpected exception occured" + e.getMessage(), e);
		} catch (IOException e) {
			logger.error("Unexpected exception occured" + e.getMessage(), e);
		}
	}

}
