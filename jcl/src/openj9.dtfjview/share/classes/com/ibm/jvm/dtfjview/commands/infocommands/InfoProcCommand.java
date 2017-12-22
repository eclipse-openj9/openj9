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
package com.ibm.jvm.dtfjview.commands.infocommands;

import java.io.PrintStream;
import java.util.Enumeration;
import java.util.Iterator;
import java.util.Properties;

import com.ibm.dtfj.image.CorruptData;
import com.ibm.dtfj.image.CorruptDataException;
import com.ibm.dtfj.image.DataUnavailable;
import com.ibm.dtfj.image.ImageAddressSpace;
import com.ibm.dtfj.image.ImageProcess;
import com.ibm.dtfj.image.ImageThread;
import com.ibm.dtfj.java.JavaVMInitArgs;
import com.ibm.dtfj.java.JavaVMOption;
import com.ibm.java.diagnostics.utils.IContext;
import com.ibm.java.diagnostics.utils.commands.CommandException;
import com.ibm.java.diagnostics.utils.plugins.DTFJPlugin;
import com.ibm.jvm.dtfjview.commands.BaseJdmpviewCommand;
import com.ibm.jvm.dtfjview.commands.helpers.Exceptions;
import com.ibm.jvm.dtfjview.commands.helpers.Utils;

@DTFJPlugin(version="1.*", runtime=false)
public class InfoProcCommand extends BaseJdmpviewCommand {
	{
		addCommand("info process", "", "displays threads, command line arguments, environment");				
		addCommand("info proc", "", "shortened form of info process");
	}
	
	public void run(String command, String[] args, IContext context, PrintStream out) throws CommandException {
		if(initCommand(command, args, context, out)) {
			return;		//processing already handled by super class
		}
		if(args.length != 0) {
			out.println("\"info proc\" command does not take any parameters");
			return;
		}
		printAddressSpaceInfo();
	}
	
	private void printAddressSpaceInfo()
	{
		ImageAddressSpace ias = ctx.getAddressSpace();

		ImageProcess ip = ias.getCurrentProcess();
		if (ip == null) {
			out.print("\t(No process in this address space)\n");
		} else {
			printProcessInfo();
		}
		out.print("\n");
	}
	
	private void printProcessInfo()
	{
		out.print("\n");
		printProcessID();
		out.print("\n");
		printThreads();
		out.print("\n");
		printCommandLine();
		if (ctx.getRuntime() != null) {
			out.print("\n");
			printJITOptions();
		}
		out.print("\n");
		printEnvironmentVariables();
	}

	private void printProcessID() {
		out.print("\t Process ID:");
		out.print("\n\t  ");
		try {
			out.print( ctx.getProcess().getID() );
		} catch (DataUnavailable e) {
			out.print(Exceptions.getDataUnavailableString());
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
			
		}
		out.print("\n");
	}
	
	private void printJITOptions() {
		try {
			if(ctx.getRuntime().isJITEnabled()) {
				out.println("\t JIT was enabled for this runtime");
				Properties props = ctx.getRuntime().getJITProperties();
				StringBuilder options = new StringBuilder("\t ");
				for(Object key : props.keySet()) {
					options.append(" " + key + " " + props.get(key) + ",");
				}
				out.println(options.substring(0, options.length() - 1));
			} else {
				out.println("\t JIT was disabled for this runtime");
			}
		} catch (CorruptDataException e) {
			out.println("\t JIT options\n");
		} catch (DataUnavailable e) {
			out.println("\t JIT options not supported by this implementation of DTFJ");
		}
	}
	
	private void printThreads()
	{
		ImageProcess ip = ctx.getProcess();
		out.print("\t Native thread IDs:");
		out.print("\n\t  ");
		int lineLength = 10; // normal tab plus 2 spaces

		// FIXME: should output architecture (32/64-bit), endianness before threads

		Iterator<?> itThread = ip.getThreads();
		while (itThread.hasNext())
		{
			Object next = itThread.next();
			if (next instanceof CorruptData) {
		        out.print("\n\t  <corrupt data>");
		        continue;
		    }
			ImageThread it = (ImageThread)next;

			try {
				String threadID = it.getID();
				if (threadID != null) {
					if (lineLength + threadID.length() > 80) {
						out.print("\n\t  ");
						lineLength = 10;
					}
					out.print(it.getID() + " ");
					lineLength += threadID.length() + 1;
				}
			} catch (CorruptDataException e) {
				out.print(Exceptions.getCorruptDataExceptionString());
			}
		}
	out.print("\n");
	}

	private void printCommandLine()
	{
		ImageProcess ip = ctx.getProcess();
		out.print("\t Command line:\n\t  ");
		
		try {
			String commandLine = ip.getCommandLine();
			if (null == commandLine)
				out.print("<null>");
			else
				out.print(commandLine);
		} catch (CorruptDataException e) {
			out.print(Exceptions.getCorruptDataExceptionString());
		} catch (DataUnavailable e) {
			out.print("Could not determine command line");
		}
		out.println();
		out.println();
		// Print the VM initialization options, similar to javacore.
		try {
			JavaVMInitArgs args = ctx.getRuntime().getJavaVMInitArgs();
			if(args != null) {
				out.print("\t Java VM init options: ");
				Iterator<?> opts = args.getOptions();
				while(opts.hasNext()) {
					out.println();
					Object obj = opts.next();
					if(obj instanceof JavaVMOption) {
						JavaVMOption opt = (JavaVMOption) obj;
						out.print("\t  " + opt.getOptionString());
						if( opt.getExtraInfo().getAddress() != 0 ) {
							out.print(" " + Utils.toHex(opt.getExtraInfo()) );
						}
					}
				}
			} else {
				out.print(Exceptions.getDataUnavailableString());
			}
		} catch (Exception cde) {
			//in the event of an exception getting the VM init options, just print the original message
			out.print(Exceptions.getDataUnavailableString());
		}
		out.print("\n");
	}
	
	private void printEnvironmentVariables()
	{
		ImageProcess ip = ctx.getProcess();
		out.print("\t Environment variables:");
		out.print("\n");
		
		Properties variables;
		try {
			variables = ip.getEnvironment();
		} catch (CorruptDataException e) {
			out.print("\t  " + Exceptions.getCorruptDataExceptionString() + "\n");
			return;
		} catch (DataUnavailable e) {
			out.print("\t  " + Exceptions.getDataUnavailableString() + "\n");
			return;
		}
		
		Enumeration<?> keys = variables.propertyNames();
		while (keys.hasMoreElements())
		{
			String key = (String)keys.nextElement();
			printVariableInfo(key, variables.getProperty(key));
		}
	}
	
	private void printVariableInfo(String key, String value)
	{
		out.print("\t  " + key + "=" + value + "\n");
	}
	
	@Override
	public void printDetailedHelp(PrintStream out) {
		out.println("displays process ID, threads, command line arguments and environment " +
		"variables for the current process\n\n" +

		"parameters: none\n\n" +
		"prints the following information about the current process\n\n" +
		"  - process ID for the process and thread IDs for all its threads\n" +
		"  - the command line arguments and JVM init arguments it's using\n" +
		"  - its environment variables\n\n" +
		"note: to view the shared libraries used by a process, use the " +
		"\"info mod\" command\n"
);
	}
}
