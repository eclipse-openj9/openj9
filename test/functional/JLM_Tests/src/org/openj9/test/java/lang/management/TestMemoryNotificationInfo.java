/*******************************************************************************
 * Copyright (c) 2005, 2019 IBM Corp. and others
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
import java.lang.management.MemoryNotificationInfo;
import java.lang.management.MemoryUsage;

import javax.management.openmbean.CompositeData;

// This class is not public API.
import com.ibm.java.lang.management.internal.MemoryNotificationInfoUtil;

@Test(groups = { "level.sanity" })
public class TestMemoryNotificationInfo {

	private static Logger logger = Logger.getLogger(TestMemoryNotificationInfo.class);

	private MemoryNotificationInfo goodMNI;

	private MemoryUsage goodMU;

	private static final long MU_INIT_VAL = 100;

	private static final long MU_USED_VAL = 150;

	private static final long MU_COMMITTED_VAL = 500;

	private static final long MU_MAX_VAL = 1000;

	private static final String NAME_VAL = "Brian";

	private static final long COUNT_VAL = 350;

	@BeforeClass
	protected void setUp() throws Exception {
		goodMU = new MemoryUsage(MU_INIT_VAL, MU_USED_VAL, MU_COMMITTED_VAL, MU_MAX_VAL);
		goodMNI = new MemoryNotificationInfo(NAME_VAL, goodMU, COUNT_VAL);
		logger.info("Starting TestMemoryNotificationInfo tests ...");
	}

	@AfterClass
	protected void tearDown() throws Exception {
	}

	@Test
	public final void testGetCount() {
		AssertJUnit.assertEquals(COUNT_VAL, goodMNI.getCount());
	}

	@Test
	public final void testGetPoolName() {
		AssertJUnit.assertEquals(NAME_VAL, goodMNI.getPoolName());
	}

	@Test
	public final void testGetMemoryUsage() {
		AssertJUnit.assertEquals(goodMU, goodMNI.getUsage());
	}

	@Test
	public final void testFrom() {
		CompositeData cd = MemoryNotificationInfoUtil.toCompositeData(goodMNI);
		AssertJUnit.assertNotNull(cd);
		MemoryNotificationInfo mni = MemoryNotificationInfo.from(cd);
		AssertJUnit.assertNotNull(mni);

		// Verify that the MemoryNotificationInfo contains what we
		// *think* it contains.
		AssertJUnit.assertEquals(NAME_VAL, mni.getPoolName());
		AssertJUnit.assertEquals(COUNT_VAL, mni.getCount());

		MemoryUsage mu = mni.getUsage();
		AssertJUnit.assertNotNull(mu);
		AssertJUnit.assertEquals(MU_INIT_VAL, mu.getInit());
		AssertJUnit.assertEquals(MU_COMMITTED_VAL, mu.getCommitted());
		AssertJUnit.assertEquals(MU_MAX_VAL, mu.getMax());
		AssertJUnit.assertEquals(MU_USED_VAL, mu.getUsed());
	}
}
