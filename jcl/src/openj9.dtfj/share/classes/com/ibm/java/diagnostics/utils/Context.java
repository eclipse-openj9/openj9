/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2017 IBM Corp. and others
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
package com.ibm.java.diagnostics.utils;

import java.io.PrintStream;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Vector;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.java.diagnostics.utils.commands.CommandParser;
import com.ibm.java.diagnostics.utils.commands.ICommand;
import com.ibm.java.diagnostics.utils.commands.PluginCommand;
import com.ibm.java.diagnostics.utils.commands.QuitCommand;
import com.ibm.java.diagnostics.utils.plugins.PluginConstants;
import com.ibm.java.diagnostics.utils.plugins.PluginManager;


/**
 * A context represents the environment within which a command is executing.
 * This abstract class provides common functionality which is required by all 
 * types of context.
 * 
 * @author adam
 *
 */
public abstract class Context implements IContext {	
	protected final ArrayList<ICommand> globalCommands = new ArrayList<ICommand>();
	protected final ArrayList<ICommand> commands = new ArrayList<ICommand>();
	protected ICommand lastExecutedCommand = null;
	protected Exception lastException = null; 
	
	{
		//global commands exist irrespective of the context type
		globalCommands.add(new QuitCommand());
		globalCommands.add(new PluginCommand());
	}
	
	/**
	 * Shared logger for all commands to write to
	 */
	public static final Logger logger = Logger.getLogger(PluginConstants.LOGGER_NAME);
	
	protected /*PluginClassloader*/ PluginManager loader;
	
	/* (non-Javadoc)
	 * @see com.ibm.java.diagnostics.IContext#getPluginClassloader()
	 */
	public PluginManager getPluginManager() {
		return loader;
	}
	
	public boolean isCommandRecognised(String command) {
		for (ICommand thisCommand : commands) {
			if (thisCommand.recognises(command, this)) {
				return true;
			}
		}
		return false;
	}
	
	/* (non-Javadoc)
	 * @see com.ibm.java.diagnostics.IContext#execute(java.lang.String, java.lang.String[], java.io.PrintStream)
	 */
	public void execute(String command, String[] arguments, PrintStream out) 
	{
		try {
			execute(new CommandParser(command, arguments), out);
		} catch (ParseException e) {
			out.println("Error executing command: " + e.getMessage());
		}
	}
	
	public void execute(CommandParser commandParser, PrintStream out) {
		for (ICommand thisCommand : commands) {
			if (tryCommand(commandParser, thisCommand, out)) {
				return;
			}
		}
		out.println("Unrecognised command: " + commandParser.getCommand());
	}

	/* (non-Javadoc)
	 * @see com.ibm.java.diagnostics.IContext#execute(java.lang.String, java.io.PrintStream)
	 */
	public void execute(String line, PrintStream out) {
		line = line.trim();

		try {
			execute(new CommandParser(line), out);
		} catch (ParseException e) {
			out.println("Error executing command: " + e.getMessage());
		}
	}
	
	/**
	 * For a given command it is determined if this command should be 
	 * invoked (via the recognises() method), and invokes it if required.
	 * 
	 * @param commandParser the command name
	 * @param arguments arguments to be passed to the command
	 * @param thisCommand the command to test
	 * @param out the output stream for the command to use
	 * @return true if the command was invoked
	 */
	private boolean tryCommand(CommandParser commandParser, ICommand thisCommand, PrintStream defaultOut)
	{
		lastExecutedCommand = null;			//reset the last command executed
		lastException = null;				//reset the last exception
		if (thisCommand.recognises(commandParser.getCommand(), this)) {
			PrintStream fileOut = null;
			lastExecutedCommand = thisCommand;	
			try {
				if (commandParser.isRedirectedToFile()) {
					fileOut = commandParser.getOutputFile();
					thisCommand.run(commandParser.getCommand(), commandParser.getArguments(), this, fileOut);
					fileOut.flush();
					fileOut.close();
				} else {
					thisCommand.run(commandParser.getCommand(), commandParser.getArguments(), this, defaultOut);
				}
			} catch (Exception e) {
				lastException = e;
				logger.log(Level.FINE, "Problem running command: ", e);
				defaultOut.println("Problem running command: ");
				defaultOut.println(e.getMessage());
			}
			
			defaultOut.flush();

			if (fileOut != null) {
				fileOut.flush();
				fileOut.close();
			}

			return true;
		} else {
			return false;
		}
	}
	
	/**
	 * List of all the command names that are available within this context
	 * 
	 * @return list of names
	 */
	public Vector<String> getCommandNames() 
	{
		Vector<String> commandNames = new Vector<String>();
		
		for (ICommand thisCommand : commands) {
			if (thisCommand.getCommandNames() != null) {
				commandNames.addAll(thisCommand.getCommandNames());
			}
		}
		
		return commandNames;
	}

	/* (non-Javadoc)
	 * @see com.ibm.java.diagnostics.IContext#getCommands()
	 */
	public List<ICommand> getCommands() {
		return Collections.unmodifiableList(commands);
	}

	
	/**
	 * This adds the list of global commands which are applicable for any
	 * context to this specific context instance  
	 */
	protected void addGlobalCommandsToContext() {
		for(ICommand cmd : globalCommands) {
			commands.add(cmd);
		}
	}

	public ICommand getLastExecutedCommand() {
		return lastExecutedCommand;
	}

	public Exception getLastCommandException() {
		return lastException;
	}
	
}
