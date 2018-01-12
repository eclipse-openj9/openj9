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

import java.util.ArrayList;
import java.util.Properties;
import java.util.logging.Level;

import com.ibm.dtfj.image.Image;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.java.JavaRuntime;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.commands.ICommand;
import com.ibm.java.diagnostics.utils.plugins.DTFJPluginManager;
import com.ibm.java.diagnostics.utils.plugins.LocalPriorityClassloader;
import com.ibm.java.diagnostics.utils.plugins.PluginConfig;
import com.ibm.java.diagnostics.utils.plugins.PluginLoader;
import com.ibm.java.diagnostics.utils.plugins.PluginManager;
import com.ibm.java.diagnostics.utils.plugins.PluginManagerLocator;


/**
 * A DTFJ context within which a DTFJ command executes
 * 
 * @author adam
 *
 */
public class DTFJContext extends Context implements IDTFJContext, PluginLoader {
	private final JavaRuntime rt;
	private final ImageAddressSpace space;
	private final ImageProcess proc;
	private final int major;
	private final int minor;
	private final Properties props = new Properties();
	private DTFJImageBean bean = null;
	private final String apiLevel;
	private final boolean hasImage;
	private ArrayList<PluginConfig> loadedPlugins = new ArrayList<PluginConfig>();
	private ArrayList<PluginConfig> failedPlugins = new ArrayList<PluginConfig>();
	
	public DTFJContext(int major, int minor,Image image, ImageAddressSpace space, ImageProcess proc, JavaRuntime rt) {
		super();
		this.rt = rt;
		this.proc = proc;
		this.space = space;
		this.major = major;
		this.minor = minor;
		bean = new DTFJImageBean(image);
		hasImage = (image != null);
		apiLevel = String.format("%d.%d", major, minor);
	}

	public Properties getProperties() {
		return props;
	}

	public int getMajorVersion() {
		return major;
	}

	public int getMinorVersion() {
		return minor;
	}

	public ImageAddressSpace getAddressSpace() {
		return space;
	}

	public JavaRuntime getRuntime() {
		return rt;
	}

	public ImageProcess getProcess() {
		return proc;
	}

	/**
	 * Refresh this context.
	 */
	public void refresh() {
		ArrayList<String> names = new ArrayList<String>();		//used to detect command name clashes
		commands.clear();
		loadedPlugins.clear();
		failedPlugins.clear();
		//these are the only hardwired commands that are always available within any context
		addGlobalCommandsToContext();
		PluginManager pluginManager = PluginManagerLocator.getManager();
		DTFJPluginManager manager = new DTFJPluginManager(pluginManager);
		try {
			pluginManager.refreshSearchPath();
			pluginManager.scanForClassFiles();
		} catch (CommandException e) { // may catch a CommandException when loading plugins 
			logger.log(Level.FINE, "Problem loading DTFJ View plugins: " + e.getMessage());
			logger.log(Level.FINEST, "Problem loading DTFJ View plugins: ", e);
		}
		ClassLoader thisloader = this.getClass().getClassLoader();
		 if(thisloader == null) {
			 thisloader = ClassLoader.getSystemClassLoader();
		 }
		LocalPriorityClassloader loader = new LocalPriorityClassloader(pluginManager.getClasspath(), thisloader);
		for(PluginConfig config : manager.getPlugins(apiLevel, hasImage, (rt != null))) {
			try {
				ICommand cmd = config.newInstance(loader);
				loadedPlugins.add(config);
				for(String cmdname : cmd.getCommandNames()) {
					//check for a name space clash i.e. two plugins could potentially be invoked by a single command
					if(names.contains(cmdname)) {
						logger.fine("Name space clash detected for " + config.getClassName() + ". The command name " + cmdname + " is already in use");
					} else {
						names.add(cmdname);
					}
				}
				commands.add(cmd);
			} catch (CommandException e) {
				failedPlugins.add(config);
				logger.log(Level.FINE, "Failed to create a new plugin instance for " + config.getClassName());
				logger.log(Level.FINEST, "Failed to create a new plugin instance: ", e);
			}
		}
	}

	
	
	public DTFJImageBean getImage() {
		return bean;
	}
	
	/**
	 * Used to determine if a property has been set in the context property
	 * bag, and that the value of that property is TRUE
	 * @param name property name to check
	 * @return true if the property exists and Boolean.parseValue() returns true
	 */
	public boolean hasPropertyBeenSet(String name) {
		if(getProperties().containsKey(name)) {
			Object value = getProperties().get(name);
			if(value == null) {
				return false;		//not supplied
			} else {
				return Boolean.parseBoolean(value.toString());
			}
		} else {
			//this is the default of not being set
			return false;
		}
	}

	public ArrayList<PluginConfig> getPlugins() {
		return loadedPlugins;
	}

	public ArrayList<PluginConfig> getPluginFailures() {
		return failedPlugins;
	}

	public void loadPlugins() throws CommandException {
		refresh();
	}

	public PluginLoader getPluginClassloader() {
		return this;
	}
}
