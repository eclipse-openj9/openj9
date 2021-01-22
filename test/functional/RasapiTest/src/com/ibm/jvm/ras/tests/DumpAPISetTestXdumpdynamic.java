/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.jvm.ras.tests;

import java.util.Arrays;
import java.io.*;

import junit.framework.TestCase;

import com.ibm.jvm.DumpConfigurationUnavailableException;
import com.ibm.jvm.InvalidDumpOptionException;
import static com.ibm.jvm.ras.tests.DumpAPISuite.getFilesByPattern;
import static com.ibm.jvm.ras.tests.DumpAPISuite.deleteFile;

public class DumpAPISetTestXdumpdynamic extends TestCase {

	private short MSG_FILTER_DUMP_COUNT = 2; 
	@Override
	protected void setUp() throws Exception {
		super.setUp();
	}

	@Override
	protected void tearDown() throws Exception {
		// TODO Auto-generated method stub
		super.tearDown();
	}

	private void clearDumpOptions() {
		String initialDumpOptions[] = com.ibm.jvm.Dump.queryDumpOptions();
		try {
			com.ibm.jvm.Dump.setDumpOptions("none");
		} catch (InvalidDumpOptionException idoe ) {
			idoe.printStackTrace();
		} catch (NullPointerException e) {
			e.printStackTrace();
		} catch (DumpConfigurationUnavailableException e) {
			e.printStackTrace();
		}
		String afterNoneOptions[] = com.ibm.jvm.Dump.queryDumpOptions();
		
		assertTrue("Expected an empty array of dump options but got: " + Arrays.toString(afterNoneOptions), afterNoneOptions.length == 0);
		
	}
	
	// This shouldn't work at runtime unless -Xdynamic was specified at startup.
	public void testSetDynamicOnlyOptions() {
		clearDumpOptions();
		String[] dumpTypes = {"java"};
		String[] dumpEvents = {"catch", "throw", "uncaught", "systhrow"}; 
		String[] dumpFilters = {"java/lang/OutOfMemoryError", "java/net/SocketException", "java/io/FileNotFoundException"};
		String[] dumpMsgFilters = {"*JavaFVTTest*", "*JTC*"};
		
		int count = 0;
		for(String type: dumpTypes) {
			for(String event: dumpEvents ) {
				for(String filter: dumpFilters ) {
				    	for(String msgFilter: dumpMsgFilters ) {
						String options = type + ":events=" + event +",filter=" + filter + ",msg_filter=" + msgFilter+ ",label=" + type + count++;
						try {
							com.ibm.jvm.Dump.setDumpOptions(options);
						} catch (InvalidDumpOptionException e) {
							fail("Expected to be able to set these dump options: " + options);
						} catch (DumpConfigurationUnavailableException e) {
							fail("Dump configuration was unavailable.");
						}
					}
				}
			}
		}
		
		String dumpAgents[] = com.ibm.jvm.Dump.queryDumpOptions();
		assertNotNull("Expected array of dump agents returned, not null", dumpAgents);
		int noOfAgentsInstalled = dumpTypes.length * dumpEvents.length * dumpFilters.length * dumpMsgFilters.length;
		assertEquals("Expected " + noOfAgentsInstalled + " dump agents configured found " + dumpAgents.length, noOfAgentsInstalled , dumpAgents.length);

		/* Throws FileNotFoundException & Expects dumps from installed agents*/
		String userDir = System.getProperty("user.dir");
		String dumpFilePattern = "java[0-9]+";
		/* Count the number of files before and after. Should increase by 2. */
		String[] beforeFileNames = getFilesByPattern(userDir, dumpFilePattern);
		int beforeCount = beforeFileNames.length;
		try {
			FileInputStream fin = new FileInputStream("HelloJavaFVTTest.java");
			fail("Expected FileNotFoundException");
		} catch (FileNotFoundException fe) {
			String[] afterFileNames = getFilesByPattern(userDir, dumpFilePattern);
            int afterCount = afterFileNames.length;
			int i = 0;
			assertEquals("Failed to find expected number of files in " + userDir + " , found:" + Arrays.toString(afterFileNames), beforeCount + MSG_FILTER_DUMP_COUNT, afterCount);
			
			/* delete the dump files */
			if (afterCount > MSG_FILTER_DUMP_COUNT) {
				int j = 0;
				for (i = 0; i < afterCount; i++) {
					for (String bfnames: beforeFileNames) {
						if (bfnames.equals(afterFileNames[i])) {
							afterFileNames[i] = null;
							break;
						}
					}
					if(afterFileNames[i] != null) {
						afterFileNames[j++] = afterFileNames[i];
					}
					if(j == 2) {
						break;
					}
				}
			}		
			if (afterCount > 0) {
				deleteFile(afterFileNames[0], this.getName());
				deleteFile(afterFileNames[1], this.getName());
			}
		}		
		clearDumpOptions();
	}

}