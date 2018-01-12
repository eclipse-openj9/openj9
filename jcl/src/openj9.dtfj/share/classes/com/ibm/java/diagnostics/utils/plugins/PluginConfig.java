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

package com.ibm.java.diagnostics.utils.plugins;

import java.net.URL;

import com.ibm.java.diagnostics.utils.commands.BaseCommand;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.commands.ICommand;

/**
 * Simple Java bean to represent a plugin
 * 
 * @author adam
 *
 */
public abstract class PluginConfig {
	protected final Entry entry;
	protected Throwable t = null;			//any exception which occurred when instantiating a new instance
	protected boolean cacheOutput = true;	//default the caching of output to true
	
	public PluginConfig(Entry entry) {
		this.entry = entry;
	}
	
	public String getName() {
		return entry.getName();
	}
	
	public ICommand newInstance(ClassLoader loader) throws CommandException {
		ClassInfo info = entry.getData();
		try {
			Class<?> clazz = Class.forName(info.getClassname(), true, loader);
			Object instance = clazz.newInstance();
			if(instance instanceof BaseCommand) {
				((BaseCommand)instance).setConfig(this);
			}
			return (ICommand) instance;
		} catch (Throwable e) {
			//have to use Throwable here to catch things like class not found errors
			t = e;
			throw new CommandException("Failed to create a new instance of command " + info.getClassname(), e);
		}
	}

	public abstract String toCSV();
	
	public String getClassName() {
		ClassInfo info = entry.getData();
		return info.getClassname();
	}

	@Override
	public boolean equals(Object o) {
		if((o == null) || !(o instanceof PluginConfig)) {
			return false;
		}
		PluginConfig compareTo = (PluginConfig) o;
		Object o1 = entry.getData();
		Object o2 = compareTo.entry.getData();
		return o1.equals(o2);
	}

	@Override
	public int hashCode() {
		return getClassName().hashCode();
	}
	
	public String getId() {
		return getClassName();
	}
	
	public Throwable getError() {
		return t;
	}
	
	public boolean isEnabled() {
		return (t == null);
	}
	
	public URL getURL() {
		return entry.toURL();
	}
	
	public boolean cacheOutput() {
		return cacheOutput;
	}
}
