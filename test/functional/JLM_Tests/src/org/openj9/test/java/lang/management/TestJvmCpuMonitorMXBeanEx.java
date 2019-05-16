/*******************************************************************************
 * Copyright (c) 2015, 2019 IBM Corp. and others
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

package org.openj9.test.java.lang.management;

import org.testng.annotations.AfterClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeClass;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.io.File;
import java.io.IOException;
import java.lang.management.ManagementFactory;
import java.net.MalformedURLException;
import java.util.ArrayList;

import javax.management.JMX;
import javax.management.MBeanServer;
import javax.management.MBeanServerConnection;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.remote.JMXConnector;
import javax.management.remote.JMXConnectorFactory;
import javax.management.remote.JMXServiceURL;

import com.ibm.lang.management.*;

@Test(groups = { "level.sanity" })
public class TestJvmCpuMonitorMXBeanEx {
	private static Logger logger = Logger.getLogger(TestJvmCpuMonitorMXBeanEx.class);
	private static Process remoteServer;
	private static ChildWatchdog watchdog;
	private final static int MaxIterations = 5;
	private ObjectName mxbeanName = null;
	private JvmCpuMonitorMXBean jcmBean = null;

	@BeforeClass
	public void setUp() throws Exception {
		/* We need the object name in any case - remote as well as local. */
		try {
			mxbeanName = new ObjectName("com.ibm.lang.management:type=JvmCpuMonitor");
		} catch (MalformedObjectNameException e) {
			e.printStackTrace();
			Assert.fail("MalformedObjectNameException!");
		}
	}

	@AfterClass
	public void tearDown() throws Exception {
	}

	@Test
	public void testLocal() {
		MBeanServer mbeanServer = null;

		/* Local Statistics */
		try {
			mbeanServer = ManagementFactory.getPlatformMBeanServer();
			if (true != mbeanServer.isRegistered(mxbeanName)) {
				Assert.fail("JvmCpuMonitorMXBean is not registered. " + "Cannot Proceed.");
			}

			jcmBean = JMX.newMXBeanProxy(mbeanServer, mxbeanName, JvmCpuMonitorMXBean.class);
		} catch (Exception e) {
			Assert.fail("Exception occurred in setting up local environment.");
		}

		/* Display the statistics of the JvmCpuMonitorMXBean */
		display_cpuUsageInfo(jcmBean);

	}

	@Test
	public void testRemote() {
		int retryCounter = 0;
		boolean isConnected = false;
		JMXServiceURL urlForRemoteMachine = null;
		JMXConnector connector = null;
		MBeanServerConnection mbsc = null;

		/*
		 * A Start the remote server if it is not running yet. Also, attach a
		 * watch dog to this, to bail out, in case it hangs.
		 */
		if (null == remoteServer) {
			startRemoteServer();
			watchdog = new ChildWatchdog(remoteServer);
		}

		/* Try connecting to the server; retry until limit reached. */
		while (retryCounter < 10 && !isConnected) {
			retryCounter++;
			try {
				urlForRemoteMachine = new JMXServiceURL("service:jmx:rmi:///jndi/rmi://localhost:9999/jmxrmi");

				/*
				 * Connect to remote host and create a proxy to be used to get
				 * an JvmCpuMonitorMXBean instance.
				 */
				connector = JMXConnectorFactory.connect(urlForRemoteMachine, null);
				mbsc = connector.getMBeanServerConnection();

				/*
				 * Obtain an mbean handle using the connection and the object
				 * name for the class JvmCpuMonitorMXBean.
				 */
				jcmBean = JMX.newMXBeanProxy(mbsc, mxbeanName, JvmCpuMonitorMXBean.class);
				if (true != mbsc.isRegistered(mxbeanName)) {
					Assert.fail("JvmCpuMonitorMXBean is not registered. Cannot Proceed");
				}

				/*
				 * If we reached here, it means we are connected (no connection
				 * failure exception thrown).
				 */
				isConnected = true;

			} catch (MalformedURLException e) {
				e.printStackTrace();
				Assert.fail("Please check the url supplied to JMXServiceURL!");
			} catch (IOException e) {

				/*
				 * Waiting 1000 ms before retrying to connect to remote server.
				 */
				logger.error("Failed connecting. Retry " + retryCounter + " after 1000 ms.");

				try {
					Thread.sleep(1000);
				} catch (InterruptedException ie) {
					ie.printStackTrace();
					Assert.fail("Exception occurred while sleeping thread: " + ie.getMessage());
				}
			} catch (Exception e) {
				Assert.fail("Exception occurred in setting up remote environment.");
			}
		}

		try {
			/* Display the statistics of the JvmCpuMonitorMXBean */
			display_cpuUsageInfo(jcmBean);
		} finally {
			try {
				connector.close();
			} catch (Throwable t) {
				t.printStackTrace();
				Assert.fail("Failed to close JMX connection");
			}
			stopRemoteServer();
		}
	}

	/**
	 * Function retrieves JVM Cpu Usage details of various thread categories.
	 * and prints the same.
	 *
	 * @param jcmBean
	 *            The JvmCpuMonitorMXBean instance that has already been
	 *            initialized.
	 *
	 */
	private static void display_cpuUsageInfo(JvmCpuMonitorMXBean jcmBean) {
		JvmCpuMonitorInfo jcmInfo1 = new JvmCpuMonitorInfo();
		JvmCpuMonitorInfo jcmInfo2 = new JvmCpuMonitorInfo();
		JvmCpuMonitorInfo currJcmInfo = null;
		JvmCpuMonitorInfo prevJcmInfo = null;
		long adiff = 0;
		long tdiff = 0;
		long sdiff = 0;
		long othdiff = 0;
		long timestamp = 0;
		long applicationCpuTime = 0;
		long resourceMonitorCpuTime = 0;
		long systemJvmCpuTime = 0;
		long gcCpuTime = 0;
		long jitCpuTime = 0;
		long totalCpuTime = 0;
		float apc = 0;
		float tpc = 0;
		float spc = 0;
		float gcpc = 0;
		float jitpc = 0;
		float othpc = 0;

		logger.debug("    timestamp      ResourceM     RM%       App-Cpu        AC%       Sys-Cpu         SC%       GC-Cpu%  JIT-Cpu%  Oth-Cpu%");
		logger.debug("----------------  ----------   ------  --------------  --------  --------------   --------  ---------------------------------");

		for (int i = 0; i < MaxIterations; i++) {

			if ((i % 2) == 0) {
				jcmInfo1 = jcmBean.getThreadsCpuUsage(jcmInfo1);
				currJcmInfo = jcmInfo1;
				prevJcmInfo = jcmInfo2;
			} else {
				jcmInfo2 = jcmBean.getThreadsCpuUsage(jcmInfo2);
				currJcmInfo = jcmInfo2;
				prevJcmInfo = jcmInfo1;
			}

			AssertJUnit.assertTrue(currJcmInfo != null);
			AssertJUnit.assertTrue(prevJcmInfo != null);

			timestamp = currJcmInfo.getTimestamp();
			applicationCpuTime = currJcmInfo.getApplicationCpuTime();
			resourceMonitorCpuTime = currJcmInfo.getResourceMonitorCpuTime();
			systemJvmCpuTime = currJcmInfo.getSystemJvmCpuTime();
			gcCpuTime = currJcmInfo.getGcCpuTime();
			jitCpuTime = currJcmInfo.getJitCpuTime();

			AssertJUnit.assertTrue(applicationCpuTime >= 0);
			AssertJUnit.assertTrue(resourceMonitorCpuTime >= 0);
			AssertJUnit.assertTrue(systemJvmCpuTime >= 0);
			AssertJUnit.assertTrue(gcCpuTime >= 0);
			AssertJUnit.assertTrue(jitCpuTime >= 0);

			if (i > 0) {
				adiff = applicationCpuTime - prevJcmInfo.getApplicationCpuTime();
				tdiff = resourceMonitorCpuTime - prevJcmInfo.getResourceMonitorCpuTime();
				sdiff = systemJvmCpuTime - prevJcmInfo.getSystemJvmCpuTime();
				totalCpuTime = sdiff + tdiff + adiff;
				apc = ((float)adiff / (float)totalCpuTime) * 100;
				tpc = ((float)tdiff / (float)totalCpuTime) * 100;
				spc = ((float)sdiff / (float)totalCpuTime) * 100;

				if (sdiff > 0) {
					gcpc = ((float)(gcCpuTime - prevJcmInfo.getGcCpuTime()) / (float)sdiff) * 100;
					jitpc = ((float)(jitCpuTime - prevJcmInfo.getJitCpuTime()) / (float)sdiff) * 100;
					othdiff = systemJvmCpuTime - (gcCpuTime + jitCpuTime) - (prevJcmInfo.getSystemJvmCpuTime()
							- (prevJcmInfo.getGcCpuTime() + prevJcmInfo.getJitCpuTime()));
					othpc = ((float)othdiff / (float)sdiff) * 100;
					System.out.printf("%16d %11d %7.2f%% %14d    %7.2f%% %14d   %8.2f%%    [%7.2f%% %7.2f%% %7.2f%%]\n",
							timestamp, resourceMonitorCpuTime, tpc, applicationCpuTime, apc, systemJvmCpuTime, spc,
							gcpc, jitpc, othpc);
				} else {
					System.out.printf(
							"%16d %11d %7.2f%% %14d    %7.2f%% %14d   %8.2f%%    [    -        -         -  ]\n",
							timestamp, resourceMonitorCpuTime, tpc, applicationCpuTime, apc, systemJvmCpuTime, spc);
				}

			} else {
				System.out.printf("%16d %11d\t  -   %14d \t  -   %14d\t-        [    -        -         -  ]\n",
						timestamp, resourceMonitorCpuTime, applicationCpuTime, systemJvmCpuTime);
			}

			try {
				Thread.sleep(1000);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}

	}

	/**
	 * Internal function: Starts a remote server to connect to.
	 */
	private static void startRemoteServer() {
		logger.info("Start Remote Server!");
		ArrayList<String> argBuffer = new ArrayList<String>();
		char fs = File.separatorChar;
		String javaExec = System.getProperty("java.home") + fs + "bin" + fs + "java";
		argBuffer.add(javaExec);
		argBuffer.add("-classpath");
		argBuffer.add(System.getProperty("java.class.path"));
		argBuffer.add("-Dcom.sun.management.jmxremote.port=9999");
		argBuffer.add("-Dcom.sun.management.jmxremote.authenticate=false");
		argBuffer.add("-Dcom.sun.management.jmxremote.ssl=false");
		argBuffer.add(RemoteServer.class.getName());
		String cmdLine[] = new String[argBuffer.size()];
		argBuffer.toArray(cmdLine);
		String cmds = "";
		for (int i = 0; i < cmdLine.length; i++) {
			cmds = cmds + cmdLine[i] + " ";
		}

		logger.info(cmds);
		try {
			remoteServer = Runtime.getRuntime().exec(cmds);
		} catch (IOException e) {
			e.printStackTrace();
			Assert.fail("Failed to launch child process " + remoteServer);
		}

		try {
			Thread.sleep(10);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}

	/**
	 * Internal function: Stops a remote server.
	 */
	private static void stopRemoteServer() {
		watchdog.interrupt();
		remoteServer.destroy();
		try {
			Thread.sleep(10000);
		} catch (InterruptedException e) {
			e.printStackTrace();
		}
	}

}

class ChildWatchdog extends Thread {
	Process child;

	public ChildWatchdog(Process child) {
		super();
		this.child = child;
	}

	@Override
	public void run() {
		try {
			sleep(30000);
			child.destroy();
			System.err.println("Child Hung");
		} catch (InterruptedException e) {
			return;
		}
	}
}
