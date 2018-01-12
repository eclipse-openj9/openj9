/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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

package com.ibm.j9ddr.tools.ddrinteractive.plugins;

import java.io.PrintStream;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;

public class PluginCommand extends Command 
{
	private static final String COMMAND_NAME = "!plugins";
	private static final String COMMAND_LIST = "list";
	private static final String COMMAND_RELOAD = "reload";
	private static final String COMMAND_SHOWPATH = "showpath";
	private static final String COMMAND_SETPATH = "setpath";
	private static final String COMMAND_HELP = "help";
	private static final String COMMAND_HELP_QMARK = "?";

	private final Map<String, Method> commands = new HashMap<String, Method>(); 
	private CommandDescription cd = null;
	
	{
		try {
			commands.put(COMMAND_LIST, getClass().getDeclaredMethod("commandListPlugins", new Class[] {String[].class, Context.class, PrintStream.class}));
			commands.put(COMMAND_RELOAD, getClass().getDeclaredMethod("commandReload", new Class[] {String[].class, Context.class, PrintStream.class}));
			commands.put(COMMAND_SHOWPATH, getClass().getDeclaredMethod("commandShowPath", new Class[] {String[].class, Context.class, PrintStream.class}));
			commands.put(COMMAND_SETPATH, getClass().getDeclaredMethod("commandSetPath", new Class[] {String[].class, Context.class, PrintStream.class}));
			commands.put(COMMAND_HELP, getClass().getDeclaredMethod("commandHelp", new Class[] {String[].class, Context.class, PrintStream.class}));
			commands.put(COMMAND_HELP_QMARK, getClass().getDeclaredMethod("commandHelp", new Class[] {String[].class, Context.class, PrintStream.class}));
		} catch (Exception e) {
			System.err.println("Error creating command list : " + e.getMessage());
		}
	}
	
	public PluginCommand()
	{
		cd = addCommand("plugins", "<subcmd>", "DDR Plugin management commands");
		cd.addSubCommand("list", "", "Show the list of loaded plugins for the current context");
		cd.addSubCommand("reload", "", "Reload plugins for the current context");
		cd.addSubCommand("showpath", "", "Displays the current plugin search path");
		cd.addSubCommand("setpath", "<search path>", "Sets the current plugin search path");
	}
	
	public void run(String command, String[] args, Context context,	PrintStream out) throws DDRInteractiveCommandException 
	{
		if(args.length == 0) {
			out.println("\n");
			out.println("Error, all plugin commands require one or more parameters, see !plugins help for more information");
			return;
		}
		
		if(commands.containsKey(args[0])) {
			try {
				commands.get(args[0]).invoke(this, new Object[] {args, context, out} );
			} catch (InvocationTargetException e) { // likely to have originally been a DDRInteractiveCommandException
				throw new DDRInteractiveCommandException(e.getCause().getMessage());
			} catch (Exception e) {
				throw new DDRInteractiveCommandException(e.getMessage());
			}
		} else {
			out.println(COMMAND_NAME + " " + command + " was not recognised, run " + COMMAND_NAME + " " + COMMAND_HELP + " to see all available options");
		}
	}
	
	/*
	 * Lists all the loaded plugins and any that failed to load and why.
	 */
	@SuppressWarnings("unused")
	private void commandListPlugins(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		boolean csv = false;
		if(args.length >= 2) {
			csv = args[1].equalsIgnoreCase("csv");
			out.println("id,vmversion,enabled,path,modified,exception");
		}
		DDRInteractiveClassLoader loader = context.getPluginClassloader();
		if (loader == null) {
			out.println("No plugins are currently loaded");
			return;
		}
		ArrayList<PluginConfig> plugins = loader.getPlugins();
		ArrayList<PluginConfig> pluginFailures = loader.getPluginFailures();			
		if((plugins.size() == 0) && (pluginFailures.size() == 0) && !csv) {
			out.println("No plugins are currently loaded");
			return;
		}
		printPlugins(plugins, out, csv);
		printPlugins(pluginFailures, out, csv);
	}
	
	private void printPlugins(ArrayList<PluginConfig> plugins, PrintStream out, boolean csv) {
		for(PluginConfig plugin : plugins) {
			if(csv) {
				out.println(plugin.toCSV());
			} else {
				out.println("\t" + plugin.getId());
			}
		}
	}
	
	/*
	 * Displays where plugins are being loaded from.
	 * 
	 */
	@SuppressWarnings("unused")
	private void commandShowPath(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		String path = System.getProperty(DDRInteractiveClassLoader.PLUGIN_SYSTEM_PROPERTY);
		if(null == path) {
			path = System.getenv(DDRInteractiveClassLoader.PLUGIN_ENV_VAR);
			if(null == path) {
				path = "<<warning : no plugin path has been defined>>";
			} else {
				path += String.format(" (set from environment variable %s)", DDRInteractiveClassLoader.PLUGIN_ENV_VAR);	
			}
		} else {
			path += String.format(" (set from system property %s)", DDRInteractiveClassLoader.PLUGIN_SYSTEM_PROPERTY);
		}
		out.println("DDR Plugin search path : " + path);
	}
	
	/*
	 * Sets where plugins will be loaded from
	 */
	@SuppressWarnings("unused")
	private void commandSetPath(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		if(args.length != 2) {
			out.println("The setpath option only takes a single parameter of the search path");
			return;
		}
		System.getProperties().setProperty(DDRInteractiveClassLoader.PLUGIN_SYSTEM_PROPERTY, args[1]);
		out.println("Plugin search path set to : " + args[1]);
		out.println("Execute " + COMMAND_NAME + " " + COMMAND_RELOAD + " to scan this path for plugins");
	}
	
	/*
	 * Will initiate a reloading of all commands by creating a new classloader and
	 * re-scanning the plugin search path.
	 * 
	 */
	@SuppressWarnings("unused")
	private void commandReload(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException{
		context.refreshCommandList();
		out.println("Plugins reloaded");
	}
	
	/*
	 * Prints out detailed help
	 */
	@SuppressWarnings("unused")
	private void commandHelp(String[] args, Context context, PrintStream out) throws DDRInteractiveCommandException {
		printDetailedHelp(out);
	}
}
