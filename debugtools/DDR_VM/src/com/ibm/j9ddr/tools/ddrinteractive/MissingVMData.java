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
package com.ibm.j9ddr.tools.ddrinteractive;

import java.util.Collection;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.J9DDRClassLoader;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;
import com.ibm.j9ddr.logging.LoggerNames;
import com.ibm.j9ddr.tools.ddrinteractive.plugins.DDRInteractiveClassLoader;
import com.ibm.j9ddr.tools.ddrinteractive.plugins.PluginConfig;

public class MissingVMData implements IVMData {
	private final Logger logger = Logger.getLogger(LoggerNames.LOGGER_INTERACTIVE_CONTEXT);
	
	public void bootstrap(String classname, Object... userData)	throws ClassNotFoundException {
		//do nothing
	}

	
	/* (non-Javadoc)
	 * @see com.ibm.j9ddr.IVMData#bootstrapRelative(java.lang.String, java.lang.Object[])
	 * 
	 * This is used to load the correct classes relative to the vm version e.g. from the vm26 package for the
	 * 26 VM. In this case the VM is missing, so only this needs to behave accordingly and represent the
	 * native side of things only.
	 * 
	 */
	public void bootstrapRelative(String relativeClassname, Object... userData)	throws ClassNotFoundException {
		if(relativeClassname.equals(Context.TASK_FINDVM)) {
			//find the VM and the userdata is an array of longs
			long[] passBackArray = (long[])userData[0];
			passBackArray[0] = 0;		//there is no VM so give it an address of zero
			return;
		}
		if(relativeClassname.equals(Context.TASK_GETCOMMANDS)) {
			//get a list of commands, as there is no VM this will only be a list of plugins which apply to any VM
			Object[] passbackArray = (Object[]) userData[0];
			Object loader = (Object) passbackArray[1];

			List<ICommand> toPassBack = new LinkedList<ICommand>();
			loadPlugins(toPassBack, loader);

			passbackArray[0] = toPassBack;
			return;
		}
		throw new ClassNotFoundException("The bootstrap relative class " + relativeClassname + " was not recognised");
	}

	private void loadPlugins(List<ICommand> toPassBack, Object obj) 
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
	
	@SuppressWarnings("unchecked")
	public Collection<StructureDescriptor> getStructures() {
		return Collections.EMPTY_LIST;
	}

	public J9DDRClassLoader getClassLoader() {
		return null;
	}

	public String getVersion() {
		return "Missing VM";
	}

}
