/*
 * Copyright IBM Corp. and others 2024
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
package org.openj9.criu;

import java.io.IOException;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Optional;
import javax.management.MalformedObjectNameException;
import javax.management.ObjectName;
import jdk.crac.management.CRaCMXBean;

import openj9.internal.criu.InternalCRIUSupport;

import org.openj9.test.attachAPI.AttachApiTest;
import org.openj9.test.attachAPI.TargetManager;
import org.openj9.test.attachAPI.TargetVM;
import org.openj9.test.util.StringUtilities;

import static org.testng.Assert.assertNotNull;
import static org.testng.Assert.assertTrue;

public class TestJDKCRAC extends AttachApiTest {

	private static final String JCMD_COMMAND = "jcmd";
	private static final String ERROR_TARGET_NOT_LAUNCH = "target did not launch";
	private static final String ERROR_EXPECTED_STRING_NOT_FOUND = "Expected string not found";
	private static final String CRAC_CHECKPOINT_DIR = "-XX:CRaCCheckpointTo=./cpData";
	private static final String TARGET_VM_CLASS = TargetVM.class.getCanonicalName();

	public static void main(String[] args) throws Exception {
		if (args.length == 0) {
			throw new RuntimeException("Test name required");
		} else {
			Path imagePath = Paths.get(InternalCRIUSupport.getCRaCCheckpointToDir());
			CRIUTestUtils.deleteCheckpointDirectory(imagePath);
			CRIUTestUtils.createCheckpointDirectory(imagePath);

			String test = args[0];
			TestJDKCRAC testJDKCRaC = new TestJDKCRAC();
			switch (test) {
			case "testJDKCheckpoint":
				testJDKCRaC.testJDKCheckpoint();
				break;
			case "JDK.checkpoint":
				testJDKCRaC.testJcmdCheckpoint(test);
				break;
			case "testCRaCMXBeanGetUptimeSinceRestore":
				testJDKCRaC.testCRaCMXBeanGetUptimeSinceRestore();
				break;
			case "testCRaCMXBeanGetRestoreTime":
				testJDKCRaC.testCRaCMXBeanGetRestoreTime();
				break;
			case "testCRaCMXBeanGetObjectName":
				testJDKCRaC.testCRaCMXBeanGetObjectName();
				break;
			case "testCRaCMXBeanGetObjectNameThrowsInternalError":
				testJDKCRaC.testCRaCMXBeanGetObjectNameThrowsInternalError();
				break;
			default:
				throw new RuntimeException("incorrect test name");
			}
		}
	}

	private void testJDKCheckpoint() throws Exception {
		checkpointRestore();
	}

	private void testJcmdCheckpoint(String command) throws IOException {
		getJdkUtilityPath(JCMD_COMMAND);
		TargetManager tgt = new TargetManager(TARGET_VM_CLASS, null, Collections.singletonList(CRAC_CHECKPOINT_DIR), Collections.emptyList());
		tgt.syncWithTarget();
		String targetId = tgt.targetId;
		assertNotNull(targetId, ERROR_TARGET_NOT_LAUNCH);
		List<String> argsTargetVM = new ArrayList<>();
		argsTargetVM.add(targetId);
		log("test " + command);
		argsTargetVM.add(command);
		List<String> jcmdOutput = runCommandAndLogOutput(argsTargetVM);
		String expectedString = "JVM checkpoint requested";
		log("Expected jcmd output string: " + expectedString);
		Optional<String> searchResult = StringUtilities.searchSubstring(expectedString, jcmdOutput);
		System.out.println("jcmdOutput = " + jcmdOutput);
		assertTrue(searchResult.isPresent(), ERROR_EXPECTED_STRING_NOT_FOUND + " in jcmd output: " + expectedString);
	}

	private void testCRaCMXBeanGetUptimeSinceRestore() throws Exception {
		CRaCMXBean cracMXBean = CRaCMXBean.getCRaCMXBean();
		long uptimeSinceRestore = cracMXBean.getUptimeSinceRestore();
		if (uptimeSinceRestore != -1) {
			System.out.println("FAILED: CRaCMXBean.getUptimeSinceRestore() - " + uptimeSinceRestore
					+ " is not -1 before restore");
		}
		long beforeCheckpointTime = System.currentTimeMillis();
		checkpointRestore();
		uptimeSinceRestore = cracMXBean.getUptimeSinceRestore();
		long afterRestoreTime = System.currentTimeMillis();
		long uptimeSinceRestoreLowerBound = 0;
		long uptimeSinceRestoreUpperBound = afterRestoreTime - beforeCheckpointTime;
		if (uptimeSinceRestoreLowerBound >= uptimeSinceRestore) {
			System.out.println("FAILED: CRaCMXBean.getUptimeSinceRestore() - " + uptimeSinceRestore
					+ " is less than or equal to uptimeSinceRestoreLowerBound - " + uptimeSinceRestoreLowerBound);
		} else if (uptimeSinceRestore >= uptimeSinceRestoreUpperBound) {
			System.out.println("FAILED: CRaCMXBean.getUptimeSinceRestore() - " + uptimeSinceRestore
					+ " is more than or equal to uptimeSinceRestoreUpperBound - " + uptimeSinceRestoreUpperBound);
		} else {
			System.out.println("PASSED: CRaCMXBean.getUptimeSinceRestore() - " + uptimeSinceRestore
					+ " is between uptimeSinceRestoreLowerBound - " + uptimeSinceRestoreLowerBound
					+ " and uptimeSinceRestoreUpperBound - " + uptimeSinceRestoreUpperBound);
		}
	}

	private void testCRaCMXBeanGetRestoreTime() throws Exception {
		CRaCMXBean cracMXBean = CRaCMXBean.getCRaCMXBean();
		long restoreTime = cracMXBean.getRestoreTime();
		if (restoreTime != -1) {
			System.out.println("FAILED: CRaCMXBean.getRestoreTime() - " + restoreTime
					+ " is not -1 before restore");
		}
		long beforeCheckpointTime = System.currentTimeMillis();
		checkpointRestore();
		restoreTime = cracMXBean.getRestoreTime();
		long afterRestoreTime = System.currentTimeMillis();
		if (beforeCheckpointTime >= restoreTime) {
			System.out.println("FAILED: CRaCMXBean.getRestoreTime() - " + restoreTime
					+ " is less than or equal to beforeCheckpointTime - " + beforeCheckpointTime);
		} else if (restoreTime >= afterRestoreTime) {
			System.out.println("FAILED: CRaCMXBean.getRestoreTime() - " + restoreTime
					+ " is more than or equal to afterRestoreTime - " + afterRestoreTime);
		} else {
			System.out.println("PASSED: CRaCMXBean.getRestoreTime() - " + restoreTime
					+ " is between beforeCheckpointTime - " + beforeCheckpointTime
					+ " and afterRestoreTime - " + afterRestoreTime);
		}
	}

	private void testCRaCMXBeanGetObjectName() throws Exception {
		CRaCMXBean cracMXBean = CRaCMXBean.getCRaCMXBean();
		ObjectName objectName = cracMXBean.getObjectName();
		if (objectName == null) {
			System.out.println("FAILED: CRaCMXBean.getObjectName() - " + objectName + " is null");
		}
		String objectNameString = objectName.toString();
		if (!objectNameString.equals(CRaCMXBean.CRAC_MXBEAN_NAME)) {
			System.out.println("FAILED: CRaCMXBean.getObjectName().toString() - " + objectNameString
					+ " is not CRaCMXBean.CRAC_MXBEAN_NAME - " + CRaCMXBean.CRAC_MXBEAN_NAME);
		} else {
			System.out.println("PASSED: CRaCMXBean.getObjectName() - " + objectName + " is valid");
		}
	}

	private void testCRaCMXBeanGetObjectNameThrowsInternalError() throws Exception {
		String malformedCRaCMXBeanName = "InvalidObjectName!@#";
		CRaCMXBean cracMXBean = new TestCRaCMXBeanImpl(malformedCRaCMXBeanName);
		try {
			ObjectName objectName = cracMXBean.getObjectName();
			System.out.println("FAILED: CRaCMXBean.getObjectName() - " + objectName
					+ " did not throw InternalError with malformedCRaCMXBeanName - " + malformedCRaCMXBeanName);
		} catch (InternalError e) {
			System.out.println("PASSED: CRaCMXBean.getObjectName() threw InternalError with malformedCRaCMXBeanName - " + malformedCRaCMXBeanName);
		}
	}

	private static void checkpointRestore() throws Exception {
		CRIUTestUtils.showThreadCurrentTime("Pre-checkpoint - jdk.crac.Core.checkpointRestore()");
		jdk.crac.Core.checkpointRestore();
		CRIUTestUtils.showThreadCurrentTime("Post-checkpoint - jdk.crac.Core.checkpointRestore()");
	}

	private static class TestCRaCMXBeanImpl implements CRaCMXBean {
		private final String malformedCRaCMXBeanName;

		public TestCRaCMXBeanImpl(String malformedCRaCMXBeanName) {
			this.malformedCRaCMXBeanName = malformedCRaCMXBeanName;
		}

		@Override
		public long getUptimeSinceRestore() {
			return -1;
		}

		@Override
		public long getRestoreTime() {
			return -1;
		}

		@Override
		public ObjectName getObjectName() {
			try {
				return ObjectName.getInstance(malformedCRaCMXBeanName);
			} catch (MalformedObjectNameException e) {
				throw new InternalError(e);
			}
		}
	}
}
