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

import org.testng.log4testng.Logger;

import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import j9vm.test.ddrext.Constants;
import j9vm.test.ddrext.DDRExtTesterBase;
import j9vm.test.ddrext.util.parser.MethodForNameOutputParser;

public class TestFindExt extends DDRExtTesterBase {

	private Logger log = Logger.getLogger(TestFindExt.class);

	public void testFindMethodFromPC() {
		String methodForNameOut = exec(Constants.METHODFORNAME_CMD,
				new String[] { Constants.METHODFORNAME_METHOD });
		String j9methodAddr = MethodForNameOutputParser.extractMethodAddress(methodForNameOut, null);
		String j9methodOut = exec("j9method", new String[] { j9methodAddr });
		String j9rommethodAddr = extractJ9RomMethodAddress(j9methodOut);

		long longAddr = 0;
		try {
			boolean is64BitPlatform = is64BitPlatform();
			longAddr = CommandUtils.parsePointer(j9rommethodAddr,
					is64BitPlatform);
			longAddr = longAddr + 1;
			String findMethodFromPCOut = exec(Constants.FINDMETHODFROMPC_CMD,
					new String[] { String.valueOf(longAddr) });
			assertTrue(validate(findMethodFromPCOut,
					Constants.FINDMETHODFROMPC_SUCCESS_KEY,
					Constants.FINDMETHODFROMPC_FAILURE_KEY, true));
		} catch (DDRInteractiveCommandException e) {
			e.printStackTrace();
			fail("Failed processing provided PC address value");
		}
	}

	// Findnext command will find match in the search initiated by find, it must
	// issue right after find command
	public void testFindAndFindnext() {
		// !find udata b1234567 will return the address of first occurrence of
		// string "b1234567"
		String findOutput = exec(Constants.FIND_CMD, new String[] { "u32",
				"b1234567" });
		assertTrue(validate(findOutput, Constants.FIND_SUCCESS_KEY,
				Constants.FIND_FAILURE_KEY));

		String findnextOutput = exec(Constants.FINDNEXT_CMD, new String[0]);
		assertTrue(validate(findnextOutput, Constants.FINDNEXT_SUCCESS_KEY,
				Constants.FINDNEXT_FAILURE_KEY, true));
	}

	public void testFindheader() {
		// find address of j9vmthread
		String threadOutput = exec(Constants.THREAD_CMD, new String[0]);
		if (threadOutput == null) {
			fail("threads output is null. Can not proceed with testStack");
			return;
		}
		String threadAddress = getAddressForThreads(Constants.J9VMTHREAD_CMD,
				threadOutput);

		// locate the memory allocation header for the specified address
		if (threadAddress == null) {
			fail("Error parsing Threads output for stack address");
			return;
		} else {
			String findheaderOutput = exec(Constants.FINDHEADER_CMD,
					new String[] { threadAddress });
			assertTrue(validate(findheaderOutput,
					Constants.FINDHEADER_SUCCESS_KEY,
					Constants.FINDHEADER_FAILURE_KEY, true));
		}

	}

	public void testFindvm() {
		String findvmOutput = exec(Constants.FINDVM_CMD, new String[0]);
		assertTrue(validate(findvmOutput, Constants.FINDVM_SUCCESS_KEY,
				Constants.FINDVM_FAILURE_KEY, true));
	}

	public void testFindpattern() {

		String findOutput = exec(Constants.FIND_CMD,
				new String[] { "u32", "b1" });
		if (findOutput == null) {
			fail("find output is null. Can not proceed with Findpattern");
			return;
		}
		String patternAddress = extractPatternAddr(findOutput,
				Constants.FIND_SUCCESS_KEY, 3);

		if (patternAddress == null) {
			fail("Can't get address of hexstring b1");
			return;
		} else {
			// replace pattern address' last 3 digits with "000"
			patternAddress = patternAddress.substring(0,
					patternAddress.length() - 3)
					+ "000";
			// searching pattern on AIX core dump is very slow, provide start
			// address and search byte to speed up searching.
			// start to search pattern b1 at patternAddress and only search
			// 10000000000
			// byte. Since UDATA.MAX (32bit)=4294967295, expect it will search
			// on all memory space.
			String findpatternOutput = exec(Constants.FINDPATTERN_CMD,
					new String[] { "b1,4," + patternAddress + ",10000000000" });
			assertTrue(validate(findpatternOutput,
					Constants.FINDPATTERN_SUCCESS_KEY,
					Constants.FINDPATTERN_FAILURE_KEY, true));
		}
	}

	public void testFindstackvalue() {

		String methodForNameOut = exec(Constants.METHODFORNAME_CMD,
				new String[] { Constants.METHODFORNAME_METHOD });

		String j9methodAddr = MethodForNameOutputParser.extractMethodAddress(methodForNameOut, "JI");

		if (j9methodAddr == null) {
			log.info("Get an address from '!methodforname sleep' output: ");
			log.info(methodForNameOut);
			fail("Error parsing methodforname sleep output.");
			return;
		} else {
			String findstackvalueOutput = exec(Constants.FINDSTACKVALUE_CMD,
					new String[] { j9methodAddr });
			assertTrue(validate(findstackvalueOutput,
					Constants.FINDSTACKVALUE_SUCCESS_KEY,
					Constants.FINDSTACKVALUE_FAILURE_KEY, true));
		}
	}

	private String extractJ9RomMethodAddress(String j9methodOut) {
		String[] outputLines = j9methodOut.split("!j9rommethod");
		String[] lineSplit = outputLines[1].split(System.getProperty("line.separator"));
		return lineSplit[0].trim();
	}


	public String extractPatternAddr(String output, String key, int addrIndex) {

		String[] outputLines = output.split(Constants.NL);
		for (String aLine : outputLines) {
			if (aLine.contains(key)) {
				return aLine.split(" ")[addrIndex].trim();
			}
		}
		return null;
	}
	/*
	 * private String defaultPattern(){
	 * 
	 * String pattern = null; if (java.nio.ByteOrder.nativeOrder() ==
	 * ByteOrder.LITTLE_ENDIAN){ pattern = "674523b1" ; }else{ pattern =
	 * "b1234567"; }
	 * 
	 * return pattern; }
	 */
}
