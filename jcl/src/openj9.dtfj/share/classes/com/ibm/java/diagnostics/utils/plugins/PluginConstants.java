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
package com.ibm.java.diagnostics.utils.plugins;

/**
 * Shared constants between plugin classes
 * 
 * @author adam
 *
 */
public interface PluginConstants {
	/**
	 * The system property which defines the search path to use for plugins.
	 * This is a list of directories or jars separated by the path separator
	 * on which this is being run i.e. : or ; 
	 */
	public static final String PLUGIN_SYSTEM_PROPERTY = "com.ibm.java.diagnostics.plugins";
	
	/**
	 * The name of the logger used by plugins
	 */
	public static final String LOGGER_NAME = "com.ibm.java.diagnostics.plugins";
	
	/**
	 * If set it specifies the name of a class to be instantiated from which additional
	 * plugin listeners will be created. These listeners will be notified when class files
	 * are scanned and so can determine if they represent a plugin or not.
	 */
	public static final String PLUGIN_LISTENER_FACTORY="com.ibm.java.diagnostics.plugins.listener";
	
	/**
	 * Setting this property to any value will cause the package filtering classloading of ASM and DTFJ used
	 * by the plugin scanner to be disabled. This will typically be done when these files are already on the
	 * classpath and are a later version than those shipped with the SDK e.g. in an OSGi plugin.
	 */
	public static final String PACKAGE_FILTER_DISABLE="com.ibm.java.diagnostics.nopackagefilter";
}
