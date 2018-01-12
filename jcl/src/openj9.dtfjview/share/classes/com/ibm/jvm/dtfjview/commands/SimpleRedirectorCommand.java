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
package com.ibm.jvm.dtfjview.commands;

import java.io.PrintStream;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;

/**
 * There are a number of jdmpview commands which just act as routing
 * commands onto other more specific commands. This class provides an 
 * abstract superclass for these commands to inherit from.
 * 
 * @author adam
 *
 */
public abstract class SimpleRedirectorCommand extends BaseJdmpviewCommand {
	protected static final String SUB_CMD_FORMAT = "%s %s";
	
	/**
	 * Get the base command name which triggers this routing
	 * 
	 * @return
	 */
	protected abstract String getCmdName();
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		//this command acts as a redirector onto a more specific sub command as normally you cannot have spaces to differentiate commands
		switch(args.length) {
			case 0 :
				printDetailedHelp(out);
				break;
			case 1 :
				String subcmd = String.format(SUB_CMD_FORMAT, getCmdName(), args[0]);
				context.execute(subcmd, new String[0], out);			
				break;
			default :
				String[] arguments = new String[args.length - 1];
				if(arguments.length > 0) {
					System.arraycopy(args, 1, arguments, 0, arguments.length);
				}
				subcmd = String.format(SUB_CMD_FORMAT, getCmdName(), args[0]);
				context.execute(subcmd, arguments, out);
				break;
				
		}
	}
}
