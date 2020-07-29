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

import static org.testng.Assert.assertEquals;
import static org.testng.Assert.assertNotNull;
import static org.testng.Assert.assertTrue;
import static org.testng.Assert.fail;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;

import org.openj9.test.util.PlatformInfo;
import org.openj9.test.util.StringUtilities;
import org.testng.annotations.BeforeMethod;
import org.testng.annotations.BeforeSuite;
import org.testng.annotations.Test;

import static org.openj9.test.attachAPI.TargetManager.getVmId;

@SuppressWarnings("nls")
@Test(groups = { "level.extended" })
public class TestJcmd extends AttachApiTest {

	private static final String EXPECTED_STRING_FOUND = "Expected string found";
	private static final String JCMD_OUTPUT_START_STRING = "Dump written to ";
	private static final String TARGET_DUMP_WRITTEN_STRING = " dump written to ";
	private static final String ERROR_EXPECTED_STRING_NOT_FOUND = "Expected string not found";
	private static final String ERROR_TARGET_NOT_LAUNCH = "target did not launch";

	private static final String JCMD_COMMAND = "jcmd";

	private static final String DUMP_HEAP = "Dump.heap";
	private static final String DUMP_JAVA = "Dump.java";
	private static final String DUMP_SNAP = "Dump.snap";
	private static final String DUMP_SYSTEM = "Dump.system";
	private static final String GC_CLASS_HISTOGRAM = "GC.class_histogram";
	private static final String GC_HEAP_DUMP = "GC.heap_dump";
	private static final String GC_RUN = "GC.run";
	private static final String HELP_COMMAND = "help";
	private static final String THREAD_PRINT = "Thread.print";
	private static String[] JCMD_COMMANDS = {DUMP_HEAP, DUMP_JAVA, DUMP_SNAP,
		DUMP_SYSTEM, GC_CLASS_HISTOGRAM, GC_HEAP_DUMP, GC_RUN, HELP_COMMAND, THREAD_PRINT};
	private static String[] JCMD_COMMANDS_REQUIRE_OPTION = {GC_CLASS_HISTOGRAM, GC_RUN, HELP_COMMAND, THREAD_PRINT};
	private static String[] JCMD_COMMANDS_DUMP = {DUMP_HEAP, DUMP_JAVA, DUMP_SNAP, DUMP_SYSTEM, GC_HEAP_DUMP};

	/*
	 * Contains strings expected to be contained in the outputs of various commands
	 */
	private static Map<String, String> commandExpectedOutputs;
	private File userDir;

	/**
	 * Test various ways of printing the help text, including undocumented but
	 * plausible variants
	 * 
	 * @throws IOException on error
	 */
	@Test
	public void testJcmdHelps() throws IOException {
		String[] HELP_OPTIONS = { "-h", HELP_COMMAND, "-help", "--help" };
		for (String helpOption : HELP_OPTIONS) {
			List<String> jcmdOutput = runCommandAndLogOutput(Collections.singletonList(helpOption));
			/* Sample of the text from Jcmd.HELPTEXT */
			String expectedString = "list JVM processes on the local machine.";
			Optional<String> searchResult = StringUtilities.searchSubstring(expectedString, jcmdOutput);
			assertTrue(searchResult.isPresent(), "Help text corrupt: " + jcmdOutput);
		}
	}

	/**
	 * Test help for various commands
	 * @throws IOException on error
	 */
	@Test
	public void testCommandHelps() throws IOException {
		for (String command : JCMD_COMMANDS) {
			List<String> args = new ArrayList<>();
			args.add(getVmId());
			args.add(HELP_COMMAND);
			args.add(command);
			List<String> jcmdOutput = runCommandAndLogOutput(args);
			String expectedString = command + ":";
			log("Expected string: " + expectedString);
			Optional<String> searchResult = StringUtilities.searchSubstring(expectedString, jcmdOutput);
			assertTrue(searchResult.isPresent(), "Help text corrupt: " + jcmdOutput);
			log(EXPECTED_STRING_FOUND);
		}
	}

	@Test
	public void testListVms() throws IOException {
		String myId = getVmId();
		List<String> args = new ArrayList<>();
		List<String> jcmdOutput = runCommandAndLogOutput(args);
		Optional<String> searchResult = StringUtilities.searchSubstring(myId, jcmdOutput);
		String errorMessage = "My VMID missing from VM list";
		assertTrue(searchResult.isPresent(), errorMessage);
		args.add("-l");
		jcmdOutput = runCommandAndLogOutput(args);
		searchResult = StringUtilities.searchSubstring(myId, jcmdOutput);
		assertTrue(searchResult.isPresent(), errorMessage + jcmdOutput);
		log(EXPECTED_STRING_FOUND);
	}

	@Test
	public void testCommandWrongArgumentNumber() throws IOException {
		for (String command : JCMD_COMMANDS_DUMP) {
			List<String> args = new ArrayList<>();
			args.add(getVmId());
			args.add(command);
			args.add(new File(userDir, "my" + command + "Dump").getAbsolutePath());
			args.add("wrongargument");
			List<String> jcmdOutput = runCommandAndLogOutput(args);
			String expectedString = commandExpectedOutputs.getOrDefault(command, "Test error: expected output not defined");
			log("Expected string: " + expectedString);
			Optional<String> searchResult = StringUtilities.searchSubstring(expectedString, jcmdOutput);
			assertTrue(searchResult.isPresent(), "Expected string \"" + expectedString + "\" not found");
			log(EXPECTED_STRING_FOUND);
		}
	}
	
	@Test
	public void testNoCommandOptions() throws IOException {
		for (String command : JCMD_COMMANDS_REQUIRE_OPTION) {
			List<String> args = new ArrayList<>();
			args.add(getVmId());
			args.add(command);
			List<String> jcmdOutput = runCommandAndLogOutput(args);
			String expectedString = commandExpectedOutputs.getOrDefault(command, "Test error: expected output not defined");
			log("Expected string: " + expectedString);
			Optional<String> searchResult = StringUtilities.searchSubstring(expectedString, jcmdOutput);
			assertTrue(searchResult.isPresent(), "Expected string \"" + expectedString + "\" not found");
			log(EXPECTED_STRING_FOUND);
		}
	}

	@Test
	public void testThreadPrint() throws IOException {
		String LockedSyncsOption = "-l";
		String[] options = {"", LockedSyncsOption, LockedSyncsOption + "=true", LockedSyncsOption + "=false"};
		for (String option : options) {
			List<String> args = new ArrayList<>();
			args.add(getVmId());
			args.add(THREAD_PRINT);
			if (!option.isEmpty()) {
				args.add(option);
			}
			boolean addSynchronizers = !option.endsWith("false") && !option.isEmpty();
			List<String> jcmdOutput = runCommandAndLogOutput(args);
			String expectedString = "Locked ownable synchronizers";
			log("Expected string: " + expectedString);
			Optional<String> searchResult = StringUtilities.searchSubstring(expectedString, jcmdOutput);
			assertEquals(searchResult.isPresent(), addSynchronizers, "Output contains locked synchronizer information: " + expectedString);
			log(EXPECTED_STRING_FOUND);
		}
	}

	@Test
	public void testClassHistogramAll() throws IOException {
		List<String> args = new ArrayList<>();
		args.add(getVmId());
		args.add(GC_CLASS_HISTOGRAM);
		args.add("all");
		List<String> jcmdOutput = runCommandAndLogOutput(args);
		String expectedString = commandExpectedOutputs.getOrDefault(GC_CLASS_HISTOGRAM, "Test error: expected output not defined");
		log("Expected string: " + expectedString);
		Optional<String> searchResult = StringUtilities.searchSubstring(expectedString, jcmdOutput);
		assertTrue(searchResult.isPresent(), "Expected string not found: " + expectedString);
		log(EXPECTED_STRING_FOUND);
	}

	@Test
	public void testDumps() throws IOException {
		String[][] commandsAndDumpTypes = {{DUMP_HEAP, "Heap"}, {GC_HEAP_DUMP, "Heap"}, {DUMP_JAVA, "Java"}, {DUMP_SNAP, "Snap"}, {DUMP_SYSTEM, "System"}};
		for (String[] commandAndDumpName : commandsAndDumpTypes) {
			TargetManager tgt = new TargetManager(TestConstants.TARGET_VM_CLASS, null,
					Collections.singletonList("-Xmx10M"), Collections.emptyList());
			tgt.syncWithTarget();
			String targetId = tgt.targetId;
			assertNotNull(targetId, ERROR_TARGET_NOT_LAUNCH);
			String dumpType = commandAndDumpName[1];
			File dumpFileLocation = new File(userDir, "my" + dumpType + "Dump");
			dumpFileLocation.delete();
			try {
				String dumpFilePath = dumpFileLocation.getAbsolutePath();
				List<String> args = new ArrayList<>();
				args.add(targetId);
				String command = commandAndDumpName[0];
				log("test " + command);
				args.add(command);
				args.add(dumpFilePath);
				List<String> jcmdOutput = runCommandAndLogOutput(args);

				assertTrue(dumpFileLocation.exists(), "dump file " + dumpFilePath + "missing");
				String expectedString = JCMD_OUTPUT_START_STRING + dumpFilePath;
				log("Expected jcmd output string: " + expectedString);
				Optional<String> searchResult = StringUtilities.searchSubstring(expectedString, jcmdOutput);
				assertTrue(searchResult.isPresent(), ERROR_EXPECTED_STRING_NOT_FOUND + " in jcmd output: " + expectedString);
				log(EXPECTED_STRING_FOUND + " in jcmd output: " + expectedString);

				expectedString = dumpType + TARGET_DUMP_WRITTEN_STRING + dumpFilePath;
				searchResult = StringUtilities.searchSubstring(expectedString,tgt.getTargetErrReader().lines());
				assertTrue(searchResult.isPresent(), ERROR_EXPECTED_STRING_NOT_FOUND + " in target standard error: " + expectedString);
				log(EXPECTED_STRING_FOUND + " in target standard error: " + expectedString);
			} finally {
				tgt.terminateTarget();
				dumpFileLocation.delete();
			}
		}
	}

	static void cleanupFile(String output) {
		if (output.indexOf(JCMD_OUTPUT_START_STRING) != -1) {
			String filePathName = output.substring(JCMD_OUTPUT_START_STRING.length()).trim();
			if (new File(filePathName).delete()) {
				System.out.println("Deleted the temparary dump file : " + filePathName);
			} else {
				fail("Failed to delete the temparary dump file : " + filePathName);
			}
		}
	}

	@Test
	public void testDumpsDefaultSettings() throws IOException {
		String[][] commandsAndDefaultDumpNames = {{DUMP_HEAP, "heapdump"}, {GC_HEAP_DUMP, "heapdump"}, {DUMP_JAVA, "javacore"}, {DUMP_SNAP, "Snap"}, {DUMP_SYSTEM, "core"}};
		for (String[] commandAndDumpName : commandsAndDefaultDumpNames) {
			TargetManager tgt = new TargetManager(TestConstants.TARGET_VM_CLASS, null,
				Collections.singletonList("-Xmx10M"), Collections.emptyList());
			tgt.syncWithTarget();
			String targetId = tgt.targetId;
			assertNotNull(targetId, ERROR_TARGET_NOT_LAUNCH);
			String defaultDumpName = commandAndDumpName[1];
			List<String> jcmdOutput = null;
			try {
				List<String> args = new ArrayList<>();
				args.add(targetId);
				String command = commandAndDumpName[0];
				log("test " + command);
				args.add(command);
				jcmdOutput = runCommandAndLogOutput(args);
				
				String expectedDefaultDumpName = String.format("%1$s.%2$tY%2$tm%2$td", defaultDumpName, Calendar.getInstance());
				Optional<String> searchResult = StringUtilities.searchTwoSubstrings(JCMD_OUTPUT_START_STRING, expectedDefaultDumpName, jcmdOutput);
				assertTrue(searchResult.isPresent(), ERROR_EXPECTED_STRING_NOT_FOUND + " in jcmd output: " + JCMD_OUTPUT_START_STRING + " AND " + expectedDefaultDumpName);
				log(EXPECTED_STRING_FOUND + " in jcmd output: " + JCMD_OUTPUT_START_STRING + " AND " + expectedDefaultDumpName);

				searchResult = StringUtilities.searchTwoSubstrings(TARGET_DUMP_WRITTEN_STRING, expectedDefaultDumpName, tgt.getTargetErrReader().lines());
				assertTrue(searchResult.isPresent(), ERROR_EXPECTED_STRING_NOT_FOUND + " in target standard error: " +
					JCMD_OUTPUT_START_STRING + " AND " + expectedDefaultDumpName);
				log(EXPECTED_STRING_FOUND + " in target standard error: " + JCMD_OUTPUT_START_STRING + " AND " + expectedDefaultDumpName);
			} finally {
				tgt.terminateTarget();
				if (jcmdOutput != null) {
					jcmdOutput.forEach(TestJcmd::cleanupFile);
				}
			}
		}
	}

	@BeforeMethod
	protected void setUp(Method testMethod) {
		testName = testMethod.getName();
		log("------------------------------------\nstarting " + testName);
	}

	@BeforeSuite
	protected void setupSuite() {
		userDir = new File(System.getProperty("user.dir"));
		getJdkUtilityPath(JCMD_COMMAND);
		commandExpectedOutputs = new HashMap<>();
		commandExpectedOutputs.put(HELP_COMMAND, THREAD_PRINT);
		commandExpectedOutputs.put(GC_CLASS_HISTOGRAM, "java.util.HashMap");
		commandExpectedOutputs.put(GC_RUN, "Command succeeded");
		commandExpectedOutputs.put(THREAD_PRINT, "Attach API wait loop");
		/* add the expected outputs for dump commands with no arguments */
		String WRONG_NUMBER_OF_ARGUMENTS = "Error: wrong number of arguments";
		commandExpectedOutputs.put(DUMP_HEAP, WRONG_NUMBER_OF_ARGUMENTS);
		commandExpectedOutputs.put(GC_HEAP_DUMP, WRONG_NUMBER_OF_ARGUMENTS);
		commandExpectedOutputs.put(DUMP_JAVA, WRONG_NUMBER_OF_ARGUMENTS);
		commandExpectedOutputs.put(DUMP_SNAP, WRONG_NUMBER_OF_ARGUMENTS);
		commandExpectedOutputs.put(DUMP_SYSTEM, WRONG_NUMBER_OF_ARGUMENTS);
	}

}
