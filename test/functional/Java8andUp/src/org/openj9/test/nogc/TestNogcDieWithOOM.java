/*******************************************************************************
 * Copyright (c) 2018, 2018 IBM Corp. and others
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
package org.openj9.test.nogc;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.testng.annotations.BeforeTest;
import org.testng.annotations.AfterTest;
import org.testng.log4testng.Logger;

import java.lang.management.GarbageCollectorMXBean;
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryPoolMXBean;
import java.lang.management.MemoryType;

import java.util.List;
import java.util.Random;

@Test(groups={ "level.extended" })
public class TestNogcDieWithOOM {
	static Logger logger = Logger.getLogger(TestNogcDieWithOOM.class);

	@Test (priority=1)
	public final void testNogcModeEnabled() {
		logger.debug("testNogcModeEnabled start");
		List<GarbageCollectorMXBean> gcBeans = ManagementFactory.getGarbageCollectorMXBeans();
		Assert.assertTrue(1 == gcBeans.size());
		GarbageCollectorMXBean gcBean = gcBeans.get(0);
		Assert.assertEquals(gcBean.getName(), "Epsilon");
		for (MemoryPoolMXBean memPoolBean : ManagementFactory.getMemoryPoolMXBeans()) {
			if (MemoryType.HEAP == memPoolBean.getType()) {
				Assert.assertEquals(memPoolBean.getName(), "tenured");
			}
		}
		logger.debug("testNogcModeEnabled end");
	}

	@Test (priority=2)
	public void testDieWithOOM() {
		logger.debug("testDieWithOOM");
		Random random = new Random();
		String message = "Caught expected java/lang/OutOfMemoryError: Java heap space";
		byte[] data = null;
		data = allocateMemory(3 * 1024*1024);
		try {
			for (int cnt=0; cnt<1024; cnt++) {
				int multi = random.nextInt(10);
				data = allocateMemory(((0 == multi)?1:multi) * 10 * 1024*1024);
			}
		} catch (OutOfMemoryError error) {
			logger.info(message);
			System.exit(0);
		}
		Assert.fail("Expecting Out of Memory Error!");
	}

	private byte[] allocateMemory(long bytes) {
		return new byte[(int) bytes];
	}
	
	@BeforeTest
	public void beforeTest() {
		logger.debug("beforeTest");
	}
	
	@AfterTest
	public void afterTest() {
		logger.debug("afterTest");
	}
}
