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

public class TestMonitors extends DDRExtTesterBase {

	private static final String FailurePatterns = "error,(^|[^_])exception,null,unrecog";

	/**
	 * Test !monitors help
	 */
	public void testMonitorsHelp() {
		String output = exec(Constants.MONITORS_CMD, new String[] { "help" });

		if (null == output) {
			fail("\"!monitors help\" output is null. Can not proceed with test");
			return;
		}

		assertTrue(validate(output,
				"monitors tables,monitors system,monitors object,monitors objects,"
					+ "monitors thread,monitors j9thread,monitors j9vmthread,monitors deadlock,"
					+ "monitors [ owned | waiting | blocked | active | all ]",
				FailurePatterns,
				false));
	}

	/**
	 * Test !monitors all
	 */
	public void testMonitorsAll() {
		String output = exec(Constants.MONITORS_CMD, new String[] { "all" });

		if (null == output) {
			fail("\"!monitors all\" output is null. Can not proceed with test");
			return;
		}

		/*
		 * On Windows, OMR defines a monitor named "portLibrary_omrsig_master_exception_monitor".
		 * Fail if the string 'exception' is present, but only if it does not follow an underscore ('_').
		 */
		assertTrue(validate(output,
				"Object Monitors:,System Monitors:,j9threadmonitor",
				FailurePatterns,
				false));
	}

	/**
	 * Test !monitors tables and !monitors table
	 */
	public void testMonitorsTables() {
		String output = exec(Constants.MONITORS_CMD, new String[] { "tables" });

		if (null == output) {
			fail("\"!monitors tables\" output is null. Can not proceed with test");
			return;
		}

		// If we're running on a core with some active (object) monitors:
		if (false == output.isEmpty()) {
			assertTrue(validate(output,
					"j9monitortablelistentry",
					FailurePatterns,
					false));

			String tableAddr = getAddressFor(output, null, "j9monitortablelistentry", "\t");
			tableAddr = tableAddr.substring(0, tableAddr.indexOf('>')); // getAddressFor(...) has some issues.

			String tableOutput = exec(Constants.MONITORS_CMD, new String[] { "table", tableAddr });

			assertTrue((null != tableOutput) && (false == tableOutput.isEmpty()));
		}
	}

	/**
	 * Test !monitors objects && !monitors object
	 */
	public void testMonitorsObject() {
		String objectsOutput = exec(Constants.MONITORS_CMD, new String[] { "objects" });

		if (null == objectsOutput) {
			fail("\"!monitors objects\"output is null. Can not proceed with test");
			return;
		}

		String eyecatcher = "Object monitor for ";
		objectsOutput = objectsOutput.substring(objectsOutput.indexOf(eyecatcher) + eyecatcher.length());
		String address = getAddressFor(objectsOutput, null, Constants.J9OBJECT_CMD, "\t");

		System.out.println("getAddressFor returned: " + address);

		if (null != address) { // If we're running on a core with some active (object) monitors:
			String objOutput = exec(Constants.MONITORS_CMD, new String[] { "object", address });
			assertTrue(validate(objOutput,
					"j9objectmonitor,j9threadmonitor,j9vmthread,j9thread",
					FailurePatterns,
					false));
		}
	}

	/**
	 * Test !monitors system
	 */
	public void testMonitorsSystem() {
		String output = exec(Constants.MONITORS_CMD, new String[] { "system", "all" });

		if (null == output) {
			fail("\"!monitors system\"output is null. Can not proceed with test");
			return;
		}

		assertTrue(validate(output,
				"!j9thread",
				FailurePatterns,
				false));
	}

	/**
	 * Test !monitors thread, !monitors j9thread, !monitors j9vmthread
	 */
	public void testMonitorsThread() {
		String threadOutput = exec(Constants.THREAD_CMD, new String[] {});
		String j9threadAddr = getAddressFor(threadOutput, null, Constants.J9THREAD_CMD, "\t");
		String j9vmthreadAddr = getAddressFor(threadOutput, null, Constants.J9VMTHREAD_CMD, "\t");

		String j9threadOutput = exec(Constants.MONITORS_CMD, new String[] { Constants.J9THREAD_CMD, j9threadAddr });
		assertTrue(validate(j9threadOutput,
				Constants.J9THREAD_CMD,
				FailurePatterns,
				false));

		String j9vmthreadOutput = exec(Constants.MONITORS_CMD, new String[] { Constants.J9VMTHREAD_CMD, j9vmthreadAddr });
		assertTrue(validate(j9vmthreadOutput,
				Constants.J9VMTHREAD_CMD,
				FailurePatterns,
				false));

		String findj9threadOutput = exec(Constants.MONITORS_CMD, new String[] { "thread", j9threadAddr });
		assertTrue(validate(findj9threadOutput,
				Constants.J9THREAD_CMD,
				FailurePatterns,
				false));

		String findj9vmthreadOutput = exec(Constants.MONITORS_CMD, new String[] { "thread", j9vmthreadAddr });
		assertTrue(validate(findj9vmthreadOutput,
				Constants.J9VMTHREAD_CMD,
				FailurePatterns,
				false));
	}

}
