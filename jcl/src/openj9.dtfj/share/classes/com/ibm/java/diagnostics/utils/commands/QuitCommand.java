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

import java.io.PrintStream;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.IDTFJContext;

/**
 * Command to quit out of the interactive command parser
 * 
 * @author adam
 *
 */
public class QuitCommand extends BaseCommand {

	{
		addCommand("quit", "", "Exit the application");
		addCommand("exit", "", "Exit the application");
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		switch (args.length) {
			case 0 :		//no arguments so quit out
				if(context instanceof IDTFJContext) {
					IDTFJContext dtfj_ctx = (IDTFJContext) context;
					dtfj_ctx.getProperties().setProperty("quit", "on");		//this indicates that the command parser should quit
				}
				break;
			case 1 :
				if(args[0].equalsIgnoreCase("help") || args[0].equalsIgnoreCase("?")) {
					out.println("exits the tool; any log files that are currently open will be closed prior to exit");
					break;
				}
				//allow to fall through to the case where the arguments supplied were not recognised
			default :
				out.println("The supplied parameters were not recognised");
				break;
		}
		

	}

}
