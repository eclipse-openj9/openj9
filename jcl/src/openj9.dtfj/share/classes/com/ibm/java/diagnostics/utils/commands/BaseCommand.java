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

import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.LinkedHashSet;
import java.util.Map;
import java.util.Set;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.plugins.PluginConfig;

/**
 * Base command which supplies basic support
 * 
 * @author adam
 *
 */
public abstract class BaseCommand implements ICommand {
	protected static final String nl = System.getProperty("line.separator");
	protected static final String COMMAND_FORMAT = "%-25s %-20s %s\n";
	protected static final String SUBCOMMAND_FORMAT = "%25s %-20s %s\n";
	private static final String KEY_ID = ":";

	private Map<String,CommandDescription> _commands = new LinkedHashMap<String,CommandDescription>();
	private Map<String,CommandDescription> _subCommands = new LinkedHashMap<String,CommandDescription>();
	
	private boolean isDirty = false;				//indicates if a command or subcommand has been added
	
	private Set<String> descriptions = new LinkedHashSet<String>();
	protected PluginConfig config;			//configuration used to generate this command
	
	/**
	 * 
	 * @param name				Command Name
	 * @param argDescription	Brief name of any optional or required arguments 
	 * @param helpDescription	One-liner Description of the command
	 * 
	 * argDescription should be a word describing the argument name. 
	 * 	e.g:  <address>    to specify an address argument that is mandatory
	 *        [address]    to specify an address argument that is optional 
	 * 	   
	 */
	public CommandDescription addCommand(String name, String argDescription, String helpDescription)
	{
		isDirty = true;
		CommandDescription description = new CommandDescription(name, argDescription, helpDescription);
		_commands.put(name.toLowerCase(), description);
		return description;
	}
	
	public void addSubCommand(String cmdname, String subname, String argDescription, String help)
	{
		isDirty = true;
		CommandDescription subCommand = new CommandDescription(subname, argDescription, help);
		for(CommandDescription cmd : _commands.values()) {
			if(cmd.getCommandName().toLowerCase().equals(cmdname.toLowerCase())) {
				subCommand.setParent(cmd);
			}
		}
		_subCommands.put(cmdname + KEY_ID + subname, subCommand);
	}
	
	public boolean recognises(String command, IContext context) {
		return _commands.containsKey(command.toLowerCase());
	}

	public Collection<String> getCommandDescriptions() {
		if(isDirty) {
			for(CommandDescription cmd : _commands.values()) {
				StringBuilder desc = new StringBuilder();
				desc.append(String.format(COMMAND_FORMAT, cmd.getCommandName(), cmd.getArgumentDescription(), cmd.getHelpDescription()));
				for(CommandDescription subcmd : _subCommands.values()) {
					if((subcmd.getParent() != null) &&  subcmd.getParent().getCommandName().equalsIgnoreCase(cmd.getCommandName())) {
						desc.append(String.format(SUBCOMMAND_FORMAT, subcmd.getCommandName(), subcmd.getArgumentDescription(), subcmd.getHelpDescription()));
					}
				}
				descriptions.add(desc.toString());
			}
		}
		return descriptions;
	}

	public Collection<String> getCommandNames() {
		return _commands.keySet();
	}

	public PluginConfig getConfig() {
		return config;
	}
	
	public void setConfig(PluginConfig config) {
		this.config = config;
	}
}
