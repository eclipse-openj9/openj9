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
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*", runtime=false)
public class CdCommand extends BaseJdmpviewCommand {
	{
		addCommand("cd", "", "changes the current working directory, used for log files");	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length == 0) {
			out.println("\"cd\" requires exactly one parameter");
			return;
		}
		String path = "";

		for(String arg : args) {
			path += arg + " ";
		}
		path = path.substring(0, path.length() - 1);
		
		File newPwd = Utils.absPath(ctx.getProperties(), path);
		
		if (!newPwd.isDirectory()) {
			if (newPwd.isFile()) {
				out.println("cannot change to specified path because it specifies a file, not a directory");
			} else {
				out.println("specified path is not a directory or a file; it probably doesn't exist");
			}
		} else{
			ctx.getProperties().put("pwd", newPwd);
		}
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("changes the current working directory, used for log files\n\n" +
				"parameters: <directory_name>\n\n" +
				"Changes the current working directory to <directory_name>, checking " +
				"to see if it exists and is a directory before making the change.  " +
				"The current working directory is where log files are outputted to; " +
				"a change to the current working directory has no effect on the current " +
				"log file setting because the logging filename is converted to an " +
				"absolute path when set.  Note: to see what the current working " +
				"directory is set to, use the \"pwd\" command.\n"
			);
		
	}
}
