/*
 * Copyright IBM Corp. and others 2022
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */

package org.openj9.test.java.lang.management;

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
public class TestSharedClassMemoryMXBean_SM {

	private static Logger logger = Logger.getLogger(TestSharedClassMemoryMXBean_SM.class);

	@BeforeClass
	protected void setUp() throws Exception {
		logger.info("Starting TestSharedClassMemoryMXBean_SM tests ...");
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
}
