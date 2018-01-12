/*[INCLUDE-IF Sidecar18-SE]*/
/*******************************************************************************
 * Copyright (c) 2007, 2017 IBM Corp. and others
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
package com.ibm.dtfj.javacore.parser.j9.section.common;

import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.regex.PatternSyntaxException;

public class CommonPatternMatchers {

	
	public static final Matcher whitespace = generateMatcher("\\s+");
	public static final Matcher allButLineFeed = generateMatcher("[^\\n\\r]*");
	public static final Matcher allButControlChars = generateMatcher("\\P{Cntrl}+");
	public static final Matcher quoted_stringvalue = generateMatcher("\"[^\\n\\r]*\"");
	public static final Matcher alpha_numeric_underscore_ascii_attribute = generateMatcher("[a-zA-Z_][a-zA-Z0-9_]*");
	public static final Matcher non_whitespace_attribute = generateMatcher("\\S+");
	public static final Matcher hex_0x = generateMatcher("0x\\p{XDigit}+");
	public static final Matcher signed_hex_0x = generateMatcher("-?0x\\p{XDigit}+");
	public static final Matcher hex = generateMatcher("\\p{XDigit}+");
	public static final Matcher lettervalue = generateMatcher("\\w+");
	public static final Matcher dec = generateMatcher("\\d+");
	public static final Matcher quotation = generateMatcher("\"");
	public static final Matcher colon = generateMatcher(":");
	public static final Matcher equals = generateMatcher("=");
	public static final Matcher comma = generateMatcher(",");
	public static final Matcher at_symbol = generateMatcher("@");
	public static final Matcher dot = generateMatcher("\\.");
	public static final Matcher forward_slash = generateMatcher("/");
	public static final Matcher dash = generateMatcher("-");
	public static final Matcher open_paren = generateMatcher("\\(");
	public static final Matcher close_paren = generateMatcher("\\)");

	
	private static final String pattern_java_name;
	static {
		String pattern;
		try {
			// Java 5.0 and above recognises this
			pattern = "\\p{javaJavaIdentifierStart}\\p{javaJavaIdentifierPart}*";
			// Test it works
			Pattern.compile(pattern);
		} catch (PatternSyntaxException e) {
			// Use this when running with Java 1.4.2
			pattern = "[a-zA-Z_$][a-zA-Z_$0-9]*";
		}
		pattern_java_name = pattern;
	}
	private static final String pattern_sov_java_absolute_name = pattern_java_name + "(\\." + pattern_java_name + ")*";
	private static final String pattern_java_absolute_name = pattern_java_name + "(/" + pattern_java_name + ")*";

	/* Allow for <init> and <clinit> */
	private static final String pattern_java_method_name = "\\<?" + pattern_java_name + "\\>?";
	
	private static final String pattern_sov_java_absolute_method_name = pattern_sov_java_absolute_name + "\\." + pattern_java_method_name;
	private static final String pattern_java_absolute_method_name = pattern_java_absolute_name + "\\." + pattern_java_method_name;
	
	public static final Matcher java_name = generateMatcher(pattern_java_name);
	public static final Matcher java_absolute_name = generateMatcher(pattern_java_absolute_name);
	
	public static final Matcher java_absolute_method_name = generateMatcher(pattern_java_absolute_method_name);
	public static final Matcher java_absolute_name_array = generateMatcher("[\\[]*" + pattern_java_absolute_name +";?");

	public static final Matcher java_sov_absolute_method_name = generateMatcher(pattern_sov_java_absolute_method_name);
	public static final Matcher java_file_name = generateMatcher(pattern_java_absolute_name + "\\.java");


	/*
	 * Some versions of SOV 1.4.2 do not prefix hexadecimals with 0x, therefore
	 * match either 0x00F or 00F. NOTE that prefixed pattern will be matched first, to avoid matching
	 * the 00F component of 0x00F, instead of the whole thing.
	 */
	public static final Matcher[] hexadecimal = {CommonPatternMatchers.hex_0x, CommonPatternMatchers.hex};
	
	public static final Matcher at_string = CommonPatternMatchers.generateMatcher("at", Pattern.CASE_INSENSITIVE);
	
	
	
	/*
	 * Some patterns for Java environment attributes.
	 */
	public static final Matcher build_string = CommonPatternMatchers.generateMatcher("build", Pattern.CASE_INSENSITIVE);
	public static final Matcher enabled_string = CommonPatternMatchers.generateMatcher("enabled", Pattern.CASE_INSENSITIVE);
	public static final Matcher disabled_string = CommonPatternMatchers.generateMatcher("disabled", Pattern.CASE_INSENSITIVE);
	public static final Matcher bits64 = CommonPatternMatchers.generateMatcher("64");
	public static final Matcher s390 = CommonPatternMatchers.generateMatcher("s390", Pattern.CASE_INSENSITIVE);

	
	/**
	 * 
	 * @param pattern
	 * 
	 */
	public static Matcher generateMatcher(String pattern) {
		return Pattern.compile(pattern).matcher("");
	}
	
	
	public static Matcher generateMatcher(String pattern, int flag) {
		return Pattern.compile(pattern, flag).matcher("");
	}

}
