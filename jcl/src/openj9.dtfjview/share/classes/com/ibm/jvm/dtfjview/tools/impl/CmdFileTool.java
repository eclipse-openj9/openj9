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

import java.io.File;
import java.io.IOException;
import java.io.PrintStream;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.List;

import com.ibm.jvm.dtfjview.Session;
import com.ibm.jvm.dtfjview.tools.CommandException;
import com.ibm.jvm.dtfjview.tools.Tool;
import com.ibm.jvm.dtfjview.tools.ToolsRegistry;
import com.ibm.jvm.dtfjview.tools.utils.FileUtils;

public class CmdFileTool extends Tool {
	public static final String [] AS_COMMENT_INDICATORS = {
		"//",
		"#"
	};
	public static final String COMMAND = "cmdfile";
	public static final String ARGUMENT_DESCRIPTION = "<commandFilePath> [charset]";
	public static final String HELP_DESCRIPTION = "To execute all the commands in a file.";
	public static final String USAGE = COMMAND + "\t" + ARGUMENT_DESCRIPTION + "\t" + HELP_DESCRIPTION + "\n" +
			"     Options:\n" +
			"          <commandFilePath> : the path to a file which specifies a series of jdmpview commands.  \n" +
			"               These commands are read and run sequentially.\n" +
			"               Empty lines or lines starting with \"//\" or \"#\" will be ignored.\n" +
			"     [charset] : the character set for the commands specified in the command file.\n" +
			"          The character set name must be a supported charset as defined in java.nio.charset.Charset. For example, US-ASCII.";
	
	public CmdFileTool(String charset) {
		this.defaultCharset = charset;
	}
	/**
	 * To parse a command file.
	 * <p>
	 * @param cmdFile	The command file.
	 * @param charset	If not null, the charset is used to decode the strings in the file.
	 * <p>
	 * @return	The list of commands.
	 * <p>
	 * @throws UnsupportedEncodingException		
	 * @throws IOException
	 */
	public static List<String> parseCmdFile(File cmdFile, String charset) throws UnsupportedEncodingException, IOException {
		List<String> commands = new ArrayList<String>();
		String [] cmds = FileUtils.read(cmdFile, charset);
		for (String cmd : cmds) {
			// PR100323 : skipping empty lines and comments lines.
			cmd = cmd.trim();
			if (cmd.length() > 0
					&& cmd.startsWith("//") == false
					&& cmd.startsWith("#") == false) 
			{
				commands.add(cmd);
			}
		}
		return commands;
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
		if (args.length == 0) {
			out.println(USAGE);
			return;
		}
		File file = new File(args[0]);
		if (!file.exists() || !file.isFile()) {
			out.println("The specified command file " + file.getAbsolutePath() + " does not exist or is not a file");
			return;
		}
		if (file.length() > Integer.MAX_VALUE) {
			out.println("The specified command file " + file.getAbsolutePath() + " is too large to be read");
			return;
		}
		
		String charset = defaultCharset;
		if (args.length == 2) {
			charset = args[1];
		} else if (args.length > 2) {
			out.println(USAGE);
			return;
		}
		
		try {
			List<String> commands = parseCmdFile(file, charset);
			for (String cmd : commands) {
				out.println(Session.prompt + cmd);
				try {
					ToolsRegistry.process(cmd, out);
				} catch (CommandException e) {
					out.println(e.getMessage());
				}
			}
		} catch (UnsupportedEncodingException e) {
			out.println("The supplied charset " + charset + " for reading the command file is not supported");
		} catch (IOException e) {
			out.println("Error reading from command file " + file.getAbsolutePath());
		}
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
	 * To print the detailed help message.
	 */
	public void printDetailedHelp(PrintStream out) {
		out.println(USAGE);
	}
	
	private String defaultCharset = null;
}
