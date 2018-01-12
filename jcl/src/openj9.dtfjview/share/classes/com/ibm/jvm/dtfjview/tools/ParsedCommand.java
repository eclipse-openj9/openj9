/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2012, 2017 IBM Corp. and others
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

import java.util.ArrayList;

/**
 * This class divides the command line into the command with arguments.
 * If pipeline is found in the command line, it will re-organize it so that
 * the command before the pipe line will be placed after the command behind the
 * pipe line.
 * <p>
 * @author Manqing Li, IBM.
 *
 */
public class ParsedCommand 
{
	/**
	 * This is a convenient method to parse a command line.
	 * <p>
	 * @param commandLine	The command line.  Can contain pipe lines.
	 * <p>
	 * @return	An initialized ParsedCommand object.
	 */
	public static ParsedCommand parse(String commandLine) {
		String components[] = separateAndReorganizeTokens(commandLine);

		String command = components[0];
		String arguments[] = new String[components.length - 1];

		for (int i = 1; i < components.length; i++) {
			arguments[i - 1] = components[i];
		}
		return new ParsedCommand(command, arguments);
	}
	/**
	 * To construct an object.
	 * <p>
	 * @param command		The command without arguments.
	 * @param arguments		The arguments to the command.
	 */
	public ParsedCommand(String command, String [] arguments) {
		setCommand(command);
		setArguments(arguments);
	}
	/**
	 * To get the command without arguments.
	 * <p>
	 * @return	the command without arguments.
	 */
	public String getCommand() {
		return command;
	}
	/**
	 * To set the command.
	 * <p>
	 * @param command	The command to be set.
	 */
	public void setCommand(String command) {
		this.command = command;
	}
	/**
	 * To get the arguments.
	 * <p>
	 * @return	the arguments.
	 */
	public String[] getArguments() {
		return arguments;
	}
	/**
	 * To set the arguments.
	 * <p>
	 * @param arguments	The arguments to be set.
	 */
	public void setArguments(String[] arguments) {
		this.arguments = arguments;
	}
	/**
	 * To get the command line combined from its command and arguments,
	 * using spaces to separate tokens.
	 * <p>
	 * @return	the combined command line.
	 */
	public String getCombinedCommandLine() {
		StringBuilder sb = new StringBuilder(getCommand());
		for (String argument : arguments) {
			if (0 <= argument.indexOf(" ") || 0 <= argument.indexOf("\t")) {
				if (0 <= argument.indexOf("\"")) {
					argument = "'" + argument + "'";
				} else {
					argument = "\"" + argument + "\"";
				}
			}
			sb.append(" ").append(argument);
		}
		return sb.toString();
	}
	
	private static String [] separateAndReorganizeTokens(final String line) {
		ArrayList<String> alTokens = new ArrayList<String>();
		StringBuffer sb = new StringBuffer();
		int i = 0;
		int length = line.length();
		while (i < length) {
			char c = line.charAt(i++);
			if ('"' == c || '\'' == c) {
				// if the next char matches the quotation, we have an empty String here.  
				// We need add the empty String back.
				if(i < line.length() && c == line.charAt(i)) {
					sb.append(c).append(c);
					i++;
					continue;
				}
				
				char quotation = c;
				while(i < line.length()) {
					c = line.charAt(i++);
					if(c == quotation) {
						break;
					}
					sb.append(c);
				}
			} else if (" \t\n\r\f".indexOf(c) >= 0) {
				if(sb.length() > 0) {
					alTokens.add(sb.toString());
					sb = new StringBuffer();
				}
			} else if ( '|' == c) {
				if(sb.length() > 0) {
					alTokens.add(sb.toString());					
					sb = new StringBuffer();
				}
				alTokens.add("|");
			} else if (i < length && '|' == line.charAt(i)) {
				sb.append(c);
				alTokens.add(sb.toString());
				sb = new StringBuffer();
				alTokens.add("|");
				i++;
			} else {
				sb.append(c);
			}
		}
		if (sb.length() > 0) {
			alTokens.add(sb.toString());
		}
		ArrayList<String> formatted = reorganize(alTokens);
		return (String []) formatted.toArray(new String [formatted.size()]);
	}
	private static ArrayList<String> reorganize(final ArrayList<String> al) {
		return reorganize(al, 0, false);
	}
	private static ArrayList<String> reorganize(final ArrayList<String> al, int startIndex, boolean piped) {
		ArrayList<String> alNew = new ArrayList<String>();
		for (int i = startIndex; i < al.size(); i++) {
			String token = al.get(i);
			if (token.equals("|") || token.equals(">") || token.equals(">>")) {
				ArrayList<String> pipe = reorganize(al, i + 1, token.equals("|"));
				piped = false;
				for (int x = pipe.size() - 1; x >= 0; x--) {
					alNew.add(0, pipe.get(x));
				}
				if (token.equals("|") == false) {
					alNew.add(0, token);
				}
				break;
			}
			
			if (piped) {
				if (ToolsRegistry.isPipeLineEnabled(token, null) == false) {
					alNew.add("run");
					if (token.equalsIgnoreCase("-head")) {
						// do nothing.
					} else if (token.equalsIgnoreCase("-tail")) {
						// do nothing.
					} else {
						alNew.add("-head"); // now, token has to be the normal command itself.
					}
				}
				piped = false;
			}
			
			alNew.add(token);
		}
		return alNew;
	}
	
	private String command;
	private String [] arguments;
}
