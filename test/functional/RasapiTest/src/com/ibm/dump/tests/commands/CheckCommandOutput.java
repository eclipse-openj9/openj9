/*******************************************************************************
 * Copyright (c) 2016, 2021 IBM Corp. and others
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
package com.ibm.dump.tests.commands;

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * This is the main class to validate the command output from Jdmpview.
 * Usage:
 * 			java com.ibm.dump.tests.commands.CheckCommandOutput <CommandOutputFile> <CommandCheckingFile>
 * <p>
 * The <CommandOutputFile> is what we defined as the -outfile option when launching jdmpview. It is the result
 * of executing commands in a command input file (defined as the -cmdfile option when launching Jdmpview).
 * <p>
 * The <CommandCheckingFile> defines what to check in the <CommandOutputFile>.  
 * Note:
 * 		(1) The <CommandCheckingFile> contains a list of command checking units. 
 * 			Each command checking unit contains 1 or more lines, the first line has to be the command line, followed
 * 			by the checker lines. A command line always starts with "command :"; a checker line always starts "checker :".
 * 		(2) The command checking unit has to be composed in the exact order as the commands in the command input file.
 * 		(3) You can use "#" or "//" to start a comment line.  Command line will be ignored by the file parser.
 * 		(4) Empty lines (lines with only spaces) will be ignored by the file parser.
 * <p>
 * Each checker line has to start with the full class name (including the package name).  It can follow by its arguments (if any).
 * The arguments details are defined by each checker itself.
 * <p>
 * @author Manqing Li
 *
 */
public class CheckCommandOutput {
	private static final String PROMPT_FORMAT_DEFAULT = "> ";

	public static void main(String[] args) {

		if (args.length < 2) {
			System.err.println("The command output file and the command checking file are required to check.");
			System.exit(1);
		}
		
		CheckCommandOutput checker = new CheckCommandOutput();
		List<String> commandOutputs = null;
		try {
			commandOutputs = checker.readFile(args[0]);
		} catch (FileNotFoundException e) {
			System.err.println("Command output file " + args[0] + " not found.");
			System.exit(1);
		} catch (IOException e) {
			System.err.println("Failed to read command output file " + args[0]);
			System.exit(1);
		}
	
		List<String> commandChecker = null;

		try {
			commandChecker = checker.readFile(args[1]);
		} catch (FileNotFoundException e) {
			System.err.println("Command checking file " + args[1] + " not found.");
			System.exit(1);
		} catch (IOException e) {
			System.err.println("Failed to read command checking file " + args[1]);
			System.exit(1);
		}
		
		boolean passed = checker.check(commandOutputs, commandChecker);
		if(passed) {
			System.err.println("Tests passed.");
			System.exit(0);
		} else {
			System.err.println("Tests failed.");
			System.exit(1);
		}
	}

	private boolean check(List<String> commandOutputs, List<String> commandCheckerLines) {
		boolean allTestsSucceeded = true;
		List<CommandWithCheckers> commandCheckingUnits = parseCheckers(commandCheckerLines);
		if(commandCheckerLines == null || commandCheckerLines.size() == 0) {
			System.err.println("Warning : no checkers are found.\n");
			return true;
		}
		OutputContentParser parser = new OutputContentParser(commandOutputs, PROMPT_FORMAT_DEFAULT + commandCheckingUnits.get(0).targetCommand);
		for(int i = 0; i < commandCheckingUnits.size(); i++) {
			CommandWithCheckers aCheckingUnit = commandCheckingUnits.get(i);
			String nextCommand = null;
			if( i < commandCheckingUnits.size() - 1) {
				nextCommand = commandCheckingUnits.get(i + 1).targetCommand;
			}
			List<String> output = parser.extractOutputLinesUntilTheStartOfNextCommand(aCheckingUnit.targetCommand, nextCommand); 
			
			if(aCheckingUnit.hasCheckers()) {
				System.err.println("Checking the output from command : " + aCheckingUnit.targetCommand);	
				if(aCheckingUnit.checking(output) == false) {
					System.err.println("Tests failed.");	
					allTestsSucceeded = false;
				} else {
					System.err.println("Tests passed.");
				}
			}
		}
		return allTestsSucceeded;
	}
	
	private static final Pattern CHECKER_LINE = Pattern.compile("\\s*//\\s*checker\\s*:\\s*(\\S+.*)", Pattern.CASE_INSENSITIVE + Pattern.DOTALL);
	private List<CommandWithCheckers>  parseCheckers(List<String> allLines) {
		List<CommandWithCheckers> result = new ArrayList<CommandWithCheckers>();
		String tmpCommand = null;
		List<String> tmpCheckerLineList = new ArrayList<String>();
		for(String line : allLines) {
			if(line.trim().length() == 0) {
				continue;
			}
			if(isCommentsLine(line)) {
				Matcher matcher = CHECKER_LINE.matcher(line);
				if(matcher.matches()) {
					tmpCheckerLineList.add(matcher.group(1));
				}
			} else {
				if(tmpCommand != null) {
					result.add(new CommandWithCheckers(tmpCommand, tmpCheckerLineList)); 
				}
				tmpCommand = line;
				tmpCheckerLineList = new ArrayList<String>();
			}
		}
		if(tmpCommand != null) {
			result.add(new CommandWithCheckers(tmpCommand, tmpCheckerLineList)); 
		}
		
		return result;
	}
	
	private List<String> readFile(String filePath) throws FileNotFoundException, IOException {
		BufferedReader in = new BufferedReader(new FileReader(filePath));
		List<String> result = new ArrayList<String>();
		String line = null;
		while((line = in.readLine()) != null) {
			result.add(line);
		}
		in.close();
		return result;
	}
	
	private boolean isCommentsLine(String line) {
		return line.trim().startsWith("#") || line.trim().startsWith("//");
	}
	
	private class CommandWithCheckers {
		public CommandWithCheckers(String targetCommand, List<String> checkers) {
			this.targetCommand = targetCommand;
			this.checkingLines = (checkers == null ? new ArrayList<String>() : checkers);
		}
		public boolean checking(List<String> output) {
			for(String line : checkingLines) {
				line = line.trim();
				int index = StringUtils.getFirstTokenEndIndex(line);
				String checkerClass = line.substring(0, index + 1);
				try {
					ICommandOutputChecker checkerObject = (ICommandOutputChecker)Class.forName(checkerClass).newInstance();

					if(checkerObject.check(targetCommand, line.substring(index + 1).trim(), output) == false) {
						System.err.println("The outputs are : ");
						for(String outputLine : output) {
							System.err.println("\t" + outputLine);
						}
						System.err.println("The checker is : ");
						System.err.println("\t" + line);
						return false;
					}
				} catch (IllegalAccessException e) {
					e.printStackTrace(System.err);
					System.err.println("Failed to create the checker " + checkerClass);
					return false;
				} catch (InstantiationException e) {
					e.printStackTrace(System.err);
					System.err.println("Failed to create the checker " + checkerClass);
					return false;
				} catch (ClassNotFoundException e) {
					e.printStackTrace(System.err);
					System.err.println("Failed to create the checker " + checkerClass);
					return false;
				}
			}
			return true;
		}
		
		public boolean hasCheckers() {
			return this.checkingLines.size() > 0;
		}
		
		private String targetCommand;
		private List<String> checkingLines;
	}
	
	private class OutputContentParser {
		public OutputContentParser(List<String> output, String startOfFirstCommand) {
			this.output = output;
			this.position = 0;
			while(this.position < output.size() && startOfFirstCommand.trim().equals(output.get(position).trim()) == false) {
				this.position++;
			}
		}
		
		public List<String> extractOutputLinesUntilTheStartOfNextCommand(String command, String nextCommand) {
			List<String> result = new ArrayList<String>();
			
			if(position < output.size()) {
				if(output.get(position).trim().equalsIgnoreCase(PROMPT_FORMAT_DEFAULT + command) == false) {
					System.err.println("Failed to found the matching output for command " + command);
					System.err.println("Tests failed.");
					System.exit(1);
				}
			}
			position++;
			String commandInputLine = nextCommand == null ? null : PROMPT_FORMAT_DEFAULT + nextCommand;
			while(position < output.size()) {
				String line = output.get(position);
				if(commandInputLine == null) { 
					// if the startOfNextCommand is null, then position == output.size() to exit the while loop.
					result.add(line); 
				} else {  
					// if the startOfNextCommand is NOT null, then the position will point to the 
					//nextCommand to exit the while loop or it points to the end of the list.
					if(commandInputLine.trim().equals(line.trim())) {
						break;
					} else {
						result.add(line);
					}
				}
				position++;
			}
			return result;
		}
		
		private int position;
		private List<String> output;
	}
}
