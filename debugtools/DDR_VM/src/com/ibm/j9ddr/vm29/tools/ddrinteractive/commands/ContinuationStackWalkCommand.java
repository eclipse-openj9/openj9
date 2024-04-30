/*
 * Copyright IBM Corp. and others 2023
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
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0-only WITH Classpath-exception-2.0 OR GPL-2.0-only WITH OpenJDK-assembly-exception-1.0
 */
package com.ibm.j9ddr.vm29.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.CommandUtils;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.vm29.j9.DataType;
import com.ibm.j9ddr.vm29.j9.stackwalker.BaseStackWalkerCallbacks;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalkResult;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalker;
import com.ibm.j9ddr.vm29.j9.stackwalker.StackWalkerUtils;
import com.ibm.j9ddr.vm29.j9.stackwalker.TerseStackWalkerCallbacks;
import com.ibm.j9ddr.vm29.j9.stackwalker.WalkState;
import com.ibm.j9ddr.vm29.pointer.generated.J9BuildFlags;
import com.ibm.j9ddr.vm29.pointer.generated.J9JavaVMPointer;
import com.ibm.j9ddr.vm29.pointer.helper.J9RASHelper;
import com.ibm.j9ddr.vm29.structure.J9Consts;
import com.ibm.j9ddr.vm29.tools.ddrinteractive.JavaVersionHelper;

/**
 * DDR command to walk the stack frames of a continuation.
 */
public class ContinuationStackWalkCommand extends Command {
	public ContinuationStackWalkCommand() {
		addCommand("continuationstack", "<continuation>", "Walks the Java stack for <continuation>");
		addCommand("continuationstackslots", "<continuation>", "Walks the Java stack (including objects) for <continuation>");
	}

	@Override
	public void run(String command, String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		try {
			final J9JavaVMPointer vm = J9RASHelper.getVM(DataType.getJ9RASPointer());

			if (!JavaVersionHelper.ensureMinimumJavaVersion(19, vm, out)) {
				return;
			}

			if (args.length != 1) {
				CommandUtils.dbgPrint(out, "Exactly one argument (address) must be provided.\n");
				return;
			}
			long address = CommandUtils.parsePointer(args[0], J9BuildFlags.J9VM_ENV_DATA64);
			if (0 == address) {
				CommandUtils.dbgPrint(out, "Address must be non-zero.\n");
				return;
			}

			StackWalkerUtils.enableVerboseLogging(3, out);

			WalkState walkState = new WalkState();

			walkState.flags = J9Consts.J9_STACKWALK_RECORD_BYTECODE_PC_OFFSET;

			if (command.equalsIgnoreCase("!continuationstackslots")) {
				walkState.flags |= J9Consts.J9_STACKWALK_ITERATE_O_SLOTS;
				// 100 is highly arbitrary but basically means "print everything".
				// It is used in jextract where the message levels have been copied
				// from to begin with, so it should mean we get the same output.
				StackWalkerUtils.enableVerboseLogging(100, out);
				walkState.callBacks = new BaseStackWalkerCallbacks();
			} else {
				StackWalkerUtils.enableVerboseLogging(0, out);
				walkState.callBacks = new TerseStackWalkerCallbacks();
				walkState.flags |= J9Consts.J9_STACKWALK_ITERATE_FRAMES;
			}

			StackWalkResult result = StackWalker.walkStackFrames(walkState, address);

			if (result != StackWalkResult.NONE) {
				out.println("Stack walk result: " + result);
			}

			StackWalkerUtils.disableVerboseLogging();

			out.flush();
		} catch (CorruptDataException e) {
			throw new DDRInteractiveCommandException(e);
		}
	}
}
