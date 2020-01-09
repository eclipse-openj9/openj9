/*******************************************************************************
 * Copyright (c) 2001, 2020 IBM Corp. and others
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
import j9vm.test.ddrext.util.parser.ParserUtil;

import org.testng.log4testng.Logger;

public class TestSharedClassesExt extends DDRExtTesterBase {
	private Logger log = Logger.getLogger(TestSharedClassesExt.class);
	public void testShrcExt() {
		String shrcOutput = exec(Constants.SHRC_CMD, new String[] {});
		assertTrue(validate(shrcOutput, Constants.SHRC_SUCCESS_KEY,
				Constants.SHRC_FAILURE_KEY, true));
	}

	public void testShrcStatsExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_STATS });
		assertTrue(validate(statsOutput, Constants.SHRC_STATS_SUCCESS_KEY,
				Constants.SHRC_STATS_FAILURE_KEY, true));
	}

	public void testShrcAllStatsExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_ALLSTATS });
		if (getJavaVersion() > 8) {
			assertTrue(validate(statsOutput, Constants.SHRC_ALLSTATS_SUCCESS_KEY_JAVA9,
					Constants.SHRC_ALLSTATS_FAILURE_KEY, true));
		} else {
			assertTrue(validate(statsOutput, Constants.SHRC_ALLSTATS_SUCCESS_KEY,
					Constants.SHRC_ALLSTATS_FAILURE_KEY, true));
		}
	}

	public void testShrcRcStatsExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_RCSTATS });
		assertTrue(validate(statsOutput, Constants.SHRC_RCSTATS_SUCCESS_KEY,
				Constants.SHRC_RCSTATS_FAILURE_KEY, true));
	}

	public void testShrcCpStatsExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_CPSTATS });
		if (getJavaVersion() > 8) {
			assertTrue(validate(statsOutput, Constants.SHRC_CPSTATS_SUCCESS_KEY_JAVA9,
					Constants.SHRC_CPSTATS_FAILURE_KEY, true));
		} else {
			assertTrue(validate(statsOutput, Constants.SHRC_CPSTATS_SUCCESS_KEY,
					Constants.SHRC_CPSTATS_FAILURE_KEY, true));
		}
	}

	public void testShrcAOTStatsExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_AOTSTATS });
		assertTrue(validate(statsOutput, Constants.SHRC_AOTSTATS_SUCCESS_KEY,
				Constants.SHRC_AOTSTATS_FAILURE_KEY, true));
	}

	public void testShrcOrphanStatsExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_ORPHANSTATS });
		assertTrue(validate(statsOutput,
				Constants.SHRC_ORPHANSTATS_SUCCESS_KEY,
				Constants.SHRC_ORPHANSTATS_FAILURE_KEY, true));
	}

	public void testShrcScopeStatsExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_SCOPESTATS });
		if (getJavaVersion() > 8) {
			assertTrue(validate(statsOutput, Constants.SHRC_SCOPESTATS_SUCCESS_KEY_JAVA9,
					Constants.SHRC_SCOPESTATS_FAILURE_KEY, true));
		} else {
			assertTrue(validate(statsOutput, Constants.SHRC_SCOPESTATS_SUCCESS_KEY,
					Constants.SHRC_SCOPESTATS_FAILURE_KEY, true));
		}
	}

	public void testShrcByteStatsExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_BYTESTATS });
		if (getJavaVersion() > 8) {
			assertTrue(validate(statsOutput, Constants.SHRC_BYTESTATS_SUCCESS_KEY_JAVA9,
					Constants.SHRC_BYTESTATS_FAILURE_KEY, true));
		} else {
			assertTrue(validate(statsOutput, Constants.SHRC_BYTESTATS_SUCCESS_KEY,
					Constants.SHRC_BYTESTATS_FAILURE_KEY, true));
		}
	}

	//Test whether the startuphint shows up.
	public void testShrcStartUpHintExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_STARTUPHINT });
		assertTrue(validate(statsOutput, Constants.SHRC_STARTUPHINT_SUCCESS_KEY,
				Constants.SHRC_STARTUPHINT_FAILURE_KEY, true));
	}

	public void testShrcCRVSnippetExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_CRVSNIPPETSTATS });
		assertTrue(validate(statsOutput, Constants.SHRC_CRVSNIPPETSTATS_SUCCESS_KEY,
				Constants.SHRC_CRVSNIPPETSTATS_FAILURE_KEY, true));
	}

	public void testShrcUByteStatsExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_UBYTESTATS });
		assertTrue(validate(statsOutput, Constants.SHRC_UBYTESTATS_SUCCESS_KEY,
				Constants.SHRC_UBYTESTATS_FAILURE_KEY, true));
	}

	public void testShrcCLStatsExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_CLSTATS });
		assertTrue(validate(statsOutput,
				Constants.SHRC_CLSTATS_SUCCESS_KEY_NONRTP,
				Constants.SHRC_CLSTATS_FAILURE_KEY_NONRTP, true));
	}

	/*
	 * !shrc classpath <address>-- Print classpath at address we extract
	 * <address> by first running !shrc cpstats
	 */
	public void testShrcClassPathExt() {
		String cpstatsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_CPSTATS });
		String cpAddr = cpstatsOutput.substring(cpstatsOutput.indexOf("1:") + 2,
				cpstatsOutput.indexOf("CLASSPATH")).trim();
		if (cpAddr == null || cpAddr == "") {
			fail("Failed to retrieve class path address");
		} else {
			cpAddr = cpAddr.trim();
			String cpOutput = exec(Constants.SHRC_CMD, new String[] {
					Constants.SHRC_CP, cpAddr });
			if (getJavaVersion() > 8) {
				assertTrue(validate(cpOutput, Constants.SHRC_CP_SUCCESS_KEY_JAVA9,
						Constants.SHRC_CP_FAILIRE_KEY, true));
			} else {
				assertTrue(validate(cpOutput, Constants.SHRC_CP_SUCCESS_KEY,
						Constants.SHRC_CP_FAILIRE_KEY, true));
			}
		}
	}

	/*
	 * !shrc findclass <name> -- Find named class We are looking for
	 * java/lang/Object
	 */
	public void testShrcFindClassExt() {
		String findClassOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_FINDCLASS, Constants.SHRC_FINDCLASS_NAME });
		assertTrue(validate(findClassOutput,
				Constants.SHRC_FINDCLASS_SUCCESS_KEY,
				Constants.SHRC_FINDCLASS_FAILURE_KEY, true));
	}

	/*
	 * !shrc findclassp <name>-- Find named class prefix We are using prefix
	 * java/lang/O
	 */
	public void testShrcFindClasspExt() {
		String findClasspOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_FINDCLASSP, Constants.SHRC_FINDCLASSP_PATTERN });
		assertTrue(validate(findClasspOutput,
				Constants.SHRC_FINDCLASSP_SUCCESS_KEY,
				Constants.SHRC_FINDCLASSP_FAILURE_KEY, true));
	}

	/*
	 * !shrc findaot <name> -- Find AOT for named method (see j9vm.test.ddrext.Constants)
	 */
	public void testShrcFindAOTExt() {
		String findAotOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_FINDAOT, Constants.SHRC_FINDAOT_METHODNAME });
		if (!validate(findAotOutput, Constants.SHRC_FINDAOT_NO_ENTRY_KEY,
				Constants.SHRC_FINDAOT_FAILURE_KEY, true)) {
			assertTrue(validate(findAotOutput, Constants.SHRC_FINDAOT_SUCCESS_KEY,
					Constants.SHRC_FINDAOT_FAILURE_KEY, true));
		}
	}

	/*
	 * !shrc findaotp <name> -- Find AOT for named method prefix (see j9vm.test.ddrext.Constants)
	 */
	public void testShrcFindAOTpExt() {
		String findAotpOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_FINDAOTP, Constants.SHRC_FINDAOTP_PATTERN });
		if (!validate(findAotpOutput, Constants.SHRC_FINDAOTP_NO_ENTRY_KEY,
				Constants.SHRC_FINDAOTP_FAILURE_KEY, true)) {
			assertTrue(validate(findAotpOutput,
					Constants.SHRC_FINDAOTP_SUCCESS_KEY,
					Constants.SHRC_FINDAOTP_FAILURE_KEY, true));
		}
	}

	/*
	 * !shrc aotfor <address>-- Find AOT for rom method. We first run !shrc findaot for the named
	 * method (see j9vm.test.ddrext.Constants), then extract the j9rommethod address and use that
	 * address as target for the !shrc aotfor command.
	 */
	public void testShrcAOTForExt() {
		String findAotOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_FINDAOT, Constants.SHRC_FINDAOT_METHODNAME });
		if (findAotOutput == null || findAotOutput == "") {
			fail("Prerequisite command shrc findaot output is null");
		} else if (!validate(findAotOutput, Constants.SHRC_FINDAOT_NO_ENTRY_KEY,
				Constants.SHRC_FINDAOT_FAILURE_KEY, true)) {
			// extract the j9rommethod address
			String address = null;
			String[] outputLines = null;
			if (System.getProperty("os.name").toLowerCase().contains("windows")) {
				outputLines = findAotOutput.split("\n");
			} else {
				outputLines = findAotOutput.split(Constants.NL);
			}
			for (int i = 0; i < outputLines.length; i++) {
				if (outputLines[i].contains("j9rommethod")) {
					address = outputLines[i].split("j9rommethod")[1];
					break;
				}
			}
			if (address != null && address != "") {
				String aotForOutput = exec(Constants.SHRC_CMD, new String[] {
						Constants.SHRC_AOTFOR, address });
				assertTrue(validate(aotForOutput,
						Constants.SHRC_AOTFOR_SUCCESS_KEY,
						Constants.SHRC_AOTFOR_FAILURE_KEY, true));
			} else {
				fail("Failed to retrieve method address to use in shrc aotfor command");
			}
		}
	}

	/*
	 * !shrc rcfor <address>-- Find romclass metadata from romclass address. We first run !shrc findaot
	 * for the named method (see j9vm.test.ddrext.Constants), then extract the j9romclass address for
	 * the class and use that address as target for the !shrc rcfor command.
	 */
	public void testShrcRCForExt() {
		String findAotOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_FINDAOT, Constants.SHRC_FINDAOT_METHODNAME });
		if (findAotOutput == null || findAotOutput == "") {
			fail("Prerequisite command shrc findaot output is null");
		} else if (!validate(findAotOutput,Constants.SHRC_FINDAOT_NO_ENTRY_KEY, 
				Constants.SHRC_FINDAOT_FAILURE_KEY, true)) {
			// extract the j9rommethod address for ThreadWorker.run()
			String address = null;
			String[] outputLines = findAotOutput.split(Constants.NL);
			for (int i = 0; i < outputLines.length; i++) {
				if (outputLines[i].contains("j9romclass")) {
					address = outputLines[i].split("j9romclass")[1].trim();
					break;
				}
			}
			if (address != null && address != "") {
				String rcForOutput = exec(Constants.SHRC_CMD, new String[] {
						Constants.SHRC_RCFOR, address });
				assertTrue(validate(rcForOutput,
						Constants.SHRC_RCFOR_SUCCESS_KEY,
						Constants.SHRC_RCFOR_FAILURE_KEY, true));
			} else {
				fail("Failed to retrieve j9romclass address to use in shrc rcfor command");
			}
		}
	}

	/*
	 * !shrc method <address> -- Lookup rom method in cache. We first run !shrc findaot for the named
	 * method (see j9vm.test.ddrext.Constants), then extract the j9rommethod address for the method
	 * and use that address as target for the !shrc method command.
	 */
	public void testShrcMethodExt() {
		String findAotOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_FINDAOT, Constants.SHRC_FINDAOT_METHODNAME });
		if (findAotOutput == null || findAotOutput == "") {
			fail("Prerequisite command shrc findaot output is null");
		} else if (!validate(findAotOutput,Constants.SHRC_FINDAOT_NO_ENTRY_KEY, 
				Constants.SHRC_FINDAOT_FAILURE_KEY, true)) {
			// extract the j9rommethod address
			String address = null;
			String[] outputLines = null;
			if (System.getProperty("os.name").toLowerCase().contains("windows")) {
				outputLines = findAotOutput.split("\n");
			} else {
				outputLines = findAotOutput.split(Constants.NL);
			}
			for (int i = 0; i < outputLines.length; i++) {
				if (outputLines[i].contains("j9rommethod")) {
					address = outputLines[i].split("j9rommethod")[1];
					break;
				}
			}
			if (address != null && address != "") {
				String methodForOutput = exec(Constants.SHRC_CMD, new String[] {
						Constants.SHRC_METHOD, address });
				assertTrue(validate(methodForOutput,
						Constants.SHRC_METHOD_SUCCESS_KEY,
						Constants.SHRC_METHOD_FAILURE_KEY, true));
			} else {
				fail("Failed to retrieve method address to use in shrc aotfor command");
			}
		}
	}

	/*
	 * We first run !shrc stats and get the cache header address, then run !shrc
	 * incache with that address to look for the success string - <address> is
	 * in the cache header
	 */
	public void testShrcIncacheExt() {
		String statsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_STATS });
		if (statsOutput == null || statsOutput == "") {
			fail("Prerequisite command shrc stats output is null");
		} else {
			String[] statsOutputLines = null;
			if (System.getProperty("os.name").toLowerCase().contains("windows")) {
				statsOutputLines = statsOutput.split("\n");
			} else {
				statsOutputLines = statsOutput.split(Constants.NL);
			}
			String headerAddress = null;
			for (String line : statsOutputLines) {
				if (line.startsWith("!j9sharedcacheheader")) {
					headerAddress = line.split(" ")[1].trim();
					break;
				}
			}
			if (headerAddress == null || headerAddress == "") {
				fail("Failed to retrieve header address");
			} else {
				String incacheOutput = exec(Constants.SHRC_CMD, new String[] {
						Constants.SHRC_INCACHE, headerAddress });
				assertTrue(validate(incacheOutput,
						Constants.SHRC_INCACHE_SUCCESS_KEY,
						Constants.SHRC_INCACHE_FAILURE_KEY, true));
			}
		}
	}

	/*
	 * Removing this test from non-realtime bucket Please refer to CMVC : 184417
	 */
	/*
	 * public void testShrcCacheletExt(){ String cacheletOutput =
	 * exec(Constants.SHRC_CMD,new String[] {
	 * Constants.SHRC_CACHELET,Constants.SHRC_CACHELET_ADDR_NONRTP},false);
	 * assertTrue
	 * (validate(cacheletOutput,Constants.SHRC_CACHELET_SUCCESS_KEY_NONRTP
	 * ,Constants.SHRC_CACHELET_FAILURE_KEY_NONRTP,true,true)); }
	 */

	/* JIT profile tests */
	public void testShrcJITPStatsExt() {
		String jitpstatsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_JITPSTATS });
		assertTrue(validate(jitpstatsOutput,
				Constants.SHRC_JITPSTATS_SUCCESS_KEY,
				Constants.SHRC_JITPSTATS_FAILURE_KEY, true));
	}

	public void testShrcFindJITPext() {

		String jitpStatsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_JITPSTATS });
		if (jitpStatsOutput.contains("JITPROFILE data") == false) {
			fail("No jit profile information found.");
			return;
		}

		String methodFindInJitpStats = ParserUtil
				.getMethodNameFromJITPStats(jitpStatsOutput);
		if (methodFindInJitpStats == null) {
			fail("Failed to extract j9rommethod name for JITPROFILE. Can not proceed with test");
			return;
		}

		String j9rommethodAddr = ParserUtil.getMethodAddrForName(
				jitpStatsOutput, methodFindInJitpStats);
		if (j9rommethodAddr == null) {
			fail("Failed to extract j9rommethod address for JITPROFILE. Can not proceed with test");
			return;
		}

		String findjitpOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_FINDJITP, methodFindInJitpStats });

		String successKey = methodFindInJitpStats + "," + j9rommethodAddr + ","
				+ Constants.SHRC_JITP_SUCCESS_KEY;
		assertTrue(validate(findjitpOutput, successKey,
				Constants.SHRC_FINDJITP_FAILURE_KEY, true));
	}

	public void testShrcFindJITPPext() {

		String jitpStatsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_JITPSTATS });
		if (jitpStatsOutput.contains("JITPROFILE data") == false) {
			fail("No jit profile information found.");
			return;
		}

		String methodFindInJitpStats = ParserUtil
				.getMethodNameFromJITPStats(jitpStatsOutput);
		if (methodFindInJitpStats == null) {
			fail("Failed to extract j9rommethod name for JITPROFILE. Can not proceed with test");
			return;
		}

		String j9rommethodAddr = ParserUtil.getMethodAddrForName(
				jitpStatsOutput, methodFindInJitpStats);
		if (j9rommethodAddr == null) {
			fail("Failed to extract j9rommethod address for JITPROFILE. Can not proceed with test");
			return;
		}

		String methodPre;
		if (methodFindInJitpStats.length() > 1) {
			methodPre = methodFindInJitpStats.substring(0, 2);
		} else {
			methodPre = methodFindInJitpStats.substring(0, 1);
		}

		String findjitppOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_FINDJITPP, methodPre });

		String successKey = methodFindInJitpStats + "," + j9rommethodAddr + ","
				+ Constants.SHRC_JITP_SUCCESS_KEY;
		assertTrue(validate(findjitppOutput, successKey,
				Constants.SHRC_FINDJITPP_FAILURE_KEY, true));
	}

	public void testShrcJITPForExt() {

		String jitpStatsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_JITPSTATS });
		if (jitpStatsOutput.contains("JITPROFILE data") == false) {
			fail("No jit profile information found.");
			return;
		}

		String methodFindInJitpStats = ParserUtil
				.getMethodNameFromJITPStats(jitpStatsOutput);
		if (methodFindInJitpStats == null) {
			fail("Failed to extract j9rommethod name for JITPROFILE. Can not proceed with test");
			return;
		}

		String j9rommethodAddr = ParserUtil.getMethodAddrForName(
				jitpStatsOutput, methodFindInJitpStats);
		if (j9rommethodAddr == null) {
			fail("Failed to extract j9rommethod address for JITPROFILE. Can not proceed with test");
			return;
		}

		String jitpForOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_JITPFOR, j9rommethodAddr });

		String successKey = methodFindInJitpStats + "," + j9rommethodAddr + ","
				+ Constants.SHRC_JITP_SUCCESS_KEY;

		assertTrue(validate(jitpForOutput, successKey,
				Constants.SHRC_JITPFOR_FAILURE_KEY, true));
	}

	/* JIT hint tests */
	public void testShrcJITHStatsExt() {
		if (!isJitHintSupported()) {
			log.info(getName()+" is not applicable for java version "+getVersionString());	
			return;
		}
		String jitpstatsOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_JITHSTATS });
		assertTrue(validate(jitpstatsOutput,
				Constants.SHRC_JITHSTATS_SUCCESS_KEY,
				Constants.SHRC_JITHSTATS_FAILURE_KEY, true));
	}

	public void testShrcFindJITHext() {
		if (!isJitHintSupported()) {
			log.info(getName()+" is not applicable for java version "+getVersionString());	
			return;
		}
		String findjitpOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_FINDJITH, Constants.SHRC_FINDJITH_METHODNAME });
		assertTrue(validate(findjitpOutput,
				Constants.SHRC_FINDJITH_SUCCESS_KEY,
				Constants.SHRC_FINDJITH_FAILURE_KEY, true));
	}

	public void testShrcFindJITHPext() {
		if (!isJitHintSupported()) {
			log.info(getName()+" is not applicable for java version "+getVersionString());	
			return;
		}
		String findjitppOutput = exec(Constants.SHRC_CMD,
				new String[] { Constants.SHRC_FINDJITHP,
						Constants.SHRC_FINDJITHP_METHODPREFIX });
		assertTrue(validate(findjitppOutput,
				Constants.SHRC_FINDJITHP_SUCCESS_KEY,
				Constants.SHRC_FINDJITHP_FAILURE_KEY, true));
	}

	public void testShrcJITHForExt() {
		if (!isJitHintSupported()) {
			log.info(getName()+" is not applicable for java version "+getVersionString());	
			return;
		}
		String findjitpOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_FINDJITH, Constants.SHRC_FINDJITH_METHODNAME });
		if (findjitpOutput.contains("JITHINT data") == false) {
			fail("No jit hint information found for the method :"
					+ Constants.SHRC_FINDJITH_METHODNAME
					+ ". Can not proceed with test");
			return;
		}
		String j9rommethodAddr = null;
		String lines[] = null;
		lines = findjitpOutput.split(Constants.NL);

		if (lines.length == 1) {
			lines = findjitpOutput.split("\n");
		}

		for (int i = 0; i < lines.length; i++) {
			if (lines[i].contains("java/lang/Object") || lines[i].matches("\\s<init>\\(.*\\).*j9rommethod .*")) {
				// lines[i].matches: For the case that method does not contain java/lang/Object
				// we can find find j9rommethod address from <init> method in openj9
				if (lines.length > 1) {
					j9rommethodAddr = lines[i].split("j9rommethod")[1].trim();
				} else {
					j9rommethodAddr = null;
				}
				break;
			}
		}

		if (j9rommethodAddr == null) {
			fail("Failed to extract j9rommethod address. Can not proceed with test");
			return;
		}

		String jitpForOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_JITHFOR, j9rommethodAddr });
		assertTrue(validate(jitpForOutput, Constants.SHRC_JITHFOR_SUCCESS_KEY,
				Constants.SHRC_JITHFOR_FAILURE_KEY, true));
	}
	
	/*
	 * sample output of !shrc write on zOS core: 
	 * Writing snap trace to: snap.trc
	 * !j9sharedclassconfig 0x000000483A05E290
	 * 
	 * Cache name is C260M3A64_memory_ddrextjunitSCC_G21 Cache start
	 * 0x20000000000 size 16777216 Writing cache to
	 * /team/sshi/temp/C260M3A64_memory_ddrextjunitSCC_G21 Unable to write
	 * complete cache: Memory Fault reading 0x2000025C0C0 : Attempting to write
	 * only populated areas of cache. Cache name is
	 * C260M3A64_memory_ddrextjunitSCC_G21 Cache start 0x20000000000 size
	 * 16777216 Writing cache to
	 * /team/sshi/temp/C260M3A64_memory_ddrextjunitSCC_G21 Cache successfully
	 * written to /team/sshi/temp/C260M3A64_memory_ddrextjunitSCC_G21
	 * 
	 * sample output of !shrc write on zLinux core: 
	 * !j9sharedclassconfig 0x00000000800ED920
	 * 
	 * Cache name is C260M3A64P_ddrextjunitSCC_G21 Cache start 0x200230c8000
	 * size 16777216 Writing cache to
	 * /team/sshi/temp/C260M3A64P_ddrextjunitSCC_G21
	 */
	public void testShrcWriteExt() {
		String writeOutput = exec(Constants.SHRC_CMD, new String[] {
				Constants.SHRC_WRITE_CACHE, "." });
		assertTrue(validate(writeOutput,
				Constants.SHRC_WRITE_CACHE_SUCCESS_KEY,
				Constants.SHRC_WRITE_CACHE_FAILURE_KEY, true));
	}
	/**
	 * @return true if the core file has been generated by a VM supporting JIT hints.
	 */
	protected boolean isJitHintSupported() {
		if (getJavaVersion() > 7) {
			return true;
		} else if ((getJavaVersion() == 7) && (getJavaSr() > 0)) {
			return true;
		} else if ((getJavaVersion() == 6) && (getVmVersion().equalsIgnoreCase("2.6")) && (getJavaSr() > 1)) {
			return true;
		}
		return false;
	}
}
