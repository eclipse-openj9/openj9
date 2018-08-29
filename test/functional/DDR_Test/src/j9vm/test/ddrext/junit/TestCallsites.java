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

import org.testng.log4testng.Logger;

public class TestCallsites extends DDRExtTesterBase {
	private Logger log = Logger.getLogger(TestCallsites.class);

	public void testFindallcallsites() {
		String findAllCallsitesOutput = exec(Constants.FINDALLCALLSITES_CMD,
				new String[0]);
		assertTrue(validate(findAllCallsitesOutput,
				Constants.FINDALLCALLSITES_SUCCESS_KEYS,
				Constants.FINDALLCALLSITES_FAILURE_KEY, true));
	}

	public void testPrintallcallsites() {
		String printAllCallsitesOutput = exec(Constants.PRINTALLCALLSITES_CMD,
				new String[0]);
		assertTrue(validate(printAllCallsitesOutput,
				Constants.PRINTALLCALLSITES_SUCCESS_KEYS,
				Constants.PRINTALLCALLSITES_FAILURE_KEY, true));
	}

	public void testFindfreedcallsites() {
		String findFreedCallSitesOutput = exec(
				Constants.FINDFREEDCALLSITES_CMD, new String[0]);
		assertTrue(validate(findFreedCallSitesOutput,
				Constants.FINDFREEDCALLSITES_SUCCESS_KEYS,
				Constants.FINDFREEDCALLSITES_FAILURE_KEY, true));
	}

	public void testPrintfreedcallsites() {
		String printFreedCallsitesOutput = exec(
				Constants.PRINTFREEDCALLSITES_CMD, new String[0]);
		assertTrue(validate(printFreedCallsitesOutput,
				Constants.PRINTFREEDCALLSITES_SUCCESS_KEYS,
				Constants.PRINTFREEDCALLSITES_FAILURE_KEY, true));
	}

	public void testFindcallsite() {
		String findAllCallsitesOutput = exec(Constants.FINDALLCALLSITES_CMD,
				new String[0]);
		if (findAllCallsitesOutput == null) {
			fail("findallcallsites output is null. Can not proceed with testFindcallsite");
			return;
		}
		String callsite = getCallsite(Constants.FINDALLCALLSITES_SUCCESS_KEYS,
				findAllCallsitesOutput);
		if (callsite == null) {
			fail("Error parsing findallcallsites output for "
					+ Constants.FINDALLCALLSITES_SUCCESS_KEYS);
			return;
		}

		String findCallsiteOutput = exec(Constants.FINDCALLSITE_CMD,
				new String[] { callsite });
		assertTrue(validate(findCallsiteOutput,
				Constants.FINDCALLSITE_SUCCESS_KEYS,
				Constants.FINDCALLSITE_FAILURE_KEY, true));
	}

	public void testFindfreedcallsite() {
		String findFreedCallsitesOutput = exec(
				Constants.FINDFREEDCALLSITES_CMD, new String[0]);
		if (findFreedCallsitesOutput == null) {
			fail("findfreedcallsites output is null. Can not proceed with testFindfreedcallsite");
			return;
		}

		// check whether the output from findfreedcallsites contains .c or .cpp.
		// If not, warning message will display
		// and the findfreedcallsite test will not be executed due to
		// insufficient information.
		String freedCallsite = getCallsite(".c", findFreedCallsitesOutput);
		if (freedCallsite == null) {
			log.warn("Not able to find findfreedcallsites that end with .c");
			freedCallsite = getCallsite(".cpp", findFreedCallsitesOutput);
			if (freedCallsite == null) {
				log.warn("Not able to find findfreedcallsites that end with .cpp");
				log.warn("Not able to test findfreedcallsite due to insufficient information");
				return;
			}
		}
		String findFreedCallSiteOutput = exec(Constants.FINDFREEDCALLSITE_CMD,
				new String[] { freedCallsite });
		assertTrue(validate(findFreedCallSiteOutput,
				Constants.FINDFREEDCALLSITE_SUCCESS_KEYS,
				Constants.FINDFREEDCALLSITE_FAILURE_KEY, true));
	}

	private String getCallsite(String value, String output) {
		String[] lines = output.split(Constants.NL);
		for (int i = 0; i < lines.length; i++) {
			if (lines[i].contains(value)) {
				String[] tokens = lines[i].split(" ");
				for (int j = 0; j < tokens.length; j++) {
					if (tokens[j].contains(value)) {
						return tokens[j].trim();
					}
				}
			}
		}
		return null;
	}
}
