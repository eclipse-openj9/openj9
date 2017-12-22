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
package com.ibm.jvm.dtfjview.spi;

import com.ibm.java.diagnostics.utils.commands.CommandException;

/**
 * Represents a single jdmpview session which may contain multiple contexts.
 * 
 * @author adam
 *
 */
public interface ISession {

	/**
	 * The instance which is managing all registered output channels.
	 * 
	 * @return the manager
	 */
	public IOutputManager getOutputManager();

	/**
	 * The currently selected context against which commands will be executed.
	 * 
	 * @return the current context, it may or may not have DDR interactive support
	 */
	public ICombinedContext getCurrentContext();

	/**
	 * Get the manager which is managing this context within this session.
	 * 
	 * @return the context manager
	 */
	public ISessionContextManager getContextManager();

	/**
	 * Set the context within which subsequent commands will be executed.
	 * 
	 * @param id context ID
	 * @throws CommandException thrown if an invalid context ID is specified
	 */
	public void setContext(int id) throws CommandException;

	/**
	 * Set the context within which subsequent commands will be executed.
	 * 
	 * @param switchTo the context to switch to
	 * @throws CommandException thrown if the switch fails. The underlying reason will be in the logs.
	 */
	public void setContext(ICombinedContext switchTo) throws CommandException;

	/**
	 * Display the contexts which are present in the core file currently being analysed.
	 * 
	 * @param shortFormat true for short format which just displays the first line of the java -version string
	 */
	public void showContexts(boolean shortFormat);

	/**
	 * Convenience method which instructs the session to search through all the
	 * available contexts and set the current one to the first
	 * one it finds with a non-corrupt DTFJ JavaRuntime in it.
	 */
	public void findAndSetContextWithJVM();
	
	/**
	 * Executes the supplied command. Output will be written via the IOutputManager to 
	 * all registered channels.
	 * 
	 * @param command the command and any parameters to execute.
	 */
	public void execute(String command);

}
