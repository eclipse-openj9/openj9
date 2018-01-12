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
package com.ibm.java.diagnostics.utils.commands;

/**
 * Description for how a command should be executed
 * 
 * @author adam
 *
 */
public class CommandDescription {
	private String commandName;
	private String argumentDescription;
	private String helpDescription;
	private CommandDescription parent = null;
	
	public CommandDescription(String commandName, String argumentDescription, String helpDescription) {
		super();
		this.commandName = commandName;
		this.argumentDescription = argumentDescription;
		this.helpDescription = helpDescription;
	}
	
	public String getCommandName() {
		return commandName;
	}
	
	public void setCommandName(String commandName) {
		this.commandName = commandName;
	}
	
	public String getArgumentDescription() {
		return argumentDescription;
	}
	
	public void setArgumentDescription(String argumentDescription) {
		this.argumentDescription = argumentDescription;
	}
	
	public String getHelpDescription() {
		return helpDescription;
	}
	
	public void setHelpDescription(String helpDescription) {
		this.helpDescription = helpDescription;
	}

	public CommandDescription getParent() {
		return parent;
	}

	public void setParent(CommandDescription parent) {
		this.parent = parent;
	}
}
