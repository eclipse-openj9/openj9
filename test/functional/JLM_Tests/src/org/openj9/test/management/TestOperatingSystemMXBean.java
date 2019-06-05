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

package org.openj9.test.management;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import java.io.File;
import java.io.IOException;
import java.io.BufferedReader;
import java.io.InputStreamReader;
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

import java.lang.management.ManagementFactory;
import java.lang.management.OperatingSystemMXBean;

import org.openj9.test.management.util.*;
import com.ibm.lang.management.*;

/**
 * Class for testing the APIs that are provided as part of OperatingSystemMXBean
 * Interface
 *
 */
@Test(groups = { "level.extended" })
public class TestOperatingSystemMXBean {

	private static final Logger logger = Logger.getLogger(TestOperatingSystemMXBean.class);

	private static Process remoteServer;
	private static ChildWatchdog watchdog;
	private static final int MAX_WARMUP_ROUNDS = 3;
	private static final int SLEEPINTERVAL = 5;
	private static final int NTHREADS = 1;
	private static final int NITERATIONS = 10;

	/**
	 * Starting point for the test program.
	 * Invokes the tests for the new API's that are added for the OperatingSystemMXBean
	 *
	 * @param args
	 */
	@Test
	public void runTestOSMXBean() {


		boolean localTest = true;
		boolean isConnected = false;
		boolean error = false;

		ObjectName objName = null;
		JMXServiceURL urlForRemoteMachine = null;
		JMXConnector connector = null;
		MBeanServerConnection mbeanConnection = null;
		com.ibm.lang.management.OperatingSystemMXBean osmxbean = null;

		logger.info(" ----------------------------------------------------------");
		logger.info(" Starting the OperatingSystemMXBean usage API tests........");
		logger.info(" ----------------------------------------------------------");

		/* Check and set flag 'localTest'. If the user specified 'local' or nothing, select local testing,
		 * while under explicit specification of the option 'remote' do we platform remote testing.
		 */

		if (System.getProperty("RUNLOCAL").equalsIgnoreCase("false")) {
			localTest = false;
		} else if (!System.getProperty("RUNLOCAL").equalsIgnoreCase("true")) {

			/* Failed parsing a sensible option specified by the user. */
			logger.error("TestOperatingSystemMXBean usage: ");
			logger.error(" -DRUNLOCAL=[false, true]  $TestOperatingSystemMXBean");
			logger.error("Valid options are 'true' and 'false'.");
			logger.error("In absence of a specified option, test defaults to local.");
			Assert.fail("Invalid property");
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

			int retryCounter = 0;
			/* Try connecting to the server; retry until limit reached. */
			while (retryCounter < 10 && !isConnected) {
				retryCounter++;
				try {
					urlForRemoteMachine = new JMXServiceURL("service:jmx:rmi:///jndi/rmi://localhost:9999/jmxrmi");

					/* Connect to remote host and create a proxy to be used to get an OperatingSystemMBean
					 * instance.
					 */
					connector = JMXConnectorFactory.connect(urlForRemoteMachine, null);
					mbeanConnection = connector.getMBeanServerConnection();

					/* Obtain an mxbean object using the connection and the object name for the class
					 * OperatingSystemMXBean.
					 */
					objName = new ObjectName("java.lang:type=OperatingSystem");
					osmxbean = (com.ibm.lang.management.OperatingSystemMXBean)JMX.newMXBeanProxy(mbeanConnection,
							objName, com.ibm.lang.management.OperatingSystemMXBean.class);

					/* If we reached here, it means we are connected (no connection failure exception thrown). */
					isConnected = true;

				} catch (MalformedURLException e) {
					Assert.fail("Unexpected MalformedURLException occured, please check the url supplied to JMXServiceURL!", e);
				} catch (IOException e) {
					/* Waiting 1000 ms before retrying to connect to remote server. */
					logger.warn("Connection request failed: " + e.getMessage());
					logger.warn("Connection failed at: " + System.currentTimeMillis() + " ms.  Retry " + retryCounter + " after 1000 ms.");
					try {
						Thread.sleep(1000);
					} catch (InterruptedException ie) {
						logger.warn("Sleeping thread interrupted", ie);
					}
				} catch (Exception e) {
					Assert.fail("Unexpected exception occured", e);
				}
			}
			Assert.assertTrue(isConnected, "!!!Test(s) couldn't be started in remote mode: Connection setup failed !!!");

		} else {
			// A local test has been requested.
			osmxbean = (com.ibm.lang.management.OperatingSystemMXBean)ManagementFactory.getOperatingSystemMXBean();
		}

		/* Proceed with testing only if either (1) we are running local testing or (2) if we are
		 * running remote testing and have successfully set up a JMX connection.
		 */
		try {
			if ((localTest) || (!localTest && isConnected)) {
				logger.info("Tests starting at: " + System.currentTimeMillis() + " ms.");
				if (!isPlatformZOS()) {
					/* Call the API test methods */
					error |= test_PhysicalMemoryAPIs(osmxbean);
					error |= test_SwapSpaceSizeAPIs(osmxbean);
					error |= test_getProcessVirtualMemorySize(osmxbean);
					error |= test_memoryInfo(osmxbean);
					error |= test_processorInfo(osmxbean);
				}
				/* At this point, we have enabled support for only these API's on z/OS. */
				error |= test_getProcessCpuTimeByNS(osmxbean, localTest);
				error |= test_getProcessCpuLoad(osmxbean, localTest);
				error |= test_getHardwareModel(osmxbean);
				error |= test_isHardwareEmulated(osmxbean, localTest);
			}
		} finally {
			/* Do all clean up here. */
			if (!localTest) {
				/* Close the connection and Stop the remote Server */
				try {
					connector.close();
					stopRemoteServer();
				} catch (Exception e) {
					Assert.fail("Failed to stop remote server or close connetor" + e.getMessage(), e);
				}
			}
			logger.info("Tests finished at: " + System.currentTimeMillis() + " ms.");
		}
		Assert.assertFalse(error,"!!!Test(s) Failed !!!");
	}

	/**
	 * Test the getTotalSwapSpaceSize() & getFreeSwapSpaceSize() APIs of OperatingSystemMXBean
	 * @param osmxbean The OperatingSystemMXBean instance
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */
	private static boolean test_SwapSpaceSizeAPIs(com.ibm.lang.management.OperatingSystemMXBean osmxbean) {
		long totalSwap = 0;
		long freeSwap = 0;
		String osname = System.getProperty("os.name");

		totalSwap = osmxbean.getTotalSwapSpaceSize();
		freeSwap = osmxbean.getFreeSwapSpaceSize();

		logger.info("Testing getTotalSwapSpaceSize() & getFreeSwapSpaceSize() APIs");
		logger.debug("Total swap space configured: " + totalSwap + " bytes");
		logger.debug("of which available swap: " + freeSwap + " bytes");

		if ((-1 != totalSwap) && (-1 != freeSwap)) {
			Assert.assertFalse(freeSwap > totalSwap,
					"Invalid Swap Memory Info retrieved , Free Swap size cannot be greater than total Swap Size. getTotalSwapSpaceSize() & getFreeSwapSpaceSize() API's failed ");

		} else {
			if ((-1 == totalSwap) && (osname.equalsIgnoreCase("z/OS") == false)) {
				/*
				 * An error has occurred since getTotalSwapSpaceSize() has returned -1.
				 * -1 can also mean it is not supported, but we exclude these tests on
				 * non-supported platforms(i.e System-Z).
				 */
				Assert.fail("Error: getTotalSwapSpaceSize() API returned -1, test failed");
			}
			if ((-1 == freeSwap) && (osname.equalsIgnoreCase("z/OS") == false)) {
				/*
				 * An error has occurred since getFreeSwapSpaceSize() has returned -1.
				 * -1 can also mean it is not supported, but we exclude these tests on
				 * non-supported platforms(i.e System-Z).
				 */
				Assert.fail("Error: getFreeSwapSpaceSize() API returned -1, test failed");
			}
		}
		return false; /* No error */
	}

	/**
	 * Test the getTotalPhysicalMemory() & getFreePhysicalMemorySize() APIs of OperatingSystemMXBean
	 * @param osmxbean The OperatingSystemMXBean instance
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */
	private static boolean test_PhysicalMemoryAPIs(com.ibm.lang.management.OperatingSystemMXBean osmxbean) {

		long totalMemory = osmxbean.getTotalPhysicalMemory();
		long freeMemory = osmxbean.getFreePhysicalMemorySize();
		String osname = System.getProperty("os.name");

		logger.info("Testing getTotalPhysicalMemory() & getFreePhysicalMemorySize() APIs");
		logger.debug("Total Physical Memory available: " + totalMemory + " bytes");
		logger.debug("of which Free Physical Memory: " + freeMemory + " bytes");

		if ((-1 != totalMemory) && (-1 != freeMemory)) {
			/*
			 * Check for the validity of the values from the API
			 */
			Assert.assertTrue(totalMemory > 0,
					"Invalid Physical Memory Info retrieved , Total Physical Memory cannot be 0 bytes.");
			Assert.assertFalse(freeMemory > totalMemory,
					"Free Physical Memory size cannot be greater than total Physical Memory Size.");
		} else {
			if ((-1 == totalMemory) && (false == osname.equalsIgnoreCase("z/OS"))) {
				/*
				 * An error has occurred since getTotalPhysicalMemory() has returned -1.
				 * -1 can also mean it is not supported, but we exclude these tests on
				 * non-supported platforms(i.e System-Z).
				 */
				Assert.fail("getTotalPhysicalMemory() API returned -1, test failed");
			}
			if ((-1 == freeMemory) && (false == osname.equalsIgnoreCase("z/OS"))) {
				/*
				 * An error has occurred since getFreePhysicalMemorySize() has returned -1.
				 * -1 can also mean it is not supported, but we exclude these tests on
				 * non-supported platforms(i.e System-Z).
				 */
				Assert.fail("getFreePhysicalMemorySize() API returned -1, test failed");
				return true;
			}
		}
		return false;
	}

	/**
	 * Test the getProcessVirtualMemorySize() APIs OperatingSystemMXBean
	 * @param osmxbean The OperatingSystemMXBean instance
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */
	private static boolean test_getProcessVirtualMemorySize(com.ibm.lang.management.OperatingSystemMXBean osmxbean) {
		long processVirtualMem = osmxbean.getProcessVirtualMemorySize();
		String osname = osmxbean.getName();
		logger.info("Testing getProcessVirtualMemorySize() API");
		logger.debug("Amount of Virtual Memory used by the process: " + processVirtualMem + " bytes");

		if (-1 != processVirtualMem) {
			Assert.assertTrue(processVirtualMem > 0,
					"Invalid Process Virtual Memory Size retrieved, ProcessVirtualMemory cannot be less than 0");
		} else if ((-1 == processVirtualMem) && (osname.equalsIgnoreCase("AIX") || osname.equalsIgnoreCase("z/OS"))) {
			/* API not supported on AIX for now, so we ignore the -1 */
			logger.warn(
					"getProcessVirtualMemorySize() not supported on AIX and z/OS, Process Virtual Memory Size: <undefined for platform>");
		} else {
			/*
			 * An error has occurred since getProcessVirtualMemorySize() has returned -1.
			 * -1 can also mean it is not supported, but we exclude these tests on
			 * non-supported platforms(i.e System-Z).
			 */
			Assert.fail("Error: getProcessVirtualMemorySize() returned -1, API failed!!");
		}
		return false; /* No error */
	}

	/**
	 * Test the getProcessCpuTimeByNS() API of OperatingSystemMXBean
	 * @param osmxbean The OperatingSystemMXBean instance
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */
	private static boolean test_getProcessCpuTimeByNS(com.ibm.lang.management.OperatingSystemMXBean osmxbean, boolean local) {

		int i = 0;
		int warmupCntr = MAX_WARMUP_ROUNDS;
		long processCpuTime_old = 0;
		long processCpuTime_new = 0;

		logger.info("Testing getProcessCpuTimeByNS() API");
		processCpuTime_old = osmxbean.getProcessCpuTimeByNS();

		if (-1 != processCpuTime_old) {
			logger.debug("First call to getProcessCpuTimeByNS, Process CPU Time is: " + processCpuTime_old);

			try {
				if (true == local) {
					/* For local testing, we still busy loop to generate some load before
					 * reading up Process CPU Time yet again.
					 */
					Thread[] busyObj = new Thread[NTHREADS];
					long counter = 0;

					logger.debug("Generating busy load ... ");
					for (; counter < NITERATIONS; counter++) {
						for (i = 0; i < NTHREADS; i++) {
							busyObj[i] = new Thread(new BusyThread(SLEEPINTERVAL * 500));
							busyObj[i].start();
						}
						Thread.sleep(1000);
						for (i = 0; i < NTHREADS; i++) {
							busyObj[i].join();
						}
					}
					processCpuTime_new = osmxbean.getProcessCpuTimeByNS();
					if (-1 == processCpuTime_new) {
						Assert.fail("Error: getProcessCpuTimeByNS() returned -1, API failed!!");
					}
				} else {
					logger.debug("Warm up cycles ... " + warmupCntr);
					for (; warmupCntr > 0; warmupCntr--) {
						/* Sleep for 2s */
						Thread.sleep(2000);
						processCpuTime_new = osmxbean.getProcessCpuTimeByNS();
						if (-1 == processCpuTime_new) {
							Assert.fail("Error: getProcessCpuTimeByNS() returned -1, API failed!!");
						}
					}
				}
			} catch (InterruptedException e) {
				Assert.fail("Sleep Interrupted, unexpected InterruptedException occured", e);
			}

			logger.debug("Second call to getProcessCpuTimeByNS(), Process CPU Time is: " + processCpuTime_new);
			logger.debug("Delta in Process CPU Time is: " + (processCpuTime_new - processCpuTime_old));
			Assert.assertFalse(processCpuTime_new == processCpuTime_old, "Processor load did not increase. Test failed");

		} else {
			/*
			 * An error has occurred since getProcessCpuTimeByNS() has returned -1.
			 * -1 can also mean it is not supported, but we exclude these tests on
			 * non-supported platforms(i.e System-Z).
			 */
			Assert.fail("Error: getProcessCpuTimeByNS() returned -1, API failed!!");
		}
		return false; /* No error */
	}

	/**
	 * Test the getProcessCpuLoad() API of OperatingSystemMXBean
	 * @param osmxbean The OperatingSystemMXBean instance
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */
	private static boolean test_getProcessCpuLoad(com.ibm.lang.management.OperatingSystemMXBean osmxbean, boolean local) {

		int i = 0;
		int warmupCntr = MAX_WARMUP_ROUNDS;
		double processLoad = 0;
		processLoad = osmxbean.getProcessCpuLoad();

		logger.info("Testing getProcessCpuLoad() API");
		logger.debug("First call to getProcessCpuLoad() returned: " + processLoad);

		Assert.assertTrue(-1 == processLoad, "Error: First call to getProcessCpuLoad() should return -1!!");

		try {
			if (true == local) {
				/* For local testing, we still busy loop to generate some load before
				 * reading up Process CPU Time yet again.
				 */
				Thread[] busyObj = new Thread[NTHREADS];
				long counter = 0;

				logger.debug("Generating busy load ... ");
				for (; counter < NITERATIONS; counter++) {
					for (i = 0; i < NTHREADS; i++) {
						busyObj[i] = new Thread(new BusyThread(SLEEPINTERVAL * 500));
						busyObj[i].start();
					}
					Thread.sleep(1000);
					for (i = 0; i < NTHREADS; i++) {
						busyObj[i].join();
					}
				}
				processLoad = osmxbean.getProcessCpuLoad();
			} else {
				logger.debug("Warm up cycles ... " + warmupCntr);
				for (; warmupCntr > 0; warmupCntr--) {
					/* Sleep for 2s */
					Thread.sleep(2000);
					processLoad = osmxbean.getProcessCpuLoad();
				}
			}
		} catch (InterruptedException e) {
			Assert.fail("Sleep Interrupted..Exiting the test", e);
		}
		logger.debug("Second call to getProcessCpuLoad(), ProcessCpuLoad is: " + processLoad);

		/* Process CPU Load should have increased by now */
		Assert.assertTrue(processLoad > 0, "Process Cpu Load did not increase. Error: getProcessCpuLoad() API failed!!");
		return false; /* No error */
	}

	/**
	 * Function tests the MemoryUsage retrieval functionality for sanity conditions as also prints
	 * out some basic memory usage statistics retrieved using the MXBean instance passed to it.
	 *
	 * @param osmxbean The OperatingSystemMXBean instance
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */
	private static boolean test_memoryInfo(com.ibm.lang.management.OperatingSystemMXBean osmxbean) {

		long total;
		long free;
		long totalSwap;
		long freeSwap;
		long cached;
		long buffered;
		long timestamp;
		MemoryUsage memUsage = null;

		logger.info("Testing retrieveMemoryUsage(), MemoryUsage.getTotal() and MemoryUsage.getFree()");

		try {
			/*
			 * Try obtaining the memory usage statistics at this time (timestamp).
			 */
			memUsage = osmxbean.retrieveMemoryUsage();
			total = memUsage.getTotal();
			free = memUsage.getFree();

			if (-1 == total) {
				logger.debug("Total physical memory: <undefined for platform>");
			} else {
				logger.debug("Total physical memory: " + total + " bytes");
			}
			if (-1 == free) {
				logger.debug("Available physical: <undefined for platform>");
			} else {
				logger.debug("Available physical: " + free + " bytes");
			}

			if ((-1 != total) && (-1 != free)) {
				if ((total < 0) || (free > total)) {
					Assert.fail("Invalid memory usage statistics retrieved.");
				}
			}

			totalSwap = memUsage.getSwapTotal();
			freeSwap = memUsage.getSwapFree();
			if (-1 == totalSwap) {
				logger.debug("Total swap space configured: <undefined for platform>");
			} else {
				logger.debug("Total swap space configured: " + totalSwap + " bytes");
			}
			if (-1 == freeSwap) {
				logger.debug("Available swap: <undefined for platform>");
			} else {
				logger.debug("Available swap: " + freeSwap + " bytes");
			}
			if ((-1 != totalSwap) && (-1 != freeSwap)) {
				if (freeSwap > totalSwap) {
					Assert.fail("Invalid memory usage statistics retrieved.");
				}
			}

			cached = memUsage.getCached();
			if (-1 == cached) {
				logger.debug("Size of cached memory: <undefined for platform>");
			} else {
				logger.debug("Size of cached memory: " + cached + " bytes");
			}

			buffered = memUsage.getBuffered();
			if (-1 == buffered) {
				logger.debug("Size of buffered memory: <undefined for platform>");
			} else {
				logger.debug("Size of buffered memory: " + buffered + " bytes");
			}

			timestamp = memUsage.getTimestamp();
			logger.debug("Sampled at (timestamp): " + timestamp);
			if (timestamp <= 0) {
				Assert.fail("Invalid timestamp received!");
			}

		} catch (MemoryUsageRetrievalException mu) {
			Assert.fail("Exception occurred while retrieving memory usage:" + mu.getMessage(), mu);
		} catch (Exception e) {
			Assert.fail("Unknown exception occurred:" + e.getMessage(), e);
		}
		return false;
	}

	/**
	 * Function tests the ProcessorUsage retrieval functionality for sanity conditions as also prints
	 * out processor usage statistics retrieved using the MBean passed to it.
	 *
	 * @param osmxbean The OperatingSystemMXBean instance
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */
	private static boolean test_processorInfo(com.ibm.lang.management.OperatingSystemMXBean osmxbean) {
		ProcessorUsage procUsage = null;
		ProcessorUsage[] procUsageArr = null;

		int MaxIterations = 5;
		long deltaTotalIdleTime = 0;
		long deltaTotalBusyTime = 0;
		long busy = 0;
		long user = 0;
		long system = 0;
		long idle = 0;
		long wait = 0;
		long busyTotal[] = null;
		long userTotal[] = null;
		long systemTotal[] = null;
		long idleTotal[] = null;
		long waitTotal[] = null;
		long timestamp[] = null;

		logger.info("Testing retrieveTotalProcessorUsage()");

		try {
			busyTotal = new long[MaxIterations];
			userTotal = new long[MaxIterations];
			systemTotal = new long[MaxIterations];
			idleTotal = new long[MaxIterations];
			waitTotal = new long[MaxIterations];
			timestamp = new long[MaxIterations];

			for (int iter = 0; iter < MaxIterations; iter++) {
				procUsage = osmxbean.retrieveTotalProcessorUsage();

				busyTotal[iter] = procUsage.getBusy();
				userTotal[iter] = procUsage.getUser();
				systemTotal[iter] = procUsage.getSystem();
				idleTotal[iter] = procUsage.getIdle();
				waitTotal[iter] = procUsage.getWait();
				timestamp[iter] = procUsage.getTimestamp();

				/* Check for the number of records in the array; this is equal to the number of CPUs
				 * configured on the machine. Iterate through the records printing the processor usage
				 * data and dumping the same.
				 */
				procUsageArr = osmxbean.retrieveProcessorUsage();
				int nrecs = procUsageArr.length;
				int n_onln = onlineProcessorCount(procUsageArr);

				logger.debug("Available processors: " + procUsageArr.length);
				logger.debug("Online processors: " + n_onln);
				logger.debug("Timestamp: " + timestamp[iter]);

				if ((nrecs > 0) && (n_onln > 0) && (nrecs >= n_onln)) {
					/* We can deal with deltas only the second iteration onwards. */
					if (iter > 0) {
						deltaTotalIdleTime = idleTotal[iter] - idleTotal[iter - 1];
						deltaTotalBusyTime = busyTotal[iter] - busyTotal[iter - 1];

						if (0 < (deltaTotalBusyTime + deltaTotalIdleTime)) {
							logger.debug("Processor times in monotonically increasing order.");
						} else {
							logger.error("Unexpected change in processor time deltas!");
						}
					}
				} else {
					Assert.fail("Invalid processor usage statistics retrieved.");
				}

				logger.debug("CPUID : Online : User : System : Wait : Busy : Idle : Timestamp "
						+ "----------------------------------------");
				logger.debug("----------------------------------------------------------------");
				logger.debug("ALL : - : " + user + " : " + system + " : " + wait + " : " + busy + " : " + idle + " : "
						+ timestamp[iter]);

				for (int i = 0; i < nrecs; i++) {
					int online = procUsageArr[i].getOnline();
					busy = procUsageArr[i].getBusy();
					user = procUsageArr[i].getUser();
					system = procUsageArr[i].getSystem();
					idle = procUsageArr[i].getIdle();
					wait = procUsageArr[i].getWait();
					int id = procUsageArr[i].getId();

					logger.debug(id + " : " + online + " : " + user + " : " + system + " : " + busy + " : " + wait
							+ " : " + idle + " : " + timestamp[iter]);
				}
				Thread.sleep(3000);
			} /* end for(;;) */
		} catch (ProcessorUsageRetrievalException pu) {
			Assert.fail("Exception occurred retrieving processor usage: " + pu.getMessage(), pu);
		} catch (java.lang.InterruptedException ie) {
			Assert.fail("Exception occurred while sleeping thread: " + ie.getMessage(), ie);
		} catch (Exception e) {
			Assert.fail("Unknown exception occurred: " + e.getMessage(), e);
		}
		return false;
	}

	/**
	 * Internal function: Computes and returns the number of processors currently online.
	 *
	 * @param[in] procUsageArr An array of processor usage objects.
	 *
	 * @return Online processor count.
	 */
	private static int onlineProcessorCount(ProcessorUsage[] procUsageArr) {
		int n_onln = 0;

		for (int cntr = 0; cntr < procUsageArr.length; cntr++) {
			if (1 == procUsageArr[cntr].getOnline()) {
				n_onln++;
			}
		}
		return n_onln;
	}

	/**
	 * Internal function: Starts a remote server to connect to.
	 */
	private static void startRemoteServer() {
		logger.info("Starting Remote Server!");

		ArrayList<String> argBuffer = new ArrayList<String>();
		char fs = File.separatorChar;

		String javaExec = System.getProperty("java.home") + fs + "bin" + fs + "java";
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
		BufferedReader in = new BufferedReader(new InputStreamReader(remoteServer.getInputStream()));
		String rSrvStdout;
		try {
			if ((rSrvStdout = in.readLine()) != null) {
				logger.info("Remote Server stdout: " + rSrvStdout);
			}
		} catch (IOException e) {
			Assert.fail("Exception occurred while waiting for server.", e);
		}

		logger.debug("Server started at: " + System.currentTimeMillis() + " ms.");
		try {
			Thread.sleep(1000);
		} catch (InterruptedException e) {
			logger.error("Unexoected exception: InterruptedException", e);
		}
	}

	/**
	 * Test getHardwareModel()
	 * @param osmxbean The OperatingSystemMXBean instance
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */
	private static boolean test_getHardwareModel(com.ibm.lang.management.OperatingSystemMXBean osmxbean) {
		String hwModel = "";
		boolean wasThrown = false;

		logger.info("Testing getHardwareModel() API");

		try {
			hwModel = osmxbean.getHardwareModel();
		} catch (UnsupportedOperationException e) {
			logger.warn("UnsupportedOperationException was thrown");
			wasThrown = true;
		}

		/* For the time being, it is only implemented on z/OS. It should throw
		         * on every other platform
		         */
		if (!isPlatformZOS()) {
			if (!wasThrown) {
				Assert.fail("UnsupportedOperationException was not thrown on non-z/OS, test failed");
			} else {
				return false; /* No error */
			}
		} else {
			if (wasThrown) {
				Assert.fail("UnsupportedOperationException was thrown on z/OS, test failed");
			} else {
				logger.debug("HW Model is: " + hwModel);

				try {
					/* Get hw model from cmd line in zos */
					Process proc = Runtime.getRuntime().exec("uname -m");

					BufferedReader sout = new BufferedReader(new InputStreamReader(proc.getInputStream()));

					String line = sout.readLine();
					logger.debug("'uname -m' returned: " + line);

					if (line.equalsIgnoreCase(hwModel)) {
						logger.debug("Success: " + line + " == " + hwModel);
						return false; /* No error */
					} else {
						Assert.fail("Failure: " + line + " != " + hwModel);
					}
				} catch (Exception e) {
					logger.error("Unexpected exception occured" + e.getMessage(), e);
				}

				return true; /* If we got here, there was an error */
			}
		}
		return false;
	}

	/**
	 * Test isHardwareEmulated()
	 * @param osmxbean The OperatingSystemMXBean instance
	 * @param local Switch between test using default system properties and test changing them
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */
	private static boolean test_isHardwareEmulated(com.ibm.lang.management.OperatingSystemMXBean osmxbean, boolean local) {
		boolean testFailed = true;

		logger.info("Testing isHardwareEmulated() API");

		/* Currently, this is only implemented on z/OS. We expect an exception on other
		 * platforms
		 */
		if (!isPlatformZOS()) {
			try {
				osmxbean.isHardwareEmulated();
			} catch (UnsupportedOperationException e) {
				logger.warn("UnsupportedOperationException was thrown");
				testFailed = false;
			}
		} else {
			/* We want to run this test with and w/o modifying the system property but isHardwareEmulated() caches
			 * the result. Since OperatingSystemMXBean is a singleton, once the result of isHardwareEmulated() is
			 * cached, we won't get a different result. As a work-around, I use the local var to switch between the
			 * types of test I want to run
			 */
			if (!local) {
				try {
					String hwModel = osmxbean.getHardwareModel();
					boolean isEmulated = osmxbean.isHardwareEmulated();

					logger.debug("HW Model is: " + hwModel);
					logger.debug("Is Hardware Emulated? " + (isEmulated ? 'Y' : 'N'));

					/* We don't expect tests to be running on zPDT so ... */
					testFailed = isEmulated;
				} catch (UnsupportedOperationException e) {
					logger.error("UnsupportedOperationException was thrown in remote test");
				}
			} else {
				try {
					boolean isEmulated = false;
					String hwModel = osmxbean.getHardwareModel();

					logger.debug("Adding " + hwModel + " to the list of emulated hw models...");

					System.setProperty("com.ibm.lang.management.OperatingSystemMXBean.zos.emulatedHardwareModels",
							"PHONYMODEL1," + hwModel + ";PHONYMODEL2");

					logger.debug("com.ibm.lang.management.OperatingSystemMXBean.zos.emulatedHardwareModels=" + System
							.getProperty("com.ibm.lang.management.OperatingSystemMXBean.zos.emulatedHardwareModels"));

					isEmulated = osmxbean.isHardwareEmulated();
					testFailed = (!isEmulated);

					logger.debug("Is Hardware Emulated? " + (isEmulated ? 'Y' : 'N'));
				} catch (UnsupportedOperationException e) {
					logger.debug("UnsupportedOperationException was thrown in local test");
				}
			}
		}

		logger.debug("Success? " + (testFailed ? 'N' : 'Y'));
		return testFailed;
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
				logger.debug("Remote Server stdout: " + rSrvStdout);
			}
		} catch (IOException e) {
			logger.error("Exception occurred while waiting for server.", e);
		}

		watchdog.interrupt();
		remoteServer.destroy();
		logger.info("Server stopped at: " + System.currentTimeMillis() + " ms.");
		try {
			Thread.sleep(10000);
		} catch (InterruptedException e) {
			logger.warn("Unexpected InterruptedException occured", e);
		}
	}

	private static boolean isPlatformZOS() {
		String osName = System.getProperty("os.name");
		return osName.equalsIgnoreCase("z/OS");
	}
}
