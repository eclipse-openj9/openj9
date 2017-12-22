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
package com.ibm.jvm.dtfjview.commands;

import java.io.File;
import java.io.PrintStream;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.SessionProperties;

@DTFJPlugin(version=".*", runtime=false)
public class PwdCommand extends BaseJdmpviewCommand {
	{
		addCommand("pwd", "", "displays the current working directory");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length != 0) {
			out.println("\"pwd\" does not take any parameters");
			return;
		}
		File pwd = (File)ctx.getProperties().get(SessionProperties.PWD_PROPERTY);
		
		out.print("\n");
		out.print("\t" + pwd.getPath());
		out.print("\n\n");
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("displays the current working directory\n\n" +
				"parameters: none\n\n" +
				"displays the current working directory, which is the directory " +
				"where log files are stored; see the help for \"cd\" for more " +
				"information\n"
		);
	}
}
