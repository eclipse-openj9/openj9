/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2004, 2017 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.commands.showcommands;

import java.io.PrintStream;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;
import com.ibm.jvm.dtfjview.commands.setcommands.SetLoggingCommand;

@DTFJPlugin(version=".*", runtime=false)
public class ShowLoggingCommand extends BaseJdmpviewCommand {

	{
		addCommand("show logging", "", "shows the current logging options");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length > 0) {
			out.print("\"show logging\" does not take any parameters, ignoring ");
			for(String arg : args) {
				out.print(arg + " ");
			}
			out.print("\n");
		}
		doCommand();
	}
	
	public void doCommand(){
		out.print("Logging is currently turned ");
		String value = (String)ctx.getProperties().get(SetLoggingCommand.LOG_STATE_LOGGING);
		if(value == null) {
			out.println("off");
		} else {
			out.println(value);
		}
		value = (String)ctx.getProperties().get(SetLoggingCommand.LOG_STATE_FILE);
		if(value == null) {
			out.println("Log file is not set");
		} else {
			out.println("Log file set to " + value);
		}
		out.print("Overwrite is currently turned ");
		value = (String)ctx.getProperties().get(SetLoggingCommand.LOG_STATE_OVERWRITE);
		if(value == null) {
			out.println("off");
		} else {
			out.println(value);
		}
	}
	
	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("displays the current values of logging settings\n\n" +
				"parameters: none\n" );
	}
}
