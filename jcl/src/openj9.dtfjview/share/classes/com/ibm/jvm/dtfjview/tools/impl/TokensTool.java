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
package com.ibm.jvm.dtfjview.tools.impl;

import java.io.PrintStream;
import java.util.ArrayList;
import java.util.List;
import java.util.StringTokenizer;

import com.ibm.jvm.dtfjview.tools.CommandException;
import com.ibm.jvm.dtfjview.tools.IPipe;
import com.ibm.jvm.dtfjview.tools.Tool;
import com.ibm.jvm.dtfjview.tools.ToolsRegistry;
import com.ibm.jvm.dtfjview.tools.utils.IStringModifier;
import com.ibm.jvm.dtfjview.tools.utils.OutputStreamModifier;

public class TokensTool extends Tool implements IPipe {

	private static final String COMMAND = "tokens";
	private static final String RANGE_INDICATOR = "..";
	private static final String OPTION_KEEP = "-keep";
	private static final String ARGUMENT_DESCRIPTION = "[" + OPTION_KEEP + "]" + " <range>[,<range>...]";
	private static final String HELP_DESCRIPTION = "To be used after a pipeline to pick up tokens in a line.";
	private static final String USAGE = COMMAND + "\t" + ARGUMENT_DESCRIPTION + "\t" + HELP_DESCRIPTION + "\n" +
			"     A range can be defined in the following formats:\n" +
			"          X\n" +
			"          X..Y\n" +
			"     In the latter format, if X is missing, it is assumed to be 1 (representing the first token).\n" +
			"     If Y is missing, it is assumed to be -1 (representing the last token).\n" +
			"     X or Y can be preceded by a minus sign (-), which implies counting from the right.\n" +
			"     X has to represent a position (that can be) the same or to the left of what Y represents.\n" +
			"     \n" +
			"     A range is present if it includes at least one token.\n" +
			"     The output consists of the selected tokens, in the order specified; a single space separates output tokens.\n" +
			"     If all the ranges are missing from the line, the line is suppressed unless the -keep option is used.";

	/**
	 * Processes the command.
	 * <p>
	 * @param command	The command to be processed.
	 * @param args		The arguments taken by the command.
	 * @param out		The output channel.
	 * <p>
	 * @throws CommandException
	 */
	public void process(String command, String[] args, PrintStream out) throws CommandException {
		Arguments arguments = TokensTool.processArguments(args, out);
		if(arguments == null) {
			out.println(USAGE);
			return;
		}
		PrintStream newOut = null;
		try {
			newOut = new PrintStream(new OutputStreamModifier(out, new StringModifier(arguments)));
			ToolsRegistry.process(arguments.nextCommand, arguments.nextCommandArgs,  newOut);
		} finally {
			if(newOut != null) {
				newOut.close();
			}
		}
	}

	/**
	 * Determines if a command is accepted by the current tool.
	 * <p>
	 * @param command	The command
	 * @param args		The arguments taken by the command.
	 * <p>
	 * @return		<code>true</code> if this is the correct tool for this command; 
	 * 				<code>false</code> otherwise.
	 */
	public boolean accept(String command, String[] args) {
		return COMMAND.equalsIgnoreCase(command);
	}

	/**
	 * To print the detailed help message.
	 */
	public void printDetailedHelp(PrintStream out) {
		out.println(USAGE);
	}

	/**
	 * To gets the tool's command name.
	 * <p>
	 * @return	The tool's command name.
	 */
	public String getCommandName() {
		return COMMAND;
	}

	/**
	 * To gets the tool's argument description.
	 * <p>
	 * @return	The tool's argument description.
	 */
	public String getArgumentDescription() {
		return ARGUMENT_DESCRIPTION;
	}

	/**
	 * To gets the tool's help description.
	 * <p>
	 * @return	The tool's help description.
	 */
	public String getHelpDescription() {
		return HELP_DESCRIPTION;
	}

	/**
	 * To parse the arguments.
	 * Note, if a null object is returned, it indicates some errors have encountered.
	 * <p>
	 */
	private static Arguments processArguments(String[] args, PrintStream out) {
		if(args.length < 2) {
			return null;
		}
		int currentIndex = 0;
		boolean keep = false;
		List<Range> rangeList = new ArrayList<TokensTool.Range>();

		if (args[currentIndex].equalsIgnoreCase(OPTION_KEEP)) {
			keep = true;
			currentIndex++;
		}
		
		StringTokenizer st = new StringTokenizer(args[currentIndex], ",");
		while(st.hasMoreTokens()) {
			Range range = TokensTool.parseRange(st.nextToken(), out);
			if (range == null) {
				return null; // means some range is not specified properly.
			} else {
				rangeList.add(range);
			}
		}
		currentIndex++;
		
		if (args[currentIndex].equalsIgnoreCase(OPTION_KEEP)) {
			keep = true;
			currentIndex++;
		}

		String ddrCommand = args[currentIndex];
		int startIndexForNextCommand = currentIndex + 1;
		String [] ddrCommandArgs = new String[args.length - startIndexForNextCommand ];
		for(int i = 0; i < ddrCommandArgs.length; i++) {
			ddrCommandArgs[i] = args[i + startIndexForNextCommand];
		}
		return new Arguments(keep, rangeList, ddrCommand, ddrCommandArgs);
	}
	
	/**
	 * To parse a range.
	 * Note, a NULL object will be returned if the specified range is found to have any errors.
	 */
	private static Range parseRange(String range, PrintStream out) {
		int start = 1;
		int end = -1;
		
		int index = range.indexOf(RANGE_INDICATOR);
		try {
			if (index < 0) {
				start = Integer.parseInt(range);
				end = start;
			} else {
				String x = range.substring(0, index);
				if (x.length() > 0) {
					start = Integer.parseInt(x);
				}
				String y = range.substring(index + RANGE_INDICATOR.length());
				if (y.length() > 0) {
					end = Integer.parseInt(y);
				}
			}
		} catch (NumberFormatException e) {
			out.println("Range must be numeric : " + range);
			return null;
		}
		
		// do some basic verification here.
		// (1) start and end of the range can not be 0 because we start counting from 1 or -1.
		// (2) if start and end are both positive or both negative, we need ensure start is not greater than end;
		// (3) if one of them is positive but the other one is negative, we are not sure which one is on the left position
		//     until we know how many tokens a line has.  We do not check here.  This checking is deferred until we have
		//     the line to be processed.
		if (start == 0 || end == 0 || (start * end > 0 && start > end)) {
			out.println("Invalid range : " + range);
			return null;
		}
		return new Range (start, end);
	}
	
	private static final class Range {
		final int start;
		final int end;
		
		public Range(int start, int end) {
			this.start = start;
			this.end = end;
		}
		public int getStartIndex(int tokenSize) {
			return Math.max(0, start < 0 ? tokenSize + start : start - 1);
		}
		
		public int getEndIndex(int tokenSize) {
			return Math.min(tokenSize - 1, end < 0 ? tokenSize + end : end - 1);
		}
	}
	
	private static final class Arguments {
		public Arguments(boolean keep, List<Range> rangeList, String nextCommand, String [] nextCommandArgs) {
			this.keep = keep;
			this.rangeList = rangeList;
			this.nextCommand = nextCommand;
			this.nextCommandArgs = nextCommandArgs;
		}
		public final String nextCommand;
		public final String [] nextCommandArgs;
		public final boolean keep;
		public final List<Range> rangeList; 
	}
	
	private static final class StringModifier implements IStringModifier {
		public StringModifier(Arguments arguments) {
			this.arguments = arguments;
		}
		
		public String modify(String s) {
			StringBuilder sb = null;
			List<String> tokenList = tokenize(s);
			int tokenSize = tokenList.size();
			for (Range range : arguments.rangeList) {
				int endIndex = range.getEndIndex(tokenSize);
				for (int index = range.getStartIndex(tokenSize); index <= endIndex; index++) {
					if (sb == null) {
						sb = new StringBuilder();
					} else {
						sb.append(" ");
					}
					sb.append(tokenList.get(index));
				}
			}
			
			if(sb == null || sb.length() == 0) {
				if (arguments.keep) {
					return s.endsWith("\n") ? s : s + "\n";
				} else {
					return "";
				}
			}
			return sb.toString() + "\n";
		}
		
		private static List<String> tokenize(String s) {
			List<String> tokenList = new ArrayList<String>();
			StringTokenizer st = new StringTokenizer(s);
			while (st.hasMoreTokens()) {
				tokenList.add(st.nextToken());
			}
			return tokenList;
		}
		
		private final Arguments arguments;
	}
}
