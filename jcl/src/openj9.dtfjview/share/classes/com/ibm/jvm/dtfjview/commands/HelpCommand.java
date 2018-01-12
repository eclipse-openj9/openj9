/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands;

import java.io.PrintStream;
import java.util.SortedSet;
import java.util.TreeSet;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.commands.ICommand;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;

/**
 * Prints out either a help summary for all commands in this context or forwards
 * the request onto a particular command for a detailed output. Note that the context
 * command is a 'pseudo-command' in that it is what controls switching contexts and so
 * is never actually in the list of commands available for the current context. The help
 * entries for the context command are therefore added manually.
 * 
 * @author adam
 *
 */
@DTFJPlugin(version=".*", runtime=false, image=false)
public class HelpCommand extends BaseJdmpviewCommand {
	private static final String CMD_NAME = "help";
	
	{
		addCommand(CMD_NAME, "[command name]","displays list of commands or help for a specific command");
	}

	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length == 0) {
			printHelpSummary();
		} else {
			StringBuilder cmdline = new StringBuilder();
			if(args[0].equalsIgnoreCase("context")) {
				out.println("switches to another context when more than one context is available. For z/OS the ASID can be specified with" + 
						" the 'asid' flag e.g. context asid <ID>. Execute 'context' to see the list of currently available contexts");
			} else {
				for(String arg : args) {
					cmdline.append(arg);
					cmdline.append(" ");
				}
				
				if (!ctx.isCommandRecognised(cmdline.toString().trim())) {
					out.println("Unrecognised command: " + cmdline.toString().trim());
				} else {
					cmdline.append(" ?");
					ctx.execute(cmdline.toString().trim(), out);
				}
			}
		}
	}
	
	private void printHelpSummary() {
		SortedSet<String> helpTable = new TreeSet<String>();

		String contextHelp = String.format(COMMAND_FORMAT, "context", "[ID|asid ID]", "switch to the selected context");
		helpTable.add(contextHelp);
		for (ICommand thisCommand : ctx.getCommands()) {
			for (String thisEntry : thisCommand.getCommandDescriptions()) {
				if(!thisEntry.endsWith("\n")) {
					thisEntry += "\n";
				}
				helpTable.add(thisEntry);
			}
		}

		for (String entry : helpTable) {
			out.print(entry);
		}
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("displays list of commands or help for a specific command \n\n"+
				"parameters: none, <command_name>\n\n" +
				"With no parameters, \"help\" displays the complete list of commands " +
				"currently supported.  When a <command_name> is specified, \"help\" " +
				"will list that command's sub-commands if it has sub-commands; " +
				"otherwise, the command's complete description will be displayed.\n\n" +
				"To view help on a command's sub-command, specify both the command " +
				"name and the sub-command name.  For example, \"help info thread\" " +
				"will display \"info thread\"'s description.");		
	}
}
