/*
 * Copyright IBM Corp. and others 2005
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

import org.testng.annotations.AfterClass;
import org.testng.annotations.Test;
import org.testng.log4testng.Logger;
import org.testng.annotations.BeforeClass;
import org.testng.Assert;
import org.testng.AssertJUnit;
import java.lang.management.ManagementFactory;

import com.ibm.lang.management.MemoryMXBean;

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
