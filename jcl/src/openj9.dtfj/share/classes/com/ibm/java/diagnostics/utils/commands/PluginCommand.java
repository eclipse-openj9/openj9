/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 1991, 2017 IBM Corp. and others
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
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.plugins.PluginConfig;
import com.ibm.java.diagnostics.utils.plugins.PluginConstants;

/**
 * Command which allows control of the loaded plugins
 * 
 * @author adam
 *
 */
public class PluginCommand extends BaseCommand 
{
	private static final String COMMAND_NAME = "plugins";
	private static final String SUBCOMMAND_LIST = "list";
	private static final String SUBCOMMAND_RELOAD = "reload";
	private static final String SUBCOMMAND_SHOWPATH = "showpath";
	private static final String SUBCOMMAND_SETPATH = "setpath";

	private final Map<String, Method> commands = new HashMap<String, Method>(); 
	
	{
		try {
			commands.put(SUBCOMMAND_LIST, getClass().getDeclaredMethod("commandListPlugins", new Class[] {String[].class, IContext.class, PrintStream.class}));
			commands.put(SUBCOMMAND_RELOAD, getClass().getDeclaredMethod("commandReload", new Class[] {String[].class, IContext.class, PrintStream.class}));
			commands.put(SUBCOMMAND_SHOWPATH, getClass().getDeclaredMethod("commandShowPath", new Class[] {String[].class, IContext.class, PrintStream.class}));
			commands.put(SUBCOMMAND_SETPATH, getClass().getDeclaredMethod("commandSetPath", new Class[] {String[].class, IContext.class, PrintStream.class}));
		} catch (Exception e) {
			System.err.println("Error creating command list : " + e.getMessage());
		}
	}
	
	public PluginCommand()
	{
		addCommand(COMMAND_NAME, "", "Plugin management commands");
		addSubCommand(COMMAND_NAME, SUBCOMMAND_LIST, "", "Show the list of loaded plugins for the current context");
		addSubCommand(COMMAND_NAME, SUBCOMMAND_RELOAD, "", "Reload plugins for the current context");		
		addSubCommand(COMMAND_NAME, SUBCOMMAND_SHOWPATH, "", "Show the DTFJ View plugin search path for the current context");		
		addSubCommand(COMMAND_NAME, SUBCOMMAND_SETPATH, "", "Set the DTFJ View plugin search path for the current context");		
	}
	
	public void run(String command, String[] args, IContext context,	PrintStream out) throws CommandException 
	{
		if(args.length == 0) {
			out.println("Error, all plugin commands require one or more parameters");
			return;
		}
		if(args[0].equalsIgnoreCase("help") || args[0].equalsIgnoreCase("?")) {
			out.println("The 'plugins' command can show or set the DTFJ View plugin search path, list the currently loaded plugins and reload them.");
			return;
		}
		if(commands.containsKey(args[0])) {
			try {
				commands.get(args[0]).invoke(this, new Object[] {args, context, out} );
			} catch (Exception e) {
				out.println("Error executing command " + e.getMessage());
			}
		} else {
			out.println(COMMAND_NAME + " " + command + " was not recognised, run " + COMMAND_NAME + " " + SUBCOMMAND_LIST + " to see all available options");
		}
	}
	
	@SuppressWarnings("unused")
	private void commandListPlugins(String[] args, IContext context, PrintStream out) throws CommandException {
		boolean csv = false;
		if(args.length >= 2) {
			csv = args[1].equalsIgnoreCase("csv");
			out.println("id,vmversion,enabled,path,modified,exception");
		}
		if(context.getPluginClassloader() == null) {
			out.println("No plugins are currently loaded");
			return;
		}
		ArrayList<PluginConfig> plugins = context.getPluginClassloader().getPlugins();
		ArrayList<PluginConfig> pluginFailures = context.getPluginClassloader().getPluginFailures();
		if((plugins.size() == 0) && (pluginFailures.size() == 0) && !csv) {
			out.println("No plugins are currently loaded");
			return;
		}
		printPlugins("Loaded plugins", plugins, out, csv);
		printPlugins("Failed plugins", pluginFailures, out, csv);
	}
	
	private void printPlugins(String msg, ArrayList<PluginConfig> plugins, PrintStream out, boolean csv) {
		if(!csv) {
			out.println(msg);
		}
		if((plugins.size() == 0) && !csv) {
			out.println("\t<none>");		//special formatting case for when no plugins are in the list
			return;
		}
		for(PluginConfig plugin : plugins) {
			if(csv) {
				out.println(plugin.toCSV());
			} else {
				out.println("\t" + plugin.getId());
			}
		}
	}
	
	private void commandShowPath(String[] args, IContext context, PrintStream out) {
		String path = System.getProperty(PluginConstants.PLUGIN_SYSTEM_PROPERTY);
		if(null == path) {
				path = "<<warning : no dtfj plugin path has been defined>>";
		} else {
			path += String.format(" (set from system property %s)", PluginConstants.PLUGIN_SYSTEM_PROPERTY);
		}
		out.println("DTFJ View Plugin search path : " + path);
	}
	
	/*
	 * Sets where plugins will be loaded from
	 */
	@SuppressWarnings("unused")
	private void commandSetPath(String[] args, IContext context, PrintStream out) {
		if(args.length != 2) {
			out.println("The setpath option only takes a single parameter of the search path");
			return;
		}
		System.getProperties().setProperty(PluginConstants.PLUGIN_SYSTEM_PROPERTY, args[1]);
		out.println("DTFJ View Plugin search path set to : " + args[1]);
		out.println("Execute " + COMMAND_NAME + " " + SUBCOMMAND_RELOAD + " to scan this path for plugins");
	}
	
	@SuppressWarnings("unused")
	private void commandReload(String[] args, IContext context, PrintStream out) {
		context.refresh();
		out.println("Plugins reloaded");
	}

}
