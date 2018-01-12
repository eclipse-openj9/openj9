/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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
import java.util.LinkedList;

import com.ibm.jvm.dtfjview.tools.CommandException;
import com.ibm.jvm.dtfjview.tools.Tool;
import com.ibm.jvm.dtfjview.tools.ToolsRegistry;

public class HistoryTool extends Tool {
	public static final String COMMAND = "history";
	public static final String COMMAND_SHORT = "his";
	public static final int DEFAULT_DISPLAY_N = 20;
	public static final String ARGUMENT_DESCRIPTION = "[-r] [N]";
	public static final String HELP_DESCRIPTION = 
			"     If option -r is used, the Nth history command (default to the last one) will be run;\n" +
			"     otherwise, at most N (default " + DEFAULT_DISPLAY_N + ") history commands will be displayed.";
	public static final String USAGE = COMMAND + "|" + COMMAND_SHORT + "\t" + ARGUMENT_DESCRIPTION + "\n" + HELP_DESCRIPTION;
	
	public HistoryTool() {
		super();
		history = new LinkedList<String>();
		defaultExecutingIndex = -1;
	}
	
	/**
	 * To record a history command.
	 * <p>
	 * @param cmd	The command to be recorded.
	 */
	public void record(String cmd) 
	{
		history.add(cmd);
		defaultExecutingIndex = history.size() - 1;
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
	public void process(String command, String[] args, PrintStream out) throws CommandException 
	{
		int n = DEFAULT_DISPLAY_N;
		boolean executeCommand = false;

		if (args.length == 0) {
			showHistoryCommands(DEFAULT_DISPLAY_N, out);
			return;
		}
		
		if (args.length > 0) {
			if (args[0].startsWith("-r")) {
				executeCommand = true;
				n = defaultExecutingIndex;
				if (args[0].length() > 2) {
					try {
						n = Integer.parseInt(args[0].substring(2));
					} catch(NumberFormatException e) {
						printDetailedHelp(out);
						return;
					}
				} else if (args.length > 1) {
					try {
						n = Integer.parseInt(args[1]);
					} catch(NumberFormatException e) {
						printDetailedHelp(out);
						return;
					}
				}
			} else { // args[0].startsWith("-r") == false
				try {
					n = Integer.parseInt(args[0]);
				} catch(NumberFormatException e) {
					printDetailedHelp(out);
					return;
				}
			}
		}

		if (executeCommand) {
			defaultExecutingIndex = n - 1;
			executeHistoryCommand(n, out);
		} else {
			showHistoryCommands(n, out);
		}
	}
	
	/**
	 * Determines if a command is accepted by the current tool.
	 * <p>
	 * @param command	The command
	 * @param args		The arguments taken by the command.
	 * <p>
	 * @return		<code>true</code> if this is the correct tool for this command; 
	 * 				<code>false</code> otherwise.
	 */
	public boolean accept(String command, String[] args) {
		return command.equalsIgnoreCase(COMMAND_SHORT) || command.equalsIgnoreCase(COMMAND);
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
		return ARGUMENT_DESCRIPTION;
	}

	/**
	 * To gets the tool's help description.
	 * <p>
	 * @return	The tool's help description.
	 */
	public String getHelpDescription() {
		return HELP_DESCRIPTION;
	}
	
	private void executeHistoryCommand(int n, PrintStream out) throws CommandException {
		if (history.size() == 0) {
			out.println("The history repository is empty.");
			return;
		}
		if (n < 0 || n > history.size() - 1) {
			out.println("The number " + n + " is not a valid sequence number in history repository.");
			return;
		}
		ToolsRegistry.process(history.get(n), out);
	}
	
	private void showHistoryCommands(int counter, PrintStream out) {		
		if (counter <= 0) {
			out.println("The number " + counter + " is not a valid counter.");
			return;
		}
		if (history.size() == 0) {
			out.println("The history repository is empty.");
			return;
		}
		counter = Math.min(counter, history.size()); 
		for (int index = history.size() - counter; index < history.size(); index++) {
			out.println(index + " : " + history.get(index));
		}
	}

	private LinkedList<String> history;
	private int defaultExecutingIndex;
}

