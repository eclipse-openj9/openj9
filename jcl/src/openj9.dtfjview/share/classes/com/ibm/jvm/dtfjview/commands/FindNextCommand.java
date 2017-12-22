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

import java.io.PrintStream;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*", runtime=false)
public class FindNextCommand extends BaseJdmpviewCommand{
	{
		addCommand("findnext", "", "finds the next instance of the last string passed to \"find\"");	
	}
	

	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		FindCommand.FindAttribute findAtt = (FindCommand.FindAttribute)ctx.getProperties().get(Utils.FIND_ATTRIBUTES);
		if (null == findAtt){
			out.println("No find command has been executed.");
			return;
		}
		
		String startAddress = Long.toHexString((findAtt.lastMatch + 1));
		String endAddress = Long.toHexString(findAtt.endAddress);
		
		ctx.execute("find" + " " 
				+ findAtt.pattern + "," + startAddress + ","
				+ endAddress + "," + findAtt.boundary + ","
				+ findAtt.numBytesToPrint + "," + findAtt.numMatchesToDisplay, out);
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("finds the next instance of the last string passed to \"find\"\n\n" +
				"parameters: none\n\n" +
				"The findnext command is used in conjunction with find or findptr command " +
				"to continue searching for upcoming matches. It repeats " +
				"the previous find or findptr command (depending on which command is " +
				"most recently issued) starting from the last match."
				);
	}
}
