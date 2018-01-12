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
package com.ibm.java.diagnostics.utils.plugins;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Decorator class for the plugin manager which adds DTFJ specific capabilities.
 * 
 * @author adam
 *
 */
public class DTFJPluginManager {
	private static final String LISTENER_ID = "dtfj.class.listener";	//ID to prevent more than one DTFJ class listener being added
	private Logger logger = Logger.getLogger(PluginConstants.LOGGER_NAME);
	public static final String ANNOTATION_CLASSNAME = "Lcom/ibm/java/diagnostics/utils/plugins/DTFJPlugin;";
	private final PluginManager manager;
	

	public DTFJPluginManager(PluginManager manager) {
		this.manager = manager;
		manager.addListener(new DTFJClassListener(LISTENER_ID));
		String className = System.getProperty(PluginConstants.PLUGIN_LISTENER_FACTORY);
		if(className != null) {
			try {
				Class<?> clazz = Class.forName(className);
				if(!PluginListenerFactory.class.isAssignableFrom(clazz)) {
					throw new IllegalArgumentException("The class " + className + " specified by the system property " 
							+ PluginConstants.PLUGIN_LISTENER_FACTORY + " does not implement " + PluginListenerFactory.class.getName());
				}
				PluginListenerFactory factory = (PluginListenerFactory) clazz.newInstance();
				Set<ClassListener> listeners = factory.getListeners();
				if(listeners != null) {
					for(ClassListener listener : listeners) {
						manager.addListener(listener);
					}
				}
			} catch (Exception e) {
				//log and ignore exception
				logger.log(Level.FINE, "Error occurred loading listener factory, setting is ignored.", e);
			}
		}
	}

	public List<DTFJPluginConfig> getPlugins(String apiLevel, boolean hasImage, boolean hasRuntime) {
		List<DTFJPluginConfig> availablePlugins = new ArrayList<DTFJPluginConfig>();
		for(ClassListener listener : manager.getListeners()) {
			setAvailablePlugins(availablePlugins, listener.getPluginList(), apiLevel, hasImage, hasRuntime);
		}
		return availablePlugins;
	}
	
	/*
	 * Sets the available plugins from the supplied list. Allows the manager to iterate 
	 * over all the listeners and give them a chance to add their plugins as well.
	 */
	private void setAvailablePlugins(List<DTFJPluginConfig> availablePlugins, Set<PluginConfig> plugins, String apiLevel, boolean hasImage, boolean hasRuntime) {
		if(plugins == null) {
			return;		//when no plugins are available null is a valid value
		}
		for(PluginConfig plugin : plugins) {
			if(!(plugin instanceof DTFJPluginConfig)) {
				continue;
			}
			DTFJPluginConfig config = (DTFJPluginConfig) plugin;
			Pattern pattern = Pattern.compile(config.getVersion());
			Matcher matcher = pattern.matcher(apiLevel);
			if(matcher.matches()) {									//match API version
				if(config.isRuntime() && !hasRuntime) {
					continue;		//runtime required but is not available
				}
				if(config.isImage() && !hasImage) {
					continue;		//Image required but is not available
				}
				availablePlugins.add(config);
			}
		}
	}	
}
