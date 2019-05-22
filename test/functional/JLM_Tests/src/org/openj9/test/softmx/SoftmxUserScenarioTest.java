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
package org.openj9.test.softmx;

import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.AssertJUnit;
import java.io.IOException;
import java.lang.management.ManagementFactory;
import javax.management.JMX;
import javax.management.MBeanServer;
import javax.management.ObjectName;
import org.apache.commons.exec.CommandLine;
import org.apache.commons.exec.DefaultExecutor;
import org.apache.commons.exec.ExecuteException;


@Test(groups = { "level.extended" })
public class SoftmxUserScenarioTest {

	private static final Logger logger = Logger.getLogger(SoftmxUserScenarioTest.class);

	private com.ibm.lang.management.OperatingSystemMXBean ibmOSMBean = (com.ibm.lang.management.OperatingSystemMXBean)ManagementFactory.getOperatingSystemMXBean();

	/**
	 * Start with -Xmx larger than (memory available + swap) on machine,
	 * softmx is 1/2 that value which is less than  (memory available + swap),
	 * excepted result: JVM starts up and runs ok.
	 */
	@Test
	public void testSoftmxUserScenario() {
		try {
			long totalPhysicalMemory = ibmOSMBean.getTotalPhysicalMemory() / ( 1024 * 1024 ) ; // get the value in MB
			long totalSwapSpace = ibmOSMBean.getTotalSwapSpaceSize() / (1024 * 1024 ); // get the value in MB

			logger.debug( "Total physical memory : " + totalPhysicalMemory );
			logger.debug( "Total swap space : " + totalSwapSpace );

			long new_Xmx_value = totalPhysicalMemory + totalSwapSpace + 100;

			long new_softmx_value = (long) ((totalPhysicalMemory + totalSwapSpace)/2);

			logger.info( "Starting a new JVM using -Xmx = "+ new_Xmx_value + " and -Xsoftmx = " + new_softmx_value );

			int exitValueOfSecondJVM = startSecondJVM( new_Xmx_value, new_softmx_value, HelperClass.class );

			logger.info("Result exit value :  " + exitValueOfSecondJVM);

			AssertJUnit.assertTrue("FAIL: JVM did not start up and run ok when we set a -Xmx larger than (memory available + swap) and then " +
					"set a softmx 1/2 that value which is less than ( memory available + swap) ", exitValueOfSecondJVM == 0 );
		} catch ( IOException e ) {
			e.printStackTrace();
		} catch ( InterruptedException e ) {
			e.printStackTrace();
		}
	}

	/**
	 * Starts a JVM in a subprocess
	 * @param xmxVal : -Xmx value to use in the command line of the JVM to be spawned
	 * @param softmxVal : -Xsoftmx value to use in the command line of the JVM to be spawned
	 * @param classToRun : The class that should be run using java
	 * @return : return code of the sub-process which runs the JVM
	 * @throws IOException
	 * @throws InterruptedException
	 */
	private static int startSecondJVM( long xmxVal, long softmxVal, Class classToRun) throws IOException, InterruptedException {
		String classpath = System.getProperty("java.class.path");
		String path = System.getProperty("java.home") + "/bin/java";
		String cmdLineStr = null;
		int exitValue = -1;

		if ( softmxVal == -1 ) {
			cmdLineStr = path + " -cp "+ classpath + " -Xmx" + xmxVal + "m " + classToRun.getCanonicalName();
		} else {
			cmdLineStr = path + " -cp "+ classpath + " -Xmx" + xmxVal + "m -Xsoftmx" + softmxVal + "m " + classToRun.getCanonicalName();
		}

		logger.info("Executing cmd : " + cmdLineStr);

		CommandLine cmdLine = CommandLine.parse(cmdLineStr);
		DefaultExecutor executor = new DefaultExecutor();

		try {
			exitValue = executor.execute(cmdLine);
		} catch ( ExecuteException e ) {
			logger.warn("Exception throw from executing the second JVM:" + e, e);
		}
		return exitValue;
	}
}
