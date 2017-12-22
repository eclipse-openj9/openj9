/*******************************************************************************
 * Copyright (c) 1991, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive;

import java.io.PrintStream;
import java.text.ParseException;
import java.util.Collections;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Vector;
import java.util.logging.Level;
import java.util.logging.Logger;

import com.ibm.j9ddr.CorruptDataException;
import com.ibm.j9ddr.IVMData;
import com.ibm.j9ddr.StructureReader.StructureDescriptor;
import com.ibm.j9ddr.command.CommandParser;
import com.ibm.j9ddr.corereaders.Platform;
import com.ibm.j9ddr.corereaders.memory.IProcess;
import com.ibm.j9ddr.logging.LoggerNames;
import com.ibm.j9ddr.tools.ddrinteractive.plugins.DDRInteractiveClassLoader;

/**
 * Represents a process and its VMData.
 */
public class Context 
{
	public static final String TASK_FINDVM = "tools.ddrinteractive.FindVMTask";
	public static final String TASK_GETCOMMANDS = "tools.ddrinteractive.GetCommandsTask";
	public final IProcess process;
	public final IVMData vmData;
	public final long vmAddress;
	public List<ICommand> nonVMCommands;
	public List<ICommand> commands;
	
	/**
	 * Shared logger for all commands to write to
	 */
	public final Logger logger = Logger.getLogger(LoggerNames.LOGGER_INTERACTIVE_CONTEXT);
	
	private DDRInteractiveClassLoader loader;
	
	public Context(IProcess process, IVMData vmData, List<ICommand> nonVMCommands) {
		this.vmData = vmData;
		this.process = process;
		this.nonVMCommands = nonVMCommands;
		this.vmAddress = getVMAddress();
		refreshCommandList();
	}
	
	public DDRInteractiveClassLoader getPluginClassloader() {
		return loader;
	}
	
	public void refreshCommandList() {
		try {
			if(vmData.getClassLoader() == null) {
				loader = new DDRInteractiveClassLoader(vmData, this.getClass().getClassLoader());
			} else {
				loader = new DDRInteractiveClassLoader(vmData);
			}
		} catch (DDRInteractiveCommandException e) { // may catch a DDRInteractiveCommandException when loading plugins 
			logger.log(Level.FINE, "Problem loading DDR plugins: ", e);
			System.err.println("Problem loading DDR plugins:");
			System.err.println(e.getMessage());
		}
		commands = getContextCommands();
	}
	
	@SuppressWarnings("unchecked")
	private List<ICommand> getContextCommands()
	{
		Object bootstrapArray[] = new Object[2];
		bootstrapArray[1] = loader;
		
		try {
			vmData.bootstrapRelative(TASK_GETCOMMANDS, (Object)bootstrapArray);
		} catch (ClassNotFoundException e) {
			//Shouldn't happen
			throw new Error(e);
		}
		
		return Collections.unmodifiableList((List<ICommand>) bootstrapArray[0]);
	}

	@Override
	public String toString()
	{
		long pid;
		try {
			pid = process.getProcessId();	
		} catch (CorruptDataException e1) {
			pid = -1;
		}
		if (this.process.getPlatform() == Platform.ZOS) {
			if(pid == -1) {
				return "ASID: " + Long.toHexString(this.process.getAddressSpace().getAddressSpaceId()) + " : No JRE";
			} 
			return "ASID: " + Long.toHexString(this.process.getAddressSpace().getAddressSpaceId()) + " EDB: " + Long.toHexString(pid) + " " + vmString();
		} else {
			String pidString = pid == -1 ? "<error>" : Long.toString(pid);
			return "PID: " + pidString + "; " + vmString();
		}
	}
	
	public String toString(boolean shortFormat)
	{
		StringBuilder data = new StringBuilder();
		long pid;
		try {
			pid = process.getProcessId();	
		} catch (CorruptDataException e1) {
			pid = -1;
		}
		if (this.process.getPlatform() == Platform.ZOS) {
			data.append("ASID: 0x" + Long.toHexString(this.process.getAddressSpace().getAddressSpaceId()));
			if(pid == -1) {
				data.append(" : No JRE");
			} else {
				data.append(" EDB: 0x" + Long.toHexString(pid));
				if(!shortFormat) {
					data.append(" " + vmString());
				}
			}
			return data.toString();
		} else {
			if(shortFormat) {
				String pidString = pid == -1 ? "<error>" : Long.toString(pid);
				return "PID: " + pidString;
			} else {
				String pidString = pid == -1 ? "<error>" : Long.toString(pid);
				return "PID: " + pidString + "; " + vmString();
			}
		}
	}
	
	private long getVMAddress()
	{
		long[] passBackArray = new long[1];
		try {
			vmData.bootstrapRelative(TASK_FINDVM,(Object)passBackArray);
		} catch (ClassNotFoundException e) {
			throw new Error(e);
		}
		return passBackArray[0];
	}
	
	private String vmString() {
		String hexAddress = "0x" + Long.toHexString(vmAddress);
		return " !j9javavm " + hexAddress;
	}
	
	/**
	 * Old way - use command and arguments as plain strings.
	 * @param command
	 * @param arguments
	 * @param out
	 */
	public void execute(String command, String[] arguments, PrintStream out) 
	{
		try {
			execute(new CommandParser(command, arguments), out);
		} catch (ParseException e) {
			out.println("Error executing command: " + e.getMessage());
		}
	}
	
	/**
	 * New way - use CommandParser for added smarts in handling commands (file redirection etc.)
	 * @param command
	 * @param defaultOut - the command may be redirecting output to file, but if it isn't, the defaultOut is used to print the results.
	 * Errors are always written to defaultOut
	 */
	public void execute(CommandParser command, PrintStream defaultOut) {
		for (ICommand thisCommand : nonVMCommands) {
			if (tryCommand(command, thisCommand, defaultOut)) {
				return;
			}
		}
		
		for (ICommand thisCommand : commands) {
			if (tryCommand(command, thisCommand, defaultOut)) {
				return;
			}
		}
		
		defaultOut.println("Unrecognised command: " + command);
	}
	
	private boolean tryCommand(CommandParser command, ICommand thisCommand, PrintStream defaultOut)
	{
		String cmd;
		
		if (command.getCommand().startsWith("!") == false) {
			cmd = "!" + command.getCommand();
		} else {
			cmd = command.getCommand();
		}
		
		if (thisCommand.recognises(cmd, this)) {
			PrintStream fileOut = null;
			try {
				if (command.isRedirectedToFile()) {
					fileOut = command.getOutputFile();
					thisCommand.run(cmd, command.getArguments(), this, fileOut);
					fileOut.flush();
					fileOut.close();
				} else {
					thisCommand.run(cmd, command.getArguments(), this, defaultOut);
				}
			} catch (Exception e) {
				logger.log(Level.FINE, "Problem running command: ", e);
				defaultOut.println("Problem running command:");
				defaultOut.println(e.getMessage());
			}

			defaultOut.flush();

			if (fileOut != null) {
				fileOut.flush();
				fileOut.close();
			}

			return true;
		} else {
			return false;
		}
	}
		
	public Vector<String> getCommandNames() 
	{
		Vector<String> commandNames = new Vector<String>();
		
		for (ICommand thisCommand : nonVMCommands) {
			if (thisCommand.getCommandNames() != null) {
				commandNames.addAll(thisCommand.getCommandNames());
			}
		}
		
		for (ICommand thisCommand : commands) {
			if (thisCommand.getCommandNames() != null) {
				commandNames.addAll(thisCommand.getCommandNames());
			}
		}
		
		Map<String, StructureDescriptor> structureMap = StructureCommandUtil.getStructureMap(this);
		Iterator<String> keys = structureMap.keySet().iterator();
    	while(keys.hasNext()) {
    		String command = (String) keys.next();
    		commandNames.add(command);
    	}
		
		return commandNames;
	}

	
}
