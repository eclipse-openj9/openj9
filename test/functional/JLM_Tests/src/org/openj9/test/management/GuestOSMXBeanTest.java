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

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.openj9.test.management.util.ChildWatchdog;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.net.MalformedURLException;
import java.util.ArrayList;
import java.util.Scanner;

import javax.management.JMX;
import javax.management.MBeanServer;
import javax.management.MBeanServerConnection;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import javax.management.remote.JMXConnector;
import javax.management.remote.JMXConnectorFactory;
import javax.management.remote.JMXServiceURL;

import java.lang.management.ManagementFactory;

import com.ibm.virtualization.management.*;


/**
 * Class for testing the API that are provided as part of the GuestOSMXBean
 *
 */
@Test(groups = { "level.extended" })
public class GuestOSMXBeanTest {

	private static final Logger logger = Logger.getLogger(GuestOSMXBeanTest.class);

	private static Process remoteServer;
	private static ChildWatchdog watchdog;

	/*
	 * Strings that are looked for in exception messages to check if they are
	 * actually a valid error or not.
	 * error code reference from `j9porterror.h`
	 */

	private static final String VMWARE_ERROR = "VMWare Guest SDK Open failed";
	private static final String LPARCFG_MEM_UNSUPPORTED_ERROR = "-812";
	private static final String NOT_SUPPORTED_ERROR = "-806";
	private static final String NO_HYPERVISOR_ERROR = "-807";
	private static final String HYPFS_UPDATE_OPEN_FAILED_ERROR = "-828";
	private static final String LPARCFG_FILE = "/proc/ppc64/lparcfg";
	private static String HypervisorName;

	/**
	 * Starting point for the test program. Invokes the memory and processor test routines and
	 * indicates success or failure accordingly.
	 *
	 * @param args
	 */
	@Test
	public static void runGuestOSMXBeanTest() {
		int retryCounter = 0;
		boolean localTest = true;
		boolean isConnected = false;
		boolean error = false;

		JMXServiceURL urlForRemoteMachine = null;
		JMXConnector connector = null;
		MBeanServerConnection mbeanConnection = null;
		MBeanServer mbeanServer = null;
		ObjectName mxbeanName = null;
		GuestOSMXBean mxbeanProxy = null;

		logger.info(" ---------------------------------------");
		logger.info(" Starting the GuestOSMXBean API tests....");
		logger.info(" ---------------------------------------");

		/* Check and set flag 'localTest'. If the user specified 'local' or nothing, select local testing,
		 * while under explicit specification of the option 'remote' do we platform remote testing.
		 */
		if (System.getProperty("RUNLOCAL").equalsIgnoreCase("false")) {
			localTest = false;
		} else if (!System.getProperty("RUNLOCAL").equalsIgnoreCase("true")) {
			/* Failed parsing a sensible option specified by the user. */
			logger.error("GuestOSMXBeanTest usage:");
			logger.error(" -DRUNLOCAL=[false, true]  $GuestOSMXBeanTest");
			logger.error("Valid options are 'true' and 'false'.");
			logger.error("In absence of a specified option, test defaults to local.");
			Assert.fail("Invalid property");
		}

		/* We need the object name in any case - remote as well as local. */
		try {
			mxbeanName = new ObjectName("com.ibm.virtualization.management:type=GuestOS");
		} catch (MalformedObjectNameException e) {
			Assert.fail("MalformedObjectNameException!", e);
		}

		/* Perform the appropriate test as instructed through the command line. */
		if (false == localTest) {
			/* A remote test needs to be performed. Start the remote server if it is not running yet.
			 * Also, attach a watch dog to this, to bail out, in case it hangs.
			 */
			if (null == remoteServer) {
				startRemoteServer();
				watchdog = new ChildWatchdog(remoteServer, 30000);
			}

			/* Try connecting to the server; retry until limit reached. */
			while (retryCounter < 10 && !isConnected) {
				retryCounter++;
				try {
					urlForRemoteMachine = new JMXServiceURL("service:jmx:rmi:///jndi/rmi://localhost:9999/jmxrmi");

					/* Connect to remote host and create a proxy to be used to get an GuestOSMXBean
					 * instance.
					 */
					connector = JMXConnectorFactory.connect(urlForRemoteMachine, null);
					mbeanConnection = connector.getMBeanServerConnection();

					/* Obtain an MBean handle using the connection and the
					 *  object name for the class GuestOSMXBean.
					 */
					mxbeanProxy = JMX.newMXBeanProxy(mbeanConnection, mxbeanName, GuestOSMXBean.class);
					if (true != mbeanConnection.isRegistered(mxbeanName)) {
						Assert.fail("GuestOSMXBean is not registered. Cannot Proceed with the test. !!!Test Failed !!!");
					}

					/* If we reached here, it means we are connected (no connection failure exception thrown). */
					isConnected = true;

				} catch (MalformedURLException e) {
					Assert.fail("Please check the url supplied to JMXServiceURL!", e);
				} catch (IOException e) {
					/* Waiting 1000 ms before retrying to connect to remote server. */
					logger.warn("Failed connecting. Retry " + retryCounter + " after 1000 ms.");

					try {
						Thread.sleep(1000);
					} catch (InterruptedException ie) {
						logger.warn("Exception occurred while sleeping thread: " + ie.getMessage(), e);
					}

				} catch (Exception e){
					Assert.fail("Exception occurred in setting up remote test environment.");
				}
				if (!isConnected) {
					Assert.fail("Unable to connect to Remote Server. !!!Test Failed !!!");
				}
			}
		} else {
			/*
			 * A local test has been requested.
			 */
			try {
				mbeanServer = ManagementFactory.getPlatformMBeanServer();
				if(true != mbeanServer.isRegistered(mxbeanName)) {
					Assert.fail("GuestOSMXBean is not registered. Cannot Proceed with the test.");
				}
				mxbeanProxy =  JMX.newMXBeanProxy(mbeanServer, mxbeanName, GuestOSMXBean.class);
			} catch(Exception e){
				Assert.fail("Exception occurred in setting up local test environment." + e.getMessage(), e);
			}
		}

		try {
			MBeanServer mbs = ManagementFactory.getPlatformMBeanServer();
			HypervisorMXBean bean = ManagementFactory.getPlatformMXBean(mbs, HypervisorMXBean.class);
			HypervisorName = bean.getVendor();
			if(null == HypervisorName) {
				HypervisorName = "none";
			}
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured" + e.getMessage(), e);
		}

		try {
			error |= test_memoryInfo(mxbeanProxy);
			error |= test_processorInfo(mxbeanProxy);
		} finally {
			/* Do all clean up here. */
			if (false == localTest) {
				/* Close the connection and Stop the remote Server */
				try {
					connector.close();
					stopRemoteServer();
				} catch ( Exception e) {
					Assert.fail("Unexpected exception occured when close connector" + e.getMessage(), e);
				}

			}
		}

		if (false != error) {
			Assert.fail(" !!!Test Failed !!!");
		}
	}

	/**
	 * Function tests the MemoryUsage retrieval functionality for sanity conditions as also prints
	 * out some basic memory usage statistics retrieved using the MBean passed to it.
	 *
	 * @param mxbeanProxy The GuestOSMXBean instance that has already been initialized.
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */
	private static boolean test_memoryInfo(GuestOSMXBean mxbeanProxy)
	{
		GuestOSMemoryUsage memUsage = null;
		long memUsed = 0;
		long maxMemLimit = 0;
		long timeStamp = 0;
		logger.info(" Guest Memory Usage tests....");

		/* PowerKVM does not support any of the Guest Memory Usage counters; disable the
		 * validity checking part of the code on the platform, but enable as support for
		 * each of these is added.
		 */
		if (HypervisorName.contentEquals("PowerKVM")) {
			logger.warn("Guest memory counters not available on PowerKVM");
			return false; /* No error */
		}

		try {
			/*
			 * Try obtaining the memory usage statistics at this time (timestamp).
			 */
			memUsage = mxbeanProxy.retrieveMemoryUsage();
			memUsed = memUsage.getMemUsed();
			maxMemLimit = memUsage.getMaxMemLimit();
			timeStamp = memUsage.getTimestamp();

			if (-1 == maxMemLimit) {
				logger.warn("Maximum Memory that can be used by the Guest: <undefined for platform>");
			} else {
				logger.debug("Maximum Memory that can be used by the Guest: " + maxMemLimit + " bytes");
			}
			if (-1 == memUsed) {
				logger.warn("Current Used Memory by the Guest: <undefined for platform>");
			} else {
				logger.debug("Current Used Memory by the Guest: " + memUsed + " MB");
			}
			logger.debug("Sampled at timestamp: " + timeStamp + " microseconds");

			/*
			 * Validate the statistics obtained.
			 * - Memused cannot be <0.
			 * - Memused cannot be greater than maxMemlimit.
			 * - Timestamp cannot be 0.
			 */
			if (((memUsed > 0) && (maxMemLimit > 0)) && (memUsed > maxMemLimit)) {
				Assert.fail("Invalid memory usage statistics retrieved.");
			} else if ((memUsed != -1) && (memUsed < 0)){
				Assert.fail("Invalid Current Used Memory by the Guest reported, Memory Used cannot be less than 0!!");
			}

			if (timeStamp <= 0) {
				Assert.fail("Invalid timestamp received, timeStamp cannot be 0!!");
			}
		} catch(GuestOSInfoRetrievalException mu) {

			logger.warn("Exception occurred while retrieving Guest memory usage: " + mu.getMessage());

			if (mu.getMessage().contains(VMWARE_ERROR)) {
				/*
				 * Ignore the error for now until the hypervisor:NoGuestSDK tag
				 * is added
				 */
				logger.warn("Cannot Proceed with the test, check for the VMWare Guest SDK");
				return false;

			} else if (mu.getMessage().contains(LPARCFG_MEM_UNSUPPORTED_ERROR)) {
				/*
				 * Validate the error condition i.e if the lparcfg version is
				 * greater/equal to < 1.8 If version is < 1.8, ignore the error
				 * else fail the test
				 */
				File file = new File(LPARCFG_FILE);
				try {
					/*
					 * Use a Scanner to scan through the /proc/ppc64/lparcfg
					 */
					Scanner scanner = new Scanner(file);
					String str = "lparcfg ";
					while (scanner.hasNext("lparcfg")) {
						String line = scanner.nextLine();
						Float version = Float.valueOf(line.substring(str.length(), line.length()));
						logger.warn("Version of lparcfg: " + version);

						if (version < 1.8) {
							logger.warn("Cannot proceed with the test, the lparcfg version must be 1.8 or greater");
							return false;

						} else {
							Assert.fail("Invalid Guest Memory Usage statistics recieved!!");
						}
					}
					scanner.close();
				} catch (FileNotFoundException e) {
					Assert.fail("/proc/ppc64/lparcfg file not found.. Exiting.");
				}
			} else if (mu.getMessage().contains(NO_HYPERVISOR_ERROR)) {
				logger.warn("Not running on a Hypervisor, Guest Statistics cannot be retrieved!");
				return false;
			} else if (mu.getMessage().contains(NOT_SUPPORTED_ERROR)) {
				logger.warn("GuestOSMXBean not supported on this Hypervisor!");
				return false;
			} else if (mu.getMessage().contains(HYPFS_UPDATE_OPEN_FAILED_ERROR)) {
				logger.warn("HYPFS update failed!");
				return false;
			} else {
				/* Received a valid exception, test failed */
				Assert.fail("Received a valid exception, test failed");
			}
		} catch(Exception e) {
			Assert.fail("Unknown exception occurred:" + e.getMessage(), e);
		}
		return false; /* No error */
	}

	/**
	 * Function tests the ProcessorUsage retrieval functionality for sanity conditions as also prints
	 * out processor usage statistics retrieved using the MBean passed to it.
	 *
	 * @param mxbeanProxy The GuestOSMXBean instance that has already been initialized.
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */
	private static boolean test_processorInfo(GuestOSMXBean mxbeanProxy)
	{
		GuestOSProcessorUsage procUsage = null;
		float cpuEnt = 0;
		long timeStamp = 0;
		long hostSpeed = 0;
		long cpuTime = 0;
		logger.info(" Guest Processor Usage tests....");
		try {
			String osname = System.getProperty("os.name");
			procUsage = mxbeanProxy.retrieveProcessorUsage();

			cpuTime = procUsage.getCpuTime();
			timeStamp = procUsage.getTimestamp();
			hostSpeed = procUsage.getHostCpuClockSpeed();
			cpuEnt = procUsage.getCpuEntitlement();

			logger.debug("Guest Processor statistics received....");
			if (osname.equals("z/OS") == true) {
				logger.debug("The Host CPU Clock Speed	:" + hostSpeed + "MSU");
			} else {
				logger.debug("The Host CPU Clock Speed	:" + hostSpeed + "MHz");
			}

			/* CPU entitlement is expected to be -1 on PowerKVM hypervisor. */
			if ((-1 == cpuEnt) && (true == HypervisorName.contentEquals("PowerKVM"))) {
				logger.warn("The CPU Entitlement assigned for this Guest : <unsupported>");
			} else {
				logger.debug("The CPU Entitlement assigned for this Guest : " + cpuEnt);
			}
			logger.debug("The total CPU Time Used by this Guest		: " + cpuTime + " microseconds");
			logger.debug("Sampled at timeStamp						: " + timeStamp + " microseconds");

			/*
			 * Validate the statistics received
			 * - hostCpuClockSpeed of Guest OS is not 0
	 		 * - timeStamp is not 0
	 		 * - cpuTime is not 0
	 		 * - cpuEntitlement is not 0
			 */
			if (cpuTime > 0 && timeStamp > 0) {
				return false;
			} else {
				Assert.fail("Invalid Guest Processor statistics received");
			}
		} catch(GuestOSInfoRetrievalException pu) {
			logger.warn("Exception occurred retrieving processor usage: " + pu.getMessage());
			if (pu.getMessage().contains(VMWARE_ERROR)) {
				/*
				 * Ignore the error for now until the hypervisor:NoGuestSDK tag
				 * is added
				 */
				logger.warn("Cannot Proceed with the test, check for the VMWare Guest SDK");
				return false;
			} else if (pu.getMessage().contains(NO_HYPERVISOR_ERROR)) {
				logger.warn("Not running on a Hypervisor, Guest Statistics cannot be retrieved!");
				return false;

			} else if (pu.getMessage().contains(NOT_SUPPORTED_ERROR)) {
				logger.warn("GuestOSMXBean not supported on this Hypervisor!");
				return false;

			} else {
				/* Received a valid exception, test failed */
				Assert.fail("Received a valid exception, test failed" + pu.getMessage());
			}
		} catch(Exception e) {
			Assert.fail("Unknown exception occurred:" + e.getMessage(), e);
		}
		return false;
	}

	/**
	 * Internal function: Starts a remote server to connect to.
	 */
	private static void startRemoteServer()
	{
		logger.info("Starting Remote Server!");

		ArrayList<String> argBuffer = new ArrayList<String>();
		char fs = File.separatorChar;

		String javaExec = System.getProperty("java.home") + fs + "bin" + fs	+ "java";
		argBuffer.add(javaExec);
		argBuffer.add("-classpath");
		argBuffer.add(System.getProperty("java.class.path"));
		argBuffer.add(System.getProperty("remote.server.option"));
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
			Assert.fail("Failed to launch child process " + remoteServer, e);
		}

		/* Before proceeding, it would be nice to know that the server process can
		 * already accept connections. We attempt to better sync with the server
		 * process by blocking on its stdout and waiting for it to write something
		 * to it.
		 */
		BufferedReader in = new BufferedReader( new InputStreamReader(remoteServer.getInputStream()));
		String rSrvStdout;
		try {
			if ((rSrvStdout = in.readLine()) != null) {
				logger.info("Remote Server stdout: " + rSrvStdout);
			}
		} catch (IOException e) {
			Assert.fail("Exception occurred while waiting for server.", e);
		}

		try {
			Thread.sleep(10);
		} catch (InterruptedException e) {
			logger.error("InterruptedException occured",e);
		}
	}

	/**
	 * Internal function: Stops a remote server.
	 */
	private static void stopRemoteServer() {

		// Check all outputs from server
		BufferedReader in = new BufferedReader(new InputStreamReader(remoteServer.getInputStream()));
		String rSrvStdout;
		try {
			while ((rSrvStdout = in.readLine()) != null) {
				logger.info("Remote Server stdout: " + rSrvStdout);
			}
		} catch (IOException e) {
			logger.error("Exception occurred while waiting for server.", e);
		}

		watchdog.interrupt();
		remoteServer.destroy();
		try {
			Thread.sleep(10000);
		} catch (InterruptedException e) {
			logger.error("InterruptedException occured", e);
		}
	}
}
