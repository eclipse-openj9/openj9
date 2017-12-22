/*******************************************************************************
 * Copyright (c) 2009, 2014 IBM Corp. and others
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

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Utility class - used for parsing C type declarations.
 * 
 * Splits const int[][] into
 * 
 * prefix -> const
 * coreType -> int
 * suffix -> [][]
 * 
 * @author andhall
 *
 */
public class CTypeParser
{
	/* Matches strings that end with one or more []. The brackets can contain anything (numbers, strings etc. */
	private static final Pattern arrayPattern = Pattern.compile("(.*?)\\s*((?:\\[[^\\]]*\\]\\s*)+)$");
	
	/* Matches strings that end with one or more '*'s */
	private static final Pattern pointerPattern = Pattern.compile("(.*?)\\s*((?:[*]\\s*)+)$");
	
	/* Matches strings that end with : <number> */
	private static final Pattern bitfieldPattern = Pattern.compile("(.*?)\\s*(:\\s*\\d+)$");
	
	/* Matches strings that end with const */
	private static final Pattern trailingConstPattern = Pattern.compile("(.*?)(?<=[\\s*])const$");
	
	/* Matches strings that end with volatile */
	private static final Pattern trailingVolatilePattern = Pattern.compile("(.*?)(?<=[\\s*])volatile$");

	private String suffix;
	
	private String prefix;
	
	private String coreType;
	
	/**
	 * Default constructor.
	 * @param typeDeclaration C type declaration, e.g. int[][][]
	 */
	public CTypeParser(String typeDeclaration)
	{
		coreType = typeDeclaration.trim();
		prefix = "";
		
		/* Check for const */
		if(coreType.startsWith("const ")) {
			prefix = "const ";
			coreType = coreType.substring("const".length()).trim();
		}
		
		/* Check for volatile */
		if(coreType.startsWith("volatile ")) {
			prefix = "volatile ";
			coreType = coreType.substring("volatile".length()).trim();
		}
		
		/* Check for pointer/array cases */
		StringBuilder suffixBuilder = new StringBuilder();
		
		while (true) {
			Matcher m;
			
			if ( (m=arrayPattern.matcher(coreType)).find() ) {
				coreType = m.group(1);
				suffixBuilder.insert(0, stripSpaces(m.group(2)));
			} else if ( (m=pointerPattern.matcher(coreType)).find() ) {
				coreType = m.group(1);
				suffixBuilder.insert(0, stripSpaces(m.group(2)));
			} else if ( (m=bitfieldPattern.matcher(coreType)).find() ) {
				coreType = m.group(1);
				suffixBuilder.insert(0, stripSpaces(m.group(2)));
			} else if ( (m = trailingConstPattern.matcher(coreType)).find() ) {
				coreType = m.group(1);
				suffixBuilder.insert(0, " const ");
			} else if ( (m = trailingVolatilePattern.matcher(coreType)).find() ) {
				coreType = m.group(1);
				suffixBuilder.insert(0, " volatile "); 
			} else {
				break;
			}
			
			coreType = coreType.trim();
		}
		
		suffix = suffixBuilder.toString().trim();
	}
	
	private String stripSpaces(String input)
	{
		StringBuilder builder = new StringBuilder();
		
		for (char c : input.toCharArray()) {
			if (! Character.isWhitespace(c)) {
				builder.append(c);
			}
		}
		
		return builder.toString();
	}

	public String getPrefix()
	{
		return prefix;
	}
	
	public String getCoreType()
	{
		return coreType;
	}
	
	public String getSuffix()
	{
		return suffix;
	}
	
}
