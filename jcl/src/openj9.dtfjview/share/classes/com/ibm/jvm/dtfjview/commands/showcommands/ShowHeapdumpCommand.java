/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2008, 2017 IBM Corp. and others
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
import com.ibm.jvm.dtfjview.heapdump.HeapDumpSettings;

@DTFJPlugin(version="1.*", runtime=false)
public class ShowHeapdumpCommand extends BaseJdmpviewCommand
{
	public static final String COMMAND_NAME = "show heapdump";
	public static final String COMMAND_DESCRIPTION = "displays heapdump settings";
	public static final String LONG_DESCRIPTION = "Parameters:none\n\n"
		+ "Prints heapdump format and file name.\n"
		+ "Use \"set heapdump\" to change settings\n";

	{
		addCommand(COMMAND_NAME, "", COMMAND_DESCRIPTION);	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length != 0) {
			out.println("\"show heapdump\" does not take any parameters.");
			return;
		}
		
		out.print("Heapdump Settings:\n\n");
		
		out.print("\tFormat: " + (HeapDumpSettings.areHeapDumpsPHD(ctx.getProperties()) ? "PHD" : "Classic (TXT)") + "\n");
		out.print("\tFile Name: " + HeapDumpSettings.getFileName(ctx.getProperties()) + "\n");
		out.print("\tMultiple heaps will be written to " 
				+ (HeapDumpSettings.multipleHeapsInMultipleFiles(ctx.getProperties()) ? "multiple files":"a single file") 
				+ "\n");
	}


	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println(LONG_DESCRIPTION);
	}
	
}
