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
package j9vm.test.ddrext.util.parser;

import org.testng.log4testng.Logger;

import j9vm.test.ddrext.Constants;

public class StackSlotsOutputParser {

	private static Logger log = Logger.getLogger(FindVMOutputParser.class);
	private static final String pc = "pc = ";
	
	/*
	 * Sample output of !stackslots <thread> command :
	 * 
	 * > !stackslots 0x05583400 
	 * <5583400> *** BEGIN STACK WALK, flags = 00400001 walkThread = 0x0000000005583400 *** 
	 * <5583400> ITERATE_O_SLOTS 
	 * <5583400> RECORD_BYTECODE_PC_OFFSET 
	 * <5583400> Initial values: walkSP = 0x00000000057A1BE8, PC = 0x0000000000000001, literals = 0x0000000000000000, A0 = 0x00000000057A1C00, j2iFrame = 0x0000000000000000, ELS = 0x0000000042118BE8, decomp = 0x0000000000000000
	 * <5583400> Generic special frame: bp = 0x00000000057A1C00, sp = 0x00000000057A1BE8, pc = 0x0000000000000001, cp = 0x0000000000000000, arg0EA = 0x00000000057A1C00, flags = 0x0000000000000000 
	 * <5583400> Bytecode frame: bp = 0x00000000057A1C18, sp = 0x00000000057A1C08, pc = 0x00002AAACE34F11E, cp = 0x000000000587B450, arg0EA = 0x00000000057A1C38, flags = 0x0000000000000000 
	 * <5583400> Method: j9vm/test/corehelper/StackMapCoreGenerator.stackMapTestMethod_4()V !j9method 0x000000000587B678 
	 * <5583400> Bytecode index = 14 
	 * <5583400> Using local mapper 
	 * <5583400> Locals starting at 0x00000000057A1C38 for 0x0000000000000004 slots 
	 * <5583400> I-Slot: a0[0x00000000057A1C38] = 0x00002AAACCB7E3A0 
	 * <5583400> I-Slot: t1[0x00000000057A1C30] = 0x0000000000000000 
	 * <5583400> I-Slot: t2[0x00000000057A1C28] = 0x00002AAACCB7E3B0 
	 * <5583400> I-Slot: t3[0x00000000057A1C20] = 0x000000000587B658 
	 * <5583400> Bytecode frame: bp = 0x00000000057A1C50, sp = 0x00000000057A1C40, pc = 0x00002AAACE34F0F1, cp = 0x000000000587B450, arg0EA = 0x00000000057A1C60, flags = 0x0000000000000000 
	 * <5583400> Method: j9vm/test/corehelper/StackMapCoreGenerator.stackMapTestMethod_3(I)V !j9method 0x000000000587B658 <5583400> Bytecode index = 1 
	 * <5583400> Using local mapper 
	 * <5583400> Locals starting at 0x00000000057A1C60 for 0x0000000000000002 slots 
	 * <5583400> I-Slot: a0[0x00000000057A1C60] = 0x00002AAACCB7E3A0 
	 * <5583400> I-Slot: a1[0x00000000057A1C58] = 0x000000000000000C 
	 * <5583400> Bytecode frame: bp = 0x00000000057A1C78, sp = 0x00000000057A1C68, pc = 0x00002AAACE34F0D3, cp = 0x000000000587B450, arg0EA = 0x00000000057A1C98, flags = 0x0000000000000000 
	 * <5583400> Method: j9vm/test/corehelper/StackMapCoreGenerator.stackMapTestMethod_2(ILj9vm/test/corehelper/StackMapCoreGenerator;)I !j9method 0x000000000587B638
	 * <5583400> Bytecode index = 7 
	 * <5583400> Using local mapper 
	 * <5583400> Locals starting at 0x00000000057A1C98 for 0x0000000000000004 slots 
	 * <5583400> I-Slot: a0[0x00000000057A1C98] = 0x00002AAACCB7E3A0 
	 * <5583400> I-Slot: a1[0x00000000057A1C90] = 0x00002AAA0000000C 
	 * <5583400> I-Slot: a2[0x00000000057A1C88] = 0x00002AAACCB7E3A0 
	 * <5583400> I-Slot: t3[0x00000000057A1C80] = 0x00000000021D0F01
	 */
	
	/**
	 * This methods find out the PC value for the method names <methodName>
	 * 
	 * @arg stackSlotsOutput Output of !stackslots <vmthread> command.
	 * @arg methodName Name of the method for which PC value will be found.
	 * @return String of PC value.
	 */
	public static String getPCForMethodName(String stackSlotsOutput,
			String methodName) {
		String result;
		int tempIndex;
		
		if (null == stackSlotsOutput) {
			log.error("!stackslots output is null");
			return null;
		}
		
		if (null == methodName) {
			log.error("method name is null");
			return null;
		}

		/* Get the index of the method from the stackSlots string */
		tempIndex = stackSlotsOutput.indexOf(methodName);

		/* If the method is not in this stackslots, then return null */
		if (-1 == tempIndex) {
			log.error("method name (" + methodName + ") can not be found in !stackslots output: " + stackSlotsOutput);
			return null;
		}

		/*
		 * <5583400> Bytecode frame: bp = 0x00000000057A1C18, sp =
		 * 0x00000000057A1C08, pc = 0x00002AAACE34F11E, cp = 0x000000000587B450,
		 * arg0EA = 0x00000000057A1C38, flags = 0x0000000000000000 <5583400>
		 * Method:
		 * j9vm/test/corehelper/StackMapCoreGenerator.stackMapTestMethod_4()V
		 * !j9method 0x000000000587B678
		 * 
		 * Since pc value for the method is written before the method name, then
		 * trim the stackslots string after the method name index.
		 */
		result = stackSlotsOutput.substring(0, tempIndex);

		/*
		 * Get the pc start index of the one just before the method name. This
		 * should be the pc corresponds to the method name.
		 */
		tempIndex = result.lastIndexOf(pc);

		/* Do sanity check to see whether pc exist in the remaining string. */
		if (-1 == tempIndex) {
			log.error("pc value is missing for the method name (" + methodName + ") : " + result);
			return null;
		}

		/*
		 * Trim the beginning of the results string until the start of the pc
		 * value
		 */
		result = result.substring(tempIndex + pc.length());

		/*
		 * Check whether the start of the remaining string corresponds to pc
		 * value which starts with 0x
		 */
		if (!result.startsWith(Constants.HEXADDRESS_HEADER)) {
			log.error("pc value do not start with 0x");
			return null;
		}

		/*
		 * PC value ends with comma, so trim the string until comma and that
		 * should be the PC value string
		 */
		result = result.substring(0, result.indexOf(","));

		return result;
	}

}
