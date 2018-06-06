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
import org.testng.AssertJUnit;
import java.lang.management.ManagementFactory;
import java.lang.management.MemoryPoolMXBean;
import java.util.Iterator;
import java.util.List;

// This class is not public API.
import com.ibm.java.lang.management.internal.ManagementUtils;

@Test(groups = { "level.sanity" })
public class TestMisc {

	private static Logger logger = Logger.getLogger(TestMisc.class);

	@BeforeClass
	protected void setUp() throws Exception {
		logger.info("Starting TestMisc tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
	}

	@Test
	public void testVerboseSetting() {
		if (System.getSecurityManager() == null) {
			String val1 = System.getProperty("com.ibm.lang.management.verbose");

			if (val1 != null) {
				AssertJUnit.assertTrue(ManagementUtils.VERBOSE_MODE);
			} else {
				AssertJUnit.assertFalse(ManagementUtils.VERBOSE_MODE);
			} // end else com.ibm.lang.management.verbose was already set
		}
	}

	@Test
	public final void testWhoSupportsMemoryUsageThresholds() {
		List<MemoryPoolMXBean> allPools = ManagementFactory.getMemoryPoolMXBeans();
		Iterator<MemoryPoolMXBean> it = allPools.iterator();
		while (it.hasNext()) {
			MemoryPoolMXBean pool = it.next();
			logger.debug("Pool " + pool.getName() + ", supports usage" + " threshold ? "
					+ pool.isUsageThresholdSupported() + ", " + "supports collection usage threshold ? "
					+ pool.isCollectionUsageThresholdSupported());
		}
	}

	static String[] getJava9STEMethodNames() {
		String[] methodNames = {
				"className", //$NON-NLS-1$
				"methodName", //$NON-NLS-1$
				"fileName", //$NON-NLS-1$
				"lineNumber", //$NON-NLS-1$
				"nativeMethod", //$NON-NLS-1$
				"moduleName", //$NON-NLS-1$
				"moduleVersion", //$NON-NLS-1$
				"classLoaderName", //$NON-NLS-1$
		};
		return methodNames;
	}

}
