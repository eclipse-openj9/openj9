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
package com.ibm.dtfj.javacore.parser.j9.section.thread;

import java.util.regex.Matcher;

import com.ibm.dtfj.javacore.parser.framework.scanner.TokenManager;
import com.ibm.dtfj.javacore.parser.framework.tag.LineRule;
import com.ibm.dtfj.javacore.parser.j9.section.common.CommonPatternMatchers;

public class StackTraceLineRule extends LineRule {

	private StringBuffer fNameBuffer = new StringBuffer("");

	
	/**
	 * 
	 */
	public void processLine(String source, int startingOffset) {
		int offset = startingOffset;
		consumeUntilFirstMatch(CommonPatternMatchers.at_string);

		/*
		 * METHOD_NAME
		 */
		processMethodAndClassName(offset);

		String methodType = IThreadTypes.STACKTRACE_JAVA_METHOD;
		if (consumeUntilFirstMatch(ThreadPatternMatchers.native_method_string)) {
			methodType = IThreadTypes.STACKTRACE_NATIVE_METHOD;
		}
		addToken(IThreadTypes.STACKTRACE_METHOD_TYPE, methodType);
		/*
		 * If not a native method, get file name and line number, if available
		 */
		if (methodType.equals(IThreadTypes.STACKTRACE_JAVA_METHOD)) {
			
			// For a java method, there are two known patterns: 
			//"java_file_name:[linenumber]"
			//"Bytecode PC:[pc]"
			// Java 5.0
			// 4XESTACKTRACE          at java/util/HashMap.<init>(HashMap.java:265)
			// 4XESTACKTRACE          at java/util/HashMap.<init>(HashMap.java:277)
			// 4XESTACKTRACE          at org/eclipse/mat/dtfjtests/Collections.forceOOM(Collections.java:294)
			// 4XESTACKTRACE          at org/eclipse/mat/dtfjtests/Collections.testFinalizers(Collections.java:279(Compiled Code))
			// 4XESTACKTRACE          at org/eclipse/mat/dtfjtests/Collections.main(Collections.java:225(Compiled Code))
			// Java 1.4.2
			// 4XESTACKTRACE          at java.lang.Object.wait(Object.java(Compiled Code))
			// Check for both

			if (!findFirst(ThreadPatternMatchers.bytecode_pc)) {
				/*
				 * CLASS_FILE_NAME
				 */
				addToken(IThreadTypes.CLASS_FILE_NAME, CommonPatternMatchers.java_file_name);
				/*
				 * STACKTRACE_LINE_NUMBER
				 */
				String compilation_level = IThreadTypes.UNKNOWN_COMPILATION_LEVEL;

				if (consumeUntilFirstMatch(CommonPatternMatchers.colon)) {
					addToken(IThreadTypes.STACKTRACE_LINE_NUMBER, CommonPatternMatchers.dec);
				}	
				if (consumeUntilFirstMatch(ThreadPatternMatchers.compiled_code)){
					compilation_level = IThreadTypes.COMPILED;
				}
				addToken(TokenManager.getToken(0, offset, getLineNumber(), IThreadTypes.COMPILATION_LEVEL, compilation_level));
			}
		}
	}	
	
	
	/**
	 * 
	 * @param token
	 * @param offset
	 */
	private void processMethodAndClassName(int offset) {
		Matcher matcher = findFirst(CommonPatternMatchers.forward_slash) ? CommonPatternMatchers.java_absolute_method_name : CommonPatternMatchers.java_sov_absolute_method_name;
		String fullName = matchAndConsumeValue(matcher);
		if (fullName != null) {
			fNameBuffer.delete(0, fNameBuffer.length());
			fNameBuffer.append(fullName);
			int endingDot = 0;
			for (int index = 0; index < fNameBuffer.length();index++) {
				if(fNameBuffer.charAt(index) == '.') {
					endingDot = index;
					fNameBuffer.setCharAt(index, '/');
				}
			}
			if (endingDot > 0) {
				String className = fNameBuffer.substring(0, endingDot);
				addToken(IThreadTypes.FULL_NAME, className);
			}
			endingDot++;
			if (endingDot < fNameBuffer.length()) {
				String methodName = fNameBuffer.substring(endingDot);
				addToken(IThreadTypes.METHOD_NAME, methodName);
			}
		}
	}
}
