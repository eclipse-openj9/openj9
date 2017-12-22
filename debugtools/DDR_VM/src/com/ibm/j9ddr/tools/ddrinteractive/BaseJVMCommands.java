/*******************************************************************************
 * Copyright (c) 2015, 2015 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive;

import java.util.LinkedList;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.j9ddr.tools.ddrinteractive.commands.GpInfoCommand;
import com.ibm.j9ddr.tools.ddrinteractive.commands.SnapFormatWrapperCommand;
import com.ibm.j9ddr.tools.ddrinteractive.commands.SnapTraceCommand;
import com.ibm.j9ddr.tools.ddrinteractive.plugins.DDRInteractiveClassLoader;
import com.ibm.j9ddr.tools.ddrinteractive.plugins.PluginConfig;

public abstract class BaseJVMCommands {

	private final Logger logger = CommandUtils.getLogger();
	
	/*
	 * Return the list of commands that apply to all JVMs without needing
	 * to know which VM level they are running against.
	 * All the com.ibm.j9ddr.vmXX.tools.ddrinteractive.GetCommandsTask classes
	 * can inherit from this to share the list.
	 * 
	 * (Different from the list of commands in DDRInteractive as those apply
	 * to all dumps (for example those from Node, Ruby, Python etc)
	 * 
	 */
	protected List<ICommand> getBaseJVMCommands() {
		List<ICommand> baseJVMCommands = new LinkedList<ICommand>();
		
		baseJVMCommands.add(new GpInfoCommand());
		baseJVMCommands.add(new SnapTraceCommand());
		baseJVMCommands.add(new SnapFormatWrapperCommand());
		
		return baseJVMCommands;
	}
	
	protected void loadPlugins(List<ICommand> toPassBack, Object obj) 
	{
		if (!(obj instanceof DDRInteractiveClassLoader)) {
			logger.fine("Plugin classloader is not an instance of DRRInteractiveClassLoader. No plugins have been loaded");
			return;
		}
		DDRInteractiveClassLoader loader = (DDRInteractiveClassLoader) obj;
		for (PluginConfig plugin : loader.getPlugins()) {
			try {
				ICommand command = plugin.newInstance();
				toPassBack.add(command);
			} catch (Throwable e) {
				// log exception and move to next command
				logger.log(Level.WARNING, "Failed to create an instance of " + plugin.getId(), e);
			}
		}
	}
	
}
