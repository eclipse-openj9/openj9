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

import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_ITERATE_O_SLOTS;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_START_AT_JIT_FRAME;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.stackwalker.BaseStackWalkerCallbacks;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalkResult;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalker;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalkerUtils;
import com.ibm.j9ddr.vm29.j9.stackwalker.WalkState;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMEntryLocalStoragePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;
import com.ibm.j9ddr.vm29.types.UDATA;

public class JitstackCommand extends Command 
{

	public JitstackCommand()
	{
		addCommand("jitstack", "<thread>,<sp>,<pc>", "Dump jit stack");
		addCommand("jitstackslots", "<thread>,<sp>,<pc>", "Dump jit stack slots");
	}

	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		if(!J9BuildFlags.interp_nativeSupport) {
			CommandUtils.dbgPrint(out, "No JIT in this build\n");
			return;
		}
		
		try {
			String[] realArgs = null;
			if (args.length != 0) {
				realArgs = args[0].split(",");
			}

			if (args.length == 0 || !((realArgs.length == 3) || (realArgs.length == 4))) {
				CommandUtils.dbgPrint(out, "Usage:\n");
				CommandUtils.dbgPrint(out, "\t!jitstack thread,sp,pc\n");
				CommandUtils.dbgPrint(out, "\t!jitstack thread,sp,pc,els\n");
				CommandUtils.dbgPrint(out, "\tUse !jitstackslots instead of !jitstack to see slot values\n");
				return;
			}

			long address = CommandUtils.parsePointer(realArgs[0], J9BuildFlags.env_data64);
			
			J9VMThreadPointer thread = J9VMThreadPointer.cast(address);
			StackWalkerUtils.enableVerboseLogging(2, out);
			WalkState walkState = new WalkState();

			address = CommandUtils.parsePointer(realArgs[1], J9BuildFlags.env_data64);

			UDATAPointer sp = UDATAPointer.cast(address);
			
			address = CommandUtils.parsePointer(realArgs[2], J9BuildFlags.env_data64);
			
			U8Pointer pc = U8Pointer.cast(address);

			UDATAPointer arg0EA = UDATAPointer.NULL;
			J9MethodPointer literals = J9MethodPointer.NULL;
			J9VMEntryLocalStoragePointer entryLocalStorage = J9VMEntryLocalStoragePointer.NULL;

			if (realArgs.length == 4) {
				address = CommandUtils.parsePointer(realArgs[3], J9BuildFlags.env_data64);
				entryLocalStorage = J9VMEntryLocalStoragePointer.cast(address);
			} else {
				entryLocalStorage = thread.entryLocalStorage();
			}

			walkState.flags = J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;
			walkState.flags |= J9_STACKWALK_START_AT_JIT_FRAME;
			
			if (command.equalsIgnoreCase("!jitstackslots")) {
				walkState.flags |= J9_STACKWALK_ITERATE_O_SLOTS;
				// 100 is highly arbitrary but basically means "print everything".
				// It is used in jextract where the message levels have been copied
				// from to begin with, so it should mean we get the same output.
				StackWalkerUtils.enableVerboseLogging(100, out);
			}

			walkState.walkThread = thread;
			walkState.callBacks = new BaseStackWalkerCallbacks();
			walkState.frameFlags = new UDATA(0);

			StackWalkResult result = StackWalker.walkStackFrames(walkState, sp, arg0EA, pc, literals, entryLocalStorage);

			if (result != StackWalkResult.NONE ) {
				out.println("Stack walk result: " + result);
			}

			StackWalkerUtils.disableVerboseLogging();

			out.flush();
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
