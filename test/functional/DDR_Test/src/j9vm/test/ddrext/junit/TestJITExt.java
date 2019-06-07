/*******************************************************************************
 * Copyright (c) 2001, 2019 IBM Corp. and others
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

import org.testng.log4testng.Logger;

import j9vm.test.ddrext.Constants;
import j9vm.test.ddrext.DDRExtTesterBase;

public class TestJITExt extends DDRExtTesterBase {
	private Logger log = Logger.getLogger(TestJITExt.class);
	
	/*
	 * Tests the following extensions:
	 *		jitstack <thread>,<sp>,<pc>			Dump JIT stack
	 *		jitstackslots <thread>,<sp>,<pc>	Dump JIT stack slots
	 *		jitmetadatafrompc <pc>				Show JIT method metadata for PC
	 */
	public void testJITStack() {
		String stackAddress = null;
		String j9vmthreadAddress = null;
		String stackslotsOutput = null;
		
		String threadsOutput = exec(Constants.THREAD_CMD, new String[0]);
		if (threadsOutput == null) {
			fail("Setup failed. Threads output is null. Can not proceed with JIT extension tests");
			return;
		}
		
		// Find a thread that has a JIT frame. To do this we run the !stackslots command on each thread.
		String[] lines = threadsOutput.split(Constants.NL);
		for (int i = 0; i < lines.length; i++) {
			String[] tokens = lines[i].split("\t");
			for (int j = 0; j < tokens.length; j++) {
				if (tokens[j].contains("stack")) {
					stackAddress = tokens[j].trim().split(" ")[1];
				}
				if (tokens[j].contains("j9vmthread")) {
					j9vmthreadAddress = tokens[j].trim().split(" ")[1];
				}
			}
			if (stackAddress != null && j9vmthreadAddress != null) {
				// Got a stack address and a j9vmthread address, now run !stackslots
				stackslotsOutput = exec(Constants.STACKSLOTS_CMD, new String[] {stackAddress});
				if (stackslotsOutput.contains("TestJITExtHelperThread")) {
					if (stackslotsOutput.contains("JIT frame")) {
						break; // Found a thread stack containing a JIT frame, we are done
					} else {
						log.info("JIT frame was not found: test may be run in a mode that prevents jitting");
						return;
					}
				}
			}
		}

		if (stackslotsOutput == null) {
			fail("TestJITExt failed. Unable to find a threads contained a JIT frame " 
				+ " and using the TestJITExtHelperThread class. Cannot proceed with JIT extension tests");
			return;
		}
		
		String sp = getSP(stackslotsOutput);
		if (sp == null) {
			log.error("Missing JIT frame or unwindSP in !stackslots output:\n" + stackslotsOutput);
			fail("Error parsing stackslots output for sp value");
			return;
		}

		String pc = getPC(stackslotsOutput);
		if (pc == null) {
			fail("Error parsing stackslots output for pc value");
			return;
		}

		String args = j9vmthreadAddress + "," + sp + "," + pc;

		String jitstackOutput = exec(Constants.JITSTACK_CMD,
				new String[] { args });
		String jitstacklotsOutput = exec(Constants.JITSTACKSLOTS_CMD,
				new String[] { args });
		String jitmetadatafrompcOutput = exec(Constants.JITMETADATAFROMPC,
				new String[] { pc });
		assertTrue(validate(jitstackOutput, Constants.JIT_CMD_SUCCESS_KEY,
				Constants.JIT_CMD_FAILURE_KEY, true));
		assertTrue(validate(jitstacklotsOutput, Constants.JIT_CMD_SUCCESS_KEY,
				Constants.JIT_CMD_FAILURE_KEY, true));
		assertTrue(validate(jitmetadatafrompcOutput,
				Constants.JITMETADATAFROMPC_SUCCESS_KEY,
				Constants.JITMETADATAFROMPC_FAILURE_KEY, true));
	}

	private String getSP(String stackslotsOutput) {
		String[] lines = stackslotsOutput.split(Constants.NL);
		for (int i = 0; i < lines.length; i++) {
			if (lines[i].contains("JIT frame")) {
				String[] tokens = lines[i].split(",");
				for (int j = 0; j < tokens.length; j++) {
					if (tokens[j].contains("unwindSP")) {
						return tokens[j].split("=")[1].trim();
					}
				}
			}
		}
		return null;
	}

	private String getPC(String stackslotsOutput) {
		String[] lines = stackslotsOutput.split(Constants.NL);
		for (int i = 0; i < lines.length; i++) {
			if (lines[i].contains("JIT frame")) {
				String[] tokens = lines[i].split(",");
				for (int j = 0; j < tokens.length; j++) {
					if (tokens[j].contains("pc")) {
						return tokens[j].split("=")[1].trim();
					}
				}
			}
		}
		return null;
	}
}
