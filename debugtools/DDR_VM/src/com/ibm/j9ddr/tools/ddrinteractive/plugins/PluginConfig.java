/*******************************************************************************
 * Copyright (c) 1991, 2018 IBM Corp. and others
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

import java.net.URL;

import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;
import com.ibm.j9ddr.tools.ddrinteractive.ICommand;

/**
 * Simple java bean to hold plugin configuration information
 * 
 * @author apilkington
 *
 */
public class PluginConfig {
	private final String vmversion;
	private final URL url;
	private final boolean enabled;
	private final String id;
	private final Class<ICommand> command;
	private final Throwable t;
	
	public PluginConfig(String id, String vmversion, Class<ICommand> command, boolean enabled, URL url) {
		this.id = id;
		this.vmversion = vmversion;
		this.command = command;
		this.enabled = enabled;
		this.url = url;
		t = null;
	}

	public PluginConfig(String id, Throwable t, URL url) {
		this.id = id;
		this.t = t;
		vmversion = "Unknown";
		command = null;
		enabled = false;		//can only be disabled if being created from an error
		this.url = url;
	}
	
	public Throwable getError() {
		return t;
	}
	
	public String getVmversion() {
		return vmversion;
	}

	public URL getURL() {
		return url;
	}

	public boolean isEnabled() {
		return enabled;
	}

	public String getId() {
		return id;
	}

	public ICommand newInstance() throws DDRInteractiveCommandException {
		try {
			return command.newInstance();
		} catch (Exception e) {
			throw new DDRInteractiveCommandException("Failed to create a new instance of command " + command.getClass().getName(), e);
		}
	}

	public String toCSV() {
		StringBuilder builder = new StringBuilder();
		builder.append(id);
		builder.append(',');
		builder.append(vmversion);
		builder.append(',');
		builder.append(enabled);
		builder.append(',');
		builder.append(url);
		builder.append(',');
		if(null != t) {
			builder.append(t.getClass().getName());
		}
		return builder.toString();
	}
}
