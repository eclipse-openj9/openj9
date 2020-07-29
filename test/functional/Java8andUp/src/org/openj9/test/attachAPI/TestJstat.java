/*******************************************************************************
 * Copyright (c) 2019, 2020 IBM Corp. and others
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

package org.openj9.test.attachAPI;

import org.testng.AssertJUnit;

import java.io.IOException;
import java.util.Arrays;
import java.util.List;
import java.util.Optional;

import org.openj9.test.util.PlatformInfo;
import org.openj9.test.util.StringUtilities;
import org.testng.annotations.BeforeSuite;
import org.testng.annotations.Test;

@Test(groups = { "level.extended" })
public class TestJstat extends AttachApiTest {

	private static final String JSTAT_COMMAND = "jstat"; //$NON-NLS-1$
	private static final String JSTAT_OPTION_CLASS = "-class"; //$NON-NLS-1$
	private static final String JSTAT_OPTION_CLASS_HEADER = "Class Loaded    Class Unloaded"; //$NON-NLS-1$
	Object syncObject = new Object();
	private String vmId;

	@Test
	public void testOptions() throws IOException {
		List<String> jstatOutput = runCommand(Arrays.asList("-options", vmId)); //$NON-NLS-1$
		logOutput(jstatOutput, JSTAT_COMMAND);
		Optional<String> searchResult = StringUtilities.searchSubstring(JSTAT_OPTION_CLASS, jstatOutput);
		AssertJUnit.assertTrue(JSTAT_OPTION_CLASS + " missing", searchResult.isPresent()); //$NON-NLS-1$
	}

	@Test
	public void testOptionClass() throws IOException {
		List<String> jstatOutput = runCommand(Arrays.asList(JSTAT_OPTION_CLASS, vmId));
		logOutput(jstatOutput, JSTAT_COMMAND);
		Optional<String> searchResult = StringUtilities.searchSubstring(JSTAT_OPTION_CLASS_HEADER, jstatOutput);
		AssertJUnit.assertTrue(JSTAT_OPTION_CLASS_HEADER + " missing", searchResult.isPresent()); //$NON-NLS-1$
	}

	@BeforeSuite
	protected void setupSuite() {
		getJdkUtilityPath(JSTAT_COMMAND);
		AssertJUnit.assertTrue("Attach API failed to launch", TargetManager.waitForAttachApiInitialization()); //$NON-NLS-1$
		vmId = TargetManager.getVmId();
	}
}
