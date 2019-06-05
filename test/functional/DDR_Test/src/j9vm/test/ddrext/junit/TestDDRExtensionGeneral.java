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

import j9vm.test.ddrext.AutoRun;
import j9vm.test.ddrext.Constants;
import j9vm.test.ddrext.DDRExtTesterBase;
import j9vm.test.ddrext.SetupConfig;
import j9vm.test.ddrext.util.parser.ClassForNameOutputParser;
import j9vm.test.ddrext.util.parser.FindVMOutputParser;
import j9vm.test.ddrext.util.parser.J9JavaVmOutputParser;
import j9vm.test.ddrext.util.parser.J9MethodOutputParser;
import j9vm.test.ddrext.util.parser.J9PoolOutputParser;
import j9vm.test.ddrext.util.parser.J9PoolPuddleListOutputParser;
import j9vm.test.ddrext.util.parser.MethodForNameOutputParser;
import j9vm.test.ddrext.util.parser.ParserUtil;

import java.math.BigInteger;

import org.testng.log4testng.Logger;

import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;

public class TestDDRExtensionGeneral extends DDRExtTesterBase {
	private Logger log = Logger.getLogger(TestDDRExtensionGeneral.class);

	public void testVMCheck() {
		String vmCheckOutput = exec(Constants.VMCHECK_CMD, new String[] {});
		assertTrue(validate(vmCheckOutput, Constants.VMCHECK_SUCCESS_KEY,
				Constants.VMCHECK_FAILURE_KEY, false));
	}

	/*
	 * runs !walkinterntable first, validate output of options, then runs
	 * !walkinterntable with options 1-5, validates output for each option
	 */
	public void testWalkInternTable() {
		String menuOutput = exec(Constants.WALKINTERNTABLE_CMD,
				new String[] {});
		assertTrue(validate(menuOutput,
				Constants.WALKINTERNTABLE_MENU_SUCCESS_KEY, null, true));
		
		String helpOutput = exec(Constants.WALKINTERNTABLE_CMD,
				new String[] {"help"});
		assertTrue(validate(helpOutput,
				Constants.WALKINTERNTABLE_MENU_SUCCESS_KEY, null, true));

		String opt1Output = exec(Constants.WALKINTERNTABLE_CMD,
				new String[] { "1" });
		assertTrue(validate(opt1Output,
				Constants.WALKINTERNTABLE_OPT1_SUCCESS_KEY, null, true));

		String opt2Output = exec(Constants.WALKINTERNTABLE_CMD,
				new String[] { "2" });
		assertTrue(validate(opt2Output,
				Constants.WALKINTERNTABLE_OPT2_SUCCESS_KEY, null, true));

		String opt3Output = exec(Constants.WALKINTERNTABLE_CMD,
				new String[] { "3" });
		assertTrue(validate(opt3Output,
				Constants.WALKINTERNTABLE_OPT3_SUCCESS_KEY, null, true));

		String opt4Output = exec(Constants.WALKINTERNTABLE_CMD,
				new String[] { "4" });
		assertTrue(validate(opt4Output,
				Constants.WALKINTERNTABLE_OPT4_SUCCESS_KEY, null, true));

		String opt5Output = exec(Constants.WALKINTERNTABLE_CMD,
				new String[] { "5" });
		assertTrue(validate(opt5Output,
				Constants.WALKINTERNTABLE_OPT5_SUCCESS_KEY, null, true));
	}

	public void testWhatIsSetDepth() {
		String output = exec(Constants.WHATISSETDEPTH_CMD,
				new String[] { Constants.WHATISSETDEPTH_DEPTHVALUE });
		assertTrue(validate(output, Constants.WHATISSETDEPTH_SUCCESS_KEY, null,
				true));
	}

	/*
	 * Performs 2 tests: runs !classforname java/lang/Object first, then uses
	 * the address of j9class to run !whatis runs !methodforname sleep first,
	 * then uses the address of the j9method to run !what is
	 */
	public void testWhatis() {
		String classForNameOutput = exec(Constants.CL_FOR_NAME_CMD,
				new String[] { Constants.CL_FOR_NAME_CLASS });
		String classAddr = ClassForNameOutputParser.extractClassAddress(classForNameOutput);
		String output = exec(Constants.WHATIS_CMD, new String[] { classAddr });
		assertTrue(validate(output, Constants.WHATIS_SUCCESS_KEY_FOR_CLASS,
				Constants.WHATIS_FAILURE_KEY, true));
		/*
		 * String methodForNameOutput = exec(Constants.METHODFORNAME_CMD, new
		 * String[]{Constants.METHODFORNAME_METHOD},false); String methodAddr =
		 * extractMethodAddress(methodForNameOutput); output =
		 * exec(Constants.WHATIS_CMD, new String[]{methodAddr},false);
		 * assertTrue
		 * (validate(Constants.WHATIS_CMD+" "+methodAddr,output,Constants
		 * .WHATIS_SUCCESS_KEY_FOR_METHOD,Constants.WHATIS_FAILURE_KEY,false));
		 */
	}

	/* searches for method named sleep */
	public void testMethodForName() {
		String output = exec(Constants.METHODFORNAME_CMD,
				new String[] { Constants.METHODFORNAME_METHOD });
		assertTrue(validate(output, Constants.METHODFORNAME_SUCCESS_KEY,
				Constants.METHODFORNAME_FAILURE_KEY, true));
	}

	/*
	 * runs !methodforname sleep, gets !j9method <addr>, runs it, gets
	 * !bytecodes <addr>, runs it and validates result
	 */
	public void testByteCodes() {
		String methodForNameOutput = exec(Constants.METHODFORNAME_CMD,
				new String[] { Constants.METHODFORNAME_METHOD });
		String methodAddr = MethodForNameOutputParser.extractMethodAddress(methodForNameOutput, null);
		if (methodAddr == null || methodAddr == "") {
			fail("Failed to obtain method address for method : "
					+ Constants.METHODFORNAME_METHOD
					+ ". Can not proceed further with test");
			return;
		}
		String j9methodOutput = exec("j9method", new String[] { methodAddr });
		String byteCodesAddr = J9MethodOutputParser.extractByteCodesAddress(j9methodOutput);
		if (byteCodesAddr == null || byteCodesAddr == "") {
			fail("Failed to obtain bytecodes address for method : "
					+ Constants.METHODFORNAME_METHOD
					+ ". Can not proceed further with test");
			return;
		}
		String byteCodesOutput = exec(Constants.BYTECODES_CMD,
				new String[] { byteCodesAddr });
		assertTrue(validate(byteCodesOutput, Constants.BYTECODES_SUCCESS_KEY,
				Constants.BYTECODES_FAILURE_KEY, true));
	}

	public void testVMConstantPool() {
		String output = exec(Constants.VMCONSTANTPOOL_CMD, new String[] {});
		assertTrue(validate(output, Constants.VMCONSTANTPOOL_SUCCESS_KEY,
				Constants.VMCONSTANTPOOL_FAILURE_KEY, true));
	}

	public void testSnapTrace() {
		String output = exec(Constants.SNAPTRACE_CMD, new String[] {});
		assertTrue(validate(output, Constants.SNAPTRACE_SUCCESS_KEY,
				Constants.SNAPTRACE_FAILURE_KEY, true));
	}

	public void testRanges() {
		if (SetupConfig.getDDRContxt() != null
				&& SetupConfig.getDDRInstance() == null) {
			log.info("This test is not applicable in context of DDR pluigin for native debuggers");
			return;
		}
		String output = exec(Constants.RANGES_CMD, new String[] {});
		assertTrue(validate(output, Constants.RANGES_SUCCESS_KEY,
				Constants.RANGES_FAILURE_KEY, true));
	}

	public void testJ9x() {
		String classForNameOutput = exec(Constants.CL_FOR_NAME_CMD,
				new String[] { Constants.CL_FOR_NAME_CLASS });
		String classAddr = ClassForNameOutputParser.extractClassAddress(classForNameOutput);
		String output = exec(Constants.J9X_CMD, new String[] { classAddr });
		// j9x output contains last 32bit of the address,
		// e.g.classAddr=0x000000001AD9F600, output will contains 1AD9F600
		String classAddr2 = classAddr.substring(classAddr.length() - 7);
		assertTrue(validate(output, classAddr2, Constants.J9X_FAILURE_KEY, true));
	}

	public void testJ9xx() {
		String classForNameOutput = exec(Constants.CL_FOR_NAME_CMD,
				new String[] { Constants.CL_FOR_NAME_CLASS });
		String classAddr = ClassForNameOutputParser.extractClassAddress(classForNameOutput);
		String output = exec(Constants.J9XX_CMD, new String[] { classAddr });
		// j9x output contains last 32bit of the address,
		// e.g.classAddr=0x000000001AD9F600, output will contains 1AD9F600
		String classAddr2 = classAddr.substring(classAddr.length() - 7);
		assertTrue(validate(output, classAddr2, Constants.J9XX_FAILURE_KEY,
				true));
	}

	public void testContext() {
		if (SetupConfig.getDDRContxt() != null
				&& SetupConfig.getDDRInstance() == null) {
			log.info("This test is not applicable in context of DDR pluigin for native debuggers");
			return;
		}
		String output = exec(Constants.CONTEXT_CMD, new String[] {});
		assertTrue(validate(output, Constants.CONTEXT_SUCCESS_KEY,
				Constants.CONTEXT_FAILURE_KEY, true));
	}

	public void testJ9ExtendedMethodFlagInfo() {
		for (int i = 1; i < 16; i++) {
			String output = exec(Constants.J9EXTENDEDMETHODFLAGINFO_CMD,
					new String[] { String.valueOf(i) });
			switch (i) {
			case 1:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY1,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 2:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY2,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 3:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY3,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 4:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY4,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 5:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY5,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 6:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY6,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 7:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY7,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 8:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY8,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 9:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY9,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 10:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY10,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 11:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY11,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 12:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY12,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 13:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY13,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 14:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY14,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			case 15:
				assertTrue(validate(output,
						Constants.J9EXTENDEDMETHODFLAGINFO_SUCCESS_KEY15,
						Constants.J9EXTENDEDMETHODFLAGINFO_FAILURE_KEY, true));
				break;
			}
		}
	}

	/*
	 * In this test, we set the plugin path, reload the plugin, list the plugin,
	 * then run cmd for reloaded plugin.
	 */
	public void testPlugins() {
		if (SetupConfig.getDDRContxt() != null
				&& SetupConfig.getDDRInstance() == null) {
			log.info("This test is not applicable in context of DDR plugin for native debuggers");
			return;
		}
		
		if (AutoRun.ddrPluginJar == null || AutoRun.ddrPluginJar.isEmpty()) {
			fail("Not able to find ddrPlugin.");
			return;
		}
		String path = AutoRun.ddrPluginJar.replace("\\", "/");

		log.debug("Plugin path: " + path);

		String pluginsSetpathOutput = exec(Constants.PLUGINS_CMD, new String[] {
				Constants.PLUGINS_SETPATH_CMD, path });
		assertTrue(validate(pluginsSetpathOutput,
				Constants.PLUGINS_SETPATH_SUCCESS_KEY + path,
				Constants.PLUGINS_SETPATH_FAILURE_KEY, false));

		String pluginsReloadOutput = exec(Constants.PLUGINS_CMD,
				new String[] { Constants.PLUGINS_RELOAD_CMD });
		assertTrue(validate(pluginsReloadOutput,
				Constants.PLUGINS_RELOAD_SUCCESS_KEY,
				Constants.PLUGINS_RELOAD_FAILURE_KEY, false));

		String pluginsListOutput = exec(Constants.PLUGINS_CMD,
				new String[] { Constants.PLUGINS_LIST_CMD });
		assertTrue(validate(pluginsListOutput,
				Constants.PLUGINS_LIST_SUCCESS_KEY,
				Constants.PLUGINS_LIST_FAILURE_KEY, false));

		String pluginsTestOutput = exec(Constants.PLUGINS_TEST_CMD,
				new String[] {});
		assertTrue(validate(pluginsTestOutput,
				Constants.PLUGINS_TEST_SUCCESS_KEY,
				Constants.PLUGINS_TEST_FAILURE_KEY, false));
	}
	
	/**
	 * This junit method tests the !setvm DDR extension functionality
	 */
	public void testSetVM()
	{
		BigInteger vmAddr;
		BigInteger mainThreadAddr;
		
		/* Get the vm address from core file by using !findvm extension */
		String findVMOutput = exec(Constants.FINDVM_CMD, new String[] {});
		String j9javavmAddr = FindVMOutputParser.getJ9JavaVMAddress(findVMOutput);
		if (null == j9javavmAddr) {
			fail("j9javavm address could not be found in the !findvm output");
			return;
		} 
		
		try {
			vmAddr = ParserUtil.toBigInteger(j9javavmAddr);
		} catch (NumberFormatException e) {
			/* We should never be here */
			fail(j9javavmAddr + " is not a valid number.");
			return;
		}
		
		String customSetVMSuccessKey = Constants.SETVM_SUCCESS_KEYS + Constants.HEXADDRESS_HEADER + "[0]*" + vmAddr.toString(Constants.HEXADECIMAL_RADIX);
		
		/* Try to set the vm to its actual address */
		validate(exec(Constants.SETVM_CMD, new String[] {vmAddr.toString()}), customSetVMSuccessKey, Constants.SETVM_FAILURE_KEYS);
		validate(exec(Constants.SETVM_CMD, new String[] {Constants.HEXADDRESS_HEADER + vmAddr.toString(Constants.HEXADECIMAL_RADIX)}), customSetVMSuccessKey, Constants.SETVM_FAILURE_KEYS);
		
		/* Find the mainThread address */
		String j9javavmOutput = exec(Constants.J9JAVAVM_CMD, new String[] {j9javavmAddr});
		String mainThreadAddress = J9JavaVmOutputParser.getMainThreadAddress(j9javavmOutput);
		if (null == mainThreadAddress) {
			fail("mainThread address could not be found in the !j9javavm output");
			return;
		}
		
		try {
			mainThreadAddr = ParserUtil.toBigInteger(mainThreadAddress);
		} catch (NumberFormatException e) {
			/* We should never be here */
			fail(mainThreadAddress + " is not a valid number.");
			return;
		}
		
		/* Try to set the vm to the mainThread  address */
		validate(exec(Constants.SETVM_CMD, new String[] {mainThreadAddr.toString()}), customSetVMSuccessKey, Constants.SETVM_FAILURE_KEYS);
		validate(exec(Constants.SETVM_CMD, new String[] {Constants.HEXADDRESS_HEADER + mainThreadAddr.toString(Constants.HEXADECIMAL_RADIX)}), customSetVMSuccessKey, Constants.SETVM_FAILURE_KEYS);
		
		/* Try to set the vm to the invalid j9javavm address. 
		 * j9javavm address + 8 can be used as an invalid since 
		 * it is still in the j9javavm structure and 
		 * can not be another valid j9javavm structure
		 */
		validate(exec(Constants.SETVM_CMD, new String[] {vmAddr.add(new BigInteger("8")).toString()}), Constants.SETVM_FAILURE_KEY_1 + "," + Constants.SETVM_FAILURE_KEY_2, Constants.SETVM_SUCCESS_KEYS);
		validate(exec(Constants.SETVM_CMD, new String[] {Constants.HEXADDRESS_HEADER + vmAddr.add(new BigInteger("8")).toString(Constants.HEXADECIMAL_RADIX)}),  Constants.SETVM_FAILURE_KEY_1 + "," + Constants.SETVM_FAILURE_KEY_2, Constants.SETVM_SUCCESS_KEYS);
		
	}
	
	/**
	 * This junit method tests the !showdumpagents DDR extension functionality
	 */
	public void testShowdumpagents() {

		String output = exec(Constants.SHOWDUMPAGENTS_CMD, new String[] {});
		assertTrue(validate(output, Constants.SHOWDUMPAGENTS_SUCCESS_KEYS,
				Constants.SHOWDUMPAGENTS_FAILURE_KEYS, false));
	}

	/**
	 * This junit method tests the !sym DDR extension functionality
	 * 
	 * run !methodforname to gets !j9method <addr>, run j9method to get
	 * methodRunAddress, then run sym with the methodRunAddress
	 */
	/* CMVC 192844 - temporarily remove sub test until I can find decent replacement for array copy
	public void testSym() {

		String methodForNameOutput = exec(Constants.METHODFORNAME_CMD,
				new String[] { Constants.SYM_TEST_METHOD });
		String methodAddr = MethodForNameOutputParser
				.extractMethodAddress(methodForNameOutput);
		String j9methodOutput = exec(Constants.J9METHOD_CMD,
				new String[] { methodAddr });
		String methodRunAddr = J9MethodOutputParser
				.extractMethodAddress(j9methodOutput);
		String output = exec(Constants.SYM_CMD, new String[] { methodRunAddr });
		assertTrue(validate(output, Constants.SYM_SUCCESS_KEYS,
				Constants.SYM_FAILURE_KEYS, false));
	}
	*/

	/**
	 * This junit method tests the !dclibs DDR extension functionality
	 */
	public void testDclibs() {

		String output = exec(Constants.DCLIBS_CMD, new String[] { });
		assertTrue(validate(output, Constants.DCLIBS_SUCCESS_KEYS,
				Constants.DCLIBS_FAILURE_KEYS, false));
		
		//if library is collected to the core, then test "!dclibs extract" command to extract libs to temp folder
		if (output.contentEquals(Constants.DCLIBS_LIB_COLLENTED)){
			String output2 = exec(Constants.DCLIBS_CMD, new String[] {"extract" });
			assertTrue(validate(output2, Constants.DCLIBS_EXTRACT_SUCCESS_KEYS,
					Constants.DCLIBS_FAILURE_KEYS, false));
		}
	}

	/**
	 * This junit method tests the !walkj9pool DDR extension functionality
	 */
	public void testWalkJ9Pool() 
	{	
		/* Get the vm address from core file by using !findvm extension */
		String findVMOutput = exec(Constants.FINDVM_CMD, new String[] {});
		String j9javavmAddr = FindVMOutputParser.getJ9JavaVMAddress(findVMOutput);
		if (null == j9javavmAddr) {
			fail("j9javavm address could not be found in the !findvm output");
			return;
		} 
		
		String j9javavmOutput = exec(Constants.J9JAVAVM_CMD, new String[] {j9javavmAddr});
		
		/* Test !walkj9pool with the pool used for classLoaderBlocks in J9JavaVm */
		String classLoaderBlocks = J9JavaVmOutputParser.getClassLoaderBlocksAddress(j9javavmOutput);
		testWalkJ9PoolFor(classLoaderBlocks);

		/* Test !walkj9pool with the pool used for classLoadingStackPool in J9JavaVm */
		String classLoadingStackPool = J9JavaVmOutputParser.getClassLoadingStackPoolAddress(j9javavmOutput);
		testWalkJ9PoolFor(classLoadingStackPool);

		/* Test !walkj9pool with the pool used for dllLoadTable in J9JavaVm */
		String dllLoadTable = J9JavaVmOutputParser.getDllLoadTableAddress(j9javavmOutput);
		testWalkJ9PoolFor(dllLoadTable);

		/* Test !walkj9pool with the pool used for systemProperties in J9JavaVm */
		String systemProperties = J9JavaVmOutputParser.getSystemPropertiesAddress(j9javavmOutput);
		testWalkJ9PoolFor(systemProperties);
	}
	
	/**
	 * This method tests the !walkj9pool DDR extension with the given pool address
	 * @param poolAddr
	 */
	private void testWalkJ9PoolFor(String poolAddr) 
	{
		String j9poolOutput = exec(Constants.J9POOL_CMD, new String[] {poolAddr});
		String puddleListAddress = J9PoolOutputParser.getPuddleListAddress(j9poolOutput);
		if (null == puddleListAddress) {
			fail("Failed to get puddleList address value from !j9pool output: \n" + j9poolOutput);
		}
		String j9poolpuddlelistOutput = exec(Constants.J9POOLPUDDLELIST_CMD, new String[] {puddleListAddress});
		String numElementsValue = J9PoolPuddleListOutputParser.getnumElementsValue(j9poolpuddlelistOutput);
		if (null == numElementsValue) {
			fail("Failed to get numElements field value from !j9poolpuddlelist output: \n" + j9poolpuddlelistOutput);
		}
		
		String walkJ9PoolOutput = exec(Constants.WALKJ9POOL_CMD, new String[] {poolAddr});
		validate(walkJ9PoolOutput, Constants.WALKJ9POOL_SUCCESS_KEYS, Constants.WALKJ9POOL_FAILURE_KEY);
		
		/* Get the number of elements printed in !walkj9pool output */
		long numElementsInWalkJ9Pool = getNumElementsInWalkJ9PoolOutput(walkJ9PoolOutput);
		if (-1 == numElementsInWalkJ9Pool) {
			/* Should never be here after being successful in above validate call */
			fail("Unexpected !walkj9pool output : \n" + walkJ9PoolOutput);
		}
		
		try {
			long numElementsInPuddleList = CommandUtils.parseNumber(numElementsValue).longValue();
			/* Check whether number of elements printed is same as number of elements in the poolpuddlelist */
			if (numElementsInWalkJ9Pool != numElementsInPuddleList) {
				fail("Number of elements printed is not same as the number of elements in poolpuddlelist (" + numElementsInPuddleList + ")");
			}
		} catch (DDRInteractiveCommandException e) {
			fail(e.getMessage());
		}
		
	}
	
	/**
	 * This method calculates the number of elements printed in !walkj9pool address
	 * @param walkJ9PoolOutput Output of !walkj9pool <address> DDR extension
	 * @return
	 */
	private long getNumElementsInWalkJ9PoolOutput(String walkJ9PoolOutput) 
	{
		long numElements = -1;
		String lastInfoLine = null;
		String [] lines = walkJ9PoolOutput.split("\n");
		for (int i = 0; i < lines.length; i++) {
			String currentLine = lines[i].trim();
			if (currentLine.startsWith("[")) {
				lastInfoLine = currentLine;
			} else if (currentLine.equals("}")) {
				/* Closing parenthesis in !walkj9pool output is encountered.*/
				break;
			} else if (currentLine.startsWith("J9Pool at")) {
				numElements = 0;
			}
		}
		
		if(null != lastInfoLine) {
			int lastIndex = lastInfoLine.indexOf("]");
			if (-1 != lastIndex) {
				numElements = Long.parseLong(lastInfoLine.substring(1, lastIndex));
				numElements++; /*Index starts from 0, so add 1 to get the element number */
			} 
		}	
		return numElements;
	}

	/**
	 * This junit method tests the !j9reg DDR extension functionality
	 */
	public void testJ9Reg()
	{
		String j9regLevel0Output = exec(Constants.J9REG_CMD, new String[] {});
		String j9regLevel1Output = exec(Constants.J9REG_CMD, new String[] {"1"});
		String j9regLevel2Output = exec(Constants.J9REG_CMD, new String[] {"2"});
		String j9regLevel3Output = exec(Constants.J9REG_CMD, new String[] {"3"});
		String j9regLevel4Output = exec(Constants.J9REG_CMD, new String[] {"4"});
		
		if (null == j9regLevel0Output) {
			fail("\"!j9reg\" output is null");
		}
		
		if (null == j9regLevel1Output) {
			fail("\"!j9reg 1\" output is null");
		}

		if (null == j9regLevel2Output) {
			fail("\"!j9reg 2\" output is null");
		}

		if (null == j9regLevel3Output) {
			fail("\"!j9reg 3\" output is null");
		}

		if (null == j9regLevel4Output) {
			fail("\"!j9reg 4\" output is null");
		}
		
		assertTrue(validate(j9regLevel0Output, Constants.J9REG_SUCCESS_KEYS,
				Constants.J9REG_FAILURE_KEYS, false));		
		
		/* Default level to print is 1. So j9reg and j9reg 1 output should be exactly the same.*/
		if (!j9regLevel0Output.equals(j9regLevel1Output)) {
			fail("\"j9reg\" and \"j9reg 1\" output is not same.");
		}
		
		if (!j9regLevel2Output.startsWith(j9regLevel1Output)) {
			fail("\"j9reg 1\" output is not a subset of \"j9reg 2\" output.");
		}
		
		if (!j9regLevel3Output.startsWith(j9regLevel2Output)) {
			fail("\"j9reg 2\" output is not a subset of \"j9reg 3\" output.");
		}
		
		if (!j9regLevel4Output.startsWith(j9regLevel3Output)) {
			fail("\"j9reg 3\" output is not a subset of \"j9reg 4\" output.");
		}	
	}

	
	/**
	 * This junit method tests the !coreinfo DDR extension functionality
	 */
	public void testCoreInfo()
	{
		String coreInfoOutput = exec(Constants.COREINFO_CMD, new String[] {});
		
		if (null == coreInfoOutput) {
			fail("\"!coreinfo\" output is null");
		}
		
		assertTrue(validate(coreInfoOutput, Constants.COREINFO_SUCCESS_KEYS,
				Constants.COREINFO_FAILURE_KEYS, false));	
	}
	
	/**
	 * This junit method tests the !nativememinfo DDR extension functionality
	 */
	public void testNativeMemInfo()
	{
		String nativeMemInfoOutput = exec(Constants.NATIVEMEMINFO_CMD, new String[] {});
		if (null == nativeMemInfoOutput) {
			fail("\"!nativememinfo\" output is null");
		}
		assertTrue(validate(nativeMemInfoOutput, Constants.NATIVEMEMINFO_SUCCESS_KEYS, Constants.NATIVEMEMINFO_FAILURE_KEYS));
	}
}
