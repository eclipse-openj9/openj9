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

import com.ibm.jvm.dtfjview.tools.CommandException;
import com.ibm.jvm.dtfjview.tools.IPipe;
import com.ibm.jvm.dtfjview.tools.Tool;
import com.ibm.jvm.dtfjview.tools.ToolsRegistry;
import com.ibm.jvm.dtfjview.tools.utils.IStringModifier;
import com.ibm.jvm.dtfjview.tools.utils.OutputStreamModifier;

public class CharsToTool extends Tool implements IPipe {
	private static final String COMMAND = "charsTo"; 
	private static final String ARGUMENT_DESCRIPTION = "[options] <pattern>";
	private static final String HELP_DESCRIPTION = "To be used after a pipeline to keep the characters from a line until a specific pattern is found.";
	private static final String USAGE = COMMAND + "\t" + ARGUMENT_DESCRIPTION + "\t" + HELP_DESCRIPTION + "\n" +
			"     Options :\n" +
			"          -include : to include the matched pattern in the resulting line. If this option is not used, the matched pattern will be excluded from the resulting line.\n" +
			"          -keep : to keep the lines which does not match the pattern. If this option is not used, the line without a match will be excluded.\n" +
			"          -i, -ignoreCase : to treat the pattern case-insensitive.";

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
	public void process(String command, String[] args, PrintStream out) throws CommandException {
		Arguments arguments = CharsToTool.processArguments(args);
		if(arguments == null) {
			out.println(USAGE);
			return;
		}
		StringModifier sm = new StringModifier(arguments);
		PrintStream newOut = null;
		try {
			newOut = new PrintStream(new OutputStreamModifier(out, sm));
			ToolsRegistry.process(arguments.nextCommand, arguments.nextCommandArgs, newOut);
		} finally {
			if (newOut != null) {
				newOut.close();
			}
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

	/**
	 * To parse the arguments.
	 * Note, if a null object is returned, it indicates some errors have encountered.
	 * <p>
	 */
	private static Arguments processArguments(String[] args) {

		boolean includeToken = false;
		boolean keepMismatchedLines = false;
		boolean ignoreCase = false;

		int x = 0;
		
		for (; x < args.length; ++x) {
			if(args[x].equalsIgnoreCase("-include")) {
				includeToken = true;
			} else if(args[x].equalsIgnoreCase("-i")) {
				ignoreCase = true;
			} else if (args[x].equalsIgnoreCase("-keep")) { // the argument is -keep.
				keepMismatchedLines = true;
			} else {
				break;
			}
		}

		if (x >= args.length) {
			return null;
		}

		String searchToken = args[x];
		++x;
		if (x >= args.length) {
			return null;
		}
		
		String command = args[x];
		++x;
		
		String [] commandArgs = new String[args.length - x];
		System.arraycopy(args, x, commandArgs, 0, args.length - x);

		return new Arguments(searchToken, includeToken, keepMismatchedLines, ignoreCase, command, commandArgs);
	}
	
	private static final class Arguments {
		Arguments(String searchToken, boolean includeToken, boolean keepMismatchedLines, boolean ignoreCase, String command, String [] commandArgs) {
			this.searchToken = ignoreCase ? searchToken.toLowerCase() : searchToken;
			this.includeToken = includeToken;
			this.keepMismatchedLines = keepMismatchedLines;
			this.ignoreCase = ignoreCase;
			this.nextCommand = command;
			this.nextCommandArgs = commandArgs;
		}
		final String searchToken;
		final boolean includeToken;
		final boolean keepMismatchedLines;
		final boolean ignoreCase;
		final String nextCommand;
		final String [] nextCommandArgs;
	}
	private static final class StringModifier implements IStringModifier {
		StringModifier(Arguments attr) {
			this.attr = attr;
		}
		public String modify(String s) {
			int index = -1;
			if(attr.ignoreCase) {
				index = s.toLowerCase().indexOf(attr.searchToken);
			} else {
				index = s.indexOf(attr.searchToken);
			}
			if(index < 0) {
				if(attr.keepMismatchedLines) {
					return s;
				} else {
					return "";
				}
			} else if(attr.includeToken) {
				return s.substring(0, index + attr.searchToken.length()) + "\n";
			} else {
				return s.substring(0, index) + "\n";
			}
		}

		private final Arguments attr;
	}
}
