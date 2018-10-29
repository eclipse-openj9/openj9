/*******************************************************************************
 * Copyright (c) 2009, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive;

import java.io.PrintStream;

import com.ibm.j9ddr.corereaders.ICore;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.exceptions.UnknownArchitectureException;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;


/**
 * VM register map.
 * 
 * Per-platform register information reproduced here.
 * 
 * @author fkaraman
 *
 */
public class VMRegMapHelper
{
	/**
	 * This method find out which platform core file is generated 
	 * and calls the method that prints that specific platforms registers.   
	 * @param vm J9JavaVMPointer instance of vm from core file. 
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 * @throws UnknownArchitectureException In the case of unidentified platform of core file.  
	 */
	public static void printRegisters(J9JavaVMPointer vm, int level, PrintStream out) throws UnknownArchitectureException {
		IProcess process = vm.getProcess();
		ICore core = process.getAddressSpace().getCore();
		Platform platform = core.getPlatform();
		boolean is64BitPlatform = (process.bytesPerPointer() == 8) ? true : false;

		switch(platform) {
		case AIX:
			if (is64BitPlatform) {
				printRegistersForAIX64BitPPC(level, out);
			} else {
				printRegistersForAIX32BitPPC(level, out);
			}
			break;

		case LINUX:
			String processorType = core.getProperties().getProperty(ICore.PROCESSOR_TYPE_PROPERTY);
			if (is64BitPlatform) {
				if (processorType.equals("amd64")) {
					printRegistersForLinux64BitAMD64(level, out);
				} else if (processorType.equals("ppc")) {
					printRegistersForLinux64BitPPC(level, out);
				} else if (processorType.equals("s390")) {
					printRegistersForLinux64BitS390(level, out);
				}
			} else {
				if (processorType.equals("x86")) {
					printRegistersForLinux32BitX86(level, out);
				} else if (processorType.equals("ppc")) {
					printRegistersForLinux32BitPPC(level, out);
				} else if (processorType.equals("s390")) {
					printRegistersForLinux32BitS390(level, out);
				}
			}
			break;

		case OSX:
			printRegistersForLinux64BitAMD64(level, out);
			break;

		case WINDOWS:
			if (is64BitPlatform) {
				printRegistersForWindows64Bit(level, out);
			} else {
				printRegistersForWindows32Bit(level, out);
			}
			break;

		case ZOS:
			if (is64BitPlatform) {
				printRegistersForZOS64BitS390(level, out);
			} else {
				printRegistersForZOS32BitS390(level, out);
			}
			break;

		default:
			throw new UnknownArchitectureException(process, "Could not determine platform of core file.");
		}

	}

	/**
	 * Prints registers for AIX 64 BIT 
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 */
	private static void printRegistersForAIX64BitPPC(int level, PrintStream out) 
	{
		if (level >= 1) {
			out.println(String.format("=========== Level %d Registers ===========\n", 1));
			out.println(String.format("%16s --> %s", "vmStruct", "r15"));
			out.println(String.format("%16s --> %s", "sp", "r14"));
			out.println(String.format("%16s --> %s", "arg0EA", "r21"));
			out.println(String.format("%16s --> %s", "pc", "r16"));
			out.println(String.format("%16s --> %s", "literals", "r17"));
		}
		if (level >= 2) {
			out.println(String.format("=========== Level %d Registers ===========\n", 2));
			out.println(String.format("%16s --> %s", "bytecodes", "r18"));
			out.println(String.format("%16s --> %s", "detailMessage", "r19"));
			out.println(String.format("%16s --> %s", "exceptionIndex", "r20"));
			out.println(String.format("%16s --> %s", "javaNewClass", "r19"));
			out.println(String.format("%16s --> %s", "messageNumber", "r21"));
			out.println(String.format("%16s --> %s", "methodHandle", "r21"));
			out.println(String.format("%16s --> %s", "moduleName", "r19"));
			out.println(String.format("%16s --> %s", "resolveBytecode", "r20"));
			out.println(String.format("%16s --> %s", "resolveIndex", "r19"));
			out.println(String.format("%16s --> %s", "resolveType", "r21"));
			out.println(String.format("%16s --> %s", "returnAddress", "r19"));
			out.println(String.format("%16s --> %s", "returnPoint", "r10"));
			out.println(String.format("%16s --> %s", "returnSP", "r20"));
			out.println(String.format("%16s --> %s", "saved_cr", "CR"));
			out.println(String.format("%16s --> %s", "sendArgs", "r19"));
			out.println(String.format("%16s --> %s", "sendMethod", "r20"));
			out.println(String.format("%16s --> %s", "sendReturn", "r21"));
			out.println(String.format("%16s --> %s", "syncObject", "r6"));
		}
		if (level >= 3) {
			out.println(String.format("=========== Level %d Registers ===========\n", 3));
			out.println(String.format("%16s --> %s", "64BitReservedThrds", "r13"));
			out.println(String.format("%16s --> %s", "asmAddr1", "r5"));
			out.println(String.format("%16s --> %s", "asmAddr2", "r6"));
			out.println(String.format("%16s --> %s", "asmAddr3", "r7"));
			out.println(String.format("%16s --> %s", "asmData1", "r8"));
			out.println(String.format("%16s --> %s", "asmData2", "r9"));
			out.println(String.format("%16s --> %s", "asmData3", "r10"));
			out.println(String.format("%16s --> %s", "CR", "CR"));
			out.println(String.format("%16s --> %s", "CTR", "CTR"));
			out.println(String.format("%16s --> %s", "destEA", "r28"));
			out.println(String.format("%16s --> %s", "inParm1", "r3"));
			out.println(String.format("%16s --> %s", "inParm2", "r4"));
			out.println(String.format("%16s --> %s", "inParm3", "r5"));
			out.println(String.format("%16s --> %s", "inParm4", "r6"));
			out.println(String.format("%16s --> %s", "inParm5", "r7"));
			out.println(String.format("%16s --> %s", "inParm6", "r8"));
			out.println(String.format("%16s --> %s", "inParm7", "r9"));
			out.println(String.format("%16s --> %s", "inParm8", "r10"));
			out.println(String.format("%16s --> %s", "LR", "LR"));
			out.println(String.format("%16s --> %s", "machineSP", "r1"));
			out.println(String.format("%16s --> %s", "oldSP", "r30"));
			out.println(String.format("%16s --> %s", "outParm1", "r3"));
			out.println(String.format("%16s --> %s", "outParm2", "r4"));
			out.println(String.format("%16s --> %s", "outParm3", "r5"));
			out.println(String.format("%16s --> %s", "outParm4", "r6"));
			out.println(String.format("%16s --> %s", "outParm5", "r7"));
			out.println(String.format("%16s --> %s", "outParm6", "r8"));
			out.println(String.format("%16s --> %s", "outParm7", "r9"));
			out.println(String.format("%16s --> %s", "outParm8", "r10"));
			out.println(String.format("%16s --> %s", "return64hiRead", "r3"));
			out.println(String.format("%16s --> %s", "return64hiWrite", "r3"));
			out.println(String.format("%16s --> %s", "return64loRead", "r4"));
			out.println(String.format("%16s --> %s", "return64loWrite", "r4"));
			out.println(String.format("%16s --> %s", "returnValue", "r3"));
			out.println(String.format("%16s --> %s", "sourceEA", "r31"));
			out.println(String.format("%16s --> %s", "stringCopy5", "r5"));
			out.println(String.format("%16s --> %s", "stringCopy6", "r6"));
			out.println(String.format("%16s --> %s", "stringCopy7", "r7"));
			out.println(String.format("%16s --> %s", "stringCopy8", "r8"));
			out.println(String.format("%16s --> %s", "stringCopyCount", "r9"));
			out.println(String.format("%16s --> %s", "stringCopyDest", "r3"));
			out.println(String.format("%16s --> %s", "stringCopySource", "r4"));
			out.println(String.format("%16s --> %s", "tempEA", "r27"));
			out.println(String.format("%16s --> %s", "TOC", "r2"));
			out.println(String.format("%16s --> %s", "XER", "XER"));
		}
		if (level >= 4) {
			out.println(String.format("=========== Level %d Registers ===========\n", 4));
			out.println(String.format("%16s --> %s", "_CR", "CR"));
			out.println(String.format("%16s --> %s", "_CTR", "CTR"));
			out.println(String.format("%16s --> %s", "_fp0", "fp0"));
			out.println(String.format("%16s --> %s", "_fp1", "fp1"));
			out.println(String.format("%16s --> %s", "_fp10", "fp10"));
			out.println(String.format("%16s --> %s", "_fp11", "fp11"));
			out.println(String.format("%16s --> %s", "_fp12", "fp12"));
			out.println(String.format("%16s --> %s", "_fp13", "fp13"));
			out.println(String.format("%16s --> %s", "_fp14", "fp14"));
			out.println(String.format("%16s --> %s", "_fp15", "fp15"));
			out.println(String.format("%16s --> %s", "_fp16", "fp16"));
			out.println(String.format("%16s --> %s", "_fp17", "fp17"));
			out.println(String.format("%16s --> %s", "_fp18", "fp18"));
			out.println(String.format("%16s --> %s", "_fp19", "fp19"));
			out.println(String.format("%16s --> %s", "_fp2", "fp2"));
			out.println(String.format("%16s --> %s", "_fp20", "fp20"));
			out.println(String.format("%16s --> %s", "_fp21", "fp21"));
			out.println(String.format("%16s --> %s", "_fp22", "fp22"));
			out.println(String.format("%16s --> %s", "_fp23", "fp23"));
			out.println(String.format("%16s --> %s", "_fp24", "fp24"));
			out.println(String.format("%16s --> %s", "_fp25", "fp25"));
			out.println(String.format("%16s --> %s", "_fp26", "fp26"));
			out.println(String.format("%16s --> %s", "_fp27", "fp27"));
			out.println(String.format("%16s --> %s", "_fp28", "fp28"));
			out.println(String.format("%16s --> %s", "_fp29", "fp29"));
			out.println(String.format("%16s --> %s", "_fp3", "fp3"));
			out.println(String.format("%16s --> %s", "_fp30", "fp30"));
			out.println(String.format("%16s --> %s", "_fp31", "fp31"));
			out.println(String.format("%16s --> %s", "_fp4", "fp4"));
			out.println(String.format("%16s --> %s", "_fp5", "fp5"));
			out.println(String.format("%16s --> %s", "_fp6", "fp6"));
			out.println(String.format("%16s --> %s", "_fp7", "fp7"));
			out.println(String.format("%16s --> %s", "_fp8", "fp8"));
			out.println(String.format("%16s --> %s", "_fp9", "fp9"));
			out.println(String.format("%16s --> %s", "_LR", "LR"));
			out.println(String.format("%16s --> %s", "_r0", "r0"));
			out.println(String.format("%16s --> %s", "_r1", "r1"));
			out.println(String.format("%16s --> %s", "_r10", "r10"));
			out.println(String.format("%16s --> %s", "_r11", "r11"));
			out.println(String.format("%16s --> %s", "_r12", "r12"));
			out.println(String.format("%16s --> %s", "_r13", "r13"));
			out.println(String.format("%16s --> %s", "_r14", "r14"));
			out.println(String.format("%16s --> %s", "_r15", "r15"));
			out.println(String.format("%16s --> %s", "_r16", "r16"));
			out.println(String.format("%16s --> %s", "_r17", "r17"));
			out.println(String.format("%16s --> %s", "_r18", "r18"));
			out.println(String.format("%16s --> %s", "_r19", "r19"));
			out.println(String.format("%16s --> %s", "_r2", "r2"));
			out.println(String.format("%16s --> %s", "_r20", "r20"));
			out.println(String.format("%16s --> %s", "_r21", "r21"));
			out.println(String.format("%16s --> %s", "_r22", "r22"));
			out.println(String.format("%16s --> %s", "_r23", "r23"));
			out.println(String.format("%16s --> %s", "_r24", "r24"));
			out.println(String.format("%16s --> %s", "_r25", "r25"));
			out.println(String.format("%16s --> %s", "_r26", "r26"));
			out.println(String.format("%16s --> %s", "_r27", "r27"));
			out.println(String.format("%16s --> %s", "_r28", "r28"));
			out.println(String.format("%16s --> %s", "_r29", "r29"));
			out.println(String.format("%16s --> %s", "_r3", "r3"));
			out.println(String.format("%16s --> %s", "_r30", "r30"));
			out.println(String.format("%16s --> %s", "_r31", "r31"));
			out.println(String.format("%16s --> %s", "_r4", "r4"));
			out.println(String.format("%16s --> %s", "_r5", "r5"));
			out.println(String.format("%16s --> %s", "_r6", "r6"));
			out.println(String.format("%16s --> %s", "_r7", "r7"));
			out.println(String.format("%16s --> %s", "_r8", "r8"));
			out.println(String.format("%16s --> %s", "_r9", "r9"));
			out.println(String.format("%16s --> %s", "_XER", "XER"));
			out.println(String.format("%16s --> %s", "jit_cr", "CR"));
			out.println(String.format("%16s --> %s", "jit_fpr0", "fp0"));
			out.println(String.format("%16s --> %s", "jit_fpr1", "fp1"));
			out.println(String.format("%16s --> %s", "jit_fpr10", "fp10"));
			out.println(String.format("%16s --> %s", "jit_fpr11", "fp11"));
			out.println(String.format("%16s --> %s", "jit_fpr12", "fp12"));
			out.println(String.format("%16s --> %s", "jit_fpr13", "fp13"));
			out.println(String.format("%16s --> %s", "jit_fpr14", "fp14"));
			out.println(String.format("%16s --> %s", "jit_fpr15", "fp15"));
			out.println(String.format("%16s --> %s", "jit_fpr16", "fp16"));
			out.println(String.format("%16s --> %s", "jit_fpr17", "fp17"));
			out.println(String.format("%16s --> %s", "jit_fpr18", "fp18"));
			out.println(String.format("%16s --> %s", "jit_fpr19", "fp19"));
			out.println(String.format("%16s --> %s", "jit_fpr2", "fp2"));
			out.println(String.format("%16s --> %s", "jit_fpr20", "fp20"));
			out.println(String.format("%16s --> %s", "jit_fpr21", "fp21"));
			out.println(String.format("%16s --> %s", "jit_fpr22", "fp22"));
			out.println(String.format("%16s --> %s", "jit_fpr23", "fp23"));
			out.println(String.format("%16s --> %s", "jit_fpr24", "fp24"));
			out.println(String.format("%16s --> %s", "jit_fpr25", "fp25"));
			out.println(String.format("%16s --> %s", "jit_fpr26", "fp26"));
			out.println(String.format("%16s --> %s", "jit_fpr27", "fp27"));
			out.println(String.format("%16s --> %s", "jit_fpr28", "fp28"));
			out.println(String.format("%16s --> %s", "jit_fpr29", "fp29"));
			out.println(String.format("%16s --> %s", "jit_fpr3", "fp3"));
			out.println(String.format("%16s --> %s", "jit_fpr30", "fp30"));
			out.println(String.format("%16s --> %s", "jit_fpr31", "fp31"));
			out.println(String.format("%16s --> %s", "jit_fpr4", "fp4"));
			out.println(String.format("%16s --> %s", "jit_fpr5", "fp5"));
			out.println(String.format("%16s --> %s", "jit_fpr6", "fp6"));
			out.println(String.format("%16s --> %s", "jit_fpr7", "fp7"));
			out.println(String.format("%16s --> %s", "jit_fpr8", "fp8"));
			out.println(String.format("%16s --> %s", "jit_fpr9", "fp9"));
			out.println(String.format("%16s --> %s", "jit_lr", "LR"));
			out.println(String.format("%16s --> %s", "jit_r0", "r0"));
			out.println(String.format("%16s --> %s", "jit_r1", "r1"));
			out.println(String.format("%16s --> %s", "jit_r10", "r10"));
			out.println(String.format("%16s --> %s", "jit_r11", "r11"));
			out.println(String.format("%16s --> %s", "jit_r12", "r12"));
			out.println(String.format("%16s --> %s", "jit_r13", "r13"));
			out.println(String.format("%16s --> %s", "jit_r14", "r14"));
			out.println(String.format("%16s --> %s", "jit_r15", "r15"));
			out.println(String.format("%16s --> %s", "jit_r16", "r16"));
			out.println(String.format("%16s --> %s", "jit_r17", "r17"));
			out.println(String.format("%16s --> %s", "jit_r18", "r18"));
			out.println(String.format("%16s --> %s", "jit_r19", "r19"));
			out.println(String.format("%16s --> %s", "jit_r2", "r2"));
			out.println(String.format("%16s --> %s", "jit_r20", "r20"));
			out.println(String.format("%16s --> %s", "jit_r21", "r21"));
			out.println(String.format("%16s --> %s", "jit_r22", "r22"));
			out.println(String.format("%16s --> %s", "jit_r23", "r23"));
			out.println(String.format("%16s --> %s", "jit_r24", "r24"));
			out.println(String.format("%16s --> %s", "jit_r25", "r25"));
			out.println(String.format("%16s --> %s", "jit_r26", "r26"));
			out.println(String.format("%16s --> %s", "jit_r27", "r27"));
			out.println(String.format("%16s --> %s", "jit_r28", "r28"));
			out.println(String.format("%16s --> %s", "jit_r29", "r29"));
			out.println(String.format("%16s --> %s", "jit_r3", "r3"));
			out.println(String.format("%16s --> %s", "jit_r30", "r30"));
			out.println(String.format("%16s --> %s", "jit_r31", "r31"));
			out.println(String.format("%16s --> %s", "jit_r4", "r4"));
			out.println(String.format("%16s --> %s", "jit_r5", "r5"));
			out.println(String.format("%16s --> %s", "jit_r6", "r6"));
			out.println(String.format("%16s --> %s", "jit_r7", "r7"));
			out.println(String.format("%16s --> %s", "jit_r8", "r8"));
			out.println(String.format("%16s --> %s", "jit_r9", "r9"));
			out.println(String.format("%16s --> %s", "saved_fpr14", "fp14"));
			out.println(String.format("%16s --> %s", "saved_fpr15", "fp15"));
			out.println(String.format("%16s --> %s", "saved_fpr16", "fp16"));
			out.println(String.format("%16s --> %s", "saved_fpr17", "fp17"));
			out.println(String.format("%16s --> %s", "saved_fpr18", "fp18"));
			out.println(String.format("%16s --> %s", "saved_fpr19", "fp19"));
			out.println(String.format("%16s --> %s", "saved_fpr20", "fp20"));
			out.println(String.format("%16s --> %s", "saved_fpr21", "fp21"));
			out.println(String.format("%16s --> %s", "saved_fpr22", "fp22"));
			out.println(String.format("%16s --> %s", "saved_fpr23", "fp23"));
			out.println(String.format("%16s --> %s", "saved_fpr24", "fp24"));
			out.println(String.format("%16s --> %s", "saved_fpr25", "fp25"));
			out.println(String.format("%16s --> %s", "saved_fpr26", "fp26"));
			out.println(String.format("%16s --> %s", "saved_fpr27", "fp27"));
			out.println(String.format("%16s --> %s", "saved_fpr28", "fp28"));
			out.println(String.format("%16s --> %s", "saved_fpr29", "fp29"));
			out.println(String.format("%16s --> %s", "saved_fpr30", "fp30"));
			out.println(String.format("%16s --> %s", "saved_fpr31", "fp31"));
		}	
	}

	/**
	 * Prints registers for AIX 32 BIT 
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 */
	private static void printRegistersForAIX32BitPPC(int level, PrintStream out) 
	{
		if (level >= 1) {
			out.println(String.format("=========== Level %d Registers ===========\n", 1));
			out.println(String.format("%16s --> %s", "vmStruct", "r13"));
			out.println(String.format("%16s --> %s", "sp", "r14"));
			out.println(String.format("%16s --> %s", "arg0EA", "r21"));
			out.println(String.format("%16s --> %s", "pc", "r16"));
			out.println(String.format("%16s --> %s", "literals", "r17"));
		}
		if (level >= 2) {
			out.println(String.format("=========== Level %d Registers ===========\n", 2));
			out.println(String.format("%16s --> %s", "bytecodes", "r18"));
			out.println(String.format("%16s --> %s", "detailMessage", "r19"));
			out.println(String.format("%16s --> %s", "exceptionIndex", "r20"));
			out.println(String.format("%16s --> %s", "javaNewClass", "r19"));
			out.println(String.format("%16s --> %s", "messageNumber", "r21"));
			out.println(String.format("%16s --> %s", "methodHandle", "r21"));
			out.println(String.format("%16s --> %s", "moduleName", "r19"));
			out.println(String.format("%16s --> %s", "resolveBytecode", "r20"));
			out.println(String.format("%16s --> %s", "resolveIndex", "r19"));
			out.println(String.format("%16s --> %s", "resolveType", "r21"));
			out.println(String.format("%16s --> %s", "returnAddress", "r19"));
			out.println(String.format("%16s --> %s", "returnPoint", "r10"));
			out.println(String.format("%16s --> %s", "returnSP", "r20"));
			out.println(String.format("%16s --> %s", "saved_cr", "CR"));
			out.println(String.format("%16s --> %s", "sendArgs", "r19"));
			out.println(String.format("%16s --> %s", "sendMethod", "r20"));
			out.println(String.format("%16s --> %s", "sendReturn", "r21"));
			out.println(String.format("%16s --> %s", "syncObject", "r6"));
		}
		if (level >= 3) {
			out.println(String.format("=========== Level %d Registers ===========\n", 3));
			out.println(String.format("%16s --> %s", "asmAddr1", "r5"));
			out.println(String.format("%16s --> %s", "asmAddr2", "r6"));
			out.println(String.format("%16s --> %s", "asmAddr3", "r7"));
			out.println(String.format("%16s --> %s", "asmData1", "r8"));
			out.println(String.format("%16s --> %s", "asmData2", "r9"));
			out.println(String.format("%16s --> %s", "asmData3", "r10"));
			out.println(String.format("%16s --> %s", "CR", "CR"));
			out.println(String.format("%16s --> %s", "CTR", "CTR"));
			out.println(String.format("%16s --> %s", "destEA", "r28"));
			out.println(String.format("%16s --> %s", "inParm1", "r3"));
			out.println(String.format("%16s --> %s", "inParm2", "r4"));
			out.println(String.format("%16s --> %s", "inParm3", "r5"));
			out.println(String.format("%16s --> %s", "inParm4", "r6"));
			out.println(String.format("%16s --> %s", "inParm5", "r7"));
			out.println(String.format("%16s --> %s", "inParm6", "r8"));
			out.println(String.format("%16s --> %s", "inParm7", "r9"));
			out.println(String.format("%16s --> %s", "inParm8", "r10"));
			out.println(String.format("%16s --> %s", "LR", "LR"));
			out.println(String.format("%16s --> %s", "machineSP", "r1"));
			out.println(String.format("%16s --> %s", "oldSP", "r30"));
			out.println(String.format("%16s --> %s", "outParm1", "r3"));
			out.println(String.format("%16s --> %s", "outParm2", "r4"));
			out.println(String.format("%16s --> %s", "outParm3", "r5"));
			out.println(String.format("%16s --> %s", "outParm4", "r6"));
			out.println(String.format("%16s --> %s", "outParm5", "r7"));
			out.println(String.format("%16s --> %s", "outParm6", "r8"));
			out.println(String.format("%16s --> %s", "outParm7", "r9"));
			out.println(String.format("%16s --> %s", "outParm8", "r10"));
			out.println(String.format("%16s --> %s", "return64hiRead", "r3"));
			out.println(String.format("%16s --> %s", "return64hiWrite", "r3"));
			out.println(String.format("%16s --> %s", "return64loRead", "r4"));
			out.println(String.format("%16s --> %s", "return64loWrite", "r4"));
			out.println(String.format("%16s --> %s", "returnValue", "r3"));
			out.println(String.format("%16s --> %s", "sourceEA", "r31"));
			out.println(String.format("%16s --> %s", "stringCopy5", "r5"));
			out.println(String.format("%16s --> %s", "stringCopy6", "r6"));
			out.println(String.format("%16s --> %s", "stringCopy7", "r7"));
			out.println(String.format("%16s --> %s", "stringCopy8", "r8"));
			out.println(String.format("%16s --> %s", "stringCopyCount", "r9"));
			out.println(String.format("%16s --> %s", "stringCopyDest", "r3"));
			out.println(String.format("%16s --> %s", "stringCopySource", "r4"));
			out.println(String.format("%16s --> %s", "tempEA", "r27"));
			out.println(String.format("%16s --> %s", "TOC", "r2"));
			out.println(String.format("%16s --> %s", "XER", "XER"));
		}
		if (level >= 4) {
			out.println(String.format("=========== Level %d Registers ===========\n", 4));
			out.println(String.format("%16s --> %s", "_CR", "CR"));
			out.println(String.format("%16s --> %s", "_CTR", "CTR"));
			out.println(String.format("%16s --> %s", "_fp0", "fp0"));
			out.println(String.format("%16s --> %s", "_fp1", "fp1"));
			out.println(String.format("%16s --> %s", "_fp10", "fp10"));
			out.println(String.format("%16s --> %s", "_fp11", "fp11"));
			out.println(String.format("%16s --> %s", "_fp12", "fp12"));
			out.println(String.format("%16s --> %s", "_fp13", "fp13"));
			out.println(String.format("%16s --> %s", "_fp14", "fp14"));
			out.println(String.format("%16s --> %s", "_fp15", "fp15"));
			out.println(String.format("%16s --> %s", "_fp16", "fp16"));
			out.println(String.format("%16s --> %s", "_fp17", "fp17"));
			out.println(String.format("%16s --> %s", "_fp18", "fp18"));
			out.println(String.format("%16s --> %s", "_fp19", "fp19"));
			out.println(String.format("%16s --> %s", "_fp2", "fp2"));
			out.println(String.format("%16s --> %s", "_fp20", "fp20"));
			out.println(String.format("%16s --> %s", "_fp21", "fp21"));
			out.println(String.format("%16s --> %s", "_fp22", "fp22"));
			out.println(String.format("%16s --> %s", "_fp23", "fp23"));
			out.println(String.format("%16s --> %s", "_fp24", "fp24"));
			out.println(String.format("%16s --> %s", "_fp25", "fp25"));
			out.println(String.format("%16s --> %s", "_fp26", "fp26"));
			out.println(String.format("%16s --> %s", "_fp27", "fp27"));
			out.println(String.format("%16s --> %s", "_fp28", "fp28"));
			out.println(String.format("%16s --> %s", "_fp29", "fp29"));
			out.println(String.format("%16s --> %s", "_fp3", "fp3"));
			out.println(String.format("%16s --> %s", "_fp30", "fp30"));
			out.println(String.format("%16s --> %s", "_fp31", "fp31"));
			out.println(String.format("%16s --> %s", "_fp4", "fp4"));
			out.println(String.format("%16s --> %s", "_fp5", "fp5"));
			out.println(String.format("%16s --> %s", "_fp6", "fp6"));
			out.println(String.format("%16s --> %s", "_fp7", "fp7"));
			out.println(String.format("%16s --> %s", "_fp8", "fp8"));
			out.println(String.format("%16s --> %s", "_fp9", "fp9"));
			out.println(String.format("%16s --> %s", "_LR", "LR"));
			out.println(String.format("%16s --> %s", "_r0", "r0"));
			out.println(String.format("%16s --> %s", "_r1", "r1"));
			out.println(String.format("%16s --> %s", "_r10", "r10"));
			out.println(String.format("%16s --> %s", "_r11", "r11"));
			out.println(String.format("%16s --> %s", "_r12", "r12"));
			out.println(String.format("%16s --> %s", "_r13", "r13"));
			out.println(String.format("%16s --> %s", "_r14", "r14"));
			out.println(String.format("%16s --> %s", "_r15", "r15"));
			out.println(String.format("%16s --> %s", "_r16", "r16"));
			out.println(String.format("%16s --> %s", "_r17", "r17"));
			out.println(String.format("%16s --> %s", "_r18", "r18"));
			out.println(String.format("%16s --> %s", "_r19", "r19"));
			out.println(String.format("%16s --> %s", "_r2", "r2"));
			out.println(String.format("%16s --> %s", "_r20", "r20"));
			out.println(String.format("%16s --> %s", "_r21", "r21"));
			out.println(String.format("%16s --> %s", "_r22", "r22"));
			out.println(String.format("%16s --> %s", "_r23", "r23"));
			out.println(String.format("%16s --> %s", "_r24", "r24"));
			out.println(String.format("%16s --> %s", "_r25", "r25"));
			out.println(String.format("%16s --> %s", "_r26", "r26"));
			out.println(String.format("%16s --> %s", "_r27", "r27"));
			out.println(String.format("%16s --> %s", "_r28", "r28"));
			out.println(String.format("%16s --> %s", "_r29", "r29"));
			out.println(String.format("%16s --> %s", "_r3", "r3"));
			out.println(String.format("%16s --> %s", "_r30", "r30"));
			out.println(String.format("%16s --> %s", "_r31", "r31"));
			out.println(String.format("%16s --> %s", "_r4", "r4"));
			out.println(String.format("%16s --> %s", "_r5", "r5"));
			out.println(String.format("%16s --> %s", "_r6", "r6"));
			out.println(String.format("%16s --> %s", "_r7", "r7"));
			out.println(String.format("%16s --> %s", "_r8", "r8"));
			out.println(String.format("%16s --> %s", "_r9", "r9"));
			out.println(String.format("%16s --> %s", "_XER", "XER"));
			out.println(String.format("%16s --> %s", "jit_cr", "CR"));
			out.println(String.format("%16s --> %s", "jit_fpr0", "fp0"));
			out.println(String.format("%16s --> %s", "jit_fpr1", "fp1"));
			out.println(String.format("%16s --> %s", "jit_fpr10", "fp10"));
			out.println(String.format("%16s --> %s", "jit_fpr11", "fp11"));
			out.println(String.format("%16s --> %s", "jit_fpr12", "fp12"));
			out.println(String.format("%16s --> %s", "jit_fpr13", "fp13"));
			out.println(String.format("%16s --> %s", "jit_fpr14", "fp14"));
			out.println(String.format("%16s --> %s", "jit_fpr15", "fp15"));
			out.println(String.format("%16s --> %s", "jit_fpr16", "fp16"));
			out.println(String.format("%16s --> %s", "jit_fpr17", "fp17"));
			out.println(String.format("%16s --> %s", "jit_fpr18", "fp18"));
			out.println(String.format("%16s --> %s", "jit_fpr19", "fp19"));
			out.println(String.format("%16s --> %s", "jit_fpr2", "fp2"));
			out.println(String.format("%16s --> %s", "jit_fpr20", "fp20"));
			out.println(String.format("%16s --> %s", "jit_fpr21", "fp21"));
			out.println(String.format("%16s --> %s", "jit_fpr22", "fp22"));
			out.println(String.format("%16s --> %s", "jit_fpr23", "fp23"));
			out.println(String.format("%16s --> %s", "jit_fpr24", "fp24"));
			out.println(String.format("%16s --> %s", "jit_fpr25", "fp25"));
			out.println(String.format("%16s --> %s", "jit_fpr26", "fp26"));
			out.println(String.format("%16s --> %s", "jit_fpr27", "fp27"));
			out.println(String.format("%16s --> %s", "jit_fpr28", "fp28"));
			out.println(String.format("%16s --> %s", "jit_fpr29", "fp29"));
			out.println(String.format("%16s --> %s", "jit_fpr3", "fp3"));
			out.println(String.format("%16s --> %s", "jit_fpr30", "fp30"));
			out.println(String.format("%16s --> %s", "jit_fpr31", "fp31"));
			out.println(String.format("%16s --> %s", "jit_fpr4", "fp4"));
			out.println(String.format("%16s --> %s", "jit_fpr5", "fp5"));
			out.println(String.format("%16s --> %s", "jit_fpr6", "fp6"));
			out.println(String.format("%16s --> %s", "jit_fpr7", "fp7"));
			out.println(String.format("%16s --> %s", "jit_fpr8", "fp8"));
			out.println(String.format("%16s --> %s", "jit_fpr9", "fp9"));
			out.println(String.format("%16s --> %s", "jit_lr", "LR"));
			out.println(String.format("%16s --> %s", "jit_r0", "r0"));
			out.println(String.format("%16s --> %s", "jit_r1", "r1"));
			out.println(String.format("%16s --> %s", "jit_r10", "r10"));
			out.println(String.format("%16s --> %s", "jit_r11", "r11"));
			out.println(String.format("%16s --> %s", "jit_r12", "r12"));
			out.println(String.format("%16s --> %s", "jit_r13", "r13"));
			out.println(String.format("%16s --> %s", "jit_r14", "r14"));
			out.println(String.format("%16s --> %s", "jit_r15", "r15"));
			out.println(String.format("%16s --> %s", "jit_r16", "r16"));
			out.println(String.format("%16s --> %s", "jit_r17", "r17"));
			out.println(String.format("%16s --> %s", "jit_r18", "r18"));
			out.println(String.format("%16s --> %s", "jit_r19", "r19"));
			out.println(String.format("%16s --> %s", "jit_r2", "r2"));
			out.println(String.format("%16s --> %s", "jit_r20", "r20"));
			out.println(String.format("%16s --> %s", "jit_r21", "r21"));
			out.println(String.format("%16s --> %s", "jit_r22", "r22"));
			out.println(String.format("%16s --> %s", "jit_r23", "r23"));
			out.println(String.format("%16s --> %s", "jit_r24", "r24"));
			out.println(String.format("%16s --> %s", "jit_r25", "r25"));
			out.println(String.format("%16s --> %s", "jit_r26", "r26"));
			out.println(String.format("%16s --> %s", "jit_r27", "r27"));
			out.println(String.format("%16s --> %s", "jit_r28", "r28"));
			out.println(String.format("%16s --> %s", "jit_r29", "r29"));
			out.println(String.format("%16s --> %s", "jit_r3", "r3"));
			out.println(String.format("%16s --> %s", "jit_r30", "r30"));
			out.println(String.format("%16s --> %s", "jit_r31", "r31"));
			out.println(String.format("%16s --> %s", "jit_r4", "r4"));
			out.println(String.format("%16s --> %s", "jit_r5", "r5"));
			out.println(String.format("%16s --> %s", "jit_r6", "r6"));
			out.println(String.format("%16s --> %s", "jit_r7", "r7"));
			out.println(String.format("%16s --> %s", "jit_r8", "r8"));
			out.println(String.format("%16s --> %s", "jit_r9", "r9"));
			out.println(String.format("%16s --> %s", "saved_fpr14", "fp14"));
			out.println(String.format("%16s --> %s", "saved_fpr15", "fp15"));
			out.println(String.format("%16s --> %s", "saved_fpr16", "fp16"));
			out.println(String.format("%16s --> %s", "saved_fpr17", "fp17"));
			out.println(String.format("%16s --> %s", "saved_fpr18", "fp18"));
			out.println(String.format("%16s --> %s", "saved_fpr19", "fp19"));
			out.println(String.format("%16s --> %s", "saved_fpr20", "fp20"));
			out.println(String.format("%16s --> %s", "saved_fpr21", "fp21"));
			out.println(String.format("%16s --> %s", "saved_fpr22", "fp22"));
			out.println(String.format("%16s --> %s", "saved_fpr23", "fp23"));
			out.println(String.format("%16s --> %s", "saved_fpr24", "fp24"));
			out.println(String.format("%16s --> %s", "saved_fpr25", "fp25"));
			out.println(String.format("%16s --> %s", "saved_fpr26", "fp26"));
			out.println(String.format("%16s --> %s", "saved_fpr27", "fp27"));
			out.println(String.format("%16s --> %s", "saved_fpr28", "fp28"));
			out.println(String.format("%16s --> %s", "saved_fpr29", "fp29"));
			out.println(String.format("%16s --> %s", "saved_fpr30", "fp30"));
			out.println(String.format("%16s --> %s", "saved_fpr31", "fp31"));
		}
	}

	/**
	 * Prints registers for LINUX 64 BIT AMD64 
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 */
	private static void printRegistersForLinux64BitAMD64(int level, PrintStream out) 
	{
		if (level >= 1) {
			out.println(String.format("=========== Level %d Registers ===========", 1));
			out.println(String.format("%16s --> %s", "vmStruct", "%rbp"));
			out.println(String.format("%16s --> %s", "sp", "%rsp"));
			out.println(String.format("%16s --> %s", "arg0EA", "%rcx"));
			out.println(String.format("%16s --> %s", "pc", "%rsi"));
			out.println(String.format("%16s --> %s", "literals", "%rbx"));
		}
		if (level >= 2) {
			out.println(String.format("=========== Level %d Registers ===========\n", 2));
			out.println(String.format("%16s --> %s", "allButLowByteZero", "%rax"));
			out.println(String.format("%16s --> %s", "bytecodes", "%r14"));
			out.println(String.format("%16s --> %s", "cacheIndex", "%rcx"));
			out.println(String.format("%16s --> %s", "callInAddress", "%rsi"));
			out.println(String.format("%16s --> %s", "detailMessage", "%rdi"));
			out.println(String.format("%16s --> %s", "exceptionIndex", "%rax"));
			out.println(String.format("%16s --> %s", "javaNewClass", "%rdi"));
			out.println(String.format("%16s --> %s", "messageNumber", "%rcx"));
			out.println(String.format("%16s --> %s", "methodHandle", "%rcx"));
			out.println(String.format("%16s --> %s", "moduleName", "%rdi"));
			out.println(String.format("%16s --> %s", "resolveBytecode", "%rax"));
			out.println(String.format("%16s --> %s", "resolveIndex", "%rdi"));
			out.println(String.format("%16s --> %s", "resolveType", "%rcx"));
			out.println(String.format("%16s --> %s", "returnAddress", "%rdi"));
			out.println(String.format("%16s --> %s", "returnPoint", "%rdi"));
			out.println(String.format("%16s --> %s", "returnSP", "%rax"));
			out.println(String.format("%16s --> %s", "sendArgs", "%rdi"));
			out.println(String.format("%16s --> %s", "sendMethod", "%rax"));
			out.println(String.format("%16s --> %s", "sendReturn", "%rcx"));
			out.println(String.format("%16s --> %s", "signalHandlerNewSP", "%rax"));
			out.println(String.format("%16s --> %s", "signalHandlerOldSP", "%r12"));
			out.println(String.format("%16s --> %s", "signalHandlerParm1", "%rdi"));
			out.println(String.format("%16s --> %s", "signalHandlerParm2", "%rsi"));
			out.println(String.format("%16s --> %s", "signalHandlerParm3", "%rdx"));
			out.println(String.format("%16s --> %s", "signalHandlerParm4", "%rcx"));
			out.println(String.format("%16s --> %s", "syncObject", "%rcx"));
		}
		if (level >= 3) {
			out.println(String.format("=========== Level %d Registers ===========\n", 3));
			out.println(String.format("%16s --> %s", "%", "%rip"));
			out.println(String.format("%16s --> %s", "%eax", "%rax"));
			out.println(String.format("%16s --> %s", "%ebp", "%rbp"));
			out.println(String.format("%16s --> %s", "%ebx", "%rbx"));
			out.println(String.format("%16s --> %s", "%ecx", "%rcx"));
			out.println(String.format("%16s --> %s", "%edi", "%rdi"));
			out.println(String.format("%16s --> %s", "%edx", "%rdx"));
			out.println(String.format("%16s --> %s", "%esi", "%rsi"));
			out.println(String.format("%16s --> %s", "%esp", "%rsp"));
			out.println(String.format("%16s --> %s", "%r10d", "%r10"));
			out.println(String.format("%16s --> %s", "%r11d", "%r11"));
			out.println(String.format("%16s --> %s", "%r12d", "%r12"));
			out.println(String.format("%16s --> %s", "%r13d", "%r13"));
			out.println(String.format("%16s --> %s", "%r14d", "%r14"));
			out.println(String.format("%16s --> %s", "%r15d", "%r15"));
			out.println(String.format("%16s --> %s", "%r8d", "%r8"));
			out.println(String.format("%16s --> %s", "%r9d", "%r9"));
			out.println(String.format("%16s --> %s", "asmAddr1", "%rax"));
			out.println(String.format("%16s --> %s", "asmAddr2", "%rcx"));
			out.println(String.format("%16s --> %s", "asmAddr3", "%rbx"));
			out.println(String.format("%16s --> %s", "asmData1", "%rdx"));
			out.println(String.format("%16s --> %s", "asmData2", "%rsi"));
			out.println(String.format("%16s --> %s", "asmData3", "%rdi"));
			out.println(String.format("%16s --> %s", "cmpxchgValue", "%rax"));
			out.println(String.format("%16s --> %s", "divideQuotient", "%rax"));
			out.println(String.format("%16s --> %s", "divideRemainder", "%rdx"));
			out.println(String.format("%16s --> %s", "floatStatusWord", "%rax"));
			out.println(String.format("%16s --> %s", "fpParmCount", "%rax"));
			out.println(String.format("%16s --> %s", "inParm1", "%rdi"));
			out.println(String.format("%16s --> %s", "inParm2", "%rsi"));
			out.println(String.format("%16s --> %s", "inParm3", "%rdx"));
			out.println(String.format("%16s --> %s", "inParm4", "%rcx"));
			out.println(String.format("%16s --> %s", "inParm5", "%r8"));
			out.println(String.format("%16s --> %s", "inParm6", "%r9"));
			out.println(String.format("%16s --> %s", "instructionPointer", "%rip"));
			out.println(String.format("%16s --> %s", "machineBP", "%rbp"));
			out.println(String.format("%16s --> %s", "machineSP", "%rsp"));
			out.println(String.format("%16s --> %s", "memCount", "%rcx"));
			out.println(String.format("%16s --> %s", "memDest", "%rdi"));
			out.println(String.format("%16s --> %s", "memSource", "%rsi"));
			out.println(String.format("%16s --> %s", "memValue", "%rax"));
			out.println(String.format("%16s --> %s", "outParm1", "%rdi"));
			out.println(String.format("%16s --> %s", "outParm2", "%rsi"));
			out.println(String.format("%16s --> %s", "outParm3", "%rdx"));
			out.println(String.format("%16s --> %s", "outParm4", "%rcx"));
			out.println(String.format("%16s --> %s", "outParm5", "%r8"));
			out.println(String.format("%16s --> %s", "outParm6", "%r9"));
			out.println(String.format("%16s --> %s", "rdtscHi", "%rdx"));
			out.println(String.format("%16s --> %s", "rdtscLo", "%rax"));
			out.println(String.format("%16s --> %s", "return64hiRead", "%rdx"));
			out.println(String.format("%16s --> %s", "return64hiWrite", "%rdx"));
			out.println(String.format("%16s --> %s", "return64loRead", "%rax"));
			out.println(String.format("%16s --> %s", "return64loWrite", "%rax"));
			out.println(String.format("%16s --> %s", "returnValue", "%rax"));
			out.println(String.format("%16s --> %s", "shiftSource", "%rcx"));
		}
		if (level >= 4) {
			out.println(String.format("=========== Level %d Registers ===========\n", 4));
			out.println(String.format("%16s --> %s", "_", "%xmm15"));
			out.println(String.format("%16s --> %s", "_%r10", "%r10"));
			out.println(String.format("%16s --> %s", "_%r11", "%r11"));
			out.println(String.format("%16s --> %s", "_%r12", "%r12"));
			out.println(String.format("%16s --> %s", "_%r13", "%r13"));
			out.println(String.format("%16s --> %s", "_%r14", "%r14"));
			out.println(String.format("%16s --> %s", "_%r15", "%r15"));
			out.println(String.format("%16s --> %s", "_%r8", "%r8"));
			out.println(String.format("%16s --> %s", "_%r9", "%r9"));
			out.println(String.format("%16s --> %s", "_%rax", "%rax"));
			out.println(String.format("%16s --> %s", "_%rbp", "%rbp"));
			out.println(String.format("%16s --> %s", "_%rbx", "%rbx"));
			out.println(String.format("%16s --> %s", "_%rcx", "%rcx"));
			out.println(String.format("%16s --> %s", "_%rdi", "%rdi"));
			out.println(String.format("%16s --> %s", "_%rdx", "%rdx"));
			out.println(String.format("%16s --> %s", "_%rip", "%rip"));
			out.println(String.format("%16s --> %s", "_%rsi", "%rsi"));
			out.println(String.format("%16s --> %s", "_%rsp", "%rsp"));
			out.println(String.format("%16s --> %s", "_%xmm0", "%xmm0"));
			out.println(String.format("%16s --> %s", "_%xmm1", "%xmm1"));
			out.println(String.format("%16s --> %s", "_%xmm10", "%xmm10"));
			out.println(String.format("%16s --> %s", "_%xmm11", "%xmm11"));
			out.println(String.format("%16s --> %s", "_%xmm12", "%xmm12"));
			out.println(String.format("%16s --> %s", "_%xmm13", "%xmm13"));
			out.println(String.format("%16s --> %s", "_%xmm14", "%xmm14"));
			out.println(String.format("%16s --> %s", "_%xmm15", "%xmm15"));
			out.println(String.format("%16s --> %s", "_%xmm2", "%xmm2"));
			out.println(String.format("%16s --> %s", "_%xmm3", "%xmm3"));
			out.println(String.format("%16s --> %s", "_%xmm4", "%xmm4"));
			out.println(String.format("%16s --> %s", "_%xmm5", "%xmm5"));
			out.println(String.format("%16s --> %s", "_%xmm6", "%xmm6"));
			out.println(String.format("%16s --> %s", "_%xmm7", "%xmm7"));
			out.println(String.format("%16s --> %s", "_%xmm8", "%xmm8"));
			out.println(String.format("%16s --> %s", "_%xmm9", "%xmm9"));
			out.println(String.format("%16s --> %s", "_EAX", "%rax"));
			out.println(String.format("%16s --> %s", "_EBP", "%rbp"));
			out.println(String.format("%16s --> %s", "_EBX", "%rbx"));
			out.println(String.format("%16s --> %s", "_ECX", "%rcx"));
			out.println(String.format("%16s --> %s", "_EDI", "%rdi"));
			out.println(String.format("%16s --> %s", "_EDX", "%rdx"));
			out.println(String.format("%16s --> %s", "_ESI", "%rsi"));
			out.println(String.format("%16s --> %s", "_ESP", "%rsp"));
			out.println(String.format("%16s --> %s", "_R10", "%r10"));
			out.println(String.format("%16s --> %s", "_R10D", "%r10"));
			out.println(String.format("%16s --> %s", "_R11", "%r11"));
			out.println(String.format("%16s --> %s", "_R11D", "%r11"));
			out.println(String.format("%16s --> %s", "_R12", "%r12"));
			out.println(String.format("%16s --> %s", "_R12D", "%r12"));
			out.println(String.format("%16s --> %s", "_R13", "%r13"));
			out.println(String.format("%16s --> %s", "_R13D", "%r13"));
			out.println(String.format("%16s --> %s", "_R14", "%r14"));
			out.println(String.format("%16s --> %s", "_R14D", "%r14"));
			out.println(String.format("%16s --> %s", "_R15", "%r15"));
			out.println(String.format("%16s --> %s", "_R15D", "%r15"));
			out.println(String.format("%16s --> %s", "_R8", "%r8"));
			out.println(String.format("%16s --> %s", "_R8D", "%r8"));
			out.println(String.format("%16s --> %s", "_R9", "%r9"));
			out.println(String.format("%16s --> %s", "_R9D", "%r9"));
			out.println(String.format("%16s --> %s", "_RAX", "%rax"));
			out.println(String.format("%16s --> %s", "_RBP", "%rbp"));
			out.println(String.format("%16s --> %s", "_RBX", "%rbx"));
			out.println(String.format("%16s --> %s", "_RCX", "%rcx"));
			out.println(String.format("%16s --> %s", "_RDI", "%rdi"));
			out.println(String.format("%16s --> %s", "_RDX", "%rdx"));
			out.println(String.format("%16s --> %s", "_RIP", "%rip"));
			out.println(String.format("%16s --> %s", "_RSI", "%rsi"));
			out.println(String.format("%16s --> %s", "_RSP", "%rsp"));
			out.println(String.format("%16s --> %s", "_xmm0", "%xmm0"));
			out.println(String.format("%16s --> %s", "_xmm1", "%xmm1"));
			out.println(String.format("%16s --> %s", "_xmm10", "%xmm10"));
			out.println(String.format("%16s --> %s", "_xmm11", "%xmm11"));
			out.println(String.format("%16s --> %s", "_xmm12", "%xmm12"));
			out.println(String.format("%16s --> %s", "_xmm13", "%xmm13"));
			out.println(String.format("%16s --> %s", "_xmm14", "%xmm14"));
			out.println(String.format("%16s --> %s", "_xmm15", "%xmm15"));
			out.println(String.format("%16s --> %s", "_xmm2", "%xmm2"));
			out.println(String.format("%16s --> %s", "_xmm3", "%xmm3"));
			out.println(String.format("%16s --> %s", "_xmm4", "%xmm4"));
			out.println(String.format("%16s --> %s", "_xmm5", "%xmm5"));
			out.println(String.format("%16s --> %s", "_xmm6", "%xmm6"));
			out.println(String.format("%16s --> %s", "_xmm7", "%xmm7"));
			out.println(String.format("%16s --> %s", "_xmm8", "%xmm8"));
			out.println(String.format("%16s --> %s", "_xmm9", "%xmm9"));
			out.println(String.format("%16s --> %s", "jit_fpr0", "%xmm0"));
			out.println(String.format("%16s --> %s", "jit_fpr1", "%xmm1"));
			out.println(String.format("%16s --> %s", "jit_fpr10", "%xmm10"));
			out.println(String.format("%16s --> %s", "jit_fpr11", "%xmm11"));
			out.println(String.format("%16s --> %s", "jit_fpr12", "%xmm12"));
			out.println(String.format("%16s --> %s", "jit_fpr13", "%xmm13"));
			out.println(String.format("%16s --> %s", "jit_fpr14", "%xmm14"));
			out.println(String.format("%16s --> %s", "jit_fpr15", "%xmm15"));
			out.println(String.format("%16s --> %s", "jit_fpr2", "%xmm2"));
			out.println(String.format("%16s --> %s", "jit_fpr3", "%xmm3"));
			out.println(String.format("%16s --> %s", "jit_fpr4", "%xmm4"));
			out.println(String.format("%16s --> %s", "jit_fpr5", "%xmm5"));
			out.println(String.format("%16s --> %s", "jit_fpr6", "%xmm6"));
			out.println(String.format("%16s --> %s", "jit_fpr7", "%xmm7"));
			out.println(String.format("%16s --> %s", "jit_fpr8", "%xmm8"));
			out.println(String.format("%16s --> %s", "jit_fpr9", "%xmm9"));
			out.println(String.format("%16s --> %s", "jit_r10", "%r10"));
			out.println(String.format("%16s --> %s", "jit_r11", "%r11"));
			out.println(String.format("%16s --> %s", "jit_r12", "%r12"));
			out.println(String.format("%16s --> %s", "jit_r13", "%r13"));
			out.println(String.format("%16s --> %s", "jit_r14", "%r14"));
			out.println(String.format("%16s --> %s", "jit_r15", "%r15"));
			out.println(String.format("%16s --> %s", "jit_r8", "%r8"));
			out.println(String.format("%16s --> %s", "jit_r9", "%r9"));
			out.println(String.format("%16s --> %s", "jit_rax", "%rax"));
			out.println(String.format("%16s --> %s", "jit_rbx", "%rbx"));
			out.println(String.format("%16s --> %s", "jit_rcx", "%rcx"));
			out.println(String.format("%16s --> %s", "jit_rdi", "%rdi"));
			out.println(String.format("%16s --> %s", "jit_rdx", "%rdx"));
			out.println(String.format("%16s --> %s", "jit_rsi", "%rsi"));
		}
	}
		
	/**
	 * Prints registers for LINUX 64 BIT PPC
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 */
	private static void printRegistersForLinux64BitPPC(int level, PrintStream out) 
	{
		if (level >= 1) {
			out.println(String.format("=========== Level %d Registers ===========\n", 1));
			out.println(String.format("%16s --> %s", "vmStruct", "r15"));
			out.println(String.format("%16s --> %s", "sp", "r14"));
			out.println(String.format("%16s --> %s", "arg0EA", "r21"));
			out.println(String.format("%16s --> %s", "pc", "r16"));
			out.println(String.format("%16s --> %s", "literals", "r17"));
		}
		if (level >= 2) {
			out.println(String.format("=========== Level %d Registers ===========\n", 2));
			out.println(String.format("%16s --> %s", "bytecodes", "r18"));
			out.println(String.format("%16s --> %s", "detailMessage", "r19"));
			out.println(String.format("%16s --> %s", "exceptionIndex", "r20"));
			out.println(String.format("%16s --> %s", "javaNewClass", "r19"));
			out.println(String.format("%16s --> %s", "messageNumber", "r21"));
			out.println(String.format("%16s --> %s", "methodHandle", "r21"));
			out.println(String.format("%16s --> %s", "moduleName", "r19"));
			out.println(String.format("%16s --> %s", "resolveBytecode", "r20"));
			out.println(String.format("%16s --> %s", "resolveIndex", "r19"));
			out.println(String.format("%16s --> %s", "resolveType", "r21"));
			out.println(String.format("%16s --> %s", "returnAddress", "r19"));
			out.println(String.format("%16s --> %s", "returnPoint", "r10"));
			out.println(String.format("%16s --> %s", "returnSP", "r20"));
			out.println(String.format("%16s --> %s", "saved_cr", "CR"));
			out.println(String.format("%16s --> %s", "sendArgs", "r19"));
			out.println(String.format("%16s --> %s", "sendMethod", "r20"));
			out.println(String.format("%16s --> %s", "sendReturn", "r21"));
			out.println(String.format("%16s --> %s", "syncObject", "r6"));
		}
		if (level >= 3) {
			out.println(String.format("=========== Level %d Registers ===========\n", 3));
			out.println(String.format("%16s --> %s", "64BitReservedThrds", "r13"));
			out.println(String.format("%16s --> %s", "asmAddr1", "r5"));
			out.println(String.format("%16s --> %s", "asmAddr2", "r6"));
			out.println(String.format("%16s --> %s", "asmAddr3", "r7"));
			out.println(String.format("%16s --> %s", "asmData1", "r8"));
			out.println(String.format("%16s --> %s", "asmData2", "r9"));
			out.println(String.format("%16s --> %s", "asmData3", "r10"));
			out.println(String.format("%16s --> %s", "CR", "CR"));
			out.println(String.format("%16s --> %s", "CTR", "CTR"));
			out.println(String.format("%16s --> %s", "destEA", "r28"));
			out.println(String.format("%16s --> %s", "inParm1", "r3"));
			out.println(String.format("%16s --> %s", "inParm2", "r4"));
			out.println(String.format("%16s --> %s", "inParm3", "r5"));
			out.println(String.format("%16s --> %s", "inParm4", "r6"));
			out.println(String.format("%16s --> %s", "inParm5", "r7"));
			out.println(String.format("%16s --> %s", "inParm6", "r8"));
			out.println(String.format("%16s --> %s", "inParm7", "r9"));
			out.println(String.format("%16s --> %s", "inParm8", "r10"));
			out.println(String.format("%16s --> %s", "LR", "LR"));
			out.println(String.format("%16s --> %s", "machineSP", "r1"));
			out.println(String.format("%16s --> %s", "oldSP", "r30"));
			out.println(String.format("%16s --> %s", "outParm1", "r3"));
			out.println(String.format("%16s --> %s", "outParm2", "r4"));
			out.println(String.format("%16s --> %s", "outParm3", "r5"));
			out.println(String.format("%16s --> %s", "outParm4", "r6"));
			out.println(String.format("%16s --> %s", "outParm5", "r7"));
			out.println(String.format("%16s --> %s", "outParm6", "r8"));
			out.println(String.format("%16s --> %s", "outParm7", "r9"));
			out.println(String.format("%16s --> %s", "outParm8", "r10"));
			out.println(String.format("%16s --> %s", "return64hiRead", "r3"));
			out.println(String.format("%16s --> %s", "return64hiWrite", "r3"));
			out.println(String.format("%16s --> %s", "return64loRead", "r4"));
			out.println(String.format("%16s --> %s", "return64loWrite", "r4"));
			out.println(String.format("%16s --> %s", "returnValue", "r3"));
			out.println(String.format("%16s --> %s", "sourceEA", "r31"));
			out.println(String.format("%16s --> %s", "stringCopy5", "r5"));
			out.println(String.format("%16s --> %s", "stringCopy6", "r6"));
			out.println(String.format("%16s --> %s", "stringCopy7", "r7"));
			out.println(String.format("%16s --> %s", "stringCopy8", "r8"));
			out.println(String.format("%16s --> %s", "stringCopyCount", "r9"));
			out.println(String.format("%16s --> %s", "stringCopyDest", "r3"));
			out.println(String.format("%16s --> %s", "stringCopySource", "r4"));
			out.println(String.format("%16s --> %s", "tempEA", "r27"));
			out.println(String.format("%16s --> %s", "TOC", "r2"));
			out.println(String.format("%16s --> %s", "XER", "XER"));
		}
		if (level >= 4) {
			out.println(String.format("=========== Level %d Registers ===========\n", 4));
			out.println(String.format("%16s --> %s", "_CR", "CR"));
			out.println(String.format("%16s --> %s", "_CTR", "CTR"));
			out.println(String.format("%16s --> %s", "_fp0", "fp0"));
			out.println(String.format("%16s --> %s", "_fp1", "fp1"));
			out.println(String.format("%16s --> %s", "_fp10", "fp10"));
			out.println(String.format("%16s --> %s", "_fp11", "fp11"));
			out.println(String.format("%16s --> %s", "_fp12", "fp12"));
			out.println(String.format("%16s --> %s", "_fp13", "fp13"));
			out.println(String.format("%16s --> %s", "_fp14", "fp14"));
			out.println(String.format("%16s --> %s", "_fp15", "fp15"));
			out.println(String.format("%16s --> %s", "_fp16", "fp16"));
			out.println(String.format("%16s --> %s", "_fp17", "fp17"));
			out.println(String.format("%16s --> %s", "_fp18", "fp18"));
			out.println(String.format("%16s --> %s", "_fp19", "fp19"));
			out.println(String.format("%16s --> %s", "_fp2", "fp2"));
			out.println(String.format("%16s --> %s", "_fp20", "fp20"));
			out.println(String.format("%16s --> %s", "_fp21", "fp21"));
			out.println(String.format("%16s --> %s", "_fp22", "fp22"));
			out.println(String.format("%16s --> %s", "_fp23", "fp23"));
			out.println(String.format("%16s --> %s", "_fp24", "fp24"));
			out.println(String.format("%16s --> %s", "_fp25", "fp25"));
			out.println(String.format("%16s --> %s", "_fp26", "fp26"));
			out.println(String.format("%16s --> %s", "_fp27", "fp27"));
			out.println(String.format("%16s --> %s", "_fp28", "fp28"));
			out.println(String.format("%16s --> %s", "_fp29", "fp29"));
			out.println(String.format("%16s --> %s", "_fp3", "fp3"));
			out.println(String.format("%16s --> %s", "_fp30", "fp30"));
			out.println(String.format("%16s --> %s", "_fp31", "fp31"));
			out.println(String.format("%16s --> %s", "_fp4", "fp4"));
			out.println(String.format("%16s --> %s", "_fp5", "fp5"));
			out.println(String.format("%16s --> %s", "_fp6", "fp6"));
			out.println(String.format("%16s --> %s", "_fp7", "fp7"));
			out.println(String.format("%16s --> %s", "_fp8", "fp8"));
			out.println(String.format("%16s --> %s", "_fp9", "fp9"));
			out.println(String.format("%16s --> %s", "_LR", "LR"));
			out.println(String.format("%16s --> %s", "_r0", "r0"));
			out.println(String.format("%16s --> %s", "_r1", "r1"));
			out.println(String.format("%16s --> %s", "_r10", "r10"));
			out.println(String.format("%16s --> %s", "_r11", "r11"));
			out.println(String.format("%16s --> %s", "_r12", "r12"));
			out.println(String.format("%16s --> %s", "_r13", "r13"));
			out.println(String.format("%16s --> %s", "_r14", "r14"));
			out.println(String.format("%16s --> %s", "_r15", "r15"));
			out.println(String.format("%16s --> %s", "_r16", "r16"));
			out.println(String.format("%16s --> %s", "_r17", "r17"));
			out.println(String.format("%16s --> %s", "_r18", "r18"));
			out.println(String.format("%16s --> %s", "_r19", "r19"));
			out.println(String.format("%16s --> %s", "_r2", "r2"));
			out.println(String.format("%16s --> %s", "_r20", "r20"));
			out.println(String.format("%16s --> %s", "_r21", "r21"));
			out.println(String.format("%16s --> %s", "_r22", "r22"));
			out.println(String.format("%16s --> %s", "_r23", "r23"));
			out.println(String.format("%16s --> %s", "_r24", "r24"));
			out.println(String.format("%16s --> %s", "_r25", "r25"));
			out.println(String.format("%16s --> %s", "_r26", "r26"));
			out.println(String.format("%16s --> %s", "_r27", "r27"));
			out.println(String.format("%16s --> %s", "_r28", "r28"));
			out.println(String.format("%16s --> %s", "_r29", "r29"));
			out.println(String.format("%16s --> %s", "_r3", "r3"));
			out.println(String.format("%16s --> %s", "_r30", "r30"));
			out.println(String.format("%16s --> %s", "_r31", "r31"));
			out.println(String.format("%16s --> %s", "_r4", "r4"));
			out.println(String.format("%16s --> %s", "_r5", "r5"));
			out.println(String.format("%16s --> %s", "_r6", "r6"));
			out.println(String.format("%16s --> %s", "_r7", "r7"));
			out.println(String.format("%16s --> %s", "_r8", "r8"));
			out.println(String.format("%16s --> %s", "_r9", "r9"));
			out.println(String.format("%16s --> %s", "_XER", "XER"));
			out.println(String.format("%16s --> %s", "jit_cr", "CR"));
			out.println(String.format("%16s --> %s", "jit_fpr0", "fp0"));
			out.println(String.format("%16s --> %s", "jit_fpr1", "fp1"));
			out.println(String.format("%16s --> %s", "jit_fpr10", "fp10"));
			out.println(String.format("%16s --> %s", "jit_fpr11", "fp11"));
			out.println(String.format("%16s --> %s", "jit_fpr12", "fp12"));
			out.println(String.format("%16s --> %s", "jit_fpr13", "fp13"));
			out.println(String.format("%16s --> %s", "jit_fpr14", "fp14"));
			out.println(String.format("%16s --> %s", "jit_fpr15", "fp15"));
			out.println(String.format("%16s --> %s", "jit_fpr16", "fp16"));
			out.println(String.format("%16s --> %s", "jit_fpr17", "fp17"));
			out.println(String.format("%16s --> %s", "jit_fpr18", "fp18"));
			out.println(String.format("%16s --> %s", "jit_fpr19", "fp19"));
			out.println(String.format("%16s --> %s", "jit_fpr2", "fp2"));
			out.println(String.format("%16s --> %s", "jit_fpr20", "fp20"));
			out.println(String.format("%16s --> %s", "jit_fpr21", "fp21"));
			out.println(String.format("%16s --> %s", "jit_fpr22", "fp22"));
			out.println(String.format("%16s --> %s", "jit_fpr23", "fp23"));
			out.println(String.format("%16s --> %s", "jit_fpr24", "fp24"));
			out.println(String.format("%16s --> %s", "jit_fpr25", "fp25"));
			out.println(String.format("%16s --> %s", "jit_fpr26", "fp26"));
			out.println(String.format("%16s --> %s", "jit_fpr27", "fp27"));
			out.println(String.format("%16s --> %s", "jit_fpr28", "fp28"));
			out.println(String.format("%16s --> %s", "jit_fpr29", "fp29"));
			out.println(String.format("%16s --> %s", "jit_fpr3", "fp3"));
			out.println(String.format("%16s --> %s", "jit_fpr30", "fp30"));
			out.println(String.format("%16s --> %s", "jit_fpr31", "fp31"));
			out.println(String.format("%16s --> %s", "jit_fpr4", "fp4"));
			out.println(String.format("%16s --> %s", "jit_fpr5", "fp5"));
			out.println(String.format("%16s --> %s", "jit_fpr6", "fp6"));
			out.println(String.format("%16s --> %s", "jit_fpr7", "fp7"));
			out.println(String.format("%16s --> %s", "jit_fpr8", "fp8"));
			out.println(String.format("%16s --> %s", "jit_fpr9", "fp9"));
			out.println(String.format("%16s --> %s", "jit_lr", "LR"));
			out.println(String.format("%16s --> %s", "jit_r0", "r0"));
			out.println(String.format("%16s --> %s", "jit_r1", "r1"));
			out.println(String.format("%16s --> %s", "jit_r10", "r10"));
			out.println(String.format("%16s --> %s", "jit_r11", "r11"));
			out.println(String.format("%16s --> %s", "jit_r12", "r12"));
			out.println(String.format("%16s --> %s", "jit_r13", "r13"));
			out.println(String.format("%16s --> %s", "jit_r14", "r14"));
			out.println(String.format("%16s --> %s", "jit_r15", "r15"));
			out.println(String.format("%16s --> %s", "jit_r16", "r16"));
			out.println(String.format("%16s --> %s", "jit_r17", "r17"));
			out.println(String.format("%16s --> %s", "jit_r18", "r18"));
			out.println(String.format("%16s --> %s", "jit_r19", "r19"));
			out.println(String.format("%16s --> %s", "jit_r2", "r2"));
			out.println(String.format("%16s --> %s", "jit_r20", "r20"));
			out.println(String.format("%16s --> %s", "jit_r21", "r21"));
			out.println(String.format("%16s --> %s", "jit_r22", "r22"));
			out.println(String.format("%16s --> %s", "jit_r23", "r23"));
			out.println(String.format("%16s --> %s", "jit_r24", "r24"));
			out.println(String.format("%16s --> %s", "jit_r25", "r25"));
			out.println(String.format("%16s --> %s", "jit_r26", "r26"));
			out.println(String.format("%16s --> %s", "jit_r27", "r27"));
			out.println(String.format("%16s --> %s", "jit_r28", "r28"));
			out.println(String.format("%16s --> %s", "jit_r29", "r29"));
			out.println(String.format("%16s --> %s", "jit_r3", "r3"));
			out.println(String.format("%16s --> %s", "jit_r30", "r30"));
			out.println(String.format("%16s --> %s", "jit_r31", "r31"));
			out.println(String.format("%16s --> %s", "jit_r4", "r4"));
			out.println(String.format("%16s --> %s", "jit_r5", "r5"));
			out.println(String.format("%16s --> %s", "jit_r6", "r6"));
			out.println(String.format("%16s --> %s", "jit_r7", "r7"));
			out.println(String.format("%16s --> %s", "jit_r8", "r8"));
			out.println(String.format("%16s --> %s", "jit_r9", "r9"));
			out.println(String.format("%16s --> %s", "saved_fpr14", "fp14"));
			out.println(String.format("%16s --> %s", "saved_fpr15", "fp15"));
			out.println(String.format("%16s --> %s", "saved_fpr16", "fp16"));
			out.println(String.format("%16s --> %s", "saved_fpr17", "fp17"));
			out.println(String.format("%16s --> %s", "saved_fpr18", "fp18"));
			out.println(String.format("%16s --> %s", "saved_fpr19", "fp19"));
			out.println(String.format("%16s --> %s", "saved_fpr20", "fp20"));
			out.println(String.format("%16s --> %s", "saved_fpr21", "fp21"));
			out.println(String.format("%16s --> %s", "saved_fpr22", "fp22"));
			out.println(String.format("%16s --> %s", "saved_fpr23", "fp23"));
			out.println(String.format("%16s --> %s", "saved_fpr24", "fp24"));
			out.println(String.format("%16s --> %s", "saved_fpr25", "fp25"));
			out.println(String.format("%16s --> %s", "saved_fpr26", "fp26"));
			out.println(String.format("%16s --> %s", "saved_fpr27", "fp27"));
			out.println(String.format("%16s --> %s", "saved_fpr28", "fp28"));
			out.println(String.format("%16s --> %s", "saved_fpr29", "fp29"));
			out.println(String.format("%16s --> %s", "saved_fpr30", "fp30"));
			out.println(String.format("%16s --> %s", "saved_fpr31", "fp31"));
		}
	}
	
	/**
	 * Prints registers for LINUX 64 BIT S390
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 */
	private static void printRegistersForLinux64BitS390(int level, PrintStream out)
	{
		if (level >= 1) {
			out.println(String.format("=========== Level %d Registers ===========\n", 1));
			out.println(String.format("%16s --> %s", "vmStruct", "r13"));
			out.println(String.format("%16s --> %s", "sp", "r5"));
			out.println(String.format("%16s --> %s", "arg0EA", "r10"));
			out.println(String.format("%16s --> %s", "pc", "r8"));
			out.println(String.format("%16s --> %s", "literals", "r9"));
		}
		if (level >= 2) {
			out.println(String.format("=========== Level %d Registers ===========\n", 2));
			out.println(String.format("%16s --> %s", "bytecodes", "r7"));
			out.println(String.format("%16s --> %s", "detailMessage", "r4"));
			out.println(String.format("%16s --> %s", "exceptionIndex", "r11"));
			out.println(String.format("%16s --> %s", "javaNewClass", "r4"));
			out.println(String.format("%16s --> %s", "messageNumber", "r10"));
			out.println(String.format("%16s --> %s", "methodHandle", "r10"));
			out.println(String.format("%16s --> %s", "moduleName", "r4"));
			out.println(String.format("%16s --> %s", "resolveBytecode", "r11"));
			out.println(String.format("%16s --> %s", "resolveIndex", "r4"));
			out.println(String.format("%16s --> %s", "resolveType", "r10"));
			out.println(String.format("%16s --> %s", "returnAddress", "r4"));
			out.println(String.format("%16s --> %s", "returnPoint", "r7"));
			out.println(String.format("%16s --> %s", "returnSP", "r11"));
			out.println(String.format("%16s --> %s", "sendArgs", "r4"));
			out.println(String.format("%16s --> %s", "sendMethod", "r11"));
			out.println(String.format("%16s --> %s", "sendReturn", "r10"));
			out.println(String.format("%16s --> %s", "syncObject", "r3"));
		}
		if (level >= 3) {
			out.println(String.format("=========== Level %d Registers ===========\n", 3));
			out.println(String.format("%16s --> %s", "asmAddr1", "r2"));
			out.println(String.format("%16s --> %s", "asmAddr2", "r3"));
			out.println(String.format("%16s --> %s", "asmAddr3", "r4"));
			out.println(String.format("%16s --> %s", "asmData1", "r5"));
			out.println(String.format("%16s --> %s", "asmData2", "r6"));
			out.println(String.format("%16s --> %s", "asmData3", "r7"));
			out.println(String.format("%16s --> %s", "cmpSlot0", "r0"));
			out.println(String.format("%16s --> %s", "cmpSlot1", "r1"));
			out.println(String.format("%16s --> %s", "divMulEven", "r0"));
			out.println(String.format("%16s --> %s", "divMulOdd", "r1"));
			out.println(String.format("%16s --> %s", "expandedDestEA", "r0"));
			out.println(String.format("%16s --> %s", "expandedSourceEA", "r1"));
			out.println(String.format("%16s --> %s", "globalOffsetTable", "r12"));
			out.println(String.format("%16s --> %s", "inParm1", "r2"));
			out.println(String.format("%16s --> %s", "inParm2", "r3"));
			out.println(String.format("%16s --> %s", "inParm3", "r4"));
			out.println(String.format("%16s --> %s", "inParm4", "r5"));
			out.println(String.format("%16s --> %s", "inParm5", "r6"));
			out.println(String.format("%16s --> %s", "linkRegister", "r14"));
			out.println(String.format("%16s --> %s", "machineSP", "r15"));
			out.println(String.format("%16s --> %s", "outParm1", "r2"));
			out.println(String.format("%16s --> %s", "outParm2", "r3"));
			out.println(String.format("%16s --> %s", "outParm3", "r4"));
			out.println(String.format("%16s --> %s", "outParm4", "r5"));
			out.println(String.format("%16s --> %s", "outParm5", "r6"));
			out.println(String.format("%16s --> %s", "pointerToStackParams", "r7"));
			out.println(String.format("%16s --> %s", "return64hiRead", "r2"));
			out.println(String.format("%16s --> %s", "return64hiWrite", "r2"));
			out.println(String.format("%16s --> %s", "return64loRead", "r3"));
			out.println(String.format("%16s --> %s", "return64loWrite", "r3"));
			out.println(String.format("%16s --> %s", "returnValue", "r2"));
			out.println(String.format("%16s --> %s", "swapSlot0", "r2"));
			out.println(String.format("%16s --> %s", "swapSlot1", "r3"));
		}
		if (level >= 4) {
			out.println(String.format("=========== Level %d Registers ===========\n", 4));
			out.println(String.format("%16s --> %s", "_fp0", "fp0"));
			out.println(String.format("%16s --> %s", "_fp1", "fp1"));
			out.println(String.format("%16s --> %s", "_fp10", "fp10"));
			out.println(String.format("%16s --> %s", "_fp11", "fp11"));
			out.println(String.format("%16s --> %s", "_fp12", "fp12"));
			out.println(String.format("%16s --> %s", "_fp13", "fp13"));
			out.println(String.format("%16s --> %s", "_fp14", "fp14"));
			out.println(String.format("%16s --> %s", "_fp15", "fp15"));
			out.println(String.format("%16s --> %s", "_fp2", "fp2"));
			out.println(String.format("%16s --> %s", "_fp3", "fp3"));
			out.println(String.format("%16s --> %s", "_fp4", "fp4"));
			out.println(String.format("%16s --> %s", "_fp5", "fp5"));
			out.println(String.format("%16s --> %s", "_fp6", "fp6"));
			out.println(String.format("%16s --> %s", "_fp7", "fp7"));
			out.println(String.format("%16s --> %s", "_fp8", "fp8"));
			out.println(String.format("%16s --> %s", "_fp9", "fp9"));
			out.println(String.format("%16s --> %s", "_r0", "r0"));
			out.println(String.format("%16s --> %s", "_r1", "r1"));
			out.println(String.format("%16s --> %s", "_r10", "r10"));
			out.println(String.format("%16s --> %s", "_r11", "r11"));
			out.println(String.format("%16s --> %s", "_r12", "r12"));
			out.println(String.format("%16s --> %s", "_r13", "r13"));
			out.println(String.format("%16s --> %s", "_r14", "r14"));
			out.println(String.format("%16s --> %s", "_r15", "r15"));
			out.println(String.format("%16s --> %s", "_r2", "r2"));
			out.println(String.format("%16s --> %s", "_r3", "r3"));
			out.println(String.format("%16s --> %s", "_r4", "r4"));
			out.println(String.format("%16s --> %s", "_r5", "r5"));
			out.println(String.format("%16s --> %s", "_r6", "r6"));
			out.println(String.format("%16s --> %s", "_r7", "r7"));
			out.println(String.format("%16s --> %s", "_r8", "r8"));
			out.println(String.format("%16s --> %s", "_r9", "r9"));
			out.println(String.format("%16s --> %s", "jit_fpr0", "fp0"));
			out.println(String.format("%16s --> %s", "jit_fpr1", "fp1"));
			out.println(String.format("%16s --> %s", "jit_fpr10", "fp10"));
			out.println(String.format("%16s --> %s", "jit_fpr11", "fp11"));
			out.println(String.format("%16s --> %s", "jit_fpr12", "fp12"));
			out.println(String.format("%16s --> %s", "jit_fpr13", "fp13"));
			out.println(String.format("%16s --> %s", "jit_fpr14", "fp14"));
			out.println(String.format("%16s --> %s", "jit_fpr15", "fp15"));
			out.println(String.format("%16s --> %s", "jit_fpr2", "fp2"));
			out.println(String.format("%16s --> %s", "jit_fpr3", "fp3"));
			out.println(String.format("%16s --> %s", "jit_fpr4", "fp4"));
			out.println(String.format("%16s --> %s", "jit_fpr5", "fp5"));
			out.println(String.format("%16s --> %s", "jit_fpr6", "fp6"));
			out.println(String.format("%16s --> %s", "jit_fpr7", "fp7"));
			out.println(String.format("%16s --> %s", "jit_fpr8", "fp8"));
			out.println(String.format("%16s --> %s", "jit_fpr9", "fp9"));
			out.println(String.format("%16s --> %s", "jit_r0", "r0"));
			out.println(String.format("%16s --> %s", "jit_r1", "r1"));
			out.println(String.format("%16s --> %s", "jit_r10", "r10"));
			out.println(String.format("%16s --> %s", "jit_r11", "r11"));
			out.println(String.format("%16s --> %s", "jit_r12", "r12"));
			out.println(String.format("%16s --> %s", "jit_r13", "r13"));
			out.println(String.format("%16s --> %s", "jit_r14", "r14"));
			out.println(String.format("%16s --> %s", "jit_r15", "r15"));
			out.println(String.format("%16s --> %s", "jit_r2", "r2"));
			out.println(String.format("%16s --> %s", "jit_r3", "r3"));
			out.println(String.format("%16s --> %s", "jit_r4", "r4"));
			out.println(String.format("%16s --> %s", "jit_r5", "r5"));
			out.println(String.format("%16s --> %s", "jit_r6", "r6"));
			out.println(String.format("%16s --> %s", "jit_r7", "r7"));
			out.println(String.format("%16s --> %s", "jit_r8", "r8"));
			out.println(String.format("%16s --> %s", "jit_r9", "r9"));
			out.println(String.format("%16s --> %s", "saved_fpr10", "fp10"));
			out.println(String.format("%16s --> %s", "saved_fpr11", "fp11"));
			out.println(String.format("%16s --> %s", "saved_fpr12", "fp12"));
			out.println(String.format("%16s --> %s", "saved_fpr13", "fp13"));
			out.println(String.format("%16s --> %s", "saved_fpr14", "fp14"));
			out.println(String.format("%16s --> %s", "saved_fpr15", "fp15"));
			out.println(String.format("%16s --> %s", "saved_fpr8", "fp8"));
			out.println(String.format("%16s --> %s", "saved_fpr9", "fp9"));
		}		
	}
	
	/**
	 * Prints registers for LINUX 32 BIT X86
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 */
	private static void printRegistersForLinux32BitX86(int level, PrintStream out)
	{
		if (level >= 1) {
			out.println(String.format("=========== Level %d Registers ===========\n", 1));
			out.println(String.format("%16s --> %s", "vmStruct", "%ebp"));
			out.println(String.format("%16s --> %s", "sp", "%esp"));
			out.println(String.format("%16s --> %s", "arg0EA", "%ecx"));
			out.println(String.format("%16s --> %s", "pc", "%esi"));
			out.println(String.format("%16s --> %s", "literals", "%ebx"));
		}
		if (level >= 2) {
			out.println(String.format("=========== Level %d Registers ===========\n", 2));
			out.println(String.format("%16s --> %s", "allButLowByteZero", "%eax"));
			out.println(String.format("%16s --> %s", "bp", "%edx"));
			out.println(String.format("%16s --> %s", "bytecodes", "%edi"));
			out.println(String.format("%16s --> %s", "cacheIndex", "%ecx"));
			out.println(String.format("%16s --> %s", "callInAddress", "%esi"));
			out.println(String.format("%16s --> %s", "detailMessage", "%edi"));
			out.println(String.format("%16s --> %s", "exceptionIndex", "%eax"));
			out.println(String.format("%16s --> %s", "globalOffsetTable", "%ebx"));
			out.println(String.format("%16s --> %s", "javaNewClass", "%edi"));
			out.println(String.format("%16s --> %s", "messageNumber", "%ecx"));
			out.println(String.format("%16s --> %s", "methodHandle", "%ecx"));
			out.println(String.format("%16s --> %s", "moduleName", "%edi"));
			out.println(String.format("%16s --> %s", "resolveBytecode", "%eax"));
			out.println(String.format("%16s --> %s", "resolveIndex", "%edi"));
			out.println(String.format("%16s --> %s", "resolveType", "%ecx"));
			out.println(String.format("%16s --> %s", "returnAddress", "%edi"));
			out.println(String.format("%16s --> %s", "returnPoint", "%eax"));
			out.println(String.format("%16s --> %s", "returnSP", "%eax"));
			out.println(String.format("%16s --> %s", "sendArgs", "%edi"));
			out.println(String.format("%16s --> %s", "sendMethod", "%eax"));
			out.println(String.format("%16s --> %s", "sendReturn", "%ecx"));
			out.println(String.format("%16s --> %s", "signalHandlerNewSP", "%edi"));
			out.println(String.format("%16s --> %s", "signalHandlerOldSP", "%esi"));
			out.println(String.format("%16s --> %s", "signalHandlerParm1", "%eax"));
			out.println(String.format("%16s --> %s", "signalHandlerParm2", "%ebp"));
			out.println(String.format("%16s --> %s", "signalHandlerParm3", "%ecx"));
			out.println(String.format("%16s --> %s", "signalHandlerParm4", "%edx"));
			out.println(String.format("%16s --> %s", "syncObject", "%ecx"));
		}
		if (level >= 3) {
			out.println(String.format("=========== Level %d Registers ===========\n", 3));
			out.println(String.format("%16s --> %s", "asmAddr1", "%eax"));
			out.println(String.format("%16s --> %s", "asmAddr2", "%ecx"));
			out.println(String.format("%16s --> %s", "asmAddr3", "%ebx"));
			out.println(String.format("%16s --> %s", "asmData1", "%edx"));
			out.println(String.format("%16s --> %s", "asmData2", "%esi"));
			out.println(String.format("%16s --> %s", "asmData3", "%edi"));
			out.println(String.format("%16s --> %s", "cmpxchg8bcmpVal1", "%eax"));
			out.println(String.format("%16s --> %s", "cmpxchg8bcmpVal2", "%edx"));
			out.println(String.format("%16s --> %s", "cmpxchg8bdstVal1", "%ebx"));
			out.println(String.format("%16s --> %s", "cmpxchg8bdstVal2", "%ecx"));
			out.println(String.format("%16s --> %s", "cmpxchgValue", "%eax"));
			out.println(String.format("%16s --> %s", "divideQuotient", "%eax"));
			out.println(String.format("%16s --> %s", "divideRemainder", "%edx"));
			out.println(String.format("%16s --> %s", "floatStatusWord", "%eax"));
			out.println(String.format("%16s --> %s", "machineBP", "%ebp"));
			out.println(String.format("%16s --> %s", "machineSP", "%esp"));
			out.println(String.format("%16s --> %s", "memCount", "%ecx"));
			out.println(String.format("%16s --> %s", "memDest", "%edi"));
			out.println(String.format("%16s --> %s", "memSource", "%esi"));
			out.println(String.format("%16s --> %s", "memValue", "%eax"));
			out.println(String.format("%16s --> %s", "rdtscHi", "%edx"));
			out.println(String.format("%16s --> %s", "rdtscLo", "%eax"));
			out.println(String.format("%16s --> %s", "return64hiRead", "%edx"));
			out.println(String.format("%16s --> %s", "return64hiWrite", "%edx"));
			out.println(String.format("%16s --> %s", "return64loRead", "%eax"));
			out.println(String.format("%16s --> %s", "return64loWrite", "%eax"));
			out.println(String.format("%16s --> %s", "returnValue", "%eax"));
			out.println(String.format("%16s --> %s", "shiftSource", "%ecx"));
		}
		if (level >= 4) {
			out.println(String.format("=========== Level %d Registers ===========\n", 4));
			out.println(String.format("%16s --> %s", "_%eax", "%eax"));
			out.println(String.format("%16s --> %s", "_%ebp", "%ebp"));
			out.println(String.format("%16s --> %s", "_%ebx", "%ebx"));
			out.println(String.format("%16s --> %s", "_%ecx", "%ecx"));
			out.println(String.format("%16s --> %s", "_%edi", "%edi"));
			out.println(String.format("%16s --> %s", "_%edx", "%edx"));
			out.println(String.format("%16s --> %s", "_%esi", "%esi"));
			out.println(String.format("%16s --> %s", "_%esp", "%esp"));
			out.println(String.format("%16s --> %s", "_%xmm0", "%xmm0"));
			out.println(String.format("%16s --> %s", "_%xmm1", "%xmm1"));
			out.println(String.format("%16s --> %s", "_%xmm2", "%xmm2"));
			out.println(String.format("%16s --> %s", "_%xmm3", "%xmm3"));
			out.println(String.format("%16s --> %s", "_%xmm4", "%xmm4"));
			out.println(String.format("%16s --> %s", "_%xmm5", "%xmm5"));
			out.println(String.format("%16s --> %s", "_%xmm6", "%xmm6"));
			out.println(String.format("%16s --> %s", "_%xmm7", "%xmm7"));
			out.println(String.format("%16s --> %s", "_EAX", "%eax"));
			out.println(String.format("%16s --> %s", "_EBP", "%ebp"));
			out.println(String.format("%16s --> %s", "_EBX", "%ebx"));
			out.println(String.format("%16s --> %s", "_ECX", "%ecx"));
			out.println(String.format("%16s --> %s", "_EDI", "%edi"));
			out.println(String.format("%16s --> %s", "_EDX", "%edx"));
			out.println(String.format("%16s --> %s", "_ESI", "%esi"));
			out.println(String.format("%16s --> %s", "_ESP", "%esp"));
			out.println(String.format("%16s --> %s", "_xmm0", "%xmm0"));
			out.println(String.format("%16s --> %s", "_xmm1", "%xmm1"));
			out.println(String.format("%16s --> %s", "_xmm2", "%xmm2"));
			out.println(String.format("%16s --> %s", "_xmm3", "%xmm3"));
			out.println(String.format("%16s --> %s", "_xmm4", "%xmm4"));
			out.println(String.format("%16s --> %s", "_xmm5", "%xmm5"));
			out.println(String.format("%16s --> %s", "_xmm6", "%xmm6"));
			out.println(String.format("%16s --> %s", "_xmm7", "%xmm7"));
			out.println(String.format("%16s --> %s", "jit_eax", "%eax"));
			out.println(String.format("%16s --> %s", "jit_ebx", "%ebx"));
			out.println(String.format("%16s --> %s", "jit_ecx", "%ecx"));
			out.println(String.format("%16s --> %s", "jit_edi", "%edi"));
			out.println(String.format("%16s --> %s", "jit_edx", "%edx"));
			out.println(String.format("%16s --> %s", "jit_esi", "%esi"));
			out.println(String.format("%16s --> %s", "jit_fpr0", "%xmm0"));
			out.println(String.format("%16s --> %s", "jit_fpr1", "%xmm1"));
			out.println(String.format("%16s --> %s", "jit_fpr2", "%xmm2"));
			out.println(String.format("%16s --> %s", "jit_fpr3", "%xmm3"));
			out.println(String.format("%16s --> %s", "jit_fpr4", "%xmm4"));
			out.println(String.format("%16s --> %s", "jit_fpr5", "%xmm5"));
			out.println(String.format("%16s --> %s", "jit_fpr6", "%xmm6"));
			out.println(String.format("%16s --> %s", "jit_fpr7", "%xmm7"));
		}
	}

	/**
	 * Prints registers for LINUX 32 BIT PPC
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 */
	private static void printRegistersForLinux32BitPPC(int level, PrintStream out)
	{
		if (level >= 1) {
			out.println(String.format("=========== Level %d Registers ===========\n", 1));
			out.println(String.format("%16s --> %s", "vmStruct", "r13"));
			out.println(String.format("%16s --> %s", "sp", "r14"));
			out.println(String.format("%16s --> %s", "arg0EA", "r21"));
			out.println(String.format("%16s --> %s", "pc", "r16"));
			out.println(String.format("%16s --> %s", "literals", "r17"));
		}
		if (level >= 2) {
			out.println(String.format("=========== Level %d Registers ===========\n", 2));
			out.println(String.format("%16s --> %s", "bytecodes", "r18"));
			out.println(String.format("%16s --> %s", "detailMessage", "r19"));
			out.println(String.format("%16s --> %s", "exceptionIndex", "r20"));
			out.println(String.format("%16s --> %s", "javaNewClass", "r19"));
			out.println(String.format("%16s --> %s", "messageNumber", "r21"));
			out.println(String.format("%16s --> %s", "methodHandle", "r21"));
			out.println(String.format("%16s --> %s", "moduleName", "r19"));
			out.println(String.format("%16s --> %s", "resolveBytecode", "r20"));
			out.println(String.format("%16s --> %s", "resolveIndex", "r19"));
			out.println(String.format("%16s --> %s", "resolveType", "r21"));
			out.println(String.format("%16s --> %s", "returnAddress", "r19"));
			out.println(String.format("%16s --> %s", "returnPoint", "r10"));
			out.println(String.format("%16s --> %s", "returnSP", "r20"));
			out.println(String.format("%16s --> %s", "saved_cr", "CR"));
			out.println(String.format("%16s --> %s", "sendArgs", "r19"));
			out.println(String.format("%16s --> %s", "sendMethod", "r20"));
			out.println(String.format("%16s --> %s", "sendReturn", "r21"));
			out.println(String.format("%16s --> %s", "syncObject", "r6"));
		}
		if (level >= 3) {
			out.println(String.format("=========== Level %d Registers ===========\n", 3));
			out.println(String.format("%16s --> %s", "asmAddr1", "r5"));
			out.println(String.format("%16s --> %s", "asmAddr2", "r6"));
			out.println(String.format("%16s --> %s", "asmAddr3", "r7"));
			out.println(String.format("%16s --> %s", "asmData1", "r8"));
			out.println(String.format("%16s --> %s", "asmData2", "r9"));
			out.println(String.format("%16s --> %s", "asmData3", "r10"));
			out.println(String.format("%16s --> %s", "CR", "CR"));
			out.println(String.format("%16s --> %s", "CTR", "CTR"));
			out.println(String.format("%16s --> %s", "destEA", "r28"));
			out.println(String.format("%16s --> %s", "globalOffsetTable", "r29"));
			out.println(String.format("%16s --> %s", "inParm1", "r3"));
			out.println(String.format("%16s --> %s", "inParm2", "r4"));
			out.println(String.format("%16s --> %s", "inParm3", "r5"));
			out.println(String.format("%16s --> %s", "inParm4", "r6"));
			out.println(String.format("%16s --> %s", "inParm5", "r7"));
			out.println(String.format("%16s --> %s", "inParm6", "r8"));
			out.println(String.format("%16s --> %s", "inParm7", "r9"));
			out.println(String.format("%16s --> %s", "inParm8", "r10"));
			out.println(String.format("%16s --> %s", "LR", "LR"));
			out.println(String.format("%16s --> %s", "machineSP", "r1"));
			out.println(String.format("%16s --> %s", "oldSP", "r30"));
			out.println(String.format("%16s --> %s", "outParm1", "r3"));
			out.println(String.format("%16s --> %s", "outParm2", "r4"));
			out.println(String.format("%16s --> %s", "outParm3", "r5"));
			out.println(String.format("%16s --> %s", "outParm4", "r6"));
			out.println(String.format("%16s --> %s", "outParm5", "r7"));
			out.println(String.format("%16s --> %s", "outParm6", "r8"));
			out.println(String.format("%16s --> %s", "outParm7", "r9"));
			out.println(String.format("%16s --> %s", "outParm8", "r10"));
			out.println(String.format("%16s --> %s", "return64hiRead", "r3"));
			out.println(String.format("%16s --> %s", "return64hiWrite", "r3"));
			out.println(String.format("%16s --> %s", "return64loRead", "r4"));
			out.println(String.format("%16s --> %s", "return64loWrite", "r4"));
			out.println(String.format("%16s --> %s", "returnValue", "r3"));
			out.println(String.format("%16s --> %s", "sourceEA", "r31"));
			out.println(String.format("%16s --> %s", "stringCopy5", "r5"));
			out.println(String.format("%16s --> %s", "stringCopy6", "r6"));
			out.println(String.format("%16s --> %s", "stringCopy7", "r7"));
			out.println(String.format("%16s --> %s", "stringCopy8", "r8"));
			out.println(String.format("%16s --> %s", "stringCopyCount", "r9"));
			out.println(String.format("%16s --> %s", "stringCopyDest", "r3"));
			out.println(String.format("%16s --> %s", "stringCopySource", "r4"));
			out.println(String.format("%16s --> %s", "tempEA", "r27"));
			out.println(String.format("%16s --> %s", "XER", "XER"));
		}
		if (level >= 4) {
			out.println(String.format("=========== Level %d Registers ===========\n", 4));
			out.println(String.format("%16s --> %s", "_CR", "CR"));
			out.println(String.format("%16s --> %s", "_CTR", "CTR"));
			out.println(String.format("%16s --> %s", "_fp0", "fp0"));
			out.println(String.format("%16s --> %s", "_fp1", "fp1"));
			out.println(String.format("%16s --> %s", "_fp10", "fp10"));
			out.println(String.format("%16s --> %s", "_fp11", "fp11"));
			out.println(String.format("%16s --> %s", "_fp12", "fp12"));
			out.println(String.format("%16s --> %s", "_fp13", "fp13"));
			out.println(String.format("%16s --> %s", "_fp14", "fp14"));
			out.println(String.format("%16s --> %s", "_fp15", "fp15"));
			out.println(String.format("%16s --> %s", "_fp16", "fp16"));
			out.println(String.format("%16s --> %s", "_fp17", "fp17"));
			out.println(String.format("%16s --> %s", "_fp18", "fp18"));
			out.println(String.format("%16s --> %s", "_fp19", "fp19"));
			out.println(String.format("%16s --> %s", "_fp2", "fp2"));
			out.println(String.format("%16s --> %s", "_fp20", "fp20"));
			out.println(String.format("%16s --> %s", "_fp21", "fp21"));
			out.println(String.format("%16s --> %s", "_fp22", "fp22"));
			out.println(String.format("%16s --> %s", "_fp23", "fp23"));
			out.println(String.format("%16s --> %s", "_fp24", "fp24"));
			out.println(String.format("%16s --> %s", "_fp25", "fp25"));
			out.println(String.format("%16s --> %s", "_fp26", "fp26"));
			out.println(String.format("%16s --> %s", "_fp27", "fp27"));
			out.println(String.format("%16s --> %s", "_fp28", "fp28"));
			out.println(String.format("%16s --> %s", "_fp29", "fp29"));
			out.println(String.format("%16s --> %s", "_fp3", "fp3"));
			out.println(String.format("%16s --> %s", "_fp30", "fp30"));
			out.println(String.format("%16s --> %s", "_fp31", "fp31"));
			out.println(String.format("%16s --> %s", "_fp4", "fp4"));
			out.println(String.format("%16s --> %s", "_fp5", "fp5"));
			out.println(String.format("%16s --> %s", "_fp6", "fp6"));
			out.println(String.format("%16s --> %s", "_fp7", "fp7"));
			out.println(String.format("%16s --> %s", "_fp8", "fp8"));
			out.println(String.format("%16s --> %s", "_fp9", "fp9"));
			out.println(String.format("%16s --> %s", "_LR", "LR"));
			out.println(String.format("%16s --> %s", "_r0", "r0"));
			out.println(String.format("%16s --> %s", "_r1", "r1"));
			out.println(String.format("%16s --> %s", "_r10", "r10"));
			out.println(String.format("%16s --> %s", "_r11", "r11"));
			out.println(String.format("%16s --> %s", "_r12", "r12"));
			out.println(String.format("%16s --> %s", "_r13", "r13"));
			out.println(String.format("%16s --> %s", "_r14", "r14"));
			out.println(String.format("%16s --> %s", "_r15", "r15"));
			out.println(String.format("%16s --> %s", "_r16", "r16"));
			out.println(String.format("%16s --> %s", "_r17", "r17"));
			out.println(String.format("%16s --> %s", "_r18", "r18"));
			out.println(String.format("%16s --> %s", "_r19", "r19"));
			out.println(String.format("%16s --> %s", "_r20", "r20"));
			out.println(String.format("%16s --> %s", "_r21", "r21"));
			out.println(String.format("%16s --> %s", "_r22", "r22"));
			out.println(String.format("%16s --> %s", "_r23", "r23"));
			out.println(String.format("%16s --> %s", "_r24", "r24"));
			out.println(String.format("%16s --> %s", "_r25", "r25"));
			out.println(String.format("%16s --> %s", "_r26", "r26"));
			out.println(String.format("%16s --> %s", "_r27", "r27"));
			out.println(String.format("%16s --> %s", "_r28", "r28"));
			out.println(String.format("%16s --> %s", "_r29", "r29"));
			out.println(String.format("%16s --> %s", "_r3", "r3"));
			out.println(String.format("%16s --> %s", "_r30", "r30"));
			out.println(String.format("%16s --> %s", "_r31", "r31"));
			out.println(String.format("%16s --> %s", "_r4", "r4"));
			out.println(String.format("%16s --> %s", "_r5", "r5"));
			out.println(String.format("%16s --> %s", "_r6", "r6"));
			out.println(String.format("%16s --> %s", "_r7", "r7"));
			out.println(String.format("%16s --> %s", "_r8", "r8"));
			out.println(String.format("%16s --> %s", "_r9", "r9"));
			out.println(String.format("%16s --> %s", "_XER", "XER"));
			out.println(String.format("%16s --> %s", "jit_cr", "CR"));
			out.println(String.format("%16s --> %s", "jit_fpr0", "fp0"));
			out.println(String.format("%16s --> %s", "jit_fpr1", "fp1"));
			out.println(String.format("%16s --> %s", "jit_fpr10", "fp10"));
			out.println(String.format("%16s --> %s", "jit_fpr11", "fp11"));
			out.println(String.format("%16s --> %s", "jit_fpr12", "fp12"));
			out.println(String.format("%16s --> %s", "jit_fpr13", "fp13"));
			out.println(String.format("%16s --> %s", "jit_fpr14", "fp14"));
			out.println(String.format("%16s --> %s", "jit_fpr15", "fp15"));
			out.println(String.format("%16s --> %s", "jit_fpr16", "fp16"));
			out.println(String.format("%16s --> %s", "jit_fpr17", "fp17"));
			out.println(String.format("%16s --> %s", "jit_fpr18", "fp18"));
			out.println(String.format("%16s --> %s", "jit_fpr19", "fp19"));
			out.println(String.format("%16s --> %s", "jit_fpr2", "fp2"));
			out.println(String.format("%16s --> %s", "jit_fpr20", "fp20"));
			out.println(String.format("%16s --> %s", "jit_fpr21", "fp21"));
			out.println(String.format("%16s --> %s", "jit_fpr22", "fp22"));
			out.println(String.format("%16s --> %s", "jit_fpr23", "fp23"));
			out.println(String.format("%16s --> %s", "jit_fpr24", "fp24"));
			out.println(String.format("%16s --> %s", "jit_fpr25", "fp25"));
			out.println(String.format("%16s --> %s", "jit_fpr26", "fp26"));
			out.println(String.format("%16s --> %s", "jit_fpr27", "fp27"));
			out.println(String.format("%16s --> %s", "jit_fpr28", "fp28"));
			out.println(String.format("%16s --> %s", "jit_fpr29", "fp29"));
			out.println(String.format("%16s --> %s", "jit_fpr3", "fp3"));
			out.println(String.format("%16s --> %s", "jit_fpr30", "fp30"));
			out.println(String.format("%16s --> %s", "jit_fpr31", "fp31"));
			out.println(String.format("%16s --> %s", "jit_fpr4", "fp4"));
			out.println(String.format("%16s --> %s", "jit_fpr5", "fp5"));
			out.println(String.format("%16s --> %s", "jit_fpr6", "fp6"));
			out.println(String.format("%16s --> %s", "jit_fpr7", "fp7"));
			out.println(String.format("%16s --> %s", "jit_fpr8", "fp8"));
			out.println(String.format("%16s --> %s", "jit_fpr9", "fp9"));
			out.println(String.format("%16s --> %s", "jit_lr", "LR"));
			out.println(String.format("%16s --> %s", "jit_r0", "r0"));
			out.println(String.format("%16s --> %s", "jit_r1", "r1"));
			out.println(String.format("%16s --> %s", "jit_r10", "r10"));
			out.println(String.format("%16s --> %s", "jit_r11", "r11"));
			out.println(String.format("%16s --> %s", "jit_r12", "r12"));
			out.println(String.format("%16s --> %s", "jit_r13", "r13"));
			out.println(String.format("%16s --> %s", "jit_r14", "r14"));
			out.println(String.format("%16s --> %s", "jit_r15", "r15"));
			out.println(String.format("%16s --> %s", "jit_r16", "r16"));
			out.println(String.format("%16s --> %s", "jit_r17", "r17"));
			out.println(String.format("%16s --> %s", "jit_r18", "r18"));
			out.println(String.format("%16s --> %s", "jit_r19", "r19"));
			out.println(String.format("%16s --> %s", "jit_r20", "r20"));
			out.println(String.format("%16s --> %s", "jit_r21", "r21"));
			out.println(String.format("%16s --> %s", "jit_r22", "r22"));
			out.println(String.format("%16s --> %s", "jit_r23", "r23"));
			out.println(String.format("%16s --> %s", "jit_r24", "r24"));
			out.println(String.format("%16s --> %s", "jit_r25", "r25"));
			out.println(String.format("%16s --> %s", "jit_r26", "r26"));
			out.println(String.format("%16s --> %s", "jit_r27", "r27"));
			out.println(String.format("%16s --> %s", "jit_r28", "r28"));
			out.println(String.format("%16s --> %s", "jit_r29", "r29"));
			out.println(String.format("%16s --> %s", "jit_r3", "r3"));
			out.println(String.format("%16s --> %s", "jit_r30", "r30"));
			out.println(String.format("%16s --> %s", "jit_r31", "r31"));
			out.println(String.format("%16s --> %s", "jit_r4", "r4"));
			out.println(String.format("%16s --> %s", "jit_r5", "r5"));
			out.println(String.format("%16s --> %s", "jit_r6", "r6"));
			out.println(String.format("%16s --> %s", "jit_r7", "r7"));
			out.println(String.format("%16s --> %s", "jit_r8", "r8"));
			out.println(String.format("%16s --> %s", "jit_r9", "r9"));
			out.println(String.format("%16s --> %s", "saved_fpr14", "fp14"));
			out.println(String.format("%16s --> %s", "saved_fpr15", "fp15"));
			out.println(String.format("%16s --> %s", "saved_fpr16", "fp16"));
			out.println(String.format("%16s --> %s", "saved_fpr17", "fp17"));
			out.println(String.format("%16s --> %s", "saved_fpr18", "fp18"));
			out.println(String.format("%16s --> %s", "saved_fpr19", "fp19"));
			out.println(String.format("%16s --> %s", "saved_fpr20", "fp20"));
			out.println(String.format("%16s --> %s", "saved_fpr21", "fp21"));
			out.println(String.format("%16s --> %s", "saved_fpr22", "fp22"));
			out.println(String.format("%16s --> %s", "saved_fpr23", "fp23"));
			out.println(String.format("%16s --> %s", "saved_fpr24", "fp24"));
			out.println(String.format("%16s --> %s", "saved_fpr25", "fp25"));
			out.println(String.format("%16s --> %s", "saved_fpr26", "fp26"));
			out.println(String.format("%16s --> %s", "saved_fpr27", "fp27"));
			out.println(String.format("%16s --> %s", "saved_fpr28", "fp28"));
			out.println(String.format("%16s --> %s", "saved_fpr29", "fp29"));
			out.println(String.format("%16s --> %s", "saved_fpr30", "fp30"));
			out.println(String.format("%16s --> %s", "saved_fpr31", "fp31"));
		}	
	}

	/**
	 * Prints registers for LINUX 32 BIT S390
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 */
	private static void printRegistersForLinux32BitS390(int level, PrintStream out)
	{
		if (level >= 1) {
			out.println(String.format("=========== Level %d Registers ===========\n", 1));
			out.println(String.format("%16s --> %s", "vmStruct", "r13"));
			out.println(String.format("%16s --> %s", "sp", "r5"));
			out.println(String.format("%16s --> %s", "arg0EA", "r10"));
			out.println(String.format("%16s --> %s", "pc", "r8"));
			out.println(String.format("%16s --> %s", "literals", "r9"));
		}
		if (level >= 2) {
			out.println(String.format("=========== Level %d Registers ===========\n", 2));
			out.println(String.format("%16s --> %s", "bytecodes", "r7"));
			out.println(String.format("%16s --> %s", "detailMessage", "r4"));
			out.println(String.format("%16s --> %s", "exceptionIndex", "r11"));
			out.println(String.format("%16s --> %s", "javaNewClass", "r4"));
			out.println(String.format("%16s --> %s", "messageNumber", "r10"));
			out.println(String.format("%16s --> %s", "methodHandle", "r10"));
			out.println(String.format("%16s --> %s", "moduleName", "r4"));
			out.println(String.format("%16s --> %s", "resolveBytecode", "r11"));
			out.println(String.format("%16s --> %s", "resolveIndex", "r4"));
			out.println(String.format("%16s --> %s", "resolveType", "r10"));
			out.println(String.format("%16s --> %s", "returnAddress", "r4"));
			out.println(String.format("%16s --> %s", "returnPoint", "r7"));
			out.println(String.format("%16s --> %s", "returnSP", "r11"));
			out.println(String.format("%16s --> %s", "sendArgs", "r4"));
			out.println(String.format("%16s --> %s", "sendMethod", "r11"));
			out.println(String.format("%16s --> %s", "sendReturn", "r10"));
			out.println(String.format("%16s --> %s", "syncObject", "r3"));
		}
		if (level >= 3) {
			out.println(String.format("=========== Level %d Registers ===========\n", 3));
			out.println(String.format("%16s --> %s", "asmAddr1", "r2"));
			out.println(String.format("%16s --> %s", "asmAddr2", "r3"));
			out.println(String.format("%16s --> %s", "asmAddr3", "r4"));
			out.println(String.format("%16s --> %s", "asmData1", "r5"));
			out.println(String.format("%16s --> %s", "asmData2", "r6"));
			out.println(String.format("%16s --> %s", "asmData3", "r7"));
			out.println(String.format("%16s --> %s", "cmpSlot0", "r0"));
			out.println(String.format("%16s --> %s", "cmpSlot1", "r1"));
			out.println(String.format("%16s --> %s", "divMulEven", "r0"));
			out.println(String.format("%16s --> %s", "divMulOdd", "r1"));
			out.println(String.format("%16s --> %s", "expandedDestEA", "r0"));
			out.println(String.format("%16s --> %s", "expandedSourceEA", "r1"));
			out.println(String.format("%16s --> %s", "globalOffsetTable", "r12"));
			out.println(String.format("%16s --> %s", "inParm1", "r2"));
			out.println(String.format("%16s --> %s", "inParm2", "r3"));
			out.println(String.format("%16s --> %s", "inParm3", "r4"));
			out.println(String.format("%16s --> %s", "inParm4", "r5"));
			out.println(String.format("%16s --> %s", "inParm5", "r6"));
			out.println(String.format("%16s --> %s", "linkRegister", "r14"));
			out.println(String.format("%16s --> %s", "machineSP", "r15"));
			out.println(String.format("%16s --> %s", "outParm1", "r2"));
			out.println(String.format("%16s --> %s", "outParm2", "r3"));
			out.println(String.format("%16s --> %s", "outParm3", "r4"));
			out.println(String.format("%16s --> %s", "outParm4", "r5"));
			out.println(String.format("%16s --> %s", "outParm5", "r6"));
			out.println(String.format("%16s --> %s", "pointerToStackParams", "r7"));
			out.println(String.format("%16s --> %s", "return64hiRead", "r2"));
			out.println(String.format("%16s --> %s", "return64hiWrite", "r2"));
			out.println(String.format("%16s --> %s", "return64loRead", "r3"));
			out.println(String.format("%16s --> %s", "return64loWrite", "r3"));
			out.println(String.format("%16s --> %s", "returnValue", "r2"));
			out.println(String.format("%16s --> %s", "swapSlot0", "r2"));
			out.println(String.format("%16s --> %s", "swapSlot1", "r3"));
		}
		if (level >= 4) {
			out.println(String.format("=========== Level %d Registers ===========\n", 4));
			out.println(String.format("%16s --> %s", "_fp0", "fp0"));
			out.println(String.format("%16s --> %s", "_fp1", "fp1"));
			out.println(String.format("%16s --> %s", "_fp10", "fp10"));
			out.println(String.format("%16s --> %s", "_fp11", "fp11"));
			out.println(String.format("%16s --> %s", "_fp12", "fp12"));
			out.println(String.format("%16s --> %s", "_fp13", "fp13"));
			out.println(String.format("%16s --> %s", "_fp14", "fp14"));
			out.println(String.format("%16s --> %s", "_fp15", "fp15"));
			out.println(String.format("%16s --> %s", "_fp2", "fp2"));
			out.println(String.format("%16s --> %s", "_fp3", "fp3"));
			out.println(String.format("%16s --> %s", "_fp4", "fp4"));
			out.println(String.format("%16s --> %s", "_fp5", "fp5"));
			out.println(String.format("%16s --> %s", "_fp6", "fp6"));
			out.println(String.format("%16s --> %s", "_fp7", "fp7"));
			out.println(String.format("%16s --> %s", "_fp8", "fp8"));
			out.println(String.format("%16s --> %s", "_fp9", "fp9"));
			out.println(String.format("%16s --> %s", "_r0", "r0"));
			out.println(String.format("%16s --> %s", "_r1", "r1"));
			out.println(String.format("%16s --> %s", "_r10", "r10"));
			out.println(String.format("%16s --> %s", "_r11", "r11"));
			out.println(String.format("%16s --> %s", "_r12", "r12"));
			out.println(String.format("%16s --> %s", "_r13", "r13"));
			out.println(String.format("%16s --> %s", "_r14", "r14"));
			out.println(String.format("%16s --> %s", "_r15", "r15"));
			out.println(String.format("%16s --> %s", "_r16", "r16"));
			out.println(String.format("%16s --> %s", "_r17", "r17"));
			out.println(String.format("%16s --> %s", "_r18", "r18"));
			out.println(String.format("%16s --> %s", "_r19", "r19"));
			out.println(String.format("%16s --> %s", "_r2", "r2"));
			out.println(String.format("%16s --> %s", "_r20", "r20"));
			out.println(String.format("%16s --> %s", "_r21", "r21"));
			out.println(String.format("%16s --> %s", "_r22", "r22"));
			out.println(String.format("%16s --> %s", "_r23", "r23"));
			out.println(String.format("%16s --> %s", "_r24", "r24"));
			out.println(String.format("%16s --> %s", "_r25", "r25"));
			out.println(String.format("%16s --> %s", "_r26", "r26"));
			out.println(String.format("%16s --> %s", "_r27", "r27"));
			out.println(String.format("%16s --> %s", "_r28", "r28"));
			out.println(String.format("%16s --> %s", "_r29", "r29"));
			out.println(String.format("%16s --> %s", "_r3", "r3"));
			out.println(String.format("%16s --> %s", "_r30", "r30"));
			out.println(String.format("%16s --> %s", "_r31", "r31"));
			out.println(String.format("%16s --> %s", "_r4", "r4"));
			out.println(String.format("%16s --> %s", "_r5", "r5"));
			out.println(String.format("%16s --> %s", "_r6", "r6"));
			out.println(String.format("%16s --> %s", "_r7", "r7"));
			out.println(String.format("%16s --> %s", "_r8", "r8"));
			out.println(String.format("%16s --> %s", "_r9", "r9"));
			out.println(String.format("%16s --> %s", "jit_fpr0", "fp0"));
			out.println(String.format("%16s --> %s", "jit_fpr1", "fp1"));
			out.println(String.format("%16s --> %s", "jit_fpr10", "fp10"));
			out.println(String.format("%16s --> %s", "jit_fpr11", "fp11"));
			out.println(String.format("%16s --> %s", "jit_fpr12", "fp12"));
			out.println(String.format("%16s --> %s", "jit_fpr13", "fp13"));
			out.println(String.format("%16s --> %s", "jit_fpr14", "fp14"));
			out.println(String.format("%16s --> %s", "jit_fpr15", "fp15"));
			out.println(String.format("%16s --> %s", "jit_fpr2", "fp2"));
			out.println(String.format("%16s --> %s", "jit_fpr3", "fp3"));
			out.println(String.format("%16s --> %s", "jit_fpr4", "fp4"));
			out.println(String.format("%16s --> %s", "jit_fpr5", "fp5"));
			out.println(String.format("%16s --> %s", "jit_fpr6", "fp6"));
			out.println(String.format("%16s --> %s", "jit_fpr7", "fp7"));
			out.println(String.format("%16s --> %s", "jit_fpr8", "fp8"));
			out.println(String.format("%16s --> %s", "jit_fpr9", "fp9"));
			out.println(String.format("%16s --> %s", "jit_r0", "r0"));
			out.println(String.format("%16s --> %s", "jit_r1", "r1"));
			out.println(String.format("%16s --> %s", "jit_r10", "r10"));
			out.println(String.format("%16s --> %s", "jit_r11", "r11"));
			out.println(String.format("%16s --> %s", "jit_r12", "r12"));
			out.println(String.format("%16s --> %s", "jit_r13", "r13"));
			out.println(String.format("%16s --> %s", "jit_r14", "r14"));
			out.println(String.format("%16s --> %s", "jit_r15", "r15"));
			out.println(String.format("%16s --> %s", "jit_r16", "r16"));
			out.println(String.format("%16s --> %s", "jit_r17", "r17"));
			out.println(String.format("%16s --> %s", "jit_r18", "r18"));
			out.println(String.format("%16s --> %s", "jit_r19", "r19"));
			out.println(String.format("%16s --> %s", "jit_r2", "r2"));
			out.println(String.format("%16s --> %s", "jit_r20", "r20"));
			out.println(String.format("%16s --> %s", "jit_r21", "r21"));
			out.println(String.format("%16s --> %s", "jit_r22", "r22"));
			out.println(String.format("%16s --> %s", "jit_r23", "r23"));
			out.println(String.format("%16s --> %s", "jit_r24", "r24"));
			out.println(String.format("%16s --> %s", "jit_r25", "r25"));
			out.println(String.format("%16s --> %s", "jit_r26", "r26"));
			out.println(String.format("%16s --> %s", "jit_r27", "r27"));
			out.println(String.format("%16s --> %s", "jit_r28", "r28"));
			out.println(String.format("%16s --> %s", "jit_r29", "r29"));
			out.println(String.format("%16s --> %s", "jit_r3", "r3"));
			out.println(String.format("%16s --> %s", "jit_r30", "r30"));
			out.println(String.format("%16s --> %s", "jit_r31", "r31"));
			out.println(String.format("%16s --> %s", "jit_r4", "r4"));
			out.println(String.format("%16s --> %s", "jit_r5", "r5"));
			out.println(String.format("%16s --> %s", "jit_r6", "r6"));
			out.println(String.format("%16s --> %s", "jit_r7", "r7"));
			out.println(String.format("%16s --> %s", "jit_r8", "r8"));
			out.println(String.format("%16s --> %s", "jit_r9", "r9"));
			out.println(String.format("%16s --> %s", "saved_fpr4", "fp4"));
			out.println(String.format("%16s --> %s", "saved_fpr6", "fp6"));
		}		
	}

	/**
	 * Prints registers for WINDOWS 64 BIT 
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 */
	private static void printRegistersForWindows64Bit(int level, PrintStream out)
	{
		if (level >= 1) {
			out.println(String.format("=========== Level %d Registers ===========\n", 1));
			out.println(String.format("%16s --> %s", "vmStruct", "RBP"));
			out.println(String.format("%16s --> %s", "sp", "RSP"));
			out.println(String.format("%16s --> %s", "arg0EA", "RCX"));
			out.println(String.format("%16s --> %s", "pc", "RSI"));
			out.println(String.format("%16s --> %s", "literals", "RBX"));
		}
		if (level >= 2) {
			out.println(String.format("=========== Level %d Registers ===========\n", 2));
			out.println(String.format("%16s --> %s", "allButLowByteZero", "RAX"));
			out.println(String.format("%16s --> %s", "bytecodes", "R14"));
			out.println(String.format("%16s --> %s", "cacheIndex", "RCX"));
			out.println(String.format("%16s --> %s", "callInAddress", "RSI"));
			out.println(String.format("%16s --> %s", "detailMessage", "RDI"));
			out.println(String.format("%16s --> %s", "exceptionIndex", "RAX"));
			out.println(String.format("%16s --> %s", "javaNewClass", "RDI"));
			out.println(String.format("%16s --> %s", "messageNumber", "RCX"));
			out.println(String.format("%16s --> %s", "methodHandle", "RCX"));
			out.println(String.format("%16s --> %s", "moduleName", "RDI"));
			out.println(String.format("%16s --> %s", "resolveBytecode", "RAX"));
			out.println(String.format("%16s --> %s", "resolveIndex", "RDI"));
			out.println(String.format("%16s --> %s", "resolveType", "RCX"));
			out.println(String.format("%16s --> %s", "returnAddress", "RDI"));
			out.println(String.format("%16s --> %s", "returnPoint", "RDI"));
			out.println(String.format("%16s --> %s", "returnSP", "RAX"));
			out.println(String.format("%16s --> %s", "sendArgs", "RDI"));
			out.println(String.format("%16s --> %s", "sendMethod", "RAX"));
			out.println(String.format("%16s --> %s", "sendReturn", "RCX"));
			out.println(String.format("%16s --> %s", "signalHandlerNewSP", "RAX"));
			out.println(String.format("%16s --> %s", "signalHandlerOldSP", "R12"));
			out.println(String.format("%16s --> %s", "signalHandlerParm1", "RCX"));
			out.println(String.format("%16s --> %s", "signalHandlerParm2", "RDX"));
			out.println(String.format("%16s --> %s", "signalHandlerParm3", "R8"));
			out.println(String.format("%16s --> %s", "signalHandlerParm4", "R9"));
			out.println(String.format("%16s --> %s", "syncObject", "RCX"));
		}
		if (level >= 3) {
			out.println(String.format("=========== Level %d Registers ===========\n", 3));
			out.println(String.format("%16s --> %s", "asmAddr1", "RAX"));
			out.println(String.format("%16s --> %s", "asmAddr2", "RCX"));
			out.println(String.format("%16s --> %s", "asmAddr3", "RBX"));
			out.println(String.format("%16s --> %s", "asmData1", "RDX"));
			out.println(String.format("%16s --> %s", "asmData2", "RSI"));
			out.println(String.format("%16s --> %s", "asmData3", "RDI"));
			out.println(String.format("%16s --> %s", "cmpxchgValue", "RAX"));
			out.println(String.format("%16s --> %s", "divideQuotient", "RAX"));
			out.println(String.format("%16s --> %s", "divideRemainder", "RDX"));
			out.println(String.format("%16s --> %s", "EAX", "RAX"));
			out.println(String.format("%16s --> %s", "EBP", "RBP"));
			out.println(String.format("%16s --> %s", "EBX", "RBX"));
			out.println(String.format("%16s --> %s", "ECX", "RCX"));
			out.println(String.format("%16s --> %s", "EDI", "RDI"));
			out.println(String.format("%16s --> %s", "EDX", "RDX"));
			out.println(String.format("%16s --> %s", "ESI", "RSI"));
			out.println(String.format("%16s --> %s", "ESP", "RSP"));
			out.println(String.format("%16s --> %s", "floatStatusWord", "RAX"));
			out.println(String.format("%16s --> %s", "fpParmCount", "RAX"));
			out.println(String.format("%16s --> %s", "inParm1", "RCX"));
			out.println(String.format("%16s --> %s", "inParm2", "RDX"));
			out.println(String.format("%16s --> %s", "inParm3", "R8"));
			out.println(String.format("%16s --> %s", "inParm4", "R9"));
			out.println(String.format("%16s --> %s", "instructionPointer", "RIP"));
			out.println(String.format("%16s --> %s", "machineBP", "RBP"));
			out.println(String.format("%16s --> %s", "machineSP", "RSP"));
			out.println(String.format("%16s --> %s", "memCount", "RCX"));
			out.println(String.format("%16s --> %s", "memDest", "RDI"));
			out.println(String.format("%16s --> %s", "memSource", "RSI"));
			out.println(String.format("%16s --> %s", "memValue", "RAX"));
			out.println(String.format("%16s --> %s", "outParm1", "RCX"));
			out.println(String.format("%16s --> %s", "outParm2", "RDX"));
			out.println(String.format("%16s --> %s", "outParm3", "R8"));
			out.println(String.format("%16s --> %s", "outParm4", "R9"));
			out.println(String.format("%16s --> %s", "R10D", "R10"));
			out.println(String.format("%16s --> %s", "R11D", "R11"));
			out.println(String.format("%16s --> %s", "R12D", "R12"));
			out.println(String.format("%16s --> %s", "R13D", "R13"));
			out.println(String.format("%16s --> %s", "R14D", "R14"));
			out.println(String.format("%16s --> %s", "R15D", "R15"));
			out.println(String.format("%16s --> %s", "R8D", "R8"));
			out.println(String.format("%16s --> %s", "R9D", "R9"));
			out.println(String.format("%16s --> %s", "rdtscHi", "RDX"));
			out.println(String.format("%16s --> %s", "rdtscLo", "RAX"));
			out.println(String.format("%16s --> %s", "return64hiRead", "RDX"));
			out.println(String.format("%16s --> %s", "return64hiWrite", "RDX"));
			out.println(String.format("%16s --> %s", "return64loRead", "RAX"));
			out.println(String.format("%16s --> %s", "return64loWrite", "RAX"));
			out.println(String.format("%16s --> %s", "returnValue", "RAX"));
			out.println(String.format("%16s --> %s", "shiftSource", "RCX"));
		}
		if (level >= 4) {
			out.println(String.format("=========== Level %d Registers ===========\n", 4));
			out.println(String.format("%16s --> %s", "", "RIP"));
			out.println(String.format("%16s --> %s", "_", "xmm15"));
			out.println(String.format("%16s --> %s", "_EAX", "RAX"));
			out.println(String.format("%16s --> %s", "_EBP", "RBP"));
			out.println(String.format("%16s --> %s", "_EBX", "RBX"));
			out.println(String.format("%16s --> %s", "_ECX", "RCX"));
			out.println(String.format("%16s --> %s", "_EDI", "RDI"));
			out.println(String.format("%16s --> %s", "_EDX", "RDX"));
			out.println(String.format("%16s --> %s", "_ESI", "RSI"));
			out.println(String.format("%16s --> %s", "_ESP", "RSP"));
			out.println(String.format("%16s --> %s", "_R10", "R10"));
			out.println(String.format("%16s --> %s", "_R10D", "R10"));
			out.println(String.format("%16s --> %s", "_R11", "R11"));
			out.println(String.format("%16s --> %s", "_R11D", "R11"));
			out.println(String.format("%16s --> %s", "_R12", "R12"));
			out.println(String.format("%16s --> %s", "_R12D", "R12"));
			out.println(String.format("%16s --> %s", "_R13", "R13"));
			out.println(String.format("%16s --> %s", "_R13D", "R13"));
			out.println(String.format("%16s --> %s", "_R14", "R14"));
			out.println(String.format("%16s --> %s", "_R14D", "R14"));
			out.println(String.format("%16s --> %s", "_R15", "R15"));
			out.println(String.format("%16s --> %s", "_R15D", "R15"));
			out.println(String.format("%16s --> %s", "_R8", "R8"));
			out.println(String.format("%16s --> %s", "_R8D", "R8"));
			out.println(String.format("%16s --> %s", "_R9", "R9"));
			out.println(String.format("%16s --> %s", "_R9D", "R9"));
			out.println(String.format("%16s --> %s", "_RAX", "RAX"));
			out.println(String.format("%16s --> %s", "_RBP", "RBP"));
			out.println(String.format("%16s --> %s", "_RBX", "RBX"));
			out.println(String.format("%16s --> %s", "_RCX", "RCX"));
			out.println(String.format("%16s --> %s", "_RDI", "RDI"));
			out.println(String.format("%16s --> %s", "_RDX", "RDX"));
			out.println(String.format("%16s --> %s", "_RIP", "RIP"));
			out.println(String.format("%16s --> %s", "_RSI", "RSI"));
			out.println(String.format("%16s --> %s", "_RSP", "RSP"));
			out.println(String.format("%16s --> %s", "_xmm0", "xmm0"));
			out.println(String.format("%16s --> %s", "_xmm1", "xmm1"));
			out.println(String.format("%16s --> %s", "_xmm10", "xmm10"));
			out.println(String.format("%16s --> %s", "_xmm11", "xmm11"));
			out.println(String.format("%16s --> %s", "_xmm12", "xmm12"));
			out.println(String.format("%16s --> %s", "_xmm13", "xmm13"));
			out.println(String.format("%16s --> %s", "_xmm14", "xmm14"));
			out.println(String.format("%16s --> %s", "_xmm15", "xmm15"));
			out.println(String.format("%16s --> %s", "_xmm2", "xmm2"));
			out.println(String.format("%16s --> %s", "_xmm3", "xmm3"));
			out.println(String.format("%16s --> %s", "_xmm4", "xmm4"));
			out.println(String.format("%16s --> %s", "_xmm5", "xmm5"));
			out.println(String.format("%16s --> %s", "_xmm6", "xmm6"));
			out.println(String.format("%16s --> %s", "_xmm7", "xmm7"));
			out.println(String.format("%16s --> %s", "_xmm8", "xmm8"));
			out.println(String.format("%16s --> %s", "_xmm9", "xmm9"));
			out.println(String.format("%16s --> %s", "jit_fpr0", "xmm0"));
			out.println(String.format("%16s --> %s", "jit_fpr1", "xmm1"));
			out.println(String.format("%16s --> %s", "jit_fpr10", "xmm10"));
			out.println(String.format("%16s --> %s", "jit_fpr11", "xmm11"));
			out.println(String.format("%16s --> %s", "jit_fpr12", "xmm12"));
			out.println(String.format("%16s --> %s", "jit_fpr13", "xmm13"));
			out.println(String.format("%16s --> %s", "jit_fpr14", "xmm14"));
			out.println(String.format("%16s --> %s", "jit_fpr15", "xmm15"));
			out.println(String.format("%16s --> %s", "jit_fpr2", "xmm2"));
			out.println(String.format("%16s --> %s", "jit_fpr3", "xmm3"));
			out.println(String.format("%16s --> %s", "jit_fpr4", "xmm4"));
			out.println(String.format("%16s --> %s", "jit_fpr5", "xmm5"));
			out.println(String.format("%16s --> %s", "jit_fpr6", "xmm6"));
			out.println(String.format("%16s --> %s", "jit_fpr7", "xmm7"));
			out.println(String.format("%16s --> %s", "jit_fpr8", "xmm8"));
			out.println(String.format("%16s --> %s", "jit_fpr9", "xmm9"));
			out.println(String.format("%16s --> %s", "jit_r10", "R10"));
			out.println(String.format("%16s --> %s", "jit_r11", "R11"));
			out.println(String.format("%16s --> %s", "jit_r12", "R12"));
			out.println(String.format("%16s --> %s", "jit_r13", "R13"));
			out.println(String.format("%16s --> %s", "jit_r14", "R14"));
			out.println(String.format("%16s --> %s", "jit_r15", "R15"));
			out.println(String.format("%16s --> %s", "jit_r8", "R8"));
			out.println(String.format("%16s --> %s", "jit_r9", "R9"));
			out.println(String.format("%16s --> %s", "jit_rax", "RAX"));
			out.println(String.format("%16s --> %s", "jit_rbx", "RBX"));
			out.println(String.format("%16s --> %s", "jit_rcx", "RCX"));
			out.println(String.format("%16s --> %s", "jit_rdi", "RDI"));
			out.println(String.format("%16s --> %s", "jit_rdx", "RDX"));
			out.println(String.format("%16s --> %s", "jit_rsi", "RSI"));
			out.println(String.format("%16s --> %s", "saved_fpr10", "xmm10"));
			out.println(String.format("%16s --> %s", "saved_fpr11", "xmm11"));
			out.println(String.format("%16s --> %s", "saved_fpr12", "xmm12"));
			out.println(String.format("%16s --> %s", "saved_fpr13", "xmm13"));
			out.println(String.format("%16s --> %s", "saved_fpr14", "xmm14"));
			out.println(String.format("%16s --> %s", "saved_fpr15", "xmm15"));
			out.println(String.format("%16s --> %s", "saved_fpr6", "xmm6"));
			out.println(String.format("%16s --> %s", "saved_fpr7", "xmm7"));
			out.println(String.format("%16s --> %s", "saved_fpr8", "xmm8"));
			out.println(String.format("%16s --> %s", "saved_fpr9", "xmm9"));
		}
	}

	/**
	 * Prints registers for WINDOWS 32 BIT X86
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 */
	private static void printRegistersForWindows32Bit(int level, PrintStream out)
	{
		if (level >= 1) {
			out.println(String.format("=========== Level %d Registers ===========\n", 1));
			out.println(String.format("%16s --> %s", "vmStruct", "EBP"));
			out.println(String.format("%16s --> %s", "sp", "ESP"));
			out.println(String.format("%16s --> %s", "arg0EA", "ECX"));
			out.println(String.format("%16s --> %s", "pc", "ESI"));
			out.println(String.format("%16s --> %s", "literals", "EBX"));
		}
		if (level >= 2) {
			out.println(String.format("=========== Level %d Registers ===========\n", 2));
			out.println(String.format("%16s --> %s", "allButLowByteZero", "EAX"));
			out.println(String.format("%16s --> %s", "bp", "EDX"));
			out.println(String.format("%16s --> %s", "cacheIndex", "ECX"));
			out.println(String.format("%16s --> %s", "callInAddress", "ESI"));
			out.println(String.format("%16s --> %s", "detailMessage", "EDI"));
			out.println(String.format("%16s --> %s", "exceptionIndex", "EAX"));
			out.println(String.format("%16s --> %s", "javaNewClass", "EDI"));
			out.println(String.format("%16s --> %s", "messageNumber", "ECX"));
			out.println(String.format("%16s --> %s", "methodHandle", "ECX"));
			out.println(String.format("%16s --> %s", "moduleName", "EDI"));
			out.println(String.format("%16s --> %s", "resolveBytecode", "EAX"));
			out.println(String.format("%16s --> %s", "resolveIndex", "EDI"));
			out.println(String.format("%16s --> %s", "resolveType", "ECX"));
			out.println(String.format("%16s --> %s", "returnAddress", "EDI"));
			out.println(String.format("%16s --> %s", "returnPoint", "EDI"));
			out.println(String.format("%16s --> %s", "returnSP", "EAX"));
			out.println(String.format("%16s --> %s", "sendArgs", "EDI"));
			out.println(String.format("%16s --> %s", "sendMethod", "EAX"));
			out.println(String.format("%16s --> %s", "sendReturn", "ECX"));
			out.println(String.format("%16s --> %s", "signalHandlerNewSP", "EDI"));
			out.println(String.format("%16s --> %s", "signalHandlerOldSP", "ESI"));
			out.println(String.format("%16s --> %s", "signalHandlerParm1", "EAX"));
			out.println(String.format("%16s --> %s", "signalHandlerParm2", "EBP"));
			out.println(String.format("%16s --> %s", "signalHandlerParm3", "ECX"));
			out.println(String.format("%16s --> %s", "signalHandlerParm4", "EDX"));
			out.println(String.format("%16s --> %s", "syncObject", "ECX"));
		}
		if (level >= 3) {
			out.println(String.format("=========== Level %d Registers ===========\n", 3));
			out.println(String.format("%16s --> %s", "asmAddr1", "EAX"));
			out.println(String.format("%16s --> %s", "asmAddr2", "ECX"));
			out.println(String.format("%16s --> %s", "asmAddr3", "EBX"));
			out.println(String.format("%16s --> %s", "asmData1", "EDX"));
			out.println(String.format("%16s --> %s", "asmData2", "ESI"));
			out.println(String.format("%16s --> %s", "asmData3", "EDI"));
			out.println(String.format("%16s --> %s", "cmpxchg8bcmpVal1", "EAX"));
			out.println(String.format("%16s --> %s", "cmpxchg8bcmpVal2", "EDX"));
			out.println(String.format("%16s --> %s", "cmpxchg8bdstVal1", "EBX"));
			out.println(String.format("%16s --> %s", "cmpxchg8bdstVal2", "ECX"));
			out.println(String.format("%16s --> %s", "cmpxchgValue", "EAX"));
			out.println(String.format("%16s --> %s", "divideQuotient", "EAX"));
			out.println(String.format("%16s --> %s", "divideRemainder", "EDX"));
			out.println(String.format("%16s --> %s", "floatStatusWord", "EAX"));
			out.println(String.format("%16s --> %s", "machineBP", "EBP"));
			out.println(String.format("%16s --> %s", "machineSP", "ESP"));
			out.println(String.format("%16s --> %s", "memberThis", "ECX"));
			out.println(String.format("%16s --> %s", "memCount", "ECX"));
			out.println(String.format("%16s --> %s", "memDest", "EDI"));
			out.println(String.format("%16s --> %s", "memSource", "ESI"));
			out.println(String.format("%16s --> %s", "memValue", "EAX"));
			out.println(String.format("%16s --> %s", "pascalParmCount", "EDI"));
			out.println(String.format("%16s --> %s", "rdtscHi", "EDX"));
			out.println(String.format("%16s --> %s", "rdtscLo", "EAX"));
			out.println(String.format("%16s --> %s", "return64hiRead", "EDX"));
			out.println(String.format("%16s --> %s", "return64hiWrite", "EDX"));
			out.println(String.format("%16s --> %s", "return64loRead", "EAX"));
			out.println(String.format("%16s --> %s", "return64loWrite", "EAX"));
			out.println(String.format("%16s --> %s", "returnValue", "EAX"));
			out.println(String.format("%16s --> %s", "shiftSource", "ECX"));
		}
		if (level >= 4) {
			out.println(String.format("=========== Level %d Registers ===========\n", 4));
			out.println(String.format("%16s --> %s", "_EAX", "EAX"));
			out.println(String.format("%16s --> %s", "_EBP", "EBP"));
			out.println(String.format("%16s --> %s", "_EBX", "EBX"));
			out.println(String.format("%16s --> %s", "_ECX", "ECX"));
			out.println(String.format("%16s --> %s", "_EDI", "EDI"));
			out.println(String.format("%16s --> %s", "_EDX", "EDX"));
			out.println(String.format("%16s --> %s", "_ESI", "ESI"));
			out.println(String.format("%16s --> %s", "_ESP", "ESP"));
			out.println(String.format("%16s --> %s", "_xmm0", "xmm0"));
			out.println(String.format("%16s --> %s", "_xmm1", "xmm1"));
			out.println(String.format("%16s --> %s", "_xmm2", "xmm2"));
			out.println(String.format("%16s --> %s", "_xmm3", "xmm3"));
			out.println(String.format("%16s --> %s", "_xmm4", "xmm4"));
			out.println(String.format("%16s --> %s", "_xmm5", "xmm5"));
			out.println(String.format("%16s --> %s", "_xmm6", "xmm6"));
			out.println(String.format("%16s --> %s", "_xmm7", "xmm7"));
			out.println(String.format("%16s --> %s", "jit_eax", "EAX"));
			out.println(String.format("%16s --> %s", "jit_ebx", "EBX"));
			out.println(String.format("%16s --> %s", "jit_ecx", "ECX"));
			out.println(String.format("%16s --> %s", "jit_edi", "EDI"));
			out.println(String.format("%16s --> %s", "jit_edx", "EDX"));
			out.println(String.format("%16s --> %s", "jit_esi", "ESI"));
			out.println(String.format("%16s --> %s", "jit_fpr0", "xmm0"));
			out.println(String.format("%16s --> %s", "jit_fpr1", "xmm1"));
			out.println(String.format("%16s --> %s", "jit_fpr2", "xmm2"));
			out.println(String.format("%16s --> %s", "jit_fpr3", "xmm3"));
			out.println(String.format("%16s --> %s", "jit_fpr4", "xmm4"));
			out.println(String.format("%16s --> %s", "jit_fpr5", "xmm5"));
			out.println(String.format("%16s --> %s", "jit_fpr6", "xmm6"));
			out.println(String.format("%16s --> %s", "jit_fpr7", "xmm7"));
		}
	}

	/**
	 * Prints registers for ZOS 64 BIT S390
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 */
	private static void printRegistersForZOS64BitS390(int level, PrintStream out)
	{
		if (level >= 1) {
			out.println(String.format("=========== Level %d Registers ===========\n", 1));
            out.println(String.format("%16s --> %s", "vmStruct", "r13"));
            out.println(String.format("%16s --> %s", "sp", "r5"));
            out.println(String.format("%16s --> %s", "arg0EA", "r10"));
            out.println(String.format("%16s --> %s", "pc", "r8"));
            out.println(String.format("%16s --> %s", "literals", "r9"));
       }
       if (level >= 2) {
            out.println(String.format("=========== Level %d Registers ===========\n", 2));
            out.println(String.format("%16s --> %s", "bytecodes", "r7"));
            out.println(String.format("%16s --> %s", "detailMessage", "r15"));
            out.println(String.format("%16s --> %s", "exceptionIndex", "r11"));
            out.println(String.format("%16s --> %s", "javaNewClass", "r15"));
            out.println(String.format("%16s --> %s", "messageNumber", "r10"));
            out.println(String.format("%16s --> %s", "methodHandle", "r10"));
            out.println(String.format("%16s --> %s", "moduleName", "r15"));
            out.println(String.format("%16s --> %s", "resolveBytecode", "r11"));
            out.println(String.format("%16s --> %s", "resolveIndex", "r15"));
            out.println(String.format("%16s --> %s", "resolveType", "r10"));
            out.println(String.format("%16s --> %s", "returnAddress", "r15"));
            out.println(String.format("%16s --> %s", "returnPoint", "r10"));
            out.println(String.format("%16s --> %s", "returnSP", "r11"));
            out.println(String.format("%16s --> %s", "sendArgs", "r15"));
            out.println(String.format("%16s --> %s", "sendMethod", "r11"));
            out.println(String.format("%16s --> %s", "sendReturn", "r10"));
            out.println(String.format("%16s --> %s", "syncObject", "r2"));
       }
       if (level >= 3) {
            out.println(String.format("=========== Level %d Registers ===========\n", 3));
            out.println(String.format("%16s --> %s", "asmAddr1", "r1"));
            out.println(String.format("%16s --> %s", "asmAddr2", "r2"));
            out.println(String.format("%16s --> %s", "asmAddr3", "r3"));
            out.println(String.format("%16s --> %s", "asmData1", "r8"));
            out.println(String.format("%16s --> %s", "asmData2", "r9"));
            out.println(String.format("%16s --> %s", "asmData3", "r10"));
            out.println(String.format("%16s --> %s", "cmpSlot0", "r0"));
            out.println(String.format("%16s --> %s", "cmpSlot1", "r1"));
            out.println(String.format("%16s --> %s", "divMulEven", "r0"));
            out.println(String.format("%16s --> %s", "divMulOdd", "r1"));
            out.println(String.format("%16s --> %s", "expandedDestEA", "r0"));
            out.println(String.format("%16s --> %s", "expandedSourceEA", "r1"));
            out.println(String.format("%16s --> %s", "globalOffsetTable", "r12"));
            out.println(String.format("%16s --> %s", "inParm1", "r1"));
            out.println(String.format("%16s --> %s", "inParm2", "r2"));
            out.println(String.format("%16s --> %s", "inParm3", "r3"));
            out.println(String.format("%16s --> %s", "linkRegister", "r7"));
            out.println(String.format("%16s --> %s", "machineSP", "r4"));
            out.println(String.format("%16s --> %s", "outParm1", "r1"));
            out.println(String.format("%16s --> %s", "outParm2", "r2"));
            out.println(String.format("%16s --> %s", "outParm3", "r3"));
            out.println(String.format("%16s --> %s", "pointerToStackParams", "r5"));
            out.println(String.format("%16s --> %s", "return64hiRead", "r2"));
            out.println(String.format("%16s --> %s", "return64hiWrite", "r2"));
            out.println(String.format("%16s --> %s", "return64loRead", "r3"));
            out.println(String.format("%16s --> %s", "return64loWrite", "r3"));
            out.println(String.format("%16s --> %s", "returnValue", "r3"));
            out.println(String.format("%16s --> %s", "swapSlot0", "r2"));
            out.println(String.format("%16s --> %s", "swapSlot1", "r3"));
            out.println(String.format("%16s --> %s", "zos_entrypoint", "r6"));
            out.println(String.format("%16s --> %s", "zos_environment", "r5"));
       }
       
       if (level >= 4) {
    	   	out.println(String.format("=========== Level %d Registers ===========\n", 4));
            out.println(String.format("%16s --> %s", "_fp0", "fp0"));
            out.println(String.format("%16s --> %s", "_fp1", "fp1"));
            out.println(String.format("%16s --> %s", "_fp10", "fp10"));
            out.println(String.format("%16s --> %s", "_fp11", "fp11"));
            out.println(String.format("%16s --> %s", "_fp12", "fp12"));
            out.println(String.format("%16s --> %s", "_fp13", "fp13"));
            out.println(String.format("%16s --> %s", "_fp14", "fp14"));
            out.println(String.format("%16s --> %s", "_fp15", "fp15"));
            out.println(String.format("%16s --> %s", "_fp2", "fp2"));
            out.println(String.format("%16s --> %s", "_fp3", "fp3"));
            out.println(String.format("%16s --> %s", "_fp4", "fp4"));
            out.println(String.format("%16s --> %s", "_fp5", "fp5"));
            out.println(String.format("%16s --> %s", "_fp6", "fp6"));
            out.println(String.format("%16s --> %s", "_fp7", "fp7"));
            out.println(String.format("%16s --> %s", "_fp8", "fp8"));
            out.println(String.format("%16s --> %s", "_fp9", "fp9"));
            out.println(String.format("%16s --> %s", "_r0", "r0"));
            out.println(String.format("%16s --> %s", "_r1", "r1"));
            out.println(String.format("%16s --> %s", "_r10", "r10"));
            out.println(String.format("%16s --> %s", "_r11", "r11"));
            out.println(String.format("%16s --> %s", "_r12", "r12"));
            out.println(String.format("%16s --> %s", "_r13", "r13"));
            out.println(String.format("%16s --> %s", "_r14", "r14"));
            out.println(String.format("%16s --> %s", "_r15", "r15"));
            out.println(String.format("%16s --> %s", "_r2", "r2"));
            out.println(String.format("%16s --> %s", "_r3", "r3"));
            out.println(String.format("%16s --> %s", "_r4", "r4"));
            out.println(String.format("%16s --> %s", "_r5", "r5"));
            out.println(String.format("%16s --> %s", "_r6", "r6"));
            out.println(String.format("%16s --> %s", "_r7", "r7"));
            out.println(String.format("%16s --> %s", "_r8", "r8"));
            out.println(String.format("%16s --> %s", "_r9", "r9"));
            out.println(String.format("%16s --> %s", "jit_fpr0", "fp0"));
            out.println(String.format("%16s --> %s", "jit_fpr1", "fp1"));
            out.println(String.format("%16s --> %s", "jit_fpr10", "fp10"));
            out.println(String.format("%16s --> %s", "jit_fpr11", "fp11"));
            out.println(String.format("%16s --> %s", "jit_fpr12", "fp12"));
            out.println(String.format("%16s --> %s", "jit_fpr13", "fp13"));
            out.println(String.format("%16s --> %s", "jit_fpr14", "fp14"));
            out.println(String.format("%16s --> %s", "jit_fpr15", "fp15"));
            out.println(String.format("%16s --> %s", "jit_fpr2", "fp2"));
            out.println(String.format("%16s --> %s", "jit_fpr3", "fp3"));
            out.println(String.format("%16s --> %s", "jit_fpr4", "fp4"));
            out.println(String.format("%16s --> %s", "jit_fpr5", "fp5"));
            out.println(String.format("%16s --> %s", "jit_fpr6", "fp6"));
            out.println(String.format("%16s --> %s", "jit_fpr7", "fp7"));
            out.println(String.format("%16s --> %s", "jit_fpr8", "fp8"));
            out.println(String.format("%16s --> %s", "jit_fpr9", "fp9"));
            out.println(String.format("%16s --> %s", "jit_r0", "r0"));
            out.println(String.format("%16s --> %s", "jit_r1", "r1"));
            out.println(String.format("%16s --> %s", "jit_r10", "r10"));
            out.println(String.format("%16s --> %s", "jit_r11", "r11"));
            out.println(String.format("%16s --> %s", "jit_r12", "r12"));
            out.println(String.format("%16s --> %s", "jit_r13", "r13"));
            out.println(String.format("%16s --> %s", "jit_r14", "r14"));
            out.println(String.format("%16s --> %s", "jit_r15", "r15"));
            out.println(String.format("%16s --> %s", "jit_r2", "r2"));
            out.println(String.format("%16s --> %s", "jit_r3", "r3"));
            out.println(String.format("%16s --> %s", "jit_r4", "r4"));
            out.println(String.format("%16s --> %s", "jit_r5", "r5"));
            out.println(String.format("%16s --> %s", "jit_r6", "r6"));
            out.println(String.format("%16s --> %s", "jit_r7", "r7"));
            out.println(String.format("%16s --> %s", "jit_r8", "r8"));
            out.println(String.format("%16s --> %s", "jit_r9", "r9"));
            out.println(String.format("%16s --> %s", "saved_fpr10", "fp10"));
            out.println(String.format("%16s --> %s", "saved_fpr11", "fp11"));
            out.println(String.format("%16s --> %s", "saved_fpr12", "fp12"));
            out.println(String.format("%16s --> %s", "saved_fpr13", "fp13"));
            out.println(String.format("%16s --> %s", "saved_fpr14", "fp14"));
            out.println(String.format("%16s --> %s", "saved_fpr15", "fp15"));
            out.println(String.format("%16s --> %s", "saved_fpr8", "fp8"));
            out.println(String.format("%16s --> %s", "saved_fpr9", "fp9"));
       }
	}

	/**
	 * Prints registers for ZOS 32 BIT S390
	 * @param level Level of registers to be printed.
	 * @param out PrintStream to print the output to the console.
	 */
	private static void printRegistersForZOS32BitS390(int level, PrintStream out)
	{
        if (level >= 1) {
            out.println(String.format("=========== Level %d Registers ===========\n", 1));
            out.println(String.format("%16s --> %s", "vmStruct", "r13"));
            out.println(String.format("%16s --> %s", "sp", "r5"));
            out.println(String.format("%16s --> %s", "arg0EA", "r10"));
            out.println(String.format("%16s --> %s", "pc", "r8"));
            out.println(String.format("%16s --> %s", "literals", "r9"));
        }
        if (level >= 2) {
            out.println(String.format("=========== Level %d Registers ===========\n", 2));
            out.println(String.format("%16s --> %s", "bytecodes", "r7"));
            out.println(String.format("%16s --> %s", "detailMessage", "r15"));
            out.println(String.format("%16s --> %s", "exceptionIndex", "r11"));
            out.println(String.format("%16s --> %s", "javaNewClass", "r15"));
            out.println(String.format("%16s --> %s", "messageNumber", "r10"));
            out.println(String.format("%16s --> %s", "methodHandle", "r10"));
            out.println(String.format("%16s --> %s", "moduleName", "r15"));
            out.println(String.format("%16s --> %s", "resolveBytecode", "r11"));
            out.println(String.format("%16s --> %s", "resolveIndex", "r15"));
            out.println(String.format("%16s --> %s", "resolveType", "r10"));
            out.println(String.format("%16s --> %s", "returnAddress", "r15"));
            out.println(String.format("%16s --> %s", "returnPoint", "r10"));
            out.println(String.format("%16s --> %s", "returnSP", "r11"));
            out.println(String.format("%16s --> %s", "sendArgs", "r15"));
            out.println(String.format("%16s --> %s", "sendMethod", "r11"));
            out.println(String.format("%16s --> %s", "sendReturn", "r10"));
            out.println(String.format("%16s --> %s", "syncObject", "r2"));
        }
        	if (level >= 3) {
            out.println(String.format("=========== Level %d Registers ===========\n", 3));
            out.println(String.format("%16s --> %s", "asmAddr1", "r1"));
            out.println(String.format("%16s --> %s", "asmAddr2", "r2"));
            out.println(String.format("%16s --> %s", "asmAddr3", "r3"));
            out.println(String.format("%16s --> %s", "asmData1", "r8"));
            out.println(String.format("%16s --> %s", "asmData2", "r9"));
            out.println(String.format("%16s --> %s", "asmData3", "r10"));
            out.println(String.format("%16s --> %s", "cmpSlot0", "r0"));
            out.println(String.format("%16s --> %s", "cmpSlot1", "r1"));
            out.println(String.format("%16s --> %s", "divMulEven", "r0"));
            out.println(String.format("%16s --> %s", "divMulOdd", "r1"));
            out.println(String.format("%16s --> %s", "expandedDestEA", "r0"));
            out.println(String.format("%16s --> %s", "expandedSourceEA", "r1"));
            out.println(String.format("%16s --> %s", "globalOffsetTable", "r12"));
            out.println(String.format("%16s --> %s", "inParm1", "r1"));
            out.println(String.format("%16s --> %s", "inParm2", "r2"));
            out.println(String.format("%16s --> %s", "inParm3", "r3"));
            out.println(String.format("%16s --> %s", "linkRegister", "r7"));
            out.println(String.format("%16s --> %s", "machineSP", "r4"));
            out.println(String.format("%16s --> %s", "outParm1", "r1"));
            out.println(String.format("%16s --> %s", "outParm2", "r2"));
            out.println(String.format("%16s --> %s", "outParm3", "r3"));
            out.println(String.format("%16s --> %s", "pointerToStackParams", "r5"));
            out.println(String.format("%16s --> %s", "return64hiRead", "r2"));
            out.println(String.format("%16s --> %s", "return64hiWrite", "r2"));
            out.println(String.format("%16s --> %s", "return64loRead", "r3"));
            out.println(String.format("%16s --> %s", "return64loWrite", "r3"));
            out.println(String.format("%16s --> %s", "returnValue", "r3"));
            out.println(String.format("%16s --> %s", "swapSlot0", "r2"));
            out.println(String.format("%16s --> %s", "swapSlot1", "r3"));
            out.println(String.format("%16s --> %s", "zos_entrypoint", "r6"));
            out.println(String.format("%16s --> %s", "zos_environment", "r5"));
        }
        if (level >= 4) {
            out.println(String.format("=========== Level %d Registers ===========\n", 4));
            out.println(String.format("%16s --> %s", "_fp0", "fp0"));
            out.println(String.format("%16s --> %s", "_fp1", "fp1"));
            out.println(String.format("%16s --> %s", "_fp10", "fp10"));
            out.println(String.format("%16s --> %s", "_fp11", "fp11"));
            out.println(String.format("%16s --> %s", "_fp12", "fp12"));
            out.println(String.format("%16s --> %s", "_fp13", "fp13"));
            out.println(String.format("%16s --> %s", "_fp14", "fp14"));
            out.println(String.format("%16s --> %s", "_fp15", "fp15"));
            out.println(String.format("%16s --> %s", "_fp2", "fp2"));
            out.println(String.format("%16s --> %s", "_fp3", "fp3"));
            out.println(String.format("%16s --> %s", "_fp4", "fp4"));
            out.println(String.format("%16s --> %s", "_fp5", "fp5"));
            out.println(String.format("%16s --> %s", "_fp6", "fp6"));
            out.println(String.format("%16s --> %s", "_fp7", "fp7"));
            out.println(String.format("%16s --> %s", "_fp8", "fp8"));
            out.println(String.format("%16s --> %s", "_fp9", "fp9"));
            out.println(String.format("%16s --> %s", "_r0", "r0"));
            out.println(String.format("%16s --> %s", "_r1", "r1"));
            out.println(String.format("%16s --> %s", "_r10", "r10"));
            out.println(String.format("%16s --> %s", "_r11", "r11"));
            out.println(String.format("%16s --> %s", "_r12", "r12"));
            out.println(String.format("%16s --> %s", "_r13", "r13"));
            out.println(String.format("%16s --> %s", "_r14", "r14"));
            out.println(String.format("%16s --> %s", "_r15", "r15"));
            out.println(String.format("%16s --> %s", "_r16", "r16"));
            out.println(String.format("%16s --> %s", "_r17", "r17"));
            out.println(String.format("%16s --> %s", "_r18", "r18"));
            out.println(String.format("%16s --> %s", "_r19", "r19"));
            out.println(String.format("%16s --> %s", "_r2", "r2"));
            out.println(String.format("%16s --> %s", "_r20", "r20"));
            out.println(String.format("%16s --> %s", "_r21", "r21"));
            out.println(String.format("%16s --> %s", "_r22", "r22"));
            out.println(String.format("%16s --> %s", "_r23", "r23"));
            out.println(String.format("%16s --> %s", "_r24", "r24"));
            out.println(String.format("%16s --> %s", "_r25", "r25"));
            out.println(String.format("%16s --> %s", "_r26", "r26"));
            out.println(String.format("%16s --> %s", "_r27", "r27"));
            out.println(String.format("%16s --> %s", "_r28", "r28"));
            out.println(String.format("%16s --> %s", "_r29", "r29"));
            out.println(String.format("%16s --> %s", "_r3", "r3"));
            out.println(String.format("%16s --> %s", "_r30", "r30"));
            out.println(String.format("%16s --> %s", "_r31", "r31"));
            out.println(String.format("%16s --> %s", "_r4", "r4"));
            out.println(String.format("%16s --> %s", "_r5", "r5"));
            out.println(String.format("%16s --> %s", "_r6", "r6"));
            out.println(String.format("%16s --> %s", "_r7", "r7"));
            out.println(String.format("%16s --> %s", "_r8", "r8"));
            out.println(String.format("%16s --> %s", "_r9", "r9"));
            out.println(String.format("%16s --> %s", "jit_fpr0", "fp0"));
            out.println(String.format("%16s --> %s", "jit_fpr1", "fp1"));
            out.println(String.format("%16s --> %s", "jit_fpr10", "fp10"));
            out.println(String.format("%16s --> %s", "jit_fpr11", "fp11"));
            out.println(String.format("%16s --> %s", "jit_fpr12", "fp12"));
            out.println(String.format("%16s --> %s", "jit_fpr13", "fp13"));
            out.println(String.format("%16s --> %s", "jit_fpr14", "fp14"));
            out.println(String.format("%16s --> %s", "jit_fpr15", "fp15"));
            out.println(String.format("%16s --> %s", "jit_fpr2", "fp2"));
            out.println(String.format("%16s --> %s", "jit_fpr3", "fp3"));
            out.println(String.format("%16s --> %s", "jit_fpr4", "fp4"));
            out.println(String.format("%16s --> %s", "jit_fpr5", "fp5"));
            out.println(String.format("%16s --> %s", "jit_fpr6", "fp6"));
            out.println(String.format("%16s --> %s", "jit_fpr7", "fp7"));
            out.println(String.format("%16s --> %s", "jit_fpr8", "fp8"));
            out.println(String.format("%16s --> %s", "jit_fpr9", "fp9"));
            out.println(String.format("%16s --> %s", "jit_r0", "r0"));
            out.println(String.format("%16s --> %s", "jit_r1", "r1"));
            out.println(String.format("%16s --> %s", "jit_r10", "r10"));
            out.println(String.format("%16s --> %s", "jit_r11", "r11"));
            out.println(String.format("%16s --> %s", "jit_r12", "r12"));
            out.println(String.format("%16s --> %s", "jit_r13", "r13"));
            out.println(String.format("%16s --> %s", "jit_r14", "r14"));
            out.println(String.format("%16s --> %s", "jit_r15", "r15"));
            out.println(String.format("%16s --> %s", "jit_r16", "r16"));
            out.println(String.format("%16s --> %s", "jit_r17", "r17"));
            out.println(String.format("%16s --> %s", "jit_r18", "r18"));
            out.println(String.format("%16s --> %s", "jit_r19", "r19"));
            out.println(String.format("%16s --> %s", "jit_r2", "r2"));
            out.println(String.format("%16s --> %s", "jit_r20", "r20"));
            out.println(String.format("%16s --> %s", "jit_r21", "r21"));
            out.println(String.format("%16s --> %s", "jit_r22", "r22"));
            out.println(String.format("%16s --> %s", "jit_r23", "r23"));
            out.println(String.format("%16s --> %s", "jit_r24", "r24"));
            out.println(String.format("%16s --> %s", "jit_r25", "r25"));
            out.println(String.format("%16s --> %s", "jit_r26", "r26"));
            out.println(String.format("%16s --> %s", "jit_r27", "r27"));
            out.println(String.format("%16s --> %s", "jit_r28", "r28"));
            out.println(String.format("%16s --> %s", "jit_r29", "r29"));
            out.println(String.format("%16s --> %s", "jit_r3", "r3"));
            out.println(String.format("%16s --> %s", "jit_r30", "r30"));
            out.println(String.format("%16s --> %s", "jit_r31", "r31"));
            out.println(String.format("%16s --> %s", "jit_r4", "r4"));
            out.println(String.format("%16s --> %s", "jit_r5", "r5"));
            out.println(String.format("%16s --> %s", "jit_r6", "r6"));
            out.println(String.format("%16s --> %s", "jit_r7", "r7"));
            out.println(String.format("%16s --> %s", "jit_r8", "r8"));
            out.println(String.format("%16s --> %s", "jit_r9", "r9"));
            out.println(String.format("%16s --> %s", "saved_fpr10", "fp10"));
            out.println(String.format("%16s --> %s", "saved_fpr11", "fp11"));
            out.println(String.format("%16s --> %s", "saved_fpr12", "fp12"));
            out.println(String.format("%16s --> %s", "saved_fpr13", "fp13"));
            out.println(String.format("%16s --> %s", "saved_fpr14", "fp14"));
            out.println(String.format("%16s --> %s", "saved_fpr15", "fp15"));
            out.println(String.format("%16s --> %s", "saved_fpr8", "fp8"));
            out.println(String.format("%16s --> %s", "saved_fpr9", "fp9"));
        }
	}	
}

