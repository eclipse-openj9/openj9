/*******************************************************************************
 * Copyright (c) 2014, 2018 IBM Corp. and others
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

import org.openj9.test.management.util.ChildWatchdog;
import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
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
import java.lang.management.ThreadMXBean;
import java.lang.management.ThreadInfo;

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.CountDownLatch;

import org.openj9.test.management.util.*;
import com.ibm.lang.management.*;

/**
 * Class for testing the API that are provided as part of the JvmCpuMonitorMXBeanTest
 *
 */
@Test(groups = { "level.extended" })
public class JvmCpuMonitorMXBeanTest {

	public static final Logger logger = Logger.getLogger(JvmCpuMonitorMXBeanTest.class);

	private static Process remoteMonitor;
	private static ChildWatchdog watchdog;
	private static ProcessLocking lock;

	/*
	 * Strings that are looked for in exception messages to check if they are
	 * actually a valid error or not
	 */
	private static final int NTHREADS = 10;
	private static final int SLEEPINTERVAL = 5;

	/**
	 * Starting point for the test program. Invokes the memory and processor test routines and
	 * indicates success or failure accordingly.
	 *
	 * @param args
	 */
	@Test
	public void runJvmCpuMonitorMXBeanTest() throws IOException {
		int retryCounter = 0;
		boolean localTest = true;
		boolean isConnected = false;
		boolean error = false;

		JMXServiceURL urlForRemoteMachine = null;
		JMXConnector connector = null;
		MBeanServerConnection mbeanConnection = null;
		MBeanServer mbeanServer = null;
		ObjectName jcmObjN = null;
		ObjectName thdObjN = null;
		ThreadMXBean thdmxbean = null;
		JvmCpuMonitorMXBean jcmmxbean = null;

		logger.info(" ------------------------------------------");
		logger.info(" Starting JvmCpuMonitorMXBean API tests....");
		logger.info(" ------------------------------------------");

		/* Check and set flag 'localTest'. If the user specified 'local' or nothing, select local testing,
		 * while under explicit specification of the option 'remote' do we platform remote testing.
		 */
		if (System.getProperty("RUNLOCAL").equalsIgnoreCase("false")) {
			localTest = false;
		} else if (!System.getProperty("RUNLOCAL").equalsIgnoreCase("true")) {

			/* Failed parsing a sensible option specified by the user. */
			logger.error("JvmCpuMonitorMXBeanTest usage:");
			logger.error(" -DRUNLOCAL=[false, true]  $JvmCpuMonitorMXBeanTest");
			logger.error("Valid options are 'true' and 'false'.");
			logger.error("In absence of a specified option, test defaults to local.");
			Assert.fail("Invalid property");
		}

		/* We need the object name in any case - remote as well as local. */
		try {
			thdObjN = new ObjectName("java.lang:type=Threading");
			jcmObjN = new ObjectName("com.ibm.lang.management:type=JvmCpuMonitor");
		} catch (MalformedObjectNameException e) {
			Assert.fail("MalformedObjectNameException!", e);
		}

		/* Perform the appropriate test as instructed through the command line. */
		if (!localTest) {
			/* A remote test needs to be performed. Start the remote server if it is not running yet.
			 * Also, attach a watch dog to this, to bail out, in case it hangs.
			 */
			if (null == remoteMonitor) {
				startRemoteMonitor();
				watchdog = new ChildWatchdog(remoteMonitor, 30000);
			}

			/* Try connecting to the server; retry until limit reached. */
			while (retryCounter < 10 && !isConnected) {
				retryCounter++;
				try {
					urlForRemoteMachine = new JMXServiceURL("service:jmx:rmi:///jndi/rmi://localhost:9999/jmxrmi");

					/* Connect to remote host and create a proxy to be used to get an JvmCpuMonitorMXBean
					 * instance.
					 */
					connector = JMXConnectorFactory.connect(urlForRemoteMachine, null);
					mbeanConnection = connector.getMBeanServerConnection();

					thdmxbean = JMX.newMXBeanProxy(mbeanConnection, thdObjN, ThreadMXBean.class);
					if (true != mbeanConnection.isRegistered(jcmObjN)) {
						Assert.fail("ThreadMXBean is not registered. Cannot Proceed with the test. !!!Test Failed !!!");
					}

					/* Obtain an MBean handle using the connection and the
					 *  object name for the class JvmCpuMonitorMXBean.
					 */
					jcmmxbean = JMX.newMXBeanProxy(mbeanConnection, jcmObjN, JvmCpuMonitorMXBean.class);
					if (true != mbeanConnection.isRegistered(jcmObjN)) {
						Assert.fail("JvmCpuMonitorMXBean is not registered. Cannot Proceed with the test. !!!Test Failed !!!");
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
						logger.warn("InterruptedException occurred while sleeping thread: " + ie.getMessage(), ie);
					}
				} catch (Exception e) {
					Assert.fail("Exception occurred in setting up remote test environment.");
				}
			}
			if (!isConnected) {
				Assert.fail("Unable to connect to Remote Server. !!!Test Failed !!!");
			}
		} else {
			/*
			 * A local test has been requested.
			 */
			try {
				thdmxbean = ManagementFactory.getThreadMXBean();

				mbeanServer = ManagementFactory.getPlatformMBeanServer();
				if (true != mbeanServer.isRegistered(jcmObjN)) {
					Assert.fail("JvmCpuMonitorMXBean is not registered. Cannot Proceed with the test. !!!Test Failed !!!");
				}
				jcmmxbean = JMX.newMXBeanProxy(mbeanServer, jcmObjN, JvmCpuMonitorMXBean.class);
			} catch (Exception e) {
				Assert.fail("Exception occurred in setting up local test environment.", e);
			}
		}
		try {
			logger.info("Testing setCategory...");
			error |= test_setThreadCategory(thdmxbean, jcmmxbean, localTest);
			logger.info("Testing getThreadsCpuUsage...");
			error |= test_getThreadsCpuUsage(thdmxbean, jcmmxbean, localTest);
		} finally {
			/* Do all clean up here. */
			if (!localTest) {
				/* Close the connection and Stop the remote Server */
				try {
					connector.close();
					lock.notifyEvent("closed JMX connection");
				} catch (Exception e) {
					logger.error("Exception occurred while closing remote connection.", e);
				}
				/* Destroy the remote monitor process, if this hasn't exited already. */
				stopRemoteMonitor();
			}
		}

		if (error) {
			Assert.fail(" !!!Test Failed !!!");
		}
	}

	/**
	 *
	 * @param jcmmxbean The JvmCpuMonitorMXBean instance that has already been initialized.
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */

	private static boolean test_getThreadsCpuUsage(ThreadMXBean thdmxbean, JvmCpuMonitorMXBean jcmmxbean, boolean local) {

		JvmCpuMonitorInfo jcmInfoBefore = new JvmCpuMonitorInfo();
		JvmCpuMonitorInfo jcmInfoAfter = new JvmCpuMonitorInfo();
		long threadId = Thread.currentThread().getId();
		int appUserFail = 0;

		try {
			String thdCat = null;
			Thread[] busyObj = new Thread[NTHREADS];
			String baseCat = "Application-User";
			if (local) {
				jcmInfoBefore = jcmmxbean.getThreadsCpuUsage(jcmInfoBefore);
				CountDownLatch joinLatch = new CountDownLatch(NTHREADS);
				CountDownLatch startLatch = new CountDownLatch(1);
				for (int i = 0; i < NTHREADS; i++) {
					WorkerThread worker = new WorkerThread(startLatch, joinLatch, SLEEPINTERVAL * 500);
					busyObj[i] = new Thread(worker);
					busyObj[i].start();
					try {
						int s = i % 6;
						if (s > 0) {
							thdCat = baseCat.concat(Integer.toString(s));
							jcmmxbean.setThreadCategory(busyObj[i].getId(), thdCat);
						} else {
							jcmmxbean.setThreadCategory(busyObj[i].getId(), "Resource-Monitor");
						}
					} catch (IllegalArgumentException e) {
						Assert.fail("FAIL: Got exception changing the thread category", e);
					}
				}
				startLatch.countDown();
				/* Do not exit the thread just yet; wait until the thread's have done some measurable amount of
				 * work that the main thread can query about.
				 */
				for (int i = 0; i < NTHREADS; i++) {
					busyObj[i].join();
				}
				jcmInfoAfter = jcmmxbean.getThreadsCpuUsage(jcmInfoAfter);
			} else {
				jcmInfoBefore = jcmmxbean.getThreadsCpuUsage(jcmInfoBefore);
				/* Remote testing requested */
				lock.waitForEvent("all threads started");
				/* Wait until the child process signals that thread names have been set and the threads have
				 * executed some CPU intensive work by writing a "known" string to the output stream.
				 */
				int k = 0;
				long[] ids = thdmxbean.getAllThreadIds();
				for (ThreadInfo thdIter : thdmxbean.getThreadInfo(ids)) {
					if (thdIter != null) {
						String thdName = thdIter.getThreadName();
						/* Change/Set thread category for only those threads named "Busy-Thread". */
						if (thdName.startsWith("Busy-Thread", 0)) {
							int s = k % 6;
							if (s > 0) {
								thdCat = baseCat.concat(Integer.toString(s));
								jcmmxbean.setThreadCategory(thdIter.getThreadId(), thdCat);
							} else {
								jcmmxbean.setThreadCategory(thdIter.getThreadId(), "Resource-Monitor");
							}
							++k;
						}
					}
				}
				lock.notifyEvent("set thread categories");
				lock.waitForEvent("CPU usages available");
				jcmInfoAfter = jcmmxbean.getThreadsCpuUsage(jcmInfoAfter);
				lock.notifyEvent("done collecting threadinfo");
			}
		} catch (Exception e) {
			Assert.fail("FAIL: Call to getThreadsCpuUsage failed", e);
		}

		if ((jcmInfoBefore == null) || (jcmInfoAfter == null)) {
			Assert.fail("FAIL: Call to getThreadsCpuUsage returned null");
		}

		long appUserInfoBefore[] = jcmInfoBefore.getApplicationUserCpuTime();
		long appUserInfoAfter[] = jcmInfoAfter.getApplicationUserCpuTime();

		logger.debug("	      Parameter                   Before              After");
		logger.debug("	Timestamp             :    " + jcmInfoBefore.getTimestamp() + " : " + jcmInfoAfter.getTimestamp());
		logger.debug("	System-JVM            :    " + jcmInfoBefore.getSystemJvmCpuTime() + " : " + jcmInfoAfter.getSystemJvmCpuTime());
		logger.debug("	        GC            :    " + jcmInfoBefore.getGcCpuTime() + " : " + jcmInfoAfter.getGcCpuTime());
		logger.debug("	       JIT            :    " + jcmInfoBefore.getJitCpuTime() + " : " + jcmInfoAfter.getJitCpuTime());
		logger.debug("	Resource-Monitor      :    " + jcmInfoBefore.getResourceMonitorCpuTime() + " : " + jcmInfoAfter.getResourceMonitorCpuTime());
		logger.debug("	Application           :    " + jcmInfoBefore.getApplicationCpuTime() + " : " + jcmInfoAfter.getApplicationCpuTime());
		for (int i = 0; i < appUserInfoAfter.length; i++) {
			logger.debug("	    Application-User :    " +  (i + 1) + " : " + appUserInfoBefore[i] + " : " + appUserInfoAfter[i]);
			if (appUserInfoAfter[i] <= appUserInfoBefore[i]) {
				appUserFail = 1;
			}
		}
		if ((jcmInfoAfter.getTimestamp() <= jcmInfoBefore.getTimestamp())
				|| (jcmInfoAfter.getApplicationCpuTime() <= jcmInfoBefore.getApplicationCpuTime())
				|| (jcmInfoAfter.getResourceMonitorCpuTime() <= jcmInfoBefore.getResourceMonitorCpuTime())
				|| (jcmInfoAfter.getSystemJvmCpuTime() < jcmInfoBefore.getSystemJvmCpuTime())
				|| (jcmInfoAfter.getGcCpuTime() < jcmInfoBefore.getGcCpuTime())
				|| (jcmInfoAfter.getJitCpuTime() < jcmInfoBefore.getJitCpuTime() || (appUserFail > 0))) {
			Assert.fail("FAIL:  Invalid CPU time info retrieved");
		}
		logger.info("PASS: Application CPU values increased after activity");
		return false; /* No error */
	}

	/**
	 *
	 * @param jcmmxbean The JvmCpuMonitorMXBean instance that has already been initialized.
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */

	private static boolean test_setThreadCategory(ThreadMXBean thdmxbean, JvmCpuMonitorMXBean jcmmxbean, boolean local) {
		long[] ids = thdmxbean.getAllThreadIds();
		String thdCat = null;
		int gotException = 0;
		long threadId = 0;
		int retval = 0;

		for (int i = 0; i < ids.length; i++) {
			thdCat = jcmmxbean.getThreadCategory(ids[i]);
			//logger.debug("Thread Id: " + threadId + " category: " + jcmmxbean.getThreadCategory(threadId));
			gotException = 0;
			if ((thdCat.equalsIgnoreCase("System-JVM")) || (thdCat.equalsIgnoreCase("GC")) || (thdCat.equalsIgnoreCase("JIT"))) {
				try {
					// Try to set the thread category of a System-JVM, GC or JIT thread
					// as an application category thread. This should fail
					retval = jcmmxbean.setThreadCategory(ids[i], "Application");
				} catch (IllegalArgumentException exception) {
					logger.debug("PASS: Got exception changing the category of a " + thdCat + " thread to \"Application\" category");
					gotException = 1;
				}
				if (0 == gotException) {
					Assert.fail("FAIL: Able to change the category of a " + thdCat + " thread to \"Application\" category");
				}

				try {
					// Try to set the thread category of a System-JVM, GC or JIT thread
					// as an application category thread. This should fail
					retval = jcmmxbean.setThreadCategory(ids[i], "Application-User5");
				} catch (IllegalArgumentException exception) {
					logger.debug("PASS: Got exception changing the category of a " + thdCat + " thread to \"Application-User5\" category");
					gotException = 1;
				}
				if (0 == gotException) {
					Assert.fail("FAIL: Able to change the category of a " + thdCat + " thread to \"Application-User5\" category");
				}

				try {
					// Try to set the thread category of a System-JVM, GC or JIT thread
					// as a Resource-Monitor category thread. This should fail
					retval = jcmmxbean.setThreadCategory(ids[i], "Resource-Monitor");
				} catch (IllegalArgumentException exception) {
					logger.debug("PASS: Got exception changing the category of a " + thdCat + " thread to \"Resource-Monitor\" category");
					gotException = 1;
				}
				if (0 == gotException) {
					Assert.fail("FAIL: Able to change the category of a " + thdCat + " thread to \"Resource-Monitor\" category");
				}
			}
		}
		try {
			ExecutorService eService = Executors.newSingleThreadExecutor();
			TestSetCategory testSetCategory = new TestSetCategory(thdmxbean, jcmmxbean, local);
			Future<Boolean> fVal = eService.submit(testSetCategory);
			Boolean result = fVal.get();
			return result.booleanValue();
		} catch (Exception e) {
			Assert.fail("Unexpected exception occured : " + e.getMessage() , e);
		}
		logger.info("test_setThreadCategory done.");
		return false;
	}

	/**
	 * Internal function: Starts a remote server to connect to.
	 */
	private static void startRemoteMonitor() throws IOException {
		logger.info("Starting Remote Server to Monitor!");

		ArrayList<String> argBuffer = new ArrayList<String>();
		char fs = File.separatorChar;

		/* Set up a randomly named file and pass this to child process. Parent keeps checking
		 * for its existence on the file-system. When this goes missing, it implies the child
		 * process is up and running.
		 */
		File file = File.createTempFile("tmp", ".lock");
		String tmpName = file.getAbsolutePath();

		String javaExec = System.getProperty("java.home") + fs + "bin" + fs + "java";
		argBuffer.add(javaExec);
		argBuffer.add("-classpath");
		argBuffer.add(System.getProperty("java.class.path"));
		argBuffer.add(System.getProperty("remote.server.option"));
		/* The lock file that the parent and child processes use for event based synchronization. */
		argBuffer.add("-Djava.lock.file=" + tmpName);
		argBuffer.add(RemoteMonitor.class.getName());

		String cmdLine[] = new String[argBuffer.size()];
		argBuffer.toArray(cmdLine);

		String cmds = "";
		for (int i = 0; i < cmdLine.length; i++) {
			cmds = cmds + cmdLine[i] + " ";
		}

		logger.info(cmds);
		try {
			lock = new ProcessLocking(tmpName);
			remoteMonitor = Runtime.getRuntime().exec(cmds);
			/* Wait for the child process to signal that it has started before proceed. */

		} catch (IOException e) {
			Assert.fail("Failed to launch child process: " + remoteMonitor, e);
		}

		BufferedReader in = new BufferedReader( new InputStreamReader(remoteMonitor.getInputStream()));
		String rSrvStdout;
		try {
			if ((rSrvStdout = in.readLine()) != null) {
				logger.info("Remote Server stdout: " + rSrvStdout);
			}
		} catch (IOException e) {
			Assert.fail("Exception occurred while waiting for server.", e);
		}

		try {
			Thread.sleep(1000);
		} catch (InterruptedException e) {
			logger.warn("InterruptedException occured" + e.getMessage(), e);
		}

		lock.waitForEvent("child started");
		logger.debug("staring remoteServet finished");
	}

	/**
	 * Internal function: Stops a remote server.
	 */
	private static void stopRemoteMonitor() {
		watchdog.interrupt();
		remoteMonitor.destroy();
		try {
			Thread.sleep(10000);
		} catch (InterruptedException e) {
			logger.warn("InterruptedException occured" + e.getMessage(), e);
		}
	}
}

class TestSetCategory implements Callable<Boolean> {

	private static final Logger logger = Logger.getLogger(TestSetCategory.class);

	JvmCpuMonitorMXBean jcmmxbean = null;
	ThreadMXBean thdmxbean = null;
	boolean local = true;

	public TestSetCategory(ThreadMXBean tbean, JvmCpuMonitorMXBean jbean, boolean l) {
		thdmxbean = tbean;
		jcmmxbean = jbean;
		local = l;
	}

	public Boolean call() {
		long threadId = 0;
		String thdCat = null;
		int gotException = 0;
		Boolean retval = Boolean.FALSE;

		/* In the local case, the current thread should be of "Application" category */
		if (local) {
			threadId = Thread.currentThread().getId();
		} else {
			/* In the remote case, find any thread that is of "Application" category */
			long[] ids = thdmxbean.getAllThreadIds();
			for (int j = 0; j < ids.length; j++) {
				thdCat = jcmmxbean.getThreadCategory(ids[j]);
				if (thdCat.equalsIgnoreCase("Application")) {
					threadId = ids[j];
					break;
				}
			}
		}

		try {
			gotException = 0;
			// Set the currently executing thread as some GARBAGE thread category
			jcmmxbean.setThreadCategory(threadId, "GARBAGE");
		} catch (IllegalArgumentException exception) {
			gotException = 1;
		}

		if (0 == gotException) {
			Assert.fail("FAIL: Able to set Invalid thread category");
		} else {
			logger.debug("PASS: Unable to set Invalid thread category");
		}

		try {
			gotException = 0;
			// Set the currently executing thread as Application-User1 thread category
			jcmmxbean.setThreadCategory(threadId, "Application-User1");
		} catch (IllegalArgumentException exception) {
			gotException = 1;
		}

		if (1 == gotException) {
			Assert.fail("FAIL: Unable to set thread category as \"Application-User1\". FAIL: Current thread category: " + jcmmxbean.getThreadCategory(threadId) );
		} else {
			logger.debug("PASS: Able to set the current thread as \"Application-User1\"");
		}

		try {
			gotException = 0;
			// Set the currently executing thread as Application-User3 thread category
			jcmmxbean.setThreadCategory(threadId, "Application-User3");
		} catch (IllegalArgumentException exception) {
			gotException = 1;
		}

		if (1 == gotException) {
			Assert.fail("FAIL: Unable to set thread category as \"Application-User3\". " + "FAIL: Current thread category: " + jcmmxbean.getThreadCategory(threadId));
		} else {
			logger.debug("PASS: Able to set the current thread as \"Application-User3\"");
		}

		try {
			gotException = 0;
			// Set the currently executing thread as Application-User5 thread category
			jcmmxbean.setThreadCategory(threadId, "Application-User5");
		} catch (IllegalArgumentException exception) {
			gotException = 1;
		}

		if (1 == gotException) {
			Assert.fail("FAIL: Unable to set thread category as \"Application-User5\". " + "FAIL: Current thread category: " + jcmmxbean.getThreadCategory(threadId));
		} else {
			logger.debug("PASS: Able to set the current thread as \"Application-User5\"");
		}

		try {
			gotException = 0;
			// Set the currently executing thread as Application thread category
			jcmmxbean.setThreadCategory(threadId, "Application");
		} catch (IllegalArgumentException exception) {
			gotException = 1;
		}

		if (1 == gotException) {
			Assert.fail("FAIL: Unable to set thread category as \"Application\". " + "FAIL: Current thread category: " + jcmmxbean.getThreadCategory(threadId));
		} else {
			logger.debug("PASS: Able to set the current thread as \"Application\"");
		}

		try {
			gotException = 0;
			// Set the currently executing thread as GC thread category, this should fail
			jcmmxbean.setThreadCategory(threadId, "GC");
		} catch (IllegalArgumentException exception) {
			gotException = 1;
		}

		if (0 == gotException) {
			Assert.fail("FAIL: Able to set application thread to \"GC\" category");
		} else {
			logger.debug("PASS: Unable to set application thread to \"GC\" category");
		}

		try {
			gotException = 0;
			// Set the currently executing thread as JIT thread category, this should fail
			jcmmxbean.setThreadCategory(threadId, "JIT");
		} catch (IllegalArgumentException exception) {
			gotException = 1;
		}

		if (0 == gotException) {
			Assert.fail("FAIL: Able to set application thread to \"JIT\" category");
		} else {
			logger.debug("PASS: Unable to set application thread to \"JIT\" category");
		}

		try {
			gotException = 0;
			// Set the currently executing thread as System-JVM thread category, this should fail
			jcmmxbean.setThreadCategory(threadId, "System-JVM");
		} catch (IllegalArgumentException exception) {
			gotException = 1;
		}

		if (0 == gotException) {
			Assert.fail("FAIL: Able to set application thread to \"System-JVM\" category");
		} else {
			logger.debug("PASS: Unable to set application thread to \"System-JVM\" category");
		}

		try {
			gotException = 0;
			// Set the currently executing thread as Application-User0 thread category, this should fail
			jcmmxbean.setThreadCategory(threadId, "Application-User0");
		} catch (IllegalArgumentException exception) {
			gotException = 1;
		}

		if (0 == gotException) {
			Assert.fail("FAIL: Able to set application thread to \"Application-User0\" category");
		} else {
			logger.debug("PASS: Unable to set application thread to \"Application-User0\" category");
		}

		try {
			gotException = 0;
			// Set the currently executing thread as Application-User6 thread category, this should fail
			jcmmxbean.setThreadCategory(threadId, "Application-User6");
		} catch (IllegalArgumentException exception) {
			gotException = 1;
		}

		if (0 == gotException) {
			Assert.fail("FAIL: Able to set application thread to \"Application-User6\" category");
		} else {
			logger.debug("PASS: Unable to set application thread to \"Application-User6\" category");
		}

		try {
			gotException = 0;
			// Set the currently executing thread as Resource-Monitor thread category
			jcmmxbean.setThreadCategory(threadId, "Resource-Monitor");
		} catch (IllegalArgumentException exception) {
			gotException = 1;
		}

		if (1 == gotException) {
			Assert.fail("FAIL: Unable to set thread category as \"Resource-Monitor\". " + "FAIL: Current thread category: " + jcmmxbean.getThreadCategory(threadId));
		} else {
			logger.debug("PASS: Able to set the current thread as \"Resource-Monitor\"");
		}

		try {
			gotException = 0;
			// Try to change the category of the current thread from Resource-Monitor back to Application
			// This should fail
			jcmmxbean.setThreadCategory(threadId, "Application");
		} catch (IllegalArgumentException exception) {
			gotException = 1;
		}

		if (0 == gotException) {
			Assert.fail("FAIL: Able to change the category of a \"Resource-Monitor\" thread");
		} else {
			logger.debug("PASS: Unable to change the category of a \"Resource-Monitor\" thread");
		}

		return retval;
	}
}
