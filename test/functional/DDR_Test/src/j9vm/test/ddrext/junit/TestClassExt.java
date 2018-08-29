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

public class TestClassExt extends DDRExtTesterBase {

	private Logger log = Logger.getLogger(TestClassExt.class);

	public void testAllClassesExt() {
		String allClassesOutput = exec(Constants.ALL_CLASSES_CMD,
				new String[] {});
		assertTrue(validate(allClassesOutput,
				Constants.ALL_CLASSES_SUCCESS_KEY,
				Constants.ALL_CLASSESS_FAILURE_KEY, true));
	}

	public void testJ9ClassShapeExt() {
		
		String ramAddr = getRAMClassAddress(Constants.J9CLASSSHAPE_TEST_CLASS);
		if (ramAddr == null) {
			fail("getRAMClassAddress returns null. Can not proceed with testJ9ClassShapeExt");
			return;
		}
		String j9ClassChapeOutput = exec(Constants.J9CLASSSHAPE_CMD,
				new String[] { ramAddr });
		assertTrue(validate(j9ClassChapeOutput,
				Constants.J9CLASSSHAPE_SUCCESS_KEY, null, true));
	}

	public void testJ9VTablesExt() {
		
		String ramAddr = getRAMClassAddress(Constants.J9CLASSSHAPE_TEST_CLASS);
		if (ramAddr == null) {
			fail("getRAMClassAddress returns null. Can not proceed with testJ9VTablesExt");
			return;
		}
		String j9vtablesOutput = exec(Constants.J9VTABLES_CMD,
				new String[] { ramAddr });
		assertTrue(validate(j9vtablesOutput, Constants.J9VTABLES_SUCCESS_KEY,
				null, true));
	}

	public void testJ9StaticsExt() {
		
		String ramAddr = getRAMClassAddress(Constants.J9CLASSSHAPE_TEST_CLASS);
		if (ramAddr == null) {
			fail("getRAMClassAddress returns null. Can not proceed with testJ9StaticsExt");
			return;
		}
		String j9ClassChapeOutput = exec(Constants.J9STATICS_CMD,
				new String[] { ramAddr });
		assertTrue(validate(j9ClassChapeOutput,
				Constants.J9STATICS_SUCCESS_KEY, null, true));
	}

	public void testROMClassSummaryExt() {
		String romClassSummaryOutput = exec(Constants.ROM_CLASS_SUMMARY_CMD,
				new String[] {});
		assertTrue(validate(romClassSummaryOutput,
				Constants.ROM_CLASS_SUMMARY_SUCCESS_KEY,
				Constants.ROM_CLASS_SUMMARY_FAILURE_KEY, true));
	}

	public void testRAMClassSummaryExt() {
		String ramClassSummaryOutput = exec(Constants.RAM_CLASS_SUMMARY_CMD,
				new String[] {});
		assertTrue(validate(ramClassSummaryOutput,
				Constants.RAM_CLASS_SUMMARY_SUCCESS_KEY,
				Constants.RAM_CLASS_SUMMARY_FAILURE_KEY, true));
	}

	public void testClassForNameExt() {
		String classForNameOutput = exec(Constants.CL_FOR_NAME_CMD,
				new String[] { Constants.CL_FOR_NAME_CLASS });
		assertTrue(validate(classForNameOutput,
				Constants.CL_FOR_NAME_SUCCESS_KEY,
				Constants.CL_FOR_NAME_FAILURE_KEY, true));
	}

	public void testClassLoadersSummaryExt() {
		String classLoaderSummaryOutput = exec(
				Constants.CL_LOADERS_SUMMARY_CMD, new String[] {});
		assertTrue(validate(classLoaderSummaryOutput,
				Constants.CL_LOADERS_SUMMARY_SUCCESS_KEY,
				Constants.CL_LOADERS_SUMMARY_FAILURE_KEY, true));
	}

	public void testDumpAllROMClassLinearExt() {
		String dumpAllROMClassLinearOutput = exec(
				Constants.DUMP_ALL_ROM_CLASS_LINEAR_CMD,
				new String[] { Constants.DUMP_ALL_ROM_CLASS_LINEAR_NESTING_THRESHOLD });
		assertTrue(validate(dumpAllROMClassLinearOutput,
				Constants.DUMP_ALL_ROM_CLASS_LINEAR_SUCCESS_KEY,
				Constants.DUMP_ALL_ROM_CLASS_LINEAR_FAILURE_KEY, true));
	}

	public void testDumpROMClassLinearExt() {
		String romClassAddress = getAddress("!j9romclass");
		if (romClassAddress != null && romClassAddress != "") {
			String dumpROMClassLinearOutput = exec(
					Constants.DUMP_ROM_CLASS_LINEAR_CMD,
					new String[] { romClassAddress });
			assertTrue(validate(dumpROMClassLinearOutput,
					Constants.DUMP_ROM_CLASS_LINEAR_SUCCESS_KEY,
					Constants.DUMP_ROM_CLASS_LINEAR_FAILURE_KEY, true));
		}
	}

	public void testDumpROMClassExt() {
		String dumpROMClassOutput = null;
		String romClassAddress = getAddress("!j9romclass");

		/* test for !dumpromclass <address> */
		if (romClassAddress != null && romClassAddress != "") {
			dumpROMClassOutput = exec(Constants.DUMP_ROM_CLASS_CMD,
					new String[] { romClassAddress });
			assertTrue(validate(dumpROMClassOutput,
					Constants.DUMP_ROM_CLASS_SUCCESS_KEY,
					Constants.DUMP_ROM_CLASS_FAILURE_KEY, true));
		}

		/* test for !dumpromclass <address> maps */
		if (romClassAddress != null && romClassAddress != "") {
			dumpROMClassOutput = exec(Constants.DUMP_ROM_CLASS_CMD,
					new String[] { romClassAddress,
							Constants.DUMP_ROM_CLASS_MAPS_CMD });
			assertTrue(validate(dumpROMClassOutput,
					Constants.DUMP_ROM_CLASS_MAPS_SUCCESS_KEY,
					Constants.DUMP_ROM_CLASS_MAPS_FAILURE_KEY, true));
		}

		/* test for !dumpromclass <invalid address> */
		String invalidROMClassAddress = "0x100";
		dumpROMClassOutput = exec(Constants.DUMP_ROM_CLASS_CMD,
				new String[] { invalidROMClassAddress });
		assertTrue(validate(dumpROMClassOutput,
				Constants.DUMP_ROM_CLASS_INVALID_ADDR_SUCCESS_KEY,
				Constants.DUMP_ROM_CLASS_INVALID_ADDR_FAILURE_KEY, true, true));

		/* test for !dumpromclass name:<classname> */
		dumpROMClassOutput = exec(Constants.DUMP_ROM_CLASS_CMD,
				new String[] { Constants.DUMP_ROM_CLASS_NAME_CMD
						+ Constants.DUMP_ROM_CLASS_NAME });
		assertTrue(validate(dumpROMClassOutput,
				Constants.DUMP_ROM_CLASS_NAME_SUCCESS_KEY,
				Constants.DUMP_ROM_CLASS_NAME_FAILURE_KEY, true));

		/* test for !dumpromclass name:<classname with wild card> */
		dumpROMClassOutput = exec(Constants.DUMP_ROM_CLASS_CMD,
				new String[] { Constants.DUMP_ROM_CLASS_NAME_CMD
						+ Constants.DUMP_ROM_CLASS_NAME_WC });
		assertTrue(validate(dumpROMClassOutput,
				Constants.DUMP_ROM_CLASS_NAME_WC_SUCCESS_KEY,
				Constants.DUMP_ROM_CLASS_NAME_WC_FAILURE_KEY, true));

		/* test for !dumpromclass name:<classname> maps */
		dumpROMClassOutput = exec(Constants.DUMP_ROM_CLASS_CMD, new String[] {
				Constants.DUMP_ROM_CLASS_NAME_CMD
						+ Constants.DUMP_ROM_CLASS_NAME,
				Constants.DUMP_ROM_CLASS_MAPS_CMD });
		assertTrue(validate(dumpROMClassOutput,
				Constants.DUMP_ROM_CLASS_NAME_MAPS_SUCCESS_KEY,
				Constants.DUMP_ROM_CLASS_NAME_MAPS_FAILURE_KEY, true));

		/* test for !dumpromclass name:<invalid classname> */
		dumpROMClassOutput = exec(Constants.DUMP_ROM_CLASS_CMD,
				new String[] { Constants.DUMP_ROM_CLASS_NAME_CMD
						+ Constants.DUMP_ROM_CLASS_INVALID_NAME });
		assertTrue(validate(dumpROMClassOutput,
				Constants.DUMP_ROM_CLASS_INVALID_NAME_SUCCESS_KEY,
				Constants.DUMP_ROM_CLASS_INVALID_NAME_FAILURE_KEY, true));

	}

	public void testQueryROMClassExt() {
		String romClassAddress = getAddress("!j9romclass");
		String queryROMClassOutput = exec(Constants.QUERY_ROM_CLASS_CMD,
				new String[] { romClassAddress + ","
						+ Constants.QUERY_ROM_CLASS_QUERY });
		assertTrue(validate(queryROMClassOutput,
				Constants.QUERY_ROM_CLASS_SUCCESS_KEY,
				Constants.QUERY_ROM_CLASS_FAILURE_KEY, true));
	}

	public void testAnalyzeROMClassUTF8Ext() {
		String output = exec(Constants.ANALYSE_ROM_CLASS_UTF8_CMD,
				new String[] {});
		assertTrue(validate(output,
				Constants.ANALYSE_ROM_CLASS_UTF8_SUCCESS_KEY,
				Constants.ANALYSE_ROM_CLASS_UTF8_FAILURE_KEY, true));
	}

	public void testDumpAllRAMClassLinearExt() {
		String dumpAllRAMClassLinearOutput = exec(
				Constants.DUMP_ALL_RAM_CLASS_LINEAR_CMD,
				new String[] { Constants.DUMP_ALL_RAM_CLASS_LINEAR_NESTING_THRESHOLD });
		assertTrue(validate(dumpAllRAMClassLinearOutput,
				Constants.DUMP_ALL_RAM_CLASS_LINEAR_SUCCESS_KEY,
				Constants.DUMP_ALL_RAM_CLASS_LINEAR_FAILURE_KEY, true));
	}

	public void testDumpRAMClassLinearExt() {
		String dumpAllRAMClassLinearOutput = exec(
				Constants.DUMP_ALL_RAM_CLASS_LINEAR_CMD, new String[] { "1" });
		String ramclassAddress = null;
		for (String outLine : dumpAllRAMClassLinearOutput.split(Constants.NL)) {
			if (outLine.contains(Constants.DUMP_RAM_CLASS_LINEAR_CMD)) {
				ramclassAddress = outLine
						.split(Constants.DUMP_RAM_CLASS_LINEAR_CMD)[1].trim();
			}
		}
		if (ramclassAddress != null && ramclassAddress != "") {
			String dumpRAMClassLinearOutput = exec(
					Constants.DUMP_RAM_CLASS_LINEAR_CMD,
					new String[] { ramclassAddress });
			assertTrue(validate(dumpRAMClassLinearOutput,
					Constants.DUMP_RAM_CLASS_LINEAR_SUCCESS_KEY,
					Constants.DUMP_RAM_CLASS_LINEAR_FAILURE_KEY, true));
		} else {
			fail("Failed to retrieve ram class address");
		}
	}

	public void testDumpAllRegionsExt() {
		String dumpAllRegionsOutput = exec(Constants.DUMP_ALL_REGIONS_CMD,
				new String[] {});
		assertTrue(validate(dumpAllRegionsOutput,
				Constants.DUMP_ALL_REGIONS_SUCCESS_KEY,
				Constants.DUMP_ALL_REGIONS_FAILURE_KEY, true));
	}

	public void testDumpAllSegmentsExt() {
		String dumpAllSegmentsOutput = exec(Constants.DUMP_ALL_SEGMENTS_CMD,
				new String[] {});
		String successKeys = null;
		if (is64BitPlatform()) {
			successKeys = Constants.DUMP_ALL_SEGMENTS_COMMON_SUCCESS_KEY
					+ Constants.DUMP_ALL_SEGMENTS_64BITCORE_SUCCESS_KEY;
		} else {
			successKeys = Constants.DUMP_ALL_SEGMENTS_COMMON_SUCCESS_KEY
					+ Constants.DUMP_ALL_SEGMENTS_32BITCORE_SUCCESS_KEY;
		}
		assertTrue(validate(dumpAllSegmentsOutput, successKeys,
				Constants.DUMP_ALL_SEGMENTS_FAILURE_KEY, true));
	}

	public void testDumpSegmentsInListExt() {
		String dumpAllSegmentsOutput = exec(Constants.DUMP_ALL_SEGMENTS_CMD,
				new String[] {});
		String[] outputLines = null;
		String listAddress = null;
		if (System.getProperty("os.name").toLowerCase().contains("windows")) {
			outputLines = dumpAllSegmentsOutput.split("\n");
		} else {
			outputLines = dumpAllSegmentsOutput.split(Constants.NL);
		}

		for (String aLine : outputLines) {
			if (aLine.contains(Constants.DUMP_SEGMENTS_LIST_NAME)) {
				listAddress = aLine.split("j9memorysegmentlist")[1].trim();
			}
		}
		if (listAddress != null && listAddress != "") {
			String dumpSegmentsInListOutput = exec(
					Constants.DUMP_SEGMENTS_IN_LIST_CMD,
					new String[] { listAddress });
			assertTrue(validate(dumpSegmentsInListOutput,
					Constants.DUMP_SEGMENTS_IN_LIST_SUCCESS_KEY,
					Constants.DUMP_SEGMENTS_IN_LIST_FAILURE_KEY, true));
		} else {
			fail("Failed to retrieve list address for segment list : "
					+ Constants.DUMP_SEGMENTS_LIST_NAME);
		}
	}

	public void testFJ9ObjectExt() {
		String objAddr = getAddress("!j9object");
		if (objAddr == null) {
			fail("Not able to find object address. Can not proceed with testFJ9ObjectExt");
			return;
		}

		// Output should be same as j9object now.
		String fj9objectOutput = exec(Constants.FJ9OBJECT_CMD,
				new String[] { objAddr });
		assertTrue(validate(fj9objectOutput, Constants.J9OBJECT_SUCCESS_KEY,
				null, false));		
		
		// Test the address conversion only.
		String fj9objecttoj9objectOutput = exec(
				Constants.FJ9OBJECTTOJ9OBJECT_CMD, new String[] { objAddr });
		assertTrue(validate(fj9objecttoj9objectOutput,
				Constants.FJ9OBJECT_SUCCESS_KEY, null, true));
	}

	public void testDumpAllClassLoadersExt() {
		String dumpAllClassLoadersOutput = exec(Constants.DUMPALLCLASSLOADERS_CMD,
				new String[] {});
		assertTrue(validate(dumpAllClassLoadersOutput,
				Constants.DUMPALLCLASSLOADERS_SUCCESS_KEYS,
				Constants.DUMPALLCLASSLOADERS_FAILURE_KEYS, true));
	}
	
	private String getRAMClassAddress(String className) {
		String allClassesOutput = exec(Constants.ALL_CLASSES_CMD,
				new String[] {});
		if (allClassesOutput != null) {
			for (String strLine : allClassesOutput.split(Constants.NL)) {
				if (strLine.contains(className)) {
					return strLine.split("\t")[0].trim();
				}
			}
			log.error("Can not find " + className + " in allclasses output");
		} else {
			fail("allclasses output is null.");
			return null;
		}
		return null;
	}

	private String getAddress(String key) {
		String classForNameOutput = exec(Constants.CL_FOR_NAME_CMD,
				new String[] { Constants.CL_FOR_NAME_CLASS });
		String j9classAddr = null;
		String objAddr = null;
		for (String outLine : classForNameOutput.split(Constants.NL)) {
			if (outLine.contains("!j9class")) {
				j9classAddr = outLine.split(" ")[1].trim(); // change the blank
															// space to platform
															// independent
				break;
			}
		}
		String j9classOutput = exec("j9class", new String[] { j9classAddr });
		for (String outLine : j9classOutput.split(Constants.NL)) {
			if (outLine.contains(key)) {
				objAddr = outLine.split(key)[1].trim();
				objAddr = objAddr.split(" ")[0].trim();
				break;
			}
		}
		if (objAddr != null && objAddr != "") {
			return objAddr;
		} else {
			fail("Can not extract " + key + " address");
			return null;
		}
	}

}
