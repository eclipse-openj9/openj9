/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2011, 2019 IBM Corp. and others
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

package com.ibm.java.diagnostics.utils.commands;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintStream;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;


/**
 * Handles command line entries and does extra interpretation, such as handling quoted strings (whilst discarding
 * quotes themselves and handling escaped characters), recognising redirection to file etc.
 * 
 * NOTE: As of Nov 2011, another copy of this class lives in com.ibm.j9ddr.command. Any changes to this file must be copied across. 
 * @author blazejc
 *
 */
public class CommandParser {

	private String originalLine; // used in toString(), primarily for debugging
	
	private List<String> allTokens = new ArrayList<String>();
	private String command;
	private List<String> arguments = new ArrayList<String>(); // just the command arguments, ignoring file redirection
	private String redirectionFilename;
	
	private boolean isAppendToFile;
	private boolean isRedirectedToFile;
	
	public CommandParser(String commandLine) throws ParseException {
		originalLine = commandLine;
		tokenise(commandLine);
		parse(allTokens);
	}
	
	public CommandParser(String command, String[] arguments) throws ParseException {
		List<String> allTokens = new ArrayList<String>();
		allTokens.add(command);
		allTokens.addAll(Arrays.asList(arguments));
		
		originalLine = command;
		
		for (String arg : arguments) {
			originalLine += " " + arg;
		}
		
		parse(allTokens);
	}
	
	@Override
	public String toString() {
		return originalLine;
	}
	
	public String getOriginalLine() {
		return originalLine;
	}
	
	public String getCommand() {
		return command;
	}
	
	public boolean isRedirectedToFile() {
		return isRedirectedToFile || isAppendToFile;
	}
	
	/**
	 * Just the command arguments, strips the ">" or ">>" and all strings that follow (i.e. file redirection).
	 * @return 
	 */
	public String[] getArguments() {
		return arguments.toArray(new String[0]);
	}
	
	/**
	 * Returns a PrintStream, which either writes to a new file or overwrites or appends to an existing one,
	 * depending on the command.
	 * @return
	 * @throws IOException
	 * @throws IllegalStateException - thrown if the command is not redirected to file. Use isRedirectedToFile() to check.
	 */
	public PrintStream getOutputFile() throws IOException {
		if (!isRedirectedToFile && !isAppendToFile) {
			throw new IllegalStateException("Attempt to create output file for a command which does not contain file redirection.");
		}
		
		PrintStream outStream = null;
		File outFile = new File(redirectionFilename);
		
		if (redirectionFilename.indexOf(File.separator) > -1) {
			// The last entry is the filename itself (we've checked that during parsing stage)
			// so everything before it is the directory, some of which may not exist, so need to create it
			String parentDirectories = redirectionFilename.substring(0, redirectionFilename.lastIndexOf(File.separator));
			File dir = new File(parentDirectories);
			if (!dir.exists() && !dir.mkdirs()) {
				throw new IOException("Could not create some of the requested directories: " + parentDirectories);
			}
		}
		
		if (isAppendToFile && outFile.exists()) {
			// in this case do not create a new file, but use the existing one
			outStream = new PrintStream(new FileOutputStream(outFile, true));
		} else {
			outStream = new PrintStream(outFile); // this will create a new file
		}

		return outStream; 
	}
	
	/*
	 * The tokenise() and parse() work together to interpret the command line.
	 * Initially, tokenise() reads the raw text and applies syntax rules to break it up into tokens
	 * (e.g. by handling quoted strings). Then, parse() reads the tokens and extracts the command
	 * itself, its arguments and possible file redirection options.  
	 */

	private void tokenise(String commandLine) throws ParseException {
		char[] characters = commandLine.toCharArray();
		
		int i = 0;
		
		TokenisingState currentState = new GenericTokenState(this);
		do {
			currentState = currentState.process(characters[i++]);
			
			if (i == characters.length) {
				currentState.endOfInput();
			}
		} while (i < characters.length);
	}
	
	private void parse(List<String> allTokens) throws ParseException {
		if (allTokens.size() == 0) {
			return;
		}
		
		command = allTokens.get(0);
		int i;
		for (i = 1; i < allTokens.size(); i++) {
			if (!allTokens.get(i).equals(">") && !allTokens.get(i).equals(">>")) {
				arguments.add(allTokens.get(i));
			} else {
				break;
			}
		}
		
		// all that follows after and including '>' or '>>' is file redirection
		if (i < allTokens.size()) {
			if (allTokens.get(i).equals(">")) {
				isAppendToFile = false;
				isRedirectedToFile = true;
			} else { // isn't '>' so must be '>>', otherwise it would be another argument captured in the previous loop
				isAppendToFile = true;
				isRedirectedToFile = false;
			}
			
			if (i + 1 < allTokens.size()) {
				String filename = allTokens.get(i + 1).trim(); 
				if (filename.charAt(filename.length() - 1) == File.separatorChar) {
					throw new ParseException("Invalid redirection path - missing filename", i);
				}
				
				redirectionFilename = allTokens.get(i + 1);
			} else {
				throw new ParseException("Missing file name for redirection", i);
			}
		}
	}

// TokenisingState classes
	
	/**
	 * A mini state machine. Each state receives a character and returns the next state.
	 * So, for example, if a GenericTokenState encounters a quotation mark (either single or double),
	 * it returns a QuotedStringState, which has different rules. If a QuotedStringState encounters
	 * an (unescaped) quotation mark (i.e. end of quotation) it exits and returns a GenericTokenState.
	 */
	abstract class TokenisingState {
		
		protected CommandParser context;
		protected StringBuilder currentToken = new StringBuilder();
		abstract protected TokenisingState process(Character chr) throws ParseException;
		
		protected TokenisingState(CommandParser context) {
			this.context = context;
		}
		
		protected void storeToken() {
			if (currentToken.length() > 0) {
				context.allTokens.add(currentToken.toString());
				currentToken = new StringBuilder();
			}
		}
		
		abstract protected void endOfInput() throws ParseException;
	}

	/**
	 * Any token written to the command line as-is, without any quotation or escaping
	 * @author blazejc
	 *
	 */
	class GenericTokenState extends TokenisingState {

		protected GenericTokenState(CommandParser context) {
			super(context);
		}

		@Override
		protected TokenisingState process(Character chr) throws ParseException {
			if (Character.isWhitespace(chr)) {
				if (currentToken.toString().equals(">") || currentToken.toString().equals(">>")) {
					storeToken();
					return new FilenameState(context);
				} else {
					storeToken();
					return this;
				}
			}
			
			switch (chr) {
				case '"':
				case '\'':
					if (currentToken.length() > 0) {
						throw new ParseException("'" + chr + "'" + " invalid inside a string", 0);
					} else {
						return new QuotedStringState(context, chr);
					}
				default:
					currentToken.append(chr);
					return this;
			}
		}

		@Override
		protected void endOfInput() {
			storeToken();			
		}
	}

	class QuotedStringState extends TokenisingState {

		private Character quoteType;
		private boolean escape = false;
		
		protected QuotedStringState(CommandParser context, Character quoteType) {
			super(context);
			this.quoteType = quoteType;
		}

		@Override
		protected TokenisingState process(Character chr) throws ParseException {
			switch (chr) {
				case '\\':
					if (escape) { // are we're escaping a '\'?
						currentToken.append(chr);
						escape = false;
					} else {
						// don't store the escape character itself
						escape = true;
					}
					return this;
				case '"':
				case '\'': {
					if (chr != quoteType) { // a quote different than the surrounding one doesn't need to be escaped, just treat it as any other character 
						currentToken.append(chr);
						return this;
					} else { // same quote as the surrounding one
						if (escape) {
							currentToken.append(chr);
							escape = false;
							return this;
						} else { // same quote as the surrounding one, so we're closing the quote here
							storeToken(); 
							return new GenericTokenState(context);
						}
					}
				}
				default:
					if (escape) {
						// the previous character was a backslash (hence escaping flag is set), but what follows is just a normal character,
						// so put back the backslash, we're not escaping anything
						currentToken.append("\\");
						escape = false;
					}
					currentToken.append(chr); // append any other character, leave QuotedStringState as current state
					return this;
			}
		}

		@Override
		protected void endOfInput() throws ParseException {
			if (currentToken.length() > 0) { // we haven't closed the quote, but the input has ended - that's an error
				throw new ParseException("Unmatched " + quoteType, 0);
			}
		}
	}
	
	class FilenameState extends TokenisingState {

		protected FilenameState(CommandParser context) {
			super(context);
		}

		private Character quoteType = null; // the quote the whole filename is surrounded with, null if it isn't
		private boolean alreadyStarted = false;
		private boolean alreadyFinished = false;
		
		@Override
		protected TokenisingState process(Character chr) throws ParseException {
			if (alreadyFinished) {
				throw new ParseException("Only one file name is permitted", 0);
			}
			
			if (!alreadyStarted) {
				if (chr.equals('"') || chr.equals('\'')) {
					quoteType = chr;
				} else {
					currentToken.append(chr);
				}
				alreadyStarted = true;
			} else {
				if (chr.equals(quoteType)) {
					storeToken();
					alreadyFinished = true;
				} else if (Character.isWhitespace(chr)) {
					if (quoteType != null) {
						currentToken.append(chr); // whitespace within a quoted string is just another character, so store it
					} else {
						storeToken(); // whitespace in an unquoted string is the end of it
						alreadyFinished = true;
					}
				} else {
					currentToken.append(chr);
				}
			}

			return this;
		}

		@Override
		protected void endOfInput() throws ParseException {
			if (!alreadyStarted) {
				throw new ParseException("Missing file name", 0);
			}
			if (alreadyStarted && !alreadyFinished && quoteType != null) {
				throw new ParseException("Unmatched " + quoteType + " in file name", 0);
			}
			//started; not finished but no quotes, or finished with quotes
			storeToken();
		}
	}
}
