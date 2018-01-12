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
import java.util.List;
import java.util.Properties;

import com.ibm.java.diagnostics.utils.commands.CommandParser;
import com.ibm.java.diagnostics.utils.commands.ICommand;
import com.ibm.java.diagnostics.utils.plugins.PluginLoader;

/**
 * A context is the process and address space within which a plugin
 * is operating. There are no guarantees made as to the presence of 
 * a JVM.
 * 
 * @author adam
 *
 */
public interface IContext {
	/**
	 * A set of defined properties. This typically includes information not on the DTFJ API, but may be useful for plugin processing e.g. a logging level.
	 * Tools such as jdmpview which provide not only the execution harness but some in-built commands
	 * use properties to pass information to commands.
	 * 
	 * @return properties that have currently been set
	 */
	public Properties getProperties();
	
	/**
	 * Allows to check if the given command will be executed (i.e. if there is a command that will accept the string)
	 * without actually executing it, avoiding error messages etc.
	 * @param command - just the actual command WITHOUT ARGUMENTS
	 * @return
	 */
	public boolean isCommandRecognised(String command);
	
	/**
	 * Execute the entered command line with the output sent to the supplied print stream.
	 * 
	 * @param cmdline line to execute
	 * @param out where to write the output to
	 */
	public void execute(String cmdline, String[] arguments, PrintStream out);
	
	/**
	 * Execute the entered command line with the output sent to the supplied print stream.
	 * 
	 * @param cmdline line to execute
	 * @param out where to write the output to
	 */
	public void execute(String cmdline, PrintStream out);
	
	/**
	 * Execute the entered command line with the output sent to the supplied print stream.
	 * @param command
	 * @param out
	 */
	public void execute(CommandParser command, PrintStream out);
	
	/**
	 * Gets the classloader which has been used to add plugins. This is typically done
	 * to add new plugins at runtime or refresh the existing list.
	 * 
	 * @return the classloader
	 */
	public PluginLoader getPluginClassloader();
	
	/**
	 * Refreshes the context, this will perform actions such as rescanning
	 * the plugin path or other initialization tasks.
	 */
	public void refresh();
	
	
	/**
	 * Returns a list of commands which are available for this context
	 * @return
	 */
	public List<ICommand> getCommands();
	
	/**
	 * The last command which was successfully executed by the context.
	 * 
	 * @return the command or null if none have been executed or the command threw an exception
	 */
	public ICommand getLastExecutedCommand();
	
	/**
	 * Provides access to any uncaught exceptions thrown when the last command was executed.
	 * 
	 * @return the exception or null of no uncaught exceptions were thrown
	 */
	public Exception getLastCommandException();
}
