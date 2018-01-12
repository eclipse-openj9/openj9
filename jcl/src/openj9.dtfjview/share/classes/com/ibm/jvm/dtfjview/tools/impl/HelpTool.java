/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2015, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.tools.impl;

import java.io.PrintStream;

import com.ibm.jvm.dtfjview.tools.CommandException;
import com.ibm.jvm.dtfjview.tools.ITool;
import com.ibm.jvm.dtfjview.tools.Tool;
import com.ibm.jvm.dtfjview.tools.ToolsRegistry;

/**
 * @author Manqing Li, IBM.
 *
 */
public class HelpTool extends Tool
{
	public static final String COMMAND = "help";
	public static final String HELP_DESCRIPTION = "to display command help messages";
	public static final String USAGE = COMMAND + ":\t" + HELP_DESCRIPTION;
	public static final String JDMPVIEW_HELP_COMMAND = COMMAND;	
	public static final String COMMAND_FORMAT = "%-25s %-20s %s";

	/**
	 * Determines if a command is accepted by current tool.
	 * <p>
	 * @param command	The command
	 * @param args		The arguments taken by the command.
	 * <p>
	 * @return		<code>true</code> if this is the correct tool for this command; 
	 * 				<code>false</code> otherwise.
	 */
	public boolean accept(String command, String[] args) {
		return command.equalsIgnoreCase(COMMAND);
	}
	
	/**
	 * Processes the command.
	 * <p>
	 * @param command	The command to be processed.
	 * @param args		The arguments taken by the command.
	 * @param out		The output channel.
	 * <p>
	 * @throws CommandException
	 */
	public void process(String command, String[] args, PrintStream redirector) throws CommandException {
		if (args.length == 0) {
			printAllHelpMessages(redirector);
		} else {
			String commandToBeHelped = args[0];
			String [] newArgs = new String [args.length - 1];
			for (int i = 1; i < args.length; i++) {
				newArgs[i - 1] = args[i];
			}
			printHelpMessages(commandToBeHelped, newArgs, redirector);
		}
	}
	
	/**
	 * To print the detailed help message.
	 */
	public void printDetailedHelp(PrintStream out) {
		out.println(USAGE);
	}

	/**
	 * To gets the tool's command name.
	 * <p>
	 * @return	The tool's command name.
	 */
	public String getCommandName() {
		return COMMAND;
	}
	
	/**
	 * To gets the tool's argument description.
	 * <p>
	 * @return	The tool's argument description.
	 */
	public String getArgumentDescription() {
		return "";
	}
	
	/**
	 * To gets the tool's help description.
	 * <p>
	 * @return	The tool's help description.
	 */
	public String getHelpDescription() {
		return null;
	}
	
	/**
	 * To print help messages for all commands.
	 * <p>
	 * @param out	The PrintStream to print the messages.
	 */
	private void printAllHelpMessages(PrintStream out) {
		ToolsRegistry.executeJdmpviewCommand(JDMPVIEW_HELP_COMMAND, out);
		for (ITool aTool : ToolsRegistry.getAllTools()) {
			if (aTool instanceof HelpTool == false) {
				out.println(String.format(COMMAND_FORMAT, aTool.getCommandName(), aTool.getArgumentDescription(), aTool.getHelpDescription()));
			}
		}
	}
	
	/**
	 * To print the help message for a specific command.
	 * <p>
	 * @param commandToBeHelped		The command the help message is for.
	 * @param arguments	The arguments.
	 * @param out	The PrintStream to print the messages.
	 */
	private void printHelpMessages(String commandToBeHelped, String[] arguments, PrintStream out) 
	{
		for (ITool aTool : ToolsRegistry.getAllTools()) {
			if ((aTool instanceof HelpTool == false) && aTool.accept(commandToBeHelped, arguments)) 
			{
				aTool.printDetailedHelp(out);
				return;
			}
		}
		
		StringBuffer sb = new StringBuffer(JDMPVIEW_HELP_COMMAND);
		sb.append(" ").append(commandToBeHelped);
		for (String arg : arguments) {
			sb.append(" ").append(arg);
		}
		ToolsRegistry.executeJdmpviewCommand(sb.toString(), out);
	}
}
