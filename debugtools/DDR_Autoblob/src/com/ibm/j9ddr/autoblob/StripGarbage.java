/*******************************************************************************
 * Copyright (c) 2010, 2019 IBM Corp. and others
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
package com.ibm.j9ddr.autoblob;

import java.io.File;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.Reader;
import java.io.Writer;
import java.util.ArrayList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Removes non-ANSI garbage from pre-processed C/C++
 * 
 * Takes foo.i as an input, and produces foo.ic as an output.
 * @author andhall
 *
 */
public class StripGarbage
{
	/**
	 * @param args
	 */
	public static void main(String[] args) throws Exception
	{
		for (String filename : args) {
			System.out.println("Cleaning " + filename);
			File input = new File(filename);
			File output = new File(filename + "c");
			
			//Slurp the entire file into a buffer
			StringBuilder builder = new StringBuilder();
			Reader reader = new FileReader(input);
			char[] buffer = new char[4096];
			
			try {
				int read;
				
				do {
					read = reader.read(buffer);
				
					if (read != -1) {
						builder.append(buffer,0,read);
					}
				} while (read != -1);
				
			} finally {
				reader.close();
			}
			
			String cleaned = stripGarbage(builder.toString());
			
			Writer writer = new FileWriter(output);
			try {
				writer.write(cleaned);
				writer.close();
			} finally {
				writer.close();
			}
		}
	}
	
	private final static Pattern DECLSPEC = Pattern.compile("_+declspec\\(.*?\\)", Pattern.DOTALL + Pattern.MULTILINE);
	private final static Pattern ASM = Pattern.compile("_+asm\\s+\\{.*?\\}", Pattern.DOTALL + Pattern.MULTILINE);
	private final static Pattern ASM2 = Pattern.compile("_*asm\\s*(?:volatile)?\\(.*?\\);", Pattern.DOTALL + Pattern.MULTILINE);
	private final static Pattern ATTRIBUTE = Pattern.compile("__attribute__\\s*[(]{2}.*?[)]{2}", Pattern.DOTALL + Pattern.MULTILINE);
	private final static Pattern TRAILING_BACKSLASH = Pattern.compile("\\\\+\\s*$", Pattern.MULTILINE);
	private final static Pattern PRAGMA = Pattern.compile("^#pragma .*$", Pattern.MULTILINE);
	private final static String C_MULTILINE_DELIM = "\\";
	private final static char C_MULTILINE_DELIM_CHAR = '\\';
	
	private static String stripGarbage(String contents)
	{
		String working = contents;
				
		working = removeMultiLineGarbage(working, PRAGMA);
		
		//Windows
		working = working.replaceAll("_+cdecl", "");
		working = working.replaceAll("_+stdcall", "");
		working = DECLSPEC.matcher(working).replaceAll("");
		working = ASM.matcher(working).replaceAll("");
		working = TRAILING_BACKSLASH.matcher(working).replaceAll("");
		working = working.replaceAll("__forceinline","");
		working = working.replaceAll("\u000c", "");
		working = working.replaceAll("WINAPI", "");
		
		//gcc
		working = working.replaceAll("__restrict","");
		working = working.replaceAll("__const","");
		working = working.replaceAll("__attribute__\\s*[(]{2}.*[)]{2}", "");
		working = working.replaceAll("__asm__[^;]+", "");
		working = ATTRIBUTE.matcher(working).replaceAll("");
		working = working.replaceAll("(<?=\\s)_+inline","");
		working = working.replaceAll("__extension__","");
		working = ASM2.matcher(working).replaceAll("");
		
		return working;
	}
	
	/**
	 * Removes multi-line or single sections from the supplied data. It assumes that the data is continued over multiple lines by use of the \ character.
	 * @param data string to scan matches for
	 * @param pattern regex describing the start of the match
	 * @param blockID string describing how a multi-line block is identified e.g. { or \
	 * @param terminator string describing the termination of a the multi-line match (note this is currently not a reg-ex)
	 * @return the altered data or if no matches were found the unchanged data
	 */
	private static String removeMultiLineGarbage(String data, Pattern pattern) {
		ArrayList<Region> regions = new ArrayList<Region>();
		Matcher matcher = pattern.matcher(data);
		while(matcher.find()) {
			String declaration = matcher.group();
			if(declaration.endsWith(C_MULTILINE_DELIM)) {
				//multi-line declaration, locate first line that does not end with a \
				int pos = data.indexOf('\n', matcher.start());
				int offset = 1;
				if(pos != -1) {
					if('\r' == data.charAt(pos - 1)) {
						//adjust for running on windows
						offset++;
					}
				}
				while((pos != -1) && (C_MULTILINE_DELIM_CHAR == data.charAt(pos - offset))) {
					pos = data.indexOf('\n', ++pos);
				}
				if(-1 == pos) {
					String msg = String.format("Warning : unmatched end of multi-line declaration for %s starting at position %d", pattern.toString(), matcher.start());
					System.err.println(msg);
				} else {
					regions.add(new Region(matcher.start(), ++pos));
				}
			} else {
				//single line declaration
				regions.add(new Region(matcher.start(), matcher.end()));
			}
		}
		
		if(regions.size() != 0) {
			//now remove all the specified regions from the string
			StringBuilder temp = new StringBuilder();
			int index = 0;
			for(Region region : regions) {
				temp.append(data.substring(index, region.getStart()));
				index = region.getEnd();
			}
			temp.append(data.substring(index));	//copy remaining text
			return temp.toString();
		} else {
			//return data unchanged
			return data;
		}
	}
	
	private static class Region {
		private final int start;
		private final int end;
		
		public Region(int start, int end) {
			this.start = start;
			this.end = end;
		}

		public int getStart() {
			return start;
		}
		
		public int getEnd() {
			return end;
		}
		
		
	}

}
