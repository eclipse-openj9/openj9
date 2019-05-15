/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2019 IBM Corp. and others
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
package com.ibm.jvm.dtfjview.tools;

import java.io.PrintStream;
import java.io.UnsupportedEncodingException;
import java.util.ArrayList;
import java.util.List;

import com.ibm.jvm.dtfjview.Session;
import com.ibm.jvm.dtfjview.tools.impl.CharsFromTool;
import com.ibm.jvm.dtfjview.tools.impl.CharsToTool;
import com.ibm.jvm.dtfjview.tools.impl.CmdFileTool;
import com.ibm.jvm.dtfjview.tools.impl.GrepTool;
import com.ibm.jvm.dtfjview.tools.impl.HelpTool;
import com.ibm.jvm.dtfjview.tools.impl.HistoryTool;
import com.ibm.jvm.dtfjview.tools.impl.OutFileTool;
import com.ibm.jvm.dtfjview.tools.impl.TokensTool;
import com.ibm.jvm.dtfjview.tools.utils.StringReceiver;

/**
 * This class contains some static methods to invoke registered commands.
 * <p>
 * @author Manqing Li, IBM Canada
 *
 */
public class ToolsRegistry {
	public static void initialize(Session session) {
		
		ToolsRegistryOutputChannels.initialize(session.getCharset());
		
		if (null == registry) {
			registry = new ToolsRegistry(session);
			registry.initializeTools();
		}
	}
	
	/**
	 * To record the command in the history and then execute it.
	 * This method is called only right after the command is typed in from the console.
	 * No tools should call this method directly.
	 * <p>
	 * @param command	The command to be executed.
	 * <p>
	 * @return		The result.
	 * <p>
	 * @throws CommandException
	 */
	public static void recordAndExecute(String command) throws CommandException
	{
		if (null != command && 0 < command.trim().length()) {
			try {
				execute(command);
			} finally {
				ToolsRegistry.registry.history.record(command);
			}
		}
	}

	/**
	 * To execute a command.
	 * Note: Default print stream will be used.
	 * <p>
	 * @param command	The command to be executed.
	 */
	public static void execute(String command) throws CommandException
	{
		ParsedCommand parsedCommand = ParsedCommand.parse(command);
		execute(parsedCommand.getCommand(), parsedCommand.getArguments());
	}
	
	/**
	 * To execute a command with its arguments.
	 * Note: Default print stream will be used.
	 * <p>
	 * @param command	The command itself.
	 * @param args		The arguments to the command.
	 */
	public static void execute(String command, String [] args) throws CommandException
	{
		PrintStream out = ToolsRegistryOutputChannels.newPrintStream();
		for (ITool tool : registry.toolList) {
			if (tool.accept(command, args)) {
				tool.process(command, args, out);
				return;
			}
		}
		executeJdmpviewCommand(new ParsedCommand(command, args).getCombinedCommandLine(), out);
	}
	
	/**
	 * To execute a command.
	 * Note:  this method should not be called directly by any DDR/Jdmpview commands;
	 *        DDR/Jdmpview commands can use the one without the PrintStream parameter.
	 * <p>
	 * @param command	The command to be executed.
	 * @param out		The PrintStream
	 * <p>
	 */
	public static void process(String command, PrintStream out) throws CommandException
	{
		ParsedCommand parsedCommand = ParsedCommand.parse(command);
		process(parsedCommand.getCommand(), parsedCommand.getArguments(), out);
	}
	
	/**
	 * To execute a command.
	 * <p>
	 * @param command	The command to be executed.
	 * <p>
	 */
	public static String process(String command) throws CommandException
	{
		ParsedCommand parsedCommand = ParsedCommand.parse(command);
		return process(parsedCommand.getCommand(), parsedCommand.getArguments());
	}
	
	/**
	 * To execute a command.
	 * Note:  this method should not be called directly by any DDR/Jdmpview commands;
	 *        DDR/Jdmpview commands can use the one without the PrintStream parameter.
	 * <p>
	 * @param command	The command to be executed.
	 * @param args		The arguments of the command.
	 * @param out		The PrintStream
	 * <p>
	 */
	public static void process(String command, String [] args, PrintStream out) throws CommandException
	{
		for (ITool tool : registry.toolList) {
			if (tool.accept(command, args)) {
				tool.process(command, args, out);
				return;
			}
		}
		executeJdmpviewCommand(new ParsedCommand(command, args).getCombinedCommandLine(), out);
	}
	
	/**
	 * To execute a command.
	 * <p>
	 * @param command	The command to be executed.
	 * @param args		The arguments of the command.
	 * <p>
	 */
	public static String process(String command, String [] args) throws CommandException
	{
		StringReceiver receiver = new StringReceiver(registry.charsetName);
		process(command, args, new PrintStream(receiver));
		try {
			return receiver.release();
		} catch (UnsupportedEncodingException e) {
			throw new CommandException(e);
		}
	}
	
	/**
	 * To execute a Jdmpview command.
	 * <p>
	 * @param jdmpviewCommand	The Jdmpview command.
	 * @param out	The PrintStream.
	 */
	public static void executeJdmpviewCommand(String jdmpviewCommand, PrintStream out) {
		registry.session.execute(jdmpviewCommand, out);
	}
	
	/**
	 * To check if a command is pipeline enabled.
	 * <p>
	 * @param command	The command to be checked.
	 * @param args		The arguments to the command.
	 * <p>
	 * @return		<code>true</code> if the command is pipeline enabled; <code>false</code> otherwise.
	 */
	public static boolean isPipeLineEnabled(String command, String [] args)
	{
		for (ITool tool : registry.toolList) {
			if (tool.accept(command, args) && tool instanceof IPipe) {
				return true;
			}
		}
		return false;
	}
	
	/**
	 * To register a tool.
	 * <p>
	 * @param tool	The tool to be added to the Store.
	 */
	public static void registerTool(ITool tool) {
		registry.toolList.add(tool);
	}
	
	/**
	 * To get all the registered tools.
	 * <p>
	 * @return	The list of registered tools.
	 */
	public static List<ITool> getAllTools() {
		return registry.toolList;
	}
	
	/**
	 * To initialize the Store object.
	 */
	private ToolsRegistry(Session session) {
		this.session = session;
		this.toolList = new ArrayList<ITool>();
		this.charsetName = session.getCharset();
	}
	
	/**
	 * This method is to instantiate all the tools (except the Jdmpview/DDR tool).
	 * Note: 
	 * (1) Individual tools have to call ToolsRegistry.registerTool() to register themselves
	 * to the ToolsRegistry.
	 * (2) If a tool such as the Grep tool extends com.ibm.jvm.dtfjview.tools.Tool, the tool
	 * will be automatically registered to ToolsRegistry because the default constructor of
	 * com.ibm.jvm.dtfjview.tools.Tool calls ToolsRegistry.registerTool().
	 */
	private void initializeTools() {
		new HelpTool();
		history = new HistoryTool();
		new GrepTool();
		new CharsFromTool();
		new CharsToTool();
		new OutFileTool();
		new CmdFileTool(this.charsetName);
		new TokensTool();
	}
	
	private List<ITool> toolList;
	private HistoryTool history;
	private Session session;
	private String charsetName;
	private static ToolsRegistry registry = null;
}
