/*******************************************************************************
 * Copyright (c) 2010, 2014 IBM Corp. and others
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
package com.ibm.j9ddr.autoblob.linenumbers;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.io.UnsupportedEncodingException;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.ListIterator;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Encapsulates knowledge of how to extract data (mostly file names and line numbers) from pre-processed source
 * 
 * @author andhall
 *
 */
public class PreProcessedSourceParser
{
	private static final Pattern LINE_PATTERN = Pattern.compile("#line\\s+(\\d+)\\s+\"(.*?)\"");
	
	private static final Pattern GNU_LINE_PATTERN = Pattern.compile("#\\s+(\\d+)\\s+\"(.*?)\"");
	
	private final List<Mapping> mappings;
	
	public PreProcessedSourceParser(File inputFile, String encoding) throws IOException
	{
		BufferedReader reader = getReader(inputFile,encoding);
		
		mappings = readMappings(inputFile, reader);
	}
	
	/**
	 * @return Mapping of simple filenames to on-disk files
	 */
	public Map<String, File> getFileMappings()
	{
		Map<String, File> toReturn = new HashMap<String, File>();
		
		for (Mapping thisMapping : mappings) {
			String filename = thisMapping.getFileName();
			
			File file = new File(filename);
			
			toReturn.put(file.getName(), file);
		}
		
		return Collections.unmodifiableMap(toReturn);
	}
	
	public ILineNumberConverter getLineNumberConvertor()
	{
		return new LineNumberConvertor(mappings);
	}
	
	private static boolean rejectBogusMappings(String sourceFileName)
	{
		if (sourceFileName.endsWith("<built-in>") || sourceFileName.endsWith("<built in>") ||
			sourceFileName.endsWith("<command-line>") || sourceFileName.endsWith("<command line>")) {
			return false;
		}
		return true;
	}
	
	private static List<Mapping> readMappings(File inputFile, BufferedReader reader) throws IOException
	{
		List<Mapping> mappings = new LinkedList<Mapping>();
		int lineNumber = 0;
		
		String line = null;
		
		while ( (line = reader.readLine()) != null) {
			try {
				lineNumber++;
				
				Matcher m = LINE_PATTERN.matcher(line);
				Matcher gnuMatcher = GNU_LINE_PATTERN.matcher(line);
				
				if (m.find()) {
					int sourceLineNumber = Integer.parseInt(m.group(1));
					String sourceFileName = m.group(2);
					
					if (rejectBogusMappings(sourceFileName) == false) {
						continue;
					}
					
					String absoluteSourceFilePath = resolvePath(inputFile, sourceFileName);
					
					mappings.add(new Mapping(lineNumber, absoluteSourceFilePath, sourceLineNumber));
				} else if (gnuMatcher.find()) {
					int sourceLineNumber = Integer.parseInt(gnuMatcher.group(1));
					String sourceFileName = gnuMatcher.group(2);

					if (rejectBogusMappings(sourceFileName) == false) {
						continue;
					}
					
					String absoluteSourceFilePath = resolvePath(inputFile, sourceFileName);
					
					mappings.add(new Mapping(lineNumber, absoluteSourceFilePath, sourceLineNumber));
				}
			} catch (Throwable t) {
				throw new RuntimeException("Throwable produced on line " + lineNumber, t);
			}
		}
		
		return mappings;
	}

	//Note: this assumes that the pre-processed file is next to the original C file.
	private static String resolvePath(File preprocessedFile, String path) throws IOException
	{
		String resolvedPath;
		File filePath = new File(path);
		
		if (filePath.isAbsolute()) {
			resolvedPath = filePath.getCanonicalPath();
		} else {
			resolvedPath = new File(preprocessedFile.getCanonicalFile().getParentFile(), path).getCanonicalPath();
		}
		
		if (new File(resolvedPath).exists() == false) {
			throw new IOException("The resolved path [ " + resolvedPath + "] does not exist");
		}
		
		return resolvedPath;
	}
		

	private static BufferedReader getReader(File inputFile, String encoding) throws UnsupportedEncodingException, IOException
	{
		Reader reader = null;
		if (encoding == null) {
			reader = new FileReader(inputFile);
		} else {
			reader = new InputStreamReader(new FileInputStream(inputFile), encoding);
		}
		
		return new BufferedReader(reader);
	}

	private static class Mapping extends SourceLocation
	{
		public final int preProcessedLineNumber;
		
		public Mapping(int preProcessedLineNumber, String sourceFileName, int sourceLineNumber)
		{
			super(sourceFileName, sourceLineNumber);
			this.preProcessedLineNumber = preProcessedLineNumber;
		}
	}
	
	private static class LineNumberConvertor implements ILineNumberConverter
	{
		private final List<Mapping> mappings;
		
		private int lastLineQuery = 1;
		
		private ListIterator<Mapping> iterator;
		
		public LineNumberConvertor(List<Mapping> mappings)
		{
			this.mappings = mappings;
			resetIterator();
		}
		
		private void resetIterator()
		{
			this.iterator = mappings.listIterator();
		}
		
		public SourceLocation convert(int lineNumber)
		{
			if (lineNumber < lastLineQuery) {
				//We optimise assuming that requests will be sequential. If someone checks an older line,
				//we need to reset the iterator.
				resetIterator();
			}
			
			//Walk until we either find the first entry after our line number, 
			//or we run out of mappings. Then the last mapping must be the one we want
			
			while (iterator.hasNext()) {
				Mapping thisMapping = iterator.next();
				
				if (thisMapping.preProcessedLineNumber > lineNumber) {
					break;
				}
			}
			
			//Previous mapping must be the last mapping inside our range
			if (iterator.hasPrevious()) {
				iterator.previous();
				if (iterator.hasPrevious()) {
					Mapping lastMilestone = iterator.previous();
					
					int diff = lineNumber - lastMilestone.preProcessedLineNumber - 1;
					
					int correctedLineNumber = lastMilestone.getLineNumber() + diff;
					
					if (correctedLineNumber < 0) {
						throw new IllegalArgumentException("Out of range: " + correctedLineNumber);
					}
					
					lastLineQuery = lineNumber;
					
					return new SourceLocation(lastMilestone.getFileName(), lastMilestone.getLineNumber() + diff);
				}
			}
			
			return null;
		}

	}
}
