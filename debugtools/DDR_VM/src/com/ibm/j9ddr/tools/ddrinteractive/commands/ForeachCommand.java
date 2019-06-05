/*******************************************************************************
 * Copyright (c) 2014, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.tools.ddrinteractive.commands;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Arrays;

import com.ibm.j9ddr.command.CommandParser;
import com.ibm.j9ddr.tools.ddrinteractive.Command;
import com.ibm.j9ddr.tools.ddrinteractive.Context;
import com.ibm.j9ddr.tools.ddrinteractive.DDRInteractiveCommandException;

public class ForeachCommand extends Command 
{
	public static String[] foreachArgs = null;
	
	public ForeachCommand()
	{
		addCommand("foreach", "", "build up a list of elements to operate on ");
		addCommand("do", "<command>", "repeat a command for each element in the list");
	}

	public void run(String command, final String[] args, final Context context, final PrintStream out) throws DDRInteractiveCommandException 
	{
		if (command.equals("!do")) {
			boolean echo = false;
			boolean quiet = false;
			String token = null;
			int commandIndex = 0;
			
			
			while (commandIndex < args.length) {
				if (args[commandIndex].equals("help")) {
					out.println("The !do command is used to repeat a command on the elements gathered by the !foreach command.");
					out.println("Syntax:	!do [echo] [quiet] [token=<token>] <cmd> [args]");
					out.println();
					out.println("\techo\t\tOutput each command before it is run.");
					out.println("\tquiet\t\tSuppress delimiters between command outputs.");
					out.println("\ttoken=<chars>\tSpecify a substitution token. Occurrencs of this string in the args will be replaced by the current element.");
					return;
				} else if (args[commandIndex].equals("echo")) {
					echo = true;
					commandIndex += 1;
				} else if (args[commandIndex].equals("quiet")) {
					quiet = true;
					commandIndex += 1;
				} else if (args[commandIndex].startsWith("token=")) {
					token = args[commandIndex].substring("token=".length());
					commandIndex += 1;
				} else {
					break;
				}
			}
			if (commandIndex >= args.length) {
				out.println("The do command requires another command to repeat.");
				return;
			}
			
			if ((null == foreachArgs) || (0 == foreachArgs.length)) {
				out.println("Element list is empty. Use the foreach command to populate it.");
				return;
			}

			for (int i = 0; i < foreachArgs.length; i++) {
				if (!quiet && (i > 0)) {
					out.println("========================================");
				}
				try {
					String[] newArgs;
					if (null == token) {
						newArgs = substituteArgs(args, commandIndex, i);
					} else {
						newArgs = substituteArgs(args, commandIndex, i, token);
					}
					if (echo) {
						System.out.println("> " + args[commandIndex] + " " + Arrays.toString(newArgs));
					}
					CommandParser commandParser = new CommandParser(args[commandIndex], newArgs);
					context.execute(commandParser, out);
				} catch (ParseException e) {
					e.printStackTrace(out);
				} catch (Throwable th) {
					out.println("Exception while executing " + args[commandIndex] + " " + foreachArgs[i]);
					th.printStackTrace(out);
				}
			}
			
		} else if(command.equals("!foreach")) {
			if (args.length > 0) {
				out.println("The !foreach command takes no arguments, but will read lines from the console until it encounters a blank line.");
				out.println("These lines can then be used as arguments to commands specified using !do.");
				return;
			}
			ArrayList<String> lines = new ArrayList<String>();
			BufferedReader reader = new BufferedReader(new InputStreamReader(System.in));
			while (true) {
				String line;
				try {
					line = reader.readLine();
				} catch (IOException e) {
					break;
				}
				if (0 == line.length()) {
					break;
				} else {
					lines.add(line);
				}
			}
			
			// split by ,
			String[] newArgs = new String[lines.size()];
			lines.toArray(newArgs);
			foreachArgs = newArgs;
		}
	}

	private String[] substituteArgs(String[] args, int commandIndex, int foreachIndex) 
	{
		/* Concatenate:
		 * 		<command> <command_args> <foreach_args>
		 */
		String[] newArgs = new String[args.length - commandIndex];
		System.arraycopy(args, commandIndex + 1, newArgs, 0, args.length - commandIndex - 1);
		newArgs[newArgs.length - 1] = foreachArgs[foreachIndex];
		return newArgs;
	}
	
	private String[] substituteArgs(String[] args, int commandIndex, int foreachIndex, String token) 
	{
		/* Concatenate:
		 * 		<command> <command_args>
		 * Replacing any occurrence of token with <foreach_args>
		 */
		String[] newArgs = new String[args.length - commandIndex - 1];
		System.arraycopy(args, commandIndex + 1, newArgs, 0, args.length - commandIndex - 1);
		for (int i = 0; i < newArgs.length; i++) {
			newArgs[i] = newArgs[i].replace(token, foreachArgs[foreachIndex]);
		}
		return newArgs;
	}
}
