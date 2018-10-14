/*******************************************************************************
 * Copyright (c) 2005, 2018 IBM Corp. and others
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
import java.lang.management.ManagementFactory;
import java.security.AccessControlException;

import com.ibm.lang.management.MemoryMXBean;

import org.openj9.test.util.process.ProcessRunner;
import org.openj9.test.util.process.Task;


@Test(groups = { "level.sanity" })
public class TestSharedClassMemoryMXBean {

	private static Logger logger = Logger.getLogger(TestSharedClassMemoryMXBean.class);
	private MemoryMXBean mb;

	@BeforeClass
	protected void setUp() throws Exception {
		mb = (MemoryMXBean)ManagementFactory.getMemoryMXBean();
		logger.info("Starting TestSharedClassMemoryMXBean tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
	}

	static class ClassForTestSharedClassSoftmx implements Task {
		@Override
		public void run() throws Exception {
			System.setSecurityManager(new SecurityManager());
			MemoryMXBean bean = (MemoryMXBean)ManagementFactory.getMemoryMXBean();
			long size = 10 * 1024 * 1024;

			bean.setSharedClassCacheSoftmxBytes(size);
			AssertJUnit.assertTrue(size == bean.getSharedClassCacheSoftmxBytes());
		}
	}

	static class ClassForTestSharedClassMaxAotBytes implements Task {
		@Override
		public void run() throws Exception {
			System.setSecurityManager(new SecurityManager());
			MemoryMXBean bean = (MemoryMXBean)ManagementFactory.getMemoryMXBean();
			long size = 1024 * 1024;

			bean.setSharedClassCacheMaxAotBytes(size);
			AssertJUnit.assertTrue(size == bean.getSharedClassCacheMaxAotBytes());
		}
	}

	static class ClassForTestSharedClassMinAotBytes implements Task {
		@Override
		public void run() throws Exception {
			System.setSecurityManager(new SecurityManager());
			MemoryMXBean bean = (MemoryMXBean)ManagementFactory.getMemoryMXBean();
			long size = 1024 * 1024;

			bean.setSharedClassCacheMinAotBytes(size);
			AssertJUnit.assertTrue(size == bean.getSharedClassCacheMinAotBytes());
		}
	}

	static class ClassForTestSharedClassMaxJitDataBytes implements Task {
		@Override
		public void run() throws Exception {
			System.setSecurityManager(new SecurityManager());
			MemoryMXBean bean = (MemoryMXBean)ManagementFactory.getMemoryMXBean();
			long size = 1024 * 1024;

			bean.setSharedClassCacheMaxJitDataBytes(size);
			AssertJUnit.assertTrue(size == bean.getSharedClassCacheMaxJitDataBytes());
		}
	}

	static class ClassForTestSharedClassMinJitDataBytes implements Task {
		@Override
		public void run() throws Exception {
			System.setSecurityManager(new SecurityManager());
			MemoryMXBean bean = (MemoryMXBean)ManagementFactory.getMemoryMXBean();
			long size = 1024 * 1024;

			bean.setSharedClassCacheMinJitDataBytes(size);
			AssertJUnit.assertTrue(size == bean.getSharedClassCacheMinJitDataBytes());
		}
	}

	@Test
	public void testGetSharedClassCacheSize() {
		try {
			long val = mb.getSharedClassCacheSize();
			AssertJUnit.assertTrue(val > -1);
			logger.debug("Shared class cache size = " + val);
		} catch (Throwable e) {
			Assert.fail("Caught unexpected exception : " + e.getMessage());
		}
	}

	@Test
	public void testSharedClassSoftmxSize() {
		try {
			long val = mb.getSharedClassCacheSoftmxBytes();
			long softmxValue = 10 * 1024 * 1024;
			long unstoreBytes = mb.getSharedClassCacheSoftmxUnstoredBytes();
			logger.debug("Shared class softmx size = " + val);
			AssertJUnit.assertTrue(unstoreBytes > 0);

			try {
				mb.setSharedClassCacheSoftmxBytes(-1);
				Assert.fail("testSharedClassSoftmxSize: should throw IllegalArgumentException");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
			}

			if (mb.setSharedClassCacheSoftmxBytes(softmxValue)) {
				unstoreBytes = mb.getSharedClassCacheSoftmxUnstoredBytes();
				val = mb.getSharedClassCacheSoftmxBytes();

				AssertJUnit.assertTrue(0 == unstoreBytes);
				AssertJUnit.assertTrue(val == softmxValue);
				logger.debug("Shared class softmx size = " + val);
			} else {
				Assert.fail("Failed to set the shared class softmx");
			}
		} catch (Throwable e) {
			e.printStackTrace();
			Assert.fail("Caught unexpected exception : " + e.getMessage());
		}
	}

	@Test
	public void testSeSharedClassSoftmxSizeWithSecurityManager() throws Exception {
		try {
			if (!System.getProperty("os.name").toLowerCase().contains("aix")) {
				/*
				 * It hangs on AIX 64 bit at line: socket = serverSock.accept() in ProcessRunner.initializeCommunication()
				 */
				new ProcessRunner(new ClassForTestSharedClassSoftmx()).run();
				Assert.fail(
						"testSeSharedClassSoftmxSizeWithSecurityManager: should throw java.security.AccessControlException: "
								+ "Access denied (\"java.lang.management.ManagementPermission\" \"control\")");
			}
		} catch (AccessControlException e) {
			AssertJUnit.assertTrue(e.getMessage().contains("ManagementPermission"));
			logger.debug(e);
		}
	}

	@Test
	public void testSharedClassMaxAotBytes() {
		try {
			long val = mb.getSharedClassCacheMaxAotBytes();
			long maxAotValue = 1024 * 1024;
			logger.debug("Shared class maxAOT bytes = " + val);
			AssertJUnit.assertTrue(val == -1);

			try {
				mb.setSharedClassCacheMaxAotBytes(-1);
				Assert.fail("testSharedClassMaxAotBytes: should throw IllegalArgumentException");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
			}

			if (mb.setSharedClassCacheMaxAotBytes(maxAotValue)) {
				val = mb.getSharedClassCacheMaxAotBytes();
				AssertJUnit.assertTrue(val == maxAotValue);
				logger.debug("Shared class maxAOT bytes = " + val);
			} else {
				Assert.fail("Failed to set the shared class maxAOT");
			}

		} catch (Throwable e) {
			Assert.fail("Caught unexpected exception : " + e.getMessage());
		}
	}

	@Test
	public void testSeSharedClassMaxAotWithSecurityManager() throws Exception {
		try {
			if (!System.getProperty("os.name").toLowerCase().contains("aix")) {
				/*
				 * It hangs on AIX 64 bit at line: socket = serverSock.accept() in ProcessRunner.initializeCommunication()
				 */
				new ProcessRunner(new ClassForTestSharedClassMaxAotBytes()).run();
				Assert.fail(
						"testSeSharedClassMaxAotWithSecurityManager: should throw java.security.AccessControlException: "
								+ "Access denied (\"java.lang.management.ManagementPermission\" \"control\")");
			}
		} catch (AccessControlException e) {
			AssertJUnit.assertTrue(e.getMessage().contains("ManagementPermission"));
			logger.debug(e);
		}
	}

	@Test
	public void testSharedClassMinAotBytes() {
		try {
			long val = mb.getSharedClassCacheMinAotBytes();
			long minAotValue = 1024 * 1024;
			logger.debug("Shared class minAOT bytes = " + val);
			AssertJUnit.assertTrue(val == -1);

			try {
				mb.setSharedClassCacheMinAotBytes(-1);
				Assert.fail("testSharedClassMinAotBytes: should throw IllegalArgumentException");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
			}

			if (mb.setSharedClassCacheMinAotBytes(minAotValue)) {
				val = mb.getSharedClassCacheMinAotBytes();
				AssertJUnit.assertTrue(val == minAotValue);
				logger.debug("Shared class minAOT bytes = " + val);
			} else {
				Assert.fail("Failed to set the shared class minAOT");
			}

		} catch (Throwable e) {
			Assert.fail("Caught unexpected exception : " + e.getMessage());
		}
	}

	@Test
	public void testSeSharedClassMinAotWithSecurityManager() throws Exception {
		try {
			if (!System.getProperty("os.name").toLowerCase().contains("aix")) {
				/*
				 * It hangs on AIX 64 bit at line: socket = serverSock.accept() in ProcessRunner.initializeCommunication()
				 */
				new ProcessRunner(new ClassForTestSharedClassMinAotBytes()).run();
				Assert.fail(
						"testSeSharedClassMinAotWithSecurityManager: should throw java.security.AccessControlException: "
								+ "Access denied (\"java.lang.management.ManagementPermission\" \"control\")");
			}
		} catch (AccessControlException e) {
			AssertJUnit.assertTrue(e.getMessage().contains("ManagementPermission"));
			logger.debug(e);
		}
	}

	@Test
	public void testSharedClassMaxJitDataBytes() {
		try {
			long val = mb.getSharedClassCacheMaxJitDataBytes();
			long maxJitValue = 1024 * 1024;
			logger.debug("Shared class maxJIT bytes = " + val);
			AssertJUnit.assertTrue(val == -1);

			try {
				mb.setSharedClassCacheMaxJitDataBytes(-1);
				Assert.fail("testSharedClassMaxJitDataBytes: should throw IllegalArgumentException");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
			}

			if (mb.setSharedClassCacheMaxJitDataBytes(maxJitValue)) {
				val = mb.getSharedClassCacheMaxJitDataBytes();
				AssertJUnit.assertTrue(val == maxJitValue);
				logger.debug("Shared class maxJIT bytes = " + val);
			} else {
				Assert.fail("Failed to set the shared class maxJIT");
			}
		} catch (Throwable e) {
			Assert.fail("Caught unexpected exception : " + e.getMessage());
		}
	}

	@Test
	public void testSeSharedClassMaxJitDataWithSecurityManager() throws Exception {
		try {
			if (!System.getProperty("os.name").toLowerCase().contains("aix")) {
				/*
				 * It hangs on AIX 64 bit at line: socket = serverSock.accept() in ProcessRunner.initializeCommunication()
				 */
				new ProcessRunner(new ClassForTestSharedClassMaxJitDataBytes()).run();
				Assert.fail(
						"testSeSharedClassMaxJitDataWithSecurityManager: should throw java.security.AccessControlException: "
								+ "Access denied (\"java.lang.management.ManagementPermission\" \"control\")");
			}
		} catch (AccessControlException e) {
			AssertJUnit.assertTrue(e.getMessage().contains("ManagementPermission"));
			logger.debug(e);
		}
	}

	@Test
	public void testSharedClassMinJitDataBytes() {
		try {
			long val = mb.getSharedClassCacheMinJitDataBytes();
			long minJitValue = 1024 * 1024;
			logger.debug("Shared class minJIT bytes = " + val);
			AssertJUnit.assertTrue(val == -1);

			try {
				mb.setSharedClassCacheMinJitDataBytes(-1);
				Assert.fail("testSharedClassMinJitDataBytes: should throw IllegalArgumentException");
			} catch (Exception e) {
				AssertJUnit.assertTrue(e instanceof IllegalArgumentException);
			}

			if (mb.setSharedClassCacheMinJitDataBytes(minJitValue)) {
				val = mb.getSharedClassCacheMinJitDataBytes();
				AssertJUnit.assertTrue(val == minJitValue);
				logger.debug("Shared class minJIT bytes = " + val);
			} else {
				Assert.fail("Failed to set the shared class minJIT");
			}
		} catch (Throwable e) {
			Assert.fail("Caught unexpected exception : " + e.getMessage());
		}
	}

	@Test
	public void testSeSharedClassMinJitDataWithSecurityManager() throws Exception {
		try {
			if (!System.getProperty("os.name").toLowerCase().contains("aix")) {
				/*
				 * It on hangs on AIX 64 bit at line: socket = serverSock.accept() in ProcessRunner.initializeCommunication()
				 */
				new ProcessRunner(new ClassForTestSharedClassMinJitDataBytes()).run();
				Assert.fail(
						"testSeSharedClassMinJitDataWithSecurityManager: should throw java.security.AccessControlException: "
								+ "Access denied (\"java.lang.management.ManagementPermission\" \"control\")");
			}
		} catch (AccessControlException e) {
			AssertJUnit.assertTrue(e.getMessage().contains("ManagementPermission"));
			logger.debug(e);
		}
	}

	@Test
	public void testGetSharedClassCacheFreeSpace() {
		try {
			long val = mb.getSharedClassCacheFreeSpace();
			AssertJUnit.assertTrue(val > -1);
			logger.debug("Shared class cache free space = " + val);
		} catch (Throwable e) {
			Assert.fail("Caught unexpected exception : " + e.getMessage());
		}
	}
}
