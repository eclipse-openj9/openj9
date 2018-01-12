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
import com.ibm.jvm.dtfjview.tools.utils.BlockPostmatchHandle;
import com.ibm.jvm.dtfjview.tools.utils.BlockPrematchHandle;
import com.ibm.jvm.dtfjview.tools.utils.IMatchHandle;
import com.ibm.jvm.dtfjview.tools.utils.IPostmatchHandle;
import com.ibm.jvm.dtfjview.tools.utils.IPrematchHandle;
import com.ibm.jvm.dtfjview.tools.utils.MatchHandle;
import com.ibm.jvm.dtfjview.tools.utils.MaxLinesPostmatchHandle;
import com.ibm.jvm.dtfjview.tools.utils.MaxLinesPrematchHandle;
import com.ibm.jvm.dtfjview.tools.utils.OutputStreamModifier;
import com.ibm.jvm.dtfjview.tools.utils.RegExprMatchHandle;
import com.ibm.jvm.dtfjview.tools.utils.StringModifier;

/**
 * @author Manqing Li, IBM
 *
 */
public class GrepTool extends Tool implements IPipe {
	public static final String COMMAND = "grep";
	public static final String COMMAND_NEGATE = "grep-";
	public static final String ARGUMENT_DESCRIPTION = "[options] <pattern>";
	public static final String HELP_DESCRIPTION = "to be used after the pipeline to show lines which match a pattern.";

	public static final String USAGE = COMMAND + "\t" + ARGUMENT_DESCRIPTION + "\t" + HELP_DESCRIPTION + "\n" + 
			"     Options :\n" +
			"          -i : Ignore case.\n" +
			"          -r, -G, --regex : Use regular expression as defined in the Java documentation of class java.utils.regex.Pattern.\n" +
			"          -b, --block : show the block of lines when at least one of the lines matches the pattern.  Block of lines are separated by empty lines.\n" +
			"          -A <NUM>, +<NUM> : Show at most <NUM> lines after the matching line.\n" +
			"          -B <NUM>, -<NUM> : show at most <NUM> lines after the matching line.\n" +
			"          -C <NUM>, +-<NUM> : show at most <NUM> lines before and after the matching line.\n" +
			"          -v, --invert-match : to be used with command grep to show the lines which does not match the pattern, equivalent to using command grep-.\n" +
			"          -F, --fixed-strings : to treat character '*' not as a wide card. This option can not be used together with -r, -G or --regex.\n" +
			"     Pattern : \n" +
			"           Character '*' in a pattern will be treated as a wild card unless option -F or --fixed-strings is used.\n" +
	        "           If a pattern contains any spaces, enclose the pattern in a pair of double quotation marks.\n" +
	        "           If a pattern contains any double quotation marks, enclose the pattern in a pair of single quotation marks.\n" +
	        "           The pattern can be in the following format to show lines which match any of the sub-patterns:\n" +
	        "                    \"[<pattern1>|<pattern2>|...|<patternN>]\"\n" +
	        "               This format has to start with \"[ and ends with ]\" and use character '|' as the sub-pattern separator.\n" + 
	        "               No quotation marks and character '|' are allowed in any sub-patterns.\n" + 
	        "               Spaces are allowed in the middle of a sub-patterns, but leading and trailing spaces will be trimmed.\n" +
	        "               This format only works when option -r, -G, and --regex are not used.\n" +
	        "     Use command " + COMMAND_NEGATE + " to show lines which do not match the pattern.";

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
		return command.equalsIgnoreCase(COMMAND) || command.equalsIgnoreCase(COMMAND_NEGATE);
	}
	
	/**
	 * Processes the command.
	 * <p>
	 * @param command	The command to be processed.
	 * @param args		The arguments taken by the command.
	 * @param out		The output channel.
	 * <p>
	 * @throws CommandException
	 */
	public void process(String command, String[] args, PrintStream out) throws CommandException 
	{	
		Attributes attributes = null;
		try {
			attributes = readAttributes(command, args, command.equalsIgnoreCase(COMMAND_NEGATE), out);
		} catch (Exception e) {
			e.printStackTrace();
		}
		if(attributes == null) {
			out.println(USAGE);
			return;
		}
		
		IPrematchHandle prematchHandle = null;
		if (attributes.matchBlock) {
			prematchHandle = new BlockPrematchHandle();
		} else {
			prematchHandle = new MaxLinesPrematchHandle(attributes.maxPreviousLines);
		}

		IMatchHandle matchHandle = null;
		if (attributes.useRegularExpression) {
			matchHandle = new RegExprMatchHandle(attributes.normalStringsToGrep, attributes.caseInsensitive, attributes.negate);
		} else {
			matchHandle = new MatchHandle(attributes.normalStringsToGrep, attributes.caseInsensitive, attributes.negate, attributes.isFixedString);
		}
		
		IPostmatchHandle postmatchHandle = null;
		if (attributes.matchBlock) {
			postmatchHandle = new BlockPostmatchHandle();
		} else {
			postmatchHandle = new MaxLinesPostmatchHandle(attributes.maxNextLines);
		}
		
		ToolsRegistry.process(attributes.nextCommand, attributes.nextCommandArgs, 
				new PrintStream(
						new OutputStreamModifier(out, 
								new StringModifier(prematchHandle, matchHandle, postmatchHandle))));
		
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
	
	private Attributes readAttributes(String command, String [] args, boolean negate, PrintStream out) {
		if (2 > args.length) {
			return null;
		}
		boolean useRegularExpression = false;
		boolean caseInsensitive = false;
		String stringToGrep = null;
		String nextCommand = null;
		ArrayList<String> nextCommandArgs = new ArrayList<String>();
		int maxPreviousLines = 0;
		int maxNextLines = 0;
		boolean matchBlock = false;
		boolean dashVUsed = false;
		boolean isFixedString = false;

		boolean grepOptionsEnded = false;
		for (int i = 0; i < args.length; i++) {
			if (false == grepOptionsEnded && args[i].equals("-i")) {
				caseInsensitive = true;
				continue;
			} 
			if (false == grepOptionsEnded && (args[i].equals("-b") || args[i].equals("--block"))) {
				matchBlock = true;
				continue;
			} 
			if (false == grepOptionsEnded && (args[i].equals("-r") || args[i].equals("-G") || args[i].equals("--regex"))) {
				useRegularExpression = true;
				continue;
			} 
			if (false == grepOptionsEnded && (args[i].equals("-F") || args[i].equals("--fixed-strings"))) {
				isFixedString = true;
				continue;
			}
			if (false == grepOptionsEnded && args[i].equals("-C")) {
				if (i < args.length - 1) {
					i++;
					try {
						maxPreviousLines = Integer.parseInt(args[i]);
						maxNextLines = maxPreviousLines;
						continue;
					} catch (NumberFormatException e) {
						out.println("Option -C is not followed by a number; instead it is followed by token " + args[i]);
						return null;
					}
				}
				out.println("Option -C is not followed by a number.");
				return null;
			}
			if (false == grepOptionsEnded && (args[i].startsWith("-+") || args[i].startsWith("+-"))) {
				try {
					maxPreviousLines = Integer.parseInt(args[i].substring(2));
					maxNextLines = maxPreviousLines;
					continue;
				} catch (NumberFormatException e) {
					// This means the search string starts with "-+" or "+-" !
				}
			}
			if (false == grepOptionsEnded && args[i].equals("-A")) {
				if (i < args.length - 1) {
					i++;
					try {
						maxNextLines = Integer.parseInt(args[i]);
						continue;
					} catch (NumberFormatException e) {
						out.println("Option -A is not followed by a number; instead it is followed by token " + args[i]);
						return null;
					}
				}
				out.println("Option -A is not followed by a number.");
				return null;
			}
			if (false == grepOptionsEnded && args[i].startsWith("+")) {

				try {
					maxNextLines = Integer.parseInt(args[i].substring(1));
					continue;
				} catch (NumberFormatException e) {
					// This means the search string starts with "+" !
				}
			}
			
			if (false == grepOptionsEnded && (args[i].equals("-v") || args[i].equalsIgnoreCase("--invert-match"))) {
				dashVUsed = true;
				continue;
			}
			
			if (false == grepOptionsEnded && args[i].equals("-B")) {
				if (i < args.length - 1) {
					i++;
					try {
						maxPreviousLines = Integer.parseInt(args[i]);
						continue;
					} catch (NumberFormatException e) {
						out.println("Option -B is not followed by a number; instead it is followed by token " + args[i]);
						return null;
					}
				}
				out.println("Option -B is not followed by a number.");
				return null;
			}
			
			if (false == grepOptionsEnded && args[i].startsWith("-")) {
				try {
					maxPreviousLines = Integer.parseInt(args[i].substring(1));
					continue;
				} catch (NumberFormatException e) {
					// This means the search string starts with "-" !
				}
			}
			
			grepOptionsEnded = true;
			if (stringToGrep == null) {
				stringToGrep = args[i];
			} else if (nextCommand == null){
				nextCommand = args[i];
			} else {
				nextCommandArgs.add(args[i]);
			}
		}
		if (null == stringToGrep || null == nextCommand) {
			out.println("The grep/grep- command can only be used after the pipeline.");
			return null;
		}
		if (dashVUsed) {
			if (command.equalsIgnoreCase(COMMAND_NEGATE)) {
				out.println("Option -v can not be used with command grep-.");
				return null;
			} else if (false == negate){
				negate = true;
			}
		}
		if (isFixedString && useRegularExpression) {
			out.println("Option -F and --fixed-strings can not be used with option -r, -G, or --regex.");
			return null;
		}
		return new Attributes(negate, useRegularExpression, 
				caseInsensitive, stringToGrep, nextCommand, nextCommandArgs.toArray(new String [nextCommandArgs.size()]),
				maxPreviousLines, maxNextLines, matchBlock, isFixedString);
	}
	
	private class Attributes 
	{
		public Attributes(boolean negate, boolean useRegularExpression, boolean caseInsensitive, 
				String stringToGrep, String nextCommand, String [] nextCommandArgs, 
				int maxPreviousLines, int maxNextLines, boolean matchBlock, boolean isFixedString) 
		{
			this.negate = negate;
			this.useRegularExpression = useRegularExpression;
			this.caseInsensitive = caseInsensitive;
			this.nextCommand = nextCommand;
			this.nextCommandArgs = nextCommandArgs;
			this.maxPreviousLines = maxPreviousLines;
			this.maxNextLines = maxNextLines;
			this.matchBlock = matchBlock;
			this.isFixedString = isFixedString;
			this.normalStringsToGrep = extractNormalStringsToGrep(stringToGrep);
		}
		private String [] extractNormalStringsToGrep(String s) {
			if (this.useRegularExpression || false == s.startsWith("[") || false == s.endsWith("]")) {
				return new String [] {s};
			}
			List<String> ls = new ArrayList<String>();
			StringTokenizer st = new StringTokenizer(s.substring(1, s.length() - 1), "|");
			while (st.hasMoreTokens()) {
				String t = st.nextToken();
				if (t.trim().length() > 0) {
					ls.add(t.trim());
				}
			}
			return (String []) ls.toArray(new String [ls.size()]);
		}
		public boolean negate = false;
		public boolean useRegularExpression = false;
		public boolean caseInsensitive = false;
		public String nextCommand = null;
		public  String [] nextCommandArgs = null;
		public int maxPreviousLines = 0;
		public int maxNextLines = 0;
		public boolean matchBlock = false;
		public boolean isFixedString = false;
		public String [] normalStringsToGrep = null;
	}
}
