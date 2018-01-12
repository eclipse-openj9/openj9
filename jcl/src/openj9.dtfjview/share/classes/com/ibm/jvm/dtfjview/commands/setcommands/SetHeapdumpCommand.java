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
package com.ibm.jvm.dtfjview.commands.setcommands;

import java.io.PrintStream;

import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;
import com.ibm.jvm.dtfjview.heapdump.HeapDumpSettings;

@DTFJPlugin(version="1.*")
public class SetHeapdumpCommand extends BaseJdmpviewCommand
{
	private static final String SHORT_DESCRIPTION = "configures heapdump format, filename and multiple heap support";
	private static final String COMMAND_NAME = "set heapdump";
	private static final String LONG_DESCRIPTION = "parameters: [phd|txt], [file <filename>], [multiplefiles on|off]\n\n" 
		+ "[phd|txt] - the format for the heapdump. Default: phd.\n"
		+ "[file <filename>] - the file to write the heapdump to. Default: <core file name>.phd or <core file name>.txt.\n\n"
		+ "[multiplefiles on|off] - if set to on, multiple heaps are written to separate heapdumps. If set to off, multiple heaps are written " +
				"to the same heapdump. Default: off.\n\n"
		+ "Use \"show heapdump\" to see current settings.\n";

	{
		addCommand(COMMAND_NAME, "", SHORT_DESCRIPTION);	
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length == 0) {
			out.println("\"set heapdump\" requires at least one parameter\n");
			return;
		}
	
		String arg1 = args[0];
		
		if(arg1.equalsIgnoreCase("phd")) {
			HeapDumpSettings.setPHDHeapDumps(ctx.getProperties());
			
			out.print("Heapdump format set to PHD\n");
		} else if (arg1.equalsIgnoreCase("txt")){
			HeapDumpSettings.setClassicHeapDumps(ctx.getProperties());
			
			out.print("Heapdump format set to classic (TXT)\n");
		} else if (arg1.equalsIgnoreCase("file")) {
			if(args.length == 1) {
				out.println("\"set heapdump file\" requires at least one parameter\n");
				return;
			}
			
			if(args.length > 2) {
				out.println("\"set heapdump file\" accepts 1 parameter. You supplied " + (args.length - 1) + ".\n");
				return;
			}
			
			String filename = args[1];
			
			String originalFileName = HeapDumpSettings.getFileName(ctx.getProperties());
			
			HeapDumpSettings.setFileName(filename, ctx.getProperties());
			
			out.print("Heapdump file changed from " + originalFileName + " to " + filename + "\n");
		} else if (arg1.equalsIgnoreCase("multiplefiles")) {
			if(args.length == 1) {
				out.println("\"set heapdump multiplefiles\" requires one parameter: on or off\n");
				return;
			}
			
			if(args.length > 2) {
				out.println("\"set heapdump multiplefiles\" requires one parameter: on or off. You suppled " + (args.length - 1) + " parameters\n");
				return;
			}
			
			String setting = args[1];
			
			if(setting.equalsIgnoreCase("on")) {
				out.println("Multiple heaps will be dumped into multiple heapdumps");
				HeapDumpSettings.setMultipleHeapsMultipleFiles(ctx.getProperties());
			} else if (setting.equalsIgnoreCase("off")) {
				out.println("Multiple heaps will be dumped into a single heapdump");
				HeapDumpSettings.setMultipleHeapsSingleFile(ctx.getProperties());
			} else {
				out.println("Unrecognised setting: " + setting + ". Valid options are \"on\" or \"off\"\n");
			}
		} else {
			out.println(arg1 + " is not a valid parameter for the \"set heapdump\" command");
		}
	}

	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println(LONG_DESCRIPTION);
	}

}
