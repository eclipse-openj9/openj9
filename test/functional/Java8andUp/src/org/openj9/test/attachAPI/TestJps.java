/*******************************************************************************
 * Copyright (c) 2019, 2019 IBM Corp. and others
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

import static org.openj9.test.attachAPI.TestConstants.TARGET_VM_CLASS;
import static org.testng.AssertJUnit.assertTrue;
import static org.testng.AssertJUnit.assertEquals;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.List;
import java.util.Collections;
import java.util.Optional;

import org.openj9.test.util.PlatformInfo;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.BeforeSuite;
import org.testng.annotations.Test;

@Test(groups = { "level.extended" })
public class TestJps extends AttachApiTest {

	private static final String MISSING_ARGUMENT = "Missing argument: "; //$NON-NLS-1$
	private static final String CHILD_IS_MISSING = "child is missing"; //$NON-NLS-1$
	private static final String TEST_PROCESS_ID_MISSING = "Test process ID missing"; //$NON-NLS-1$
	private static final String CHILD_PROCESS_DID_NOT_LAUNCH = "Child process did not launch"; //$NON-NLS-1$
	private static final String JPS_COMMAND = "jps"; //$NON-NLS-1$
	private static final String JPS_Class = "Jps"; //$NON-NLS-1$
	private String vmId;
	
	@Test(groups = { "level.extended" })
	public void testJpsSanity() throws IOException {
		TargetManager tgtMgr = new TargetManager(TARGET_VM_CLASS, null);
		assertTrue(CHILD_PROCESS_DID_NOT_LAUNCH, tgtMgr.syncWithTarget());
		List<String> jpsOutput = runCommand();
		assertTrue(TEST_PROCESS_ID_MISSING, search(vmId, jpsOutput).isPresent());
		assertTrue("jps is missing", search(JPS_Class, jpsOutput).isPresent()); //$NON-NLS-1$
		assertTrue(CHILD_IS_MISSING, search(tgtMgr.targetId, jpsOutput).isPresent());
		tgtMgr.terminateTarget();
	}

	@Test(groups = { "level.extended" })
	public void testJpsPackageName() throws IOException {
		final String WRONG_PKG_NAME = "Wrong package name: "; //$NON-NLS-1$
		TargetManager tgtMgr = new TargetManager(TARGET_VM_CLASS, null);
		assertTrue(CHILD_PROCESS_DID_NOT_LAUNCH, tgtMgr.syncWithTarget());
		List<String> jpsOutput = runCommand(Collections.singletonList("-l")); //$NON-NLS-1$
		Optional<String> searchResult = search(tgtMgr.targetId, jpsOutput);
		assertTrue(CHILD_IS_MISSING, searchResult.isPresent());
		assertTrue(WRONG_PKG_NAME + searchResult.get(), searchResult.get().contains(TargetVM.class.getPackage().getName()));
		tgtMgr.terminateTarget();
	}

	@Test(groups = { "level.extended" })
	public void testJpsIdOnly() throws IOException {
		final String SPURIOUS_OUTPUT = "Spurious output: "; //$NON-NLS-1$
		TargetManager tgtMgr1 = new TargetManager(TARGET_VM_CLASS, null);
		TargetManager tgtMgr2 = new TargetManager(TARGET_VM_CLASS, "SomeRandomId"); //$NON-NLS-1$
		assertTrue(CHILD_PROCESS_DID_NOT_LAUNCH, tgtMgr1.syncWithTarget());
		assertTrue(CHILD_PROCESS_DID_NOT_LAUNCH, tgtMgr2.syncWithTarget());
		List<String> jpsOutput = runCommand(Collections.singletonList("-q")); //$NON-NLS-1$
		
		Optional<String> searchResult = search(vmId, jpsOutput);
		assertTrue(TEST_PROCESS_ID_MISSING, searchResult.isPresent());
		assertEquals(SPURIOUS_OUTPUT, vmId, searchResult.get());
		
		searchResult = search(tgtMgr1.targetId, jpsOutput);
		assertTrue(CHILD_IS_MISSING, search(tgtMgr1.targetId, jpsOutput).isPresent());
		assertEquals(SPURIOUS_OUTPUT, tgtMgr1.targetId, searchResult.get());
		
		/* now try with non-default vm ID */
		searchResult = search(tgtMgr2.targetId, jpsOutput);
		assertTrue(CHILD_IS_MISSING, search(tgtMgr2.targetId, jpsOutput).isPresent());
		assertEquals(SPURIOUS_OUTPUT, tgtMgr2.targetId, searchResult.get());
		
		tgtMgr1.terminateTarget();
		tgtMgr2.terminateTarget();
	}

	@Test(groups = { "level.extended" })
	public void testApplicationArguments() throws IOException {
		List<String> targetArgs = Arrays.asList("foo", "bar");  //$NON-NLS-1$//$NON-NLS-2$
		TargetManager tgtMgr = new TargetManager(TARGET_VM_CLASS, null, targetArgs);
		assertTrue(CHILD_PROCESS_DID_NOT_LAUNCH, tgtMgr.syncWithTarget());
		List<String> jpsOutput = runCommand(Collections.singletonList("-m")); //$NON-NLS-1$
		Optional<String> searchResult =  search(tgtMgr.targetId, jpsOutput);
		assertTrue(CHILD_IS_MISSING, search(tgtMgr.targetId, jpsOutput).isPresent());
		for (String a: targetArgs) {
			assertTrue(MISSING_ARGUMENT + a, searchResult.get().contains(a));
		}
		tgtMgr.terminateTarget();
	}

	@Test(groups = { "level.extended" })
	public void testJvmArguments() throws IOException {
		List<String> vmArgs = Collections.singletonList("-Dfoo=bar"); //$NON-NLS-1$
		TargetManager tgtMgr = new TargetManager(TARGET_VM_CLASS, null, null, vmArgs, null);
		assertTrue(CHILD_PROCESS_DID_NOT_LAUNCH, tgtMgr.syncWithTarget());
		List<String> jpsOutput = runCommand(Collections.singletonList("-v")); //$NON-NLS-1$
		Optional<String> searchResult =  search(tgtMgr.targetId, jpsOutput);
		assertTrue(CHILD_IS_MISSING, search(tgtMgr.targetId, jpsOutput).isPresent());
		for (String a: vmArgs) {
			assertTrue(MISSING_ARGUMENT + a, searchResult.get().contains(a));
		}
		tgtMgr.terminateTarget();
	}

	/**
	 * Look for a string in a list of strings using case sensitive substring searching
	 * @param needle string for which to search
	 * @param haystack list of strings in which to search
	 * @return first string in haystack containing needle
	 */
	static Optional<String> search(String needle, List<String> haystack) {
		return haystack.stream().filter(s -> s.contains(needle)).findFirst();
	}

	@BeforeMethod
	protected void setUp(Method testMethod) {
		testName = testMethod.getName();
		log("starting " + testName);		 //$NON-NLS-1$
	}
	
	/**
	 * Don't run the test if it is running on non-OpenJ9 (e.g. IBM) Java
	 */
	@BeforeSuite
	void setupSuite() {
		assertTrue("This test is valid only on OpenJ9",  //$NON-NLS-1$
				System.getProperty("java.vm.vendor").contains("OpenJ9"));  //$NON-NLS-1$//$NON-NLS-2$
		char fs = java.io.File.separatorChar;
		final String jpsCommandWithSuffix = 
				PlatformInfo.isWindows() ? JPS_COMMAND + ".exe": JPS_COMMAND; //$NON-NLS-1$
		String binDir = System.getProperty("java.home") + fs + "bin" + fs; //$NON-NLS-1$ //$NON-NLS-2$
		commandName = binDir + jpsCommandWithSuffix;
		File  commandFile = new File(commandName);
		if (!commandFile.exists()) { /* Java 8 has 2 bin directories, and JAVA_HOME is probably pointing at jre/bin */
			log("Did not find jps in "+binDir); //$NON-NLS-1$
			binDir = System.getProperty("java.home") + fs + ".." + fs + "bin" + fs; //$NON-NLS-1$ //$NON-NLS-2$ //$NON-NLS-3$
			commandName = binDir + jpsCommandWithSuffix;
			commandFile = new File(commandName);
			assertTrue("Did not find " + jpsCommandWithSuffix //$NON-NLS-1$
					+ " in " + binDir, commandFile.exists()); //$NON-NLS-1$
		}
		assertTrue("Attach API failed to launch", TargetManager.waitForAttachApiInitialization()); //$NON-NLS-1$
		vmId = TargetManager.getVmId();
	}
	
}
