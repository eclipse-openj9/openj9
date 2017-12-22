/*******************************************************************************
 * Copyright (c) 2010, 2016 IBM Corp. and others
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
package com.ibm.j9ddr.autoblob.constants;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.Reader;
import java.io.StringReader;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import com.ibm.j9ddr.util.IteratorHelpers;
import com.ibm.j9ddr.util.IteratorHelpers.IteratorFilter;

/**
 * Extracts constants (#define NAME = <numeric value>) from 
 * C files.
 * 
 * A limited-function C pre-processor.
 * 
 * @author andhall
 *
 */
public class ConstantParser
{
	/* Matches #define NAME <something> - we unpick the <something> with other patterns */
	private static Pattern CONSTANT_DEFINE_PATTERN = Pattern.compile("#define\\s+([^(\\s]+?)\\s+(.+)$");

	private static Pattern CASTS_PATTERN = Pattern.compile("^\\(+.*?\\)+(?!$)");
	
	private static Pattern HEX_NUMERIC_PATTERN = Pattern.compile("^\\(?\\s*0x[a-fA-F0-9]+\\s*\\)?$");
	
	private static Pattern DEC_NUMERIC_PATTERN = Pattern.compile("^\\(?\\s*-?[0-9]+\\)?$");
	
	private static Pattern SIMPLE_EXPRESSION_PATTERN = Pattern.compile("^\\(?[0-9x+\\-*/|&]+\\)$");
	
	//the J964CONST macro ensures the correct declaration of 64 bit values on different platforms, it contains no logic and does not alter the parameter.
	//this means we can include it in the blob, however if this changes at a later point in time then it can no longer be regarded as constant and will be invalid in the blob.
	private static Pattern J9CONST64_MACRO_PATTERN = Pattern.compile("^J9CONST64\\(0x[a-fA-F0-9]+\\)|J9CONST64\\([0-9]+\\)$");
	
	private static Pattern[] VALID_VALUE_PATTERNS = new Pattern[]{HEX_NUMERIC_PATTERN, DEC_NUMERIC_PATTERN, SIMPLE_EXPRESSION_PATTERN, J9CONST64_MACRO_PATTERN};
	
	private static final Pattern BUILD_FLAGS_PATTERN = Pattern.compile("^\\s*#(define|undef) ([^(\\s]+?)\\s*$");
	
	private static enum CommentState {OPEN, FIRST_FORWARDSLASH, IN_SINGLE_LINE_COMMENT, IN_MULTILINE_COMMENT, IN_MULTILINE_COMMENT_FIRST_STAR}; 
	
	private static final String nl = System.getProperty("line.separator");
	
	public void visitCFile(File file, ICVisitor v) throws IOException
	{
		//Pass 1 strips out comments
		CommentStripResult stripped = stripComments(file);
		
		//Pass 2 extracts constants
		BufferedReader reader = new BufferedReader(stripped.cleanContentReader);
		
		List<IFileEventCommand> events = new LinkedList<IFileEventCommand>();
		List<String> nonConstants = new LinkedList<String>();
		events.addAll(stripped.commentCommands);
		
		String line = null;
		int lineNumber = 0;
		
		String runOnLine = null;
		
		Set<String> definedMacros = new HashSet<String>();
		
		while ( (line = reader.readLine()) != null) {
			lineNumber++;
			
			if (runOnLine != null) {
				line = runOnLine + nl + line;
				runOnLine = null;
			}
			
			if (line.length() == 0) {
				continue;
			}
			
			if (line.charAt(line.length() - 1) == '\\') {
				//Run-on line
				runOnLine = line.substring(0, line.length() - 1);
				continue;
			}
			
			processLine(line, events, nonConstants, lineNumber, definedMacros);
		}
		
		events = removeNonConstants(events, nonConstants);
		
		Collections.sort(events);
		
		for (IFileEventCommand command : events) {
			command.executeCommand(v);
		}
	}

	private void processLine(String line, List<IFileEventCommand> events, List<String> nonConstants, final int lineNumber, Set<String> definedMacros)
	{
		//Constant definition?
		Matcher constantDefMatcher = CONSTANT_DEFINE_PATTERN.matcher(line);
		Matcher flagMatcher = BUILD_FLAGS_PATTERN.matcher(line);
		
		if (constantDefMatcher.find()) {
			final String name = constantDefMatcher.group(1);
			String working = constantDefMatcher.group(2);
			
			working = replaceMacros(working, definedMacros);
			working = working.replaceAll("\\s", "");
			
			Matcher castPatternMatcher = CASTS_PATTERN.matcher(working);
			
			if (castPatternMatcher.find()) {
				working = castPatternMatcher.replaceAll("");
			}
			
			boolean validValue = false;
			
			for (Pattern p : VALID_VALUE_PATTERNS) {
				Matcher m = p.matcher(working);
				
				if (m.matches()) {
					validValue = true;
					break;
				}
			}
			
			if (validValue) {
				events.add(new ConstantFileEvent(name, lineNumber));
				
				definedMacros.add(name);
			} else {
				/* Address the issue of finding multiple #define's for the same name where one of the #define's does not specify a constant
				 * Create a collection of non-constant #define names and remove them from events later.
				 */
				
				nonConstants.add(name);
				
				/* TODO: lpnguyen worry about some pathological cases where this isn't sufficient. 
				 * Also worry about the fact we won't pick up macro defines outside of a given header */
				while (definedMacros.remove(name)) {
					/* May be multiple occurrences on different lines */
				}
			}
		} else if (flagMatcher.matches()) {
			String op = flagMatcher.group(1);
			final String name = flagMatcher.group(2);
			
			final boolean isDefine = op.toLowerCase().equals("define");
			
			events.add(new BaseFileEventCommand(){

				public void executeCommand(ICVisitor visitor)
				{
					visitor.visitNoValueConstant(name, isDefine);
				}

				public int getLineNumber()
				{
					return lineNumber;
				}});
		}

	}
	
	private static List<IFileEventCommand> removeNonConstants(List<IFileEventCommand> events, final List<String> nonConstants)
	{
		/* TODO: lpnguyen, make a listHelper instead of doing all this conversion to/from iterators */
		Iterator<IFileEventCommand> eventsIterator = IteratorHelpers.filterIterator(events.iterator(), new IteratorFilter<IFileEventCommand>()
			{
				public boolean accept(IFileEventCommand obj)
				{
					if (obj instanceof ConstantFileEvent) {
						ConstantFileEvent event = (ConstantFileEvent) obj;
						return !nonConstants.contains(event.getName());
					}
					return true;
				}
			}
		);
		return IteratorHelpers.toList(eventsIterator);
	}
	
	private String replaceMacros(final String input, Set<String> definedMacros)
	{
		String working = input;
		
		for (String macro : definedMacros) {
			if (working.contains(macro)) {
				// We don't need to replace the macro with the correct value - just something the rest of the 
				// algorithm will recognise as a number
				working = working.replaceAll(Pattern.quote(macro), "1");
			}
		}
		
		return working;
	}

	private static CommentStripResult stripComments(File inputFile) throws IOException
	{
		StringBuilder nonCommentContent = new StringBuilder();
		List<IFileEventCommand> events = new LinkedList<IFileEventCommand>();
		StringBuilder currentCommentContent = null;
		int lineNumber = 1;
		CommentState state = CommentState.OPEN;
		int readValue;
		int commentStartLine = -1;
		
		Reader reader = new BufferedReader(new FileReader(inputFile));
		
		try {
			while ( (readValue = reader.read()) != -1) {
				char thisChar = (char)readValue;
				switch (state) {
				case OPEN:
					if (thisChar == '/') {
						state = CommentState.FIRST_FORWARDSLASH;
					} else {
						nonCommentContent.append(thisChar);
					}
					break;
				case FIRST_FORWARDSLASH:
					if (thisChar == '/') {
						state = CommentState.IN_SINGLE_LINE_COMMENT;
						currentCommentContent = new StringBuilder();
						commentStartLine = lineNumber;
						commentChar(nonCommentContent, currentCommentContent,'/');
						commentChar(nonCommentContent, currentCommentContent,'/');
					} else if (thisChar == '*') {
						state = CommentState.IN_MULTILINE_COMMENT;
						currentCommentContent = new StringBuilder();
						commentStartLine = lineNumber;
						commentChar(nonCommentContent, currentCommentContent,'/');
						commentChar(nonCommentContent, currentCommentContent,'*');
					} else {
						nonCommentContent.append("/");
						nonCommentContent.append(thisChar);
						state = CommentState.OPEN;
					}
					break;
				case IN_SINGLE_LINE_COMMENT:
					commentChar(nonCommentContent, currentCommentContent,thisChar);
					
					if (thisChar == '\n') {
						events.add(new CommentFileEvent(currentCommentContent.toString(), commentStartLine));
						currentCommentContent = null;
						state = CommentState.OPEN;
					}
					
					break;
				case IN_MULTILINE_COMMENT:
					if (thisChar == '*') {
						state = CommentState.IN_MULTILINE_COMMENT_FIRST_STAR;
					} else {
						commentChar(nonCommentContent, currentCommentContent,thisChar);
					}
					break;
				case IN_MULTILINE_COMMENT_FIRST_STAR:
					if (thisChar == '/') {
						state = CommentState.OPEN;
						commentChar(nonCommentContent, currentCommentContent,'*');
						commentChar(nonCommentContent, currentCommentContent,'/');
						boolean isComment = true;
						
						if (commentStartLine == lineNumber) {
							/* This is actually a single line comment which may include an undef.  If it is a comment containing
							 * an undef declaration we need to turn it into a regular undef without the comment header/footer */
							String search = "/* #undef OMR_";
							int searchLength = search.length();
							if ((searchLength < currentCommentContent.length()) && currentCommentContent.substring(0, searchLength).equals(search)) {
								int nonCommentLength = nonCommentContent.length();
								int currentCommentLength = currentCommentContent.length();
								String start = "/* ";
								String end = " */";
								/* get the undef flag string out of the comment buffer */
								String undefineFlag = currentCommentContent.substring(start.length(), currentCommentLength - end.length());
								/* replace the ' ' characters with the actual undef flag line */
								nonCommentContent.replace(nonCommentLength - currentCommentLength, nonCommentLength, undefineFlag);
								
								isComment = false;
							}
						}
						if (isComment) {
							events.add(new CommentFileEvent(currentCommentContent.toString(), commentStartLine));
						}
						currentCommentContent = null;
					} else {
						state = CommentState.IN_MULTILINE_COMMENT;
						commentChar(nonCommentContent, currentCommentContent, '*');
						commentChar(nonCommentContent, currentCommentContent,thisChar);
					}
					break;
				default:
					throw new RuntimeException("Unexpected state: " + state);
				};
				
				if (thisChar == '\n') {
					lineNumber++;
				}

			}
		} finally {
			reader.close();
		}
		
		return new CommentStripResult(new StringReader(nonCommentContent.toString()), events);
	}

	private static void commentChar(StringBuilder nonCommentContent,
			StringBuilder currentCommentContent, char thisChar)
	{
		currentCommentContent.append(thisChar);
		if (Character.isWhitespace(thisChar)) {
			nonCommentContent.append(thisChar);
		} else {
			nonCommentContent.append(" ");
		}
	}
	
	private static class CommentStripResult
	{
		final Reader cleanContentReader;
		
		final List<IFileEventCommand> commentCommands;
		
		public CommentStripResult(Reader cleanContentReader, List<IFileEventCommand> commentCommands)
		{
			this.cleanContentReader = cleanContentReader;
			this.commentCommands = commentCommands;
		}
	}
	
	private static abstract class BaseFileEventCommand implements IFileEventCommand
	{
		public int compareTo(IFileEventCommand o)
		{
			return this.getLineNumber() - o.getLineNumber();
		}
	}
	
	private static class CommentFileEvent extends BaseFileEventCommand implements IFileEventCommand
	{
		private final String comment;
		private final int lineNumber;
		
		public CommentFileEvent(String comment, int lineNumber)
		{
			this.comment = comment;
			this.lineNumber = lineNumber;
		}
		
		public void executeCommand(ICVisitor visitor)
		{
			visitor.visitComment(comment, lineNumber);
		}

		public int getLineNumber()
		{
			return lineNumber;
		}
		
	}
	
	private static class ConstantFileEvent extends BaseFileEventCommand implements IFileEventCommand
	{
		private final String name;
		private final int lineNumber;
		
		public ConstantFileEvent(String name, int lineNumber)
		{
			this.name = name;
			this.lineNumber = lineNumber;
		}
		
		public void executeCommand(ICVisitor visitor)
		{
			visitor.visitNumericConstant(name, lineNumber);
		}

		public int getLineNumber()
		{
			return lineNumber;
		}
		
		public String getName()
		{
			return name;
		}
	}
	
	private static interface IFileEventCommand extends Comparable<IFileEventCommand>
	{
		public int getLineNumber();
		
		public void executeCommand(ICVisitor visitor);
	}
}
