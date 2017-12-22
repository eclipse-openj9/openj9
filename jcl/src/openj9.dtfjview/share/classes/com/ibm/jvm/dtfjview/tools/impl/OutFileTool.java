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
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;

import com.ibm.jvm.dtfjview.tools.CommandException;
import com.ibm.jvm.dtfjview.tools.IPipe;
import com.ibm.jvm.dtfjview.tools.Tool;
import com.ibm.jvm.dtfjview.tools.ToolsRegistry;

/**
 * Note, this class copied some logic from class com.ibm.java.diagnostics.utils.commands.CommandParser
 * <p>
 * @author Manqing Li
 *
 */
public class OutFileTool extends Tool implements IPipe
{
	public static final String COMMAND_OVERWRITE = ">";
	public static final String COMMAND_APPEND = ">>";
	public static final String ARGUMENT_DESCRIPTION = "<targetFilePath>";
	public static final String HELP_DESCRIPTION = "to be used at the end of a command to redirect messages to a file (overwrite|append).";
	public static final String USAGE = COMMAND_OVERWRITE + "|" + COMMAND_APPEND + "\t" + ARGUMENT_DESCRIPTION + "\t" + HELP_DESCRIPTION;
	
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
		return command.equalsIgnoreCase(COMMAND_OVERWRITE) 
				|| command.equalsIgnoreCase(COMMAND_APPEND);
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
		final Attributes attributes = readAttributes(command, args);
		if(attributes == null) {
			redirector.println(USAGE);
			return;
		}
		if(attributes.filePath == null) {
			redirector.println("Missing file name for redirection");
			redirector.println(USAGE);
		}
		String filename = attributes.filePath.trim();
		if (filename.charAt(filename.length() - 1) == File.separatorChar) {
			redirector.println("Invalid redirection path - missing filename");
			redirector.println(USAGE);
		}

		PrintStream fileOut = null;
		try {
			fileOut = getOutputFile(attributes);
			ToolsRegistry.process(attributes.nextCommand, attributes.nextCommandArgs, fileOut);
		} catch (IOException e) {
			redirector.println("Problem running command: ");
			redirector.println(e.getMessage());
		} finally {
			if(fileOut != null) {
				fileOut.flush();
				fileOut.close();
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
		return COMMAND_OVERWRITE + "|" + COMMAND_APPEND;
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
	 * Returns a PrintStream, which either writes to a new file or overwrites or appends to an existing one,
	 * depending on the command.
	 */
	private PrintStream getOutputFile(final Attributes attributes) throws IOException {

		PrintStream outStream = null;
		String redirectionFilename = attributes.filePath;
		File outFile = new File(redirectionFilename);

		if (redirectionFilename.indexOf(File.separator) > -1) {
			// The last entry is the filename itself (we've checked that during parsing stage)
			// so everything before it is the directory, some of which may not exist, so need to create it
			String parentDirectories = redirectionFilename.substring(0, redirectionFilename.lastIndexOf(File.separator));
			File dir = new File(parentDirectories);
			if (!dir.exists() && !dir.mkdirs()) {
				throw new IOException("Could not create some of the requested directories: " + parentDirectories);
			}
		}
		
		if (attributes.append && outFile.exists()) {
			// in this case do not create a new file, but use the existing one
			outStream = new PrintStream(new FileOutputStream(outFile, true));
		} else {
			outStream = new PrintStream(outFile); // this will create a new file
		}

		return outStream; 
	}

	private Attributes readAttributes(String command, String[] args) {
		String filePath = null;
		String ddrCommand = null;
		String [] ddrCommandArgs = null;

		if(args.length < 2) {
			return null;
		}
		int index = 0;

		filePath = args[index++];
		ddrCommand = args[index++];
		ddrCommandArgs = new String[args.length - index];
		for(int i = 0; i < ddrCommandArgs.length; i++) {
			ddrCommandArgs[i] = args[i + index];
		}
		return new Attributes(filePath, ddrCommand, ddrCommandArgs, command.equalsIgnoreCase(COMMAND_APPEND));
	}
	
	private class Attributes 
	{
		public Attributes(String filePath,String ddrCommand,String [] ddrCommandArgs, boolean append) 
		{
			this.filePath = filePath;
			this.nextCommand = ddrCommand;
			this.nextCommandArgs = ddrCommandArgs;
			this.append = append;
		}
		private String filePath = null;
		private String nextCommand = null;
		private String [] nextCommandArgs = null;
		private boolean append = false;
	}
}
