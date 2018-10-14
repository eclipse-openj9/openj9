/*******************************************************************************
 * Copyright (c) 2001, 2018 IBM Corp. and others
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
package j9vm.test.ddrext.junit;

import j9vm.test.ddrext.Constants;
import j9vm.test.ddrext.DDRExtTesterBase;

public class TestThread extends DDRExtTesterBase {
	public void testThread() {
		String threadOutput = exec(Constants.THREAD_CMD, new String[0]);
		assertTrue(validate(threadOutput, Constants.THREAD_SUCCESS_KEYS,
				Constants.THREAD_FAILURE_KEY, true));
	}

	/* tests extension !stack thread */
	public void testStack() {
		String threadOutput = exec(Constants.THREAD_CMD, new String[0]);
		if (threadOutput == null) {
			fail("threads output is null. Can not proceed with testStack");
			return;
		}
		String address = getAddressForThreads(Constants.STACK_CMD, threadOutput);
		if (address == null) {
			fail("Error parsing Threads output for stack address");
			return;
		}
		String stackOutput = exec(Constants.STACK_CMD,
				new String[] { address });
		assertTrue(validate(stackOutput, Constants.STACK_SUCCESS_KEYS,
				Constants.STACK_FAILURE_KEY, true));
	}

	/*
	 * tests !stackslots extension with all options !stackslots thread
	 * !stackslots thread,sp,a0,pc,literals !stackslots
	 * thread,sp,a0,pc,literals,els
	 */
	public void testStackSlots() {
		String threadOutput = exec(Constants.THREAD_CMD, new String[0]);
		if (threadOutput == null) {
			fail("threads output is null. Can not proceed with testStack");
			return;
		}
		String threadAddress = getAddressForThreads(Constants.STACK_CMD, threadOutput);
		if (threadAddress == null) {
			fail("Error parsing Threads output for stack address");
			return;
		}
		/* test !stackslots thread */
		String stackSlotsOutput = exec(Constants.STACKSLOTS_CMD,
				new String[] { threadAddress });
		assertTrue(validate(stackSlotsOutput, Constants.STACKSLOTS_SUCCESS_KEY,
				Constants.STACKSLOTS_FAILURE_KEY, true));

		/* test !stackslots thread,sp,a0,pc,literals */
		String paramSet1 = getStackSlotsParamSet(stackSlotsOutput,
				threadAddress, false);
		if (paramSet1 == null) {
			fail("Failed to construct parameter list for command : !stackslots thread,sp,a0,pc,literals\nStackSlots Output being used : \n"
					+ stackSlotsOutput);
		} else {
			String stackSlotsOutputParam1 = exec(Constants.STACKSLOTS_CMD,
					new String[] { paramSet1 });
			assertTrue(validate(stackSlotsOutputParam1,
					Constants.STACKSLOTS_SUCCESS_KEY,
					Constants.STACKSLOTS_FAILURE_KEY, true));
		}

		/* test !stackslots thread,sp,a0,pc,literals,els */
		String paramSet2 = getStackSlotsParamSet(stackSlotsOutput,
				threadAddress, true);
		if (paramSet2 == null) {
			fail("Failed to construct parameter list for command : !stackslots thread,sp,a0,pc,literals,els\nStackSlots Output being used : \n"
					+ stackSlotsOutput);
		} else {
			String stackSlotsOutputParam2 = exec(Constants.STACKSLOTS_CMD,
					new String[] { paramSet2 });
			assertTrue(validate(stackSlotsOutputParam2,
					Constants.STACKSLOTS_SUCCESS_KEY,
					Constants.STACKSLOTS_FAILURE_KEY, true));
		}
	}

	public void testJ9VMThread() {
		String threadOutput = exec(Constants.THREAD_CMD, new String[0]);
		if (threadOutput == null) {
			fail("threads output is null. Can not proceed with testJ9VMThread");
			return;
		}
		String address = getAddressForThreads(Constants.J9VMTHREAD_CMD, threadOutput);
		if (address == null) {
			fail("Error parsing Threads output for j9vmthread address");
			return;
		}
		String j9vmthreadOutput = exec(Constants.J9VMTHREAD_CMD,
				new String[] { address });
		assertTrue(validate(j9vmthreadOutput,
				Constants.J9VMTHREAD_SUCCESS_KEYS,
				Constants.J9VMTHREAD_FAILURE_KEY, true));
	}

	public void testJ9Thread() {
		String threadOutput = exec(Constants.THREAD_CMD, new String[0]);
		if (threadOutput == null) {
			fail("threads output is null. Can not proceed with testJ9Thread");
			return;
		}
		String address = getAddressForThreads(Constants.J9THREAD_CMD, threadOutput);
		if (address == null) {
			fail("Error parsing Threads output for j9thread address");
			return;
		}
		String j9threadOutput = exec(Constants.J9THREAD_CMD,
				new String[] { address });
		assertTrue(validate(j9threadOutput, Constants.J9THREAD_SUCCESS_KEYS,
				Constants.J9THREAD_FAILURE_KEY, true));
	}

	/* Tests all !thread options */
	public void testThreadOptions() {
		String threadHelpOutput = exec(Constants.THREAD_CMD,
				new String[] { "help" });
		assertTrue(validate(threadHelpOutput,
				Constants.THREAD_HELP_SUCCESS_KEY, null, true));

		String threadStackOutput = exec(Constants.THREAD_CMD,
				new String[] { "stack" });
		assertTrue(validate(threadStackOutput,
				Constants.THREAD_STACK_SUCCESS_KEY, null, true));

		String threadStackSlotsOutput = exec(Constants.THREAD_CMD,
				new String[] { "stackslots" });
		assertTrue(validate(threadStackSlotsOutput,
				Constants.THREAD_STACK_SUCCESS_KEY, null, true));

		String threadFlagsOutput = exec(Constants.THREAD_CMD,
				new String[] { "flags" });
		assertTrue(validate(threadFlagsOutput,
				Constants.THREAD_FLAGS_SUCCESS_KEY, null, true));

		String threadDebugEventDataOutput = exec(Constants.THREAD_CMD,
				new String[] { "debugEventData" });
		assertTrue(validate(threadDebugEventDataOutput,
				Constants.THREAD_DEBUGEVENTDATA_SUCCESS_KEYS, null, true));

		String threadMonitorsOutput = exec(Constants.THREAD_CMD,
				new String[] { "monitors" });
		assertTrue(validate(threadMonitorsOutput,
				Constants.THREAD_MONITORS_SUCCESS_KEY, null, true));

		String threadTraceOutput = exec(Constants.THREAD_CMD,
				new String[] { "trace" });
		assertTrue(validate(threadTraceOutput,
				Constants.THREAD_TRACE_SUCCESS_KEY, null, true));

		String threadOutput = exec(Constants.THREAD_CMD, new String[0]);
		String tid = getAddressForThreads("tid", threadOutput);
		String threadSearchOutput = exec(Constants.THREAD_CMD, new String[] {
				"search", tid });
		assertTrue(validate(threadSearchOutput,
				Constants.THREAD_SEARCH_SUCCESS_KEY, null, true));
	}

	public void testLocalmap() {
		String threadOutput = exec(Constants.THREAD_CMD, new String[0]);
		if (threadOutput == null) {
			fail("threads output is null. Can not proceed with testLocalmap");
			return;
		}
		String threadAddress = getAddressForThreads(Constants.STACK_CMD, threadOutput);
		String stackSlotsOutput = exec(Constants.STACKSLOTS_CMD,
				new String[] { threadAddress });
		if (stackSlotsOutput == null) {
			fail("stackslots output is null. Can not proceed with testLocalmap");
			return;
		}
		String results = getStackSlotsValues(stackSlotsOutput);
		if (results == null) {
			fail("Not able to get pc and method values from stackslots output. Can not proceed with testLocalmap");
			return;
		}
		String tokens[] = results.split(",");
		String localmapOutput = exec(Constants.LOCALMAP_CMD,
				new String[] { tokens[0] });
		String successKeys = Constants.LOCALMAP_SUCCESS_KEY
				+ ","
				+ tokens[1].replace(".", "\\.").replace("(", "\\(")
						.replace(")", "\\)").replace("$", "\\$");
		assertTrue(validate(localmapOutput, successKeys,
				Constants.LOCALMAP_FAILURE_KEY, true));
	}

	private String getStackSlotsValues(String stackslotOutput) {
		String results = null;
		String output[] = stackslotOutput.split(Constants.NL);
		boolean found = false;
		for (String aLine : output) {
			if (aLine.contains("Bytecode frame")) {
				String tokens[] = aLine.split(",");
				for (int i = 0; i < tokens.length; i++) {
					String token = tokens[i].trim();
					if (token.contains("pc")) {
						results = token.split("=")[1].trim();
						found = true;
						break;
					}
				}
			}
			if (found && aLine.contains("Method:")) {
				String tokens[] = aLine.split(" ");
				results = results + "," + tokens[2].trim();
				break;
			}
		}
		String tokens[] = results.split(",");
		if (tokens.length < 2 || tokens[0].isEmpty() || tokens[1].isEmpty()) {
			return null;
		}
		return results;
	}

	/*
	 * If needELS, we return the second param list for stackslots:
	 * thread,sp,a0,pc,literals,elsElse,we return the first param list for
	 * stackslots : thread,sp,a0,pc,literals
	 */
	private String getStackSlotsParamSet(String stackOutput,
			String threadAddress, boolean needELS) {
		String output[] = stackOutput.split(Constants.NL);
		String paramList = threadAddress;
		for (String aLine : output) {
			if (aLine.contains("Initial values")) {
				String sp = null, pc = null, literals = null, a0 = null, els = null;
				String tokens[] = aLine.split(",");
				for (int i = 0; i < tokens.length; i++) {
					String token = tokens[i].trim();
					if (token.contains("walkSP")) {
						sp = tokens[i].split("=")[1].trim();
					} else if (token.startsWith("PC")) {
						pc = token.split("=")[1].trim();
					} else if (token.startsWith("literals")) {
						literals = token.split("=")[1].trim();
					} else if (token.startsWith("A0")) {
						a0 = token.split("=")[1].trim();
					} else if (token.startsWith("ELS")) {
						els = token.split("=")[1].trim();
					}
				}
				if (needELS) {
					if (sp == null || a0 == null || pc == null
							|| literals == null || els == null) {
						return null;
					}
					return paramList + "," + sp + "," + a0 + "," + pc + ","
							+ literals + "," + els;
				} else {
					if (sp == null || a0 == null || pc == null
							|| literals == null) {
						return null;
					}
					return paramList + "," + sp + "," + a0 + "," + pc + ","
							+ literals;
				}
			}
		}
		return null;
	}
}
