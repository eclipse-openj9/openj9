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
package com.ibm.jvm.dtfjview.commands.setcommands;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintStream;
import java.util.Calendar;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.FileOutputChannel;
import com.ibm.jvm.dtfjview.SessionProperties;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;
import com.ibm.jvm.dtfjview.spi.IOutputChannel;
import com.ibm.jvm.dtfjview.tools.ToolsRegistryOutputChannels;

@DTFJPlugin(version="1.*", runtime=false, image=false)
public class SetLoggingCommand extends BaseJdmpviewCommand {
	public static final String LOG_STATE_OVERWRITE = "set_logging_overwrite";		//indicates the current setting for overwriting log files
	public static final String LOG_STATE_LOGGING = "set_logging";					//indicates if logging is on or off
	public static final String LOG_STATE_FILE = "current_logging_file";				//file logging is being or will be written to
	
	private static final String CMD = "set logging";
	private static final String SUBCMD_ON = "on";
	private static final String SUBCMD_OFF = "off";
	private static final String SUBCMD_FILE = "file";
	private static final String SUBCMD_OVERWRITE = "overwrite";
	
	private boolean isOverwriteEnabled = false;
	private boolean isLoggingEnabled = false;
	private File logFile = null;
	private IOutputChannel fileChannel = null;

	private enum SubCommand {
		on(SUBCMD_ON, "", "turn on logging"),
		off(SUBCMD_OFF, "", "turn off logging"),
		file(SUBCMD_FILE, "", "turn on logging"),
		overwrite(SUBCMD_OVERWRITE, "", "controls the overwriting of log files");
		
		private final String name;
		private final String params;
		private final String help;
		private SubCommand(String name, String params, String help) {
			this.name = name;
			this.params = params;
			this.help = help;
		}
	}
	
	{
		addCommand(CMD, "", "configures several logging-related parameters, starts/stops logging");
		for(SubCommand sub : SubCommand.values()) {
			addSubCommand(CMD, sub.name, sub.params, sub.help);	
		}
	}

	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length == 0) {
			out.println("\"set logging\" requires at least one parameter");
			printDetailedHelp(out);
			return;
		}
		restoreState();
		try {
			SubCommand cmd = SubCommand.valueOf(args[0]);
			switch(cmd) {
				case on :
				case off :
					if (args.length != 1) {
						out.println("\"set logging " + cmd.name + "\" does not take any additional parameters");
						return;
					}
					boolean enabled = cmd.equals(SubCommand.on);
					setLogging(enabled);
					break;
				case file :
					switch(args.length) {
						case 3 :	//need to check if this is the overwrite
							if(args[2].equalsIgnoreCase(SUBCMD_OVERWRITE)) {
								setOverwrite(true);
							} else {
								out.println("\"set logging file\" takes exactly one parameter, the filename to log entries to");
								return;
							}
						case 2 :	//deliberately allow to fall through from the overwrite case above to now handle setting the log file
							setLogFile(args[1]);
							setLogging(true);
							break;
						default :
							out.println("\"set logging file\" takes exactly one parameter, the filename to log entries to");
							break;
					}
					break;
				case overwrite :
					if(args.length != 2) {
						out.println("\"set logging overwrite\" takes exactly one parameter, either \"on\" or \"off\"");
						return;
					}
					setOverwrite(parseBoolean(args[1]));
					break;
				default :
					out.println("\"" + args[0] + "\" is not a valid parameter for the \"set logging\" command");
					break;
			}
		} catch (IllegalArgumentException e) {
			out.println("\"" + args[0] + "\" is not a valid parameter for the \"set logging\" command");
		}
	}
	
	//restores the state of the logging command from the context property bag
	private void restoreState() {
		isOverwriteEnabled = parseBoolean(ctx.getProperties().get(LOG_STATE_OVERWRITE));
		isLoggingEnabled = parseBoolean(ctx.getProperties().get(LOG_STATE_LOGGING));
		String file = (String)ctx.getProperties().get(LOG_STATE_FILE);
		if(file != null) {
			logFile = Utils.absPath(ctx.getProperties(), file);
		}
	}
	
	//allows boolean 'true' to be either 'true' or 'on'
	private boolean parseBoolean(Object obj) {
		if(obj == null) {
			return false;
		}
		String value = obj.toString().trim();
		if(value.equalsIgnoreCase("on")) {
			return true;
		} else {
			return Boolean.parseBoolean(value);
		}
	}

	private void setOverwrite(boolean enabled) {
		if (!(isOverwriteEnabled ^ enabled)) {
			out.println("Command ignored : overwriting already has this setting");
			return;
		}
		String state = (enabled ? "on" : "off");
		isOverwriteEnabled = enabled;
		ctx.getProperties().put(LOG_STATE_OVERWRITE, state);
		out.println("overwriting of log file option changed to \"" + state + "\"");
	}
	
	private void setLogging(boolean enabled) {
		if (!(isLoggingEnabled ^ enabled)) {
			out.println("Command ignored : logging already has this setting");
			return;
		}
		if(enabled) {					//turning logging on
			if(logFile == null) {		//but haven't specified a log file so use a default one
				setDefaultLogFile();
			}
			if(!isLogFileValid()) {
				return;					//something was wrong with the specified log file
			}
			openLogFile();				//open the specified log file for writing
			ctx.getProperties().put(LOG_STATE_LOGGING, "on");
		} else {						//turning logging off
			closeLogFile();				//close any open log file
			ctx.getProperties().put(LOG_STATE_LOGGING, "off");
		}
		isLoggingEnabled = enabled;
	}
	
	private boolean isLogFileValid() {
		if(logFile.exists()) {
			if(logFile.isDirectory()) {
				out.println("Cannot write to " + logFile.getPath() + " as it is a directory");
				return false;
			} else {
				if(isOverwriteEnabled) {
					out.println("Specified log file already exists, overwriting");
					if(!logFile.delete()) {
						out.println("Failed to delete existing log file");
						return false;
					}
				} else {
					out.println("Specified log file already exists, either\n1) set logging file <new file>\n2) set logging overwrite on");
					return false;
				}
			}
		}
		return true;
	}
	
	private void setDefaultLogFile() {
		Calendar cal = Calendar.getInstance();
		String filename = String.format("jdmpview.%1$tY%1$tm%1$td.%1$tH%1$tM%1$tS.txt", cal);
		out.println("log file not specified; using default log file " + filename);
		out.println("to change this type 'set logging off' then specify a file with 'set logging file <log file>'");
		setLogFile(filename);
	}
	
	private void setLogFile(String filename) {	
		if (null != logFile) {
			if(isLoggingEnabled) {
				setLogging(false);
			}
		}
		if(filename == null) {		//removing the current log file setting
			ctx.getProperties().remove(LOG_STATE_FILE);
		} else {
			// test whether the file name is an absolute path
			File test = new File(filename);
			if (test.isAbsolute()) {
				logFile = new File(filename);				
			} else {
				File pwd = (File)ctx.getProperties().get(SessionProperties.PWD_PROPERTY);				
				logFile = new File(pwd.getPath() + File.separator + filename);				
			}
			ctx.getProperties().put(LOG_STATE_FILE, logFile.getAbsolutePath());
		}
	}
	
	private void openLogFile() {
		FileWriter f;
		try {
			f = new FileWriter(logFile, false);
		} catch (IOException e) {
			f = null;
			out.println("IOException encountered while opening file \"" + logFile.getAbsolutePath() + "\"; make sure the file can be written to");
			return;
		}

		fileChannel = new FileOutputChannel(f, logFile);
		ToolsRegistryOutputChannels.addChannel(fileChannel);
		out.println("logging turned on; outputting to \"" + logFile.getAbsolutePath() + "\"");
	}
	
	private void closeLogFile() {
		ToolsRegistryOutputChannels.removeChannel(fileChannel);
		out.println("logging turned off; was logging to \"" + logFile + "\"");
		logFile = null;
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("configures several logging-related parameters, " +
				"starts/stops logging\n\n" +
				
				"parameters: [on|off], file <filename>, overwrite [on|off]\n" +
				"- [on|off]           - turns logging on or off (default: off)\n" +
				"- file <filename>    - sets the file to log to; this will be relative " +
				"to the directory returned by the \"pwd\" command unless an " +
				"absolute path is specified; if the file is set while logging is on, " +
				"the change will take effect the next time logging is started " +
				"(default: <not set>)\n" +
				"- overwrite [on|off] - turns overwriting of the specified log file " +
				"on or off (off means that the log file will be appended to); if " +
				"this is on, the log file will be cleared every time \"set logging " +
				"on\" is run (default: off)\n"
		);
		
	}

}
