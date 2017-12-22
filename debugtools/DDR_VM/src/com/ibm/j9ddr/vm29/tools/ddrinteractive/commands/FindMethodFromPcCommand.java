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
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9JavaVMHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9MethodHelper;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;

public class FindMethodFromPcCommand extends Command 
{

	public FindMethodFromPcCommand()
	{
		addCommand("findmethodfrompc", "<pc>", "find the method corresponding to pc");
	}


	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			long address = CommandUtils.parsePointer(args[0], J9BuildFlags.env_data64);
			
			U8Pointer pc = U8Pointer.cast(address);
			J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());

			if (pc.isNull()) {
				CommandUtils.dbgPrint(out, "bad or missing PC\n");
				return;
			}

			CommandUtils.dbgPrint(out, "Searching for PC=%s in VM=%s...\n", pc.getHexAddress(), vm.getHexAddress());
			J9MethodPointer result = J9JavaVMHelper.getMethodFromPC(vm, pc);
			if (!result.isNull()) {
				CommandUtils.dbgPrint(out, "!j9method %s %s\n", result.getHexAddress(), J9MethodHelper.getName(result));
				CommandUtils.dbgPrint(out, "Bytecode PC offset = %s\n", pc.sub(result.bytecodes()).getHexValue());	
			} else {
				CommandUtils.dbgPrint(out, "Not found\n");
			}

		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
