/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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

import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_ITERATE_FRAMES;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_ITERATE_O_SLOTS;
import static com.ibm.j9ddr.vm29.structure.J9Consts.J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;

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
import com.ibm.j9ddr.vm29.j9.stackwalker.TerseStackWalkerCallbacks;
import com.ibm.j9ddr.vm29.j9.stackwalker.WalkState;
import com.ibm.j9ddr.vm29.pointer.U8Pointer;
import com.ibm.j9ddr.vm29.pointer.UDATAPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9MethodPointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMEntryLocalStoragePointer;
import com.ibm.j9ddr.vm29.pointer.generated.J9VMThreadPointer;

public class StackWalkCommand extends Command 
{

	public StackWalkCommand()
	{
		addCommand("stack", "<thread>", "Walks the Java stack for <thread>");
		addCommand("stackslots", "<thread>", "Walks the Java stack (including objects) for <thread>");
	}
	
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException 
	{
		try {
			UDATAPointer sp = UDATAPointer.NULL;
			UDATAPointer arg0EA = UDATAPointer.NULL;
			U8Pointer pc = U8Pointer.NULL;
			J9MethodPointer literals = J9MethodPointer.NULL;
			J9VMEntryLocalStoragePointer entryLocalStorage = J9VMEntryLocalStoragePointer.NULL;
			
			String[] realArgs = null;			
			if (args.length != 0) {
				realArgs = args[0].split(",");
			}
			if (args.length == 0 || !((realArgs.length == 1) || (realArgs.length == 5) || (realArgs.length == 6))) {
				CommandUtils.dbgPrint(out, "Usage:\n");
				CommandUtils.dbgPrint(out, "\t!stack thread\n");
				CommandUtils.dbgPrint(out, "\t!stack thread,sp,a0,pc,literals\n");
				CommandUtils.dbgPrint(out, "\t!stack thread,sp,a0,pc,literals,els\n");
				CommandUtils.dbgPrint(out, "\tUse !stackslots instead of !stack to see slot values\n");
				if (J9BuildFlags.interp_nativeSupport) {
					CommandUtils.dbgPrint(out, "\tUse !jitstack or !jitstackslots to start the walk at a JIT frame\n");
				}
				// dbgPrintRegisters(1);
				return;
			}

			long address = CommandUtils.parsePointer(realArgs[0], J9BuildFlags.env_data64);
			if (0 == address) {
				/* Parse error is captured in CommandUtils.parsePointer method and message is printed */
				return;
			}

			J9VMThreadPointer thread = J9VMThreadPointer.cast(address);

			StackWalkerUtils.enableVerboseLogging(3, out);

			WalkState walkState = new WalkState();
			walkState.flags = J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;
			
			if (realArgs.length >= 5) {
				address = CommandUtils.parsePointer(realArgs[1], J9BuildFlags.env_data64);
				sp = UDATAPointer.cast(address);
				
				address = CommandUtils.parsePointer(realArgs[2], J9BuildFlags.env_data64);
				arg0EA = UDATAPointer.cast(address);
				
				address = CommandUtils.parsePointer(realArgs[3], J9BuildFlags.env_data64);
				pc = U8Pointer.cast(address);

				address = CommandUtils.parsePointer(realArgs[4], J9BuildFlags.env_data64);
				literals = J9MethodPointer.cast(address);
			} else {
				sp = thread.sp();
				arg0EA = thread.arg0EA();
				pc = thread.pc();
				literals = thread.literals();
			}

			if (realArgs.length >= 6) {
				address = CommandUtils.parsePointer(realArgs[5], J9BuildFlags.env_data64);
				entryLocalStorage = J9VMEntryLocalStoragePointer.cast(address);
			} else {
				if (J9BuildFlags.interp_nativeSupport) {
					entryLocalStorage = thread.entryLocalStorage();
				}
			}
			
			if (command.equalsIgnoreCase("!stackslots")) {
				walkState.flags |= J9_STACKWALK_ITERATE_O_SLOTS;
				// 100 is highly arbitrary but basically means "print everything".
				// It is used in jextract where the message levels have been copied
				// from to begin with, so it should mean we get the same output.
				StackWalkerUtils.enableVerboseLogging(100, out);
				walkState.callBacks = new BaseStackWalkerCallbacks();
			} else {				
				StackWalkerUtils.enableVerboseLogging(0, out);
				walkState.callBacks = new TerseStackWalkerCallbacks();
				walkState.flags |= J9_STACKWALK_ITERATE_FRAMES;
			}

			walkState.walkThread = thread;

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
