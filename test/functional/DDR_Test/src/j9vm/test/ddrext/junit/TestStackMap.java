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
import j9vm.test.ddrext.util.parser.StackSlotsOutputParser;
import j9vm.test.ddrext.util.parser.ThreadsOutputParser;

import org.testng.log4testng.Logger;

/**
 * DDR extension test class to validate !stackmap functionality.
 */
public class TestStackMap extends DDRExtTesterBase {
	private Logger log = Logger.getLogger(TestStackMap.class);

	/* Number of test methods in StackMapCoreGenerator */
	private final int testMethodCount = 9;

	/* Test method name suffix from StackMapCoreGenerator */
	private final String methodNameSuffix = "stackMapTestMethod_";

	/*
	 * Expected stack mapping for each method in StackMapCoreGenerator See
	 * StackMapCoreGenerator.java for more details.
	 */
	private final String[] methodStackMaps = { "111", "100101", "1011001",
			"01000111", "11", "00100011011", "01", "1", "001" };

	/**
	 * Runs !stackmap tests
	 */
	public void testStackMap() {
		String mainThreadAddr;
		String stackSlotsOutput;
		String stackMapOutput;

		/*
		 * Test !stackmap command without argument
		 * 
		 * Expected output: > !stackmap stackmap <pc> - calculate the stack slot
		 * map for the specified PC
		 */
		stackMapOutput = exec(Constants.STACKMAP_CMD, new String[0]);
		assertTrue(validate(stackMapOutput, Constants.STACKMAP_SUCCESS_KEYS_1,
				Constants.STACKMAP_FAILURE_KEYS_1, true));

		/*
		 * Get the main threads address by using !threads DDR extension
		 * 
		 * Sample output:
		 * 
		 * !threads !stack 0x000d2d00 !j9vmthread 0x000d2d00 !j9thread
		 * 0x003c5788 tid 0x1700 (5888) // (main) !stack 0x00136b00 !j9vmthread
		 * 0x00136b00 !j9thread 0x003c59f4 tid 0x16d4 (5844) // (JIT Compilation
		 * Thread-0) !stack 0x00172100 !j9vmthread 0x00172100 !j9thread
		 * 0x003c5ecc tid 0xb90 (2960) // (IProfiler) !stack 0x633c0a00
		 * !j9vmthread 0x633c0a00 !j9thread 0x003c6138 tid 0x10b0 (4272) //
		 * (Signal Dispatcher) !stack 0x63361100 !j9vmthread 0x63361100
		 * !j9thread 0x637d0078 tid 0x144 (324) // (Concurrent Mark Helper)
		 * !stack 0x633ce300 !j9vmthread 0x633ce300 !j9thread 0x637d02e4 tid
		 * 0xe38 (3640) // (GC Slave) !stack 0x63432200 !j9vmthread 0x63432200
		 * !j9thread 0x637d07bc tid 0x154c (5452) // (Attach API wait loop)
		 * !stack 0x633f7a00 !j9vmthread 0x633f7a00 !j9thread 0x637d0550 tid
		 * 0x15e0 (5600) // (Finalizer thread)
		 */
		String threadsOutput = exec(Constants.THREAD_CMD, new String[0]);
		mainThreadAddr = ThreadsOutputParser.getJ9VMThreadAddress(
				threadsOutput, "main");

		if (null == mainThreadAddr) {
			fail("VM thread address could not be found for the thread named \"main\" in the !threads output :\n"
					+ threadsOutput);
		}

		/*
		 * Get stackslots for each method in the stack list by using !stackslots
		 * <vmthread> DDR extension. Stackslots output is used to get the PC for
		 * each method.
		 * 
		 * Sample output:
		 * 
		 * > !stackslots 0x000d2d00 <d2d00> *** BEGIN STACK WALK, flags =
		 * 00400001 walkThread = 0x000D2D00 *** <d2d00> ITERATE_O_SLOTS <d2d00>
		 * RECORD_BYTECODE_PC_OFFSET <d2d00> Initial values: walkSP =
		 * 0x00107070, PC = 0x00000001, literals = 0x00000000, A0 = 0x0010707C,
		 * j2iFrame = 0x00000000, ELS = 0x0088FDC8, decomp = 0x00000000 <d2d00>
		 * Generic special frame: bp = 0x0010707C, sp = 0x00107070, pc =
		 * 0x00000001, cp = 0x00000000, arg0EA = 0x0010707C, flags = 0x00000000
		 * <d2d00> Bytecode frame: bp = 0x00107088, sp = 0x00107080, pc =
		 * 0x634635DB, cp = 0x63483130, arg0EA = 0x00107098, flags = 0x00000000
		 * <d2d00> Method: j9vm/test/corehelper/StackMapCoreGenerator.
		 * stackMapTestMethod_throwException(J)V !j9method 0x6348347C <d2d00>
		 * Bytecode index = 7 <d2d00> Using local mapper <d2d00> Locals starting
		 * at 0x00107098 for 0x00000004 slots <d2d00> I-Slot: a0[0x00107098] =
		 * 0x60D4D1C8 <d2d00> I-Slot: a1[0x00107094] = 0x00000000 <d2d00>
		 * I-Slot: a2[0x00107090] = 0x00000001 <d2d00> I-Slot: t3[0x0010708C] =
		 * 0x001070AA <d2d00> Bytecode frame: bp = 0x001070A4, sp = 0x0010709C,
		 * pc = 0x634635AA, cp = 0x63483130, arg0EA = 0x001070A8, flags =
		 * 0x00000000 <d2d00> Method:
		 * j9vm/test/corehelper/StackMapCoreGenerator.stackMapTestMethod_9()V
		 * !j9method 0x6348346C <d2d00> Bytecode index = 2 <d2d00> Using local
		 * mapper <d2d00> Locals starting at 0x001070A8 for 0x00000001 slots
		 * <d2d00> I-Slot: a0[0x001070A8] = 0x60D4D1C8 <d2d00> Bytecode frame:
		 * bp = 0x001070B4, sp = 0x001070AC, pc = 0x6346357D, cp = 0x63483130,
		 * arg0EA = 0x001070BC, flags = 0x00000000 <d2d00> Method:
		 * j9vm/test/corehelper/StackMapCoreGenerator.stackMapTestMethod_8(I)V
		 * !j9method 0x6348345C <d2d00> Bytecode index = 1 <d2d00> Using local
		 * mapper <d2d00> Locals starting at 0x001070BC for 0x00000002 slots
		 * <d2d00> I-Slot: a0[0x001070BC] = 0x60D4D1C8 <d2d00> I-Slot:
		 * a1[0x001070B8] = 0x00000005
		 */
		stackSlotsOutput = exec(Constants.STACKSLOTS_CMD,
				new String[] { mainThreadAddr });

		for (int i = 0; i < testMethodCount; i++) {
			String methodName = methodNameSuffix + Integer.toString(i + 1);
			String methodPC = StackSlotsOutputParser.getPCForMethodName(
					stackSlotsOutput, methodName);

			if (null == methodPC) {
				fail("PC value could not found for the method named \""
						+ methodName + "\" in !stackslots output :\n"
						+ stackSlotsOutput);
			}

			stackMapOutput = exec(Constants.STACKMAP_CMD,
					new String[] { methodPC });

			/*
			 * Check whether fix strings are in the output for !stackmap
			 * 
			 * Sample !stackmap output:
			 * 
			 * Searching for PC=1665545248 in VM=0x000BAE18... Found method
			 * j9vm/
			 * test/corehelper/StackMapCoreGenerator.stackMapTestMethod_2(Ljava
			 * /lang
			 * /Object;Lj9vm/test/corehelper/StackMapCoreGenerator$WeekDays;)V
			 * !j9method 0x634833FC Relative PC = 32 Method index is 3 Using ROM
			 * method 0x634633EC Stack map (6 slots mapped): 100101
			 */
			assertTrue(validate(stackMapOutput,
					Constants.STACKMAP_SUCCESS_KEYS_2,
					Constants.STACKMAP_FAILURE_KEYS_2, false));

			/* Build the stack map info string to check in the output */
			String stackMapCustomSuccessStr = new String("Stack map \\("
					+ methodStackMaps[i].length() + " slots mapped\\): "
					+ methodStackMaps[i]);
			assertTrue(validate(stackMapOutput, stackMapCustomSuccessStr,
					Constants.STACKMAP_FAILURE_KEYS_2, false));

		}
	}

}
