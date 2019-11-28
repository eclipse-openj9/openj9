/*******************************************************************************
 * Copyright (c) 2004, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive.commands;

import java.io.PrintStream;

import com.ibm.j9ddr.corereaders.memory.MemoryFault;
import com.ibm.j9ddr.corereaders.memory.MemoryRecord;
import com.ibm.j9ddr.corereaders.memory.PatchedMemoryList;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.ExpressionEvaluator;
import com.ibm.j9ddr.tools.ddrinteractive.ExpressionEvaluatorException;

/**
 * Commands to patch the memory.
 * <p>
 * @author Manqing LI, IBM Canada
 *
 */
public class PatchMemoryCommand extends Command {

	public static final String COMMAND_SET = "setMemory";
	public static final String ARGUMENTS_SET = "<targetAddress> <byte>[ <byte> ...]";
	public static final String ARGUMENTS_DESCRIPTION_SET = "Set the memory starting at <targetAdderss> to the specified bytes.";
	
	public static final String COMMAND_REMAP = "remapMemory";
	public static final String COMMAND_REMAP_SHORT = "remap";
	public static final String ARGUMENTS_REMAP = "<sourceAddress> <length> <targetAddress>";
	public static final String ARGUMENTS_DESCRIPTION_REMAP = "Remap <length> bytes from <sourceAddress> to <targetAddress>.";

	public static final String COMMAND_CLEAR = "clearPatchedMemory";
	public static final String ARGUMENTS_CLEAR = "[<address>[,<length>] ...]";
	public static final String ARGUMENTS_DESCRIPTION_CLEAR = "Clear all the patched memory, or only the patched memory for the specified address.";

	public static final String COMMAND_LIST = "listPatchedMemory";
	public static final String ARGUMENTS_LIST = "";
	public static final String ARGUMENTS_DESCRIPTION_LIST = "Display all the current patched memory records.";

	public static final String COMMAND_VERBOSE = "verbosePatchedMemory";
	public static final String ARGUMENTS_VERBOSE = "[<No>|<False>]";
	public static final String ARGUMENTS_DESCRIPTION_VERBOSE = "Print verbose messages for the patched memory, or do not print verbose messages if <No> or <False> is specified.";

	public PatchMemoryCommand() {
		addCommand(COMMAND_SET, ARGUMENTS_SET, ARGUMENTS_DESCRIPTION_SET);
		addCommand(COMMAND_REMAP, ARGUMENTS_REMAP, ARGUMENTS_DESCRIPTION_REMAP);
		addCommand(COMMAND_REMAP_SHORT, ARGUMENTS_REMAP, ARGUMENTS_DESCRIPTION_REMAP);
		addCommand(COMMAND_CLEAR, ARGUMENTS_CLEAR, ARGUMENTS_DESCRIPTION_CLEAR);
		addCommand(COMMAND_LIST, ARGUMENTS_LIST, ARGUMENTS_DESCRIPTION_LIST);
		addCommand(COMMAND_VERBOSE, ARGUMENTS_VERBOSE, ARGUMENTS_VERBOSE);
	}
	
	public static final String HELP = "Operations in related to the patched memory:\n" +
			"\t" + COMMAND_SET + " " + ARGUMENTS_SET + " : " + ARGUMENTS_DESCRIPTION_SET + "\n" + 
			"\t" + COMMAND_REMAP + "/" + COMMAND_REMAP_SHORT + " " + ARGUMENTS_REMAP + " : " + ARGUMENTS_DESCRIPTION_REMAP + "\n" + 
			"\t" + COMMAND_CLEAR + " " + ARGUMENTS_CLEAR + " : " + ARGUMENTS_DESCRIPTION_CLEAR + "\n" + 
			"\t" + COMMAND_LIST + " " + ARGUMENTS_LIST + " : " + ARGUMENTS_DESCRIPTION_LIST + "\n" + 
			"\t" + COMMAND_VERBOSE + " " + ARGUMENTS_VERBOSE + " : " + ARGUMENTS_VERBOSE;

	@Override
	public void run(String command, String[] args, Context context, PrintStream out)
			throws DDRInteractiveCommandException 
	{
		if (command.startsWith("!")) {
			command = command.substring(1);
		}
		if (command.equalsIgnoreCase(COMMAND_CLEAR)) {
			try {
				clearExistingComponentCaches();
				clearPatchedMemory(args, out);
				PatchedMemoryList.print(out);
			} catch (ExpressionEvaluatorException e) {
				throw new DDRInteractiveCommandException(e);
			}
			return;
		}

		if (command.equalsIgnoreCase(COMMAND_LIST)) {
			PatchedMemoryList.print(out);
			return;
		}

		if (command.equalsIgnoreCase(COMMAND_VERBOSE)) {
			if (args.length > 0 && (args[0].equalsIgnoreCase("false") || args[0].equalsIgnoreCase("no"))) {
				PatchedMemoryList.verbose = false;
			} else {
				PatchedMemoryList.verbose = true;
				PatchedMemoryList.print(out);
			}
			return;
		}
		
		if (command.equalsIgnoreCase(COMMAND_SET)) {
			try {
				clearExistingComponentCaches();
				setPatchedMemory(args, out);
				PatchedMemoryList.print(out);
			} catch (ExpressionEvaluatorException e) {
				throw new DDRInteractiveCommandException(e);
			}
			return;
		}
		
		if (command.equalsIgnoreCase(COMMAND_REMAP) || command.equalsIgnoreCase(COMMAND_REMAP_SHORT)) {
			try {
				clearExistingComponentCaches();
				remapMemory(args, out, context);
				PatchedMemoryList.print(out);
			} catch (ExpressionEvaluatorException e) {
				throw new DDRInteractiveCommandException(e);
			} catch (MemoryFault e) {
				throw new DDRInteractiveCommandException(e);
			}
			return;
		}		

		out.println(HELP);
		return;
	}
	
	/**
	 * To be overwritten by VM version specific implementations to clear existing caches (such as cached J9Object).
	 */
	protected void clearExistingComponentCaches() {
	};
	
	private void remapMemory(String [] args, PrintStream out, Context context) throws ExpressionEvaluatorException, MemoryFault {
		if (args.length < 3) {
			out.println(HELP);
			return;
		}
		long sourceAddress =  new ExpressionEvaluator(args[0]).calculate(16);
		int length = (int) new ExpressionEvaluator(args[1]).calculate(16);
		long targetAddress =  new ExpressionEvaluator(args[2]).calculate(16);

		byte [] content = new byte[(int)length];
		context.process.getBytesAt(sourceAddress, content);
		PatchedMemoryList.clear(targetAddress, length);
		PatchedMemoryList.record(new MemoryRecord(targetAddress, content));
	}
	
	private void setPatchedMemory(String [] args, PrintStream out) throws ExpressionEvaluatorException {
		if (args.length < 2) {
			out.println(HELP);
			return;
		}
		
		long address = new ExpressionEvaluator(args[0]).calculate(16);
		byte [] buffer = new byte [args.length - 1];
		for (int i = 0; i < buffer.length; i++) {
			buffer[i] = (byte)Integer.parseInt(args[1 + i], 16);
		}
		PatchedMemoryList.clear(address, buffer.length);
		PatchedMemoryList.record(new MemoryRecord(address, buffer));
	}
	
	private void clearPatchedMemory(String [] args, PrintStream out) throws ExpressionEvaluatorException {
		if (args.length == 0) {
			PatchedMemoryList.clearAll();
			return;
		}
		for (int i = 0; i < args.length; i++) {
			int index = args[i].indexOf(",");
			if (index < 0) {
				PatchedMemoryList.clear(new ExpressionEvaluator(args[i]).calculate(16), 1);
			} else {
				String address = args[i].substring(0, index);
				String length = args[i].substring(index + 1);
				PatchedMemoryList.clear(new ExpressionEvaluator(address).calculate(16), (int)(new ExpressionEvaluator(length).calculate(16)));
			}
		}
	}
}
