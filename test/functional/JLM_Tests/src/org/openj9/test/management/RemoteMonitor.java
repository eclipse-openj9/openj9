/*******************************************************************************
 * Copyright (c) 2015, 2018 IBM Corp. and others
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

import org.openj9.test.management.util.WorkerThread;
import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
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

import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.CountDownLatch;

import org.openj9.test.management.util.*;
import com.ibm.lang.management.*;

/**
 * Start a server that busy loops with various application thread categories.
 * This is used by JvmCpuMonitorMXBean to get remote CPU usage info on a per thread category basis.
 */

public class RemoteMonitor {

	public static final Logger logger = Logger.getLogger(RemoteMonitor.class);

	private static final int NTHREADS = 10;
	private static final int SLEEPINTERVAL = 5;
	private static ProcessLocking lock;

	/**
	 * Start the RemoteServer with jmx enabled, for example:
	 * -Dcom.sun.management.jmxremote.port=9999
	 * -Dcom.sun.management.jmxremote.authenticate=false
	 * -Dcom.sun.management.jmxremote.ssl=false
	 */
	public static void main(String[] args) {
		boolean error = false;
		MBeanServer mbeanServer = null;
		ObjectName jcmObjN = null;
		JvmCpuMonitorMXBean jcmmxbean = null;
		String lockFile = null;

		logger.info("=========RemoteMonitor Starts!=========");
		lockFile = System.getProperty("java.lock.file");

		Assert.assertTrue(null != lockFile, "lockFile from -Djava.lock.file is null ");
		lock = new ProcessLocking(lockFile);
		lock.notifyEvent("child started");

		/* We need the object name in any case - remote as well as local. */
		try {
			jcmObjN = new ObjectName("com.ibm.lang.management:type=JvmCpuMonitor");
		} catch (MalformedObjectNameException e) {
			Assert.fail("MalformedObjectNameException!", e);
		}
		try {
			mbeanServer = ManagementFactory.getPlatformMBeanServer();
			if (true != mbeanServer.isRegistered(jcmObjN)) {
				Assert.fail("JvmCpuMonitorMXBean is not registered. " + "Cannot Proceed with the test. !!!Test Failed !!!");
			}
			jcmmxbean = JMX.newMXBeanProxy(mbeanServer, jcmObjN, JvmCpuMonitorMXBean.class);
		} catch (Exception e) {
			Assert.fail("Exception occurred in setting up local test environment.", e);
		}

		error |= test_getThreadsCpuUsage(jcmmxbean);
		/* Ensure the child process does NOT race towards completion even as the parent is
		 * waiting to call the connector.close() on the JMX connection to this process.
		 */
		lock.waitForEvent("closed JMX connection");
		logger.info("========RemoteMonitor Complete=========");
	}

	/**
	 *
	 * @param jcmmxbean The JvmCpuMonitorMXBean instance that has already been initialized.
	 *
	 * @return false, if the test runs without any errors, true otherwise.
	 */
	private static boolean test_getThreadsCpuUsage(JvmCpuMonitorMXBean jcmmxbean) {
		long threadId = Thread.currentThread().getId();
		int appUserFail = 0;

		try {
			/* Require joinLatch to prevent threads from joining before their CPU usages have been examined. */
			CountDownLatch joinLatch = new CountDownLatch(NTHREADS);
			/* Require startLatch to prevent main thread from proceeding (for CPU usages to be measured remotely) before
			 * all threads have executed their share of busy time.
			 */
			CountDownLatch startLatch = new CountDownLatch(1);
			Thread[] busyObj = new Thread[NTHREADS];
			String baseName = "Busy-Thread";
			/* Let each thread do some work. */
			for (int i = 0; i < NTHREADS; i++) {
				WorkerThread worker = new WorkerThread(startLatch, joinLatch, SLEEPINTERVAL * 500);
				try {
					/* Set thread names to "Busy-Thread<integer>". */
					busyObj[i] = new Thread(worker, baseName.concat(Integer.toString(i + 1)));
					busyObj[i].start();
				} catch (Exception e) {
					Assert.fail("FAIL: Got exception setting the thread name", e);
				}
			}
			lock.notifyEvent("all threads started");
			/* Wait for the parent process to reach a point where it is ready to gather usage and does *not* run past. */
			lock.waitForEvent("set thread categories");
			startLatch.countDown();
			/* Do not exit the thread just yet; wait until the thread's have done some measurable amount of
			 * work that the parent process can query about.
			 */
			try {
				joinLatch.await();
			} catch (InterruptedException e) {
				Assert.fail("FAIL: Got exception awaiting thread join", e);
			}
			/* Signal the parent that threads have been started and running (or awaiting join) until such
			 * time when it (the parent) has "collected" info for all threads. Block calling waitForEvent() so
			 * that threads do not die off before parent can collect these.
			 */
			lock.notifyEvent("CPU usages available");
			lock.waitForEvent("done collecting threadinfo");
			for (int i = 0; i < NTHREADS; i++) {
				busyObj[i].join();
			}
		} catch (Exception e) {
			Assert.fail("FAIL: Call to getThreadsCpuUsage failed", e);
		}

		return false; /* No error */
	}
}
