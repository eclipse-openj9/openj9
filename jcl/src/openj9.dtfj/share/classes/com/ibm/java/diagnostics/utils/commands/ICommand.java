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
package com.ibm.java.diagnostics.utils.commands;

import java.io.PrintStream;
import java.util.Collection;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.plugins.PluginConfig;

public interface ICommand {
	/**
	 * Method used by an interactive engine to decide whether this command
	 * matches the command passed in by the user
	 * @param command Command string entered by user. e.g. !j9x or info class
	 * @param context Current context
	 * @return True if this command object can process the supplied command, commands should not assume that returning true will guarantee a subsequent invocation
	 */
	public boolean recognises(String command, IContext context);
	
	/**
	 * Executes the command
	 * @param command Command string e.g. !j9x
	 * @param args Arguments for command
	 * @param context Context to work in
	 * @param out PrintStream to write command output on
	 * @throws CommandException If there is any problem running the command (incorrect usage, CorruptData etc.)
	 */
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException;
	
	/**
	 * @return Strings to be inserted in help output
	 */
	public Collection<String> getCommandDescriptions();
	

    /**
     * @return Strings containing command names
     */
	public Collection<String> getCommandNames();
	
	/**
	 * Identifies the configuration from which this command was created.
	 * 
	 * @return configuration for this command
	 */
	public PluginConfig getConfig();
}
