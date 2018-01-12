/*******************************************************************************
 * Copyright (c) 2001, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.JITLook;
import com.ibm.j9ddr.vm29.pointer.generated.J9JITExceptionTablePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9UTF8Helper;
import com.ibm.j9ddr.vm29.types.UDATA;

public class JitMetadataFromPcCommand extends Command 
{
	public JitMetadataFromPcCommand()
	{
		addCommand("jitmetadatafrompc", "<pc>", "Show jit method metadata for PC");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			UDATA searchValue = new UDATA(Long.decode(args[0]));
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());
			J9JITExceptionTablePointer metaData = JITLook.jit_artifact_search(vm.jitConfig().translationArtifacts(), searchValue);

			if (!metaData.isNull()) {
				dbgext_j9jitexceptiontable(out, metaData);
			}
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}

	}

	void dbgext_j9jitexceptiontable(PrintStream out, J9JITExceptionTablePointer parm) throws CorruptDataException 
	{
		/* print individual fields */
		CommandUtils.dbgPrint(out, "J9JITExceptionTable at %s {\n", parm.getHexAddress());
		CommandUtils.dbgPrint(out, "    struct J9UTF8* className = !j9utf8 %s   // %s\n", parm.className().getHexAddress(), parm.className().isNull() ? "NULL" : J9UTF8Helper.stringValue(parm.className()));
		CommandUtils.dbgPrint(out, "    struct J9UTF8* methodName = !j9utf8 %s   // %s\n", parm.methodName().getHexAddress(), parm.methodName().isNull() ? "NULL" : J9UTF8Helper.stringValue(parm.methodName()));
		CommandUtils.dbgPrint(out, "    struct J9UTF8* methodSignature = !j9utf8 %s   // %s\n", parm.methodSignature().getHexAddress(), parm.methodSignature().isNull() ? "NULL" :J9UTF8Helper.stringValue(parm.methodSignature()));
		CommandUtils.dbgPrint(out, "    struct J9ConstantPool* constantPool = !j9constantpool %s \n", parm.constantPool().getHexAddress());
		CommandUtils.dbgPrint(out, "    struct J9Method* ramMethod = !j9method %s   // %s\n", parm.ramMethod().getHexAddress(), J9MethodHelper.getName(parm.ramMethod()));
		CommandUtils.dbgPrint(out, "    UDATA parm.startPC = %s;\n", parm.startPC().getHexValue());
		CommandUtils.dbgPrint(out, "    UDATA parm.endWarmPC = %s;\n", parm.endWarmPC().getHexValue());
		CommandUtils.dbgPrint(out, "    UDATA parm.startColdPC = %s;\n", parm.startColdPC().getHexValue());
		CommandUtils.dbgPrint(out, "    UDATA parm.endPC = %s;\n", parm.endPC().getHexValue());
		CommandUtils.dbgPrint(out, "    UDATA parm.totalFrameSize = %s;\n", parm.totalFrameSize().getHexValue());
		CommandUtils.dbgPrint(out, "    I_16 parm.slots = %s;\n", parm.slots().getHexValue());
		CommandUtils.dbgPrint(out, "    I_16 parm.scalarTempSlots = %s;\n", parm.scalarTempSlots().getHexValue());
		CommandUtils.dbgPrint(out, "    I_16 parm.objectTempSlots = %s;\n", parm.objectTempSlots().getHexValue());
		CommandUtils.dbgPrint(out, "    U_16 parm.prologuePushes = %s;\n", parm.prologuePushes().getHexValue());
		CommandUtils.dbgPrint(out, "    I_16 parm.tempOffset = %s;\n", parm.tempOffset().getHexValue());
		CommandUtils.dbgPrint(out, "    U_16 parm.numExcptionRanges = %s;\n", parm.numExcptionRanges().getHexValue());
		CommandUtils.dbgPrint(out, "    I_32 parm.size = %s;\n", parm.size().getHexValue());
		CommandUtils.dbgPrint(out, "    UDATA parm.registerSaveDescription = %s;\n", parm.registerSaveDescription().getHexValue());
		CommandUtils.dbgPrint(out, "    void* gcStackAtlas = !void %s \n", parm.gcStackAtlas().getHexAddress());
		CommandUtils.dbgPrint(out, "    void* inlinedCalls = !void %s \n", parm.inlinedCalls().getHexAddress());
		CommandUtils.dbgPrint(out, "    void* bodyInfo = !void %s \n", parm.bodyInfo().getHexAddress());
		CommandUtils.dbgPrint(out, "    struct J9JITExceptionTable* nextMethod = !j9jitexceptiontable %s \n", parm.nextMethod().getHexAddress());
		CommandUtils.dbgPrint(out, "    struct J9JITExceptionTable* prevMethod = !j9jitexceptiontable %s \n", parm.prevMethod().getHexAddress());
		CommandUtils.dbgPrint(out, "    void* debugSlot1 = !void %s \n", parm.debugSlot1().getHexAddress());
		CommandUtils.dbgPrint(out, "    void* debugSlot2 = !void %s \n", parm.debugSlot2().getHexAddress());
		CommandUtils.dbgPrint(out, "    void* osrInfo = !void %s \n", parm.osrInfo().getHexAddress());
		CommandUtils.dbgPrint(out, "    void* runtimeAssumptionList = !void %s \n", parm.runtimeAssumptionList().getHexAddress());
		CommandUtils.dbgPrint(out, "    I_32 parm.hotness = %s;\n", parm.hotness().getHexValue());
		CommandUtils.dbgPrint(out, "    UDATA parm.codeCacheAlloc = %s;\n", parm.codeCacheAlloc().getHexValue());
		CommandUtils.dbgPrint(out, "    void* gpuCode = !void %s \n", parm.gpuCode().getHexAddress());
		CommandUtils.dbgPrint(out, "    void* riData = !void %s \n", parm.riData().getHexAddress());
		CommandUtils.dbgPrint(out, "}\n");
	}
}
